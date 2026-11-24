/* ============================================================
   scene3d.js - The machine, rendered live, as the whole page

   Loads the optimized FarmBot Genesis v1.8 model and drives its moving parts
   from the same
   status_update data every other part of the interface uses. The 3D view is
   another way of looking at one shared truth, never a separate source of it.

   COORDINATE SYSTEMS
   ------------------
   The Onshape model was exported as a Y-up GLB. The mapping is:

       machine X (gantry along bed) -> model Z
       machine Y (cross-slide)      -> model X
       machine Z (tool height)      -> model Y (up)

   The values below are model-space offsets measured from this specific
   optimized export. The physical travel shown to the user remains defined in
   config.py; the 3D model consumes the same 0..100% axis fractions.

   THE MOVING PARTS
   ----------------
   Parenting is Gantry_X -> Y_Carriage -> Z_Extension, and each level adds one
   axis, exactly like the four-motor machine:

       Gantry_X      rolls along the bed's length     -> machine X (XL + XR)
       Y_Carriage    rides across the top beam        -> machine Y (Y)
       Z_Extension   slides down through the carriage -> machine Z (Z)

   Because the chain nests, moving the gantry carries the carriage and tool
   head with it, and moving the carriage carries the vertical extension - the
   same way the steel does. Each part therefore only ever needs its own single
   axis set here; the rest follows for free.

   The source CAD was prepared by tools/prepare_farmbot_model.py. That script
   removes nonessential electrical/plumbing detail, preserves the tools as
   independent groups for future pickup simulation, and creates the nested
   motion groups used here.

   RENDERING ON DEMAND (this matters on a Raspberry Pi)
   ----------------------------------------------------
   The canvas fills the whole page, so a naive 60 fps loop would keep the GPU
   busy forever - including all the time the machine is sitting perfectly
   still. On a Pi that is work stolen from the server actually driving the
   motors.

   Instead nothing is drawn unless something changed: the machine moved, the
   user is dragging the camera, or the window resized. An idle machine costs
   essentially nothing.
   ============================================================ */

import * as THREE from "three";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";

const PART_GANTRY = "Gantry_X";
const PART_CARRIAGE = "Y_Carriage";
const PART_EXTENSION = "Z_Extension";

// Model-space offsets from the authored pose. The web model's bed and X tracks
// are intentionally shown at 65% of the CAD length. The same simulated
// 0..2800 mm X values therefore map onto this shorter visual stroke. The source
// Z-axis was authored fully lowered, so 0% raises the tool 500 mm and 100%
// returns it to that pose.
const MODEL_TRAVEL = {
  x: { prop: "z", at0: -1.109, at1: 0.656 },
  y: { prop: "x", at0: -0.349, at1: 0.951 },
  z: { prop: "y", at0: 0.500, at1: 0.000 }
};

// Camera directions use model coordinates: X is the machine width, Y is up,
// and Z is the machine length. Each preset is selected by the view cube.
const VIEW_PRESETS = {
  overview: { direction: [1.15, 0.38, 1],  up: [0, 1, 0] },
  top:    { direction: [0, 1, 0],          up: [0, 0, -1] },
  bottom: { direction: [0, -1, 0],         up: [0, 0, 1] },
  front:  { direction: [0, 0, 1],          up: [0, 1, 0] },
  back:   { direction: [0, 0, -1],         up: [0, 1, 0] },
  left:   { direction: [-1, 0, 0],         up: [0, 1, 0] },
  right:  { direction: [1, 0, 0],          up: [0, 1, 0] },
  "top-front-right":    { direction: [1, 1, 1],    up: [0, 1, 0] },
  "top-front-left":     { direction: [-1, 1, 1],   up: [0, 1, 0] },
  "top-back-right":     { direction: [1, 1, -1],   up: [0, 1, 0] },
  "top-back-left":      { direction: [-1, 1, -1],  up: [0, 1, 0] },
  "bottom-front-right": { direction: [1, -1, 1],   up: [0, 1, 0] },
  "bottom-front-left":  { direction: [-1, -1, 1],  up: [0, 1, 0] },
  "bottom-back-right":  { direction: [1, -1, -1],  up: [0, 1, 0] },
  "bottom-back-left":   { direction: [-1, -1, -1], up: [0, 1, 0] }
};

