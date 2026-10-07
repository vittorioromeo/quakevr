# Audio review: the VR interactions' sound feedback (2026-10-07)

Every VR interaction and feature (docs/FEATURES.md, `QC/vr_*.qc`, `QC/weapons.qc` / `combat.qc` / `client.qc` /
`items.qc`, `Quake/vr/*.cpp`) checked for the sound it makes today, and whether that is missing, weak, repetitive,
heard from the wrong place, too loud or quiet next to the rest, or without its haptic. Only existing sounds are
proposed for reuse (id's `sound/` in the paks, referenced by name at runtime, and `quakevr/sound`); new recordings are
described for sourcing.

Three small fixes are in (see "Done in this review"); everything else is for Vittorio to pick from. What has been
done since is under "Status".

## Two things that shape every row

- **Only the weapon channels play from the hand.** `vr_audio.cpp` `handOf`: a sound on the player's `CHAN_WEAPON` (main
  hand) or `CHAN_WEAPON2` (off hand) comes from that hand's muzzle; every other channel on the player (`CHAN_AUTO`,
  `CHAN_BODY`, `CHAN_ITEM`, `CHAN_VOICE`) comes from the middle of the head, in both ears alike. Parries, bashes,
  reloads, holstering, the grenade pouch, the chainsaw's pull, a dry click, the carry swish: all of them are hand
  actions heard from the head. They can't simply move to the weapon channel, which would cut off the gun's own sound
  (one sound per entity channel). Row 1 proposes "hand any-free" channels.
- **QuakeC's `sound()` has no pitch.** The only variation available is picking between recordings, and the volume. The
  engine already resamples every voice (the Doppler, `vr_timescale_sound`), so a pitch argument (row 3) would give the
  repetitive one-recording sounds a ±4% jitter without new assets.

Loudness, measured from the files (RMS as a share of full scale): `weapons/pkup.wav` 32%, `fisthit.wav` 32%,
`vr/shove.wav` 34%, `items/inv1.wav` 36%, `reload1.wav` 11%, `weapons/holster0.wav` 5%, `holster1.wav` 4%,
`vr/phys/grab_*` 8–9%, `knight/sword1/2` 20–21%, `misc/power.wav` 13%.

## Ranked

Effort: S an hour or so, QC or one engine function; M a day, engine work across files; A needs a new asset.

