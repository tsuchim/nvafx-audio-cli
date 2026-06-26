# nvafx-audio-cli

Small offline WAV processing CLI for NVIDIA Audio Effects SDK.

This project is a small command-line tool that processes WAV files offline through a locally installed NVIDIA Audio Effects SDK. Ubuntu SDK-free build and test support starts in the `0.2.0` source line, Ubuntu/Debian `.deb` packaging starts in the `0.2.1` source line, and Linux SDK-enabled local processing is available from source builds configured with `NVAFX_ENABLE_SDK=ON`.

NVIDIA SDK binaries, models, AI features, redistributables, installers, generated audio/video files, and sample media are not included in this repository. The official NVIDIA Maxine AFX SDK API repository is used only as a local build dependency and is not vendored here.

Supported effects:

- `denoiser`
- `dereverb`
- `dereverb_denoiser`

Supported workflows:

- Temporary WAV files: `ffmpeg` extracts audio to WAV, `nvafx-audio-cli` processes the WAV file, and `ffmpeg` remuxes or normalizes the processed audio.
- Stdin/stdout WAV pipes: `ffmpeg` writes WAV bytes to stdout, `nvafx-audio-cli --input - --output -` processes the stream, and `ffmpeg` reads the processed WAV bytes from stdin.

Processing requires an SDK-enabled build, a runtime root, and an explicit model path:

```powershell
nvafx-audio-cli --input in.wav --output out.wav --effect denoiser --sample-rate 48000 --intensity 1.0 --model C:\Path\To\denoiser_48k.trtpkg --runtime-root C:\Path\To\NVIDIA-Audio-Effects-SDK
```

This project is not sponsored, endorsed, or approved by NVIDIA. NVIDIA, Maxine, RTX, and related names are trademarks of NVIDIA Corporation.

## Branch Policy

- `main` is release-only.
- `devel` is the development integration branch.
- Intermediate development is integrated into `devel` without development pull requests.
- Release pull requests are only from `devel` to `main`.
- GitHub Actions are reserved for `devel` to `main` release PR guardrails and explicit release packaging. They are not routine development validation.
- Routine development validation is local-only; CI protects `main` and does not replace local testing.
- Copilot review and bot review must not be requested.

## Current WAV Support

The current WAV foundation supports only the subset intended for early ffmpeg-generated inputs:

- RIFF/WAVE.
- PCM signed 16-bit and IEEE float 32-bit.
- WAVE_FORMAT_EXTENSIBLE PCM signed 16-bit and IEEE float 32-bit.
- Mono and stereo.
- 16000 Hz and 48000 Hz.
- Internal planar `float` audio representation.
- 32-bit float WAV writing infrastructure.

Mono input is supported with the installed SDK models tested locally. Stereo input is rejected clearly when the loaded SDK effect/model reports mono-only input and output channels.


## Pipe Usage

`--input -` reads WAV bytes from stdin. `--output -` writes processed 32-bit float WAV bytes to stdout. File paths remain supported and are the safest workflow when shell byte-stream behavior is uncertain.

When `--output -` is used for processing, stdout contains WAV bytes only. Diagnostics, warnings, and errors are written to stderr. Do not use `2>&1` with `--output -`, because that can corrupt the WAV stream.

Windows pipe safety is conservative:

- `cmd.exe` binary pipes are allowed.
- PowerShell 7.4 or newer is required for native byte-stream piping.
- Windows PowerShell 5.1 is rejected by default for binary WAV pipes.
- PowerShell cmdlets must not be placed in the middle of a binary WAV pipeline.
- Unknown parent shells are rejected by default.
- `--allow-unsafe-pipe` overrides the refusal and prints a warning to stderr.

FFmpeg pipe example from `cmd.exe` or PowerShell 7.4+:

```powershell
ffmpeg -i input.mp4 -vn -ac 1 -ar 48000 -f wav - | nvafx-audio-cli --input - --output - --effect denoiser --sample-rate 48000 --intensity 1.0 --model C:\Path\To\denoiser_48k.trtpkg --runtime-root C:\Path\To\NVIDIA-Audio-Effects-SDK | ffmpeg -f wav -i - -i input.mp4 -map 1:v? -map 0:a -c:v copy output.mp4
```

If unsure, keep using temporary WAV files:

```powershell
ffmpeg -i input.mp4 -vn -ac 1 -ar 48000 temp-in.wav
nvafx-audio-cli --input temp-in.wav --output temp-out.wav --effect denoiser --sample-rate 48000 --intensity 1.0 --model C:\Path\To\denoiser_48k.trtpkg --runtime-root C:\Path\To\NVIDIA-Audio-Effects-SDK
ffmpeg -i input.mp4 -i temp-out.wav -map 0:v? -map 1:a -c:v copy output.mp4
```
## CLI Checks

Use `--dry-run` to validate arguments and supported input WAV metadata without calling the SDK or writing output:

```powershell
nvafx-audio-cli --input in.wav --output out.wav --effect denoiser --sample-rate 48000 --intensity 1.0 --dry-run
```

Use `--check-sdk` to inspect a user-provided API root and runtime root:

```powershell
nvafx-audio-cli --check-sdk --api-root C:\Path\To\Maxine-AFX-SDK\nvafx --runtime-root C:\Path\To\NVIDIA-Audio-Effects-SDK
```

## Build

