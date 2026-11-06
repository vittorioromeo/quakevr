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

## Armour round four's loose ends (branch `agent/armorcheck`)

- **The tests that timed out with nothing printed** (`vr_agarmor_test 1`, `5`, `7`, `vr_pp_test 2`, `3`): two causes, both
  there before round four. (1) A script `map x; vr_agarmor_test n` (no wait between) asks in the level's first frames, before
  the player is in: the request was taken (the cvar set back to 0) and dropped, so nothing ran (`vr_pp_test`: run with no
  player). Now `VR_AgArmor_TestFrame` and `VR_PP_TestFrame` keep the request until a player is there. (2) `vr_agarmor_test 5`
  printed through dprint: without `developer 1` not even its verdict showed. Now broadcast (`VR_TEST_LOUD`) like the others.
  With `wait30` and `developer 1` every one ran on both builds. Now, no waits, every one ends in about 10 s on its verdict.
- **Results, now (before round four, d81eb28fa):** `vr_agarmor_test 1` 26/0 on e1m1 and vrtesthall (26/0); `5` 24/0 on both
  (e1m1 23/1, its old flaky "dead ragdoll: 23 of 24 points read as their own piece"; vrtesthall 24/0); `7` 14/0 (14/0);
  `vr_pp_test 2` 5/0, `3` 5/0 (5/0, 5/0).
- **`vr_agarmor_test 5` on round four's build read 23/24 on both maps, every run:** "the corpse's right thigh and shin worn as
  their own pieces": the thigh's plates now reach up over the hip, and the middle of their outer face (limbpiece 14) lies
  nearer the pelvis's vertices than the thigh's (`limbAt`: the nearest bone vertex), so a hit there wears the torso. The
  test strikes the thigh at its plates' knee end (limbpiece 13: joint 17). In play: a hit on the upper half of a thigh plate
  wears a corpse's torso, not its leg (left as is).
- **The skins from the real generator:** Blender 5.2's bundled Python has numpy 2.3.4 (4.0's 1.23.5; the plain Pythons none).
  Proof it matches: the pre-round-four `make_agarmor.py` under it gives the pre-round-four files byte for byte. The current one
  differs from the stand-in's: 200-240 texels a skin, all in the joint block; the relief's heights all over the atlas (the old
  helmet brackets' block held the deepest texels, so with it gone every plate's height stretches: new = 1.083 x old - 21,
  within 1.2). Committed. Run it as `"C:/Program Files/Blender Foundation/Blender 5.2/5.2/python/bin/python.exe"
  Misc/quakevr/make_agarmor.py`.
- **The death knight's 240 more triangles, timed:** bench `armour_dknights16` (new: 16 armed death knights in full yellow,
  standing in view, vrtesthall), LTO, exclusive, 90 Hz, 5 reps each, d81eb28fa against 6afe7541d: CPU busy 2.006 -> 2.042 ms
  (+1.8%), GPU eyes 1.831 -> 1.901 ms (+3.8%). Rep to rep: CPU 1.95 .. 2.02 old, 1.94 .. 2.30 new (two reps with hitches);
  GPU 1.83 .. 2.00 old, 1.83 .. 2.06 new. Inside the noise; at most 0.07 ms for 16 of them.
- **The loosened checks, each broken on purpose once:**
  - The look check's death knight rest limit (0.8): his least at rest is his shin, 0.82, not the elbows the note named (his
    arms 1.00). With the elbow's and knee's plates 6 units short of the joint his upper arm reads 0.38: caught. On the grunt,
    enforcer and knight the same break isn't (the sleeve lengthens and covers the gap: no body shows); the player's bodies are.
  - Pieces under 8 vertices not measured: with every piece measured the one that fails is the death knight's upper arm, 0.75
    (3 of 4) at rest; his thigh (2) reads 1.00. Everything else is measured as before: the breaks above and below caught on
    every wearer.
  - `vr_agarmor_check` leaving the sleeves out: plate triangles dropped (one in 53) -> 101-120 open edges, "each shell
    closed" FAIL on all four; the plates' feet 2 units inside the skin -> "the rest pose" FAIL on all four. The sleeves sunk
    1.5 units into the body pass `vr_agarmor_check` (20/0) and `vr_agarmor_pop_check` (10/0) but fail the look check on every
    wearer (24 failures): the look check is what guards the sleeves.

## Armour round five: his annotated screenshots (branch `agent/armor5`, partial)

His notes 2026-11-06 (pictures 12-18, yellow). `vr_agarmor.cpp` (`planGrid`, `fillRadii`, `mirrorRadii`, `sleeveOf`, `buildShell`), humanoids only
(the player's plates unchanged: `PieceFrame::ends` is false on the skeletal bodies).

- **Hands and feet bare:** the humanoids' hand and foot skin weighs on the forearm and the shin far past the wrist and ankle (grunt shin region
  to 14.0 on an 8.7 bone, forearm to 9.8 on 7.0), so plates covered the hands and wrapped the feet below the sole. Now a forearm's rows end
  `plateWristAbove` 0.9 short of its bone's end, a shin's `plateAnkleAbove` 0.6; the limb's own vertices more than 0.3 past its rows no longer
  push its end rows out (the joint end of an upper arm or thigh 1.0). The look check's coverage skips skin past a forearm's or shin's last
  row (+0.2).
- **Slimmer:** `plateRidgeLimb` 0.5 on forearms and shins, `plateRidgeThigh` 0.8 (was 1.1; pauldrons and cuirass keep 1.1). A thigh's rows
  start by `plateHipFrom` 0.12 of its bone at the latest; its rings above its region (an enforcer's and a death knight's thighs weigh on
  it only near the knee) take the whole skin's exit (`bodyExit`) instead of the next ring's radius. Death knight thigh rows 3.2..5.9 -> 0.9..5.9.
- **Shoulders symmetric:** `mirrorRadii` modes: the player's as before; a humanoid's upper arms always mirrored, half way where they differ
  (death knight top ring 4.6 / 1.6 -> 3.1 both); a humanoid's other limbs corner by corner (alike: the larger; else each its own; mirroring
  everything half way sank a knight's and an enforcer's forearm plates into the skin: coverage 0.47 / 0.67).
- **Sleeves:** `sleevePast` 0.8 -> 0.3 (the two halves overlapped); a humanoid's rings eased round (each corner half itself, a quarter each
  neighbour, within 0.85..1.25 of the ring's mean, never more than 0.3 in), its per-vertex push `sleeveRidgeHumanoid` 1.6 (the slimmer end
  rows no longer cover the knee's and elbow's points: the sleeve does).
- **Numbers:** world mirror distance (left's outer face against the right's mirrored about y 0, `vr_agarmor_info <model>`), before -> after:
  grunt upper arms 2.02/1.85 -> 1.75/1.62; death knight 2.60/2.50 -> 2.61/2.66 (rest poses differ; the radii now equal). Grunt bulk
  (`vr_agarmor_pop_check progs/vrgrunt.mdl`, faces off skin, mean): torso 1.33 (1.33), upper arm 1.99 (1.89), forearm 0.89 (0.88), thigh
  2.72 (2.94), shin 1.29 (1.43). Triangles: grunt 2 920 -> 2 632 (no timing: fewer).
- **Tried and dropped:** a higher reach over a humanoid's shoulder (1.8-2.2, for the knight's red pad): it flared the enforcer's pauldrons
  into shells, the pad still showed. A ring's corners clamped to 1.35-1.6 of its median: cost the enforcer's forearm coverage. A limb's ring
  middle from its own rows' skin alone: the grunt's coverage bent fell to 12%.
- **Second pass (look check 111/3 -> 114/0, limits unchanged):**
  - Grunt thigh coverage 75% -> 88%: a humanoid's sleeve's rounding eased corners in by up to `sleeveRoundIn` and bared the knee's point
    the ridge pass had covered; the ridge pass (`sleeveOf`'s `ridgeOut`) now runs again after the rounding.
  - Enforcer bracer_r 8.37 over: two causes. The hand's vertices past the bone's end set the end ring's middle (the axis band now stops
    at the bone's end), and half way down his forearm weighs on the upper arm and the hand too, so the region's own vertices were a
    sliver and the plates sat 1 unit to one side (-4.8..3.0 over an arm -3.7..3.9). A humanoid's forearm's and shin's rings are now
    centred on the whole skin's exits (as a sleeve's), a corner no region ray finds takes the whole skin's exit (`fillRadii`, as the
    thigh's): plates -4.4..4.4. The width check measured that row against a 0.2-unit sliver of skin: where the region's slice is under
    1 unit, it takes the skin weighing 0.3 on the piece (`lookFit`).
  - The thigh's top rings the same where its region's slice is a sliver (an enforcer's thigh weighs on the hip only at its front: the
    rings 3 units forward). Pop check bulk, thigh mean: enforcer 3.51 -> 2.28 (it failed the 3.2 limit after the first pass; 1.97 before
    round five), grunt 2.72 -> 1.25.
- **Death knight's face:** guards end higher (jaw 0.42 -> 0.48 of the head), front edges further apart (0.37/0.44 -> 0.40/0.48), his
  collar row lower (`Wearer::yoke` 0.5, the others 1.2). Moving the guards' front edges back (0.54/0.42 -> 0.46/0.36) opened the helmet
  (14 rim edges, `vr_agarmor_check` "each shell closed"): kept.
- **Knight's shoulder pads:** a humanoid's two upper arms start as high as the higher (`PieceFrame::topAt`; his right weighs on its bone
  only from 1.4 down, so it started 1.7 lower); the knight's pauldrons go out over the skin round them by 1.8 (`Wearer::shoulderRidge`,
  plateRidge 1.1 elsewhere) and mirror the larger (`Mirror::ShouldersMost`; halving undid the pad side's push). His right pad's 8
  vertices all under plates (a ray along each normal; one bare before). Upper arm bulk 2.28. The enforcer untouched (per kind).
- **Death knight's shoulders:** both upper arms round one axis, the left's and the mirrored right's mean (`Wearer::shoulderAxis`: his
  arms rest differently), plates from 5 up the bone at most (`shoulderTop`; 10.8 before, his shells' reach): mirror distance 2.61 -> 2.04.
- **Open:** in the sheet, a sliver of the knight's red pad still shows at the top of each shoulder from front and back (the pad's
  upper faces, at grazing angles). The death knight's top ring now cuts his shoulder shells at their widest (radii 6.7/7.0/7.4/4.3).
- **Sheet:** `vr_pp_test 15` (vrtesthall, `god; notarget` first): each kind front and back, hands and feet close to, a corpse's legs
  (`ppshot: done`). `scratch/r5_before.png` (origin/next), `scratch/r5_after.png` (low-impact shots: his game was running).

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
## The arcs on your arms on a flat screen

His note: struck by lightning, the arcs on the player's own arms never show in flat-screen.

**Why.** Three things, each hiding them on its own. `drawStruck` and `armArcs` queue them with `lines::line`, drawn only by
`stereo::drawUi`, the headset's UI pass: on next's drawing `vr_shock_self_info` says 17 arcs and the frame has 0 blue pixels. Moved to
the scene pass (`lines::sceneLine`, the water's fix), the gun held then hides nearly all of them: 0, 291, 0 and 76 pixels in the four
cases' frames. And `ViewKeep`, a headset's comfort, dims them below the test's threshold on hands this close to the eye (whole
frames at 0); the hand's arcs also reached to `s.pos`, about 10 units from where the stance's free hand is drawn.

**Now.** Flatscreen play only (`flatplay::active`; a headset takes the old branch everywhere, with the same random numbers):
`lines::sceneOverLine`, in the scene's translucent pass with no depth test, what "drawn over all" is in a headset; no `ViewKeep` on
them; the hand's arcs reach on from the wrist the body draws. Tunables (Debug > Tunables): `vr_shock_self_flat_arcs` 3 (more on the
hands and forearms: a flat view shows one hand and little arm), `vr_shock_self_flat_hand` 0.14 m. `armArcs` is shared, so the shock
in water and Quad's arcs show on a flat screen too. Still not drawn flat: `drawSelf`'s arcs round the body and in front of the eyes.

