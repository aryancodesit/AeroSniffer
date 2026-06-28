"""Automated CI regression tests for /fw signature matching.

Each evidence pack with metadata.yaml is discovered automatically and
tested as a parameterized test case.  Expectations (expected_signature,
minimum_confidence) are loaded from metadata, not hardcoded — adding a
new pack requires only creating the directory + metadata.yaml + build.log.

Corpus organization::
    tools/tests/ci/
        synthetic/   — hand-crafted scenarios (maintained by developers)
        real/        — archived GitHub Actions evidence packs
        (root)       — legacy packs (backward compat, no prefix in test ID)

To archive a real GitHub Actions failure::
    1. Copy the CI-generated evidence/ directory into tools/tests/ci/real/<name>/
    2. Write metadata.yaml with the expected signature and confidence
    3. Run tests — the parametrized runner picks it up automatically
"""
import re
import sys
from pathlib import Path

import pytest
import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from signatures.matcher import SignatureMatcher

SIGNATURES_DIR = Path(__file__).resolve().parent.parent.parent / "signatures"
MATCHER = SignatureMatcher(SIGNATURES_DIR)

# Schema version we understand — bump on breaking metadata.yaml changes.
SUPPORTED_METADATA_SCHEMA = 1


def _discover_evidence_packs():
    """Yield (display_name, pack_dir, metadata) for every pack with metadata.yaml.

    Scans tools/tests/ci/synthetic/, tools/tests/ci/real/, and the root
    directory itself (backward compat).  Packs without metadata.yaml are
    silently skipped — they remain in the tree but don't generate tests.

    The display_name is the pack directory path relative to ci_dir using
    POSIX separators (e.g. ``synthetic/linker-hwcdc`` for subdirectory
    packs, ``linker-hwcdc`` for root-level legacy packs).
    """
    ci_dir = Path(__file__).parent

    def _scan(base_dir, skip_dirs=None):
        skip = set(skip_dirs or ())
        for d in sorted(base_dir.iterdir()):
            if not d.is_dir() or d.name.startswith("_") or d.name in skip:
                continue
            meta_file = d / "metadata.yaml"
            if not meta_file.exists():
                continue
            with open(meta_file, encoding="utf-8") as f:
                metadata = yaml.safe_load(f)
            display_name = d.relative_to(ci_dir).as_posix()
            yield display_name, d, metadata

    # Primary scan: synthetic/ and real/ subdirectories.
    for sub in ("synthetic", "real"):
        subdir = ci_dir / sub
        if subdir.is_dir():
            yield from _scan(subdir)

    # Backward-compat scan: root level (skipping synthetic/, real/).
    yield from _scan(ci_dir, skip_dirs={"synthetic", "real"})


EVIDENCE_PACKS = list(_discover_evidence_packs())

# Map scenario_id → [display_name, ...] for path-level duplicate diagnostics.
_SCENARIO_ID_MAP: dict[str, list[str]] = {}
for disp_name, _, meta in EVIDENCE_PACKS:
    _SCENARIO_ID_MAP.setdefault(meta["scenario_id"], []).append(disp_name)

# Build the set of valid signature IDs for cross-referencing.
# Use a separate instance to avoid mutating the global MATCHER used in tests.
_SIG_DB = SignatureMatcher(SIGNATURES_DIR)
_SIG_DB.load_all()
VALID_SIGNATURE_IDS = frozenset(s.id for s in _SIG_DB._signatures)


# ── Corpus integrity ───────────────────────────────────────────────────

def test_scenario_ids_globally_unique():
    """No two evidence packs may share a scenario_id, regardless of source.

    This prevents ambiguity when a real CI pack archived under real/ has
    the same scenario_id as a synthetic pack under synthetic/.
    """
    dupes = {sid: names for sid, names in _SCENARIO_ID_MAP.items()
             if len(names) > 1}
    assert not dupes, (
        "Duplicate scenario_id(s) detected across evidence packs:\n"
        + "\n".join(f"  {sid}: {', '.join(names)}"
                     for sid, names in dupes.items()))


# ── Metadata validation helpers ────────────────────────────────────────

REQUIRED_METADATA_FIELDS = (
    "metadata_schema_version",
    "scenario_id",
    "source",
    "expected_signature",
    "minimum_confidence",
    "framework_version",
    "regression_issue",
)

VALID_SOURCES = ("synthetic", "real")


