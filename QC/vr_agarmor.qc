// vr_agarmor.qc -- armour for armed humanoids, P1 (ROUND23.md, "Armour for armed humanoids, P1"; the research:
// qvr-kit/scratch/research_agarmor.md). An armed grunt may wear up to ten plates (the engine's shells, vr_agarmor.cpp):
// each its own pool from its type's total (Quake's: green 100, yellow 150, red 200) split by the piece's share. A hit on
// a plate (its bone, as drawn: ragdollbone's QVR_ARMOR_PIECE_AT) is absorbed by the player's rule (vr_agarmor_rule 0:
// ceil(type x damage) from the pool) or stopped whole (1) while the pool lasts; a shotgun's pellets each count on their
// own piece; a blast or a hit with no point is shared by the pieces facing it. At 0 the plate breaks: its bit off (the
// engine sends the mask), a metal crack (vr/armor_crack.wav), and it falls as debris
// ("<model>#plate<piece>": a prop you take in a hand, never force-grab or wear, fading out after
// vr_agarmor_debris_time, its time counted again when taken). Helmets halve a headshot's extra and stop head pops
// (vr_agarmor_helmet_head); no limb is cut under an unbroken plate (vr_agarmor_block_cuts). Quad Damage does not pass
// through. Corpses keep their plates; a gibbing sheds them. Nothing changes for a humanoid without armour.
// P2 (ROUND23.md, "Armour P2"): the enforcer wears it too (the same rolls); each piece's damage marks at two thirds and a
// third of its pool (.vr_ar_wear, drawn with the dented and the battered skins); the toolgun's spawn page dresses what
// it spawns (VR_AgArmor_Toolgun).

float VR_AR_PIECES = 10;
float VR_AR_HELMET = 1;

// Each piece's share of its type's total (out of 96: the cuirass 30, the helmet 12, pauldrons 7, bracers 6, thighs 8,
// shins 6).
float(float i) VR_AgArmor_Share =
{
    if(i == 0) return 30;
    if(i == 1) return 12;
    if(i < 4) return 7;
    if(i < 6) return 6;
    if(i < 8) return 8;
    return 6;
};

float(float i) VR_AgArmor_Bit =
{
    float b = 1;
    while(i > 0)
    {
        b = b * 2;
        i = i - 1;
    }
    return b;
};

float(entity e, float i) VR_AgArmor_On = { return (e.vr_ar_mask & VR_AgArmor_Bit(i)) != 0; };

// The share of a hit a plate of `type` saves (the player's rule).
float(float type) VR_AgArmor_Frac =
{
    if(type >= 3) return 0.8;
    if(type == 2) return 0.6;
    return 0.3;
};

// Piece `i`'s full pool in `type` (its share of Quake's total).
float(float type, float i) VR_AgArmor_Full = { return max(1, rint((50 + 50 * type) * VR_AgArmor_Share(i) / 96)); };

// P2's damage marks: piece `i`'s wear bits from its pool left (two thirds or under, a third or under: the engine draws
// its dented, then battered and scorched skins).
void(entity e, float i) VR_AgArmor_Wear =
{
    float b = VR_AgArmor_Bit(i);
    float b2 = b * 1024;
    e.vr_ar_wear = e.vr_ar_wear - (e.vr_ar_wear & (b | b2));
    if(!VR_AgArmor_On(e, i))
    {
        return;
    }
    float left = e.vr_ar_hp[i] / VR_AgArmor_Full(e.vr_ar_type, i);
    if(left <= 0.66) e.vr_ar_wear = e.vr_ar_wear | b;
    if(left <= 0.33) e.vr_ar_wear = e.vr_ar_wear | b2;
};

// `e` wears `type` (0 none) on the pieces of `mask`: each pool full.
void(entity e, float type, float mask) VR_AgArmor_Give =
{
    float i;
    e.vr_ar_type = type;
    e.vr_ar_mask = (type > 0) ? mask : 0;
    e.vr_ar_wear = 0;
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        float on = (e.vr_ar_type > 0) && VR_AgArmor_On(e, i);
        e.vr_ar_hp[i] = on ? VR_AgArmor_Full(type, i) : 0;
    }
    if(type > 0)
    {
        precache_sound("items/armor1.wav");
        precache_sound("vr/armor_crack.wav"); // (VR_AgArmor_Break's)
    }
};

// vr_agarmor_set <entity> <type> [pieces] in a live game (vr_agarmor.cpp set_f): a monster's armour.
float(entity e, float type, float pieces) VR_AgArmor_SetCmd =
{
    if(!(e.flags & FL_MONSTER) && !e.vr_corpse)
    {
        return FALSE;
    }
    VR_AgArmor_Give(e, max(0, min(3, type)), pieces);
    dprint(sprintf("agarmor: %s (kind %g) wears type %g, pieces %g\n", e.classname, e.vr_ag_kind, e.vr_ar_type, e.vr_ar_mask));
    return TRUE;
};

// ----------------------------------------------------------------------------
// The skill's roll (vr_agarmor_skill0..3: the chance, _type the best type, _pieces how many). Not random(): a spread
// of its own (each spawn the next of a low-discrepancy sequence, offset by where it stands), so it never shifts the
// game's other rolls.
float vr_ar_seq;
float vr_ar_roll_type; // VR_AgArmor_Roll's answer
float vr_ar_roll_mask;
float vr_ar_hold_roll; // 1 while the Survival Arena spawns a wave foe: its armour is the arena's roll, not the skill's
                       // (vr_arena.qc VR_ARENA_SpawnMonster; the skill's sequence does not move either)

