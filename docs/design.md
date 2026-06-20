# Initial Design

`nvafx-audio-cli` is a small Windows-first command-line project for offline WAV processing with NVIDIA Audio Effects SDK.

## Scope

- Windows-first CLI.
- C++17.
- CMake build.
- No GUI.
- No OBS integration.
- No FFmpeg plugin.
- Operates on WAV files only.
- Does not vendor NVIDIA SDK binaries, models, AI features, redistributables, installers, generated media, or sample media.
- Does not hardcode local NVIDIA SDK paths.
- Uses the official external NVIDIA Maxine AFX SDK API repository as a local build dependency only.

## Branch Policy

- `main` is release-only.
- `devel` is the development integration branch.
- Feature and foundation branches branch from `devel`.
- Intermediate development is integrated into `devel` without development pull requests.
- Release pull requests are only from `devel` to `main`.
- Nothing is merged into `main` unless explicitly treated as a release step.
- GitHub Actions are reserved for `devel` to `main` release PR guardrails.
- Routine development validation is local-only.
- CI exists to protect `main`, not to replace local development testing.
- Copilot review and bot review must not be requested.

## CLI Shape

```powershell
nvafx-audio-cli --input in.wav --output out.wav --effect denoiser --sample-rate 48000 --intensity 1.0 --model denoiser_48k.trtpkg --runtime-root C:\Path\To\NVIDIA-Audio-Effects-SDK
```

Initial accepted effect names:

- `denoiser`
- `dereverb`
- `dereverb_denoiser`

## SDK Build And Discovery

The default build is SDK-free. SDK processing is enabled only when configured with:

- `NVAFX_ENABLE_SDK=ON`
- `NVAFX_API_ROOT=<external Maxine-AFX-SDK\nvafx path>`
- `NVAFX_RUNTIME_ROOT=<installed runtime path>`

If CMake cache variables are absent, `NVAFX_API_ROOT` and `NVAFX_RUNTIME_ROOT` environment variables are accepted. `AFX_SDK_ROOT` remains a runtime-root compatibility fallback when useful. Local development paths must remain outside committed source and must not be hardcoded.

`--check-sdk` resolves API and runtime roots, checks structure, reports headers, import libraries, runtime DLLs, model area, and detected model files. It does not load SDK DLLs and does not claim processing will succeed.

A structurally plausible setup has `include\nvAudioEffects.h`, `lib\NVAudioEffects.lib`, `NVAudioEffects.dll`, and a `models` directory.

## Dry Run

`--dry-run` validates CLI arguments, validates supported input WAV metadata when `--input` is provided, resolves SDK root when available, prints the planned operation, does not call the SDK, does not write output, and exits successfully when validation succeeds.

## WAV Foundation

The current WAV reader is intentionally narrow and designed for early ffmpeg-generated files:

- RIFF/WAVE only.
- Uncompressed PCM signed 16-bit.
- IEEE float 32-bit.
- WAVE_FORMAT_EXTENSIBLE PCM signed 16-bit.
- WAVE_FORMAT_EXTENSIBLE IEEE float 32-bit.
- Mono and stereo.
- 16000 Hz and 48000 Hz.
- Decoded audio is represented internally as planar `float` samples.
- Output support writes 32-bit float WAV.

Unsupported formats, bit depths, channel counts, sample rates, truncated files, and malformed chunks fail clearly. No WAV files are committed to the repository.

## Main Release Gate

`.github/workflows/ci.yml` is named `Main Release Gate` and runs only for pull requests targeting `main`. It does not run on pushes to `devel`, pushes to feature or foundation branches, or pull requests targeting `devel`.

The release gate is intentionally minimal but checks guardrails:

- Clean Windows build from checkout.
- Full CTest suite.
- CLI public contract for help, version, invalid effect, invalid numeric options, and unimplemented SDK processing.
- WAV guardrails for supported generated inputs, unsupported sample rate, unsupported channel count, malformed or truncated WAV, and writer round-trip.
- Repository hygiene through `scripts/check_repo_hygiene.ps1`.
- No NVIDIA SDK requirement.

Development branches should run the same checks locally instead of using GitHub Actions as routine confirmation.

## Processing

The SDK adapter is isolated in `src/afx_sdk.cpp` and `src/afx_sdk.hpp`. All direct NVIDIA includes and SDK calls stay there. Processing reads WAV into planar float buffers, loads the requested SDK effect with an explicit `--model`, queries SDK frame size, sample rate, and channel constraints, processes padded frames, trims the final output to the original frame count, and writes 32-bit float WAV.

Mono processing is supported for the tested installed models. Stereo is accepted by the WAV reader but rejected for SDK processing when the loaded effect/model reports mono-only input and output channels. The CLI does not process stereo channels independently because the SDK effect contract does not document that as equivalent.
