"""Compile and run the real firmware with fake pins/serial using MSVC on Windows."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "Farm-Bot"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--standalone", action="store_true", help="Test the single-file Arduino sketch")
    args = parser.parse_args()
    environment = {key.upper(): value for key, value in os.environ.items()}
    if not shutil.which("cl"):
        installs = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Microsoft Visual Studio"
        scripts = sorted(installs.glob("*/**/VC/Auxiliary/Build/vcvars64.bat"))
        if not scripts:
            raise SystemExit("Install MSVC C++ build tools or run from a Visual Studio developer prompt.")
        result = subprocess.run(f'"{scripts[-1]}" >nul && set', shell=True,
                                capture_output=True, text=True, check=True)
        for line in result.stdout.splitlines():
            if "=" in line:
                key, value = line.split("=", 1)
                if key:
                    environment[key.upper()] = value
    compiler = shutil.which("cl.exe", path=environment["PATH"])
    if compiler is None:
        raise SystemExit("MSVC environment did not provide cl.exe")
    sources = [*sorted((PROJECT / "lib/Motors_X").glob("*.cpp")),
               PROJECT / "src/main.cpp", PROJECT / "test/native/test_controller.cpp"]
    if args.standalone:
        sources = [ROOT / "Arduino/AgriGantry/AgriGantry.ino", PROJECT / "test/native/test_controller.cpp"]
    for name, extra in (("default", []), ("configured", ["/DX_TRAVEL_STEPS=200L"])):
        output = ROOT / ".build" / f"native-{'standalone-' if args.standalone else ''}{name}"
        output.mkdir(parents=True, exist_ok=True)
        executable = output / "test_controller.exe"
        subprocess.run([compiler, "/nologo", "/TP", "/EHsc", "/std:c++14", "/W4", "/WX",
                        "/D_CRT_SECURE_NO_WARNINGS", f"/I{PROJECT / 'test/native'}",
                        f"/I{PROJECT / 'lib/Motors_X'}", *extra, *map(str, sources),
                        f"/Fe:{executable}"], cwd=output, env=environment, check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