float(float k, vector at) VR_AgArmor_Rand =
{
    float a = 0.6180339887;
    if(k == 1) a = 0.7548776662;
    else if(k == 2) a = 0.5698402910;
    else if(k >= 3) a = 0.4142135624 + k * 0.0731;
    float x = vr_ar_seq * a + at_x * 0.0137 + at_y * 0.0071 + at_z * 0.0029 + k * 0.31;
    if(vr_map_content_seed > 0) // (the map's content seed, vr_fresh_map_seeds: another spread each new start; 0 as before)
    {
        float s = vr_map_content_seed - 30011 * floor(vr_map_content_seed / 30011);
        float o = (s + 1) * (0.3819660113 + k * 0.1234);
        x = x + (o - floor(o));
    }
    return x - floor(x);
};

// The skill's roll settings (sk 0..3): what 0 its chance (vr_agarmor_skill<sk>), 1 its best type (_type), 2 its
// pieces (_pieces).
float(float sk, float what) VR_AgArmor_SkillValue =
{
    float h;
    if(sk <= 0)
        h = what == 0 ? cvarh_vr_agarmor_skill0 : (what == 1 ? cvarh_vr_agarmor_skill0_type : cvarh_vr_agarmor_skill0_pieces);
    else if(sk == 1)
        h = what == 0 ? cvarh_vr_agarmor_skill1 : (what == 1 ? cvarh_vr_agarmor_skill1_type : cvarh_vr_agarmor_skill1_pieces);
    else if(sk == 2)
        h = what == 0 ? cvarh_vr_agarmor_skill2 : (what == 1 ? cvarh_vr_agarmor_skill2_type : cvarh_vr_agarmor_skill2_pieces);
    else
        h = what == 0 ? cvarh_vr_agarmor_skill3 : (what == 1 ? cvarh_vr_agarmor_skill3_type : cvarh_vr_agarmor_skill3_pieces);
    return cvar_hget(h);
};

// The skill's armour for one spawn at `at` (vr_ar_roll_type 0: none).
void(float sk, vector at) VR_AgArmor_Roll =
{
    vr_ar_seq = vr_ar_seq + 1;
    vr_ar_roll_type = 0;
    vr_ar_roll_mask = 0;
    sk = max(0, min(3, floor(sk)));
    float chance = VR_AgArmor_SkillValue(sk, 0);
    if(chance <= 0 || VR_AgArmor_Rand(0, at) >= chance)
    {
        return;
    }
    float best = max(1, min(3, floor(VR_AgArmor_SkillValue(sk, 1))));
    vr_ar_roll_type = 1 + min(best - 1, floor(VR_AgArmor_Rand(1, at) * best));
    float n = max(1, min(VR_AR_PIECES, floor(VR_AgArmor_SkillValue(sk, 2))));
    vr_ar_roll_mask = n >= 2 ? 3 : 1;
    // The rest at random among the eight limb plates.
    float left = n - 2;
    float k = 3;
    while(left > 0)
    {
        float i = 2 + floor(VR_AgArmor_Rand(k, at) * 8);
        k = k + 1;
        float tries = 0;
        while(vr_ar_roll_mask & VR_AgArmor_Bit(i))
        {
            i = 2 + (i - 1) - 8 * floor((i - 1) / 8); // (the next one round: 2..9)
            tries = tries + 1;
            if(tries > 8) break;
        }
        vr_ar_roll_mask = vr_ar_roll_mask | VR_AgArmor_Bit(i);
        left = left - 1;
    }
};

void(string base) VR_AgArmor_PrecacheOf;

void(entity e) VR_AgArmor_Spawn =
{
    if(e.vr_ag_kind <= 3)
    {
        VR_AgArmor_PrecacheOf(e.model); // (its plates ahead of its first break; the level's load did the armed kinds)
    }
    if(e.vr_ag_kind > 3 || deathmatch || vr_ar_hold_roll) // (VR_AM_GRUNT 0, VR_AM_ENFORCER 1, VR_AM_KNIGHT 2, VR_AM_DKNIGHT 3: vr_armedmon.qc, compiled later)
    {
        return; // (P2: the enforcer too, by the same chances: no free set; the knights too (agent/dknight))
    }
    // (The server's skill, not the `skill` global: StartFrame sets that, so a map's own monsters, spawned with the map
    // before its first frame, read 0 there: Easy's chance, none; his voice note 2026-11-01 18:52, "even in nightmare".)
    VR_AgArmor_Roll(cvar_hget(cvarh_skill), e.origin);
    if(vr_ar_roll_type > 0)
    {
        VR_AgArmor_Give(e, vr_ar_roll_type, vr_ar_roll_mask);
    }
};

// The toolgun's Armed Humanoid page (vr_toolgun.cpp; vr_toolgun_armed_armor, _pieces) on what it spawned: `armour` 0
// none (not even the skill's roll), 1 green, 2 yellow, 3 red, 4 one of them at random; `pieces` 0 the cuirass and the
// helmet, 1 all ten, 2 random (the cuirass, the helmet and 0..8 of the limb plates).
void(entity e, float armour, float pieces) VR_AgArmor_Toolgun =
{
    float type = armour >= 4 ? 1 + floor(random() * 2.999) : max(0, min(3, floor(armour)));
    float mask = 3;
    if(pieces == 1)
    {
        mask = 1023;
    }
    else if(pieces >= 2)
    {
        float i;
        for(i = 2; i < VR_AR_PIECES; i++)
        {
            if(random() < 0.5) mask = mask | VR_AgArmor_Bit(i);
        }
    }
    VR_AgArmor_Give(e, type, type > 0 ? mask : 0);
    dprint(sprintf("agarmor: toolgun: %s (kind %g) wears type %g, pieces %g\n", e.classname, e.vr_ag_kind, e.vr_ar_type, e.vr_ar_mask));
};

// ----------------------------------------------------------------------------
// Breaking: the debris.
float vr_ar_debris_made; // the tests' count

