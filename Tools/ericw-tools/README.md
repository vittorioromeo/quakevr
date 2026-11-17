# light.exe (ericw-tools 2.0.0-alpha11-qvr1, Windows, x64)

The `light` the game's relighting runs (Graphics > Relighting, the installer's relight of the official maps:
`Quake/vr/vr_relight.cpp`), shipped in the package as `quakevr\tools\ericw-tools\` (`Windows/package-quakevr.ps1`
copies this folder, with `Misc/quakevr/ericw-tools-NOTICE.txt` as `NOTICE.txt`). It is ericw-tools 2.0.0-alpha11's
`light` with a few patches: with bounced light (the game's default, `vr_relight_bounce 1.2`) a relight takes about half
as long (e1m1 8 s -> 4 s, e3m5 21 s -> 8.5 s, ad_mountain 95-113 s -> 50 s, 31 threads), and two runs write the same
bytes. The game passes `-bounceextra 2` whenever it relights with bounced light (`vr_relight_bounceextra`, default 2;
0: stock behaviour), and only to a `light.exe` that has the option (stock ericw-tools would stop at it).

- **Source:** https://github.com/vittorioromeo/ericw-tools, branch `qvr-fast`: three commits on top of upstream
  `ericwa/ericw-tools` tag `2.0.0-alpha11` (`c243aa34`), the same changes as `patches/` here (`git am` applies them to
  the tag), tagged `2.0.0-alpha11-qvr1` (`4e0f8c9f`). The Corresponding Source of these binaries is also offered as
  `ericw-tools-2.0.0-alpha11-qvr1-src.zip` (the tag's source with its submodules, the patches committed, `patches\`
  and `README-qvr.md`) on the support-files release `assets-2026-11-17` beside the Windows zip
  `ericw-tools-2.0.0-alpha11-qvr1-win64.zip` the game's Download ericw-tools fetches (docs/vr-port/RELEASING.md,
  "Support files").
- **`0001`, deterministic lightmap packing:** each face's lightmap gets its place in the file in face order once all
  faces are lit, not in the order the threads finish: two runs with the same input write byte-identical
  `.bsp`/`.lit`/`.lux`. It also adds the options below (all off by default).
- **`0001` + `0002`, `-bounceextra N`:** in the indirect (bounced light) pass, each (E/N) x (E/N) block of a face's
  `-extra E` samples traces its bounced light from the one unoccluded sample nearest the block's centre and shares it
  with the block (each sample keeps its own direct light and ambient occlusion; samples on another face or normal, or
  off the grid, are traced alone). With `-extra4 -bounceextra 2` the bounce pass is about 2x faster; the lightmaps
  change by a mean of 0.25-0.31 per byte (of 255), 0.3-0.7% of bytes by more than 2, a few small faces up to ~17; the
  `.lux` and the light grid are unchanged. Experimental, unused by the game: `-rtpackets N` (occlusion rays in
  packets: under 10% faster, not bit-exact) and `-bouncegate G` (visibly darker: rejected).
- **`0003`, the version label:** `2.0.0-alpha11-qvr1` (set in `CMakeLists.txt`, so a build from the source zip prints
  it too: `light` prints `---- light / ericw-tools 2.0.0-alpha11-qvr1 ----` at its start).
- **Output** without `-bounceextra` (and the experimental options) is the same light as stock 2.0.0-alpha11: a face by
  face comparison of the `.bsp` lighting, `.lit` and `.lux` (e1m1 and e3m5, with and without bounce) finds 0 bytes
  different and the same light grid; the files differ only in the lightmaps' order (the packing above).
- **Built with** VS 2022 (MSVC x64, its CMake and Ninja), Embree 4.4.0 and oneTBB 2021.11.0 (their release archives),
  Release, from the source zip's tree:
  `cmake -S <src> -B build -GNinja -DCMAKE_BUILD_TYPE=Release -DENABLE_LIGHTPREVIEW=OFF -DDISABLE_TESTS=ON
  -DDISABLE_DOCS=ON -Dembree_DIR=<embree-4.4.0>\lib\cmake\embree-4.4.0 -DTBB_DIR=<oneapi-tbb-2021.11.0>\lib\cmake\tbb`,
  then `cmake --build build --target light`. The DLLs are those of ericw-tools' own
  `ericw-tools-2.0.0-alpha11-win64.zip`, unchanged.

| File | Bytes | SHA-256 |
|---|---|---|
| `light.exe` | 1,135,616 | `2cdf37c938d8cceb554865adb2b0570e243703c33e5cbad12beeb00317f9db6f` |
| `embree4.dll` | 37,316,096 | `5544154cb033375ef03ce07804bdfe5153a588607f0fa0a23764a3863983c732` |
| `tbb12.dll` | 318,080 | `68c07bc7c2161637f37f11ebf4af5be2f940138cbcaa717bb71282fd55a827ef` |
| `tbbmalloc.dll` | 115,040 | `a53efbc0f996ed45575571b7f65c62856ba890a705ff639d4e701061c7063f8c` |

**Licences.** ericw-tools is GPL-3.0-or-later (`gpl_v3.txt`; Copyright (C) Eric Wasylishen, Kevin Shanahan (Tyrann),
David Walton (Spike) and the ericw-tools contributors; the patches Copyright (C) Vittorio Romeo); its source is at the
link above. `light.exe` runs as a separate program and is never linked into Quake VR. Embree (`embree4.dll`) and oneTBB
(`tbb12.dll`, `tbbmalloc.dll`): Copyright (C) Intel Corporation, Apache-2.0 (`LICENSE-embree.txt`, the same text for
both), which allows redistributing the binaries with the licence text, as ericw-tools' own release does.

To use another `light`: `vr_relight_tool <path>` in the game, `--light` for `Misc/quakevr/relight_maps.py`,
`QVR_ERICW_TOOLS=<folder>` for the packaging. Stock ericw-tools 2.0.0-alpha11 works too (the game then relights at
the full cost of the bounce and says so in the console).
