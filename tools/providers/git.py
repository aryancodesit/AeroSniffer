"""Git state snapshot provider."""

import subprocess
import shutil
from pathlib import Path
from . import EvidenceProvider


class GitProvider(EvidenceProvider):
    @property
    def name(self): return "git"

    @property
    def description(self): return "Git state, diff, and status"

    @property
    def priority(self) -> int:
        return 20

    def collect(self, evidence_dir, build_dir, arduino15):
        files = []
        git_dir = evidence_dir / "git"
        git_dir.mkdir(parents=True, exist_ok=True)

        cmds = [
            (["git", "describe", "--tags", "--dirty"], "describe.txt"),
            (["git", "diff"], "diff.txt"),
            (["git", "status", "--porcelain"], "status.txt"),
            (["git", "log", "--oneline", "-20"], "log.txt"),
        ]
        for cmd, name in cmds:
            try:
                out = subprocess.run(cmd, capture_output=True, stderr=subprocess.DEVNULL, text=True, errors="replace", timeout=10,
                                     cwd=evidence_dir.parents[1])
                content = out.stdout if out.stdout.strip() else "(clean)\n"
                (git_dir / name).write_text(content)
                files.append(git_dir / name)
            except Exception:
                (git_dir / name).write_text("(unavailable)\n")
                files.append(git_dir / name)

        return files
