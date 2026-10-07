# Immersive manual reloading: the plan

The author's request (2026-10-07): a third reloading mode, "manual-immersive", next to today's holster reload
("manual-simple") and "off": a pouch on the front of the belt hands out physical ammunition (shells, magazines,
grenades, rockets) for the gun held in the other hand, and that ammunition goes into the gun by hand. Built in four
phases, the author testing after each.

## What there is today (read before designing)

- **The setting.** `vr_reload_mode` (Quake/vr/vr_cvars.inc): 0 none, 1 reload at any holster, 2 at the hip holsters
  (default). Read by server QC (`cvarh_vr_reload_mode`: `VRIsWeaponReloadingEnabled`, QC/vr_util.qc;
  `isReloadHolsterHotspot`, QC/weapons.qc) and by the client for the ammo screens (`clip/size` over the reserve:
  vr_view.cpp `queueWeaponText`, `idleWeaponText`). Reloading also needs the immersive weapon mode
  (`vr_holster_mode 0`) and a human player (`.ishuman`: bots never reload). Also on the setup boards
  (vr_setup.cpp `"reload"`) and the old hub (vrstart.ent: raw-cvar buttons 0 and 1, left alone).
- **Magazines.** Every weapon is a record (QC/vr_weaponinst.qc: `weapon_inst`, `.wi_clip`) that the hand, the holster
  or the prop it lies as refers to: its magazine and its blood go with it through holstering, throwing, hand-offs and
  level changes. Map pickups come loaded (`weapon_touch`, items.qc: the base clip from the reserve), drops full.
  `VRReloadWeapon` (weapons.qc) fills the magazine from the reserve; it runs from the reload button (`+reloadmain`),
  a gun held at a reload holster, and the super shotgun's flick. Clip sizes: `WeaponIdToBaseClipSize`
  (vr_weaponutil.qc): shotgun 8, super shotgun 2, nailgun 24, super nailgun 36, grenade/proximity/rocket 4,
  thunderbolt 36.
