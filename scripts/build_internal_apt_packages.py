#!/usr/bin/env python3
"""Build internal SDK-enabled Debian packages for local APT use."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


PACKAGE_VERSION = "0.3.1+eikai3"
PACKAGE_ARCH = "amd64"


class PackageError(RuntimeError):
    pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build internal nvafx-audio-cli Debian package split without NVIDIA artifacts."
    )
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--model", required=True, type=Path)
    parser.add_argument("--output-dir", default=Path("build-internal-apt/packages"), type=Path)
    parser.add_argument("--build-root", default=Path("build-internal-apt"), type=Path)
    parser.add_argument("--version", default=PACKAGE_VERSION)
    parser.add_argument("--build-type", default="RelWithDebInfo")
    parser.add_argument("--generator", default="Ninja")
    return parser.parse_args()


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def run(command: list[str], *, cwd: Path) -> None:
    print("+ " + " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def require_tool(name: str) -> None:
    if shutil.which(name) is None:
        raise PackageError(f"required tool is unavailable: {name}")


def resolve(path: Path) -> Path:
    path = path.expanduser()
    if path.is_absolute():
        return path
    return (Path.cwd() / path).resolve()


def write(path: Path, text: str, mode: int = 0o644) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    path.chmod(mode)


def copy(src: Path, dst: Path, mode: int = 0o644) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    dst.chmod(mode)


def control(package: str, version: str, architecture: str, depends: str, description: str, extra: str = "") -> str:
    lines = [
        f"Package: {package}",
        f"Version: {version}",
        "Section: sound",
        "Priority: optional",
        f"Architecture: {architecture}",
        "Maintainer: Eikai Intelligent Systems <packages@eikai.co.jp>",
    ]
    if depends:
        lines.append(f"Depends: {depends}")
    if extra:
        lines.extend(extra.rstrip().splitlines())
    lines.append(f"Description: {description.splitlines()[0]}")
    for line in description.splitlines()[1:]:
        lines.append(f" {line}" if line else " .")
    return "\n".join(lines) + "\n"


def build_deb(root: Path, output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    debian = root / "DEBIAN"
    control_text = (debian / "control").read_text(encoding="utf-8")
    fields = {}
    for line in control_text.splitlines():
        key, sep, value = line.partition(":")
        if sep:
            fields[key] = value.strip()
    package = fields["Package"]
    version = fields["Version"]
    arch = fields["Architecture"]
    deb_path = output_dir / f"{package}_{version}_{arch}.deb"
    run(["dpkg-deb", "--build", str(root), str(deb_path)], cwd=repo_root())
    return deb_path


def package_root(build_root: Path, package: str) -> Path:
    root = build_root / "pkgroot" / package
    if root.exists():
        shutil.rmtree(root)
    (root / "DEBIAN").mkdir(parents=True)
    return root


def assert_no_embedded_sdk_path(binary: Path, sdk_root: Path) -> None:
    readelf = shutil.which("readelf")
    if readelf is None:
        print("warning: readelf is unavailable; skipping binary RPATH/RUNPATH inspection", file=sys.stderr)
        return
    result = subprocess.run(
        [readelf, "-d", str(binary)],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    dynamic_paths = [
        line
        for line in result.stdout.splitlines()
        if "RPATH" in line or "RUNPATH" in line
    ]
    if not dynamic_paths:
        return
    sdk_markers = [str(sdk_root), "Audio_Effects_SDK", "/workspace/_external"]
    for line in dynamic_paths:
        if any(marker in line for marker in sdk_markers):
            raise PackageError(f"installed binary embeds SDK path in dynamic section: {line.strip()}")


def configure_builds(args: argparse.Namespace) -> tuple[Path, Path]:
    root = repo_root()
    sdk_root = resolve(args.sdk_root)
    sdk_free_build = resolve(args.build_root) / "sdk-free"
    sdk_enabled_build = resolve(args.build_root) / "sdk-enabled"
    sdk_free_install = resolve(args.build_root) / "sdk-free-install"
    sdk_enabled_install = resolve(args.build_root) / "sdk-enabled-install"

    run(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            str(sdk_free_build),
            "-G",
            args.generator,
            f"-DCMAKE_BUILD_TYPE={args.build_type}",
            "-DNVAFX_ENABLE_SDK=OFF",
        ],
        cwd=root,
    )
    run(["cmake", "--build", str(sdk_free_build), "--target", "nvafx-audio-cli"], cwd=root)
    if sdk_free_install.exists():
        shutil.rmtree(sdk_free_install)
    run(["cmake", "--install", str(sdk_free_build), "--prefix", str(sdk_free_install)], cwd=root)

    run(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            str(sdk_enabled_build),
            "-G",
            args.generator,
            f"-DCMAKE_BUILD_TYPE={args.build_type}",
            "-DNVAFX_ENABLE_SDK=ON",
            f"-DNVAFX_RUNTIME_ROOT={sdk_root}",
        ],
        cwd=root,
    )
    run(["cmake", "--build", str(sdk_enabled_build), "--target", "nvafx-audio-cli"], cwd=root)
    if sdk_enabled_install.exists():
        shutil.rmtree(sdk_enabled_install)
    run(["cmake", "--install", str(sdk_enabled_build), "--prefix", str(sdk_enabled_install)], cwd=root)

    sdk_free_binary = sdk_free_install / "bin" / "nvafx-audio-cli"
    sdk_enabled_binary = sdk_enabled_install / "bin" / "nvafx-audio-cli"
    assert_no_embedded_sdk_path(sdk_enabled_binary, sdk_root)
    return sdk_free_binary, sdk_enabled_binary


def package_validation(args: argparse.Namespace, build_root: Path, output_dir: Path, sdk_free_binary: Path) -> Path:
    root = package_root(build_root, "nvafx-audio-cli-validation")
    write(
        root / "DEBIAN" / "control",
        control(
            "nvafx-audio-cli-validation",
            args.version,
            PACKAGE_ARCH,
            "libc6, libstdc++6",
            """SDK-free validation command for NVIDIA Audio Effects workflows
