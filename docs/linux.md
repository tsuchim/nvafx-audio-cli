# Ubuntu SDK-Free Support

`nvafx-audio-cli` `0.2.0` adds first-class Ubuntu SDK-free configure, build, and test support. This foundation is intended for CLI validation, WAV I/O guardrails, dry-run behavior, and repository hygiene without NVIDIA SDK files.

## Prerequisites

Ubuntu 24.04 packages:

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends build-essential cmake ninja-build pkg-config ffmpeg ca-certificates git python3
```

The development container persists the same package set in its Dockerfile. NVIDIA drivers, CUDA, and NVIDIA Audio Effects SDK are intentionally not installed by this phase.

## Configure, Build, Test

```bash
cmake -S . -B build-linux -G Ninja
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
python3 scripts/check_repo_hygiene.py
./build-linux/nvafx-audio-cli --help
./build-linux/nvafx-audio-cli --version
```

`./build-linux/nvafx-audio-cli --version` should print:

```text
nvafx-audio-cli 0.2.0
```

## What Works

- `--help`
- `--version`
- `--dry-run`
- WAV read/write tests
- stdin/stdout WAV guardrail tests
- `--check-sdk` structural checks
- Clear SDK-disabled failure when processing is requested from an SDK-free build
- Cross-platform repository hygiene through `scripts/check_repo_hygiene.py`

## Not In Scope Yet

- NVIDIA Linux SDK-enabled processing
- Loading NVIDIA `.so` runtime libraries
- CUDA setup
- GPU processing assumptions
- Linux release binaries
- `.deb`, AppImage, Snap, or Flatpak packaging

## SDK Artifact Policy

NVIDIA SDK runtime files, shared libraries, DLLs, models, headers, import libraries, generated media, and sample media are not included and must not be committed. SDK/runtime/model paths remain external user-provided inputs for later SDK-enabled work.
