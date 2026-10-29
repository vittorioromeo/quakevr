//
// vr_arena.qc - Survival Arena: the wave director.
//
// The waves and their ladder: the first waves are unarmed grunts, then grunts with a nail pistol and a shotgun,
// and so on up the weapon ladder with the mix drawn afresh every wave (vr_arena_seed's rolls), a boss every
// vr_arena_boss_every waves, Quake's own monsters joining as the waves go, and medkits and armour dropped between
// waves. The player starts at their fists and climbs the ladder on the guns their enemies carried (the game's
// own enemy drops: vr_enemy_drops).
//
// Off by default: none of this runs unless the map is "vrarena" or vr_arena is set. A wave's foes are marked
// with .ar_slot (their wave's number, flipped negative once they fall), so the director never has to guess who
// of the map's monsters is its own.
//
// The knobs (Quake/vr/vr_cvars.inc; read here with plain cvar()):
//   vr_arena              0    the arena runs even in a map that is not "vrarena"
//   vr_arena_wave_rest    5    seconds of rest from a wave clear (and from the run's start) to the next wave's
//                             countdown (at Normal skill: a harder skill rests less)
//   vr_arena_countdown    5    seconds the next wave counts down in the middle of the view ("Wave 7 in 5..."; a
//                             boss wave names its boss; 0: none; the same at every skill)
//   vr_arena_max_alive    8    foes alive at once at most; the rest of the wave trickles in as they free up (at
//                             Normal skill: a harder skill has more of them up at once)
//   vr_arena_boss_every   5    a boss wave every this many waves (0: no boss waves; at Easy skill: effectively
//                             +1, a boss less often)
//   vr_arena_boss_health  1.25 a boss's health times this (times the skill's factor too; a Nightmare boss wave
//                             brings two of them, the second half as big)
//   vr_arena_medkits_after 2   medkits after every wave (0: none; at Normal skill: an Easy clear drops more, a
//                             Nightmare one less)
//   vr_arena_armor_every  4    an armour after every this many waves (0: none; at Normal skill: the harder the
//                             skill, the longer its gap)
//   vr_arena_score_kill   100  the score of a kill, times its ladder tier (a boss: four of those; at Normal skill:
//                             a harder skill pays more)
//   vr_arena_seed         0    the waves' rolls: 0 a different ladder every run, else that seed's ladder
//   vr_arena_skill_impact 1    Difficulty Impact: how far the game's difficulty (skill) moves the waves -- 0 the
//                             arena is the same at every skill, 1 the steps below (Easy softer, Nightmare harder)
//   vr_arena_corpse_fade  14   s a fallen foe's body lies before it fades out (give or take 3 s; 0: never by time)
//   vr_arena_corpse_max   12   fallen bodies kept at most: past it the oldest fades first (0: no limit)
//   vr_arena_item_waves   2    a wave clear's medkits and armour still lying there this many wave clears later are
//                             removed (0: never)
//   vr_arena_gun_max      24   guns the arena's foes dropped lying about at most: past it the oldest fades (0: no
//                             limit; twice this with the Spoils perk)
//   vr_arena_test         0    the QC's arena tests (vr_arena_test.qc): 1 the plans, 2 the live waves, 3 a boss
//                             wave, 4 the places, 5 the difficulty scaling, 6 the long run, 7 the cleanup, 8 none of
//                             it off the arena, 9 the wave flow, 10 the foes' armour, 11 the loot the waves leave
//                             behind, 12 the weapon schedule, 13 no overlapping spawns, 14 the rest and countdown
//
// None of the cleanup ever takes a thing a hand holds, a force grab pulls or a body still dying: it waits for it.
//

// ---------------------------------------------------------------------------
// The fields the arena puts on things
// ---------------------------------------------------------------------------

.float kind;    // info_arena_spawn: 1 a ground point, 2 a high one (a wave's ranged and flying foes come in above the floor)
.float ar_slot; // > 0: one of the arena's wave monsters, of the wave that number (its fall flips it to that number's negative)
.float ar_tier; // what a wave monster is worth on the ladder: 1, 2, 3 (a boss: plus AR_BOSS_MARK)
.float ar_armour; // the armour type the arena dressed a wave monster in (0 none): its kill pays more
.float ar_drop; // > 0: an arena's own drop between waves: 1 a medkit, 2 an armour
.float ar_drop_wave; // a drop: the wave whose clear dropped it (vr_arena_item_waves)
.float ar_fell;     // a fallen foe's body: its place in the order the run's foes fell (the oldest goes first)
.float ar_expiring; // 1: an arena leftover on the cleanup's walk (findflags): a body, a drop on its way out
.float ar_expire;   // > 0: when the cleanup removes it (its last second fades it out); 0 not by time

// ---------------------------------------------------------------------------
// The waves' tables
// ---------------------------------------------------------------------------

#define AR_PLAN_MAX 40   // a wave's planned foes at most (its alive pool caps how many of them are up at once)
#define AR_FIELD_MAX 16  // vr_arena_max_alive's own clamp, after the skill's factor: never more foes up at once
#define AR_RING_MAX 8    // the fallback ring's places around the player, when a map has no info_arena_spawn
#define AR_ASIDE_MAX 5   // a place is tried as the map drew it, then aside this many ways
#define AR_ASIDE_STEP 48 // how far a step off a taken place goes (a monster's own box is 32 wide)
#define AR_PLACE_GAP  48 // a place this close to one of this wave's live foes counts as taken

// The species a wave sends (the plan's kind: its monster's own spawn function).
float AR_SP_GRUNT    = 0; // monster_vr_grunt: a grunt holding one of the player's weapons
float AR_SP_ENFORCER = 1; // monster_vr_enforcer: an enforcer holding one of the player's weapons
float AR_SP_ZOMBIE   = 2; // monster_zombie
float AR_SP_DOG      = 3; // monster_dog
float AR_SP_KNIGHT   = 4; // monster_knight
float AR_SP_OGRE     = 5; // monster_ogre
float AR_SP_FIEND    = 6; // monster_demon1: a fiend
float AR_SP_HKNIGHT  = 7; // monster_hell_knight: a death knight

// The ladder's rungs a wave may draw from, and the waves Quake's own monsters join at. These are the Normal
// skill's waves: VR_ARENA_WaveAdj moves every one of them by the difficulty (earlier at Hard and Nightmare,
// later at Easy).
float AR_W_TIER2   = 10; // the second tier (the super shotgun, the nail guns) from this wave
float AR_W_TIER3   = 18; // the third tier (the launchers, the lightning) from this wave
float AR_W_ZOMBIE  = 2;  // (a wave 3's zombie: the plan's own line; the table's floor)
float AR_W_KNIGHT  = 4;
float AR_W_DOG     = 6;
float AR_W_OGRE    = 8;
float AR_W_FIEND   = 12;
float AR_W_HKNIGHT = 16;

float AR_FIST        = -1;   // the plan's "no gun": an armed humanoid at their fists (monster_vr_grunt's "weapon" -1)
float AR_BOSS_MARK   = 8;    // a boss's ar_tier carries this (its worth: the tier times four)
float AR_TICK        = 0.5;  // the director's beat (s)
float AR_BOSS_WARN   = 3;    // s a mid-wave boss is announced before it comes in (in place of its gap)
float AR_NEAR        = 320;  // a foe comes in no nearer the player's eyes than this, when the map has a farther place
float AR_VIEW_COS    = 0.5;  // a place within this cosine of the player's view (about 60 degrees off it) is in view
float AR_RING_R      = 400;  // no info_arena_spawn in the map: foes come in this far off the player
float AR_DROP_R      = 220;  // its medkits and armour land about this far off the player
float AR_STALL       = 300;  // a wave this long over is cleared (its stragglers removed) so the arena keeps going
float AR_WAVE_BONUS  = 250;  // a wave clear's score times the wave
float AR_KILL_BOSS   = 4;    // a boss is worth this many of its tier's kills
float AR_KIT_MAX     = 4;    // vr_arena_medkits_after's own clamp: a wave clear never drops more medkits than this
float AR_ARMOR_MAX   = 12;   // vr_arena_armor_every's own clamp: an armour's gap never stretches past this
float AR_FADE        = 1;    // s a leftover's fade out takes, at the end of its time (the Horde's bodies' own)
float AR_FADE_JITTER = 3;    // s vr_arena_corpse_fade is given or taken at random (at most a quarter of it)
float AR_HELD_GRACE  = 3;    // s a leftover a hand let go of lies on at least before its fade
float AR_DYING_MAX   = 10;   // s a body still dying is waited for at most (then it goes as any other)
float AR_SPOILS_GUNS = 2;    // the Spoils perk: vr_arena_gun_max times this guns lie about

// A wave's humanoids' armour (vr_agarmor.qc's plates; VR_ARENA_ArmourChance and the rest below; ROUND23.md's table).
float AR_ARM_BASE     = 0.10; // the chance at the armour's first wave (Normal, vr_arena_foe_armor_scale 1)
float AR_ARM_STEP     = 0.03; // and this much more each wave after it
float AR_ARM_CAP      = 0.85; // never past this (a wave always has some bare foes)
float AR_ARM_PIECE_W  = 3;    // one more plate past the cuirass and helmet every this many waves (by the factor)
float AR_ARM_GUARD    = 0.15; // a boss wave's humanoids: this much more chance (the boss's guard)
float AR_ARM_BOSS_PCS = 4;    // a humanoid boss wears this many plates past the wave's
float AR_ARM_PAY      = 0.25; // an armoured kill's score: this much more per armour type (green 1.25, red 1.75)

// ---------------------------------------------------------------------------
// The run's state (file-scope: QC has no function-local statics)
// ---------------------------------------------------------------------------

entity ar_mgr;             // the arena's director (world: no arena in this map)
entity ar_fallen;          // the last player the arena saw fall (its message's target)

float ar_wave;             // the wave in the field (0: none yet)
float ar_score;            // the run's score
float ar_kills;            // the run's kills
float ar_wave_kills;       // this wave's kills
float ar_plan_n;           // this wave's planned foes
float ar_plan_left;        // its foes not in the field yet
float ar_plan_live;        // its foes put in the field (up or down)
float ar_plan_kind[AR_PLAN_MAX];
float ar_plan_gun[AR_PLAN_MAX];
float ar_plan_tier[AR_PLAN_MAX];
float ar_plan_boss[AR_PLAN_MAX];
float ar_plan_armour[AR_PLAN_MAX]; // its armour's type (0 none; vr_agarmor.qc's 1 green, 2 yellow, 3 red)
float ar_plan_amask[AR_PLAN_MAX];  // and its plates (vr_agarmor.qc's piece bits)
float ar_arm_type;         // VR_ARENA_ArmourRoll's answer: the type (0 none) and the plates
float ar_arm_mask;
float ar_next_spawn;       // the time the next planned foe may come in
float ar_wave_at;          // the time this wave began
float ar_alive_peak;       // the most foes the arena had up at once, this run (its wave cap at work)
float ar_drop_health;      // a wave clear's medkits
float ar_drop_armour;      // a wave clear's armours
float ar_spot_next;        // the round robin over the map's spawn points
float ar_item_next;        // the round robin over the map's item points
float ar_used_ground;      // this wave's foes placed on the map's floor places
float ar_used_high;        // this wave's foes placed on the map's high places
float ar_s1; float ar_s2; float ar_s3; // the wave rolls' three states (Levy's triple, as vr_armedrepl.qc's)
float ar_serverflags0;   // the map's serverflags as they were, for a fresh run (as vr_mg_horde.qc keeps its own)
float ar_stripped;         // the players have been put down to their fists already
float ar_down;             // the players are down: a fresh run is on its way
float ar_fell_n;           // the run's bodies so far (their .ar_fell order)
float ar_pool;             // this wave's alive pool: its foes up at once at most (VR_ARENA_Pool)
float ar_burst_left;       // this wave's opening burst: foes still to come in at once, without a gap
float ar_gap_from;         // the time the pending gap was started (a slot freed up), 0: no gap pending
float ar_last_spawn;       // the time this wave's last foe came in (the stall is counted from it)
float ar_wave_peak;        // the most of this wave's foes up at once
float ar_gap_lo;           // this wave's shortest and longest trickle gap (s; the tests read them)
float ar_gap_hi;
float ar_gap_n;            // this wave's trickle gaps so far
float ar_count;            // the countdown's seconds still to say (VR_ARENA_CountThink)
float ar_count_at;         // the time the countdown to the next wave began (0: none; the tests read it)
float ar_rest_at;          // the time the rest before it began (a wave clear, the run's start)
float ar_count_said;       // the countdown's lines said so far (the tests read it)

// The director's own beats (vr_arena.qc's own cycle; world.qc only calls WorldSpawn and Frame).
void() VR_ARENA_WaveStart;
void() VR_ARENA_CountStart;
float() VR_ARENA_Rand;
float(float xKind) VR_ARENA_ArmsHumanoid;
float(float x) VR_ARENA_SkillWhole;
void() VR_ARENA_ManagerThink;
void() VR_ARENA_RestartThink;

// The boss wave's perk choice (vr_arena_perks.qc, compiled after this file): every wave clear reports itself to
// it, and so does the director's frame. Neither does anything unless the run took a perk / the offer knob is on.
// (A foe's fall goes to it from combat.qc's Killed, where the killer is at hand.)
void(float xWave) VR_ARK_WaveDone;
void() VR_ARK_Frame;

// ---------------------------------------------------------------------------
// The knobs, and the small maths they need
// ---------------------------------------------------------------------------

float() VR_ARENA_On =
{
    if(mapname == "vrarena") { return TRUE; }
    return cvar("vr_arena") != 0;
}

