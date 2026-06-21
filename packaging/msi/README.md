# MSI packaging

This directory contains the WiX source and build script for the Windows x64 MSI
package for `nvafx-audio-cli`.

The MSI installs only project files:

- `nvafx-audio-cli.exe`
- `README.md`
- `LICENSE`
- `THIRD_PARTY_NOTICES.md`
- `BUILD_INFO.txt`

NVIDIA Audio Effects SDK runtime files, DLLs, models, headers, import
libraries, installers, redistributables, generated media, and sample media are
not included. Users must install NVIDIA Audio Effects SDK separately.

Runtime processing still requires explicit paths:

```powershell
nvafx-audio-cli --input in.wav --output out.wav --effect denoiser --sample-rate 48000 --model <path-to-model.trtpkg> --runtime-root <path-to-NVIDIA-Audio-Effects-SDK-runtime>
```

The v0.1.0 MSI is unsigned unless code signing is explicitly configured later.
Unsigned MSI packages may show Windows SmartScreen or publisher warnings.

MSIX is intentionally not produced for v0.1.0 because production MSIX
distribution requires trusted package signing, which this project does not have
configured.

Build example:

```powershell
.\packaging\msi\build-msi.ps1 `
  -InputDirectory .\build-release-package\nvafx-audio-cli-v0.1.0-windows-x64 `
  -OutputDirectory .\build-msi-package `
  -Version 0.1.0 `
  -Architecture x64
```

The script validates the staging directory before invoking WiX and fails if
forbidden SDK, model, media, archive, or build artifacts are present.
