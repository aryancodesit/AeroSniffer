"""Arduino environment provider: cores, libraries, boards, variants, SDK config."""

import subprocess
import shutil
from pathlib import Path
from . import EvidenceProvider


class ArduinoProvider(EvidenceProvider):
    @property
    def name(self): return "arduino"

    @property
    def description(self): return "Arduino cores, libraries, boards.txt, variant, partitions, platform.txt"

    @property
    def priority(self) -> int:
        return 10

    def collect(self, evidence_dir, build_dir, arduino15):
        files = []
        deps_dir = evidence_dir / "deps"
        deps_dir.mkdir(parents=True, exist_ok=True)

        for cmd, name in [
            (["arduino-cli", "core", "list"], "core_list.txt"),
            (["arduino-cli", "lib", "list"], "lib_list.txt"),
        ]:
            try:
                out = subprocess.run(cmd, capture_output=True, stderr=subprocess.DEVNULL, text=True, errors="replace", timeout=15)
                (deps_dir / name).write_text(out.stdout)
            except Exception:
                (deps_dir / name).write_text("(unavailable)\n")
            files.append(deps_dir / name)

        boards_file = arduino15 / "packages" / "esp32" / "hardware" / "esp32" / "3.1.0" / "boards.txt"
        if boards_file.exists():
            import re
            text = boards_file.read_text(encoding="utf-8", errors="replace")
            lines = [l for l in text.splitlines() if l.startswith("XIAO_ESP32S3.")]
            (deps_dir / "boards.txt.extract").write_text("\n".join(lines) + "\n", encoding="utf-8")
            files.append(deps_dir / "boards.txt.extract")

        variant_dir = arduino15 / "packages" / "esp32" / "hardware" / "esp32" / "3.1.0" / "variants" / "XIAO_ESP32S3"
        pins_h = variant_dir / "pins_arduino.h"
        if pins_h.exists():
            shutil.copy2(pins_h, deps_dir / "pins_arduino.h")
            files.append(deps_dir / "pins_arduino.h")

        partitions = variant_dir / "partitions-8MB.csv"
        if not partitions.exists():
            partitions = variant_dir / "partitions.csv"
        if partitions.exists():
            shutil.copy2(partitions, deps_dir / "partitions.csv")
            files.append(deps_dir / "partitions.csv")

        for candidate in [
            Path.home() / "Arduino" / "libraries" / "TFT_eSPI" / "User_Setup.h",
            Path.home() / "Documents" / "Arduino" / "libraries" / "TFT_eSPI" / "User_Setup.h",
        ]:
            if candidate.exists():
                shutil.copy2(candidate, deps_dir / "User_Setup.h")
                files.append(deps_dir / "User_Setup.h")
                break

        hw_dir = arduino15 / "packages" / "esp32" / "hardware" / "esp32" / "3.1.0"
        platform_txt = hw_dir / "platform.txt"
        if platform_txt.exists():
            text = platform_txt.read_text(encoding="utf-8", errors="replace")
            variant_lines = []
            in_section = False
            for line in text.splitlines():
                if line.startswith("# XIAO_ESP32S3") or line.startswith("XIAO_ESP32S3."):
                    in_section = True
                if in_section:
                    if line.startswith("# ") and not line.startswith("# XIAO_ESP32S3"):
                        break
                    variant_lines.append(line)
            if variant_lines:
                (deps_dir / "platform.txt.extract").write_text(
                    "\n".join(variant_lines) + "\n", encoding="utf-8"
                )
                files.append(deps_dir / "platform.txt.extract")

        sdk_dir = evidence_dir / "sdkconfig"
        sdk_dir.mkdir(parents=True, exist_ok=True)
        for f in arduino15.rglob("sdkconfig.h"):
            shutil.copy2(f, sdk_dir / "sdkconfig.h")
            files.append(sdk_dir / "sdkconfig.h")
            break

        return files
