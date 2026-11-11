# Survival Arena: the budget director (design, for review)

Status: **design only**, nothing in the game reads this yet. Branch `agent/arenabudget`, 2026-11-11.
Files: this doc; the draft cost table `Misc/quakevr/arena_director_draft.txt` (the proposed data file's format, read by
the simulation); the simulation `Misc/quakevr/arena_director_sim.py` (`python Misc/quakevr/arena_director_sim.py
[--seeds 400] [--skill 0..3] [--wave-size 2] [--no-reactive]`).

## 1. One-screen summary

Every wave gets a **threat budget** and buys its foes from one price list. A foe costs **body + weapon + extras**
(armour, a second weapon, infection, a boss mark). Tiers stay as properties of the items: the wave an item unlocks at,
and a cap on its share of a wave's budget. The waves are planned like this:

1. **Budget** = a curve over waves, times a **sawtooth** across the boss cycle (0.85, 0.95, 1.05, 1.15, then the boss
   wave at 1.6), times the difficulty's factor and `vr_arena_wave_size`, plus or minus what the last wave carried over.
2. **Theme**: one is rolled per wave (Mixed, Swarm, Melee Rush, Ranged Siege, One Family, Elite, Monster Mash,
   Armory). It biases what gets picked. The same theme never comes twice in a row.
3. **Boss** (on boss waves): bought first, at most 55% of the wave's budget.
4. **Progression guarantee**: if 3 waves have gone by without a foe carrying a gun the player hasn't had yet, one
   grunt gets one, from the lowest tier he's missing.
