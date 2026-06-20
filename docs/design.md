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
- Intermediate development is integrated into `devel` without development pull requests.
- Release pull requests are only from `devel` to `main`.
- Nothing is merged into `main` unless explicitly treated as a release step.
- GitHub Actions are reserved for `devel` to `main` release PR guardrails.
- Routine development validation is local-only.
- CI exists to protect `main`, not to replace local development testing.
- Copilot review and bot review must not be requested.

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

A structurally plausible SDK root currently means an existing directory with `include` and either `lib` or `bin`. The probe reports missing and present SDK-like subpaths but does not load or call the SDK.

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

## Next Step

After the WAV foundation is merged into `devel`, the intended next step is actual NVIDIA AFX SDK binding behind the existing CLI and WAV abstractions. That work must still avoid vendoring SDK binaries, models, features, redistributables, installers, archives, generated media, or sample media.
