// vr_attrs.qc -- enemy attributes (his ask, 2026-11-28: "a system to attach special attributes to enemies, like today's
// 'infected' for armed humanoids ... an 'elite' flag ... much much faster firing rate, reaction time, accuracy, and
// generally be much harder to fight ... 1.25x walking speed"; ROUND26.md, "elite").
//
// A monster's attributes are bits (.vr_attrs; VR_ATTR_* in vr_attrs_defs.qc), any of them together. The table below
// gives each its name, its factor on each stat (VR_ATTR_ST_*: 1 for none) and its look:
//   infected  Dawn of the Machine's (vr_mg3_infected.qc): its row in .vr_mg3_infected says what it rises as (the bit is
//             that row: set with VR_Attr_Set it takes the zombie's row; behaviour unchanged); no stats, no look.
//   elite     every stat from Tunables > Armed Humanoids > Elite (vr_elite_*); its look the elite dye (the engine's
//             humlook: .vr_hum_look's elite bit, sent to every client with the look; the table's "elite" rows).
// The stats are read where the armed humanoids' knobs are (vr_armedgrunt_ai.qc, VR_AG_AimPoint, the swing's speed),
// T_Damage's pain (any monster), and on gaining one its health and turning speed (given back on losing it).
// Who gets elites: the toolgun's Armed Humanoid page (Elite), Hard and Nightmare with "Elites in Hard and Nightmare"
// (vr_elite_spawn: a chance per armed humanoid, vr_elite_spawn_chance), the arena's later waves (vr_arena.qc
// VR_ARENA_EliteAt), a map's "vr_attrs" key, vr_attr_set (Debug > Tools).

// The table: an attribute's name.
string(float bit) VR_Attr_Name =
{
    if(bit == VR_ATTR_INFECTED) { return "infected"; }
    if(bit == VR_ATTR_ELITE) { return "elite"; }
    return "";
};

// ... and its factor on a stat.
float(float bit, float stat) VR_Attr_StatOf =
{
    if(bit != VR_ATTR_ELITE)
    {
        return 1; // (infected: as it was)
    }
    if(stat == VR_ATTR_ST_FIRERATE) { return max(0.05, cvar_hget(cvarh_vr_elite_firerate)); }
    if(stat == VR_ATTR_ST_REACTION) { return max(0, cvar_hget(cvarh_vr_elite_reaction)); }
    if(stat == VR_ATTR_ST_ACCURACY) { return max(0.05, cvar_hget(cvarh_vr_elite_accuracy)); }
    if(stat == VR_ATTR_ST_AGGRESSION) { return max(0, cvar_hget(cvarh_vr_elite_aggression)); }
    if(stat == VR_ATTR_ST_SPEED) { return max(0, cvar_hget(cvarh_vr_elite_speed)); }
    if(stat == VR_ATTR_ST_MELEE) { return min(3, max(0.1, cvar_hget(cvarh_vr_elite_melee))); }
    if(stat == VR_ATTR_ST_PAIN) { return min(1, max(0, cvar_hget(cvarh_vr_elite_pain))); }
    if(stat == VR_ATTR_ST_AIMLAG) { return max(0, cvar_hget(cvarh_vr_elite_aimlag)); }
    if(stat == VR_ATTR_ST_TURN) { return max(0.1, cvar_hget(cvarh_vr_elite_turn)); }
    if(stat == VR_ATTR_ST_HEALTH) { return max(0.1, cvar_hget(cvarh_vr_elite_health)); }
    return 1;
};

float(entity e) VR_Attrs =
{
    float a = e.vr_attrs - (e.vr_attrs & VR_ATTR_INFECTED);
    return a | (e.vr_mg3_infected ? VR_ATTR_INFECTED : 0);
};

float(entity e, float stat) VR_Attr_Mult =
{
    float a = VR_Attrs(e);
    if(!a)
    {
        return 1; // (none: exactly as before)
    }
    float m = 1;
    float bit;
    for(bit = 1; bit <= VR_ATTR_LAST; bit = bit * 2)
    {
        if(a & bit)
        {
            m = m * VR_Attr_StatOf(bit, stat);
        }
    }
    return m;
};

float(entity e) VR_Attr_ShrugsPain =
{
    float m = VR_Attr_Mult(e, VR_ATTR_ST_PAIN);
    return m < 1 && random() >= m; // (no random number drawn without an attribute)
};

void(entity e) VR_Attr_Look =
{
    float look = e.vr_hum_look - (e.vr_hum_look & VR_HUM_LOOK_ELITE);
    if((e.vr_attrs & VR_ATTR_ELITE) && e.vr_ag && !(look & 4096))
    {
        // (no weapon colour of its own: its kind's row alone, category 0, for the dye; AD's death guard and crossbow
        // knight (rows 4, 5) have no dye's masks: the bit gives them the engine's marker alone, vr_humlook.cpp eliteGlow)
        e.vr_hum_look = (look ? look : e.vr_ag_kind * 16) + VR_HUM_LOOK_ELITE;
        return;
    }
    if(e.vr_ag && !(look & 15))
    {
        look = 0; // (the kind's row alone was for the dye)
    }
    e.vr_hum_look = look;
};