This package installs nvafx-audio-cli-validation only. It cannot perform real
NVIDIA denoiser processing and does not provide the primary nvafx-audio-cli
command. Install nvafx-audio-cli for the internal SDK-enabled command chain.""",
        ),
    )
    copy(sdk_free_binary, root / "usr" / "bin" / "nvafx-audio-cli-validation", 0o755)
    return build_deb(root, output_dir)


def package_sdk_present(args: argparse.Namespace, build_root: Path, output_dir: Path) -> Path:
    root = package_root(build_root, "nvafx-audio-effects-sdk-present")
    write(
        root / "DEBIAN" / "control",
        control(
            "nvafx-audio-effects-sdk-present",
            args.version,
            "all",
            "",
            """NVIDIA Audio Effects SDK presence check for nvafx-audio-cli
This package contains no NVIDIA SDK files. Its postinst verifies the
operator-provided SDK paths configured in /etc/nvafx-audio-cli/sdk.env.""",
        ),
    )
    write(root / "DEBIAN" / "conffiles", "/etc/nvafx-audio-cli/sdk.env\n")
    write(
        root / "etc" / "nvafx-audio-cli" / "sdk.env",
        """NVAFX_SDK_ROOT=/opt/nvidia/Audio_Effects_SDK
NVAFX_DENOISER_MODEL=/opt/nvidia/Audio_Effects_SDK/features/denoiser/models/sm_89/denoiser_48k.trtpkg
""",
    )
    write(
        root / "DEBIAN" / "postinst",
        r"""#!/bin/sh
