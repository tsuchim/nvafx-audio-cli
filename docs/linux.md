# Ubuntu SDK-Free Support

`nvafx-audio-cli` `0.2.0` adds first-class Ubuntu SDK-free configure, build, and test support. The `0.2.1` source line adds Ubuntu/Debian `.deb` packaging for that SDK-free build. This foundation is intended for CLI validation, WAV I/O guardrails, dry-run behavior, repository hygiene, and package structure validation without NVIDIA SDK files.

## Prerequisites

Ubuntu 24.04 packages:

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends build-essential cmake ninja-build pkg-config ffmpeg ca-certificates git python3 dpkg-dev file
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
nvafx-audio-cli 0.2.1
```

## Debian Package

Build the SDK-free `.deb` package:

```bash
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-linux
cpack --config build-linux/CPackConfig.cmake -G DEB -B build-linux/package
python3 scripts/check_deb_package.py build-linux/package/nvafx-audio-cli_0.2.1_amd64.deb
```

The package installs:

```text
/usr/bin/nvafx-audio-cli
/usr/share/doc/nvafx-audio-cli/
```

After a release provides the `.deb`, install it manually:

```bash
sudo apt install ./nvafx-audio-cli_0.2.1_amd64.deb
```

This package is SDK-free. It does not include NVIDIA SDK runtime files, shared libraries, models, CUDA setup, generated media, or sample media. NVIDIA Linux SDK-enabled processing remains future work.

## What Works

- `--help`
- `--version`
- `--dry-run`
- WAV read/write tests
- stdin/stdout WAV guardrail tests
- `--check-sdk` structural checks
- Clear SDK-disabled failure when processing is requested from an SDK-free build
- Cross-platform repository hygiene through `scripts/check_repo_hygiene.py`
- SDK-free Debian package structure validation through `scripts/check_deb_package.py`

## Not In Scope Yet

- NVIDIA Linux SDK-enabled processing
- Loading NVIDIA `.so` runtime libraries
- CUDA setup
- GPU processing assumptions
- APT repository publishing
- APT repository signing
- AppImage, Snap, or Flatpak packaging

## APT Publishing

APT repository publishing remains future work. APT signing identity and public key inventory are centrally managed by `local-infra`, but this project does not publish an APT repository and does not use production APT signing in the v0.2.1 package workflow.

## SDK Artifact Policy

NVIDIA SDK runtime files, shared libraries, DLLs, models, headers, import libraries, generated media, and sample media are not included and must not be committed. SDK/runtime/model paths remain external user-provided inputs for later SDK-enabled work.
