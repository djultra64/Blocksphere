# M1 initial symbol map

The machine-readable sources are `config/recomp/sections.json`,
`config/recomp/symbols.toml`, `config/recomp/runtime-symbols.json` and the
sanitized static/dynamic research reports. Generated C and ROM bytes remain
private under `.local/m1/`.

Key confirmed anchors:

| Address | Role | Evidence |
|---|---|---|
| `0x80025C50` | recompiled program entry | ROM header/M0 plus executed boot path |
| `0x800D1DC0` | `osInitialize` | static signature and runtime route |
| `0x800D2050` | `osPiRawReadIo` | observed cartridge PIO and bounded contract |
| `0x800D1250` | `osCreateThread` | static map and ultramodern delegation |
| `0x800D13A0` | `osStartThread` | static map and ultramodern delegation |
| `0x800DD3D0` | graphics microcode entry | live `OSTask`/RT64 submissions |
| `0x800DE7D0` | audio microcode entry | live tasks, hash and RSPRecomp output |
| `0x8007EF9C` | confirmed indirect target | static/dynamic merge and guarded dispatch |

The initial program is modeled as the boot-loaded section set in
`sections.json`. No overlay load was confirmed in the traced title/Rescue
route; candidates remain explicitly unconfirmed rather than being treated as
functions or sections. Jump tables and indirect sites retain provenance and
unknown targets fail with address/callsite/phase diagnostics.