float(float xValue, float xLow, float xHigh) VR_ARENA_Clamp =
{
    if(xValue < xLow)  { return xLow; }
    if(xValue > xHigh) { return xHigh; }
    return xValue;
}

float(float x, float xMod) VR_ARENA_Mod =
{
    float r = x - xMod * floor(x / xMod);
    if(r < 0)  { r = r + xMod; }
    if(r >= xMod) { r = r - xMod; }
    return r;
}

// The difficulty this arena is running at. world.qc reads the skill cvar into the `skill` global at the top of every
// frame (world.qc:588); at world spawn the global is not set yet -- triggers.qc's own note -- so there the arena
// takes the cvar, the difficulty the run started on. (0 is Easy's own value too, so falling back changes nothing
// where the global is already right.)
float() VR_ARENA_SkillLevel = { return skill > 0 ? skill : cvar("skill"); }

// The skill's factor for a knob: `easy`, `hard`, `nightmare` its steps (Normal 1), by vr_arena_skill_impact -- the
// shape of the armed humanoids' VR_AGAI_Skill (vr_armedgrunt_ai.qc), the arena's own impact knob, and nothing to do
// with vr_agrunt_ai_skill or the armed humanoids' AI.
//
// The skill's steps (impact 1):                   Easy   Normal  Hard   Nightmare
//   a wave's size, its pacing, a boss's health,
//   a wave clear's supplies, a kill's score,
//   and how many perks a boss clear offers:       0.75   1       1.25   1.5
//   (impact 0: 1 at every skill -- the skill changes nothing about the arena; impact 2: the deviation from 1
//   doubled; the factor is never below 0, so a knob it multiplies never turns negative)
float(float easy, float hard, float nightmare) VR_ARENA_Skill =
{
    float s = VR_ARENA_SkillLevel();
    float m = s < 1 ? easy : s < 2 ? 1 : s < 3 ? hard : nightmare;
    return max(0, 1 + cvar("vr_arena_skill_impact") * (m - 1));
}

// The difficulty's own word, for the line the player reads: one word per skill.
string(float xSkill) VR_ARENA_SkillName =
{
    return xSkill < 1 ? "Easy" : xSkill < 2 ? "Normal" : xSkill < 3 ? "Hard" : "Nightmare";
}

float() VR_ARENA_Opt_MaxAlive   { return VR_ARENA_Clamp(cvar("vr_arena_max_alive"), 1, AR_FIELD_MAX); }
float() VR_ARENA_Opt_WaveSize   { return VR_ARENA_Clamp(cvar("vr_arena_wave_size"), 0.25, 4); }
float() VR_ARENA_Opt_AliveMax   { return VR_ARENA_Clamp(cvar("vr_arena_alive_max"), 1, AR_FIELD_MAX); }
float() VR_ARENA_Opt_AliveGrow  { return VR_ARENA_Clamp(cvar("vr_arena_alive_grow"), 0, 20); }
float() VR_ARENA_Opt_Burst      { return VR_ARENA_Clamp(cvar("vr_arena_burst"), 1, AR_FIELD_MAX); }
float() VR_ARENA_Opt_Gap        { return VR_ARENA_Clamp(cvar("vr_arena_spawn_gap"), AR_TICK, 10); }
float() VR_ARENA_Opt_Rest       { return VR_ARENA_Clamp(cvar("vr_arena_wave_rest"), 1, 60); }
float() VR_ARENA_Opt_Countdown  { return VR_ARENA_Clamp(floor(cvar("vr_arena_countdown")), 0, 10); }
float() VR_ARENA_Opt_BossEvery  { return VR_ARENA_Clamp(cvar("vr_arena_boss_every"), 0, 20); }
float() VR_ARENA_Opt_BossHealth { return VR_ARENA_Clamp(cvar("vr_arena_boss_health"), 1, 8); }
float() VR_ARENA_Opt_Medkits    { return VR_ARENA_Clamp(cvar("vr_arena_medkits_after"), 0, AR_KIT_MAX); }
float() VR_ARENA_Opt_ArmorEvery { return VR_ARENA_Clamp(cvar("vr_arena_armor_every"), 0, AR_ARMOR_MAX); }
float() VR_ARENA_Opt_ScoreKill  { return VR_ARENA_Clamp(cvar("vr_arena_score_kill"), 1, 10000); }

// A group of a wave's foes at this skill: the wave's own count times the skill's factor, whole foes (floor), and a
// group is never emptied out -- the wave the difficulty thins still brings its zombie, its dog, its boss.
float(float xN) VR_ARENA_SkillCount = { return max(1, floor(VR_ARENA_Skill(0.75, 1.25, 1.5) * xN)); }

// How many of a wave's foes may be up at once: vr_arena_max_alive (clamped to what works, above) by the skill, and
// still never more than a wave can plan and never below one.
float() VR_ARENA_FieldCap = { return VR_ARENA_Clamp(VR_ARENA_SkillCount(VR_ARENA_Opt_MaxAlive()), 1, AR_FIELD_MAX); }

// A crowd group of a wave's foes: the ladder's own count times vr_arena_wave_size (whole foes, rounded), then by the
// skill's factor as every group is (VR_ARENA_SkillCount). A wave's single signature foe and its boss are not crowds.
float(float xN) VR_ARENA_WaveCount = { return VR_ARENA_SkillCount(VR_ARENA_SkillWhole(xN * VR_ARENA_Opt_WaveSize())); }

// A wave's alive pool: vr_arena_alive_max, one more every vr_arena_alive_grow waves (0: never more), by the skill's
// factor, and never past the field's cap (vr_arena_max_alive by the factor).
float(float xWave) VR_ARENA_Pool =
{
    float grow = VR_ARENA_Opt_AliveGrow();
    float n = VR_ARENA_Opt_AliveMax() + (grow > 0 ? floor(max(0, xWave - 1) / grow) : 0);
    return VR_ARENA_Clamp(VR_ARENA_SkillCount(n), 1, VR_ARENA_FieldCap());
}

// The wait before the next foe comes in, once a slot in the pool is free: vr_arena_spawn_gap at this skill (the
// harder the skill, the shorter), and up to as long again at random -- 1.5 to 3 s at Normal.
float() VR_ARENA_SpawnGap =
{
    float lo = VR_ARENA_SkillTime(VR_ARENA_Opt_Gap(), AR_TICK);
    return lo + lo * VR_ARENA_Rand();
}

// A span of seconds the arena waits, at this skill: the harder the skill the less it waits (the factor divides the
// span), never below xMin. A factor of 0 leaves the span as the knob sets it -- it does not make a wave arrive all
// at once.
float(float xSec, float xMin) VR_ARENA_SkillTime =
{
    float f = VR_ARENA_Skill(0.75, 1.25, 1.5);
    if(f <= 0) { return xSec; }
    return max(xMin, xSec / f);
}

// A whole number the difficulty buys: the scaled value rounded to the nearest whole thing it buys (a medkit, a
// wave of gap, a point of score), so a run's supplies and score are readable off the console.
float(float x) VR_ARENA_SkillWhole = { return floor(x + 0.5); }

// A wave clear's supplies at this skill: the knob's Normal value divided by the factor -- an Easy clear is the more
// generous one, a Nightmare the thinner -- never below xMin, never past the knob's own clamp xMax, and a knob of 0
// (no medkits, no armour) means none at every skill. A factor of 0 leaves the knob as it is set: the arena does not
// drop an infinite number of medkits.
float(float xN, float xMin, float xMax) VR_ARENA_SkillSupply =
{
    float f = VR_ARENA_Skill(0.75, 1.25, 1.5);
    if(xN <= 0) { return 0; }
    if(f <= 0) { return xN; }
    return VR_ARENA_Clamp(VR_ARENA_SkillWhole(xN / f), xMin, xMax);
}

// The gap between one of a wave clear's armours and the next, at this skill: an interval divides the generosity, so
// it moves the other way from a count -- times the factor (vr_arena_armor_every 4 is an armour every 3 waves at
// Easy, every 4 at Normal, every 6 at Nightmare).
float(float xN, float xMax) VR_ARENA_SkillGap =
{
    if(xN <= 0) { return 0; }
    return VR_ARENA_Clamp(VR_ARENA_SkillWhole(xN * VR_ARENA_Skill(0.75, 1.25, 1.5)), 1, xMax);
}

// What something is worth at this skill: times the factor (risk pays -- a Nightmare kill is worth half again),
// whole points, and never nothing.
float(float xValue) VR_ARENA_SkillWorth =
{
    return max(1, VR_ARENA_SkillWhole(xValue * VR_ARENA_Skill(0.75, 1.25, 1.5)));
}

// A wave number of the ladder -- a gate a rung or a species arrives at -- as this difficulty counts it: whole
// waves, not a factor (3 waves later at Easy, none at Normal, 2 sooner at Hard, 4 sooner at Nightmare, times
// vr_arena_skill_impact), and never earlier than wave 2, the bestiary's own floor. So on Nightmare the fiends,
// the death knights and the launchers arrive early and on Easy they hold off.
float(float xWave) VR_ARENA_WaveAdj =
{
    float s = VR_ARENA_SkillLevel();
    float d = s < 1 ? 3 : s < 2 ? 0 : s < 3 ? -2 : -4;
    return max(2, xWave + d * cvar("vr_arena_skill_impact"));
}

// How many of the first waves' humanoids come unarmed: the softer the difficulty, the wider the share (its own
// ladder 0.30 Easy / 0.20 Normal / 0.15 Hard / 0.10 Nightmare, moved by vr_arena_skill_impact from Normal's 0.20
// and held to a share).
float() VR_ARENA_EarlyUnarmed =
{
    float s = VR_ARENA_SkillLevel();
    float m = s < 1 ? 0.30 : s < 2 ? 0.20 : s < 3 ? 0.15 : 0.10;
    return VR_ARENA_Clamp(0.20 + cvar("vr_arena_skill_impact") * (m - 0.20), 0, 0.5);
}

// A boss wave's gap, as this difficulty counts it: at Easy the marked big ones come a wave LESS often
// (vr_arena_boss_every effectively +1); Normal, Hard and Nightmare keep the knob as it is, and at
// vr_arena_skill_impact 0 every skill keeps it exactly.
float() VR_ARENA_BossEvery =
{
    float every = VR_ARENA_Opt_BossEvery();
    if(every <= 0) { return 0; }
    if(VR_ARENA_SkillLevel() >= 1) { return every; }
    return every + cvar("vr_arena_skill_impact");
}

// How many marked big ones a boss wave brings: Nightmare brings two (the second the same kind at half its bulk),
// every other skill the one it always had.
float() VR_ARENA_BossTwice = { return VR_ARENA_SkillLevel() >= 3 && cvar("vr_arena_skill_impact") > 0; }

// A boss's bulk: its kind's own health times vr_arena_boss_health times the difficulty's factor (0.75 Easy /
// 1 Normal / 1.25 Hard / 1.5 Nightmare), and a boss wave's second boss (ar_plan_boss 2) half as much. Its
// ar_tier still carries AR_BOSS_MARK exactly as it did.
float(float xHealth, float xPlanBoss) VR_ARENA_BossHealth =
{
    float h = xHealth * VR_ARENA_Opt_BossHealth() * VR_ARENA_Skill(0.75, 1.25, 1.5);
    if(xPlanBoss >= 2) { h = h / 2; }
    return max(1, floor(h) + 1);
}

// What a wave monster's ar_tier is worth on the ladder: its mark (AR_BOSS_MARK) is not a rung, so it comes off
// before the tier is scored (a boss is worth four of its rung, not four of the mark).
float(float xTier) VR_ARENA_TierOf =
{
    if(xTier >= AR_BOSS_MARK) { return xTier - AR_BOSS_MARK; }
    return xTier;
}

// The score of one kill: 100 (vr_arena_score_kill) times the ladder tier it held, a boss four of those, by the
// difficulty's factor -- risk pays, so a Nightmare kill is worth half again what an Easy one is.
float(float xTier, float xBoss) VR_ARENA_KillScore =
{
    float t = VR_ARENA_Clamp(xTier, 1, 3);
    float per = VR_ARENA_Opt_ScoreKill() * t;
    if(xBoss) { per = per * AR_KILL_BOSS; }
    return VR_ARENA_SkillWorth(per);
}

// A wave clear's bonus: 250 times the wave, by the difficulty's factor.
float(float xWave) VR_ARENA_WaveBonus = { return VR_ARENA_SkillWorth(AR_WAVE_BONUS * xWave); }

// A wave clear's medkits, and which armour it adds. The medkits are vr_arena_medkits_after divided by the factor
// (an Easy clear drops three where a Nightmare one drops one), never past the knob's own clamp, and still at least
// one wherever the knob asks for any. Which armour it adds follows the interval (vr_arena_armor_every by the
// factor: the harder the skill, the longer its gap) and the table's own wave for the heavier one (AR_W_FIEND): what
// the player has earned is the player's own progression, and the difficulty moves how often it comes, not when the
// heavier kind joins.
float(float xWave) VR_ARENA_MedkitCount = { return VR_ARENA_SkillSupply(VR_ARENA_Opt_Medkits(), 1, AR_KIT_MAX); }
float(float xWave) VR_ARENA_ArmourKind =
{
    float every = VR_ARENA_SkillGap(VR_ARENA_Opt_ArmorEvery(), AR_ARMOR_MAX);
    if(every <= 0) { return 0; }
    if(VR_ARENA_Mod(xWave, every) != 0) { return 0; }
    if(xWave < AR_W_FIEND) { return 1; }
    return VR_ARENA_Mod(xWave / every, 2) == 1 ? 2 : 1;
}