set -e
[ "$1" = configure ] || exit 0
. /etc/nvafx-audio-cli/sdk.env
fail() { echo "nvafx-audio-effects-sdk-present: $*" >&2; exit 1; }
[ -d "$NVAFX_SDK_ROOT" ] || fail "NVAFX_SDK_ROOT does not exist: $NVAFX_SDK_ROOT"
[ -f "$NVAFX_SDK_ROOT/nvafx/include/nvAudioEffects.h" ] || fail "missing SDK header: $NVAFX_SDK_ROOT/nvafx/include/nvAudioEffects.h"
ls "$NVAFX_SDK_ROOT"/nvafx/lib/libnv_audiofx.so "$NVAFX_SDK_ROOT"/nvafx/lib/libnv_audiofx.so.* >/dev/null 2>&1 || fail "missing SDK base library under $NVAFX_SDK_ROOT/nvafx/lib"
ls "$NVAFX_SDK_ROOT"/features/denoiser/lib/libnv_audiofx_denoiser.so "$NVAFX_SDK_ROOT"/features/denoiser/lib/libnv_audiofx_denoiser.so.* >/dev/null 2>&1 || fail "missing denoiser feature library under $NVAFX_SDK_ROOT/features/denoiser/lib"
exit 0
""",
        0o755,
    )
    return build_deb(root, output_dir)


def package_model_present(args: argparse.Namespace, build_root: Path, output_dir: Path) -> Path:
    root = package_root(build_root, "nvafx-audio-effects-denoiser-model-48k-present")
    write(
        root / "DEBIAN" / "control",
        control(
            "nvafx-audio-effects-denoiser-model-48k-present",
            args.version,
            "all",
            "nvafx-audio-effects-sdk-present",
            """NVIDIA denoiser 48 kHz model presence check for nvafx-audio-cli
This package contains no model files. Its postinst verifies the operator-
provided denoiser .trtpkg path configured in /etc/nvafx-audio-cli/sdk.env.""",
        ),
    )
    write(
        root / "DEBIAN" / "postinst",
        r"""#!/bin/sh
set -e
[ "$1" = configure ] || exit 0
. /etc/nvafx-audio-cli/sdk.env
fail() { echo "nvafx-audio-effects-denoiser-model-48k-present: $*" >&2; exit 1; }
[ -n "${NVAFX_DENOISER_MODEL:-}" ] || fail "NVAFX_DENOISER_MODEL is not set in /etc/nvafx-audio-cli/sdk.env"
case "$NVAFX_DENOISER_MODEL" in
  *.trtpkg) ;;
  *) fail "NVAFX_DENOISER_MODEL must end with .trtpkg: $NVAFX_DENOISER_MODEL" ;;
esac
[ -f "$NVAFX_DENOISER_MODEL" ] || fail "denoiser model does not exist: $NVAFX_DENOISER_MODEL"
exit 0
""",
        0o755,
    )
    return build_deb(root, output_dir)


def package_gpu_present(args: argparse.Namespace, build_root: Path, output_dir: Path) -> Path:
    root = package_root(build_root, "nvafx-gpu-runtime-visible")
    write(
        root / "DEBIAN" / "control",
        control(
            "nvafx-gpu-runtime-visible",
            args.version,
            "all",
            "",
            """NVIDIA GPU runtime visibility check for nvafx-audio-cli
This package does not install NVIDIA drivers or CUDA. Its postinst verifies
that libcuda.so.1 and GPU query capability are visible to this environment.""",
        ),
    )
    write(
        root / "DEBIAN" / "postinst",
        r"""#!/bin/sh
set -e
[ "$1" = configure ] || exit 0
fail() { echo "nvafx-gpu-runtime-visible: $*" >&2; exit 1; }
if ! ldconfig -p 2>/dev/null | grep -q 'libcuda[.]so[.]1'; then
  [ -f /usr/lib/x86_64-linux-gnu/libcuda.so.1 ] || [ -f /usr/lib/wsl/lib/libcuda.so.1 ] || [ -f /usr/local/cuda/compat/libcuda.so.1 ] || fail "libcuda.so.1 is not visible through ldconfig or standard paths"
fi
if command -v nvidia-smi >/dev/null 2>&1; then
  nvidia-smi >/dev/null 2>&1 || fail "nvidia-smi is present but cannot query GPU runtime"
