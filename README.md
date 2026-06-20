# nvafx-audio-cli

Small offline audio processing CLI for NVIDIA Audio Effects SDK.

This project is intended to become a small Windows-first command-line tool that processes WAV files offline through NVIDIA Audio Effects SDK. It currently contains only a source-only CLI scaffold and does not call the NVIDIA SDK yet.

NVIDIA SDK binaries, models, AI features, redistributables, installers, generated audio/video files, and sample media are not included in this repository. Users must install NVIDIA Audio Effects SDK separately before future SDK-backed processing can work.

Initial planned effects:

- `denoiser`
- `dereverb`
- `dereverb_denoiser`

Initial intended workflow:

1. `ffmpeg` extracts audio to WAV.
2. `nvafx-audio-cli` processes the WAV file.
3. `ffmpeg` remuxes and normalizes the processed audio.

Future CLI shape:

```powershell
nvafx-audio-cli --input in.wav --output out.wav --effect denoiser --sample-rate 48000 --intensity 1.0
```

This project is not sponsored, endorsed, or approved by NVIDIA. NVIDIA, Maxine, RTX, and related names are trademarks of NVIDIA Corporation.

## Branch Policy

- `main` is release-only.
- `devel` is the development integration branch.
- Development pull requests target `devel`.

## Current WAV Support

The current WAV foundation supports only the subset intended for early ffmpeg-generated inputs:

- RIFF/WAVE.
- PCM signed 16-bit and IEEE float 32-bit.
- Mono and stereo.
- 16000 Hz and 48000 Hz.
- Internal planar `float` audio representation.
- 32-bit float WAV writing infrastructure.

No NVIDIA Audio Effects SDK processing is implemented yet.

## CLI Checks

Use `--dry-run` to validate arguments and supported input WAV metadata without calling the SDK or writing output:

```powershell
nvafx-audio-cli --input in.wav --output out.wav --effect denoiser --sample-rate 48000 --intensity 1.0 --dry-run
```

Use `--check-sdk` to inspect a user-provided SDK root or `AFX_SDK_ROOT`:

```powershell
nvafx-audio-cli --check-sdk --sdk-root C:\Path\To\AFX
```

## Build

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build -C Release --output-on-failure
```

## Current Status

The CLI, SDK discovery checks, and narrow WAV I/O foundation are implemented. SDK processing is intentionally not implemented yet and returns a non-zero exit code when requested. The intended next step is actual AFX binding after the WAV foundation is merged into `devel`.