// ---------------------------------------------------------------------------
// The wave rolls: a Levy triple reseeded from vr_arena_seed and the wave, so a wave's mix of guns is the
// same every run for a seed, and different every run for vr_arena_seed 0.
// ---------------------------------------------------------------------------

void(float xWave) VR_ARENA_SeedWaves =
{
    float base = VR_ARENA_Mod(cvar("vr_arena_seed"), 30000);
    if(cvar("vr_arena_seed") <= 0) { base = 1 + floor(29998 * random()); } // (an unseeded run: its own ladder)
    base = base + 7919 * xWave;

    ar_s1 = 1 + VR_ARENA_Mod(base, 30268);
    ar_s2 = 1 + VR_ARENA_Mod(3 * base + 17, 30306);
    ar_s3 = 1 + VR_ARENA_Mod(7 * base + 29, 30322);
}

float() VR_ARENA_Rand =
{
    ar_s1 = VR_ARENA_Mod(171 * ar_s1, 30269);
    ar_s2 = VR_ARENA_Mod(172 * ar_s2, 30307);
    ar_s3 = VR_ARENA_Mod(170 * ar_s3, 30323);
    float r = ar_s1 / 30269 + ar_s2 / 30307 + ar_s3 / 30323;
    return r - floor(r);
}

// ---------------------------------------------------------------------------
// The wave's plan: who it sends, and the gun an armed humanoid holds
// ---------------------------------------------------------------------------

// What a wave monster is worth on the ladder (a classic monster's own tier, an armed humanoid's gun's tier).
float(float xKind, float xGun) VR_ARENA_WeaponTier =
{
    if(xKind == AR_SP_OGRE || xKind == AR_SP_FIEND)    { return 2; }
    if(xKind == AR_SP_HKNIGHT)                         { return 3; }
    if(xGun == WID_SUPER_SHOTGUN || xGun == WID_NAILGUN || xGun == WID_SUPER_NAILGUN) { return 2; }
    if(xGun == WID_ROCKET_LAUNCHER || xGun == WID_GRENADE_LAUNCHER || xGun == WID_LIGHTNING ||
       xGun == WID_PROXIMITY_GUN || xGun == WID_LASER_CANNON) { return 3; }
    return 1; // (its fists, the shotgun, the nail pistol, the axe)
}

float(float xTier) VR_ARENA_PickTierGun =
{
    float r = VR_ARENA_Rand();

    if(xTier == 3)
    {
        if(r < 0.34) { return WID_ROCKET_LAUNCHER; }
        if(r < 0.67) { return WID_LIGHTNING; }
        return WID_GRENADE_LAUNCHER;
    }
    if(xTier == 2)
    {
        if(r < 0.40) { return WID_SUPER_SHOTGUN; }
        if(r < 0.75) { return WID_NAILGUN; }
        return WID_SUPER_NAILGUN;
    }
    if(r < 0.50) { return WID_SHOTGUN; }
    if(r < 0.90) { return WID_NAILPISTOL; }
    return WID_AXE;
}

// A humanoid boss's gun (an enforcer boss, before the classic bosses): its own pick, from the tiers this wave has
// reached (the tiers arrive earlier the harder the skill) -- not the weapon schedule below.
float(float xWave) VR_ARENA_PickBossGun =
{
    float r = VR_ARENA_Rand();
    if(xWave >= VR_ARENA_WaveAdj(AR_W_TIER3) && r < 0.25) { return VR_ARENA_PickTierGun(3); }
    if(xWave >= VR_ARENA_WaveAdj(AR_W_TIER2) && r < 0.60) { return VR_ARENA_PickTierGun(2); }
    return VR_ARENA_PickTierGun(1);
}

// ---------------------------------------------------------------------------
// The weapon schedule: each gun of the ladder becomes eligible at its own wave (Normal's, moved by the difficulty
// as every gate: VR_ARENA_WaveAdj) and comes in as a trickle -- AR_GUN_STEP waves on 1 foe a wave, AR_GUN_STEP waves
// on 2 (VR_ARENA_GunCarriers, dealt over the wave's humanoids by VR_ARENA_PlanTrickle) -- then joins the wave's pool
// (VR_ARENA_PickGun). In the pool a newcomer starts rare and ramps up over AR_GUN_STEP * 2 waves, holds
// AR_GUN_PRIME waves, then fades by AR_GUN_FADE a wave to AR_GUN_FLOOR, so the older guns thin out as the waves go.
// The bottom rungs (shotgun, nail pistol, axe) are the pool from the first armed wave, no trickle. Bosses keep their
// own pick (VR_ARENA_PickBossGun). The table (Normal; ROUND23 "Survival Arena: the polish"):
//   row gun               eligible  on 1 foe  on 2 foes  in the pool
//    0  shotgun             base        -        -          3
//    1  nail pistol         base        -        -          3   (wave 2's own plan brings the first)
//    2  axe                 base        -        -          3   (rare)
//    3  super shotgun         3       3-4      5-6          7
//    4  nailgun               5       5-6      7-8          9
//    5  super nailgun         7       7-8      9-10        11
//    6  grenade launcher     10     10-11     12-13        14
//    7  proximity gun        12     12-13     14-15        16   (Scourge of Armagon's: only with its data)
//    8  rocket launcher      13     13-14     15-16        17
//    9  lightning gun        16     16-17     18-19        20
//   10  laser cannon         19     19-20     21-22        23   (Scourge of Armagon's: only with its data)
// ---------------------------------------------------------------------------

float AR_GUN_N     = 11;   // the table's rows
float AR_GUN_FIRST = 3;    // the schedule deals no carriers before this wave (waves 1 and 2 are their own plans)
float AR_GUN_BASE  = 3;    // the bottom rungs' pool counts its age from this wave
float AR_GUN_STEP  = 2;    // waves at each trickle step (1 carrier, then 2)
float AR_GUN_RAMP0 = 0.35; // a newcomer's pool weight on its first pool wave, times its share
float AR_GUN_PRIME = 6;    // waves in the pool before it starts to fade
float AR_GUN_FADE  = 0.08; // and how much it fades a wave after that
float AR_GUN_FLOOR = 0.2;  // never under this (a veteran gun still turns up)

float(float i) VR_ARENA_GunWid =
{
    if(i == 0) { return WID_SHOTGUN; }
    if(i == 1) { return WID_NAILPISTOL; }
    if(i == 2) { return WID_AXE; }
    if(i == 3) { return WID_SUPER_SHOTGUN; }
    if(i == 4) { return WID_NAILGUN; }
    if(i == 5) { return WID_SUPER_NAILGUN; }
    if(i == 6) { return WID_GRENADE_LAUNCHER; }
    if(i == 7) { return WID_PROXIMITY_GUN; }
    if(i == 8) { return WID_ROCKET_LAUNCHER; }
    if(i == 9) { return WID_LIGHTNING; }
    return WID_LASER_CANNON;
}

// The wave a row becomes eligible at, at Normal (0: a bottom rung, the pool's from the first armed wave).
float(float i) VR_ARENA_GunFrom =
{
    if(i <= 2) { return 0; }
    if(i == 3) { return 3; }
    if(i == 4) { return 5; }
    if(i == 5) { return 7; }
    if(i == 6) { return 10; }
    if(i == 7) { return 12; }
    if(i == 8) { return 13; }
    if(i == 9) { return 16; }
    return 19;
}

// A row's share of the pool at its full weight (the bottom rungs split one share as the early waves always did).
float(float i) VR_ARENA_GunShare =
{
    if(i == 0) { return 0.6; }
    if(i == 1) { return 0.3; }
    if(i == 2) { return 0.1; }
    if(i == 3 || i == 4) { return 0.5; }
    if(i == 7) { return 0.5; }
    return 1;
}

// A row this game can arm a humanoid with (the mission packs' guns only with their data: VR_AG_Usable).
float(float i) VR_ARENA_GunOk = { return VR_AG_Usable(VR_ARENA_GunWid(i)); }

// The wave a row is eligible from, as this difficulty counts it (never before the schedule's first wave).
float(float i) VR_ARENA_GunEligible =
{
    float from = VR_ARENA_GunFrom(i);
    if(from <= 0) { return AR_GUN_FIRST; }
    return max(AR_GUN_FIRST, VR_ARENA_WaveAdj(from));
}

// The wave a row joins the pool at: a bottom rung at once, a newcomer after its two trickle steps.
float(float i) VR_ARENA_GunPoolFrom =
{
    if(VR_ARENA_GunFrom(i) <= 0) { return AR_GUN_FIRST; }
    return VR_ARENA_GunEligible(i) + 2 * AR_GUN_STEP;
}

// How many of this wave's humanoids carry a row's gun on its trickle (0: not eligible yet, or in the pool now).
float(float i, float xWave) VR_ARENA_GunCarriers =
{
    if(VR_ARENA_GunFrom(i) <= 0 || xWave < AR_GUN_FIRST || !VR_ARENA_GunOk(i)) { return 0; }
    float d = xWave - VR_ARENA_GunEligible(i);
    if(d < 0) { return 0; }
    if(d < AR_GUN_STEP) { return 1; }
    if(d < 2 * AR_GUN_STEP) { return 2; }
    return 0;
}

// A row's weight in this wave's pool (0: not in it).
float(float i, float xWave) VR_ARENA_GunWeight =
{
    if(!VR_ARENA_GunOk(i)) { return 0; }
    float from = VR_ARENA_GunPoolFrom(i);
    if(xWave < from) { return 0; }
    float a = xWave - from;
    float ramp = 1;
    if(VR_ARENA_GunFrom(i) > 0) { ramp = min(1, AR_GUN_RAMP0 + (1 - AR_GUN_RAMP0) * a / (2 * AR_GUN_STEP)); }
    float fade = max(AR_GUN_FLOOR, 1 - AR_GUN_FADE * max(0, a - AR_GUN_PRIME));
    return VR_ARENA_GunShare(i) * ramp * fade;
}

// A wave's humanoid's gun, drawn from the wave's pool by the rows' weights.
float(float xWave) VR_ARENA_PickGun =
{
    float i;
    float sum = 0;
    for(i = 0; i < AR_GUN_N; i++) { sum = sum + VR_ARENA_GunWeight(i, xWave); }
    float r = VR_ARENA_Rand() * sum;
    for(i = 0; i < AR_GUN_N; i++)
    {
        float w = VR_ARENA_GunWeight(i, xWave);
        if(w > 0 && r < w) { return VR_ARENA_GunWid(i); }
        r = r - w;
    }
    return WID_SHOTGUN; // (the sum's own rounding)
}

// The first waves' humanoids: still some of them unarmed, and only the ladder's bottom rungs. The unarmed share is
// wider at a softer difficulty, and reaches further into the early waves there (VR_ARENA_EarlyUnarmed,
// VR_ARENA_WaveAdj).
float(float xWave) VR_ARENA_PickEarlyGun =
{
    float r = VR_ARENA_Rand();
    if(xWave <= VR_ARENA_WaveAdj(4) && r < VR_ARENA_EarlyUnarmed()) { return AR_FIST; }
    return VR_ARENA_PickGun(xWave);
}

float(float xWave) VR_ARENA_IsBossWave =
{
    float every = VR_ARENA_BossEvery(); // (vr_arena_boss_every, a wave longer at Easy)
    if(every <= 0) { return FALSE; }
    return VR_ARENA_Mod(xWave, every) == 0;
}

float(float xWave) VR_ARENA_BossKind =
{
    if(xWave < VR_ARENA_WaveAdj(AR_W_TIER2)) { return AR_SP_ENFORCER; } // (a boss with a gun of its own before the classic bosses)
    if(xWave < VR_ARENA_WaveAdj(AR_W_HKNIGHT)) { return AR_SP_OGRE; }
    return AR_SP_HKNIGHT;
}

// Which of Quake's own monsters this wave sends, cycling the kinds unlocked by now (never one before its wave --
// and the wave it joins at is the difficulty's, not the table's).
float(float xWave, float i) VR_ARENA_ClassicKind =
{
    float k = VR_ARENA_Mod(i, 4);
    if(k == 0) { return AR_SP_ZOMBIE; }
    if(k == 1) { return xWave >= VR_ARENA_WaveAdj(AR_W_DOG) ? AR_SP_DOG : AR_SP_ZOMBIE; }
    if(k == 2)
    {
        if(xWave >= VR_ARENA_WaveAdj(AR_W_OGRE))   { return AR_SP_OGRE; }
        if(xWave >= VR_ARENA_WaveAdj(AR_W_KNIGHT)) { return AR_SP_KNIGHT; }
        return AR_SP_ZOMBIE;
    }
    if(xWave >= VR_ARENA_WaveAdj(AR_W_HKNIGHT)) { return AR_SP_HKNIGHT; }
    if(xWave >= VR_ARENA_WaveAdj(AR_W_FIEND))   { return AR_SP_FIEND; }
    if(xWave >= VR_ARENA_WaveAdj(AR_W_KNIGHT))  { return AR_SP_KNIGHT; }
    return AR_SP_ZOMBIE;
}

void(float xKind, float xGun, float xBoss) VR_ARENA_PlanAdd =
{
    if(ar_plan_n >= AR_PLAN_MAX) { return; } // (a wave is planned no thicker than this)
    ar_plan_kind[ar_plan_n] = xKind;
    ar_plan_gun[ar_plan_n]   = xGun;
    ar_plan_boss[ar_plan_n]  = xBoss;
    ar_plan_tier[ar_plan_n]  = VR_ARENA_WeaponTier(xKind, xGun);
    ar_plan_armour[ar_plan_n] = 0; // (VR_ARENA_PlanArmour dresses the humanoids once the plan is written)
    ar_plan_amask[ar_plan_n]  = 0;
    ar_plan_n = ar_plan_n + 1;
}