def _validate_pack(display_name, pack_dir, metadata):
    """Validate that an evidence pack is structurally sound.

    Checks:
    - metadata schema version (required)
    - required field presence (required)
    - field types (required)
    - build.log existence and non-empty (required)
    - manifest.yaml schema when present (optional, backward compat)
    Returns the build log text on success, raises AssertionError on failure.
    """
    # ── Schema version — future-proofing for metadata.yaml evolution. ──
    schema_ver = metadata.get("metadata_schema_version")
    assert isinstance(schema_ver, int), (
        f"[{display_name}] metadata_schema_version must be int, "
        f"got {type(schema_ver).__name__}")
    assert schema_ver == SUPPORTED_METADATA_SCHEMA, (
        f"[{display_name}] metadata_schema_version={schema_ver}, "
        f"expected {SUPPORTED_METADATA_SCHEMA}")

    # ── Required field presence. ──
    for field in REQUIRED_METADATA_FIELDS:
        assert field in metadata, (
            f"[{display_name}] missing required field: {field}")

    # ── Type validation — catches YAML type errors at test time. ──
    assert isinstance(metadata["scenario_id"], str), (
        f"[{display_name}] scenario_id must be str, "
        f"got {type(metadata['scenario_id']).__name__}")
    assert isinstance(metadata["source"], str), (
        f"[{display_name}] source must be str, "
        f"got {type(metadata['source']).__name__}")
    assert metadata["source"] in VALID_SOURCES, (
        f"[{display_name}] source={metadata['source']!r}, "
        f"expected one of {VALID_SOURCES}")

    exp_sig = metadata["expected_signature"]
    assert exp_sig is None or isinstance(exp_sig, str), (
        f"[{display_name}] expected_signature must be str or None, "
        f"got {type(exp_sig).__name__}")
    if exp_sig is not None:
        assert exp_sig in VALID_SIGNATURE_IDS, (
            f"[{display_name}] expected_signature={exp_sig!r} not found in "
            f"loaded signature database "
            f"(available: {', '.join(sorted(VALID_SIGNATURE_IDS))})")

    min_conf = metadata["minimum_confidence"]
    assert isinstance(min_conf, (int, float)), (
        f"[{display_name}] minimum_confidence must be numeric "
        f"(int or float), got {type(min_conf).__name__}")
    assert 0.0 <= min_conf <= 1.0, (
        f"[{display_name}] minimum_confidence={min_conf} must be in [0.0, 1.0]")

    assert isinstance(metadata["framework_version"], str), (
        f"[{display_name}] framework_version must be str, "
        f"got {type(metadata['framework_version']).__name__}")
    assert re.fullmatch(
        r"v\d+\.\d+\.\d+([-+][a-zA-Z0-9.]+)?",
        metadata["framework_version"],
    ), (f"[{display_name}] framework_version={metadata['framework_version']!r} "
        f"must be semantic version (e.g. v0.6.0, v1.2.3-rc1)")
    assert isinstance(metadata["regression_issue"], str), (
        f"[{display_name}] regression_issue must be str, "
        f"got {type(metadata['regression_issue']).__name__}")

    # ── Build log must exist and be non-empty. ──
    log_file = pack_dir / "build.log"
    assert log_file.is_file(), (
        f"[{display_name}] build.log not found at {log_file}")
    log_text = log_file.read_text(encoding="utf-8")
    assert len(log_text) > 0, (
        f"[{display_name}] build.log is empty")

    # ── Optional manifest.yaml validation (backward compat). ──
    # Real evidence packs from CI carry an evidence_schema_version and
    # build_success field.  Validate them when present without requiring
    # manifest.yaml to exist (synthetic packs may omit it).
    manifest_file = pack_dir / "manifest.yaml"
    if manifest_file.is_file():
        with open(manifest_file, encoding="utf-8") as f:
            manifest = yaml.safe_load(f)
        ev_schema = manifest.get("evidence_schema_version")
        assert ev_schema is not None, (
            f"[{display_name}] manifest.yaml missing evidence_schema_version")
        # Normalise: evidence.py generates str "1"; hand-crafted may use int 1.
        assert str(ev_schema) == "1", (
            f"[{display_name}] manifest.yaml evidence_schema_version={ev_schema!r}, "
            f"expected '1'")
        bs = manifest.get("build_success")
        assert isinstance(bs, bool), (
            f"[{display_name}] manifest.yaml build_success must be bool, "
            f"got {type(bs).__name__}")

    return log_text


# ── Data-driven evidence pack tests ────────────────────────────────────