// Its piece's model: a literal (precaches keep the string; the wearers are the engine's: vr_agarmor.cpp `wearers`).
string(string base, float i) VR_AgArmor_ModelOf =
{
    if(base == "progs/soldier.mdl")
    {
        if(i == 0) return "progs/soldier.mdl#plate0";
        if(i == 1) return "progs/soldier.mdl#plate1";
        if(i == 2) return "progs/soldier.mdl#plate2";
        if(i == 3) return "progs/soldier.mdl#plate3";
        if(i == 4) return "progs/soldier.mdl#plate4";
        if(i == 5) return "progs/soldier.mdl#plate5";
        if(i == 6) return "progs/soldier.mdl#plate6";
        if(i == 7) return "progs/soldier.mdl#plate7";
        if(i == 8) return "progs/soldier.mdl#plate8";
        return "progs/soldier.mdl#plate9";
    }
    if(base == "progs/vrenforcer.mdl")
    {
        if(i == 0) return "progs/vrenforcer.mdl#plate0";
        if(i == 1) return "progs/vrenforcer.mdl#plate1";
        if(i == 2) return "progs/vrenforcer.mdl#plate2";
        if(i == 3) return "progs/vrenforcer.mdl#plate3";
        if(i == 4) return "progs/vrenforcer.mdl#plate4";
        if(i == 5) return "progs/vrenforcer.mdl#plate5";
        if(i == 6) return "progs/vrenforcer.mdl#plate6";
        if(i == 7) return "progs/vrenforcer.mdl#plate7";
        if(i == 8) return "progs/vrenforcer.mdl#plate8";
        return "progs/vrenforcer.mdl#plate9";
    }
    if(base == "progs/vrknight.mdl")
    {
        if(i == 0) return "progs/vrknight.mdl#plate0";
        if(i == 1) return "progs/vrknight.mdl#plate1";
        if(i == 2) return "progs/vrknight.mdl#plate2";
        if(i == 3) return "progs/vrknight.mdl#plate3";
        if(i == 4) return "progs/vrknight.mdl#plate4";
        if(i == 5) return "progs/vrknight.mdl#plate5";
        if(i == 6) return "progs/vrknight.mdl#plate6";
        if(i == 7) return "progs/vrknight.mdl#plate7";
        if(i == 8) return "progs/vrknight.mdl#plate8";
        return "progs/vrknight.mdl#plate9";
    }
    if(base == "progs/vrhknight.mdl")
    {
        if(i == 0) return "progs/vrhknight.mdl#plate0";
        if(i == 1) return "progs/vrhknight.mdl#plate1";
        if(i == 2) return "progs/vrhknight.mdl#plate2";
        if(i == 3) return "progs/vrhknight.mdl#plate3";
        if(i == 4) return "progs/vrhknight.mdl#plate4";
        if(i == 5) return "progs/vrhknight.mdl#plate5";
        if(i == 6) return "progs/vrhknight.mdl#plate6";
        if(i == 7) return "progs/vrhknight.mdl#plate7";
        if(i == 8) return "progs/vrhknight.mdl#plate8";
        return "progs/vrhknight.mdl#plate9";
    }
    if(base == "progs/vrbody.mdl") // (the player's plates' debris, whatever build is drawn: vr_playerplates.qc)
    {
        if(i == 0) return "progs/vrbody.mdl#plate0";
        if(i == 1) return "progs/vrbody.mdl#plate1";
        if(i == 2) return "progs/vrbody.mdl#plate2";
        if(i == 3) return "progs/vrbody.mdl#plate3";
        if(i == 4) return "progs/vrbody.mdl#plate4";
        if(i == 5) return "progs/vrbody.mdl#plate5";
        if(i == 6) return "progs/vrbody.mdl#plate6";
        if(i == 7) return "progs/vrbody.mdl#plate7";
        if(i == 8) return "progs/vrbody.mdl#plate8";
        return "progs/vrbody.mdl#plate9";
    }
    if(base != "progs/vrgrunt.mdl") return "";
    if(i == 0) return "progs/vrgrunt.mdl#plate0";
    if(i == 1) return "progs/vrgrunt.mdl#plate1";
    if(i == 2) return "progs/vrgrunt.mdl#plate2";
    if(i == 3) return "progs/vrgrunt.mdl#plate3";
    if(i == 4) return "progs/vrgrunt.mdl#plate4";
    if(i == 5) return "progs/vrgrunt.mdl#plate5";
    if(i == 6) return "progs/vrgrunt.mdl#plate6";
    if(i == 7) return "progs/vrgrunt.mdl#plate7";
    if(i == 8) return "progs/vrgrunt.mdl#plate8";
    return "progs/vrgrunt.mdl#plate9";
};

string(entity e, float i) VR_AgArmor_Model = { return VR_AgArmor_ModelOf(e.model, i); };

// A plate breaking: a recorded metal crack (his note 2026-11-03; CREDITS.md, SOUNDS_OURS.md), never heard as armour picked up.
string VR_AR_BREAK_SOUND = "vr/armor_crack.wav";

// Every piece's debris model of the wearer `base` (already precached: its plates are cut from its shell) and the break's
// sounds, precached ahead: each plate's model is made as it loads (vr_agarmor.cpp derivedFile), and loaded late, at a
// piece's first break, each one cost a hitch of ~80 ms (his note, 2026-11-01). Called at the level's load
// (VR_AGR_PrecacheArmour, worldspawn) for the armed kinds' bodies and at each wearer's spawn (VR_AgArmor_Spawn).
float vr_ar_precached; // the tests: the plate models precached so far (each call's own count)
float vr_ar_half_precached; // ... and their halves' (vr_agarmor_split)
void(string base) VR_AgArmor_PrecacheOf =
{
    if(deathmatch || VR_AgArmor_ModelOf(base, 0) == "")
    {
        return;
    }
    float i;
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        precache_model(VR_AgArmor_ModelOf(base, i));
        vr_ar_precached = vr_ar_precached + 1;
    }
    // (and the halves a plate shot off splits into, vr_agarmor_split: the engine names them for a body of `base`)
    if(cvar_hget(cvarh_vr_agarmor_split))
    {
        entity t = spawn();
        t.model = base;
        for(i = 0; i < VR_AR_PIECES * 2; i++)
        {
            string hm = limbmodel(t, QVR_ARMOR_HALF - i);
            if(hm != "")
            {
                precache_model(hm);
                vr_ar_half_precached = vr_ar_half_precached + 1;
            }
        }
        remove(t);
    }
    precache_sound("items/armor1.wav");
    precache_sound(VR_AR_BREAK_SOUND);
};
string vr_ar_break_sound_last; // the tests: the last break's sound
float vr_ar_debris_grabs;      // the tests: debris taken in a hand

