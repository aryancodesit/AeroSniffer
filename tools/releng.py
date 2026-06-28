#!/usr/bin/env python3
"""AeroSniffer release engineering CLI.

Commands:
  install     Install dependencies (cores, libraries, setup)
  build       Build firmware locally (legacy, prints VERSION= to stdout)
  ci-build    CI build: install/build/evidence — single entry point
  evidence    Generate evidence pack from existing build/
  version     Print version string (for CI GITHUB_OUTPUT)

Stdout is reserved for version output. All other output goes to stderr.
"""

import datetime
import hashlib
import pathlib
import platform
import shutil
import subprocess
import sys

try:
    import tomllib
except ImportError:
    try:
        import tomli as tomllib
    except ImportError:
        sys.exit("error: Python 3.11+ required, or install tomli: pip install tomli")

VERSION = "0.6.0"
ROOT = pathlib.Path(__file__).resolve().parents[1]
DEPS = ROOT / "tools" / "deps.toml"
OUT = ROOT / "build"
SKETCH = ROOT / "AeroSniffer"


def die(msg):
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)


def run(cmd, capture=False, **kw):
    kw.setdefault("check", True)
    kw.setdefault("text", True)
    if capture:
        kw["capture_output"] = True
        result = subprocess.run(cmd, **kw)
        return result.stdout.strip()
    subprocess.run(cmd, **kw)


def check_tool(name):
    if shutil.which(name) is None:
        die(f"{name} not found in PATH")


def get_tool_version(tool, args=None):
    try:
        return run([tool] + (args or ["version"]), capture=True).splitlines()[0].strip()
    except Exception:
        return "unknown"


def load_deps():
    if not DEPS.exists():
        die(f"deps.toml not found at {DEPS}")
    return tomllib.loads(DEPS.read_text())


def validate_environment():
    check_tool("arduino-cli")
    check_tool("git")
    return load_deps()


def validate_build_inputs():
    if not (SKETCH / "TFT_eSPI_UserSetup.h").exists():
        die("missing TFT_eSPI_UserSetup.h")


def validate_build_environment() -> dict:
    """Validate build environment and return deps dict.

    Checks that the required ESP32 core and all libraries are installed
    at the versions specified in deps.toml. Dies (SystemExit) on any
    mismatch.

    Returns the parsed deps dict so callers don't need a redundant
    load_deps() after validation — the single source of truth flows
    through this function.
    """
    deps = load_deps()
    board = deps["board"]
    libs = deps["libraries"]
    fqbn = board["fqbn"]
    board_list = run(["arduino-cli", "board", "listall"], capture=True)
    if fqbn not in board_list.split():
        die(f"FQBN {fqbn} not found in installed cores")
    core_parts = board["core"].split("@")
    installed_ver = get_installed_core_version(core_parts[0])
    if len(core_parts) > 1 and core_parts[1]:
        if installed_ver != core_parts[1]:
            die(f"core {core_parts[0]}@{installed_ver} installed, {board['core']} required")
    for name, ver in libs.items():
        installed_ver = get_installed_lib_version(name)
        if installed_ver != ver:
            die(f"library {name}@{installed_ver} installed, {name}@{ver} required")
    return deps


def parse_table_output(out):
    result = {}
    lines = out.splitlines()
    for line in lines[1:]:
        parts = line.strip().split()
        if len(parts) >= 2:
            result[parts[0]] = parts[1]
    return result


def get_installed_cores():
    try:
        return parse_table_output(run(["arduino-cli", "core", "list"], capture=True))
    except Exception:
        return {}


def get_installed_libs():
    try:
        return parse_table_output(run(["arduino-cli", "lib", "list"], capture=True))
    except Exception:
        return {}


def get_installed_core_version(name):
    return get_installed_cores().get(name, "")


def get_installed_lib_version(name):
    return get_installed_libs().get(name, "")


def is_tree_dirty():
    try:
        out = run(["git", "status", "--porcelain"], capture=True)
        return bool(out.strip())
    except Exception:
        return False


def get_version():
    try:
        ver = run(["git", "describe", "--tags", "--dirty"], capture=True)
        if ver:
            return ver
    except Exception:
        pass
    print("warning: git tags unavailable, using 'local-dev' as version", file=sys.stderr)
    return "local-dev"


def write_build_info(board, core, libs, version, git_commit, tree_state, arduino_ver, py_ver):
    lines = [
        f"version: {version}",
        f"git_commit: {git_commit}",
        f"tree: {tree_state}",
        f"board: {board}",
        f"core: {core}",
        "libraries:",
    ]
    for name, ver in libs.items():
        lines.append(f"  - name: {name}")
        lines.append(f"    version: {ver}")
    lines.extend([
        f"arduino_cli: {arduino_ver}",
        f"python: {py_ver}",
        f"built_at: {datetime.datetime.now(datetime.timezone.utc).isoformat()}",
        f"host: {platform.system()} {platform.machine()}",
        f"tool: releng.py v{VERSION}",
    ])
    (OUT / "build-info.yaml").write_text("\n".join(lines) + "\n", encoding="utf-8")


