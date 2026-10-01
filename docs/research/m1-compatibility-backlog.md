# M1 compatibility backlog

## M2 blockers to investigate

- Exercise full start-to-result sessions and add each newly reached indirect
  target or libultra route only after a focused failing regression.
- Persist EEPROM safely, including interrupted-write recovery and path policy;
  M1 proves the in-memory 4-Kbit contract only.
- Validate audio pacing over long sessions, device loss and focus changes.
- Cover every mode's display-list state, transitions and framebuffer effects;
  the M1 command sample is representative, not exhaustive.
- Add a ROM importer/frontend so users never place a ROM inside a package.

## Input and prompts

- Preserve logical actions independently from physical buttons.
- Detect the last active Xbox, PlayStation or Nintendo Switch family; account
  for Nintendo A/B and X/Y positions; keep keyboard and manual-family fallback.
- Replace original N64 prompt textures/overlays with the active physical glyph
  and verify prompt plus resulting action on real controllers. M1 exposes and
  tests the shared binding/label API but does not claim visual replacement.

## Deferred robustness

- Make Linux RT64/ultramodern shutdown clean after the diagnostic window or a
  termination signal; the final evidence artifacts are complete, but teardown
  currently reports `terminate called without an active exception`.
- Tighten reserved MIPS encoding and zero-register heuristics in the analyzer.
- Preserve primary trace-collector errors if detach also fails; reject
  non-finite event times and boolean integer fields.
- Complete JSON escaping for all control bytes in fatal diagnostics.
- Remove the runtime-contract test's default-checkout include assumption when
  an alternate verified N64ModernRuntime root is selected.

No item above is silently stubbed. Reached unsupported routes remain fatal and
name their symbol/address, callsite and phase.