float() VR_AgArmor_DebrisTime = { return max(0.1, cvar_hget(cvarh_vr_agarmor_debris_time)); };

// Its last second: fading out, then gone. Held (or taken since), it waits its whole time again (VR_AgArmor_DebrisGrabbed).
void() VR_AgArmor_DebrisFade =
{
    if(self.carry_player != world)
    {
        self.wait = 0;
        self.alpha = 1;
        self.nextthink = time + VR_AgArmor_DebrisTime();
        return;
    }
    if(!self.wait)
    {
        self.wait = time;
    }
    float a = 1 - (time - self.wait);
    if(a <= 0.05)
    {
        remove(self);
        return;
    }
    self.alpha = a;
    self.nextthink = time + 0.05;
};

// VR_Carry_Start: debris taken in a hand: its fade put off, its time counted again from now.
void(entity h) VR_AgArmor_DebrisGrabbed =
{
    if(h.classname != "vr_ar_debris")
    {
        return;
    }
    h.wait = 0;
    h.alpha = 1;
    h.think = VR_AgArmor_DebrisFade;
    h.nextthink = time + VR_AgArmor_DebrisTime();
    vr_ar_debris_grabs = vr_ar_debris_grabs + 1;
};

// Piece `i` of `e` off as debris at `at`, turned `ang`, flying `vel`: a physics prop of its own model, taken in a hand
// to be thrown about (its fade put off), never force-grabbed, worn or put away.
entity(string m, float type, float i, vector at, vector ang, vector vel) VR_AgArmor_DebrisOf =
{
    if(m == "" || at == '0 0 0')
    {
        return world; // (no rig, no shell)
    }
    precache_model(m); // (made as it loads: vr_agarmor.cpp derivedFile)
    entity h = spawn();
    h.classname = "vr_ar_debris";
    setmodel(h, m);
    h.skin = max(0, type - 1) + 6; // (battered and scorched: make_agarmor.py's last three)
    setorigin(h, at);
    h.angles = ang;
    h.movetype = MOVETYPE_BOUNCE;
    h.takedamage = DAMAGE_NO;
    h.flags = 0; // (never FL_FORCEGRABBABLE, never an item)
    h.velocity = vel;
    // A hand takes it (its box the model's: the engine's hand test for a thing not force-grabbable): carried, thrown,
    // nothing given (carry_use nothing: never armour to wear, never put away).
    setsize(h, modelbounds(h, 0), modelbounds(h, 1));
    h.solid = SOLID_NOT_BUT_TOUCHABLE;
    h.handtouch = SUB_Null;
    // Thrown hard (VR_Carry_Throw) it hurts what it meets as any thrown prop does, by its mass and speed
    // (forcegrabbable_touch: VR_Thrown_Damage; vr_debris.qc's rocks the same); swung in the fist, a prop's blow.
    h.touch = forcegrabbable_touch;
    h.throwhit = QVR_THROWHIT_HIT;
    SetThrownWeaponDealDamage(h, FALSE);
    SetThrownWeaponDisappearOnHit(h, FALSE);
    VR_Carry_Setup(h);
    h.vr_rigid = 1;
    h.vr_prop_mass = i == 0 ? 4 : 1.5;
    h.vr_spin = [crandom() * 4, crandom() * 4, crandom() * 4];
    h.think = VR_AgArmor_DebrisFade;
    h.nextthink = time + VR_AgArmor_DebrisTime();
    vr_ar_debris_made = vr_ar_debris_made + 1;
    return h;
};

entity(entity e, float i, vector at, vector ang, vector vel) VR_AgArmor_DebrisAt =
{
    return VR_AgArmor_DebrisOf(VR_AgArmor_Model(e, i), e.vr_ar_type, i, at, ang, vel);
};

// A plate shot off (its pool gone: VR_AgArmor_Break, the player's VR_PP_Shed): piece `i` of `e` (whole: model `whole`
// at `at`) falls as two halves (vr_agarmor_split; ROUND24.md, "Broken armour splits in two"): each its own prop where it
// lay on the body (limbpiece 16 / 17), half the whole's mass, its own hull (its model's), the fall's `vel` and
// vr_agarmor_split_impulse apart along the line between them; the same skin (its dents and scorches). The helmet, a
// piece with no halves: whole. The debris made (0..2).
float vr_ar_split_made;   // the tests: plates split
float vr_ar_split_mass;   // the tests: the last split's halves' masses summed
float vr_ar_split_whole;  // ... and the whole plate's they came from
float(entity e, string whole, float type, float i, vector at, vector ang, vector vel) VR_AgArmor_DebrisBroken =
{
    if(cvar_hget(cvarh_vr_agarmor_split) && i != VR_AR_HELMET)
    {
        string m0 = limbmodel(e, QVR_ARMOR_HALF - 2 * i);
        string m1 = limbmodel(e, QVR_ARMOR_HALF - 2 * i - 1);
        vector a0 = limbpiece(e, i, 16);
        vector a1 = limbpiece(e, i, 17);
        if(m0 != "" && m1 != "" && a0 != '0 0 0' && a1 != '0 0 0')
        {
            vector apart = normalize(a1 - a0) * max(0, cvar_hget(cvarh_vr_agarmor_split_impulse));
            entity h0 = VR_AgArmor_DebrisOf(m0, type, i, a0, ang, vel - apart);
            entity h1 = VR_AgArmor_DebrisOf(m1, type, i, a1, ang, vel + apart);
            float n = 0;
            vr_ar_split_mass = 0;
            if(h0 != world)
            {
                vr_ar_split_whole = h0.vr_prop_mass;
                h0.vr_prop_mass = h0.vr_prop_mass * 0.5;
                vr_ar_split_mass = vr_ar_split_mass + h0.vr_prop_mass;
                n = n + 1;
            }
            if(h1 != world)
            {
                vr_ar_split_whole = h1.vr_prop_mass;
                h1.vr_prop_mass = h1.vr_prop_mass * 0.5;
                vr_ar_split_mass = vr_ar_split_mass + h1.vr_prop_mass;
                n = n + 1;
            }
            vr_ar_split_made = vr_ar_split_made + 1;
            dprint(sprintf("agarmor: piece %g split in two (%s, %s), %g apart\n", i, m0, m1, vlen(a1 - a0)));
            return n;
        }
    }
    return VR_AgArmor_DebrisOf(whole, type, i, at, ang, vel) != world;
};

