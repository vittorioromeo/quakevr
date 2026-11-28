# Weapon attachments

Models drawn on a weapon, in the weapon model's own space (+x forward, +y left, +z up, the units of its .mdl), each at
a mount: a position, angles (pitch, yaw, roll in degrees, Quake's order) and a scale. The attachment system itself is
agent/attach1's (posebake); this file holds the attachments' models and their measured mounts.

## The system (agent/attach1, 2026-11-26)

- **Kinds are data.** `Quake/vr/vr_attach.cpp` `kinds[]`: id, name, model, its tip and its blade's start in its own model
  space (1 the bayonet). QC `vr_attach.qc`: the prop class of each kind (`VR_Attach_PropKind`: `vr_bayonet`) and what it
  adds to a weapon. A new kind: a row in `kinds[]` and `mountKeys[]`, its `attN_*` keys in `vr_weapons.inc` and the
  weapons.txt defaults, its prop, and its effect in QC.
- **Which weapons take it, and where: per weapon in the Weapon Offsets data** (`att1_ok`; the place, one hierarchy since
  mountoffs, `attach::mount`: **gun -> its mount** (`mnt_x/y/z`, `mnt_pitch/yaw/roll`, `mnt_size`: the gun model's space,
  the mount table below) **-> every gun's offset of the kind** (`vr_attach_bayonet_*`, `vr_attach_dagger_*`: x/y/z,
  pitch/yaw/roll, scale, in the mount's units and axes; the dagger's default -2.79 0.339 -1.47 puts its grip behind the
  socket) **-> the gun's own** (`attN_x/y/z`, `attN_pitch/yaw/roll`, `attN_scale`, the same units; 0 / 1 by default) **->
  the kind's frame**. Moving or turning a gun's mount takes its bayonet and dagger along. Edited in **Weapon Offsets >
  Attachments** (Bayonet Mount, Bayonet, Every Weapon's Bayonet, Dagger, Every Weapon's Dagger; the attachment drawn on
  the held weapon while the page is open, whether it has one or not: `menu::attachPreview`). vr_wofs_version 81 converts
  a config's old gun-space values (`takeMountOffsets81`).
- **Part of the weapon's state.** The weapon's record (`vr_weaponinst.qc`) holds it: `.wi_attach`. Hands, holsters, props
  (dropped, thrown, stuck), the quick slot's copy refer to the record, so it goes everywhere the weapon goes with no code
  per path; saves keep the field; a level change carries it in **parm77** (3 bits per carried weapon; the engine's
  `lastExtSpawnParm` 77).
- **Network (QVR_NET_VERSION 48).** `QVR_SVC_WEAPONATTACH` (58): [long weapon id][byte kind], reliable, sent when a record's
  kind changes (`attach::serverFrame`) and all of them with the spawn state; the client keeps id -> kind
  (`attach::kindOfUid`; `QVR_SVC_WEAPONGONE` forgets it). Your hands and holsters (their id stats) and every lying gun
  (`U_QVR_WEAPONUID`) are drawn from it. Another player's guns: one byte more in each `VrGunLook` (held) and per holstered
  gun of the avatar (snapshots and avatars share the writer).
- **Drawing: one function** `view::placeAttachment(out, gun, kind, ssgHand)` for every state: your hands' guns (VR and flat
  play: the same entities), your holsters', the lying guns' (`worldWeaponsNear`), another player's (`drawGunParts`: held
  and holstered). Rigid in the gun's model space (its place, turn, Scale, mirroring), moved with the gun's firing frames
  by its muzzle anchor (drawn now against frame 0), and under your own super shotgun's barrels as they drop open
  (`ssgBarrelsCarry`).