def _environment_ready() -> bool:
    """Check if the build environment is fully installed and correct.

    Delegates to validate_build_environment() so that ALL validation rules
    live in a single code path. Any future rule added to
    validate_build_environment() is automatically reflected here.
    Never raises or calls die() — purely a predicate for ci-build.
    """
    if shutil.which("arduino-cli") is None:
        return False
    if shutil.which("git") is None:
        return False
    try:
        load_deps()
    except SystemExit:
        return False
    try:
        validate_build_environment()
        return True
    except (SystemExit, subprocess.CalledProcessError):
        return False


def verify_install():
    deps = load_deps()
    cores = get_installed_cores()
    libs_installed = get_installed_libs()
    core_id = deps["board"]["core"]
    core_name = core_id.split("@")[0]
    core_ver = core_id.split("@")[1] if "@" in core_id else ""
    actual_ver = cores.get(core_name)
    if actual_ver and core_ver and actual_ver != core_ver:
        print(f"  warning: core {core_name}@{actual_ver} installed, {core_id} expected", file=sys.stderr)
    for name, ver in deps["libraries"].items():
        actual = libs_installed.get(name)
        if actual and actual != ver:
            print(f"  warning: {name}@{actual} installed, {name}@{ver} required", file=sys.stderr)


# ── Internal shared helpers ──────────────────────────────────────────