// Piece `i` of `e` falls off as debris where it is: out from the body and up.
float(entity e, float i) VR_AgArmor_Debris =
{
    vector at = limbpiece(e, i, 10);
    vector out = at - (e.absmin + e.absmax) * 0.5;
    out_z = 0;
    return VR_AgArmor_DebrisBroken(e, VR_AgArmor_Model(e, i), e.vr_ar_type, i, at, limbpiece(e, i, 11),
        e.velocity + normalize(out) * (60 + 40 * random()) + '0 0 120');
};

void(entity e, float i) VR_AgArmor_Break =
{
    e.vr_ar_hp[i] = 0;
    e.vr_ar_mask = e.vr_ar_mask - (e.vr_ar_mask & VR_AgArmor_Bit(i));
    precache_sound(VR_AR_BREAK_SOUND);
    // (CHAN_AUTO: no other sound on the body cuts it off. Louder than full, over the hit's own sounds.)
    sound(e, CHAN_AUTO, VR_AR_BREAK_SOUND, max(0, min(4, cvar_hget(cvarh_vr_agrunt_armor_crack_volume))),
        max(0, cvar_hget(cvarh_vr_agrunt_armor_crack_attn)));
    vr_ar_break_sound_last = VR_AR_BREAK_SOUND;
    VR_AgArmor_Debris(e, i);
    dprint(sprintf("agarmor: %s (kind %g): piece %g broke (left %g)\n", e.classname, e.vr_ag_kind, i, e.vr_ar_mask));
};

// Gibbed (ThrowHead, VR_Corpse_Gib; self `e`, before its limbs fly): every piece left flies off with the gibs (`dm`:
// VelocityForDamage's), none lost.
void(entity e, float dm) VR_AgArmor_Shed =
{
    float i;
    float n = 0;
    for(i = 0; i < VR_AR_PIECES && e.vr_ar_type > 0; i++)
    {
        if(VR_AgArmor_On(e, i))
        {
            vector at = limbpiece(e, i, 10);
            vector out = normalize(at - (e.absmin + e.absmax) * 0.5);
            vector vel = VR_Gore_Fling(VelocityForDamage(dm)) * 0.6 + out * (80 + 60 * random()) + '0 0 80';
            n = n + (VR_AgArmor_DebrisAt(e, i, at, limbpiece(e, i, 11), vel) != world);
        }
    }
    if(e.vr_ar_type > 0)
    {
        dprint(sprintf("agarmor: %s gibbed: %g plates shed (pieces %g)\n", e.classname, n, e.vr_ar_mask));
    }
    e.vr_ar_mask = 0;
    e.vr_ar_type = 0;
    e.vr_ar_wear = 0;
};

// ----------------------------------------------------------------------------
// Cuts: a limb or the head cut or popped off takes the plates it carries with it, dislodged as debris (VR_Limb_Cut,
// VR_Decap_Cut). Their places are taken before the cut (after it the bones it took are gone from the body as drawn).
entity vr_ar_cut_ent;
float vr_ar_cut_mask;
vector vr_ar_cut_at[10];
vector vr_ar_cut_ang[10];

// Before a cut of `e` taking the rig's bones `takes` (ragdolllimbs' bits).
void(entity e, float takes) VR_AgArmor_CutPrepare =
{
    float i;
    vr_ar_cut_ent = e;
    vr_ar_cut_mask = 0;
    if(e.vr_ar_type <= 0 || !e.vr_ar_mask || !takes)
    {
        return;
    }
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        if(VR_AgArmor_On(e, i) && (ragdollbone(e, [i, 0, 0], QVR_ARMOR_PIECE_BONES) & takes))
        {
            vr_ar_cut_mask = vr_ar_cut_mask | VR_AgArmor_Bit(i);
            vr_ar_cut_at[i] = limbpiece(e, i, 10);
            vr_ar_cut_ang[i] = limbpiece(e, i, 11);
        }
    }
};

// The cut done (`vel` the piece's way): its plates off as debris. Not done: nothing.
void(entity e, float done, vector vel) VR_AgArmor_CutDone =
{
    float i;
    if(vr_ar_cut_ent != e || !vr_ar_cut_mask || !done)
    {
        vr_ar_cut_ent = world;
        return;
    }
    vr_ar_cut_ent = world;
    if(vlen(vel) > 400)
    {
        vel = normalize(vel) * 400;
    }
    float n = 0;
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        if(vr_ar_cut_mask & VR_AgArmor_Bit(i))
        {
            e.vr_ar_hp[i] = 0;
            e.vr_ar_mask = e.vr_ar_mask - (e.vr_ar_mask & VR_AgArmor_Bit(i));
            VR_AgArmor_Wear(e, i);
            vector v = vel * 0.6 + [crandom() * 60, crandom() * 60, 60 + random() * 60];
            n = n + (VR_AgArmor_DebrisAt(e, i, vr_ar_cut_at[i], vr_ar_cut_ang[i], v) != world);
        }
    }
    dprint(sprintf("agarmor: %s's cut dislodged %g plates (pieces %g -> left %g)\n", e.classname, n, vr_ar_cut_mask, e.vr_ar_mask));
    vr_ar_cut_mask = 0;
};

