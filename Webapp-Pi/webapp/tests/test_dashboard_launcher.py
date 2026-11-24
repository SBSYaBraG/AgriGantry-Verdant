"""A page can look current while its Python simulator still runs an older build."""
import io
import json
import unittest
from unittest.mock import patch

import start_dashboard as launcher


class DashboardLauncherTest(unittest.TestCase):
    @patch.object(launcher, "HTTPConnection")
    def test_running_build_must_match_even_when_old_server_supports_plant_tasks(self, connection):
        cfg = {"axes": {}, "motors": [], "bed_map": {}, "garden": {"plant_tasks": True}}
        for build, expected in [(None, "older"), ("previous", "older"), (launcher.SERVER_BUILD, "current")]:
            with self.subTest(build=build):
                response = io.BytesIO(json.dumps({**cfg, "server_build": build}).encode())
                response.status = 200
                connection.return_value.getresponse.return_value = response
                self.assertEqual(launcher.dashboard_on_port(5000), expected)

    @patch.object(launcher, "show_dashboard")
    @patch.object(launcher, "dashboard_on_port", side_effect=["older", "current"])
    def test_play_opens_only_the_verified_address(self, probe, show):
        with patch.object(launcher, "SERVER_PORT", 5000), patch.object(launcher.subprocess, "Popen") as spawn:
            self.assertEqual(launcher.main(), 0)
        self.assertEqual([call.args[0] for call in probe.call_args_list], [5000, 5001])
        show.assert_called_once_with(5001, True)
        spawn.assert_not_called()

    @patch.object(launcher, "show_dashboard")
    def test_new_server_is_ready_before_its_tab_opens(self, show):
        events = []
        states = iter(["free", "free", "current"])
        def probe(_port):
            state = next(states)
            events.append(state)
            return state

        show.side_effect = lambda *_: events.append("open")
        with patch.object(launcher, "dashboard_on_port", side_effect=probe), \
                patch.object(launcher.time, "sleep"), patch.object(launcher.subprocess, "Popen") as spawn:
            child = spawn.return_value
            child.poll.side_effect = [None, 0]
            child.wait.return_value = 0
            self.assertEqual(launcher.main(), 0)
        self.assertEqual(events, ["free", "free", "current", "open"])