def _build_firmware(board: dict, version: str, capture_log: bool = True) -> bool:
    """Run arduino-cli compile. Returns True on success.

    When capture_log=True, output is tee'd to build/build.log and stderr.
    When capture_log=False, output goes directly to terminal via run().
    version.h is written before compile and cleaned up in all cases.
    """
    OUT.mkdir(parents=True, exist_ok=True)
    version_h = OUT / "version.h"
    version_h.write_text(f'// auto-generated by releng.py\n#define FW_VERSION "{version}"\n')
    include_path = version_h.resolve().as_posix()
    args = [
        "arduino-cli", "compile",
        "--fqbn", board["fqbn"],
        "--build-path", str(OUT),
        "--build-property", f"compiler.cpp.extra_flags=-include {include_path}",
        str(SKETCH),
    ]
    try:
        if capture_log:
            build_log = OUT / "build.log"
            with open(build_log, "w", encoding="utf-8") as log_f:
                proc = subprocess.Popen(
                    args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                for line in proc.stdout:
                    log_f.write(line)
                    print(line, end="", file=sys.stderr)
                proc.wait()
                return proc.returncode == 0
        else:
            run(args)
            return True
    except subprocess.CalledProcessError:
        return False
    finally:
        version_h.unlink(missing_ok=True)


def _post_build(board: dict, libs: dict, version: str, dirty: bool) -> None:
    """Create checksums.sha256 and build-info.yaml. Dies on invalid artifacts."""
    bin_files = [f for f in sorted(OUT.iterdir()) if f.is_file() and f.suffix == ".bin"]
    if not bin_files:
        die("no .bin files found in build output")
    for f in bin_files:
        if f.stat().st_size == 0:
            die(f"zero-byte artifact: {f.name}")
    checksums = [
        f"{hashlib.sha256(f.read_bytes()).hexdigest()}  {f.name}"
        for f in bin_files
    ]
    (OUT / "checksums.sha256").write_text("\n".join(checksums) + "\n", encoding="utf-8")
    arduino_ver = get_tool_version("arduino-cli")
    git_commit = run(["git", "rev-parse", "--short", "HEAD"], capture=True)
    write_build_info(
        board=board["fqbn"], core=board["core"], libs=libs,
        version=version, git_commit=git_commit,
        tree_state="dirty" if dirty else "clean",
        arduino_ver=arduino_ver, py_ver=sys.version.split()[0],
    )


# ── Commands ─────────────────────────────────────────────────────────

def cmd_install():
    deps = validate_environment()
    board = deps["board"]
    libs = deps["libraries"]
    board_url = board["core_url"]
    run(["arduino-cli", "config", "init", "--overwrite"], check=False)
    run(["arduino-cli", "config", "set", "board_manager.additional_urls", board_url])
    run(["arduino-cli", "core", "update-index"])
    core_parts = board["core"].split("@")
    core_name = core_parts[0]
    core_ver = core_parts[1] if len(core_parts) > 1 else ""
    installed_ver = get_installed_core_version(core_name)
    if installed_ver:
        if installed_ver == core_ver:
            print(f"  Core {core_name}@{installed_ver} already installed", file=sys.stderr)
        else:
            print(f"  warning: core {core_name}@{installed_ver} installed, {core_name}@{core_ver} required — reinstalling", file=sys.stderr)
            run(["arduino-cli", "core", "uninstall", core_name])
            spec = f"{core_name}@{core_ver}" if core_ver else core_name
            run(["arduino-cli", "core", "install", spec])
    else:
        spec = f"{core_name}@{core_ver}" if core_ver else core_name
        run(["arduino-cli", "core", "install", spec])
    for name, ver in libs.items():
        installed_ver = get_installed_lib_version(name)
        if installed_ver:
            if installed_ver == ver:
                print(f"  Library {name}@{installed_ver} already installed", file=sys.stderr)
            else:
                print(f"  warning: {name}@{installed_ver} installed, {name}@{ver} required — reinstalling", file=sys.stderr)
                run(["arduino-cli", "lib", "uninstall", name])
                run(["arduino-cli", "lib", "install", f"{name}@{ver}"])
        else:
            run(["arduino-cli", "lib", "install", f"{name}@{ver}"])
    src = SKETCH / "TFT_eSPI_UserSetup.h"
    if src.exists():
        home = pathlib.Path.home()
        for candidate in [
            home / "Arduino" / "libraries" / "TFT_eSPI" / "User_Setup.h",
            home / "Documents" / "Arduino" / "libraries" / "TFT_eSPI" / "User_Setup.h",
        ]:
            if candidate.parent.exists():
                shutil.copy2(src, candidate)
                print(f"  Copied TFT_eSPI_UserSetup.h -> {candidate}", file=sys.stderr)
                break
    verify_install()


def cmd_verify():
    validate_environment()
    validate_build_environment()
    print("All dependencies verified", file=sys.stderr)


def cmd_build(no_install=False):
    """Build firmware locally. Dies on failure. Prints VERSION= to stdout."""
    if not no_install:
        cmd_install()
    else:
        validate_environment()
    validate_build_inputs()
    deps = validate_build_environment()  # returns deps, dies on failure
    board = deps["board"]
    libs = deps["libraries"]
    version = get_version()
    dirty = is_tree_dirty()
    if not _build_firmware(board, version, capture_log=False):
        die("arduino-cli compile failed")
    _post_build(board, libs, version, dirty)
    print(f"VERSION={version}")


def cmd_evidence():
    """Generate evidence pack from existing build/ directory."""
    import runpy
    ev_path = pathlib.Path(__file__).parent / "evidence.py"
    runpy.run_path(str(ev_path), run_name="__main__")


def cmd_ci_build():
    """CI build: install → build (captured) → evidence — single entry point.

    Designed to be the ONLY build command called from GitHub Actions.
    Always exits 0. Build failure is captured via build/build-failed sentinel.
    Evidence generation is guaranteed even on failure via try/finally.

    Produces:
      build/build.log        — full build output (stdout + stderr)
      build/build-failed     — sentinel file (only if build failed)
      build/evidence/        — evidence pack (always)
      build/checksums.sha256 — SHA256 of .bin files (on success)
      build/build-info.yaml  — build metadata (on success)

    Prints VERSION=<version> to stdout for $GITHUB_OUTPUT.
    """
    version = "local-dev"
    build_ok = False

    try:
        # 1. Install dependencies if the environment is not ready.
        #    Uses an explicit predicate check rather than try/except SystemExit
        #    so that normal control flow never depends on catching sys.exit().
        if not _environment_ready():
            cmd_install()

        validate_build_inputs()
        deps = validate_build_environment()  # returns deps, dies on failure

        board = deps["board"]
        libs = deps["libraries"]
        version = get_version()
        dirty = is_tree_dirty()

        # 2. Build with log capture
        build_ok = _build_firmware(board, version, capture_log=True)

        # 3. Post-build on success
        if build_ok:
            _post_build(board, libs, version, dirty)
        else:
            (OUT / "build-failed").touch()
    except SystemExit:
        # Validation or post-build detected an unrecoverable problem
        (OUT / "build-failed").touch()
    finally:
        # 4. Evidence pack — guaranteed regardless of build outcome.
        #    Only Exception is caught here (not BaseException) because
        #    SystemExit and KeyboardInterrupt from evidence indicate a
        #    programming error that should propagate.
        try:
            cmd_evidence()
        except Exception:
            pass
        # 5. Version output for $GITHUB_OUTPUT
        print(f"VERSION={version}")


def cmd_version():
    print(get_version())


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print("Usage: releng.py <install|build|ci-build|evidence|version> [--no-install|--verify-only]",
              file=sys.stderr)
        sys.exit(1)
    cmd = sys.argv[1]
    if cmd == "install":
        cmd_install()
    elif cmd == "build":
        if "--verify-only" in sys.argv:
            cmd_verify()
            return
        no_install = "--no-install" in sys.argv
        cmd_build(no_install=no_install)
    elif cmd == "ci-build":
        cmd_ci_build()
    elif cmd == "evidence":
        cmd_evidence()
    elif cmd == "version":
        cmd_version()
    else:
        die(f"unknown command: {cmd}")


if __name__ == "__main__":
    main()
