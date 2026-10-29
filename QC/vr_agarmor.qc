// vr_agarmor.qc -- armour for armed humanoids, P1 (ROUND23.md, "Armour for armed humanoids, P1"; the research:
// qvr-kit/scratch/research_agarmor.md). An armed grunt may wear up to ten plates (the engine's shells, vr_agarmor.cpp):
// each its own pool from its type's total (Quake's: green 100, yellow 150, red 200) split by the piece's share. A hit on
// a plate (its bone, as drawn: ragdollbone's QVR_ARMOR_PIECE_AT) is absorbed by the player's rule (vr_agarmor_rule 0:
// ceil(type x damage) from the pool) or stopped whole (1) while the pool lasts; a shotgun's pellets each count on their
// own piece; a blast or a hit with no point is shared by the pieces facing it. At 0 the plate breaks: its bit off (the
// engine sends the mask), a clank, and it falls as debris (its own model, "<model>#plate<piece>": a physics prop you
// can't force-grab, fading out after vr_agarmor_debris_time). Helmets halve a headshot's extra and stop head pops
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
        precache_sound("vr/derived/armor_break.wav"); // (VR_AgArmor_Break's)
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
    return x - floor(x);
};

// The skill's armour for one spawn at `at` (vr_ar_roll_type 0: none).
void(float sk, vector at) VR_AgArmor_Roll =
{
    vr_ar_seq = vr_ar_seq + 1;
    vr_ar_roll_type = 0;
    vr_ar_roll_mask = 0;
    sk = max(0, min(3, floor(sk)));
    string base = strcat("vr_agarmor_skill", ftos(sk));
    float chance = cvar(base);
    if(chance <= 0 || VR_AgArmor_Rand(0, at) >= chance)
    {
        return;
    }
    float best = max(1, min(3, floor(cvar(strcat(base, "_type")))));
    vr_ar_roll_type = 1 + min(best - 1, floor(VR_AgArmor_Rand(1, at) * best));
    float n = max(1, min(VR_AR_PIECES, floor(cvar(strcat(base, "_pieces")))));
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

void(entity e) VR_AgArmor_Spawn =
{
    if(e.vr_ag_kind > 1 || deathmatch || vr_ar_hold_roll) // (VR_AM_GRUNT 0, VR_AM_ENFORCER 1: vr_armedmon.qc, compiled later)
    {
        return; // (P2: the enforcer too, by the same chances: no free set)
    }
    VR_AgArmor_Roll(skill, e.origin);
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
string(entity e, float i) VR_AgArmor_Model =
{
    if(e.model == "progs/soldier.mdl")
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
    if(e.model == "progs/vrenforcer.mdl")
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
    if(e.model != "progs/vrgrunt.mdl") return "";
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

// A plate breaking: the armour pickup's chime backwards and lower (the engine makes it from items/armor1.wav as it loads:
// vr_audio.cpp derivedSounds), so it is never heard as armour picked up.
string VR_AR_BREAK_SOUND = "vr/derived/armor_break.wav";
string vr_ar_break_sound_last; // the tests: the last break's sound
float vr_ar_debris_grabs;      // the tests: debris taken in a hand

float() VR_AgArmor_DebrisTime = { return max(0.1, cvar("vr_agarmor_debris_time")); };

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
entity(entity e, float i, vector at, vector ang, vector vel) VR_AgArmor_DebrisAt =
{
    string m = VR_AgArmor_Model(e, i);
    if(m == "" || at == '0 0 0')
    {
        return world; // (no rig, no shell)
    }
    precache_model(m); // (made as it loads: vr_agarmor.cpp derivedFile)
    entity h = spawn();
    h.classname = "vr_ar_debris";
    setmodel(h, m);
    h.skin = max(0, e.vr_ar_type - 1) + 6; // (battered and scorched: make_agarmor.py's last three)
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
    VR_Carry_Setup(h);
    h.vr_rigid = 1;
    h.vr_prop_mass = i == 0 ? 4 : 1.5;
    h.vr_spin = [crandom() * 4, crandom() * 4, crandom() * 4];
    h.think = VR_AgArmor_DebrisFade;
    h.nextthink = time + VR_AgArmor_DebrisTime();
    vr_ar_debris_made = vr_ar_debris_made + 1;
    return h;
};

// Piece `i` of `e` falls off as debris where it is: out from the body and up.
entity(entity e, float i) VR_AgArmor_Debris =
{
    vector at = limbpiece(e, i, 10);
    vector out = at - (e.absmin + e.absmax) * 0.5;
    out_z = 0;
    return VR_AgArmor_DebrisAt(e, i, at, limbpiece(e, i, 11), e.velocity + normalize(out) * (60 + 40 * random()) + '0 0 120');
};

void(entity e, float i) VR_AgArmor_Break =
{
    e.vr_ar_hp[i] = 0;
    e.vr_ar_mask = e.vr_ar_mask - (e.vr_ar_mask & VR_AgArmor_Bit(i));
    precache_sound(VR_AR_BREAK_SOUND);
    sound(e, CHAN_AUTO, VR_AR_BREAK_SOUND, 1, ATTN_NORM);
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
// plate on the bones it would take keeps it on (vr_agarmor_block_cuts; the head: vr_agarmor_helmet_head): the plate
// breaks first.
float(entity e, float joint) VR_AgArmor_Holds =
{
    float i;
    if(e.vr_ar_type <= 0 || !e.vr_ar_mask)
    {
        return FALSE;
    }
    if(joint == 0)
    {
        return cvar("vr_agarmor_helmet_head") && VR_AgArmor_On(e, VR_AR_HELMET);
    }
    if(!cvar("vr_agarmor_block_cuts"))
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
    float p = ragdollbone(targ, org, QVR_ARMOR_PIECE_AT);
    if(p < 0 || p >= VR_AR_PIECES || !VR_AgArmor_On(targ, p))
    {
        return 1;
    }
    float save = cvar("vr_agarmor_rule") ? damage : ceil(VR_AgArmor_Frac(targ.vr_ar_type) * damage);
    save = min(min(save, damage), targ.vr_ar_hp[p]);
    float through = max(0, damage - save) / damage;
    vector out = vel == '0 0 0' ? '0 0 1' : normalize(vel); // (the blood's way: off the surface)
    particle2(org, (out + '0 0 0.3') * 60, QVR_PARTICLE_PRESET_SPARKS, max(6, min(16, 4 + damage)));
    vr_ar_fx_sparks = vr_ar_fx_sparks + 1;
    vr_ar_fx_bled = vr_ar_fx_bled + (through > 0);
    vr_ar_fx_ent = targ;
    vr_ar_fx_time = time;
    return through;
};

// ----------------------------------------------------------------------------
// The shotgun's pellets: each counted on its own piece (AddMultiDamage), the blast shared by them (ApplyMultiDamage's
// T_Damage).
entity vr_ar_pel_ent;
float vr_ar_pel[10];

void() VR_AgArmor_PelletsClear =
{
    float i;
    vr_ar_pel_ent = world;
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        vr_ar_pel[i] = 0;
    }
};

void(entity hit) VR_AgArmor_PelletAdd =
{
    if(hit.vr_ar_type <= 0 || !hit.vr_ar_mask || vr_hitpart_target != hit || vr_hitpart_time != time)
    {
        return;
    }
    vr_ar_pel_ent = hit;
    float p = ragdollbone(hit, vr_hitpart_pos, QVR_ARMOR_PIECE_AT);
    if(p >= 0 && p < VR_AR_PIECES)
    {
        vr_ar_pel[p] = vr_ar_pel[p] + 1;
    }
};

// ----------------------------------------------------------------------------
// The hit.
float vr_ar_share[10];  // this hit's share per piece
float vr_ar_last_save;  // the last hit's damage the plates took (the tests)
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
        if(cvar("vr_agarmor_helmet_head") && VR_AgArmor_On(targ, VR_AR_HELMET))
        {
            vr_decap_ent = world;
            dprint("agarmor: the helmet stops the head pop\n");
        }
        return;
    }
    if(!cvar("vr_agarmor_block_cuts"))
    {
        return;
    }
    float p = ragdollbone(targ, vr_decap_at, QVR_ARMOR_PIECE_AT);
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
    if(targ.vr_ar_type <= 0 || !targ.vr_ar_mask || damage <= 0)
    {
        return damage;
    }
    VR_AgArmor_Guard(targ);
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        vr_ar_share[i] = 0;
    }
    if(vr_hitpellets > 0 && vr_ar_pel_ent == targ)
    {
        // A blast of pellets: each piece its pellets' share.
        for(i = 0; i < VR_AR_PIECES; i++)
        {
            vr_ar_share[i] = vr_ar_pel[i] / vr_hitpellets;
        }
        at = vr_hitpart_pos;
    }
    else if(vr_hitpart_target == targ && vr_hitpart_time == time)
    {
        // One hit at a point.
        p = ragdollbone(targ, vr_hitpart_pos, QVR_ARMOR_PIECE_AT);
        vr_ar_last_piece = p;
        at = vr_hitpart_pos;
        if(p >= 0 && p < VR_AR_PIECES)
        {
            vr_ar_share[p] = 1;
        }
        // A helmet takes half a headshot's extra off (the positional multiplier is in the damage already).
        float head = cvar_hget(cvarh_vr_headshot_mult);
        if(p == VR_AR_HELMET && vr_hitpart == 1 && VR_AgArmor_On(targ, p) && cvar("vr_agarmor_helmet_head") &&
           cvar_hget(cvarh_vr_positional_damage) && head > 1)
        {
            damage = damage * (1 + (head - 1) * 0.5) / head;
        }
    }
    else if(vr_limb_wear_ent == targ && vr_limb_wear_time == time)
    {
        // A blow or a blade at a ragdoll's point (VR_Limb_RagdollHit, the lightning gun's bolt: VR_Limb_CorpseWear's).
        p = ragdollbone(targ, vr_limb_wear_at, QVR_ARMOR_PIECE_AT);
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
    float frac = VR_AgArmor_Frac(targ.vr_ar_type);
    float rule = cvar("vr_agarmor_rule");
    float saved = 0;
    float cost = VR_ARK_PlateCost(attacker); // (1; the Survival Arena's Can Opener: a plate pays twice what it saves)
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        if(vr_ar_share[i] > 0 && VR_AgArmor_On(targ, i))
        {
            float part = damage * vr_ar_share[i];
            float save = rule ? part : ceil(frac * part);
            save = min(save, part);
            if(save * cost > targ.vr_ar_hp[i])
            {
                save = targ.vr_ar_hp[i] / cost;
            }
            targ.vr_ar_hp[i] = targ.vr_ar_hp[i] - save * cost;
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
    return damage - saved;
};