/**
 * Fallback colours, keyed by material name.
 *
 * An earlier export of the model arrived with material names but no colours,
 * so everything rendered white. The model has since been re-exported
 * correctly, and applyPalette() only touches materials that are still pure
 * white - so this table now does nothing. It is kept because it costs
 * nothing and quietly rescues the view if a future export loses colours
 * again, which is an easy mistake to make in Blender: the glTF exporter reads
 * only the Principled BSDF's Base Color input, and a colour plugged in
 * anywhere else in the node graph is silently dropped.
 */
const PALETTE = {
  FB_Extrusion: { color: 0x2b2f36, metalness: 0.55, roughness: 0.55 },
  FB_Slot:      { color: 0x3b414a, metalness: 0.45, roughness: 0.65 },
  FB_Silver:    { color: 0xb9bec6, metalness: 0.85, roughness: 0.35 },
  FB_Wood:      { color: 0xc2905c, metalness: 0.0,  roughness: 0.85 },
  FB_PVC:       { color: 0xe6e4de, metalness: 0.0,  roughness: 0.6  },
  FB_Teal:      { color: 0x1f8f8f, metalness: 0.3,  roughness: 0.5  },
  FB_Mustard:   { color: 0xd9a327, metalness: 0.3,  roughness: 0.5  },
  FB_Red:       { color: 0xc0392b, metalness: 0.3,  roughness: 0.5  },
  FB_Tool:      { color: 0x51575f, metalness: 0.6,  roughness: 0.45 }
};

let renderer, scene, camera, controls;
let modelRoot = null;

// The three moving parts, found in the model by name once it has loaded.
const parts = { gantry: null, carriage: null, extension: null };

/**
 * How each axis moves, filled in by configureTravel().
 *
 * One entry per machine axis, each shaped:
 *
 *     { node, prop, basePos, at0, at1 }
 *
 *   node    - the object that moves
 *   prop    - which of its local position components changes ("x"/"y"/"z")
 *   basePos - that component's value as the model was authored
 *   at0/at1 - the offset from basePos at 0% and at 100% of travel
 *
 * Storing offsets from the authored pose, rather than absolute coordinates,
 * means nothing here has to know about the model's internal origins or its
 * parent transforms - which is exactly what makes the nested chain work.
 */
const travel = { x: null, y: null, z: null };

let container = null;
let ready = false;
let pendingStatus = null;
let machineConfig = null;

// --- Render-on-demand bookkeeping ---
// `dirty` means something changed and one more frame is owed. `interactingUntil`
// keeps frames coming briefly after the user lets go of the camera, so the
// OrbitControls damping can glide to a stop instead of freezing mid-drift.
let dirty = true;
let interacting = false;
let interactingUntil = 0;

// The last pose actually drawn, so an unchanged status update costs nothing.
const lastDrawn = { x: null, y: null, z: null };

/* ============================================================
   SETUP
   ============================================================ */

