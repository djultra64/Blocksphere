# M6 candidate with platform-specific documentation — 2026-10-01

This package refresh keeps the Linux and Windows game and configurator binaries from `e6904e3fc77f4ab6f68a3d56cdddbed4068d1d97`. Its build ID remains `d074b284451ccc0398f045e1c7249c81f5f43e615ef3779e5defc74a1c093eb3`. Only the distributed guides changed. The previous packages remain available separately.

| Platform | Private archive | SHA-256 |
| --- | --- | --- |
| Linux x86-64 | `.local/m6/packages-platform-docs/tetrisphere-0.9-linux-x86_64.tar.gz` | `5881d5a15272a1701de37047015fb66b27a93bf079da941334b85e8207c08a71` |
| Windows x86-64 | `.local/m6/packages-platform-docs/tetrisphere-0.9-windows-x86_64.zip` | `cc11694244f635407be7dd0b69e41ee9ad8ba6a5d9d2514e44ec82d7ac37e088` |

Each archive has its own English README, controls, troubleshooting, and release notes. The Linux guides contain no Windows commands, DLLs, or platform references; the Windows guides contain no Linux commands, shared-library paths, Wayland, or X11 references. The package creator and auditor reject opposite-platform references in these guides. Both archives passed their individual audit and the matched-pair audit; five focused package tests passed. No binaries were rebuilt or game modes retested for this documentation-only refresh. Hands-on M6 acceptance remains with the user.
