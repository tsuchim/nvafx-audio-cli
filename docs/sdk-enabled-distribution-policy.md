# SDK-Enabled Distribution Policy

This policy describes how `nvafx-audio-cli` should distribute public binaries and packages now that Linux SDK-enabled processing works from local source builds.

## Recommendation

Near term, public GitHub Release assets and future public APT packages should remain SDK-free validation packages. Real SDK-enabled Linux processing is supported through a local source build/install workflow that uses user-provided NVIDIA Audio Effects SDK core files, feature libraries, model files, and GPU runtime.

Do not distribute an SDK-enabled `.deb`, SDK-enabled public binary, NVIDIA SDK file, feature library, model, CUDA redistributable, generated media, or sample media until license review and runtime-path policy are complete.

This keeps the public package within the project's current artifact boundary by excluding NVIDIA SDK/runtime/model material, while still letting users with a valid local NVIDIA SDK setup install a local processing command. This project policy is not legal advice or a substitute for license review.

The `v0.3.0` release follows this recommendation as a source/docs/helper release for the Linux SDK-enabled local workflow. See `docs/release-v0.3.0-scope.md`.

## Distribution Options

### 1. SDK-Free Public `.deb` Only

This is the current released Linux package model. It is the appropriate near-term model for GitHub Releases and future public APT work because it contains only project-built files and documentation and does not bundle NVIDIA SDK/runtime/model material. Normal license, security, packaging, and operational review still applies.

The SDK-free `.deb` does not include NVIDIA runtime libraries, feature libraries, models, CUDA redistributables, generated media, or sample media. It supports CLI checks, WAV guardrails, `--dry-run`, `--check-sdk`, repository hygiene validation, and clear failure when real processing is requested from an SDK-free build. It cannot perform real NVIDIA Audio Effects processing.

### 2. SDK-Enabled Source Build Only

Users obtain NVIDIA SDK and model material separately, then build locally with `NVAFX_ENABLE_SDK=ON`. This is the strongest artifact and license boundary because the repository and public release assets do not carry NVIDIA material.

This workflow is the near-term path for real processing. It also keeps public CI SDK-free and avoids storing NGC credentials or downloaded SDK material in GitHub-hosted infrastructure.

### 3. SDK-Enabled Binary Without SDK/Model

A public binary could be built with SDK support but still require user-provided SDK runtime and model paths at run time. This may be legally simpler than shipping NVIDIA artifacts, but it still needs careful design.

The binary must not include NVIDIA SDK files, feature libraries, models, CUDA redistributables, or generated media. Linux also needs a clear policy for dynamic loading, `RPATH`, `RUNPATH`, `LD_LIBRARY_PATH`, relocation, and whether the binary links directly to `libnv_audiofx.so` or loads it from a user-selected SDK root. This option should wait until those runtime-path and support expectations are explicit.

### 4. Separate SDK-Enabled Package

A package such as `nvafx-audio-cli-sdk-enabled` could make the distinction visible to users, but it is awkward for a public Debian package if it depends on external SDK paths that Debian cannot install or verify.

It may require local build/install scripts, user-managed SDK discovery, or post-install configuration that is outside normal public APT expectations. Do not publish this package publicly until the dependency, runtime-path, support, and license policies are clear.

### 5. Private/Internal Package

A private package may be possible only if NVIDIA license terms allow the intended internal redistribution and deployment model. Do not assume redistribution is allowed.

Any internal package that includes selected NVIDIA artifacts, feature libraries, models, CUDA redistributables, or generated sample material requires license review before implementation or distribution.

## Current Public Package Contract

The public `.deb` package line, including `v0.2.1` and `v0.3.0`, is SDK-free. It is installable and useful for command-line validation, WAV input/output checks, dry runs, SDK tree structure checks, and package smoke tests.

It does not perform real NVIDIA Audio Effects processing because it does not include or link against the NVIDIA SDK runtime and does not include feature libraries or models.

## Current SDK-Enabled Contract

Linux SDK-enabled processing works from a local source build when the user provides:

- NVIDIA Linux Audio Effects SDK core files.
- The target feature package, such as denoiser.
- A real model for the target GPU architecture.
- A compatible visible GPU runtime, including `libcuda.so.1`.
- A local build configured with `NVAFX_ENABLE_SDK=ON`.

The user must pass explicit runtime and model paths when processing audio. Public CI does not download SDK material and does not run SDK-enabled processing.

`scripts/build_linux_sdk_local.py` is the recommended local helper for this workflow. It uses user-provided SDK/runtime/model paths, configures and builds an SDK-enabled local binary, optionally runs a real smoke test when a model and GPU runtime are available, and can generate a local wrapper under a user-selected install prefix. By default the wrapper is `<install-prefix>/bin/nvafx-audio-cli`, so it is the command users run for real local processing when that prefix is on `PATH`. The wrapper is generated locally and may contain local SDK paths, but it installs only project-built files and does not copy NVIDIA SDK, feature, model, or CUDA redistributable files.

## Artifact Boundary

The repository, release assets, and public packages must not include:

- NVIDIA SDK files or headers.
- NVIDIA feature libraries.
- `.trtpkg`, `.onnx`, or `.engine` model files.
- CUDA redistributables.
- Generated audio/video media.
- Sample media.
- Local credentials, NGC API keys, or generated SDK acquisition logs.

## Future Work

- Publish a public APT repository for the SDK-free package only, after APT publishing and signing policy are ready.
- Maintain the SDK-enabled local build/install helper that validates user-provided SDK paths without downloading or vendoring SDK material.
- Decide whether an SDK-enabled binary without bundled SDK/model artifacts is supportable, including Linux dynamic loading and `LD_LIBRARY_PATH` expectations.
- Complete license review before any SDK-enabled binary/package distribution that might include or redistribute NVIDIA artifacts.
- Clean up or revoke operator-managed NGC credentials after SDK acquisition is no longer needed.
