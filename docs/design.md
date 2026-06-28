# Initial Design

`nvafx-audio-cli` is a small Windows-first command-line project for offline WAV processing with NVIDIA Audio Effects SDK. The `0.2.0` source line adds Ubuntu SDK-free build and test support.

## Scope

- Windows-first CLI with Ubuntu SDK-free validation support.
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
- GitHub Actions are reserved for `devel` to `main` release PR guardrails and explicit release packaging.
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

`--check-sdk` resolves API and runtime roots, checks structure, reports headers, Windows import libraries, Windows runtime DLLs, Linux shared libraries, Linux feature libraries, model areas, and detected model files. It does not load SDK DLLs or shared libraries and does not claim processing will succeed.

A structurally plausible setup has `include\nvAudioEffects.h`, `lib\NVAudioEffects.lib`, `NVAudioEffects.dll`, and a `models` directory.

A structurally plausible Linux setup has `nvafx/include/nvAudioEffects.h`, `nvafx/lib/libnv_audiofx.so`, a `features` directory with feature libraries, and `.trtpkg` models under feature model directories.

SDK-enabled processing is available for Windows local SDK builds and Linux local SDK builds. Public package and CI workflows remain SDK-free and do not vendor or download NVIDIA SDK material.

The near-term distribution policy is to ship public GitHub Release assets and future public APT packages as SDK-free validation builds only. SDK-enabled Linux processing is the local source build/install workflow with user-provided SDK/runtime/model material. See `docs/sdk-enabled-distribution-policy.md`.

The local Linux helper `scripts/build_linux_sdk_local.py` is an orchestration layer for that source-build workflow. It validates user-provided SDK paths, runs the SDK-enabled CMake build, can run the manual SDK processing CTest, and can install a generated local `nvafx-audio-cli` wrapper that sets process-local library paths. It does not download, vendor, package, or redistribute NVIDIA artifacts.


## Pipe I/O

The CLI supports `--input -` for stdin and `--output -` for stdout. Stream I/O uses the same WAV parser and writer as file I/O, preserving the RIFF/WAVE, PCM16, float32, WAVE_FORMAT_EXTENSIBLE, channel, sample-rate, malformed, and truncated-file validations.

On Windows, stdin and stdout are switched to binary mode only when pipe I/O is used. Processing with `--output -` never writes status text to stdout; stdout is reserved for WAV bytes only. Diagnostics remain on stderr.

Windows shell safety is conservative. `cmd.exe` is allowed. `powershell.exe` is rejected because Windows PowerShell 5.1 is not safe for native binary WAV pipelines. `pwsh.exe` is allowed only when version detection confirms PowerShell 7.4 or newer. Unknown parent shells are rejected unless `--allow-unsafe-pipe` is used, in which case a warning is printed to stderr.

Users should avoid `2>&1` with `--output -` and avoid inserting PowerShell cmdlets inside binary WAV pipelines. Temporary WAV files remain the recommended workflow when shell byte-stream behavior is uncertain.
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
- Clean Ubuntu SDK-free build from checkout.
- Full CTest suite.
- CLI public contract for help, version, invalid effect, invalid numeric options, and unimplemented SDK processing.
- WAV guardrails for supported generated inputs, unsupported sample rate, unsupported channel count, malformed or truncated WAV, and writer round-trip.
- Cross-platform repository hygiene through `scripts/check_repo_hygiene.py`.
- No NVIDIA SDK requirement.

Development branches should run the same checks locally instead of using GitHub Actions as routine confirmation.


## Release Packaging Workflow

`.github/workflows/release.yml` creates GitHub Actions-built release assets for explicit release tags or manual release dispatch. The workflow builds the public SDK-free Windows executable, creates the zip and MSI packages, builds the SDK-free Ubuntu/Debian `.deb` package, writes `SHA256SUMS.txt`, publishes the GitHub Release, and emits GitHub Artifact Attestations.

The workflow intentionally does not require or include NVIDIA SDK files. Until a legal CI SDK setup exists, attested CI binaries and the `.deb` package are SDK-free and actual AFX processing remains a local SDK-enabled build path. The workflow does not publish an APT repository and does not perform production APT signing.
## Processing

The SDK adapter is isolated in `src/afx_sdk.cpp` and `src/afx_sdk.hpp`. All direct NVIDIA includes and SDK calls stay there. Processing reads WAV into planar float buffers, loads the requested SDK effect with an explicit `--model`, queries SDK frame size, sample rate, and channel constraints, processes padded frames, trims the final output to the original frame count, and writes 32-bit float WAV.

Mono processing is supported for the tested installed models. Stereo is accepted by the WAV reader but rejected for SDK processing when the loaded effect/model reports mono-only input and output channels. The CLI does not process stereo channels independently because the SDK effect contract does not document that as equivalent.
