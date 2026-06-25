#!/usr/bin/env python3
"""Cross-platform repository hygiene checks for tracked files."""

from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path


BLOCKED_EXTENSIONS = {
    ".wav",
    ".mp4",
    ".mov",
    ".mkv",
    ".avi",
    ".flac",
    ".aac",
    ".mp3",
    ".dll",
    ".exe",
    ".deb",
    ".lib",
    ".pdb",
    ".ilk",
    ".obj",
    ".exp",
    ".zip",
    ".7z",
    ".tar",
    ".gz",
    ".tgz",
    ".trtpkg",
    ".onnx",
    ".engine",
}

BLOCKED_PATH_PATTERNS = [
    r"(^|/)Audio_Effects_SDK",
    r"(^|/)NVIDIA_Audio_Effects_SDK",
    r"(^|/)NVIDIA-Audio-Effects-SDK",
    r"(^|/)nvidia-audio-effects-sdk",
    r"(^|/)nvidia_sdk",
    r"(^|/)nvidia-sdk",
    r"(^|/)external/nvidia",
    r"(^|/)vendor/nvidia",
    r"(^|/)third_party/nvidia",
    r"(^|/)models(/|$)",
    r"(^|/)features(/|$)",
    r"(^|/)sdk(/|$)",
]

MAX_TRACKED_BYTES = 5 * 1024 * 1024


def git_ls_files() -> list[str]:
    result = subprocess.run(
        ["git", "ls-files"],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return [line for line in result.stdout.splitlines() if line]


def main() -> int:
    tracked_files = git_ls_files()
    violations: list[str] = []
    compiled_patterns = [re.compile(pattern, re.IGNORECASE) for pattern in BLOCKED_PATH_PATTERNS]

    for file_name in tracked_files:
        normalized = file_name.replace("\\", "/")
        extension = Path(file_name).suffix.lower()

        if extension in BLOCKED_EXTENSIONS:
            violations.append(f"blocked extension: {file_name}")

        for pattern in compiled_patterns:
            if pattern.search(normalized):
                violations.append(f"blocked path pattern '{pattern.pattern}': {file_name}")
                break

        if os.path.isfile(file_name):
            size = os.path.getsize(file_name)
            if size > MAX_TRACKED_BYTES:
                violations.append(
                    f"tracked file exceeds {MAX_TRACKED_BYTES} bytes: {file_name} ({size} bytes)"
                )

    if violations:
        print("Repository hygiene failed:", file=sys.stderr)
        for violation in violations:
            print(violation, file=sys.stderr)
        return 1

    print(f"Repository hygiene passed: inspected {len(tracked_files)} tracked files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
