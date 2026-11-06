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
// crowbar's guard held: vr_parry_test_guard 1 and the mock hand across): the nearest monster's bite parried. AD's soul
// rams from 200-400 away and bites only after a ram (between 60 and 200 it circles you, its enemy AD's marker floating
// above you): it spawns at ram range, its ram lands (your guard down), you hold your ground, your guard parries the bite.
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

// 213: the nearest monster (a lost soul) bites you, your guard held: at least one bite parried within 8 s (it charges
// from where it spawns, then bites; a parried charge alone does not count).
entity vr_mrt_mon;
float vr_mrt_wait; // (213: how long it waits for the lost soul)
float vr_mrt_p0;     // (213: the parry count already judged)
float vr_mrt_bites;  // (213: its bites parried)
float vr_mrt_others; // (213: other parries: its charge's ramming touch)
float vr_mrt_guard;  // (213: the script's vr_parry_test_guard, restored at the end)
void() VR_MRT_ParryFrame =
{
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        if(!vr_mrt_wait)
        {
            vr_mrt_wait = time + 3;
            // AD's lost soul (the dispenser's 207) at its charge range: 200 to 400 away it rams you, nearer it strafes
            // round you and bites only within 60 (ad_LostCheckAttack), so it bites when a charge has brought it in.
            // Spawned nearer, it circles a still player for long seconds before it is far enough to charge.
            // (ahead, or the way round you with the most room)
            float d = 0;
            float bestyaw = 0;
            for(float a = 0; a < 360; a += 45)
            {
                makevectors('0 1 0' * (p.angles_y + a));
                traceline(p.origin + '0 0 16', p.origin + '0 0 16' + v_forward * 400, TRUE, p);
                if(min(300, trace_fraction * 400 - 48) > d + 1)
                {
                    d = min(300, trace_fraction * 400 - 48);
                    bestyaw = a;
                }
            }
            makevectors('0 1 0' * (p.angles_y + bestyaw));
            VR_AGT_Print(sprintf("meleerules: the lost soul spawns %g away, %g degrees left of ahead (its ram range "
                "200-400)", d, bestyaw));
            VR_MRT_Spawn(207, p.origin + v_forward * d + '0 0 16'); // (AD's monster readies itself a moment after it spawns)
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
        // (AD's monsters hunt by AD's own AI: id's FoundTarget would think their unset th_run, a NULL function)
        entity os = self;
        self = vr_mrt_mon;
        float woke = AD_TestWake(p);
        self = os;
        if(!woke)
        {
            VR_AGT_Hunt(vr_mrt_mon);
        }
        vr_mrt_wait = 0;
        vr_mrt_p0 = vr_parry_count;
        vr_mrt_bites = vr_mrt_others = 0;
        // Its ram lands (your guard down, the crowbar at your hip): a parried ram bounces it off you and its chomp
        // misses; a ram that lands leaves it on you, chomping (as in AD). Your guard comes up as it starts to chomp.
        vr_mrt_guard = cvar_hget(cvarh_vr_parry_test_guard);
        cvar_set("vr_parry_test_guard", "0");
        localcmd("vr_mock_hand main 0.25 0.8 -0.05 70 0 0\n");
        vr_agt_until = time + 8;
        vr_agt_step = 1;
        return;
    }
    entity mon = vr_mrt_mon;
    float chomp = mon.frame - ad_LOST_MELEE * ad_LOST_ANIMBLOCK;
    float chomping = chomp >= 0 && chomp < ad_LOST_ANIMBLOCK; // (ad_lost_melee2's frames)
    if(mon.frame >= ad_LOST_CHARGE * ad_LOST_ANIMBLOCK && chomp < ad_LOST_ANIMBLOCK)
    {
        // Its ram shoves you back (ad_lost_charge_touch: 200 a second) out of its bite's reach (60 on the flat, by
        // the chomp's third frame, 0.3 s on): you stand your ground here, as with a wall at your back.
        p.velocity = '0 0 0';
    }
    if(chomping && !cvar_hget(cvarh_vr_parry_test_guard))
    {
        cvar_set("vr_parry_test_guard", "1");
        localcmd("vr_mock_hand main 0.15 1.35 -0.35 0 90 0\n"); // (the crowbar across again)
    }
    if(vr_parry_count > vr_mrt_p0)
    {
        // A parry this frame: a bite if the soul shows its chomping frames, else its charge's ramming touch
        // (ad_lost_charge_touch, the charge frames until its next think).
        vr_mrt_p0 = vr_parry_count;
        if(vr_parry_count_by == mon && chomping)
        {
            vr_mrt_bites = vr_mrt_bites + 1;
        }
        else
        {
            vr_mrt_others = vr_mrt_others + 1;
        }
    }
    if(vr_mrt_bites > 0 || time >= vr_agt_until)
    {
        // (its enemy is AD's marker floating above you while it flies, ad_ai_enemytarget.qc: the real one through it)
        entity en = mon.enemy;
        string via = "";
        if(en.ad_classtype == ad_CT_ENEMYTARGET && en.state == ad_STATE_ON)
        {
            en = en.enemy;
            via = " (through AD's marker above it)";
        }
        VR_AGT_Check(sprintf("meleerules: %s's bite parried (%g bites parried, %g other parries; it is %g away, health "
            "%g, enemy %s%s)", mon.classname, vr_mrt_bites, vr_mrt_others, vlen(mon.origin - vr_agt_player.origin),
            mon.health, en.classname, via), vr_mrt_bites > 0);
        cvar_set("vr_parry_test_guard", ftos(vr_mrt_guard));
        cvar_set("vr_agrunt_test", "0");
    }
};
#endif