Ubuntu SDK-free build:

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends build-essential cmake ninja-build pkg-config ffmpeg ca-certificates git python3 dpkg-dev file
cmake -S . -B build-linux -G Ninja
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
python3 scripts/check_repo_hygiene.py
./build-linux/nvafx-audio-cli --version
```

The Ubuntu SDK-free build supports `--help`, `--version`, `--dry-run`, WAV read/write guardrails, `--check-sdk` structural checks, and clear failure when actual processing is requested without SDK support. It does not include NVIDIA SDK runtime files, models, `.so` feature libraries, CUDA setup, or GPU assumptions.

Ubuntu/Debian SDK-free package build:

```bash
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-linux
cpack --config build-linux/CPackConfig.cmake -G DEB -B build-linux/package
python3 scripts/check_deb_package.py build-linux/package/nvafx-audio-cli_0.2.1_amd64.deb
```

Once a release provides the `.deb`, install it manually with:

```bash
sudo apt install ./nvafx-audio-cli_0.2.1_amd64.deb
```

The `.deb` package is SDK-free. It installs `nvafx-audio-cli` to `/usr/bin` and project documentation to `/usr/share/doc/nvafx-audio-cli/`. It does not include NVIDIA SDK runtime files, shared libraries, models, CUDA setup, generated media, or sample media. APT repository publishing is future work.

Windows SDK-free build:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python scripts/check_repo_hygiene.py
```

Windows SDK-enabled build:

```powershell
cmake -S . -B build-sdk -DNVAFX_ENABLE_SDK=ON -DNVAFX_API_ROOT=C:\Path\To\Maxine-AFX-SDK\nvafx -DNVAFX_RUNTIME_ROOT=C:\Path\To\NVIDIA-Audio-Effects-SDK
cmake --build build-sdk --config Release
```

`NVAFX_API_ROOT` must point to the external API root containing `include\nvAudioEffects.h` and `lib\NVAudioEffects.lib`. `NVAFX_RUNTIME_ROOT` must point to the installed runtime root containing `NVAudioEffects.dll` and `models\*.trtpkg`. Environment variables with the same names are also accepted; `AFX_SDK_ROOT` remains a runtime-root compatibility fallback.

Linux SDK-enabled local build:

```bash
SDK_ROOT=/path/to/Audio_Effects_SDK
python3 scripts/build_linux_sdk_local.py \
  --sdk-root "$SDK_ROOT" \
  --model "$SDK_ROOT/features/denoiser/models/sm_89/denoiser_48k.trtpkg" \
  --build-dir build-linux-sdk \
  --install-prefix "$HOME/.local" \
  --run-test
```

The helper uses user-provided SDK/runtime/model paths. It does not download, vendor, commit, package, or redistribute NVIDIA SDK files, feature libraries, models, CUDA redistributables, generated media, or sample media. When `--install-prefix` is supplied, it installs only the project-built binary plus a generated local wrapper such as `$HOME/.local/bin/nvafx-audio-cli-sdk`; the wrapper sets process-local `LD_LIBRARY_PATH` entries for the user-provided SDK and then preserves all CLI arguments.

Manual Linux SDK-enabled build:

```bash
cmake -S . -B build-linux-sdk -G Ninja -DNVAFX_ENABLE_SDK=ON -DNVAFX_RUNTIME_ROOT="$SDK_ROOT"
cmake --build build-linux-sdk
./build-linux-sdk/nvafx-audio-cli --check-sdk --api-root "$SDK_ROOT/nvafx" --runtime-root "$SDK_ROOT"
```

Linux processing requires the external SDK core library, feature library, real model files, and visible NVIDIA GPU runtime/driver. The SDK-free `.deb` remains installable for checks only and does not perform real NVIDIA processing.

The current distribution policy keeps public release assets and future public APT packages SDK-free. SDK-enabled Linux processing is documented as a local source build workflow using external NVIDIA SDK/model material; see `docs/sdk-enabled-distribution-policy.md`. The planned `v0.3.0` scope keeps that artifact boundary and treats the Linux SDK-enabled workflow as source/docs/helper capability; see `docs/release-v0.3.0-scope.md`.

The main release gate runs on pull requests targeting `main` only. It performs clean Windows and Ubuntu SDK-free builds, CTest guardrails, repository hygiene checks, and CLI public-contract checks without requiring NVIDIA Audio Effects SDK.

See `docs/linux.md` for Ubuntu-specific notes.


## Release Packaging

`v0.1.0` was the initial manual binary/MSI release. `v0.1.1` is the first GitHub Actions-built and GitHub-attested release. `v0.1.2` added MSI machine `PATH` registration. `v0.1.3` fixes PATH registration correctness so the MSI uses the actual install directory and repair/reinstall do not create duplicates; reopen existing terminals after installing. The `0.2.0` source line adds Ubuntu SDK-free build/test foundation only. The `0.2.1` source line adds Ubuntu/Debian `.deb` packaging for the SDK-free build. The planned `0.3.0` source line documents the Linux SDK-enabled local workflow and helper while keeping public artifacts SDK-free. The MSI and `.deb` packages do not include NVIDIA SDK/runtime/model files. See `docs/release-packaging.md` for provenance, SDK-free CI binary, MSI, Debian package, and signing notes.
## Current Status

The CLI, SDK discovery checks, WAV I/O, Windows NVIDIA AFX SDK processing path, Linux NVIDIA AFX SDK local processing path, and Ubuntu SDK-free build/test path are implemented. The default build remains SDK-free and fails clearly if processing is requested without SDK support.
