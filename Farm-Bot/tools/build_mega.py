"""Build a Mega 2560 HEX using an installed Arduino AVR core, without uploading.

Usage: python Farm-Bot/tools/build_mega.py
Requires the Arduino AVR board package (or use the project's PlatformIO build).
All generated files stay in this repository's ignored .build directory.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "Farm-Bot"
ARDUINO = Path(os.environ["LOCALAPPDATA"]) / "Arduino15/packages/arduino"


def latest(directory):
    versions = sorted(directory.iterdir(), key=lambda p: tuple(
        int(part) if part.isdigit() else 0 for part in p.name.replace('-', '.').split('.')))
    if not versions:
        raise SystemExit(f"No installed Arduino package found in {directory}")
    return versions[-1]


def main():
    package = latest(ARDUINO / "hardware/avr")
    compiler = latest(ARDUINO / "tools/avr-gcc") / "bin"
    core = package / "cores/arduino"
    variant = package / "variants/mega"
    library = PROJECT / "lib/Motors_X"
    output = ROOT / ".build/mega2560"
    output.mkdir(parents=True, exist_ok=True)
    flags = ["-Os", "-g", "-ffunction-sections", "-fdata-sections", "-flto",
             "-mmcu=atmega2560", "-DF_CPU=16000000L", "-DARDUINO=10819",
             "-DARDUINO_AVR_MEGA2560", "-DARDUINO_ARCH_AVR",
             f"-I{core}", f"-I{variant}", f"-I{library}"]
    sources = sorted(p for p in core.iterdir() if p.suffix in (".cpp", ".c", ".S"))
    sources += sorted(library.glob("*.cpp")) + [PROJECT / "src/main.cpp"]
    objects = []
    for source in sources:
        cpp = source.suffix == ".cpp"
        exe = compiler / ("avr-g++.exe" if cpp else "avr-gcc.exe")
        extra = ["-std=gnu++11", "-fpermissive", "-fno-exceptions", "-fno-threadsafe-statics"] if cpp else ["-std=gnu11"]
        if source.suffix == ".S":
            extra = ["-x", "assembler-with-cpp"]
        obj = output / f"{source.parent.name}_{source.name}.o"
        subprocess.run([str(exe), *flags, *extra, "-c", str(source), "-o", str(obj)], check=True)
        objects.append(str(obj))
    elf = output / "firmware.elf"
    subprocess.run([str(compiler / "avr-gcc.exe"), "-Os", "-flto", "-mmcu=atmega2560",
                    "-Wl,--gc-sections", "-o", str(elf), *objects, "-lm"], check=True)
    subprocess.run([str(compiler / "avr-objcopy.exe"), "-O", "ihex", "-R", ".eeprom",
                    str(elf), str(output / "firmware.hex")], check=True)
    subprocess.run([str(compiler / "avr-size.exe"), "-C", "--mcu=atmega2560", str(elf)], check=True)
    print(f"Build passed: {output / 'firmware.hex'} (not uploaded)")


if __name__ == "__main__":
    main()
