# Round 24: from 2026-11-05, towards 1.1

A per-feature reference: current behaviour only. Earlier work: `ROUND23.md` (1.1.0-beta4 to 2026-11-05: 8,600 lines, grep it, never
read it whole), `ROUND22.md` (1.0.1 to beta4). Add new work as a short section per feature, or fold it into its section: what it does,
settings and defaults, files, tests, open issues. Keep sections short: this file is meant to stay small.

## The armour's look (branch `agent/armorlook`)

Player and armed humanoids alike, in the shared generator; his mockup (too clean and cartoon-saturated before; shards, grime, less red).

- **Shapes** (third round, his notes: "bulky and pillowy", no layering): `vr_agarmor.cpp` `planGrid`, `fillRadii`, `addPlate`, `platesOf`: a piece is a regular grid of plates, 8 columns on the cuirass with a seam down the front and back, 6 on a limb with a plate facing front, in short straight rows. Each ring's radii are the skin's own, by rays out from the ring's middle at that height (`skinAlong`), raised only so a flat plate's chord clears the skin and the region's vertices (each corner at most `plateRidge` 1.1 over its ray), `plateFit` 0.12 units off: so rows follow the body's outline, narrow at waist, ankle and joints. Each plate is a shingle: its top lies on its foot under the row above, its lower edge stands `plateStep` 0.62 proud and laps `plateLap` 0.34 over the row below, a real step and shadow at every row. Feet share the grid's corners, weights included, so nothing shows between plates of a piece in any pose. A knee's plate on each thigh's end, an elbow's on each upper arm's. Mirrored: the cuirass's columns about its centre line (half way where a rest pose leans), a limb and its mirror image where the two are alike; a humanoid whose limbs differ keeps each its own fit. Covered too: the clavicles' skin, a closing row round the neck (`closeYoke`), a little over the joints, down to the belt. The bracer stays the forearm's elbow half (the wrist gadget). The helmet keeps its shape, new paint.
- **Paint:** `make_agarmor.py`, from noise and shapes alone, in the game's palette at the skin's own 256 x 128 (the full-colour PNGs are gone: an 8-bit skin needs no pull to the palette, and the debris is the same skin): `vragarmor.mdl` nine skins, `vragarmor.mdl_0_norm.png` the relief (rivets, dents, scratches, plate edges). A plate maps onto one of 30 tiles: its own tone, worn steel on its bevel in broken lengths, the shadow under the lip above and in its corners, rust as an accent, rivets at its corners, stronger than before. Each tier keeps to its own palette colours (`TIER_COLOURS`): green the sage row (no true green in the palette), yellow the tan row, red the pure reds and a little of the browns.
- **Triangles:** limit 3400 a body (`vr_agarmor_look_check`). Player 2 728 on every build (default bare forearms 23 016 originally), grunt 3 256 (3 436), enforcer 2 776, knight 3 256, death knight 2 824. Traded for the shorter rows: a plate is 12 triangles, down from 20: no top edge to bevel or side there (it's under the lip), side bevels and seams triangles.
- **Hues:** the plates' mean hues in the check, tiles within 14 degrees of each other. **The checker on plate faces** isn't the skin: it's `vr_retrolight_model_dither` (0.25, every model's ordered dither between light levels, skin texels; gone with it at 0). A global look setting, left as it is.
- **Checks:** `vr_agarmor_look_check` (Debug > Tests; `agarmor look: N passed, M failed`), every wearer and build: same seed same hash, another seed another; within budget; every piece has plates; left against right (mean nearest mirrored outer vertex, at most 0.25 units on the player, 4 on a humanoid's own limbs, whose rest poses and meshes differ); each interior ring's width against the skin's slice at that height (at most 5.5 units wider on the player, 7.5 on a humanoid's few edges; a death knight's upper arms not measured: his shoulder shells); each piece's thinnest-to-thickest ring against the body's (within 0.25, a humanoid's 0.45); the step at every row at least 0.25 units; rays out from each covered piece's skin hitting a plate, at least 85% at rest and 80% with knees and elbows bent, the thighs and arms forward and the spine leaning (a death knight's bent 60%: his shells stay on his clavicles; a limb's skin facing along it not counted); tier hues at least 30 degrees apart, one tier's tiles at most 14, red's saturation 0.35 .. 0.82. `vr_agarmor_check` still applies. `vr_pp_test 10` the tiers' shots where you stand; 11 every wearer and build; 12 bent, front and side. Scene: `armor_look`.
- **Painted body skin:** not retired; the cuirass's last row reaches the belt so it doesn't show at the waist.
- **Performance:** like for like on one tree (`origin/next`'s armour files and skins swapped in, then this branch's; `--lto`, `bench.sh`, exclusive, 2048 eyes, 90 Hz paced, every armed humanoid in full red: `scratch/al_bench.cfg`; medians of 3 reps, ms, original -> this): `play_e1m1_fight` CPU 3.26 -> 3.24, GPU 3D 2.03 -> 1.98; `crowd32_armed` CPU 4.53 -> 4.39, GPU 3D 2.90 -> 2.79. Within noise (one scene's reps spread up to 0.45 CPU, 0.42 GPU): no worse. One crowd rep (34 hitches over 33 ms) was set aside and the scene rerun; it didn't come back, its cause not found.
- **The dark things on the player's chest:** his two chest holsters (`vr_dumpview`: `legholster` at the chest, both sides) and the torch: worn on the skin, they show at the plates. The first rounds' two square patches are gone with the fit.
- **Open:** green is the palette's sage, as the original (no true green there). The death knight's shoulders read as a wide cape (his pauldron shells). `vr_agarmor_test 5` read 23/24 on some runs before this round, its check never caught: five in a row, two ways in, read 24/24 on the final build.
## The armour's joints (branch `agent/armorjoints`)

His notes (2026-11-05, screenshots `vrtesthall_2026-11-05_18-47-04` .. `19-22-04`): grunts' skin through the armour at the shoulder,
the left pec and the leg; the leg's plates mostly inside it; a lone plate floating at each elbow; a part of a limb worn alone has the
next part come out of it oddly; your own plates good, but the elbows' and knees' plates float. Files: `vr_agarmor.cpp` (`planGrid`,
`fillRadii`, `sleeveOf`, `bodyExit`), `vr_agarmor_check.cpp` (`vr_agarmor_pop_check`, `_pop_sample`, `_pop_report`, `lookCoverage`),
`make_agarmor.py` (the joint block), `QC/vr_playerplates_test.qc` (`vr_pp_test 13`, `14`).

- **The joints bare of plates, a leather sleeve over them:** the elbow's and knee's own plates are gone (they floated). The upper arm's
  plates end 1.6 units short of the elbow, the thigh's 2.0 short of the knee; the forearm's start 0.9 past the elbow, the shin's 1.2
  past the knee. Between, each part carries a sleeve of dark leather bellows (`sleeveOf`: 4 rings, 36 triangles): from under its last
  row of plates to the joint (or past it as far as its region goes, at most 0.8 under the next part's plates), 0.22 units off the skin
  (the lower part's 0.34, over the upper's where both are worn), each vertex weighted as the skin under it is (the skin triangle the
  ray leaves through), so it bends as the joint does. Its corners clear the skin between them and the skin's vertices between its rings
  (by at most 0.8 units each; past that a knee's point shows). A part worn alone ends at its joint, the next part's skin carrying on.
- **The fringe:** the body's vertices weighing on a piece's bones less than its region's cut but at least 0.2 (a deltoid between the
  cuirass and an upper arm, a buttock over a thigh) push its plates out over them, if a plate needn't swell more than `plateRidge`
  (1.1) for them; else they're left to show (a little overlap rather than a bulky plate).
- **Paint:** the joint block (224, 64)-(256, 96) of the atlas (the old helmet brackets', unused): dark leather folds round the joint,
  stitched on each fold. No numpy on this machine for `make_agarmor.py` (updated to paint it): `scratch/aj_paint_joint.py` painted the
  same block into the nine 8-bit skins and copied the inner block's leather relief, `generated.json` updated. Rerun the generator
  where numpy is to make it its own.
- **Triangles:** grunt 3 256 -> 2 920, enforcer 2 776 -> 2 728, knight 3 256 -> 3 064, death knight 2 824 -> 3 064, player 2 728 -> 2 680
  (the caps out, fewer rows on the limbs, 36 a sleeve). No timing taken: no wearer rose more than 5% but the death knight (+8.5%, 240
  triangles, under the budget).
- **The measure, `vr_agarmor_pop_check [model] [pose mask]`** (Debug > Tests: Pop-through Check): the poses rest, elbows bent 45/90/130,
  knees 45/90/130, arms up, a crouch, against ten sets of pieces (all, the cuirass, upper arms, forearms, both with the cuirass, the
  arms, thighs, shins, the legs). Per body region: *through*, a vertex outside every worn surface over it, a worn plate or sleeve behind
  it reached inside the body; *cross*, the skin's edges through a worn outer surface; plates whose face's middle is inside the body;
  plates off their neighbours (a check: none); the plates' bulk, their faces' mean distance off the skin (a check: 3.2 at most).
  `vr_pp_test 14` the same in the poses the armed grunts are drawn in (79 samples, three kinds coming for you).
- **Numbers, grunt, summed over the 9 poses (before -> after):** all worn: torso 74 -> 56, upper arm 15 -> 13, forearm 1 -> 0, thigh
  23 -> 16, shin 14 -> 14, other (neck, hands, feet) 110 -> 95. Thighs alone: shin through them 60 -> 5. Shins alone: thigh through
  them 35 -> 12. Upper arms alone: forearm through them 23 -> 15, torso 76 -> 63. Forearms alone: upper arm 42 -> 26. Plates off their
  neighbours: 4 -> 0 (the caps) on every wearer. Live, 79 drawn poses: thighs alone, shin through 554 -> 105; shins alone, thigh 333 ->
  60; forearms alone, upper arm 252 -> 174; all worn, upper arm 309 -> 167, forearm 32 -> 3; thigh plates inside the leg 25% -> 29% (fewer
  of them, the rest at the hip). Worse: all worn live, the shin's own skin through its sleeve 162 -> 186, the upper arm's 88 -> 153 (a
  low-poly knee and elbow past their sleeves in a stride).
- **Player (`vrbody_bare`), all worn:** torso 510 -> 135, upper arm 272 -> 100, forearm 51 -> 118 (the elbow's crease at 90 and 130 degrees
  through the sleeve, which hugs the skin where the plates stood off it). Forearms alone: forearm 78 -> 158.
- **Bulk (faces off the skin, mean):** grunt torso 1.28 -> 1.33, upper arm 1.88 -> 1.89, forearm 1.02 -> 0.88, thigh 2.70 -> 2.94 (its bulky
  half left, the knee's rows gone), shin 1.40 -> 1.43; the player's within 0.05 of before.
- **The look check's coverage** counts a limb's skin only as far as its sleeve goes (past it, the next part's, whatever the weights
  say) and leaves out a piece with fewer than 8 vertices (a death knight's thigh under his skirt has two); the death knight's at rest
  0.8 (his elbows' points past the sleeves).
- **Not fixed:** the grunt's jaw through the cuirass's collar at rest (by up to 2.8 units: the head bends down into it); the cuirass's
  side over the arms where the arms hang against the chest; a fit of the yoke over the shoulders' tops was tried and changed nothing
  (taken out).
- **Sheets:** `C:/OHWorkspace/qvr-kit/scratch/armorjoints/` (`vr_pp_test 13` in vrtesthall): the grunt front and back, an elbow and a knee
  close to, each part of a limb alone, yours bent; `sheet_before.png`, `sheet_after.png`.

## Flatscreen: the stance, replacing the barrel rest

A long gun with a two-handed hotspot turns the torso (free hand's shoulder forward), brings the gun's hand towards the shoulder pocket,
and draws the free hand on the weapon's own hotspot from Weapon Offsets, through the code that draws a helping hand in a headset. A
look: no grip, aim, weight or recoil. `vr_flat_stance` 1, `vr_flat_stance_turn` 25, `_back` 10, `_in` 2, `_up` 4; `vr_flat_rest*` retired.
Details and measures: FLATPLAY.md, "The stance". Files: `vr_flatsight.cpp` (`stanceAndLook`), `vr_flatplay.cpp`, `vr_view_hands.cpp`
(`setupHand`, `steadyOther`), `vr_view_diag.cpp` (`twoHandSpot`), `vr_twohand.cpp` (`drawnTransition`, `drawnHelping`). Test:
`Misc/quakevr/flatstance_test.sh`, including that editing a hotspot moves and turns the flat hand. Open: in the ironsight at eye relief
14 the arm can't reach the hotspot on the guns without a real grip, so the hand hangs there.
## The player's armour plates (branch `agent/playerplates`)

The armour you wear drawn as plates on your body, a look alone: `.armorvalue`, `.armortype` and T_Damage's rules are untouched.

- **Plates:** `vr_agarmor.cpp`'s generator on `progs/vrbody*` (each build, bare or not): its own bones taken for a rig (`playerRig`), its mesh's weights (`buildSkeletalBody`), skinned on the body's matrices, so they follow the IK. No second armour system; only the player's state is apart.
- **State:** QC `vr_playerplates.qc` polls the armour each frame (`PlayerPostThink`), whatever changed it. `.vr_pp_type` (tier by `.armortype`) and `.vr_pp_mask`, fields of their own because combat.qc reads a wearer's `.vr_ar_*` for damage. Sent to the clients as `QVR_SVC_ARMOR` (`serverFrame` now loops the client edicts too). A save keeps them; parm75 carries the mask to the next level (only the base game's parms: on an Arcane Dimensions level the count is right, the pieces canonical).
- **Count:** `1 + floor(min(1, value / most) * 9)` with most 100/150/200: 10 at full, 5 at half, cuirass alone at 1, none at 0.
- **Shed:** limbs and helmet first, nearest `.vr_pp_hit` (T_Damage: the inflictor's middle) when a hit just came; else the reverse of the put-back order; the cuirass last. The humanoids' crack (`VR_AR_BREAK_SOUND`) and debris (`VR_AgArmor_DebrisOf`, models `progs/vrbody.mdl#plate<n>`, precached at worldspawn, no debris in deathmatch) and a pulse in both hands. The helmet sheds no debris: it would be round your eyes.
- **Morph:** `setupPlates` (`vr_view_body.cpp`), `Entities::plate[5]`: 0 the settled pieces, the others one change each. A gained piece comes in with the guns' ammo morph (the alias shader's `Morph`, kind 1's amber seam; `VR_AliasMorph` now allows skeletal models, their noise in the rest pose, `bind_pos`, so it rides the body). A new tier: the old colour dissolves out where the new one appears. A level's or load's first second is taken as it comes (no morph).
- **Helmet:** drawn only where the head is (`plateBonePoses`: the body's own rule, `VR_AliasBonePoses`' `away`): never from the eyes, VR or flat-screen; yes in the mirror clone, spectator cameras and shadows.
- **Limits so nothing obstructs:** the bracer is the forearm's elbow half (forearm and `foretwist1` bones), leaving the wrist gadget, its straps and the hands clear. The pauldron caps (`vr_body_pauldrons`) are hidden on a side whose plate is on, leather (style 1) on a side without: they showed through the plates and would stay armour-coloured after the plate fell.
- **Painted skin (`vrbody_*` armour * 4 + damage, `armor_texel`):** five lames on the torso, in the armour's colour, belt to collar. It is entirely under the cuirass, which is on as long as any armour is, so it doesn't double up in play; only a morph's dissolve shows it through. Left as is; retiring it (the unarmoured skins' torso under the plates) is the author's call.
- **Settings:** `vr_player_plates` (1; Player Appearance), `vr_player_plates_morph_time` (0.6 s). `vr_player_plates_info` prints what's drawn. `vr_agarmor_info` lists the body's plates too.
- **Cost:** model made in 3 ms (lean mesh) to 25 ms (bare forearms' denser mesh: 23k triangles, the lean mesh's 6k) as the body first shows. With `--lto --exclusive`, full red in view, 1024 x 1024 eyes, RTX 4090: host frame 1.08 against 1.01 ms (+0.07), `view body` 0.03 ms CPU.
- **Also fixed:** `makeModel` read the skins' header after growing the hunk, which may evict and clear it: a big model came out without skins, not drawn. `loadMd5Replacement` took `progs/vrbody.mdl#plate0` for the body's md5mesh (no model with a `#` has one).
- **Test:** `vr_pp_test` (`vr_playerplates_test.qc`). `map vrtesthall; vr_pp_test 1` `pptest: 53 passed` (flat: `-ExtraArgs -flatscreenmode`, 52: no spectator camera there); `vr_pp_test 2` `ppsave: 5 passed`; `vr_pp_test 3` `pplevel: 5 passed`; `vr_pp_test 8` the picture sheet; 5-7 Debug > Tests. Scene: `player_plates`.
- **Open:** another player's plates aren't drawn (bots and clients keep the state; nothing draws it, no crash: deathmatch with bots run). Debris is `vrbody.mdl`'s at its model size, whatever the build and body scale.

## Magnetic holster chain on fast swings

The backlog's "worst piece 78 %" was a squeeze, not a stretch: on the fast swing the longest piece was 15.7 % over, the
shortest 78 % under. A hand at 5-8 m/s moves the pinned end most of a piece's length a frame, and the pull-only
`holdLengths` has nothing to push the next point away, so the end runs into it and the pieces bunch (why more iterations
never helped). Second cause: the drawn gun's weight spring snaps back (`vr_weight_spring_snap`), a 60 cm jump in a frame,
and the tether's jump check restarted it dead straight, each piece 12 % short at 65 cm (more closer in) with nothing to
bend them out of the line.

- `verlet::limitPieces` (`vr_verlet.cpp`): after `holdLengths`, each piece kept within `vr_magholster_chain_limit` (0.1,
  0: off; Immersion > Magnetic Holsters > Chain Physics, "Chain Piece Limit"). Up to 3 rounds of a pass from the weapon
  back and one from the holster on, each point moved only to its neighbour's limit, `prev` with it (no speed from it, so
  no energy in); it stops at the first round that moves nothing. A chain within its limits is not touched. Points lying
  on the world are left to the collision (limited too, the corner case's worst piece went from 43 % to 122 %). The tether
  only: the grappling rope and the AD hose are unchanged.
- `hangAfresh` (`vr_magholster.cpp`): a tether started afresh hangs in a parabola of its length, its points even along it,
  stopped by the world below the line.

Test: `Misc/quakevr/magchain_stretch_test.sh <agent>` (`RATE=72` for 72 Hz): slow, fast and whipped moves, the limit off
then on, verdict `magchain_stretch: ... PASS`. `vr_magholster_chain_log` also prints a `magpiece` line per step (stretch,
squeeze, sag). Worst piece on fast swings, limit off / on: 78.0 / 10.0 % at 90 Hz, 78.1 / 10.0 % at 72 Hz (78.0 before any
change too); the slow swing's sag differs by 0.0 mm at both; jerk p95 31 / 30 mm; 0 not finite. The tether's step 0.009 ms
either way (no-LTO exe, ratio only). `magchain_test.sh`: fast tether 78 -> 10 %, slow 2.5 % and fast jerk p95 27.7 mm as
before; jerk max 220 -> 300 mm, on the gun's snap frame. `magchain_sim_test.sh`: 0 pieces through the world; worst piece
the same with the limit on and off: corner 43 %, floor 94 % (a chain lying on the world, not limited).

Open: the lanyard is a spring cord, not this chain: 16.7 % on the same fast swing, untouched. A chain dragged along the
world on a fast swing is not limited where it touches.
## Shoves off ledges: a backpedal, not a launch

His words: "Shoves off ledges look too perfect: whatever the force, humanoids get exactly the force that makes them fall,
every time." A shove with a ledge in its reach (`VR_Knockdown_Ledge`, unchanged) on one that can stagger now goes through
`VR_Knockdown_LedgeBackpedal` (vr_knockdown.qc): the engine's stagger in a ledge mode (`ragdollstagger`'s `sure` 2, the
builtins' table being full; `box3d::ragdollBackpedal`). It backs up on its feet and tips over where the floor ends under its
pelvis, keeping what is left of the shove. Shoves away from a ledge, those that can't stagger (launched as before,
`vr_knockdown_shove_ledge_*`) and hits by a ledge (`vr_kdtrig.qc`'s Over a Ledge) don't change.

- **Tunables** (Debug > Tunables, `vr_knockdown_ledge_backpedal*`): on/off 1 (0: the old launch); `_speed` 0.4 of the shove's
  own speed for its first speed, between `_least` 70 and `_most` 300 u/s; `_vary` 0.2 on that and on how far it would go;
  `_carry` 2, how far it would go at least, times the way to past the edge (it slows evenly over that); `_balance` 0.7 to 1
  by chance; `_yaw` 6 degrees off the shove by chance; `_push` 0.35 of the shove's push on its top; `_hertz` 10, its pelvis's
  pin (a stagger's 5); `_tip` 100 u/s and `_past` 32 units, carried on past the lip so a slow one doesn't sit on it.
- **Ledge mode in the stagger** (vr_ragactive.cpp, `Stagger::ledge`): its steps uncounted; `sure`, so only the floor's end
  takes it over; a foot steps at most to the lip (`lastOnFloor`), never onto air; a foot its hip has left behind slides
  rather than holding (held, it swung the pelvis down and sat the body down about 100 units in, for good).
- **Test**: `sv_random_seed <n>;map vrclimb;wait30;god;vr_agrunt_test 210`, `-QuitOn "test 210 over"`
  (QC/vr_ledgeshove_test.qc): real shoves of a grunt, its back to the long ledge's lip; one hand, two hands, two hands running,
  each from 48 units and from far, 6 trials each; then 6 away from the lip. Measured over seeds 1-4: 144 of 144 over; on its feet 98-99% of the way
  to the lip (one and two hands), 81-89% (running); lip speeds from 48 units: one hand 73-85 u/s (59 to 90), two hands
  133-156 (120 to 176), running 224-239 (193 to 267). Scene: `{test:shove_ledge}`.
  The old launch for comparison (`tune vr_knockdown_ledge_backpedal 0`): lip speeds 234, 378, 557 u/s, each within 3 u/s of
  the shove's own speed; 34 of 36 over.
- `vr_agrunt_test 202` (ledges, vrclimb): the block's case now goes over every time, on a lesser landing: it keeps or drops
  its weapon by the impact rule alone (`vr_agrunt_fall_keep_impact`), and the test checks that.
- **Open**: the look is for him to judge in VR (the far backpedals' sliding back foot above all). A tall drop's ledge grab
  (`vr_ragdoll_active_pullup`) may still catch a slow one at the lip, as it could before.
## Lightning on water

His note: lightning shot into water (thunderbolt, Mjolnir hit, Shambler) should electrify the surface. After the first fix,
in flat-screen on vrfiringrange and vrstart: still no arcs on the surface.

**Why.** Two gaps. First, only the thunderbolt ever had surface arcs (`git log -S QVR_SHOCK_SURFACE`: one hit, `35f25522f`);
Mjolnir and the Shambler never did, and no commit removed anything. Second, the one he saw: `drawSurface` queued its arcs with
`lines::line`, which only `stereo::drawUi` draws, the headset's UI pass. A flat screen sends and receives the effect and queues
its lines every frame, and nothing draws them. The first fix's test counted `drawSurface` calls in the mock headset, so it passed.

**Now.** `VR_LGWater_Zap(from, end)` (weapons.qc; off with `vr_lg_water 0`) from `LightningDamage` and `ad_LightningDamage`, so
every bolt; `VR_LGWater_ZapBelow(at)` for Mjolnir's blow and arcs and any shock. Only the look: damage through the water is still
only the thunderbolt's and the shock's. The surface's arcs and the shock's burst now go through `lines::sceneLine`, the scene's
translucent pass, as a beam's arcs: both flat and headset, hidden by walls. Above the waves (`vr_water_waves`); wider further off
(from 200 units, up to 3x); arches leaping off the water, since flat arcs close to a line at a glancing angle; thinner within 96
units of the eye, or a ribbon by the camera fills the view. `vr_lg_water_arcs` 1 (tunable, Debug > Tunables > Electrified Water
Arcs): their width, 0 none (the blue light stays). Not drawn on a flat screen still: the shock's arcs on your own arms and in
front of your eyes (other `lines::line` users, not touched). A hammer slapping the surface with nothing solid is still no blow.

**Test.** `bash Misc/quakevr/waterzap_test.sh <agent> <outdir>`: pictures, not a counter. Each case (`QC/vr_waterzap_test.qc`:
thunderbolt by `+attack` steep and glancing, Mjolnir swung onto a knight over the water, a shambler's bolt by `CastLightning` on a
bare entity) on vrfiringrange and vrstart, `-flatscreenmode` and mock, twice: `vr_lg_water_arcs` 1 and 0, the screenshot 12 frames
after the client first draws the effect. PASS: at least 2000 pixels brighter and bluer by over 40 (`waterzap_diff.py`, no image
libraries; `<tag>.png` is both frames and the changed pixels). Two runs differ a little anyway (a bolt's model, particles: at most
1656 blue pixels on next's flat frames, with Mjolnir's beams and knight not drawn: `r_drawentities 0` for that case).
On next's drawing: flat 0 of 8 (blue 0..1573), mock 8 of 8 (the UI pass draws them there). With the fix: 16 of 16 (flat 2966..43965,
mock 24869..105174), and flat with `r_oit 0; r_wateralpha 1` 25720 and 3466: not hidden by water that writes depth.

**Test trap.** A QC `localcmd` lands behind whatever the script's buffer still holds, so behind a `wait100000` it never runs: end
the script at the test's command and let `-QuitOn` stop it. That is why the first test's trigger "did not fire". And a frame takes
one impulse: `impulse 9; wait5; impulse 161`, `vr_weapon_grip_mode 1`.

## Close two-hand grips: both hands stay where they gripped (branch `agent/cratebound`)

The grip fit of the second hand (`vr_carry2h.cpp`, `fitGrip`) moved its grip out of the prop but left the hand behind: the
grips' middle was then off the hands' middle, so `solve` shifted the prop and each hand sat half the move off its grip (the
first hand pulled along), and the grips' line was off the hands' line (a phantom turn, bigger the shorter the line). Now the
move is also the hand's `Hold::reach`, as a hand held out of a prop already was: at the grab's pose `solve` gives the prop
where it was. A grab with nothing to fit runs as before. When the first hand lets go, the prop jumps by the reach as it does
for a hand held out.

Test: `Misc/quakevr/crate_grip_test.sh <agent>`, `vr_carry2h_grip_test` (a small crate in both mock hands, a second of fixed
frames, each hand's largest distance from its grip, limit 0.5 cm; verdict line `vr_carry2h_grip_test: N passed, M failed`).
The bug reproduces in the mock:

| case | grips | before: each hand off its grip | now |
| --- | --- | --- | --- |
| close, off hand beside the main | 17.8 cm | 0.70 cm (FAIL) | 0.03 cm |
| close, pushed 10 cm in | 17.8 cm | 0.70 cm (FAIL) | 0.03 cm |
| close, pushed in, keep-out off | 17.8 cm | 2.60 cm (FAIL) | 0.03 cm |
| wide, nothing to fit | 94 cm | 0.01 cm | 0.01 cm (the same) |
| wide, the fit moving the grip 23 cm | 94 cm | 11.75 cm (FAIL) | 0.01 cm |

The mock's hands themselves move 0.72 cm over that second, and the crate follows them (2.5 cm at its middle, before and after).
The test's first grab uses `vr_grip_fit_reach 3` (a spawned crate lands on the hand). Scene: `{test:crate_close_grip}`.

Not tested: the drawn hand in a real headset, or hands turning together in a close grip.

## A turned teleporter's exit: the body turned with the gate (branch `agent/cratebound`)

The body carried through a gate was met at the destination with the axis-aligned bound of its turned box: at 45 degrees 41%
wider each way, so a narrow exit the body fits blocked it. Now the destination half of a move (`portals::destHalf`, for
`VR_PortalBodyMove` and `VR_PortalReachMove`) sweeps the box itself, turned with the gate: `SV_MovePortalTurned`, then
`hull::clipPortal`'s `turned` and the sweep's `BoxTurn` (`vr_hull.cpp`). The bound still finds what is near. The box's corners
meet each brush's faces and bevels, and its own faces meet the brush's corners (`clipToTurnedFaces`, worked out only for a brush
its planes haven't missed, near such a gate). Right-angle turns take the old path, unchanged.

Still conservative, never through a wall: recovered clip brushes meet the bound; edge-against-edge axes are left out, so a sloped
edge can stop the box slightly early. A prop tossed through a gate keeps its bound (`toss`'s placement test).

Test: `Misc/quakevr/teleporters/turned_exit_test.sh <agent>` (vrteleporters' new room d45, west of T's west wall door: walks into
gates that come out at yaw 45 into free-standing corridors; verdict line `vr_portals_turned_test: N passed, M failed`). With the
old bound: the 28.3-wide exit stops the 16-unit body (FAIL, the player stays at the gate, 1 passed, 1 failed). Now: through it
and on; the 14.1-wide exit still stops him, his origin 8.4 units short of the gate's plane (2 passed, 0 failed).
`teleporters_test.sh walk` on the new code: `turn45` crosses both ways, and the other gates cross or stop as the map intends (no
run of the old code to compare line by line). Open, from the box sizes and not tested: once carried the body is axial again and
needs 22.6 across a 45-degree corridor, so an exit between 16 and 22.6 wide passes the gate's test and may stop or stick him just
past it.
## Still grips (super nailgun's off hand) and the shotgun's stray vertex

- Cause of the super nailgun's drifting off hand: `animationMotion` (vr_view_weapon.cpp) carries a hand holding a weapon with the rigid motion of the model's vertices round the grip, so a spinning barrel beside the grip turned the hand (5.1 units at the grip over the spin frames).
- New hotspot field 25, `hsN_still` (Weapon Offsets > Hotspots > "Ignore Animation", Grip only; default 0): the hand on it ignores the firing animation, frame 0 assumed (`gripMotion`). On for the super nailgun's hotspot 1 (weapons.txt slot 4, and the slots that inherit it).
- Test: `vr_still_grip_test` (console; `map e1m1;wait30;vr_still_grip_test`): every Grip hotspot over every pose; a still grip's hand moves 0.0000 (the animation alone: 5.116), all other grips equal `animationMotion`. 4 passed, 0 failed.
- Shotgun: `Misc/quakevr/mdl_motion.py` reads a model's poses (per-vertex motion and deviation from its neighbours). In `progs/v_shot.mdl` no single stray vertex shows in the data: near the stock (x < 12) the largest deviation from the neighbours' median is 0.77 u (v75, v83, v172, v173, a symmetric group) in shot3; the whole stock follows the kick to the byte grid. Not fixed, no model change. Candidate left: the engine's kick cancel (vr_kickcancel.cpp, `vr_kickcancel_info progs/v_shot.mdl`: 58 moving-part vertices, 55 are the muzzle flash).

Follow-up: `vr_kickcancel_info` now lists the moving parts by vertex. Besides the 55 flash vertices, v55, v74, v81 (coincident, the stock's top corner at 4.0 -1.8 5.7) were parts; `Misc/quakevr/fix_shot_stock_corner.py` puts them on the gun's fitted rigid kick in v_shot.mdl and vr_pumpbody_on_v_shot.mdl: parts now 55 (flash only). The second still-grip slot is slot 11 (inherits slot 4, correct). `vr_menu_path_check maps/vrcalibration.map`: 6 found, 0 missing.

## Weapons by ammo boxes (nail pistol, heavy revolver)

- `SpawnAmmoBoxWeaponsImpl` (items.qc), after the 45% box gate (times `vr_ammobox_drops_chance_mult`): nail boxes get the nail pistol last in the chain after the axe (chance 0.15, no ownership gate: 5.4% per box with no nail weapon, 3.2% with the nailgun, 1.6% with both); shell boxes get the heavy revolver after the axe, before the shotgun row (chance 0.10, only with the super shotgun or a revolver held: about 3.6% per box). Not the flare gun or chainsaw.
- The rules live in `TryAmmoBoxDropRule` (vr_weaponutil.qc; `AMMOBOX_RULE_*` in vr_defs.qc); `TryAmmoBoxDrop` keeps its signature (rule: owned). `VR_WeaponUtil_EntIsEligibleForWeapon` (shared with monster drops and armed replacements) is untouched. In a Survival Arena run the rules do nothing (the run's pack rule as before); `vr_ammobox_drops` always drops the revolver without the gate, disable drops nothing. Chances are literals like the chain's others (no tunable).
- Armed grunts (read only): the replacement tiers (vr_armedrepl.qc) put the revolver in the rare tier, gated on a player owning one (`VR_AGR_Owned`) and the rare map index, so they cannot hand it over before the player has it. The pool (`VR_AG_PickWeapon`: an armed grunt placed with no gun; `vr_agrunt_w_revolver` 0.25) has no ownership or map gate.
- Test: `vr_arena_test 37` (any open map off the arena: `map e1m1;wait60;vr_arena_test 37`): 8 rows of 10000 real boxes; ends `arena: test 37 over: P passed, F failed`.
## Shotgun and nailgun looks (branch `agent/gunlooks`)

The author's two mockups as a guide (textures that fit the geometry, no flat faces, wood grain, subtle rust, low rivets; not their rust-brown overall colour, and the glowing sights untouched). His nailgun mockup is `v_nail.mdl` (two tubes, hex rings, muzzle boxes, magazine under the grip), not the nail pistol, compared in renders first.

- **Generator:** `Misc/quakevr/gunlooks.py` (`--capture` once on what polish_weapons.py leaves, then it reruns byte for byte), `bake_normals.py v_shot.mdl v_nail.mdl v_lava.mdl`, `gunlooks_check.py [--kick log]` (`gunlooks_kick.sh <agent>` makes the log). Rerun after polish_weapons.py. Inputs in `src_models/gunlooks/`: each model with its skin blanked but for the fullbright texels and what the unchanged triangles use, and each old triangle's mean colour: no old paint in the new art.
- **Skin:** every face re-mapped into charts of one facing at one density (6 texels a unit on the guns, 4 on the pickups; the old maps ran from stretched streaks to single colours), the shotgun's skin widened from 128 x 215 to 512 x 219, the nailgun's 512 x 210. Painted per texel from the surface point it shows (a solid texture, continuous over seams): metal, wood rings and fibres, worn edges, grime, inset panel lines, flutes on the nailgun's tubes, rust patches (10-12% of the metal) and stains under rivets. Palette only, never a fullbright index. Metal's mean colour goes from about (21,20,23) to (30,29,32): off black, same family.
- **Geometry:** 14 rivets on the shotgun (420 triangles), 10 on each nailgun (300), low hex heads appended and carried by their piece through every frame. None within 1.41 of the sight line, none on the pump's rings; the body part file gains them. Old vertices and triangles keep their bytes and order (a corner may be a coincident copy at the end). The fullbright texels (3292 on the shotgun) stay where they are, byte for byte, with the 225 triangles that use them.
- **Relief:** the bake now takes these heights (`src_models/gunlooks/*.relief.npz`, `normaltiles.py` `MODELS`), not guesses from the paint, whose dither is no shape. Sidecars: only `_0_norm.png` exists for these models (the three guns and two pump parts, all rebaked); they have no glow or bump files, the glow is the skin's fullbright indices.
- **Variants:** `v_lava.mdl` the same mesh and paint from its own colours, its glow texels carried onto the new charts; `g_shot0.mdl`, `g_nail.mdl` (pickups) the same paint, no rivets, no baked relief. The magazines and wells are unchanged. `v_nail2`, `v_lava2`, `g_nail2`, `g_shot` were not part of this.
- **Traps met:** `fx_gfx_mask` names its triangle count and reads as none if it differs: weapons.txt's three masks extended (the author's config holds none). The engine draws the nailgun's tubes about three times darker than the plate beside them, whatever their vertex normals (it was so before: near black): painted 2.4 times lighter (`TUBE_LIFT`), the cause not found. The rivets nudged the kick fit enough to make the stock's other corner (4.0 +1.7 5.7) a moving part of the pump body: `fix_shot_stock_corner.py` now fixes both sides, on the input (6 vertices' firing poses, up to 0.67 units): the one change to old vertices, against the shipped model.
- **Verified:** `gunlooks_check: 68 passed, 0 failed` (faces of 0.5+ square units and 6+ texels wide: shotgun 4.8-6.5 texels a unit, limits 3.9-7.5, stretch at most 1.28, limit 1.7; none of 934 faces of 8+ texels in one colour; moving places none new: 55 parts' vertices on the shotgun as before; nail 232 places as before, more vertices only as copies). `autopump_test.sh`: 7 passed, 7 failed, the same seven on the old models (shell timing; not from this). `vr_menu_path_check` not touched.
- **Sheets:** `C:/OHWorkspace/qvr-kit/scratch/gunlooks/shotgun_before_after.png`, `nailgun_before_after.png` (`r_fullbright 1` for neutral light).

## Flatscreen model FOV (agent/flatfov)

- **Settings:** `vr_flat_model_fov` (default 0, off), `_near` 18, `_far` 35, under Debug > Tunables > Flatscreen > Model FOV. `flatplay::modelFov`, shader `FlatFovPoint` (`vr_flatfov_glsl.h`). `vr_flatplay_status` prints the scale, each part's distance from the eye and the batch counts.
- **Near and far are measured** (`vr_flatplay_status`, shotgun, standing, e1m1): elbows to 16.5, hipfire muzzle to 17.5, hips 22.3, knees 34.7, ankles 47.1, toes 48.1. A sighted muzzle is at 26 but on the axis, where the scale moves nothing. Other guns were not measured.
- **Alias shader:** the point is chosen before the one multiply; light tiles, fog and shadows keep the true place. `Motion.z` marks the instance, `VR_FlatModelFovEntity` picks hands, body, held, holstered, worn, casings and the held prop. Shadow maps never (`aliasdepth`).
- **Temporal AA does not run here:** with `vr_taa 1`, 0 of 200 batches were inside a headset's eye, and motion is collected only there.
- **Mapped:** muzzle flash, casings, attachments, wounds, hot barrels (the alias path); tracers, wisps, the flashlight's lens and cone, the gadget, ammo screens with their bezels and glows, lines (laser, arcs), cords and hoses, all particles (by distance: a particle has no owner).
- **Not mapped:** bullet-time trails, ropes, lying particles, non-screen `text3d::queue` text (only with `vr_weapon_screen` 0). Decals and outlines never land on his own models.
- **Off against before:** not bit-identical between runs on either build. Base against itself: up to 767 pixels, max 29, on the gun's edges; base against off: up to 597, max 29, same place. The wall scene: 0 and 1 pixels.
- **Side effects to judge:** a model FOV wider than the sight's world FOV shrinks the sight picture (0.91 at 75, 0.70 at 90, world 110). Looking down, the near body moves out further than what is behind it, so his floor shadow and a holstered gun show.
## Shotgun and nailgun looks, second pass (branch `agent/gunlooks2`)

The author's notes on the first pass ("the new nailgun is an improvement in some areas, but large downgrade in other areas"). The first pass kept one mean colour per triangle and repainted everything from one metal recipe, which lost the guns' materials and their painted detail.

- **Now:** each texel starts from the old skin at the surface point it shows (`src_models/gunlooks/` keeps the models with their paint; the re-mapped charts stay): materials, the hex rings, their centre holes, the muzzles' grates and dark interiors, the pump's ridges come back as they were. A face the old map folded nearly to a line (over 30:1) takes its mean colour instead. On top: slight tone and grit, worn edges, rust, then a baked occlusion. This replaces round 24's first brief's "no old paint": the notes ask for it; it is the same paint the shipped models always had.
- **Pump:** wood's tone follows each face's old brightness (rings light, the fore-end under them dark), edges lit. **Aiming view's bright facets:** gone. Not the normal map (the same with `vr_normalmap_authored 0`): they were the first pass's paint lightness differing per chart and worn lines on the receiver's shallow folds and T-junctions; the old paint's smooth highlight is back, only creases over 35 degrees wear (`CREASE_COS`), open edges none, and slivers take the paint of the wider face beside them. A thin dark crease line and a small lighter wedge remain behind the sights, cause not found.
- **Occlusion** (`bake_ao`): 20 cosine rays from each texel against the model's own triangles within 3 units, darkening at most 60% (`AO_STRENGTH`); fullbright texels and the triangles that stay are never touched.
- **Rivets:** 0.4 across and 0.19 high (were 0.34, 0.14), a lighter steel, a dark ring on what they sit on; same places.
- **Colour:** metal's mean warmer by about 4 (red minus blue) and blotch noise cut to a third. Rust a dark saturated red-brown whose brightness follows the metal under it (at most 1.9 times, plus a little): the lit-map render showed pale dashes and dots on the near-black receiver that flat light hid; making the scratches and scuffs much fainter is what removed most of them (a few remain near the loading port under a strong light). The palette now also has the old paint's dark olives and reds; colours are matched one hue at a time (mixed hues read as a blue-and-brown camouflage).
- **Tubes:** `TUBE_LIFT` 1.25 (2.4 in the first pass): they are near black under flat light, as in the old model, but not under the map's light. The first pass's flutes and panel lines are gone: the old paint's vents are back.
- **Relief:** the heights softened and lower, the old paint's lines and grain only, no rust bumps. The fine scale-like pattern seen under strong light is in the old model too.
- **Check:** `gunlooks_check.py` gains: the bake leaves glowing triangles' texels as they were; the nail muzzles' back faces near black (limit 18 of 255).
- **Grips:** `vr_still_grip_test`: 4 passed, 0 failed, on e2aa179c0 (no code diffs, no source newer than the exe; an incremental build, not from scratch).
- **Sheets:** `C:/OHWorkspace/qvr-kit/scratch/gunlooks2/`: `shotgun_three_rows.png`, `nailgun_three_rows.png` (old, first pass, now; flat light), `lit_map.png` (no `r_fullbright`: old, now), `lava_and_pickups.png`.

**Gun looks, fewer rivets (branch `agent/gunlooks3`, the author's "a bit overboard"):** 14 to 6 on the shotgun (a pair up the receiver's front edge at the fore-end, one more in the row of pins along its lower edge), 10 to 4 on the nailgun and lava nailgun (the receiver plate's two front corners); mirrored pairs on the gun's own lines, same size and dark ring; the barrel masks' counts follow (1756, 1008).

## Survival Arena: banners, and "any arena map" (branch `agent/arenabanners`)

The author's ask: live text banners close to the arena's ceiling (foes left, wave, time, score), in the wrist gadget's and text boards' style, reusable for future arena maps, and nothing hardcoded on `vrarena`. Having seen a first version: the ceiling 40-60% higher and the banners close to it, "so that they are less distracting in general".

- **The arena map check:** `arenaMapNames` in `Quake/vr/vr_arenawaves.cpp` is the one list (holds `vrarena`). C++: `arenawaves::isArenaMap`, `arenaMaps`, `defaultArenaMap`. QC: builtin `isarenamap(string)`, used by `VR_ARENA_On`. A map named only in `assign.cfg`, or `vr_arena 1`, still runs the waves as before. Moved onto it: `vr_arena_play`'s default map, the wave editor's default map, map list and open, test 11's and 34's checks. Menu and cvar help now say "an arena map". Steps for a new one: MAPPING.md, "Adding an arena map".
- **What stays literal:** the menu's `command("Survival Arena", "map vrarena")` (a destination, a `const char*` the row keeps, not a check); `vrarena_gen.py`'s `MAPNAME`; the waves' self-test fixtures in `vr_arenawaves.cpp`; test scripts, scenarios and the benchmark (each names the map it runs on); comments saying where a number was measured or which map a test wants.
- **The banners:** `QC/vr_arena_banners.qc`, entity `info_arena_banner` (origin the face's middle, `angle`, `worldtext_scale`), so a new arena gets them by placing entities; no coordinates in code. They are ordinary world text boards (the engine's CRT boards, `vr_text3d.cpp`), no second text system. With no arena running in the map (`ar_mgr`) the entity removes itself and makes no board. Text, two lines of one width: `WAVE   6  FOES LEFT   7` / `TIME 00:02 SCORE      0`. `BOSS` for `WAVE` on a boss wave; between waves the right says `WAVE 7 IN  5`, `CHOOSE A PERK` under the cards, `YOU FELL` or `CLEARED` at the end. Clock: from the run's first wave to its end. Foes left: `ar_foes_left`, set on the director's beat (a fall wakes it within 0.05 s). A board is only sent a new text when a number moved (about once a second); the compare values are `nosave`, so a load, which starts boards empty, writes them on its first frame.
- **Setting:** `vr_arena_banners` 1 (Combat > Survival Arena, "Arena Banners"): 0 empties the boards. New test builtin `worldtext_hgettext`.
- **The map:** the ceiling goes from 384 to 576 (+50%; 11.7 m to 17.6 m). Walls: the panels run 96..480 (3 whole tiles), upper rail 480..496, a new plated frieze 496..544, cornice 544..560, border 560..576; uprights stop at the upper rail, their lamps stay at 272..304. 306 brushes (was 290: the rail and frieze's 16); no spawn, item, flight, step, deck or tower moved. 4 boards, the middle of each cardinal wall, 34 out, z 464, scale 4.5 (914 x 158): top 543, just under the cornice. Coffer lights moved up with the ceiling, 300/0.47 to 305/0.40 (a longer reach; 350/0.47 matched the floor too, but washed out the walls beside them). Floor brightness in the same two window shots: 45.9 before, 44.5 now; the middle band 40.7 and 37.0. `vrarena_gen.py --check --compile --preset final --holes 400128`: 0 bad steps, 0 holes, 0 trim past the clip.
- **Tests:** `vr_arena_test 38`: `map vrarena`: 18 passed, 0 failed (check accepts `vrarena`, refuses `e1m1` and no name; each board's text read back from the server's list against the expected text: before the run, countdown falling, foes left falling, score rising, time running, off and on, wave changing); the same 18/0 after a save and a load. `map e1m1`: 8 passed, 0 failed (no arena, a placed banner removed, the map's text list empty). Existing, at the final height, all 0 failed: tests 4 (8), 7 (19), 9 (37), 11 (25, vrtesthall), 13 (12), 14 (9), 15 (130), 16 (9), 17 (24), 18 (4), 19 (7), 36 (16: the walkers' routes), `vr_arena_waves_test` 23, `vr_waveeditor_test` 28. Test 34 not run.
- **Frame cost: partly measured.** An LTO `--exclusive` run, floor view with 3 boards in sight, 500 frames, with the ceiling at the interim 512 and the same 4 boards: banners off 0.624 and 0.610 ms CPU, 0.612 and 0.625 ms GPU; on 0.643 ms CPU, 0.659 ms GPU, draw calls 142 to 152. About +0.03 ms CPU and +0.04 ms GPU, from one "on" sample. Not repeated at 576: timed runs are refused while the author's game runs. Worth repeating (`scratch/ab_time2.sh` in the worktree has the runs).
- **Images:** `C:/OHWorkspace/qvr-kit/scratch/arenabanners/`: `final_floor_south_looking_north.png`, `final_floor_corner_across_room.png`, `final_dais.png` (1024 eyes, by the script's `vr_mock_eye_size` in a low-impact run), `final_window_view_flat.png`, `ab_before.png` (the old room).
## Lightning burn marks

His note: thunderbolt and Shambler bolts should leave permanent burns on what they hit, like the flamethrower's; "might already
have been done". Half was: the thunderbolt's `particle2(boltEnd, LIGHTNING)` already laid a small round scorch (8-14 units, 222
dark pixels flat) that baked and survived a load. A Shambler's bolt sent no effect, so it left nothing.

**Now.** `VR_Lightning_Burn` (weapons.qc) from `LightningDamage` and `ad_LightningDamage`, so every bolt: shamblers, traps, AD's.
`lightningBurn` (vr_decals.cpp, `Preset::Lightning`): a ragged scorch baked at once (`MarkOptions::bakeNow`, the flamethrowers'
path), none in a liquid or on the sky. Tunables (Debug > Tunables): `vr_lightning_burn_size` 20 units (0 none),
`vr_lightning_burn_gap` 0.25 s, `vr_lightning_burn_stack` 3 on a spot among the last 24. Mjolnir's strikes and the other senders
of that preset get the same mark.

**Test.** `bash Misc/quakevr/burnmark_test.sh <agent> <outdir>` (`QC/vr_burnmark_test.qc`): the thunderbolt by `+attack`, a
shambler's `CastLightning` on a bare entity, flat and mock, `r_drawentities 0`, vrtesthall's south wall. Pictures before, 300
frames after the bolts, and after a save and a load; PASS at 60 pixels darker by over 18. `burnmark: 4 passed, 0 failed`. Next:
the Shambler's cases 0 dark pixels (FAIL), the thunderbolt's pass (flat 222, mock 667). Now flat 566 and 948, mock 2534 and 2303,
the same after a load. Held six seconds on one spot: 3 stamps, as at one second. Into the pool and at the sky: 0 stamps, 0 scorch.

**Open.** The same held beam lays a chip per `SPARKS` puff: 36 live chips in those six seconds. It was so before and is not
limited here; if they bake they count against the stamps.
## Pick Up a Second, smarter (branch `agent/ag2ndweapon`)

`vr_agrunt_dual_pickup` stays 0. With it on, going for a second weapon now has to be cheap (`VR_AG_SecondOk`, `QC/vr_armedgrunt.qc`;
used when picking the weapon and every frame of the trip). Tunables (`vr_tunables.inc`, Debug > Tunables): `vr_agrunt_second_range` 160 and
`_second_time` 4 s out of a fight; `vr_agrunt_second_combat_range` 72 and `_second_combat_time` 2 s with a live target in sight, and then
only a weapon the target can see from where it lies (the trip keeps it shooting). The trip is estimated as distance / 150 u/s + 1.3 s of
reaching down; time already spent on the way counts, so a trip that drags past the limit (or a target that comes into sight) is given up
(shunned `vr_agrunt_seek_shun`) and it goes back to shooting. A weapon taken away by someone else clears the trip as before.
A dual wielder recovering a lost weapon (`vr_ag_dual`) is the same function but skips all of this: unchanged.
Test 211 (`vr_agrunt_test 211`, `map vrtesthall`): idle 120 away takes it (1.9 s); idle 300 away leaves it; hunting, 110 away leaves it;
hunting, 52 away takes it (downtime not firing 1.9 s); taken away on the way, it gives up. 5/5; test 42 still 20/20.
Note: a freed entity keeps its `classname`, so a weapon taken by another grunt is only dropped from a trip by the "no nearer" check.
**Gun looks, the author's marked rivets (branch `agent/gunlooks4`):** on the tops as he dotted them: the shotgun 4 more (6 to 10: a pair where the receiver's top ends ahead of the sights, a pair behind them), the nailgun and lava nailgun 6 more (4 to 10: the top bar's ends, two pairs on the rear body's top edges); the top bar's head 0.12 high (the model's byte box ends just over the bar as it kicks) and riding the bar, a part that already moved; the check now wants them clear of the sights and their lines of sight; barrel masks 1876, 1188.

## Flat pose, phase 1: the sweep and the neck (branch `agent/flatsolve1`)

The plan's first of three (body-based hand target; stances by weapon come later and are not started). Docs: FLATPLAY.md, "His eye", "The neck", "The pose sweep".

**The sweep.** `vr_flat_sweep` (Debug > Reports) and `Misc/quakevr/flatsweep_test.sh`: -85 to 85 degrees in 5s, standing and crouching, axe, revolver, shotgun, hipfire and ironsight; three places (vrtesthall's open floor; his box against its far wall; e1m1 under the lowest ceiling a grid of traces finds, 47.9 over the floor). 560 rows, each written when the pose has stopped moving, 2-3 minutes a pass. `flatsweep_check.py` against `flatsweep_known.txt`: `flatsweep: 322 passed, 81 known failing, 0 failed` (65 lines for phase 2, 16 for phase 3). With the pivot off the same list fails 54 (23 rows not on it, the 4 camera rows among them; 31 worse than it says). The limits were not moved to pass.

**The number that says what he saw.** The screen's share first seeing a triangle of the collapsed neck and head (a grid of rays against the drawn body skinned on the CPU): 2.26-2.43% straight down standing, 0.35% at 75, 0 from 70 up. His `vr_flat_cam_up 4`: 0. That is the eye right above the neck's stump, 3.4 from it. The inside of the torso itself barely shows (0.17%), so "inner geometry" is mostly the stump. Seam leaks read 0.17-0.35% in places: the limit is 0.5%.

**The offsets**, long gun standing, then the crouched pistol's neck (the tightest):

| forward, up | at 85: forward, up, hidden %, body %, torso | at -85: back, up, torso, neck | crouched -85: neck |
|---|---|---|---|
| eye about itself | 0, 0, 2.43, 20.7, 6.1 | 0, 0, 6.0, 3.4 | 3.4 |
| `vr_flat_cam_up 4` (his) | 4.0, +0.4, 0, 6.8, 8.5 | 4.0, +0.4, 5.8, 3.2 | |
| 0, 4 | 4.0, -3.6, 0, 5.6, 5.8 | 4.0, -3.6, 1.8, **0.6** | |
| 0, 6 | 6.0, -5.4, 0, 4.5, 6.9 | 5.9, -5.4, **0.2**, **0.2** | |
| 2.5, 7 | 4.7, -8.8, 0, 5.6, 5.1 | 9.2, -3.9, 2.9, 2.9 | |
| 1, 5 | 4.1, -5.5, 0, 5.6, 5.1 | 5.9, -3.5, 1.9, 1.9 | **0.9** |
| **1, 4 (chosen)** | 3.1, -4.6, 0, 6.9, 4.5 | 4.9, -2.6, 2.8, 1.9 | 1.2 |

No forward offset leaves the eye on the neck's surface on a look up; more up drops it further on a look down. 1, 4 shows as much torso as his 4 (6.9 and 6.8% of the screen), and the sheets agree for standing hipfire: `camup4_after.png`, straight down, the same chest band and hand, with the floor nearer and a little less of the shoulders. Crouched in the ironsight they don't: cam_up 4 keeps the gun its size, this has it filling the middle of the screen (below). Most move 5.5; the box's side is 16 (11.1 left under the low ceiling and at the wall, 3.85 to the box's top standing, down from 3.97: the eye rises 0.1 at -5). Never in solid, the line from the head clear, in all 560 rows. `vr_flat_cam_up` can go back to 0; his 4 on top of this would add up.

**Off is off.** `vr_flat_eye_pivot 0` against the sweep run before the pivot's code existed: no column of 350 rows differs at all.

**Better with it on** (off -> on, worst): gun in torso, long hip standing 3.19 -> 2.30, the pistol's ironsight 2.45 -> -0.19; hand in torso 12 -> 7 stances failing; an arm's inside on the screen 9 -> 3.

**Worse with it on, known and listed:**
- The free arm through the eye, the long gun's crouched hipfire: the whole screen at -60 and -55 with the pivot off, at -75 to -50 and -20 with it on (the eye goes back towards the shoulder the stance brings forward). The check reads the worst, the same 100%, so it doesn't show this: it is the one to fix first in phase 2.
- The look straight down crouched, on a flat floor: the eye is 4.6 nearer it, so the gun meets it sooner. The ironsight's relief is pushed from 8.5-8.9 to 3 (pistol), 10.4 to 4.6 (long gun): the gun looks big on `before_after.png`, crouch down. The sights stay exact. The crouched pistol's hipfire at 80-85: the shot lands 15.6 off the crosshair's point (the aim's 12-unit near limit; `shot_deg` 0): was 0. Phase 2 sets the gun's height from the shoulders and is where this belongs.
- Crouched hipfire, the forearm and hand in the torso a little deeper: melee forearm 1.13 -> 2.50 at 35, pistol hand 1.58 -> 1.82 at 55 (the hands carry the eye's move down so the gun stays on the screen).
- The long gun's ironsight standing has no free hand on the hotspot at 75-85 any more (it was there; the arm can't reach the sight's new place). Off by the same most: 34.5.
- The free hand's snap (below) in the wall's ironsight rows at 25-40: not the pivot, the same with it off and the eye nudged the same way (`vr_flat_cam_fwd 1.9`, `_up -1`: 9.5 units a frame); `vr_model_collide`, `vr_body_collide` and `vr_hand_collide` off change nothing.

**What it says about phases 2 and 3** (after the pivot):
- Looking down, in the body: gun in torso up to 3.28 (long, crouched), 2.30 standing; hand 4.67; forearm 4.61. Still fully a phase 2 matter; the hip and shoulder never move.
- Crouched too close: the free arm goes through the eye on the long gun, crouched hipfire: the whole screen is the inside of his arm at -75 to -50 and at -20, the eye within 0.6 of its surface from -85 to 10. Gun in head 3.40 at 10, hand in head 1.63 at 15. This is the worst row, and the 2H stance is why.
- The 2H hand: on the shotgun's hotspot hipfire from -5 to 85 standing, -20 to 85 crouched, 28.6 off it above that, moved 3.1-3.8 units a degree where it lets go. Ironsight: crouched 60 to 85, else off by up to 34.5. A revolver has no hand on it at any pitch (19.9-27.4 off): his "pistols should be held with two hands".
- A snap: at pitch 10 standing (11-17 crouched) on the long gun's hipfire the free hand doesn't settle: 2.05 units a frame, the same with the pivot off. It sits where the look lets go and the real grip takes over.

Flat suites, each with the pivot on and off, the same verdicts: flatplay 28/0, flatpose 21/0, flatsight 12/0, flatkeys 28/0; none re-based. Two things on the way:
- flatpose failed one line once with the pivot on (`the eye nudged, a knight 300 away`: the crosshair on something 0 away, the eye nudged 12 forward), in a low-impact run beside his game. It passed on the rerun in a normal run (271 away), and the knight's distance on that line moves between runs anyway (28, 42, 43 on eye-near). Not explained; the line is a level look, where the pivot moves nothing.
- flatsight's read 8 frames after a jump to 85: rear sight 0.44 off the ray with it on, 0.002 off; settled 60 frames later, 0.0003. Inside its half-degree limit. So the sights trail a little on a very fast look down, and are exact once it stops.

To look at: straight down standing, then crouched in the ironsight on a flat floor (the big gun above); a look up with the shotgun crouched (the arm). Set `vr_flat_cam_up` back to 0 first.

The sheets are composed from a run's own frames: run `flatsweep_test.sh` with its sheet argument, as `flatpose_test.sh` clears the folder's pictures (`-Clean`) and a later suite leaves nothing to compose. `flatsweep_sheet.ps1` now stops on a missing frame; it used to save black cells.

Checklist lines: "Flat: look straight down, standing, each weapon: you see your chest from above and no neck stump, as with cam_up 4; cam_up back at 0." "Flat: look level, walk and jump: the same height and feel as before." "Flat: crouched, ironsight, straight down at the floor: is the big gun acceptable until phase 2?"
## Magazines sliding home and out (branch `agent/magslide`)

His note: a seated magazine should slide the last bit home, right from every direction and with the gun tilting; the nail
pistol's, which goes fully inside the handle, also in reverse for the ejection, then dropped with the hand's velocity but
mostly popping outward; tunables for the times and speeds.

**Seat (every magazine gun in a hand).** Drawn only; the count, sound and flag land when they always did. The client
(`vr_view.cpp` `MagSlide`, `magSlideStep`) sees the gun's magazine come in with the same gun (`STAT_QVR_WEAPONUID` and its
model) and draws it from `vr_nailpistol_mag_travel` (5.25 model units: the grip's bottom, its load point) or
`vr_reload_mag_seat_travel` (1.5) out, easing to a stop at home, over `vr_nailpistol_mag_seat_time` (0.12) or
`vr_reload_mag_seat_time` (0.07). A lying gun's magazine seats at once, as before.

**The slide is worked out in `setMagazine`**, in the gun's model space, through the gun as drawn that frame
(`gunSpace.point(seat + axis * slide) - gunSpace.point(seat)`): no state in the world, so no lag however the gun moves, and
the off hand's mirroring and the weapon's scale come with it. The axis (`magOutAxis`): the nail pistol's from home to its
load point (the grip's 12 degree lean); the others' from the seat to the magazine's middle.

**Eject (the nail pistol's button only: `VR_Reload_MagOutStart`, `vr_reload.qc`).** The clip, the no-magazine flag, the
sound and the haptic at the press, as before. The rounds wait on the player (`.vr_mag_out*`), `STAT_QVR_MAGOUT` says to
draw it sliding out (`entities.magOut`: a separate entity, so that everything treating `entities.mag` as seated, the
collisions, the hand's wrap, stays right) over `vr_nailpistol_mag_eject_time` (0.14), from rest, gathering speed. Its box
is still sent, slid with it (no grip or hotspot on it), and `VR_Reload_MagOutRelease` makes the round there, with
`vr_nailpistol_mag_eject_speed` (120 u/s) along the way out plus `vr_nailpistol_mag_eject_hand` (0.6) of the hand's velocity,
at most 0.45 of the pop (so outward wins whichever way the hand goes). The client reaches full travel a frame before the
server lets go, and holds there if the server is later. The stat clears in the update that has the round: no gap, no
overlap, and it cannot stick.

**Relation to `vr_nailpistol_slide_delay` (0.175):** none in the code: the rack still waits that long from the seat, which
lands at the same moment. The seat's 0.12 is under it, so the magazine is home first; a longer seat time would rack before
it arrives (said in the row's help). The rack's timing is unchanged.

**Interrupted:** the gun leaving the hand (`.vr_mag_out_inst*` differs), the player dead, a new magazine seated: free there
and then (`VR_Reload_MagOutFrame`, `VR_Reload_SeatMagInto`). Other ejects (a knock, a pull with `vr_nailpistol_mag_pull`, the
ammo changing) stay immediate, as does every other gun's button.

**Test:** `vr_magslide_test 1` (`vr_magslide_test.qc`; Debug > Tests > Nail Pistol) on any map: `run.sh <name> -Script
"developer 1;map e1m1;wait60;vr_magslide_test 1" -QuitOn "^magslide: [0-9]+ passed"`: 63 passed, 0 failed. The engine measures
the drawn magazine in the gun's frame each frame (`vr_test_magslide*`), the gun turning 3-4 degrees a frame on all axes, in
each hand: 0.000 units off the line (limit 0.02); seat from full (2.65 world units) to 0, at most 0.58 a frame (limit 0.77);
out from 0 to full, at most 0.55 a frame (limit 0.73); the loose one 0.07 units from the magazine last drawn, 0.96 with the
hand moving at 69 u/s (limit 1.5); the pop 120 u/s, 130 along the grip with 41 from the hand (inherited to 0.0 u/s); against
a fast hand 66 along the grip to the hand's 54. Nailgun: 0.81 to 0 units in 7 frames; its button free at once. Times 0: no slide.
Also `vr_np_test 1` 39 passed, 0 failed; `vr_reload_test 9` 77 passed, 0 failed. Not run: `reload_test.sh`, the mock suite (no
`--only`; it never ejects a nail pistol).

**Pictures** (`C:/OHWorkspace/qvr-kit/scratch/magslide/`: `side_crop`, `inner_crop`, `tilted_crop`, eject then seat, and
`handoff`): the magazine stays in line with the grip from each side and tilted, the foot plate leaves flush and comes back flush, nothing
pokes through the grip, the loose one carries on as it sat. They use the new `vr_shot_hide` bit 8: the hands hidden, what they
hold still drawn (the hand wraps the grip otherwise). Low-impact runs, so low-res. One or two of the runs' screenshots show the head's
view for a single frame at a random moment, the magazine's state unconnected (a run without the button is clean, a repeat of the same
run has it elsewhere or not at all): it looks like the low-impact screenshot's readback, not the game; worth a look only if it shows in VR.

**Checklist line:** Nail pistol magazine: B/Y slides it smoothly out of the grip, then it pops outward, carrying some of your hand's motion; a new one slides home up the grip, and the other magazine guns seat with a short slide; all right with the gun tilting and turning. {test:nailpistol}
