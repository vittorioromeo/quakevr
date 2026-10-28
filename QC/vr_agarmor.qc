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
    if(e.vr_ag_kind > 1 || deathmatch) // (VR_AM_GRUNT 0, VR_AM_ENFORCER 1: vr_armedmon.qc, compiled later)
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

// Its last second: fading out, then gone.
void() VR_AgArmor_DebrisFade =
{
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

// Piece `i` of `e` falls off as debris: a physics prop of its own model where it was, not grabbable.
entity(entity e, float i) VR_AgArmor_Debris =
{
    if(VR_AgArmor_Model(e, i) == "")
    {
        return world;
    }
    string m = VR_AgArmor_Model(e, i);
    precache_model(m); // (made as it loads: vr_agarmor.cpp derivedFile)
    vector at = limbpiece(e, i, 10);
    if(at == '0 0 0')
    {
        return world; // (no rig, no shell)
    }
    entity h = spawn();
    h.classname = "vr_ar_debris";
    setmodel(h, m);
    h.skin = max(0, e.vr_ar_type - 1) + 6; // (battered and scorched: make_agarmor.py's last three)
    setorigin(h, at);
    h.angles = limbpiece(e, i, 11);
    h.movetype = MOVETYPE_BOUNCE;
    h.solid = SOLID_NOT;
    h.takedamage = DAMAGE_NO;
    h.flags = 0; // (never FL_FORCEGRABBABLE)
    vector out = at - (e.absmin + e.absmax) * 0.5;
    out_z = 0;
    h.velocity = e.velocity + normalize(out) * (60 + 40 * random()) + '0 0 120';
    h.vr_rigid = 1;
    h.vr_prop_mass = i == 0 ? 4 : 1.5;
    h.vr_spin = [crandom() * 4, crandom() * 4, crandom() * 4];
    h.think = VR_AgArmor_DebrisFade;
    h.nextthink = time + max(0.1, cvar("vr_agarmor_debris_time"));
    vr_ar_debris_made = vr_ar_debris_made + 1;
    return h;
};

void(entity e, float i) VR_AgArmor_Break =
{
    e.vr_ar_hp[i] = 0;
    e.vr_ar_mask = e.vr_ar_mask - (e.vr_ar_mask & VR_AgArmor_Bit(i));
    sound(e, CHAN_AUTO, "items/armor1.wav", 1, ATTN_NORM);
    VR_AgArmor_Debris(e, i);
    dprint(sprintf("agarmor: %s (kind %g): piece %g broke (left %g)\n", e.classname, e.vr_ag_kind, i, e.vr_ar_mask));
};

void(entity e) VR_AgArmor_Shed =
{
    float i;
    for(i = 0; i < VR_AR_PIECES && e.vr_ar_type > 0; i++)
    {
        if(VR_AgArmor_On(e, i))
        {
            VR_AgArmor_Debris(e, i);
        }
    }
    e.vr_ar_mask = 0;
    e.vr_ar_type = 0;
    e.vr_ar_wear = 0;
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
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        if(vr_ar_share[i] > 0 && VR_AgArmor_On(targ, i))
        {
            float part = damage * vr_ar_share[i];
            float save = rule ? part : ceil(frac * part);
            save = min(save, min(part, targ.vr_ar_hp[i]));
            targ.vr_ar_hp[i] = targ.vr_ar_hp[i] - save;
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
        // The plate rings: sparks, a ricochet.
        particle2(at, '0 0 40', QVR_PARTICLE_PRESET_SPARKS, min(12, 3 + saved * 0.5));
        float r = random();
        if(r < 0.33) sound(targ, CHAN_AUTO, "weapons/ric1.wav", 0.8, ATTN_NORM);
        else if(r < 0.66) sound(targ, CHAN_AUTO, "weapons/ric2.wav", 0.8, ATTN_NORM);
        else sound(targ, CHAN_AUTO, "weapons/ric3.wav", 0.8, ATTN_NORM);
    }
    vr_ar_last_save = saved;
    return damage - saved;
};
