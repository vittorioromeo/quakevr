# Weapon attachments

Models drawn on a weapon, in the weapon model's own space (+x forward, +y left, +z up, the units of its .mdl), each at
a mount: a position, angles (pitch, yaw, roll in degrees, Quake's order) and a scale. The attachment system itself is
agent/attach1's (posebake); this file holds the attachments' models and their measured mounts.

## The bayonet

`quakevr/progs/bayonet.mdl`, made by `Misc/quakevr/make_bayonet.py` (an original design, scripted: rerun it to rebuild
the model, its full-colour skin `bayonet.mdl_0.png`; `python Misc/quakevr/bake_normals.py bayonet.mdl` bakes its normal
map from the generator's relief). A socket bayonet in Quake's style: a short octagonal socket with a lug on top that
clips under a barrel (steel bands, a leather strap), a cranked leather-wrapped shank, a small guard and a 20 cm
single-edged blade (the dagger's is 27 cm) with fullers and a clipped point, edge down.

Its model space: one unit a world unit (its real size on its own), +x along the blade, +z up. **The origin is the
mounting point**: the top of the socket's front end, where it meets the barrel's underside at the muzzle. Its bounds:
x -0.9 (the socket's back) to 6.5 (the tip), y +-0.43, z -1.42 (the guard's foot) to 0.12 (the lug's top).

### Mounts (frame 0, the guns' idle pose)

| gun model | position (gun units) | angles | scale | the tip lands at | where |
|---|---|---|---|---|---|
| `progs/v_shot.mdl` | 32.9 0 2.84 | 0 0 0 | 2.2727 (1 / 0.44) | 47.67 0 0.98 | under the barrel at the muzzle (crown x 33.07); the socket's back (x 30.85) clear of the pump's front (x <= 30.8) |
| `progs/v_shot2.mdl` | 26.0 0 3.62 | 0 0 0 | 2.0833 (1 / 0.48) | 39.54 0 1.91 | under the barrels' shroud, between and below the two barrels, behind its chamfered front (muzzle x 28.5) |
| `progs/g_shot0.mdl` (shotgun pickup) | 18.2 0 33.79 | 0 0 0 | 2.3048 | 33.18 0 31.90 | under the barrel at the muzzle (crown x 18.4) |
| `progs/g_shot.mdl` (super shotgun pickup) | 21.6 0 34.86 | 0 0 0 | 2.9398 | 40.71 0 32.45 | under the two barrels where they stand out of the shroud; the socket's back tucked into the shroud's front |

The scale: a view model's is 1 / its weapons.txt `scale` (slots 1 and 2: 0.44, 0.48), so the bayonet draws at its real
size beside the gun as the game draws it. The world models' keep the same proportion to their gun (their length over the
view model's, over the view scale). The numbers are `MOUNTS` in `make_bayonet.py`, which prints them and, with
`--placed <folder>`, writes `bayonet_on_<gun>.mdl` (the bayonet moved into each gun's space) for renders:
`blender -b --factory-startup -P Misc/quakevr/blender/render_views.py -- quakevr/progs/v_shot.mdl+<folder>/bayonet_on_v_shot.mdl out 512 left fl`.

Mount data for the attachment system (same numbers; one line a gun: model, position, angles, scale):

```
bayonet progs/bayonet.mdl
  progs/v_shot.mdl   32.9 0 2.84    0 0 0  2.2727
  progs/v_shot2.mdl  26.0 0 3.62    0 0 0  2.0833
  progs/g_shot0.mdl  18.2 0 33.79   0 0 0  2.3048
  progs/g_shot.mdl   21.6 0 34.86   0 0 0  2.9398
```

Notes for the system: the guns' fire animations move their front (up to 17 units over frames 1-8 on both view models):
a mount that follows the gun's recoil wants the barrel's vertices' motion (or the frame's), not frame 0's fixed place.
The shotgun's auto pump (`vr_pump_on_v_shot.mdl`) slides back from x 30.8, away from the socket. Only the blade's line
(below the barrel by 0.62 bayonet units) passes the muzzle, so the shot's line is clear.

### On its own

A small knife-like prop: `vr_bayonet` (QC `vr_bayonet.qc`; the toolgun's Props, "Bayonet"), taken, swung and thrown as
a held object, its hold in `quakevr/data/props.txt` slot 71 (Held Object Offsets, `vr_prop_*_72`): 0.45 kg, held Along
the Handle from the socket's back (x -0.9) to the guard (x 1.06), one hand, its tip (6.5 0 -0.82) the striking end, the
socket's back (-0.9 0 -0.4) the butt. In the dagger's terms (weapons.txt slot 51's kind, if it is made a sword kind
later): the tip (muzzle_av) 6.5 0 -0.82, the blade's root (wpnbtn_av) 1.2 0 -0.97, the grip's middle 0.6 0 -0.94, the
blade from 0.10 of the grip-to-tip line (bl_from).