- **The back pouch** (round 21, "Hand grenades from the back pouch"): the client finds the hand at it
  (`body::hotspot`, `HS_GRENADE_POUCH` 11, sent with each move), draws `vrpouch.mdl` (make_pouch.py; frame by
  `STAT_ROCKETS`); the server does everything else (QC/vr_grenade.qc `VR_HandGrenade_HandFrame`, `_Take`, `_LetGo`,
  `_LevelEnd`): an empty gripping hand there takes a grenade (a rocket off the reserve), carried by the carry system
  (QC/vr_carry.qc `VR_Carry_Start`, held per its prop slot's grip: vr_props.inc / vr_grip.cpp), let go of there unarmed
  it goes back (the rocket refunded, up to the most carried), at a level's end an unarmed one in a hand is refunded.
- **Carried props.** Any `VR_Carry_Setup` entity with `FL_FORCEGRABBABLE` can be taken by hand (`VR_Carry_Handtouch`),
  force grabbed (`VR_Forcegrab_IsEligible`), thrown (`VR_Carry_Release`: the hand's throw), and is a Box3D body while
  loose (`.vr_rigid`, `MOVETYPE_TOSS`/`BOUNCE`). Its grip and finger pose come from its model's prop slot (64 slots,
  `vr_prop_<key>_NN`), edited live in Held Object Offsets (the menu claims a slot for whatever the hand holds, and
  "Print Changes" writes `QVR_PROP_DEFAULT` lines).
- **Spent shells** (vr_shells.cpp, `vr_shell.mdl` from make_shell.py) are client-only effects: no edict, not
  grabbable. Their model and generator are the base for the live shell.
- **Gun points.** The muzzle is a model anchor (`muzzle_av` + offset, vr_weapons.inc), placed by the client as drawn
  (vr_view.cpp) and sent with each move (`VrMove::muzzlePos`, vr_move.cpp) to the player's `.muzzlepos`/`.offmuzzlepos`.

## Architecture

Server-side QC owns every rule; the engine supplies what only the client knows (where the hands, the pouch and the
gun's port are, as drawn) and draws.

- **Client (engine).**
  - The front pouch's place (vr_body.cpp `ammoPouchPosition`, like `pouchPosition`: on the front of the belt between
    the hip holsters, carried by the pelvis, `vr_ammo_pouch_x/y/z`) and the hotspot `HS_AMMO_POUCH` (12) when a hand
    is within `vr_ammo_pouch_thresh` (the hotspot competition as the holsters': the one the hand is most within).
    Every place that lists hotspots learns it (vr_climb.cpp, vr_flashlight.cpp, vr_selfcollide.cpp; QC
    `VRLetGoDropsAt`, the unholster ignore list, `VR_Carry_AtHolster`).
  - Its drawing (vr_view.cpp `setupAmmoPouch`: `progs/vrpouch_ammo.mdl`, turned by `vr_ammo_pouch_pitch/yaw/roll`,
    scaled by `vr_ammo_pouch_scale`, lit up while a hand is at it; frame 0 with ammo for the other hand's gun, 1 empty).
  - Each gun's **loading port**: a new anchor per weapon slot (`lport_av` + `lport_x/y/z`, vr_weapons.inc; -1: none),
    placed as drawn like the muzzle, sent with each move (`VrMove::loadPort`) to `.loadportpos`/`.offloadportpos`
    (vr_server.cpp). The hand without a port sends its hand position (QC never reads it then).
- **Server (QC, new QC/vr_reload.qc).**
  - `VR_Reload_Mode()`, `VR_Reload_Immersive()`, `VR_Reload_ManualFor(weapon)`: which weapons load by hand (phase 1:
    the shotgun). Holster, button and flick reloads skip those weapons in immersive mode; every other weapon keeps
    the hip-holster reload (mode 2's rule).
  - The pouch: per hand per frame (`VR_Reload_HandFrame`, from W_Frame beside the grenade pouch's): the arrival tap,
    the take on a grip press with an empty hand (what it gives: the ammo of the gun in the *other* hand), the empty
    knock when the reserve has none, nothing at all when the other hand holds no hand-loaded gun.
  - The ammo props (`vr_ammo_shell`, later `vr_ammo_mag`, ...): carried things (`VR_Carry_Setup`, force grabbable,
    Box3D bodies), each with its ammo id (`.vr_ammo_aid`) and count (`.vr_ammo_count`). While one is held, each frame
    it is checked against the other hand's port (`VR_Reload_HeldFrame`, from `VR_Carry_HandFrame`); let go of at the
    pouch it is refunded (`VR_Reload_LetGo`, before the carry's own let-go, as the grenade's).
- **Multiplayer and co-op.** Everything above runs per player on the server: the pouch, the props and the counts are
  that player's; other players see the props as entities. The client sends only its hotspot and port points (as it
  does the muzzle). A loose ammo prop is anyone's: picked up, it goes into whoever's gun or pouch it is put in (it is
  ammo). The client draws the pouch by its own `vr_reload_mode` (as it decides today
  whether to show `clip/size`); a client whose setting differs from the server's sees a pouch that does nothing, or
  none: noted, a stat bit can fix it later if it matters. Bots (`!ishuman`) never reload, as today.

## Ammo accounting

- Taking ammo from the pouch removes it from the reserve at once (a shell: 1; a taped pair: 2; a magazine: its
  count, at most what the reserve has). The HUD and ammo screens update (`W_SetCurrentAmmo`).
- Loading it moves its count into the gun's magazine (its record, so it stays with the gun). A pair with one place
  left loads one and leaves a single shell in the hand. A full gun takes nothing (a dull tap).
- Let go of at the pouch: refunded (its count back to the reserve, up to the most carried: `VR_MaxAmmo`; what doesn't
  fit stays in the hand's prop and drops).
- Dropped or thrown: it lies about (a physics prop), can be taken again by hand or force grab, loaded, or put back.
  It is wasted only when it is gone: removed by the loose-ammo cap (`vr_reload_loose_max`, the oldest not held goes)
  or its time (`vr_reload_loose_time`, fading as small gibs do), or left behind at a level change.
- In a hand at a level's end: refunded (as an unarmed hand grenade), so nothing is lost by walking through a
  slipgate with a shell in the hand.
- A partly used magazine (phase 2) keeps its count; put back in the pouch it refunds that count.
- The mode switch never creates or destroys ammo: magazines keep what they hold whatever the mode.

## Model work per weapon

All generated by the repo's Python generators (mdlgen / mdlpolish, Quake palette indices, genguard), normal maps by
`bake_normals.py`; anchors checked by polish_weapons.py's strip order.

- **Front pouch** `vrpouch_ammo.mdl` (new `make_ammo_pouch.py`, after make_pouch.py; by ammo and count since the author's notes: shells, magazines, cells): a wide belt pouch with an open
  top, shells' brass heads standing in a row in frame 0, flat and empty in frame 1. Later phases add frames (a
  magazine's lip, a grenade's head, a rocket's nose) chosen by the other hand's gun.
- **Shotgun** `v_shot.mdl`: a loading port under the receiver, in front of the trigger guard and behind the pump: a
  dark recessed opening framed by a steel gate (mdlpolish parts appended, the old vertices untouched), with its own
  anchor vertex (`lport_av`).
- **Shells** `vr_shell_live.mdl` (an unfired shell: closed star crimp) and `vr_shell_pair.mdl` (two side by side,
  taped round the middle), new outputs of make_shell.py.
- Phase 2: nailgun (a drum magazine under it, from the rotating pickup's look), super nailgun (a box magazine on its
  outer side, raised 35-40 degrees), thunderbolt (a cell under it); magazine props for each.
- Phase 3: a loading port under the grenade and proximity launchers.
- Phase 4: the rocket launcher's breech at its back end; a rocket prop.

## Menu

A new page, **Reloading** (Weapons hub, beside Immersion Settings' reload row, which stays and gains "Immersive"):

- Mode (Off / Holsters (All) / Holsters (Hips) / Immersive), Ammo Screen hint.
- Phase 1: Shell Pairs (taped pairs from the pouch), Port Leniency (units), Pouch: Show, X, Y, Z, Reach, Pitch, Yaw,
  Roll, Size; Haptics Strength, Sound Volume; Loose Ammo: Most, Time; "Shell Pose: Held Object Offsets" (open the
  props page with a shell in hand).
- Phase 2: Pull Force, Pull Wrist Snap, Bump Force, Eject Button.
- Debug > Tests: Reloading (give shells, take a shell into each hand, load the held shell, drop it, put it back,
  print the counts, the self-test).

## Test plan

Headless (mock hands, `vr_fixed_frames 1`), per phase:

- QC self-test `vr_reload_test N` steps printing `reload: PASS|FAIL <what>` and the counts: take a shell (reserve -1,
  a `vr_ammo_shell` in the hand), load it (magazine +1, prop gone), load until full (the ninth refused, reserve
  unchanged), put back (reserve +1), drop (reserve unchanged, prop on the floor), force grab it back, a pair (2/1),
  empty reserve (nothing given), level end refund.
- Mock-driven end to end: the hand moved to the pouch (`vr_mock_hand_to main ammopouch`), grip, moved to the other
  hand's port (`vr_mock_hand_to main lport`), the magazine +1; to a lying shell (`nearest vr_ammo_shell`), grip.
- The mode switch: modes 0/1/2 behave as before (the holster reload of the shotgun works in 1 and 2, the pouch gives
  nothing; mode 3 refuses the shotgun's holster reload and the button, and reloads the nailgun at a hip holster).
- Regressions: `eval.sh` (weapons and hands touched), e1m1 smoke, `vr_test_weaponinst` steps, the loaded map guns
  scripts (`Misc/quakevr/loadedmapguns/*.ps1`), QC 0 warnings, `vr_menu_path_check maps/vrcalibration.map`.
- Pictures: the pouch on the body and the shotgun's port close up, before and after, one composed image.

## Phases

1. **Mode, pouch, shotgun** (done 2026-10-07: ROUND21.md, "Immersive manual reloading, phase 1"): the mode (default Immersive, `vr_cfg_version` migration of the untouched
   default), the front pouch (drawn, placed, sized, turned), the shotgun's port, the shell props (single, taped pair),
   the pouch take / put back / waste, force grab of loose shells, haptics and sound, the shell's pose (its prop slot),
   loading with a leniency, the Reloading page and the Debug test aids. Other guns as before (the hip holster reload).
2. **Magazines** (done 2026-10-07: ROUND21.md, "Immersive manual reloading, phase 2"): nailgun, super nailgun,
   thunderbolt: magazine models and props, a magazine in the gun (drawn while it is in), the three ejects: B/Y on the gun's controller; the other hand pulling it out with force and a wrist snap
   (thresholds on the page); a bump with a fresh magazine (the "Iraqi reload": the old one knocked out, the new one in).
   A partly used magazine keeps its count.
3. **Grenade and proximity launchers**: a port under each; grenades from the front pouch (a launcher's grenade or a
   proximity one by the B/Y as the back pouch's multi-grenade; armed and dropped as a back pouch grenade is); the back
   pouch stays.
4. **Rockets**: a rocket prop loaded at the launcher's back end, one at a time; thrown rockets don't light, but a shot
   (hitscan, a nail, a blast) sets them off, in the air or lying.