**Test.** `bash Misc/quakevr/sa_shockarcs_test.sh <agent> <outdir>`, ends `shockarcs: <n> passed, <m> failed`: pictures through a
shambler's real bolts from behind (`CastLightning`, `QC/vr_shockarcs_test.qc`), on and off (`vr_shock_self_time` 0.8 and 0, the blue
light off in both), five frames each, as the arcs are reshaped every frame. Lower blue pixels, vrfiringrange: level 5870 4272 2430
4224 10029; looking down 3690 3154 2995 1779 9360; crouched a little 11596 8874 933 23166 20252; the look still swinging 3846 2565
1943 1963 8584. The same stage with the flat draw off: 0 in all ten frames of two cases. Looked at: on the hand and the gun in each
of the four, mid swing too. The main hand in the view (forward, right, up) moves little as the look goes: 2.7 3.2 -3.2 level, 4.8 5.2
-4.4 at 35 down, 4.2 6.4 -5.5 at 50. The mock headset: `SA_EXTRA="vr_mock_look 50 0;" ... --modes mock --cases 1`, 5 of 5 lit (looking
level its hands are below the view). The shock in water by `vr_shock_test 0`, flat, flash off: 5525. `waterzap_test.sh` flat and mock,
cases 1 and 4: 4 of 4. The arcs near the eye are wide (a line's width is in world units): a broad smear at the shallow crouch's elbow.
Pixels and frames from the arcs over the scene on were taken low-impact, his game running.

**Open: the full flat crouch.** With a weapon held, `+flatcrouch` at the default drop (18) puts the gun and the hands behind the eye
and a huge arm across the view, with no arcs on it (0 pixels in five frames): main hand 2.7 3.2 -3.2 standing, -5.1 -0.5 2.3 crouched;
`vr_flat_stance 0` gives 4.4 5.8 -1.7, so the stance's pull back and the crouch add up. `vr_flat_arm_hold 0` changes nothing. This
fix doesn't touch the pose, seen with the thunderbolt only. So the test's crouch case uses `vr_flat_crouch_drop` 8.

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
Arcs): their width, 0 none (the blue light stays). Not drawn on a flat screen still: the shock's arcs in front of your eyes and
round the body (`drawSelf`, other `lines::line` users; the arms' were fixed after: "The arcs on your arms on a flat screen"). A hammer slapping the surface with nothing solid is still no blow.

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

### Model FOV, second pass (agent/flatvm)

His notes at fov 70: the heavy revolver stayed at model FOV 70; with a model FOV above the world's, a horizontal line with the viewmodel darker below it, every weapon.

- **Revolver:** in a hand it is drawn as its parts (`rvFrame`, `rvCrane`, `rvCyl`, `rvShell`: setupRevolverParts), and `view::drawGroup` had no range for them: VR_DRAW_OTHER, so `VR_FlatModelFovEntity` left them at the world's FOV (the hand under it shrank). Now VR_DRAW_HELD; a `static_assert` checks the ranges count every member of `Entities`.
- **Dark line:** the alias shader reads the light tiles at a part's true place (as it should), but a model FOV wider than the world's draws on the screen parts whose true place is off it: the tile coordinate is out of range, and an out-of-range `imageLoad` reads no lights. The line is where the true place leaves the screen (at s = tan(fov/2) / tan(model fov/2) of the way to the edge). `ModelDynamicLights` (vr_glsl_alias.h) now takes every light off the tiles, each tested by its reach as before: exact, and only those pixels pay.
- **Test:** `bash Misc/quakevr/flatfov_test.sh <agent> [label] [wids]` (flatfov_check.py): fov 70, the world hidden, `_near` 200 / `_far` 201 so the model-FOV-M frame is the 70 frame shrunk round the middle; 21 weapons (1-18, 29-31) at 70, 90, 110, the chest flashlight on and off. Size: IoU against the shrunk 70 frame, at least 0.80 (revolver before 0.575 / 0.393, after 0.91 / 0.87; the lowest 0.85, the nail pistol at 110). Light: the share of lit pixels past the line against inside, at least a quarter (before: 0.000-0.003 lit past the line on every judged weapon, 0.6-1.0 inside; after 0.50-1.00 past it). Before 42 passed, 38 failed; after 80 passed, 0 failed. The nailgun (inside barely lit) and crowbar (too few pixels) are not judged for light.
- **Suites:** flatsweep 397 passed, 13 known, 0 failed; flatplay_test.sh 31 PASS, 0 FAIL (exit 0).
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

## Arena banners: the gadget's layout (branch `agent/banners2`)

The author, having seen the text boards: "too big, please reduce their size by roughly ~35% and keep them close to the ceiling"; "the internal layout ... is not easily readable ... Can you use the same system as the wrist gadget HUD to provide nicer layouting even in world banners?"

- **It could:** the gadget's screen is a 2D layout drawn into an offscreen target (`gfx::begin2D`, `drawText`, the status bar's numbers, `fill`), then a quad through `Shade::Screen`; the map's boards were already an offscreen image on such a quad, depth tested in the world. So a board only needed a different image.
- **Panel boards:** a world text whose text starts `{arena:` is not shown as text (`worldtext::isPanel`). The QC writes `{arena:wave boss show value total seconds score}` when a number moves; it travels, replays to a spawning client, goes into a demo and survives a load as any world text. `Quake/vr/vr_gadget_panel.inc` (part of `vr_gadget.cpp`, beside the hologram and the frame rate) lays it out with the gadget's own `paletteAt` (not dimmed with the gear lights), `drawText`, `numberRun` (split out of `number`; also narrows, and takes the new `num_colon`) and `meterFrame`: no copy of any of them. One image for all the boards, as they all say the same, 1500 x 240, redrawn when the text or the look changes (about once a second), freed when no board wants it. `vr_text3d.cpp`'s boards keep their box, glow, cull and CRT (`vr_worldtext_crt`, which no longer turns these into plain text); a panel board has no page-turn glitch, which the text boards fired on every text change, so 0.3 s of every second of the old banners was a glitch.
- **The layout** (250 x 40, the gadget's 8-pixel labels and 24-pixel numbers): `WAVE` big number | `FOES LEFT` big number with a bar of what's left | `TIME` and `SCORE` in 15-pixel numbers, one over the other, narrowed only if the score needs it. `BOSS` in the warning's red on a boss wave. Between waves the middle says `NEXT WAVE` and its seconds (red for the last 3), or `CHOOSE A` `PERK`; `--` before the first wave. At the end the wave's tile and the middle give way to `YOU FELL` (red, red frame) or `CLEARED` under `WAVE n`. Colours: the gadget's, so the player's hue (`vr_gadget_screen_hue`), not `vr_worldtext_hue`.
- **Size:** `worldtext_scale` is now units a pixel of the face, default 2.2: 550 x 88, 566 x 104 with its bezel (was 914 x 158: 38% and 34% smaller). A big number is 53 units. `BANNER_Z` 464 to 491: the top at 543, still just under the cornice at 544. Map rebuilt: `vrarena_gen.py --check --compile --preset final --holes 400128`: 0 holes.
- **Tests:** `vr_arena_test 38` now also reads the client's image (new test builtin `panelboard_read`, answering only once that image holds the board's text, so it has come through the real message and draw): the tile that moved changes and the others don't, at the countdown, perk choice (the director really held), fight, foe down (bar falls), time, off (image freed) and on, wave change, `YOU FELL` (red), `CLEARED`. `map vrarena`: 46 passed, 0 failed; the same after a save and a load; `map e1m1`: 8 passed, 0 failed. It now ends on its verdict line (`-QuitOn "test 38 over"`). Test 34 (`vr_arena_waves_dir arena_waves_test; vr_arena_waves_fixture; skill 1; vr_arena_assign e1m1 1 fixture_othermap; map e1m1; developer 1; vr_arena_test 34`), not run since its check line changed: 3 passed, 0 failed.
- **A real frame, banners off against on:** headset, 720 eyes, from the floor: 3707 pixels differ, in the north board and the side boards; from behind a tower the two middle columns of eight stay 0 (depth correct); `vr_worldtext_crt 0`: the same 3712; flat (`vr_enabled 0`, 960 x 540): the board's two columns 1777 and 1787 pixels (the rest is the view weapon's bob). Flat draws the image from `VR_End2D`'s `text3d::renderScreens`, the existing path for boards. Not the UI line pass of the earlier bug.
- **Debug:** `vr_panelboard_dump [name]` (Debug, Save to Files, "Arena Banner").
- **Frame cost, measured:** LTO, `--exclusive`, real clock (frame times are what is measured), 2048 eyes, paced at 90 Hz, 3 boards in sight from the floor; off and on alternating in one session, 8 pairs of 450 frames, each pair compared (on minus off, the noise limit twice the standard error): GPU (eyes) +0.001 ms, +-0.009; CPU busy +0.031 ms, +-0.125, so no CPU cost could be told from the noise, and about 0.15 ms is the most the run allows; draw calls +10 (+-1.2). The noise on CPU is that wave 1 came during the windows (`vr_arena_wave_rest 99999` did not hold it), with 2 foes up: the image was redrawn once a second, 67 draws in all, as in a fight. Off to on, not old to new: the old boards' cost was only ever measured once (above).
- **Images:** `C:/OHWorkspace/qvr-kit/scratch/banners2/` (2048 eyes, the game closed): `floor_south_looking_north.png`, `corner_across_room.png`, `perk_choice_floor.png`, `flat_window_view.png`, and the image itself, `panel_image_fight.png`, `panel_image_perk.png`.

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
## Flat pose, phase 2: the hands on the body (branch `agent/flatpose2`)

Phase 2 of the three (stances by weapon type are not started). Docs: FLATPLAY.md, "The hands stand on the body". His notes on phase 1:
straight down the gun went through his body; straight up the arms stayed out in front instead of over the shoulders; crouched, the
gun went into the camera; the ironsights were fine.

**What was wrong, measured.** The pose stood on `hands::bodyAnchor` with `vr_shoulder_offset`: 4.7 units *above* the eye and 12 above
the shoulder the arm hangs from, and crouched 5.6 higher still against the eye and 8.6 further back (it tips back with the crouch).
So at a level look the gun sat at real shoulder height; turning that offset with the view put it behind the chest looking down
and far out in front looking up; crouched, the long gun's hand ended up 5 behind the eye (-5.4, -0.5, 2.3 forward, right, up of
it: the coordinator's thunderbolt numbers were the same), with the free arm across the screen.

**The solve** (`vr_flat_body_pose` 1, Debug > Tunables > Flatscreen > Body Pose; 0 puts the old pose back, off the old anchor):
- The hand stands off the shoulder the arm hangs from (`avatar::restShoulders`, the clavicle at rest: `clavicleRest`, shared with
  `solveArm`). Gun Forward / Down / Out and the free hands' numbers were rebased onto it: 14 / 12 / 4 -> 16.5 / 0 / 1 (empty 7.5 / 1 / 4,
  support 8.5 / 3 / 3), so a level look stands where it did, within half a unit.
- The arm turns about its shoulder with a share of the look (0.7 down, 0.85 up), out at its side looking down (3) and up (2.5).
- The chest takes 0.3 of a look down and 0.2 of a look up (half of it in the spine), and carries the head with it (`vr_flat_eye_lean`
  12: the eye goes 3.1 further forward at 85 down; without it the neck's stump came into the view again, 3.47% of the screen).
- The crouch puts the shoulders 3 further under the eye and 2 forward (the forward part fades with a look down), takes 0.6 of the
  stance's pull, and puts the gun 2 further out.
- Smooth limits, each eased in (no corner at any look): the stance's hotspot within the free arm (0.87, across the view then along
  it), clear of the chest (4 ahead of the shoulder below it), of the eye and the neck under it (3.5), on the screen (55 degrees, 75
  for the long gun's hand by his cheek), within the arm (0.92).
- View-fixed tweaks on top, `vr_flat_gun_view_fwd/_right/_up`, 0. The eye's own nudges carry the hands as ever; the neck's turn no
  longer does (it moves the eye off a body that stays).

**Three things the sweep found on the way.**
- *The shot near a surface.* With what the crosshair is on nearer than 12 past the muzzle the gun pointed along the ray from where
  it stood: the shot landed as far off the point as the muzzle stood off the ray (15.6 for the crouched pistol at 85; 9.9 at the
  wall). And a ledge the ray clears but the muzzle's line doesn't sent it 500 units away (e1m1). Now the shot's start eases onto the
  ray between 24 and 12 ahead, and goes from the ray with anything in the way (`shotFrom`; `keepMuzzle` hands it on): nothing drawn
  moves, the gun turns at most as far as 24 ahead allows. Every `shot_apart` row passes, the worst 0.03.
- *The shotgun's free hand never settled about a level look* (phase 1's "snap at pitch 10"): the cup was chosen, the stance brought
  the gun back, then the grip ahead was nearer, a real grip took the stance off, the gun went out, the cup was nearer... The grip is
  now chosen from where the free hand hangs off the gun without the stance at a level look (`flatplay::levelHang`), which no look
  changes; the shotgun keeps its cup and its look, as at the level look before.
- *The free hand let go in a frame.* Its need was measured from the shoulder it is posed on, which swings to its own hand: lower
  with the hand on the hotspot than off it. Now from the rest shoulder (hands on the body), and it eases off the hotspot from 0.78
  to 0.98 of the arm instead of switching at 0.92.

**Against walls.** A hipfire gun goes back no nearer its shoulder than 9 (was 6: the long gun's stock went into his chest at the
wall, 3.3 deep); an ironsight's rear sight no nearer the eye than `vr_flat_ads_wall_keep` 8 (was 3): the crouched gun at the floor
no longer fills the screen, its end goes into the floor and is hidden there. The sights stay exact (every row under 0.05).

**The sweep** (`flatsweep_test.sh`, the defaults; his settings, `vr_flat_aim_fov 45`, read the same): `flatsweep: 384 passed, 19 known failing,
0 failed` with the new list, against 322 / 81 before. Of the 65 phase 2 lines, 61 pass and are out. Left, by phase:
- 2: the long gun's free hand easing off its hotspot crouched, low ceiling and wall: 1.75 and 1.23 units a degree (were 3.90, 3.81;
  open floor and standing pass now). The gun crouched at 45 down: 1.008 units a degree against a limit of 1 (new: the arm's turn,
  the crouch's gun out and the hotspot's limit all move it there).
- 3: the off hand off its hotspot (all of them; a revolver has none to be on), and one row at 45 under e1m1's low ceiling,
  crouched in the ironsight, that doesn't settle within the sweep's wait (swing 0.01).

**Row by row** against the `before` rows at his settings (`scratch/fp2_compare.py`: each pitch of each stance and place, each check,
worse by more than the check's noise and past its limit): none but the phase 3 ones: `offhand_spot`, 213 rows (the hanging hand
stands elsewhere now, 3.6 further off its hotspot at worst: 34.5 against 30.8, crouched in the ironsight at -85), and `settled`, 2.
Worse but inside their limits, as you'd expect when the hands move: the eye's distance to the torso and neck (84, 45 rows), the
gun, hands and forearms nearer the body shapes (250 rows of gun in torso, all at least 0.5 clear), the arm's share (309: the hands
are further from the shoulders they hang from, all under 1).

**The sheets**, `C:/OHWorkspace/qvr-kit/scratch/flatpose2/before.png` and `after.png` (the run's own frames, `vr_flat_aim_fov 45`): stand down,
the chest from above with the gun out in front of it at the right and the free hand at the left, nothing through the body;
stand up, the axe and the pistol held up over the right shoulder, the shotgun from the right corner; crouch level, the shotgun at
the lower right where the inside of his arm filled the screen before; crouch down, the pistol beside his chest at its size,
where its ironsight filled the screen; the long gun's ironsight crouched at the floor is still large (its end goes into the floor
at 8 from the eye).

Flat suites: flatplay 28/0, flatpose 21/0, flatkeys 28/0, flatsight 11/1. The one: the moved picture (`vr_flat_ads_screen_x 5`, `_y -8`), whose sights lie on the moved ray to 0.0003 units, reads 0.028 degrees on its far-along measure against a 0.02 limit; with `vr_flat_body_pose 0` 0.0000. Not found yet: the look is level there, so neither the chest nor the arm's turn moves; the sight's solve is the eye's and unchanged. Open.

To look at: straight down and straight up standing with each weapon; crouched with the shotgun at a level look, then up and down;
the ironsight crouched at the floor.

