# Internal APT Packaging

Internal APT packaging has a stricter goal than public GitHub Release packaging.

Public GitHub Release assets remain SDK-free validation artifacts. They are useful for CLI checks and packaging validation, but they are not the product completion target.

The internal APT goal is real offline NVIDIA denoiser processing from file input/output and stdin/stdout.

## Required Install Contract

For internal APT:

```bash
sudo apt install nvafx-audio-cli
```

must do exactly one of two things:

1. install and configure a real SDK-enabled `nvafx-audio-cli` command that can run NVIDIA denoiser processing in the user's actual environment; or
2. fail during package configuration with a precise missing requirement.

It must never succeed while leaving `/usr/bin/nvafx-audio-cli` as an SDK-free binary that cannot perform real denoiser processing.

## Package Split

Internal packages use this split:

- `nvafx-audio-cli-validation`: SDK-free validation binary only, installed as `nvafx-audio-cli-validation`.
- `nvafx-audio-effects-sdk-present`: verifies operator-provided NVIDIA Audio Effects SDK files.
- `nvafx-audio-effects-denoiser-model-48k-present`: verifies operator-provided denoiser `.trtpkg` model.
- `nvafx-gpu-runtime-visible`: verifies NVIDIA GPU runtime visibility.
- `nvafx-audio-cli-sdk-enabled`: installs the SDK-enabled project binary plus `/usr/bin/nvafx-audio-cli` wrapper.
- `nvafx-audio-cli`: meta package depending on the real SDK-enabled package chain.

Internal Debian versions use an internal suffix such as:

```text
0.3.1+eikai1
```

This is greater than the public SDK-free `0.3.1` package and allows upgrade from the old SDK-free command path.

## External SDK Boundary

Internal packages may include project-built binaries, wrappers, configuration, and maintainer scripts.

They must not include NVIDIA SDK files, feature libraries, model files, CUDA redistributables, generated media, credentials, or SDK acquisition logs.

The operator-provided SDK configuration file is:

```text
/etc/nvafx-audio-cli/sdk.env
```

Default values:

```sh
NVAFX_SDK_ROOT=/opt/nvidia/Audio_Effects_SDK
NVAFX_DENOISER_MODEL=/opt/nvidia/Audio_Effects_SDK/features/denoiser/models/sm_89/denoiser_48k.trtpkg
```

Presence-check packages fail configuration when these paths, GPU runtime visibility, or denoiser smoke tests are missing.

## Real Command Wrapper

The real internal command is `/usr/bin/nvafx-audio-cli`.

It reads `/etc/nvafx-audio-cli/sdk.env`, sets process-local library paths, preserves user arguments, and execs:

```text
/usr/lib/nvafx-audio-cli/nvafx-audio-cli-sdk-enabled
```

Successful package configuration must prove real file-to-file and stdin/stdout denoiser processing.