// A corpse's piece worn through (VR_Limb_WearInto; `joint` 0 its head, else a limb's joint): TRUE while an unbroken
// plate on the bones it would take keeps it on (vr_agarmor_block_cuts; the head: vr_agarmor_helmet_head), or this hit
// struck a plate still on as it landed (VR_AgArmor_Damage: the hit that breaks it pops nothing): the plate breaks first.
entity vr_ar_plate_hit_ent; // the last hit at a point struck an unbroken plate of it (VR_AgArmor_Damage)
float vr_ar_plate_hit_time;

float(entity e, float joint) VR_AgArmor_Holds =
{
    float i;
    if(vr_ar_plate_hit_ent == e && vr_ar_plate_hit_time == time && e.vr_ar_type > 0 &&
       cvar_hget(joint == 0 ? cvarh_vr_agarmor_helmet_head : cvarh_vr_agarmor_block_cuts))
    {
        return TRUE;
    }
    if(e.vr_ar_type <= 0 || !e.vr_ar_mask)
    {
        return FALSE;
    }
    if(joint == 0)
    {
        return cvar_hget(cvarh_vr_agarmor_helmet_head) && VR_AgArmor_On(e, VR_AR_HELMET);
    }
    if(!cvar_hget(cvarh_vr_agarmor_block_cuts))
    {
        return FALSE;
    }
    float takes = ragdolllimbs(e, 4, joint);
    for(i = 2; i < VR_AR_PIECES; i++)
    {
        if(VR_AgArmor_On(e, i) && (ragdollbone(e, [i, 0, 0], QVR_ARMOR_PIECE_BONES) & takes))
        {
            return TRUE;
        }
    }
    return FALSE;
};

// ----------------------------------------------------------------------------
// The plate a shot meets first (ROUND23.md, "Armour: the plate a shot meets"): every shot's and projectile's point and
// direction pass PositionalDamage, which asks here once (QVR_ARMOR_SHOT: the plates' triangles as drawn, on the shot's
// line before it crosses the skin). Its sparks, its blood, its pellet count, its damage, a cut it arms all read that
// answer (VR_AgArmor_PieceAt) while it is this target's this frame and near the point they ask about; a blow or a hit
// with no line (a blade at a ragdoll's point) asks the point alone (QVR_ARMOR_PIECE_AT).
entity vr_ar_shot_ent;
float vr_ar_shot_time;
vector vr_ar_shot_at;
float vr_ar_shot_piece;
float vr_ar_shots; // the tests: shots asked about

// A projectile's flight line (VR_Proj_Begin, before its hit asks here): the query reads the line from `from` (a frame's
// flight back from where it touched `targ`), not from its touch point, short of the model or beside the line.
entity vr_ar_fly_ent;
float vr_ar_fly_time;
vector vr_ar_fly_from;

void(entity targ, vector from, vector dir) VR_AgArmor_Flight =
{
    vr_ar_fly_ent = targ;
    vr_ar_fly_time = time;
    vr_ar_fly_from = from;
};

float VR_AR_SHOT_BACK = 64; // units: how far before its point the engine's query starts (vr_agarmor.cpp shotBack)
float VR_AR_SHOT_NEAR = 16; // units: a point this near the shot's (a nail's own origin at its touch) is the shot's

void(entity targ, vector at, vector dir) VR_AgArmor_Shot =
{
    vr_ar_shot_ent = world;
    if(targ.vr_ar_type <= 0 || !targ.vr_ar_mask || dir == '0 0 0')
    {
        return;
    }
    vr_ar_shot_ent = targ;
    vr_ar_shot_time = time;
    vr_ar_shot_at = at;
    vector from = at;
    vector way = normalize(dir);
    float along = (at - vr_ar_fly_from) * way;
    if(vr_ar_fly_ent == targ && vr_ar_fly_time == time && along >= 0 && along <= VR_AR_SHOT_BACK &&
       vlen(at - (vr_ar_fly_from + way * along)) <= VR_AR_SHOT_NEAR)
    {
        // (a projectile's, each time its hit asks this frame (its touch's and its damage's): the query's line starts
        // where it was a frame before, and runs on through the body to where that line first crosses the skin)
        from = vr_ar_fly_from + way * VR_AR_SHOT_BACK;
    }
    vr_ar_shot_piece = ragdollbone(targ, from, QVR_ARMOR_SHOT, dir);
    vr_ar_shots = vr_ar_shots + 1;
};

// A shot's push (VR_ShotPush: a ragdoll's part, a knocked-down body's) where it meets `targ` at `at` going along `dir`:
// times vr_agarmor_impulse when an unbroken plate takes it first (the plate the same query finds), else 1. (A living
// body's flinch: T_DamageDeal's, the plates' share times the same.)
float(entity targ, vector at, vector dir) VR_AgArmor_PushScale =
{
    if(targ.vr_ar_type <= 0 || !targ.vr_ar_mask || dir == '0 0 0')
    {
        return 1;
    }
    float p = ragdollbone(targ, at, QVR_ARMOR_SHOT, normalize(dir));
    if(p < 0 || p >= VR_AR_PIECES || !VR_AgArmor_On(targ, p))
    {
        return 1;
    }
    return max(0, cvar_hget(cvarh_vr_agarmor_impulse));
};


