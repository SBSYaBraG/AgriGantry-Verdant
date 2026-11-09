"""Prepare the official FarmBot Genesis CAD export for the web dashboard.

The Onshape GLB contains tens of thousands of tiny CAD primitives.  Loading
that file directly would overwhelm a browser (and especially a Raspberry Pi),
so this script:

* removes electrical, plumbing, wiring, camera, and tiny fastener detail;
* keeps the raised bed, tools, motors, frame, and carrier mounting supports;
* shortens the bed and X tracks for the compact dashboard mock-up;
* replaces the non-animating X belt loops with simple flat track belts;
* consolidates CAD primitives by material and motion group;
* simplifies the resulting meshes; and
* emits named groups that the dashboard can animate independently.

The source GLB is never modified.

Model-build dependencies (not web-server runtime dependencies):
    pip install trimesh fast-simplification numpy
"""

from __future__ import annotations

import argparse
import copy
import re
from collections import defaultdict
from pathlib import Path

import numpy as np
import trimesh
from trimesh.visual.material import PBRMaterial
from trimesh.visual.texture import TextureVisuals


ROOT_NAME = "FarmBot Genesis v1.8 Beta"

# These assemblies are deliberately removed wholesale.  They either describe
# infrastructure outside the robot or contain systems that are not part of the
# digital-twin scope agreed for this dashboard.
REMOVE_SUBTREES = {
    "Utilities Post <1>",
    "Electronics Box <1>",
    "Solenoid w/ Mount, Regulator, Barbs <1>",
    "Mounted Vacuum Pump <1>",
    "Camera w/ Mount <1>",
    "Power Supply <1>",
    # A real drag chain changes shape between fixed and moving endpoints. A
    # rigid animation looks wrong, while hundreds of articulated links are too
    # expensive for the Raspberry Pi 3 target. Keep the mechanical supports
    # elsewhere in the assembly, but omit the loaded carrier assemblies.
    "Loaded X-Axis Cable Carrier <1>",
    "Loaded Y-Axis Cable Carrier <1>",
    "Loaded Z-Axis Cable Carrier <1>",
}

# This is a visual experiment, not a change to the simulated 0..2800 mm X
# coordinates. Long members are shortened along model Z (FarmBot X), while
# small end brackets are moved inward without being squashed.
X_LENGTH_SCALE = 0.65
LONG_MEMBER_METRES = 0.75
LENGTH_ASSEMBLIES = {
    "Fixed Raised Bed <1>",
    "Tracks <1>",
}
POSITION_ONLY_ASSEMBLIES = {
    "Loaded Toolbays <1>",
}

# The CAD X belts include a fixed loop that climbs to the moving gantry motor.
# That loop cannot follow the motor without deforming the mesh. Remove the CAD
# belt assembly and add two lightweight, flat strips along the shortened rails.
X_BELT_ASSEMBLIES = {
    "X-Axis Belt w/ Clips <1>",
    "X-Axis Belt w/ Clips <2>",
}
FLAT_BELT_X = (-0.690, 0.690)
FLAT_BELT_Y = -0.0015
FLAT_BELT_WIDTH = 0.006
FLAT_BELT_THICKNESS = 0.002
FLAT_BELT_LENGTH = 2.868 * X_LENGTH_SCALE

# Named subassemblies that must stay independent for future tool pickup.
TOOL_GROUPS = {
    "Watering Nozzle <1>": "Tool_Watering_Nozzle",
    "Rotary Tool <1>": "Tool_Rotary",
    "Soil Sensor <1>": "Tool_Soil_Sensor",
    "Seeder <1>": "Tool_Seeder",
}

# The model is exported Y-up.  The FarmBot axes therefore map as follows:
#   FarmBot X (gantry, along bed) -> model Z
#   FarmBot Y (cross-slide)       -> model X
#   FarmBot Z (vertical)          -> model Y
MOTION_GROUPS = {
    "Gantry <1>": "Gantry_X",
    "Cross-Slide <1>": "Y_Carriage",
}

# Most of the Z-axis assembly (motor, leadscrew, and carrier supports) is fixed
# to the cross-slide.  Only these pieces actually translate vertically.  The
# distinction prevents the dashboard from making the Z motor float up and down
# with the tool head.
Z_MOVING_SUBTREES = {
    "occurrence of 20mm x 20mm x 1000mm Extrusion",
    "Z-Axis Hardstop w/ Hardware <1>",
    "Z-Axis Hardstop w/ Hardware <2>",
    "UTM <1>",
}

FASTENER_RE = re.compile(
    r"(?:screw|locknut|washer|zip tie|o-ring|gasket)", re.IGNORECASE
)