5. **Roles**: the rest is split between fodder, mid, heavy and extras. The split moves toward heavy and extras as the
   waves go. Each role buys foes in its cost band, weighted by the theme, by **anti-repetition** (recently seen items
   are less likely, long-unseen ones get a bonus) and by a **humanoid quota** (70% of picks are armed humanoids, so the
   armed grunts stay the arena's backbone).
6. **Reinforcements**: from wave 6, 15% of the budget is held back for a second group that arrives mid-wave.
7. **Leftover** budget buys upgrades and the rest carries over (at most 25% of the next wave). The last foe may
   overspend by 2 points, and that debt comes off the next wave.
8. **Supplies** after a clear come from a separate budget that scales with the threat just beaten. It can lean on how
   the player is doing (low health: more healing; dominating: a power-up). `vr_arena_supply_reactive 0` turns that
   lean off.

All costs, curves, themes and supplies live in a data file (`quakevr/data/arena_director.txt`), editable without
recompiling. `vr_arena_seed` still reproduces a run's waves. The old director stays as `vr_arena_director 0`.

**What the simulation says** (Normal, 400 seeds, waves 1-30; current director vs budget director, priced with the
same table): total threat is about the same (4549 vs 4613). Repeats drop by more than half: 42% of a wave's
(body, weapon) combos were also in the previous wave, now 17%. Every 5-wave window holds more distinct combos (for
example 18.8 vs 33.8 in waves 16-20), more guns (13.0 vs 21.2) and more body types (4.0 vs 8.9). Waves where 3 or more
waves passed without a new gun drop from 1.89 per run to 0.06. Waves 6-12 are no longer the same every run: 1% of
those waves' plans are unique across seeds today, 100% with the budget director.

### The knobs you'll tweak

| knob | where | default | what it does |
|---|---|---|---|
| `vr_arena_director` | cvar | 0 (1 after your test) | 0 the current director, 1 the budget director |
| `vr_arena_wave_size` | cvar (exists) | 1 | multiplies the threat budget and the foe cap |
| `vr_arena_supply_reactive` | cvar | 1 | 0: supplies ignore how the player is doing |
| `vr_arena_themes` | cvar | 1 | 0: every wave is Mixed |
| `vr_arena_reinforce` | cvar | 1 | 0: no held-back group (the reserve is spent up front) |
| `vr_arena_supply_scale` | cvar | 1 | multiplies the supply budget |
| `vr_arena_director_log` | cvar (Debug menu) | 0 | prints each wave's budget, theme, picks and leftover |
| `curve threat` base/slope/power/saw | data | 9 / 7.5 / 1.05 / .85 .95 1.05 1.15 1.6 | how fast the waves grow; the build-up, peak and breather shape |
| `curve threat` count | data | 3 + 0.5 x wave | the most foes a wave plans (overflow becomes upgrades) |
| `curve roles` split | data | see section 4 | fodder/mid/heavy/extras over the waves |
| `curve roles` humanoid | data | 0.7 | the armed humanoids' share of the picks |
| `curve novelty` | data | recent .4 .6 .85, cap 1.6 | how hard repeats are avoided |
| item `cost` / `from` / `share` / `weight` | data | the table below | one item's price, unlock wave, cap and how often it's picked |
| `theme` weights and biases | data | section 6 | how often a theme comes and what it pushes |
| `curve supply` | data | 8 + 0.06 x threat | the supply budget |

Unchanged and still used: `vr_arena_boss_every`, `vr_arena_boss_health`, `vr_arena_max_alive` and the alive pool
(`alive_max`, `alive_grow`, `burst`), `vr_arena_spawn_gap`, `wave_rest`, `countdown`, `seed`, `skill_impact`,
`corpse_fade`/`corpse_max`, `item_waves`, `gun_max`, `scatter`, `foe_armor` (0: no armour bought), and presets
(`arena_waves` files override either director, as they do now). `vr_arena_medkits_after`, `armor_every` and `shards`
are the old director's. Under director 1 they are ignored, and the supply budget and `vr_arena_supply_scale` take
their place.

## 2. The cost table

Costs are **threat points**. An unarmed grunt is 4, a shotgun grunt 8, an ogre 25, a rocket grunt 20.
The evidence: id's health (`ad_chaosmode.qc` `ad_HP_MON*`: grunt 30, dog 25, zombie 60, knight 75, enforcer and scrag
80, ogre 200, death knight 250, fiend 300, vore 400, shambler 600); the weapons' player damage (an armed humanoid fires
the player's weapon, times `vr_agrunt_damage_scale`, at its AI's own cadence, so only the ratios matter): shotgun
24 per 0.5 s, super shotgun 56 per 0.7 s, nailgun 9 x 10/s, super nailgun 18 x 10/s, grenades 120 splash per 0.6 s,
rockets 120 + splash per 0.8 s, lightning 30 per 0.1 s; their ranges (`VR_AG_RangeOf`: shotguns and pistols near,
nail guns far, launchers keep their blast's distance); and what today's hand-made plans put together in one wave.
**(g)** marks a guess: AD's and the packs' guns were priced by their role next to the classic gun they resemble,
not measured. A foe's **role** is the band its total cost falls in: fodder up to 12, mid 13-24, heavy 25 and up.
`share` is the most of a wave's budget the item may take (a cap, relaxed only to keep a thin wave's grunt backbone).
Waves are Normal's. Difficulty moves every `from` past wave 3 as `VR_ARENA_WaveAdj` does today (+3 Easy, -2 Hard,
-4 Nightmare). The first rungs, from wave 3 or sooner, never move.

### Bodies

