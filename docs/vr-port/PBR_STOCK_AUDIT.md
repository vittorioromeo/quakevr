# Stock and mission-pack relief preservation — 2026-10-13

The learned AD corpus shared content hashes with many existing Quake materials. The previous loader could
replace their procedural normals/height before the original material loader ran. The two explosive-box models
had already been protected, but that fix did not protect world textures.

The stock protection index now preserves the complete original loader for every scanned stock BSP source and
effective installed replacement. This also preserves those materials when another map reuses the same pixels.
Named authored normals/bump files, matching external materials, animation/frame reuse and procedural relief
keep their original selection, uploaded bytes and strength. Protected companions are not decoded unnecessarily.
The box trait guards remain; alias skins already exclude learned world companions. Novel AD sources retain
generated-only strength and independent normal/height composition.

## Complete discovery

The native Steam and agent-kit id1/Hipnotic/Rogue roots were scanned together with quakevr overrides, using the
engine's exact extension/root ordering and contiguous PAK mounting. PAKs beyond a numbering gap are not mounted.
The census contains 8,165 source occurrences, including embedded and selected HD replacements, all BSP miptex
animation frames, and every MDL single/group skin. No scan errors occurred and no stock image assets are shipped.

| Campaign | BSP assets (maps and brush models) | MDL assets | Source occurrences |
|---|---:|---:|---:|
| Quake | 51 | 79 | 3,872 |
| Hipnotic | 18 | 23 | 2,315 |
| Rogue | 23 | 64 | 1,978 |

The index protects 1,559 distinct BSP diffuse identities. The corpus overlaps 712 eligible stock identities:
526 contain generated components; 186 are copies of authored pack data baked by the pipeline. The 526 generated
overlaps include both original miptex and installed HD pixels (4,237 occurrences: 2,159 Quake, 1,204 Hipnotic,
874 Rogue; 2,519 embedded and 1,718 selected replacements). Alias skins have zero corpus collisions and remain
excluded by the loader regardless of content identity. These overlap counts are potential selection collisions,
not counts of authored maps actually overwritten.

Machine-readable sources: [pbr_stock_audit.json](pbr_stock_audit.json). Runtime evidence:
[pbr_stock_runtime.json](pbr_stock_runtime.json). Neither artifact includes pixels from stock assets.

## Runtime reproduction and preservation

Every stock campaign map was loaded with `vr_pbr 0` and `1`, with generated strength at 8. GPU readback compares
selected normal/height bytes, dimensions, material source and component strength, including external pack and
animation selection. Source filenames alone do not establish whether a selected animation/ammo normal is
authored: those materials can reuse another frame/model's procedural map.

| Campaign | Maps tested | Previous loader: changed world material comparisons |
|---|---:|---:|
| Quake | 38 | 535 |
| Hipnotic | 18 | 360 |
| Rogue | 17 | 280 |

The previous behavior was reconstructed from the shipped loader, retaining the earlier box-only guard. Across
73 maps it changed 1,175 procedural world material comparisons: all 1,175 changed normal RGB and 1,135 changed
height alpha. All 1,953 selected authored external materials were unchanged. No additional brush-model or alias
regression was reproduced. Therefore the confirmed regression is procedural world replacement, not an authored
external map being overridden. The authored copies in the corpus also must obey the stock material selection.

With protection, all 81,770 paired precached world/brush/alias/frame comparisons were identical to `vr_pbr 0`,
with no missing pairs, across all 73 maps. This includes the stock map set plus the models Quake VR precaches.
Additional Quake/Hipnotic/Rogue controls cover sharp and smooth indexed textures, native HD textures with external
maps disabled, and sharp native HD textures with authored external maps enabled. Generated-only synthetic
fixtures verify independent normal/height precedence and strength; native AD verifies that novel generated
materials still scale and stock/authored ones remain at 1.

Reproduce discovery and GPU comparisons with `Misc/quakevr/pbr_stock_audit.py`, `pbr_stock_audit_test.py`,
`pbr_stock_sweep.ps1`, and the commands in [PBR_PIPELINE.md](PBR_PIPELINE.md). Refresh the index after adding
stock texture replacement packs. Unknown future replacement hashes are not inferred from texture names.