float(entity targ, vector at) VR_AgArmor_PieceAt =
{
    if(vr_ar_shot_ent == targ && vr_ar_shot_time == time && vlen(at - vr_ar_shot_at) <= VR_AR_SHOT_NEAR)
    {
        return vr_ar_shot_piece;
    }
    return ragdollbone(targ, at, QVR_ARMOR_PIECE_AT);
};

// What plate `p` lets through of `damage` (`rule` vr_agarmor_rule): Plate Blocks All nothing, even as it breaks (a shot
// that meets a plate never hurts the body); the player's rule its share past the pool.
float(entity targ, float p, float damage, float rule) VR_AgArmor_Through =
{
    if(rule)
    {
        return 0;
    }
    float save = min(ceil(VR_AgArmor_Frac(targ.vr_ar_type) * damage), damage);
    return damage - min(save, targ.vr_ar_hp[p]);
};

// ----------------------------------------------------------------------------
// A hit's look where it struck (TraceAttack, spawn_touchblood, VR_HitBloodSplash: before its blood): on an unbroken
// plate, sparks; the share of the blood to draw returned: 1 no plate there, 0 the plate took it all, else what it let
// through (the rule's share of `damage`, as VR_AgArmor_Damage will take it).
float vr_ar_fx_sparks;   // the tests: plate hits that sparked
float vr_ar_fx_bled;     // the tests: of them, those that bled too
entity vr_ar_fx_ent;     // sparked at this frame (VR_AgArmor_Damage's own sparks then skipped)
float vr_ar_fx_time;

float(entity targ, vector org, vector vel, float damage) VR_AgArmor_HitFx =
{
    if(targ.vr_ar_type <= 0 || !targ.vr_ar_mask || damage <= 0)
    {
        return 1;
    }
    float p = VR_AgArmor_PieceAt(targ, org);
    if(p < 0 || p >= VR_AR_PIECES || !VR_AgArmor_On(targ, p))
    {
        return 1;
    }
    float through = max(0, VR_AgArmor_Through(targ, p, damage, cvar_hget(cvarh_vr_agarmor_rule))) / damage;
    vector out = vel == '0 0 0' ? '0 0 1' : normalize(vel); // (the blood's way: off the surface)
    particle2(org, (out + '0 0 0.3') * 60, QVR_PARTICLE_PRESET_SPARKS, max(6, min(16, 4 + damage)));
    vr_ar_fx_sparks = vr_ar_fx_sparks + 1;
    vr_ar_fx_bled = vr_ar_fx_bled + (through > 0);
    vr_ar_fx_ent = targ;
    vr_ar_fx_time = time;
    return through;
};

// ----------------------------------------------------------------------------
// The shotgun's pellets: each one's damage counted on the piece it met (AddMultiDamage; none: bare skin), the blast
// shared by them (ApplyMultiDamage's T_Damage): a plate takes its pellets' damage, not a count's share (a headshot
// pellet's is three of a body pellet's).
entity vr_ar_pel_ent;
float vr_ar_pel[10];
float vr_ar_pel_sum; // every pellet's damage on it

void() VR_AgArmor_PelletsClear =
{
    float i;
    vr_ar_pel_ent = world;
    vr_ar_pel_sum = 0;
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        vr_ar_pel[i] = 0;
    }
};

void(entity hit, float damage) VR_AgArmor_PelletAdd =
{
    if(hit.vr_ar_type <= 0 || !hit.vr_ar_mask || !VR_Hit_On(hit))
    {
        return;
    }
    vr_ar_pel_ent = hit;
    vr_ar_pel_sum = vr_ar_pel_sum + damage;
    float p = VR_AgArmor_PieceAt(hit, hit_at);
    if(p >= 0 && p < VR_AR_PIECES)
    {
        vr_ar_pel[p] = vr_ar_pel[p] + damage;
    }
};

// ----------------------------------------------------------------------------
// The hit.
float vr_ar_share[10];  // this hit's share per piece
float vr_ar_last_save;  // the last hit's damage the plates took (the tests)
entity vr_ar_block_inf; // a projectile a plate of vr_ar_block_ent took at vr_ar_block_time (its blast spares the body)
entity vr_ar_block_ent;
float vr_ar_block_time;
float vr_ar_last_piece; // the last single hit's piece (-1 none)

// A cut or a pop armed at `targ` (vr_decap_ent) under an unbroken plate is off.
void(entity targ) VR_AgArmor_Guard =
{
    if(targ.vr_ar_type <= 0 || !targ.vr_ar_mask || vr_decap_ent != targ || vr_decap_time != time)
    {
        return;
    }
    if(vr_decap_limb < 0)
    {
        if(cvar_hget(cvarh_vr_agarmor_helmet_head) && VR_AgArmor_On(targ, VR_AR_HELMET))
        {
            vr_decap_ent = world;
            dprint("agarmor: the helmet stops the head pop\n");
        }
        return;
    }
    if(!cvar_hget(cvarh_vr_agarmor_block_cuts))
    {
        return;
    }
    float p = VR_AgArmor_PieceAt(targ, vr_decap_at);
    if(p >= 2 && p < VR_AR_PIECES && VR_AgArmor_On(targ, p))
    {
        vr_decap_ent = world;
        dprint(sprintf("agarmor: piece %g stops the cut\n", p));
    }
};