def should_remove(name: str) -> bool:
    """Return whether a node and everything below it should be removed."""
    lowered = name.lower()

    if name in REMOVE_SUBTREES:
        return True

    # Carrier assemblies are removed above, but their mounting supports remain
    # elsewhere in the machine. Also remove anything routed through a carrier.
    # The distinction matters because both contain "cable".
    if "cable" in lowered and "cable carrier" not in lowered:
        return True

    if any(
        phrase in lowered
        for phrase in (
            "water tube",
            "vacuum tube",
            "garden hose",
            "garden faucet",
            "extension cord",
            "outlet box",
            "farmduino",
            "raspberry pi",
            "pi adapter board",
            "power supply",
            "solenoid",
            "vacuum pump",
            "inline air filter",
            "led strip",
        )
    ):
        return True

    # Internal circuit boards add a surprising amount of CAD detail but are
    # invisible at dashboard viewing distance.  Mechanical tool shells remain.
    if "pcb" in lowered:
        return True

    # Only remove named fastener occurrences, not structural assemblies such
    # as "Nut Bar w/ Screws" that contain useful mechanical geometry.
    if lowered.startswith("occurrence of") and FASTENER_RE.search(lowered):
        return True

    return False


def ancestry(node: str, parents: dict[str, str]) -> list[str]:
    """Return node-to-root ancestry, including *node*."""
    result = [node]
    while result[-1] in parents:
        result.append(parents[result[-1]])
    return result


def output_group(chain: list[str]) -> str:
    """Choose the consolidated output group for one geometry node."""
    for source_name, target_name in TOOL_GROUPS.items():
        if source_name in chain:
            return target_name

    for source_name, target_name in MOTION_GROUPS.items():
        if source_name in chain:
            return target_name

    if "Z-Axis <1>" in chain:
        if any(source_name in chain for source_name in Z_MOVING_SUBTREES):
            return "Z_Extension"
        return "Y_Carriage"

    return "Static"


def material_key(mesh: trimesh.Trimesh) -> tuple:
    material = getattr(mesh.visual, "material", None)
    if material is None:
        return ("default",)

    color = getattr(material, "baseColorFactor", None)
    if color is None:
        color_tuple = None
    else:
        color_tuple = tuple(np.asarray(color).reshape(-1).tolist())

    return (
        getattr(material, "name", "material"),
        color_tuple,
        getattr(material, "metallicFactor", None),
        getattr(material, "roughnessFactor", None),
        getattr(material, "doubleSided", None),
    )


def shorten_along_machine_x(mesh: trimesh.Trimesh) -> None:
    """Shorten a long member or move a small end component inward."""
    z_min, z_max = mesh.bounds[:, 2]
    if z_max - z_min >= LONG_MEMBER_METRES:
        mesh.vertices[:, 2] *= X_LENGTH_SCALE
        return

    centre_z = (z_min + z_max) / 2
    mesh.vertices[:, 2] += centre_z * X_LENGTH_SCALE - centre_z


def move_inward_along_machine_x(mesh: trimesh.Trimesh) -> None:
    """Move an assembly inward without changing the size of its parts."""
    z_min, z_max = mesh.bounds[:, 2]
    centre_z = (z_min + z_max) / 2
    mesh.vertices[:, 2] += centre_z * X_LENGTH_SCALE - centre_z


def add_flat_x_belts(
    grouped: dict[str, dict[tuple, list[trimesh.Trimesh]]],
    materials: dict[tuple, object],
) -> None:
    """Add the useful flat belt runs without the fixed motor-return loops."""
    material = PBRMaterial(
        name="FB_Belt",
        baseColorFactor=[24, 26, 29, 255],
        metallicFactor=0.0,
        roughnessFactor=0.85,
    )
    for x_position in FLAT_BELT_X:
        belt = trimesh.creation.box(
            extents=(FLAT_BELT_WIDTH, FLAT_BELT_THICKNESS, FLAT_BELT_LENGTH)
        )
        belt.apply_translation((x_position, FLAT_BELT_Y, 0.0))
        belt.visual = TextureVisuals(material=copy.deepcopy(material))
        key = material_key(belt)
        grouped["Static"][key].append(belt)
        materials.setdefault(key, material)


def consolidate(
    meshes: list[trimesh.Trimesh],
    material,
    simplify_ratio: float,
) -> trimesh.Trimesh:
    """Join same-material meshes and simplify the result."""
    merged = trimesh.util.concatenate(meshes)
    merged.remove_unreferenced_vertices()
    merged.merge_vertices()

    before = len(merged.faces)
    if before >= 100:
        target = max(50, int(before * simplify_ratio))
        try:
            simplified = merged.simplify_quadric_decimation(
                face_count=target,
                aggression=7,
            )
            if simplified is not None and len(simplified.faces) > 0:
                merged = simplified
        except Exception as exc:  # CAD meshes can be non-manifold.
            print(f"warning: simplification skipped ({exc})")

    if material is not None:
        merged.visual = TextureVisuals(material=copy.deepcopy(material))

    return merged