export function init(containerElement, modelUrl) {
  container = containerElement;

  scene = new THREE.Scene();
  scene.background = new THREE.Color(0xa8dafa);

  camera = new THREE.PerspectiveCamera(42, 1, 0.01, 100);

  renderer = new THREE.WebGLRenderer({
    antialias: true,
    powerPreference: "high-performance"
  });
  // Cap the pixel ratio hard. Phones report 3x, which is nine times the pixels
  // for a barely perceptible gain - and this may be driving a monitor from a
  // Raspberry Pi.
  // Match the display's pixel density, capped at 2. Rendering below the
  // device's actual ratio means the canvas is upscaled to fit, which looks
  // exactly like a blurred image. Above 2 the extra pixels are imperceptible
  // and cost real work on a Pi.
  // The Pi 3's VideoCore IV and 1 GB shared memory are much more sensitive to
  // pixel count than a desktop GPU. A 1.25 cap cuts the worst-case drawing
  // buffer to about 39% of the old 2x cap while remaining crisp at dashboard
  // viewing distance.
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, 1.25));
  container.appendChild(renderer.domElement);

  controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;
  controls.dampingFactor = 0.08;
  // Keep the camera above the floor; below it is disorienting and shows
  // nothing useful.
  // A bottom preset is intentionally available from the view cube, so the
  // orbit controller must permit the camera to cross below the model.
  controls.maxPolarAngle = Math.PI;
  controls.minDistance = 0.6;
  controls.maxDistance = 6;

  // Any camera movement owes a frame.
  controls.addEventListener("change", requestRender);
  controls.addEventListener("start", function () {
    interacting = true;
  });
  controls.addEventListener("end", function () {
    interacting = false;
    // Keep drawing for a moment so damping can settle smoothly.
    interactingUntil = performance.now() + 1200;
  });

  addLighting();

  // WATCH THE CONTAINER, NOT JUST THE WINDOW.
  //
  // The scene now grows to fill whatever height the layout leaves it, so it
  // can change size without the window changing at all: an emergency-stop or
  // alarm banner appearing pushes it shorter, and dismissing one gives the
  // height back. Neither fires a window resize event. Listening only to the
  // window would leave the canvas at its old size while CSS stretched it to
  // the new one - which is precisely the mismatch that made the model look
  // blurred before.
  if (window.ResizeObserver) {
    new ResizeObserver(resize).observe(container);
  }
  // Kept as well: ResizeObserver is well supported, but a window resize is
  // the one case that must never be missed, and the handler is idempotent.
  window.addEventListener("resize", resize);

  // Size the renderer to its container straight away. Without this the canvas
  // keeps WebGL's default 300x150 and CSS stretches it across the panel,
  // which looks exactly like a badly blurred image - and leaves the camera
  // with a 1:1 aspect ratio, so the framing is wrong too.
  resize();

  return loadModel(modelUrl);
}

function addLighting() {
  // Hemisphere light gives a soft base: cool from above, warm bounce from the
  // ground. Cheap, and enough to read the machine's shape without shadows -
  // shadow maps would be a real cost on a Pi for little gain here.
  scene.add(new THREE.HemisphereLight(0xbfd4ff, 0x3a2f24, 2.2));

  const key = new THREE.DirectionalLight(0xffffff, 1.8);
  key.position.set(2, 3, 1.5);
  scene.add(key);

  const fill = new THREE.DirectionalLight(0xcfe0ff, 0.6);
  fill.position.set(-2, 1.5, -1.5);
  scene.add(fill);
}

function addLawn(root) {
  const bounds = new THREE.Box3().setFromObject(root);
  const size = bounds.getSize(new THREE.Vector3());
  const span = Math.max(size.x, size.z);
  const tile = document.createElement("canvas");
  tile.width = tile.height = 64;
  const pen = tile.getContext("2d");
  pen.fillStyle = "#98bc79";
  pen.fillRect(0, 0, 64, 64);
  // A small repeating tuft pattern costs two triangles for the entire lawn.
  for (const [x, y] of [[5, 11], [24, 34], [46, 8], [38, 54], [57, 40], [9, 58]]) {
    pen.strokeStyle = "#87ad6b";
    pen.beginPath();
    pen.moveTo(x - 2, y - 2);
    pen.lineTo(x, y);
    pen.lineTo(x + 2, y - 3);
    pen.stroke();
  }
  const texture = new THREE.CanvasTexture(tile);
  texture.colorSpace = THREE.SRGBColorSpace;
  texture.wrapS = texture.wrapT = THREE.RepeatWrapping;
  texture.repeat.set(320, 320);
  const lawn = new THREE.Mesh(new THREE.PlaneGeometry(span * 80, span * 80),
    new THREE.MeshBasicMaterial({map: texture, side: THREE.FrontSide}));
  lawn.rotation.x = -Math.PI / 2;
  bounds.getCenter(lawn.position);
  lawn.position.y = bounds.min.y - span * 0.002;
  scene.add(lawn);
  scene.fog = new THREE.Fog(scene.background, span * 4, span * 24);
}

