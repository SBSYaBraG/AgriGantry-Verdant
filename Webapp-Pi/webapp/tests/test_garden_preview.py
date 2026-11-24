"""Regression checks for complete and interrupted simulated seed placement."""
import unittest

from hardware.simulated import SimulatedHardware


class GardenPlacementTest(unittest.TestCase):
    def setUp(self):
        self.sim = SimulatedHardware()
        self.plan = {"spot": "B2", "crop": "radish"}
        self.target = self.plant()["target"].copy()
        self.target["z"] //= 2

    def action(self, name, **fields):
        return self.sim.garden_preview({**self.plan, **fields, "action": name})

    def plant(self):
        return self.sim.get_status()["garden"]["spots"]["B2"]

    def home(self):
        self.sim.home_all()
        self.assertTrue(all(a["homing"] for a in self.sim.get_status()["axes"].values()))
        for _ in range(6):
            self.sim._tick(90)
        self.assertTrue(all(a["homed"] for a in self.sim.get_status()["axes"].values()))

    def test_placement_requires_a_plan_all_homes_and_a_complete_valid_target(self):
        self.assertIsNotNone(self.action("place", target=self.target))
        self.assertIsNone(self.action("plan"))
        self.assertIsNotNone(self.action("place", target=self.target))
        self.home()
        before = self.sim.get_status()["axes"]
        for bad in ({"x": self.target["x"]}, {**self.target, "x": -1}, {**self.target, "z": True}):
            with self.subTest(target=bad):
                self.assertIsNotNone(self.action("place", target=bad))
                self.sim._tick(90)
                self.assertEqual(self.sim.get_status()["axes"], before)
                self.assertEqual(self.plant()["stage"], "planned")
        self.assertIsNotNone(self.action("place", crop="carrot", target=self.target))

    def test_status_changes_to_growing_only_on_arrival_then_harvest_frees_spot(self):
        self.assertIsNotNone(self.action("harvest_ready", spot=None))
        self.assertIsNotNone(self.action("low_moisture"))
        self.home()
        self.action("plan")
        self.assertIsNone(self.action("place", target=self.target))
        self.assertIsNotNone(self.action("place", target=self.target))
        self.assertIsNotNone(self.action("remove"))
        self.sim.move_relative("x", -100)
        self.sim.home_axis("z")
        self.sim.home_all()
        self.sim._tick(.1)
        self.assertEqual(self.plant()["stage"], "positioning")
        self.sim._tick(90)
        self.assertEqual(self.plant()["stage"], "growing")
        axes = self.sim.get_status()["axes"]
        self.assertEqual({id_: axis["mm"] for id_, axis in axes.items()}, self.target)
        self.assertTrue(all(axis["homed"] for axis in axes.values()))
        self.assertIsNone(self.action("harvest_ready"))
        self.assertEqual(self.plant()["stage"], "ready")
        self.assertIsNone(self.action("remove"))
        self.assertEqual(self.plant()["stage"], "empty")
        self.assertEqual(self.sim.get_status()["garden"]["notices"], [])

    def test_care_tasks_stay_with_their_plant_and_survive_other_spots_being_tended(self):
        for spot in ("A1", "B2"):
            self.assertIsNone(self.action("demo_plant", spot=spot, crop=None))
            self.assertIsNotNone(self.action("demo_plant", spot=spot))
            for name in ("low_moisture", "harvest_ready", "harvest_ready"):
                self.assertIsNone(self.action(name, spot=spot))
        self.assertIsNotNone(self.action("mark_watered", crop="carrot"))
        self.assertIsNone(self.action("mark_watered", spot="A1"))
        garden = self.sim.get_status()["garden"]["spots"]
        self.assertEqual([garden[s]["stage"] for s in ("A1", "B2")], ["ready", "ready"])
        self.assertFalse(garden["A1"]["needs_water"])
        self.assertTrue(garden["B2"]["needs_water"])
        self.assertIsNone(self.action("remove", spot="A1"))
        self.assertTrue(self.plant()["needs_water"])
        self.action("clear_alerts")
        self.assertFalse(self.plant()["needs_water"])
        self.assertEqual(self.plant()["stage"], "ready")
        self.action("remove")
        self.assertIsNone(self.plant()["crop"])
        self.assertIsNotNone(self.action("low_moisture"))

    def test_sample_bed_readings_are_opt_in_and_independent_of_plant_tasks(self):
        self.assertEqual(self.sim.get_status()["bed_readings"], {})
        self.action("high_temperature")
        self.assertEqual(self.sim.get_status()["bed_readings"], {"temperature_c": 36})
        self.action("sample_conditions")
        readings = self.sim.get_status()["bed_readings"]
        self.assertEqual(set(readings), {"soil_moisture_pct", "temperature_c", "soil_ph", "humidity_pct"})
        self.assertEqual(readings["temperature_c"], 36)
        self.action("clear_alerts")
        self.assertEqual(self.sim.get_status()["bed_readings"]["temperature_c"], 23)
        self.assertFalse(any(p["needs_water"] for p in self.sim.get_status()["garden"]["spots"].values()))

    def test_previewing_a_planned_crop_for_a_demo_does_not_move_any_axis(self):
        self.assertIsNone(self.action("plan", crop="basil"))
        before = self.sim.get_status()["axes"]
        self.assertIsNotNone(self.action("demo_plant", crop="radish"))
        self.assertIsNone(self.action("demo_plant", crop="basil"))
        self.sim._tick(90)
        self.assertEqual(self.sim.get_status()["axes"], before)
        self.assertEqual((self.plant()["crop"], self.plant()["stage"]), ("basil", "growing"))

    def test_interlocks_discard_partial_placement_and_never_resume_it(self):
        self.home()
        self.action("plan")
        for interrupt in ("stop", "alarm"):
            with self.subTest(interrupt=interrupt):
                self.assertIsNone(self.action("place", target=self.target))
                self.sim._tick(.1)
                if interrupt == "stop":
                    self.sim.emergency_stop()
                else:
                    self.sim.inject_alm_fault("XR")
                before = self.sim.get_status()["axes"]
                self.assertEqual(self.plant()["stage"], "planned")
                self.assertIsNotNone(self.action("place", target=self.target))
                self.sim._tick(90)
                self.action("clear_alerts")
                if interrupt == "stop":
                    self.assertTrue(self.sim.get_status()["emergency_stop"])
                    self.sim.resume()
                self.sim._tick(90)
                self.assertEqual(self.sim.get_status()["axes"], before)
                self.assertEqual(self.plant()["stage"], "planned")
