# .mdl vertex dequantisation (vr_mdl_dequant)

## Status

- [x] Load-time solver, VBO offsets (SSBO 11 `PoseFrac`), shader, cvar `vr_mdl_dequant` (default 1), menu row
  (Graphics > Models and Effects > Steady Model Vertices), anchors on the drawn positions, `vr_mdl_dequant_check`.
- [x] Merged into `next` (first milestone).
- [ ] Byte-identical proof for `vr_mdl_dequant 0`, before/after pinned screenshots, better jitter metric, load-time
  optimisation (first measure: 140 models in 356 ms at e1m1, player.mdl 41 ms), Debug menu row.

## Design

See the header of `Quake/vr/vr_mdldequant.cpp`. In short: per vertex and axis, along each animation run (frame groups;
single frames by name stem, split at large jumps; loops detected by the closing distance), the path with the least
squared second differences that stays within +/- half a byte step of each stored value (projected FISTA). The offsets
(three snorm8 per pose and vertex, 1/254 step) are kept in the model's hunk data (`aliashdr_t::dequant`) and uploaded
after the poses in the model's VBO (zeros while the cvar is 0, so the shader adds exactly 0). Normals are kept.
CPU readers keep the stored bytes, except anchors (`vr_anchor.cpp`), which read the drawn positions.

## Numbers

(first run, e1m1; `vr_mdl_dequant_check`) max offset 0.500 steps on every model (bound 0.5). Mean |second difference|
(all motion, so dominated by real movement): player 7.43 -> 6.98, dog 7.66 -> 7.31, demon 5.67 -> 5.08, zombie
2.41 -> 2.15 units. Memory: 4 bytes per pose and vertex (as much as the poses themselves), 3.9 MB for 140 models.
