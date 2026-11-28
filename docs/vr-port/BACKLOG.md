# Backlog: agreed work for the next rounds

Work the author approved or asked about, not yet done, one or two lines each, with the doc that holds the detail.
Remove an item when it ships (git history keeps it); keep new ones as short. Last cleanup 2026-11-27 (ROUND26.md "blclean").
Staying here by his decision (2026-11-05): pickup thinks, heart gib and berserk pack, TAA/DLSS.
Checked, by design (2026-11-19, tester "Joyeuse"'s two hand-off reports, closed): the old hand "keeps aiming"
after a hand-to-hand swap is the designed two-handed hand-off (vr_2h_handoff 1: the gun hangs from its foregrip until
the free hand grips the handle), kept as is by his decision, no hints; a toss to the other hand isn't reproduced (the
mock toss keeps the hand's speed; a slow toss may hit the open palm after the 0.2 s vr_box3d_throw_grace).

## 1. Open bugs and failing tests

Open on 2026-11-27 (notes in ROUND26.md; the named ROUND25.md sections for older ones):
- **Lance pull-out flake**: the test's chance checks moved to flatthrust 59 (ROUND26 "lancepull"); one real loss in 10 flat runs (2026-11-26: the hand empty after a shaft hit on a touching ogre, cause unknown). Lean aim is not wired for Copper, rogue eel/lavaman or AD's own checkattack. Was: `flatlance_test` section 5 fails ~1 run in 6 ("Lance gone after a pull-out"; the H case found
  no Lance stuck). ROUND25.md "lance4", "flatembed".
- **VR embed test 24 and test 33 case 46 still fail** (ROUND25.md "embedregress"): 24's mock axe swing is aimed by the old
  pitch 0 (the axe ships 15.5); 33 case 46: a dead grunt held by the handle doesn't come to the hand (the ragdoll
  handle-hold motor).
- **Spears**: a thrown spear sticks in a grunt ~1 in 3 (skids at ~44 deg); armed grunts' spear pose is the sword pose;
  no spear .blend. ROUND25.md "spear", "spearblunt".
