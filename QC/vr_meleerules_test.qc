#ifdef QVR_TESTS
// vr_meleerules_test.qc -- enemy melee under one set of rules (vr_meleerules.qc; ROUND24.md, "Melee rules").
// `vr_agrunt_test 212` (vrtesthall; case 2 puts a spawn, a grunt and a dragon beside and ahead: the dispenser's
// spawns 11, 10, 19): an armed humanoid's sword under the shared rules:
//   case 0: you stand still ahead of it: its first blow lands (the control);
//   case 1: as its swing starts you step to its side (100 degrees off its facing, as far as you were): the blow misses;
//   case 2: Show Melee Arcs (vr_enemy_melee_arc_show 1) with every monster near you after you: each kind's rule set on
//   it and shown (the sword's arc with its weapon's reach, the spawn's leap, the dragon's all round, the vore's shove
//   cone); the engine's count of what it drew: the script's vr_enemy_melee_arc_status.
// `vr_agrunt_test 213` (AD's start; the script puts a lost soul ahead first: vr_test_spawn 207, impulse 241; your
// crowbar's guard held: vr_parry_test_guard 1 and the mock hand across): the nearest monster's bite parried.
// Each `agtest: PASS|FAIL meleerules ...`; ends on `agtest: test 212 over: P passed, F failed` (213 likewise).

float vr_mrt_case;
float vr_mrt_s0;      // its strikes at the case's start
float vr_mrt_d0;      // the damage armed humanoids dealt you then
float vr_mrt_stepped; // when you stepped aside (case 1; 0: not yet)
float vr_mrt_keep_show;

// The dispenser's monster `choice` (buttons.qc VR_EnemyDispenser_Spawn) at `at`.
void(float choice, vector at) VR_MRT_Spawn =
{
    entity os = self;
    self = spawn();
    setorigin(self, at);
    self.angles_y = vectoyaw(vr_agt_player.origin - at);
    VR_EnemyDispenser_Spawn(choice);
    self = os;
};

// Every monster within 800 units of you hunts you (case 2's).
void(entity p) VR_MRT_HuntAll =
{
    entity m = findradius(p.origin, 800);
    while(m)
    {
        if((m.flags & FL_MONSTER) && m.health > 0)
        {
            VR_AGT_Hunt(m);
        }
        m = m.chain;
    }
};

// The first monster within 800 units of you with rule `rule` shown in the last half second, or world.
entity(entity p, float rule) VR_MRT_Shown =
{
    entity m = findradius(p.origin, 800);
    while(m)
    {
        if((m.flags & FL_MONSTER) && m.health > 0 && m.vr_melee_rule == rule && m.vr_melee_shown > time - 0.5 &&
           m.vr_melee_reach > 0)
        {
            return m;
        }
        m = m.chain;
    }
    return world;
};

