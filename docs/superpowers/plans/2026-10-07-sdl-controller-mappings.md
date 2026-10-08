# SDL mapping compatibility correction

Authorized scope: adapt the Blitz correction to the latest Tetrisphere source,
with no release publication, guessed joystick layout or gameplay change.

- [x] Inspect current source and existing data/input/package paths.
- [x] Reproduce missing bundled mappings with failing package/input checks.
- [x] Load pinned bundled and optional local mappings before SDL initialization.
- [x] Preserve explicit SDL settings, independent players and action/family layer.
- [x] Include database/license/provenance in build and package inputs.
- [x] Run affected C++/Python checks and compile the changed game platform unit.
- [x] Record final broader verification and apply exact reviewed files to main.

Single-agent execution; independent copies for tests. Full game binaries and
physical/native Windows acceptance are outside the completed source checks.
