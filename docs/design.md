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

## Branch Policy

- `main` is release-only.
- `devel` is the development integration branch.
- Feature and foundation branches branch from `devel`.
- Development pull requests target `devel`.
- Nothing is merged into `main` unless explicitly treated as a release step.

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

`--check-sdk` resolves the SDK root from `--sdk-root` or `AFX_SDK_ROOT`, checks whether the path exists, and reports SDK-like subpaths such as `include`, `lib`, `bin`, and `models`. The SDK is not required for `--help`, `--version`, tests, or CI.

## Dry Run

`--dry-run` validates CLI arguments, validates supported input WAV metadata when `--input` is provided, resolves SDK root when available, prints the planned operation, does not call the SDK, does not write output, and exits successfully when validation succeeds.

## WAV Foundation

The current WAV reader is intentionally narrow and designed for early ffmpeg-generated files:

- RIFF/WAVE only.
- Uncompressed PCM signed 16-bit.
- IEEE float 32-bit.
- Mono and stereo.
- 16000 Hz and 48000 Hz.
- Decoded audio is represented internally as planar `float` samples.
- Output support writes 32-bit float WAV.

Unsupported formats, bit depths, channel counts, sample rates, truncated files, and malformed chunks fail clearly. No WAV files are committed to the repository.

## Next Step

After the WAV foundation is merged into `devel`, the intended next step is actual NVIDIA AFX SDK binding behind the existing CLI and WAV abstractions. That work must still avoid vendoring SDK binaries, models, features, redistributables, installers, archives, generated media, or sample media.