- **Lance against a wall**: it tilts up and the off hand sits ~10 u off the pole, then lets go. ROUND25.md "lanceoffhand".
- **Rope slide**: one elbow snap ~10 frames in (elbow IK). ROUND25.md "flatfb1124b".
- **Dog wedged against a wall**: it can stay standing wedged with the wiggle off. ROUND25.md "wallglue".
- **Flat: no low or sideways swing, so no flat leg sweeps** (his call 2026-11-24: not now).
- **Splitscreen**: agent/split (6e6f2af6e, split19, SPLITSCREEN.md) pushed, not merged; waits for his try; check `vr_dm_test 3`'s failure against next if it still shows.
- **Quest**: cold start compiles 100 GL programs, 29-35 s (2026-11-26); an alias draw-list cache across frames would rarely hit (questperf2: the instance data changes every frame with the flickering dynamic lights and the wounds' timers); LRZ in the opaque entity pass (questperf2): alive with no alias draws and after your first model, dead after your leg holsters (batches 2-3), not from one state alone; `vr_debug_alias_lrz 62` plus `vr_gl_alias_lazy_blend 1` together keep it (1.16M pixels killed): next, bisect that combination (ROUND26 "questperf2"); CPU-bound big fights
  (alias setup ~4 ms); Quest 3S untested by us; the Intel HD 630 link fix unconfirmed on hardware (ROUND25.md "intelhd", beta4).
  hip1m1 (questopt, 2026-11-28, his perf recording): CPU-bound at 24 ms a frame in its fights (GPU 11.4): QuakeC 7.5 ms
  (worn packs' anchors fixed; the rest unprofiled on the headset: `vr_qcprofile` + `profile_qc` in a hip1m1 fight next),
  the flashlight's lightmap light 4-5 ms (now ~1.6), models 2 ms, sound spikes 2.5 ms; the unworn headset still pauses
  ("xr user absent") despite `-unattended`.
- **Baseline failures, 2026-11-26 list, what is left** (agrunt 203 fixed: ROUND26 "basefix2"; `ragdoll_grab` and teleporters: "basefix1"/"basefix2"): `rigstudio_posekeys` case 5 (right elbow) fails in every baseline since 11-14 (0/5 on 11-27, same check); agrunt 16 (0/5: the drawn grip 2.73-2.76 off, limit 2.5) and agrunt 65 (1/5: "grab anywhere: an axe swung at its chest at 4.5 m/s: its parry raised 0.12, parried 0, disarmed 1") as in basefix2. Agrunt 74 and the recoil test passed 5/5 on 11-27 (ROUND26 "flakysweep").
- **Spear/Lance numbers differ between trees** (quest vs mp, same commit): the relit junction was fixed 2026-11-26 (every tree's `quakevr/relit` points to `qvr-kit/relit`); no recheck of quest's spear/flatthrust numbers is recorded since: needs one.
- **scenario_check** fails in trees without Quoth kellbase / Dwell data (test bases, not code).

- **Flaky/stale lists below, from 11-12 to 11-21 (rechecked 2026-11-27 by the flaky sweep, 5 unseeded runs each: ROUND26 "flakysweep" has the table; the lines below are updated)**.
- **AD-only tests** (ran 2026-11-13 with `-game ad`, ROUND25.md "adtests"): `vr_flatswap_test 4` 48/0 on `map start`
  (one earlier run of two failed with faulty-frame counts on the blades' stance cases while it timed out under the
  default 240 s: a one-off, not reproduced; watch it), visor_test 37/0 with AD's two custom keys counted, `vr_agrunt_test
  213` fixed (the test gave no weapon) and 1/0, `vr_ad_test` 73-80, 184 print their gun (no verdicts), 87/88 PASS,
  `vr_ad_mtest 3` a diagnostic (no verdict: the death knight and zombie knight gib at the kill, the crossbow knight leaves
  a corpse). The baseline runner has `flatswap_4_ad` and 213 under AD; the others are described in the round notes.
- **Random run to run** (re-run 3 times each on 2026-11-12, ROUND25.md "Test triage B"; flatpose and flatswing seeded
  and read earlier, ROUND25.md "testfix": flatpose's last version unrun): `vr_systems_test 15` (2/5 on 11-27: the
  blast's body never knocks the torch off; the body's flight varies: UNKNOWN), ragdoll_ragtest (4/5 on 11-27: "catch: NOT begun in 3.0 s"), ragdoll_guard
  (4/5: "guard stagger: guards its head (staggering)"), **reload is now a real failure, 0/5 on 11-27** ("a prop held in the other hand hits the
  barrels open and shut by its surface (prop 10, want 10; the fist as far off: 1, want 0)" in all 5; also "the lying shell taken again by the fist" and
  "let go of at the pouch: refunded" in 4, the SSG sights' colour in 2; the old "a hit: the old one knocked out" check did not show).
- **ragdoll_stagger: what is left** (the lip fixed, ROUND25.md "realfix_stagger"): the grunt's beaten backpedal falls
  at 0.43 s after 2 steps instead of giving way after 3 (F, not G; on level ground, every trial); in one combined
  batched run (lip rates steady backpedal) the 8th steady grunt (balance 0.52) and monster 2's 8th fell, and the lip
  case got 5/8, while lip alone gets 8/8 (batched or not): something between a batch's runs.
- **ragdoll_climb** (24 of 24 at seed 7 since agent/climbleft, 2026-11-13; the grunt's move-in and the hell knight's
  second hand fixed): pull at seed 8 is 4 of 6: the ogre "lost its hold" at 1.25 s (before too), the hell knight
  "slid back off the lip" in the batched run (stands on the top; alone at seed 8 it gets in). The hell knight's chest
  stays under the lip for about 2 s going over (both hands holding) and it gets in late. ROUND25.md, "Ragdoll climb
  leftovers: the grunt's move-in, the hell knight's second hand".
  (agrunt 60, agarmor_5, limb_wear, stealth settled 2026-11-12; autopump, cellcord, animsmooth, xr_runtime and
  secret_hits too: ROUND25.md, "Always-failing monster tests" and "Always-failing tests, part b".)
- Stale list, rechecked 2026-11-27 (5 unseeded runs each): steady 5/5 (off `test_flaky.txt`): agrunt 47/63/72/76/86, ragdoll_grab, rv_1, nail_1,
  judo, parry_pose, weapon_catch, dead_gear, stealth (all scenes, seeded 1: its scenes seed themselves). Still flaky (stay on the list):
  agrunt 66 4/5 ("lying (as shipped): walked up and reached for its weapon: it holds nothing, 1 dropped"), agrunt 67 4/5 ("grabtable normal
  time ...: 101/128 taken (90% or more)"), agrunt 209 4/5 ("grabtable bullet time ...: 137/153 taken"), agrunt 71 3/5 ("balance grunt: damage a
  second within 25%"), ragdoll_guard 4/5. **Real failure: `teleporters_chase` 0/5** ("chase dog at framed_sill32: never reached the north
  room ... (try 3 of 3, seed 3)", the same text every run) and agrunt 60 0/5 (agparry: "weapon: parried 1, 0 damage (kept 5 of 20), still armed 1").
- **Embedded blades** (`vr_embed` on since 2026-11-08): real heavy monsters, saves, level change and multiplayer
  unproven (tests 14-20 written); tests 24 and 33 as above.
- **Embedded blades, the handle hold** (2026-11-12, ROUND25.md "embedgrip"): his VR check of the handle staying in the
  hand; a stab's own gap along the blade now moves the body to the hand (keep it as slack if that reads wrong); the
  off hand's pose on a handle isn't probed; `vr_embed_test` 7 had 2 more failures in one run with the hold on.
- **`vrtutorial_playtest.py` room 8**: the shells' throw is fixed (ROUND25.md "testfix"); still open: the mock's main
  hand never grips the bench's shotgun (stops ~25 units short), so nothing loads; the `weapons` gate and later fail.
- **Tooling:** Tracy GPU zones lost after toggling GPU detail mid-run; some hidden-mock eye/window captures come back
  black; 15 test commands still registered in `vr_main.cpp`; `vr_physsound.cpp` keeps its own copy of the box test
  `R_SurfaceHoldsPoint` replaced for the light.
- **Baseline 2026-11-12, still failing alone** (ROUND25.md "basefix1112": first-bad merges by bisect, alone, seed 1):
  agrunt 16's hunt check (the drawn grip 2.7-2.8 units off the drawn hand hull, limit 2.5; its vertex checks fixed,
  ROUND25.md "agfails2"; 0/5 on 11-27, 2.73-2.76), stealth props_round (5/5 on 11-27 in the full stealth run: steady). Older (11-27, 5 runs): agrunt 3
  0/5 ("yank: your main hand Fist, the grunt holds Shotgun": the hand does not take the gun), agrunt 91 3/5 ("raggrab: 8/9 won" / "9/10 won"), agrunt 92
  5/5; 213 fixed 2026-11-13 (adtests).
- **vrtrailer: the test's swing, not the game** (ROUND25.md "testenv", 2026-11-21): it has failed since 11-10 (passed
  only in the 11-07 lanes; release12's "3 of 3 on 2026-11-12" was weapon_catch's, not this test's). Its mock (10-08)
  was written for vr_gunangle 39.5 and an estimated Super Axe line; the defaults went to 70 on 10-16 and the axe's blow
  line moved to the blade's edge on 11-10 (a1da23a80), with the head-zone path rule. The mock now carries its settings
  and the measured line; still open: a swing path whose blade crosses the neck sideways (now 8 of 9: killed, "a stab
  (0.82)" at 0.63 along the blade).
- **Baseline 2026-11-13** (ROUND25.md "basefix1113"): agrunt 12/47 and teleporters_chase fixed (the tests); arena 11
  failed under load only (25/0 alone, 2 of 2; 4/5 on 11-27: "its humanoids hold its 5 tier-2 and tier-3 guns (4 hold one)"); ragdoll_ragtest's catch as above (4/5); agrunt 12 and 47 5/5.
- **r2m1 (Rogue): a counted monster that is no monster edict is inherited, not ours** (r2m1 branch, 2026-11-13): stock
  QuakeSpasm (-rogue, skill 1, map r2m1, save) and Quake VR give the same save: total_monsters 93, 92 FL_MONSTER edicts,
  95 monster_* edicts (the extra ones: a monster_morph spawner and a knight-class entity that counts itself without
  FL_MONSTER). The 95/94 on another skill is the same one-off. vr_run_enemies_check failing r2m1 on it needs an
  exemption, not a spawn-code fix.
- **mp_coopbots, rare flake (2026-11-20, mpfinal)**: vr_dm_test 11's "the bot kills the grunt" gave up once in ten
  takes (the grunt at 19 health at 864 frames; the fight is the bot's aim over 12 s of real time); the base before the
  review passed its ten. Known; don't chase unless it repeats.
- **Known failing tests, flakehunt's leftovers (2026-11-21, ROUND25.md "flakehunt")**: fixed there: flateject `rv`
  (stale pose), `vr_pickup_test 15` (the bots' reaction delay), hotblade's crowbar/chainsaw vertex totals (mdltopo).
  Still open: hotblade `holster` (the holstered axe found lying: the test's holster move), `blade_parts`/`superaxe`
  (the hammer's measured length 5.9 -> 7.0 and its glowing verts 295 -> 172 of 795 with no model change: wants his
  look before the numbers move); flateject `ssg` (second casing up 0.29 against > 0.3) and `vr` (one stroke line) run
  to run; nav_lift_down (its verdict prints since noisefix, the scene's cmd lines ran behind the script's waits: it
  fails, the knight boards, loses "on the lift", gives the ride up; also with vr_hull_holes 0: ROUND25 "noisefix");
  `vr_dm_test 19` (bot navmesh chase: 2 of 4 runs fail, the bot back on its waypoints once it sees him at the door);
  mp_debugviz_listen's bot case (rare: the bot's AI swinging while held, the server's line and the drawn one a frame
  apart; ROUND25.md "mpopen"). (mpopen fixed: the suite's order effect and fixed 27500 base port, mp_loss's margin.)

## 2. 1.2 release prep (only on his word; the 1.1 list was RELEASE_TODO.md)

- Review the installer's settings page (`Misc/installer/settings_page.txt`): rows, order, labels, help; whether "VR or
  flat screen" belongs there (2026-11-06).
- A VR RenderDoc capture to confirm the perf work on real hardware.
- The lower-end presets (`vr_graphics_preset`) checked on real hardware (round 20).
- `vr_flat_chest_down`: default 0.3, his 0 (plates hidden at 0.3 on the flat screen); decide at release prep.

## 3. Flat-screen (FLATPLAY.md)

- **Procedural reloads, on by default** (2026-11-05): e.g. the shotgun rolls toward the off hand, which shuttles between
  pouch and port until full or interrupted; recorded motions where they fit.
- **Recorded motions for flat melee and reloads** (2026-11-05): phase 1 recorder shipped (ROUND24.md, "Animation
  recorder, phase 1"); phase 2 after he records.
- **Hotkeys for the remaining VR interactions** (2026-11-05): audit what is left (parry, ammo switch, throw, shove,
  climb...) and propose a key or leave it VR-only.
- **Left-handed mode** (2026-11-05): the mouse buttons' routing by hand (`flatplay::routeFire`) mirrored.
- **Flat-screen two-handed melee pose adjustment** (2026-11-15): he ticked the checklist item for now; the 2H melee pose in
  flat play needs adjusting later; low priority.
- **The window's graphics** (round 20): float scene, tone mapping, grades and bloom on the window; check each feature
  with VR off.
- **Flat-screen trailer: "taking the headset off"** (2026-11-19, his idea; once flat play is fully polished). It opens
  in VR; the player brings both hands to their face as if lifting the headset off; the view changes to an in-game room
  with a desk, a chair and a computer; Ranger walks to the desk, sits, and starts playing the flat-screen version.
  Easiest route (three in-engine shots, joined in Resolve; reuses vrtrailer's generated-map + scene-QC pattern,
  MAPPING.md "vrtrailer", and TRAILER.md's takes):
  1. **VR shot**: recorded live in the headset as a take; he lifts his hands to his face, the hands fill the view, cut
     on black. Optional in-engine help: a "hands at the face" fade to black (both hands within ~15 cm of the head
     for 0.3 s) so the cut lands on its own.
  2. **Desk room**: a small generated map (`vrdesk_gen.py`, from vrstart/vrtrailer parts: room, desk, chair, monitor,
     a lamp). The monitor's face is a fullbright pure-green texture, replaced in Resolve by the flat gameplay footage
     (planar tracker for a moving camera): no render-to-texture work in the engine.
  3. **Ranger** as a scripted actor (the remote-player/bot body we already draw for MP bots; the scene QC walks it along
     path_corners), filmed by a scripted camera (svc_setview to a camera entity with a dolly, as vrtrailer's scene).
     Sitting: Quake's player.mdl has no sit; easiest is framing from behind with the chair back hiding the legs and the
     body lowered, or one baked sit pose with the pose tools if a side view is wanted.
  4. **Into the game**: the camera pushes into the monitor and cross-fades to a full-screen flat take (recorded with
     the takes tool), so the last shot is real flat play.
  New work, small: the desk map generator, the scene QC (actor walk + camera), optionally the hands-to-face fade and a
  sit pose. Everything else exists.
- **Splitscreen: per-seat controller bindings in the rebinding screen** (2026-11-28, todo): the Controller Bindings
  screen edits the one bind table; with true seats (agent/split) each seat's controller gets its own page and binds.

## 4. Monsters' movement: the navmesh (NAVMESH.md)

- **Teleporters** (NAVMESH.md 16): walkers path through teleporters (one-way far links, shut ones closed, paired gates
  walked through when the box fits; NAVMESH.md 17: lining up in front of a wall teleporter, ranged monsters walking
  through a gate between shots, the plan drawn through, the `vr_nav_teletest` metric; NAVMESH.md 18: the stealth
  trail goes through a teleporter he went into, `vr_stealth_chase_tele`). Left: a scene on a map whose teleporter is
  gated by a button; AD's own `trigger_teleport` states (STARTOFF, estate) beyond the targetname rule; a scene where
  the trail is what carries the hunter through (`nav_tele`'s dog sees him go in: it passes with the trail off too);
  the debug view didn't show in a headless screenshot (not even the mesh).
- **Lifts** (NAVMESH.md 10, 16): on by default; the reach scene (`nav_lift`) and the waiting-at-the-top case
  (`nav_lift_down`: the lift is called up). Left: a scene for a train (every id train with stops is triggered first:
  e2m6's elevator, e3m4's).
- **Brushes beyond the world** (ROUND26 vsnav): the navmesh now cuts any brush reaching past the world's box (old
  vrstart's mesh lost the whole island to one). The cause is fixed at the source (ROUND26 clipstray: `recoverClips`
  boxes a sliver piece that lost its faces, and cuts a clip brush to its bounds where its moved faces reach past them;
  `vr_hull_strays` says 0 and 0 on 21 maps), so the navmesh's cut is now only a safety net. Left: `vr_nav_toptest`'s
  input check takes a cut brush's box top as its top (none on shipped maps).
- **Flyers and swimmers**: a 3D grid of open space for scrags and fish (NAVMESH.md 7).
- **Flanking and cover**: goal slots round the player, extra cost on polygons other monsters' paths use, cover use
  (NAVMESH.md 7, 11).
- **The tower dog fall**: `vr_arena_test 41`'s last capped trial (1 of 60): a dog leaves the south-west tower's top
  within 1.3 s and dies of the 256-unit fall; how it goes over the edge is not found (NAVMESH.md 15).

## 5. Survival Arena (ARENA_DIRECTOR.md)

- **Arena geometry that changes between waves** (his request, round 24): the layout altered between waves, the navmesh
  following it (movers are already flagged blocked: NAVMESH.md 5).

## 6. Armed humanoids (the key feature; ARMED_MONSTERS.md)

- **The enforcer's real backpack** (2026-11-02): today a lift zone baked in vrenforcer.mdl (ARMED_MONSTERS.md, "Open
  items moved from BACKLOG.md").
- **AD's humanoids armed, then their worn packs** (2026-11-02; same section).
- **Crouching and crawling** (2026-11-05): a design study. A shot leg cripples, both legs crawl while still fighting
  (unarmed: retreat); generic for every humanoid, IK and aim right in any stance; leaning at corners and ledges.
- **Gremlins armed** (2026-11-02): Armagon's gremlin already steals and fires the player's gun; check its rig first.
- **Player disarm: make it consistent, then on for PvE** (his notes vrragtriggers 2026-11-16 00:33, vrtesthall 00:48):
  off by default for 1.1 (`vr_player_disarm` 0; the feature and its sounds stay). Very inconsistent: hard to predict
  whether a shot or blow knocks your gun out; some enemy melee never disarms; enemies punching (fists) never disarm you.
  Needs tuning and fixes (QC/vr_playerdisarm.qc: which blows count, the rolls, feedback before the drop). Once polished,
  on by default in PvE. Multiplayer: two settings, PvE and PvP; PvP on by default (the MP branch does it), PvE off
  until this is done. 2026-11-22 (disarmfix, ROUND25): PvE now by jolts (no dice, a warning before the drop, fists and
  heavy blows count); left: his try in VR, then his OK to turn `vr_player_disarm` on.

## 7. Combat and feel

- **Crossed-sword parry** (2026-10-18): two blades crossed in front of the body count as a parry. After the AD melee work.
- **Re-record the melee eval takes** (2026-11-28, his decision): the 2026-10-12 takes (177, with the eval baseline)
  had drifted too far from today's melee and were parked (F:/QVR-Parked/2026-11-28/melee_takes; the kit's
  eval_baseline.csv and canary.txt emptied; eval.sh not used until then). He records a new, smaller set in the VR
  recorder (vrmelee-recording; quakevr/motions/expect.cfg keeps the expected verdict per name): per weapon class
  (axe/crowbar, sword, dagger, spear/pole, hammer/mace, fists, a bayonet stab, a two-handed blow) one each of a slash,
  a stab, an overhead, a weak wiggle that must not hit, and a parry; plus the neck snap, lift/judo and leg cases if
  still wanted. Then: a fresh baseline (eval.sh --full), a canary list, and the rule in the kit's brief_rules lifted.
- **From Quake Combat+ v5.7** (2026-10-16; ideas only, nothing of theirs reusable): hit-confirm audio layering;
  wounded monsters bleeding a trail, big ones split and crawling.
- **Flies on corpses and gibs** (2026-10-02): flies and their particles on every body after a few seconds; Armagon's
  head flies are off (`vr_head_flies` 0).
- **Active ragdoll research** (2026-10-22): FLINCH_RETROSPECTIVE.md, "Open items moved from BACKLOG.md".

## 8. Weapons, content and art

- **The dagger's flight, next patch** (his note vrtesthall 2026-11-16 00:38): fine for 1.1; refine its point-first
  settling, "the dagger is much like an arrow" (Debug > Tunables > Dagger Settling: its own strength, time, angle,
  tumble and spin damping, times the swords'; vr_box3d_step.cpp pointAlign).
- **Ammo type follows the loaded magazine** (2026-10-16): a lava-nail magazine fires lava nails.
- **One model per gun, not one per ammo type** (2026-11-16, his words): "rather than having different models for the
  ammo types, we should be able to adjust one single model: add attachments, change parts or textures; maintaining two
  separate sets of models with different rules is annoying and causes issues." (Today `v_lava`, `v_lava2`,
  `v_light_plasma` and `v_multi2` each repeat their base's housing, cuts and weapons.txt slot: ROUND25.md, "vr1116c".)
- **AD enemies' weapons for the player, the Deflector's gun, and AD soldiers' gun models** (2026-10-15/23):
  ARCANE_DIMENSIONS.md, "Open items moved from BACKLOG.md".
- **Bloody shotguns' own look** (MG3 decision 4): bloody skins for `v_shot`/`v_shot2` while `MG3_BloodyBits()` is set,
  or MG3's models laid out as the Super Axe's (`QC/vr_mg3_weapons.qc`).
- **AD inventory** (2026-10-12): the custom keys' names on the gadget's AD page and the jump boots' power level.
- **Reliefs:** generate the expansions' missing ones as AD's; the AD relief authoring pilot (10-20 textures) unstarted.
- **Art:** stretched UVs on the grappling hook's claws and a few faces of the rocket launcher, plasma gun, lava super
  nailgun and multi rocket launcher (ROUND21.md; ask whether they still show); body skins' wrist texel density (needs
  his repaint at a higher resolution).

## 9. Refactors (1.1 is out: now due)

1. Formatting: clang-format (Zancle configs) on first-party code, `// clang-format off/on` round hand-aligned tables,
   then clang-tidy fixes; one commit in `.git-blame-ignore-revs`. Never Ironwail's code. Hold until agent/split is merged (a mass reformat conflicts with it).
2. Ironwail extraction: about 1,600 lines of VR code out of Ironwail files into `Quake/vr/` (pure moves), the VR main
   menu out of `menu.c`, the VR heap macro remap; `IRONWAIL_DIFF.md` refreshed.
3. One player walk (mpreview B4): `vr_pmove.cpp` copies sv_user.c / sv_phys.c's walk, air, swim, gravity and flyMove
   line for line; a flat-screen player, `vr_pmove_own 0` and remote flat clients still walk the sv_phys.c copy. Route
   them through `pmove::playerMove` with an EdictBody and drop the QVR-modified SV_WalkMove/SV_AirMove paths (or at
   least a qvr_checks rule tying each copy to its original), proven by the B11b frame-identical traces.
Each verified by an unchanged physics hash, melee eval and `vr_menu_path_check`.

## 10. Big features (each needs its own decision)

- **Dynamic spawns: pick ideas** (2026-11-09): go through DYNAMIC_SPAWNS.md with him (its idea list and "Systems ideas
  from the 2026-11-09 audit"); the prerequisites are built.
- **Multiplayer: what is left** (co-op, joins, voice chat, NAT punch shipped: ROUND25/26, MULTIPLAYER.md): our VR systems synchronised, from
  Quakespasm-VR (GPL): MP_DESIGN.md, "Open items moved from BACKLOG.md"; the relay and the review lows are in sections 1 and 11.
- **Discord Rich Presence** (2026-11-18, his request): built 2026-11-20 (DISCORD.md; branch agent/discord): the RPC over
  Discord's local IPC pipe, no SDK; Menu Settings > Discord Status (`vr_discord`, off by default). Left: he creates the
  Discord application and gives its id (`vr_discord_appid`, then the shipped default) and, if wanted, a logo asset
  (`vr_discord_image`); decide whether it ships on; try it on Linux; the "join" button with multiplayer room codes
  (the `JOIN-HOOK` places in vr_discord.cpp).
- **Ranger helmet and biosuit visor view** (2026-11-18, his words): "having the Quake Marine Helmet also viewable as an
  option? You can put a little bit of lag to simulate being inside a helmet when moving the head and maybe use it to
  get some geometry culling going on for an extra bit of performance. The question also extends to the biosuit, since
  it has a visor it would be cool to see the liquids dripping from it when leaving a body of liquid or simulating
  thin film interference when light hits the biosuit visor." Ideas: an optional helmet interior at the view's edges
  (comfort first: the lag only on the helmet shell, never on the view), its edges usable as a cheaper view mask for
  culling/foveation (Quest); the biosuit's visor overlay with drips after leaving a liquid and a thin-film tint where
  lights hit it.
- **Seated play** (2026-10-18): holster and pouch zones laid out for a seated body, picked from the seated calibration.
- **Easier modelling** (2026-11-05): weapons have source .blends with export on save (MODELS_IN_BLENDER.md); left: the
  format audit and the other models.
- **glTF (.glb) for the hand and body models** (after 1.1; 2026-11-03): a cgltf loader filling the MD5 loader's
  structure, MD5 as fallback (ask before downloading cgltf). Weapons and other .mdl models stay.
- **Pillar and pipe climbing** (2026-10-14): a hand braced each side, hand over hand. Research.
- **Rendering:** TAA tuning for the headset, then DLSS/DLAA and FSR 3.1 (TEMPORAL.md, section 12's questions);
  `.rtlights` support; distance-field AO only if capsule/box AO leaves gaps; a foveation check and a GL-call audit before
  any Vulkan decision (VULKAN_MIGRATION.md); QuakeRay (github.com/f1ames0ff/QuakeRay) unreviewed.
- **Platforms:** Linux (WSL build in progress), Steam Frame (with kblood), bHaptics (2026-10-11); the standalone Quest build shipped (ANDROID*.md; its open points in section 1).
- **Emscripten/WebGL port** (2026-11-15): the game in a browser (flat first; WebXR later?). Builds on the Quest work's
  GLES 3 profile (`-glescompat`, agent/quest) and the no-child-process mode; open: threads (SharedArrayBuffer), the
  file system for paks, audio, download size.
- **AI companion** (2026-11-15): a coop partner that plays campaigns/levels with the player, using the navmesh
  pathfinding and answering the player's commands (follow, stay, attack that, fetch, go there), like the Skyrim dog
  demo. Builds on multiplayer's coop (agent/mp), FrikBot, the navmesh and the stealth/hunt AI.
- **HDR display output for flat play** (2026-11-15): the scene is already float (RGBA16F with `vr_tonemap`) and
  tone-mapped to SDR (vr_tonemap.hpp, vr_flatlook.cpp); missing is an HDR10/scRGB window. Windows + OpenGL has no
  standard HDR swapchain: present through a DXGI flip-model swapchain via WGL_NV_DX_interop2, or the Vulkan path
  (VK_EXT_swapchain_colorspace). Needs: a peak-brightness/paper-white setting, HUD and menus at paper white, SDR
  fallback, an HDR monitor to test. Gain is mostly highlights (fire, lava, muzzle flashes, bloom, sky); VR stays SDR. M.
- **Renderer ideas from Hexenwail** (2026-11-15 research; github.com/hexenwail/hexenwail, a Hexen II engine on
  Ironwail's renderer approach; clone in kit `scratch/hexenwail_cmp/hw`):
  - Bicubic lightmap filtering (`r_lightmap_bicubic` in their world shader, plus their lightmap gutter fix #113):
    smoother lightmap shadows up close in VR; a few texture fetches per world pixel. S-M.
  - Pre-compressed DDS/KTX replacement textures (their img_dds.c: BC1-7, KTX 1.1), uploaded as is: less VRAM and
    faster loads with HD packs. Desktop only (Quest needs ASTC/ETC2). M.
  - Hi-Z occlusion culling (`gl_hiz_cull`, gl_worldcull.c: previous frame's depth pyramid in the cull compute
    shader; 44-58% of surfaces culled on demo1): only if a capture shows world draws as the cost; one-frame holes on
    head turns/doors and per-eye pyramids make it risky in VR. L.
  - Their web/PWA build (web/, docs/WEBGL_RENDERER.md, docs/PWA.md): reference for the Emscripten/WebGL item.
  - Small: MAX_ENT_LEAFS 32 -> 64 (entity popping on big maps); V_RenderView returning when not connected (their
    headless map-change crash, f67ccf0); CI that compiles every shader with glslang.

## 11. Performance and parked

- **Frame drops close to active-ragdoll enemies** (tester "Joyeuse", Discord, 2026-11-18, a lower-end PC): melee range
  against active ragdolls drops the frame rate and makes melee hard. Profile a melee fight with 2-4 active ragdolls
  up close on a mid/low GPU+CPU setting (Box3D contacts with the hands/blade, ragdoll solver iterations, gore/dents,
  hit-box traces); consider a cheaper close-range mode or a quality setting. Measured 2026-11-20 (ROUND25.md,
  "ragperf"): not the drive, hit-model traces or hand contacts; Box3D contacts (~110 a driven ragdoll) and the armed
  grunts' strike sweeps (QC) grow with bodies close. Continued ("ragperf2"): hands kept in the bodies have 2-3 touching
  contacts; the contacts are a ragdoll's own parts' pairs that never touch (not filterable without changing the
  physics); the strike sweeps got an exact broad test (QC a frame 6 close: 67.5 k -> 59.6 k). Measured ("box3dup"):
  those self pairs cost the collide phase only, ~0.04 ms a frame with 2 driven ragdolls and ~0.1-0.13 ms with 6 (of a
  0.37-0.46 ms step; `vr_ragdoll_self_pairs 0` A/B). An exact skip (a Box3D change: a self pair's narrow phase skipped
  while its last distance less a motion bound stays over the speculative distance) would save at most that. Left: the
  GPU side close up, and a test on a lower-end PC to see whether the drop remains.
- **Linux Flatpak needed Steam folder access** (same tester): it worked once she granted access to
  ~/.local/share/Steam (she had first installed the Windows build there to try Proton). The manifest has
  `--filesystem=home:ro`, which should cover it: find why it didn't (a Flatpak'd Steam's path, a symlinked library,
  host vs sandbox paths) and make the launcher say exactly which override to run when it finds no Quake.
  (2026-11-27, linuxtar: the launcher now prints the `flatpak override` for each Steam library from
  libraryfolders.vdf the sandbox can't see, and the unsandboxed tarball avoids the question; the cause is not found.)

- **Performance, only when a capture makes one the top cost:** decals (PERF_DECISIONS.md 15), parallax option B and
  depth write, hit-box traces, shadows in crowded light, teleporter views, model loading in parallel, kept level copies:
  PERF_DECISIONS.md, "Open items moved from BACKLOG.md". Pickup thinks: PICKUP_THINKS.md (low priority).
- **Stealth AI limits:** STEALTH.md, "Open items moved from BACKLOG.md".
- **Dawn of the Machine's co-op and dm1** (MG3.md, decision 6): later.
- **A relay for the joins hole punching can't make** (his decision 2026-11-21: low priority, not now): a TURN-like
  fallback through the room service (or a second process beside it), used only when a room-code join's direct link
  fails (a symmetric NAT on either side and no forwarded port): both games send to the relay, which forwards a pair's
  datagrams by an allocation it hands out. Costs: a hop of latency (the server's distance from both players, ~20-80 ms
  more in Europe from the one VPS), all the pair's game traffic through the VPS (~25 KB/s down a player; 16 players
  ~400 KB/s), its own abuse limits (bytes and rooms relayed at once, an allocation per cookie-checked pair). How others
  do it: Steam Datagram Relay (Valve's relay network always between peers, the addresses hidden; free only on Steam),
  Epic Online Services' P2P (NAT punch first, Epic's relays when it fails), WebRTC's ICE (direct and server-reflexive
  candidates first, a TURN server's relayed candidate last). The port check (2026-11-21, MULTIPLAYER.md) tells a host
  first whether he needs one. Design note: Misc/rendezvous/README.md, "When the punch fails".
- **Old QuakeC notes** (TECHDEBT_2026-10-09.md 10): Rogue's teamplay variants don't work in VR (item handling
  commented out). (`readytime`, one for every client, is each player's `.vr_readytime` since 2026-11-19.)

- Weapon attachments (attach1, 2026-11-26): the system and the bayonet are in; the next kinds (scopes, under-barrel grenade launcher / flamethrower, nailgun igniter, LG lenses and more) and the open points: docs/vr-port/ATTACHMENTS.md, "The system" and "Ideas"; ROUND26.md "attach1".
