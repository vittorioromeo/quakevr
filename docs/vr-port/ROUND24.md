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
- **Pass 5c (branch `agent/armor5c`):**
  - **Corpse tiles:** `vr_pp_test 15`'s tiles 10-11 showed a standing grunt: a monster's start comes a few frames after its spawn
    (takedamage off till then), so the hit did nothing. The test now kills it once it takes damage (state 3) and shoots once its
    frame has held half a second (FAIL "timed out" after 5 / 10 s), from closer (his pictures 12, 13). Its death animation is
    random, so before and after lie differently. The feet: the shin plates end above the boot; nothing below the sole.
  - **Thighs:** the enforcer's and the death knight's plates started at the hip's pivot, but their skin from the belt (enforcer)
    or the tabard's hem (death knight) down to mid-thigh weighs on the pelvis, so it stayed bare. `Wearer::hipFrom` (the
    thigh's rows start this far down the bone at the latest, of its length; `plateHipFrom` 0.12 the others): enforcer -0.5,
    death knight -0.3. Rows -0.9..6.0 -> -4.0..6.0 (enforcer), 0.9..5.9 -> -2.4..5.9 (death knight), 3 rows each, not 2. A
    humanoid thigh's rings above its region take at least the whole skin's exit (`fillRadii`). Thigh bulk (pop check, mean/most):
    enforcer 2.28/8.54 -> 1.34/8.65, death knight 0.56/1.14 -> 0.67/8.27. Triangles: enforcer 2 440 -> 2 584, death knight
    2 632 -> 2 776 (+144 each, one row per thigh; the knight already had 2 776; no timing taken).
  - **Open:** the enforcer's thighs from behind are still bare from the buttocks to the knee row (his back thigh skin is
    pelvis-weighted; the plates there sit under it); the knight's red pad wedge at the front of his right shoulder (a cuirass
    ridge of 2.6 on the knight hid it from behind but not the front, and raised his collar over his beard: dropped); the
    grunt's spiky knee leather and the enforcer's elbow band untouched.

## Armour tweaks file and the Armour Studio (branch `agent/armourstudio`)

His ask: "tweak the armour plates manually for each armed humanoid". Details: `docs/vr-port/ARMOUR_STUDIO.md`.