**Follow-up (branch `agent/flatpose2b`): flatsight's "picture moved" row.** Not the pose: the measure. `offTheRay`'s angle was
`acos(dot / len)` in floats, and a cosine that near 1 only comes in float's steps: 0, 0.0198, 0.028 degrees, ... The sight line stood 0.079
units off the ray 4096 along, 0.0011 degrees, and read 0.028 or 0 by how its cosine rounded (so 0 with the body pose off, by luck). Now
`atan2(across, along)`, and `degreesApart` for flataim's and the sweep's shot angle, which had the same steps (flataim's 0.0198 jitter).
The limit is unchanged: flatsight 12/0, the row 0.0011 degrees, the shot 0.0000.

With `vr_flat_body_pose 0` (built in as the default for the run): flatplay 28/0, flatsight 12/0, flatkeys 28/0, flatpose 20/1. The one
is the control (`vr_flat_aim_point 0` with the picture moved: the shot must pass more than 8 off the point): 5.13 with the knight 40
away, against 9.23 at 60 in the run with the pose on. The miss grows with the range (0.13 and 0.15 a unit), and the knight stands
at a different range from run to run (phase 1 saw 28, 42, 43), so it reads as the range, not the pose; not re-run. Off is the old pose, not the whole of before: the shot's
start on the ray near a surface, the gun's stops against walls (9 from the shoulder, the sight 8 from the eye) and the measures above
apply either way; the grip's choice from the level hang, the eye's lean with the chest and the free hand's eased letting go are the body
pose's only.

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

## Embedded blades: his notes of 2026-11-05, firing range (branch `agent/embedb`)

`vr_embed` stays off by default. Numbers: Debug > Tunables > Combat > Embedded Blades (Axe, Moving the Body).

- **Tug (axe):** `vr_embed_hold_axe_tug` 5 -> **20** cm (his setting). And force: the hand's pull out, averaged over
  `vr_embed_hold_axe_tug_time` 0.15 s (`.vr_hold_tugv`), must be `vr_embed_hold_axe_tug_speed` 0.8 m/s as the distance is met;
  slower, the hands come off and the axe stays. Nearing it (from `vr_embed_tug_shake_from` 0.4 of the distance) the axe and the hand
  on it shake (`vr_embed_tug_shake` 2.5 deg, QC's `.vr_stick_shake`, the engine's place) and the buzz rises in pitch and strength.
- **Out into the hand:** the drawn hand eases from where it was on the blade to its controller over `vr_embed_free_ease` 0.25 s, the
  weapon with it (vr_view_hands.cpp embedDrawnHand: the blade's entity is gone at once, so the old 0.1 s ease never ran). Letting go
  eases the same way.
- **Ghost:** `vr_ghost_hands_ignore_embed` 1 (Player Appearance, "No Ghost on Stuck Blades").
- **Second hand:** drawn on the weapon's grip hotspot nearest it (every bladed weapon has one: its two-handed place), placed from the
  blade each frame (groundSpotPose from the handle hand's pose), not where its controller was as it took hold (the 20 cm).
- **Leeway:** a held blade turns with the handle hand's turn since it took hold, at most `vr_embed_lean_axe` 6 / `vr_embed_lean_sword` 3
  deg, any axis, about the wound, eased (`.vr_stick_lean`; `.vr_stick_base` keeps the angles easeOnBody eases from).
- **Arm stretch:** a hand on a blade lets its arm stretch to `vr_embed_arm_stretch` 1.3 (eased in and back over `_ease` 0.2 s); the
  avatar writes how far the drawn wrist is past it (`.vr_embed_armgap`), and past `vr_embed_arm_letgo` 1 cm more than the least it
  has been since it took hold, the hand lets go. Only an arm that reached the hand at some point (the tests' mock hands are held
  beyond any arm, so tests 7/11/13 are untouched by it).
- **Stuck handles:** `vr_embed_handle_free` 1: a blade in a body no hand holds, 4 times a second, its grip point against the model
  (hitmodel nearestAny): inside, it is turned about the wound out of it (+1.5 units clear), 15 deg a look, 45 all told (`.vr_stick_freed`).
- **Lifting:** the spring's force is at least `vr_embed_move_sword_lift` 0.8 to `_deep` 2 (axe `vr_embed_move_axe_lift` 1.2) times the
  ragdoll's weight (ragdolltug what 15: its parts' mass) under the physics' gravity (sv_gravity / units per metre, about 30 m/s^2: it
  used 9.81, a third of the weight, and lifted nothing), times Skewer.
- **A blade in a corpse (his decision on note 10, "Yes, like the axe"; agent embedb2):** the hand's pull out along the blade is averaged
  for every blade now (`.vr_hold_tugv`, over `vr_embed_hold_axe_tug_time`). In a dead body, a pull at least `vr_embed_hold_corpse_tug_sword`
  / `_axe` (0.8 m/s each, Debug > Tunables, Dead Body: Sword/Axe Tug Speed; 0: the old 1:1 always) is a tug: the arm's way counts 1:1
  as before (an axe then still needs its 20 cm). Slower, the body is carried: the spring takes the hand's way along the blade too
  (xAxial 0) and the blade doesn't slide out; while the spring is short of its most (`.vr_hold_strain` < 0.95) the hand's lead on the wound
  isn't counted; held back at its most, past `vr_embed_hold_corpse_slip` 20 cm the hand slips off and the blade stays ("the body won't
  come"). A tug's first moments, still averaging under the speed, carried the body: once it is a tug that way (a leaky sum over the tug
  time, `.vr_hold_carried`) goes back to the blade, as a tug from its start. Test 24 case 5 (a sword in a corpse lifted 30 units in 2 s)
  and the new case 7 (an axe in a corpse pulled out 30 units in 1.5 s, 0.63 m/s): rose 53 units, dragged 26.9-28.3, both held.
- **The regressions this branch brought (agent embedb2):** found by turning each new feature off by its cvars (all off: test 7 76/6,
  test 11 103/0, as origin/next). (1) The axe tug (20 cm, 0.8 m/s) failed two checks that tested the old numbers: test 7 case 5 "pulled
  a few cm" pulled 8 cm past the depth (the old 5 + 3), now its tug + 3 cm in 0.2 s; test 11 "tugged out, into the hand" pulled 30 units
  in 2 s (0.57 m/s, any speed then), now at 1.5 times the tug speed; its message printed "+ 5 + 2", now the cvars. (2) The arm let-go
  (`vr_embed_arm_letgo`) let go of a second hand as it took hold: the avatar's gap as it took hold was its controller's (in reach), and
  then drawn on the grip hotspot it was out of reach ("one hand opens: the other holds it still"; test 11's two-hand case 0.9 vs 1.0).
  Its least gap now counts only from `max(arm_stretch_ease, free_ease) + 0.1` s after taking hold (`.vr_hold_gapat`). (3) The lift
  floor carries one hand's weak push as far as two (test 11 cases 24/25 tune the sword's force down to 300 N to see the second hand's
  share): those two cases tune the lift to 0 as well. (4) "Sliding deeper sounds / spurts blood" (test 7 case 6) are not this branch's:
  seeded, all features off, they fail the same (seeds 1 and 2: 74/8). The push slides the blade under a unit (the old failure "pushed:
  it slides deeper"), so whether a 3 cm sound step is crossed depends on where the stab's depth fell; unseeded runs differ by start-up.
- **Flaky, open:** test 24's "out into the hand eased" (case 4, a hard axe tug) reads 2.56 cm in some runs, 3.58 in others (limit 3;
  the hand itself moves 2.78 cm a frame at 2 m/s), with the corpse rule on or off at the same seed: not this change's, a thin margin.
- **Test 7 case 7's stab that didn't stick (agent stabstick):** not the leeway. The failing stab never made a blade: VR_Embed_Try
  said "not yet, the blade as drawn 1.3 (another run 1.4) cm off its model" and the 0.12 s wait ran out (`embedhold: blow at
  monster_army not stuck: the blade 1.3 cm off its model`). The leeway only turns a blade already stuck and held. vr_model_collide
  holds a drawn weapon 0.3 units (about 0.94 cm) off a body, and `vr_embed_touch` 1.2 cm left a quarter of a centimetre over that:
  held stabs that stuck measured 0.7-1.2 cm off (median 0.8, p90 1.1) with the leeway on or off, the same; let through, the tail goes
  to 1.5. So **`vr_embed_touch` 1.2 -> 1.8 cm**. Unseeded test 7 runs: leeway on, touch 1.2: 4 of about 52 failed this way (1.3,
  1.4, 1.3, 1.3 cm); leeway 0: 0 of 30 (a 3-7% chance at the same rate, and the gap spread is identical, so I read it as luck);
  touch 1.6/1.8: 0 of 50. Open: at 1.6/1.8, case 21 (the body taken away; its grunt is not held still) landed 10-25 cm short 3 times
  in 50 ("stuck", 2 skipped), never in about 82 runs at 1.2: maybe a knock-on, not understood. Test 11 103/0; test 24 at 1.8: 22/0
  three times, 21/1 twice (eased; the corpse lift), at 1.2 the same kind (the corpse drag, the corpse lift).
- **Skewered gib:** the hand gripping it is drawn on the gib where it took hold (skewerDrawnHand), as a held prop's. Its grab already
  goes by the fist meeting the drawn model (carryreach -> held::grabTouch, 2 cm slack); without the jointed hand's fist it falls back to
  the box test, which may be what he saw.
- **Test:** `vr_embed_test 24` (`embedfeel: N passed, M failed`; now 7 cases, the corpse drag added). Engine numbers through
  `vr_embed_probe*` cvars. Test 21 gained the gib drawing check.
- **Before (origin/next) and after (this branch), low-impact runs, `map vrtesthall;wait30;god;notarget;vr_embed_test N`:**

  | test | origin/next | branch as left | after the fixes |
  |---|---|---|---|
  | 7 | 75/7 | 72/10 | 67/7 unseeded (case 7's 6 m/s stab didn't stick: "stuck", 8 checks skipped; also with only the leeway on, all else off); 75/7 seeded 1 |
  | 11 | 103/0 | 101/2 | 103/0 |
  | 24 | (new) | 19/1 | 20/2 unseeded (eased 3.58; the axe drag 5.2 units, held); 21/1 seeded 1 (eased) |
  | 1 | 4 failed, no verdict | 4 failed, no verdict | 57/4, with its verdict (the same 4) |
  | 21 | 39/2 | 40/2 | 40/2 (the same 2) |

## Embedded blades: his notes of 2026-11-06 (agent embedfix, branch `agent/embedfix`)

- **"Unsticking an embedded axe makes the hand lag behind."** The cause, as he guessed: the blade's entity goes at once and the axe is
  drawn in the hand again at its controller's pose (setupWeapon: `s.pos`, `s.visualRot`), while the drawn hand eased from where it was on
  the blade (embedDrawnHand, vr_embed_free_ease); and a hand holding a weapon took its turn from `s.rot`, so the hand's turned ease was
  dropped too. Now `embedPinStep` (vr_view_hands.cpp, each frame before the weapons) works out the ease once, as one rigid transform
  about the controller (`embedEase`: the turn, then the move), and setupWeapon (the weapon, before it is placed: its attachments and
  hotspots follow) and setupHand (the hand, after its weapon's pose) both apply it. An empty hand eases as before.
- **"Decapitation or a limb cut with the axe leaves it embedded where the part was."** A killing blow's cut happens inside the blow,
  before VR_Embed_BlowsDone sticks the blade; just killed, the body may not be a hit model yet and the engine falls back to sticking in
  its box at the contact (kind 4, "8.8 cm off its model"), in nothing, and the hand on it moved the ragdoll. Now VR_Embed_Try (vr_embedhold.qc)
  doesn't stick a blade in a body that had a head or limb cut off within the blow's wait (`.vr_stick_cutat`, set by VR_AxeStick_CutDone:
  a blow that cuts a part off went through it), nor where the part it would go into is cut off (VR_AxeStick_BoneCut: the bone's bit in
  ragdolllimbs 2, or a head bone of a headless body), checked again on where it went in; the blade stays in the hand. A stuck blade
  whose part is cut off by anything later comes free (VR_AxeStick_Think: into the hand if held, else it falls), on top of the cut's own
  CutPrepare/CutDone marks.
- **Decision:** not embedded in the severed part. The severed head or limb is a flying gib: an edge's blade held in one would be a new
  mechanism (the hold's spring and drag are a ragdoll's; skewering is a pointed blade's, along its line). The blow went through it, so
  the axe stays free in the hand. Ask him if he wants the gib to stay on the axe (as a skewer).
- **Test:** `vr_embed_test 25` (`embedfix: N passed, M failed`): 0 a killing axe swing at the head (beheads), 1 at a limb (cuts it off):
  no blade in the body, the axe in the hand, the hand drawn back doesn't move the body; 2 a hard axe tug: `vr_embed_probe_grip` (the drawn
  hand from where it holds the drawn axe, the most over the ease) and `_wstep` (the drawn axe's per-frame step from its controller,
  from the blade's last drawn place on) against the hand's `_step`. A blow that didn't land as the case wants (the mock swing sometimes
  turns side on, or misses the head) is dealt again, at most 3 times, said in the log. `vr_embed_probe_split 1` draws the weapon the old way.
- **Numbers (low-impact runs):** before (split 1): grip 31.1 cm, the axe's step 31.4 cm in one frame; after: grip 0.00 cm over 18 frames,
  the axe's step = the hand's (3.42 cm at 2 m/s, the 20 cm tug's eased offset). The cut: the first run, with only the bone check, had
  the limb blow (limb 7 cut, the blade "in" bone 6, kind 4) stuck and held, the body dragged 30 units; without the blow check it
  happened in 1 run of 5 (4 more with the QC checks off: none; timing, whether the dead body is a hit model yet); with it, 0 of 4. Test 25: 13/0 three times. Test 7:
  75/7 seeded 1 (as origin/next), 76/6 and 74/8 unseeded (the known stab/slide ones); 11: 103/0 seeded and not; 24: 21/1 seeded 1 (the
  known "eased" 3.59 > 3 thin margin), 20/2 and 21/1 unseeded (the known corpse lift/drag); 21: 40/2 (the same 2).
## His notes of 2026-11-05: nailgun muzzles, crate sounds, ledge shove zones, grab ease (agent notesb)

**Nailgun muzzles.** "The sparks come out of the center between the two muzzles, not from the muzzle that's currently firing."
The nailguns' firing frames (1..8) each push one barrel forward (v_nail/v_lava: the right barrel on odd frames, the left on even;
v_nail2/v_lava2: one of the four in turn, frames 1/5, 2/6, 3/7, 4/8), found from the models' poses. `vr_gunfx.cpp` firingBarrels:
each barrel's bore end as the middle of two opposite points of its front ring (anchors, so they follow the drawn, blended pose); the
shot's sparks, frontal smoke cone and flash glow now leave that barrel (`emit`); frame 0 (an armed grunt's gun, not animated) keeps
the muzzle as set. The wisps were per barrel already, untouched. The off hand's gun is drawn mirrored, so its frame's barrel is on
the other side: QC `player_nail_BaseImpl` now flips the nail's side (and the QC smoke puff's) for the off hand, so nail and sparks
agree. Debug: `vr_debug_gunfx 1` prints "gunfx barrel who .. frame .. barrel .. side .. up .."; with developer 1, "nailside" a nail.
Test: `python scratch/nb_muzzle_test.py <agent>` (both hands, nailgun and super nailgun, 11 shots each): 13 passed, 0 failed; the
nailgun's barrels 1.98..2.01 units off the muzzle, the main hand's odd frames on its right, the off hand's on its left, 11 of 11
nails on the sparks' side in each hand. Pixels (looking down on the gun, r_dynamic 0): the first shot's spark pixels 493 of 534
right of the middle, the second's 823 of 933 left. Note: the model's own flame for a frame grows in over its 0.1 s blend, so for an
instant after a shot the previous barrel's flame still shows while the sparks leave the new one.

