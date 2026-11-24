/* The simulator owns machine and garden state. Views change on status replies. */
(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const all = (selector) => Array.from(document.querySelectorAll(selector));
  const cfg = JSON.parse($("machine-config").textContent);
  const axes = Object.keys(cfg.axes);
  const spots = cfg.garden.spots;
  const crops = cfg.garden.crops;
  const motorNames = {XL: "Left X motor", XR: "Right X motor", Y: "Y motor", Z: "Z motor"};
  const stageNames = {empty: "Empty", planned: "Planned", positioning: "Positioning", growing: "Growing", ready: "Ready to harvest"};
  const socket = io({autoConnect: false});
  const arrows = all("[data-axis]");
  const homeButtons = all("[data-home]");
  const seedButtons = all("[data-crop]");
  const spotButtons = all("[data-spot]");
  const demos = all("[data-alarm], [data-notice]");
  const noticeButtons = Object.fromEntries(all("[data-notice]").map(b => [b.dataset.notice, b]));
  const tabs = all("[data-page]");
  const pages = {controls: "Gantry controls", garden: "Garden plan", tutorial: "Dashboard tutorial"};
  const topics = all("[data-guide-topic]");
  const conditionRows = all("[data-reading]");
  let guideTopic = null;
  let guideStep = 0;
  let guideReturn = "controls";
  let currentPage = "";
  let status = null;
  let receivedAt = 0;
  let awaitingSnapshot = true;
  let previousSnapshot = "";
  let armToken = null;
  let benchPending = false;
  const activePhases = ["armed", "queued", "starting", "running", "stopping"];
  const trajectoryPhases = ["starting", "running", "stopping"];
  let activeSpot = Object.keys(spots)[0];
  let pickedCrop = null;
  let targetKey = "";
  let alertKey = "";
  let gardenKey = "";
  let feedbackTimer;
  window.farmbotConfig = cfg;
  window.dispatchEvent(new CustomEvent("farmbot:config", {detail: cfg}));
  document.documentElement.style.setProperty("--bed-ratio",
    cfg.axes[cfg.bed_map.length].travel_mm / cfg.axes[cfg.bed_map.width].travel_mm);

  function navigate() {
    const [name, topicKey, step] = location.hash.slice(1).split("/");
    const page = Object.hasOwn(pages, name) ? name : "controls";
    const leavingGuide = currentPage === "tutorial" && page !== "tutorial";
    if (page === "tutorial" && currentPage && currentPage !== "tutorial") {
      guideReturn = currentPage === "garden" ? "garden/" + activeSpot : currentPage;
    }
    for (const id of Object.keys(pages)) $(id + "-page").hidden = id !== page;
    $("page-title").textContent = pages[page];
    tabs.forEach(link => {
      if (link.dataset.page === page) link.setAttribute("aria-current", "page");
      else link.removeAttribute("aria-current");
    });
    if (page === "tutorial") {
      $("guide-close").href = "#" + guideReturn;
      showGuide(topicKey, step);
    } else {
      if (page === "garden" && Object.hasOwn(spots, topicKey) && activeSpot !== topicKey) chooseSpot(topicKey, null);
      if (leavingGuide) tabs.find(link => link.dataset.page === page).focus();
    }
    currentPage = page;
    window.dispatchEvent(new CustomEvent("farmbot:page", {detail: page}));
  }
  window.addEventListener("hashchange", navigate);
  navigate();

  function showGuide(topicKey, step) {
    guideTopic = topics.find(topic => topic.dataset.guideTopic === topicKey);
    for (const topic of topics) topic.hidden = topic !== guideTopic;
    $("guide-index").hidden = Boolean(guideTopic);
    $("guide-reader").hidden = !guideTopic;
    $("guide-title").textContent = guideTopic?.dataset.title || "Choose a topic";
    if (!guideTopic) {
      if (currentPage) $("guide-title").focus();
      return;
    }
    const slides = guideTopic.querySelectorAll(".guide-slide");
    const selected = Number(step || 1);
    guideStep = Number.isInteger(selected) ? Math.max(0, Math.min(selected - 1, slides.length - 1)) : 0;
    slides.forEach((slide, index) => { slide.hidden = index !== guideStep; });
    $("guide-prev").disabled = guideStep === 0;
    $("guide-next").textContent = guideStep === slides.length - 1 ? "Finish topic" : "Next";
    if (currentPage) slides[guideStep].querySelector("h3").focus();
  }
  function turnGuidePage(delta) {
    if (!guideTopic) return;
    const next = guideStep + delta;
    if (next < 0) return;
    location.hash = next >= guideTopic.querySelectorAll(".guide-slide").length ? "tutorial"
      : "tutorial/" + guideTopic.dataset.guideTopic + "/" + (next + 1);
  }
  $("guide-prev").addEventListener("click", () => turnGuidePage(-1));
  $("guide-next").addEventListener("click", () => turnGuidePage(1));
  document.addEventListener("keydown", event => {
    if (currentPage !== "tutorial" || event.altKey || event.ctrlKey || event.metaKey) return;
    if (event.key === "Escape") {
      event.preventDefault();
      location.hash = guideReturn;
      return;
    }
    if (!guideTopic || !$("tutorial-page").contains(event.target) || !["ArrowLeft", "ArrowRight"].includes(event.key)) return;
    event.preventDefault();
    turnGuidePage(event.key === "ArrowLeft" ? -1 : 1);
  });

  function fresh() {
    return Boolean(socket.connected && !awaitingSnapshot && status && Date.now() - receivedAt < 3500);
  }

  function showFeedback(message) {
    $("feedback").textContent = message;
    $("feedback").hidden = false;
    clearTimeout(feedbackTimer);
    feedbackTimer = setTimeout(() => { $("feedback").hidden = true; }, 4500);
  }

  function request(event, data) {
    if (status?.commands_enabled !== false && socket.connected && (event === "emergency_stop" || fresh())) socket.emit(event, data);
  }

  function benchCommand(action) {
    if (!socket.connected) return;
    if (action !== "stop" && (benchPending || document.hidden)) return;
    const sid = socket.id;
    if (action !== "stop") benchPending = true;
    render();
    socket.timeout(3000).emit("paired_x", {action, token: armToken}, (timeout, reply) => {
      if (socket.id !== sid) return;
      benchPending = false;
      if (timeout || !reply?.ok) showFeedback(reply?.error || "No acknowledgement. Check controller status.");
      if (action === "arm" && reply?.ok) armToken = reply.token;
      if (action === "stop") armToken = null;
      render();
    });
  }
  $("paired-arm").addEventListener("click", () => benchCommand("arm"));
  $("paired-move").addEventListener("click", () => benchCommand("move"));
  $("paired-stop").addEventListener("click", () => benchCommand("stop"));
  setInterval(() => {
    if (!armToken || !socket.connected) return;
    if (document.hidden || currentPage !== "controls") {
      benchCommand("stop");
      armToken = null;
    } else socket.volatile.emit("paired_x", {action: "watch", token: armToken});
  }, 350);
  document.addEventListener("visibilitychange", () => {
    if (document.hidden && armToken) { benchCommand("stop"); armToken = null; }
  });

  $("scan-drivers").addEventListener("click", () => {
    $("scan-drivers").disabled = true;
    socket.timeout(4000).emit("scan_drivers", {}, (timeout, reply) => {
      if (timeout || !reply?.ok) showFeedback(reply?.error || "Scan acknowledgement unavailable. Check the latest driver report.");
    });
  });

  function preview(action, fields = {}, onSuccess) {
    if (!fresh() || status.mode !== "simulated") return;
    socket.timeout(5000).emit("garden_preview", {action, ...fields}, (timeout, reply) => {
      if (timeout || !reply) return showFeedback("No confirmation received. Check the current spot before retrying.");
      if (!reply.ok) return showFeedback(reply.error);
      if (onSuccess) onSuccess();
    });
  }

  arrows.forEach(button => button.addEventListener("click", () => {
    request("move_relative", {axis: button.dataset.axis, mm: Number(button.dataset.sign) * Number($("jog-distance").value)});
  }));
  homeButtons.forEach(button => button.addEventListener("click", () => request("home_axis", {axis: button.dataset.home})));
  $("btn-home-all").addEventListener("click", () => request("home_all"));
  $("btn-estop").addEventListener("click", () =>
    status?.paired_x ? benchCommand("stop") : request("emergency_stop"));
  $("btn-resume").addEventListener("click", () => request("resume"));
  $("clear-alerts").addEventListener("click", () => preview("clear_alerts"));
  $("jump-alerts").addEventListener("click", () => {
    $("alerts-panel").scrollIntoView({block: "center"});
    $("alerts-title").focus({preventScroll: true});
  });

  function triggerNotice(action, spot) {
    if (["harvest_ready", "low_moisture", "demo_plant"].includes(action)) {
      const crop = status.garden.spots[spot].crop;
      const label = (crops[crop] || "Radish") + " in " + spot;
      const messages = {harvest_ready: label + " is ready to harvest.", low_moisture: label + " needs water.", demo_plant: label + " is growing in the demo."};
      preview(action, {spot, crop}, () => showFeedback(messages[action]));
    } else {
      preview(action, {}, () => showFeedback(action === "high_temperature" ? "High temperature is active for the whole bed." : "Sample bed readings are on."));
    }
  }
  demos.forEach(button => button.addEventListener("click", () => {
    if (button.dataset.alarm) return request("inject_alm", {motor: button.dataset.alarm});
    triggerNotice(button.dataset.notice, activeSpot);
  }));
  $("demo-spot").addEventListener("change", () => chooseSpot($("demo-spot").value, null));

  function updateSelection() {
    seedButtons.forEach(button => button.setAttribute("aria-pressed", String(button.dataset.crop === pickedCrop)));
    $("seed-hint").textContent = pickedCrop ? "Choose a spot for " + crops[pickedCrop].toLowerCase() + "."
      : "Drag a seed to a spot, or select it and tap the spot.";
  }
  seedButtons.forEach(button => {
    const crop = button.dataset.crop;
    button.addEventListener("click", () => {
      pickedCrop = pickedCrop === crop ? null : crop;
      updateSelection();
    });
    button.addEventListener("dragstart", event => {
      if (!fresh() || button.disabled) { event.preventDefault(); return; }
      event.dataTransfer.setData("text/plain", crop);
      event.dataTransfer.effectAllowed = "copy";
    });
  });

  function chooseSpot(id, crop) {
    activeSpot = id;
    $("demo-spot").value = id;
    if (location.hash.split("/")[0] === "#garden" && location.hash !== "#garden/" + id) {
      window.history.replaceState(null, "", "#garden/" + id);
    }
    render();
    if (!crop) return;
    preview("plan", {spot: id, crop}, () => {
      pickedCrop = null;
      updateSelection();
      showFeedback(crops[crop] + " planned for " + id + ".");
    });
  }

  spotButtons.forEach(button => {
    const id = button.dataset.spot;
    button.addEventListener("click", () => chooseSpot(id, pickedCrop));
    button.addEventListener("dragover", event => {
      if (!fresh() || status.mode !== "simulated") return;
      event.preventDefault();
      event.dataTransfer.dropEffect = "copy";
      button.classList.add("drag-over");
    });
    button.addEventListener("dragleave", () => button.classList.remove("drag-over"));
    button.addEventListener("drop", event => {
      event.preventDefault();
      button.classList.remove("drag-over");
      const crop = event.dataTransfer.getData("text/plain");
      if (Object.hasOwn(crops, crop)) chooseSpot(id, crop);
    });
  });

  $("placement-form").noValidate = true;
  $("placement-form").addEventListener("submit", event => {
    event.preventDefault();
    if ($("place-seed").disabled) return;
    const form = event.currentTarget;
    if (!form.checkValidity()) {
      form.querySelector("details").open = true;
      form.reportValidity();
      return;
    }
    const target = Object.fromEntries(axes.map(id => [id, $("target-" + id).valueAsNumber]));
    preview("place", {spot: activeSpot, crop: status.garden.spots[activeSpot].crop, target});
  });
  $("remove-seed").addEventListener("click", () => {
    const plant = status.garden.spots[activeSpot];
    preview("remove", {spot: activeSpot, crop: plant.crop});
  });
  $("mark-watered").addEventListener("click", () => {
    preview("mark_watered", {spot: activeSpot, crop: status.garden.spots[activeSpot].crop});
  });
  $("mark-ready").addEventListener("click", () => triggerNotice("harvest_ready", activeSpot));

  function plantTasks(beds) {
    const tasks = [];
    for (const [spot, plant] of Object.entries(beds || {})) {
      if (plant.stage === "ready") tasks.push({spot, title: crops[plant.crop] + " ready to harvest", tone: "info"});
      if (plant.needs_water) tasks.push({spot, title: crops[plant.crop] + " needs water", tone: "warning"});
    }
    return tasks;
  }

  function renderGarden(online, canMove, tasks) {
    const garden = status?.garden;
    const available = online && status.mode === "simulated" && Boolean(garden);
    spotButtons.forEach(button => { button.disabled = !garden; });
    seedButtons.forEach(b => { b.disabled = !available; });
    demos.forEach(b => { b.disabled = !available; });
    $("demo-tools").hidden = status?.mode !== "simulated" || !garden;
    $("demo-spot").disabled = !available;
    $("clear-alerts").disabled = !available || !(garden.notices.length || status.alm_critical || Object.values(garden.spots).some(p => p.needs_water));
    if (!garden) {
      if (status && status.mode !== "simulated") {
        $("bed-total").textContent = "No planting data";
        $("plant-help").textContent = "Planting data is unavailable from this controller.";
        $("selected-stage").textContent = "No reading";
        spotButtons.forEach(button => { button.querySelector(".spot-crop").textContent = "No reading"; });
      }
      $("place-seed").disabled = true;
      $("remove-seed").disabled = true;
      $("mark-watered").disabled = true;
      $("mark-ready").disabled = true;
      axes.forEach(id => { $("target-" + id).disabled = true; });
      return;
    }

    const key = JSON.stringify([garden.spots, activeSpot]);
    if (key !== gardenKey) {
      gardenKey = key;
      spotButtons.forEach(button => {
        const id = button.dataset.spot;
        const plant = garden.spots[id];
        const art = button.querySelector(".spot-art");
        button.setAttribute("aria-pressed", String(id === activeSpot));
        const name = crops[plant.crop] || "Empty";
        const count = tasks.filter(task => task.spot === id).length;
        const badge = button.querySelector(".spot-count");
        badge.hidden = !count;
        badge.textContent = count;
        button.setAttribute("aria-label", ["Spot " + id, name, stageNames[plant.stage], count ? count + (count === 1 ? " task" : " tasks") : ""].filter(Boolean).join(", "));
        $("demo-spot").querySelector('option[value="' + id + '"]').textContent = id + " · " + name;
        art.className = "spot-art" + (plant.crop ? " crop-art crop-" + plant.crop : "");
        art.textContent = plant.crop ? "" : "+";
        button.querySelector(".spot-crop").textContent = name;
        button.querySelector(".spot-stage").textContent = plant.crop ? stageNames[plant.stage] : "";
      });
      const used = Object.values(garden.spots).filter(plant => plant.crop).length;
      $("bed-total").textContent = used + " / " + spotButtons.length + " spots in use";
    }

    // The bed tile, demo selector and spot actions share one selection.
    if ($("demo-spot").value !== activeSpot) $("demo-spot").value = activeSpot;
    const selected = garden.spots[activeSpot];
    const canPreviewCare = available && cfg.garden.plant_tasks === true;
    const canNotify = canPreviewCare && ["growing", "ready"].includes(selected?.stage);
    const addDemo = noticeButtons.demo_plant;
    addDemo.hidden = !canPreviewCare || !["empty", "planned"].includes(selected?.stage);
    addDemo.disabled = addDemo.hidden;
    addDemo.textContent = selected?.stage === "planned" ? "Preview growing " + crops[selected.crop].toLowerCase() : "Add demo radish";
    noticeButtons.harvest_ready.disabled = !canNotify || selected.stage === "ready";
    noticeButtons.low_moisture.disabled = !canNotify || selected.needs_water;
    noticeButtons.sample_conditions.disabled = !canPreviewCare;
    const hint = $("demo-plant-hint");
    hint.hidden = !available || canNotify;
    if (!hint.hidden) hint.textContent = !canPreviewCare ? "This session uses an earlier simulator. Press Run in start_dashboard.py to open the current dashboard."
      : selected?.stage === "empty" ? "Add a demo plant here to try a task immediately."
      : selected?.stage === "planned" ? "Preview this seed as grown to try a task without moving the gantry." : "Wait for placement to finish.";

    const plant = garden.spots[activeSpot];
    const planned = plant.stage === "planned";
    const homed = axes.every(id => status.axes[id].homed);
    const idle = !garden.active && axes.every(id => !status.axes[id].moving && !status.axes[id].homing);
    $("placement-title").textContent = "Spot " + activeSpot;
    $("selected-stage").textContent = stageNames[plant.stage];
    $("plant-name").textContent = crops[plant.crop] || "Choose a seed";
    const help = {empty: "Select a seed, then tap this spot.", positioning: "The simulator is moving to the target.",
      growing: status.mode === "simulated" ? "You can mark this plant ready immediately in the simulation." : "This plant is growing.",
      ready: "Mark harvested to free this spot."};
    let message = help[plant.stage];
    if (planned) {
      if (!online) message = "Waiting for a fresh update.";
      else if (!canMove) message = "Clear the stop or motor alarm before placing.";
      else if (!homed) message = "Home X, Y and Z before placing.";
      else if (!idle) message = "Wait for the current move to finish.";
      else message = "Place seed moves the simulator to this spot’s target.";
    }
    $("plant-help").textContent = plant.needs_water ? "Soil moisture is low here. Mark watered after tending this plant." : message;
    $("mark-watered").hidden = !plant.needs_water;
    $("mark-watered").disabled = !available || !plant.needs_water;
    $("mark-ready").hidden = plant.stage !== "growing" || status.mode !== "simulated";
    $("mark-ready").disabled = !canPreviewCare || plant.stage !== "growing";
    $("place-seed").hidden = !planned && plant.stage !== "empty";
    $("remove-seed").hidden = !planned && plant.stage !== "ready";
    $("place-seed").disabled = !(available && planned && canMove && homed && idle);
    $("remove-seed").disabled = !(available && (planned || plant.stage === "ready"));
    $("remove-seed").textContent = plant.stage === "ready" ? "Mark harvested" : "Remove plan";

    const formKey = JSON.stringify([activeSpot, plant.crop, plant.target]);
    axes.forEach(id => {
      const field = $("target-" + id);
      field.disabled = !(available && planned);
      if (targetKey !== formKey) {
        field.min = spots[activeSpot].min[id];
        field.max = spots[activeSpot].max[id];
        field.value = plant.target[id];
        field.title = field.min + "–" + field.max + " mm";
      }
    });
    targetKey = formKey;
  }

  function tabCount(id, count, unit) {
    const badge = $(id + "-count");
    badge.hidden = count === 0;
    badge.textContent = count;
    const label = id === "garden" ? "Garden" : "Controls";
    badge.parentElement.setAttribute("aria-label", count ? label + ", " + count + " " + unit + (count === 1 ? "" : "s") : label);
  }

  function renderConditions(online) {
    const readings = status?.bed_readings || {};
    const simulated = status?.mode === "simulated";
    const dry = Object.values(status?.garden?.spots || {}).filter(plant => plant.needs_water).length;
    const hot = status?.garden?.notices?.includes("high_temperature");
    let hasReading = false;
    for (const row of conditionRows) {
      const {reading: field, unit, low, high} = row.dataset;
      const value = readings[field];
      const measured = Number.isFinite(value);
      hasReading ||= measured;
      row.querySelector(".reading-value").textContent = measured ? value.toLocaleString() + unit : "No reading";
      let label = "Unknown", state = "unknown", reason = "No current rating";
      if (!online && status) {
        label = "Offline";
        reason = "Waiting for fresh readings";
      } else if (online && field === "soil_moisture_pct" && dry) {
        state = "warning";
        label = dry === 1 ? "1 dry spot" : dry + " dry spots";
        reason = "Open the plant alert to see which spot needs water";
      } else if (online && field === "temperature_c" && hot) {
        state = "warning";
        label = "High";
        reason = "The high-temperature demo is active";
      } else if (online && simulated && measured) {
        const inRange = value >= Number(low) && value <= Number(high);
        state = inRange ? "good" : "warning";
        label = inRange ? "Good" : value < Number(low) ? "Low" : "High";
        reason = "Demo range " + low + "–" + high + unit;
      }
      const badge = row.querySelector(".reading-status");
      badge.dataset.state = state;
      badge.title = reason;
      badge.querySelector("i").textContent = {good: "✓", warning: "⚠", unknown: "—"}[state];
      badge.querySelector("span").textContent = label;
    }
    $("reading-source").hidden = !simulated || !hasReading;
  }

  function renderAlerts(online, tasks) {
    const items = [];
    const notices = status?.garden?.notices || [];
    const bench = status?.paired_x;
    if (fresh() && trajectoryPhases.includes(status?.paired_x?.phase)) {
      items.push(["info", "Supervised X test", "Waiting for the STM32 to report measured travel and driver status."]);
    } else if (fresh() && status?.commands_enabled === false && !status.hardware_connected) {
      items.push(["warning", "Controller offline", status.error || "Waiting for a new serial reply."]);
    } else if (!online) {
      const lastSeen = receivedAt ? "Last update at " + new Date(receivedAt).toLocaleTimeString() + "." : "Waiting for the first update.";
      items.push(["warning", receivedAt ? "Connection lost" : "Waiting for connection", lastSeen]);
    }
    if (status?.emergency_stop) items.push(["critical", "Stop active", "Clear stop to allow new commands."]);
    const result = bench?.result;
    const reviewX = bench?.phase === "finished" && result && !result.verified;
    if (bench?.phase === "unknown") items.push(["warning", "X test result pending", bench.detail]);
    if (reviewX) {
      const title = result.run === "STOPPED" ? "X test stopped" : "Check X test result";
      const detail = result.run === "DONE" ? "The encoder or driver evidence fell outside the expected range." :
        "Controller reported " + result.run + " after " + result.steps + " pulses.";
      items.push([result.run === "STOPPED" ? "info" : "warning", title, detail]);
    }
    const faulted = Object.keys(status?.alm || {}).filter(motor => status.alm[motor]);
    for (const motor of faulted) {
      items.push(["critical", (motorNames[motor] || motor) + " alarm", "ALM " + motor + " · All axes are stopped."]);
    }
    if (notices.includes("high_temperature")) items.push(["warning", "Bed temperature is high", "Garden bed · demo reading"]);
    for (const task of tasks) {
      items.push([task.tone, task.title, "Spot " + task.spot, task.spot]);
    }
    const key = JSON.stringify(items);
    if (key === alertKey) return;
    alertKey = key;
    tabCount("controls", faulted.length + (reviewX && result.run !== "STOPPED" ? 1 : 0), "item to check");
    tabCount("garden", tasks.length, "plant task");
    $("alert-count").textContent = items.length;
    $("jump-alerts").hidden = !items.length;
    $("jump-alerts").textContent = items.length + (items.length === 1 ? " alert" : " alerts");
    if (!items.length && status?.commands_enabled === false && status.alm_critical == null)
      items.push(["info", "Alarm state unavailable", "The current status report does not include motor alarms."]);
    else if (!items.length) items.push(["info", "All clear", "No active alerts."]);
    const nodes = items.map(([tone, title, detail, spot]) => {
      const node = document.createElement("div");
      node.className = "alert-item " + tone;
      const label = document.createElement("b");
      label.textContent = title;
      const text = document.createElement("small");
      text.textContent = detail;
      node.append(label, text);
      if (spot) {
        const link = document.createElement("a");
        link.href = "#garden/" + spot;
        link.className = "alert-spot";
        link.textContent = "View spot " + spot + " →";
        link.addEventListener("click", event => {
          event.preventDefault();
          pickedCrop = null;
          updateSelection();
          chooseSpot(spot, null);
          location.hash = "garden/" + spot;
          navigate();
          $("placement-title").focus();
        });
        node.append(link);
      }
      return node;
    });
    $("alert-list").replaceChildren(...nodes);
  }

  function renderBench(online) {
    const bench = status?.paired_x;
    $("paired-panel").hidden = !bench;
    if (!bench) return;
    const active = activePhases.includes(bench.phase);
    $("paired-arm").disabled = benchPending || !online || !bench.supported || bench.phase !== "idle" ||
      status.scan_pending || status.encoder_x1 == null || !status.enables?.every(v => v === 0);
    $("paired-move").disabled = benchPending || !online || bench.phase !== "armed" || !armToken;
    $("paired-stop").disabled = !socket.connected || !active;
    $("paired-state").textContent = bench.detail;
    const r = bench.result;
    $("paired-result").textContent = r ? r.run + " · " + r.steps + " pulses · X1 encoder " +
      (r.encoder ?? "unavailable") + " · Index " + (r.index ?? "unavailable") : "";
    $("paired-details").hidden = !r;
    if (r) $("paired-drivers").replaceChildren(...Object.entries(r.drivers).map(([name, d]) => {
      const row = document.createElement("p");
      row.textContent = name + " · " + (d.configured ? "Configuration verified" : "Configuration incomplete") +
        " · CTRL1 " + d.ctrl1 + " · FAULT " + d.fault + " · DIAG2 " + d.diag2 + " · " + d.samples +
        " samples · Last register " + d.reg + " · HAL " + d.hal + " · Raw reply " + d.raw;
      return row;
    }));
  }

  function renderController(online) {
    const physical = status?.commands_enabled === false;
    $("controller-panel").hidden = !physical;
    if (!physical) return;
    const stm = status.mode === "stm32";
    const label = stm ? "STM32" : "Arduino";
    const time = status.sample_time ? new Date(status.sample_time).toLocaleTimeString() : null;
    const qualifier = trajectoryPhases.includes(status.paired_x?.phase) ? " · Last idle sample" : online ? "" : " · Last known";
    $("controller-sample").textContent = time
      ? label + " reply #" + status.sample_number + " at " + time + qualifier
      : "Waiting for the first " + label + " reply.";
    const values = [];
    if (stm && time) {
      values.push("X1 encoder: " + (status.encoder_x1 ?? "Unavailable") + " counts",
                  "MCU enables X/Y/Z: " + status.enables.join("/"), "Uptime: " + status.uptime_ms + " ms");
    } else if (time) {
      for (const [axis, value] of Object.entries(status.axes)) values.push(axis.toUpperCase() + ": " + value.steps + " steps");
    }
    $("controller-values").textContent = values.join(" · ");
    $("scan-drivers").hidden = $("scan-hint").hidden = !stm;
    $("scan-drivers").disabled = !online || status.scan_pending || activePhases.includes(status.paired_x?.phase) ||
      !status.enables?.every(value => value === 0);
    const scan = status.scan;
    const report = [];
    if (status.scan_pending) report.push("Checking driver SPI…");
    if (scan) {
      report.push("Last scan at " + new Date(scan.time).toLocaleTimeString());
      if (scan.error) report.push(scan.error);
      for (const [driver, row] of Object.entries(scan.drivers || {})) {
        report.push(driver + " · SPI " + row.spi + (row.first ? " · FAULT " + row.first + "/" + row.settled +
          " · status " + row.status_bits + " · CTRL1/2 " + row.ctrl1 + "/" + row.ctrl2 : "") +
          (row.pins ? " · nFAULT " + row.pins : ""));
      }
    }
    $("driver-report").replaceChildren(...report.map(line => {
      const p = document.createElement("p"); p.textContent = line; return p;
    }));
  }

  function render() {
    const serverOnline = fresh();
    const physical = status?.commands_enabled === false;
    const online = serverOnline && (!physical || status.hardware_connected);
    const canMove = online && !physical && !status.emergency_stop && !status.alm_critical;
    const placing = Boolean(status?.garden?.active);
    $("conn-text").textContent = online ? "Connected" : serverOnline && physical ? "Controller offline" : receivedAt ? "Offline" : "Connecting…";
    $("conn-text").classList.toggle("connected", online);
    if (status) $("mode-badge").textContent = {simulated: "Simulation", stm32: "STM32", arduino: "Arduino"}[status.mode] || status.mode;
    const inBench = activePhases.includes(status?.paired_x?.phase);
    const inTrajectory = trajectoryPhases.includes(status?.paired_x?.phase);
    if (serverOnline && inTrajectory) $("conn-text").textContent = "X test in progress";
    $("btn-estop").disabled = !status || (physical && !inBench) || !socket.connected || (online && status.emergency_stop);
    $("btn-resume").disabled = physical || !online || !status.emergency_stop;
    $("btn-home-all").disabled = !canMove || placing || axes.some(id => status.axes[id].homing);
    homeButtons.forEach(b => { b.disabled = !canMove || placing || status.axes[b.dataset.home].homing; });
    arrows.forEach(b => { b.disabled = !canMove || placing || !status.axes[b.dataset.axis].homed; });
    if (status) {
      for (const id of axes) {
        const axis = status.axes[id];
        $("position-" + id).textContent = !online && physical ? "Offline" : axis?.homed ? axis.percent + "%" :
          physical ? "Unreferenced" : axis?.homing ? "Homing…" : "Home needed";
        $("bar-" + id).style.width = (axis?.homed ? axis.percent : 0) + "%";
      }
      const along = cfg.bed_map.length, across = cfg.bed_map.width;
      const pct = id => physical ? status.axes[id].percent : Math.max(0, Math.min(100, 100 * status.axes[id].steps / cfg.axes[id].max_steps));
      $("mini-gantry").hidden = !status.axes[along]?.homed || !status.axes[across]?.homed || (physical && !online);
      if (!$("mini-gantry").hidden) {
        $("mini-gantry").style.left = pct(along) + "%";
        $("mini-tool").style.top = pct(across) + "%";
      }
      const unknownPose = physical && (!online || axes.some(id => !status.axes[id]?.homed));
      $("scene-frame").classList.toggle("unmeasured-pose", unknownPose);
      $("plan-status").textContent = unknownPose ? "Model preview · Position unavailable" : !online ? "Last known bed view" :
        $("mini-gantry").hidden ? "Home X and Y to locate the tool" : "Bed overview";
    } else $("mini-gantry").hidden = true;
    const tasks = plantTasks(status?.garden?.spots);
    renderBench(online);
    renderController(online);
    renderGarden(online, canMove, tasks);
    renderAlerts(online, tasks);
    renderConditions(online);
  }

  function appendLog(line) {
    const row = document.createElement("div");
    row.textContent = line;
    $("log").append(row);
    while ($("log").childElementCount > 160) $("log").firstElementChild.remove();
    $("log").scrollTop = $("log").scrollHeight;
  }
  socket.on("status_update", value => {
    const wasFresh = fresh();
    const signature = JSON.stringify(value);
    status = value;
    if (!activePhases.includes(value.paired_x?.phase)) armToken = null;
    receivedAt = Date.now();
    awaitingSnapshot = false;
    if (wasFresh && signature === previousSnapshot) return;
    previousSnapshot = signature;
    window.farmbotStatus = status;
    window.dispatchEvent(new CustomEvent("farmbot:status", {detail: status}));
    render();
  });
  socket.on("log", data => (data.lines || []).forEach(appendLog));
  socket.on("connect", render);
  socket.on("disconnect", () => {
    armToken = null;
    benchPending = false;
    awaitingSnapshot = true;
    render();
    appendLog("Connection lost. Waiting for fresh machine state.");
  });
  setInterval(() => { if (!fresh()) render(); }, 1000);
  render();
  socket.connect();
})();
