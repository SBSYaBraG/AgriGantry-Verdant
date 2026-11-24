"""Upload this dashboard over SSH and start it on the Pi. Run on the laptop."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import signal
import socket
import subprocess
import sys
import tarfile
import tempfile
import time
from urllib.request import urlopen


APP = Path(__file__).resolve().parents[1]
RUNNERS = {"stm32": "run_stm32.py", "paired-x": "run_stm32.py", "arduino": "run_arduino.py", "simulated": "run.py"}
FILES = ["app.py", "config.py", "requirements.txt", *sorted(set(RUNNERS.values())), "tools/host_on_pi.py"]


def upload(host, mode):
    files = [APP / f for f in FILES] + list((APP / "hardware").glob("*.py"))
    for directory in ("static", "templates"):
        files += [p for p in (APP / directory).rglob("*") if p.is_file()]
    digest = hashlib.sha256()
    with tempfile.TemporaryFile() as payload:
        with tarfile.open(fileobj=payload, mode="w:gz") as archive:
            for path in sorted(files):
                name = path.relative_to(APP).as_posix()
                digest.update(name.encode() + b"\0" + hashlib.sha256(path.read_bytes()).digest())
                archive.add(path, arcname=name, recursive=False)
        payload.seek(0)
        version = digest.hexdigest()
        remote = (
            'set -eu; umask 077; base="$HOME/.local/share/agrigantry-gabriel"; '
            f'stage="$base/release.{version}"; mkdir -p "$stage"; '
            '(flock -x 9; if test -f "$stage/.complete"; then cat >/dev/null; '
            'else tar -xzf - -C "$stage" && touch "$stage/.complete"; fi) 9>"$base/upload.lock"; '
            'cd "$stage"; exec python3 tools/host_on_pi.py '
            + shlex.join(["--start-upload", version, "--profile", mode])
        )
        print(f"Uploading the {mode} dashboard to {host} via SSH.", flush=True)
        return subprocess.run(["ssh", "-T", "-o", "ConnectTimeout=8", "--", host, remote], stdin=payload).returncode


def read_api(port, route):
    with urlopen(f"http://127.0.0.1:{port}/api/{route}", timeout=1) as response:
        return json.load(response)


def profile_matches(snapshot, mode):
    if mode == "paired-x":
        return snapshot["mode"] == "stm32" and snapshot.get("paired_x") is not None
    return snapshot["mode"] == mode and snapshot.get("paired_x") is None


def owns_server(state, base):
    if not state:
        return False
    process = Path("/proc") / str(state["pid"])
    try:
        directory = (process / "cwd").resolve(strict=True)
        argv = (process / "cmdline").read_bytes().split(b"\0")
        runner = directory / RUNNERS[state["profile"]]
        return directory.parent == base and directory.name.startswith("release.") and os.fsencode(runner) in argv
    except (OSError, KeyError):
        return False


def free_port():
    for port in range(5000, 5011):
        with socket.socket() as probe:
            probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                probe.bind(("", port))
                return port
            except OSError:
                continue
    raise RuntimeError("Pi ports 5000 through 5010 are occupied.")


def report(port):
    address = os.environ["SSH_CONNECTION"].split()[2]
    host = f"[{address}]" if ":" in address else address
    print(f"Phone dashboard: http://{host}:{port}/", flush=True)
    print(f"Controller status: http://{host}:{port}/api/status", flush=True)


def start_upload(mode, digest):
    import fcntl

    base = Path.home() / ".local/share/agrigantry-gabriel"
    if APP.parent != base or not APP.name.startswith("release."):
        raise RuntimeError("Start the upload from the laptop launcher.")
    state_file = base / "server.json"
    with (base / "deploy.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        old = json.loads(state_file.read_text()) if state_file.exists() else {}
        running = owns_server(old, base)
        if running and old["profile"] == mode and old["digest"] == digest:
            try:
                if profile_matches(read_api(old["port"], "status"), mode):
                    report(old["port"])
                    return
            except (OSError, ValueError, KeyError):
                pass  # A failed readiness check is repaired by restarting this dashboard.

        python = base / "venv/bin/python"
        if not python.exists():
            subprocess.run([sys.executable, "-m", "venv", "--system-site-packages", str(base / "venv")], check=True)
        subprocess.run([str(python), "-m", "pip", "install", "--disable-pip-version-check", "--no-input",
                        "--prefer-binary", "-r", str(APP / "requirements.txt")], check=True)
        build = subprocess.check_output([str(python), "-c", "import config; print(config.SERVER_BUILD)"],
                                        cwd=APP, text=True).strip()
        if running:
            os.kill(old["pid"], signal.SIGTERM)
            for _ in range(40):
                if not owns_server(old, base):
                    break
                time.sleep(0.1)
            else:
                raise RuntimeError("The previous Gabriel dashboard is still shutting down.")
        port = free_port()
        log_path = APP / "server.log"
        with log_path.open("ab") as log:
            runner = [str(python), "-u", str(APP / RUNNERS[mode])]
            if mode == "paired-x":
                runner.append("--paired-x")
            child = subprocess.Popen(runner, cwd=APP,
                                     env={**os.environ, "FARMBOT_PORT": str(port)},
                                     stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True)
        try:
            for _ in range(75):
                if child.poll() is not None:
                    raise RuntimeError(f"Dashboard exited. {log_path.read_text(errors='replace')[-1000:]}")
                try:
                    if read_api(port, "config")["server_build"] == build and profile_matches(read_api(port, "status"), mode):
                        break
                except OSError:
                    pass
                time.sleep(0.2)
            else:
                raise RuntimeError(f"Dashboard did not become ready. See {log_path}.")
            temporary = state_file.with_suffix(".new")
            temporary.write_text(json.dumps(dict(pid=child.pid, port=port, profile=mode, digest=digest)))
            temporary.replace(state_file)
        except BaseException:
            if child.poll() is None:
                child.terminate()
            raise
        report(port)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", nargs="?", default="pi@farm.local")
    parser.add_argument("--profile", choices=RUNNERS, default="stm32")
    parser.add_argument("--start-upload", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.start_upload:
        start_upload(args.profile, args.start_upload)
    else:
        sys.exit(upload(args.host, args.profile))