// ---------------------------------------------------------------------------
// The humanoids' armour (vr_agarmor.qc's plates): the arena's own roll, in place of the skill's
// (vr_agarmor_skill*), for its wave foes only. Every number below rises with the wave and the difficulty:
//   - none before vr_arena_foe_armor_from (4) as the difficulty counts waves (VR_ARENA_WaveAdj: 7 / 4 / 2 / 2);
//   - the chance AR_ARM_BASE (10%) at that wave, AR_ARM_STEP (3%) more each wave after, times the difficulty's
//     factor and vr_arena_foe_armor_scale, never past AR_ARM_CAP (85%); a boss wave's humanoids AR_ARM_GUARD more;
//   - the best type: green until the second rung of guns has been in the foes' hands two waves (AR_W_TIER2 + 2,
//     wave 12 at Normal: the player can pick up a super shotgun or a nailgun by then), yellow from there, red from
//     AR_W_TIER3 + 4 (wave 22 at Normal: the launchers have been about four waves); a foe's type 1..best at random;
//   - the plates: the cuirass and the helmet, one more limb plate every AR_ARM_PIECE_W (3) waves by the factor;
//   - a humanoid boss: always armoured from the armour's first wave, a type above the wave's best and
//     AR_ARM_BOSS_PCS (4) more plates.
// The rolls are their own stream (the wave's seed, salted), so they never move the wave's guns or its gaps.
// ---------------------------------------------------------------------------

float AR_ARM_SALT = 1000; // the armour stream's offset in VR_ARENA_SeedWaves' waves
float ar_arm_seq;         // the wave's armour rolls so far, and their offset (VR_ARENA_ArmourSeed)
float ar_arm_off;

// The wave's armour rolls start: the salted stream (its plates), and the chance and the type off a low-discrepancy
// pair (R2: the golden and the plastic ratio's steps) from an offset that stream rolls -- so a wave's armoured share
// is the table's share, with no streaks, and the type is spread evenly over the armoured ones.
void(float xWave) VR_ARENA_ArmourSeed =
{
    VR_ARENA_SeedWaves(xWave + AR_ARM_SALT);
    ar_arm_off = VR_ARENA_Rand();
    ar_arm_seq = 0;
}

float() VR_ARENA_Opt_ArmourScale { return VR_ARENA_Clamp(cvar("vr_arena_foe_armor_scale"), 0, 4); }
float() VR_ARENA_Opt_ArmourFrom  { return VR_ARENA_Clamp(cvar("vr_arena_foe_armor_from"), 2, 60); }
float() VR_ARENA_Opt_FoeArmour   { return cvar("vr_arena_foe_armor") != 0 && VR_ARENA_Opt_ArmourScale() > 0; }

// The first wave a foe may wear armour, as this difficulty counts waves.
float() VR_ARENA_ArmourFrom = { return VR_ARENA_WaveAdj(VR_ARENA_Opt_ArmourFrom()); }

// A wave's humanoid's chance to be armoured (0 before the first armour wave, or with the armour off).
float(float xWave) VR_ARENA_ArmourChance =
{
    if(!VR_ARENA_Opt_FoeArmour()) { return 0; }
    float from = VR_ARENA_ArmourFrom();
    if(xWave < from) { return 0; }
    float c = (AR_ARM_BASE + AR_ARM_STEP * (xWave - from)) * VR_ARENA_Skill(0.75, 1.25, 1.5) * VR_ARENA_Opt_ArmourScale();
    return VR_ARENA_Clamp(c, 0, AR_ARM_CAP);
}

// The best type a wave's armour may be (1 green, 2 yellow, 3 red), gated on the weapon ladder's rungs.
float(float xWave) VR_ARENA_ArmourBest =
{
    if(xWave >= VR_ARENA_WaveAdj(AR_W_TIER3 + 4)) { return 3; }
    if(xWave >= VR_ARENA_WaveAdj(AR_W_TIER2 + 2)) { return 2; }
    return 1;
}

// How many plates a wave's armour has: 2 (the cuirass and the helmet), one more every AR_ARM_PIECE_W waves past the
// first armour wave by the difficulty's factor, at most all ten.
float(float xWave) VR_ARENA_ArmourPieces =
{
    float past = max(0, xWave - VR_ARENA_ArmourFrom());
    return VR_ARENA_Clamp(2 + floor(past * VR_ARENA_Skill(0.75, 1.25, 1.5) / AR_ARM_PIECE_W), 2, 10);
}

// One foe's roll, off the wave stream as it stands (ar_arm_type 0: bare). xBoss: one of the wave's marked big ones;
// xBossWave: a boss wave (its guard's chance is higher).
void(float xWave, float xBoss, float xBossWave) VR_ARENA_ArmourRoll =
{
    float c = VR_ARENA_ArmourChance(xWave);
    float best = VR_ARENA_ArmourBest(xWave);
    float n = VR_ARENA_ArmourPieces(xWave);
    float i;

    ar_arm_type = 0;
    ar_arm_mask = 0;
    if(c <= 0) { return; }
    if(xBoss)
    {
        ar_arm_type = min(3, best + 1);
        n = min(10, n + AR_ARM_BOSS_PCS);
    }
    else
    {
        if(xBossWave) { c = VR_ARENA_Clamp(c + AR_ARM_GUARD * VR_ARENA_Opt_ArmourScale(), 0, AR_ARM_CAP); }
        ar_arm_seq = ar_arm_seq + 1;
        float r = ar_arm_off + ar_arm_seq * 0.7548776662;
        float rt = ar_arm_off + ar_arm_seq * 0.5698402910;
        if(r - floor(r) >= c) { return; }
        ar_arm_type = 1 + min(best - 1, floor((rt - floor(rt)) * best));
    }

    // The cuirass and the helmet (vr_agarmor.qc's pieces 0 and 1), then n - 2 of the eight limb plates (2..9), each
    // taken with the chance still needed over the ones still to go: exactly n - 2 of them.
    ar_arm_mask = 3;
    float need = n - 2;
    for(i = 2; i < 10 && need > 0; i++)
    {
        if(VR_ARENA_Rand() * (10 - i) < need)
        {
            ar_arm_mask = ar_arm_mask | VR_AgArmor_Bit(i);
            need = need - 1;
        }
    }
}

// The plan's humanoids dressed, once the plan is written: the wave's own stream (salted), so the plan's guns and the
// wave's gaps roll exactly as they do with the armour off; the wave stream is put back after.
void(float xWave) VR_ARENA_PlanArmour =
{
    float s1 = ar_s1;
    float s2 = ar_s2;
    float s3 = ar_s3;
    float bw = VR_ARENA_IsBossWave(xWave);
    float i;

    if(VR_ARENA_ArmourChance(xWave) <= 0) { return; }
    VR_ARENA_ArmourSeed(xWave);
    for(i = 0; i < ar_plan_n; i++)
    {
        if(!VR_ARENA_ArmsHumanoid(ar_plan_kind[i])) { continue; }
        VR_ARENA_ArmourRoll(xWave, ar_plan_boss[i] > 0, bw);
        ar_plan_armour[i] = ar_arm_type;
        ar_plan_amask[i]  = ar_arm_mask;
    }
    ar_s1 = s1;
    ar_s2 = s2;
    ar_s3 = s3;
}

// An armoured kill's score: AR_ARM_PAY more per type (whole points).
float(float xScore, float xType) VR_ARENA_ArmourWorth =
{
    return VR_ARENA_SkillWhole(xScore * (1 + AR_ARM_PAY * VR_ARENA_Clamp(xType, 0, 3)));
}

// The wave's plan, wave 1 to the waves past the table (13 and on: the same ladder, thicker).
//
// The counts written below are the wave's Normal-skill counts at vr_arena_wave_size 1: VR_ARENA_WaveCount scales
// every crowd group by the wave size, then VR_ARENA_SkillCount moves it by the skill's factor (whole foes, a group never emptied -- a wave's own zombie, dog or boss stays). The cap on a
// wave's whole plan is VR_ARENA_PlanAdd's (AR_PLAN_MAX), the cap on the field is VR_ARENA_FieldCap's.
void(float xWave) VR_ARENA_PlanFoes =
{
    float boss = VR_ARENA_IsBossWave(xWave);
    float n;
    float i;
    float bk;
    float nb = 1; // (a boss wave's marked big ones: one, two at Nightmare)

    ar_plan_n = 0;
    VR_ARENA_SeedWaves(xWave);

    if(boss)
    {
        // Its big ones first, so a thick wave's plan never cuts them out. Nightmare brings two of them, the
        // second the same kind at half its bulk (ar_plan_boss 2), still under VR_ARENA_PlanAdd's cap.
        bk = VR_ARENA_BossKind(xWave);
        nb = VR_ARENA_BossTwice() ? 2 : 1;
        for(i = 0; i < nb; i++)
        {
            VR_ARENA_PlanAdd(bk, bk == AR_SP_ENFORCER ? VR_ARENA_PickBossGun(xWave) : 0, 1 + i);
        }
    }

    if(xWave == 1)
    {
        n = VR_ARENA_WaveCount(1 + (VR_ARENA_Rand() < 0.5)); // (one or two grunts, all of them unarmed)
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, AR_FIST, 0); }
        return;
    }
    if(xWave == 2)
    {
        // One unarmed and one with a nail pistol; a thinner wave brings the armed one, so the ladder's step stays.
        n = VR_ARENA_WaveCount(2);
        if(n < 2)
        {
            VR_ARENA_PlanAdd(AR_SP_GRUNT, WID_NAILPISTOL, 0);
        }
        else
        {
            VR_ARENA_PlanAdd(AR_SP_GRUNT, AR_FIST, 0);
            for(i = 1; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, WID_NAILPISTOL, 0); }
        }
        return;
    }
    if(xWave == 3)
    {
        n = VR_ARENA_WaveCount(3 + (VR_ARENA_Rand() < 0.5));
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickEarlyGun(3), 0); }
        VR_ARENA_PlanAdd(AR_SP_ZOMBIE, 0, 0);
        return;
    }
    if(xWave == 4)
    {
        n = VR_ARENA_WaveCount(4);
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickEarlyGun(4), 0); }
        VR_ARENA_PlanAdd(AR_SP_KNIGHT, 0, 0);
        return;
    }
    if(xWave == 5) // (its boss, an enforcer with a gun, was planned above)
    {
        n = VR_ARENA_WaveCount(3 + (VR_ARENA_Rand() < 0.5));
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(5), 0); }
        return;
    }
    if(xWave == 6)
    {
        n = VR_ARENA_WaveCount(3);
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(6), 0); }
        VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(6), 0);
        VR_ARENA_PlanAdd(AR_SP_DOG, 0, 0);
        return;
    }
    if(xWave == 7)
    {
        n = VR_ARENA_WaveCount(3 + (VR_ARENA_Rand() < 0.4));
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(7), 0); }
        VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(7), 0);
        VR_ARENA_PlanAdd(AR_SP_DOG, 0, 0);
        return;
    }
    if(xWave == 8)
    {
        n = VR_ARENA_WaveCount(3);
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(8), 0); }
        VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(8), 0);
        VR_ARENA_PlanAdd(AR_SP_KNIGHT, 0, 0);
        VR_ARENA_PlanAdd(AR_SP_OGRE, 0, 0);
        return;
    }
    if(xWave == 9)
    {
        n = VR_ARENA_WaveCount(4);
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(9), 0); }
        VR_ARENA_PlanAdd(AR_SP_ZOMBIE, 0, 0);
        VR_ARENA_PlanAdd(AR_SP_OGRE, 0, 0);
        return;
    }
    if(xWave == 10) // (its boss, an ogre, was planned above)
    {
        n = VR_ARENA_WaveCount(2);
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(10), 0); }
        n = VR_ARENA_WaveCount(2);
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(10), 0); }
        VR_ARENA_PlanAdd(AR_SP_DOG, 0, 0);
        return;
    }
    if(xWave == 11)
    {
        n = VR_ARENA_WaveCount(4);
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(11), 0); }
        VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(11), 0);
        n = VR_ARENA_WaveCount(2);
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_OGRE, 0, 0); }
        return;
    }
    if(xWave == 12)
    {
        n = VR_ARENA_WaveCount(4);
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(12), 0); }
        VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(12), 0);
        VR_ARENA_PlanAdd(AR_SP_FIEND, 0, 0);
        VR_ARENA_PlanAdd(AR_SP_KNIGHT, 0, 0);
        return;
    }

    // 13 and on: the same ladder, thicker, a boss every vr_arena_boss_every waves (a wave longer at Easy). Its boss
    // or bosses take a place, as they do at Normal. Where the ladder starts thickening stays wave 13 at every skill
    // -- how thick a wave there is VR_ARENA_SkillCount's business (its own gates, the guns and the species it draws,
    // are the difficulty's: VR_ARENA_WaveAdj, inside VR_ARENA_PickGun and VR_ARENA_ClassicKind).
    n = VR_ARENA_WaveCount(VR_ARENA_Clamp(4 + floor((xWave - 13) / 4), 1, 7));
    if(boss) { n = max(1, n - nb); } // (its boss or bosses take a place)
    for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(xWave), 0); }

    float nenf = VR_ARENA_WaveCount(VR_ARENA_Clamp(1 + floor((xWave - 13) / 8), 1, 3));
    for(i = 0; i < nenf; i++) { VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(xWave), 0); }

    float nclass = VR_ARENA_WaveCount(VR_ARENA_Clamp(1 + floor((xWave - 13) / 8), 1, 3));
    for(i = 0; i < nclass; i++) { VR_ARENA_PlanAdd(VR_ARENA_ClassicKind(xWave, i), 0, 0); }
}

