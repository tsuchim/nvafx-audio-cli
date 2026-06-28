# Release Packaging

## Release lineage

`v0.1.0` was the initial manual binary/MSI release. It remains published for historical continuity and is not rewritten or deleted.

`v0.1.1` is the first GitHub Actions-built and GitHub-attested release. Its release assets are produced by the repository release workflow from public source.

`v0.1.2` added MSI machine `PATH` registration. `v0.1.3` fixes PATH registration correctness so the MSI uses the actual install directory, repair/reinstall keep exactly one PATH entry, and uninstall removes that entry.

The `0.2.0` source line adds Ubuntu SDK-free build/test support. The `0.2.1` source line adds Ubuntu/Debian `.deb` packaging for the SDK-free Linux build.

The `0.3.0` source line documents and ships the Linux SDK-enabled local source workflow and helper script while keeping public release artifacts SDK-free. See `docs/release-v0.3.0-scope.md`.

## Provenance

Release artifacts are built by GitHub Actions from the public repository source and receive GitHub Artifact Attestations.

This lets users verify that the distributed artifacts came from this repository, commit/tag, and workflow. It does not prove the code is harmless; it makes the source-to-artifact relationship inspectable.

Example verification:

```powershell
gh attestation verify nvafx-audio-cli-v0.1.3-windows-x64.zip -R tsuchim/nvafx-audio-cli
gh attestation verify nvafx-audio-cli-v0.1.3-windows-x64.msi -R tsuchim/nvafx-audio-cli
```

## SDK policy

The release workflow intentionally does not vendor NVIDIA SDK files, NVIDIA DLLs, NVIDIA models, headers, import libraries, installers, redistributables, generated media, or sample media.

GitHub-hosted runners do not include NVIDIA Audio Effects SDK. Until a legal CI SDK setup exists, GitHub Actions-built binaries are SDK-free. They are useful for CLI validation, WAV I/O guardrails, `--dry-run`, `--check-sdk`, and package structure verification, but they cannot perform real NVIDIA AFX processing by themselves. Actual NVIDIA AFX processing still requires a local SDK-enabled build.

Ubuntu CI validates SDK-free configure, build, CTest, hygiene, help, version output, and `.deb` package structure. It does not install NVIDIA drivers, CUDA, NVIDIA Audio Effects SDK, models, headers, import libraries, shared libraries, generated media, or sample media.

NVIDIA SDK runtime and models remain external user-provided dependencies. Processing with an SDK-enabled build still requires explicit runtime/model arguments:

```powershell
nvafx-audio-cli --input in.wav --output out.wav --effect denoiser --sample-rate 48000 --model C:\Path\To\denoiser_48k.trtpkg --runtime-root C:\Path\To\NVIDIA-Audio-Effects-SDK
```

## Debian package

Starting with `v0.2.1`, the release workflow is prepared to produce Ubuntu/Debian `.deb` packages for the SDK-free Linux build. For `v0.3.0`, the expected package is:

```text
nvafx-audio-cli_0.3.0_amd64.deb
```

The package installs:

```text
/usr/bin/nvafx-audio-cli
/usr/share/doc/nvafx-audio-cli/
```

Manual install after download:

```bash
sudo apt install ./nvafx-audio-cli_0.3.0_amd64.deb
```

The `.deb` package does not include NVIDIA SDK/runtime/model files, feature libraries, CUDA setup, generated media, or sample media, and it cannot perform real NVIDIA AFX processing by itself. NVIDIA Linux SDK-enabled processing is available only from local source builds configured with external SDK/runtime/model paths.

Near-term distribution policy keeps public GitHub Release assets and future public APT packages SDK-free. SDK-enabled Linux processing is a documented local source build workflow only. See `docs/sdk-enabled-distribution-policy.md`.

The `v0.3.0` release scope should preserve that policy: source/docs/helper support for the local SDK-enabled workflow, no SDK-enabled public Linux binary, and no SDK-enabled public `.deb`. See `docs/release-v0.3.0-scope.md`.

## MSI and signing

The MSI installs only `nvafx-audio-cli` project files. NVIDIA runtime files and models are not included and are not detected during installation.

Starting with `v0.1.3`, the MSI adds its actual install directory to the machine `PATH`. Existing terminals may need to be reopened before the command is visible. Repair and reinstall keep exactly one PATH entry, and uninstall removes that entry. NVIDIA SDK runtime/model paths are not added to `PATH`; processing still requires explicit `--runtime-root` and `--model` arguments.

MSI packages remain unsigned unless Authenticode signing is added later. Unsigned MSI packages may show Windows publisher or SmartScreen warnings.

Authenticode signing and future APT archive signing are managed as centralized Eikai Intelligent Systems release infrastructure outside this project repository. No signing secrets, private keys, certificates, PFX files, or package archive secret material are stored in this repository.

GitHub Artifact Attestation is separate from Authenticode signing. Attestation proves repository/workflow provenance for a release artifact; it does not make Windows treat the executable or MSI as a signed publisher binary.

APT publishing is future work. APT signing identity and public key inventory are centrally managed by `local-infra`, but production APT repository signing and publication are not enabled in this project workflow.

MSIX is deferred until trusted package signing is available. No MSIX package is produced for `v0.3.0`.