function loadModel(modelUrl) {
  return new Promise(function (resolve, reject) {
    new GLTFLoader().load(
      modelUrl,
      function (gltf) {
        modelRoot = gltf.scene;
        scene.add(gltf.scene);

        parts.gantry = gltf.scene.getObjectByName(PART_GANTRY);
        parts.carriage = gltf.scene.getObjectByName(PART_CARRIAGE);
        parts.extension = gltf.scene.getObjectByName(PART_EXTENSION);

        const missing = [
          !parts.gantry && PART_GANTRY,
          !parts.carriage && PART_CARRIAGE,
          !parts.extension && PART_EXTENSION
        ].filter(Boolean);

        if (missing.length) {
          reject(new Error("Model is missing: " + missing.join(", ")));
          return;
        }

        applyPalette(gltf.scene);
        configureTravel();
        addLawn(gltf.scene);

        // Re-measure before framing. The panel may still have been settling
        // when init() ran, and frameCamera depends on camera.aspect being
        // right - a stale aspect would frame the machine badly.
        resize();
        frameCamera(gltf.scene);

        ready = true;
        requestRender();
        resolve();
      },
      undefined,
      reject
    );
  });
}

/** Recolour only materials that arrived pure white. See PALETTE above. */
function applyPalette(root) {
  root.traverse(function (object) {
    if (!object.isMesh) { return; }

    const materials = Array.isArray(object.material)
      ? object.material
      : [object.material];

    materials.forEach(function (material) {
      if (!material) { return; }

      const preset = PALETTE[material.name];
      if (!preset) { return; }

      // glTF's default when baseColorFactor is absent is pure white; anything
      // else means the exporter carried a real colour through.
      const isDefaultWhite =
        material.color &&
        material.color.r === 1 && material.color.g === 1 && material.color.b === 1;

      if (!isDefaultWhite) { return; }

      material.color.setHex(preset.color);
      if (material.metalness !== undefined) { material.metalness = preset.metalness; }
      if (material.roughness !== undefined) { material.roughness = preset.roughness; }
    });
  });
}

/** Configure the three prepared CAD groups against their model-space ranges. */
function configureTravel() {
  travel.x = linearTravel(parts.gantry, MODEL_TRAVEL.x);
  travel.y = linearTravel(parts.carriage, MODEL_TRAVEL.y);
  travel.z = linearTravel(parts.extension, MODEL_TRAVEL.z);
}

/** Bundle a part, the component it slides along, and its two end offsets. */
function linearTravel(node, modelRange) {
  return {
    node: node,
    prop: modelRange.prop,
    basePos: node.position[modelRange.prop],
    at0: modelRange.at0,
    at1: modelRange.at1
  };
}

/**
 * Point the camera at the machine, framed to fill the view.
 *
 * The distance is computed from the model's bounding sphere and the camera's
 * actual field of view, rather than multiplied out of the bounding box by
 * some hand-picked factor. Guessed factors were what left the machine sitting
 * small in the middle of a wide frame: the box diagonal of a long, low
 * machine is much larger than the height that actually needs to fit, so
 * scaling by it pushed the camera far further back than necessary.
 *
 * Both dimensions are checked because a wide panel constrains vertically
 * while a narrow one constrains horizontally, and the view must fit either
 * way.
 */
