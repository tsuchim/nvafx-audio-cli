#!/usr/bin/env python3
"""Validate the SDK-free Debian package contents."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path


EXPECTED_PACKAGE = "nvafx-audio-cli"
EXPECTED_VERSION = "0.3.0"
EXPECTED_ARCHITECTURE = "amd64"

REQUIRED_CONTENTS = {
    "./usr/bin/nvafx-audio-cli",
    "./usr/share/doc/nvafx-audio-cli/README.md",
    "./usr/share/doc/nvafx-audio-cli/LICENSE",
    "./usr/share/doc/nvafx-audio-cli/THIRD_PARTY_NOTICES.md",
    "./usr/share/doc/nvafx-audio-cli/design.md",
    "./usr/share/doc/nvafx-audio-cli/linux.md",
    "./usr/share/doc/nvafx-audio-cli/release-packaging.md",
}

FORBIDDEN_CONTENT_PATTERNS = [
    re.compile(pattern, re.IGNORECASE)
    for pattern in [
        r"NVIDIA[-_ ]?Audio[-_ ]?Effects[-_ ]?SDK",
        r"Audio_Effects_SDK",
        r"(^|/)models?(/|$)",
        r"\.trtpkg$",
        r"\.onnx$",
        r"\.engine$",
        r"\.(wav|mp4|mov|mkv|avi|flac|aac|mp3)$",
        r"\.(dll|lib|pdb|ilk|obj|exp|zip|msi|exe)$",
    ]
]


def run_dpkg_deb(*args: str) -> str:
    result = subprocess.run(
        ["dpkg-deb", *args],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return result.stdout


def parse_control(control: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    current_key: str | None = None

    for line in control.splitlines():
        if not line:
            current_key = None
            continue
        if line.startswith(" ") and current_key:
            fields[current_key] += "\n" + line
            continue
        key, separator, value = line.partition(":")
        if separator:
            current_key = key
            fields[key] = value.strip()

    return fields


def normalize_content_path(line: str) -> str | None:
    parts = line.split()
    if not parts:
        return None
    return parts[-1].rstrip("/")


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: check_deb_package.py <package.deb>", file=sys.stderr)
        return 2

    deb_path = Path(argv[1])
    if not deb_path.is_file():
        print(f"Debian package not found: {deb_path}", file=sys.stderr)
        return 2

    control = parse_control(run_dpkg_deb("--field", str(deb_path)))
    violations: list[str] = []

    expected_fields = {
        "Package": EXPECTED_PACKAGE,
        "Version": EXPECTED_VERSION,
        "Architecture": EXPECTED_ARCHITECTURE,
    }
    for key, expected in expected_fields.items():
        actual = control.get(key)
        if actual != expected:
            violations.append(f"{key}: expected {expected!r}, got {actual!r}")

    description = control.get("Description", "")
    if "SDK-free" not in description:
        violations.append("Description must state that the package is SDK-free")
    for required_phrase in [
        "cannot perform real NVIDIA Audio Effects processing by itself",
        "externally supplied NVIDIA Audio Effects SDK",
        "model .trtpkg",
        "Public packages intentionally omit NVIDIA SDK/runtime/model/CUDA material",
    ]:
        if required_phrase not in description:
            violations.append(f"Description must contain: {required_phrase!r}")

    listing = run_dpkg_deb("--contents", str(deb_path))
    contents = {path for line in listing.splitlines() if (path := normalize_content_path(line))}

    for required in sorted(REQUIRED_CONTENTS):
        if required not in contents:
            violations.append(f"missing package content: {required}")

    for package_path in sorted(contents):
        for pattern in FORBIDDEN_CONTENT_PATTERNS:
            if pattern.search(package_path):
                violations.append(f"forbidden package content: {package_path}")
                break

    if violations:
        print("Debian package validation failed:", file=sys.stderr)
        for violation in violations:
            print(violation, file=sys.stderr)
        return 1

    print(
        "Debian package validation passed: "
        f"{control['Package']} {control['Version']} {control['Architecture']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