@pytest.mark.parametrize("display_name,pack_dir,metadata", EVIDENCE_PACKS)
def test_evidence_pack(display_name, pack_dir, metadata):
    """Data-driven: validate every evidence pack against metadata.yaml.

    Each pack's build.log is fed to SignatureMatcher. The test asserts
    that the best match matches expected_signature at or above
    minimum_confidence.  Packs with expected_signature: null expect
    zero signature matches (no false positives).
    """
    log_text = _validate_pack(display_name, pack_dir, metadata)

    all_matches = [m for m in MATCHER.match(log_text) if m.matched]
    best = max(all_matches, key=lambda m: m.confidence) if all_matches else None

    expected_id = metadata.get("expected_signature")
    min_conf = metadata.get("minimum_confidence", 0.0)

    if expected_id is None:
        assert best is None, (
            f"[{display_name}] expected no match, "
            f"got {best.signature.id} at {best.confidence:.3f}")
        assert len(all_matches) == 0, (
            f"[{display_name}] expected 0 matches, got {len(all_matches)}")
    else:
        assert best is not None, (
            f"[{display_name}] expected match for {expected_id}, got no matches")
        assert best.matched, (
            f"[{display_name}] best match returned matched=False")
        assert best.signature.id == expected_id, (
            f"[{display_name}] expected {expected_id}, got {best.signature.id}")
        assert best.confidence >= min_conf, (
            f"[{display_name}] expected confidence >= {min_conf}, "
            f"got {best.confidence:.3f}")


# ── _environment_ready unit tests ──────────────────────────────────────

def test_environment_ready_notices_missing_tools(monkeypatch):
    """_environment_ready returns False when git or arduino-cli is missing."""
    from tools.releng import _environment_ready
    monkeypatch.setattr("shutil.which", lambda x: None)
    assert _environment_ready() is False


def test_environment_ready_false_on_bad_deps(monkeypatch):
    """_environment_ready returns False when deps.toml can't be loaded."""
    from tools.releng import _environment_ready
    monkeypatch.setattr("shutil.which", lambda x: "C:\\fake\\path.exe")
    monkeypatch.setattr(
        "tools.releng.load_deps",
        lambda: (_ for _ in ()).throw(SystemExit(1)),
    )
    assert _environment_ready() is False


def test_environment_ready_delegates_to_validate(monkeypatch):
    """_environment_ready defers to validate_build_environment for rules.

    If a new check is added to validate_build_environment, this test
    proves _environment_ready automatically picks it up — no duplicate
    maintenance needed.
    """
    from tools.releng import _environment_ready
    fake_path = "C:\\fake\\arduino-cli.exe"
    monkeypatch.setattr("shutil.which", lambda x: fake_path)
    monkeypatch.setattr(
        "tools.releng.load_deps",
        lambda: {"board": {"fqbn": "esp32:esp32:esp32s3dev",
                           "core": "esp32:esp32@3.1.0"},
                 "libraries": {}},
    )

    # Simulate validate_build_environment failing (e.g., core mismatch)
    def failing_validate():
        raise SystemExit(1)

    monkeypatch.setattr(
        "tools.releng.validate_build_environment", failing_validate)
    assert _environment_ready() is False, (
        "should return False when validate_build_environment raises SystemExit")


# ── Lifecycle filtering ────────────────────────────────────────────────

def test_deprecated_signatures_excluded_by_default():
    """Signatures with lifecycle=deprecated should NOT be loaded by default."""
    MATCHER.load_all()
    ids = [s.id for s in MATCHER._signatures]
    assert "deprecated-example-001" not in ids
    assert "superseded-example-001" not in ids


def test_include_deprecated_flag():
    """include_deprecated=True + include_experimental=True loads both."""
    matcher = SignatureMatcher(
        SIGNATURES_DIR, include_deprecated=True, include_experimental=True)
    matcher.load_all()
    ids = [s.id for s in matcher._signatures]
    assert "deprecated-example-001" in ids
    assert "experimental-example-001" in ids
    assert "superseded-example-001" not in ids


def test_include_deprecated_skips_superseded():
    """Superseded is always excluded even with include_deprecated=True."""
    matcher = SignatureMatcher(SIGNATURES_DIR, include_deprecated=True)
    matcher.load_all()
    ids = [s.id for s in matcher._signatures]
    assert "superseded-example-001" not in ids


def test_experimental_excluded_by_default():
    """Experimental signatures are excluded by default (include_experimental=False)."""
    matcher = SignatureMatcher(SIGNATURES_DIR)
    matcher.load_all()
    ids = [s.id for s in matcher._signatures]
    assert "experimental-example-001" not in ids


def test_include_experimental_flag():
    """include_experimental=True loads experimental signatures."""
    matcher = SignatureMatcher(SIGNATURES_DIR, include_experimental=True)
    matcher.load_all()
    ids = [s.id for s in matcher._signatures]
    assert "experimental-example-001" in ids