**Crate sounds.** "Very quiet, very hard to hear": the knock (`VR_Crate_Pain`) and the break (the crate's and the tutorial's) now
play louder than full volume (SND_LOUD): tunables `vr_crate_hit_volume` 2.5 and `vr_crate_break_volume` 2 (Debug > Tunables, Knock
Volume and Break Volume; up to 4). Test: `bash scratch/nb_crate_test.sh <agent>` (vrarena_crate, shotgun knocks then a rocket):
knocks at 1.68 and 1.98 (volume bytes 427, 503), breaks at 2.0 (510): 3 passed.

**Ledge shove zones.** "Close to the ledge it should topple as before... another distance from which it backpedals a little"; "I
can shove an enemy from very far away and it backpedals for an entire second". `VR_Knockdown_Ledge` keeps the edge's distance from
the monster's middle (`vr_kd_ledge_edge`); `VR_Knockdown_Try`: within `vr_knockdown_ledge_topple` (32: its front about at the lip)
the old launch over it; within `vr_knockdown_ledge_backpedal_reach` (80: two or three steps) the backpedal; further an ordinary
shove (no ledge). With the backpedal off, as before. The defaults are mine, his to tune (Debug > Tunables, Ledge Shove: Topple
Within, Backpedal Within). **Debug > Show Ledge Shove Zones** (`vr_knockdown_ledge_show`, also on Combat > Knockdowns' Debug;
`vr_ledgeshow.cpp`): a line from your feet ahead along the floor to the first ledge, red within Topple Within of the edge, yellow
within Backpedal Within, grey further, a white tick at the edge; scene lines, so the flat view shows it too (checked in both); a
console line gives the distances when they change. Test 210 (`sv_random_seed 1;map vrclimb;wait30;god;vr_agrunt_test 210`, QuitOn
"test 210 over") now has 12 cells: 48 and 72 a strength (backpedals), 24 (topples), 128 (too far), and checks each shove's zone:
8 passed, 0 failed; zones 72 of 72; over 50 of 54 (the four misses are topples from 24 lying across the lip at two spots, as the
old launch did); backpedal lip speeds 79, 141, 230 u/s; the 18 too-far shoves all stayed up. Not done: the line ignores the
shove's own reach (a weak shove from the yellow may not reach the edge) and a crack too narrow to fall through.

**Grab ease.** "Grab a falling magazine, the hand 19 degrees off: it snaps at once... a split second's transition." `vr_held.cpp`
holdFrame: a prop just taken in one hand is drawn from where its last message before the take had it (the server puts it in the
hand at once) onto its place in the hand over `vr_carry_grab_ease` (0.12 s, smoothstep; 0: at once), the two-hand let-go's ease
reused; drawn only, the physics body is in the hand from the start. Test: `bash scratch/nb_grabease_test.sh <agent>` (a brick, In
the Palm, taken off the floor with the hand turned): eased from 2.7 units and 32.9 degrees; none with the ease at 0: 2 passed.
## Flat-screen: the assertion, walking over armour, the view through teleporters (agent/flatfix, 2026-11-05)

No checklist lines (flat-screen rule). FLATPLAY.md has the behaviour ("Pickups", "The console's give", "Teleporters").

**1. The assertion ("I grabbed armor in my left hand with a shotgun in my right hand... shooting").** Not the armour. Through
the real path (map spawn function, E, 2, +attack, RMB too; his config and the test baseline, ironsight on, grunts firing,
armour, key, suit, rune, shells, health) a held pickup never reaches it: the hand holding it is a fist, ammo id 0, and every
reader guards that. The one route found to that line from a shot: the engine's `give` wrote a count into `.currentammo`, the
main hand's ammo id; with the shotgun (weapon 4 = `IT_NAILGUN`) `give n 40` leaves 40 (`give_flat`: 3 assertions in 3 shots,
as his line). That reaches a shot only with no magazine: every flat-screen shot. The headset run of the same give and shot showed no
assertion before or after the fix (its magazine takes the shot); the corrupted field is shared, the fix is the engine's,
and another counter update there (an ammo pickup after a give) is untested. **Not proved that his session used give**: Debug > Tests' "Shotgun in the Off
Hand" and its kin do (`give s 40`), and so does the console. If it comes back with no give, the assertion's stack is wanted.

**2. Walking over armour.** `VRHandsTracked` answers yes for a flat-screen player, so the "walking does not take it" rule
meant for tracked hands caught him. What the walk does now, each by `vr_flatitems_test 2`: armour (all three), keys, biosuit,
rune, super shotgun, quad, pentagram, ring taken (weapons and powerups had no gap: they were taken before too). Ammo and
health boxes are not taken by walking (rigid boxes; E takes them): listed, not changed, his call. E on armour or a key now
takes it at once as E on a box does.

**3. The view through a teleporter.** See FLATPLAY.md. Measured (`flatgate_test.sh`): pitch through a gate 25, -20, 30, -40
kept exactly (it was reset to 0); yaw turned by the gate's 0, 90 and 135 alone, off by 0.00; walking on the way he looks, off
by 1 degree or less; a turning view through a 90 gate: the step off by 0.005 degrees. The gun's largest step in the camera's
frame at a crossing was 708 units (a blink), now 0.1 at a fresh gate; up to 9.5 for a frame while the walk's sway still runs,
gone in 0.15 s.

Failing before, passing after (the fixes taken out of a build of this branch, the tests kept): give and a shot, 1
assertion; every armour, key, suit and rune walked over left lying, E on better armour not worn; the pitch reset at all
four gates, the gun 714 units off for a frame. With the fixes: flatitems_sh 8/8, flatgate_sh 5/5; flatplay 28/28, flatpose
21/21, flatsight 12/12, flatkeys 28/28, turned_exit 2/2; teleporter cross, edges, selfhit and unstick print as before
(headset runs: none of the changes is reached there).

Noticed, not changed: E searches along the free hand (`VR_Forcegrab_FindTarget`), and an empty hand hangs 60 degrees down, so
at a level view E does not look where the crosshair is; FLATPLAY.md says it does.

## Flat-screen: walking over boxes too (agent/flatfix2, 2026-11-05)

His answer to the list above: "Yes, walk over all". Ammo, health, megahealth and backpacks are taken by walking over them in
flat-screen play; E still takes them. Why they were not: a box rests by its drawn shape and its small box sits under the
floor's plane (on e1m1 its top 10 units below his feet), so Quake's overlap never met it; the engine now touches a
flat-screen player's pickup box (an FL_ITEM with VR_Carry_Setup's hand touch) up to 18 units under his feet, with only the
pickup's own touch (`flatPickupUnderFeet`). The box rules stand, checked by `vr_flatitems_test 2`: shells when full and
health at 100 stay lying; megahealth takes him over 100; a monster's backpack (DropBackpack, the armed grunts' drop too) is
taken. A dead armed grunt's worn backpack (vr_wornpack.qc) is on the corpse, not an item on the floor: not touched by this.
A box let go with G at his feet is taken again only if it can be (a held box is one he was full of). Headset: the same walk
leaves all 13 lying, as before.

## Flat pose, phase 3: stances by weapon, and his notes of 2026-11-05 (branch `agent/flatsolve3`)

Items 1 and 2 of six done; 3 to 6 not started (bob and sway, gun out by pitch, the off hand crouched, sizes). Measured at his
settings (defaults, `vr_flat_aim_fov 45`; his `tunables.cfg` holds only that line, so none of his saved tunables override a new default).

**1. Stances by weapon.** One-handed (melee, a gun with no hotspot, dual wielding): as before. A long gun: as before, and now:
- **A pistol** held with the other hand free is held in both hands (`vr_flat_stance_pistol` 1, Free Hands): its own numbers
  (`vr_flat_pistol_turn` 10 deg, `_back` 2, `_in` 6, `_up` 2: both hands in front of him), the free hand drawn on the revolver's cup
  as a headset draws a helping hand there; no cheek limit (that is the long gun's).
- **The ironsight keeps the off hand on**: the torso turns `vr_flat_ads_turn` 15 more and the chest leans `vr_flat_ads_lean` 10 into
  the gun (the head and the eye stay: `flatsight::stanceLean`, in `solveTorso` only), and the relief comes in as far as the free arm
  needs to take the hotspot within 0.95 of its reach (`reliefFor`, as it did for a real grip), never nearer than 9.5. Where even that
  can't (sighted looking up, crouched from -35, standing from -70), the hand **slides down the fore-end** towards the holding hand,
  at most to 3 ahead of it (`flatsight::lookSlide`, drawn by `vr_view_hands.cpp`; the sweep's new `spot_slide` column: 3.7 at most,
  crouched at -85). Hipfire the same past 0.88 (crouched, looking up). It stays on the gun; it doesn't let go.
- The stance's hotspot keep is applied again across the view after the chest, eye and screen limits (they took it out of reach
  crouched and looking down); the free hand goes off its hotspot from 0.9 of its reach (was 0.78), and sighted from 0.96.
- The sweep measures the free hand from where the full look draws it (the hotspot's own offsets: the revolver's cup has the hand 4.9
  off its point) and from the slid place.

Sweep (`flatsweep_test.sh`, his settings): before `384 passed, 19 known failing, 0 failed`; after `398 passed, 5 known failing,
0 failed` with the new `flatsweep_known.txt`. Off hand on its hotspot (or slid) in all 462 rows that were off; all 16 phase 3
lines are out, and three of the four phase 2 lines (`jump_gun open/long/crouch/hip` 1.005 against 1.0 stays). Row by row against
the before run (`scratch/fs3_compare.py`): 10 rows worse past their limits: **arm_surface** crouched, sighted, looking up -75 to -85
(the free arm on the fore-end passes 0.63 from the eye; was 4.2 with the hand hanging) -- new known lines, open; and one
`settled` row (low ceiling, crouched, sighted, -55; that list's row moved from 45). Flat suites: flatplay 28/0, (see the report for
flatpose, flatsight, flatkeys).

