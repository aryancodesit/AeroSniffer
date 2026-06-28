"""Compiler flag extraction and compiler version detection."""

import re
import subprocess
import shutil
from pathlib import Path
from . import EvidenceProvider


class CompilerProvider(EvidenceProvider):
    @property
    def name(self): return "compiler"

    @property
    def description(self): return "Compiler flags, defines, and version"

    @property
    def priority(self) -> int:
        return 10

    def collect(self, evidence_dir, build_dir, arduino15):
        files = []
        flags_dir = evidence_dir / "flags"
        flags_dir.mkdir(parents=True, exist_ok=True)
        deps_dir = evidence_dir / "deps"
        deps_dir.mkdir(parents=True, exist_ok=True)

        build_log = build_dir / "build.log"
        if build_log.exists():
            text = build_log.read_text(encoding="utf-8", errors="replace")
            d_flags = set()
            for m in re.finditer(r'(?<=\s)-D\w[^\s]*', text):
                d_flags.add(m.group())
            (flags_dir / "all_defines.txt").write_text(
                "\n".join(sorted(d_flags)) + "\n", encoding="utf-8"
            )
            files.append(flags_dir / "all_defines.txt")

            for unit_name in ["AeroSniffer.ino.cpp.o", "TFT_eSPI.cpp.o", "HWCDC.cpp.o"]:
                lines = [l for l in text.splitlines() if unit_name in l and "xtensa" in l]
                if lines:
                    flag_file = flags_dir / f"{unit_name.replace('.cpp.o','')}.flags"
                    flag_file.write_text(lines[-1] + "\n", encoding="utf-8")
                    files.append(flag_file)

        esp32_flags = arduino15 / "packages" / "esp32" / "tools" / "esp32-arduino-libs"
        if esp32_flags.exists():
            import glob as glob_mod
            for f in glob_mod.glob(str(esp32_flags / "**" / "flags" / "*"), recursive=True):
                fp = Path(f)
                if fp.is_file():
                    target_dir = flags_dir / "platform"
                    target_dir.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(fp, target_dir / fp.name)
                    files.append(target_dir / fp.name)

        tools_dir = arduino15 / "packages" / "esp32" / "tools" / "esp-x32"
        if tools_dir.exists():
            for ver_dir in sorted(tools_dir.iterdir(), reverse=True):
                for triple in ["xtensa-esp32s3-elf-g++", "xtensa-esp32s3-elf-gcc"]:
                    gxx = ver_dir / "bin" / triple
                    gxx_exe = ver_dir / "bin" / (triple + ".exe")
                    compiler = gxx_exe if gxx_exe.exists() else (gxx if gxx.exists() else None)
                    if compiler:
                        try:
                            out = subprocess.run([str(compiler), "--version"],
                                                 capture_output=True, stderr=subprocess.DEVNULL, text=True, errors="replace", timeout=10)
                            ver_file = deps_dir / "compiler_version.txt"
                            ver_file.write_text(out.stdout)
                            files.append(ver_file)
                        except Exception:
                            pass
                        break
                else:
                    continue
                break

        return files
