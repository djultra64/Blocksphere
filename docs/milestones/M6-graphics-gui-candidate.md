# M6 graphics settings GUI candidate — 2026-10-01

This candidate adds desktop dialogs to `tetrisphere-config` when launched without arguments. The dialogs display the saved resolution, aspect ratio, refresh request, and window mode; each selection saves immediately through the existing settings store. The command-line interface still supports graphics settings and controller mappings. Controller mappings are deliberately absent from the desktop dialogs, as requested by the user.

Both executables were built from `e6904e3fc77f4ab6f68a3d56cdddbed4068d1d97`. The common build ID is `d074b284451ccc0398f045e1c7249c81f5f43e615ef3779e5defc74a1c093eb3`.

| Platform | Private archive | SHA-256 |
| --- | --- | --- |
| Linux x86-64 | `.local/m6/packages-gui-candidate/tetrisphere-0.9-linux-x86_64.tar.gz` | `b473849bcafb8ea4e44bd50946295414c8da3fdd36d64ba6c70aadec01e1e65b` |
| Windows x86-64 | `.local/m6/packages-gui-candidate/tetrisphere-0.9-windows-x86_64.zip` | `f15a426b8aeb6352b72b1d976c6d01c4544ecc2139a1afc7a29dcadc2bde82b9` |

The package pair audit passed with matching source commit and build ID. The Windows native build receipt identifies the same commit, and the five retrieved executable and runtime files match its SHA-256 values. From extracted packages, `tetrisphere --help` and the configurator's `show` command returned successfully on both systems. Both Linux executables resolved the packaged SDL2 library. A single isolated Linux desktop check confirmed that the settings dialog opens and closes. The targeted graphics GUI and settings-store tests passed.

The user is performing visual and hands-on M6 acceptance testing. The Windows desktop dialog, game modes, audio, controllers, physical 4K output, and extended sessions were not tested in this candidate by the agent. The earlier matched candidate and the layout preview remain preserved separately.