- **Physics and blows.** With a bayonet the gun strikes as a blade (`VR_Melee_Thing`: the sword's kind; `VR_Melee_Line`: the
  line from the hand to the bayonet's tip, the blade from where its blade starts), sticks by its point
  (`VR_Attach_Embed` wherever weapons.txt `embed` was read; the engine's `vr_axestick.cpp attachBlade`: the point's tip is
  the bayonet's) and fires while stuck (`VR_Attach_EmbedFire`: embedfire's prop path). Flat play: V thrusts with it
  (`thrustsWith`). The gun's own Box3D body does not include the bayonet yet.
- **Putting it on / taking it off.** VR: hold the bayonet, bring its socket (its model origin) within
  `vr_attach_snap_radius` (5) of the gun's mount, pointing within `vr_attach_snap_angle` (50 deg) of the gun's way: it
  snaps on (`VR_Attach_Frame`). The empty hand gripping within `vr_attach_pull_radius` (6) of the mount and pressing its
  B/Y pulls it off into that hand, as the head torch comes off (`vr_attach_pull_button` 1; 0: the grip alone; a grip there
  alone is the gun's two-handed grip). Stuck by it: `vr_embed_bayonet_depth` (1.25x a sword's depth share of its blade);
  fired stuck, `vr_embed_fire_push`, `vr_embed_fire_letgo` and `vr_embed_fire_hold_time` keep the body on the hand
  (ROUND26 attach1, bayonet2). Flat play: the Attach key (`vr_flat_key_attach`, J; impulse 52, any mode) puts the bayonet the
  other hand holds on, or takes it off into the empty other hand. Server-side points: `attachinfo(model, kind, what)`
  (the mount and the tip from the muzzle in the gun's axes) along the hand's axes from its muzzle.

- **Daggers as Bayonets** (daggerattach, `vr_attach_daggers` 1 for testing): kind 2 is the player's dagger
  (`progs/v_dagger.mdl`, weapons.txt `att2_*`): a held dagger's tip brought to where the mounted one's would be, pointing
  its way, or J; its record rides on the gun's (`.wi_attach_inst`) and comes back with it. A weapon model as an
  attachment draws with its own Scale (`Mount::own`), so its mount scale is 1 / the gun's Scale like the bayonet's.
  Mounts: v_shot 26.56 0 -4.6, v_shot2 20.19 0 -3.22, both pitch -76.46 roll 180 (its blade runs up its model's +z).

### Ideas (his and ours)

His: scopes (a zoomed view through the lens: a second view or a render-to-texture on the lens), an under-barrel grenade
launcher (the gun's alt trigger or the off hand's trigger on its grip fires the grenade launcher's grenade; its own
ammunition), an under-barrel flamethrower (the AD flamethrower's stream from under the barrel), an igniter under the
nailgun and super nailgun (every nail a lava nail: the Dawn of the Machine / Scourge lava nails), lenses for the lightning
gun that split the beam (two or three weaker beams fanned out) or focus it (a longer, narrower, harder-hitting beam).
Ours: a laser sight (a dot where the shots go; a flat-play aim aid), a flashlight mount (the existing flashlight clipped
as an attachment kind, one path for both), a suppressor (quieter shots: the stealth AI hears less), a foregrip (a 2H hotspot
the attachment adds: steadier aim), an extended magazine / shell holder on the stock (more rounds, slower to load), a
choke for the shotguns (tighter spread), a bayonet for the nailguns and the Burst Rifle, a spike or a hatchet on the
grenade launcher, a grappling hook under the barrel, attachments as loot (found in secret areas, dropped by armed grunts
whose guns carry them, the Survival Arena's supplies), and armed grunts with bayonets (a grunt's bayonet charge).

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

bayomounts (2026-11-28) added, measured on the models as the engine draws them (`vr_attach_mdl_dump <model> <file>`: frame 0
as an OBJ, AD's made `#vr` guns too; the underside on the barrel's line at the front, the socket's length clear behind;
where a front lip is shorter than the socket, under the body's front):

```
  progs/v_nail.mdl        20.0 -1.48 2.14   0 0 0  2.0833   (lava nailgun: slot 11 inherits)
  progs/v_rock.mdl        29.2 0 0.55       0 0 0  2.6316   (grenade launcher; multi grenade: slot 13 inherits)
  progs/v_prox.mdl        29.2 0 0.58       0 0 0  2.6316
  progs/v_rock2.mdl       50.6 0 -1.95      0 0 0  2.9412   (rocket launcher; multi rocket: slot 14 inherits)
  progs/v_light.mdl       33.4 0 7.55       0 0 0  2.8571   (plasma-look lightning gun: slot 15 inherits)
  progs/v_plasma.mdl#vr   30.3 0 2.62       0 0 0  2.8571   (AD's Plasma Gun)
  progs/v_gruntgun.mdl    32.85 0 6.74      0 0 0  2.5641   (burst rifle; its #adv variants, slots 27-31, the same)
  progs/v_enfrifle.mdl    40.6 0 4.28       0 0 0  2.5
  progs/v_revolver.mdl    20.26 0 5.56      0 0 0  2.2222
  pickups (vr_attach.cpp pickupRails): g_nail 13.33 -3 34.24 2.1315, g_rock 17.3 0 31.21 2.107, g_prox 17.4 0 27.97 2.1107,
  g_rock2 21.9 0 28.39 2.249, g_light 20.3 0 34.63 3.0316
```

Their dagger mounts (att2): the bayonet's moved by (-2.79, 0.339, -1.47) bayonet units (the shotguns' two tuned ones'
mean), level. The bloody shotguns (MG3) are drawn as the shotguns: the same slots, the same mounts.

### The bayonet's mount (bayomounts)

`quakevr/progs/bayonet_mount.mdl` (make_bayonet.py `build_mount`, the bayonet's skin): a short iron rail along under the
barrel behind the socket and a ring round the socket's axis where the socket's back sits, in the bayonet's own space, so
it is drawn at a gun's bayonet mount (`attach::railMount`: the slot's `mnt_x/y/z`, `mnt_pitch/yaw/roll` in the gun
model's space, `mnt_size` the bayonet's size on it; Weapon Offsets > Attachments, "Bayonet Mount"; a map pickup maps its
view gun's mount onto itself, `pickupRails`). Drawn on every gun a bayonet fits by the one placement (`view::placeRail`, placeAttachment's): your hands'
(`entities.rail`), your holsters' (`holsterRail`), another player's (GunParts::Rail), and every other gun in the world
(`setupWorldRails`: lying, a monster's, the map pickups: the nearest 32 within `vr_attach_mount_dist` 1500).
`vr_attach_mount_show`: 1 always (default; the socket covers the rail when a bayonet is on, the ring sits just behind
it), 2 hidden while an attachment is on, 0 never. `vr_attach_report` lists them (`where=rail_*`).

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
