# Gameplay preset: Classic, Light, Unleashed (and Classic Quake)

Player feedback: "a preset to make the mod as vanilla Quake as possible... going through so many options can be
daunting". **The settings list below is the author's decision**: the rows marked "?" are taste calls the first draft
was unsure of. Change a value in `Quake/vr/vr_gameplaypresets.cpp` (`rows`) and the table here together.

- **Classic**: as close to Quake's monsters, combat and movement as the mod allows.
- **Light**: Quake's rules, keeping the vanilla-but-better things: seamless teleporters, reloading, melee (parry,
  bash), physics props, ragdolls.
- **Unleashed**: every row's default (`default_string`, `vr_defaults.cfg`'s included): the game as shipped.
- Custom when the settings match no level; My Settings kept the first time a level is applied
  (`<gamedir>/gameplay_my_settings.cfg`), Save as My Settings replaces it. VR Settings > Gameplay Presets; console
  `vr_gameplay_preset 0|1|2` (3: My Settings), `vr_gameplay_preset_save`, `vr_gameplay_preset_status`.
- **Never in the table:** comfort, calibration, the body and arms, controls, sound, graphics (the Graphics preset's),
  gore (the Gore preset's). The test (`vr_gameplay_preset_test`) fails on a row of the other presets' or of those
  prefixes.
- **VR interaction is the same at every level:** hands, grabbing (props, weapons, force grab), holsters, climbing,
  throwing, swimming, item pickup by hand, the player's VR body (crouch, prone, hull width).
- **Tunables are not in it:** `vr_tunables.inc`'s constants are compiled in (`constexpr`) in a release build; only cvars.
- Most QC rows are read as a monster spawns or a map starts (armed replacements, crates, health cut): a level applied
  mid-map shows fully from the next map (the menu's help says so).

## Classic Quake

VR Settings' first rows (the desktop's Options > VR Settings is the same page) and the vrstart pavilion's east board
(`vr_setup_option classic`): **Classic Quake** applies Graphics Off (Quake), Gore Minimal and Gameplay Classic; the line
under it names what it set and what each was ("Set Graphics Off (Quake), Gore Minimal, Gameplay Classic (were High,
Full, Unleashed)."); **Back to My Settings** sets the three presets' My Settings back (graphics, gore and gameplay:
each kept on its first apply). Console: `vr_classic_quake 1` / `0` (it shows 1 while all three match).

## The rows

| cvar | what it does | Classic | Light | Unleashed (default) | ? |
|---|---|---|---|---|---|
| `vr_agrunt_repl` | grunts and enforcers spawn as armed humanoids holding their own guns | 0 | 0 | 1 |  |
| `vr_agknight` | knights spawn as armed humanoids with swords | 0 | 0 | 1 |  |
| `vr_agdknight` | death knights spawn as armed humanoids with swords (still casting) | 0 | 0 | 1 |  |
| `vr_grunt_burst` | grunts fire 3-round bursts (0: id's 4-pellet shotgun) | 0 | 0 | 1 | ? |
| `vr_enemy_drops` | monsters drop their weapons (0 when eligible, 1 always, 2 never) | 2 | 2 | 0 | ? |
| `vr_ammobox_drops` | ammo boxes drop weapons (0 when eligible, 1 always, 2 never) | 2 | 2 | 0 | ? |
| `vr_adgun_drops` | AD soldiers drop their guns | 0 | 0 | 1 | ? |
| `vr_admelee_drops` | AD melee monsters drop their weapons | 0 | 0 | 1 | ? |
| `vr_monster_health` | big monsters' health cut on a curve (1.1 pacing); 0: Quake's health | 0 | 0 | 1 | ? |
| `vr_ai_enhanced` | stealth AI umbrella (suspicion meter, hearing, searching, trails); 0: Quake's AI | 0 | 0 | 1 | ? |
| `vr_nav` | walking monsters follow navmesh paths; 0: Quake's movetogoal | 0 | 1 | 1 | ? |
| `vr_mhull` | monsters move with narrower compiled hulls; 0: Quake's hulls 1 and 2 | 0 | 1 | 1 | ? |
| `vr_enemy_shove` | grunts, enforcers and vores shove you away up close | 0 | 0 | 1 |  |
| `vr_enemy_melee_arc` | a monster's melee blow lands only inside its arc (dodgeable in VR) | 0 | 1 | 1 | ? |
| `vr_enemy_melee_space` | melee attackers kept 16 units off your head | 0 | 16 | 16 |  |
| `vr_enemy_aim_cone` | ranged shots leave along the monster's facing (bent at most its cone) | 0 | 1 | 1 | ? |
| `vr_enemy_liquid_damage` | monsters burn in slime and lava, drown underwater | 0 | 1 | 1 | ? |
| `vr_portals_monsters` | monsters walk through teleporters as you do | 0 | 1 | 1 |  |
| `vr_portals_ai` | monsters see and fire at you through teleporters | 0 | 1 | 1 |  |
| `vr_knockdown` | shoves and blows knock monsters down (ragdoll) | 0 | 0 | 1 |  |
| `vr_kdtrig` | ragdoll triggers: stairs, ledges, slopes, shotgun range, blasts | 0 | 0 | 1 |  |
| `vr_hit_stagger` | hits unbalance monsters (stagger) | 0 | 0 | 1 |  |
| `vr_ragimpact` | flung and knocked-down bodies hurt what they hit | 0 | 0 | 1 |  |
| `vr_legsweep` | a staff-like thing swept through the legs knocks down | 0 | 0 | 1 |  |
| `vr_hit_push` | heavy weapon hits shove monsters | 0 | 1 | 1 | ? |
| `vr_kill_push` | killing blows throw the bodies | 0 | 1 | 1 | ? |
| `vr_positional_damage` | head and limb hit zones change damage | 0 | 1 | 1 | ? |
| `vr_ragdoll` | dead bodies as ragdolls; 0: Quake's death animations | 0 | 1 | 1 | ? |
| `vr_ragdoll_active` | dying monsters stumble, clutch wounds, crawl (active ragdolls) | 0 | 0 | 1 | ? |
| `vr_corpse_collide` | corpses in the physics (4 heavy pushable shape); 0: things pass through | 0 | 4 | 4 |  |
| `vr_parry` | parry monsters' melee with a weapon held across | 0 | 1 | 1 | ? |
| `vr_bash` | guard bash and one-palm shove | 0 | 1 | 1 |  |
| `vr_counter` | after a parry, the next melee attack hits harder | 0 | 1 | 1 |  |
| `vr_headbutt` | lunge your head to headbutt | 0 | 1 | 1 |  |
| `vr_deflect` | melee swings bat projectiles back | 0 | 1 | 1 |  |
| `vr_parry_stamina` | parries cost stamina | 0 | 0 | 1 | ? |
| `vr_shove_stamina` | shoves and bashes cost stamina | 0 | 0 | 1 | ? |
| `vr_stamina_speed` | tired, you run slower | 0 | 0 | 1 |  |
| `vr_weight_stamina` | tired, things weigh more | 0 | 0 | 1 |  |
| `vr_climb_stamina` | hanging from a hold tires you | 0 | 0 | 1 | ? |
| `vr_foegrab` | an empty hand can hold a living enemy | 0 | 1 | 1 | ? |
| `vr_embed` | blades stick in bodies and walls; 0: thrown axes alone | 0 | 1 | 1 |  |
| `vr_grenade_catch` | grenades as physics you can catch and throw back (2 also yours); 0: Quake's | 0 | 2 | 2 |  |
| `vr_grenade_shoot` | shots, blows and blasts set grenades off | 0 | 1 | 1 | ? |
| `vr_bullettime_enabled` | bullet time (gadget tap) | 0 | 0 | 1 |  |
| `vr_reload_mode` | reloading (3 immersive); 0: guns fire from the reserve | 0 | 3 | 3 |  |
| `vr_weapon_recoil` | programmatic recoil kicks the gun in your hand | 0 | 1 | 1 | ? |
| `vr_portals_walk` | seamless teleporters (walk and shoot through); 0: Quake's teleport | 0 | 1 | 1 |  |
| `vr_teleporter_pair_exits` | two-way gates come out of the paired gate's face; 0: at the marker | 0 | 1 | 1 | ? |
| `vr_crates` | crates placed about the maps (next map) | 0 | 1 | 1 |  |
| `vr_explobox_physics` | explosive boxes pushed, tipped, carried; 0: fixed (id's) | 0 | 1 | 1 |  |
| `vr_debris_rocks` | rocks to pick up and throw | 0 | 1 | 1 | ? |
| `vr_debris_bricks` | bricks to pick up and throw | 0 | 1 | 1 | ? |
| `vr_closequarters` | walking straight at a near monster is slowed | 0 | 0 | 1 | ? |
| `vr_lootbeams` | beams over dropped weapons and magazines | 0 | 1 | 1 | ? |

Why each "?":

- `vr_grunt_burst`: Light: the burst rifle is the armed grunts' world; off with them, or kept as a plain grunt's gun?
- `vr_enemy_drops`: Light: weapon drops are a Quake VR economy, not vanilla; Light could keep When Eligible.
- `vr_ammobox_drops`: Light: as weapon drops.
- `vr_adgun_drops`: Classic/Light: AD's own guns only exist in AD; off as non-vanilla, or kept (AD is its own mod)?
- `vr_admelee_drops`: as AD gun drops.
- `vr_monster_health`: Light: the health cut makes up for VR aiming; Quake's health (0) or the cut?
- `vr_ai_enhanced`: Light: stealth is a big rules change, so off; but it is "better AI", maybe on in Light.
- `vr_nav`: Classic: Quake's monsters get stuck on ledges; navmesh paths are a fix more than a feature: keep on?
- `vr_mhull`: Classic: narrower monster hulls fix monsters stuck in doorways for the VR body; keep on?
- `vr_enemy_melee_arc`: Light: a fairness fix for a body that can dodge (on); Classic: Quake's 360-degree blows.
- `vr_enemy_aim_cone`: Light: as the melee arc.
- `vr_enemy_liquid_damage`: Light: not Quake's (monsters ignore lava there); kept as "vanilla-but-better"?
- `vr_hit_push`: Light: physical hit feedback; kept, or off with knockdowns?
- `vr_kill_push`: Light: as hit push.
- `vr_positional_damage`: Light: headshots are what VR aiming is for; Classic off (Quake's whole-body damage).
- `vr_ragdoll`: Light: ragdolls are physics flair (kept); Classic: Quake's death animations.
- `vr_ragdoll_active`: Light: dying monsters stumbling is close to knockdowns; off in Light?
- `vr_parry`: Classic: parrying is a VR melee mechanic, not Quake's; off in Classic?
- `vr_parry_stamina`: Light: stamina is not Quake's, off; but without it parry and bash have no cost.
- `vr_shove_stamina`: as parry stamina.
- `vr_climb_stamina`: Light: climbing stays (hands); its stamina off with the rest?
- `vr_foegrab`: Classic: holding enemies is VR grabbing (which stays everywhere) but also a combat move; off?
- `vr_grenade_shoot`: Light: shooting a grenade off is a physics bonus; kept?
- `vr_weapon_recoil`: Classic: Quake has no gun kick in the hand; recoil is VR feel, maybe kept everywhere.
- `vr_teleporter_pair_exits`: Classic: only matters with seamless teleporters; Quake's marker exit for clarity.
- `vr_debris_rocks`: Light: physics props stay, so rocks and bricks too; or only crates and boxes?
- `vr_debris_bricks`: as rocks.
- `vr_closequarters`: Light: a VR body comfort-ish rule (no walking into monsters); kept?
- `vr_lootbeams`: Classic: a visual helper, not gameplay strictly; off in Classic for the look?

## Considered and left out

- `vr_climb`, `vr_forcegrab_mode`, `vr_menu_pickup_mode` (item pickup), `vr_holster_mode`, `vr_2h_mode`: VR
  interaction, the same at every level.
- `vr_hull_width`, `vr_hull_hit_head`, `vr_crouch_sight`, `vr_prone`: the VR body (Quake's 32-unit hull is too wide
  for a room-scale body).
- `vr_decap_zombies`, `vr_limbs_zombies` (a zombie losing its head or a limb dies for good): gameplay, but under the
  Gore preset's `vr_decap` and `vr_limbs` (Gore Minimal turns those off).
- `vr_stealth_*`: under `vr_ai_enhanced` (the umbrella); `vr_agrunt_*`, `vr_agarmor_*`: only matter with the armed
  replacements.
- `vr_arena` (not saved, off by default), `vr_fresh_map_seeds` (randomness, not rules), `vr_sandevistan` (off by
  default), `vr_grenade_fuse_regrab` (off by default).
- Weapon wheel, monster climbing: no cvar of their own on `next` (2026-11-13).
