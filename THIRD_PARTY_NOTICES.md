# Third Party Notices

## NVIDIA Audio Effects SDK

No NVIDIA SDK binaries, DLLs, models, AI features, redistributables, installers, archives, generated audio/video files, or sample media are included in this repository.

NVIDIA Audio Effects SDK is used only when separately installed by the user. The official NVIDIA Maxine AFX SDK API repository may be cloned outside this repository as a local build dependency for headers and import libraries. It is not vendored or copied into this repository.

If a small piece of NVIDIA sample source is later adapted, add the following notice:

> This software contains source code provided by NVIDIA Corporation.

This project is not sponsored, endorsed, or approved by NVIDIA. NVIDIA, Maxine, RTX, and related names are trademarks of NVIDIA Corporation.

## Future Dependencies

No third-party source dependencies are included at this time.

The current C++ implementation does not include NVIDIA SDK source, binaries, models, or generated media. SDK-enabled builds include NVIDIA headers from an external local checkout at build time only.
