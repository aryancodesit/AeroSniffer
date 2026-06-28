"""TFT_eSPI-specific provider: display configuration, processor backend, DMA context."""

import re
from pathlib import Path
from . import EvidenceProvider


class TFTProvider(EvidenceProvider):
    @property
    def name(self): return "tft"

    @property
    def description(self): return "TFT_eSPI setup validation and DMA context"

    @property
    def priority(self) -> int:
        return 15

    def collect(self, evidence_dir, build_dir, arduino15):
        files = []
        tft_dir = evidence_dir / "tft"
        tft_dir.mkdir(parents=True, exist_ok=True)

        build_log = build_dir / "build.log"
        if build_log.exists():
            text = build_log.read_text(encoding="utf-8", errors="replace")

            backend_lines = [l for l in text.splitlines()
                            if "TFT_eSPI_ESP32_S3" in l or "TFT_eSPI_ESP32.cpp" in l]
            if backend_lines:
                (tft_dir / "backend.txt").write_text("\n".join(backend_lines) + "\n")
                files.append(tft_dir / "backend.txt")

            dma_lines = [l for l in text.splitlines() if "dma" in l.lower() and "TFT" in l]
            if dma_lines:
                (tft_dir / "dma_context.txt").write_text("\n".join(dma_lines) + "\n")
                files.append(tft_dir / "dma_context.txt")

        return files