// The weapon schedule's trickle (the table above VR_ARENA_PickGun): each row on its trickle this wave is dealt to
// its count of the wave's humanoids (not its bosses), at random among them, the newest row first, so a thin wave
// still shows its newest gun. The rest keep the pool's draw.
float ar_plan_dealt[AR_PLAN_MAX]; // 1: its gun is the trickle's
float ar_trickle_n;               // this wave's trickle carriers

void(float xWave) VR_ARENA_PlanTrickle =
{
    float i;
    float j;
    float free = 0;
    for(j = 0; j < ar_plan_n; j++)
    {
        ar_plan_dealt[j] = 0;
        if(VR_ARENA_ArmsHumanoid(ar_plan_kind[j]) && !ar_plan_boss[j]) { free = free + 1; }
    }
    ar_trickle_n = 0;
    for(i = AR_GUN_N - 1; i >= 0 && free > 0; i--)
    {
        float c = VR_ARENA_GunCarriers(i, xWave);
        while(c > 0 && free > 0)
        {
            float pick = min(free - 1, floor(VR_ARENA_Rand() * free)); // (the pick'th humanoid still free)
            for(j = 0; j < ar_plan_n; j++)
            {
                if(!VR_ARENA_ArmsHumanoid(ar_plan_kind[j]) || ar_plan_boss[j] || ar_plan_dealt[j]) { continue; }
                if(pick <= 0) { break; }
                pick = pick - 1;
            }
            float wid = VR_ARENA_GunWid(i);
            ar_plan_gun[j] = wid;
            ar_plan_tier[j] = VR_ARENA_WeaponTier(ar_plan_kind[j], wid);
            ar_plan_dealt[j] = 1;
            free = free - 1;
            c = c - 1;
            ar_trickle_n = ar_trickle_n + 1;
        }
    }
}

// The wave's plan: who it sends (VR_ARENA_PlanFoes), the schedule's newest guns dealt over its humanoids
// (VR_ARENA_PlanTrickle), then what armour its humanoids wear (VR_ARENA_PlanArmour).
void(float xWave) VR_ARENA_Plan =
{
    VR_ARENA_PlanFoes(xWave);
    VR_ARENA_PlanTrickle(xWave);
    VR_ARENA_PlanArmour(xWave);
}

// ---------------------------------------------------------------------------
// The players, the readout, the map's spots
// ---------------------------------------------------------------------------

entity() VR_ARENA_FindPlayer =
{
    entity p = find(world, classname, "player");
    while(p)
    {
        if(p.health > 0 && !p.deadflag) { return p; }
        p = find(p, classname, "player");
    }
    return world;
}

float() VR_ARENA_AnyPlayerUp = { return VR_ARENA_FindPlayer() != world; }

// The arena's readout: in the middle of the view (the engine's view message: eye level in the headset, not the wrist's
// hologram; a centre print held that long on a flat screen), its first line (up to a blank line) highlighted, for
// xSecs. One message shows at a time: a newer one replaces it. The console's log keeps a copy (developer).
float AR_SAY = 4; // s an arena line holds in the view

void(entity xPlayer, string xText, float xSecs) VR_ARENA_Show =
{
    viewmessage(xPlayer, xText, xSecs);
    dprint(sprintf("ARENA> %s", xText));
}

void(string xText, float xSecs) VR_ARENA_ShowAll =
{
    entity p = find(world, classname, "player");
    while(p) { viewmessage(p, xText, xSecs); p = find(p, classname, "player"); }
    dprint(sprintf("ARENA> %s", xText));
}

void(string xText) VR_ARENA_PrintAll = { VR_ARENA_ShowAll(xText, AR_SAY); }

// The map's spots of a class (info_arena_spawn, info_arena_item), round robin; none: world.
entity(string xClassname, float xIndex) VR_ARENA_MapSpot =
{
    entity e = find(world, classname, xClassname);
    float total = 0;
    while(e) { total = total + 1; e = find(e, classname, xClassname); }
    if(!total) { return world; }

    float want = VR_ARENA_Mod(xIndex, total);
    e = find(world, classname, xClassname);
    while(want > 0) { e = find(e, classname, xClassname); want = want - 1; }
    return e;
}

// How many of the map's spawn places are of a kind ("kind" 1 the floor, 2 the high places).
float(float xWant) VR_ARENA_SpotCount =
{
    float total = 0;
    entity e = find(world, classname, "info_arena_spawn");
    while(e)
    {
        if(e.kind == xWant) { total = total + 1; }
        e = find(e, classname, "info_arena_spawn");
    }
    return total;
}

// The xIndex'th place of a kind: world when the map has no more of it (its places are used up, not walked twice).
entity(float xWant, float xIndex) VR_ARENA_SpawnSpot =
{
    float total = VR_ARENA_SpotCount(xWant);
    if(!total || xIndex >= total) { return world; }

    entity e = find(world, classname, "info_arena_spawn");
    while(e)
    {
        if(e.kind == xWant)
        {
            if(xIndex <= 0) { return e; }
            xIndex = xIndex - 1;
        }
        e = find(e, classname, "info_arena_spawn");
    }
    return world;
}

// A ring around the player, for a map with no info_arena_spawn / info_arena_item of its own.
vector(float i) VR_ARENA_RingDir =
{
    float k = VR_ARENA_Mod(i, AR_RING_MAX);
    if(k == 0) { return '1 0 0'; }
    if(k == 1) { return '-0.5 0.87 0'; }
    if(k == 2) { return '-1 0 0'; }
    if(k == 3) { return '-0.5 -0.87 0'; }
    if(k == 4) { return '0.5 -0.87 0'; }
    if(k == 5) { return '0.5 0.87 0'; }
    if(k == 6) { return '0.7 0.7 0'; }
    return '-0.7 -0.7 0';
}

// A spot used twice in one wave is stepped aside, so two foes never spawn inside one another. The first is the
// place as the map drew it; then the four diagonals a little over a monster's box (its own box is 32 wide) off it.
vector(float i) VR_ARENA_Aside =
{
    float k = VR_ARENA_Mod(i, AR_ASIDE_MAX);
    if(k == 0) { return '0 0 0'; }
    if(k == 1) { return AR_ASIDE_STEP * '1 1 0'; }
    if(k == 2) { return AR_ASIDE_STEP * '-1 1 0'; }
    if(k == 3) { return AR_ASIDE_STEP * '1 -1 0'; }
    return AR_ASIDE_STEP * '-1 -1 0';
}

// A place already holding one of this wave's foes is stepped aside. The step is small on purpose (a monster's own
// box is 32 wide): from a tower top, a wall walk or the octagon's cut corner it stays on the ground it belongs to.
// ---------------------------------------------------------------------------
// No overlaps: a box at a place is clear of everything standing in the world -- monsters (any wave's, the map's),
// the players, items, guns lying about, props, pedestals: anything with a model that is not a brush (the world's,
// a door's, a trigger's). A fallen body (SOLID_NOT, which findradius passes over) is not in the way.
// ---------------------------------------------------------------------------

float AR_CLEAR_R   = 160; // how far round a box its blockers are looked for (a big hull's corner, a long gun)
float AR_CLEAR_PAD = 8;   // a box is this much wider every way than what it holds
float ar_place_big;       // the foe being placed has the big hull (an ogre, a fiend, a death knight: 64 wide)
float ar_place_drop;      // and drops this far onto its place (a high place's 96: the column under it must be clear)
float ar_place_ok;        // VR_ARENA_SpawnPlace found a free place (FALSE: none this beat; the foe waits a beat)
float ar_overlaps;        // the run's spawns that came in overlapping something (the tests read it; must stay 0)
float ar_spawn_scans;     // and how many spawns were scanned for it

// A thing's arrival, made obvious: the teleport's flash (TE_TELEPORT, every client) and one of its five sounds, at
// the place, as it comes in (triggers.qc's spawn_tfog, without its 0.2 s wait for the sound).
void(vector xAt) VR_ARENA_Tfog =
{
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_TELEPORT);
    WriteVec3(MSG_BROADCAST, xAt);

    entity s = spawn();
    setorigin(s, xAt);
    s.think = play_teleport; // (its sound, then it goes)
    s.nextthink = time + 0.01;
}

float(entity e) VR_ARENA_Blocks =
{
    return e.modelindex && e.solid != SOLID_BSP && substring(e.model, 0, 1) != "*";
}

// The first blocker in the box xLo..xHi (world: none), not counting xSkip.
entity(vector xLo, vector xHi, entity xSkip) VR_ARENA_BoxBlocker =
{
    entity e = findradius(0.5 * (xLo + xHi), AR_CLEAR_R + 0.5 * vlen(xHi - xLo));
    while(e)
    {
        if(e != xSkip && (!xSkip || (e.owner != xSkip && e != xSkip.vr_ag_prop)) && VR_ARENA_Blocks(e) &&
           e.absmin_x < xHi_x && e.absmax_x > xLo_x && e.absmin_y < xHi_y && e.absmax_y > xLo_y &&
           e.absmin_z < xHi_z && e.absmax_z > xLo_z)
        {
            return e;
        }
        e = e.chain;
    }
    return world;
}

// A foe's box at a place (its origin there, 32 over the place, as VR_ARENA_SpawnMonster sets it), padded, and the
// column it drops down when it comes in over a high place.
float(vector o) VR_ARENA_FoeBoxClear =
{
    vector at = o + '0 0 32';
    vector lo = at + (ar_place_big ? '-32 -32 -24' : '-16 -16 -24') - AR_CLEAR_PAD * '1 1 1' - ar_place_drop * '0 0 1';
    vector hi = at + (ar_place_big ? '32 32 64' : '16 16 40') + AR_CLEAR_PAD * '1 1 1';
    return VR_ARENA_BoxBlocker(lo, hi, world) == world;
}

// The spawn scan (the tests' check, and a log line should it ever happen): does a thing just put in the field
// overlap anything else's box.
void(entity xNew, string xWhat) VR_ARENA_ScanSpawn =
{
    ar_spawn_scans = ar_spawn_scans + 1;
    entity b = VR_ARENA_BoxBlocker(xNew.absmin + '2 2 2', xNew.absmax - '2 2 2', xNew); // (the engine links boxes 1 wider every way: these are the true ones)
    if(b == world) { return; }
    ar_overlaps = ar_overlaps + 1;
    dprint(sprintf("arena: overlap %s at %s (%s..%s) with %s", xWhat, vtos(xNew.origin), vtos(xNew.absmin),
                   vtos(xNew.absmax), b.classname));
    dprint(sprintf(" at %s (%s..%s, solid %g, health %g)\n", vtos(b.origin), vtos(b.absmin), vtos(b.absmax), b.solid,
                   b.health));
}

float(vector o) VR_ARENA_PlaceTaken =
{
    entity e = findflags(world, ar_slot, ar_wave);
    while(e)
    {
        if(e.health > 0)
        {
            vector d = e.origin - o;
            d_z = 0; // (apart on the ground it stands on: a foe on the floor below is not in this place)
            if(vlen(d) < AR_PLACE_GAP) { return 1; }
        }
        e = findflags(e, ar_slot, ar_wave);
    }
    return !VR_ARENA_FoeBoxClear(o); // (anything else standing there: a foe of an earlier wave, the player, a gun)
}

entity(float xIndex) VR_ARENA_ItemSpot { return VR_ARENA_MapSpot("info_arena_item", xIndex); }

// Is there ground under a place, read off the brushes: 600 units down, this wave's own foes not counted. A place
// inside a wall counts as having none (the trace starts inside solid), and a place with nothing under it is a foe
// falling out of the map.
float(vector o) VR_ARENA_HasFloor =
{
    traceline(o + '0 0 4', o + '0 0 -600', TRUE, world);
    return !trace_allsolid && !trace_startsolid && trace_fraction < 1;
}

// How a place sits for the player: 0 out of his sight (behind him, off to the side, or a wall between) and not
// near, 1 in his sight, 2 near him (AR_NEAR off his eyes). No player: 0.
float(vector o) VR_ARENA_PlaceSeen =
{
    entity p = VR_ARENA_FindPlayer();
    if(p == world) { return 0; }

    vector eye = p.origin + p.view_ofs;
    vector at = o + '0 0 24'; // (about a foe's chest)
    vector d = at - eye;
    if(vlen(d) < AR_NEAR) { return 2; }

    makevectors(p.v_angle);
    if(normalize(d) * v_forward < AR_VIEW_COS) { return 0; }
    traceline(eye, at, TRUE, p);
    return trace_fraction < 1 ? 0 : 1;
}