// What gaining (`on`) or losing an attribute does at once (the stats read as they are used do the rest).
void(entity e, float bit, float on) VR_Attr_Gain =
{
    if(bit != VR_ATTR_ELITE)
    {
        return;
    }
    float h = VR_Attr_StatOf(bit, VR_ATTR_ST_HEALTH);
    float t = VR_Attr_StatOf(bit, VR_ATTR_ST_TURN);
    if(!on)
    {
        h = 1 / h;
        t = 1 / t;
    }
    if(e.health > 0)
    {
        e.health = max(1, ceil(e.health * h));
    }
    if(e.max_health > 0)
    {
        e.max_health = max(1, ceil(e.max_health * h));
    }
    e.yaw_speed = (e.yaw_speed ? e.yaw_speed : 20) * t; // (20: walkmonster_start_go's own when none was set)
};

void(entity e, float bit, float on) VR_Attr_Set =
{
    if(bit == VR_ATTR_INFECTED)
    {
        // (its row: the zombie's when given, kept when it had one; none when taken)
        if(on && !e.vr_mg3_infected)
        {
            e.vr_mg3_infected = MG3_INFECTED_ZOMBIE;
            VR_Infect_Precache(MG3_INFECTED_ZOMBIE);
        }
        else if(!on)
        {
            e.vr_mg3_infected = 0;
        }
        return;
    }
    float had = (e.vr_attrs & bit) != 0;
    if(on && !had)
    {
        e.vr_attrs = e.vr_attrs | bit;
        VR_Attr_Gain(e, bit, TRUE);
    }
    else if(!on && had)
    {
        e.vr_attrs = e.vr_attrs - bit;
        VR_Attr_Gain(e, bit, FALSE);
    }
    VR_Attr_Look(e);
};

// Hard and Nightmare's elites (vr_elite_spawn, default off): an armed humanoid spawned by the map or a replacement may
// roll `chance`; never the tools' (the toolgun, the tests, the dummies: vr_agr_hold), the arena's (its own wave rule),
// deathmatch's. No random number drawn when off.
float(entity e, float chance) VR_Elite_Rolls =
{
    if(!cvar_hget(cvarh_vr_elite_spawn) || skill < 2 || vr_agr_hold > 0 || vr_ar_hold_roll || !e.vr_ag || deathmatch ||
       (e.vr_attrs & VR_ATTR_ELITE))
    {
        return FALSE;
    }
    return random() < chance;
};

void(entity e) VR_Attr_Spawned =
{
    float keyed = e.vr_attrs; // (a map's "vr_attrs" key: taken up as given now, with its gains)
    e.vr_attrs = 0;
    float bit;
    for(bit = 1; bit <= VR_ATTR_LAST; bit = bit * 2)
    {
        if(keyed & bit)
        {
            VR_Attr_Set(e, bit, TRUE);
        }
    }
    if(!(e.vr_attrs & VR_ATTR_ELITE) && VR_Run_ForcesElite(e))
    {
        VR_Attr_Set(e, VR_ATTR_ELITE, TRUE); // (the run's All Elites: no roll)
    }
    else if(VR_Elite_Rolls(e, cvar_hget(cvarh_vr_elite_spawn_chance)))
    {
        VR_Attr_Set(e, VR_ATTR_ELITE, TRUE);
        dprint(sprintf("elite: %s rolled elite (skill %g)\n", e.classname, skill));
    }
};

// The toolgun's armed humanoid just made (vr_toolgun_pick.inc, after its Infected With): elite or not (its page's Elite).
void(entity e, float xElite) VR_Attr_Toolgun =
{
    VR_Attr_Set(e, VR_ATTR_ELITE, xElite != 0);
};

// The engine's names (vr_attr_set): the table's rows (bit 2^i), and its command: attribute `bit` given (`on`) or taken
// on entity `m`, or (world) on the monster nearest the first player. Prints what it did.
float() VR_Attr_UiCount = { return 2; };
string(float i) VR_Attr_UiName = { return VR_Attr_Name(pow(2, max(0, floor(i)))); };
void(float bit, float on, entity m) VR_Attr_Cmd =
{
    if(!m)
    {
        entity p = VR_Player();
        float best = 999999;
        entity e;
        for(e = nextent(world); e; e = nextent(e))
        {
            if((e.flags & FL_MONSTER) && e.health > 0 && p && vlen(e.origin - p.origin) < best)
            {
                best = vlen(e.origin - p.origin);
                m = e;
            }
        }
    }
    if(!m || !(m.flags & FL_MONSTER))
    {
        bprint("vr_attr_set: no monster there\n");
        return;
    }
    VR_Attr_Set(m, bit, on);
    bprint(sprintf("vr_attr_set: %s %s %s: attributes %g, look %g, health %g\n", m.classname, on ? "now" : "no longer",
        VR_Attr_Name(bit), VR_Attrs(m), m.vr_hum_look, m.health));
};
