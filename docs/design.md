# Initial Design

`nvafx-audio-cli` is a small source-only command-line project for future offline WAV processing with NVIDIA Audio Effects SDK.

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

## Future CLI Shape

```powershell
nvafx-audio-cli --input in.wav --output out.wav --effect denoiser --sample-rate 48000 --intensity 1.0
```

Initial accepted effect names:

- `denoiser`
- `dereverb`
- `dereverb_denoiser`

## SDK Discovery

Future SDK-backed processing should discover the NVIDIA Audio Effects SDK through one or both of:

- `--sdk-root <path>`
- `AFX_SDK_ROOT` environment variable

Local development paths must remain outside committed source and must not be hardcoded.