// Where this wave's foe comes in. Its own kind of place first: the map's high places (kind 2: a tower top, a wall
// walk) for the fiend and the enforcer, its floor places (kind 1) for everything that walks. The places are gone
// through in order from where this wave starts and each one is used at most once, so 12 foes do not arrive in 4
// corners; a place already holding a foe of this wave is passed over rather than shared. A high foe whose places are
// all taken comes in on the floor places instead of being stepped off its ledge, and a map with no places of its own
// gets the ring around the player, stepped aside until it is free and never over a hole or into a wall.
vector(float xHigh) VR_ARENA_SpawnPlace =
{
    float want = xHigh ? 2 : 1;
    float total = VR_ARENA_SpotCount(want);

    float used;
    if(xHigh) { used = ar_used_high; ar_used_high = ar_used_high + 1; }
    else { used = ar_used_ground; ar_used_ground = ar_used_ground + 1; }

    // Three passes over the places: first one out of the player's sight and not near him, then one not near him,
    // then any free one (a small map, a player standing in the middle of its places).
    float k;
    float pass;
    for(pass = 0; pass < 3; pass++)
    {
        for(k = 0; k < total; k++)
        {
            entity e = VR_ARENA_SpawnSpot(want, VR_ARENA_Mod(used + ar_wave + k, total));
            if(!e) { break; } // (out of this kind of place: the ring below)

            vector here = e.origin;
            ar_place_drop = 0;
            if(xHigh && e.kind == 2) { here_z = here_z + 96; ar_place_drop = 96; } // (over a high place: it drops onto the ledge it belongs to)
            if(VR_ARENA_PlaceTaken(here)) { continue; }
            float seen = VR_ARENA_PlaceSeen(here);
            if((pass == 0 && seen != 0) || (pass == 1 && seen == 2)) { continue; }
            ar_spot_next = ar_spot_next + 1;
            ar_place_ok = TRUE;
            return here;
        }
    }

    // Every high place is holding one of this wave's foes already: better its ground places than a step off a ledge.
    if(xHigh && VR_ARENA_SpotCount(1) > 0) { return VR_ARENA_SpawnPlace(0); }

    entity p = VR_ARENA_FindPlayer();
    vector base = ar_mgr.origin;
    if(p != world) { base = p.origin; }
    vector ring = base + AR_RING_R * VR_ARENA_RingDir(ar_spot_next);

    float k2;
    vector o = ring;
    ar_place_drop = 0;
    float ground = FALSE; // (any place with ground under it: the last resort)
    float free = FALSE;   // (a place with ground under it and no foe of this wave on it)
    float pass2;          // (the first pass wants one out of the player's sight as well)
    for(pass2 = 0; pass2 < 2 && !free; pass2++)
    {
        for(k2 = 0; k2 < AR_RING_MAX && !free; k2++)
        {
            vector r = (k2 == 0) ? ring : base + AR_RING_R * VR_ARENA_RingDir(ar_spot_next + k2);
            float t;
            for(t = 0; t < AR_ASIDE_MAX; t++)
            {
                vector c = r + VR_ARENA_Aside(t);
                if(!VR_ARENA_HasFloor(c)) { continue; }
                if(!ground) { ground = TRUE; o = c; }
                if(VR_ARENA_PlaceTaken(c)) { continue; }
                if(pass2 == 0 && VR_ARENA_PlaceSeen(c) != 0) { continue; }
                o = c; free = TRUE; break;
            }
        }
    }

    ar_spot_next = ar_spot_next + 1;
    ar_place_ok = free; // (no free place anywhere: the foe waits for a later beat rather than come in on another)
    return o;
}

// A drop's (a medkit's, an armour's, a pedestal's) spacing: its place as given, then a step of AR_DROP_STEP off it
// each of eight ways, then two steps -- the first with ground under it and its box clear (a medkit's corner-origin
// box and an armour's centred one both inside it, padded). FALSE in ar_place_ok when there is none.
float AR_DROP_STEP = 64;
float AR_DROP_TRIES = 17;

vector(float k) VR_ARENA_DropAside =
{
    if(k <= 0) { return '0 0 0'; }
    float r = k <= 8 ? AR_DROP_STEP : 2 * AR_DROP_STEP;
    float j = VR_ARENA_Mod(k - 1, 8);
    vector d = '1 0 0';
    if(j == 1) { d = '0 1 0'; }
    if(j == 2) { d = '-1 0 0'; }
    if(j == 3) { d = '0 -1 0'; }
    if(j == 4) { d = '0.7071 0.7071 0'; }
    if(j == 5) { d = '-0.7071 0.7071 0'; }
    if(j == 6) { d = '-0.7071 -0.7071 0'; }
    if(j == 7) { d = '0.7071 -0.7071 0'; }
    return r * d;
}

vector(vector xAt) VR_ARENA_FreeNear =
{
    float k;
    for(k = 0; k < AR_DROP_TRIES; k++)
    {
        vector c = xAt + VR_ARENA_DropAside(k);
        if(k > 0 && !VR_ARENA_HasFloor(c)) { continue; }
        vector lo = c + '-16 -16 8' - AR_CLEAR_PAD * '1 1 1';
        vector hi = c + '32 32 64' + AR_CLEAR_PAD * '1 1 1';
        if(VR_ARENA_BoxBlocker(lo, hi, world) != world) { continue; }
        ar_place_ok = TRUE;
        return c;
    }
    ar_place_ok = FALSE;
    return xAt;
}

// Where a wave clear's medkit or armour lands: the map's info_arena_item, or the ring off the player.
vector(float i) VR_ARENA_DropPlace =
{
    entity e = VR_ARENA_ItemSpot(ar_item_next);
    vector o;

    if(e)
    {
        // The next item point with room on it (or a step off it): an earlier clear's medkit still lying there, the
        // player standing on it, a gun fallen on it are stepped round, never landed in.
        float total = 0;
        entity s = find(world, classname, "info_arena_item");
        while(s) { total = total + 1; s = find(s, classname, "info_arena_item"); }
        float t;
        for(t = 0; t < total; t++)
        {
            o = VR_ARENA_FreeNear(VR_ARENA_ItemSpot(ar_item_next + t).origin);
            if(ar_place_ok) { break; }
        }
        ar_item_next = ar_item_next + 1;
        return o;
    }
    else
    {
        entity p = VR_ARENA_FindPlayer();
        vector base = ar_mgr.origin;
        if(p != world) { base = p.origin; }
        o = VR_ARENA_FreeNear(base + AR_DROP_R * VR_ARENA_RingDir(i));
    }

    ar_item_next = ar_item_next + 1;
    return o;
}

// QVR: Where a perk's pedestal stands (vr_arena_perks.qc asks for it). The same info_arena_item points a wave clear
// drops its medkits and armour on, but pushed AR_PED_OUT away from the map's centre; with no item points, the same
// ring off the player at its own wider radius. So a wave's medkit and a pedestal never stand in one spot -- a
// grabbable item inside a trigger box plays badly in VR. And unlike VR_ARENA_DropPlace this does not walk the item
// cursor: a wave's drops keep every point they had.
float AR_PED_OUT = 96;

vector(float i) VR_ARENA_PedPlace =
{
    entity e = VR_ARENA_ItemSpot(i);
    vector o;

    if(e)
    {
        vector d = e.origin - ar_mgr.origin;
        d.z = 0;
        float l = vlen(d);
        if(l < 1) { d = VR_ARENA_RingDir(i); l = 1; }
        o = e.origin + (AR_PED_OUT / l) * d;
    }
    else
    {
        entity p = VR_ARENA_FindPlayer();
        vector base = ar_mgr.origin;
        if(p != world) { base = p.origin; }
        o = base + (AR_DROP_R + AR_PED_OUT) * VR_ARENA_RingDir(i);
    }

    return VR_ARENA_FreeNear(o); // (stepped round a medkit, a gun or the player standing there)
}

// ---------------------------------------------------------------------------
// A wave's monster: its place, its gun, its kind's own spawn
// ---------------------------------------------------------------------------

string(float xKind) VR_ARENA_ClassName =
{
    if(xKind == AR_SP_ZOMBIE)   { return "monster_zombie"; }
    if(xKind == AR_SP_DOG)      { return "monster_dog"; }
    if(xKind == AR_SP_KNIGHT)   { return "monster_knight"; }
    if(xKind == AR_SP_OGRE)     { return "monster_ogre"; }
    if(xKind == AR_SP_FIEND)    { return "monster_demon1"; }
    if(xKind == AR_SP_HKNIGHT)  { return "monster_hell_knight"; }
    if(xKind == AR_SP_ENFORCER) { return "monster_enforcer"; } // (its armed version puts its own classname on: VR_AM_Spawn)
    return "monster_army";
}

void(float xKind) VR_ARENA_CallSpawn =
{
    if(xKind == AR_SP_GRUNT)    { monster_vr_grunt(); return; }    // (vr_armedgrunt.qc: a grunt holding one of the player's weapons)
    if(xKind == AR_SP_ENFORCER) { monster_vr_enforcer(); return; }
    if(xKind == AR_SP_ZOMBIE)   { monster_zombie(); return; }
    if(xKind == AR_SP_DOG)      { monster_dog(); return; }
    if(xKind == AR_SP_KNIGHT)   { monster_knight(); return; }
    if(xKind == AR_SP_OGRE)     { monster_ogre(); return; }
    if(xKind == AR_SP_FIEND)    { monster_demon1(); return; }
    monster_hell_knight();
}

float(float xKind) VR_ARENA_ArmsHumanoid = { return xKind == AR_SP_GRUNT || xKind == AR_SP_ENFORCER; }

// The wave's monster at its place: an armed humanoid holding its plan's gun (or its fists), or one of Quake's
// own monsters. Its kind's spawn function does the rest (its health, its sounds, its pains, its death).
// world when it could not be put in the field at all (its kind's spawn held it back): the spawner tries again.
entity(float xKind, float xGun, float xTier, float xBoss, float xArmour, float xMask) VR_ARENA_SpawnMonster =
{
    entity manager = self;
    entity m = spawn();
    ar_place_big = (xKind == AR_SP_OGRE || xKind == AR_SP_FIEND || xKind == AR_SP_HKNIGHT || xKind == AR_SP_DOG);
    vector o = VR_ARENA_SpawnPlace(xKind == AR_SP_FIEND || xKind == AR_SP_ENFORCER);
    ar_place_big = 0;
    ar_place_drop = 0;
    if(!ar_place_ok)
    {
        // (no place free of every box this beat: it waits for the next rather than come in inside something)
        dprint(sprintf("arena: no free place for kind %g, waiting\n", xKind));
        remove(m);
        return world;
    }
    self = m;

    m.classname = VR_ARENA_ClassName(xKind);
    m.ar_slot = ar_wave;
    m.ar_tier = xTier + (xBoss ? AR_BOSS_MARK : 0);
    m.angles = '0 0 0';
    if(VR_ARENA_ArmsHumanoid(xKind))
    {
        m.weapon = xGun; // (monster_vr_grunt's "weapon": a WID_*, -1 its fists; its own spawn clears it)
        m.weapon2 = 0;
        m.weaponflags = 0;
        m.weaponflags2 = 0;
    }
    entity p = VR_ARENA_FindPlayer();
    if(p != world) { m.angles = vectoangles(p.origin - o); }
    setorigin(m, o + '0 0 32');

    // (the monster's own spawn, on self; its armour is the plan's, so the skill's roll is held: vr_agarmor.qc)
    vr_ar_hold_roll = 1;
    VR_ARENA_CallSpawn(xKind);
    vr_ar_hold_roll = 0;

    if(!self.modelindex && !self.MG_deferred)
    {
        remove(self);
        self = manager;
        return world;
    }

    if(VR_ARENA_ArmsHumanoid(xKind) && xArmour > 0)
    {
        VR_AgArmor_Give(self, xArmour, xMask);
        self.ar_armour = self.vr_ar_type;
        dprint(sprintf("arena: armour type %g pieces %g on kind %g%s\n", xArmour, xMask, xKind, xBoss ? " (boss)" : ""));
    }

    if(xBoss)
    {
        // A boss is a thicker one of its kind: vr_arena_boss_health times the difficulty's factor (and a boss wave's
        // second one, ar_plan_boss 2, half as much). What else a boss does is worker W3's boss pass.
        self.health = VR_ARENA_BossHealth(self.health, xBoss);
        dprint(sprintf("arena: boss kind %g health %g tier %g plan %g\n", xKind, self.health, xTier, xBoss));
    }

    dprint(sprintf("arena: foe kind %g gun %g tier %g at %g %g %g\n", xKind, xGun, xTier, o_x, o_y, o_z));
    VR_ARENA_Tfog(o + '0 0 32'); // (its arrival seen and heard where it comes in)
    if(self.modelindex) { VR_ARENA_ScanSpawn(self, "foe"); }

    // Its kind's own start runs as it always does (monsters.qc's walkmonster_start_go: its takedamage, its full
    // health, its AI): a wave monster must not be started another way, or it never becomes something you can hit.
    // VR_ARENA_HuntPlayer gives it the player once that start is through.

    self = manager;
    return m;
}

// ---------------------------------------------------------------------------
// A wave clear's drops: items.qc's medkit and armour makers, made grabbable as they are
// ---------------------------------------------------------------------------

// xWhat: 1 a medkit, 2 the light armour, 3 the heavier one.
entity(float xWhat, vector xAt) VR_ARENA_SpawnDrop =
{
    entity manager = self;
    entity it = spawn();
    self = it;

    if(xWhat == 1)
    {
        it.classname = "item_health";
        it.spawnflags = 0; // (items.qc's plain medkit: 25 health, its own grabbable placement)
        setorigin(it, xAt + '0 0 8');
        item_health();
    }
    else
    {
        if(xWhat == 3) { it.classname = "item_armor2"; }
        else { it.classname = "item_armor1"; }
        setorigin(it, xAt + '0 0 8');
        if(xWhat == 3) { item_armor2(); } else { item_armor1(); }
        self.armortype  = xWhat == 3 ? 0.6 : 0.3; // (its touch's own values: the map's keys, for a made item)
        self.armorvalue = xWhat == 3 ? 150 : 100;
    }

    if(!self.modelindex)
    {
        remove(self);
        self = manager;
        return world;
    }

    it.ar_drop = xWhat == 1 ? 1 : 2;
    it.ar_drop_wave = ar_wave;
    VR_ARENA_Tfog(xAt + '0 0 24');
    VR_ARENA_ScanSpawn(it, "drop");
    self = manager;
    return it;
}