| # | Feature | Current sound | Gap | Proposed | Spatialization | Volume / variation | Effort |
|---|---|---|---|---|---|---|---|
| 1 | Hand actions on the player: parry (`combat.qc` VR_Parry), bash/shove/counter (`vr_melee.qc` VR_Bash, VR_Counter_*), reload/unload (`weapons.qc` VRReloadWeapon), draw/holster/hand-off (VRDrawHolsterWeapon, VRWeaponLetGoDone), grenade pouch (`vr_grenade.qc`), chainsaw pull (`vr_chainsaw.qc`), dry click (W_CheckNoAmmo), carry swish | right sounds, `CHAN_AUTO`/`CHAN_BODY`/`CHAN_ITEM` on the player | heard from the middle of the head, not the hand doing it | two "any free" hand channels (for example 8 and 9: main, off): `handOf` maps them to the hand like 1 and 5, and the mixer treats them as channel 0 (never cut off); QC uses them through a `VRGetHandChannel(hand)` | from the hand | unchanged | M (engine `vr_audio.cpp`, `snd_dma.c` channel pick; `SV_StartSound` refuses channels above 7 (`sv_main.c`) though the Fitz message carries them (`SND_LARGEENTITY`); QC call sites) |
| 2 | Ragdolls: corpses falling, thrown, dragged, knocked down off a ledge, piled | none: `vr_box3d.cpp` `soundHits` plays only `Kind::Prop` bodies, ragdoll limbs are not props | bodies land, tumble down stairs and get flung in silence | the impacts of the ragdoll's limbs through `physsound::hit` as `Material::Flesh` (`vr/phys/flesh_h1..4`, `flesh_m1..4`, existing), rate-limited per ragdoll, not per limb (the heaviest limb's hit, at most every `vr_physsound_interval`); drags with `scrape_soft1..4` | at the contact (as props) | by speed and mass, as props; a 70 kg body is "heavy" | M |
| 3 | Pitch jitter for one-recording sounds (`fisthit.wav`, `weapons/pkup.wav`, `weapons/tink1.wav`, holster0/1, `reload1.wav`, `vr/headshot.wav`) | same recording, same pitch every time | the most frequent sounds are the most repetitive | a pitch argument for QC: a `soundpitch` global read by `PF_sound` (or FTE's extended `sound` with pitch), sent in the start-sound message and applied by the voice's resampler | unchanged | ±4% random on the listed sounds | M |
| 4 | Fist, gun-butt, headbutt, held-gib and held-prop blows on flesh (`weapons.qc` W_GunMelee, W_FistMelee, `client.qc` VR_Headbutt_Hit, `vr_carry.qc` VR_Gib_Blow, `combat.qc` VR_Corpse_StrikeFrame) | `fisthit.wav` (one recording) at 1.0 | the commonest melee hit is the same thud every time; a sword on flesh is likewise always `knight/sword2.wav` | now: a quiet random `vr/phys/flesh_m1..4` layered under `fisthit` (0.5) on a body; later: 4 punch recordings and 3 blade-into-flesh cuts | hand (needs row 1 for `CHAN_AUTO`; these are on the gun channels already) | a hard blow louder (they are all 1.0 today) | S (layer) / A |
| 5 | Bullet time / Sandevistan (`vr_bullettime.cpp`, `vr_bullettime_sound_*`) | on `items/inv1.wav`, off `items/inv2.wav` (3 s), denied `misc/menu2.wav`, all 2D | known placeholders: the Ring of Shadows' sounds; the 3 s "off" outlasts short bursts | a slow-down whoosh (pitch falling), a low muffled heartbeat or hum while it lasts (stopped at the end), a speed-up whoosh; Sandevistan its own glitchy start; denied a short muted click | 2D (it is the player's state) | the hum quiet (0.3) | A (the cvars already take the names) |
| 6 | Two-handed grip on a foregrip (`vr_twohand.cpp`) | none, and no haptic | the off hand closes on the gun without any feedback; letting go likewise | `vr/phys/grab_metal1..3` (existing) and a short haptic in the helping hand as the grip engages (`twohand::transition` rising past 0.5); a softer one on release | at the helping hand (client `S_StartSound` at its pose, as the flashlight's) | 0.6 | S–M |
| 7 | Burning monsters, corpses and crates (`vr_burning.qc`) | `vr/torch_light.wav` once as it catches; you: `player/lburn1/2` | a body burning for seconds is silent after the first whoosh | `ambience/fire1.wav` looped on the burning thing (as the wall torch does, `vr_walltorch.qc` VR_WallTorch_Crackle), stopped with `misc/null.wav` when it goes out, gibs or is removed | on the body | 0.4, `ATTN_STATIC` (close only) | S–M (every way a fire ends must stop it) |
| 8 | Picking up a weapon lying in the level, your own dropped or thrown one, and catching one (`weapons.qc` wpnthrow_handtouch_impl) | `weapons/pkup.wav` at 1.0, from the weapon | Quake's pickup chime, the loudest UI-like sound, on every regrip of your own weapon | `pkup` only when the weapon is new to you; a regrip `vr/phys/grab_metal1..3` (existing, via `VR_Carry_GrabSound`'s pattern) | from the weapon (as now) | grab at 1.0 | S |
| 9 | Force grab (`vr_wpnforcegrab.qc`) | lock-on: haptic only; flick: `forcegrab.wav` 0.65 from the item; flight: none; catch: the item's pickup sound and a haptic | lock-on is silent; the item flies to you silently | lock-on: `weapons/tink1.wav` 0.25 from the item on the lock's first frame (VR_Forcegrab_Set with TRUE, edge only); flight: a soft whoosh loop (new) | from the item | — | S / A |
| 10 | Reload and unload (`weapons.qc` VRReloadWeapon, VRUnloadWeapon) | `reload1.wav` for every weapon, both ways, from the head (`CHAN_BODY`) | the same click for a shotgun, the nailgun and the rocket launcher; reaching for a reload with no reserve ammo is silent (VRTryReloadWeapon does nothing) | per weapon: shells pushed in, a drum seated, a rocket slid in (new); unload `weapons/holster0.wav`; no reserve: `gunclick.wav` once as the hand reaches the holster | hand (row 1) | — | S (no-ammo click) / A |
| 11 | Drawing and holstering (VRDrawHolsterWeapon, VRWeaponLetGoDone) | `weapons/holster0/1.wav` at 1.0, `CHAN_BODY` | very quiet (RMS 4–5%, a sixth of the pickup's) and from the head, not the hip or shoulder | +4 dB in the files (or 1.0 is the cap: re-export louder), from the hand (row 1) | hand | — | S (asset gain) |
| 12 | A gun or a hand touching a wall; hands meeting (`vr_held.cpp`, `vr_view.cpp`) | haptic only | the gun bumping a wall is felt, not heard | a speed-scaled light knock, `vr/phys/metal_l1..4` for a gun, `soft_m1..4` for a hand, rate-limited, only above a hand speed (a slow lean on a wall stays silent) | at the contact, client-side | 0.2–0.6 by speed | M |
| 13 | Passing a weapon between hands (VRSwitchWeaponHands), a second hand on a carried prop (VR_Carry_TrySecondHand), boxes, backpacks, armour and keys taken by hand (VR_Carry_Start's rest) | `knight/sword2.wav` (a sword's swish) 0.7 / 0.25 / 0.4 from the head | a swish for a hand closing on something | the material grab (`VR_Carry_GrabSound`, done for crates, rocks, explosive boxes and gibs); a builtin exposing `physsound::materialOf` would cover the boxes (metal) and backpacks (soft) the same way | from the thing | as the grabs | S |
| 14 | Map tips' floating panel (`vr_tips.cpp` show) | none (a hologram tip chimes only when the gadget is out of view) | a panel fades in beside a torch or a button without a sound | `misc/talk.wav` (Quake's message blip) at 0.4 at the tip's subject, as it appears; with a volume cvar | at the subject | once per tip | S |
| 15 | Shock arcs on the living and on you (`vr_shock.qc`, `vr_shock_self_*`) | the bolt's own sound; after it none | the arcs crawling on after the bolt are silent (bodies now crackle: done) | short random zaps (3 new recordings) every 0.3–0.6 s while the arcs last | on the monster / you | quiet, fading with the arcs | A |
| 16 | Relighting a batch, a Map Library download/install finishing | the menu's own clicks | a background job finishing while you play is shown, not heard | `gadget::chime` (`misc/talk.wav`) and a buzz when a batch or an install ends in game | the gadget | once | S |
| 17 | Teleport locomotion (off by default, `vr_teleport.cpp`) | none | arriving is silent | a short soft whoosh (new), or `misc/r_tele4.wav` at 0.2 (existing, but Quake's teleporter is louder and brighter) | 2D | — | S / A |
| 18 | Walking through a slipgate (`triggers.qc` VR_Portal_Crossed) | none, on purpose ("no flash, no sound") | — (a design choice) | if wanted: a faint shimmer as you pass the surface (new) | at the gate | quiet | A |
| 19 | Voice notes (`vr_voicenotes.cpp`, a dev tool) | haptics only | you can't hear that the recording started | `misc/menu1.wav` local, start and end | 2D | 0.5 | S |
| 20 | Blood dripping off you, washing it off | none | minor: the water's own hand sounds cover washing | none needed | — | — | — |

## Status

| Row | State | What was done (ROUND21.md has each one's notes) |
|---|---|---|
| 1 | done (ca888732) | channels 8 and 9 (`CHAN_HAND`, `CHAN_HAND2`, `VRGetHandChannel`): any-free channels from the main and off hand |
| 2 | done (5a18f374) | ragdoll parts and pushable corpses knock as flesh through the physics sounds; the dragging scrape left (row 2b below) |
| 3 | done (0fc1340d) | `sound()`'s pitch argument; `VR_SoundVaried` and `vr_snd_pitch_jitter` (4%) on the frequent sounds |
| 4 | done (layer) | a blunt blow's second layer of what it hit (flesh, armour, wood, a wall) under the thud, varied in pitch (`VR_Blunt_HitLayer`, `vr_snd_hit_layer`); the new punch and cut recordings still to source |
| 6 | done | a metal click from the helping hand and a 30 ms pulse in it as it takes a foregrip, a quieter click as it lets go (`vr_twohand.cpp` gripFeedback, `vr_2h_grip_sound` 0.6) |
| 7 | done | `ambience/fire1.wav` looped on the burning body's channel 7, near only, a three-step fade in the last 1.5 s, stopped however the fire ends; at most `vr_burn_sound_max` (4) at once (`VR_Burn_Crackle`, `vr_burn_sound` 0.6) |

## Well covered (no change proposed)

Flashlight (on, off, grab, flip, attach, detach: from the light itself), hand grenades (pouch, pin, fuse, tick, bounce,
catch), the grappling hook (shot, anchor, taut, reel in and out, unwind), the chainsaw (pulls, start, idle, run, cut,
stall), wall torches (pull, knock off, out, relight, crackle loop), crates (break 3, dust 2, hits by size), rocks and
bricks (3 each), physics props' knocks and scrapes (by material and weight, rate-limited), shell casings (tinks and
plips), water (splashes, wading, strokes, plips), climbing (palm slap and the hold's material, mantle grunt, stamina
gasps), pommel strikes (3, never the same twice), the crowbar (3 hits, 2 on walls), parry, bash and counter (their own
recordings, layered), gibs, heads and limbs (squishes 4+4, bursts), headshot tick, the training dummy (wind-up, strike,
pain), menus (Quake's three clicks), the gadget's chime and buzz for messages out of view.

## Haptics pairing

Missing: the two-handed grip (row 6; done since: a pulse as the grip is taken). Present everywhere else checked: holster hover, catches, pickups, reloads, the
force grab (eligible, lock, launch, catch), climbing, splashes, hits taken, bash, parry, hands meeting, walls.

## Done in this review

| Commit | Change | Verified (headless, `run.sh -Sound`, `snd_show 2`) |
|---|---|---|
| Melee whoosh | A blade's, a fist's or a gun's swish takes the knights' two swings in turn (`knight/sword1`, `sword2`, as their own attacks pick them), not `sword1` every blow (`vr_melee.qc` VR_Melee_Whoosh). The axe, club, crowbar and saw keep `weapons/ax1.wav`. | four mock gun swings: `sword2` then `sword1` on the main hand's channel |
| Carry grab by material | A hand taking a prop sounds of what it is made of, played from the prop (in the hand), not a sword's swish from the head: crates and their pieces `vr/phys/grab_wood1..3`, rocks and bricks `grab_stone1..3`, explosive boxes `grab_metal1..3` (the climbing hand's grabs), gibs and heads `vr/squish_s1..4` at 0.7 (`vr_carry.qc` VR_Carry_GrabSound; precached in `world.qc`). Ammo and health boxes, backpacks, armour and keys keep the swish (row 13). | `impulse 245`'s gib: `vr/squish_s4` from the gib; a test crate gripped: `vr/phys/grab_wood2` from the crate |
| Shocked body crackle | A body's lasting lightning shock starting crackles (`misc/power.wav`, already the lightning gun's shock-in-water sound), again only once it has played out (1.8 s), so a beam held on a body crackles under its hum and the arcs after it are heard (`vr_shock.qc` VR_Shock_Crackle). `vr_shock_sound` 0.5 (Gore > Lightning Shock > Crackle Volume; 0 none). | `vr_shock_hit_test -1` on a grunt: one `misc/power.wav` from its body, `shock: monster_army crackles, volume 0.5` |

To try in the headset: swing a sword or punch a few times (the swish alternates); pick up a crate, a rock, an
explosive box and a gib (each its own grab, heard at the hand); kill something with the lightning gun and stop firing
(the body crackles while it arcs).
