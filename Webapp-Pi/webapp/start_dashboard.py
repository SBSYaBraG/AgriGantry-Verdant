"""Press ▶ Run Python File in VS Code to start or open this dashboard."""

import argparse
import json
import os
from http.client import HTTPConnection, HTTPException
from pathlib import Path
import subprocess
import sys
import time
import webbrowser

from config import SERVER_BUILD, SERVER_PORT


APP_DIR = Path(__file__).resolve().parent


def dashboard_on_port(port):
    """Return 'current', 'older' or 'free'. An unrelated listener is an error."""
    connection = HTTPConnection("127.0.0.1", port, timeout=1)
    try:
        # Some Windows setups time out when connecting to an unused port.
        # Once connected, a slow or invalid reply belongs to an existing service.
        try:
            connection.connect()
        except (ConnectionRefusedError, TimeoutError):
            return "free"
        connection.request("GET", "/api/config")
        response = connection.getresponse()
        if response.status == 200:
            settings = json.load(response)
            if (isinstance(settings, dict) and isinstance(settings.get("garden"), dict)
                    and {"axes", "bed_map", "motors"} <= settings.keys()):
                return "current" if settings.get("server_build") == SERVER_BUILD else "older"
    except (OSError, HTTPException, ValueError) as error:
        raise SystemExit(f"Could not check localhost port {port} ({error}).") from None
    finally:
        connection.close()

    raise SystemExit(f"Port {port} is in use by another service. Choose a different FARMBOT_PORT.")


def show_dashboard(port, open_tab):
    url = f"http://127.0.0.1:{port}/"
    print(f"AgriGantry dashboard ready: {url}", flush=True)
    if open_tab and (os.name == "nt" or sys.platform == "darwin" or os.getenv("DISPLAY") or os.getenv("WAYLAND_DISPLAY")):
        webbrowser.open_new_tab(url)


def main(open_tab=True):
    port = SERVER_PORT
    service = dashboard_on_port(port)
    while service == "older" and port < 65535:
        port += 1
        service = dashboard_on_port(port)
    if service == "current":
        show_dashboard(port, open_tab)
        return 0
    if service == "older":
        raise SystemExit("An earlier simulation occupies this port. Set FARMBOT_PORT to an unused port.")

    # VS Code may have selected the repository or system Python instead.
    venv_python = APP_DIR / ".venv" / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    python = venv_python if venv_python.is_file() else sys.executable
    child = subprocess.Popen([python, "-u", APP_DIR / "run.py"], cwd=APP_DIR,
                             env={**os.environ, "FARMBOT_PORT": str(port)})
    try:
        deadline = time.monotonic() + 15
        while dashboard_on_port(port) != "current":
            if child.poll() is not None or time.monotonic() >= deadline:
                raise SystemExit(f"The dashboard did not start on port {port}. Check the server output above.")
            time.sleep(0.15)
        show_dashboard(port, open_tab)
        return child.wait()
    except KeyboardInterrupt:
        return 0
    finally:
        if child.poll() is None:
            child.terminate()
            child.wait()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-browser", action="store_true", help="Print the verified URL without opening a tab.")
    sys.exit(main(open_tab=not parser.parse_args().no_browser))
