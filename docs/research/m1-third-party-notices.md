# M1 third-party notices

The diagnostic prototype is built from the exact revisions in
`config/dependencies-m1.json`. N64Recomp, N64ModernRuntime and RT64 are used
under their upstream open-source licenses. Their transitive build includes
SDL2 (zlib license), nlohmann/json (MIT), xxHash (BSD-2-Clause), zstd
(BSD/GPL dual license) and renderer/compiler components recorded by RT64.

The archives contain a deterministic `THIRD_PARTY_NOTICES.md` generated from
every `LICENSE*` and `COPYING*` file in the pinned N64ModernRuntime and RT64
checkouts. Generate it with:

```sh
python3 tools/qa/build_m1_notices.py \
  --dependency N64ModernRuntime=.local/deps/N64ModernRuntime \
  --dependency RT64=.local/deps/rt64 \
  --output build/packages/THIRD_PARTY_NOTICES.md
```

The package audit requires the complete-bundle marker and its exact hash. No
Nintendo, H2O or game data, ROM, extracted asset, save, trace, screenshot or
captured audio is included in a diagnostic archive.
