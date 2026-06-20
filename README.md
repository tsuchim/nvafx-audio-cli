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

## Build

```powershell
cmake -S . -B build
cmake --build build
```

## Current Status

Only `--help`, `--version`, and basic option parsing are implemented. SDK processing is intentionally not implemented yet and returns a non-zero exit code when requested.
