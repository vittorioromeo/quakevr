# Quetoo material maps for Quake's textures

The normal maps (`*_norm.png`, height in the alpha), specular maps (`*_spec`), glow maps (`*_luma`, `*_glow`),
material files (`*.mat`) and pictures (`<name>.jpg/png`) in this folder are from the **Quetoo game data**. Quake VR
reads them as a map loads (`vr_extmaps 1`, `vr_extmaps_dir textures_quetoo`: Graphics > Surfaces > External Maps) and
uses a texture's maps only where the picture here matches the texture drawn (`vr_extmaps_match`).

## Source

- Project: [Quetoo](https://github.com/jdolan/quetoo), a free first-person shooter by **Jay Dolan (jdolan)** and
  contributors.
- Files: [github.com/jdolan/quetoo-data](https://github.com/jdolan/quetoo-data), `target/default/textures/quake/`, at
  commit `fcb502b3fa0a6b6edf23ad628fdba417b002b5f6` (2026-09-30).
- Licence: **Creative Commons Attribution-ShareAlike 4.0 International** (CC BY-SA 4.0), the quetoo-data licence;
  its full text is [LICENSE.md](LICENSE.md) here (also at
  [creativecommons.org/licenses/by-sa/4.0](https://creativecommons.org/licenses/by-sa/4.0/legalcode)).

## Authors

- **Quetoo game data**: Jay Dolan (jdolan) and the Quetoo contributors: the normal, specular and glow maps and the
  material files for the Quake texture set, and its pictures at their size.
- The pictures come from **Rygel's "Texturepack Ultra for Quake/DarkPlaces"** (quetoo-data
  `target/default/docs/textures-rygel.txt`). Rygel credits as his sources: Yves "evillair" Allaire (hfx); the Debaser
  Texture Set v1 by Jon "Starbuck" Miles; quake.cz's qe1 set; the **Quake Retexture Project** (QRP: Urgefor, RaRe,
  Up2nOgOoD[ROCK], Primevil, My-Key, Moon[Drunk] and the QRP team; quetoo-data `docs/textures-qrp.txt` adds
  [Win]Elchtest, who started the project); the Aerowalk textures by Pez/Mortuality; the CTF & EXMX Map Texture Pak by
  woodsk7; Randy's Quake Textures (QExpo style); Alexander "_argv[-1]"; free textures by William Smith and Mayang Murni
  Adnin (mayang.com); Filter Forge; MoonDrunk; Ruohis; n30g3n3s1s (Quake Remodeled); Jose "Jaj" Arcediano;
  LordHavoc; and id Software.
- The textures they retexture are **id Software**'s Quake textures (not included: the player's own copy of Quake is
  drawn).

## What Quake VR changed

- **The files are unchanged**: byte-for-byte copies of quetoo-data's. Only a subset is here (1076 of the folder's 1880
  files, 46.4 MB): the files of the 327 texture names of Quake, Scourge of Armagon and Dissolution of Eternity whose
  picture here matches the texture drawn with id's textures or with the QRP pack (`vr_extmaps_match` 0.5; picked by
  `Misc/quakevr/select_quetoo_maps.py` from `vr_extmaps_stats all` on every map).
- At load (`Quake/vr/vr_extmaps.cpp`, nothing written back): a normal map's green channel is turned where it runs
  against its own heights (Quetoo's run down the rows; `vr_extmaps_green`); the specular maps' brightness is scaled
  (`vr_extmaps_spec_scale`), times the .mat's `specularity`, and its `hardness` narrows the highlight; the pictures are
  only compared with the textures, never drawn.

## Licence obligations

CC BY-SA 4.0 applies to these files (not to the rest of Quake VR, which is GPLv2). Anyone redistributing them must:
keep this credit (the authors above, the licence's name and link, the source) and say what was changed; and license
any modified version of them (resized, recoloured, turned maps) under CC BY-SA 4.0 or a compatible licence, with no
added restrictions (no DRM on them, no further terms). Merely using them beside other work, as Quake VR does, does not
put that work under CC BY-SA.
