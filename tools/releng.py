#!/usr/bin/env python3
"""AeroSniffer release engineering CLI.

Commands:
  install    Install toolchain + library dependencies per deps.toml
  build      Install deps (if missing), compile firmware, generate build/ artifacts
  build --no-install    Skip install, compile only

Stdout is reserved for the version marker (VERSION=...).
All other output goes to stderr.
"""

import sys, os, subprocess, hashlib, shutil, pathlib, datetime, platform

try:
    import tomllib
except ImportError:
    try:
        import tomli as tomllib
    except ImportError:
        sys.exit("error: Python 3.11+ required, or install tomli: pip install tomli")

VERSION = "0.4.0"
ROOT = pathlib.Path(__file__).resolve().parents[1]
DEPS = ROOT / "tools" / "deps.toml"
OUT = ROOT / "build"
SKETCH = ROOT / "AeroSniffer"
EXPECTED_ARTIFACTS = (".bin", ".bootloader.bin", ".partitions.bin")


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
        die("missing TFT_eSPI_UserSetup.h — build will produce incorrect pin mappings")


def parse_table_output(out):
    """Parse two-column table from arduino-cli output into {key: value}."""
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


def is_core_installed(core_id):
    name = core_id.split("@")[0]
    version = core_id.split("@")[1] if "@" in core_id else ""
    installed = get_installed_cores()
    if not version:
        return name in installed
    return installed.get(name) == version


def is_lib_installed(name, version):
    installed = get_installed_libs()
    return installed.get(name) == version


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


def write_build_info(OUT, version, git_commit, tree_state, board, core, libs, arduino_ver, py_ver):
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
            print(f"  warning: {name}@{actual} installed, {name}@{ver} expected", file=sys.stderr)


def cmd_install():
    deps = validate_environment()
    board = deps["board"]
    libs = deps["libraries"]
    board_url = board["core_url"]

    run(["arduino-cli", "core", "update-index", "--additional-urls", board_url])

    if is_core_installed(board["core"]):
        print(f"  Core {board['core']} already installed, skipping", file=sys.stderr)
    else:
        run(["arduino-cli", "core", "install", "--additional-urls", board_url, board["core"]])

    for name, ver in libs.items():
        spec = f"{name}@{ver}"
        if is_lib_installed(name, ver):
            print(f"  Library {spec} already installed, skipping", file=sys.stderr)
        else:
            run(["arduino-cli", "lib", "install", spec])

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


def cmd_build(no_install=False):
    if not no_install:
        cmd_install()
    else:
        validate_environment()

    validate_build_inputs()
    deps = load_deps()
    board = deps["board"]
    libs = deps["libraries"]
    version = get_version()
    dirty = is_tree_dirty()
    OUT.mkdir(parents=True, exist_ok=True)

    version_h = OUT / "version.h"
    version_h.write_text(f'// auto-generated by releng.py\n#define FW_VERSION "{version}"\n')

    include_path = version_h.resolve().as_posix()
    run([
        "arduino-cli", "compile",
        "--fqbn", board["fqbn"],
        "--build-path", str(OUT),
        "--build-property", f"build.extra_flags=-include {include_path}",
        str(SKETCH),
    ])

    version_h.unlink(missing_ok=True)

    bin_files = [f for f in sorted(OUT.iterdir()) if f.is_file() and f.suffix == ".bin"]
    if not bin_files:
        die("no .bin files found in build output — compilation may have failed")
    for f in bin_files:
        if f.stat().st_size == 0:
            die(f"zero-byte artifact: {f.name}")

    checksums = []
    for f in bin_files:
        h = hashlib.sha256(f.read_bytes()).hexdigest()
        checksums.append(f"{h}  {f.name}")
    (OUT / "checksums.sha256").write_text("\n".join(checksums) + "\n", encoding="utf-8")

    arduino_ver = get_tool_version("arduino-cli")
    git_commit = run(["git", "rev-parse", "--short", "HEAD"], capture=True)
    py_ver = sys.version.split()[0]
    tree_state = "dirty" if dirty else "clean"

    write_build_info(
        OUT=OUT,
        version=version,
        git_commit=git_commit,
        tree_state=tree_state,
        board=board["fqbn"],
        core=board["core"],
        libs=libs,
        arduino_ver=arduino_ver,
        py_ver=py_ver,
    )

    print(f"VERSION={version}")


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print("Usage: releng.py <install|build> [--no-install]", file=sys.stderr)
        sys.exit(1)
    cmd = sys.argv[1]
    if cmd == "install":
        cmd_install()
    elif cmd == "build":
        no_install = "--no-install" in sys.argv
        cmd_build(no_install=no_install)
    else:
        die(f"unknown command: {cmd}")


if __name__ == "__main__":
    main()
