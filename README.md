# nvafx-audio-cli

Small offline WAV processing CLI for NVIDIA Audio Effects SDK.

This project is a small Windows-first command-line tool that processes WAV files offline through a locally installed NVIDIA Audio Effects SDK. Ubuntu SDK-free build and test support starts in the `0.2.0` source line.

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
sudo apt-get install -y --no-install-recommends build-essential cmake ninja-build pkg-config ffmpeg ca-certificates git python3
cmake -S . -B build-linux -G Ninja
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
python3 scripts/check_repo_hygiene.py
./build-linux/nvafx-audio-cli --version
```

The Ubuntu SDK-free build supports `--help`, `--version`, `--dry-run`, WAV read/write guardrails, `--check-sdk` structural checks, and clear failure when actual processing is requested without SDK support. It does not include NVIDIA SDK runtime processing on Linux, `.so` runtime loading, CUDA setup, GPU assumptions, Linux release binaries, or Linux packages.

Windows SDK-free build:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python scripts/check_repo_hygiene.py
```

SDK-enabled build:

```powershell
cmake -S . -B build-sdk -DNVAFX_ENABLE_SDK=ON -DNVAFX_API_ROOT=C:\Path\To\Maxine-AFX-SDK\nvafx -DNVAFX_RUNTIME_ROOT=C:\Path\To\NVIDIA-Audio-Effects-SDK
cmake --build build-sdk --config Release
```

`NVAFX_API_ROOT` must point to the external API root containing `include\nvAudioEffects.h` and `lib\NVAudioEffects.lib`. `NVAFX_RUNTIME_ROOT` must point to the installed runtime root containing `NVAudioEffects.dll` and `models\*.trtpkg`. Environment variables with the same names are also accepted; `AFX_SDK_ROOT` remains a runtime-root compatibility fallback. This SDK-enabled path remains Windows-oriented in `0.2.0`; NVIDIA Linux SDK-enabled processing is a later phase.

The main release gate runs on pull requests targeting `main` only. It performs clean Windows and Ubuntu SDK-free builds, CTest guardrails, repository hygiene checks, and CLI public-contract checks without requiring NVIDIA Audio Effects SDK.

See `docs/linux.md` for Ubuntu-specific notes.


## Release Packaging

`v0.1.0` was the initial manual binary/MSI release. `v0.1.1` is the first GitHub Actions-built and GitHub-attested release. `v0.1.2` added MSI machine `PATH` registration. `v0.1.3` fixes PATH registration correctness so the MSI uses the actual install directory and repair/reinstall do not create duplicates; reopen existing terminals after installing. The `0.2.0` source line adds Ubuntu SDK-free build/test foundation only; it does not create Linux release binaries. The MSI does not add NVIDIA SDK/runtime/model paths to `PATH`. See `docs/release-packaging.md` for provenance, SDK-free CI binary, MSI, and signing notes.
## Current Status

The CLI, SDK discovery checks, WAV I/O, Windows NVIDIA AFX SDK processing path, and Ubuntu SDK-free build/test path are implemented. The default build remains SDK-free and fails clearly if processing is requested without SDK support.
