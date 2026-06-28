"""Regression Signature Matcher — matches build failures against known signatures.

Signatures are YAML files in tools/signatures/ with the following schema:

    id: unique-string-id
    symptom: "human-readable symptom description"
    lifecycle: ACTIVE             # ACTIVE | DEPRECATED | SUPERSEDED | EXPERIMENTAL
    superseded_by: null           # id of the signature that replaces this one (SUPERSEDED only)
    patterns:                    # list of regex patterns to match against build log
      - "HWCDCSerial"
      - "undefined reference to.*Serial"
    trigger: "condition that causes the failure"
    root_cause: "underlying reason"
    fix: "how to fix it"
    priority: 5                  # 1 (low) to 10 (critical)
    confidence_threshold: 0.7    # minimum confidence to report as match
    tags: ["usb", "cdc", "serial", "build-flags"]
    first_seen: "2026-03-15"
    resolved: true
    related_files:
      - .github/workflows/firmware-build.yml
"""

import sys
import re
import yaml
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Optional


class Lifecycle(Enum):
    ACTIVE = "ACTIVE"
    DEPRECATED = "DEPRECATED"
    SUPERSEDED = "SUPERSEDED"
    EXPERIMENTAL = "EXPERIMENTAL"

    @classmethod
    def from_yaml(cls, value: str) -> "Lifecycle":
        if not value:
            return cls.ACTIVE
        try:
            return cls(value.upper())
        except ValueError:
            return cls.ACTIVE


@dataclass
class Signature:
    """A single regression signature loaded from a YAML file."""
    id: str
    symptom: str
    patterns: list[str]
    trigger: str
    root_cause: str
    fix: str
    lifecycle: Lifecycle = Lifecycle.ACTIVE
    superseded_by: Optional[str] = None
    priority: int = 5
    confidence_threshold: float = 0.7
    tags: list[str] = field(default_factory=list)
    first_seen: str = ""
    resolved: bool = False
    related_files: list[str] = field(default_factory=list)

    @classmethod
    def from_yaml(cls, path: Path) -> "Signature":
        """Load from YAML, filling defaults for missing fields."""
        raw = yaml.safe_load(path.read_text(encoding="utf-8"))
        patterns = raw.get("patterns")
        if not patterns:
            legacy_symptom = raw.get("symptom", "")
            patterns = [re.escape(legacy_symptom)] if legacy_symptom else [".*"]
        return cls(
            id=raw.get("id", path.stem),
            symptom=raw.get("symptom", ""),
            patterns=patterns,
            trigger=raw.get("trigger", ""),
            root_cause=raw.get("root_cause", ""),
            fix=raw.get("fix", ""),
            lifecycle=Lifecycle.from_yaml(raw.get("lifecycle")),
            superseded_by=raw.get("superseded_by"),
            priority=raw.get("priority", 5),
            confidence_threshold=raw.get("confidence_threshold", 0.7),
            tags=raw.get("tags", []),
            first_seen=raw.get("first_seen", ""),
            resolved=raw.get("resolved", False),
            related_files=raw.get("related_files", []),
        )


@dataclass
class MatchResult:
    """Result of matching a single signature against build log text."""
    signature: Signature
    matched: bool
    confidence: float
    matched_patterns: list[str] = field(default_factory=list)
    match_positions: list[int] = field(default_factory=list)

    @property
    def match_quality(self) -> str:
        if self.confidence >= 0.9:
            return "exact"
        elif self.confidence >= 0.7:
            return "strong"
        elif self.confidence >= 0.4:
            return "partial"
        return "weak"


class SignatureMatcher:
    """Matches build failure text against a database of regression signatures."""

    def __init__(self, signatures_dir: Path = None,
                 include_deprecated: bool = False,
                 include_experimental: bool = False):
        self.signatures_dir = signatures_dir or Path(__file__).parent
        self.include_deprecated = include_deprecated
        self.include_experimental = include_experimental
        self._signatures: list[Signature] | None = None

    def load_all(self) -> list[Signature]:
        """Load all .yaml files from signatures directory.

        Filters by lifecycle:
        - DEPRECATED signatures are excluded by default.
        - EXPERIMENTAL signatures are excluded by default.
        - SUPERSEDED signatures are excluded by default (use the superseding one).
        Use include_deprecated/include_experimental constructor flags to override.
        """
        if self._signatures is not None:
            return self._signatures
        sigs = []
        for f in sorted(self.signatures_dir.glob("*.yaml")):
            try:
                sig = Signature.from_yaml(f)
                if sig.lifecycle == Lifecycle.DEPRECATED and not self.include_deprecated:
                    continue
                if sig.lifecycle == Lifecycle.SUPERSEDED:
                    continue
                if sig.lifecycle == Lifecycle.EXPERIMENTAL and not self.include_experimental:
                    continue
                sigs.append(sig)
            except Exception as exc:
                print(f"  warning: failed to load signature {f.name}: {exc}",
                      file=sys.stderr)
        self._signatures = sigs
        return sigs

    def match(self, text: str, min_confidence: float = 0.0) -> list[MatchResult]:
        """Match text against all loaded signatures.

        Args:
            text: Build log text to search (can include error messages).
            min_confidence: Minimum confidence threshold (0.0-1.0).
                           Use 0.0 to see all matches.

        Returns:
            Sorted list of MatchResult (highest confidence first).
        """
        signatures = self.load_all()
        results: list[MatchResult] = []

        for sig in signatures:
            matched_patterns: list[str] = []
            match_positions: list[int] = []
            matched_count = 0

            for pattern_str in sig.patterns:
                try:
                    compiled = re.compile(pattern_str, re.IGNORECASE | re.MULTILINE)
                    for m in compiled.finditer(text):
                        matched_count += 1
                        if pattern_str not in matched_patterns:
                            matched_patterns.append(pattern_str)
                        match_positions.append(m.start())
                except re.error:
                    continue

            if matched_count == 0:
                results.append(MatchResult(
                    signature=sig, matched=False, confidence=0.0))
                continue

            pattern_coverage = len(matched_patterns) / max(len(sig.patterns), 1)
            priority_weight = sig.priority / 10.0
            position_bonus = min(len(match_positions) / 10.0, 0.2)
            confidence = (pattern_coverage * 0.5
                          + priority_weight * 0.3
                          + position_bonus)
            confidence = min(max(confidence, 0.0), 1.0)

            if confidence >= max(min_confidence, sig.confidence_threshold):
                results.append(MatchResult(
                    signature=sig,
                    matched=True,
                    confidence=round(confidence, 3),
                    matched_patterns=matched_patterns,
                    match_positions=match_positions,
                ))

        results.sort(key=lambda r: r.confidence, reverse=True)
        return results

    def match_build_log(self, build_log: Path,
                        min_confidence: float = 0.0) -> list[MatchResult]:
        """Convenience: match the contents of a build.log file."""
        if not build_log.exists():
            return []
        text = build_log.read_text(encoding="utf-8", errors="replace")
        return self.match(text, min_confidence=min_confidence)

    def best_match(self, text: str, min_confidence: float = 0.5) -> MatchResult | None:
        """Return the single best match, or None if none exceed min_confidence."""
        results = self.match(text, min_confidence=min_confidence)
        if results:
            return results[0]
        return None

    def signatures_by_tag(self, tag: str) -> list[Signature]:
        """Filter loaded signatures by tag."""
        return [s for s in self.load_all() if tag in s.tags]