def prepare(source: Path, destination: Path, simplify_ratio: float) -> None:
    print(f"Loading {source} ...")
    scene = trimesh.load(source, force="scene", process=False)
    if not isinstance(scene, trimesh.Scene):
        raise RuntimeError("Expected a GLB scene, not a single mesh")

    parents = dict(scene.graph.transforms.parents)
    grouped: dict[str, dict[tuple, list[trimesh.Trimesh]]] = defaultdict(
        lambda: defaultdict(list)
    )
    materials: dict[tuple, object] = {}

    kept_instances = 0
    removed_instances = 0
    source_faces = 0

    for node in scene.graph.nodes_geometry:
        chain = ancestry(node, parents)
        if any(should_remove(name) for name in chain):
            removed_instances += 1
            continue

        # The official raised-bed assembly contains one 90 x 90 mm post in
        # the soil area. After browser simplification it reads as a floating
        # triangular wedge, so omit only that post. Other structural posts in
        # the FarmBot assembly are unaffected.
        if (
            "Fixed Raised Bed <1>" in chain
            and "occurrence of Post" in chain
        ):
            removed_instances += 1
            continue

        if any(name in chain for name in X_BELT_ASSEMBLIES):
            removed_instances += 1
            continue

        transform, geometry_name = scene.graph[node]
        source_mesh = scene.geometry[geometry_name]
        mesh = source_mesh.copy()
        mesh.apply_transform(transform)

        if any(name in chain for name in LENGTH_ASSEMBLIES):
            shorten_along_machine_x(mesh)
        elif any(name in chain for name in POSITION_ONLY_ASSEMBLIES):
            move_inward_along_machine_x(mesh)

        group = output_group(chain)
        key = material_key(mesh)
        grouped[group][key].append(mesh)
        materials.setdefault(key, getattr(source_mesh.visual, "material", None))
        kept_instances += 1
        source_faces += len(mesh.faces)

    add_flat_x_belts(grouped, materials)

    output = trimesh.Scene(base_frame="FarmBotGenesis")
    identity = np.eye(4)

    # Keep a physically meaningful hierarchy.  The carriage rides the gantry,
    # and the Z extension rides the carriage, so parent motion carries every
    # downstream component exactly as it does on the machine.
    output.graph.update(frame_to="Static", frame_from="FarmBotGenesis", matrix=identity)
    output.graph.update(frame_to="Gantry_X", frame_from="FarmBotGenesis", matrix=identity)
    output.graph.update(frame_to="Y_Carriage", frame_from="Gantry_X", matrix=identity)
    output.graph.update(frame_to="Z_Extension", frame_from="Y_Carriage", matrix=identity)

    for tool_group in TOOL_GROUPS.values():
        output.graph.update(
            frame_to=tool_group,
            frame_from="FarmBotGenesis",
            matrix=identity,
        )

    final_faces = 0
    final_meshes = 0
    for group in sorted(grouped):
        for index, (key, meshes) in enumerate(grouped[group].items()):
            merged = consolidate(meshes, materials[key], simplify_ratio)
            final_faces += len(merged.faces)
            final_meshes += 1
            safe_material = re.sub(r"[^A-Za-z0-9_-]+", "_", str(key[0])).strip("_")
            safe_material = safe_material or "material"
            output.add_geometry(
                merged,
                geom_name=f"{group}_{safe_material}_{index}",
                node_name=f"{group}_{safe_material}_{index}",
                parent_node_name=group,
            )

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output.export(file_type="glb"))

    print("Model preparation complete")
    print(f"  kept geometry instances:    {kept_instances}")
    print(f"  removed geometry instances: {removed_instances}")
    print(f"  source rendered triangles:  {source_faces:,}")
    print(f"  output triangles:           {final_faces:,}")
    print(f"  output meshes:              {final_meshes}")
    print(f"  output size:                {destination.stat().st_size / 1048576:.2f} MiB")
    print(f"  wrote:                      {destination}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument(
        "--ratio",
        type=float,
        default=0.30,
        help="Target fraction of faces retained per consolidated mesh (default: 0.30)",
    )
    args = parser.parse_args()
    if not 0 < args.ratio <= 1:
        parser.error("--ratio must be greater than 0 and at most 1")
    prepare(args.source, args.destination, args.ratio)


if __name__ == "__main__":
    main()