function frameCamera(
  root,
  direction = new THREE.Vector3(...VIEW_PRESETS.overview.direction),
  up = new THREE.Vector3(...VIEW_PRESETS.overview.up)
) {
  const box = new THREE.Box3().setFromObject(root);
  const sphere = box.getBoundingSphere(new THREE.Sphere());
  const centre = sphere.center;
  const radius = sphere.radius;

  const vFov = THREE.MathUtils.degToRad(camera.fov);
  const hFov = 2 * Math.atan(Math.tan(vFov / 2) * camera.aspect);

  direction = direction.clone().normalize();
  up = up.clone().normalize();
  camera.up.copy(up);

  // Start from the distance that fits the bounding sphere. That is guaranteed
  // not to clip, but it is a loose fit - the sphere is much bigger than the
  // machine's actual silhouette from this angle, which leaves it looking
  // small and far away in a wide frame.
  let distance = Math.max(
    radius / Math.sin(vFov / 2),
    radius / Math.sin(hFov / 2)
  );

  camera.near = Math.max(radius / 500, 0.01);
  camera.far = radius * 40;

  // Now close in on the real silhouette. Measuring how much of the frame the
  // model's projected corners actually occupy, and scaling the distance by
  // how far that is from the target, converges in a couple of passes -
  // projected size varies as roughly 1/distance, so each pass lands close.
  // Keep the complete machine visible, but use the panel more tightly than
  // the previous 92% framing. Stopping below 1.0 retains a small background
  // margin instead of cropping the highest rail or the bed corners.
  const TARGET_FILL = 0.97;

  for (let pass = 0; pass < 4; pass++) {
    camera.position.copy(centre).addScaledVector(direction, distance);
    camera.lookAt(centre);
    camera.updateMatrixWorld(true);
    camera.updateProjectionMatrix();

    const occupied = projectedFill(box, camera);
    if (occupied <= 0) { break; }

    distance *= occupied / TARGET_FILL;
  }

  controls.target.copy(centre);
  camera.position.copy(centre).addScaledVector(direction, distance);
  camera.updateProjectionMatrix();
  controls.update();
  requestRender();
}

/**
 * How much of the frame the box occupies, as a fraction of the tighter axis.
 *
 * Projects the box's eight corners into normalised device coordinates, where
 * the visible frame runs -1..1 on both axes, and returns the larger of the
 * two extents. 1.0 means the model exactly touches the frame edge; above 1.0
 * it is being cropped.
 */
function projectedFill(box, camera) {
  const corner = new THREE.Vector3();
  let maxX = 0;
  let maxY = 0;

  for (const x of [box.min.x, box.max.x]) {
    for (const y of [box.min.y, box.max.y]) {
      for (const z of [box.min.z, box.max.z]) {
        corner.set(x, y, z).project(camera);
        maxX = Math.max(maxX, Math.abs(corner.x));
        maxY = Math.max(maxY, Math.abs(corner.y));
      }
    }
  }

  return Math.max(maxX, maxY);
}

/* ============================================================
   DRIVING THE MODEL FROM MACHINE STATE
   ============================================================ */

export function setMachineConfig(cfg) {
  machineConfig = cfg;
  requestRender();
}

/**
 * Hand the newest machine status to the 3D view.
 *
 * Only stores it; the render loop applies it. A burst of status updates
 * therefore cannot cause more work than there are frames to draw.
 */
export function update(status) {
  pendingStatus = status;
  requestRender();
}

/** Mark that one more frame is owed. */
function requestRender() {
  dirty = true;
}

/**
 * Apply the stored status to the model.
 *
 * Returns true if anything actually moved. An unchanged pose returns false,
 * which is what lets an idle machine stop consuming frames entirely - the
 * server keeps broadcasting several times a second even when nothing is
 * happening, and redrawing for each of those would defeat the whole point.
 */
