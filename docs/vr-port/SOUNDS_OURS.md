# Sounds made or found by us

Every sound in `quakevr/sound/` that is not an id Software / expansion original: synthesised by a script in
`Misc/quakevr/`, derived from a recording, or downloaded (all CC0; details and cut times in CREDITS.md). Made for
Vittorio to audit and suggest better replacements. The sources of the downloaded ones are not in the repository (each
script's header has the link); rerun a script to regenerate its files. All synthesised files are 22050 Hz mono 16-bit;
the cut recordings are 22050 Hz, or 44100 Hz where the clicks are bright (autopump, reload_shell_in, reload_ssg).

Columns: files (variants collapsed as `name1..n`), length (min-max seconds, sample rate if not 22050), how made, what
it is for, where used (`file:function`; QC `world.qc` precaches, engine `.cpp`; cvar if one controls it). Usage was
found by grepping `QC/` and `Quake/` for each path, and for the engine's material tables (`vr_physsound.cpp`), so a
sound played by a constructed name may list only its table.


## Weapons

| Files | Length | How made | For | Used (file:function) | Cvar |
|---|---|---|---|---|---|
| `vr/widowmaker_shot.wav` | 0.62 s | synthesised: `make_adweapon_sounds.py` (sines + noise) | Arcane Dimensions Widowmaker Gatling shot (auto shotgun punch, bolt clacks) | vr_ad_bridge.qc:AD_WorldSpawn; vr_ad_bridge.qc:AD_FireWidowmaker |  |
| `vr/xbow_bolt_in.wav` | 0.45 s | synthesised: `make_adweapon_sounds.py` | AD crossbow's next bolt morphing into its groove | vr_adcrossbow.cpp:file scope | `vr_adgun_crossbow_bolt_sound` |
| `vr/burst1.wav`, `vr/burst2.wav`, `vr/burst3.wav` | 0.50 s | downloaded: OpenGameArt, The Free Firearm Sound Library (Jaszczak, Nelson, Heras, Nanney), CC0, `AK-47/C_31P.wav`, cut by `make_burst_sounds.py` (burst3 = burst2 5% slower) | the grunts' burst rifle rounds | vr_enemyguns.qc:VR_EnemyGuns_Precache; vr_enemyguns.qc:VR_BurstSound |  |
| `vr/headshot.wav` | 0.42 s | synthesised: `Misc/quakevr/make_sounds.py` | headshot confirmation tick | weapons.qc:PositionalHeadshotFeedback; world.qc:worldspawn | `vr_headshot_sound` |
| `vr/shell_tink1.wav`, `vr/shell_tink2.wav`, `vr/shell_tink3.wav` | 0.26 s | synthesised: `Misc/quakevr/make_sounds.py` | a spent shotgun shell landing (brass ring, plastic tock) | vr_reload.qc:VR_Reload_Precache; vr_reload.qc:VR_Reload_Impact; vr_shells.cpp:file scope |  |
| `vr/shell_plip1.wav`, `vr/shell_plip2.wav`, `vr/shell_plip3.wav` | 0.20 s | derived: `Misc/quakevr/make_sounds.py` pitches up the recorded plip1/plip3/plip4 (Freesound, see Water) | a spent shell dropping into water | vr_shells.cpp:file scope |  |
| `vr/autopump_back.wav`, `vr/autopump_home.wav` | 0.10-0.14 s @44k | downloaded: OpenGameArt, zer0_sol, Shotgun Reload Sound effects, CC0, `Rack.mp3`, cut by `make_autopump_sounds.py` | shotgun auto-pump's two clacks | vr_autopump.cpp:file scope | `vr_autopump_sound` |
| `vr/blade_hot.wav`, `vr/blade_quench.wav`, `vr/blade_sizzle.wav` | 0.50-1.40 s | synthesised: `make_hotblade_sounds.py` (noise bursts, inharmonic partials) | incandescent blades: heating in a flame, glowing red-hot, quenched in water | vr_hotblade.qc:VR_HotBlade_HandTick; weapons.qc:W_Precache; vr_hotblade.qc:VR_HotBlade_Lying |  |
| `vr/magholster_draw.wav`, `vr/magholster_zip.wav` | 0.30-0.42 s | synthesised: `make_magholster_sounds.py` (numpy) | magnetic holsters: weapon flying home / drawn on its chain | vr_magholster.qc:VR_MagHolster_DrawSound; weapons.qc:W_Precache; vr_magholster.qc:VR_MagHolster_Launch | `vr_magholster_zip_volume`, `vr_magholster_draw_volume` |
| `vr/flashlight_attach.wav` ... `vr/flashlight_on.wav` (6) | 0.09-0.16 s | synthesised: `make_flashlight.py` (switch click and clamp latch) | weapon-mounted flashlight: switch, clamp on/off a gun, flipped in hand, grabbed | vr_magholster.qc:VR_MagHolster_Press; weapons.qc:W_Precache; vr_flashlight.cpp:clipOn; vr_flashlight.cpp:clipOnHead; vr_flashlight.cpp:clipOffHead; vr_flashlight.cpp:clipOff; vr_flashlight.cpp:flip (+3 more) |  |
| `vr/saw_cut.wav` ... `vr/saw_start.wav` (7) | 0.80-6.45 s | downloaded: BigSoundBank, Joseph Sardin, CC0, 0982 Chainsaw (Starting) and 0707 Chainsaw #2, cut by `make_chainsaw_sounds.py` (loops crossfaded, cue points) | the ogres' chainsaw: pull, start, idle, run, cut, stall | vr_chainsaw.qc:VR_Chainsaw_Precache; vr_chainsaw.qc:VR_Chainsaw_Frame; vr_chainsaw.qc:VR_Saw_PropRuns; vr_chainsaw.cpp:file scope; vr_chainsaw.qc:VR_Saw_PropThink; vr_chainsaw.qc:VR_Chainsaw_StartAll |  |
| `vr/grapple_reel.wav`, `vr/grapple_taut.wav`, `vr/grapple_unreel.wav` | 0.25-0.35 s | synthesised: `Misc/quakevr/make_sounds.py` | grappling hook reel winding, rope snapping taut, unreel | vr_grapple.qc:VR_Grapple_ReelFeel; weapons.qc:W_Precache; vr_grapple.qc:VR_Grapple_Taut; vr_grapple.qc:VR_Grapple_HoldTwo; vr_grapple.qc:VR_Grapple_Unwind |  |
| `vr/torch_hit.wav` ... `vr/torch_pull.wav` (4) | 0.40-0.80 s | synthesised: `Misc/quakevr/make_sounds.py` | wall torches (pulled from holder, going out, catching fire, burning blow); also the toolgun | vr_toolgun.qc:VR_Toolgun_Precache; vr_walltorch.qc:VR_WallTorch_KnockOff; vr_walltorch.qc:VR_WallTorch_Melee; vr_walltorch.qc:VR_WallTorch_Setup; vr_ad_bridge.qc:AD_WorldSpawn; vr_burning.qc:VR_Burn_Ignite; vr_walltorch.qc:VR_WallTorch_Relight (+2 more) |  |
| `vr/nail_sizzle1.wav`, `vr/nail_sizzle2.wav` | 0.13 s | synthesised: `Misc/quakevr/make_sounds.py` | a nail turning into a lava nail in a torch flame | vr_burning.qc:VR_Burn_NailToLava; vr_walltorch.qc:VR_WallTorch_Setup | `vr_burn_nail_sound` |
| `vr/necksnap.wav` | 0.55 s | synthesised: `make_necksnap_sound.py` | neck snap bone crack | vr_foegrab_throw.qc:VR_FoeGrab_HeadLeft; vr_foegrab_throw.qc:VR_FoeGrab_Crack; world.qc:worldspawn |  |
| `vr/crowbar_hit1.wav` ... `vr/crowbar_wall2.wav` (5) | 0.38-0.95 s | synthesised: `Misc/quakevr/make_sounds.py` | the crowbar landing on a body / on a wall | vr_crowbar.qc:VR_Crowbar_Precache; vr_crowbar.qc:W_CrowbarMelee |  |
| `vr/reload_ssg_close.wav`, `vr/reload_ssg_eject.wav`, `vr/reload_ssg_open.wav` | 0.16-0.42 s @22k/44k | downloaded: OpenGameArt, zer0_sol, Shotgun Reload Sound effects, CC0 (`Rack.mp3`, `Shell in Chamber.mp3`), cut by `make_reload_ssg_sounds.py`; reload_ssg_eject: no generator or record found | immersive reloading: super shotgun break-open, close, eject | vr_reload.qc:VR_Reload_Precache; vr_reload.qc:VR_Reload_SsgClose; vr_reload.qc:VR_Reload_SsgBreak |  |
| `vr/reload_shell_in.wav`, `vr/reload_shell_in_2.wav`, `vr/reload_shell_in_3.wav` | 0.18-0.23 s @44k | downloaded: OpenGameArt, zer0_sol, Shotgun Reload Sound effects, CC0 (`First Shell.mp3`, `Subsequent Shells.mp3`, `5 Shell Reload.mp3`), cut by `make_reload_shell_sounds.py` | immersive reloading: shell pushed into the shotgun | vr_reload.qc:VR_Reload_Precache; vr_reload.qc:VR_Reload_ShellInSound |  |
| `vr/reload_blocked.wav` ... `vr/reload_pouch.wav` (6) | 0.09-0.30 s | synthesised: `Misc/quakevr/make_sounds.py` | immersive reloading: shell from/to pouch, pouch empty, full-gun gate click, tube full, magazine seated / released | vr_reload.qc:VR_Reload_Precache; vr_reload.qc:VR_Reload_LoadInto; vr_reload.qc:VR_Reload_HeldFrame; vr_reload.qc:VR_Reload_PropRound; vr_reload.qc:VR_Reload_Take; vr_reload.qc:VR_Reload_PouchFrame; vr_reload.qc:VR_Reload_IntoProp (+4 more) |  |
| `vr/pack_full.wav` | 0.30 s | synthesised: `Misc/quakevr/make_sounds.py` | a box put away into a full pack | vr_carry.qc:VR_Carry_FullCue; vr_reload.qc:VR_Reload_Precache |  |
| `vr/grenade_fuse.wav` ... `vr/grenade_tick.wav` (4) | 0.08-0.70 s | synthesised: `Misc/quakevr/make_sounds.py` | hand grenades: lit fuse, ticks, from pouch, pin pulled | vr_grenade.qc:VR_Grenade_Caught; vr_grenade.qc:VR_HandGrenade_Arm; world.qc:worldspawn; vr_grenade.qc:VR_HandGrenade_Take; vr_grenade.qc:VR_HandGrenade_LetGo; vr_grenade.qc:VR_Grenade_Cue |  |

## Melee

| Files | Length | How made | For | Used (file:function) | Cvar |
|---|---|---|---|---|---|
| `vr/bash.wav` ... `vr/shove.wav` (4) | 0.55-0.80 s | synthesised: `Misc/quakevr/make_sounds.py` | shove, weapon bash, parry-bash, parry ring | vr_melee.qc:VR_Bash; world.qc:worldspawn; combat.qc:VR_Parry; vr_enemyshove.qc:VR_EnemyShove_Hit | `vr_bash_sound` |
| `vr/counter.wav`, `vr/counter_open.wav` | 0.60-0.70 s | synthesised: `Misc/quakevr/make_sounds.py` | counter-attack window opening / counter landing | vr_melee.qc:VR_Counter_Open; vr_melee.qc:VR_Counter_Use; world.qc:worldspawn | `vr_counter_sound` |
| `vr/pommel1.wav`, `vr/pommel2.wav`, `vr/pommel3.wav` | 0.30 s | synthesised: `Misc/quakevr/make_sounds.py` | pommel, hilt or gun-butt knock | vr_melee.qc:VR_Melee_KindName; vr_melee.qc:VR_Melee_HitSound; world.qc:worldspawn; vr_carry.qc:VR_Carry_Start |  |
| `vr/slap1.wav`, `vr/slap2.wav`, `vr/slap3.wav` | 0.28 s | synthesised: `Misc/quakevr/make_sounds.py` | open-hand slap landing | vr_melee.qc:VR_Melee_HitSound; world.qc:worldspawn | `vr_melee_slap` |
| `vr/slap_whoosh1.wav`, `vr/slap_whoosh2.wav` | 0.26 s | synthesised: `Misc/quakevr/make_sounds.py` | open-hand slap swing swish | vr_melee.qc:VR_Swish_Sample; vr_melee.qc:VR_Swish_Frame; world.qc:worldspawn |  |
| `vr/dummy_windup.wav` | 0.45 s | synthesised: `Misc/quakevr/make_sounds.py` | training dummy winding up a blow | vr_dummy.qc:VR_DamageNumber; vr_dummy.qc:VR_Dummy_WindUp; vr_dummy.qc:vr_dummy | `vr_dummy_attacks` |

## Gadgets and UI

| Files | Length | How made | For | Used (file:function) | Cvar |
|---|---|---|---|---|---|
| `vr/gadget_click_off.wav`, `vr/gadget_click_on.wav` | 0.09 s | synthesised: `make_gadget_sounds.py` (via `make_flashlight.py`'s click, higher) | wrist gadget's side button (gear lights) | `vr_cvars.inc` cvar defaults; played by vr_gearlights.cpp | `vr_gear_lights_sound_on`, `vr_gear_lights_sound_off` |
| `vr/gadget_tap.wav`, `vr/gadget_tap_on.wav` | 0.04-0.11 s | synthesised: `Misc/quakevr/make_sounds.py` | wrist gadget screen tap registering; bullet-time tap | vr_gadget.cpp:tapFeedback | `vr_bullettime_tap_sound` |
| `vr/sync_beep.wav` | 0.25 s | synthesised: `Misc/quakevr/make_sounds.py` (1 kHz tone) | highlight log sync mark | vr_highlights.cpp:syncMark |  |

## Physics and debris

| Files | Length | How made | For | Used (file:function) | Cvar |
|---|---|---|---|---|---|
| `vr/phys/flesh_h1.wav` ... `vr/phys/wood_m4.wav` (40) | 0.08-0.47 s | downloaded: Kenney Impact Sounds 1.0 (kenney.nl/assets/impact-sounds, CC0), cut/levelled by `make_physics_sounds.py` | prop impacts by material (wood/metal/soft/flesh) and weight (l/m/h) | vr_physsound.cpp:file scope; vr_crates.qc:VR_Hit_Armoured; vr_crates.qc:VR_Blunt_HitLayer; world.qc:worldspawn; vr_crates.qc:VR_Crates_Precache; vr_crates.qc:VR_Metal_Sound; vr_crates.qc:VR_Crate_Pain |  |
| `vr/phys/scrape_metal1.wav` ... `vr/phys/scrape_wood4.wav` (12) | 0.50 s | downloaded: Freesound CC0 previews: 481861 craigsmith (wood), 690010 matth3wc04 + 844755 MeanRaccoon (metal), 545562 rsellick (stone); windows picked by `make_physics_sounds.py` | props sliding with friction (0.5 s grains crossfaded) | vr_physsound.cpp:file scope |  |
| `vr/phys/scrape_soft1.wav` ... `vr/phys/scrape_soft4.wav` (4) | 0.50 s | derived: the wood scrape grains low-passed at 700 Hz, quieter (`make_physics_sounds.py`) | a sack or body dragged | vr_physsound.cpp:file scope |  |
| `vr/phys/grab_metal1.wav` ... `vr/phys/grab_wood3.wav` (9) | 0.14-0.25 s | downloaded: Kenney Impact Sounds (impactSoft_medium + impactWood_light / impactMetal_light / footstep_concrete), CC0, mixed by `make_physics_sounds.py` | a climbing hand taking a hold | vr_carry.qc:VR_Carry_GrabSound; weapons.qc:wpnthrow_touch; world.qc:worldspawn; vr_physsound.cpp:file scope; vr_twohand.cpp:applyHand; vr_twohand.cpp:gripFeedback |  |
| `vr/brick1.wav` ... `vr/rock3.wav` (6) | 0.28-0.32 s | synthesised: `Misc/quakevr/make_sounds.py` | rock thud / brick clack (debris landing, thrown, struck) | vr_debris.qc:VR_Debris_Precache; vr_debris.qc:VR_Debris_Sound; vr_physsound.cpp:file scope |  |
| `vr/crate_break1.wav`, `vr/crate_break2.wav`, `vr/crate_break3.wav` | 0.70 s | derived: `make_crate_sounds.py` (Kenney wood knocks pitched down + synthesised splinters) | wooden crate breaking | vr_crates.qc:VR_Crates_Precache; vr_crates.qc:VR_Crate_Break; vr_tutorial.qc:func_vr_target_die |  |
| `vr/crate_dust1.wav`, `vr/crate_dust2.wav` | 0.45 s | derived: `make_crate_sounds.py` (Kenney knock pitched up + synthesised dust hiss) | crate piece bursting into dust | vr_crates.qc:VR_Crates_Precache; vr_crates.qc:VR_CratePiece_Burst; vr_crates.qc:VR_CratePiece_BurnAway |  |

## Gore

| Files | Length | How made | For | Used (file:function) | Cvar |
|---|---|---|---|---|---|
| `vr/squish1.wav` ... `vr/squish_s4.wav` (8) | 0.26-0.42 s | synthesised: `Misc/quakevr/make_sounds.py` | gib landing / sticking (squish_s = small gibs) | vr_smallgibs.qc:VR_SmallGib_Precache; vr_smallgibs.qc:VR_Gib_SquishSound; vr_physsound.cpp:file scope |  |

## Water

| Files | Length | How made | For | Used (file:function) | Cvar |
|---|---|---|---|---|---|
| `vr/splash_big1.wav`, `vr/splash_big2.wav`, `vr/splash_big3.wav` | 1.79-1.86 s | downloaded: Freesound, CC0, cut by hand (times in CREDITS.md): 585744 Nox_Sound | big splash | world.qc:worldspawn; vr_physics.cpp:VR_ClientRoomscaleMove |  |
| `vr/splash_big4.wav` | 1.82 s | downloaded: Freesound, CC0, cut by hand (times in CREDITS.md): 434978 felix.blume | big splash | world.qc:worldspawn; vr_physics.cpp:VR_ClientRoomscaleMove |  |
| `vr/splash_small1.wav`, `vr/splash_small3.wav` | 0.77-0.83 s | downloaded: Freesound, CC0, cut by hand (times in CREDITS.md): 390391 N-RAZM | small splash | world.qc:worldspawn; vr_physics.cpp:VR_ClientRoomscaleMove |  |
| `vr/splash_small2.wav` | 1.00 s | downloaded: Freesound, CC0, cut by hand (times in CREDITS.md): 212143 qubodup (from 195877 blaukreuz) | small splash | world.qc:worldspawn; vr_physics.cpp:VR_ClientRoomscaleMove |  |
| `vr/splash_small4.wav` | 1.05 s | downloaded: Freesound, CC0, cut by hand (times in CREDITS.md): 390006 morganveilleux | small splash | world.qc:worldspawn; vr_physics.cpp:VR_ClientRoomscaleMove |  |
| `vr/splash_out1.wav` | 1.12 s | downloaded: Freesound, CC0, cut by hand (times in CREDITS.md): 389987 morganveilleux | hand pulled out of water fast | world.qc:worldspawn; vr_physics.cpp:VR_ClientRoomscaleMove |  |
| `vr/plip1.wav`, `vr/plip2.wav` | 0.33 s | downloaded: Freesound, CC0, cut by hand (times in CREDITS.md): 50623 vibe_crc | shot / nail / grenade into water | world.qc:worldspawn; vr_physics.cpp:VR_ClientRoomscaleMove |  |
| `vr/plip3.wav` | 0.34 s | downloaded: Freesound, CC0, cut by hand (times in CREDITS.md): 420227 14FPanska_Nemec_Petr | into water | world.qc:worldspawn; vr_physics.cpp:VR_ClientRoomscaleMove |  |
| `vr/plip4.wav` | 0.36 s | downloaded: Freesound, CC0, cut by hand (times in CREDITS.md): 272392 danhelbling | into water | world.qc:worldspawn; vr_physics.cpp:VR_ClientRoomscaleMove |  |
| `vr/slosh1.wav` ... `vr/slosh4.wav` (4) | 0.59-0.68 s | downloaded: Freesound, CC0, cut by hand (times in CREDITS.md): 342932 ryansitz | wading | world.qc:worldspawn; vr_physics.cpp:VR_ClientRoomscaleMove |  |
| `vr/stroke1.wav` ... `vr/stroke4.wav` (4) | 0.79-0.93 s | downloaded: Freesound, CC0, cut by hand (times in CREDITS.md): 390006 morganveilleux | swimming strokes | world.qc:worldspawn; vr_physics.cpp:VR_ClientRoomscaleMove |  |
| `vr/swim_hard1.wav` ... `vr/swim_soft3.wav` (6) | 0.62-0.75 s | derived: `Misc/quakevr/make_sounds.py` (`swim_stroke`) from the recorded slosh1..3, slowed/muffled/bubbles | a hand's stroke under water | world.qc:worldspawn; vr_physics.cpp:VR_ClientRoomscaleMove |  |

## Not ours or unknown origin (outside `sound/vr/`)

These came with the initial import of the game folder; no generator, credits entry or later change in git: came with the initial import of `quakevr/` (2026-09-24). Names match id1 / Hipnotic / Rogue sound paths (probably game assets or overrides of them, not ours): verify. None is made by a script; none is credited. Listed for completeness, with their uses, so the ones that are really ours can be marked.

| File | Length | Used (file:function) |
|---|---|---|
| `fisthit.wav` | 0.52 s | client.qc:VR_Headbutt_Hit; combat.qc:VR_Corpse_Struck; combat.qc:VR_Parry; vr_carry.qc:VR_Prop_Flung; vr_carry.qc:VR_Gib_Blow (+11) |
| `forcegrab.wav` | 0.86 s | builtins.qc:physicsblast; vr_walltorch.qc:VR_WallTorch_ForceGrabbable; vr_wpnforcegrab.qc:VR_Forcegrab_IsEligible; vr_wpnforcegrab.qc:VR_Forcegrab_Launch; world.qc:worldspawn (+2) |
| `gunclick.wav` | 0.07 s | vr_chainsaw.qc:VR_Chainsaw_Frame; vr_enemyguns.qc:VR_EnemyGun_Attack; vr_reload.qc:VR_Reload_Precache; weapons.qc:W_CheckNoAmmo; world.qc:worldspawn |
| `hipweap/laserg.wav` | 0.86 s | weapons.qc:HIP_LaunchLaser; weapons.qc:W_Precache |
| `hipweap/laserric.wav` | 0.30 s | weapons.qc:HIP_LaserTouch; weapons.qc:W_Precache |
| `hipweap/mjolhit.wav` | 3.06 s | hip_items.qc:trap_tesla_think; hip_items.qc:trap_tesla_coil; vr_mg3_weapons.qc:VR_MG3_Weapons_Precache; weapons.qc:HIP_SpawnMjolnirBase; weapons.qc:W_Precache |
| `hipweap/mjolslap.wav` | 0.25 s | vr_mg3_weapons.qc:VR_MG3_Weapons_Precache; vr_mg3_weapons.qc:W_SuperAxeMelee; weapons.qc:HIP_SpawnMjolnirBase; weapons.qc:W_Precache |
| `hipweap/mjoltap.wav` | 0.25 s | not referenced by our QC or engine |
| `hipweap/mjoltink.wav` | 0.27 s | vr_mg3_weapons.qc:VR_MG3_Weapons_Precache; vr_mg3_weapons.qc:W_SuperAxeMelee; weapons.qc:HIP_FireMjolnir; weapons.qc:W_Precache |
| `hipweap/mjolwam.wav` | 0.27 s | not referenced by our QC or engine |
| `hipweap/proxbomb.wav` | 0.75 s | weapons.qc:W_Precache; weapons.qc:W_FireProximityGrenade |
| `hipweap/proxwarn.wav` | 0.19 s | weapons.qc:W_Precache; weapons.qc:ProximityBomb |
| `misc/bell.wav` | 2.79 s | not referenced by our QC or engine |
| `misc/clang.wav` | 1.18 s | not referenced by our QC or engine |
| `misc/click.wav` | 0.22 s | vr_menuui.cpp:mockMouse_f |
| `misc/deepthud.wav` | 1.19 s | not referenced by our QC or engine |
| `misc/drawbrig.wav` | 1.78 s | not referenced by our QC or engine |
| `misc/flys.wav` | 7.00 s | player.qc:HeadThink; vr_carry.qc:VR_Gib_Die; world.qc:worldspawn |
| `misc/foot1.wav` | 0.21 s | hip_decoy.qc:decoy_walk1; player.qc:player_run; world.qc:worldspawn |
| `misc/foot2.wav` | 0.15 s | hip_decoy.qc:decoy_walk1; player.qc:player_run; world.qc:worldspawn |
| `misc/foot3.wav` | 0.17 s | hip_decoy.qc:decoy_walk1; player.qc:player_run; world.qc:worldspawn |
| `misc/foot4.wav` | 0.15 s | hip_decoy.qc:decoy_walk1; player.qc:player_run; world.qc:worldspawn |
| `misc/foot5.wav` | 0.27 s | hip_decoy.qc:decoy_walk1; player.qc:player_run; world.qc:worldspawn |
| `misc/foot6.wav` | 0.25 s | hip_decoy.qc:decoy_walk1; player.qc:player_run; world.qc:worldspawn |
| `misc/foot7.wav` | 0.17 s | hip_decoy.qc:decoy_walk1; player.qc:player_run; world.qc:worldspawn |
| `misc/longexpl.wav` | 3.84 s | hip_expl.qc:exploder_fire; hip_expl.qc:func_exploder; hip_expl.qc:func_multi_exploder; hip_mon_arma.qc:armagon_body_explode2; world.qc:worldspawn |
| `misc/quake.wav` | 3.09 s | hip_quake.qc:earthquake_postthink; hip_quake.qc:func_earthquake; rogue_earthq.qc:earthquake_rumble; rogue_earthq.qc:earthquake; rogue_earthq.qc:earthquake_touch (+3) |
| `misc/quakeend.wav` | 0.73 s | hip_quake.qc:earthquake_postthink; hip_quake.qc:func_earthquake; vr_mg_triggers.qc:trigger_screenshake |
| `misc/rockthud.wav` | 0.87 s | not referenced by our QC or engine |
| `misc/shortexp.wav` | 2.49 s | hip_expl.qc:exploder_fire; hip_expl.qc:func_exploder; hip_expl.qc:multi_exploder_fire; hip_expl.qc:func_multi_exploder; world.qc:worldspawn |
| `misc/spike.wav` | 0.31 s | misc.qc:spikeshooter_use; misc.qc:trap_spikeshooter |
| `misc/tesla.wav` | 2.03 s | hip_items.qc:trap_tesla_think; hip_items.qc:trap_tesla_coil |
| `misc/weton.wav` | 1.42 s | hip_items.qc:item_artifact_wetsuit |
| `misc/wetsuit.wav` | 4.46 s | client.qc:doWetsuit; hip_items.qc:item_artifact_wetsuit |
| `pendulum/hit.wav` | 0.54 s | rogue_pendulum.qc:pend_touch; rogue_pendulum.qc:pendulum; vr_grapple.qc:VR_Grapple_ServiceAnchor; weapons.qc:W_Precache |
| `pendulum/swing.wav` | 1.16 s | rogue_pendulum.qc:pend_swing4; rogue_pendulum.qc:pend_swing17; rogue_pendulum.qc:pendulum |
| `reload1.wav` | 0.42 s | weapons.qc:RReloadWeapon; weapons.qc:RUnloadWeapon; world.qc:worldspawn |
| `weapons/chain1.wav` | 0.52 s | vr_grapple.qc:VR_Grapple_Retract; vr_grapple.qc:W_FireGrapple; weapons.qc:W_Precache |
| `weapons/holster0.wav` | 0.54 s | weapons.qc:W_Precache; weapons.qc:VRHandOffWeapon; weapons.qc:VRTakeCarriedWeapon; weapons.qc:VRDrawHolsterWeapon |
| `weapons/holster1.wav` | 0.38 s | vr_magholster.qc:VR_MagHolster_Arrive; weapons.qc:W_Precache; weapons.qc:RPutWeaponInHolster |
