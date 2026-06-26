# Ubuntu Linux Support

`nvafx-audio-cli` supports two Linux workflows:

- SDK-free Ubuntu/Debian builds and `.deb` packages for CLI checks, WAV I/O guardrails, dry-run behavior, repository hygiene, and package structure validation.
- Local Linux SDK-enabled builds for real NVIDIA Audio Effects SDK processing with an externally installed SDK, feature package, models, compatible driver/runtime, and GPU.

## Prerequisites

Ubuntu 24.04 packages:

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends build-essential cmake ninja-build pkg-config ffmpeg ca-certificates git python3 dpkg-dev file
```

The SDK-free package does not install NVIDIA drivers, CUDA, NVIDIA Audio Effects SDK files, feature libraries, or models.

## SDK-Free Configure, Build, Test

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

## SDK-Free Debian Package

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

This package is SDK-free. It does not include NVIDIA SDK runtime files, shared libraries, models, CUDA setup, generated media, or sample media. It can validate CLI behavior and SDK tree structure, but it cannot perform real NVIDIA processing.

Public Linux packages should remain SDK-free until SDK-enabled binary/package policy, license review, and runtime path handling are complete. See `docs/sdk-enabled-distribution-policy.md`.

## Linux SDK-Enabled Local Build

Use an externally extracted NVIDIA Audio Effects SDK. Do not vendor or commit SDK files, feature libraries, models, CUDA redistributables, generated media, or sample media.

Expected Linux SDK layout:

```text
Audio_Effects_SDK/
  nvafx/include/nvAudioEffects.h
  nvafx/lib/libnv_audiofx.so
  external/cuda/lib/
  features/denoiser/lib/libnv_audiofx_denoiser.so
  features/denoiser/models/<arch>/denoiser_48k.trtpkg
```

Configure and build:

```bash
SDK_ROOT=/path/to/Audio_Effects_SDK
cmake -S . -B build-linux-sdk -G Ninja \
  -DNVAFX_ENABLE_SDK=ON \
  -DNVAFX_RUNTIME_ROOT="$SDK_ROOT" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-linux-sdk
```

`NVAFX_RUNTIME_ROOT` points to `Audio_Effects_SDK`. If `NVAFX_API_ROOT` is not provided, CMake uses `$NVAFX_RUNTIME_ROOT/nvafx` when the header is present there.

Check the SDK structure:

```bash
./build-linux-sdk/nvafx-audio-cli --check-sdk \
  --api-root "$SDK_ROOT/nvafx" \
  --runtime-root "$SDK_ROOT"
```

Run a local denoiser model:

```bash
./build-linux-sdk/nvafx-audio-cli \
  --input input-48k-mono.wav \
  --output output.wav \
  --effect denoiser \
  --sample-rate 48000 \
  --model "$SDK_ROOT/features/denoiser/models/sm_89/denoiser_48k.trtpkg" \
  --runtime-root "$SDK_ROOT"
```

The local build records build-tree `RPATH` entries for the SDK core, bundled CUDA libraries, and feature libraries so the SDK can load `libnv_audiofx_<effect>.so` during local validation. Installed or relocated SDK-enabled binaries may still require an appropriate `LD_LIBRARY_PATH`.

## Manual SDK CTest

Public CI remains SDK-free and does not download SDK material. A local SDK-enabled build can opt into a manual processing test by setting:

```bash
export NVAFX_LINUX_SDK_ROOT=/path/to/Audio_Effects_SDK
export NVAFX_LINUX_FEATURE_ROOT="$NVAFX_LINUX_SDK_ROOT/features/denoiser"
export NVAFX_TEST_MODEL="$NVAFX_LINUX_FEATURE_ROOT/models/sm_89/denoiser_48k.trtpkg"
ctest --test-dir build-linux-sdk -R manual-linux-sdk-processing --output-on-failure
```

The test skips when those variables are absent.

## WSL and Containers

WSL 2 and containers inside WSL can expose GPU runtime differently from a normal Linux host. Check more than `nvidia-smi` on `PATH`:

- WSL-style paths: `/usr/lib/wsl/lib/libcuda.so.1`, `/usr/lib/wsl/lib/nvidia-smi`, and `/dev/dxg`.
- Normal Linux/container paths: `/dev/nvidia*`, `nvidia-smi`, `ldconfig -p | grep libcuda`, `/usr/lib/x86_64-linux-gnu/libcuda.so.1`, and `/usr/local/cuda/compat/libcuda.so.1`.
- Linker resolution: `ldd "$SDK_ROOT/nvafx/lib/libnv_audiofx.so"` should resolve `libcuda.so.1`.

If `/usr/lib/wsl/lib/libcuda.so.1` exists but `ldd` cannot see it, test with a process-local `LD_LIBRARY_PATH=/usr/lib/wsl/lib:$LD_LIBRARY_PATH`. If the environment is a container and the host GPU libraries are absent, relaunch the container with GPU runtime/libcuda visibility.

## NGC and Secrets

NGC is used only to acquire SDK features and models. API keys must not be stored in the repository, sidecar reports, logs, documentation, issues, PRs, or comments. Existing SDK/model material should be reused rather than redownloaded.

## SDK-Free What Works

- `--help`
- `--version`
- `--dry-run`
- WAV read/write tests
- stdin/stdout WAV guardrail tests
- `--check-sdk` structural checks
- Clear SDK-disabled failure when processing is requested from an SDK-free build
- Cross-platform repository hygiene through `scripts/check_repo_hygiene.py`
- SDK-free Debian package structure validation through `scripts/check_deb_package.py`
- Clear separation from the local SDK-enabled build path

## Not In Scope For SDK-Free Packages

- Bundling NVIDIA Linux SDK-enabled processing
- Bundling NVIDIA `.so` runtime libraries
- Bundling CUDA setup
- GPU processing assumptions
- APT repository publishing
- APT repository signing
- AppImage, Snap, or Flatpak packaging

## APT Publishing

APT repository publishing remains future work. APT signing identity and public key inventory are centrally managed by `local-infra`, but this project does not publish an APT repository and does not use production APT signing in the v0.2.1 package workflow.

## SDK Artifact Policy

NVIDIA SDK runtime files, shared libraries, DLLs, models, headers, import libraries, generated media, and sample media are not included and must not be committed. SDK/runtime/model paths remain external user-provided inputs for SDK-enabled local builds.

Do not publish SDK-enabled `.deb` packages or public SDK-enabled Linux binaries until the distribution policy is updated after license review.
