"""Build Evidence Pack — one-shot diagnostic capture for offline firmware failure analysis.

Collects compiler flags, symbol tables, linker maps, environment snapshots,
git state, and per-provider artifacts into a structured evidence directory.

Usage:
    python3 -m tools.evidence     # collects from build/ into build/evidence/

Schema: Evidence Schema v1 (evidence_schema_version: "1")
"""

import sys
import datetime
import platform
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

OUT = ROOT / "build"
ARD15 = Path.home() / ".arduino15"
VERSION = "0.5.0"
EVIDENCE_SCHEMA_VERSION = "1"


def _archive_history(evidence_dir: Path):
    """Move previous evidence pack into history/ before collecting new one."""
    history_dir = evidence_dir / "history"
    history_dir.mkdir(parents=True, exist_ok=True)
    for item in evidence_dir.iterdir():
        if item.name == "history" or item.name == "manifest.yaml":
            continue
        if item.is_dir() or item.is_file():
            stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d_%H%M%S")
            dest = history_dir / stamp
            shutil.move(str(item), str(dest))


def _load_build_base_artifacts(evidence_dir: Path, build_dir: Path) -> list[Path]:
    """Copy build log, build err, preprocessor output into evidence dir."""
    files = []
    build_log = build_dir / "build.log"
    build_err = build_dir / "build.err"
    if build_log.exists():
        shutil.copy2(build_log, evidence_dir / "build.log")
        files.append(evidence_dir / "build.log")
    if build_err.exists():
        shutil.copy2(build_err, evidence_dir / "build.err")
        files.append(evidence_dir / "build.err")
    preproc = build_dir / "sketch" / "AeroSniffer.ino.cpp"
    if preproc.exists():
        pp_dir = evidence_dir / "preprocessed"
        pp_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(preproc, pp_dir / "AeroSniffer.ino.cpp")
        files.append(pp_dir / "AeroSniffer.ino.cpp")
    return files


def _compute_build_exit_code(build_dir: Path) -> int:
    sentinel = build_dir / "build-failed"
    if sentinel.exists():
        sentinel.unlink(missing_ok=True)
        return 1
    build_log = build_dir / "build.log"
    if build_log.exists():
        if "error:" in build_log.read_text(encoding="utf-8", errors="replace"):
            return 1
    return 0


def _build_manifest(provider_results, evidence_dir, build_dir, base_files) -> dict:
    exit_code = _compute_build_exit_code(build_dir)
    now = datetime.datetime.now(datetime.timezone.utc)

    manifest = {
        "evidence_schema_version": EVIDENCE_SCHEMA_VERSION,
        "generator": f"evidence.py v{VERSION}",
        "generated_at": now.isoformat(),
        "host": f"{platform.system()} {platform.machine()}",
        "build_exit_code": exit_code,
        "build_success": exit_code == 0,
        "total_providers": len(provider_results),
        "providers": {},
        "files": {},
    }

    for base_f in base_files:
        key = str(base_f.relative_to(evidence_dir).as_posix())
        manifest["files"][key] = {"source": "core"}

    for result in provider_results:
        pkey = result.provider_name
        manifest["providers"][pkey] = {
            "version": result.provider_version,
            "success": result.success,
            "duration_ms": result.duration_ms,
            "files": result.file_count(),
            "warnings": len(result.warnings),
            "errors": len(result.errors),
        }
        for pf in result.files:
            try:
                key = str(pf.relative_to(evidence_dir).as_posix())
            except ValueError:
                key = str(pf)
            manifest["files"][key] = {"source": pkey}

    return manifest


def generate(evidence_dir: Path = None, build_dir: Path = None, arduino15: Path = None) -> Path:
    """Generate a complete evidence pack from existing build artifacts.

    Args:
        evidence_dir: output directory (default: build/evidence/)
        build_dir: directory containing build output (default: build/)
        arduino15: Arduino15 data directory (default: ~/.arduino15)

    Returns:
        Path to the evidence directory
    """
    evidence_dir = evidence_dir or (OUT / "evidence")
    build_dir = build_dir or OUT
    arduino15 = arduino15 or ARD15
    evidence_dir.mkdir(parents=True, exist_ok=True)

    _archive_history(evidence_dir)
    base_files = _load_build_base_artifacts(evidence_dir, build_dir)

    from tools.providers import collect_all
    provider_results = collect_all(evidence_dir, build_dir, arduino15)

    manifest = _build_manifest(provider_results, evidence_dir, build_dir, base_files)

    import yaml
    (evidence_dir / "manifest.yaml").write_text(
        yaml.dump(manifest, default_flow_style=False, sort_keys=False),
        encoding="utf-8"
    )

    zip_path = build_dir / "evidence.zip"
    shutil.make_archive(str(zip_path.with_suffix("")), "zip", str(evidence_dir))
    total_files = sum(1 for _ in evidence_dir.rglob("*") if _.is_file())
    total_providers = len(provider_results)
    failed = sum(1 for r in provider_results if not r.success)

    status = "FAIL" if failed else "OK"
    print(f"Evidence pack: {zip_path} ({zip_path.stat().st_size} bytes)", file=sys.stderr)
    print(f"Evidence dir:  {evidence_dir}", file=sys.stderr)
    print(f"Status:        [{status}] {total_providers} providers, {total_files} files, {failed} failures",
          file=sys.stderr)
    print(f"EVIDENCE_ZIP={zip_path}")
    print(f"EVIDENCE_DIR={evidence_dir}")

    return evidence_dir


def main():
    generate()


if __name__ == "__main__":
    main()