elif [ -x /usr/lib/wsl/lib/nvidia-smi ]; then
  /usr/lib/wsl/lib/nvidia-smi >/dev/null 2>&1 || fail "WSL nvidia-smi is present but cannot query GPU runtime"
elif [ ! -e /dev/dxg ] && ! ls /dev/nvidia* >/dev/null 2>&1; then
  fail "no nvidia-smi, /dev/dxg, or /dev/nvidia* GPU runtime signal is visible"
fi
exit 0
""",
        0o755,
    )
    return build_deb(root, output_dir)


def package_sdk_enabled(args: argparse.Namespace, build_root: Path, output_dir: Path, sdk_binary: Path) -> Path:
    root = package_root(build_root, "nvafx-audio-cli-sdk-enabled")
    depends = (
        f"nvafx-audio-effects-sdk-present (= {args.version}), "
        f"nvafx-audio-effects-denoiser-model-48k-present (= {args.version}), "
        f"nvafx-gpu-runtime-visible (= {args.version}), libc6, libstdc++6, python3"
    )
    write(
        root / "DEBIAN" / "control",
        control(
            "nvafx-audio-cli-sdk-enabled",
            args.version,
            PACKAGE_ARCH,
            depends,
            """SDK-enabled NVIDIA denoiser command for nvafx-audio-cli
Installs only project-built binaries and wrapper/config. It requires external
operator-provided NVIDIA SDK, denoiser model, and GPU runtime to be present
and verified during package configuration.""",
            extra=f"Replaces: nvafx-audio-cli (<< {args.version})\nBreaks: nvafx-audio-cli (<< {args.version})",
        ),
    )
    copy(sdk_binary, root / "usr" / "lib" / "nvafx-audio-cli" / "nvafx-audio-cli-sdk-enabled", 0o755)
    write(
        root / "usr" / "bin" / "nvafx-audio-cli",
        r"""#!/bin/sh
set -e
. /etc/nvafx-audio-cli/sdk.env
export LD_LIBRARY_PATH="$NVAFX_SDK_ROOT/nvafx/lib:$NVAFX_SDK_ROOT/features/denoiser/lib:${LD_LIBRARY_PATH:-}"

has_model=0
has_runtime=0
has_api=0
has_check=0
for arg in "$@"; do
  [ "$arg" = "--model" ] && has_model=1
  [ "$arg" = "--runtime-root" ] && has_runtime=1
  [ "$arg" = "--sdk-root" ] && has_runtime=1
  [ "$arg" = "--api-root" ] && has_api=1
  [ "$arg" = "--check-sdk" ] && has_check=1
done

case " $* " in
  *" --version "*|*" --help "*) exec /usr/lib/nvafx-audio-cli/nvafx-audio-cli-sdk-enabled "$@" ;;
esac

if [ "$has_runtime" -eq 0 ]; then
  set -- "$@" --runtime-root "$NVAFX_SDK_ROOT"
fi
if [ "$has_check" -eq 1 ] && [ "$has_api" -eq 0 ]; then
  set -- "$@" --api-root "$NVAFX_SDK_ROOT/nvafx"
fi
if [ "$has_model" -eq 0 ]; then
  set -- "$@" --model "$NVAFX_DENOISER_MODEL"
fi

exec /usr/lib/nvafx-audio-cli/nvafx-audio-cli-sdk-enabled "$@"
""",
        0o755,
    )
    write(
        root / "DEBIAN" / "postinst",
        r"""#!/bin/sh
set -e
[ "$1" = configure ] || exit 0
. /etc/nvafx-audio-cli/sdk.env
fail() { echo "nvafx-audio-cli-sdk-enabled: $*" >&2; exit 1; }
nvafx-audio-cli --help >/dev/null
nvafx-audio-cli --check-sdk --api-root "$NVAFX_SDK_ROOT/nvafx" --runtime-root "$NVAFX_SDK_ROOT" --model "$NVAFX_DENOISER_MODEL" >/dev/null
tmpdir="$(mktemp -d)"
cleanup() { rm -rf "$tmpdir"; }
trap cleanup EXIT
python3 - "$tmpdir/in.wav" <<'PY'
import math, struct, sys, wave
path = sys.argv[1]
with wave.open(path, "wb") as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(48000)
    frames = bytearray()
    for i in range(480):
        sample = int(1200 * math.sin(i / 8.0))
        frames.extend(struct.pack("<h", sample))
    wav.writeframes(bytes(frames))
