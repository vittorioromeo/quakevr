# HQ texture pack (PNG)

The high-resolution textures the author plays Quake VR with, in one package: extract it into your Quake folder and
the game looks exactly as it does on his PC. Download: the **`assets-2026-10-08`** release of
[github.com/vittorioromeo/quakevr](https://github.com/vittorioromeo/quakevr/releases/tag/assets-2026-10-08) (the
support files; the installer's HD textures come from there), asset `quakevr-hq-textures-png-2026-10-03.zip`
(614,919,925 bytes, SHA-256 `0c0df0e7b19525ba3fabfd1a88cce871b4b1321a5cae0134d5ae21636116b706`; first published as
the `textures-2026-10-03` release).

## Install

1. Close Quake VR.
2. Extract the zip **into your Quake folder** (the one with `id1`, and `hipnotic`/`rogue` if you own the mission
   packs), keeping its folders. You get `id1\textures\`, `hipnotic\textures\`, `rogue\textures\` and
   `quakevr\textures_quetoo\`. Say yes if it asks to replace files in `quakevr\textures_quetoo\`: they are the same.
3. If you had another texture pack in those `textures` folders, move it out first. If you had the same QRP pack as
   `.tga` files, you can leave them or delete them: the engine loads a `.png` before a `.tga` of the same name.
4. If you use relit maps, run the relight again after installing (it relights the maps whose textures changed);
   see [RELIGHTING.md](../RELIGHTING.md).

To play without the pack, move the three `textures` folders out of `id1`, `hipnotic` and `rogue`.

## What's in it

| Folder | Files | Size | What |
| --- | ---: | ---: | --- |
| `id1\textures\` | 727 | 360 MB | Quake's map textures (QRP), with the per-map folders (`e1m1`, `dm3`, ...) |
| `hipnotic\textures\` | 129 | 88 MB | Scourge of Armagon's map textures (QRP add-on) |
| `rogue\textures\` | 198 | 121 MB | Dissolution of Eternity's map textures (QRP add-on) |
| `quakevr\textures_quetoo\` | 1078 | 47 MB | Quetoo's normal, specular and glow maps (also in the Quake VR package; included so the pack is complete) |

- The map textures are the **Quake Revitalization Project (QRP)** map texture packages
  (`QRP_map_textures_v.1.00.pk3`, `QRP_SoA_map_textures_add-on_v.1.00.pk3` and the Dissolution of Eternity map
  textures, from the [QRP Archive on ModDB](https://www.moddb.com/addons/quake-revitalization-project-archive)),
  as installed in the author's Quake folder.
- **Lossless:** the original `.tga` files converted to `.png` (zlib, no colour change). Every file was decoded with the
  engine's own image decoder (stb_image, as `Quake/image.c` builds it) from the `.tga` and from the `.png`: the same
  RGBA pixels, alpha included, for all 1054 files. A `.tga` whose alpha is 255 everywhere (10 files) is stored as an
  RGB `.png`: the engine reads alpha 255 for it, the same pixels. Half the size of the `.tga` files (569 MB against
  1.32 GB).
- **Checked in the game:** `imagehash` (Debug menu > Save to Files > Texture Checksums) checksums every texture as the
  GPU holds it. On e1m1, e2m1, hip1m1 and r1m1 the `.tga` and `.png` files give the same checksum for every texture
  (723 to 866 each). The maps load as fast or a little faster from `.png` (e1m1: 0.86 s against 0.94 s).
- The Quetoo files are unchanged copies of Quake VR's `quakevr/textures_quetoo` (see its `README.md`).

## Credits

- **id Software**: Quake, and with Hipnotic Interactive and Rogue Entertainment its mission packs Scourge of Armagon
  and Dissolution of Eternity, and their original textures, which every texture here retextures. Quake itself is not
  included: you need your own copy.
- **Quake Revitalization Project (QRP)** (qrp.quakeone.com; the "Quake Retexture Project" in its first releases): the
  map textures. Urgefor, RaRe, Up2nOgOoD[ROCK], Primevil, My-Key, Moon[Drunk], [Win]Elchtest (who started the
  project) and the QRP team.
- **Rygel's "Texturepack Ultra for Quake/DarkPlaces"**: the pictures of the Quetoo material maps come from it. Rygel
  credits as his sources: Yves "evillair" Allaire (hfx); the Debaser Texture Set v1 by Jon "Starbuck" Miles; quake.cz's
  qe1 set; the QRP (above); the Aerowalk textures by Pez/Mortuality; the CTF & EXMX Map Texture Pak by woodsk7; Randy's
  Quake Textures (QExpo style); Alexander "_argv[-1]"; free textures by William Smith and Mayang Murni Adnin
  (mayang.com); Filter Forge; MoonDrunk; Ruohis; n30g3n3s1s (Quake Remodeled); Jose "Jaj" Arcediano; LordHavoc; and
  id Software.
- **Quetoo** ([github.com/jdolan/quetoo](https://github.com/jdolan/quetoo)): **Jay Dolan (jdolan)** and the Quetoo
  contributors, the normal, specular and glow maps and material files in `quakevr/textures_quetoo`
  ([quetoo-data](https://github.com/jdolan/quetoo-data), commit `fcb502b3fa`).
- **Quake VR** (Vittorio Romeo): the PNG conversion and the package.

## Licence

- `quakevr/textures_quetoo/`: **Creative Commons Attribution-ShareAlike 4.0 International** (CC BY-SA 4.0), the
  quetoo-data licence; full text in `quakevr/textures_quetoo/LICENSE.md` and at
  [creativecommons.org/licenses/by-sa/4.0](https://creativecommons.org/licenses/by-sa/4.0/legalcode).
- The QRP map textures (`id1`, `hipnotic`, `rogue`): the QRP packages came with no licence file. They were released
  free of charge by the QRP team for players of Quake in every engine, and are redistributed here unchanged in
  content (only converted from TGA to PNG, losslessly), free of charge, with credit. They are retextures of
  id Software's art, which remains id Software's. If you are one of their authors and want them removed or credited
  differently, open an issue on [github.com/vittorioromeo/quakevr](https://github.com/vittorioromeo/quakevr/issues).
- Quake VR itself is GPL v2 (`LICENSE.txt`); the pack is not part of it.

## How it was made (for maintainers)

- The `.tga` originals are backed up in `C:\OHWorkspace\quake-textures-tga-backup\<gamedir>\textures\` on the
  author's PC. The conversion decodes each `.tga` with stb_image (native channels), writes the `.png` with Pillow
  (`optimize`), and checks: stb(`.png`, RGBA) equals stb(`.tga`, RGBA); Pillow reads back the exact pixels; the backup
  is byte-identical to the original.
- In-game check: a game folder with only the `.tga` files and one with only the `.png` files, `map <m>; imagehash` in
  each; the `imagehash.txt` files (sorted) are identical.
