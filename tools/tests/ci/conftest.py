"""Shared fixtures and configuration for CI regression tests."""
import pytest


def pytest_configure(config):
    """Register custom markers."""
    config.addinivalue_line(
        "markers",
        "pack(name): evidence pack scenario name (legacy, prefer metadata.yaml)",
    )
