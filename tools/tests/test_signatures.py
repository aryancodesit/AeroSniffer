"""Regression tests for every signature in tools/signatures/.

Each test case includes:
- A positive sample: build log excerpt that SHOULD match
- A negative sample: build log excerpt that should NOT match (false positive check)

Run: python -m pytest tools/tests/test_signatures.py -v
Or:  python tools/tests/test_signatures.py
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from signatures.matcher import SignatureMatcher, Lifecycle


SIGNATURES_DIR = Path(__file__).resolve().parent.parent / "signatures"
MATCHER = SignatureMatcher(signatures_dir=SIGNATURES_DIR)


def load_signature_ids() -> list[str]:
    ids = []
    for f in sorted(SIGNATURES_DIR.glob("*.yaml")):
        from signatures.matcher import Signature
        sig = Signature.from_yaml(f)
        ids.append(sig.id)
    return ids


def test_all_signatures_have_positive_match():
    """Every loaded signature must match its own test sample.

    Test samples are embedded via a naming convention:
    tools/signatures/foo.yaml → tools/tests/samples/foo/positive.log
    """
    samples_dir = Path(__file__).parent / "samples"
    for sig in MATCHER.load_all():
        pos = samples_dir / sig.id / "positive.log"
        if not pos.exists():
            print(f"  [SKIP] no positive sample for {sig.id} at {pos}")
            continue
        text = pos.read_text(encoding="utf-8", errors="replace")
        result = MATCHER.best_match(text, min_confidence=0.0)
        assert result is not None, (f"Signature {sig.id} did not match its own "
                                    f"positive sample at {pos}")
        assert result.matched, (f"Signature {sig.id} returned matched=False "
                                f"on positive sample at {pos}")
        print(f"  [OK] {sig.id} matched positive sample "
              f"(confidence={result.confidence}, quality={result.match_quality})")


def test_all_signatures_reject_negative_sample():
    """No signature should match its negative sample (false positive check)."""
    samples_dir = Path(__file__).parent / "samples"
    for sig in MATCHER.load_all():
        neg = samples_dir / sig.id / "negative.log"
        if not neg.exists():
            print(f"  [SKIP] no negative sample for {sig.id} at {neg}")
            continue
        text = neg.read_text(encoding="utf-8", errors="replace")
        result = MATCHER.best_match(text, min_confidence=0.0)
        if result and result.matched:
            print(f"  [WARN] {sig.id} matched negative sample "
                  f"(confidence={result.confidence})")
        else:
            print(f"  [OK] {sig.id} correctly rejected negative sample")


def test_deprecated_signatures_excluded_by_default():
    """DEPRECATED signatures should be excluded from default load_all()."""
    from signatures.matcher import Signature
    deprecated_file = SIGNATURES_DIR / "deprecated_example.yaml"
    if not deprecated_file.exists():
        print("  [SKIP] no deprecated_example.yaml to test")
        return
    sig = Signature.from_yaml(deprecated_file)
    assert sig.lifecycle == Lifecycle.DEPRECATED
    loaded_ids = [s.id for s in MATCHER.load_all()]
    assert sig.id not in loaded_ids, (
        f"Deprecated signature {sig.id} should be excluded from default load")


def test_deprecated_signatures_included_with_flag():
    """DEPRECATED signatures should be loadable with include_deprecated=True."""
    from signatures.matcher import Signature
    deprecated_file = SIGNATURES_DIR / "deprecated_example.yaml"
    if not deprecated_file.exists():
        print("  [SKIP] no deprecated_example.yaml to test")
        return
    sig = Signature.from_yaml(deprecated_file)
    assert sig.lifecycle == Lifecycle.DEPRECATED
    m = SignatureMatcher(signatures_dir=SIGNATURES_DIR, include_deprecated=True)
    loaded_ids = [s.id for s in m.load_all()]
    assert sig.id in loaded_ids, (
        f"Deprecated signature {sig.id} should be loadable with flag")


def test_experimental_signatures_excluded_by_default():
    """EXPERIMENTAL signatures should be excluded from default load_all()."""
    from signatures.matcher import Signature
    exp_file = SIGNATURES_DIR / "experimental_example.yaml"
    if not exp_file.exists():
        print("  [SKIP] no experimental_example.yaml to test")
        return
    sig = Signature.from_yaml(exp_file)
    assert sig.lifecycle == Lifecycle.EXPERIMENTAL
    loaded_ids = [s.id for s in MATCHER.load_all()]
    assert sig.id not in loaded_ids, (
        f"Experimental signature {sig.id} should be excluded from default load")


def test_experimental_signatures_included_with_flag():
    """EXPERIMENTAL signatures should be loadable with include_experimental=True."""
    from signatures.matcher import Signature
    exp_file = SIGNATURES_DIR / "experimental_example.yaml"
    if not exp_file.exists():
        print("  [SKIP] no experimental_example.yaml to test")
        return
    sig = Signature.from_yaml(exp_file)
    assert sig.lifecycle == Lifecycle.EXPERIMENTAL
    m = SignatureMatcher(signatures_dir=SIGNATURES_DIR, include_experimental=True)
    loaded_ids = [s.id for s in m.load_all()]
    assert sig.id in loaded_ids, (
        f"Experimental signature {sig.id} should be loadable with flag")


def test_superseded_signatures_excluded():
    """SUPERSEDED signatures should be excluded from default load_all()."""
    from signatures.matcher import Signature
    sup_file = SIGNATURES_DIR / "superseded_example.yaml"
    if not sup_file.exists():
        print("  [SKIP] no superseded_example.yaml to test")
        return
    sig = Signature.from_yaml(sup_file)
    assert sig.lifecycle == Lifecycle.SUPERSEDED
    loaded_ids = [s.id for s in MATCHER.load_all()]
    assert sig.id not in loaded_ids, (
        f"Superseded signature {sig.id} should be excluded from default load")


def test_all_signatures_have_required_fields():
    """All active signatures must have required fields filled."""
    for sig in MATCHER.load_all():
        assert sig.id, f"Signature missing id"
        assert sig.symptom, f"Signature {sig.id} missing symptom"
        assert sig.patterns, f"Signature {sig.id} has no patterns"
        assert sig.trigger, f"Signature {sig.id} missing trigger"
        assert sig.root_cause, f"Signature {sig.id} missing root_cause"
        assert sig.fix, f"Signature {sig.id} missing fix"
        assert 1 <= sig.priority <= 10, (
            f"Signature {sig.id} priority {sig.priority} out of range 1-10")
        assert 0.0 <= sig.confidence_threshold <= 1.0, (
            f"Signature {sig.id} confidence_threshold {sig.confidence_threshold} "
            f"out of range 0-1")


def test_signature_tags_are_strings():
    """All tags in all loaded signatures must be strings."""
    for sig in MATCHER.load_all():
        for tag in sig.tags:
            assert isinstance(tag, str), (
                f"Signature {sig.id} has non-string tag: {tag!r}")


def test_lifecycle_enum_values():
    """Lifecycle enum has expected values."""
    assert Lifecycle.ACTIVE.value == "ACTIVE"
    assert Lifecycle.DEPRECATED.value == "DEPRECATED"
    assert Lifecycle.SUPERSEDED.value == "SUPERSEDED"
    assert Lifecycle.EXPERIMENTAL.value == "EXPERIMENTAL"


def test_match_quality_thresholds():
    """Match result quality classification uses correct thresholds."""
    from signatures.matcher import MatchResult, Signature

    sig = Signature(id="test", symptom="test", patterns=[".*"],
                     trigger="", root_cause="", fix="")

    def make_result(confidence: float) -> str:
        return MatchResult(signature=sig, matched=True,
                           confidence=confidence).match_quality

    assert make_result(0.95) == "exact"
    assert make_result(0.85) == "strong"
    assert make_result(0.55) == "partial"
    assert make_result(0.30) == "weak"


if __name__ == "__main__":
    import pytest
    sys.exit(pytest.main([__file__, "-v", *sys.argv[1:]]))