float(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) VR_AgArmor_Damage =
{
    float i;
    float p;
    vector at = (targ.absmin + targ.absmax) * 0.5;
    vr_ar_last_save = 0;
    vr_ar_last_piece = -1;
    vr_ar_plate_hit_ent = world;
    if(targ.vr_ar_type <= 0 || damage <= 0)
    {
        return damage;
    }
    // Plate Blocks All: a projectile whose own hit a plate of this body just took (a plasma bolt's, a rocket's) spares it
    // its blast too (the plate met it at its face), even the plate broken by it. Before: the bolt's splash hurt the body
    // after its plate broke (test 10: plasma at every plate let 50 through).
    if(inflictor != world && inflictor != attacker && inflictor == vr_ar_block_inf && targ == vr_ar_block_ent &&
       time == vr_ar_block_time && cvar_hget(cvarh_vr_agarmor_rule))
    {
        vr_ar_last_save = damage;
        return 0;
    }
    if(!targ.vr_ar_mask)
    {
        return damage;
    }
    VR_AgArmor_Guard(targ);
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        vr_ar_share[i] = 0;
    }
    if(VR_Hit_Pellets() > 0 && vr_ar_pel_ent == targ && vr_ar_pel_sum > 0)
    {
        // A blast of pellets: each piece its pellets' share of the blast's damage.
        for(i = 0; i < VR_AR_PIECES; i++)
        {
            vr_ar_share[i] = vr_ar_pel[i] / vr_ar_pel_sum;
        }
        at = hit_at;
    }
    else if(VR_Hit_On(targ))
    {
        // One hit at a point (a shot's: the plate its line met first).
        p = VR_AgArmor_PieceAt(targ, hit_at);
        vr_ar_last_piece = p;
        at = hit_at;
        if(p >= 0 && p < VR_AR_PIECES)
        {
            vr_ar_share[p] = 1;
        }
        // A helmet takes half a headshot's extra off (the positional multiplier is in the damage already).
        float head = cvar_hget(cvarh_vr_headshot_mult);
        if(p == VR_AR_HELMET && VR_Hit_Zone(targ) == VR_REGION_HEAD && VR_AgArmor_On(targ, p) && cvar_hget(cvarh_vr_agarmor_helmet_head) &&
           cvar_hget(cvarh_vr_positional_damage) && head > 1)
        {
            damage = damage * (1 + (head - 1) * 0.5) / head;
        }
    }
    else if(vr_limb_wear_ent == targ && vr_limb_wear_time == time)
    {
        // A blow or a blade at a ragdoll's point (VR_Limb_RagdollHit, the lightning gun's bolt: VR_Limb_CorpseWear's).
        p = VR_AgArmor_PieceAt(targ, vr_limb_wear_at);
        vr_ar_last_piece = p;
        at = vr_limb_wear_at;
        if(p >= 0 && p < VR_AR_PIECES)
        {
            vr_ar_share[p] = 1;
        }
    }
    else
    {
        // A blast, a beam, no point: the pieces facing where it came from, each its share of the facing body.
        vector from = attacker.origin;
        if(inflictor != world && inflictor != targ)
        {
            from = (inflictor.absmin + inflictor.absmax) * 0.5;
        }
        vector d = from - at;
        float facing = 0;
        for(i = 0; i < VR_AR_PIECES; i++)
        {
            vector pos = limbpiece(targ, i, 10);
            if(vlen(d) < 1 || pos == '0 0 0' || (pos - at) * d >= 0)
            {
                facing = facing + VR_AgArmor_Share(i);
                vr_ar_share[i] = VR_AgArmor_Share(i);
            }
        }
        for(i = 0; i < VR_AR_PIECES; i++)
        {
            vr_ar_share[i] = vr_ar_share[i] / max(1, facing);
        }
    }
    if(vr_ar_last_piece >= 0 && VR_AgArmor_On(targ, vr_ar_last_piece))
    {
        vr_ar_plate_hit_ent = targ; // (a corpse's piece this hit wears pops nothing: VR_AgArmor_Holds)
        vr_ar_plate_hit_time = time;
    }
    float frac = VR_AgArmor_Frac(targ.vr_ar_type);
    float rule = cvar_hget(cvarh_vr_agarmor_rule);
    float saved = 0;
    float cost = VR_ARK_PlateCost(attacker); // (1; the Survival Arena's Can Opener: a plate pays twice what it saves)
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        if(vr_ar_share[i] > 0 && VR_AgArmor_On(targ, i))
        {
            float part = damage * vr_ar_share[i];
            float save = rule ? part : ceil(frac * part);
            save = min(save, part);
            // (the player's rule: no more than the pool; Plate Blocks All: the whole part, the pool or not: it breaks)
            if(!rule && save * cost > targ.vr_ar_hp[i])
            {
                save = targ.vr_ar_hp[i] / cost;
            }
            targ.vr_ar_hp[i] = max(0, targ.vr_ar_hp[i] - save * cost);
            saved = saved + save;
            if(targ.vr_ar_hp[i] <= 0)
            {
                VR_AgArmor_Break(targ, i);
            }
            VR_AgArmor_Wear(targ, i);
        }
    }
    if(saved > 0)
    {
        // The plate rings: sparks (where it struck, unless its hit's look sparked there already: VR_AgArmor_HitFx), a
        // ricochet.
        if(vr_ar_fx_ent != targ || vr_ar_fx_time != time)
        {
            particle2(at, '0 0 40', QVR_PARTICLE_PRESET_SPARKS, max(6, min(16, 4 + saved)));
            vr_ar_fx_sparks = vr_ar_fx_sparks + 1;
            vr_ar_fx_bled = vr_ar_fx_bled + (saved < damage);
        }
        float r = random();
        if(r < 0.33) sound(targ, CHAN_AUTO, "weapons/ric1.wav", 0.8, ATTN_NORM);
        else if(r < 0.66) sound(targ, CHAN_AUTO, "weapons/ric2.wav", 0.8, ATTN_NORM);
        else sound(targ, CHAN_AUTO, "weapons/ric3.wav", 0.8, ATTN_NORM);
    }
    vr_ar_last_save = saved;
    if(saved > 0 && vr_ar_last_piece >= 0)
    {
        vr_ar_block_inf = inflictor; // (its blast this frame: above)
        vr_ar_block_ent = targ;
        vr_ar_block_time = time;
    }
    return damage - saved;
};
