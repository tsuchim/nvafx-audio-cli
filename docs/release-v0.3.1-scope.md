# v0.3.1 Release Scope

This document plans the intended `v0.3.1` release scope. It is not a tag instruction, release execution approval, or artifact publication approval.

## Objective

Release `v0.3.1` as the first distributable release after PR #13. It keeps public artifacts SDK-free while making the real Linux processing path explicit and user-usable through the local SDK-enabled build/install helper.

`v0.3.1` must not mutate, replace, or reissue the already published `v0.3.0` release.

## Public Artifacts

Expected public artifacts remain SDK-free validation packages:

- Windows zip package, if produced by the release workflow.
- Windows MSI package, if produced by the release workflow.
- Linux `.deb` package.
- `SHA256SUMS.txt`.

These artifacts should contain project-built files and project documentation only. They must not contain NVIDIA SDK files, feature libraries, model files, CUDA redistributables, generated media, sample media, credentials, or SDK acquisition logs.

## Real Processing Path

Real Linux NVIDIA AFX processing is provided through local SDK-enabled build/install with user-provided SDK/runtime/model material:

```bash
python3 scripts/build_linux_sdk_local.py \
  --sdk-root /path/to/Audio_Effects_SDK \
  --model /path/to/Audio_Effects_SDK/features/denoiser/models/sm_89/denoiser_48k.trtpkg \
  --install-prefix "$HOME/.local" \
  --run-test
```

The helper installs a generated local wrapper at `<install-prefix>/bin/nvafx-audio-cli` by default. That wrapper is the intended local processing command when the selected prefix is on `PATH`.

The wrapper installs only project-built files. It does not copy NVIDIA SDK files, feature libraries, model files, CUDA redistributables, generated media, or sample media.

## Validation Scope

Before releasing `v0.3.1`, validate:

- Public CI SDK-free release gate.
- SDK-free Windows and Ubuntu builds/tests.
- SDK-free Debian package build and inspection.
- Repository hygiene.
- CLI public contract checks.
- Helper CI-safe tests, including versioned Linux `.so` SDK layout handling.
- Optional local SDK-enabled helper install and real denoiser smoke test when local SDK/model/GPU runtime material exists.
- Artifact content inspection for forbidden NVIDIA/model/CUDA/generated/secret files.

## Out Of Scope

`v0.3.1` does not include:

- Public SDK-enabled Linux binaries.
- Public SDK-enabled `.deb` packages.
- Bundled NVIDIA SDK/runtime/model/CUDA material.
- SDK download in CI.
- GPU requirement in public CI.
- APT repository publishing.
- Production signing changes.
- Any change to existing `v0.3.0` release assets.

## Decision

Proceed with `v0.3.1` as an SDK-free public artifact release that ships the corrected local SDK-enabled build/install helper and documentation. Defer public SDK-enabled binary/package distribution until a separate policy and implementation task is explicitly approved.
