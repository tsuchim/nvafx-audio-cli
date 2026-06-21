# Release Packaging

## Release lineage

`v0.1.0` was the initial manual binary/MSI release. It remains published for historical continuity and is not rewritten or deleted.

`v0.1.1` is the first GitHub Actions-built and GitHub-attested release. Its release assets are produced by the repository release workflow from public source.

## Provenance

Release artifacts are built by GitHub Actions from the public repository source and receive GitHub Artifact Attestations.

This lets users verify that the distributed artifacts came from this repository, commit/tag, and workflow. It does not prove the code is harmless; it makes the source-to-artifact relationship inspectable.

Example verification:

```powershell
gh attestation verify nvafx-audio-cli-v0.1.1-windows-x64.zip -R tsuchim/nvafx-audio-cli
gh attestation verify nvafx-audio-cli-v0.1.1-windows-x64.msi -R tsuchim/nvafx-audio-cli
```

## SDK policy

The release workflow intentionally does not vendor NVIDIA SDK files, NVIDIA DLLs, NVIDIA models, headers, import libraries, installers, redistributables, generated media, or sample media.

GitHub-hosted runners do not include NVIDIA Audio Effects SDK. Until a legal CI SDK setup exists, GitHub Actions-built binaries are SDK-free. They are useful for CLI validation, WAV I/O guardrails, `--dry-run`, `--check-sdk`, and package structure verification. Actual NVIDIA AFX processing still requires a local SDK-enabled build.

NVIDIA SDK runtime and models remain external user-provided dependencies. Processing with an SDK-enabled build still requires explicit runtime/model arguments:

```powershell
nvafx-audio-cli --input in.wav --output out.wav --effect denoiser --sample-rate 48000 --model C:\Path\To\denoiser_48k.trtpkg --runtime-root C:\Path\To\NVIDIA-Audio-Effects-SDK
```

## MSI and signing

The MSI installs only `nvafx-audio-cli` project files. NVIDIA runtime files and models are not included and are not detected during installation.

MSI packages remain unsigned unless Authenticode signing is added later. Unsigned MSI packages may show Windows publisher or SmartScreen warnings.

GitHub Artifact Attestation is separate from Authenticode signing. Attestation proves repository/workflow provenance for a release artifact; it does not make Windows treat the executable or MSI as a signed publisher binary.

MSIX is deferred until trusted package signing is available. No MSIX package is produced for `v0.1.1`.