void(float xWave) VR_ARENA_Drops =
{
    float n = VR_ARENA_MedkitCount(xWave);
    float armour = VR_ARENA_ArmourKind(xWave);
    float i;

    ar_drop_health = 0;
    ar_drop_armour = 0;

    vector at;
    for(i = 0; i < n; i++)
    {
        at = VR_ARENA_DropPlace(i);
        if(!ar_place_ok) { dprint("arena: no free place for a medkit\n"); continue; }
        if(VR_ARENA_SpawnDrop(1, at)) { ar_drop_health = ar_drop_health + 1; }
    }
    if(armour)
    {
        at = VR_ARENA_DropPlace(n + 1);
        if(!ar_place_ok) { dprint("arena: no free place for an armour\n"); }
        else if(VR_ARENA_SpawnDrop(armour + 1, at)) { ar_drop_armour = 1; }
    }
}

// ---------------------------------------------------------------------------
// The field: who of the wave is up, who fell, and what that is worth
// ---------------------------------------------------------------------------

float() VR_ARENA_CountAlive =
{
    if(ar_wave <= 0) { return 0; }

    float n = 0;
    entity e = findflags(world, ar_slot, ar_wave);
    while(e)
    {
        if(e.health > 0) { n = n + 1; }
        e = findflags(e, ar_slot, ar_wave);
    }
    return n;
}

// A wave foe whose kind's own start is through (monsters.qc: its takedamage is on) and who has not spotted anyone
// yet: it is given the player, so a wave comes at you instead of standing where it landed. Its own run takes over
// from there (ai.qc's FoundTarget puts it on its th_run).
void() VR_ARENA_HuntPlayer =
{
    if(ar_wave <= 0) { return; }

    entity p = VR_ARENA_FindPlayer();
    if(p == world) { return; }

    entity e = findflags(world, ar_slot, ar_wave);
    while(e)
    {
        if(e.health > 0 && e.takedamage && e.enemy == world && e.th_run)
        {
            e.enemy = p;
            e.think = FoundTarget;
            e.nextthink = time + 0.1 + 0.1 * VR_ARENA_Rand();
        }
        e = findflags(e, ar_slot, ar_wave);
    }
}

// combat.qc (after MGH_Kill): this monster fell. Its score, once (its ar_slot flips negative).
void() VR_ARENA_MonsterDie =
{
    if(!ar_mgr) { return; }
    if(ar_wave <= 0 || self.ar_slot != ar_wave) { return; }

    self.ar_slot = -ar_wave;
    ar_kills = ar_kills + 1;
    ar_wave_kills = ar_wave_kills + 1;
    float worth = VR_ARENA_KillScore(VR_ARENA_TierOf(self.ar_tier), self.ar_tier >= AR_BOSS_MARK);
    if(self.ar_armour > 0) { worth = VR_ARENA_ArmourWorth(worth, self.ar_armour); } // (an armoured foe pays more)
    ar_score = ar_score + worth;
    VR_ARENA_BodyDown(self); // (its body's time: the cleanup)

    // Its slot in the pool is free: the director looks at once, so the next foe's gap runs from this fall.
    if(ar_mgr.think == VR_ARENA_ManagerThink && ar_mgr.nextthink > time + 0.05) { ar_mgr.nextthink = time + 0.05; }
}

// (a stalled wave, and the tests' jump: its monsters still up leave). Exactly that wave's: findflags matches any
// shared bit, so it would also take other waves' foes and every fallen body (a negative ar_slot shares bits with
// nearly any wave). One in a hand is let go of first, as the Horde's expiring bodies are (VR_Carry_Forget).
void(float xWave) VR_ARENA_ClearField =
{
    if(xWave <= 0) { return; }
    entity e = findflags(world, ar_slot, xWave);
    while(e)
    {
        entity next = findflags(e, ar_slot, xWave);
        if(e.ar_slot == xWave)
        {
            VR_Carry_Forget(e);
            remove(e);
        }
        e = next;
    }
}

// ---------------------------------------------------------------------------
// The cleanup: what the waves leave lying (ROUND23.md, "Survival Arena: the cleanup"). A fallen foe's body fades
// out after vr_arena_corpse_fade (as the Horde's do, vr_mg_horde.qc MGH_corpse_expire) and at most
// vr_arena_corpse_max of them lie (the oldest goes first); a wave clear's drops go vr_arena_item_waves clears later;
// the guns the foes dropped are capped in vr_enemyguns.qc (VR_ARENA_GunMost). Nothing in a hand, pulled by a force
// grab or still dying is taken: its time waits on it.
// ---------------------------------------------------------------------------

// Held: carried, a hand on it (a ragdoll's limb, VR_Carry_SetHeld), or pulled by a force grab.
float(entity e) VR_ARENA_Held =
{
    if(e.carry_player != world || e.fg_state != 0) { return TRUE; }
    entity p = find(world, classname, "player");
    while(p)
    {
        if(p.mainhand_held == e || p.offhand_held == e) { return TRUE; }
        p = find(p, classname, "player");
    }
    return FALSE;
}

// Not to be taken now: held, or a body still dying (its death animation, its corpse's gibbing: VR_Corpse_Think2),
// AR_DYING_MAX at most.
float(entity e) VR_ARENA_Spared =
{
    if(VR_ARENA_Held(e)) { return TRUE; }
    return (e.vr_corpse == 1 || e.vr_corpse_dying) && time < e.ar_expire + AR_DYING_MAX;
}

// Its fade begins now: gone AR_FADE from now (a body past vr_arena_corpse_max, a drop past vr_arena_item_waves).
void(entity e) VR_ARENA_FadeNow =
{
    e.ar_expiring = 1;
    if(e.ar_expire <= 0 || e.ar_expire > time + AR_FADE) { e.ar_expire = time + AR_FADE; }
}

// A body lying on (not fading yet).
float(entity e) VR_ARENA_BodyLies = { return e.ar_fell > 0 && (e.ar_expire <= 0 || e.ar_expire > time + AR_FADE); }

// Past vr_arena_corpse_max: the oldest bodies not spared fade (one held stays: the next oldest goes).
void() VR_ARENA_BodyRoom =
{
    float most = cvar("vr_arena_corpse_max");
    if(most <= 0) { return; }
    float guard;
    for(guard = 0; guard < 64; guard++)
    {
        float n = 0;
        entity oldest = world;
        entity e = findflags(world, ar_expiring, 1);
        while(e)
        {
            if(VR_ARENA_BodyLies(e))
            {
                n = n + 1;
                if(!VR_ARENA_Spared(e) && (oldest == world || e.ar_fell < oldest.ar_fell)) { oldest = e; }
            }
            e = findflags(e, ar_expiring, 1);
        }
        if(n <= most || oldest == world) { return; }
        VR_ARENA_FadeNow(oldest);
    }
}

// VR_ARENA_MonsterDie: `e`, one of the waves' foes, fell. Its body's time (vr_arena_corpse_fade, give or take
// AR_FADE_JITTER), then room under vr_arena_corpse_max.
void(entity e) VR_ARENA_BodyDown =
{
    ar_fell_n = ar_fell_n + 1;
    e.ar_fell = ar_fell_n;
    e.ar_expiring = 1;
    e.ar_expire = 0;
    float lie = cvar("vr_arena_corpse_fade");
    if(lie > 0)
    {
        e.ar_expire = time + max(AR_FADE, lie + crandom() * min(AR_FADE_JITTER, lie * 0.25));
    }
    VR_ARENA_BodyRoom();
}

// A wave clear (before its own drops): the earlier clears' drops vr_arena_item_waves clears old fade (one held stays
// till let go of); the ones picked up already (items.qc leaves them, unseen, in a single player game) go now.
void(float xWave) VR_ARENA_DropsExpire =
{
    float waves = cvar("vr_arena_item_waves");
    entity e = findflags(world, ar_drop, 3);
    while(e)
    {
        entity next = findflags(e, ar_drop, 3);
        if(e.model == "" && !VR_ARENA_Held(e))
        {
            VR_Carry_Forget(e);
            remove(e);
        }
        else if(waves > 0 && e.ar_drop_wave <= xWave - waves)
        {
            VR_ARENA_FadeNow(e);
        }
        e = next;
    }
}

// vr_enemyguns.qc VR_EnemyWeapons_MakeRoom: in a running arena, the most guns its foes dropped may lie about
// (vr_arena_gun_max; 0 no limit; the Spoils perk: AR_SPOILS_GUNS times that); -1 off the arena (the shipped cap).
float() VR_ARENA_GunMost =
{
    if(!ar_mgr) { return -1; }
    float most = max(0, floor(cvar("vr_arena_gun_max")));
    if(VR_ARK_Kept()) { most = most * AR_SPOILS_GUNS; }
    return most;
}

// Every frame (VR_ARENA_Frame): the leftovers whose time has come fade out and go; one spared waits (let go of, it
// lies AR_HELD_GRACE more, seen whole again).
float ar_mend_next; // VR_EnemyWeapons_MendFades: its next look
void() VR_ARENA_Leftovers =
{
    if(time >= ar_mend_next || ar_mend_next > time + 1)
    {
        ar_mend_next = time + 0.5;
        VR_EnemyWeapons_MendFades(); // (the gun cap's fades: none left half done)
    }
    entity e = findflags(world, ar_expiring, 1);
    while(e)
    {
        entity next = findflags(e, ar_expiring, 1);
        if(e.ar_fell > 0 && e.health > 0) // (up again: no body)
        {
            e.ar_expiring = 0;
            e.ar_expire = 0;
            e.alpha = 0;
        }
        else if(e.ar_expire > 0 && time > e.ar_expire - AR_FADE)
        {
            if(VR_ARENA_Spared(e))
            {
                if(VR_ARENA_Held(e)) { e.ar_expire = time + AR_HELD_GRACE; }
                e.alpha = 0;
            }
            else if(time >= e.ar_expire)
            {
                VR_Carry_Forget(e);
                VR_Head_StopFlies(e);
                remove(e);
            }
            else
            {
                e.alpha = max(0.01, e.ar_expire - time);
            }
        }
        e = next;
    }
}

// ---------------------------------------------------------------------------
// The director's cycle
// ---------------------------------------------------------------------------

// A boss wave's big ones are planned first (so a thick plan never cuts them out); they come in halfway through it,
// after the opening burst: the plan's head is turned so they sit at its middle.
void() VR_ARENA_BossMidway =
{
    float nb = 0;
    while(nb < ar_plan_n && ar_plan_boss[nb]) { nb = nb + 1; }
    if(nb <= 0 || nb >= ar_plan_n) { return; }

    float mid = min(max(floor(ar_plan_n / 2), ar_burst_left), ar_plan_n - nb); // (where the first of them goes)
    float len = mid + nb;
    float r;
    float j;
    for(r = 0; r < nb; r++) // (the head [0, len) turned left by one, nb times: the bosses end at [mid, len))
    {
        float k0 = ar_plan_kind[0];
        float g0 = ar_plan_gun[0];
        float t0 = ar_plan_tier[0];
        float b0 = ar_plan_boss[0];
        float a0 = ar_plan_armour[0];
        float m0 = ar_plan_amask[0];
        for(j = 0; j + 1 < len; j++)
        {
            ar_plan_kind[j] = ar_plan_kind[j + 1];
            ar_plan_gun[j]  = ar_plan_gun[j + 1];
            ar_plan_tier[j] = ar_plan_tier[j + 1];
            ar_plan_boss[j] = ar_plan_boss[j + 1];
            ar_plan_armour[j] = ar_plan_armour[j + 1]; // (its armour goes with it)
            ar_plan_amask[j]  = ar_plan_amask[j + 1];
        }
        ar_plan_kind[len - 1] = k0;
        ar_plan_gun[len - 1]  = g0;
        ar_plan_tier[len - 1] = t0;
        ar_plan_boss[len - 1] = b0;
        ar_plan_armour[len - 1] = a0;
        ar_plan_amask[len - 1]  = m0;
    }
}

// This wave's next planned foe into the field: TRUE when it came in (its kind's spawn may hold it back).
float() VR_ARENA_SpawnNext =
{
    float i = ar_plan_n - ar_plan_left;
    entity m = VR_ARENA_SpawnMonster(ar_plan_kind[i], ar_plan_gun[i], ar_plan_tier[i], ar_plan_boss[i],
                                     ar_plan_armour[i], ar_plan_amask[i]);
    if(!m) { return FALSE; }
    ar_plan_left = ar_plan_left - 1;
    ar_plan_live = ar_plan_live + 1;
    ar_last_spawn = time;
    return TRUE;
}

// Is the wave's next planned foe one of its bosses.
float() VR_ARENA_NextIsBoss = { return ar_plan_left > 0 && ar_plan_boss[ar_plan_n - ar_plan_left] > 0; }