function applyStatus() {
  if (!ready || !pendingStatus || !machineConfig) { return false; }

  const axes = pendingStatus.axes;
  if (pendingStatus.commands_enabled === false) {
    if (!pendingStatus.hardware_connected || !["x", "y", "z"].every(id => axes[id]?.homed)) return false;
  }
  const fx = fractionOf("x", axes.x);
  const fy = fractionOf("y", axes.y);
  const fz = fractionOf("z", axes.z);

  if (fx === lastDrawn.x && fy === lastDrawn.y && fz === lastDrawn.z) {
    return false;
  }
  lastDrawn.x = fx; lastDrawn.y = fy; lastDrawn.z = fz;

  // Each axis drives exactly one part along exactly one component. Everything
  // that makes that correct - which part, which direction, how far, and which
  // end is 0% - was settled once in configureTravel(), so all that is left here
  // is to slide each part to its fraction of its own stroke. The nesting in
  // the model does the rest: the carriage rides the gantry, the extension
  // rides the carriage.
  place(travel.y, fy);
  place(travel.x, fx);
  place(travel.z, fz);

  return true;
}

/** Slide one part to `fraction` (0..1) along its travel. */
function place(axisTravel, fraction) {
  axisTravel.node.position[axisTravel.prop] =
    axisTravel.basePos + lerp(axisTravel.at0, axisTravel.at1, fraction);
}

/**
 * An axis's position as a fraction (0..1) of its travel.
 *
 * Recomputed from raw steps rather than the server's rounded `percent`: a
 * whole-number percentage would make the gantry advance in visible jumps
 * instead of gliding.
 */
function fractionOf(axisId, axisState) {
  if (!axisState) { return 0; }
  if (pendingStatus.commands_enabled === false) return clamp01(axisState.percent / 100);

  const axisConfig = machineConfig.axes[axisId];
  if (!axisConfig || !axisConfig.max_steps) {
    return clamp01(axisState.percent / 100);
  }
  return clamp01(axisState.steps / axisConfig.max_steps);
}

function clamp01(v) { return Math.max(0, Math.min(1, v)); }
function lerp(a, b, t) { return a + (b - a) * t; }

/* ============================================================
   RENDER LOOP
   ============================================================ */

/** Begin the loop. Frames are only actually drawn when something changed. */
export function start() {
  resize();
  requestRender();
  renderer.setAnimationLoop(tick);
}

/** Stop drawing completely while the optional 3D panel is disabled. */
export function stop() {
  if (renderer) { renderer.setAnimationLoop(null); }
}

function tick(now) {
  // Damping keeps the camera drifting for a moment after the user lets go.
  const settling = interacting || now < interactingUntil;

  // applyStatus() must run every tick, not only when a frame is due, so that
  // a change is noticed the moment it arrives.
  const moved = applyStatus();

  if (!dirty && !moved && !settling) { return; }

  if (settling) { controls.update(); }   // required for damping

  renderer.render(scene, camera);
  dirty = false;
}

// The size the canvas was last set to. Dragging a window edge fires resize
// events far faster than frames are drawn, and reallocating the drawing buffer
// is not free - on a Pi least of all. Comparing first turns the repeats into
// nothing.
const lastSize = { width: 0, height: 0 };

function resize() {
  if (!container || !renderer) { return; }
  const width = container.clientWidth;
  const height = container.clientHeight;
  if (width === 0 || height === 0) { return; }
  if (width === lastSize.width && height === lastSize.height) { return; }

  lastSize.width = width;
  lastSize.height = height;

  renderer.setSize(width, height, false);
  camera.aspect = width / height;
  camera.updateProjectionMatrix();
  requestRender();
}

/** Reset the camera to its framing shot. */
export function resetView() {
  setView("overview");
}

/** Snap the camera to a face or diagonal selected on the view cube. */
export function setView(viewName) {
  if (!modelRoot) { return; }
  const preset = VIEW_PRESETS[viewName];
  if (!preset) { return; }

  frameCamera(
    modelRoot,
    new THREE.Vector3(...preset.direction),
    new THREE.Vector3(...preset.up)
  );
}