| body | cost | from | share | weight | role alone | reason |
|---|---|---|---|---|---|---|
| grunt (armed) | 4 | 1 | 1.0 | 1 | fodder | 30 hp; the weapon is its threat; the arena's backbone |
| enforcer (armed) | 8 | 6 | 0.5 | 1 | fodder-mid | 80 hp: 2.7 grunts of bulk; today one a wave from 6 |
| armed knight | 7 | 9 | 0.35 | 1 | fodder-mid | 75 hp, blades only; comes in with the knight's sword (row 9) |
| armed death knight | 18 | 17 | 0.4 | 1 | mid-heavy | 250 hp, blades only (g: AD's swords from 16-19) |
| zombie | 6 | 3 | 0.3 | 0.7 | fodder | 60 hp, gib-only, slow; flesh 10 a throw |
| dog | 5 | 6 | 0.3 | 0.7 | fodder | 25 hp, fast, bites; today's wave 6 |
| knight | 8 | 4 | 0.3 | 0.7 | fodder | 75 hp, melee; today's wave 4 |
| scrag (wizard) | 12 | 10 | 0.25 | 1 | fodder | 80 hp, flies, spits at range (new to the arena's own waves; g) |
| ogre | 25 | 8 | 0.4 | 1 | heavy | 200 hp, grenades and a chainsaw; today's wave 8 |
| fiend | 30 | 12 | 0.4 | 1 | heavy | 300 hp, the leap does 40-60; today's wave 12 |
| death knight | 30 | 16 | 0.4 | 1 | heavy | 250 hp, magic spray, sword; today's wave 16 |
| vore (shalrath) | 45 | 20 | 0.35 | 1 | heavy | 400 hp, homing 40-damage voreballs (new; g) |
| shambler | 60 | 26 | 0.4 | 1 | heavy | 600 hp, half damage from explosives, lightning (new; g) |
| id grunt / id enforcer | 8 / 14 | presets only | 0 | - | - | the plain ones (`plain_grunt`, `plain_enforcer`); the director leaves them to presets |

### Weapons (the 34-row ladder; cost is added to the body)

`from` is the row's eligible wave today. `weight` is today's pool share, so the rare ones stay rare.

| row | weapon | cost | tier | from | share | weight | on a grunt | reason |
|---|---|---|---|---|---|---|---|---|
| - | fists | 0 | 0 | 1 (until 6) | 1.0 | 1 | fodder | today's unarmed early grunts |
| 0 | shotgun | 4 | 1 | 3 | 0.5 | 0.6 | fodder | 48 dmg/s near |
| 1 | nail pistol | 3 | 1 | 2 | 0.5 | 0.3 | fodder | weaker than the shotgun, far (g) |
| 2 | axe | 3 | 1 | 3 | 0.25 | 0.1 | fodder | 40 dmg/s, must close in |
| 3 | crowbar | 3 | 1 | 3 | 0.25 | 0.1 | fodder | as the axe |
| 4 | super shotgun | 8 | 2 | 3 | 0.4 | 0.5 | fodder (12) | 80 dmg/s close |
| 5 | nailgun | 8 | 2 | 5 | 0.4 | 0.5 | fodder (12) | 90 dmg/s, far |
| 6 | heavy revolver | 7 | 2 | 6 | 0.3 | 0.5 | fodder | big single shots (g) |
| 7 | super nailgun | 12 | 2 | 7 | 0.4 | 1 | mid | 180 dmg/s, far |
| 8 | burst rifle | 9 | 2 | 8 | 0.3 | 0.6 | mid | bursts, mid range (g) |
| 9 | knight's sword | 5 | 1 | 9 | 0.3 | 0.3 | fodder | melee, parries |
| 10 | grenade launcher | 14 | 3 | 10 | 0.4 | 1 | mid | 200 dmg/s splash, area denial |
| 11 | enforcer's laser rifle | 9 | 2 | 11 | 0.3 | 0.6 | mid | far, slow bolts (g) |
| 12 | defender's shotgun | 9 | 2 | 11 | 0.3 | 0.5 | mid | AD; a heavier shotgun (g) |
| 13 | proximity gun | 15 | 3 | 12 | 0.3 | 0.5 | mid | mines: denial (g) |
| 14 | rocket launcher | 16 | 3 | 13 | 0.4 | 1 | mid | 120 + splash, the scariest burst |
| 15 | crossbow | 9 | 2 | 13 | 0.3 | 0.5 | mid | AD; far, slow (g) |
| 16 | lightning gun | 20 | 3 | 16 | 0.4 | 1 | mid | 300 dmg/s hitscan inside 400 |
| 17 | chainsaw | 8 | 2 | 16 | 0.25 | 0.3 | fodder (12) | melee, fast (g) |
| 18 | death knight's sword | 9 | 2 | 16 | 0.25 | 0.3 | mid | melee (g) |
| 19 | Shadow Axe | 9 | 2 | 17 | 0.25 | 0.3 | mid | AD melee (g) |
| 20 | Widowmaker | 10 | 2 | 17 | 0.3 | 0.5 | mid | AD super-shotgun class (g) |
| 21 | grenade soldier's launcher | 14 | 3 | 17 | 0.3 | 0.5 | mid | as the GL (g) |
| 22 | plasma soldier's rifle | 15 | 3 | 18 | 0.3 | 0.5 | mid | AD energy, far (g) |
| 23 | laser cannon | 18 | 3 | 19 | 0.4 | 1 | mid | Armagon's, far, bouncing (g) |
| 24 | Mjolnir | 12 | 2 | 19 | 0.25 | 0.3 | mid | melee + lightning (g) |
| 25 | AD knights' swords | 9 | 2 | 19 | 0.25 | 0.3 | mid | melee (g) |
| 26 | Super Axe | 10 | 2 | 20 | 0.25 | 0.3 | mid | melee (g) |
| 27 | rocket soldier's launcher | 16 | 3 | 20 | 0.3 | 0.5 | mid | as the RL (g) |
| 28 | flamethrower | 16 | 3 | 21 | 0.3 | 0.5 | mid | stream, short, burns (g) |
| 29 | monster swords | 13 | 3 | 23 | 0.2 | 0.2 | mid | the strong blades (g) |
| 30 | hammer and mace | 14 | 3 | 24 | 0.25 | 0.3 | mid | heavy melee (g) |
| 31 | centurion's plasma gun | 16 | 3 | 24 | 0.3 | 0.5 | mid | AD energy (g) |
| 32 | poison sprayer | 15 | 3 | 25 | 0.3 | 0.5 | mid | stream, poison (g) |
| 33 | Plasma Gun | 18 | 3 | 26 | 0.3 | 0.5 | mid | AD's top energy gun (g) |

An enforcer adds 4 to all of these (one band up for many). Melee-only bodies (armed knight, armed death knight) take
only the blades family. A row the game lacks (`VR_AG_Decode`: no AD data, no Armagon's) is not in the list, the same
as today. The simulation assumes every game's data is present.

### Extras

| extra | cost | from | share | reason |
|---|---|---|---|---|
| green armour | 3 + 0.5 a limb plate | 4 | 0.5 (all armour) | +25% effective health on a humanoid (today's ROUND23 table: the same gates) |
| yellow armour | 6 + 0.5 a plate | 12 | | today's yellow gate (AR_W_TIER2 + 2) |
| red armour | 10 + 0.5 a plate | 22 | | today's red gate (AR_W_TIER3 + 4) |
| plates | 2 at the armour's first wave, +1 every 3 waves (x factor) | | | today's `VR_ARENA_ArmourPieces` |
| second weapon (`ar_plan_gun2`) | 0.6 x that weapon's cost | 14 | 0.15 | two guns, not twice the fire (g) |
| infected (`ar_plan_infect`) | 4 | 10 | 0.1 | an infection's added threat (g) |
| boss mark | (body + weapon) x 2.5, + its armour (a type above the wave's best, 4 more plates) | | the boss's share (0.55) | today's boss: health x 1.25, armour, the player's focus. The Nightmare second boss is x 0.6 of that |

Boss candidates (each boss wave picks one; recently used ones are less likely): enforcer with a gun (5-14), ogre (10+),
grunt with a top-tier gun (15+), death knight (16+), armed death knight with a top blade (18+), vore (20+),
shambler (25+). The armed ones take the best tier unlocked. If no candidate fits the boss's 55% share, the cheapest
one is taken.

## 3. Budget curves

**Threat** (Normal, wave size 1): `(9 + 7.5 x (w-1)^1.05) x saw`, where saw is `0.85 0.95 1.05 1.15 1.6` across each
`vr_arena_boss_every` cycle (boss wave last). The breather after a boss is the cycle's 0.85. Calibrated so a 30-wave
run's total threat matches today's (4613 vs 4549). The shape is smoother: today's hand-made waves 8-12 are heavier
(wave 11: 127 vs 80) and its boss waves lighter (wave 20: 196 vs 278).

**Foe cap**: 3 + 0.5 x wave (x wave size x skill factor; 40 at most, the plan's own limit). A wave with budget left
over after the cap is reached spends it on upgrades, then carries it.

**Supply**: `(8 + 0.06 x threat just beaten) / skill factor`, at most 60 points, x `vr_arena_supply_scale`. Normal:
about 9 points after wave 1, 14 after wave 12, 16 after the wave-10 boss, 20-33 after waves 25-30. Today's is a flat ~11.5, with 23-31 on armour
waves. Per run: 517 vs 453. It grows with the threat on purpose.

## 4. Role split over waves (fodder / mid / heavy / extras, share of the budget after the boss and the guarantee)

| wave | 1 | 4 | 8 | 14 | 22 | 30 |
|---|---|---|---|---|---|---|
| fodder | 1.00 | 0.78 | 0.54 | 0.36 | 0.24 | 0.20 |
| mid | 0 | 0.20 | 0.30 | 0.30 | 0.29 | 0.27 |
| heavy | 0 | 0 | 0.10 | 0.17 | 0.20 | 0.20 |
| extras | 0 | 0.02 | 0.06 | 0.17 | 0.27 | 0.33 |

Linear between the key waves. A role with nothing unlocked passes its share down (heavy to mid to fodder), and so does
a role's leftover. Extras buy armour (75%), a second weapon (15%) or infection (10%) on the wave's humanoids, the
priciest first.

## 5. Anti-repetition, progression, reinforcements

- **Novelty**: an item last seen 1/2/3 waves ago weighs 0.4/0.6/0.85. From 4 waves on it weighs 1, plus 0.1 a wave,
  up to 1.6. An item never seen yet weighs 1.6, so a newly unlocked gun shows up soon (this replaces today's trickle).
  Bodies take the square root of this (grunts are in every wave).
- **In-wave repeats**: each copy of a weapon already in this wave multiplies its weight by 0.75 (by 0.35 under Armory;
  not at all for One Family's family).
- **Age**: an item in play more than 10 waves fades 5% a wave, down to 0.35 (today's `AR_GUN_FADE`/`FLOOR` idea).
- **Progression guarantee**: from wave 3, if 3 waves have gone by without a foe carrying a gun the player hasn't had,
  one grunt carries one: the lowest tier he's missing, the cheapest of those. If every unlocked gun is already his, it
  may take one up to 3 waves before its unlock (`peek`). "Had" means **offered by an earlier wave's foes**, not his
  real inventory, which keeps `vr_arena_seed` reproducible (open question 2). In the simulation this forces a gun
  0.9 times a run. The novelty bonus does the rest.
- **Reinforcements**: from wave 6, 15% of the budget (after the boss) is held back and bought from the fodder and mid
  bands. Those foes are planned with a hold flag and come in when the main group first drops to 40% of its planned
  foes alive, under a "Reinforcements!" banner. About 1.6 foes a wave (49 a run).

## 6. Themes

Rolled per wave from wave 4 (waves 1-3 are Mixed). Never the same as the last wave's, and half weight if used in the
last 3. Each theme's word is shown in the countdown ("Wave 9: Melee Rush").

| theme | from | weight | what it does | share of waves (sim) |
|---|---|---|---|---|
| Mixed | 1 | 2.5 | nothing | 43% |
| Swarm | 4 | 1 | split +25% fodder; cheap x2, fast x1.5 | 10% |
| Melee Rush | 5 | 1 | melee x4, ranged x0.4 | 9% |
| Ranged Siege | 6 | 1 | ranged x2.5, far x2, melee x0.3 (the high places fill) | 9% |
| One Family | 7 | 1 | one family rolled (shotguns, bullets, explosives, energy, blades) x5, repeats free | 8% |
| Elite | 8 | 1 | split -30% fodder, more heavy and extras: few, armoured, big | 8% |
| Monster Mash | 8 | 0.8 | monsters x3, humanoids x0.4, humanoid quota 0.25 | 7% |
| Armory | 10 | 0.8 | humanoids x2, quota 0.9, every gun different (repeat 0.35) | 6% |

## 7. Skill levels and wave size

The same factors as today (Easy 0.75, Normal 1, Hard 1.25, Nightmare 1.5, x `vr_arena_skill_impact`):

- threat budget and foe cap times the factor; unlock waves moved by `VR_ARENA_WaveAdj`; Nightmare's two bosses (the
  second at 0.6 of the cost); Easy's boss a wave less often. All as today.
- supply budget divided by the factor (Easy more generous).
- `vr_arena_wave_size` multiplies the threat budget and the foe cap, not the shares.

Simulated per run, current vs budget: Easy threat 2790 vs 3406 and foes 156 vs 215. Nightmare threat 7192 vs 6922
and foes 309 vs 378, supply 271 vs 437. The budget's Easy is a bit heavier than today's, because today's hand-made
early waves don't scale.

## 8. How it maps onto the existing machinery

- **The plan arrays stay**: `ar_plan_kind/gun/gun2/armour/amask/infect/boss/at`. The budget director writes the same
  plan that `VR_ARENA_PlanFoes` + `PlanTrickle` + `PlanArmour` write today. Everything after the plan is unchanged:
  places (ground/high; ranged and flying foes prefer high), the spawn gap, the alive pool and field cap, the burst,
  corpse fade, the x-ray of the last foes, gun cleanup, perks on boss clears, banners. New is one field,
  `ar_plan_hold` (1: a reinforcement, waits for the trigger), and the theme word on the countdown.
- **Where it runs**: recommended in C++ (`Quake/vr/vr_arenadirector.cpp`), next to `vr_arenawaves.cpp`. A wave from
  the director is handed to QC exactly like a preset's wave (`VR_ARENA_PresetPlan` reads its entries through
  `arenawavesfield`), so QC gains no new planning code and the presets' path gets reused. QC passes in what only it
  knows: which rows this game has (`VR_ARENA_GunOk`, once per run) and the player's health, armour and pace for the
  reactive supply. Why not QC: the QC's globals are near the id format's limit (`vr_arena_supply.qc` notes it), and
  a pick evaluates about 300 combos.
- **Reproducibility**: every roll comes from the wave's stream (`VR_ARENA_SeedWaves(w)`, a salt per purpose: plan,
  theme, upgrades). The director's memory (seen, offered guns, carry, themes) depends only on the seed and the plans,
  not on the player, so it can be **rebuilt by replanning waves 1..w-1** (a load, a restart) instead of being saved.
  `vr_arena_seed 0` needs one random base per run (today it draws a new one each wave).
- **Supplies** use their own stream (`AR_DROP_SALT`) and the existing scatter (`vr_arena_supply.qc`). Reactive leans
  change only what lands, never the foes.
- **Score**: recommended, a kill pays `vr_arena_score_kill x cost / 8` (a shotgun grunt 100, as today; a rocket grunt
  250; an ogre 313). Armour and the boss mark are already in the cost, so `AR_ARM_PAY` and `AR_KILL_BOSS` retire under
  director 1. The wave bonus stays.
- **Tests**: the arena tests 1 (plans), 5 (difficulty), 10 (armour) and 12 (weapon schedule) assert today's tables.
  Under director 1 they need budget variants: the threat within ±1 foe of the curve, the shares respected, the
  guarantee firing, the same seed giving the same plans.
- **Debug menu**: an "Arena Director" page with this wave's budget, theme, spend by role, carry and the guarantee's
  state; buttons to force a theme, reroll the next wave, and reload the data file (`arenadirector_reload`).

## 9. The data file

`quakevr/data/arena_director.txt`, in the same block syntax as `quakevr/data/monsters.txt` (parsed by the same reader
as `vr_monsterdata.cpp`): `curve <name> { key "value" ... }`, `body`, `weapon`, `extra`, `boss`, `theme`, `supply`
blocks, `//` comments. The full draft, with every number above, is `Misc/quakevr/arena_director_draft.txt`. One line
per item:

```
body grunt   { kind "grunt" cost "4" from "1" share "1" arms "any" tags "humanoid fodder" }
weapon rl    { row "14" cost "16" tier "3" from "13" share "0.4" weight "1" family "explosives" tags "ranged far" }
theme siege  { from "6" weight "1" name "Ranged Siege" bias "ranged:2.5 far:2 melee:0.3" }
supply quad  { cost "20" from "8" kind "power" max "1" gap "3" }
```

It's read at map start and on `arenadirector_reload`. A bad line is reported on the console and skipped, as preset
entries are.

## 10. Player supplies (separate budget)

Points: small medkit 3, medkit 5, megahealth 18 (from 10, only when he's under 35% health), shard 1, green 12,
yellow 20 (12+), red 30 (22+), ammo pack 3 (his guns' ammo), quad 20 and ring 12 (8+), pent 30 (15+). Split:
heal 55%, armour 30%, ammo 8%, power 7%. The power-ups' share is **banked** across clears until it buys one, at least
3 waves apart. Leftover points become shards. Reactive (on by default): heal +/-25% by his health (+ at low), -10% when
dominating; power +up to 10% when dominating (fast clear, little damage taken). In the simulation, healing is 47% of
the spend reactive (50% without) and power-ups 6.8% (3.7%).

## 11. Simulation results (Normal, 400 seeds; cells are CURRENT / BUDGET)

| wave | threat | foes | budget fod/mid/hvy % of threat | bodies | guns | combos | repeat % | armoured % | supply |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 6 / 8 | 1.5 / 2.0 | 100/0/0 | 1.0 / 1.0 | 1.0 / 1.0 | 1.0 / 1.0 | - | 0 / 0 | 11.5 / 8.5 |
| 3 | 35 / 27 | 4.5 / 4.1 | 100/0/0 | 2.0 / 1.4 | 3.0 / 2.7 | 4.0 / 3.0 | 21 / 27 | 0 / 0 | 11.5 / 9.6 |
| 5 boss | 79 / 67 | 4.5 / 3.8 | 26/2/72 | 2.0 / 2.4 | 3.3 / 2.7 | 3.5 / 3.2 | 36 / 30 | 44 / 36 | 11.5 / 12.0 |
| 8 | 93 / 70 | 6.0 / 6.2 | 58/42/0 | 4.0 / 3.0 | 3.0 / 3.7 | 5.5 / 5.3 | 33 / 20 | 13 / 25 | 23.5 / 12.2 |
| 10 boss | 141 / 135 | 6.0 / 7.1 | 30/19/51 | 4.0 / 4.0 | 3.0 / 4.1 | 5.7 / 6.3 | 41 / 15 | 28 / 25 | 11.5 / 16.1 |
| 12 | 133 / 97 | 7.0 / 7.5 | 43/46/11 | 4.0 / 3.6 | 4.0 / 4.9 | 6.4 / 6.7 | 35 / 14 | 25 / 26 | 31.5 / 13.8 |
| 15 boss | 164 / 206 | 6.0 / 10.5 | 21/30/49 | 4.0 / 4.5 | 2.0 / 6.5 | 5.0 / 9.3 | 62 / 17 | 39 / 33 | 11.5 / 20.4 |
| 18 | 142 / 164 | 7.0 / 10.2 | 19/49/31 | 3.0 / 4.9 | 5.0 / 6.2 | 6.3 / 9.0 | 63 / 13 | 45 / 35 | 11.5 / 17.8 |
| 20 boss | 196 / 278 | 7.0 / 12.1 | 12/31/58 | 4.0 / 5.4 | 5.0 / 8.0 | 7.0 / 11.0 | 44 / 13 | 52 / 41 | 31.5 / 24.7 |
| 22 | 212 / 183 | 10.0 / 10.6 | 21/35/44 | 4.0 / 4.7 | 6.5 / 7.1 | 9.1 / 9.4 | 48 / 13 | 52 / 33 | 11.5 / 19.0 |
| 25 boss | 304 / 352 | 11.0 / 14.4 | 11/25/64 | 5.0 / 5.7 | 7.5 / 9.3 | 10.7 / 12.7 | 36 / 14 | 62 / 40 | 11.5 / 29.1 |
| 28 | 256 / 260 | 11.0 / 13.6 | 15/35/50 | 4.0 / 5.0 | 7.6 / 9.2 | 10.1 / 12.0 | 37 / 13 | 68 / 40 | 31.5 / 23.6 |
| 30 boss | 364 / 424 | 13.0 / 17.1 | 9/25/66 | 6.0 / 6.4 | 7.5 / 10.6 | 12.2 / 14.9 | 39 / 15 | 59 / 44 | 11.5 / 33.5 |

Distinct per 5-wave window (combos / guns / bodies): waves 1-5 8.7/5.8/4.0 vs 7.7/5.9/3.3; 6-10 14.6/7.3/6.0 vs
19.0/10.9/6.3; 11-15 15.8/8.5/6.0 vs 26.1/15.0/7.3; 16-20 18.8/13.0/4.0 vs 33.8/21.2/8.9; 21-25 29.7/21.1/5.0 vs
39.4/25.5/9.2; 26-30 32.6/21.9/6.0 vs 44.2/28.1/9.4.

Per run: threat 4549 vs 4613; foes 222 vs 270; repeat rate 42% vs 17%; combos a wave 6.8 vs 7.9; drought waves (3+
waves without a new gun) 1.89 vs 0.06; guarantee forced 0.91 times; reinforcement foes 49; second guns and infections
33; supply points 453 vs 517. Unique plans across seeds: today's waves 6-10 and 12-20 are 0-13% unique (a fixed
structure with few rolls), the budget director's 100% from wave 8. Armed humanoids are 67-86% of a wave's foes today and 74-82% with the budget director.
How often each item appears (share of all waves): grunt 100%, enforcer 82%, shotgun 52%, armed knight 44%, dog 42%,
nail pistol 41%, the classic tier 2-3 guns 11-32%, AD's 4-10%, vore 2%, shambler 1%. Today it's grunt 100%,
enforcer 83%, zombie 67%, dog 43%, the classic launchers and LG 24-29%, the bottom rungs 2-12%.

### What the simulation changed in the design

1. **Boss waves were the boss alone** (wave 5: one foe). Added the boss's 55% share cap and raised the boss wave's
   saw to 1.6.
2. **Rare guns came up as often as the shotgun** (the axe in 45% of waves): base weights are now today's pool shares,
   and the novelty cap went from 2 to 1.6.
3. **Too little armour** (14% of foes vs today's 60%): extras became a fourth column of the split, rising to 33%.
   It's still 1 in 3 armoured late, against 2 in 3 today (open question 9).
4. **Waves bloated to 25 foes**: added the foe cap (3 + 0.5 x wave). Overflow goes to upgrades, then to the carry.
5. **Humanoids fell to half the foes** (cheap monsters filled the fodder): added the humanoid quota (0.7).
6. **Power-ups were never affordable**: their share is banked across clears.
7. **Easy delayed the first guns to waves 5-6**: the first rungs aren't moved by the difficulty, as today.
8. **Late fodder only ever held bottom-rung guns**: the fodder band went from 11 to 12, so a grunt with a super
   shotgun or nailgun counts as fodder.
9. **Gaps with nothing new to offer** (Easy, Nightmare): the guarantee may peek 3 waves ahead.

## 12. Open questions (each with a recommendation)

1. **Default director?** Rec: ship `vr_arena_director 0` for one round of your testing, then make 1 the default and
   keep 0 as the classic option.
2. **Does the guarantee follow the guns offered or his real inventory?** Rec: offered. It keeps seeds reproducible and
   the memory rebuildable; a gun he left on the floor still counts as offered.
3. **Scrags, vores and shamblers in the arena's own waves** (never in today's)? Rec: yes, at 10/20/26, with the
   weights above (2% and 1% of waves for the last two). Scrags use the high places.
4. **Second weapons and infection as regular extras** (today only presets use them)? Rec: second weapons yes (from
   14). Infection from 10 at a 10% cap, or off (share 0) until you've seen it in the arena.
5. **Power-ups in the supplies** (none today; Quad Strike is a perk)? Rec: yes, rare (banked, 3 waves apart), quad and
   ring from 8, pent from 15.
6. **Score per kill from cost** (`score_kill x cost / 8`)? Rec: yes. It rewards the dangerous foes, and armour and
   boss pay come with it.
7. **About 20% more foes for the same threat** (270 vs 222 a run)? Rec: keep (more, cheaper foes means more variety
   and more of the armed grunts' moments). Lower `count` or raise the extras column if waves run long.
8. **A stronger sawtooth** (boss waves +40-50% over today's, the waves after them -20%)? Rec: keep; that's the
   build-up, peak and breather. Flatten `saw` toward 1 if it feels uneven.
9. **Armour: 1 in 3 humanoids late vs 2 in 3 today?** Rec: keep. Raising the late extras column to ~0.45 matches
   today, at the cost of fewer foes.
10. **Show the theme in the countdown** ("Wave 9: Melee Rush")? Rec: yes, with `vr_arena_theme_banner 1` to turn it
    off.
11. **Nightmare supplies grow with its threat** (437 vs 271 a run)? Rec: divide the supply by the factor squared at
    Nightmare only, which brings it near 290.
12. **Planner in C++ handing waves to the preset path** (section 8) rather than in QC? Rec: C++, for the globals
    limit, speed and unit tests. The QC keeps spawning, armour dressing and everything after the plan.
