"""Linker analysis provider: nm symbol tables, size sections, map file, largest symbols."""

import subprocess
import shutil
from pathlib import Path
from . import EvidenceProvider


class LinkerProvider(EvidenceProvider):
    @property
    def name(self): return "linker"

    @property
    def description(self): return "Symbol tables, section sizes, linker map"

    def _find_nm(self, arduino15):
        tools_dir = arduino15 / "packages" / "esp32" / "tools" / "esp-x32"
        if not tools_dir.exists():
            return None
        for ver_dir in sorted(tools_dir.iterdir(), reverse=True):
            for name in ["xtensa-esp32s3-elf-nm.exe", "xtensa-esp32s3-elf-nm"]:
                cand = ver_dir / "bin" / name
                if cand.exists():
                    return str(cand)
        return None

    def _find_size(self, nm_tool):
        if nm_tool:
            return nm_tool.replace("-nm", "-size")
        return None

    def collect(self, evidence_dir, build_dir, arduino15):
        files = []
        nm_tool = self._find_nm(arduino15)
        sym_dir = evidence_dir / "symbols"
        sym_dir.mkdir(parents=True, exist_ok=True)

        core_a = build_dir / "core" / "core.a"
        if core_a.exists() and nm_tool:
            try:
                out = subprocess.run([nm_tool, "-C", str(core_a)], capture_output=True, stderr=subprocess.DEVNULL, text=True, errors="replace", timeout=30)
                sym_file = sym_dir / "core.a.symbols"
                sym_file.write_text(out.stdout)
                files.append(sym_file)
                unresolved = [l for l in out.stdout.splitlines() if l.startswith("U ")]
                if unresolved:
                    uf = sym_dir / "unresolved.txt"
                    uf.write_text("\n".join(unresolved) + "\n")
                    files.append(uf)
            except Exception:
                pass

        sketch_dir = build_dir / "sketch"
        if sketch_dir.exists() and nm_tool:
            combined = []
            for obj in sorted(sketch_dir.glob("*.cpp.o")):
                try:
                    out = subprocess.run([nm_tool, "-C", str(obj)], capture_output=True, stderr=subprocess.DEVNULL, text=True, errors="replace", timeout=15)
                    combined.append(f"# {obj.name}\n{out.stdout.strip()}")
                except Exception:
                    pass
            if combined:
                sf = sym_dir / "sketch.symbols"
                sf.write_text("\n\n".join(combined) + "\n")
                files.append(sf)

        map_file = build_dir / "AeroSniffer.ino.map"
        if map_file.exists():
            map_dir = evidence_dir / "maps"
            map_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(map_file, map_dir / "firmware.map")
            files.append(map_dir / "firmware.map")

            size_tool = self._find_size(nm_tool)
            elf_file = build_dir / "AeroSniffer.ino.elf"
            if elf_file.exists() and size_tool:
                try:
                    out = subprocess.run([size_tool, "-A", str(elf_file)], capture_output=True, stderr=subprocess.DEVNULL, text=True, errors="replace", timeout=15)
                    sf = map_dir / "sections.txt"
                    sf.write_text(out.stdout)
                    files.append(sf)
                except Exception:
                    pass

            if nm_tool and elf_file.exists():
                try:
                    out = subprocess.run(
                        [nm_tool, "--size-sort", "-C", str(elf_file)],
                        capture_output=True, stderr=subprocess.DEVNULL, text=True, errors="replace", timeout=15
                    )
                    lines = out.stdout.splitlines()
                    lf = map_dir / "largest_symbols.txt"
                    lf.write_text("\n".join(lines[-50:]) + "\n" if len(lines) > 50 else out.stdout)
                    files.append(lf)
                except Exception:
                    pass

        return files