**1b. The free arm clears the eye looking up** (his question: tweak the arm, or move the gun forward). The arm, not the gun: the
sweep's elbows (new `el*`/`wr*` columns, and `vr_flat_sweep <label> ads`: the long gun sighted only) showed the off arm's own pole
(down by the chest, tucked back) bending the elbow up onto the view's axis, 2.3 ahead of the eye, crouched and sighted at -85.
`flatsight::elbowPole` turns that arm's pole to the view's down and out (`vr_flat_up_elbow` 1, `vr_flat_up_elbow_out` 0.5;
smooth from 15 degrees up to 60; the wrist's ease swings it back only by the rest): the elbow goes under the gun, as a support
elbow does. Least eye-to-arm looking up, before -> after: crouched sighted 0.63 -> 4.06 (low ceiling 4.07, at the wall 3.84),
standing sighted 1.72 -> 4.03, the pistol crouched 3.11 -> 4.95; level and down unchanged (4.4 to 8.0: they never had it).
The hand stays on the gun, the gun where it was, so no gun forward was needed (no tunable for it). Sweep: `401 passed, 2 known
failing, 0 failed`; the 9 `arm_surface` rows pass and their lines are out; row by row against the start, 0 worse.

**4. Gun out by look** (his values as defaults). `gunOutAt`: `vr_flat_gun_out` (1) level, new `vr_flat_gun_out_crouch` (-2) level
crouched (by how deep; Crouch Gun Out's 2 still on top), going to new `vr_flat_gun_out_down` (-4) and `vr_flat_gun_out_up` (-4) at
85 along a smoothstep (no corner). Sweep column `gun_out` and check `gun_out`: -4 / 1 / -4 standing, -4 / -2 / -4 crouched at
-85 / 0 / 85, all rows exact (-45: -1.72 standing, -3.09 crouched). The cost: the long gun in its stance comes in across the
chest, its stock 1.1 to 2.3 into the torso's capsule hipfire (crouched level, standing at +-40) and the off forearm 1.1 to 1.7
(`gun_torso`, `fore_torso`, 115 rows past their limits): new known lines, open (a chest clearance for the stock, or his call on
the long gun's values). Sweep `397 passed, 13 known failing, 0 failed`.

**3. Bob and sway.** The drawn body rides the drawn view's own move off the pose's eye (`flatplay::bodyRide`: the walk's bob, a
step's easing; `vr_view_body.cpp` poses a copy of the state with its head moved) by new `vr_flat_body_bob` (1), and the drawn hands
ride the same share (`drawnOffset`'s `to`), all of it sighted (the sights stand on the view). The whole body moves, feet too
(taking it in the legs by the head height changed the crouch's depth: crouched shoulders moved 2.5 on the screen; now 0.1), so the
feet may ride a few units off the floor mid-step. Sighted, the sway and the walk's bob are kept at new `vr_flat_ads_sway` and
`vr_flat_ads_bob` (0.15 each; was none), no breathing. `vr_flat_bob` stays his 0. `flatwalk_test.sh` (12 runs; the probe takes a
held pitch): shoulders on the screen walking 6.06 (ride off) -> 0.10, crouched 5.94 -> 0.12 with hands 1.2 (the start's sway);
the off wrist off its rest 2.15 (before) -> 0.46 walking, 0.015 crouched, 0 sighted; sighted bob 0.26 against hipfire's 1.83
(Walk Bob 1). Verdict `11 passed, 1 failed`: walking looking straight down the off wrist moves 0.543 (limit 0.5) -- item 5, open.
Sweep unchanged (0 rows worse).

**2. E aims at the crosshair.** `VR_Forcegrab_FlatGrab` searches from his eye along his view (`VR_Forcegrab_FindTargetAlong`),
not along the free hand (which hangs about 60 down). flatitems' E cases now put the thing on the crosshair's ray, 36 ahead, 50 deg off
the hanging hand's line (the cone is 15): they pass (flatitems 8/0), so the search is the view's.

**3 (started): the walk probe.** `vr_flat_walk_probe <label> <frames> [shot every N]` and `Misc/quakevr/flatwalk_test.sh`
(+forward on e1m1's start; the wrist on the arm against the drawn hand in the camera's frame, its most a frame, each hand's and
shoulder's range on the screen). Before any change (shotgun): the off hand's wrist 2.15 off its rest walking and running
(the stance's look hand), the main 0.012; the cause found: the drawn hands carry Quake's view bob (`V_CalcBob`, up to 4 up and
7 down, in `r_refdef.vieworg`) through `drawnOffset`'s camera, the body doesn't (`avatar::pose` stands on `s.head`), so the body
moves under hands pinned to the view; and `vr_flat_bob` is 0 by default, so the hands have no move of their own. Planned: the drawn
body takes the camera's own vertical move (bob, step smoothing; the floor kept, the legs take it), the hands their own bob, eased
down by new ADS shares (no breathing in the ironsight, so the sights stay exact at rest).

## Flat-screen controls and status bar: his notes of 2026-11-05 (branch `agent/flatcontrols`)

**1. The mouse by what the hands hold.** `flatplay::routeFire` (`vr_flatplay.cpp`): with one weapon held, in either hand, LMB
(+attack) fires it and RMB (+offhandattack) is the ironsight and fires nothing; with a weapon in each hand, or none, LMB the
right hand and RMB the left, as before. Routed where the move is built (`VR_MoveMainFire` in `cl_input.c`'s attack bit,
`setButtons`' off-hand bit) and in `client::attacking` (the punches, AD's guns' spin), so a rebound button routes the same.
The ironsight: `vr_flat_aim_mode` (0 hold, the default; 1 toggle; VR Settings > Locomotion > Desktop Screen Play > Right Click
Ironsight), edges only, so `vr_flat_aim` on the console stays until the button is used. The wheel's button (`K_MOUSE3`) no
longer aims ("no need for dedicated ADS button"). The left-handed mode is in BACKLOG.md (3).

**2. Q swaps the hands.** `impulse 48` → `VR_FlatSwapHands` (`QC/vr_wpnholster.qc`): weapons, their records (magazine, id),
flags (the foregrip-carried bit off) and next-shot times change hands; a carried box or prop blocks it (G lets it go). The key is
`vr_flat_key_swap` (a bind name, `q`). **Q and Z were bound to nothing** (`quakevr/default.cfg`, his `ironwail.cfg`): nothing
moved.

**3. Z: bullet time.** `bullettime::keyPress` (the stick press's rules: `vr_bullettime_enabled`, single player, then the tap's
`trigger`: the bounce guard, the meter, the cooldown, the sounds). Key `vr_flat_key_bullettime` (`z`). Its meter is the status
bar's bar (4).

**4. The status bar.** *Why the held count was wrong:* the classic client data message (`SV_WriteClientdataToMessage`) wrote
`.currentammo` as the ammo count, and the VR progs keep the main hand's ammo **type** there (`AID_*`): the bar read 1 with the
shotgun and 17 shells, 2 with the nail gun. `VR_CalcStats` already sent the counter, but only the custom-stats path reads it.
`VR_AmmoStat` (`vr_server.cpp`) now writes the main hand's counter (every HUD style). The per-hand counters
(`STAT_QVR_AMMOCOUNTER`/`2`, QC's `updateAmmoCounter`) were right: probe shotgun 17 → 15 shells, revolver 15 → 14, the grunt's
shotgun its magazine 30 → 24, dual wield each its own. *The layout* (`hudstyle 1`, `vr_flat_hud 1`; `Sbar_DrawFlatHud`,
`Quake/sbar.c`, numbers from `Quake/vr/vr_flathud.cpp`): portrait and health centred over the ammo bar (armour to their left),
bullet time's bar over them (cyan ready, dim blue filling, yellow running, grey cooling), each hand's ammo at its own bottom
corner with its ammo picture; an empty hand or an ammo-less weapon shows none. `vr_flathud_report` (Debug > Reports > Flatscreen
Play) prints both counters, the pools, STAT_AMMO, the bar and the canvas.

**Tests.** `flathud_test.sh` (new, 11 checks): counts (shotgun before/after firing, STAT_AMMO = the main hand's count, the
revolver, the grunt's shotgun's magazine, dual wield) and pixels (`flathud_check.py`, against the same frame at viewsize 120: face,
health, both corners, the left gone after G, the bar cyan then yellow, `vr_flat_hud 0` back to Ironwail's). `flatkeys_test.sh` +8
cases, every press through `vr_mock_key` (Key_Event and the binds): LMB/RMB with the weapon in the right hand, in the left (after
Q), dual wield, the toggle mode; `vr_flat_key_swap x`; Q and Z refused in the headset; Z on, a bounce ignored, off.
`vr_mock_key` now runs a key's bind next (`Cbuf_TailToFront`, `cmd.c`): Key_Event appends it to the command text, which in a
script put the press, its waits and its release all after the rest of the script. flatplay's melee case gave its two weapons
in one frame (only the last impulse is sent), so it was the main hand's fist against the off hand's shotgun, which the new
rule now fires with LMB: the axe goes in a frame earlier (punches 0/0 -> 1/0 -> 1/0). Results: flathud 11/0, flatkeys 37/0,
flatplay 28/0.
## Flatscreen: the headset's crosshair, the death view, the armour from the eyes (branch `agent/flatvisuals`, 2026-11-06)

His notes of 2026-11-05.

- **Crosshair** (`vr_crosshair.cpp` `queueFlat`, from `VR_SetupViewEntities` once a frame in flatscreen play): the same modes and
  settings as the headset (`vr_crosshair` 0 off, 1 dot, 2 laser, 3 soft laser; `_size`, `_alpha`, `_hue`, `_depth`,
  `vr_crosshairy`). Where it lands is the game's: the trace from the game's muzzle along `weapons::shotAngles`; the laser starts
  at the gun as drawn (its muzzle anchor on the drawn weapon: the sway, the bob and the sights move it). Drawn in the scene's
  translucent pass over what is there (`lines::sceneOverLine`, new `lines::sceneOverPoint`): the headset draws it in the UI
  pass, over everything, which flatscreen never draws; at the model FOV as the tracers are (near end with the gun). **With the
  flat crosshair:** independent, both drawn when both are on (`crosshair` the 2D mark in the screen's middle, `vr_crosshair`
  the 3D dot or laser where the gun's shots go; with the gun on the crosshair's ray the dot sits on the mark). His config has
  `crosshair 0`, `vr_crosshair 1`: he now gets the dot in flatscreen too.
- **Death view** (`deathview::flatView`, `VR_FlatDeathView` in `V_CalcRefdef` after `VR_FlatViewDrop`; not in a chase view):
  `vr_death_view 2` (default) puts the flat camera in the ragdoll's head as the headset's: its place smoothed
  (`vr_death_view_smooth`), its yaw turn (`vr_death_view_turn`) added to the mouse's look, Quake's dead roll (80) gone, out to
  Quake's view while a menu or the console is open (`vr_death_view_menu_time`); Vomit Mode: the head's own turn, the mouse's look
  on top. The hands and gear are hidden and the body's head too, as in the headset (the state is the same). 0 and 1: Quake's
  view on the floor, as before.
- **Test:** `Misc/quakevr/flatvisuals_test.sh <agent>` (+ `flatvisuals_check.py`): shotgun in hand, red, the flat crosshair
  off; each mode's red pixels against mode 0 (dot 6 px centred at 480 269 of 480 270; laser 330 px from the gun at the low right
  to the middle; soft laser 21 px), then `impulse 195`: `vr_death_view_status` immersive, the camera 0.8 from the head's eyes,
  roll 0; vr_death_view 1 after: 25.5% of the pixels differ. `flatvisuals: 6 passed, 0 failed`.
- **Armour from the eyes, not fixed (his call):** the plates are drawn from the flat eye (`vr_player_plates_info`: 1021 pieces
  "drawn from the eyes", the helmet left out as it should be) and the forearm's bracer is the elbow half, off the screen in
  flatscreen. The chest and legs don't show because **the body's own torso doesn't**: looking down, `vr_flat_chest_down` (0.3:
  the chest's share of the look) bends the chest forward round the eye, which then sits inside the torso, whose faces are
  culled from inside. Measured looking down about 60 degrees, the world hidden: default, no chest at all (only the pouch and the
  gun); `vr_flat_chest_down 0.15` the chest's plates and the shoulders show; `0` the chest, the shoulders' and the arms' plates,
  at both 60 and 85 degrees; the eye pulled back (`vr_flat_cam_fwd -20`) shows the plated chest too; `vr_flat_eye_lean` 24 or
  30 shows less; `vr_flat_eye_pivot 0` puts the eye inside the shoulders. The orbit view saw them because its eye is outside
  the body. Left at 0.3: the share also moves his hands and gun when he looks down (`flatpose`/`flatstance` tuning). Options:
  a lower `vr_flat_chest_down` (0.15 or 0), or keep the bend and carry the eye clear of the bent chest.
## Flat pose: the stock out of the chest; sizes and the walking wrist measured (branch `agent/flatsolve5`)

**The stance's torso turn was never drawn.** `flatplay::poseHands` sets `s.bodyYaw` to the view's plus `flatsight::stanceYaw()`
(25 hipfire, 40 sighted), and `hands::update` then reset it to the view's (`vr_hands.cpp`, flat branch), so the hands stood on the
shoulders of a turned body (`avatar::restShoulders`) while the drawn arms hung from an unturned one (the sweep's `sh*` against `an*`:
the gun's shoulder 2.3 apart crouched). Kept now: the drawn shoulders match the pose's within 0.3. Most of the stock and forearm
depth was that: the gun placed for a turned chest, drawn in front of a square one. The gun's place on the screen is unchanged
(the hands were already placed on the turned shoulders); the body under them now turns as FLATPLAY.md says.

**Stock Clear Of The Torso** (`vr_flat_clear_stock`, 1 unit; Debug > Tunables > Flatscreen): the last of `handOnBody`'s limits.
The held weapon's rear (its model's back end, as last drawn, in the view's frame: `flatsight::stockOff`) is kept 1 out of the
torso's four capsules (the sweep's and `vr_selfcollide`'s), the gun going forward along the view as far as that takes (eased over
1.5): his Gun Out values keep their place across the screen. A wall's push back (`flatsight::pushBack`) stops where the stock would
go in (`flatplay::stockBackMost`), and the gun hand's elbow goes out at its side and a little down while a wall pushes the gun back
(`vr_flat_wall_elbow`, 1; all the way out at 3 units back): sighted at the wall with the body turned 40, its forearm went 2.5 into
the chest.

Sweep (his defaults), before `397 passed, 13 known failing, 0 failed`, after `407 passed, 3 known failing, 0 failed`: the 10
`gun_torso`/`fore_torso` lines pass and are out (worst now: gun 0.37 crouched at the wall, -0.6 elsewhere; forearms 0.91).
Row by row against origin/next (`scratch/fs5_compare.py`): 0 rows worse past a limit; rows worse within their limits come from the
turned body (eye to chest surface, arm surface, hand depths), all still passing. Sheets: `qvr-kit/scratch/flatsolve5/sheet_before.png`,
`sheet_after.png`: the same guns in the same places; the long gun crouched level sits a little further forward.

**Sizes (measured, not changed).** Level hipfire standing, from the sweep's rows: the holding hand's distance from the eye is
melee 16.0, pistol (two hands) 11.6, long gun 5.6 (its hand at the cheek, the stance's shoulder pocket); the muzzle 19.8 / 19.6 /
24.7; the weapon's span on the screen (muzzle to grip, at the eye) 20.7 / 8.5 / 48.2 degrees. So a long gun's hand is drawn about
2x a pistol's and 3x a melee hand's: that is the "much bigger". Proposed rule (his call before applying): the holding hand at one
distance from the eye at level hipfire for every kind, about 10 (the long gun's stance back less, the melee and pistol poses nearer),
so the size differences left are the weapons' own. Dual pistols are not in the sweep yet (a `dual` sweep stance would measure them).

**The walking wrist (open).** `flatwalk_test.sh` unchanged by this branch: 11 passed, 1 failed (`walk_down`: the off wrist 0.543
against 0.5; level 0.46). The off arm is not near its reach there (0.85 of it). The measure is the wrist against the drawn hand in
the camera's frame, 5.1 apart: 0.5 is about 5.5 degrees of the free hand turning against the camera, which the main hand doesn't
(0.012), so the look hand's own turn while walking (`vr_view_hands.cpp`'s `steadyOther` on the hotspot) is the next place to look.
## Flat-screen: crouch speed and swimming (agent/flatmove, 2026-11-05)

His flat-screen notes: crouching ran as fast as standing, and swimming should be Quake's, with the jump out of the water.

- **Crouch speed.** The headset's rule and tunables (`vr_crouch_speed`, `vr_crouch_speed_scale`) were already applied, but by
  the eyes' height: a flat-screen player's eyes stand at Quake's viewheight, under the tallest crouched box's top, so he was
  slowed standing too (260 of 320) and crouched barely more (240). How crouched he is now comes from his crouch key
  (`crouchFlat`/`crouchFlatHeld` in `vr_hull.cpp`, from `updateCrouch`'s `flatscreenCrouch`): 320 standing, 240 crouched, the
  headset's 0.75. Walking (`cl_alwaysrun 0`, 200) stays 200 crouched, as a headset's half stick does: the rule is a cap.
- **Swimming.** `swimmer()` (`vr_physics_water.cpp`) answers none for a flat-screen player: his keys keep Quake's water speed
  where he looks (no 0.25 stick, no strokes from his posed hands, no glide), and `CheckWaterJump` (QC) takes Quake's water
  jump for him whatever `vr_water_jump` says. Headset swimming untouched (the same mock swim rises 38.1 before and after).
- **Test:** `Misc/quakevr/flatmove_test.sh` (`vr_flatmove_test` 1/2/3; test aids 8/9 find deep water and water-jump ledges;
  Debug > Tests > Flat Move: Speed, Flat Ledges). Before: 1 passed, 6 failed (standing 260.3; looking up rose 23.3, down
  28.8; at the start map's pool edge it timed out). After: 7 passed (320/240, the headset's 320/240; up 77.6, down 81.7,
  jump 48.6; lifted out onto the ledge). Jump-to-rise already worked before (48.6 both).
- **To try (flat-screen):** run and crouch-run (Left Ctrl); swim in e1m4 looking up and down, hold Space to rise, and swim
  at a low pool edge (the start map's pool) to be lifted out.

## Flat pose: grips, ADS recoil, the launchers' height, fov-scaled zoom, the crosshair fade (branch `agent/flatsolve6`)

His notes of 2026-11-05. `Misc/quakevr/flatads_test.sh` (sections a-e, one run each, its own verdict) covers each item done.

**1. The grenade and rocket launchers sat lower.** Why: those guns (and the proximity gun, laser cannon, Enforcer's rifle) are the
ones whose two-handed grip the free hand takes for real (`carry`), and `flatsight::stanceShare` is 0 for them, so `targetOffset`
gave them none of the stance's numbers: not its way back (on purpose: pulled back, the free hand's punch missed, FLATPLAY.md "The
stance"), but not its lift either (`vr_flat_stance_up`, 4). New `flatsight::stanceLift` (any gun in the stance) carries the lift
only. Level hipfire, before -> after: hand height grenade launcher -7.08 -> -4.02, rocket launcher -7.45 -> -4.52 (shotgun -3.34);
the muzzle on the screen -19.2 / -17.3 deg -> -10.0 / -10.7 (shotgun -9.6). `vr_flat_sight_check` now also prints a
`flatgunview:` line (hand, muzzle, sights in the camera's frame, each's pitch on the screen) and `flatcrosshair:`.

**2. Fingers.** Flat input has no grip value, so every hand was drawn open (thumb only). `updateFingerFrames` (vr_view.cpp) gives a
hand holding a weapon (not the fist), or drawn on its hotspot (`twohand::drawnHelping`: the stance's and the ironsight's look
hand), the same input a posed weapon hand gets: grip pressed, thumb and trigger touched. Curls, shotgun hipfire and sighted:
main 5 / 2.5 (index on the trigger) / 5 / 5 / 5, off 5 on all (its index closes with the grip). The grasp solve stops them on
the model as in a headset.

**3. Ironsight recoil** `vr_flat_ads_recoil` (0.35; Debug > Tunables > Flatscreen > Ironsight Recoil): the drawn recoil
(`weaponfx::recoilOffset`) is kept at that share as the sight comes up (`flatsight::keepRecoil`, smoothed `adsT`). Shotgun shot:
hipfire 3.15 u / 12.04 deg, sighted 1.10 u / 4.21 deg (0.350). Probe: `vr_flat_recoil_probe`. Not touched: `punchangle` (only
the rogue lava guns' QC sets it on the player).

**5. Zoom and relief follow the fov.** `vr_flat_aim_fov`'s degrees are now counted at new `vr_flat_aim_fov_ref` (95: his fov);
at any other fov the ironsight is the same magnification (tangents of the half-angles), and the range goes to 75.
`vr_flat_ads_relief` is the relief at that reference with Model FOV off; another fov, or `vr_flat_model_fov`, scales it by
tan(half the reference's sighted fov) / tan(half the fov the gun is drawn at), so the sight picture keeps its size.
His settings (fov 95, `vr_flat_aim_fov` 45, relief 4): 50.00 deg and relief 4.00, as before. Fov 70: 33.31 deg, relief 6.23;
fov 110: 62.79 deg, relief 3.06 (both x2.340); Model FOV 70 at fov 95 with relief 14: 9.32. At the default fov 90 the old test
values come out nearly the same (12 / 30 / 45 take 11.9 / 29.5 / 43.7 off); the default relief 14 at fov 90 and
`vr_flat_aim_fov` 40 becomes 15.3 drawn (x1.09).

**Crosshair fade** (his note via the coordinator): the 2D crosshair (`SCR_DrawCrosshair` through `VR_FlatCrosshairAlpha`) and
vr_crosshair's dot and lasers fade by 1 - smooth(`adsT`): out over the ironsight's own travel (`vr_flat_aim_time`), back in on
release; 1 outside flat play, so a headset is unchanged. Read 4 frames into the travel 0.69, sighted 0.000, released 1.000;
pixels in the middle fifth (crosshairs on vs off): hipfire 505 differ (control, the same setting a frame later, 93), sighted 84
(control 415: the scene's own change).

**4 / 4b. Front sights (found, not fixed).** The flare gun's sight line (`vr_sightalign_table.inc`, from `build_flaregun.py`)
runs from the rear notch's *bottom* to the front blade's top, both at SIGHT_Z: on that line the blade's top only touches the
notch's bottom and the hump under the notch hides the rest of it; a sight picture has the blade's top level with the notch's top
(EAR_TOP_Z, 0.28 higher), as the nail pistol's and the revolver's lines are. Fix: the rear point on the ears' top (table or
generator). Not done: the super shotgun (painted sights), the grunt burst rifle (rear 14.350, front 14.450, both y 0 in the
table: its posts' y against the bore not measured), 4c (walls), 6 (the walking wrist).

**Verified.** Sweep (defaults) before (origin/next) `407 passed, 3 known failing, 0 failed`, after the same; row by row
(`scratch/fs5_compare.py fs6base fs6after`): 0 rows worse past a limit, 290 better, 107 worse inside their limits (59 of them
`arm_reach`: the default relief drawn 15.3 at fov 90). flatads 7/0, flatsight 12/0, flatplay 28/0, flatkeys 28/0, flatpose 21/0.
Sheets `qvr-kit/scratch/flatsolve6/sheet_before.png`, `sheet_after.png`: the same poses; the hands now close round the handles
(axe, revolver, shotgun's grip and fore-end), the shotgun's front beads visible sighted. His `tunables.cfg` now also holds
`vr_flat_gun_down 2`, `vr_flat_stance_back 4`, `vr_flat_pistol_in 2`, `vr_flat_pistol_up 4`, `vr_flat_ads_relief 4` and
`vr_flat_aim_fov 45` (none promoted here); none overrides a new default.

## Flat pose leftovers: the front sights (branch `agent/flatsolve7`, 2026-11-06)

**`flatads_test.sh` reported 2 passed** after the flatsolve6 merge: its `SECTIONS` defaulted to `a` (the recoil's two
checks), so a plain run skipped b-e. Run with all sections: 7 passed, 0 failed. The default is now every section.

**The front sights (4 / 4b).** Measured on each gun's mesh (frame 0) with the eye on its sight line, as the flat
ironsight puts it (rays from the eye to points under the front sight's top, stopped by anything nearer): how far down the
front sight is seen. Before: shotgun 0.27, lightning gun 0.60+, nail pistol 0.29, revolver 0.35; **flare gun 0.00, grunt
burst rifle 0.01, enforcer rifle 0.00, super shotgun 0.00**. The three modelled ones had the same cause: the line ran
from the rear notch's *bottom* (the rear sight's block is solid under it and hides everything below the line) to a front
top no higher. **Not asymmetry:** the grunt gun's post, ears, muzzle and bore all sit at y +0.016 (the MDL's y
quantization; the whole gun +0.040), so nothing is off sideways. Fixes, each front top raised level with the rear's tops
so the line runs over them (the nail pistol's and revolver's design):
- Flare gun: the blade's top to EAR_TOP_Z (+0.28; the .blend edited, 4 vertices, re-exported: 12 MDL vertices moved,
  skins and UVs identical). `build_flaregun.py` builds it so too (its rebuild is not byte-reproducible: it differs in
  vertex order and noise, so the .blend was edited instead, as its README says). Line `{1.15, 0, 6.412}` -> `{11.75, 0,
  6.412}`, still parallel to the bores. Seen 0.26 of the 0.28 notch.
- Grunt burst rifle (`make_enemyguns.py`): the post's top 14.45 -> 14.9 (the ears' tops): the line level (was 0.22 deg
  up). Seen 0.59.
- Enforcer rifle: the notch 0.3 (its ears) under the line's rear point; the generator's floor (notch 0.2 over the housing)
  lifts the line 0.14: rear 10.799, post top 10.659 (0.44 over the band, was 0.30), still zeroed at 10 m (0.483 deg, was
  0.471). Seen 0.42.
- Super shotgun: **not fixed: a model issue.** Its ring is painted on the back of a solid block and its bead is a painted
  patch on the barrels' top: on the line through the ring the block hides the bead, and over the ring (tried: a flat-only
  line 0.09 over the ring's top) the bead, flush with the top, is seen edge-on (a pixel or two at 960x540). A real fix is
  geometry: a raised bead (and/or a cut aperture) on v_shot2.mdl.

Headset `vr_sight_check` (mock, laser vs sight line): super shotgun 0.653 -> 0.653, grunt gun 0.221 -> 0.000, enforcer
rifle 0.470 -> 0.481 (its zero), flare gun 0.048 -> 0.048. flatsight 12/0, flatads 7/0. Sighted sheets:
`scratch/fs7_ads_before.png`, `fs7_ads_after.png`, `fs7_zoom_after.png` (the posts and the blade in the notches).
Measure: `python Misc/quakevr/sight_picture.py` (ONLY=<gun>, YS=1).

**Not done:** 2 (walls: push back and muzzle up, tunables), 3 (the walking wrist, `steadyOther`).
## Melee rules: enemy melee under one set of rules (branch `agent/meleerules`, 2026-11-06)

From the melee audit (his answer: "All, armed ones too").

- **One helper** (fight.qc): `VR_MeleeBlow(inreach, wide, los)` (reach, then the arc, then CanDamage), id's form
  `VR_MeleeBlow_Near(base, wide, los)`, `VR_MeleeBlow_Radius(dist, reach)` (the dragon's tail: all round),
  `VR_MeleeBlow_Leap()` (the spawn's, fiend's, dog's touch: printed). They replace the 35 copied reach + arc checks (id,
  Hipnotic, Rogue, MG3, and the 23 AD sites, through `Misc/quakevr/ad_import.py` and `AD_IMPORT.txt`). CanDamage now always
  comes after the arc (ai_melee_side had it before: only the debug print differs). Each blow keeps its reach on the
  monster (`.vr_melee_blow_reach`/`_time`) for the parry's range. Identical: every melee-arc line (id, ad, rogue sets) and
  tests 24/29/35/78 matched the baseline exactly after stage 1.
- **Parry gaps:** AD's lost soul and seeker get `VR_ADM_MELEE`; the Christmas pack (elf, reindeer, Santa, snowman) is
  parried through `.vr_ad_parried` without registering them (registration would give them ragdolls and corpses). The
  parry's range is `VR_Parry_Range(attacker, usual)` (combat.qc): the blow's own reach when it was struck through the
  rules this frame (the dragon's tail 250, an armed blade), else 150 (110 a shambler: its lightning). Same for the
  unparried blow's push.
- **Debug:** `vr_enemy_melee_arc_debug` prints every blow: out of reach, all round, leaps (touch), shoves (cone), armed
  humanoids' (reach with arm and weapon length). Show Melee Arcs draws each blow's rule (`.vr_melee_rule`,
  vr_meleerules.qc `VR_MeleeRule_Of`): an arc; a leap (grey ring at 120, "lands by touch": the spawn, MG3 slime); all
  round (dragon, 250); a shove's cone (grunts, enforcers, vores); none for non-blows (zombie flesh, Armagon, the spike
  mine, eel, lava man). New command `vr_enemy_melee_arc_status` (Debug > Tests, "Melee Arcs Drawn").
- **Shove cone** = the shared arc for its size times `vr_enemy_melee_kind_shove` (0.923: the old 60 at the default 65; a
  vore's grows with its size: 70). Melee arc off: its own 60.
- **Armed humanoids** (vr_armedgrunt.qc `VR_AG_BlowLands`, `VR_AG_BlowBase`): each blow (posed `VR_AGP_Strike`, the gun
  butt's `VR_AG_Strike`) is decided by the arc and reach on its hit frame (through `VR_MeleeBlow`), not by the re-aimed
  line; reach = shoulder + arm (its row) + the weapon's length (`VR_AG_MeleeLen`; punch and gun butt 0) + your body's
  front 12 + `vr_enemy_melee_ag_reach` (16), never under 62, then the size growth. A blow that lands still runs the
  weapon's own melee (its line delivers the hit and the directional parry). The melee start turns first when facing away
  (`VR_MeleeArc_Launch`), and the swing's turn is held (`VR_MeleeArc_Face` in `VR_AGP_Frame` for punch/swing/saw, and the
  gun-butt frames). Why Show Melee Arcs showed nothing for them: never found for certain; they now show with the weapon's
  reach (the sword grunt 71) and the test asserts it.
- **Damage per weapon** (weapons.qc `VR_AG_WeaponBlowDmg`, armed humanoids only; flat-screen players keep 20):
  `vr_agrunt_melee_dmg_fist` 14, `_axe` 22 (axe, crowbar, great axe), `_sword` 22 (times vr_sword_damage_mult, as before),
  `_hammer` 26, `_chainsaw` 18, `_gunbutt` 18: mean 20, the old flat value. Then vr_agrunt_damage_scale as before.
- **Directional parry:** unchanged where their blade is swept already (swings, thrusts with a blade, Mjolnir). Guard-only
  still: the punch, the gun butt, the chainsaw, the shove.
- **Tests:** `vr_agrunt_test 212` (vr_meleerules_test.qc): an armed sword's blow from ahead lands; a side-step to 100 deg
  as its swing starts dodges it (89 deg off at the hit frame, 0 damage); Show Melee Arcs sets and the engine draws each
  kind (arc, leap, all round, shove: `meleearcviz: drawn arc 1 leap 1 round 1 shove 1`). `vr_agrunt_test 213`: the
  nearest lost soul's bite parried (spawned through the dispenser, 207): needs AD mounted (`vr_ad_mounted` is 0 in the
  agents' trees, so it reports "no lost soul" here; not verified). Night check 2026-11-06 with AD's paks copied into
  the agent's base (`kit/bases/<agent>/qbase/ad`) and `-Games "-game hipnotic -game rogue -game ad -game quakevr"`:
  it first stopped on `Host_Error: PR_ExecuteProgram: NULL function` (VR_AGT_Hunt ran id's FoundTarget, which thinks
  the lost soul's unset `th_run`); now it wakes the soul by AD's own AI (`AD_TestWake`). It then runs but **fails**:
  0 parries in 8 s, the soul 150-200 away with no enemy at the end (map start and vrtesthall, with and without god).
  Open: why the woken lost soul drops its enemy instead of biting.
- **Before/after** (vrtesthall, seed 7; melee_arc_test.sh id/ad/rogue at arc 1): every meleearc result line identical
  after both stages. `vr_agrunt_test` 78: 47/0 -> 47/0; 24: 2/0 -> 2/0; 29: 1/0 -> 1/0; 35 (no mock guard in this run):
  4/2 -> 5/1 (Fist now parried; Axe still 0 of 2, as before); 18 (long blade's standoff, blows landing): 24/0 after;
  89: 3/0 after. The ad set ran without AD mounted (id's monsters).
- **To try in VR:** an armed sword or axe grunt: step sideways as it winds up (it should whiff), then stand in front
  (it lands); the hits per weapon should feel like before on average (fists lighter, hammer heavier). Show Melee Arcs
  near a spawn, a dragon, a vore: each shows its own rule.
## Flat-screen: standing up under a low ceiling (agent/flatmove2, 2026-11-06)

His note: "flatscreen crouch allows the player to uncrouch even when there is geometry above, causing the player to
penetrate geometry. It should share the same checks/code paths as the VR crouch." The server's box already took the
headset's path when Ctrl was let go (`updateCrouch`: standing if it fits, else the tallest crouched box that fits, else
the one he has), but the client eased the view back up whatever the box, so the camera went into the ceiling.

- **The view follows the box.** `pollKeysFlat` keeps the eased crouch no lower than leaves his eyes 8 units
  (`flatEyeHeadroom`) under his box's top, read from the local server (`hull::localEyeCap`, as the lean's `playerBoxFits`
  reads it). 40-high tunnel: he stays down (36 box, eyes 28). 48-high gap: 44 box, eyes 36. Walked out: standing again.
- **Bit 14 is the key now** (`flatplay::crouchHeld`, was "eased crouch above 0"): with the view held down by a kept box,
  the eased bit would have kept the box crouched for ever.
- **Crouch speed** follows the box too: slowed while the key is held or his box is kept crouched.
- **Multiplayer client** (no local server): no cap; the server's box still holds, only the view is not capped.
- **Test:** `flatmove_test.sh` rise (`vr_flatmove_test 4`, vrteleporters' crouching room). Before: tunnel eyes 46 under a
  40 ceiling, gap eyes 46 under 48 (2 failed); walked out stood (passes before and after, the guard against a stuck box).
- **To try (flat-screen):** vrteleporters, east room: crouch into the LOW TUNNEL and the GAP, let go of Ctrl inside: the
  view stays under the ceiling; walk out and you stand.
## The shotgun as a source .blend (spike, branch `agent/blendsg`)

His goal (BACKLOG, "Easier modelling and asset creation"): the weapons natively editable in Blender, Quake's animations
kept. The shotgun first: `Misc/quakevr/src_models/blend/v_shot/` (v_shot.blend 300 KB, its skin PNG 85 KB, its relief
PNG 2 KB) is now the source of `v_shot.mdl`, `vr_pump_on_v_shot.mdl`, `vr_pumpbody_on_v_shot.mdl` and their three
normal maps; `blender -b <the .blend> --python Misc/quakevr/blend_export.py` writes them (MODELS_IN_BLENDER.md, section 9).

- **What it is built on:** the models add-on's import and export (mdl_blender.py), which already kept an unedited model
  byte for byte. New: `source.py` (the build, the checks, the parts, frames baked from armature actions, the barrel
  mask, the anchors' and hotspots' write-back to weapons.txt), `sights.py` (vr_sightalign.cpp's painted sight line in
  Python), `blend_build.py`, `blend_export.py`.
- **A fix on the way:** the add-on's anchor report read vr_weapons.inc's `QVR_WEAPON_DEFAULT` rows, gone since weapons.txt
  (round 22): every weapon export reported "no slot uses" and checked no anchor. It reads weapons.txt now (mdl.py
  `slot_table`, via weapons_data.py).
- **The pump parts** are subsets of the gun's triangles (gunlooks.py's write_subset): the .blend marks each face's part
  (`qvr_part`); of the 60 triangles that cover nothing (two corners welded: not faces in Blender, kept in the file in
  place), 8 are in a part and keep it in a property (the other 52: the first ring, collapsed by split_auto_pump).
- **Proof:** the untouched .blend exports all six files byte for byte (cmp: SAME x6); no rounding at all, since the
  export keeps every unchanged vertex's, normal's and texel's bytes. The sight line from Python equals the game's
  `vr_sight_lines` (rear 9.86 -0.05 6.54, front 32.52 -0.00 6.21, -0.84 deg up, 0.11 left).
- **Round trip (not shipped):** a rivet moved 1.5 units, 42 faces' texels painted red (5503 texels changed), and an
  armature on the pump with an action `pumpalt` (back 2.5 units, rolled 12 degrees, home: 6 frames) baked as frames
  9-14 of the gun and of both parts. In the game: the skin shows, `vr_debug_weapon_frame` 11 and 13 draw the new pose
  (17694 and 19808 pixels changed against frame 0, 36 for frame 0 against itself); the shot, the auto pump stroke and
  the shell eject as before (start 0.18 s after the shot, back 2.50 units, home on time); `vr_weaponstudio_align`: the
  sight line 0.80 deg from where it shoots, as shipped; flat ironsight `vr_flat_sight_check` 0.0005 deg; reload_test.sh
  114 passed, 1 failed: "the shotgun's sights keep their colour as it pumps" counts red pixels round the sights, and
  the test edit had painted the gun red (657 red against 105 sight pixels). The untouched export then put the shipped
  files back (git clean) and gave the same numbers.
- **Appended frames' normals** are frame 0's turned with their piece (a rigid fit per connected piece), so the shading
  the model was made with comes along; unmoved pieces keep frame 0's bytes.
- **Hold the Weapon Frame** (Debug; `vr_debug_weapon_frame`) goes up to 31, for frames an action added.
- **To try (headset):** nothing changes in the game (the shipped files are the same bytes). To try the workflow: open
  the .blend in Blender 5.2 with the Quake VR Models add-on, move a rivet or paint the skin, save, run the export, then
  `vr_model_reload v_shot vr_pump_on_v_shot vr_pumpbody_on_v_shot`.
- **Open:** a new action plays only through `vr_debug_weapon_frame` until QuakeC names its frames (`$frame` lines and a
  `player_*` sequence); the plan for the other weapons is in MODELS_IN_BLENDER.md section 9.

## The shotgun's top retopologized (agent/sgtopo, 2026-11-06)

His words: the workflow works; there is cursed topology on top of the shotgun between the back sights and the stock: fix
it without messing up the textures and UVs. Done in the source .blend (`Misc/quakevr/src_models/blend/v_shot/`), then
`blend_export.py` (anchors all ok, sight line turned 0.000 deg / moved 0.000, normal maps re-baked, weapons.txt's barrel
mask re-indexed: 1876 -> 1908 triangles).

- **What was there** (top view, x along the gun): from the receiver's front corner (18.55, -1.74) a fan of 10 faces to the
  rear sight's base (x 9.97), min angle 0.2 deg, with a zero-area filler for the T-junction at v32; behind the sight, inside
  the V the two bevels make towards the stock's ridge, 24 faces: slivers over five UV islands, five zero-area fillers along
  x 9.75, two faces facing down and an interior vertex; on the +y bevel a vertical sliver hiding the T-junction at v55.
- **Now:** each area is a constrained Delaunay triangulation of the same outline (its boundary vertices kept, so no face
  round it changes), interior points on a staggered grid, relaxed: the front 40 faces (min angle 16.1 deg), the V 26 (12.2;
  its centre line v53-v104 kept as an edge, with points on it), the +y bevel split at v55 as the -y one is at v52 (16.5).
  The bevels, the rivets, the sight posts and the notch floor are untouched; 7 vertices only the old faces used are gone
  (and v98, 0.023 under the V's plane). Faces 1816 -> 1848; the file 1908 triangles (1780 kept in place, 68 new), the
  T-junction cracks 26 -> 23.
- **Frames:** every new vertex is a barycentric mix of an old face's corners, the same in every shape key (and the export
  carries new vertices with their piece).
- **UVs:** the front area is one island (a top-down projection): new corners read the old faces' UVs, interior points sit
  on texel centres; rendered from above (nearest texel) against before: 6.2% of its pixels differ, all texel-edge shifts
  (max 0.33 / 0.40 texel, mean 0.035). The V had no island to carry (a collage): its new faces read one island (the one its
  -y half used, a top-down projection fitted to 0.6 texel), and the 208 texels of that island no other face reads were
  painted with what the old faces showed at their place (skin and relief PNGs): rendered from above it looks as before
  (18% of its pixels differ, texel-edge resampling of the old stretched slivers; the stock and the front 0%).
- **In the game:** flat ironsight frame before/after: 532 pixels differ, all on the gun's top at the sights; the headset
  frame (gun seen from above) shows the same gun (its run-to-run noise hides the difference); `vr_flat_sight_check`
  0.0004 deg either way; reload_test.sh 114 passed, 1 failed, the same one with the old files ("the shotgun's sights keep
  their colour as it pumps": 107/55 99/0 104/49 now, 105/55 99/0 104/49 before; the frame drawn whole shows red sight
  pixels, the pump's parts none).
- **Left as they are** (the same kind, not equally safe): the receiver's sides are fans too (-y from v33: faces 47-50, min
  2.8 deg; +y from v49: 63-66, min 3.0), but their faces are not coplanar (normals up to 4 deg apart) and the shell
  port's anchor sits on v39 / triangle 48; under the receiver, long slivers (faces 83-93, 45, 46, min 1.2-4.7 deg, facing
  down, rarely seen); the barrels' top (faces 142, 143, 146, 147, min 2.2) and the rib's long strips (958, 959, 966, 967);
  the collapsed first pump ring (zero-area by design, split_auto_pump).
- **To try (VR):** look down on the shotgun's top behind the rear sight in the headset: the same texture, no streaks or
  sparkles; the sights and the auto pump as before.
## Survival Arena: its messages on the wrist gadget (agent/arenawrist, 2026-11-06)

His words: now that the arena has banners, move the arena's text from the centre of the view to the wrist gadget, as an
option, on by default.

- **What moved:** every arena message (QC `VR_ARENA_Show` / `ShowAll`: the welcome, the countdown, a wave's start, a boss
  coming, a wave's clear with its supplies, a perk taken, Quartermaster, the run's end: fell / cleared) now goes out as
  `viewmessage(p, text, secs, 1)` (new optional `kind`; the builtins' table is full) = new `QVR_SVC_ARENAMESSAGE` (41,
  the view message's payload). The client (`worldtext::clientParseViewMessage(arena)`) hands it to
  `gadget::arenaMessage` with `vr_arena_wrist 1` (new, archived, default 1; Combat > Survival Arena > Messages on the
  Wrist): the gadget's own message hologram (the one the game's centre prints already use), one arena message at a time
  (the next replaces it in place: the countdown's seconds don't stack or regrow), for its seconds, never held. The perk's
  separate centre print (its name and what it does) is folded into the arena's own line for the picker
  (`ARENA - X taken` + blank line + what it does), so no arena text goes through `centerprint` any more.
- **Out of view:** a message arriving while the gadget isn't facing you (and no arena message is still up) buzzes the
  gadget's hand once (the watch buzz the hologram's notifications use); it shows as soon as you raise the wrist while it
  lasts. Messages are not held until seen (they are of the moment): the banners keep the wave, foes, time, score,
  NEXT WAVE / CHOOSE A PERK and YOU FELL / CLEARED; the perk cards are untouched (they stay in front of you).
- **Fallbacks:** no hologram (HUD not the gadget, `vr_messages_hologram 0`, recording mode) = in view as before; `0` = in
  the middle of the view as before; flat screen = the centre print either way (checked: `vr_enabled 0` logs `centre print`).
- **Test aids:** `panelboard_read` `what` 4..8 read the wrist's arena messages (ink of the last one's block in the
  hologram image, its frames shown facing the eyes, counts taken / shown in view / buzzes); `vr_gadget_info` also prints
  how much the gadget is in view.
- **Test:** `vr_arena_test 39` (`developer 1;map vrarena;wait5;god;vr_arena_test 39`, `-QuitOn "arena: test [0-9]+ over"`):
  the director's countdown, wave 1, its clear (gadget turned away: 1 buzz, 0 frames for half a second, then raised: shown),
  a perk taken, a fall, each on the wrist (ink 0.023..0.092, 3 frames) with none in view; off: the next ones in view, none
  on the wrist. 13 passed, 0 failed. Tests 38 (46/0), 37 (17/0), 34 (3/0) pass.
- **To try (VR):** in vrarena, raise the wrist during the countdown and at a wave's start and clear; keep it down at a
  clear (one buzz) then raise it; take a perk; set Messages on the Wrist off and see them in the view again.

## The plates drawn as a gib (agent/platesmodel, 2026-11-06)

- **Bug:** after a campaign switch (here id1 to rogue, then R2M6), the armour plates morphed in as another model: a gib,
  a fish, a door (`vr_player_plates_info`: `model *78`). Any build; which model it hit depended on the slots.
- **Cause:** `agarmor::armorFor` (vr_agarmor.cpp) kept each body's armour model as a `qmodel_t*`. A campaign switch's
  `Mod_ResetAll` keeps the alias models whose files stay the same (vr_modelkeep.cpp) in their slots, the player's body
  among them, but empties the derived `<body>#armor` ones, and `Mod_FindName` gives the emptied slots to the next map's
  models. The body found its cached kind again (same pointer, same name) and got the slot now holding another model.
  `forgetModels` (`vr_agarmor_rebuild`) would also have freed that other model's cache.
- **Fix:** each kind keeps the name its armour was loaded as; `armorLive` (the slot still that model, loaded, alias) or
  the armour is looked up again, as `view::viewModel` already checks its own cache.
- **Check:** `vr_player_plates_model_check [end]` (`ppmodel: ok|FAIL ...`; `end`: `ppmodel: N passed, M failed`): the
  plates drawn as "<body>#armor", an alias model, the body the build's own.
- **Test:** `bash Misc/quakevr/plates_model_test.sh <agent>` (needs rogue): builds 1-3, armour in e1m1, the switch,
  armour in r2m6; the check on each map, and the pixels (`plates_model_pixels.py`: a camera in front of the player, the
  plates on against off: the silhouette in r2m6 the same as in e1m1, at least 0.4 intersection over union). Without the
  fix: 3 passed, 6 failed (plates `*62`, `*87`; same pixels 0.23-0.26). With it: 9 passed, 0 failed (0.68-0.70).
  `vr_pp_test 1` 53/0, `3` 5/0.
- **To try:** flat or VR, build 3: a map in id1, then Dissolution of Eternity, R2M6, the armour by the entrance.
## Blender: export on save, the game reads it again (branch `agent/blendauto`, 2026-11-06)

His words: avoid running the export script by hand. Now the loop is edit, Ctrl+S, look (MODELS_IN_BLENDER.md, sections
1, 4 and 9).

- **The add-on** (`quakevr_models/autoexport.py`, version 1.1.0): a `save_post` handler. Saving a weapon's source .blend
  (one model object with its anchors' empties, inside a repository that has `Misc/quakevr/blend_export.py`) runs that
  same script on the saved file in a **background Blender** (`blender -b --factory-startup <file> --python
  blend_export.py`), not in-process: the export changes the object it exports (mode_set to Object, triangulates it,
  sets its frame and properties), which would change the scene being edited after every Ctrl+S. It takes ~5 s and
  doesn't block the editing; a timer reads the result: the status bar says ok (the files and the folder) or `NOT
  exported, nothing written` with a popup of the reasons; the whole report is `%TEMP%\quakevr_export_on_save.log`. A
  save during an export exports again after it. The checks are blend_export's own: nothing is written on a refusal.
  Preferences (and the panel's new top box): **Export on Save** (default on), **Game Folder** (empty: the repository's
  `quakevr/progs`, weapons.txt written too; another folder: `--out`, weapons.txt left alone).
- **The engine** (`vr_modelwatch.cpp`; `vr_model_watch`, default 1, Debug > Tests > Read Models Again When Saved;
  `vr_model_watch_status`, a Debug button too): the loaded alias models in `progs/` that a folder has (not a pak) are
  watched: the .mdl and beside it its `_0_norm.png`, full-colour skins (`_0.png`, `_0_glow`, `_0_luma`) and a body's
  `.md5mesh`/`.md5anim` that exist. Windows watches their folders (FindFirstChangeNotification), asked every half
  second of real time: nothing changing, no file is looked at. A notification has every file's write time looked at
  over the next half seconds (spread over the frames); a model whose file changed is read when a whole half second
  passes with no new change (an export's files read together), and only when its .mdl is whole (header, version, size
  at least what its counts need; else retried at the next half second, said once). It is read through
  `view::reloadModels` (vr_model_reload's own work, split out of its command) and prints `vr_model_watch: changed on
  disk, read again: ...`. Newly loaded models are listed for at most 0.2 ms a frame, their folder listed once (no
  lookup of a missing file). New engine helpers: `Mod_KnownModel` (gl_model.c), `COM_PackHasFile` (common.c).
- **His Blender:** checked (read-only, a background Blender with his preferences): Blender 5.2 already has the script
  directory `C:\OHWorkspace\quakevr-iw\Misc\quakevr\blender\` and both add-ons (Quake VR Models, the hand's) enabled,
  preferences auto-saved: the docs' method, which loads the add-ons from his checkout. Nothing was copied or changed
  (a copy in the user add-ons folder would duplicate the modules). After this branch is merged into his checkout:
  restart Blender (or F3 > Reload Scripts) once.
- **Test:** `bash Misc/quakevr/blend_autoexport_test.sh <agent> --idle` (blend_autoexport_test.py is its Blender half),
  on a copy of v_shot.blend in `scratch/ba_autoexport`: a renamed frame saved: refused, 5 of 5 game files untouched;
  the game on e1m1 and the copy saved: exported (4 game files newer, byte for byte the shipped ones) and the game printed
  `vr_model_watch: changed on disk, read again: progs/v_shot.mdl, progs/vr_pump_on_v_shot.mdl,
  progs/vr_pumpbody_on_v_shot.mdl` and vr_model_reload's lines; a real-time minute idle on e1m1: 224 files of 123
  models in 1 folder, 131 asks, 0 notifications, 0 looks, 0 reloads, 52.7 ms in 66 s (4.4 us a frame, the first
  listing included; low-impact run). 8 passed, 0 failed.
- **To try (VR / desktop):** restart Blender, open `Misc/quakevr/src_models/blend/v_shot/v_shot.blend`, see the Export
  on Save box at the top of the Quake VR panel; with the game running and the shotgun in hand, move a vertex of the
  stock and Ctrl+S: the status bar says exported, the gun changes within a second or two. Rename a shape key and
  Ctrl+S: the popup refuses and the game doesn't change. (Undo both; don't commit the .blend.)

## Every weapon's source is a .blend (branch `agent/blendall`, 2026-11-06)

The author: "Let's make the '.blend' the source of truth for all weapon models ... Ensure everything works as before
after the migration and that all models are committed." And, for the super shotgun: "Please model a raised bead if it's
not there already."

**Inventory and order.** Every weapon model the game loads: 25 view models made by generators (polish_weapons.py,
gunlooks.py, make_swords.py, make_enemyguns.py, make_chainsaw.py, make_crowbar.py, make_toolgun.py) or shipped since the
game folder (v_spike), the 8 pickups (g_*: gunlooks.py for g_shot0 and g_nail, the rest shipped as they are), the
magazines and wells (make_mags.py, make_nailpistol_mags.py), the parts (the shotgun's pump: v_shot.blend already; the
super shotgun's open halves: make_ssg_open.py), the nail pistol (models_src/nailpistol + build_nailpistol_slide.py), and
the hand-made flare gun, revolver and widowmaker (models_src/<gun>/<gun>.blend + export_mdl.py). Order: the generic
build/export first (source.py), then all generic ones at once (they round-trip byte for byte), then ownership and
retirement, then the hand-made ones' export path, then the bead.

**What changed** (MODELS_IN_BLENDER.md section 9, "Every weapon", has the list of sources):
- `source.py`: a one-frame model needs no shape keys; an anchor already off the surface in weapons.txt isn't refused
  until moved; the normal maps are kept as shipped while the model file and relief they're baked from are unchanged
  (the shipped maps of 8 guns and the 9 magazines were baked by older code: a fresh bake differs by a few bytes), baked
  again otherwise; full-colour skins (`v_crowbar.mdl_0.png`) kept beside the .blend and copied; the super shotgun's open
  halves made from the exported gun by make_ssg_open.py's `build(src, out_dir, guarded=False)`; v_nail and v_lava get
  gunlooks.py's relief as PNG. `owned_files`, `HANDMADE_FILES`, `handmade_script`.
- 53 new generic sources under `Misc/quakevr/src_models/blend/<model>/` (11 MB), built by `blend_build.py` from the
  shipped files; v_shot's (made before) unchanged.
- `genguard.py`: `generated.json` entries with `"source"` (112 files) are owned by a .blend: Guard leaves them (even
  with `--force`) and names the .blend; a generator whose files are all owned stops (`retired`). `blend_build.py`
  records a new source's files. The add-on's Bake Normal Map refuses an owned map.
- `blend_export.py` and export on save: any source made by blend_build.py (`qvr_source_blend`), and the hand-made ones
  (runs the `export_mdl.py` beside the .blend). The nail pistol's hand-made .blend no longer made the shipped gun (its
  export differs from the committed `v_nailpistol_classic.mdl` the full slide was built from): archived, its
  export_mdl.py refuses. The hand-made `build_*.py` refuse without `--from-scratch`.

**The super shotgun's bead.** Its "front post" was a painted patch flush with the muzzle band's top, and the rib's back
face, just behind the ring, had its top at the ring's middle: through the ring the bead was hidden (sight_picture.py:
"seen down to 0.00"). Edited in v_shot2.blend: a glowing bead (an octagon seen from behind, radius 0.2, 0.22 over the
band, on the front post's sight texels, in every frame with the band) and the rib's back end lowered 3 grid steps
(7.556 -> 7.389), so the ring is a real aperture. The sight line: 0.65 -> 1.47 deg up, 0.06 -> 0.11 left; the shots
follow it (`shot_pitch 1.47`, `shot_yaw 0.11`, slot 2 in weapons.txt). sight_picture.py's line updated; vr_sightalign's
comment.

**Proof.** Every source exported by `blend_export.py --out` (56 sources, 112 files) is byte for byte the shipped file
(before the bead). The retired generators (polish_weapons, gunlooks, make_swords, make_mags, bake_normals v_axe,
fix_shot_stock_corner, build_nailpistol_slide) stop and write nothing. Export on save on a copy of v_axe.blend: ok, the
same file. Super shotgun, headset (mock) `vr_sight_check`: laser vs sight line 0.652 -> 0.003 deg; flat
`vr_flat_sight_check`: rear 0.0002, front 0.0004 off the ray (shotgun 0.0001 / 0.0002); sight_picture.py: the bead seen
down to 0.25 under its top (was 0.00). Only shipped changes: v_shot2.mdl, its normal map, its two open halves,
weapons.txt slot 2 (barrel mask over the new triangles, shot pitch and yaw).

**Checklist lines (suggested):**
- Super shotgun: aim down the sights: a glowing bead centred in the ring, the whole bead visible; shots land on it.
- Blender: open any weapon's .blend (Misc/quakevr/src_models/blend/<model>/), move a vertex, Ctrl+S: the game shows it.
## A cut-off head or limb stays on the axe (branch `agent/limbonaxe`, 2026-11-06)

His note: "The cut off limb should get stuck on the blade that cut it off." (This replaces agent embedfix's decision "not embedded
in the severed part".) His direction: build it on the sword's gib skewering, the axe's own bits on top.

- **Reused (vr_skewer.qc, the skewer path):** the gib rides the hand by its own think (VR_Skewer_Think, its fade waiting in
  `.vr_skewer_think`), not solid, MOVETYPE_NONE; its weight in the hand (VR_Skewer_Tally, vr_skewer_mass_head/limb, the engine's
  vr_weight.cpp); the other hand's grip (VR_Skewer_Gripper/ForGrip, the engine's skewerDrawnHand draws that hand on the gib) and the
  tug's numbers (`vr_embed_skewer_tug_speed` 4 m/s with half the stretch, or `vr_embed_skewer_tug_stretch` 30 cm) into that hand
  (VR_Skewer_Take); coming off when the blade is gone, holstered, switched, the player dead, vr_embed off, the level's end
  (VR_Skewer_Find/Off); a swing's burst on what it meets (VR_Skewer_Impact, `vr_embed_skewer_swing_burst`) and a swing's contact at it
  (VR_Skewer_SwingHit); save/load as the skewer's (all state in entity fields and its think; not run separately).
- **New (the edge, `.vr_skewer_cut`):** VR_Skewer_CutOn, called by VR_Decap_Cut and VR_Limb_Cut with the thrown head or limb and the
  neck or joint: a swing's cut (QVR_DECAP_SWING, live or corpse) by a player's hand whose blade takes parts (`vr_embed_cuton`, default 1:
  the edge blades, the axes; 2: the pointed ones too, the swords) with its blade proper (an axe's head from 0.6 of its line, a sword's
  from 0.4) within 16 units of the cut. The part is moved so its cut end is on the edge and kept rigid there in the hand's frame
  (`.vr_skewer_at`, the turn in `.vr_skewer_fw/up`), not along the line; at most `vr_embed_skewer_most` on a blade. Only in the hand
  that cut it: let go of, thrown, or the axe stuck in a body, it falls (no sphere on a thrown axe). The other hand's tug is measured from
  where it took hold, any way (no slide along the edge). Off by a stop: its own speed at least `vr_embed_cuton_fling` 2.5 m/s then down
  to half within 0.25 s (a hard swing that stops, a shake's turn back) flings it the way it was going (VR_Skewer_Fling). For 0.4 s after
  it went on (the swing that cut it, going on through the body) no fling, no burst. Its fling-to-a-wall watch ends as it goes on.
- **Swords:** off by default (`vr_embed_cuton` 1): a sword's slice goes through, and a sword already skewers by a stab; 2 turns it on with
  the same rules. Debug > Tunables > Combat > Embedded Blades: "Cut-Off Parts: Stay on the Blade", "Cut-Off Parts: Fling Speed".
  Like skewering, it needs `vr_embed` on.
- **Test:** `vr_embed_test 26` (`limbonaxe: N passed, M failed`), the mock hand's real killing axe swings: 0 a live grunt's head, 1 a live
  grunt's limb, 2 a corpse's limb, 3 a corpse's head (killed whole, lain still, you put 40 units from the part): cut off onto the blade
  (its edge 4.6-8.5 units from the cut), no blade in the body, the axe in the hand, at the axe's head; rides the hand 25 cm up at 0.5 m/s
  (0.01-0.11 units off the hand's move), its weight in the hand (head 1.50 kg, limbs 0.54-0.85); off: 0 and 3 a 4 m/s move that stops
  (flung at 4.8-5.3 m/s), 2 a 5 m/s swing that stops, 1 the other hand's grip, then a 35 cm pull at 1 m/s: "tugged off 31 cm", in that
  hand. 27/0. Across the floor, the part met the floor or the body and burst (the skewer's swing burst): the test swings up and across.
## The .blend migration checked in game (branch `agent/blendverify`, 2026-11-06)

agent/blendall's weapon migration on next, against the build before its merge (14d0b35b9). Shipped game files the merge
changed: only v_shot2.mdl, its normal map, its two open halves and weapons.txt slot 2 (`git diff --stat 14d0b35b9
462384339 -- quakevr/`); code: one comment. Every other weapon draws from byte-identical files.
- **Every weapon, headset mock** (`vr_test_give_wid 1..31; impulse 149; vr_sight_check main`, grip mode sticky): each
  draws its model; laser vs sight line, before = after on every gun but the super shotgun: grapple 0.0023, shotgun
  1.0349, nailgun 0.0004, super nailgun 0.4978, rocket launchers 0.0007/0.0016, prox 0.0007, thunderbolt 0.2383,
  laser cannon 0.0561, grunt gun 0.0020, enforcer rifle 0.4794, nail pistol 0.1299, flare gun 0.0296, revolver 0.1510
  (the kit base config's saved shot keys). Super shotgun: 0.6542 -> 1.4730 with that config's saved `vr_wofs_shot_pitch_03
  "0"`; with the shot keys reset to weapons.txt's (1.47, 0.11): 0.0020.
- **Flat** (the same with `-flatscreenmode`, `+attack`): the guns fire (ammo down: shotgun 1, super shotgun 2, the
  nailguns, rockets, prox, cells, nail pistol, revolver); flatsight 12/0 (15 guns with sights: at most rear 0.0002,
  front 0.0007 units off the ray, 0.0032 deg far along), flatplay 28/0, flatkeys 37/0, flatads 7/0.
- **Reload** (reload_test.sh): 114/1; the one fail is the known "the shotgun's sights keep their colour as it pumps"
  (107/56 99/0 104/49), and it fails the same before the migration (107/55 99/0 104/49: v_shot is unchanged). The super
  shotgun's checks pass: break-open, its pair, the ring on the barrels part, its sights' colour shut, fired, open, shut
  (159/0 163/25 230/24 163/24), its blood open and shut.
- **Weapon self-tests:** nptest 39/0, npaim 7/0, flaretest 22/0, flaremuzzle 5/0, magslide 63/0, agtest 1 17/0; rvtest
  43/3 (a slow push, a push with a shell in the other hand, the live shells' speed): the same 3 before the migration.
- **Generators against owned files:** the release path (make_release.ps1/.py, package-quakevr.ps1, the kit build.sh,
  run_test_suite.py's 43 scripts and everything they run, 122 scripts) runs no model generator. Four in-place rewriters
  had no guard and would have overwritten owned models when run by hand: reuv_shot2.py (v_shot2), refine_light.py
  (v_light), refine_laserg.py (v_laserg), reuv_weapons.py (v_plasma, v_nail2, v_lava2, v_rock2, v_multi2, v_grpple,
  v_hammer): now `genguard.retired` (`--report` still runs). Every weapon generator run (no arguments, then `--force`) in a
  copy of the tree: the 112 owned files unchanged.
- **For the author:** his own config has `vr_wofs_shot_pitch_03 "0.650418"`, `vr_wofs_shot_yaw_03 "0.059087"` (his
  Align Sights to My Aim on the old sight line). It overrides the new default (1.47, 0.11): with it the super shotgun shoots
  about 0.8 deg under the new bead. Not changed (his setting): run Align Sights again on the super shotgun, or reset those
  two keys, to take the new line.

**Checklist line (suggested):** Super shotgun: Align Sights to My Aim again (your saved shot pitch 0.65 predates the bead's
1.47), then the bead sits on what you hit.