PY
nvafx-audio-cli --input "$tmpdir/in.wav" --output "$tmpdir/out.wav" --effect denoiser --sample-rate 48000 --intensity 1.0 >/dev/null
cat "$tmpdir/in.wav" | nvafx-audio-cli --input - --output - --effect denoiser --sample-rate 48000 --intensity 1.0 --allow-unsafe-pipe > "$tmpdir/out-stdio.wav" 2>"$tmpdir/stdio.err"
[ ! -s "$tmpdir/stdio.err" ] || fail "stdin/stdout smoke test wrote stderr diagnostics"
python3 - "$tmpdir/out.wav" "$tmpdir/out-stdio.wav" <<'PY'
import struct, sys

def check(path):
    data = open(path, "rb").read()
    assert len(data) > 44, path
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE", path
    pos = 12
    fmt = None
    payload = 0
    while pos + 8 <= len(data):
        chunk_id = data[pos:pos + 4]
        size = struct.unpack_from("<I", data, pos + 4)[0]
        body = data[pos + 8:pos + 8 + size]
        if chunk_id == b"fmt " and len(body) >= 16:
            audio_format, channels, sample_rate = struct.unpack_from("<HHI", body, 0)
            fmt = (audio_format, channels, sample_rate)
        elif chunk_id == b"data":
            payload = size
        pos += 8 + size + (size % 2)
    assert fmt is not None, path
    audio_format, channels, sample_rate = fmt
    assert audio_format in (1, 3, 65534), (path, audio_format)
    assert channels == 1, (path, channels)
    assert sample_rate == 48000, (path, sample_rate)
    assert payload > 0, path

for path in sys.argv[1:]:
    check(path)
PY
exit 0
""",
        0o755,
    )
    return build_deb(root, output_dir)


def package_meta(args: argparse.Namespace, build_root: Path, output_dir: Path) -> Path:
    root = package_root(build_root, "nvafx-audio-cli")
    write(
        root / "DEBIAN" / "control",
        control(
            "nvafx-audio-cli",
            args.version,
            "all",
            f"nvafx-audio-cli-sdk-enabled (= {args.version})",
            """Real NVIDIA Audio Effects offline denoiser CLI
Installs the real NVIDIA Audio Effects offline denoiser CLI for environments
where the operator-provided NVIDIA Audio Effects SDK, denoiser model, and GPU
runtime are present and verified.""",
        ),
    )
    return build_deb(root, output_dir)


def main() -> int:
    args = parse_args()
    try:
        for tool in ["cmake", "dpkg-deb"]:
            require_tool(tool)
        if args.generator.lower() == "ninja":
            require_tool("ninja")

        args.sdk_root = resolve(args.sdk_root)
        args.model = resolve(args.model)
        if not args.sdk_root.is_dir():
            raise PackageError(f"SDK root does not exist: {args.sdk_root}")
        if not args.model.is_file():
            raise PackageError(f"model does not exist: {args.model}")

        build_root = resolve(args.build_root)
        output_dir = resolve(args.output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)

        sdk_free_binary, sdk_enabled_binary = configure_builds(args)
        packages = [
            package_validation(args, build_root, output_dir, sdk_free_binary),
            package_sdk_present(args, build_root, output_dir),
            package_model_present(args, build_root, output_dir),
            package_gpu_present(args, build_root, output_dir),
            package_sdk_enabled(args, build_root, output_dir, sdk_enabled_binary),
            package_meta(args, build_root, output_dir),
        ]
        print("Built internal packages:")
        for package in packages:
            print(package)
        return 0
    except (PackageError, subprocess.CalledProcessError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