void() VR_ARENA_WaveStart =
{
    ar_wave = ar_wave + 1;
    VR_ARENA_Plan(ar_wave);

    ar_plan_left = ar_plan_n;
    ar_plan_live = 0;
    ar_wave_kills = 0;
    ar_next_spawn = 0;
    ar_wave_at = time;
    ar_used_ground = 0;
    ar_used_high = 0;
    ar_spot_next = ar_spot_next + AR_ASIDE_MAX; // (a wave begins at another place on the map's spots)

    // The alive pool: the wave opens with a small burst, then the rest trickle in a gap after each other while a
    // slot is free (VR_ARENA_ManagerThink). Its boss comes in halfway through, announced.
    ar_pool = VR_ARENA_Pool(ar_wave);
    ar_burst_left = min(VR_ARENA_Opt_Burst(), ar_pool);
    ar_gap_from = 0;
    ar_last_spawn = time;
    ar_wave_peak = 0;
    ar_gap_lo = 0;
    ar_gap_hi = 0;
    ar_gap_n = 0;
    VR_ARENA_BossMidway();

    float armoured = 0;
    float i;
    for(i = 0; i < ar_plan_n; i++) { if(ar_plan_armour[i] > 0) { armoured = armoured + 1; } }
    string foes = sprintf("%g foes", ar_plan_n);
    if(armoured > 0) { foes = sprintf("%g foes, %g in armour", ar_plan_n, armoured); }
    string head = VR_ARENA_IsBossWave(ar_wave) ? sprintf("WAVE %g - BOSS WAVE", ar_wave) : sprintf("WAVE %g", ar_wave);
    VR_ARENA_PrintAll(sprintf("%s\n\n%s\nscore %g\n", head, foes, ar_score));
    dprint(sprintf("arena: wave %g score %g alive %g pool %g armoured %g trickle %g\n", ar_wave, ar_score, ar_plan_n,
                   ar_pool, armoured, ar_trickle_n));
    ar_count_at = 0;

    self.think = VR_ARENA_ManagerThink;
    self.nextthink = time + 0.2;
}

// The rest before a wave's countdown, as this difficulty counts it (the harder the skill, the less rest).
float() VR_ARENA_RestTime = { return VR_ARENA_SkillTime(VR_ARENA_Opt_Rest(), 1); }

// The rest begins (a wave clear, the run's start): the countdown follows it (VR_ARENA_CountStart).
void(float xRest) VR_ARENA_RestBegin =
{
    ar_rest_at = time;
    ar_count_at = 0;
    self.think = VR_ARENA_CountStart;
    self.nextthink = time + xRest;
}

void() VR_ARENA_WaveClear =
{
    float w = ar_wave;

    ar_score = ar_score + VR_ARENA_WaveBonus(w);
    VR_ARENA_DropsExpire(w); // (the earlier clears' drops still lying there: vr_arena_item_waves)
    VR_ARENA_Drops(w);

    float rest = VR_ARENA_RestTime();
    VR_ARENA_ShowAll(sprintf("WAVE %g CLEAR\n\n%g foes down\nscore %g\n", w, ar_wave_kills, ar_score), max(AR_SAY, rest));
    dprint(sprintf("arena: wave %g clear kills %g medkits %g armour %g score %g\n", w, ar_wave_kills,
                   ar_drop_health, ar_drop_armour, ar_score));

    VR_ARK_WaveDone(w); // QVR: the boss wave's clear offers the player his choice of perks (vr_arena_perks.qc)

    VR_ARENA_RestBegin(rest);
}

// The boss a wave brings, by name, for its countdown ("" no boss wave).
string(float xWave) VR_ARENA_BossName =
{
    if(!VR_ARENA_IsBossWave(xWave)) { return ""; }
    float k = VR_ARENA_BossKind(xWave);
    if(VR_ARENA_BossTwice())
    {
        if(k == AR_SP_ENFORCER) { return "two enforcers"; }
        if(k == AR_SP_OGRE) { return "two ogres"; }
        return "two death knights";
    }
    if(k == AR_SP_ENFORCER) { return "an enforcer"; }
    if(k == AR_SP_OGRE) { return "an ogre"; }
    return "a death knight";
}

// The countdown to the next wave: one line a second in the middle of the view, then the wave.
void() VR_ARENA_CountThink =
{
    if(ar_count <= 0)
    {
        VR_ARENA_WaveStart();
        return;
    }

    float w = ar_wave + 1;
    string boss = VR_ARENA_BossName(w);
    if(boss != "") { VR_ARENA_ShowAll(sprintf("BOSS WAVE %g in %g...\n\n%s\n", w, ar_count, boss), 1.2); }
    else { VR_ARENA_ShowAll(sprintf("Wave %g in %g...\n\n", w, ar_count), 1.2); }
    dprint(sprintf("arena: countdown wave %g %g at %g\n", w, ar_count, time - ar_count_at));

    ar_count = ar_count - 1;
    ar_count_said = ar_count_said + 1;
    self.think = VR_ARENA_CountThink;
    self.nextthink = time + 1;
}

// Between two waves: the rest after a clear, the countdown, or a jump's wave about to begin.
float() VR_ARENA_Between =
{
    return ar_mgr.think == VR_ARENA_CountStart || ar_mgr.think == VR_ARENA_CountThink ||
           ar_mgr.think == VR_ARENA_WaveStart;
}

void() VR_ARENA_CountStart =
{
    ar_count = VR_ARENA_Opt_Countdown();
    ar_count_said = 0;
    ar_count_at = time;
    dprint(sprintf("arena: rest over wave %g after %g\n", ar_wave + 1, time - ar_rest_at));
    VR_ARENA_CountThink();
}

void() VR_ARENA_ManagerThink =
{
    float alive = VR_ARENA_CountAlive();
    if(alive > ar_alive_peak) { ar_alive_peak = alive; }

    // The players, all of them down: their score stays, a fresh wave 1 comes (as the Machine Games' horde does).
    if(!VR_ARENA_AnyPlayerUp())
    {
        ar_down = 1;
        ar_fallen = find(world, classname, "player");
        if(ar_fallen) { VR_ARENA_Show(ar_fallen, sprintf("YOU FELL AT WAVE %g\n\nscore %g\n", ar_wave, ar_score), 9); }
        dprint(sprintf("arena: fell wave %g score %g\n", ar_wave, ar_score));
        self.think = VR_ARENA_RestartThink;
        self.nextthink = time + 10;
        return;
    }

    // The alive pool (ar_pool, VR_ARENA_Pool): its opening burst comes in at once; after that, each free slot is
    // filled a randomized gap after it freed up (vr_arena_spawn_gap), a boss after its warning instead.
    while(ar_burst_left > 0 && ar_plan_left > 0 && alive < ar_pool && !VR_ARENA_NextIsBoss())
    {
        if(!VR_ARENA_SpawnNext()) { break; } // (held back: the next beat tries again)
        ar_burst_left = ar_burst_left - 1;
        alive = alive + 1;
    }
    if(ar_burst_left > 0 && VR_ARENA_NextIsBoss()) { ar_burst_left = 0; } // (a thin wave: its boss ends the burst)

    if(ar_burst_left <= 0 && ar_plan_left > 0 && alive < ar_pool)
    {
        if(ar_gap_from <= 0)
        {
            ar_gap_from = time;
            if(VR_ARENA_NextIsBoss())
            {
                ar_next_spawn = time + AR_BOSS_WARN;
                VR_ARENA_ShowAll(sprintf("A BOSS IS COMING\n\n%s\n", VR_ARENA_BossName(ar_wave)), AR_BOSS_WARN);
            }
            else
            {
                ar_next_spawn = time + VR_ARENA_SpawnGap();
            }
        }
        else if(time >= ar_next_spawn)
        {
            float boss = VR_ARENA_NextIsBoss();
            if(VR_ARENA_SpawnNext())
            {
                alive = alive + 1;
                if(!boss)
                {
                    float gap = time - ar_gap_from;
                    if(ar_gap_n == 0 || gap < ar_gap_lo) { ar_gap_lo = gap; }
                    if(ar_gap_n == 0 || gap > ar_gap_hi) { ar_gap_hi = gap; }
                    ar_gap_n = ar_gap_n + 1;
                }
                ar_gap_from = 0;
            }
            else
            {
                ar_next_spawn = time + AR_TICK; // (held back: tried again a beat later, the gap still running)
            }
        }
    }
    if(alive > ar_wave_peak) { ar_wave_peak = alive; }
    if(alive > ar_alive_peak) { ar_alive_peak = alive; }

    if(ar_plan_left == 0 && VR_ARENA_CountAlive() == 0)
    {
        VR_ARENA_WaveClear();
        return;
    }

    VR_ARENA_HuntPlayer(); // (a wave foe that has come to and has not spotted anyone: it comes at you)

    // (a foe that could not be reached: the wave lets go and the arena goes on; counted from its last foe's arrival,
    // since a big wave trickling in takes its time)
    if(time - max(ar_wave_at, ar_last_spawn) > AR_STALL)
    {
        dprint(sprintf("arena: wave %g stalled alive %g\n", ar_wave, VR_ARENA_CountAlive()));
        VR_ARENA_ClearField(ar_wave);
        VR_ARENA_WaveClear();
        return;
    }

    self.nextthink = time + AR_TICK;
    if(ar_gap_from > 0 && ar_next_spawn > time && ar_next_spawn < self.nextthink) { self.nextthink = ar_next_spawn; }
}

void() VR_ARENA_RestartThink =
{
    // A fresh run: the map again as it was, wave 1, the players at their fists again.
    ar_down = 0;
    serverflags = ar_serverflags0;
    localcmd("restart fresh\n");
}

// The players put down to their fists, once a run begins (vr_trailer.qc's own strip, every rung of the ladder).
void() VR_ARENA_StripPlayers =
{
    entity p = find(world, classname, "player");
    while(p)
    {
        float wid;
        for(wid = WID_FIST + 1; wid <= WID_LAST; wid++)
        {
            VR_Trailer_TakeWeapon(p, wid);
        }
        p = find(p, classname, "player");
    }
}

// ---------------------------------------------------------------------------
// world.qc's two hooks
// ---------------------------------------------------------------------------

void() VR_ARENA_WorldSpawn =
{
    if(!VR_ARENA_On()) { return; }
    if(deathmatch) { return; } // (the arena is one player's run up the ladder)

    ar_mgr = spawn();
    ar_mgr.classname = "vr_arena_manager";
    ar_mgr.origin = world.origin;
    ar_serverflags0 = serverflags;

    ar_wave = 0;
    ar_score = 0;
    ar_kills = 0;
    ar_wave_kills = 0;
    ar_plan_n = 0;
    ar_plan_left = 0;
    ar_plan_live = 0;
    ar_alive_peak = 0;
    ar_drop_health = 0;
    ar_drop_armour = 0;
    ar_spot_next = 0;
    ar_item_next = 0;
    ar_used_ground = 0;
    ar_used_high = 0;
    ar_stripped = 0;
    ar_down = 0;
    ar_fell_n = 0;
    ar_pool = 0;
    ar_burst_left = 0;
    ar_gap_from = 0;
    ar_last_spawn = 0;
    ar_wave_peak = 0;

    MGH_PrecacheMonsters(); // (vr_mg_horde.qc: the models and sounds of the monsters the waves bring)

    // The run's first rest, then wave 1's countdown (both after the map's own spawns, so its info_arena_spawn points
    // are known).
    ar_count = 0;
    ar_count_at = 0;
    entity oself = self;
    self = ar_mgr;
    VR_ARENA_RestBegin(max(1, VR_ARENA_RestTime()));
    self = oself;

    dprint(sprintf("arena: %s on (skill %g %s, impact %g, max alive %g, boss every %g, seed %g)\n", mapname,
                   VR_ARENA_SkillLevel(), VR_ARENA_SkillName(VR_ARENA_SkillLevel()), cvar("vr_arena_skill_impact"),
                   VR_ARENA_FieldCap(), VR_ARENA_BossEvery(), cvar("vr_arena_seed")));
    dprint(sprintf("arena: pool %g at wave 1, wave size %g\n", VR_ARENA_Pool(1), VR_ARENA_Opt_WaveSize()));
}

void() VR_ARENA_Frame =
{
    if(!ar_mgr) { return; }
    if(!ar_stripped)
    {
        entity p = find(world, classname, "player");
        if(!p) { return; } // (its clients are not in yet)
        ar_stripped = 1;
        VR_ARENA_StripPlayers();
        VR_ARENA_ShowAll(sprintf("SURVIVAL ARENA\n\n%s: you start unarmed\nwave 1 is coming\n",
                                 VR_ARENA_SkillName(VR_ARENA_SkillLevel())), max(AR_SAY, ar_mgr.nextthink - time));
    }
    VR_ARK_Frame(); // QVR: the perks that have to keep their word (vr_arena_perks.qc)
    VR_ARENA_Leftovers(); // (the cleanup: bodies and drops whose time has come fade out)
}

// ---------------------------------------------------------------------------
// The tests' jump: the arena runs the wave after this one next (vr_arena_test.qc)
// ---------------------------------------------------------------------------

void(float xWave) VR_ARENA_FastForward =
{
    if(!ar_mgr) { return; }
    VR_ARENA_ClearField(ar_wave);
    ar_plan_left = 0;
    ar_wave = xWave - 1;
    ar_count = 0; // (a jump: no rest or countdown first)
    ar_count_at = 0;
    ar_mgr.think = VR_ARENA_WaveStart;
    ar_mgr.nextthink = time + 0.5;
}

// ---------------------------------------------------------------------------
// The map's spots (vr_arena_gen.py's / the arena map's entities)
// ---------------------------------------------------------------------------

/*QUAKED info_arena_spawn (0 .5 .8) (-16 -16 -16) (16 16 16)
A wave's foe comes in here. "kind": 1 (or unset) a ground point, 2 a high one: a wave's ranged and flying foes
come in above the floor there. The waves use these round robin; a point used twice in a wave is stepped aside.
With none in the map, its foes come in a ring 400 units off the player instead.
*/
void() info_arena_spawn =
{
    if(!self.kind) { self.kind = 1; }
}

/*QUAKED info_arena_item (0 .8 .5) (-16 -16 0) (16 16 32)
Where a wave clear's medkit or armour lands, round robin over the map's item points.
With none in the map, the drops land in a ring 220 units off the player instead.
*/
void() info_arena_item =
{
}