void() VR_MRT_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float c = vr_mrt_case;
    if(vr_agt_step == 0)
    {
        if(c == 0)
        {
            vr_mrt_keep_show = cvar_hget(cvarh_vr_enemy_melee_arc_show);
        }
        if(c <= 1)
        {
            VR_AGPT_Clear();
            vr_agt_grunt = g = VR_AGT_Spawn(WID_SWORD, 56);
            VR_AGT_Hunt(g);
            vr_mrt_s0 = g.vr_ag_strikes;
            vr_mrt_d0 = vr_ag_player_dmg;
            vr_mrt_stepped = 0;
            vr_agt_until = time + 8;
        }
        else
        {
            // (a spawn to your left, a vore to your right (a grunt here is armed), a dragon ahead: the dispenser's spawns)
            cvar_set("vr_enemy_melee_arc_show", "1");
            makevectors('0 1 0' * p.angles_y);
            VR_MRT_Spawn(11, p.origin - v_right * 160);
            makevectors('0 1 0' * p.angles_y);
            VR_MRT_Spawn(10, p.origin + v_right * 140);
            makevectors('0 1 0' * p.angles_y);
            VR_MRT_Spawn(19, p.origin + v_forward * 300 + '0 0 96');
            vr_agt_until = time + 1;
        }
        vr_agt_step = 1;
        return;
    }
    if(c == 2 && vr_agt_step == 1)
    {
        if(time >= vr_agt_until)
        {
            VR_MRT_HuntAll(p);
            vr_agt_until = time + 1;
            vr_agt_step = 2;
        }
        return;
    }
    if(c <= 1)
    {
        if(c == 1 && !vr_mrt_stepped && g.vr_ag_act == VR_AGP_SWING)
        {
            // (to its right, 100 degrees off its facing, as far as you were)
            vector to = p.origin - g.origin;
            to_z = 0;
            float d = vlen(to);
            vector a = '0 0 0';
            a_y = g.angles_y - 100;
            makevectors(a);
            setorigin(p, g.origin + v_forward * d + '0 0 1' * (p.origin_z - g.origin_z));
            vr_mrt_stepped = time;
            VR_AGT_Print(sprintf("meleerules: stepped aside (%.0f units, 100 deg off its facing)", d));
        }
        if(g.vr_ag_strikes > vr_mrt_s0)
        {
            float dmg = vr_ag_player_dmg - vr_mrt_d0;
            if(c == 0)
            {
                VR_AGT_Check(sprintf("meleerules: a sword's blow from ahead lands (%.0f damage)", dmg), dmg > 0);
            }
            else
            {
                VR_AGT_Check(sprintf("meleerules: a side-step out of a sword's swing dodges it (stepped %g, %.0f damage)",
                    vr_mrt_stepped > 0, dmg), vr_mrt_stepped > 0 && dmg == 0);
            }
            vr_mrt_case = c + 1;
            vr_agt_step = 0;
            return;
        }
        if(time >= vr_agt_until)
        {
            VR_AGT_Check(sprintf("meleerules: case %g timed out (no blow struck)", c), FALSE);
            vr_mrt_case = c + 1;
            vr_agt_step = 0;
        }
        return;
    }
    if(time < vr_agt_until)
    {
        return;
    }
    entity arc = VR_MRT_Shown(p, VR_MELEE_RULE_ARC);
    entity ag = world;
    entity m = findradius(p.origin, 800);
    while(m)
    {
        if(m.flags & FL_MONSTER)
        {
            VR_AGT_Print(sprintf("meleerules: %s armed %g, %.0f units, after you %g, rule %g shown %.2f s ago", m.classname,
                m.vr_ag, vlen(m.origin - p.origin), m.enemy == p, m.vr_melee_rule, time - m.vr_melee_shown));
        }
        if(m.vr_ag && m.vr_melee_shown > time - 0.5 && m.vr_melee_rule == VR_MELEE_RULE_ARC)
        {
            ag = m;
        }
        m = m.chain;
    }
    entity leap = VR_MRT_Shown(p, VR_MELEE_RULE_LEAP);
    entity round = VR_MRT_Shown(p, VR_MELEE_RULE_ROUND);
    entity shove = VR_MRT_Shown(p, VR_MELEE_RULE_SHOVE);
    VR_AGT_Check(sprintf("meleerules: Show Melee Arcs: an arc (%s), the armed sword's (reach %.0f, arc %.0f)",
        arc.classname, ag.vr_melee_reach, ag.vr_melee_arc), arc != world && ag != world && ag.vr_melee_reach > 62);
    VR_AGT_Check(sprintf("meleerules: Show Melee Arcs: a leap (%s, reach %.0f)", leap.classname, leap.vr_melee_reach),
        leap.classname == "monster_tarbaby");
    VR_AGT_Check(sprintf("meleerules: Show Melee Arcs: all round (%s, reach %.0f, arc %.0f)", round.classname,
        round.vr_melee_reach, round.vr_melee_arc), round.classname == "monster_dragon" && round.vr_melee_arc == 180);
    // (a vore's: the arc's growth with its size; a grunt-sized one's the shove's old 60)
    entity os = self;
    self = g;
    float cone = VR_MeleeArc_ShoveAngle();
    self = os;
    VR_AGT_Check(sprintf("meleerules: Show Melee Arcs: a shove's cone (%s, cone %.1f, reach %.0f; a grunt-sized one's %.1f)",
        shove.classname, shove.vr_melee_arc, shove.vr_melee_reach, cone),
        shove != world && shove.vr_melee_arc > 0 && fabs(cone - 60) < 1);
    localcmd("vr_enemy_melee_arc_status\n");
    cvar_set("vr_enemy_melee_arc_show", ftos(vr_mrt_keep_show));
    vr_mrt_case = 0;
    cvar_set("vr_agrunt_test", "0");
};

// 213: the nearest monster (a lost soul) bites you, your guard held: at least one bite parried within 8 s.
entity vr_mrt_mon;
float vr_mrt_wait; // (213: how long it waits for the lost soul)
float vr_mrt_p0;
void() VR_MRT_ParryFrame =
{
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        if(!vr_mrt_wait)
        {
            vr_mrt_wait = time + 3;
            makevectors('0 1 0' * p.angles_y);
            VR_MRT_Spawn(207, p.origin + v_forward * 96 + '0 0 16'); // (AD's lost soul: the dispenser's 207) // (AD's monster readies itself a moment after it spawns)
        }
        vr_mrt_mon = world;
        float best = 1000;
        entity m = nextent(world);
        while(m)
        {
            if(m.classname == "monster_lostsoul" && m.health > 0 && m.takedamage && vlen(m.origin - p.origin) < best)
            {
                vr_mrt_mon = m;
                best = vlen(m.origin - p.origin);
            }
            m = nextent(m);
        }
        if(!vr_mrt_mon)
        {
            if(time >= vr_mrt_wait)
            {
                m = findradius(p.origin, 400);
                while(m)
                {
                    VR_AGT_Print(sprintf("meleerules: near you %s health %g takedamage %g", m.classname, m.health,
                        m.takedamage));
                    m = m.chain;
                }
                VR_AGT_Check("meleerules: no lost soul to bite you", FALSE);
                vr_mrt_wait = 0;
                cvar_set("vr_agrunt_test", "0");
            }
            return;
        }
        VR_AGT_Hunt(vr_mrt_mon);
        vr_mrt_wait = 0;
        vr_mrt_p0 = vr_parry_count;
        vr_agt_until = time + 8;
        vr_agt_step = 1;
        return;
    }
    float n = vr_parry_count - vr_mrt_p0;
    if(n > 0 || time >= vr_agt_until)
    {
        VR_AGT_Check(sprintf("meleerules: %s's bite parried (%g parries, the last by %s)", vr_mrt_mon.classname, n,
            vr_parry_count_by.classname), n > 0 && vr_parry_count_by == vr_mrt_mon);
        cvar_set("vr_agrunt_test", "0");
    }
};
#endif
