"""Provider-based evidence collection framework.

Each provider implements EvidenceProvider and is discovered automatically.
Provider execution is isolated — one failure never blocks another.
"""

import sys
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from pathlib import Path
import importlib
import inspect
import pkgutil
import time
import traceback


@dataclass
class EvidenceProviderResult:
    """Result of a single provider's collect() execution.

    All fields are populated by the framework. Providers should never
    construct this directly — they return file lists. The framework
    wraps their execution and captures metadata.
    """
    provider_name: str
    provider_version: str
    success: bool
    duration_ms: int
    files: list[Path] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)

    def file_count(self) -> int:
        return len(self.files)

    def has_errors(self) -> bool:
        return len(self.errors) > 0

    def summary(self) -> str:
        status = "OK" if self.success else "FAIL"
        extras = []
        if self.warnings:
            extras.append(f"{len(self.warnings)} warnings")
        if self.errors:
            extras.append(f"{len(self.errors)} errors")
        extra = f" ({', '.join(extras)})" if extras else ""
        return f"  [{status}] {self.provider_name} v{self.provider_version} — {self.file_count()} files in {self.duration_ms}ms{extra}"


class EvidenceProvider(ABC):
    """Base class for all evidence collectors.

    Subclasses must implement:
        name, description, collect()

    The framework discovers subclasses automatically via pkgutil.
    One provider failure never blocks other providers.
    """

    @abstractmethod
    def collect(self, evidence_dir: Path, build_dir: Path, arduino15: Path) -> list[Path]:
        """Collect evidence artifacts. Returns list of created file paths.

        Implementations should NOT catch unexpected exceptions — the
        framework wraps collect() in a try/except and captures failures
        into EvidenceProviderResult.

        Each returned path must exist and reside inside evidence_dir.
        """
        ...

    @property
    @abstractmethod
    def name(self) -> str:
        """Unique provider identifier (e.g. 'compiler', 'linker'). Must be unique."""
        ...

    @property
    @abstractmethod
    def description(self) -> str:
        """Human-readable description of what this provider collects."""
        ...

    @property
    def version(self) -> str:
        """Semantic version of this provider's output schema."""
        return "1.0.0"

    @property
    def priority(self) -> int:
        """Execution priority. Higher values run first (default 0).

        Use priority for cheap-fast checks (e.g. git state) to run
        before expensive-slow checks (e.g. symbol extraction).
        """
        return 0


def _check_duplicate_names(providers: list[EvidenceProvider]) -> None:
    """Validate provider name uniqueness. Raises ValueError on duplicates."""
    seen: dict[str, list[str]] = {}
    for p in providers:
        seen.setdefault(p.name, []).append(type(p).__module__)
    dupes = {name: mods for name, mods in seen.items() if len(mods) > 1}
    if dupes:
        msg_parts = []
        for name, mods in sorted(dupes.items()):
            msg_parts.append(f"  '{name}' defined in {', '.join(mods)}")
        raise ValueError(
            f"Duplicate provider name(s) detected:\n" + "\n".join(msg_parts))


def _validate_provider_output(files: list[Path],
                              evidence_dir: Path) -> tuple[list[Path], list[str]]:
    """Validate provider output. Returns (validated_deduplicated_files, warnings).

    Checks:
    - Every path exists on disk
    - Every path is inside evidence_dir (no escaping the sandbox)
    - No duplicate paths
    """
    warnings: list[str] = []
    validated: list[Path] = []
    seen: set[Path] = set()

    for f in files:
        try:
            resolved = f.resolve()
        except (OSError, RuntimeError):
            warnings.append(f"  cannot resolve path: {f}")
            continue

        if not resolved.exists():
            warnings.append(f"  file does not exist: {f}")
            continue

        try:
            resolved.relative_to(evidence_dir.resolve())
        except ValueError:
            warnings.append(f"  file outside evidence directory: {f}")
            continue

        if resolved in seen:
            warnings.append(f"  duplicate file: {f}")
            continue

        seen.add(resolved)
        validated.append(f)

    return validated, warnings


def _discover_providers() -> list[EvidenceProvider]:
    """Dynamically discover all EvidenceProvider subclasses in the providers package.

    Uses pkgutil to find modules, then inspects each for EvidenceProvider
    subclasses (excluding the ABC itself). No hardcoded import list needed.
    """
    providers = []
    package_name = __name__

    for importer, modname, ispkg in pkgutil.iter_modules(
            __path__, prefix=f"{package_name}."):
        if ispkg:
            continue
        if modname == f"{package_name}.__init__":
            continue
        try:
            module = importlib.import_module(modname)
        except Exception as exc:
            print(f"  warning: failed to import {modname}: {exc}", file=sys.stderr)
            continue

        for _, cls in inspect.getmembers(module, inspect.isclass):
            if (issubclass(cls, EvidenceProvider)
                    and cls is not EvidenceProvider
                    and not inspect.isabstract(cls)):
                try:
                    instance = cls()
                    providers.append(instance)
                except Exception as exc:
                    print(f"  warning: failed to instantiate {cls.__name__}: {exc}",
                          file=sys.stderr)

    _check_duplicate_names(providers)
    return providers


def _exec_provider(provider: EvidenceProvider,
                   evidence_dir: Path,
                   build_dir: Path,
                   arduino15: Path) -> EvidenceProviderResult:
    """Execute a single provider with full isolation and timing."""
    name = provider.name
    version = provider.version
    warnings: list[str] = []
    errors: list[str] = []
    files: list[Path] = []
    start = time.monotonic()

    try:
        collected = provider.collect(evidence_dir, build_dir, arduino15)
        if collected is None:
            collected = []
        raw_files = list(collected)
        validated_files, val_warnings = _validate_provider_output(raw_files, evidence_dir)
        files = validated_files
        warnings.extend(val_warnings)
        if len(val_warnings) > len(raw_files):
            success = False
            errors.append("All provider output files were invalid")
        else:
            success = True
    except Exception as exc:
        errors.append(f"{type(exc).__name__}: {exc}")
        tb = traceback.format_exc()
        errors.append(tb)
        success = False

    duration_ms = int((time.monotonic() - start) * 1000)
    return EvidenceProviderResult(
        provider_name=name,
        provider_version=version,
        success=success,
        duration_ms=duration_ms,
        files=files,
        warnings=warnings,
        errors=errors,
    )


def collect_all(evidence_dir: Path,
                build_dir: Path,
                arduino15: Path) -> list[EvidenceProviderResult]:
    """Run all discovered providers with full isolation.

    Providers are executed in priority order (highest priority first).
    Returns a list of EvidenceProviderResult, one per provider.
    Failures are captured in results, never propagated.
    """
    providers = _discover_providers()
    providers.sort(key=lambda p: (-p.priority, p.name))
    results: list[EvidenceProviderResult] = []

    for provider in providers:
        result = _exec_provider(provider, evidence_dir, build_dir, arduino15)
        results.append(result)
        print(result.summary(), file=sys.stderr)

    return results
