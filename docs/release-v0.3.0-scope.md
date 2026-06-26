# v0.3.0 Release Scope

This document plans the intended `v0.3.0` release scope. It is not a release execution checklist, tag instruction, or artifact publication approval.

## Recommendation

Release `v0.3.0` as a documentation, source, and helper release for the Linux SDK-enabled local workflow. Public release artifacts should remain SDK-free.

This means `v0.3.0` is the first release line that exposes Linux NVIDIA Audio Effects SDK processing as a documented local source-build capability, including `scripts/build_linux_sdk_local.py`, while keeping GitHub Release assets and future public APT packages inside the current SDK-free artifact boundary.

Do not distribute SDK-enabled public binaries or SDK-enabled `.deb` packages in `v0.3.0`. Defer that until license review and runtime/model path policy are complete.

## Public Artifact Scope

The existing release workflow supports SDK-free public artifacts. For `v0.3.0`, the expected public artifacts are:

- SDK-free Windows zip package, if the existing release workflow continues to produce it.
- SDK-free Windows MSI package, if the existing release workflow continues to produce it.
- SDK-free Linux `.deb` package.
- `SHA256SUMS.txt`.
- GitHub Artifact Attestations or other provenance files already produced by the release workflow.

These artifacts should include project-built files and project documentation only. They should not include an SDK-enabled public binary unless the distribution policy changes before release execution.

## Forbidden Public Artifact Contents

`v0.3.0` public artifacts must not include:

- NVIDIA SDK files or headers.
- NVIDIA feature libraries.
- `.trtpkg`, `.onnx`, or `.engine` model files.
- CUDA redistributables.
- Generated audio/video media.
- Sample media.
- Credentials or API keys.
- SDK acquisition logs.

## Validated Scope

`v0.3.0` should validate:

- Public CI SDK-free release gate.
- SDK-free Windows build and tests.
- SDK-free Ubuntu build and tests.
- SDK-free Debian package build and inspection.
- Repository hygiene checks.
- CI-safe helper tests for `scripts/build_linux_sdk_local.py`.
- Optional local SDK-enabled smoke test when local SDK/model material and GPU runtime are available.

The optional SDK-enabled smoke test is documented for maintainers and local operators. It should not be required in public CI, should not download SDK material, and should not require credentials.

## Explicit Non-Goals

`v0.3.0` does not include:

- Public SDK-enabled `.deb` packages.
- Public SDK-enabled Linux binaries.
- NVIDIA SDK/model downloads in CI.
- GPU runtime requirements in public CI.
- APT repository publishing, unless planned separately.
- Production signing changes, unless planned separately.
- Changes to existing `v0.1.x` or `v0.2.x` release assets.

## Manual Pre-Release Validation

Before executing the actual `v0.3.0` release, validate:

- Main Release Gate passes for the release PR.
- SDK-free regression passes locally where practical:
  `ctest --test-dir build-linux --output-on-failure`
- Helper CI-safe tests pass through CTest.
- `python3 scripts/check_repo_hygiene.py` passes.
- `git diff --check` passes before the release commit.
- Optional local real SDK helper smoke test passes if local SDK/model material and GPU runtime are available.
- Staged and generated release artifacts contain no forbidden NVIDIA files, models, CUDA redistributables, generated media, sample media, credentials, or SDK acquisition logs.
- Generated helper build trees, install trees, wrapper scripts, and WAV smoke-test files are outside git and are not committed.
- No secret values or local credential paths appear in docs, logs, release notes, PRs, issues, sidecar reports, or artifacts.

## Release Decision

Proceed with `v0.3.0` as an SDK-free public artifact release that documents and ships the Linux SDK-enabled local source workflow and helper script.

Do not treat `v0.3.0` as an SDK-bundling release. Keep SDK-enabled public binary and package distribution deferred until license review, support expectations, and runtime/model path policy are complete.
