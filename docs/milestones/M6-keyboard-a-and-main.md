# M6 keyboard Magic update and main integration — 2026-10-01

Runtime source: `a5974f7024a2c9ea093a2b8b276467d440d6c6c1`. Package pair build ID: `67a9a266a98239ce612cab0a0ca081e23fcbb019a67633eb1f74e08fb99e9dba`.

## Change

Keyboard Magic uses A, beside Z (confirm/drop) and X (cancel/slide). The former N64 L keyboard input moves to J so A emits Magic alone. Controller Magic remains Xbox X, PlayStation Square, or Nintendo Switch Y. Persistent settings and controller assignments retain their existing schema and values.

Keyboard physical-button and Magic labels match the new keys. Training/Hide & Seek font variants and the prompt atlas use an A glyph for the keyboard west-button assignment, including optional per-action remapping. Each platform's English README and controls document show A for Magic and J for N64 L.

## Verification

Three targeted CTests passed: `keyboard_focus_loss`, `training_prompts`, and `prompt_atlas_icons`. The first two were observed failing for the missing A mapping/glyph before implementation. The checks exercise A producing only Magic, the matching keyboard label, the displaced shoulder input on J, key release/focus clearing, remapped actions, and a hand-checked A glyph raster.

Linux and Windows executables were rebuilt natively from the runtime source above. Windows transfer and binary hashes matched the build receipt. Both extracted game executables ran their help successfully; Linux default settings inspection retained the west-button Magic assignment. Both archives passed individual and matched-pair audits. No game modes or desktop screens were tested for this small update; physical testing remains with the user.

## Packages

| Platform | Private archive | SHA-256 |
| --- | --- | --- |
| Linux x86-64 | `.local/m6/packages-keyboard-a/tetrisphere-0.9-linux-x86_64.tar.gz` | `3be31de6b896c859d9eb614a699742a3afefc7dd4d34f8f75d119f9b79d35ea6` |
| Windows x86-64 | `.local/m6/packages-keyboard-a/tetrisphere-0.9-windows-x86_64.zip` | `90448c3ac6d04538ec44a4032edc6c902e2d8049f7a5b8b759865b1ab0fa51e5` |

The updated Windows package is copied to `transfer/tetrisphere-0.9-windows-keyboard-a.zip` under the existing private build workspace and extracted into `transfer/beta-keyboard-a`. Previous packages are retained.

## Integration setup

The user explicitly requested local integration into `main` as the base for a later beta 2. There was no existing local or remote `main`; its worktree is `.worktrees/main`. The main repository remains on `codex/m0-baseline`, and existing milestone branches and worktrees are retained. The authorized integration contains `codex/m6-beta` and this keyboard update. Uncommitted changes in `codex/close-game` are separate and are not included.

Use `main` as the base for the next beta's development branch. Package runtime provenance remains the code commit above; later receipt-only commits do not change the compiled executables. This update does not constitute hands-on M6 acceptance or implement the future beta 2 feature.