- **Data:** `quakevr/data/armour.txt` (`vr_armourdata.cpp`): per kind (grunt, enforcer, knight, death_knight, player, or a model's
  path) and part (cuirass, helmet, pauldron, bracer, thigh, shin, the four sleeves; a limb part both sides mirrored, `_l`/`_r` one
  side's own): offset, rotate, scale, standoff, start, end, rows. Laid over the generator in `buildShell`: start/end/rows in
  `planGrid` (`PieceFrame::shiftStart/shiftEnd/rows`; the upper arms' common top taken before them, `PlateGrid::hTopRaw`), standoff
  on the grid's radii after mirroring, the rest on the piece's vertices after it is made (`tweakVerts`: a sleeve's and the helmet's
  stand-off pushed out from their middle). Nothing set, nothing touched. Read at start, on `vr_armour_reload`, and when the file
  changes on disk (`vr_armour_watch` 1, once a second); a changed table makes again only the shells whose tweaks changed
  (`agarmor::overridesChanged`, `Shell::tweaks`), and the armour models with them. `vr_armour_print`. Cvars: `vr_armour_file`
  (`data/armour.txt`), `vr_armour_watch` (1).
- **Empty file = today's armour:** `vr_agarmor_info`'s 14 shell hashes (every wearer and body build) identical to `origin/next`'s.
  `vr_agarmor_check` 20/0, `vr_agarmor_look_check` 114/0, `vr_agarmor_pop_check` 10/0, `vr_agarmor_test 1` 26/0 (also with a bracer
  override on), `vr_agarmor_test 5` 24/0, `vr_pp_test 1` 53/0.
- **Studio:** Debug > Tools > Armour Studio (`vr_armourstudio.cpp`, the VR menu's own sliders: headset and flat screen alike): a
  subject ahead (QC `vr_armourstudio.qc` `VR_ArmourStudio_Spawn`, fists, `vr_agrunt_freeze` while open, takedamage off), its tier,
  turn, body ghost (`vr_armourstudio_ghost` 0.4: the body's alpha, the plates whole), the part alone; the part picked outlined in
  cyan (`agarmor::studioView`, through `debugDraw`); every value a `vr_armourstudio_*` cvar, each change laid into the table and
  the kind's armour made again (grunt about 40 ms). Undo (a slider's run on one value one step, 64 kept), Reset Part, Reset Kind,
  Save, Reload, Close. The player's kind edits your own body's plates (no subject or outline: the menu's body clone, a mirror).
- **Test:** `vr_armour_test` (`armourtest: 24 passed, 0 failed`; Debug > Tests): an override moves the right piece's vertices (the
  right side's mirrored) and nothing else, a sleeve's stand-off, rows; saved and read back; edited on disk and read again by the
  watcher; kept over a map change; the shipped file again the generator's shell.
- **Not done:** the generator's per-kind taste constants (`Wearer::hipFrom`, the stand-offs) stay in the code (moving them was
  optional; no output change wanted). A part's move or turn is not followed by the hit tests' tube (`PlateTube`), only its stand-off,
  start, end and rows; a player's debris models (`#plate<n>`) already loaded keep the old shape till the next map.

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

## Flat pose: the muzzle up at a wall; the walking wrist measured again (branch `agent/flatwall`)

His note 4c ("pushed back a bit or even have their muzzle pointed upwards if the player is right against a wall") and the
walking wrist (6).

**Measured first.** Every weapon (vr_test_give_wid 1-31) with his box against the vrtesthall wall, facing it and at 45, level and
looking 30 down or up, hipfire and sighted (`vr_gun_wall_depth`): 41 of 248 reads had drawn points over 2 cm in the wall, deepest
27 cm (a long gun looking down; the long melee weapons 78). The push back stops with the arm folded, the stock at the chest or the
rear sight `vr_flat_ads_wall_keep` from the eye; past that the gun went in.

**The muzzle up** (FLATPLAY.md "Against the walls"; Debug > Tunables > Flatscreen "Wall: ..."): what the push back can't clear,
the drawn gun tilts up out of: the least tilt round the hand about the view's right that leaves its points out of the walls
(`handpose::gunDepth`: walls only, a surface whose normal's up is over 0.7 is a floor or ceiling and not counted), at most
`vr_flat_wall_tilt` 85 and 5 short of straight up, moving at most `vr_flat_wall_tilt_speed` 300 deg/s. Drawn only: the game's
hand keeps the crosshair's turn (the shots 0.0000 deg off the crosshair's point while tilted), and the view puts the two-handed
hotspot back on the untilted gun (`flatplay::drawnWallTurn`; without it the tilt dropped holds). Also new:
`vr_flat_wall_arm_keep` 9 (the push back's arm limit, a constant before), `vr_flat_wall_tilt_pivot` 0 (1: round the shoulder;
tried, it swings the gun up into the face), `vr_flat_wall_tilt_sighted` 0 and `vr_flat_wall_tilt_drop` 12 (below). With the
tilt (sighted share 1): 3 of 232 reads over 2 cm, all looking 30 down or up with the box pressed on the wall, deepest 6.6 cm;
the long melee weapons (wid 12, 14) still go in looking down.

**Decision: sighted, no tilt by default** (`vr_flat_wall_tilt_sighted 0`, as before: the sights stay on the ray and the gun's
end goes into the wall). Tilted on the sight line its underside filled the screen (the 12 unit drop helps), and sighted looking
down 30-55 at the wall the gun's rear and the hands went 3-9 units into the chest (sweep wall rows). At 1 it tilts as in hipfire,
fading out looking down 30 to 50.

**Verified.** `Misc/quakevr/flatwall_test.sh` 7/0 (every weapon at the wall, sighted share 1; no hold lost; walked in and back
sighted: peak 20.6 deg, a frame's step at most 4.2, gone off the wall, the shots on the point). Sweep row by row (the same build
with `vr_flat_wall_tilt 0` against the defaults, origin/next merged): open and low rows identical; wall 403 passed, 3 known, 4
failed (base 407/3/0): `jump_gun` / `jump_hand0` of the long gun's hipfire at 20-30 down (4.7 / 3.7 deg of gun move per degree
of look, limit 1): the tilt itself growing as the look goes down into the wall. flatsight 12/0 (section d now runs with the tilt
off: it checks the push back), flatplay 28/0, flatkeys 37/0, flatads 7/0 (abcde), flatpose 20/1 (eye-far: the knight walked off
the crosshair, 0 pellets, the shot line 0.03 off; it passed the run before), flatwalk 10/2 (below).

**The walking wrist (open).** New `flatwalkturn:` line from `vr_flat_walk_probe`: walking looking down, each drawn hand turns
only 1.50 deg against the camera (both alike; level 0.16), and the free hand's look share stays 1.00. So the off wrist's 0.545 is
neither the free hand turning (the guess above) nor `steadyOther`'s blend: it is the arm's wrist moving against the drawn hand
(the arm's solve, vr_avatar.cpp). A screen-relative yaw sway was tried: no change (bobYaw is 0.3 deg), not kept.
`run_crouch_down` now also fails (wrist 1.142, jump 0.193) on origin/next: identical with the tilt off, so not this branch.

**His answer: tunables, an ease, no fire while raised** ("Give me tunables for the tilted pose, also including yaw and roll ...
transition time, start with 0.3s ... mirrored for the left hand ... inhibits shooting while in this tilted state").
- Pose: `vr_flat_wall_tilt_fixed` 0 (the least pitch that clears, as before; 1: always all of `vr_flat_wall_tilt`),
  `vr_flat_wall_tilt_yaw` 0 (-60..60, muzzle inwards) and `vr_flat_wall_tilt_roll` 0 (-90..90, top inwards), mirrored for
  the off (left) hand; the least pitch is searched with them on (`tiltTurn`: yaw about the view's up, pitch about its right,
  roll about its forward, in that order applied last to first). `flatsight::wallTurn` gives the drawn turn to drawnOffset
  and the stock's untilt.
- Ease: `vr_flat_wall_tilt_time` 0.3 s replaces `vr_flat_wall_tilt_speed`: the pose eases in (smoothstep) when the wall is
  met and out when it clears; in it the pitch follows the least that clears at 4/time; easing out keeps the pitch it had.
- Fire: `vr_flat_wall_block_fire` 1 (Movement > Locomotion, Desktop Screen Play, "No Fire With Gun Raised"): a gun turned
  more than 5 degrees does not fire (`flatplay::routeFire`; both buttons, either hand); melee weapons always swing. Note: a
  shot's own kick at a wall can push the gun in and raise it, which then blocks the next shots for the ease (seen at level:
  the click's recoil started a 20 degree tilt).
- Verified: `flatwall_test.sh` 15/0 (new c: eased in and out in 0.292 s at 0.3 and 0.583 at 0.6, measured from the first
  frame off zero; right hand up 0.643 side -0.262 cant -0.259, left hand 0.643 / +0.262 / +0.259 at pitch 40 yaw 20 roll
  15; new d: tilted at the wall 17 -> 17 shells, block off 17 -> 16, off the wall -> 15). Section b's peak check now 3
  degrees: walking in sighted, the way back clears the gun 4 frames after the wall is met, so the eased tilt reaches 5-9
  degrees there, not the 20 the 300 deg/s step reached. Sweep: the 4 wall rows are settled rows, which the ease can't
  reach (the least tilt itself is 45 at 20 down, 57.5 at 30), unchanged at 3.660 / 4.694 / 2.053 / 2.633: added to
  `flatsweep_known.txt` with a note (403 passed, 7 known, 0 failed).

**Turn lag cap and lead; the forward bob** (his words: "make the cap a tunable, and allow negative values of turnlag ...
Goldeneye-like 'turn with the camera'"; "a forward/back bobbing for the viewmodel ... a different tunable ... take into account
the camera rotation"). `vr_flat_turnlag_cap` 4 (was the constant), `vr_flat_turnlag` -3..3 (negative: the lag shown the
other way, the hands lead); `vr_flat_bob_fwd` 0 and `vr_flat_ads_bob_fwd` 0.15 (FLATPLAY.md "Bob and sway"). The walk probe
prints `flatwalkfwd:` (hands' and shoulders' range along the camera's forward, the main hand's yaw off the view);
`Misc/quakevr/flatlagbob_test.sh` 5/0 (numbers in FLATPLAY.md). One commit for both (they share vr_flatplay.cpp's hunks).

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

## Night regression's new failures (branch `agent/regfix`, 2026-11-06)

The night run (`run_baseline.sh --no-bench`: agrunt cases undrawn, `vr_mock_fast 2`, `sv_random_seed 1`). Bisected on a
second tree; none was a gameplay regression, each a test that no longer fit:
- **78 dirparry** (43/4): not the melee rules. Drawn it is 47/0 before and after; undrawn its weapon-line cases (13-19)
  measure the line against the renderer's matrix (`debugdirparryline`), 2.6-3.4 units off with nothing drawn (3 of
  them failed undrawn before the melee-rules merge too). "directional parries 0" there is not judged: those cases judge
  only the line. The test now sets `vr_mock_fast 1` for its line cases when the run is undrawn (back to 2 at its end):
  47/0 undrawn.
- **202 impact** (9/4): his default `vr_agrunt_fall_keep_impact` 12 (promoted 2026-11-03) is over every case's landing
  (fall 9.4, thrown into the floor 8.5, into a wall 8.7, banged 4.1 m/s): they keep the weapon by his tuning. The test
  pins the rule's own 4.5 for its run (restored after): 13/0. **For him:** at 12 a throw or a fall from height keeps
  the weapon; that is his value, not changed.
- **203 dualdisarm** (80/1, seed 1 only; seeds 2 and 3 81/0): case 4's knight took the sword lying at its feet up again
  within the 10 frames after the last knock (`VR_AG_TakeBegin`: the weapon its own at once). The check accepts a take-up
  begun after the knock (printed). Drawn, seed 1 fails case 11 (Nailgun "going for the fallen one") instead: not
  looked at (the regression runs undrawn).
- **91 raggrab** (0/1): from the embedfix merge on (bisected; view-code changes only: a chaotic shift of the ragdoll's
  pose), 'lying forearm' lay on its forearm, and the grip took its worn backpack. The test's grunt now wears none (as
  test 202 removes them): that try is "no state" (the hand never got there), 1/0.
- **74 downtug** (0/1): from the armorlook merge on (bisected; first bad 329a7a1d9; `vr_agarmor_pieces 0` doesn't change
  it: a chaotic shift too). 'shove down grip fast': the grip took a limb, the fast pull struck the shotgun out of its
  hand (a strike disarm, 4.1 m/s), and the hand held the body for 7 s: the test called it STUCK. No tug, weapon gone:
  now "took its body" (not ok, not stuck): 1/0 (14/17, stuck 0).
- **213 lost soul** (needs AD; night branch's wake fix cherry-picked): still 0 parries, the soul 141 units away with no
  enemy. Not fixed: open (why the woken soul drops its enemy).
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

## Animation recorder, phase 1 (branch `agent/animrec`, 2026-11-06)

His ask: record motions in VR (melee strikes per weapon kind, reloads) to drive the flat-screen hands later, with
per-animation and per-weapon tweaks and the hand jitter smoothed out. Phase 1 built: the recorder (Debug > Animation
Recorder; `vr_ar_record`; the armed record button, its choices shared with the motion recorder through `vr_recbutton.hpp`;
a countdown), the `.anim` text file (`quakevr/anims`: the head and both hands relative to the head and to the body, the
fingers, grip and trigger, the weapons, and the events fire, pump, load, mag_out/in, pouch_take, rounds_in,
break_open/close, each with the nearest hotspot), the smoothing (`vr_ar_smooth` and `Misc/quakevr/anim_smooth.py`, the
same keys), and a preview on the flat-screen hands and as a ghost in the headset (`vr_ar_play`: blended in and out,
mirror, offset, rotation, scale, speed, loop). The format, the smoothing, and the phase-2 plan with his four design
notes (retargeting to each player's hotspots, mirroring, blending from any pose, what stays procedural): ANIMATIONS.md.

Test: `bash Misc/quakevr/animrec_test.sh <agent>`: record 11/11 (145 frames -> 46 keys, 3.80 mm / 0.52 degrees within
4 mm / 1.5), anim_smooth.py the same 46 keys, flat play 3/3 (on target to 0.000 units; blending steps 2.26 against 1.41
units a frame played; level, looking up, looking down, crouched).

**Checklist lines (suggested):** Animation recorder: set `vr_ar_name`, Debug > Animation Recorder > Start, do a shotgun
reload after the countdown, Stop: the file in quakevr/anims lists your steps (mag, pouch, pump). -- Smooth Last, then
Play Last: the ghost follows your motion without the shake; Mirror plays it with the other hand.
## Every other model's source is a .blend (branch `agent/blendmodels`, 2026-11-06)

The author: "Let's make the '.blend' the source of truth for all weapon models, then we can do the same with other models
too." Part two, after the weapons.

**Inventory** (`quakevr/progs`: 180 `.mdl`, 10 `.md5mesh`; 68 owned by the weapons' sources). The other 122: 102
real `.mdl`, 10 md5 meshes and their 10 244-byte `.mdl` stubs. MODELS_IN_BLENDER.md, "Every other model", has the
groups and what made each: our gear and props (19 generators), the unrigged hand (make_bloody_hands.py's blood skins),
id's models we ship changed (make_enemyshove, make_voreshove, make_grenade_skins; make_vrgrunt.py's armed models cut
from them, unguarded until now), shipped as they are (49: id's monsters, gibs, heads, items, projectiles; wpnbutton,
hand). Not migrated: `vragarmor.mdl` (an atlas on a placeholder quad; the plates are cut by the engine at load), the
md5 body and hands (below).

**What changed:**
- 96 new sources under `Misc/quakevr/src_models/blend/<model>/` (42 MB), made by `blend_build.py`; generated.json
  records their 101 files (and normal maps, full-colour skins) as owned.
- `source.py`: frames without names, or with one name for all (id's player, dog, soldier; the shambler's "From ASE"),
  were refused by the frames check (the shape keys get `frame<n>` and `.001` names): `key_names` gives the check
  Blender's names. `post` specs: skins after 0 made from skin 0 by the generator's code at each export (the hand's
  and fingers' blood: make_bloody_hands.py `process`; the grenades' unarmed skin: make_grenade_skins.py `mute`).
  `derived` specs for the armed monsters (soldier, enforcer, knight, hknight write vrgrunt, vrenforcer, vrknight,
  vrhknight: make_vrgrunt.py's new `build`) and the molten debris (vr_rock1 writes vr_explosion_debris:
  make_explosion_debris.py's new `build`); a derived generator's failure (vertex numbers it needs) is a refusal.
- Retired: make_vrgrunt.py, make_explosion_debris.py and taper_hand.py had no guard: they now stop
  (`genguard.retired`) for owned files. The guarded generators stop by themselves (all files owned) or leave the owned
  ones (make_flashlight.py still writes its sounds).
- `blend_autoexport_test.sh`: `BA_MODEL=<model>` tests another source (default v_shot); the .py skips a normal map the
  model hasn't.

**Proof.** Every source built to a scratch folder and exported (`source.export --out`): 102/102 byte for byte (the
models, their normal maps, full-colour skins, derived files, the post-made skins). Then for real: built, and each
exported into the repository's `quakevr/progs` (`blend_export.py`, as export on save): 96/96 ok, `git status
quakevr/` clean (every file byte for byte, so the game draws the same bytes: a before/after pixel compare would compare
identical inputs). Every retired generator run (no arguments, then `--force`; `py -3.13` for the numpy ones):
make_debris, make_crates, make_brains, make_bloody_hands, make_grenade_skins, make_shell, make_enemyshove,
make_voreshove, make_holster, make_muzzleflash, make_rounds, make_teeth, make_pouch, make_ammo_pouch, make_walltorch,
make_gadget, make_pauldron, make_vrgrunt, make_explosion_debris stop ("retired"), make_flashlight leaves its two
files and writes its sounds: `quakevr/`, `Quake/` and generated.json unchanged. Export on save with the game watching
(`blend_autoexport_test.sh`): v_shot 6/0, `BA_MODEL=soldier` 6/0 (its export writes vrgrunt.mdl too; the game reads
soldier.mdl again by itself), `BA_MODEL=vrgadget` 6/0 (a one-frame model: the refusal adds a shape key).
Game tests on the props (low-impact runs): crate_grip 10/0, explosion_debris 13/0, gadget_tap_melee 11/0, flatitems
8/0; holsterzones 9/4 (the branch's `quakevr/`, `Quake/`, `QC/` are origin/next's byte for byte: not from this
work, not looked at).

**The md5 models (plan only).**
- **The jointed hand** (`hand_rig`, `hand_rig_f`): the hand add-on already imports and exports it (bones, weights,
  skins), byte for byte but the `commandline` line. A source .blend would take: the commandline kept on the
  armature (then byte for byte); the export also writing `Quake/vr/vr_handrig_data.inc` (make_hand_rig.py's tables
  compiled into the engine, which measure edits against them: an engine rebuild after a hand edit, or the engine
  reading the tables from the file instead); the four `.lmp` skins' blood levels made from the clean one as
  make_hand_rig.py does; the feminine hand a derived file (make_hand_feminine.py's code on the exported hand). About
  a worker's day; the hand add-on's import and export stay as they are.
- **The body** (`vrbody*`): make_vrbody.py makes four builds (lean, athletic, brawny, feminine) and their bare
  variants from one parametric body, and 16 shared TGA skins (armour x damage). A .blend per build would cut that
  link; the source would be one .blend of the base body with the builds and skins derived by make_vrbody.py's code
  at export (as the armed monsters are), and the body add-on's export (section 3) kept for the builds. Larger: not
  started.

## Gloves in Quake's art style (worktree `embed5`, branch `agent/gloves`, 2026-11-06)

His words: the gloves look very high-res up close in VR, and the hand and the wrist don't match in texture/art style.

**Measured** (`scratch/gl_density.py`: area-weighted texels a mm over the meshes, at their in-game sizes): the hand's skin
(512, Quake palette) 1.31 (quartiles 1.19-1.52); the body's forearm (`vrbody`, 512 x 2048, palette, dithered ramps)
1.04 (0.91-1.29), its wrist 1.33, upper arm 0.81; id's weapons 0.12-0.21 (at 38.1 mm a unit). The glove paint was
1024 (2.63 a mm, twice the skin), full colour, with a ~1.7 mm pebbled grain painted in, and the shader added a finer
procedural pebble grain (~0.7 mm) and its own pebble relief on top: 2-4 times finer than anything else on the avatar,
in continuous colour, while the skins next to it are nearest-filtered palette texels (his `gl_texturemode`
GL_NEAREST_MIPMAP_LINEAR). With his bare forearms (`vr_body_bracers` 0 is his setting) the glove's cuff on the forearm
is the plain procedural leather on the body's grid while the hand had the fine painted one: the wrist seam.

**Changed:**
- `make_glove_paint.py`: paints at twice the skin's grid and averages down to the skin's own 512 (SHIP), drops the
  pebbles (blotches of a few texels instead), creases strong enough to stay a colour step, and paints the light in as
  Quake's skins do (CAVITY: a texel's height over its 5 x 5 neighbours: ridges, thread, seam lips paler, grooves
  darker). The shipped `hand_rig_00_00_glove.png` / `_glove_height.png` are 512 now (regenerated, guard updated).
- Shader (`vr_glsl_alias.h`): `vr_glove_retro` 1 (new, default 1; Player Appearance > Gloves > Quake-Style Leather):
  the leather's texels (the paint's; the plain leather and the cuff on the skin's own grid, hand or forearm) read as
  blocks at their centres whatever the filtering, no sub-texel grain or grain relief, and `LeatherSteps` puts each
  texel's brightness on one of 16 ramp steps in the chosen hue (never black), dithered per texel within half a step
  (the body's `pick()` dithers its ramps per texel too); fades to plain mipmapping where texels get under a pixel.
  Glove.w carries it as 16s. 0 gives the smooth reading (with the old fine grain) of the new paint.
- Both sides of the wrist are now the same treatment at about the same density (hand 1.3, forearm cuff ~1.0-1.3
  texels a mm) and the same steps of the same colour: no change of detail at the seam.

**Tested** (low-impact runs, his game was open: 512 eyes): `scratch/gl_shots.sh` (start map, four looks: his black
fingerless with bare forearms, brown full with bracers, brown fingerless bare, brown full plain bare; two poses each),
sheets `scratch/gl_cmp_side.png` (close, exposure x3 for viewing) and `scratch/gl_cmp_back.png`: up close the
leather is texel blocks in two or three colour steps with the creases and seams a step darker, like the bracers and
the arm's skin; at arm's length the two looks are nearly the same (mipmapped). A dark checker on the full glove with
bracers on is in the before shots too (a shadow, not this change). `vr_gloves_reload`, `vr_hand_reload`, both hand
shapes: paint read 512 x 512, relief 1.7 ms, no errors. No dedicated glove test exists.

**For him in VR:** hold the hand close (10-20 cm) with his black fingerless gloves and with a brown pair: the texels
should read like the forearm's skin, no fine pebbles; the wrist where the cuff meets the hand shouldn't change in
detail; toggle Quake-Style Leather to compare. Not shipped as his look: the coordinator shows the sheets first.

**Checklist line (suggested):** Gloves up close: texel blocks and colour steps like your arm's skin, no fine grain, the
wrist seamless (Player Appearance > Gloves > Quake-Style Leather toggles the old smooth look).
## Why the lost soul "drops its target" (branch `agent/lostsoul`, 2026-11-06)

It doesn't: test 213 misread AD's design (AD's QC, our bridge untouched).

- **The "no enemy"** is AD's enemytarget marker (`ad_ai_enemytarget.qc`): a flying monster's `enemy` becomes a
  classname-less marker floating 128 above you (`ad_LostCheckAttack` sets it up; `self.height` = `ad_MONAI_ABOVEDIST`),
  the real enemy behind it. Traced every 0.25 s: `enemy` the marker, `ad_lostenemy` world, `ad_lostsearch` 0, never the
  guard state (its lost-sight branch never ran). The player's flags, notarget and god play no part.
- **Why no bite:** `ad_LostCheckAttack` bites only within 60 (flat, `ad_ai_checkmelee`), rams only 200-400 away
  (`ad_MONAI_RANGELOSTNEAR`/`FAR`) and strafes round you between (`ad_AS_SLIDING`, `ad_ai_run_slide`, 8 units a step).
  Spawned 96 ahead, it circled a still player at 90 to 195 for the whole 8 s (the circle widens about 2 units a second;
  walls push it back in), never far enough to ram. Its bite comes after a ram (`ad_lost_melee1` from the touch).
- **After a ram** (traced): a crowbar across your face parries the ram itself and the soul bounces off (300 a second,
  flying, no friction) and chomps the air; a ram that lands shoves you back 200 a second (AD's `ad_lost_charge_touch`),
  so the chomp's bite (0.3 s on) finds you 80 away, out of reach: a still player on open floor is never bitten. A real
  player sees AD's behaviour: rams from range; bites when you are pinned (a wall at your back) or step in.
- **213 now** tests that: the soul spawns at ram range (300, or the way round you with the most room), your guard is
  down for the ram (the mock hand at your hip), you hold your ground (velocity held 0 through the ram and chomp: a wall
  at your back), your guard comes up as it chomps, and only a parry in its chomping frames counts (a parried ram is
  counted apart). The verdict prints the real enemy through the marker. PASS on start, vrtesthall, ad_e1m1 (AD's own
  map, 9 lost souls), and start without god; each ends on its verdict in about 4.5 s of game time.

**Checklist line (suggested):** AD (ad_e1m1): a lost soul rams you from range and circles you when near; with your back
to a wall its bite after a ram lands, and your crowbar's guard parries it.

## The grunts' burst rifle made anew (branch `agent/burstrifle`, 2026-11-06)

`progs/v_gruntgun.mdl` built from nothing after Vittorio's reference (a compact burst carbine): `Misc/quakevr/models_src/
burstrifle/build_burstrifle.py -- --from-scratch` (Blender 5.2, headless) makes the shapes (508 triangles, flat faceted,
the main edges bevelled), the UVs, a palette skin painted by script (512 x 512: gunmetal, blued barrel, post and ears,
brass rivets and band bolt, oiled wood grip, rubber pad; light grime from occlusion, a little rust, worn convex edges;
the red lamp glows: 16 fullbright texels), writes the model (nine frames `frame1..frame9`, packed from the old corner
-29.9 -6.17 -4.6 so slot 21's settings and the AD variants hold it), bakes its normal map, writes the anchors and then
makes the source .blend from them (`src_models/blend/v_gruntgun/`, the generic layout: the old migrated one is in git's
history). An untouched `blend_export.py` of the new .blend writes the model and the map byte for byte.
`render_burstrifle.py` renders the .blend beside the reference.

- **Kept:** the origin in the pistol grip's middle (the grip z -3.9..3.8, leaning back 12.5 deg), the bore (z 8.08), the
  muzzle anchor (33.15 0 8.08), the corner. **Changed from the image (agreed):** a clamp band on the barrel (x 22.0-23.5,
  hotspot 1 on its left: hs1 -9.3665 -3.365 0.1797, the hotspots' frame = 0.61 * corner + 0.39 * the model point); the
  front post on the muzzle block, its top level with the rear ears' tops: the sight line z 11.6 (was 14.9: 3.5 over the
  bore, was 6.8), rear -2.15, front 28.15 (vr_sightalign_table.inc, sight_picture.py, sightline_table.py).
- **Shorter behind the grip:** the reference's butt is much nearer the grip than the old gun's; the butt pad's back is
  at x -24.0 (was -29.4): 57.2 units long (was 62.8), 85 cm at Scale 0.39. The stock drops 2.6 under the sight line at the
  butt (the flat ironsight's cheek room).
- **weapons.txt:** slot 21 and the AD slots 27-30 (they repeat its anchors): muzzle_av, hand_av/wpnbtn_av (the
  receiver's right side), 2h_hand_av (the band's underside), wpntxt_av (the housing's top, at its back), hs1; slot 21's
  barrel mask (the barrel, band, muzzle block, post).
- **Verified:** sight_picture: the front post seen 0.42-0.67 units down from its top in the notch; flat
  `vr_flat_sight_check` rear/front/far off 0.0000; flat hipfire and ADS screenshots (the post in the notch);
  `vr_agrunt_test 85` 40 passed, 0 failed (the burst rifle's shots from its own muzzle, off 0.0).
- **Not verified:** headset `vr_sight_check` (the mock's main hand had no weapon after `impulse 149` in e1m1 and
  vrfiringrange: a test-setup question, the flat run held it); the AD variants (vr_adgunart.cpp's add-ons were placed on
  the old shape: the fuelled guns' canister x 5-12 under the receiver now crosses the magazine, the hose inlet at x -11.5
  sits by the lower stock rod); the soldier's own held gun (soldier.mdl) is still the old shape, so a dead grunt drops a
  gun that looks different from the one he held.
- **Files:** the normal map is 1.3 MB (the skin is 512 x 512, was the soldier's 256 x 260). `progs/v_gruntgun.mdl_0.clean`
  (a blood patch for the old skin) no longer matches: to delete.

**Checklist line (suggested):** the grunts' burst rifle (pick one up from a dead grunt): the new model, held by its wooden
grip, the other hand on the barrel's band, the front post level in the rear notch when aimed, the red lamp glowing.

### Round two (his notes on the comparison: blockier than the mockup; the AD guns keep the old rifle; real muzzle holes)

- **The AD rifles keep the old model.** `progs/vr_adrifle_base.mdl` (+ `_0_norm.png`) is the old `v_gruntgun.mdl` byte for
  byte (cmp against origin/next), with its own source .blend (`src_models/blend/vr_adrifle_base/`, blend_build.py; its
  untouched export is byte-identical). vr_adgunart.cpp builds the `#adv<n>` variants from it (and stands it in when one
  can't be made), vr_adguns.cpp's AD-soldier stand-in is it, vr_skinrelief.cpp's rifle map is its map; weapons.txt slots
  27-30 are origin/next's exactly. Same input bytes, same code: the variants are unchanged. The plain burst rifle
  (`v_gruntgun.mdl`: the dropped gun, QC's WID_GRUNTGUN, the armed grunts' burst rifle, the tool gun's spawn, gunfx) is the
  new one.
- **Shape:** the receiver's front is a heavily chamfered nose block with brass bolt heads (geometry) and an octagonal
  collar stepping down to the barrel; the grip raked 20 deg, bigger, in a steel frame under the receiver; a thin swept
  trigger guard loop and a curved trigger; the stock's two parallel rods (the upper from a collar at the receiver's top
  back) into a tall chamfered butt block (z 3.0-10.4) with a ribbed pad.
- **Real holes:** the muzzle block's bore (0.49 deep, 8-sided; the muzzle anchor on its bottom, 32.6554 0 8.08, on the
  model's x grid) and three 6-sided vents through it, cut by Blender's boolean (the float solver: the exact one left the
  vents uncut), dark inside.
- **Paint:** warmer gunmetal with broad light/dark patches, wider lighter worn edges, darker recesses, thin scratches;
  brass rivets with highlights; the red lamp a glowing lens (49 fullbright texels), the bronze one a domed lens; dark
  window glass in a raised frame; ring grain and pores on the walnut grip.
- 746 triangles (the holed muzzle block 168 of them). Flat sight check 0.0000; sight_picture: the post seen 0.42 down;
  `vr_agrunt_test 85` 40/0, `vr_agrunt_test 12` (AD guns) 21/0.

### Round three (his notes: "the stock shape is still not triangular like the mockup and the wooden handle is still too straight")

- **Stock:** an open triangle tapering to the receiver, as the mockup's side view: a chamfered steel adapter block on the
  receiver's back (x -4.9..-2.8, z 6.2-9.95, two rivets), the upper rod level from its top (z 9.3) to the butt block's top,
  the lower rod from its lower part (z 7.0), a short mitred elbow, then down and back to the butt block's bottom (z 2.3):
  the rods 2.3 apart at the adapter, 7.0 at the butt. The butt block upright and tall (z 1.3-10.5, 1.3 x the receiver),
  the pad behind it a lighter rubber. Rods thicker (r 0.58, was 0.5). (The round-two sockets are gone.)
- **Grip:** lofted through seven sections (eight sides each) along an axis raked 24 deg (was 20) through the origin, bending
  back a little more towards the foot: a narrow web under the receiver swelling into the palm at the back, the finger
  curve under the guard and a middle-finger bump in front, a flared foot with a bevelled bottom edge (102 triangles). The
  origin (the fist) is inside its section at t 0.39 as before; the steel frame's back follows the grip's back. Wood grain
  runs along the curved axis (worked out per point from the loft's sections), fewer pores, a lighter walnut.
- **Also:** the receiver 0.6 taller (z 3.1-10.3, mostly downward: the sight line, ears and post are unchanged; the housing
  top 11.15, `wpntxt_av` follows); lamps 1.2 across (were 0.8) with round lenses, a brass rivet beside them; warmer
  brown-grey steel, blued parts and rods (the reference's receiver measures R:G:B about 1 : 0.81 : 0.71).
- **The comparison render** now splits his picture where it is split (576 rows first person, 448 turntable; it assumed
  512 each) and frames the first person with a camera fitted to seven points of the reference's gun (28 px rms); the
  turntable nearly orthographic, 27 deg round to the front, 22 above.
- 822 triangles. sight_picture: the post seen 0.61 down from its top; flat `vr_flat_sight_check` 0.0000 and the post in
  the notch in the ADS screenshot; `vr_agrunt_test 85` 40/0; an untouched `blend_export.py` of the new .blend writes the
  model and its map byte for byte; `vr_adrifle_base` untouched.
## Shooting yourself (branch `agent/selfshot`, 2026-11-06)

Vittorio (BACKLOG): "the player should be able to damage themselves in VR by pointing their weapon at their own
limbs/body and firing", and the wound "precisely where the hit is registered on the player's arms/hands/body".

- **The hit geometry** is the wounds' (what an enemy's hit on you is painted by): capsules round the drawn torso, legs,
  each forearm (elbow to wrist) and hand, as posed. It moved from `vr_wounds.cpp` to `vr_selfshot.cpp`
  (`selfshot::build`), each capsule now tagged with its part; the wounds use it unchanged. For your own shots the torso
  stops 6 units under the eyes (`chinUnderEyes`): **the head is no part** (seen from inside it; a gun at your face is
  never met), though the wounds' torso reaches round the head for enemy hits.
- **Who:** only the listen server's own player, in a headset (`VRHandsTracked`, not flat-screen play), `vr_selfshot 1`
  (default on). The engine draws no body for a co-op guest or a bot, so they are never met (`selfhit_segment` -1).
- **What is never met:** the firing hand's own forearm and hand; the other arm too while both hands hold the gun
  (`VRGet2HAiming`). A hitscan shot is tested from the muzzle on (its trace starts behind the muzzle,
  `VR_ShotBackPoint`: what lies along the gun, the grip, a hand on the fore-end, is the gun's).
- **Hitscan** (`FireBullets`: shotgun, super shotgun, revolver; each pellet): `VR_SelfShot_Pellet` after its trace,
  armed by FireBullets alone (`vr_ss_armed`: monsters' and the tests' FireBulletsImpl calls never meet the shooter).
  **Lightning:** the bolt ends at your body and strikes you (`VR_LightningHit`: the shock arcs too). Not through a
  teleporter (your image is met there as before).
- **Missiles** (every hand's: `VR_ShotLaunched` marks `.vr_ss_skip`): `SV_PushEntity` calls `VR_SelfShotMove`, which
  stops the move at your body (from where it starts: a muzzle pressed into your forearm hits it at once), reports you as
  struck (`.vr_ss_struck`), once; `VR_OwnerSpared` lets that touch through. Nails, rockets, grenades, lasers, the
  expansions' and AD's missiles whose touches use `VR_OwnerSpared`.
- **Damage** takes the normal path (T_Damage: armour, god mode, Quad, teamplay) times `vr_selfshot_damage` (1) in place
  of `vr_damage_self`; the armour plate nearest the point is shed (`vr_pp_hit`); a hit on a forearm or hand buzzes that
  hand (0.25 s, 90 Hz).
- **The wound** is sent marked `QVR_WOUND_PRECISE` (128 on the kind) at the point: the client paints it where the line
  from 2 units before it meets your drawn capsules (no spread, no jitter, no room-scale shift; a point already inside:
  there), on the body and both hand models (it lands on whichever is there) and it moves with them (texture space).
  `vr_wounds_debug` as before; `vr_selfshot_debug 1` prints each self-hit (2: every test).
- **Blades:** not done (a swing past your other arm is ordinary melee; cutting yourself would be accidental too often).
- Menu: Damage and Knockback (the "Gameplay" damage multipliers) has Shoot Yourself and its damage; VR Settings >
  Weapons and Bullet Time has the toggle. Debug > Tests: Shoot Yourself: Aimed / Ordinary Firing.

**Tests** (`vr_selfshot_test`, mock headset, e1m1): **1** aimed: a pellet at the off palm, the off forearm, the thigh,
a nail at the forearm, a nail started inside it: each hit its part, 3-14 damage, the off hand buzzed, the wound 0.00-0.15
units from the hit on that part; a line through the head, `vr_selfshot 0`, another player: never. 23 passed, 0 failed.
**2** ordinary firing: ten poses (one hand forward, dual side by side, arms crossed, the off hand cupping the grip,
pointing down, kicked up, from the hip, turned out, turned in, at the floor ahead), both hands each firing a six-pellet
blast, a nail and the lightning's line along their aims: 0 self-hits. (The mock's controller pitch 70 is a level aim.)
`vr_selfshot_debug 3` takes an eyeshot of the wounds at the end of test 1.

**Checklist lines (suggested):** In a headset, point your gun at your other palm, forearm, thigh and fire: it hurts, that
hand buzzes and the wound is right where you aimed, following the arm. Normal firing (dual, crossed arms, two-handed, from
the hip) never hits you; Shoot Yourself off (Damage and Knockback) and nothing happens.
## The gadget locked while both hands hold one thing (branch `agent/gadgetlock`, 2026-11-06)

His note: bullet time's double tap still turned on by accident in fights with both hands busy. New
`Quake/vr/vr_gadgetlock.cpp` (`gadgetlock::locked()`, worked out once a frame): while both hands hold the same thing --
one weapon two-handed (`twohand::helping`: a hotspot, a free grip, a blade grip, a cup), the same carried prop or crate
(the carry stats equal), the same embedded blade (the embed stats), the same enemy (foegrab, the listen server) or the
same ragdoll (`box3d::ragdollHold`) -- and for `vr_gadget_lock_grace` (0.2) real seconds after, the gadget takes no
input. Gated: the screen tap (bullet time: no tap, a double tap's first dropped, the hand must lift off before the next),
the side button (gear lights, Doom's opening presses) and AD's page button (a fingertip on one must leave it first), and
the touchscreen when an app takes the gadget's screen (Doom: a touch under way ends). Different things in each hand:
unchanged. `vr_gadget_lock_twohand` (1) turns it off: HUD and Menus > Screens, "Lock While Both Hands Hold One Thing"
(the grace a developer row under it). `vr_debug_bullettime` or `vr_debug_gadget_button` print its changes ("gadget
lock: on (both hands hold ...)", "let go, 0.20 s grace", "off"). The older guards (`vr_bullettime_tap_twohanded`,
`vr_bullettime_tap_both_held`) are kept as they were.

Test: `Misc/quakevr/gadget_lock_test.sh <agent>` (fixed frames, ends on `gadget_lock_test: N passed, M failed`; 18
passed): two different weapons count; a shotgun two-handed, a sword in both hands and a crate in both hands lock, and no
tap counts once locked (4 taps each); with the lock off the two-handed shotgun's double tap counts; letting go of the
shotgun's foregrip then double-tapping at once: the first tap (inside the grace) is ignored, the second counts as a
first; the side button pressed during the hold does nothing, after letting go it presses. Test aids: `vr_2h_sticky 30`
and `vr_carry_two_hands_drift 60` keep the holds while the mock's tracked main hand reaches the off wrist's screen (as
shipped it would pull the hand off its grip).

**Checklist line (suggested):** Wrist gadget: hold the shotgun two-handed, or a crate or the sword in both hands, and
double-tap the gadget's screen: no slow motion (and the side button does nothing); let go and double-tap after a moment:
it works; one weapon in each hand: it works as before (HUD and Menus > Screens, "Lock While Both Hands Hold One Thing").
## Flat hit feedback (branch `agent/flathit`, 2026-11-06)

His words: "reduce the amount of movements the arms/weapons get when the player is hit during ADS, tunable through a new
value. I think 0.2x would be a good start"; "add a tunable camera aimpunch + directional hit effects ... Make everything
tunable"; "Like for VR, add a blood vignette when the player is low on health ... flatscreen-specific size and opacity
tunables". FLATPLAY.md, "Hit feedback" has the whole of it; `vr_flathit.cpp` the code.

- The hands' knock on a hit (`vr_painknock.cpp`) is scaled in `vr_view.cpp` by `flathit::keepHitReact`: 1 hipfire,
  `vr_flat_ads_hitreact` (0.2) sighted, by the ironsight's smoothed travel (as `vr_flat_ads_recoil`).
- The punch goes into `cl.viewangles` as each frame's change of a sum of per-hit pulses (8 kept), so the aim and the
  crosshair never part: the shots go where the crosshair is during the punch, and the aim returns exactly to where the mouse
  put it when the punch settles. Pitch stays inside `cl_minpitch` / `cl_maxpitch`; what the clamp refused is not owed back.
  Defaults: 2 degrees for a 20-point hit, at most 6, 0.05 s in, 0.3 s back, half sighted. Quake's own view kick is left as
  it was (a look only).
- The marks are drawn in `SCR_UpdateScreen` before the crosshair (`VR_FlatHitDraw`, on the crosshair's canvas) with a new
  `Draw_FillQuad` (gl_draw.c): 9 quads an arc, ends faded. Damage gives the mark half to all of its opacity.
- The low-health blood was already drawn in flat by the shared post-process; `vr_blood.cpp` now takes its size and peak
  opacity from `vr_flat_blood_health_size` / `_opacity` while flatscreen play is on.
- Tests: `bash Misc/quakevr/flathit_test.sh <agent>` (e1m1, a real grunt firing from the right; each run ends on the
  trigger's own line): punch 1.47 deg left in the aim 4 frames on, 0 after settling, the picture moved 6 px (the turn says
  9.2 at 960 px, within its margin); the mark's ring pixels right 381, others 0, the probe's mark at 91.5 deg; knock
  share 1.000 hipfire, 0.200 sighted (1.934 -> 0.387); punch sighted 0.478 of 0.956; vignette edge +30.3 redness at
  10 health, middle 0.0. 10 of 10. Flat suites after it: flatads 7/7, flatplay 28/28, flathud 11/11, flatsight 12/12,
  flatvisuals 6/6.
- Test aids: Debug > Flatscreen Play > "Flatscreen Test Hit From The Right" (`vr_blood_test packet right`) and "Flatscreen
  Hit Probe" (`vr_flat_hit_probe`); `vr_flat_hit_test a, b, waitN` runs commands at the next hit.

To test in flat: get shot from each side, hipfire and sighted: the view's punch and its settle, the marks' place and fade,
the arms' smaller knock sighted; drop under 35 health for the beating edges. Tune under Tunables > Flatscreen > Hit Feedback.
## Installer: the settings page after installing (branch `agent/instset`, 2026-11-06)

His request: an installer page for the most common settings, easy to add/remove, organized in subcategories with a
tree view in the installer's look, and never stale. Players asked to set them before the first start.

- **Done**: step 6 **Set up the game** (between Install and Play): categories > subcategories > settings, the right
  control per kind, the game's help and menu path, Reset to default, Reset all. The list is
  `Misc/installer/settings_page.txt` (cvar names only); labels, help, ranges, choices and defaults come from the game's
  menu rows (`vr_menu_dump_settings`, new) into `quakevr/settings_page.json` (shipped; Setup reads it from the install).
  Full design, order and how to add a setting: INSTALLER.md, section 14.
- **Writing**: only the page's changes, to `quakevr/installer_settings.cfg`; `quake.rc` runs it once after the saved
  config (`vr_installer_settings`, stamp kept in the new archived cvar `vr_installer_applied`), before `quakevr.cfg`.
  In-game changes later win; upgrades start from the player's `ironwail.cfg`.
- **Staleness**: `settings_page.py check <agent>` fails on a missing, unsaved or changed setting or a stale manifest;
  warns on a removed menu row; in `run_test_suite.py` as `settings_page` (`Misc/quakevr/settings_page_test.py`: 21 checks,
  including the failures on purpose and three game runs: applied, an in-game change kept after a second install, not run
  again).
- **Starting list**: Comfort (Turning `vr_snap_turn`, Turn Speed, Vignette, Position seated/standing
  `vr_bodycal_seated`), Your Body (Handedness `vr_handedness`, Gloves, Body Type), Gameplay (Replace All Humanoids
  `vr_agrunt_repl`, Gore), Graphics (Graphics Presets). **Left out: VR or flat screen**: `quakevr.cfg` forces
  `vr_enabled 1` at every start, so it isn't a saved setting; the flat-screen shortcut and the Play page's "Play on the
  monitor" cover it. Handedness and Graphics Presets are presets (applied once, "As it is now" on an update).
- **Test in VR**: install into a fresh folder, change a few settings on the page (Snap 45, left-handed, Medium), start:
  they are set; change Turning in the game, run Setup's Update and change only Turn Speed: Turning stays as set in game.

Suggested checklist lines:
- Installer: after installing, the Set up the game page shows the settings tree; picks are applied at the first start.
- Installer: after an update, the page shows your current settings; a change made in the game earlier is kept.

## One Blender add-on, a shorter loop (branch `agent/blendaddon`, 2026-11-06)

His ask: .blend as the source of truth for every model, a simpler workflow, one add-on, engine help. The page for him:
[BLENDER_WORKFLOW.md](BLENDER_WORKFLOW.md).

- **One add-on, "Quake VR".** The hand's add-on (`quakevr_hand`) is `quakevr_models/hand_blender.py` now and
  `md5hand.py` moved beside it (make_hand_rig.py, make_hand_feminine.py, normalbake.py follow). The package keeps the
  name `quakevr_models` (decision: his Blender stays ticked with its preferences, and blend_export.py and the tests'
  imports don't change); only the shown name changed. On a windowed start it takes the old `quakevr_hand` entry out
  of the preferences (marked dirty: Auto-Save keeps it). One panel finds what the .blend edits (a model's source, a
  hand-made source, the jointed hand, the body, an imported .mdl: that one gets **Open Its Source .blend**, from
  generated.json) and shows that kind's tools; the imports sit at its foot. Operator ids kept (`quakevr_hand.*`).
- **Export on save for every kind.** A .blend holding the imported hand or body exports on save too
  (`md5source.py`, through blend_export.py): to the Game Folder, else the .blend's repository's `quakevr/progs`. Saved
  unedited: the shipped files byte for byte but the md5mesh's `commandline` line (hand: 7 files; body: 20). The
  normal maps are baked again only when the mesh or a skin changed.
- **Two body export bugs fixed** (found by the round trip): the skin export took the normal map the import shows
  (1024x1024) as the skin and refused; and it put every off-palette texel of an untouched skin on the palette (20% of
  the bytes of each of the 16 skins changed). Now only changed texels are quantized, as its option says.
- **Engine** (`vr_blender.cpp`): `vr_edit_in_blender [model|hand|look] [-n]` opens the model's source .blend in Blender
  (generated.json's `source`, else `src_models/blend/<m>/<m>.blend`; the hand and the body, with none, imported by the
  add-on through `--python-expr`); the repository is the game folder resolved through links (`files::resolved`).
  `vr_blender_path` (archived, empty: the newest `Program Files/Blender Foundation/Blender X.Y/blender.exe`).
  `vr_model_show <model>`: the model in front of you, turning (a client-side visedict; not the hand or body);
  `vr_blender_link 1`: `quakevr/blender_request.txt` (the add-on's **Show in Game**) asked every half second (one
  file time), only `show`/`reload` lines run, a request older than the game ignored. Debug > Tests: Edit in Blender,
  Edit Looked-At Model in Blender, Hear Blender's Show in Game, Hide the Model from Blender.
- **Tests:** `blend_addon_test.sh <agent>`: `vr_blender_test` 17/17 (sources of v_shot, a hand-made gun, vrgrunt, the
  pump part, a crate, the hand; the main hand's shotgun; Blender found; the request file: 2 lines run, 2 refused, not
  run twice; show and hide), and the hand's and body's .blend exported on save through the one add-on (9/9 each,
  the panel drawn into a stand-in layout). `blend_autoexport_test.sh` 6/6 for v_shot and for `BA_MODEL=vr_crate1`.
- **Not checked:** a windowed Blender (the panel, Show in Game's button, the hand's import from the game waits for
  the window with a timer), and a real launch of Blender from the game (`-n` only: no GUI in tests); "look" picking
  has no assertion. The md5 plan's first step (a hand source .blend with the engine's tables) is not done: saving
  the imported hand inside the repository is its stand-in.

**Checklist lines (suggested):**
- Blender: only "Quake VR" in Add-ons (no Quake VR Hand, no "add-on not found" at start); its sidebar panel says what
  the file is (open v_shot.blend: "Source of v_shot.mdl").
- In game, shotgun in hand: Debug > Tests > Edit in Blender opens v_shot.blend in Blender; edit, Ctrl+S, the gun
  changes in your hand.
- Blender: Show in Game on vr_crate1.blend draws the crate in front of you, turning; Debug > Tests > Hide the Model
  from Blender takes it away.
- `vr_edit_in_blender hand_rig` imports the hand into Blender; saved as src_models/blend/hand_rig/hand_rig.blend, a
  Ctrl+S after an edit changes your hand in game.

## Flat-screen crouch: an eased transition, crouched gun offsets (branch `agent/flatcrouch`, 2026-11-06)

His notes: "Going from uncrouched to crouched is instant, I would like a short visual transition (tweakable)", and
tunables for the crouch weapon pose's up/down offsets, blended as the existing ones are.

**What was instant.** The ease itself (`vr_flat_crouch_time` 0.25 s) was fine; the headroom cap from the uncrouch fix
(`hull::localEyeCap`, applied in `pollKeysFlat`) was not. While the key is held the server gives him the lowest crouched box
over his own lowered eyes (levels 36 / 44 / 52), so the cap (that box's top less 8) always stood under his eyes, and each
frame it pulled the eased crouch down to it: measured with the new trace, the eye fell 2, 1, **7**, 1.5, **6.5** units in
five frames and was fully down in 5 frames (0.07 s) instead of 18. Going up was already eased. Fix: `localEyeCap` answers -1
while the server reads the key held (`crouchFlatHeld`); after he lets go the cap holds his rise under a kept box as before.
The ease now goes through smoothstep (`vr_flat_crouch_smooth` 1, new; 0 even), on a linear progress (`keyState.lin`) the
cap sets back through the inverse when it holds him. View, head, box, body, arms and weapon all read the one share.

**Crouched gun offsets.** `vr_flat_gun_down_crouch` (0) and `vr_flat_gun_ahead_crouch` (16.5): Gun Down and Gun Forward's
values crouched, blended in by the crouch's share as `vr_flat_gun_out_crouch` is (Debug > Tunables, after Gun Out
Crouched). Defaults are the standing ones: nothing moves until he tunes them. `vr_flatplay_status` prints them.

**Probe and test.** `vr_flat_crouch_trace N` (Debug > Tests > Flat Crouch Trace): N frames of the drawn eye's and the main
hand's height, then the biggest step against what the ease allows (`flatcrouch: trace done ... PASS`).
`Misc/quakevr/flatcrouch_test.sh`: shotgun held, 40 frames crouched and let go: eye step at most 1.49 (allowed 1.50), hand
1.84, 18 down and back; Gun Down Crouched -6 lifts the crouched hand 5.8.

**To try (flat screen):** tap and hold Left Ctrl: the view, arms and gun should sink and rise over a quarter second with
soft ends; Crouch Time and Crouch Ease (Debug > Tunables) change it; Gun Down Crouched / Gun Forward Crouched move the gun
while crouched only. The low-ceiling cases (vrteleporters' crouch room) should still hold him down.

## Flat-screen: his values as defaults, Vomit Mode on flat deaths (branch `agent/flatpromote`, 2026-11-06)

His words: "Please promote my flatscreen values as defaults. Also add an option (on by default) to force 'vomit mode on'
for flatscreen player ragdoll deaths."

**Promoted** (from his `quakevr/tunables.cfg`, only these 15, with `tune_promote.py` on a file of those lines):
`vr_flat_gun_down` 0 -> 2, `_gun_down_crouch` 0 -> -5, `_stance_turn` 25 -> 15, `_stance_back` 10 -> 4, `_pistol_in` 6 -> 2,
`_pistol_up` 2 -> 4, `_wall_tilt_time` 0.3 -> 0.2, `_ads_relief` 14 -> 4, `_aim_fov` 40 -> 45, `_chest_down` 0.3 -> 0,
`_bob` 0 -> 1, `_bob_fwd` 0 -> 1, `_ads_recoil` 0.35 -> 0.1, `_turnlag` 1 -> 5, `_turnlag_cap` 4 -> 10.
**Turn Lag's range widened** from -3..3 to -10..10 (`lagAmountMax`, the Tunables row): his 5 was past it, so the game
clamped it to 3 while he played. At 5 the hands trail a 360 deg/s turn by 28 degrees (flatplay's turn, measured); at 3,
what he actually saw, 16.8; the old 1, 3.4. If 5 feels too much, 3 is what he tuned on.

**Vomit Mode on flat deaths.** `vr_flat_death_vomit` (1, archived; the Desktop Screen Play rows, "Vomit Mode on Death"):
on a flat screen with Death View on Immersive (`vr_death_view 2`), `deathview::vomit()` answers yes for the death
whatever `vr_death_view_vomit` says. The cvar is never written, so his setting holds for the headset and the next
death; Third Person (`vr_death_view 1`) is not forced. Test: flatvisuals' death part, now three statuses: off (vomit 0,
camera 0.8 from the eyes, roll 0), on (vomit 1, camera 0.0 from the eyes), Third Person after (vomit 0).

**Tests moved to the new defaults** (the check was about the default, not the behaviour): flatads a (sighted recoil
0.1 of hipfire, 0.08..0.12; measured 0.100), flatcrouch offsets (Gun Down Crouched -11: 6 higher than the default -5),
flatwall c (the first ease is the default 0.2 s: 0.16..0.22, measured 0.195), flatwall c/d's place 20 down instead of
30 (his gun pose clears the wall 30 down: no tilt there; 20 down tilts, 47.5 degrees least), flatplay locked (also
`vr_flat_bob_fwd 0`) and feel (pinned to turn lag 1 cap 4: its bounds, 4.5 degrees, are sized for that).
flatsweep_known.txt: four rows pass now and are out (jump_gun low/open/wall long crouch hip, jump_hand0 wall/long/crouch/hip).

**Real failures with his values (limits kept):** flatsweep 24 new rows, all `ads` but two hipfire long-gun forearm rows
(fore_torso 9, gun_torso 6, hand_head 5, hand_torso 4; worst: gun_torso open/long/stand/ads 3.44 at pitch -85 limit 0.5,
fore_torso open/pistol/stand/ads 3.53 at 15 limit 1.0, hand_torso open/pistol/stand/ads 2.53 at 85, hand_head long ads
0.77 at -25 limit 0.5): Eye Relief 4 puts the sighted gun and hands against the head and chest. flatsight d: "the wall:
the weapon goes back" (0.68 units, needs 2): at relief 4 the sighted gun has 1 unit left before the 3-unit minimum.
flatwalk's one failure (wrist off its rest 0.596 > 0.5) fails on origin/next too.

**To try (flat screen):** the ironsight close to the eye (relief 4, 45 degrees of zoom), the hands' bob and forward bob on
a walk, the turn's lag (is 5 too much?); die with Death View Immersive: the view tumbles with the head; Vomit Mode on
Death off: the smoothed view again.
## Armour Studio: an ImGui window (branch `agent/armourimgui`)

His words: "Can you also add a floating ImGui armour studio window that I can open close like for the weaponstudio so that I
can tweak things easily in flatscreen without opening/closing the quake menu? You can keep the current menu as is too."

- `Quake/vr/vr_armourstudio_window.cpp`: `vr_armourstudio_window [open|close|toggle|test]` (no argument: toggle) and
  `vr_armourstudio_window_close`; Debug > Tools > Armour Studio's first row opens it. Every row writes the page's own
  `vr_armourstudio_*` cvar or runs its command, so the studio's frame does the rest (undo, subject, table): the menu and the
  window always agree. Layout, input, the file's-values table: `ARMOUR_STUDIO.md`, "The ImGui window".
- The Weapon Studio has no key binding (console only), so this ships none either; `bind <key> vr_armourstudio_window` makes
  one, and `vr_imgui.cpp` now lets a key bound to `vr_armourstudio_window` reach its binding while ImGui holds the keys
  (`VR_ImGuiEvent` takes the game key from `in_sdl.c`), so the same key closes it.
- Test: `vr_armourstudio_window test` with `-flatscreenmode` (`map vrtesthall`, `-QuitOn "^armourwin: [0-9]+ passed"`):
  `armourwin: 17 passed, 0 failed`; in the mock headset `armourwin: 1 passed, 0 failed` (it refuses to open).

**To try (flat screen):** `bind F7 vr_armourstudio_window`, F7 in a map: Start, pick a part, drag and type values, Save;
open the menu page and see the same values; F7 again closes it and mouse look comes back.

## Flat-screen wall margin (branch `agent/flatwallmargin`, 2026-11-06)

He: the wall push back and the muzzle tilt keep the gun out of the wall, but the hand still goes into it; start them a
bit further off.

- New tunable `vr_flat_wall_margin` (default 4 units, 0..24; VR Settings, the flat Wall rows: "Wall: Keep Off Distance").
  In hipfire (sighted: none, `vr_flat_ads_wall_keep` places the sight), the push back (`handpose::gunBack`) and the tilt
  test (`handpose::gunDepth`) count a weapon point as in a wall once a wall (not a floor or a ceiling) is within the margin
  of it: their lines from the head go on past the point by twice the margin. The push back counts the hand's own point
  too; the tilt doesn't (a tilt round the hand can't move it). 0 is exactly the old code path.
- `vr_gun_wall_depth` prints a second line per hand, `gunwalloff: <hand> fist off Xcm gun off Ycm`: how near the drawn
  hand model's vertices (`view::drawnHandSurface`) and the weapon's points come to a wall (negative: in it).
- Measured (vrtesthall north wall, `setpos 0 623.5`, rifle 16 / revolver 31 / shotgun 4, yaw 90, 45, 135, pitch 0 and
  +-30, standing and crouched): margin 0, the gun up to 0.9 cm in, the shotgun's pump hand 0.4 cm off; margin 2, both
  hands >= 4.6 cm off; margin 4, both hands >= 9.8 cm off, the gun >= 4.4 cm; side walls and the open: no change.
- **But walking into the wall the hand still goes in, whatever the margin** (scratch `fwm_walk.sh`: +forward into the
  wall, `noclip` off -- `setpos` turns noclip on): the player stops at y 632, not 623.5 (8 units nearer than the tests'
  setpos), the hand ends at 638, and the push back has no room at all: the arm is ~11 units from the shoulder, keep 9,
  and `stockBackMost` (vr_flat_clear_stock) takes the rest to 0. So the whole job falls to the tilt, round the hand:
  main fist 5-9 cm in, the support hand 5-30 cm in, at margins 0..8 alike. With `vr_flat_wall_arm_keep 0;
  vr_flat_clear_stock 0` and margin 4 the gun hand stays out (>= 9.6 cm) but the shotgun's pump hand is still 9 cm in;
  `vr_flat_wall_tilt_pivot 1` keeps the gun hand out (>= 2 cm), not the support hand. A real fix needs room for the push
  back at the wall (or a shoulder pivot, or lifting the support hand off), the author's call.
- Tests: `flatwall_test.sh` 14 passed, 0 failed. `flatsweep_test.sh`: margin 0 383 passed, 27 known, 0 failed; margin 4
  381 passed, 25 known, **4 failed** (not accepted): wall/long hipfire at pitch 45, `jump_gun` 6.1 a degree crouched
  (new; limit 1) and standing (6.2, listed 4.7), `jump_hand0` 3.4 crouched (new) and 3.1 standing (listed 2.6). The
  margin makes the known jump there bigger; the level probe (no floor-then-wall switch) did not change it.
  `flatsight_test.sh` did not run (it waits for the author's game to close: "AUTHOR GAME RUNNING").

### Second pass: pull back, let go (his choice)

- The wall state (`wallOn`, per hand, eased over `vr_flat_wall_tilt_time` like the tilt): a hipfire gun pushed back more
  than 0.5 units or tilted. In it the arm may fold to 2 units from the shoulder (`wallArmKeep`, was 9) and the stock's
  chest clearance (`stockBackMost`) fades out, so the push back has room; the free hand lets go (`flatsight::wallLetGo`:
  the pin onto the grip fades, flatplay's support pose blends to the empty hand's, and twohand stops holding past half
  of it) and takes the grip again as the gun comes off the wall. No new tunables.
- Walking into the wall (scratch `fwm_walk3.sh`, empty off hand, at rest 0.5 s after the walk): the gun 8.9..15 cm off
  the wall in all 18 cases (rifle, revolver, shotgun; head-on, 45 both ways; standing, crouched), the gun hand out in
  all; the gun's nearest point 7.6..25 cm from the eyes. Still in: the rifle's free (empty) hand at rest (-1.5 cm
  head-on standing, -12 crouched, -19/-25 at 45 with the wall on its side): the empty hand's own pose is in the wall and
  nothing pushes an empty hand. During the walk-in (the 0.2 s ease) the gun and the free hand still go in (gun up to 44 cm).
- Tests: flatsweep 384 passed, 21 known, 5 failed (the four jump rows are gone): `gun_torso wall/long/stand/hip` 1.63
  at pitch -25 (new, limit 0.5: the stock past the chest clearance), and `fore_torso` open+wall/long ads crouch 2.53
  (listed 1.95) and stand 3.43 (listed 3.29) -- open and sighted rows, which this change should not touch: possibly
  from the origin/next merge, not checked. flatwall 8 passed, 6 failed: its hold, tilt-ease, mirrored-tilt and
  block-fire rows expect the tilt and the kept hold at its wall spot, where the push back now clears the gun untilted
  and the hand lets go (by design; the test needs new expectations). Not accepted.

### Third pass: wall poses, chosen once and kept (his design)

- **Wall poses** (`wallPoses`, vr_flatsight.cpp): what the way back can't clear, the drawn gun turns out of one of four
  ways, per degree of the turn: **up** (vr_flat_wall_tilt's pose, its yaw and roll cvars as before), **left** and
  **right** (raised 0.35 a degree, turned across the body or out), **down** (0.8 a degree, low ready). As the tilt is
  first wanted with no pose locked, each pose's least clearing turn is searched (`tiltNeedFor`, the old search) and
  scored (`wallPoseScore`: depth left in the wall, plus 4x its depth in the torso (`wallTorsoWeight`), plus what it comes
  nearer the eyes than 6 units (15 cm), all x100; then the turn's degrees x0.01 as the tie's breaker). The eye and torso
  measures (`handpose::gunNearest`, no traces) take the gun's points and the lines between them, so a barrel across the
  face counts. The best is **locked** until
  the state and the turn have been off 0.5 s (`wallUnlockAfter`), so walking along a wall keeps it. Chosen again only if
  the kept pose stays 1 unit deep for 0.3 s (`wallPoseBad`, `wallPoseBadFor`); the new pose eases in from the drawn turn
  (a slerp in the view's frame over vr_flat_wall_tilt_time). Sighted: up only (a pose kept from hipfire eases over to
  it). The free hand lets go in every pose. Cost: a locked frame searches one pose (about 6 gun probes a frame, each a
  trace per gun point, as the old tilt); a choice searches four (70-80 probes, once). The status line `flatwallturn:`
  prints `pose`, `locked`, `wall` (the state) and `probes`.
- **Walk-in:** the wall state and the turn come on ahead: where his level speed takes his box in vr_flat_wall_tilt_time
  (a box trace, so the lines never start inside the wall), the gun pushed back as far as the state lets it; and the state
  eases in over 0.35 of the time (out over all of it). Gun's worst during the walk: 44 cm in before, now 26 (rifle at
  135 standing) and 18 (shotgun head-on) for a frame or two, else 10 or less.
- **The stock** comes to the chest in the wall state (`wallStockKeep` 0, not vr_flat_clear_stock's 1); what that leaves
  the hand short of a wall at its side, it goes across the chest (level, inwards, up to 8 units: `wallAcrossMost`);
  head-on nothing.
- **gun_torso wall/long/stand/hip, not fixed** (flatsweep's wall run, his box on the wall; 1.93 with the stock to the
  chest). Wall sweeps (`scratch/fw3_wsweep.sh`, `fw3_rows.sh`): with the turn off (vr_flat_wall_tilt 0) and the stock
  kept every row passes; the stock kept, turned: 0.53..1.30 looking 20..45 down; fold 9: the same; no stock clearance at
  all: 2.7. So the depth is the turned poses' (and the stock to the chest adds ~0.6). Tried and reverted: keeping
  vr_flat_clear_stock in the wall state (the rifle head-on then stays 1.3 in the wall, flatwall's look-30 row fails with
  the weapon 5 10 cm in, and the row stays 1.30); and capping each pose's turn at 0.3 in the torso by `gunNearest`'s
  torso depth (no change in the row, so that measure doesn't see what flatsweep's gun shapes see; and the sighted rows
  then stayed up to 11.8 cm in the wall). The pose score keeps that torso term (x4) for what it does see. Next: score
  the poses with flatsweep's own gun shapes (vr_flatsweep.cpp `torsoDepth`).
- **The arm** folds to `vr_flat_wall_fold` (new tunable, default 5 units, VR Settings "Wall: Fold Arm To"; was a fixed 2):
  standing, the stock at the chest binds first, so fold 2, 5 and 7 measured the same standing; crouched 5 keeps the gun
  further out.
- **The let-go free hand** (`freeOffWall`): pulled from its empty-hand place towards its side (below its shoulder, the arm
  hanging) as far as keeps it vr_flat_wall_margin + 2 units off the wall (the least share, halved 8 times), eased like the
  push back and only as far as it has let go.
- Measured (`scratch/fw2_walk.sh`, walked in, at rest 0.5 s after): all 18 out: the gun hand >= 13 cm off the wall, the
  free hand >= 3.3, the gun >= 2.2 (rifle crouched at 135), else >= 7.3. Eyes >= 12.2 cm but the rifle crouched: head-on
  7.8 (left pose, across the face) and at 135 6.1 (right pose): the eye term (gun points and lines, from `s.head`) did not
  move those choices; open. Poses: rifle right, right, down / left, down, right (head-on, 45, 135; standing / crouched);
  revolver right x3 / right, right, left; shotgun right, right, left / down, up, down.
- Sighted fix: the drawn turn used the hipfire-locked pose while the sighted measure used up; a pose kept from hipfire now
  eases over to up when sighted. flatwall section c runs the rifle sighted (the shotgun's sighted way back clears it at
  that spot): ease 0.195 / 0.583 s, mirrored.
## Rolled guns: barrel and sights rigid with the hand (branch `agent/rollaim`, 2026-11-06)

His note: with Show Controller Laser on he lined up red (the shots) and green (the barrel) holding the gun level; rolling
his wrist, they came apart.

- **Cause:** `view::weaponTurn` (vr_view_weapon.cpp) turned a held gun by its angle offsets (Pitch, Yaw, Roll, plus
  `gunModelPitchBase` 7) by **adding** them to the hand's Euler angles. That is the hand's turn composed with the offsets
  only while the hand is level: rolled, the pitch offset stayed a world-vertical pitch instead of the hand's (and a Yaw
  offset went wrong pitched up or down too). The shots (`weapons::shotAngles`) were already composed in the aim frame,
  so the shots stayed rigid and the drawn barrel, sights, muzzle and hotspots drifted. Worst on guns whose angle offset is
  far from 0 net of the base pitch (the enforcer's rifle, Pitch 0 with Shot Pitch 7: barrel 2.9 to 6 degrees off its
  level place in the hand at roll 90; the super shotgun with Yaw 4: 3.4 degrees pitched 30 down, even unrolled).
- **Fix:** `weaponTurn` composes (`anglesBasis(rot) * anglesBasis({-o.x, o.y, o.z})`), as `idleAttachmentAngles`, the
  posing code (setupPosingHand's `frame`) and `attachedTurn` already assumed. Its two hand-made copies follow:
  `twohand::bladeDirection` (vr_twohand.cpp) and the posing floating weapon's inverse (`holdFloating`, vr_view_hands.cpp).
  The shots are unchanged everywhere; with the hand level (pitch and roll 0) the gun is drawn exactly as before, so
  tuned Pitch/Yaw/Roll and Shot Pitch/Yaw stay valid (he tuned them level). Pitched up or down, a gun with a Yaw offset is
  drawn slightly differently (now correctly: rigid).
- **Sight line:** `sightalign::poseOf` reads the drawn weapon (`view::weaponFrame`), so it was off with the barrel and is
  rigid with it now.
- **`vr_aim_check`** prints one more line per armed hand: the shot, the barrel and the sight line in the frame of the hand
  the gun was drawn from this frame (`drawnAs`; `hands::State::rot` itself is a frame further on while the hand moves),
  as up/left degrees, with shot-barrel and shot-sight.
- **Test:** `bash Misc/quakevr/rollaim_test.sh <agent>` (rollaim_check.py): the enforcer's rifle, the super shotgun with
  Pitch -3, Yaw 4, Roll 12, Shot Pitch 2.5, Yaw -1.5 in the main hand and in the off hand (mirrored), the heavy revolver;
  controller pitch 0/+40/-30 by roll 0/+-45/+-90; each direction within 0.5 degrees of its level pose's: `rollaim: 12
  passed, 0 failed` (worst 0.00). With the old Euler sum put back: 6 failed (barrels and sights 2.9 to 3.7 degrees off).
  AD's guns need AD's own models, not in the test's game: none checked. Other aim tests: npaim 7/0, weaponstudio
  17/0 (its own 28/0), rvtest 42/4 (the known slow push, push with a shell, live shells' speed, and "a grunt's body":
  the same 4 with the old code). cup_aim_test.sh fails 0/4 before and after: its `vr_ad_test 98` (the Widowmaker) no
  longer exists, so no gun is given (a stale test).

**To try (VR):** Show Controller Laser on, any gun (the enforcer's rifle shows it most): red and green stay together as
you roll your wrist and aim up and down.

### Hands against Show Controller's ghost, and the pose pipeline audited (branch `agent/rollhand`)

His follow-ups: with the wrist upright everything matched the ghost; palm to the floor or the ceiling, the gun and the
hand came off it ("the hand position itself seems to not match my IRL hand").

- **Measured** (`vr_hand_check`, new, Debug > Reports > Hand Check: each hand's aim frame, drawn weapon and drawn hand
  entity, turn and place, in the controller's grip frame, the ghost's): with the old `weaponTurn` the drawn **weapon and
  the drawn hand** were 5.6 to 8.0 degrees off their upright pose at palm down/up (the hand follows: its grasp is fitted
  to the drawn weapon), melee too (the axe 8.0, a sword in the off hand 7.6). The aim frame (`hands::State::rot`) was
  rigid all along: the calibration (Gun Angle/Yaw, roll, move) is composed as quaternions (`withHandOffsets`,
  `calibrate`), the Weapon in Hand and Hand Only offsets and Hand and Weapon Together are composed, the ghost is
  `grip * anglesBasis(preview offsets)`. So the IRL-hand mismatch was the same bug: fixed by agent/rollaim's
  `weaponTurn`.
- **Audit, other Euler sums on hand poses:** one more, fixed: holding a sword's blade with the other hand
  (`twohand::bladeGripHand`'s holder turn, vr_view_hands.cpp) took the holding hand as `s.rot + fist offsets`; composed
  now (as the empty hand is drawn). Left as they are (transient, and made for adding): the knock and shake angles added
  to `s.rot` (pain knock, recoil: `gunLocalTurn` converts the kick for the hand's pose; chainsaw; flat-screen sway).
  The weapon-frame sums of offsets with offsets (a cup hotspot's angles plus the fist's) don't depend on the hand's pose.
  No roll or pitch limit on the drawn hand showed in the grid. The held weight's spring (vr_weight.cpp) lags a moving
  hand by design and its sag pulls towards the world's down (a physical effect: settled, under 0.3 degrees here).
- **Upright wrist unchanged:** at hand pitch and roll 0 the composed turns equal the old sums exactly; no tuned key
  changes meaning, no migration.
- **Test:** `bash Misc/quakevr/handroll_test.sh <agent>` (handroll_check.py): a nonzero calibration on both hands (Gun
  Angle 30, Gun Yaw 6, roll 10, move 1/-1.5/2; the off hand its own); super shotgun (Pitch -3, Yaw 4, Roll 12) + sword,
  enforcer's rifle + super shotgun (off, mirrored), axe + empty hand; pitch 0/+40/-30 by roll 0/+-45/+-90; each hand's
  aim, weapon and hand within 0.5 degrees and 0.3 units of the upright pose (the weight spring's sag off, 30 frames to
  settle): `handroll: 17 passed, 0 failed` (worst 0.32 degrees, the enforcer's rifle: its spring). With the old
  `weaponTurn`: 7 passed, 10 failed. rollaim still 12/0.

## Armour Studio round 2: his tweaks, taper, leather, animations, your body (branch `agent/armourstudio2`)

- **His armour.txt is the default**: his edited file (grunt, enforcer, knight, death knight blocks; 36 parts) copied over
  `quakevr/data/armour.txt` (LF), byte for byte. `vr_armour_test`'s last check now compares the shipped file's shell with
  the one it made before the test (it was the generator's, which the shipped file no longer is).
- **Taper** (`taper_start`, `taper_end`; `vr_armourstudio_taper_start/_end`, default 1): each plate ring's radii scaled,
  the first row's value lerped to the last row's, after the stand-off (`vr_agarmor.cpp` buildShell). The hit tubes follow
  (they take the grid). Clamped 0.2 .. 3.
- **Leather** (`leather "0"` on a sleeve part, per side; `vr_armourstudio_leather`, a toggle): that sleeve is not made
  (`sleeveOf` skipped). Default all on; 1 is not written to the file.
- **Animations**: the subject plays its model's frame groups (`vr_armourstudio_anim*`, Animation header on the page and in
  the window: combo, Play/Pause, Speed, Frame scrub). The engine writes the subject's frame; `VR_AGP_Pose` keeps its stand
  frame unless it is the studio's subject with `vr_armourstudio_anim` on. vrgrunt/vrenforcer/vrknight/vrhknight frames are
  named "frameN": their names come from soldier/enforcer/knight/hknight.mdl, frame for frame (114 = 114 on the grunt).
- **Your body in the ImGui window**: with Kind = Player the body clone (`vr_bodyclone.cpp`) is placed for the open window
  too, 1.4 m ahead and left of the view (`armourstudio::cloneWanted`); the menu page with Kind = Player now counts as a
  body page (`menu::bodyClonePage`).
- Tests: `vr_armour_test` 27 passed (new: the thighs' taper 1.3/0.8 on the rings, a left thigh sleeve's leather off, both
  through the save/load round trip); `vr_armourstudio_window test` 17 passed. Screenshots checked: his grunt vs the
  generator's, a death knight's thigh taper (side view), a hidden knee leather, the death knight's pain frames with plates,
  the clone with armour on builds 3 and 1.
- Open: switching `vr_body_build` 1 -> 3 while the window was open showed no clone in one run (starting on build 3 works);
  not chased.

## Armour Studio round 3: leather taper, your clone always armoured (branch `agent/armourstudio3`, 2026-11-07)

His words: "Is it also possible to add tapering to the leather parts of the armor?" and "The player view still doesn't work in
the armor studio. I can now see my clone, but it isn't wearing armor."

- **Leather taper**: `taper_start` / `taper_end` on the sleeve parts (`pauldron_sleeve`, `bracer_sleeve`, `thigh_sleeve`,
  `shin_sleeve`, per side): each of the sleeve's 4 rings scaled round its middle (`sleeveOf`, after its rounding), lerped in
  the limb's order (start = the end nearer the body: an upper part's sleeve's ring under its plates, a lower part's at the
  joint). The ring under the plates is still clamped to their feet afterwards, so the plate/leather seam holds; the two halves
  of a knee or an elbow overlap past the joint (sleevePast), so a taper on one gives a step, not a gap. Clamped 0.2 .. 3. The
  same sliders as the plates' (the ImGui window now shows them on the sleeve parts; hidden only on the helmet; the menu
  page's help says so). A taper set back to 1 (the plates' or a sleeve's) drops out of the part's values: not written to
  armour.txt. Untweaked: the branch is skipped, so every shell is the same as before (vr_armour_test's cleared-hash checks).
- **Why the clone wore nothing**: the clone is the mirror image of your own body's view entities, and the plates on your body
  come from the server's word on your armour (`agarmor::clientState`): no armour item, no plates (round 2 tested it after
  `give a 200`). Now `armourstudio::playerArmour` overrides that word in `view::setupPlates` while the studio shows the player's
  kind (the ImGui window open, or the menu's Armour Studio page, `menu::armourStudioPage`): the studio's Tier on every piece (or
  the part alone). As the kind first shows, Tier is set to the armour you wear, if any. Your own first-person body wears it
  too while the studio shows (the clone is its image).
- **Build switch**: the 1 -> 3 switch with the window open now keeps the clone and its armour (checked each frame by the test
  below: the body's model is the build's, `vr_player_plates_info`).
- Tests: `vr_armour_test` 30 passed (new: the left knee's leather taper, thigh's half 1 -> 1.4 = rings 1.000 1.133 1.267 1.400,
  shin's 1.4 -> 0.7 = 0.760 (under its plates) 0.933 1.167 1.400, the right's 1.000; the plates unmoved; cleared = the same
  shell). New `vr_armourstudio_clone_test` (Debug > Tests): fresh game, no armour; Kind = Player shown; tiers 1, 2, 3, then
  build 2 -> 1 -> 3 -> 2 switched while shown, each with the colour, all 1023 pieces settled, the clone's image showing the body
  and all pieces drawn in it; then kind grunt: nothing on you. `armourclone: 8 passed, 0 failed` flat (-flatscreenmode, the
  window) and in the mock headset (the menu page). `vr_armourstudio_window test` 17 passed. Screenshots checked: the clone in
  yellow with no armour (HUD armour 0), a grunt's knee sleeves tapered 0.7 -> 1.8 / 1.8 -> 0.7 (flared at the knee, joined).
- **Clone test fixed (branch `agent/clonefix`, 2026-11-07)**: `vr_armourstudio_clone_test` failed "step 1 ... the clone's
  pieces 0" on next. Bisect: it fails at 5a273fbe4 itself (no later commit caused it). The test was stale, not the feature:
  "the clone's pieces" read `platesState(16)`, the pieces a *view* (PlatePass::View) drew with the head, i.e. from a view
  more than 0.3 m from the eyes (a gate's, a spectator's); the mirror image's plates go through PlatePass::Clone, which was
  never counted. Now that pass has its own count (`platesState(19)`, also in `vr_player_plates_info`: "in the mirror image
  N"); the frame's rollover is done by whichever of the two passes draws first. Feature checked in a flat screenshot: Kind
  = Player, Tier Red, no armour item: the clone wears the full red armour and helmet. Tests: `armourclone: 8 passed, 0
  failed` (mock VR e1m1, flat vrtesthall), `armourtest` 30/0, `armourwin` 17/0.

## Swimming tilt: the body's pitch (branch `agent/swimtilt`, 2026-11-06)

His request: fully under water the body leans about 45 degrees forward, the legs trailing behind and up, easing both
ways; straightening near a ledge you can climb out at, so the legs are under you when the hands take it.

- **One pose state, `BodyPitch`** (`vr_avatar_internal.hpp`, new `vr_avatar_pitch.cpp`): the trunk (pelvis, spine,
  chest) turned `angle` degrees forward about the top of the neck, at the end of `solveTorso` (`vr_avatar.cpp`): the head
  stays at the headset's (or the flat-screen eye's) place; the arms solve from the turned shoulders to the controllers;
  the legs hang from the turned hips and, swimming, trail along the trunk (`swimFoot`: the trail and the kicks' plane
  turned with it, the stick's own trail halved at 60 degrees; the knees point along the body's tilted front). Every
  `avatar::torso()` user (holsters, flashlight, self-collision) sees the turned chest. VR and flat-screen both pose the
  body through `avatar::pose` (`vr_view_body.cpp`), so both tilt, no fork.
- **The ease**: a critically damped spring (exact step, any dt): angle and rate continuous, no overshoot; the ease time is
  the 95% time. Settled upright it snaps to exactly 0 (out of water the pose is bit-identical to before).
- **Swimming, the one source today**: target `vr_body_swim_tilt_angle` when the local player's `waterlevel` is 3 and he
  is not on the ground (`progs::localPlayer()`; a client of another server: the eyes in a liquid, `cl.onground`).
  **Waterlevel 2 (head out, treading water at the surface) stays upright**, as do standing on the bottom (waterlevel 3,
  `FL_ONGROUND`: walking on a pool's floor) and holding a hold (times `1 - climbing.weight`). Bobbing at the surface
  (3, 2, 3, ...) only blends partway: the spring keeps it smooth.
- **Ledges**: the world's ledge map (`vr_ledges.hpp`, climbing's) near the eyes: a lip whose top is out of the water with
  water in its drop within 40 units under the lip is a way out; within `vr_body_swim_tilt_ledge` units (3D) the target
  falls off (smoothstep, upright inside a third of it), so the tilt is gone before the hands reach the lip and the
  climbing feet (`vr_avatar_climb.cpp`) start from legs under the body. Brush models' and props' ledges are not looked at.
- **Tunables** (Body page, after the swimming kicks): `vr_body_swim_tilt` 1 (Lean Forward Under Water),
  `vr_body_swim_tilt_angle` 45 (0 .. 80), `vr_body_swim_tilt_time` 0.8 s (0.1 .. 3), `vr_body_swim_tilt_ledge` 64 units
  (0: never straightens).
- **Debug** (Debug menu): `vr_body_pitch` (the state now; `vr_body_pitch <range>`: the nearest way out too) and
  `vr_body_pitch_test` (the self-test).
- **Lying down and crawling** (BACKLOG.md) plugs in as a second `PitchSource`: its own target (about 90), the same pivot
  (the head stays at the headset) and ease, `BodyPitch::legs` for the legs (prone: on the floor along the trunk, a
  target in `solveLeg` instead of `swimFoot`'s trail). Missing for it: the source (when he lies down) and the legs' floor.
- Tests: `vr_body_pitch_test` 22 passed; `swimtilt_test.py` 12 passed (VR mock and flat-screen; screenshots checked:
  upright, easing in, 45 with the legs trailing, near the ledge, surface, the water jump out); handroll 17, ghost body 4,
  bodytrack_solve 240, flatmove 10 passed. Unverified: climbing out with the hands in VR (the mock run uses Quake's water
  jump), and how 45 degrees feels with real head motion.
## Texturing as fast as modelling (branch `agent/texflow`, 2026-11-06)

- **What:** the add-on's panel has a **Texturing** box (texflow.py: Blender side; texgen.py: numpy pixel work, no
  bpy). It has two loops. **Paint in Blender** sets up Texture Paint on the chosen skin with nearest texels and Blender
  palettes "Quake" (224 colours) and "Quake Fullbright (glows)". **Palette Preview** and **UV Wires** show a view
  image in the material, which is put back to the skin around every save. Ctrl+S writes dirty skins: an external PNG
  is saved and a packed skin is repacked (`save_pre`), then the export runs. **Edit Skin Externally** opens the skin's
  own PNG in the paint program (pref **Skin Editor**; otherwise Krita, Aseprite, GIMP or Photoshop is found installed;
  otherwise `os.startfile`) and writes `<skin>_uv.png` beside it. It then watches the PNG (0.5 s timer; a change is
  read once it has stood still for a poll), reloads the image and saves the .blend, which exports. Helpers: **Bake
  Starter Shading** bakes three Cycles EMIT bakes of AO nodes (reach, small, small inside = edges) on a temporary copy
  and shades each texel within its own palette row. **Fill Material** fills with metal, wood, leather, rust or cloth
  patterns on a palette row. **Write UV Layout PNG**. Palette files for paint programs are in
  `quakevr_models/palettes/` (.gpl ×3, .pal, .hex; generated by `texgen.write_palette_files`). In the engine:
  `vr_edit_skin [model|hand|look] [skin] [-n]` (vr_blender.cpp) opens the source .blend with `--python-expr
  q.edit_skin_from_game(<skin>)`. Debug > Tests has two new buttons.
- **Rules kept:** the helpers leave glowing texels and texel (0, 0)'s flood area alone unless asked (the bake also
  skips texels no face uses). The watcher never replaces a skin painted in Blender and not saved; it shows **Load PNG
  Anyway** instead. When Blender's newer pixels overwrite a PNG, the old PNG is copied to
  `%TEMP%/quakevr_skin_backups` first. Ctrl+Z: each pixel change keeps its before and after pixels in the module
  (40 steps), the scene's `qvr_skin_step` rides Blender's memfile undo, and `undo_post`/`redo_post` show the pixels of
  that step.
- **Export change (qpal.Quantizer, decision):** an ordinary texel painted *exactly* in a glowing colour now becomes that
  fullbright index; otherwise the Fullbright swatches could never make a texel glow. A colour merely near a glowing
  one stays ordinary, as before. Unedited skins export byte for byte (blend_texflow_test checks that the shipped files
  come back).
- **Fix found:** a source .blend kept only skin 0's image (skins 1+ had no user, so the save dropped them, and the export
  silently kept the old ones). `texflow.skin_images` now restores a missing skin from its PNG beside the .blend (or from
  the reference model) with a fake user, so the armour's skins 1 and 2 can be painted and exported.
- **Test:** `bash Misc/quakevr/blend_texflow_test.sh <agent>` (Blender half: blend_texflow_test.py) on copies of
  v_shot, soldier and armour (skin 2) in `scratch/texflow`, 26 checks each. The game on e1m1 printed `vr_model_watch:
  changed on disk, read again: progs/v_shot.mdl, ...` and `progs/vrgrunt.mdl, progs/soldier.mdl`. The exported
  .mdl has the externally painted fullbright block at index 251. The shipped files are put back byte for byte; `vr_edit_skin
  -n` was checked. 9 passed, 0 failed. blend_addon_test.sh: 3 passed (vr_blender_test 18 passed). Before/after images are
  in `scratch/texflow/<model>/out/*_fill.png`, `*_bake.png`.
- **Not verified:** everything that needs a Blender window. That covers the timers firing (view refresh, watcher), the
  watcher's save through a window override, `undo_push` after an external read, the real Ctrl+Z walk (the handler was
  tested directly), the brush settings in Blender 5.2's asset brushes, the editors' launch, and the look of Palette
  Preview while painting.
- **His Blender:** its script directory is his checkout's `Misc/quakevr/blender` (checked read-only). After the merge:
  restart Blender or F3 > Reload Scripts. Nothing was copied.

## His weapon offsets as the defaults; sight and alignment items retired (branch `agent/wofspromote`, 2026-11-06)

His words: "All weapon sights and muzzle and alignment issues in VR are solved now. Please retire any corresponding items
from the checklist and promote all my new weapon offset changes as the defaults."

**Promoted** with `python Misc/quakevr/weapons_data.py promote <his quakevr/ironwail.cfg>` (read from a copy; his checkout
untouched): his config's 163 `vr_wofs_<key>_NN` lines, 157 of them off the shipped `quakevr/data/weapons.txt` values (6 were
the same but for float noise). Per slot (changed values): axe 3, shotgun 10, super shotgun 6, nailgun 3, super nailgun 3,
grenade launcher 1, rocket launcher 7, lightning gun 1, Mjolnir 3, laser cannon 1, proximity gun 2, grappling hook 5,
knight sword 3, hell knight sword 4, chainsaw 1, grunt gun 2, crowbar 3, MG3 hammer 3, AD crossbow 8, Shadow Axe 3,
Widowmaker 5, AD plasma gun 1, nail pistol 3, flare gun 2, heavy revolver 6; AD melee: fury's sword 4, death knight's 4,
zombie's 4, death guard's 6, hunter ogre's hammer 12, mace 5, sword 10, dark prince's 6, justice 5, judge 6, blue flail 6.
Mostly grip pose (Offset X/Y/Z, Pitch/Yaw/Roll), then Shot Pitch/Yaw (12), Muzzle X/Y/Z (7), the ogre hammer's and
sword's hotspot 1 (12), the Widowmaker readout (4), fingers (4), two-handed (2), Scale (1), blade overlap (3).
No migration step: configs are sparse since configVersion 218, so a changed default reaches every player who did not set
the key (vr_weapons.cpp's comment; CODE_STYLE.md, "Settings and the config"). Once he runs this build his lines equal the
defaults and the next save drops them.

**The promoter tidies float noise** now (`weapons_data.py tidy`): a slider's `12.150001` lands as `12.15` (the shortest
text within 1e-6 relative); his hotspot grabs' six-decimal values stay as they are.

**Not promoted:** his `tunables.cfg` (smoke blow, knockdown ledge, light grid cell, `vr_flat_turnlag 2`: not weapon
values; the ship log's UNPROMOTED TUNABLES lists these 7, never a weapon key, as the weapon values are not tunables);
`vr_gunfx_wisp_delay 0.45` / `vr_gunfx_wisp_time 6` (gun smoke, not offsets: ask); his `vr_agrunt_grips` already equals
the shipped grips (rows of 2026-10-31). Hand calibration and toolgun values are personal.

**Verified:** one run, `vr_config_defaults` (a fresh config) then `vr_wofs_dump` of all 51 slots, then his 164 lines
exec'd and dumped again: 17578 values, 0 different. `vr_aim_check` with the axe, super shotgun, enforcer's rifle, heavy
revolver, grappling hook and toolgun held (main hand, mock pose): 32 lines, identical before and after his lines.

**Checklist retired** (quakevr/checklist.txt): "Front sights now visible when aiming down the sights of the flare gun..."; "Super
shotgun: aim down the sights: a raised glowing bead..."; "Super shotgun: run Align Sights to My Aim again..."; "[ ] Show
Controller + Controller Laser on ... gun, hand, ghost and lines stay together (agent/rollaim + agent/rollhand)"; "Weapon
Studio, sights: `vr_weaponstudio align both` ...". Kept (more than sights in them): "Shotgun top cleaned up" (texture),
"The grunts' burst rifle is a new model" (the model as a whole).

**Tests.** The agents' test baseline config is a full one (vr_wofs_version 43, every weapon value written), which pins
the old values: the tests were run on a copy of it without its 8320 weapon lines (as a fresh config, or his once his
lines equal the defaults), before (origin/next's weapons.txt) and after:
- rollaim 12/0, handroll 17/0, npaim 7/0 (after); rvtest 42/4, the 4 known (slow push, push with a shell, live shells'
  speed, a grunt's body).
- weaponstudio: 17/0 before, 16/1 after: its aligner check needed a sight that was off, and the shotgun's was the one
  (0.79 deg); his Shot Pitch -2.83 lines it up (0.00). weaponstudio_test.sh now puts the shotgun's Shot Pitch 1 degree
  off weapons.txt's first (the tool writes it back to 0.00): 17/0.
- flatsight: 12/0 before, 11/1 after: "the wall: the weapon goes back", KNOWN short of 2 at relief 4, now 0.53 units
  (accepted 0.6 on 2026-11-06). Not accepted here: for him to judge.
- flatsweep: 4 failed before (the long gun's forearm in the torso, ads), 15 failed after (383 passed both): the long
  gun's forearm in the torso also from the hip (2.08 / 2.25, listed 1.77 / 1.90) and up to 3.70 ads (listed 3.29), its
  hand in the head ads at pitch -25 (0.87, listed 0.77; five rows), the pistol's gun and hand in the torso at pitch 85
  (1.78, 1.14, 2.79; listed 1.68, 0.89, 2.53). His grips sit the flat body's arms a little deeper; flatsweep_known.txt
  not changed (report, don't accept).

**To try (VR):** just play (his lines equal the defaults now; a fresh config gives the same): a few guns and melee
weapons sit and aim as he left them.

## Flat-screen wall: muzzle up only (branch `agent/flatwall3`, 2026-11-07)

His test of the third pass: it keeps off the wall, but the left / right / down poses look weird.

- **One pose:** the wall poses, their scoring, lock, re-choice and swap ease are gone (`wallPoses`, `wallPoseScore`,
  `st.pose/locked/rescore/swapT`): a gun turns muzzle up only (vr_flat_wall_tilt, the least that clears; its ease, the
  walk-in's early start and the sighted fade kept). The way across the chest went too (the room does it; across brought
  the rifle 9.6 cm from the eyes at 45 degrees). **Melee** (`flatplay::meleeIn`: axe, mjolnir, sword, chainsaw, crowbar,
  super axe) never turns: it only goes back.
- **Room** (`roomFor`): in the wall state the way back is the turned weapon's, as drawn so far: the least that clears it
  (its points vr_flat_wall_margin off, the hand's own 1 unit: `wallHandRoom`) from the pose's own place, up to the hand
  3 units past its shoulder (`wallPastShoulder`): past the fold and the stock's place, the arm and the stock into the
  torso. Turned up, that is less than the level gun's way back, so the gun comes forward again; still turning in, more
  (the walk-in transient). The wall state is still decided by the level gun's way back (decided by the room's, it
  flickered and flatsweep's poses never settled).
- **Eye cap: none; the wall wins.** At a wall his eyes are too near it for the margin, a muzzle-up gun and 15 cm:
  capping the room at 15 cm left guns 3 cm in the wall, at 8 cm the rifle's hand 1.2 cm in (crouched, 135); driving the
  gun back till it was 15 cm off took it out of the view (down at the shoulder). The status line's `eyecap` says it is
  nearer than 15 cm (`flatwallturn:` now prints `room` and `eyecap` instead of `pose` and `locked`).
- **Free hand:** lets go as before. Kept on (tried): twohand's hold took the rifle and both hands 3-5 cm into the wall.
- Measured (`scratch/fw4_walk.sh`, walked in, at rest 0.5 s after; rifle, revolver, shotgun, axe; head-on, 45 both
  ways; standing, crouched): at rest all out: gun >= 8.6 cm off the wall, gun hand >= 7.8, free hand >= 3.3. Gun to the
  eyes at rest: rifle 6.7-14.0, revolver 13.7-16.8, shotgun 11.3-17.2, axe >= 17.1 (3.2 crouched at 135 before). Walk-in
  worst (a frame or two): gun -1.1 cm (axe, 135), gun hand -1.1 (revolver, 135), free hand -2.3 (rifle, 45 crouched);
  before: gun -26, free hand -21. A pistol turned up stands below the view; the rifle stands up at the right edge.
- Tests: flatwall 14/0, flatsight 12/0, flatsweep 383 passed, 25 known, 2 failed: jump_gun / jump_hand0
  wall/long/stand/hip 4.99 / 2.76 at pitch 30 (listed 4.69 / 2.63: the room moves the gun with the look; not accepted
  here). flatsweep's torso rows skip the wall state (new CSV column `wall_on`, flatsweep_check.py `wall_state`); its
  head rows still count.
- First, separately: the flat rows his weapon offsets moved recorded as known (flatsweep_known.txt, flatsight's wall row
  0.45), at the higher of agent/wofspromote's run and origin/next's re-run.

**To try (flat):** walk into a wall with the rifle, a pistol, the shotgun and the axe, straight and at an angle: the gun
turns muzzle up and nothing goes into the wall; the axe just comes back.

**His call (2026-11-07):** the two jump rows accepted (flatsweep_known.txt); the pistol raised into view.
- **Raise into view** (`vr_flat_wall_raise`, default 0.4; flatscreen settings "Wall: Raise Into View"; `raiseFor`):
  in the wall state, after the way back, a weapon whose muzzle (as drawn, turned) stands lower than this share of the
  view's height up from its bottom edge comes straight up till it is there (eased as the way back), at most 12 units;
  the way back is then found again from the raised place, and where that can't clear (a ceiling) the raise is halved
  down to what does. Generic, no number per weapon: at his box on the vrtesthall wall the revolver comes up 2.66
  units, the nail pistol 3.57, the flare gun 3.22 (all three sat under the status bar, a barrel tip showing; none of
  them is turned at that wall, the way back clears them); the shotgun, the enforcer rifle and the axe 0 standing (they
  reach higher); the shotgun crouched comes up 0 to 3.9 (20 up to 20 down, smooth). `vr_gun_wall_depth` 0.0 cm for all
  of them, hand 0.0. The status line `flatwallturn:` prints `raise`. Tried first: along the view's up, checked against
  the walls before the way back: looking down that went into the wall (the crouched shotgun's raise fell 3.4 -> 1.0
  from level to 2 down: flatsweep jump_gun 1.05) and the turn then came and went frame after frame at 35 down
  (flatsweep `settled`). Less looking down past 20 degrees, none by 50 (there the view finds the weapon anyway, and the
  wall state ends: a full raise dropped 4.5 units at once at 52 down, flatsweep jump_hand1 1.04).
  Pictures: `scratch/fw5_before.png`, `scratch/fw5_after.png` (revolver, nail pistol, flare gun, shotgun, rifle).
## Release 1.1 readiness pass (night 11-07, agent/release11)

Full regression (`run_baseline.sh --no-bench --jobs 4`, 300 cases, run 20261107-022308) against the last full run on
next (20261106-025836): 278 pass; 6 changed. Summary table and triage: `qvr-kit/scratch/release11_report.md`.
- **Crash fixed:** holding one enemy with both hands crashed the game (`NUM_FOR_EDICT` with no VM: the gadget lock's
  "the same enemy" check, `vr_gadgetlock.cpp` from 56b36f59f, called `foegrab::heldBy` in the view frame before its
  `progs::ServerVm`). judo 0/16 (a crash a case) -> 16/0; gadget_lock 18/0.
- **VR Settings > Gameplay** (BACKLOG "Gameplay page before 1.1"): one page of the big switches, existing rows only:
  Replace All Humanoids, Enemy Weapon Drops, Enemy Shoves, Melee Arc; Knockdowns, Holding Enemies, Embedded Blades;
  Parry, Bash, Parry Stamina, Slower When Tired, Melee Bloodlust; Bullet Time; Gore; Arena In Any Map. The installer
  manifest's paths follow (`settings_page.py update`); calibration boards 0 missing.
- **Graphics presets:** Off (Quake) also turns off the muzzle smoke cone, the smoke wisps, enemies' muzzle smoke,
  explosion debris and the model light grid; Low keeps 4 debris lights (of 8), as its torch and lava-nail lights.
  hazepreset 8/0.
- **cup_aim_test:** needs AD's data (the Widowmaker is AD's; `vr_ad_test 98` does nothing unless AD is mounted): runs
  with `-game ad` now and skips with a verdict line where the base has no `ad/` (agents' bases have none).
## Lying down and crawling (branch `agent/prone`, 2026-11-07)

His backlog item (2026-11-03): the player can lie down and crawl by moving his body. Built on the crouch's box and the
swimming tilt's `BodyPitch`.

- **When (VR: physical, decision).** Your eyes under `vr_prone_eye` (18 units over your feet, about 0.5 m of headset:
  on your front propped on the elbows; on all fours, about 0.6 m, stays a crouch) and you lie down; up again once they
  are 2 units over the lying box's top *and* the lowest crouched box fits where you are. No button in a headset: a
  seated player has no way to lie down (his crouch toggle lowers the view 0.5 m: a crouch). In water (waterlevel 2+) you
  don't start lying down.
- **Flat-screen: a toggle, the same code path.** `vr_flat_key_prone` "x" (free in the mode; Z is bullet time, C swims
  down) or `vr_prone_toggle` (Debug > Tests too): the view eases down `vr_prone_body_time` to 4 under `vr_prone_eye` (46
  to 14), the crouch bit is held meanwhile, so the server sees the low eyes exactly as a headset's and lies him down.
  Toggled up under a ceiling the crouch doesn't fit, the server keeps him down and his view is held under the box's top
  (`localEyeCap`, as the crouch's). Dead or between maps: up.
- **The box (vr_hull.cpp).** Lying down is a value of the crouch's own `crouchBox` slot: `vr_prone_height` 24 (24 is the
  lowest box the clip recovery's height growth covers, so no tree-builder change: decision; a lower box needs that grown).
  moveBox/entBox/touchBox/hitBox, monsters' sight (bit 23, crouched) and the trees (one more, 16x24, in `wantedExts`)
  follow. Needs `vr_crouch_hull` on.
- **Moving.** `vr_prone_speed_scale` 0.3 of the most speed (96 of 320), eased 0.25 s like the crouch's slowing, whatever
  `vr_crouch_speed`. Jumping: QC `PlayerJump` returns while `.vr_prone` (a new field the engine sets from each move;
  jump boots included, swimming up untouched). **Weapons: all usable lying down (decision)**: aiming from the floor is the
  point; nothing changed in QC.
- **The body (vr_avatar_pitch.cpp, vr_avatar.cpp, vr_avatar_legs.cpp).** `PitchSource::Prone`: target
  `vr_prone_body_angle` 90 plus asin((eyes over the floor - 8) / torso) (at most +30: propped up, the hips slope down to
  the floor), eased over `vr_prone_body_time` 0.6 s; the server's state for the local player (a remote client: his eyes
  that low). `BodyPitch::lying` (the angle's share while it comes from lying down) straightens the crouched trunk
  (crouch pose, crouch tilt and lean fade out) so the turn lays a straight trunk behind the head, and lays the legs back
  along it with the feet on the floor, knees down, toes pointed. Swimming wins over lying down.
- **Settings.** Movement > Player Hitbox > Lying Down: Lie Down and Crawl (`vr_prone` 1), Lying Down Below
  (`vr_prone_eye` 18), Lying Down Height (`vr_prone_height` 24), Crawling Speed (`vr_prone_speed_scale` 0.3), Lying
  Down Status (`vr_prone_status`). Body page: Lying Down Body Angle (`vr_prone_body_angle` 90), Lying Down Time
  (`vr_prone_body_time` 0.6). Debug > Tests: Lying Down Status, Lie Down / Get Up (Flat-Screen).
- **Tests.** `Misc/quakevr/prone_test.py <agent>`: 21 passed (VR mock and flat-screen: standing, down at once (box 24),
  the body's angle (VR 104 of 106, flat about 105), no jump, the crawl 96, into the 40-high tunnel, up to a crouch (36) in
  it, kept down under it with `vr_crouch_height` 44, standing out of it, `vr_prone 0` a crouch). `vr_body_pitch_test`
  adds lying down's targets and ease (29 passed). Not regressed: swimtilt 12, flatmove 10, flatcrouch 3, crouch_speed 4,
  bodytrack_solve 240, handroll 17, ghost body 4 passed; teleporters/crouch_test.sh's cases as documented. Screenshots checked (side view and first person looking down, both modes): the
  body flat behind the head, the arms out to the hands.
- **Left:** crawling by pulling with the arms (the stick moves you); a box under 24; seated lying down; the flat-screen
  view on lying down is Quake's own camera at 14 units (no special framing).
## The hand's and the body's source .blends (branch `agent/blendmd5`, 2026-11-07)

The md5 plan ("The md5 models (plan only)", above), done:

- **Sources:** `Misc/quakevr/src_models/blend/hand_rig/hand_rig.blend` (215 KB) and `.../vrbody/vrbody.blend` (719 KB),
  made by `blend_build.py -- hand_rig` / `-- vrbody` (the add-on's import of the shipped files, saved; paths in them
  relative to the .blend). generated.json: the hand's .blend owns `hand_rig.*`, `hand_rig_f.*`, the 4 `.lmp` skins;
  the body's owns the four builds' `.md5mesh`/`.md5anim`/`.mdl` and the 16 skins. make_hand_rig.py and
  make_hand_feminine.py are retired (all their files owned: they stop, nothing written; `--stats`/`--preview` still
  run); make_vrbody.py keeps the owned files and still writes the `_bare` meshes and `vr_bodybare.inc`.
- **The body's builds are shape keys** (decision: one .blend, not one a build; the plan's "builds derived at export"
  without make_vrbody.py's parametric code): basis `vrbody`, keys `vrbody_lean`, `vrbody_brawny`, `vrbody_fem` (the
  feminine build too: same joints, vertices, UVs and weights; only the places differ). Each build's file text is kept
  on the object, so an unedited key writes its file byte for byte; an edit of the basis in Edit Mode carries into the
  keys (Blender's own), Apply Pose to Mesh moves the keys by as much. The import's new **Every Build (Shape Keys)**
  (default on); The Other Builds Too stays for a body without keys.
- **The hand:** the commandline line kept (the armature remembers the imported files' line and a digest of the rest:
  an unedited export is byte for byte, an edited one says Blender's); a changed hand makes the feminine hand again on
  export (`make_hand_feminine.derive`, the generator's code: on the shipped hand it gives the shipped `hand_rig_f`
  byte for byte); the blood skins as before (carried under the blood). **Decision:** the export does not write
  `vr_handrig_data.inc`: the engine derives the rig from the file as it loads and measures against the compiled
  tables, which stay the generator's last output (the reference); no engine rebuild after a hand edit.
- **Engine test** `vr_blender_test` 20/20 (hand_rig and vrbody_lean now resolve to their .blends; the import case is
  `vrbody_bare`, still the generator's).
- **Test** `blend_md5_test.sh <agent>` 9/9: both .blends exported unedited into copies, every file as shipped (hand
  21 of 21, body 58 of 58: no float noise, all bytes equal); the feminine derive as shipped; with the game on e1m1, a
  fingertip edit (5/5: the feminine hand made again, skins untouched) and a head-top edit of the basis (7/7: every build
  0.5 higher at that vertex, nothing else) saved through export on save, and the game read `hand_rig.mdl` and
  `vrbody.mdl` again by itself; files put back. Also: blend_addon 3/3 (hand 9/9, body 9/9), bodytrack_solve 240/0,
  vr_ghost_body_test 4/0, handroll 17/0, armourtest 27/0 (low-impact runs).
- **Not done / open:** an edit of the body doesn't reach the `_bare` meshes (make_vrbody.py's parametric body, as
  before); the generators' procedural hand and body are frozen as the reference (to start over from them, the
  generated.json entries go first). Not checked in a windowed Blender (the shape key panel, Edit Mode on a key).

## Armour's four open items on armed humanoids (branch `agent/armorleft`, 2026-11-07, overnight)

Shots: the Armour Studio's subject (`vr_armourstudio 3` first, so later kinds spawn along the player's settled yaw), tier 2, body
ghost 1, `vr_mock_camera` close to it (scratch `shots.sh`): 12 tiles, each item idle and two `vr_armourstudio_anim*` frames.
Sheet `C:/OHWorkspace/qvr-kit/scratch/armorleft_sheet.png` (before: origin/next's `vr_agarmor.cpp`; after: this branch).

- **Knight's red pad (fixed, code):** it is his own mesh: red shoulder pads on both shoulders (skin, not a tint or a leftover
  plate), standing up into the pauldrons' open top ring; seen from above (your eyes are higher than his) the red showed between
  the pauldron's top and the collar. No armour.txt value hid it (tried: pauldron stand-off 0 / 0.3, start -3.5 each side, scale
  1.15, offset up 1 / in 1-2, roll +-12, taper_start 0.5-0.8; cuirass start 0, stand-off 0.4, taper_start 1.12): each moved the
  red elsewhere. Now `Wearer::shoulderCap` (knight 0.8): `platesOf` closes a pauldron's top ring with a dome (rim, a middle ring
  1.18 wider than the rim (`capBulge`) half way up, an apex 0.8 of the ring's radius + `plateStep` out along the arm's line,
  and a flat underside: a closed shell, one plate tile stretched over it, the rim layer). Red gone in idle, run and swing frames.
  `vr_agarmor_check` 20/0 ("each shell closed" on all four). Only the knight (shoulderCap -1 elsewhere); his armour.txt untouched.
- **Enforcer's thighs from behind (not changed):** at the studio's close back view, before and after look the same: covered in
  idle and run 1/5. Tried: the thighs' plates weighted as the whole skin under them (this leg's and the pelvis's vertices), so
  the back rows follow the pelvis-weighted skin: no visible change in the sheet, dropped (not worth the risk). The bare skin the
  round-five note describes shows only from further off and lower, at the buttocks' crease.
- **Grunt's knee leather (not changed):** the thigh sleeve's last ring flares wide (a skirt with a point at the outer back), and
  the knee's low-poly point shows between the two sleeves' halves in front. Tried: a sleeve corner capped at 1.2 / 1.12 of its
  ring's median (no visible change: the whole ring is wide, not one corner); thigh_sleeve scale 0.85, stand-off -0.6, pitch 0 /
  -10 / 25; shin_sleeve stand-off 0.6, offset up 0.8-1, pitch +-15; the new sleeve tapers (thigh_sleeve taper_end 0.7-0.8):
  narrower leather bares more of the knee. Needs a shape change in `sleeveOf` (the rings' middle at the joint is pulled back by
  the calf), not a value.
- **Enforcer's elbow band (not changed):** no description of what is wrong beyond "elbow band"; front and three-quarter shots
  show the bracer sleeve as a dark ring at his elbow (his bracer_sleeve scale 0.696). Nothing changed without knowing the ask.
- **Tests:** `vr_armour_test` 30/0, `vr_armourstudio_window test` 17/0, `vr_agarmor_check` 20/0; `vr_armourstudio_clone_test`
  1/1 ("step 1 ... the clone's pieces 0") on this branch and the same on origin/next's `vr_agarmor.cpp` (headless mock, e1m1 and
  vrtesthall): not this change.
- **Noticed:** the studio's Animation list shows the grunt's and the knight's frames as one "frame" group (114 and 97 frames;
  the enforcer's are named); `vr_armourstudio_turn` changed after the subject is made seemed not to turn it in these runs.

## Tracy session: loading and gameplay (branch `agent/tracy1107`, 2026-11-07, overnight)

Release-Tracy, `vr_tracy_preset full`, his settings, the mock at 2048 eyes paced 90 Hz; E1M1 (VR and flat), armed_mix32,
AD ad_tfuma (data copied into the kit game folder), arena wave 6, and a loads run (cold start, e1m1, vrfiringrange,
changelevel, restart, AD cold and warm). Full numbers and the open items: qvr-kit/scratch/tracy1107_report.md.

- AD fight: CPU busy p50 11.9 ms (over 90 Hz); `findradius` alone 2.0 ms a frame (187 calls, every edict walked each).
- Applied, exact, each its own commit: `CRC_Block` slicing-by-8 (the 12 MB progs' CRC: server progs 30-36 -> 12-17 ms at
  every map load); the model light grid reused on a reload of the same map (restart 51 -> 0 ms; `vr_model_light_grid_info`
  prints a contents hash); `findradius` prefetching (AD -8% a call); the worn pack's rest box cached per shell and fit, and
  `Mod_FindName` remembering a name's slot by its hash (`limbpiece` E1M1 0.30 -> 0.16 ms, AD 0.40 -> 0.12). Restart of e1m2
  228 -> 142 ms; CPU busy p50 E1M1 4.27 -> 4.16, AD 11.89 -> 11.05.
- Tests: wornpack 25 passed 0 failed; the grid's contents hash equal reused and rebuilt; progs CRC 23819 old and new.
- Left for him: an area-tree or QC-side findradius (~1.5 ms on AD fights), GL programs compiled at init (a 16.7 ms
  first-wisp hitch), limb textures probed during play, cold skin normal maps on the main thread (176-357 ms), armed_mix32's
  78 heap allocations a frame.

## Fresh map seeds (branch `agent/mapseeds`, 2026-11-07, overnight)

His ask: the per-map random content new at every new start of a map, not at a save's load. `vr_fresh_map_seeds` (default
1, archived; VR Settings > Gameplay > Maps > "New Each Time") picks a map content seed at each map load that is not a
saved game's (new game, changelevel, restart): 24 bits from the clock (srvrandom::chooseMapSeed, vr_srvrandom.cpp). It
is mixed into what was seeded by the map's name alone:
- the crates' and barrels' layout, skins, stacks, crowbars and flare guns on them (vr_crates.cpp's planner seed);
- the rocks and bricks (vr_debris.cpp's planner seed; the seed is in the plan key, so vr_place_keep never reuses another
  seed's plan);
- the armed replacements' loadouts (QC VR_AGR_SeedSet: an offset to vr_agrunt_repl_seed's shuffle);
- the grunts' armour roll (QC VR_AgArmor_Rand's low-discrepancy spread, offset).
Already random per load before this (the server stream, clock-seeded): which pooled grunts are armed, crate contents
and break patterns (QC random() at break time). Not per-map content, left alone: the ragdolls' death rolls (entity and
frame), vr_agarmor_seed (the plates' look per model).

Saves: QC global `vr_map_content_seed` (vr_armedgrunt_defs.qc) is written by the engine before anything spawns and is a
saved global; VR_OnLoadGame reads it back (a save from before: 0, the map's name alone, exactly as it was). Crates and
debris in a save are entities anyway; the seed matters for replacements still to roll (a spawner's, "rolled now").

Seed 0 is today's behaviour bit for bit. It is 0 when: `vr_fresh_map_seeds 0`; a test session (`vr_backend mock`: every
kit run and the eval suite, whatever their config); a test's seeded load (seedNextMap: motion takes, vr_bench_seed).
`vr_map_seed N` (not archived) fixes it for repro (wins over everything but a save); `-1` a fresh one even under the
mock. `vr_map_seed_info` prints it and why (the Debug menu's "Map Content Seed", next to "Server Random Seed").
`vr_agrunt_test 73`'s census prints `loadouts hash H (map seed S)`.

Verified (e1m1): seed 0 crates layout c4276fdd, debris 03be04b6, census 18 replaced (7 own gun, 11 varied), equal to
the parent build; seed 4242: 22e429ac, ba8ddaa6, loadouts hash 10104 vs 24627; save at 4242, `vr_map_seed 0; load`:
seed 4242 restored, hash 10104 again; `vr_map_seed -1; restart; restart`: two different fresh seeds. vr_agrunt_test 70
22 passed 0 failed; 5 (save and load) 0 failed; eval canary 36/44, 21 differ, full 161/171, 69 differ: the same
numbers on the parent build (pre-existing drift of the pommel distances, not this change).

## Map-load work off the main thread (branch `agent/loadjobs`, 2026-11-07, on `agent/perfalloc`)

Follows perfdesign's section A (kit `scratch/perfdesign_report.md`). Method: Release-Tracy exe, exclusive runs, a new
process a load, `vr_cache_dir` a fresh folder (engine-cold) then the same folder again (engine-warm); the time is
`vr_startup_times`' last map load total (the `map` command to its first frame drawn). Tracy captures (full preset) only
for the breakdowns. Results table: kit `scratch/loadjobs_report.md`.

**Failed lookups (item 3), commit "File cache: files written outside every search path's folder forget nothing".** The
index asked for exists already (vr_fscache.cpp's listings and misses, pak stem hashes, COM_StemNowhere); what made a
first load's misses cost 879 ms on ad_tfuma (15 ms on a warm one) was the derived-data cache: each of the ~750 files
the workers write during the load reached VR_FileCacheForgetFile, which forgot every listing of a folder that wasn't
there and had the main thread scan every miss for names the file could be. The file cache now keeps the identities of
the folders lookups look in (loose search paths, pak folders; added at each window, search-path change and full
forget; never removed) and a written file whose folder isn't one of them nor inside one (named parents walked up to
the volume's root, by identity: junctions followed) forgets nothing. Writes inside them, unknown cases and folders
made behave as before. `vr_fscache_test` gained the case (24 passed). ad_tfuma cold "looked for, none found" 879 -> 20 ms.

**Skins' normal maps on jobs (items 1 and 4), cvar `vr_load_jobs` (1; 0 the serial path; Debug menu, "Load Work on Job Threads", beside "Decode Textures Ahead").** See the
comment at `VR_SkinNormalsPost` (vr_normalmaps.cpp) for inputs, outputs and joins. TexMgr_LoadImage32 posts a skin's
normal map (made from its colours, or read from the disk cache: both on the job) and returns without uploading; the
skin keeps the flat normal map until the commit (TexMgr_CommitNormalMap: upload, label, bindless handle, link). Commits
happen in post order: each later post commits the oldest finished ones without waiting; SCR_UpdateScreen (before
drawing), TexMgr_ReloadImages(Named), imagedump and imagehash join all; TexMgr_FreeTexture waits for and drops its
entries; VR_Shutdown drops the rest. TexMgr_SkinToNormals and its helpers now take the islands explicitly (MaskView)
and allocate on the heap (was the hunk; gx/gy zeroed as Hunk_Alloc did); the islands maker is pure (no hunk, no
timing); the cache read on the job counts nothing (`readRgbaAt`), the counts, the cache write and the prints are the
commit's. Ownership: the job's struct is the job's alone from the post until the Future is waited for.

Verified bit-identical: `imagehash` (every texture, normal maps included) the same frame as the `map` command and 3
frames later, vr_load_jobs 0 against 1, engine-cold (normal maps made) and engine-warm (read from the cache), e1m1,
ad_tfuma, vrfiringrange: all 12 sums equal per map (e1m1 d071c79aadc70bbc, ad_tfuma 8e89ed76898d6fd4, vrfiringrange
bab4dce68cbff385).

Loads (ms, engine-cold / engine-warm, rep 1): baseline e1m1 2990 / 1419, ad_tfuma 4574 / 1463, vrfiringrange 4050 /
1612; with both commits 2090 / 1292, 2552 / 1440, 3340 / 1437. Model light grid contents hash equal with vr_load_jobs 0
and 1 (ad_tfuma f13a9fac10031d2d, e1m1 ff18e0fd8568016c); loads print no error or warning; vr_limb_test 2 and 3 done.

**Not done (this round's budget):** authored skin normal maps' PNG decode (item 2: most of the remaining `Image decode`
after item 3), Box3D convex pieces cache (item 5), limb models' wait (item 6: their precaches stay at spawn, as model
indices depend on the order; their skins' normal maps now go to jobs with step 1).

**Round 2 (same branch).** `vr_wornpack_test 1` giving no verdict in 400 s was the script, not the branch: the test's
lines are dprints (VR_Test_Print), so its verdict shows only with `developer 1`; with it, 25 passed 0 failed with
vr_load_jobs 0 and 1.

New load-timing lines (commit "Load timing: ..."): under a model's "their normal maps" its authored files read and
decoded / looked for / shared / prepared and uploaded, the made ones posted, the weapons' relief maps; under "VR after
spawn: limb models" the lookups and posts, the builds waited for, the limb models loaded; "disk cache: a write waited
for (64 under way)". What they showed (ordinary runs, so proportions only; ad_tfuma cold / warm):
- Item 2 as briefed has little to take: ad_tfuma loads no authored normal map at all; vrfiringrange's 78 authored
  files cost 68 ms to read and decode (most come decoded ahead by vr_imgprefetch) and 117-162 ms to prepare and upload
  (1024 x 1024: the mip chain and the upload). The made ones' posts are 460-530 ms cold against 200 ms warm for the
  same 566 posts: the uploads at the commits (338 / 166 ms) and the mipping down before each post (179 / 21 ms) run
  slower while every worker is making normal maps (the main thread shares the cores); the cache's write queue waited
  0 ms on ad_tfuma, 91 ms on vrfiringrange. Next candidates: the mipping down on the job (the texels posted at full
  size), and the four AD weapons' relief maps (VR_SkinRelief: 204 ms cold, 29 warm, made on the main thread; needs the
  islands mask and the model's units copied for the job, and TexMgr_AuthoredHeights after it).
- Item 5: the held guns' pieces were already on the disk cache (vr_gun_pieces_cache, keyed by the triangles and the
  settings: vrfiringrange warm 2.7 ms); the cold cost was the cut, waited for at the first frame. Commit "Held guns'
  convex pieces cut on the pool behind the first frames": with vr_load_jobs 1 each gun is its own job; joinHeldGuns
  adds the finished ones at each world step, a hand asking for a gun (gunPieces, key -2) waits for that gun alone,
  destroyWorld for all; a model slot reused meanwhile is skipped. vr_gun_pieces_cache 2 (Check) and vr_load_jobs 0 cut
  in place as before. Same pieces: vrfiringrange's 24 guns' points hashes (a developer 1 line) equal with vr_load_jobs
  0 and 1, engine-cold and engine-warm. prepareHeldGuns on vrfiringrange cold 570 -> 1 ms (waited for later: 1.6 ms).
- Item 6: "VR after spawn: limb models" is the limb models' loads (Mod_ForName: 662 ms cold / 235 warm on ad_tfuma);
  their builds are waited for 12-22 ms, the rigs and lookups 50-120 ms. The loads stay (the precache order gives the
  model indices); their skins' normal maps already go to jobs; the cold excess is the same as above (uploads and
  draw lists made on main while the workers are busy).

Loads with round 2 (Release-Tracy, exclusive, ms, rep 1 / rep 2; before it: 2090 / 2087, 2552 / 2780, 3340 / 3362
cold): e1m1 cold 2131 / 2135, warm 1361 / 1288; ad_tfuma cold 3890 (*) / 2557, warm 1406 / 1536; vrfiringrange cold
2866 / 3020, warm 1498 / 1464. (*) An outlier: its limb models alone took 974 ms (rep 2 and earlier runs: 400-490);
ad_tfuma's and e1m1's loads gain nothing here (their guns' cuts were ~70 ms), vrfiringrange cold -0.35..-0.5 s.
## Per-frame allocations, VR programs at start-up (branch `agent/perfalloc`, 2026-11-07)

Round 1 (tree regfix): disarm hulls' vectors reused, shadow brush candidates in the shadow scratch, no folder listed for
limb models in play (the file cache answers them from the load's listings), every VR program made at start-up
(gfx::precompilePrograms: no compile at a first draw in play). armed_mix32: 71 -> 18 heap events a frame.

Round 2:

- **VR programs with the driver's parallel compile.** gfx::beginDeferredChecks / endDeferredChecks round
  precompilePrograms (only with GL_KHR/ARB_parallel_shader_compile, as GL_CreateShaders): compiles and links are queued
  without reading their status, all read in order at the end. The program labels moved there too: NVIDIA's
  glObjectLabel on a program waits for its link (480 of a cold 555 ms). Cold driver cache (a nonce in the sources, a
  temporary measuring build): 555-640 ms -> 175 ms; warm (every later start): 12 ms either way. A program that fails is
  warned about as before and relinked to one that draws (or dispatches) nothing, as its owner already holds the name
  (made one at a time it would have had 0 and skipped its pass). Checked with every VR shader forced to fail (#error):
  50 relinked, e1m1 loads and runs.
- **Disarm hulls: Box3D's quickhull into a scratch arena.** The hands' hulls are rebuilt each frame because their points
  (the drawn arm in the world) differ every frame: a last-frame cache hit 0 of 1200 builds in armed_mix32, so no cache.
  b3CreateHullWith (a marked local change in `external/box3d`, its README.md) runs b3CreateHull's code with the
  caller's allocator; disarmhull::make gives it two grown-once slots in its mem::Scratch: 4 heap events a hull -> 0, the
  same hull (same code, same points). vr_agrunt_test 16: 274 passed 0 failed (once 273/1, "Grunt dual nail pistols, the
  right one: held in its right hand", a set-up check, passing on two reruns; the base 274/0).
- **Warm-up allocations.** decals::prepare (the map's load) sets the ring to vr_decal_max and reserves each slot 12
  corners and 16 grid buckets (200 marks of an E1M1 fight and an arena wave: 12 corners at most, 97% in 8 buckets or
  fewer): about 230 KB at 768, no slot growing in play. held::drawnMesh remakes a hash-clashing entry in place (its
  vectors kept) instead of erase + emplace. The grasp, barrel mask, model collide and dequant caches are left (bounded
  warm-up, keyed by model and frame).
## Arms IK and the armed pose's cost (branch `agent/armsperf`, 2026-11-07)

The approved list from perfdesign section C, one commit each; vr_agarms_hash (new) compares runs bit for bit: the sum of
each arms result's hash (entity, frame, its places and turns) and the ARMPOSE messages' bytes as the client read them.
The fixed-frame benches are deterministic (two runs, same hashes), so every change below was checked by hash against
the run before it.

- **vr_agarms_hash, vr_agarms_hash_reset** (Debug tool): prints both hashes; the bench cfgs reset at the window's start.
- **1. Warm start (vr_agarms_warm, default 0).** A free hand's or fist's refinement (hand turned with its forearm, up to 4
  arm solves) started from last frame's forearm turn on the chest (per Ent; reset with its goal's kind, a stale solve,
  a new Ent; not settled in 3 rounds: the old search). It settles fully where the old search stops after 3 rounds, so
  the free hands' turns differ by up to ~25 degrees (vr_agarms_fast_check: 25-32% of armed_mix32's solves past 2
  degrees, hands' places 0.0035 units mean): a look change, so off until Vittorio chooses. armed_mix32: 34.8 -> 23.8 us
  a solve with it on (-32% with item 2); AD's fight: no change (its humanoids hold weapons: Frame goals, no rounds).
- **vr_agarms_fast_check** (Debug): each solve also made with the shortcuts off on a copy of its state; vr_agarms_cost
  prints how far apart.
- **2. Grip slide bisected (vr_agarms_fast, default 1).** The off hand's slide along a two-handed gun: the whole-unit
  steps and the stretch's 7 halvings give the same answer by bisection while the reach grows with the slide; last
  frame's step and halving choices are checked first (their bracket's two ends: 2 solves). armed_mix32's bench window:
  22876 solves bit for bit the old ones (hash equal); the warm-up had 1 of 7579 apart (a reach not growing with the
  slide). armed_mix32 34.8 -> 31.3 us a solve (-10%).
- **3. Distant tier (vr_agarms_lod_distant 3, vr_agarms_lod_distant_at 600; Graphics rows).** In view past 600 units:
  every 3rd frame (never more often than Far in View). AD's fight: 17492 -> 11852 solves in the window (-32%, 0.47 ->
  0.33 ms a frame); jerk (far in view) 0.0216 -> 0.0222 units mean, over a unit 13 -> 24 of 33047, most 3.675 both.
  armed_mix32 and E1M1 unchanged (nothing that far).
- **4. Solves on the job threads (vr_agarms_jobs, default 1).** agarms::solve in three: solvePrepare (main: skin, Ent and
  Lod found or made, clocks, body proxies posed, every setting read into Limits), solveCore (any thread: its own Ent,
  Lod and job only; no cvar, QC, GL or allocation), solveFinish (main: jerk, cost, glitch and error notes, hash).
  relinkArms runs relinkSkinnedBegin for each armed humanoid (its posed bones kept in a SkinWork), one parallelFor over
  the queued solves, then relinkSkinnedEnd in order; one solved or reused in place finishes at once as before. A skin
  made again flushes the queue first; debug modes that print or note inside the solve, and the fast check, solve in
  place. Hash equal with vr_agarms_jobs 0 and 1 and with the old relinkArms (armed_mix32, AD). Save and load:
  armed_mix32 saved and loaded mid-fight, solves back on the jobs after the load (5516 in 246 batches), no errors.
  Low-impact run (8 CPUs, idle priority: not a timing): the main thread 163 us a batch for ~19 solves of ~41 us.
- **5. drawnExtent cached on its DrawnMesh** (a function of the mesh alone; dropped with it). Hashes equal.
- **6. VR_AG_WeaponLine once a server frame** for a weapon on its drawn frame (vr_ag_wl_frame = framecount + 1 and the
  line on the weapon); the line from its own origin (makevectors) is worked out each time. No caller reads its side
  globals (vr_ag_df_*, vr_ag_wx_*) after it. Hashes equal.
- **7. agposesend (vr_agposesend, default 1).** VR_AGP_Pose hands the ARMPOSE message to one builtin: its globals read
  by name (Bindings::agPose, the new vr_agp_waist), flags/off-hand/stretch/take bytes and the gun's axes as arguments,
  the VM's arithmetic (rint's half in double, min/max, OP_MUL_V), each write through VR_BroadcastDest/Written. FALSE
  (cvar 0, a global missing): QC's Writes as before. ARMPOSE bytes equal (both benches).

Baseline (Release-Tracy ThinLTO, exclusive, QC zones level 2, 900-frame window): AD fight busy p50 10.70 / p99 15.40 ms,
solves 0.47 ms/f, agweaponextent 0.24, Write* builtins ~0.21; armed_mix32 5.31 / 6.98, solves 0.89, extent 0.11;
E1M1 fight 3.95 / 5.51, solves 0.08, extent 0.20. **Not measured yet:** the exclusive after-runs for items 4-7 (the
author's game was running: the timing runs could not start). `bash scratch/ap_mfin.sh` (Release-Tracy build in place)
takes them: final, vr_agarms_jobs 0, vr_agposesend 0, and all new settings off.

Checklist lines: "Armed humanoids far off in view (600+ units): arms still smooth (distant tier, every 3rd frame)";
"Armed humanoids two-handed: the off hand still on the second grip as before (bisected slide)"; "Try vr_agarms_warm 1:
free hands/fists look better or worse? (their wrists settle straighter)".
## findradius through the area grid (branch `agent/findradius`, 2026-11-07)

From perfdesign's section B (the prototype `vr_frp`, commit ebf911a47, reused for its measurements, not merged). Quake's
`findradius` walked every edict; an AD edict is ~15.8 KB, so each is a page and 2259 of them cost ~10.7 us a call, 229
calls a frame in AD's fight (the armed humanoids' three each a frame): 2.4 ms a frame.

**Engine** (`Quake/vr/vr_findradius.cpp`; PF_findradius now calls `VR_FindRadius`):
- The area grid's node arrays (world.c) keep each edict's abs box as it was linked; `SV_AreaRadiusCandidates` reads
  those (contiguous) instead of the edicts, so only real candidates touch an edict's page.
- Exact by construction: an edict is "dirty" (tested by itself at each search) from any change of its .origin, .mins,
  .maxs or .solid until SV_LinkEdict places it again. QuakeC's stores reach it through the edict index's field watch
  (vr_edictindex.cpp: the fields' value `watchGeometry`; the `OP_STOREP_V` hook it lacked, in both interpreter loops);
  the engine's through SV_UnlinkEdict, VR_EdictIndex_Touch (freed, taken, cleared, parsed), sv_phys.c's pusher solid
  restore, vr_propthink.cpp's native .solid store; SV_ClearWorld makes every edict dirty. A link whose box does not
  hold the middle (NaN, mins > maxs) stays dirty. A search compacts the dirty list (dropping the clean, the free, the
  SOLID_NOT), tests it, then the grid's clean candidates in a cube round the sphere (margin 1 + 1e-5 x magnitude), with
  the walk's very test (same expressions), and sorts by edict number: the walk's chain, in its order. The debris case
  (`vr_ar_debris`: setorigin, then .solid, unlinked a frame) is a .solid store: dirty.
- Cvars: `vr_findradius_grid` 1 (0 walk, 1 grid from `vr_findradius_grid_min` edicts, 2 always), `vr_findradius_grid_min`
  100, `vr_findradius_check` 0 (both run, the walk's chain used, differences counted and the first printed);
  `vr_findradius_stats`. Debug > Profiling and Memory rows. A non-number centre or radius, or a radius over 4096: the walk.
- Crossover: with the box cache the grid was quicker on every scene, down to the arena's 135 edicts (0.36 vs 0.52 us a
  call; armed_mix32 1.18 vs 1.43, where the prototype's grid lost); min set to 100.

**QC**: VR_AGPar_Frame skips its r170 search for monsters' blows when no monster began a melee attack in the last 1.5 s
(`vr_melee_begun_last`, set at every site that sets `.vr_melee_begun = time`, the tests' too): exact, as the loop needs
`time - m.vr_melee_begun < 1.5`. VR_AGS_Projectiles (r128) and VR_AG_WeaponStruck (r48) are left: what they want
(a client-owned moving entity with a projectile's touch, a moving thrown weapon / gib / force-grabbable) has no cheap
exact test without tagging every spawn site or an engine tally; after the grid they cost ~1 us a call.

**Verified** (`vr_findradius_check 1`, grid forced): e1m1 fight 125,026 searches, AD tfuma fight 206,297 and a long one
(3180 frames) 528,591, armed_mix32 88,190, arena waves (example_mixed, 3082 frames) 30,139, e1m1 saved mid-fight and
loaded (67,254 + 41,453): **0 differ**. vr_agarmor_test 5 (debris) 24/0 with the grid forced, and checked; vr_agrunt_test
60 6/0; 78, 96, 98, 35 fail the same on origin/next (pre-existing). End-state hashes can't compare (two walk runs differ).

**Timings** (Release-Tracy, exclusive, 2048 eyes, 900-frame window; findradius ms a frame from vr_findradius_stats, CPU
busy avg / p50 / p99 ms from vr_bench; the AD scene's busy moves +-0.4 ms between runs):

| scene | findradius ms/frame: walk -> grid -> + QC gate | calls/frame | CPU busy before (origin/next) | after |
|---|---|---|---|---|
| AD tfuma fight (2259 edicts) | 2.44 -> 0.24 -> 0.16 | 229 -> 169 | 8.48 / 9.40 / 13.51 | 6.50 / 6.99 / 10.50 (rep 6.72 / 7.21 / 10.77) |
| E1M1 fight (415) | 0.24 -> 0.06 -> 0.05 | 139 -> 104 | 2.96 / 3.34 / 4.58 | 2.81 / 3.05 / 4.97 |
| armed_mix32 (371) | 0.14 -> 0.12 -> 0.08 | 100 -> 70 | 4.56 / 4.84 / 6.83 | 4.52 / 4.75 / 6.61 |
| arena waves (135) | 0.004 -> 0.003 -> 0.002 | 7.5 -> 5.5 | 1.30 / 1.18 / 6.25 | 1.29 / 1.17 / 5.54 |

With the QC gate and the walk (`vr_findradius_grid 0`) AD reads 1.79 ms and busy 8.17 avg: the grid is ~1.6 ms of the
AD gain. The run scripts: C:/OHWorkspace/qvr-kit/scratch/findradius_report.md.

## Edict walks through the index and lists (branch `agent/edictwalks`, 2026-11-07)

The low-risk conversions of the edict audit (`C:/OHWorkspace/qvr-kit/scratch/edictaudit_report.md`, approved by the
author): each one's own commit, each giving the same edicts in the same order as the walk it replaces.
- **#6 `.vr_flameprop` indexed** (vr_edictindex.cpp `watchedFloats`): QC `findflags(world, vr_flameprop, 3)` (burning,
  wall torches) no longer walks.
- **#4 AD tanks**: `relinkWorn` (`vr_fueltank_worn`) and `syncPropRopes` (`vr_fueltank` + `vr_flailhead`, merged in
  edict order) step through the classname index; `setupStubs` through `.vr_hosecut`, indexed as **"not 0"**: a new kind
  of index field (`WatchedFloat::nonzero`, one bit, for the engine's own loops; `findflags()` on it still walks: its
  bits aren't indexed). Pending masks widened to 16 bits (9 fields now).
- **#3 FL_MONSTER walks**: corpses (watchCorpses x3, settleCorpses), footsteps, limbmodel `spawnFrame`, legsweep
  `sweepHand`, hitmodel `serverFrame` step through the flags index (`edictindex::nextFlagged`). watchCorpses and
  hitmodel clear the state of the edicts they skip, so they visit the monsters **and** the edicts they left state on
  last time (`edictindex::flaggedOrKept`; `World::corpseNums`, `hitmodel::trackNums`): every other edict's state is
  clear already (checked, below).
- **#2 Box3D slot loops** (beforeStep, liftAgain, the corpses' lift, the publish loop): `World::slotsActive`, a bitset
  set where a slot gets a body (createBody, createRagdoll: the only places a slot's kind is set), cleared when
  beforeStep finds the slot without one (Kind::None, no ragdoll, its arrival and lift just zeroed). The syncEntities
  change feed (#1) not done (medium risk).
- **#5 skewerGripped** (vr_view_hands.cpp, twice a frame): `.vr_skewer_grip` indexed "not 0"; its own test unchanged.
- **#10 VR_WP_Tug / VR_WP_Pulls** (QC): QC has no player count, so the loop ends once no edict after the current one is
  a player (`find()` on classname: the index); every edict past that did nothing in the loop (its body is for
  `classname == "player"` only), so the same players with the same numbers.
- New C++ API (vr_edictindex.hpp): `nextFlagged`, `nextNonzero`, `nextClassname` (the walk when there is no index:
  another VM current, a field not indexed), `flaggedOrKept`. The engine's queries answer only with the server's VM
  current; otherwise they walk and leave the index alone (0 index rebuilds during every fight below).
- Skipped (as decided): SV_PushMove's box query, the QC field reorder.

**Checked** (a throwaway branch `agent/edictwalks-check`: at each converted site the old walk's list and the new list
per call, timed; the corpses' and hit models' "every other edict is clear already", the Box3D slots' "every slot
outside the bitset is inert"): **0 differences** in every site, over the long fights (2650 frames each: 40 bursts) of
AD tfuma (2259 edicts), armed_mix32, the arena, and E1M1 (1126 frames); also `vr_embed_test 21` (skewering) and
`vr_wornpack_test 3`. Not exercised there: `liftAgain` (only on a slow frame's later step pieces: 0 calls; the same
bitset as the other three, which were checked), a hose actually cut (the hose test's sweep found no hose headless;
the stubs' list was compared every frame, empty).

**Measured** (Release-Tracy ThinLTO, not exclusive; the walk alone, old list vs new, us per host frame; the loop
bodies are the same either way). AD tfuma:

| site | old | new |
|---|---|---|
| flameprop (QC findflags) | 27.2 | 0.4 |
| tank worn / stubs / ropes | 30.2 / 18.9 / 30.1 | 0.6 / 0.3 / 0.7 |
| corpse watch / settle | 7.6 / 6.2 | 1.4 / 1.4 |
| footsteps / limbmodel / legsweep / hitmodel | 8.9 / 6.3 / 8.5 / 7.1 | 1.7 / 1.4 / 1.2 / 1.3 |
| Box3D slots: before / corpse lift / publish | 12.4 / 5.7 / 7.2 | 1.6 / 1.6 / 1.8 |
| skewer | 33.2 | 0.5 |
| **total** | **~210** | **~16** |

armed_mix32: ~31 -> ~4; E1M1: ~29 -> ~4; arena (small): ~7 -> ~1.3. The audit's figures were larger (its timers
wrapped whole loops with their bodies, and ran low-impact); these isolate the walks. The QC worn pack loops (#10) are
not in the table (QC; the audit put them at ~30 us in AD: 815 nextent calls a frame, now ~3 per pack).

**Tests**: `vr_embed_test 21`: 40 passed, 2 failed, the same 2 on origin/next (the gib thrown at a wall never bursts;
state 83 timeout): not from this. `vr_wornpack_test 3` never ends on origin/next either (timed out at 250 s after
the two lifts, on both builds, the same lines): not from this; open. eval.sh: no failure.
The server state hash (`vr_bench_statehash`) can't compare the builds here: origin/next gives a different hash run to
run on the same script (E1M1 fight, worn pack test), so the exactness rests on the per-call check above.
## Flat-screen slide and lean; the keys moved (branch `agent/flatmove`, 2026-11-07)

His brief (flat-screen round 2026-11-07, worker 1): Q / E lean, 3 swaps the hands, crouching at a run slides (Apex Legends),
the lean's eye and gun feed the wall tilt (worker 2's code is left alone), enemies can hit what leans out.

**Keys.** Q, E lean left / right (`vr_flat_key_lean_left` `q`, `vr_flat_key_lean_right` `e`; `vr_flat_lean_toggle` 0 hold,
1 toggle; `+flatleanleft` / `+flatleanright`). 3 swaps the hands (`vr_flat_key_swap` `q` -> `3`); the third weapon is
reached with 1, 2 and the wheel, which cycle every weapon collected. The force grab moved off E to **R** (`vr_flat_key_grab`,
new; E is the right lean now -- R was free and reloading is off in the mode: a decision, say if another key is wanted). V
(melee) and G (drop) as they were. `bind e +moveup` dropped from `Quake/default_cfg.h` and `quakevr/default.cfg` (jump
swims up). His config needs nothing: while the mode runs `Key_Event` skips the bind of a key the mode reads itself
(`VR_FlatKeyOwned`: the leans, swap, grab, prone, bullet time, F, G, V), so his `bind "e" "+moveup"` and `bind "3"
"impulse 3"` stay in the file and do nothing in flat-screen play; `vr_flat_keys_own 0` runs them too. In a headset and
outside the mode nothing changed. (vr_ settings are written sparse: his config has no `vr_flat_key_swap` line, so the new
default reaches him.)

**Slide** (`Quake/vr/vr_flatmove.cpp`, server side: `VR_FlatSlideMove` from `SV_AirMove`). Crouch pressed (or landed with it
held) on the ground faster than `vr_flat_slide_min` 220 -> speed + `vr_flat_slide_boost` 60 (to `vr_flat_slide_max` 560),
then `slideStep`: the slope's pull (`vr_flat_slide_slope` 900 x sine; uphill the same against), the move keys' turn
(`vr_flat_slide_steer` 40 deg/s), friction `vr_flat_slide_friction` 260 u/s^2, the top speed; under `vr_flat_slide_end` 130
it ends in the crouch, then `vr_flat_slide_cooldown` 0.5 s. Letting go ends it; a jump ends it keeping the speed (Quake's air
move: 340 of 344 u/s in the test); a ledge run off resumes it on landing. Found on the way: fast over the floor Quake's
FL_ONGROUND is off every other move (the walk clips down to the floor only on the moves that meet it), so "on the ground"
is the flag or the floor within 4 units and not going up -- before that, vrarena's slope (a real fall) and the flicker
looked alike. The view: `vr_flat_slide_drop` 6 more than the crouch. The body: BodyPitch's new source Slide
(`vr_avatar_pitch.cpp`): -`vr_flat_slide_body_angle` (45) about a neck `vr_flat_slide_neck_back` (10) behind the eye, the
trunk straight (lying's straightening), the chest's share of the look off, the legs out in front on the floor
(`vr_avatar_legs.cpp`). Without the neck's move back the chest filled the lower screen and hid the legs (looking 20 down);
with it the legs show past the chest. Eased over `vr_flat_slide_body_time` 0.2 s.

**Lean** (client: `flatmove::frame`, `leanShift`, `leanRoll`). The eye `vr_flat_lean_dist` 16 sideways and down round
`vr_flat_lean_pivot` 30 (4.6 at the defaults), roll `vr_flat_lean_angle` 15, `vr_flat_lean_time` 0.18 s. The pose's head
moves (placeFlat) with `State::lean` and `leanHold` 1 (the trunk tilts from the hips, the headset's lean path), the drawn
view by the same (`VR_FlatViewLean` after `VR_FlatViewDrop`). Interface to the wall tilt: none of its code changed -- it reads
the leant head and hands like everything else. The roll is the drawn view's only (the pose's angles unrolled: the drawn
hands keep the camera's roll through drawnOffset), so the gun rolls with the screen and the sights stay on the crosshair.
Wall clamp: three lines (eye, under it, the weapon's place ahead) stop it `vr_flat_lean_margin` 6 short, at once; it comes
back at the lean's speed. Not prone, sliding, swimming or dead. Walking leant `vr_flat_lean_speed` 0.6 of the speed actually
moved (the keys' length held to sv_maxspeed first: 0.6 of 400 was 0.75 of 320). Hit boxes: `VR_LeanHitBoxes` (head box
7x7x8 half, chest box at 0.55 of the way from the origin to the head) for a flat-screen client whose head is 6+ units off his
middle; `SV_ClipToLeanBoxes` (`Quake/world.c`) tries them after the entities for every non-body, non-NOMONSTERS trace (shots,
missiles, point traces: a monster's CheckAttack line sees the head too), with the usual owner rules (his own shots pass).
World collision untouched. `vr_flat_lean_hit` 0: his box only.

**Verified** (`bash Misc/quakevr/flatslide_test.sh selfshot`: 10/0): `vr_flatslide_selftest` 6/0 (flat 380 -> 250 in 0.5 s;
ends; downhill 25 deg 380 -> 440, six seconds -> 560; uphill 15 deg 188 vs the flat's 279, ends); `vr_flatslide_test` 5/0 in
vrtesthall (307 -> peak 367; 0.92 s against the friction's 0.91, 231 units, 66 moves all on the ground; ends crouched; jump
340 of 344); `vr_flatlean_test` 4/0 (eye 16.0 right, roll 15.0, down 4.6; a line across the leant head 20 off his middle hits
him, misses with his box alone; walking leant 192 of 320 = 0.60; leant into a wall: the eye 6.0 off it, 15 of 16); sights
leant right / right crouched / left within 0.001 units (`vr_flat_sight_check`), the shot 0.0000 deg off its point leant;
the keys by `vr_mock_key`: E, Q lean, 3 swaps, R grabs, the binds on E and 3 run only with `vr_flat_keys_own 0`.
`flatkeys_test.sh` updated for 3 (was Q): 37/0; `flatmove_test.sh` 10/0, `flatcrouch_test.sh` 3/0. (In the vrtesthall
pictures the slab over the first-person slide is a shells box the slide ran over and the off hand took: walked-over
pickups.) Pictures: scratch/fm_pose.png (slide first person and from the side, the lean
first person / front / behind, crouched), scratch/fm_slidefp.png (the slide's legs seen past the chest), scratch/fm_ads.png
(ironsight leant, standing and crouched).

**To try (desktop, -flatscreenmode):** run and hold Left Ctrl (slide, legs in front looking down), a ramp down (faster) and up
(stops), jump out of a slide; Q / E at a doorway's edge, ironsight leant, lean into a wall (no clipping), a grunt shooting
your leant head.

## Hand motion source, and the flat-screen shove (branch `agent/flatinput`, 2026-11-07)

**The bug.** On the flat screen the hands are posed from the view, so a fast mouse flick swings them (and what they hold)
through the world as fast as a real swing: with `vr_flat_flick_test 144 40` (40 degrees a frame, yaw and half of it in
pitch, the other way every three frames) an axe chopped a wall and a gib and played five blow swishes and sounds. Every
gesture that fires on motion read that motion as the player's.

**The layer** (`Quake/vr/vr_handsource.hpp`, `.cpp`): each frame every hand's motion has a source.
- `Tracked`: a controller's. `Synthesized`: made by the game or derived from the camera (the flat-screen look, the wall
  tilt, a lean, a slide, recoil). `Action`: the game's motion for an explicit button action that the gesture rules must
  judge (flat-screen V's punch, the fire button's punch with a melee weapon).
- `hands::update` calls `handsource::beginFrame(vrActive())` before the hands are placed: Tracked in the headset,
  Synthesized on the flat screen. Whatever moves a hand calls `handsource::mark(h, Source::..., "why")` that frame,
  before the move is built; the strongest mark wins (Action > Synthesized > Tracked). **Other features plug in here**:
  lean, slide, wall tilt and recoil mark the hands they move as `Synthesized` (on the flat screen that is already the
  base; in the headset it would turn gestures off for that frame, so mark only motion that is really made up).
- `handsource::gestures(h)` is the one rule (Tracked or Action; `vr_handsource_gate 0` lets everything through, the old
  way). The consumers ask it, nothing else:
  - client: the super shotgun's flick reload and its pry/lift (`vr_flick.cpp`), the flashlight's flick
    (`vr_flashlight.cpp`), the chainsaw's cord pull (`vr_chainsaw.cpp`);
  - the move (`vr_client.cpp handMove`): a hand that fails it sends no velocity (`vel`, `ownVel`, `angVel`, `velMag`
    zero; the throw's estimate stays: letting go is a button), so every server gesture that goes by the velocity sees a
    still hand (the force grab's flick pull, the revolver's push, the reload's tosses and ejects, a crate burst by a
    hand, a hand's hit reaction, projectiles inheriting the hand's speed);
  - the move's new byte `handSynth` -> QC `.handsynth` (bits 1 off, 2 main), read by QC `VR_HandMotionTracked(p, hand)`
    (`vr_util.qc`), the same rule for the server's detectors that go by the poses: `VR_Melee_Track` (every swing, slap,
    bash and hand shove) and `VR_Revolver_Flick` (the cylinder's flick open and shut).
- Buttons are untouched: V, the fire button's punch, reload, `+flickreload`, the eject button, the shove key.
- `vr_handsource_debug 1` (Debug > Flatscreen Play) prints each hand's source as it changes.

**The flat-screen shove** (mouse 5, `vr_flat_key_shove "mouse5"`; `vr_flat_shove_now` from the console): the headset's
one-hand shove of an open palm, straight ahead of the body at chest height (QC `VR_FlatShove`, impulse 53 the off hand,
54 the main): what it finds (`VR_Bash_Find`, its reach times `vr_flat_shove_reach`) is shoved by the same landing as a
palm's push (`VR_Bash_Land`, split out of `VR_Bash` unchanged): stamina, a counter's window, the stagger and push, the
sound, the cooldown. The engine draws the hand's push (out a third of `vr_flat_shove_time` 0.4 s, `vr_flat_shove_anim_reach`
14 units ahead and in towards the middle) and sends the impulse as the hand reaches out; the push is synthesized motion,
so the melee rules do not read it as a slap of its own. The hand: an empty one (the off hand first), else the off hand.
Cvars (menu: Movement > Desktop Screen Play): `vr_flat_shove 1`, `vr_flat_shove_cooldown 0.8`, `vr_flat_shove_push 1`,
`vr_flat_shove_reach 1`; also `vr_flat_shove_damage 1`, `vr_flat_shove_time 0.4`, `vr_flat_shove_anim_reach 14`.

**Tests.** `bash Misc/quakevr/flatinput_test.sh <agent>` (14 checks, "flatinput: N passed, M failed"): 40-degree flicks
with an axe at a grunt, the revolver, the super shotgun: no blow, swing out, eject, flick or pry; the same flicks with
`vr_handsource_gate 0` do strike (the control); the axe walked into the wall's tilt and out: no blow; V's punch lands and
is marked an action; two shoves land, a press within the cooldown does nothing; the mock headset's hands are never
synthesized. The headset: the melee eval (eval.sh) and the revolver's `vr_rv_test 1`; the super shotgun's flick and pry
by `Misc/quakevr/reload/reload_test.sh`.
## Flat-screen wall: slide along, no ironsight tilted, no torch model (branch `agent/flatwall4`, 2026-11-07)

His jitter: moving along a wall facing along it slightly away, then turning a little towards it. Repro `scratch/fw4_sweep.sh`
(vrtesthall's south wall on the right; s: his box on it, the view turning 5 deg/s from 10 out to 50 in; w: walking along it
from 10 out, turning 15 deg/s for 20 degrees; frame by frame the drawn gun's place and turn in the view: the new
`flatwalldrawn:` and `flatslide:` lines under vr_flatplay_debug 2). Before (vr_flat_wall_slide 0): walking, the rifle went
from level to 85 degrees up within 20 degrees of the wall (a frame's turn up to 75 deg; shotgun 70); standing, the rifle's
turn stepped 30 deg in a frame (4.1 units), the shotgun's 6.3. Cause: gunBack's way back towards the shoulder runs nearly
along a wall met at a slant (1/out: 10 units back for 1 in, and none past its leastOut 0.1), the walk-in's ahead probe the
same, and the stop at the chest cut the room from 16 to 2.5 units within a degree, so the tilt came all at once.

- **Slide along** (`vr_flat_wall_slide` 1, `_from` 15, `_to` 40 deg, `_time` 0.12 s; Flatscreen > Free Hands rows;
  `slideFor`): the gun's level way against the wall's normal (gunDepth now hands back the deepest point's normal) gives a
  share: 1 up to `_from`, smooth to 0 at `_to`, eased. With it the drawn gun turns level out of the wall (`flatTurn`, in
  drawnTurn after the tilt: the least that clears, up to the share of parallel; a melee weapon never) and the way out leans
  from the way back to the wall's level normal, its most up to 10 units (`slideOutMost`) past the chest stop. The wall
  state's effects (fold, room, raise, the free hand's let-go, the elbow) are scaled by 1 - share. Head-on: as before.
- Measured (rifle / shotgun): standing, turning in: a frame's step 0.49 / 0.39 units, turn 2.9 / 1.9 deg (before 4.1 / 1.9
  units, 30 / 6.3 deg); it lies along the wall from about 25 deg in (where it first reaches the wall from his box) and tilts
  up from about 33. Walking: turn 5.2 / 5.0 deg a frame (before 75 / 70); the place's 7.3 / 4.4 unit step is flatplay's
  step sway (the velocity's change kicks the hands) as his box meets the wall: there before, slide off too, not changed.
  Settled on the wall at yaw 5 to -50: every drawn point out (vr_gun_wall_depth 0.0 cm, on and off), the rifle keeps its
  two-handed hold to 25 deg in (20 before).
- **Block ADS** (`vr_flat_wall_block_ads` 1): adsApplies is false while the tilt up (`upAngle`: the drawn turn less the
  level one) is over 5 deg; the key's ask is kept. vr_flat_wall_block_fire reads the same tilt up (laid along, it fires).
- **Torch model** (`vr_flat_flashlight_hide` 1): flatscreen play hides the torch's entity after `place`; beam and light stay.
- Tests: flatwall e (new: the turning sweep has no step, 0.49 units / 2.9 deg a frame; lies along before it tilts up; the
  ironsight blocked tilted) 3/0, b-d all pass, flatsweep 383 passed, 27 known, 0 failed. flatwall a and flatsight a crash
  on origin/next's base (`SV_ModelIndex: model progs/v_gruntgun.mdl#adv0 not precached`, vr_test_give_wid on a grunt gun,
  slide off too): before it, flatwall a's 136 reads all out of the wall (0 over 2 cm), flatsight's 12 guns all on the ray
  (0.0011 units at most); flatsight b-d pass. Pictures: `scratch/fw4_shots_off.png`,
  `scratch/fw4_shots_on.png` (rifle at 0, 15, 25, 35 deg in).
- Lean (agent/flatmove): the leaned eye and gun come in as flatplay poses them (`s.head`, `s.pos`, `s.rot`, the crosshair
  ray); slideFor and pushBack read only those, so a leaned pose feeds the wall code with no change here.

## flatwall a: the two-handed sighted holds at a wall, after block ADS (branch `agent/flatwall5`, 2026-11-07)

With the precache crash fixed (2c8754e4e) flatwall a ran to its end and failed "sighted, the tilt keeps every two-handed
hold": 18 of the 24 sighted two-handed reads (weapons 8, 9, 10, 12, 16, 28) lost the support hand with the tilt on.
Bisected with the fix cherry-picked: e0e821f7b (flatwall3 in, before flatwall4) 0 lost; 4c96bc061 (flatwall4) fails;
next 18 lost; next with `vr_flat_wall_block_ads 0` 0 lost. So it is block ADS, not a hold regression: the test asks for the
sight with the gun already tilted up at the wall (`vr_flat_wall_tilt_sighted 1`), the ironsight waits as designed, the
pose stays hipfire and the free hand lets go as hipfire's does at a wall.

- The check is now two: of the two-handed sighted holds, those still sighted with the tilt keep the hold (6: weapons 8, 9,
  16 at 45 degrees and looking up), and every other one is a sight that waits (18). Each read takes one frame of
  vr_flatplay_debug 2 for the ironsight's state. The script outgrew the command line (bash "Argument list too long"): it
  goes in `quakevr/scratch_flatwall_a.cfg` (git-ignored), run with exec, its waitN spelled out as run.ps1 does.
- The `flatsight: t ... ads` debug field says `waits` (was `no sights`) when the ironsight is asked for, the weapon has
  sights and block ADS holds it.

## Perf review fixes (branch `agent/regfix`, 2026-11-07)

The review of the 2026-11-06/07 performance changes (five findings, the save check, the nits), each fixed:

- **A held mesh remade in place kept the old mesh's extent** (vr_held.cpp drawnMesh, 4c5f12102 + fa63a90c6). The
  in-place branch now resets extentDone/extentOk/the ends/radius, and the hash takes the whole DrawnTransform (the
  weapon model transform `t`: vr_wofs, Weapon Studio, vr_gunmodelscale; the header's `so`/`hs`), so a mid-map tune makes
  a new mesh instead of taking that branch. New `vr_agrunt_test 214` (vr_armedgrunt_extent_test.qc): a sword grunt's
  agweaponextent front end 8.15, vr_gunmodelscale x1.5 -> 11.64, put back -> 8.15 (3/0).
  VR_AG_WeaponLine's kept line now also gives back vr_ag_df_o/_fw/_up and vr_ag_wx_back/_front (five more fields), so
  a caller reading those after it sees this weapon's.
- **findradius grid: C moves before QuakeC with no relink.** VR_FindRadius_Dirty after SV_FlyMove's origin copy
  (before SV_Impact's touch functions), after VR_BeforePlayerPostThink's compat-muzzle shift and after
  VR_ClientTeleport's origin write. The other C origin writes the probe lists (sv_phys.c SV_WalkMove/TryUnstick/
  CheckStuck restores, vr_physics.cpp's sweep `place`, which restores before returning) relink before any QuakeC.
  Check (vr_findradius_grid 2, vr_findradius_check 1): e1m1 1200 frames 63,468 compared, AD tfuma 1500 frames 57,490,
  vr_agrunt_test 60 (6/0): 0 differ.
- **File cache: a write through a junction's target** (vr_fscache.cpp, 03c5042bb). Each listing's folder identity goes
  into `roots.listed` (locked, deduplicated, never taken from); underRoots also matches the written file's own folder
  against it, so D:\maps\x.bsp forgets the id1/maps -> D:\maps listing.
- **Skin normal maps on jobs: the duplicate guard** (vr_normalmaps.cpp). VR_LoadNormalMap also returns when the skin
  has a normal map still pending (VR_SkinNormalsPending: a scan of pendingSkinNormals by base), so a skin asked twice
  before its join gets one texture and one job.
- **A failed deferred shader compile** (vr_glutil.cpp, 5e5039db8). Owners say which variable holds a program and its
  failure flag (`gfx::ownDeferred(program, &failed)`, right after making it); endDeferredChecks gives a failed one's
  owner 0 and sets the flag (its fallback or skipped pass, as a one-at-a-time make), and deletes the program. Every
  program gfx::precompilePrograms makes has its owner (vr_gfx_gl: triangles, particles, tube, reverse-order and half-size
  composites, bent mesh; bloom, bttrails, dynres, foveated, haze, lighting (no flag: asked again at next use, as
  before), stereo mirror, temporal, upscale, water). relinkToNothing stays only as a net for an owner not said.
  Parallel compile kept. Not exercised (shaders compile on the dev GPU).
- **Save check:** vr_melee_begun_last is a plain global: a save written on vrtesthall has `"vr_melee_begun_last"` in its
  globals and loads back. The WeaponLine kept-line fields are saved as fields; their frame key (framecount + 1) can't
  match after a load (framecount is saved and moves on before the first read).
- **Nits:** packRestCache dropped when it would hold more than 64 shells (vr_agarmor.cpp); the disarm hull arena
  gives a block past its two slots from an overflow list dropped at the next call (no Sys_Error); vr_wornpack.qc's
  two missing spaces.

Tests: vr_agrunt_test 214 3/0, 77 12/0, 60 6/0; vr_wornpack_test 1 25/0; flatwall 18/0, flatsight 12/0, flatsweep 383/0 (27 known); melee eval
no verdict changes (PASS->PASS 0.1 degree drifts); e1m1 and a save loaded clean.

## Leather bridge at elbows and knees (branch `agent/regfix`, 2026-11-07)

His note: the leather between the plates is a good idea, but it is rigid, does not bend round the elbows and knees and does
not join the next part's plates. Before: `sleeveOf` made each joint's leather once at load as two halves (the upper part's
down to the joint, the lower's up from it), skinned as the skin under them; near the joint the skin is mostly one bone,
so the halves moved rigidly, never met, and the elbow's skin showed between them when bent.

Now (`vr_agarmor_bridge 1`, the default; `vr/vr_agarmor_bridge.cpp`): one tube per joint made every frame the body is
drawn, from the posed last row of the upper part's plates (pauldron, thigh) to the posed first row of the lower part's
(bracer, shin):
- **Ends:** each end ring is that row's six feet (`cornerAt` with `plateWeights`, moved by armour.txt's tweaks with the
  plates, the weights quantised as `makeModel` quantises them), so it is the drawn plates' row to the float (the check:
  0.004-0.005 units from the plates' nearest vertex at every angle, which is `plateFootApart`). A part not worn: that end
  is a ring on the skin where its row would be (`bodyExit` rays, 0.25 units off, skinned as the skin there).
- **Curve:** Hermite from one row's middle to the other's along each row's direction, the tangents reaching the joint's
  posed pivot along each row's line (a Bezier's inner points there), so it turns close round the elbow's point instead of
  cutting the corner as a circular arc did (the arc let the elbow's skin through by up to 2.5 units). Each ring's frame:
  the two rows' frames slerped, turned onto the curve's direction. The rows' corners are lerped in those frames; the
  lower row's corners are matched to the upper's order round the limb at load (lerped by index, a mismatched pair crossed
  the middle and pinched the enforcer's tube shut).
- **Bellows:** `folds` folds (rings `2 x folds + 1`, at least 7), a fold's top `fold_depth` out on every other ring. Their
  depth by bend: all round on a straight limb, 1.25x on the inside of a full bend (90 degrees or more) and none on its
  outside (stretched flat). The rings themselves bunch on the inside as the curve turns. The atlas's joint block is
  mapped a painted fold per geometric fold (make_agarmor.py paints four down the block); 0 folds: the old mirrored v.
- **Skin under it:** up to 96 skin vertices between the rows (on the two limb bones, nearest the row-joint-row line away
  from its ends, within 1.35x the rows' mean radius: a held gun's stock is not skin) are posed each frame, found between
  two rings, and the corners round any that would show pushed out (0.2 off the skin, at most 5 units, three passes, each
  push shared 0.6 with its neighbours so it is a swell, not a spike). Not on the inside of a bend (the limbs overlap
  there) and not within half a ring of either row (the plates' own fit).
- **Drawing:** an entity per armoured body (`relink`; the player's: `entities.plate[plateBridge]`, vr_view_body.cpp) with
  the model `<model>#bridge` whose every vertex is its own bone (252 at most: 4 joints x 9 rings x 7, within the skinned
  path's 256): `bonePoses` sets each vertex's matrix to its rest normal turned onto its normal now and its rest place
  carried onto its place now, once a frame, so lighting, shadows, culling and the depth caches work as on the plates.
  No per-frame allocation (stack arrays; the bones in file-scope buffers).
- **armour.txt:** the sleeve parts' `taper_start` / `taper_end` (the upper sleeve's over the first half, the lower's over
  the second, eased to 1 at the rows), `standoff`, `scale`, `leather "0"` (on either sleeve part: that joint's leather
  gone); new `folds` and `fold_depth` (the upper sleeve's, else the lower's, else `vr_agarmor_folds` 4 /
  `vr_agarmor_fold_depth` 0.3); Armour Studio rows Leather Folds and Fold Depth on the sleeve parts. A sleeve's offset
  and rotate no longer apply (the tube has no rigid place to move).
- `vr_agarmor_bridge 0`: the old load-time sleeves, for comparison (the shells are made again when it, the folds or the
  depth change). `vr_agarmor_bend <deg>` (debug): every armoured body in its rest pose with elbows and knees bent.
  `vr_agarmor_headcam` takes a 4th argument, `drop` (units lower: the joints).

Cost (vr_agarmor_bridge_stats; quick build, low-impact run, so high): 5.8-7 us a joint, 69 us a frame for three grunts
(12 joints), the player's 15 us a joint (24 skin samples). Made once a frame, whatever the views and shadow passes.

Tests: `vr_agarmor_bridge_check` 40/0: every joint of the grunt, enforcer, knight and death knight swept over its hinge's
range (9 steps): the end rings on the plates' rows (0.004-0.005; worn alone too), no NaN, the skin between under the
leather (0 out; a few in the crease and at the rows, reported); armour.txt's leather 0, taper and folds on the grunt's left
knee, and the shell back as before when cleared. `vr_armour_test` 30/0 (its sleeves' checks now run on
`vr_agarmor_bridge 0`), `vr_agarmor_check` 20/0, `vr_agarmor_test 1` 26/0, `vr_pp_test 1` 53/0. Before/after shots:
`scratch/leather_side_g*.png` (grunt from his right, 100 degrees), `leather_front60*.png`, `leather_knights*.png`,
`leather_player.png`.

Open: the Armour Studio's sleeve outline (vr_agarmor_debug's lines for a sleeve part) draws nothing with the bridge on.

## Crawling with the arms (branch `agent/regfix`, 2026-11-07)

His words: lying down, crawl forward by lifting the arms, reaching ahead, and pulling back towards yourself and the
ground; the stick little or nothing, as swimming. "Lying down and crawling"'s left-over item.

- **The stroke (vr_physics_water.cpp `VR_AfterCrawlMove`, after `SV_AirMove`).** VR only, lying down
  (`hull::isProne`), the swimming strokes' gate (`strokingMove`, shared now: tracked hands, not flat-screen, not
  noclip). A hand pulls while it moves back along the floor (within about 66 degrees of straight back from where you
  look, at `vr_prone_crawl_min_speed` 0.3 m/s or faster) on the floor: it starts under `vr_prone_crawl_plant_height`
  10 units over the feet (or coming down at 0.2 m/s or more under twice that) and goes on under twice that. The body
  goes against the hand along the floor: the hand's level speed times `vr_prone_crawl_gain` 2.5 (1: a hand gripping
  the floor exactly), reached in a few frames, never braking the crawl along it. Between pulls the crawl glides out
  (`vr_prone_crawl_friction` 3 /s), at most `vr_prone_crawl_max_speed` 120 u/s. On the ground the player's speed along
  the crawl is kept up to it (Quake's floor friction would eat a slow crawl). Either hand, in turn or together.
- **Not from (decision).** A hand through the air (no plant: reaching ahead does nothing, nor coming back high), a hand
  whose motion the game made up (`.handsynth`, `vr_handsource_gate`), a hand whose trigger fires (main: button0; off:
  the off-hand attack): empty hands and held weapons or melee alike crawl, aiming from the floor doesn't (it needs the
  hand low and moving back briskly; firing never).
- **The stick.** `vr_prone_stick_scale` 0.25 (0 none, 1 all) of the crawl's speed (`vr_prone_speed_scale` 0.3: 96 of
  320, so 24) while `vr_prone_crawl` is on, in VR; it feeds the crawl as a pull does (SV_AirMove alone, its ground
  friction over its acceleration at 24 wanted, went 3 u/s), and `hull::crouchSpeedMax` caps SV_AirMove's own stick at
  as much. Flat-screen untouched (its keys crawl at 96).
- **Feedback.** A pulse in the hand as it plants (`vr_prone_crawl_haptic` 0.3; 0 none) with the climbing hold's palm
  slap by the floor's texture (`physsound::grab`, `vr_physsound_grab` loud).
- **Settings.** Movement > Player Hitbox > Lying Down: Crawl With Your Arms (`vr_prone_crawl` 1), Stick While Crawling
  (0.25), Arm Crawl Strength (2.5), Arm Crawl Pull Speed (0.3 m/s), Arm Crawl Floor Height (10 units), Arm Crawl
  Friction (3), Arm Crawl Top Speed (120), Arm Crawl Haptics (0.3). Debug: Arm Crawl Pulls (`vr_prone_crawl_debug`:
  each plant's height and each pull's length and speed), Force Hands Synthesized (`vr_handsource_force`, a new test
  aid: both hands' motion counts as synthesized in a headset too).
- **Tests.** `Misc/quakevr/crawl_test.py <agent>` 8/0 (mock, vrteleporters' crouching room): three reach-and-pull
  strokes 175 units forward; reaching and coming back through the air 0.5; the strokes standing 0, crouching 0, with
  `vr_handsource_force 1` 0, with `vr_prone_crawl 0` 0; the stick lying down 24.0 u/s, standing 320. `prone_test.py`
  21/0 (it sets `vr_prone_stick_scale 1`: its checks are the box's and the stick's whole crawl), `swim_sound_test.sh`
  11/0.
- **Left:** the tuning (gain, floor height, min speed) wants a real headset on the floor: the mock's hands at 0.04 m
  are 2 units over the feet.

## Knight's right elbow leather spazzing (branch `agent/flatinput`, worktree flatwall, 2026-11-07)

- **Root cause.** The knight's sword arm is bent at rest. Its bracer's first row sits before the corner where the two
  rows' lines meet, and the elbow's pivot lies 3.7 units off the upper row's line. The "round" Hermite tangents reach
  the pivot's projection on each row's line, so they came out four chords long (14.3 units for a 3.8-unit chord). The
  curve looped: its tangent turned 281 degrees for a 79-degree bend. At the middle rings the tangent pointed back
  against the rings' slerped frames (cosine -0.998). Each ring's frame is the slerped frame turned onto the tangent by
  the shortest turn, and that turn has no defined axis at 180 degrees, so the rings spun from one frame to the next.
  This happened in 727 of the knight's 864 animation steps; on other joints only in 2 steps of the grunt's left elbow
  (the knight's left elbow, by comparison, never turns back past cosine 0.85).
- **Fix** (`vr_agarmor_bridge.cpp` evalBridge). The curve's worst cosine against its frames is measured with its own
  tangents. Below 0 the tangents are eased towards half a chord, and below -0.5 they are a half chord (`bridgeBackFrom`
  / `bridgeBackTo`). Joints whose curves never turn back are unchanged.
- **The same class, found on every model by the new check** (all made continuous; shapes unchanged away from the old
  switch points):
  - The push's least-norm share (`need / sum2`) grew without bound next to an end ring, whose corners can't move. A
    sample sliding towards a row flung its corners out to the 5-unit cap and back. Now `sum2` is at least 0.25, a cell
    middle's value (`bridgePushShare`).
  - The crease's hard edge turned a sample's push on and off. The push now fades out over 0.3 past the edge, in the bend
    and in the cosine round the ring (`bridgeCreaseFade`). Pushes outside the crease are unchanged.
  - A skin sample was placed in the first stretch between posed ring planes that held it. In a bend's crossed planes it
    jumped to another stretch far along. It is now measured at its rest place along the leather
    (`Bridge::underAlong`, set in bridgesOf).
  - The round/arc tangent switch at 0.15 of the chord moved both tangents together. Each end is now eased on its own
    reach, from 0.1 to 0.15.
  - polygonReach takes the outermost crossing rather than the first edge found.
- **Check.** `vr_agarmor_bridge_check` lerps each model's animation frames to the next in 8 steps (`rig2::posedBetween`,
  the game's own blend). No leather vertex may move more than 3x its end rings' furthest plus 0.5 units in a step.
  `developer 1` prints each joint's worst step with the curve's numbers. Before the fix: 41 passed, 15 failed (15 of
  16 joints). The knight's right elbow moved 7.71 units against 0.44, and pushes jumped up to 5 units on grunt,
  enforcer and death knight joints. After: 56 passed, 0 failed. The worst steps are the enforcer's left elbow (2.25
  against 0.72) and the knight's right elbow (2.06 against 0.57), both in fast swings.
- **Other tests.** `vr_armour_test` 30/0, `vr_agarmor_check` 20/0, `vr_agarmor_test 1` 26/0.
- **Screenshots.** `scratch/knightelbow_before_after.png`: the knight's right elbow, three-quarter view, idle. Top row
  is before, bottom row is after, and the left and right columns are two frames apart. The mean pixel change between
  the two frames in the crop is 2.33 before and 1.61 after. Stills show the spin poorly; the check's numbers are the
  evidence.
- **Test in VR.** The knight's right elbow while it idles, walks and swings: the leather should stay put round the
  elbow. Check the other joints too, for no new lumps near the rows (the push next to the rows is now smaller).

## The shotgun's sight housings (branch `agent/regfix`, 2026-11-08)

His note: "Can you please start a worker to improve the shotgun ironsights like in this ChatGPT mockup?" (raised metal
housings with recessed luminous inserts instead of floating glowing rectangles; the mockups are in the kit's
`scratch/shotsight_mockup/`).

- **Geometry** (`v_shot.blend`, edited headless: a one-off bpy script, not kept; the .blend is the source). The old
  glowing blocks (20 + 10 triangles) are gone. Rear: one solid across the receiver at x 9.75..10.41, its back flat, its
  front leaning, a U notch (floor 0.27 under the line, walls leaning out) between two trapezoid ears 0.4 tall, their
  tops 0.13 over the line; its bottom sunk into the receiver (the receiver's two holes under the old posts closed).
  Front: a ramp post on the muzzle band, its back flat (0.42 wide at the top), its top one grid step (0.067) over the
  insert. Higher won't fit: the kick's frame `shot3` lifts the muzzle to the model's box top (10.48), and the box is
  kept. 53 new vertices, all on the gun's body (`qvr_part` 0), riding it in every frame; the pump parts re-exported.
- **Skin.** The old sight patch (s 61..123, t 89..124; only the old blocks read it) repainted: metal patches (Quake's
  brown-grey row and the receiver's blue-grey, dithered, lit top edges) and the ears' and the post's back faces, each a
  metal frame round a glowing insert (palette 234 rim, 235, 236 core: fullbright, recoloured by `vr_sight_hue`). The
  x grid is 0.22 units (0.56 cm): a modelled recess would hide the insert from above, so the recess is painted (a dark
  line over and beside the insert, a lit lip under it) and the relief has the inserts one step deep (2 levels) for the
  normal map. The notch's floor is plain metal.
- **Sight line.** Rear 9.754 -0.050 6.545, front 32.406 -0.005 6.212 (were 9.856 -0.050 6.545, 32.519 -0.005 6.212):
  -0.843 deg up, 0.115 left (were -0.842, 0.114): turned 0.001 deg, so no `--allow-sights`, no realign, weapons.txt
  unchanged. The insert tops sit on the old posts' tops; the points moved 0.11 back (the inserts are on the back faces).
  vr_sightalign's table keeps `Rear::Notch`. sight_picture.py's line updated: the front insert seen down to 0.27 under
  its top, as before (the rest under the rib at x 19.4).
- **The mockup's colour:** `vr_sight_hue 73` (yellow-green); his default (-1, the player's hue) is unchanged.
- **Not done: `g_shot0.mdl`** (the pickup) keeps its old glowing blocks: its own low-poly mesh, a 256x128 skin and a
  z grid of 0.145 units (the housings' details are 0.067), and no one aims through it. A follow-up if wanted.
- **Checks.** The export: anchors ok, holes as before (2 see-through, 8 wide cracks: the old ones), sights turned
  0.001 deg; an untouched re-export writes the same bytes. `vr_sight_lines`: v_shot "sights rear 9.75 -0.05 6.54 front
  32.41 -0.00 6.21". flatsight_test 12/0; flatsweep 383 passed, 27 known, 0 failed.
- **Screenshots** (`scratch/` of the worktree): `ss_before_flat.png` / `ss_after_flat.png` (flat: hipfire, ADS, ADS at
  hue 73), `ss_before_blend.png` / `ss_after_blend.png` (Blender: in hand, the rear and the front close up),
  `ss_after_ads_crop2.png` (the sight picture, default hue and 73, and the housing in hipfire).
- **Follow-up: the barrel mask, and the export's add-on.** The first export left weapons.txt's shotgun barrel mask at
  "1908:" for a 1928-triangle model: the engine drops a mask whose count differs (no barrel glow mask). Two causes:
  the mask was carried over only when it numbers the .blend's reference's triangles, and the shotgun's reference is
  gunlooks.py's input (1876 triangles, the rivets come after); and `blend_export.py` run in a worktree used the add-on
  Blender had already loaded from the author's checkout (its `sys.path` insert came too late). Now blend_export.py drops
  an add-on loaded from elsewhere and imports its own checkout's, and a mask for the shipped file's triangles follows
  them by their corners (frame 0): 386 of 1928 marked (was 396 of 1908: the old front post's 10 are gone).
- **His black slivers on the receiver's top** (between the rear sight and the eye): not found in the model. On the
  shipped and the new v_shot.mdl, its pump body and fore-end, every frame: no open edge, T-joint or crack on the top
  (vertices welded by place in every frame), no triangle folded over, the top covered once seen from above, no black
  texel and no bad normal-map texel under its triangles, no thin skin triangle (a near-singular normal-map frame), none
  in the barrel mask. A one-sided render of the quantized model from above and behind shows none. Likely the renderer,
  not the mesh; reproducing it needs his setup (map, auto pump, graphics preset).
## Flat-screen fly camera (branch `agent/flatinput`, worktree flatwall, 2026-11-07)

Vittorio: a fly camera in flatscreen play that moves anywhere while the body, hands and arms stand still, pausing the game
by default. Full description: FLATPLAY.md, "The fly camera".

- **Use.** F8 (`vr_flat_key_flycam`) or `vr_flat_flycam_toggle` (or `vr_flat_flycam 1/0`): mouse looks, W A S D fly,
  Space up, Ctrl down, Shift x3, no collision. Cvars: `vr_flat_flycam_pause` 1, `_speed` 320, `_accel` 1600 (0 instant),
  `_fast` 3. Rows under Desktop Screen Play; Debug > Reports has the toggle and the test.
- **How he stays still** (`Quake/vr/vr_flycam.cpp`). Look: `CL_AccumulateCmd` saves `cl.viewangles` before the frame's
  look and `VR_FlatFlycamLook` turns the camera by the difference and puts them back (the server's setangle before that
  still lands). Move: `VR_AdjustMove` zeroes it (the Weapon Studio's path). Buttons and impulses: zeroed in `CL_SendMove`;
  `client::attacking` false. Hands to the server: `poseMove`'s posing hold (the last move before it took off, walked
  along with him), also keeping 2H aim, the flatscreen and crouch bits. Mode keys, ironsight, hit punch, lean: not
  stepped in `flatplay::frame`. Drawn hands: `viewIsHisOwn`, `bodyRide` and `drawnOffset` read his own eye
  (`flycam::eyeOrg/eyeAngles`, captured at the end of `V_CalcRefdef` before the camera takes it; kept as it was while the
  game is held).
- **Pause.** `flycam::holds()` joins `VR_RuntimeMenuPause` (no `SV_Physics`, no `SV_ClientThink`; no `sv.paused`, so no
  PAUSE plaque and the mouse still reads). Only when `sv.active && svs.maxclients == 1` and not a demo: a multiplayer
  server or a remote one never pauses for one client's camera (decision).
- **Body.** Nothing new: the avatar already draws the head for any view more than a head's length from the eyes.
- **Left as is.** A body animation already in flight (a punch, a shove, a crouch's ease) is frozen where it was; the
  drawn pose's own idle eases run on `vr_gametime`, which a pause does not stop, so a pose still settling when the camera
  comes on settles (the test waits 2 s after spawn: before that the hands were still moving 0.3 units).
- **Test.** `Misc/quakevr/flatflycam_test.sh` (`vr_flat_flycam_test`): 21 checks: on, held, +right turns the camera
  179 deg, +back/+jump fly it 231 units off (+attack held), screenshot; in to 80 units, +lookdown, screenshot
  (`scratch/flatflycam.png`); origin and angles moved 0.000, hands 0.006 units at most, 0 buttons sent, game time
  unchanged; pause 0 with +forward/+jump/+attack held for a second: time ran 1.000 s, he stayed; off: view back to 0.0000
  units, 0.0000 deg. In the mock headset the toggle only prints a note.
- **Test on the desktop.** F8 in a fight: fly round, look at the body, hands and weapon from the front and back; F8 again
  and nothing should have changed. `vr_flat_flycam_pause 0`: monsters move, he doesn't, no shots from clicking.
## Nail pistol sights (branch `agent/release`, 2026-11-07)

Vittorio: "Please add the nail pistol to the sighted models" (he paints its glowing sights in Blender himself).

- **What changed.** `vr_sights.cpp` `sightedModels`: `progs/v_nailpistol.mdl` (also its own pickup) and
  `progs/vr_nailpistol_preview.mdl` (Debug menu, Nail Pistol Art). Its magazines (`vr_mag_npistol`,
  `vr_mag_on_v_nailpistol`, `v_nailpistol_mag`) have their own skin and no sights: left out. `vr_sightalign.cpp`
  `painted`: the nail pistol, Rear::Notch (two rear ears, a front post); until the skin has sights, the table's line
  (`vr_sightalign_table.inc`) is used as before. The Blender add-on's `sights.py` `PAINTED` lists it too, and the
  export (`source.py`) now reports a new sight line (none before) and warns to run the align, instead of saying nothing.
- **Fire texels.** None in any shipped nail pistol skin (`v_nailpistol`, `vr_nailpistol_preview`, the three magazine
  models), nor in the author's work in progress (2026-11-07): no muzzle flash, light or screen to repaint.
- **After painting.** Paint in the exact Fullbright swatches (qpal.py: on a texel that did not glow, a near colour stays
  ordinary). Paint the two rear ears and the front post, never the notch floor between the ears (the rear sight is
  read as the widest gap across: a painted floor leaves no gap, and the line falls back to the table's). The first
  export is not refused (no old line: the 0.25 degree check has nothing to compare), so no `--allow-sights`. Then in
  the game: `vr_sight_lines` (the nail pistol says `sights`, not `table`), and `vr_weaponstudio_align main apply`
  (flat-screen, the nail pistol in the main hand). Later exports are held to 0.25 degrees.
- **Tests** (a scratch copy painted 236 on the ears' and the front post's top faces, swapped in for the runs; scratch
  `np_paint.py`, `test_npsights.sh`). `vr_sight_lines`: `sights rear -1.21 -0.00 6.38 front 9.15 0.05 6.38 (0.29
  left)`, the same as the add-on's `sights.py` reads on that file; the shipped model: `table`, as before. The skin as
  uploaded (`imagedump`) at `vr_sight_hue` 30 and 120: 405 texels differ, exactly the 405 painted, hue 29 to 119;
  nothing else. Flat view, paused: `vr_weapon_glow` 0 to 0.3 brightens the sights ((255,217,184) to (255,237,202));
  hue 120 moves the drawn sights' hue as the shotgun's in the same shots (mean 28 to 59; the shotgun 21 to 55); the
  shipped nail pistol through the same steps: ~100 grey pixels (noise). `flatsight_test.sh`: see the report.
## Flat-screen: the hidden torch's lens glow (branch `agent/embed5`, 2026-11-07)

Vittorio: "the flashlight model is invisible but I still see a floating circle where it used to be only when the
flashlight is on".

- **Cause.** `vr_flat_flashlight_hide` hid the torch's view entity, but `drawLens` (vr_flashlight.cpp), the additive
  disc in the beam's colour drawn over the lens while the light is on, checked only that the light was on and placed.
  It went on drawing at the hidden model's lens. Nothing else is drawn from the lens: the visible beam is an open cone
  in the air (no cap, and no circle was seen with the lens moved into view), and the dynamic lights are lighting.
- **Fix.** New `view::drawn(ve)` (vr_view.hpp/.cpp): visible, with a model, and not hidden by `vr_shot_hide`, the same
  test the view uses to submit the entity. The flashlight keeps a pointer to its view entity (`setupView`), and
  `drawLens` draws only when `view::drawn` says the model is drawn. The lens therefore also goes with `vr_shot_hide 2`.
  The beam, the spotlight and the lamp glow light are unchanged. VR is unchanged (the model is drawn there).
- **Probe.** `vr_flashlight_probe [tag]` prints a new line, `torchprobe <tag> lens drawn D model M`: the disc drawn in
  the last two frames, and the model drawn. flash_grab_test.py ignores lines it doesn't know.
- **Check.** `bash Misc/quakevr/flashglow_test.sh <agent> [tag]` (with `flashglow_check.py`; it prints
  `flashglow: N passed, M failed`): flat-screen, light on, at a wall and down
  the corridor, lens 0 and model 0; `vr_flat_flashlight_hide 0` gives lens 1 and model 1; VR mock, torch on the head and
  lit, gives lens 1 and model 1. After: 4 passed, 0 failed. Before: 2 failed (lens 1 with model 0).
  A one-off variant (not committed) did the same with the torch moved into view (`vr_flashlight_head_forward 0.12`,
  `_out -0.05`, `_up -0.06`), to show the disc. Screenshots: `scratch/flashglow_before_inview_flat.png` (the bright
  circle at the wall and in open space) and `scratch/flashglow_after_inview_flat.png` (gone, the wall still lit). The
  tiles are: wall with light on, corridor with light on, light off, and model shown.
- **Test in VR.** Flat-screen with F on: no circle anywhere, and walls still lit. In the headset, the clipped or held
  torch's lens still glows when it is on.
## Grow the Box on Export on Save (branch `agent/loadjobs`, worktree phases, 2026-11-07)

Vittorio: "add the option to the plugin please" (v_nailpistol.blend refused: 15 vertices outside the model's box, "tick
Grow the Box", an option only File > Export's dialog had).

- **Checkbox.** Grow the Box is in the add-on's Quake VR panel (3D view sidebar), in the top box under Export on Save,
  for a weapon's source .blend. It is the model object's `qvr_grow_box` (default off), saved in the .blend, so the
  background Ctrl+S export reads it. `blend_export.py --grow` does the same. File > Export's dialog starts from it.
- **Where the box comes from.** A source .blend's export starts from the reference model (`qvr_mdl`, its header as
  built). Export on Save runs on the saved file in a background Blender and saves nothing back, so the reference
  never learned a grown box: every save would grow again from the old box (and an edit near the edge would move the
  grid each time). Now the export starts from the grid of the model file it replaces when that grid holds the
  reference's box (`mdl_blender.replaced_box`). Once grown, it stays grown, grows again only when a vertex leaves the
  grown box, and an unchanged .blend saves to the same bytes. An anchor whose surface moved no more than the grids'
  half steps is a warning, not a refusal (the grid moves every vertex a little).
- **Offsets.** When the low side grows, the origin moves and the weapon's Scale pivots about it. The export writes
  the compensating Offset X/Y/Z (`x`/`y`/`z`, old minus `moved x (1 - Scale)`) into weapons.txt with the anchors. It
  measures from the replaced file's origin, so a second save adds nothing. The `  box:` report lines (the grid, the
  `inc vr_wofs_*` console lines for his saved config, what weapons.txt got) pop up after Ctrl+S, and the status bar
  says THE BOX GREW. A shipped grown model still needs a `vr_wofs_version` step for other players' configs.
- **Refusal message** names where the option is (the panel, File > Export's dialog, `--grow`).
- **Test** (headless, copies in `scratch/gb`): an untouched copy exports the shipped v_nailpistol.mdl byte for byte. With
  3 vertices moved 3 units past -x, it is refused without grow and grows with `--grow` (origin x -2.58 to -5.52, `inc
  vr_wofs_x_37 1.6202`, weapons.txt slot 36 x -0.18 to 1.4402). A second export with or without `--grow` is
  byte-identical (it starts from the grown file). The stored `qvr_grow_box` alone gives the same file. Exported into
  the worktree's own progs, weapons.txt was written once and the second save changed nothing (the files were put
  back after).
- **Test in Blender.** On a copy of a weapon's .blend, pull a vertex past the box and save: a refusal popup names the
  checkbox. Tick Grow the Box and save: the status bar says THE BOX GREW and a popup shows the offsets. Save again: no
  popup, the same file.
## The shotgun's sight housings (branch `agent/regfix`, 2026-11-08)

His note: "Can you please start a worker to improve the shotgun ironsights like in this ChatGPT mockup?" (raised metal
housings with recessed luminous inserts instead of floating glowing rectangles; the mockups are in the kit's
`scratch/shotsight_mockup/`).

- **Geometry** (`v_shot.blend`, edited headless: a one-off bpy script, not kept; the .blend is the source). The old
  glowing blocks (20 + 10 triangles) are gone. Rear: one solid across the receiver at x 9.75..10.41, its back flat, its
  front leaning, a U notch (floor 0.27 under the line, walls leaning out) between two trapezoid ears 0.4 tall, their
  tops 0.13 over the line; its bottom sunk into the receiver (the receiver's two holes under the old posts closed).
  Front: a ramp post on the muzzle band, its back flat (0.42 wide at the top), its top one grid step (0.067) over the
  insert. Higher won't fit: the kick's frame `shot3` lifts the muzzle to the model's box top (10.48), and the box is
  kept. 53 new vertices, all on the gun's body (`qvr_part` 0), riding it in every frame; the pump parts re-exported.
- **Skin.** The old sight patch (s 61..123, t 89..124; only the old blocks read it) repainted: metal patches (Quake's
  brown-grey row and the receiver's blue-grey, dithered, lit top edges) and the ears' and the post's back faces, each a
  metal frame round a glowing insert (palette 234 rim, 235, 236 core: fullbright, recoloured by `vr_sight_hue`). The
  x grid is 0.22 units (0.56 cm): a modelled recess would hide the insert from above, so the recess is painted (a dark
  line over and beside the insert, a lit lip under it) and the relief has the inserts one step deep (2 levels) for the
  normal map. The notch's floor is plain metal.
- **Sight line.** Rear 9.754 -0.050 6.545, front 32.406 -0.005 6.212 (were 9.856 -0.050 6.545, 32.519 -0.005 6.212):
  -0.843 deg up, 0.115 left (were -0.842, 0.114): turned 0.001 deg, so no `--allow-sights`, no realign, weapons.txt
  unchanged. The insert tops sit on the old posts' tops; the points moved 0.11 back (the inserts are on the back faces).
  vr_sightalign's table keeps `Rear::Notch`. sight_picture.py's line updated: the front insert seen down to 0.27 under
  its top, as before (the rest under the rib at x 19.4).
- **The mockup's colour:** `vr_sight_hue 73` (yellow-green); his default (-1, the player's hue) is unchanged.
- **Not done: `g_shot0.mdl`** (the pickup) keeps its old glowing blocks: its own low-poly mesh, a 256x128 skin and a
  z grid of 0.145 units (the housings' details are 0.067), and no one aims through it. A follow-up if wanted.
- **Checks.** The export: anchors ok, holes as before (2 see-through, 8 wide cracks: the old ones), sights turned
  0.001 deg; an untouched re-export writes the same bytes. `vr_sight_lines`: v_shot "sights rear 9.75 -0.05 6.54 front
  32.41 -0.00 6.21". flatsight_test 12/0; flatsweep 383 passed, 27 known, 0 failed.
- **Screenshots** (`scratch/` of the worktree): `ss_before_flat.png` / `ss_after_flat.png` (flat: hipfire, ADS, ADS at
  hue 73), `ss_before_blend.png` / `ss_after_blend.png` (Blender: in hand, the rear and the front close up),
  `ss_after_ads_crop2.png` (the sight picture, default hue and 73, and the housing in hipfire).
- **Follow-up: the barrel mask, and the export's add-on.** The first export left weapons.txt's shotgun barrel mask at
  "1908:" for a 1928-triangle model: the engine drops a mask whose count differs (no barrel glow mask). Two causes:
  the mask was carried over only when it numbers the .blend's reference's triangles, and the shotgun's reference is
  gunlooks.py's input (1876 triangles, the rivets come after); and `blend_export.py` run in a worktree used the add-on
  Blender had already loaded from the author's checkout (its `sys.path` insert came too late). Now blend_export.py drops
  an add-on loaded from elsewhere and imports its own checkout's, and a mask for the shipped file's triangles follows
  them by their corners (frame 0): 386 of 1928 marked (was 396 of 1908: the old front post's 10 are gone).
- **His black slivers on the receiver's top** (between the rear sight and the eye): not found in the model. On the
  shipped and the new v_shot.mdl, its pump body and fore-end, every frame: no open edge, T-joint or crack on the top
  (vertices welded by place in every frame), no triangle folded over, the top covered once seen from above, no black
  texel and no bad normal-map texel under its triangles, no thin skin triangle (a near-singular normal-map frame), none
  in the barrel mask. A one-sided render of the quantized model from above and behind shows none. Likely the renderer,
  not the mesh; reproducing it needs his setup (map, auto pump, graphics preset).

## The super shotgun's notch sight (branch `agent/selfshot`, 2026-11-08)

His note: "The new shotgun sights are great. Can you make the same change to the super shotgun? I'd like to have the
circle removed, and use the same idea as the normal shotgun." (His mockup `scratch/shotsight_mockup/ssg_mockup.webp` in
the kit: its blocky rear notch sight liked, its front post too bulky: slim like the shotgun's; glowing inserts on the
rear's ears as the shotgun has.)

- **Geometry** (`v_shot2.blend`, edited headless: a one-off bpy script, not kept; the .blend is the source). Gone: the
  ring (96 triangles), the old front patch (24) and the bead (28), each a piece of its own; every glowing texel of the
  skin (2493, all theirs) repainted steel. Rear: one closed solid on the rib just in front of the barrels' back, x 12.82
  (back) to 13.05 (the ears' tops) / 13.28 (below), a square U: a notch 0.55 wide, its floor 0.28 under the line (one
  step over the rib), between two square ears 0.29 wide, their tops 0.28 over the line (0.28 over the barrels' tops);
  its bottom in the rib. Front: a slim ramp post at the muzzle, its back at x 27.82 (0.33 wide at the top, 0.48 lower),
  its top one grid step over the insert, a ramp down to x 28.27, a block under it down into the barrels. 30 new
  vertices on the barrels (past make_ssg_open.py's cut, x 12.5): both sights swing open with the barrels; the frame
  part keeps its 535 triangles. The z grid is 0.056 units, y 0.037.
- **Skin.** New patches in free skin (s 128-196, t 98-145): metal (Quake's brown-grey row and the blue-grey, dithered,
  lit top edges), the ears' and the post's back faces each a metal frame round a glowing insert (palette 234 rim, 235,
  236 core: fullbright, recoloured by `vr_sight_hue`), the recess painted (a dark line over and beside, a lit lip under),
  as the shotgun's. No relief PNG for this gun (its normal map is baked from the model).
- **Sight line.** Rear 12.823 -0.030 7.7225, front 27.818 0.0066 8.112 (were the ring's middle 12.325 -0.028 7.545 and
  the bead 27.924 0.001 7.945): 1.488 deg up, 0.140 left (were 1.468, 0.108): turned 0.02 up, 0.03 left (the grid:
  0.5 y steps of play). It is 0.17 higher than the ring's: with the inserts' tops on the old line the barrels' backs
  (the V between them) hid the ears' inserts but for small triangles in the sight picture; one grid step higher (3
  steps, the post's top too) they show whole (the kick's frame `shot4` lifts the post to 248 of the box's 255: no
  growing). sight_picture.py: the front seen down to 0.41 under its top (was 0.25). The shots turned with the line:
  weapons.txt slot 2 `shot_pitch` 3.09 -> 3.11, `shot_yaw` 0.19 -> 0.22 (his promoted values, their offset from the sights
  kept; his own `vr_wofs_shot_*_03`, if saved, untouched). vr_sightalign's table, sights.py `PAINTED`: `Rear::Notch` /
  "notch" (was ring); sight_picture.py's line; the empties `sight rear` / `sight front` moved onto it.
- **Barrel mask:** 374 of 915 triangles (was 480 of 1011: the ring and the patch were marked; the new sights aren't, as
  the shotgun's). `fx_gfx_mask` "915:..." matches the model.
- **Checks.** The export: holes as before (0 open loops, 3 cracks, 0 wide); the breech face's middle 12.77 -0.03 6.77 and
  the barrels' back 9 points as before (vr_view.cpp's ssg constants hold). `vr_sight_lines`: v_shot2 "sights rear 12.82
  -0.03 7.72 front 27.82 0.01 8.11 (1.49 deg up, 0.14 left)". Flat `vr_flat_sight_check`: rear 0.0001, front 0.0001
  off the ray.
- **reload_test.sh** (section 7): the ring check reads the rear sight's box x 12.0-13.4 (ssg_checks.py); the sights'
  colour checks look from above and behind with the gun held level (both guns' inserts are on back faces now: the side
  camera saw 1-3 pixels of the super shotgun's and 12 of the shotgun's), at least 15 pixels (was 50), the ammo screen's
  text white, and the fired shot 2 s after the shot (from above the barrels' red heat glow lies in the sights' box: 46/395
  at once, 46/0 later; the sights stay yellow); the blood check keeps the side camera.
- **Not done:** `g_shot.mdl` (the pickup) keeps its old look; nobody aims through it.

## Lying body: slide and prone, on the floor (branch `agent/embed5`, 2026-11-08)

His flat-screen notes (fly camera shots): sliding, the legs bent, the holsters and ammo pouch floating up by the head and on
screen, the walk's bob making the feet go through the floor; lying down, the body floating above the floor, the legs not
straight. Plus: a slide sound; the legs pointing the slide's way, not the camera's.

- **Measured first** (`vr_body_lie_test`, `vr_avatar_lie.cpp`: the posed body against a trace down to the real floor and the
  drawn eye). Before: prone pelvis 20.4, chest 16.7 units over the floor, knees bent 51/56 deg, feet 20.7 up; slide knees
  35 deg, the body yaw 15 deg off the view (the stance), the pelvis bobbing 4.0 .. 9.3 and the eye -3.3 .. 2.0, the ammo
  pouch 15 units from the eye 28 deg off the view, the gear up to 10 units off where it sits on the standing body.
- **Prone float: the target's sign.** `pronePitchTarget` went past 90 as the eyes stood higher, meant to slope the hips
  down to the floor; turning the hanging trunk forward past 90 lifts the pelvis above the neck instead (the flat eye is
  14 over the floor: 111.7 deg, the hips 20 up). Now it is the turn that puts the pelvis `vr_prone_hip_floor` (3.5) over
  the box's floor: `a = atan2(torso, torsoBack) + asin(-drop / r)` from the drawn trunk's own lengths (the pelvis to the
  head's bind, `vr_body_torso_back`, `vr_body_eye_up` under the eye), capped by `vr_prone_body_angle` (now the most, 90).
  Flat (eye 14): about 60 deg, propped on the elbows, the hips on the floor. The headset too (its floor from the server's
  box: the calibrated height had the hips 2 units up): the mock's head at 0.28 m gives pelvis 3.5.
- **Straight legs.** Lying down and sliding, the foot's target is `reach` (`vr_prone_leg_reach` 0.997,
  `vr_flat_slide_leg_reach` 0.993) of the leg from the hip, down to the floor, along the body's facing; the spread across
  the body is out along its own left (it was the foot's yaw, off the body's by the stance: one leg 2% long, one short:
  bends 0 and 19). Knee bend: prone 9/9, slide 14 (was 51/56 and 35).
- **The gear on the slid body.** The slide's neck-back move (`vr_flat_slide_neck_back`) also moved the Follower's upright
  reference (solveTorso's reference poses), so everything it carries came out 10 units forward: the ammo pouch in front
  of the camera. Reference poses no longer take it. And the hip holsters and ammo pouch no longer ride the thighs by the
  lying and sliding share (`Follower::thigh`: the legs out in front swung them up). Every item now sits as far from its
  bone as standing (0.0 off).
- **2H stance in a slide.** The stance's torso turn (`flatsight::stanceYaw`) eases out over `vr_flat_slide_turn_time`
  (0.12 s) while sliding (flatplay poseHands).
- **Legs the slide's way** (`vr_flat_slide_legs` 1, `vr_flat_slide_legs_twist` 70): the body's yaw goes to the slide's
  velocity, the chest turns back to the view about the trunk (`flatplay::trunkTwist`, half in the spine; less by
  cos(pitch) so that seen from above it faces the view once leant back). Test: the view turned 40 deg in a slide, legs
  0.0 off the slide, chest 0.6 off the view.
- **No bob in a slide** (`vr_flat_slide_bob` 0): Quake's view bob (`V_CalcBob`, view.c through `VR_FlatBobScale`) and
  flatplay's walk bob (hands, body ride) go by the slide pose's share. Pelvis 5.17 .. 5.25, eye constant.
- **Slide sound** (`vr_slidesound.cpp`, `quakevr/sound/vr/slide_start.wav` and `slide_loop.wav` from
  `Misc/quakevr/make_slide_sounds.py`, synthesised, seeded): a thump and scuff as the slide starts, a looped gritty scrape
  (cue point) whose volume and pitch follow the speed (full at 480 u/s), fading as it slows and 0.18 s after it ends;
  `vr_flat_slide_sound` (0.6). Test: started once, up to 0.77, 0.63 -> 0.27 over the pose, stopped 0.06 s after.
- **Test:** `bash Misc/quakevr/body_lie_test.sh <agent>`: flat 20/0, vr 5/0. `vr_body_pitch_test` 30/0 (the prone target's
  checks rewritten), `flatslide_test.sh` 10/0. Debug > Tests: Lying Body Test, Lying Body Report.
- **Rows:** VR Settings > Body: Lying Down Hips Over Floor, Lying Down Leg Reach; Debug > Tunables > Flatscreen > Slide:
  Slide Leg Reach, Slide Stance Turn Time, Slide Legs Follow Slide, Slide Legs Most Twist, Slide Walk Bob, Slide Sound
  Volume.
- **Not done:** the flat prone eye stays 14 over the floor (its drop is the camera's, not the body's): lying down is propped
  on the elbows rather than flat on the chest; lower `vr_prone_hip_floor` or the eye for flatter.
## Flat tools: force grab key, F7 toolgun, weapon_* classnames (worker C, 2026-11-08)

- **Force grab (flat)**: `vr_flat_key_grab` now `mouse4` (the side button the shove's `mouse5` leaves); the old R stays as
  `vr_flat_key_grab_alt` (`r`; no clash with Q/E/V/G/3/F7/F8). Both are flat-owned keys (`vr_flat_keys_own`). With both
  hands empty the main (dominant) hand grabs, else the empty one (QC `VR_Forcegrab_FlatGrab`). Effects are the headset's:
  the lock-on sparks (`QVR_PARTICLE_PRESET_GUNFORCEGRAB`) at the grab, then `VR_Forcegrab_Launch` (forcegrab.wav), and the
  flat view now queues `fgfx::queue(s, true)` (glow outline, tendril, trail; its glow lines go to the scene pass:
  `lines::sceneGlow`, flat has no UI pass). Checked: `vr_flatitems_test 1` 9/0 (the off hand still takes with the main
  hand busy).
- **F7 toolgun**: `vr_flat_key_toolgun` (`F7`) sends impulse 49 (QC `VR_FlatToolgun_Toggle`): the toolgun in the main hand,
  what it held kept (weapon, flags, its record entity); F7 again gives it back. With the toolgun in the main hand the left
  mouse button (the main hand's fire) is its trigger (`toolgun::button(Trigger)`), and `vr_flat_key_toolgun_menu`
  (`mouse3`) is its B/Y (opens/closes the panel). NOT DONE: the panel is still drawn only in the headset's eyes and
  pointed at with the other hand's laser, so in flat it opens unseen; the plan is a screen overlay (the panel's texture
  drawn centred over the 2D pass, the desktop mouse moving a cursor via an `IN_MouseMotion` hook while it is open, the
  left button as `panel::trigger`).
- **weapon_* classnames** (his decision): `weapon_axe` (AD's name for the same weapon; AD maps keep AD's own pickup via
  `adOwnedClashes`), `weapon_knight_sword`, `weapon_hknight_sword` (flags 1), `weapon_chainsaw`, `weapon_gruntgun`,
  `weapon_enfrifle` (named after their WID_ and models, as weapon_nailpistol/revolver). Each a one-line wrapper over
  `VR_WeaponClass_Spawn` (buttons.qc: func_weapon_grabbable with the table's model, `VR_Pack_WeaponAvailable` gate); the
  weapon table's classname column and `VR_GunPrep_ClassWid` name them; quakevr.fgd regenerated. None clashes with
  vanilla, Rogue, Hipnotic, MG3 (weapon_superaxe, weapon_bloody_*) or AD names other than the axe. The AD soldiers'
  guns keep their AD spawn rows (pack-gated, no new names); the grapple has none (a hook, not a lying weapon).
  Toolgun Weapons entries for these six use the classnames; Debug > Cheats > Spawn Pickup Weapons gains Axe, both
  swords, Chainsaw, Grunt Burst Rifle, Enforcer's Rifle and Super Axe. Tests: `vr_pickup_test 9` (classtest 15/0: each
  spawns, is picked up into the main hand, the two guns fire), `vr_pickup_test 10`/`11` around a save and load
  (classsave 3/0).
- NOT DONE: the empty-hand pose (relaxed / guard cvar).
## Flat-screen: the lean at a wall, the prone lean, the prone gun (branch `agent/loadjobs`, worktree phases, 2026-11-08)

His report: leaning against a wall only rolled the view; lean while lying down (smaller, the body rolling in place);
tunables for the prone gun pose; the crouch pressed lying down changed the gun pose; looking down lying down the gun
went into the floor.

- **The lean at a wall: the cause.** `leanRoom` (vr_flatmove.cpp) traces three lines sideways: from the eye, 8 under it
  and from the weapon's place 20 units ahead of the eye. Facing a wall he stands against (his box's front 16 off his
  middle, the wall 17), that third line started inside the wall, `startsolid` gave it room 0, and the lean's eye move
  was cut to nothing: the view rolled, the eye stayed. Leaning away from a side wall was fine. **Fix:** the weapon's
  line starts only as far ahead as is open (a line forward first, stopped 2 short of what it meets); a line from under
  the eye or from ahead that starts in something (a ledge, a step) is skipped, not taken as a wall at the side (the eye's
  own still counts). Only the way the lean goes is ever traced, so away from and along a wall it goes all the way.
- **Lean lying down.** Allowed now; its full eye move and roll blend to `vr_flat_lean_prone_dist` (3) and
  `vr_flat_lean_prone_angle` (8) by `flatplay::proneRatio`. The lying trunk rolls in place by the same:
  `flatmove::proneLeanRoll()` (the roll times the prone share), applied by `detail::bodyLeanRollTurn` (vr_avatar_pitch.cpp,
  about `b.fwd`) composed onto `bodyPitchTurn` at one place only, the trunk's turn in vr_avatar.cpp (`bodyLeanRollTurn(b)
  * bodyPitchTurn(b)`); the legs and the lying pose itself are untouched (worker A's). `leanShare` now divides by the
  current full move (the walk's lean slowdown reads it).
- **Prone gun pose tunables** (Debug > Tunables > Flatscreen > Held Weapon): `vr_flat_gun_ahead_prone` / `_down_prone` /
  `_out_prone` at a level look, and `..._prone_down` / `..._prone_up` looking straight down / up, along Gun Out's smooth
  curve (`proneAt`, vr_flatplay.cpp), taking over from the standing/crouched ones by the prone share. Defaults are the
  standing values (16.5, 2, 1; out -4 at either end), so nothing moves until he tunes them.
- **Crouch from prone.** A crouch press (Left Ctrl or `+flatcrouch`) while lying down clears the prone ask: the prone
  share eases out while the crouch's eases in (the eye takes the deeper of the two), the gun from the prone pose to the
  crouch one. Before, he stayed lying down and the crouch's gun pose came on top.
- **The gun flush on the floor** (`vr_flat_prone_floor` 1, `vr_flat_prone_floor_margin` 1; Debug > Tunables > Flatscreen
  > Wall). `handpose::gunDepth` skips floors on purpose (standing, the gun's end in the floor is hidden); lying down
  (prone share over 0.5) `gunDepthFloor` adds `handpose::gunFloorDepth` (the weapon's points against floors only, the
  margin under each) to the wall tilt's and the room's measures. When the floor is what the gun is in, the tilt is
  all of it (sighted too, up to 85 whatever `vr_flat_wall_tilt` says), so the existing wall-tilt blocks hold: no shot
  (`vr_flat_wall_block_fire`), the ironsight waits (`vr_flat_wall_block_ads`). The status's `flatwallturn:` line ends
  with `floor 0/1`. e1m1, shotgun, prone, looking down: before tilt 0, wall state on with the gun's end in the floor;
  after tilt 44.7 deg, block 1, 0.23 left (under the 0.25 the tilt counts as clear).
- **Tests.** `vr_flatlean_test` gains: leant away from the side wall (16.0 of 16), facing the wall leant right and left
  (16.0 of 16 each): 7/0. New `vr_flatprone_test` (Debug > Tests: Flat Prone Test): lying down leant right (eye 3.0,
  roll 8.0, body roll 8.0); crouch pressed lying down: up into the crouch in 0.61 s, the gun's largest frame step 0.64
  units; looking down (80) with the ironsight asked: flush (47.8 deg, 0.20 left), no shot, ironsight 0.00; looking level:
  off the floor, shots, ironsight 1.00: 4/0. `flatslide_test.sh` runs both: 11/0.
- **Not done / not checked:** VR is untouched (the lean and the flush are flat-screen only). The prone lean's eye move
  is not clamped by the floor (3 units sideways). The body roll is a few degrees and was only glanced at from the fly
  camera.
## Irons level: the shotguns' and the lightning gun's sights (branch `agent/selfshot`, 2026-11-08)

His note, with flat ADS screenshots (`scratch/shotsight_mockup/ads_shot_misaligned.webp`, `ads_ssg_misaligned.webp` in the
kit): the glowing inserts lined up on the aim, the irons didn't (the rear ears' metal tops well over the front post's);
"in real guns, people look for the alignment of the irons, not of the glowy bits". Then the lightning gun
(`lightning_ads.webp`): rear posts asymmetric, their gap off centre, the glow sitting on top, the front post lopsided.

- **The sight line is the irons' tops** (`vr_sightalign.cpp` `fromTexels`, `sights.py` `sight_line`): the rear and front
  heights are the highest corner of the triangles the sight texels are painted on (the faces the inserts are on), not the
  texels' own. One rule for every painted gun, no per-model number: where an insert is painted to its face's top (the
  old lightning gun's blocks) the two are the same. The nail pistol has no painted texels and keeps its table line.
- **Shotgun** (`v_shot.blend`): the ears' tops lowered one z step (0.0666) to 6.6116, the front post unchanged (6.2784):
  the iron line is the old insert line raised 0.0666 at both ends, so it is parallel: -0.843 deg up, 0.115 left, as before
  (turned 0.000). The ears' back faces keep their texel size (their top corners' UVs two rows down). Inserts: the rear
  ones' tops 1 row (0.033) under the ears' tops (the patch moved up a row: dark line on the top edge, insert, lip), the
  front one's 4 rows (0.133) under the post's top (two core rows fewer). In the sight picture (eye ~11 units behind the
  rear, ~34 from the front) those are about the same angle under the iron line (0.003-0.004 rad), so the inserts' tops
  line up too. The relief PNG moved with them.
- **Super shotgun** (`v_shot2.blend`): the ears' tops lowered 4 z steps (0.2226) to 7.7781 = the old insert line + one
  step; the post unchanged (8.1676): 1.488 deg up, 0.140 left, as before. Rear inserts' tops 1 row (0.028) under the
  ears' tops, the front one's 3 rows (0.083) under the post's (one core row fewer). The ears now just clear the barrels'
  backs (their humps reach the same height): the inserts show whole in the notch's sides, as before.
- **Lightning gun and plasma gun** (`v_light.blend`, `v_plasma.blend`: the same sight geometry, one script for both):
  a rear housing like the shotgun's, a closed solid across the receiver (x -2.378..-1.354, 1.76 wide, its bottom sunk),
  centred on the barrel's line (y -0.098, the muzzle's; the old notch was at -0.1715 and the posts' bottoms lopsided):
  a square U notch 0.44 wide, its floor 0.274 under the tops, between two ears 0.293 wide, their tops at 11.4806 (the box
  top, as the old posts'); the old posts sunk inside it (the receiver's slope that ran up to them now comes out of its
  front face). Front: a square post 0.293 wide on the same line, top 11.3435 (the old pyramid's apex, removed), its back
  face at x 31.0765. Each ear's and the post's back face a metal frame round a 5-wide glowing insert (234 rim, 235, 236
  core), the rear ones' tops 1 row (0.034) under the ears' tops, the front's 2 rows (its top 4 rows show over the
  barrel); new UV patches in free skin (v_light s 22-52, v_plasma s 144-174, t 94-102); the old glow (s 145-158,
  t 133-143) repainted metal. Line: rear -2.378 -0.098 11.4806, front 31.0765 -0.098 11.3435: -0.235 deg up, 0.000 left
  (was -0.238, 0.012 left: turned 0.012 deg; no shot offsets in weapons.txt for these slots). Barrel masks "948:..." for
  both (were 865), written by the export. Holes as before (6 open loops, 4 cracks, none see-through).
- **sight_picture.py**: the lines updated; the shotgun's ears now +0.003 over the line (were +0.070), the post 0.000; the
  super shotgun's front 0.000 (the rear "ears top" +0.234 it prints is the barrels' backs at x 12.37, not the ears).
- **Flat ADS screenshots** (`scratch/` of the worktree): `ads_iron_after.png` (shotgun, super shotgun),
  `light_ads_after.png` (lightning gun).

## Black slivers on the shotgun (branch `agent/regfix`, 2026-11-08)

His note: two small black triangles on the flat top of the shotgun's receiver behind the rear sight, on every map,
not in Blender (his `gl_texturemode GL_NEAREST_MIPMAP_LINEAR`, anisotropy 16, `vr_foveated 2`, auto pump on).

- **Reproduced** headless in vrtesthall (his map in the picture) with his graphics cvars: `vr_weapon_grip_mode 1;
  impulse 154; vr_mock_hand main 0.0 1.345 -0.30 55 0 0; vr_mock_camera 0.03 1.405 -0.235 35 0; vr_eyeshot 1` (the
  mock hand's pitch 55 holds the gun about level: 70 is level). The slivers: 43 near-black pixels in the eyeshot's box
  570-610 x 565-610.
- **Bisected** (one run, a cvar toggled between eyeshots): no change with `vr_foveated 0`, `r_oit 0`, `vr_autopump 0`,
  `vr_normalmaps 0`, `vr_parallax_models 0`, `vr_specular_aa 0`, `vr_rim_light 0`, `vr_weapon_reflections 0`,
  `vr_retrolight 0`, `gl_fullbrights 0`, `vr_sight_saturation 0`, `r_lerpmodels 0`, anisotropy 1, `GL_NEAREST`,
  `GL_NEAREST_MIPMAP_NEAREST`; gone with any linear magnification (`GL_LINEAR*`). So: the skin's own texels read nearest.
- **Root cause.** A shader that wrote each pixel's skin texel and `gl_PrimitiveID` (temporary) put the black pixels on
  v_shot.mdl's triangle 1849 (the receiver's top, x 3.8-6.9, z 6.2), reading texels (368-373, 39-40). The triangle's
  corners sit on texel middles (gl_mesh.c's + 0.5: (362.5, 42.5), (381.5, 37.5), (377.5, 42.5)); its slanted top
  edge runs through texels whose middles lie outside it, and the painted part of the skin steps back there: those
  texels are the skin's black background. Nearest filtering reads them along the edge (the slivers); linear blends
  them in (Blender). The skin, not the renderer: the same holds for every model whose UV islands are painted only up
  to the texel middles (v_shot.mdl 3350 such texels, 122 of them dark against a lit inside; the id soldier 1231 / 183).
- **Fix (general):** `vr_skin_edge_fill` (default 1, archived; a Debug menu toggle "Fill Skin Edges" beside Show
  Foveation, to compare) and
  `vr/vr_skinedge.cpp`. As an MDL loads (Mod_LoadAliasModel, before its skins upload), every texel a triangle's skin
  hull touches (more than 0.001 of a texel) whose middle no triangle has gets the neighbour (sides first, then
  corners) the touching triangle has inside, else any neighbour some triangle has inside; a list of (texel, source)
  pairs per model (146 models, 81524 texels in e1m1's set). TexMgr_LoadImage8 / LoadImage32 apply it after the clean
  skins' patch to the 8-bit skin, its fullbright mask and a full-colour replacement of the same size (a replacement of
  another size, normal maps and MF_HOLEY skins untouched). No texel any triangle covers at its middle changes, so the
  art inside the islands is as painted. Toggling the cvar re-uploads the models' skins. `vr_skin_edges [model [x y]]`
  lists the models, their edge texels and the uploads filled.
- **Checks.** The box: 43 dark pixels -> 0 (and 0 with plain `GL_NEAREST`); `vr_skin_edge_fill 0` brings them back (44).
  Close-ups (before | after): `scratch/slivers_before_after.png`, full eyeshots `scratch/slivers_before_full.png`,
  `scratch/slivers_after_full.png`.
- **Test it in VR:** the shotgun (and the other guns) looked at closely along their tops with his nearest texture mode:
  no black slivers at triangle edges; nothing inside the painted parts looks different.

## Lightning and plasma gun sights: clean topology (branch `agent/regfix`, 2026-11-08)

His note, with Blender screenshots (kit `scratch/shotsight_mockup/light_topo_top.webp`, `light_front_post.png`): the
top of v_light round the new rear housing a mess of slivers and overlapping faces, z-fighting in the game; the front
post's glowing cube misaligned and stuck into the post; both sights' tops to taper a little, as the shotgun's.

- **What was wrong.** The original model's receiver top (x -2.72..4.79, z 10.8636) is two big triangles with the old
  rear hump's faces lying on it (three of them facing down in the same plane, two degenerate pairs duplicated,
  T-junctions); the last round sank the old posts into the new housing (the slivers fanning from them) and stood the
  housing in it. The barrel top's diamond under the front sight (z 9.8352) the same: a fan over a fan, two down-facing
  faces in its plane, the old lopsided stem with a cap inside its apex, the little cube on top. `scratch/lt/coplanar.py`
  (pairs of triangles in one plane that overlap, frame 0 as the engine decodes it): 41 such pairs round the rear sight.
- **Rebuilt** (`scratch/lt/bl_lighttopo.py`, both .blends; `S0` 21 v_light, 143 v_plasma): every face of the hump, the
  old posts, the sunk housing, the top's overlapping faces, the stem, its caps and the cube deleted.
  - Rear housing: one profile extruded along x, standing on the receiver top, its back flush with the receiver's rear
    face (x -2.7194..-1.6952, 6 steps long as before): a base the receiver's full width, 3 z steps tall; over it a tower
    16 y steps wide at the shoulders whose outer sides taper 2 steps in to the ears' tops (19 deg); a square U notch 6
    steps wide and 4 deep (0.44 x 0.274, as before) on y -0.0982; ears 3 steps wide at the top (0.22), their tops at
    11.4806 as before. Back and front faces: each ear two triangles, both with a top corner (the sight reader's iron
    top); the rest as the triangulation with the largest smallest angle (back 13.1 deg, front 6.2: its bottom edge is
    one edge with the top's); the rear face's top edge split under the shoulders to match (4 triangles, 8.4 deg).
  - Receiver top: the quad from the housing's front to the receiver's front edge, 2 triangles (23 deg); each side slope
    re-cut at the housing's front corner (3 triangles, 18.5 deg). No vertex lies on an edge it doesn't end (no
    T-junctions): the housing's bottom edges are the top's and the rear face's edges.
  - Front post: one solid, a block 6 steps wide (0.44) from the barrel top to 4 steps under its top, its front ramped (4
    x steps deep at the foot, 2 at the waist); over it the top, tapered to 4 steps (0.293, the cube's width) in 4 z steps
    (14 deg); back face x 31.0765, top 11.3435 as before. It stands in a rectangular hole cut in the diamond: 12
    triangles round it (two points on the barrel's line behind and ahead of the foot keep them at 13.5 deg or more).
  - UVs: the top, the slopes, the rear face and the diamond take their old maps (sampled at the new corners); the
    housing's and the post's other faces the metal patch.
- **Paint** (`scratch/lt/paint_lighttopo.py`), new patches in the same free skin, 3 texels a grid step: the ears' back
  faces (13 x 13 at s S0+1, t 94; mirrored for the right ear, so both read outwards from the notch): a 6-wide glowing
  insert (234 rim, 235, 236 core) centred on the ear's top, its top 1 row (0.023) under the ear's top, in a recess (a
  dark line over it and on its inner side, a lit outer side, a lit lip under it); the post's top back face (19 x 13 at
  S0+15): a 7-wide insert centred on the post, its top 2 rows (0.046) under the post's top, a lit top edge; metal 9 x 9
  at S0+35. These guns have no relief source (their normal maps are baked from the model): the recess is paint only.
- **Sight line:** rear -2.7194 -0.0982 11.4806, front 31.0765 -0.0987 11.3435: -0.232 deg up, -0.001 left (was -0.235,
  0.000): the rear point is the housing's back face, now 0.34 further back. Flat ADS `flatsightcheck`: rear off 0.0000,
  front off 0.0002, far off 0.067 (0.0009 deg). `sight_picture.py` updated.
- **Counts:** 948 -> 877 triangles (both guns; the original model 865). Barrel masks 877: the diamond's 12 new
  triangles marked, as its 8 old ones were (`scratch/lt/maskfix.py`: the export marks by the shipped file's corners, and
  the diamond's are new), the post not, as the stem wasn't. Overlapping coplanar pairs round both sights 41 -> 0 (63
  remain elsewhere in the original model, untouched). Holes (check_mdl_holes.py): open loops 6 (0 see-through) as
  before, cracks 4 -> 2 (none in the sights), pieces 18 -> 16.
- **Pictures** (worktree `scratch/lt/`): `top_before_after.png` (his angle), `front_before_after.png`,
  `light_ads_crop.png` and `light_ads_zoom.png` (flat ADS), `zf_new_ab.png` (two eyeshots looking down at the rear
  sight, the head moved 3 mm between them: the top slides, no stripes).
- **Test it in VR:** look along the lightning gun's top and round the rear housing while moving the head: no shimmer;
  the ears and the post a little narrower at the top; in the ironsight the front insert centred in the notch between
  the rear inserts, the irons level.
## Enforcer's elbow leather jumping (branch `agent/blendmd5`, worktree blendmodels, 2026-11-08)

His note: "The enforcer's right elbow leather connector has a similar issue to the knight one you fixed". Files:
`vr_agarmor_bridge.cpp` (evalBridge, the checks, bridgeTrace), `vr_agarmor.cpp` (benchPose, headcam view 5).

- **Reproduced** live with `vr_agarmor_bridge_trace` (new, below) on three armed enforcers in yellow attacking
  (scene `agarmor_enforcer_elbow`), and offline through every frame and a degree-at-a-time sweep of each joint.
  Two causes, both general (any joint, any humanoid):
  - **The tangents' size switched where it should have eased.** Each end's tangent was the circle's arc's below a
    tenth of the chord of joint reach and the "round" one (three times the reach) above 0.15: at the switch they differ
    by most of a chord, so a joint reach crossing it (the enforcer's left elbow at frame 104, the grunt's left elbow at
    116, the knight's right knee) changed a tangent by up to two units in a step and the middle rings turned 12-21 degrees
    while the rows turned 3-4. And the knight fix's easing (tangents cut towards half a chord as the curve turned back)
    is itself steep: on the knight's sword arm the tangents went 3 -> 14 units within a frame, the rings turning 36
    degrees against the rows' 3.
  - **The crease's push let go over a narrow bend.** The enforcer's inner elbow has a lump of skin 3.4 units outside
    the leather; it was pushed out in full below 26 degrees of bend and let go by 52, and the push (corners kept at the
    most any sample asks) dropped 2 units in about 8 degrees: the leather moved 8 times as far as its rows (sweep 0.372
    against 0.048 a degree), pumping in and out as his arms swung through 27-48 degrees (live: 2.07 against 0.39 in a
    frame, 44 frames in 25 seconds over twice the rows').
- **Fix.** Each tangent is now the smooth larger of the arc's and the round one (`bridgeSoftMax` 0.2 of the chord: they
  meet with no step), then both are scaled down together as far as needed for the curve's middle leg (its Bezier's two
  inner points apart) to run forwards along the middle ring's direction (the rows' directions halved;
  `bridgeForwardLeast` 0.05 of the chord): the chord's reach that way over the inner points', smooth in the pose and
  never divided by nothing short of straight back. The old back-easing stays as a net (no sweep turns back now). The
  crease's push fades over 26 to 90 degrees of bend (`bridgeCreaseFrom` 0.3, `bridgeCreaseBend` 0.75 of 86 degrees).
  Shapes change little: the tangents the same away from a reach near a tenth of the chord or a curve that turned back;
  the enforcer's inner-elbow lump held longer (to ~60 degrees) and let go more slowly.
- **Checks, tightened** (`vr_agarmor_bridge_check`): through the frames a vertex may move 2x its rows plus 0.25 (was 3x
  plus 0.5); new, no middle ring may turn more than 2x its rows plus the bend's change plus 2 degrees in a step; new,
  each joint swept a degree at a time (rest pose, `vr_agarmor_bend`'s), no vertex moving more than 2.5x its rows plus
  0.1. `developer 1` prints each joint's worst step and its push and crease; `developer 2` every sweep step and every
  failing frame step (tangents, fit, back, push).
  - Before: 78 passed, 10 failed: enforcer left elbow 2.25 against 0.72 through the frames and 0.372 against 0.048 in
    the sweep; knight right elbow 2.06 / 0.57 and turn 35.9 / 3.1, its sweep 0.239; knight right knee 1.12 / 0.39 and
    turn 8.3; grunt left elbow turn 20.8 / 3.0, right 4.4 / 0.9; enforcer right knee turn 5.0 / 0.8.
  - After: 88 passed, 0 failed (every joint of grunt, enforcer, knight, death knight; enforcer left elbow sweep 0.173,
    its right 0.111; knight right 0.107).
  - Live, the same scene (the AI differs run to run): frames over twice the rows' + 0.25 on the left elbows 44 -> 5,
    worst excess 1.29 -> 0.53 (an 8-degree-a-frame swing through 63-87 degrees, the lump letting go 1.2 units).
- **Debug aids** (Debug > the armour rows by Check the Elbow and Knee Leather): `vr_agarmor_bridge_trace` (mask: 1
  left elbow, 2 right, 4 left knee, 8 right; a console line a frame per armoured body: its furthest vertex move against
  its rows', its rings' turn against theirs, the curve's numbers); `vr_agarmor_pose_frame` (-1 off: every armoured
  body held in its animation at that frame, fractions lerped as drawn: step through a check's worst step);
  `vr_agarmor_headcam` view 5 (its left side).
- **Shots** (low-impact run, low-res): `scratch/enfelbow_before_after.png`, the enforcer at frame 104 -> 104.125 (the
  left elbow's worst step), rows: before left side, after left side, before right side, after right side.
- **Open:** a lump this big still changes visibly when an arm swings 8 degrees a frame through 60-80 degrees of bend;
  the push's max-over-samples makes the let-go happen over about 20 degrees whatever the fade's width.
- **Test in VR:** scene `agarmor_enforcer_elbow`: each enforcer's elbows (both sides; his "right" may be the enforcer's
  left, `bracer_l`, the one that jumped) while it stands, aims, shoots, walks and swings: the leather stays round the
  elbow, no ring spinning, the inner-elbow lump easing rather than pumping. Grunts' and knights' elbows and knees: no
  new lumps or skin through the leather on the inside of a bend.

## Armour look: fewer, bigger plates (branch `agent/regfix`, 2026-11-08)

His note: the armoured knight "a bit too plain/boring/repetitive"; ChatGPT's mockup (few big plates, a recessed chest panel, a
rune on a pauldron, vented bracers and shins, knee cops, gold-brown trim, a dark layer between plates). He approved the low-risk
steps 1-3 (the plate system kept); step 4 (hero plates: a row's plates merged) after he sees this.

- **Fewer, bigger plates** (`vr_agarmor.cpp`): rows' wanted heights 2.5 / 2.9 -> torso 3.4, arms 4.2, legs 3.6 (`plateHighLeg`,
  new), most rows 6 / 5 / 3 -> 5 / 4 / 2 (torso, legs, arms). Rows now: grunt cuirass 6 (was 7), upper arm 3 (armour.txt, see
  below), forearm 2, thigh 2 (3), shin 2 (4); enforcer 6/2/2/4/3 (7/3/3/5/3); knight 6/2/2/5/2 (7/3/3/5/3); death knight
  6/2/2/3/2 (7/2/2/4/3). Triangles 1960-2296 a body (2488-2968).
- **armour.txt** (shipped defaults): grunt `shin_l rows "4"` dropped (both shins the generator's); grunt `pauldron rows "3"`
  (at 2 rows his left elbow's leather ring spun 14 degrees in a frame: `vr_agarmor_bridge_check` 87/1; the bridge code is
  another worker's); knight `thigh rows "5"` (at 3 or 4 rows a thigh row stood 11.7-12 units wider than his skin:
  `vr_agarmor_look_check`'s width limit 7.5); `kind "progs/vrbody_brawny.mdl"` `cuirass rows "7"` (the brawny build's chest
  row 5.78 units wide at 6 rows, limit 5.5; the other player builds take the new rows).
- **Details by place** (`plateLook`, `vr_agarmor_details 1`): the two plates either side of the cuirass's seam in its chest row
  get a recessed panel tile, flipped so each recess sits on the seam's side and the two make one panel (variants: plain, a slot
  across, two rivets); the front plates below it down to the waist row a recessed ribbed tile; the forearm's last row (the
  cuff) and the shin's last row vents on their three front plates; a cop tile on the back plate of the forearm's first row (the
  elbow) and the front plate of the shin's first row (the knee), its step 1.5 times a plate's (`copRaise`); the emblem on the
  upper arm's top-row plate most to the outside (`vr_agarmor_emblem`: 1 right arm (default), 2 left, 3 both, 0 none). The
  knight's shoulder dome keeps a plain tile (its disc's UV now within the tile's face). Shells made again when either cvar
  changes (the relink key). Both in Debug > Tools by the leather rows (Armour Plate Details, Armour Emblem).
- **The skins** (`make_agarmor.py`, regenerated `vragarmor.mdl` and its relief): the 30 tiles are now 20 plain, 3 panel, 3
  vent, 2 rib, the emblem, the cop (TILE_* = vr_agarmor.cpp tile*). Every tile: brighter faces (shade 1.06 + 0.1 v), a
  near-black under-layer on its top 15% (the gap under the row above: rows read as separate pieces), a darker trim (the paint
  x0.55 and towards gold-brown: TRIM_TINT green 0.25, yellow 0.45, red 0.3; TRIM_SHARE 0.85, TRIM_REACH 1.15 bevels) round
  its sides and lower edge, rivets at its four corners only, less bare steel on the edges at wear 0 (0.8 -> 0.6). The emblem:
  our own rune (a ring, a stem, two raised arms; not id's logo), dark red on green and yellow (its texels quantised with the
  palette's reds, EMBLEM_REDS), near black on red, scuffed. Vents' and grooves' insides are the tier's paint darkened (its
  tiles stay one hue).
- **Fixed on the way:** the engine's `Mod_FloodFillSkin` flooded the region of the atlas's top-left texel's colour with black
  (it took the under-layer along the atlas's top); the top-left texel is now black, which it leaves alone.
- **Checks:** `vr_agarmor_look_check`'s one-hue-per-tile check skips tile 28 (the emblem's red is on purpose).

Tests (low-impact runs): `vr_agarmor_check` 20/0, `vr_agarmor_bridge_check` 88/0, `vr_agarmor_test 1` 26/0, `vr_armour_test`
30/0. `vr_agarmor_look_check` 89/25: the 25 failures were there before (coverage on every player build and the humanoids,
the enforcer's and knight's bent coverage now 56% and 59% against 62% and 65%, both failing before); none new; the death
knight's width and the knight's taper now pass, and the tiers' hues pass (green 13 degrees across its tiles, limit 14).
Renders: `scratch/al_k_pair.png` (knights, before over after), `al_ge_pair.png` (grunt and enforcer), `al_tiers_worn.png`
(green, yellow, red; wear 2), `al_k_front_Rc.png`, `al_k_chest.png`, `al_k_side_c.png`, `skin_b.png` (the atlas: yellow,
green, red, yellow at wear 2).

Not done: trim strength and rivet style aren't cvars (baked in the 8-bit skins; TRIM_SHARE, TRIM_REACH in the generator).
Step 4 (hero plates): merge the chest row's front plates into one wider plate (and the pauldron's top row into two) at grid
time: a plate across 2-4 columns is a quad over a curved row, so its corners need raising (fillRadii's chord rule over the
wider span) and its tile one wide texture (new atlas tiles at 2:1); the hits, debris and look checks count plates per column
(plateVerts, plateNear), so those need a merged plate's columns. A day's work with its tests; bulkier chests are the risk.
