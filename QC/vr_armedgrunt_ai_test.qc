#ifdef QVR_TESTS
// vr_armedgrunt_ai_test.qc -- armed humanoids' AI behaviour knobs (vr_armedgrunt_ai.qc; ROUND22.md, "Armed humanoids'
// AI behaviour"). `vr_agrunt_test 86` (Debug > Tests > Armed Grunts: AI Behaviour), vrtesthall from its start, the
// player kept alive and still: case by case one armed humanoid ahead with the knobs set as the case has them (skill
// set as the case has it, the map's back after), each printing its numbers (`agtest: ai: <case>: ...`); the checks
// compare cases (each knob moves its measure the way it should, Normal at the defaults as the knobs at 1). Every case
// ends on its own condition (a first shot, a count of shots, its seconds of watching); the run ends on "agtest: test 84
// over". `vr_agrunt_test 87` (Armed Grunts: Strafe Lean): a still grunt moved sideways at 0..100 u/s; the engine's
// numbers by speed printed (vr_agrunt_lean_info: its lean, its pelvis's roll, its chest's); scratch1 1: a screenshot
// at 80 u/s and one standing (the eyeshots).

float VR_AGAIT_REACT = 0;  // the time to its first shot (a shotgun at 260)
float VR_AGAIT_AIM = 1;    // its shots' aim error and spread, its hits (a gun at 300, 8 shots)
float VR_AGAIT_PRESS = 2;  // its distance and shots (a shotgun at 400, 8 s)
float VR_AGAIT_BACK = 3;   // its backing off (a nailgun at 150, 5 s)
float VR_AGAIT_SEEK = 4;   // unarmed 136 ahead, a shotgun lying 140 behind it: does it go for it (4 s)
float VR_AGAIT_N = 25;

float vr_agait_case;
float vr_agait_t0;
float vr_agait_shots0;
float vr_agait_first;  // its first shot's time after the hunt (-1 none)
float vr_agait_dsum;
float vr_agait_dn;
float vr_agait_backs0;
float vr_agait_aim_sum;
float vr_agait_aim_n;
float vr_agait_hits;
float vr_agait_hp;
float vr_agait_went;
float vr_agait_skill0;
vector vr_agait_start;
float vr_agait_yaw0;
float vr_agait_val[25]; // each case's measure (the checks compare them)
float vr_agait_press_shots; // aggressiveness 1's shots
entity vr_agait_lying;      // the weapon lying (the weapon search's)
float vr_agait_dmg;

// The case `c`: its kind of measure, its weapon (-1 fists), its skill, and its knobs (aggression, accuracy, reaction,
// retreat, fire rate, impact, rearm range, punch).
float vr_agait_kind;
float vr_agait_wid;
float vr_agait_native;
string vr_agait_name;

void(float c) VR_AGAIT_Setup =
{
    float sk = 1;
    float agg = 1, acc = 1, react = 1, ret = 1, fr = 1, imp = 1, rng = 384, punch = 0;
    vr_agait_native = 0;
    vr_agait_wid = WID_SHOTGUN;
    if(c <= 6)
    {
        vr_agait_kind = VR_AGAIT_REACT;
        if(c == 0) { vr_agait_name = "reaction 1 (Normal)"; }
        if(c == 1) { vr_agait_name = "reaction 0.3"; react = 0.3; }
        if(c == 2) { vr_agait_name = "reaction 2"; react = 2; }
        if(c == 3) { vr_agait_name = "reaction, Easy"; sk = 0; }
        if(c == 4) { vr_agait_name = "reaction, Hard"; sk = 2; }
        if(c == 5) { vr_agait_name = "reaction, Nightmare"; sk = 3; }
        if(c == 6) { vr_agait_name = "reaction, Hard, impact 0"; sk = 2; imp = 0; }
    }
    else if(c <= 12)
    {
        vr_agait_kind = VR_AGAIT_AIM;
        if(c == 7) { vr_agait_name = "accuracy 1"; }
        if(c == 8) { vr_agait_name = "accuracy 2"; acc = 2; }
        if(c == 9) { vr_agait_name = "accuracy 0.5"; acc = 0.5; }
        if(c == 10) { vr_agait_name = "accuracy, Nightmare"; sk = 3; }
        if(c == 11) { vr_agait_name = "accuracy 1, its own gun"; vr_agait_wid = WID_GRUNTGUN; vr_agait_native = 1; }
        if(c == 12) { vr_agait_name = "accuracy 0.5, its own gun"; vr_agait_wid = WID_GRUNTGUN; vr_agait_native = 1; acc = 0.5; }
    }
    else if(c <= 16)
    {
        vr_agait_kind = VR_AGAIT_PRESS;
        if(c == 13) { vr_agait_name = "aggressiveness 1"; }
        if(c == 14) { vr_agait_name = "aggressiveness 2"; agg = 2; }
        if(c == 15) { vr_agait_name = "aggressiveness 0.5"; agg = 0.5; }
        if(c == 16) { vr_agait_name = "fire rate 2"; fr = 2; }
    }
    else if(c <= 19)
    {
        vr_agait_kind = VR_AGAIT_BACK;
        vr_agait_wid = WID_NAILGUN;
        if(c == 17) { vr_agait_name = "retreat 1"; }
        if(c == 18) { vr_agait_name = "retreat 0"; ret = 0; }
        if(c == 19) { vr_agait_name = "retreat 2"; ret = 2; }
    }
    else
    {
        vr_agait_kind = VR_AGAIT_SEEK;
        vr_agait_wid = -1;
        if(c == 20) { vr_agait_name = "weapon search 384"; }
        if(c == 21) { vr_agait_name = "weapon search 100"; rng = 100; }
        if(c == 22) { vr_agait_name = "weapon search 384, punches within 200"; punch = 200; }
        if(c == 23) { vr_agait_name = "weapon search 384, punches within 100"; punch = 100; }
        if(c == 24) { vr_agait_name = "weapon search 384, Nightmare"; sk = 3; }
    }
    skill = sk;
    cvar_set("skill", ftos(sk)); // (StartFrame takes it each frame)
    cvar_set("vr_agrunt_ai_aggression", ftos(agg));
    cvar_set("vr_agrunt_ai_accuracy", ftos(acc));
    cvar_set("vr_agrunt_ai_reaction", ftos(react));
    cvar_set("vr_agrunt_ai_retreat", ftos(ret));
    cvar_set("vr_agrunt_ai_firerate", ftos(fr));
    cvar_set("vr_agrunt_ai_skill", ftos(imp));
    cvar_set("vr_agrunt_rearm_range", ftos(rng));
    cvar_set("vr_agrunt_rearm_punch", ftos(punch));
    cvar_set("vr_agrunt_range", vr_agait_kind == VR_AGAIT_AIM ? "0" : "1"); // (the aim: it comes straight on, in sight)
};

void() VR_AGAIT_Defaults =
{
    cvar_set("vr_agrunt_ai_aggression", "1");
    cvar_set("vr_agrunt_ai_accuracy", "1");
    cvar_set("vr_agrunt_ai_reaction", "1");
    cvar_set("vr_agrunt_ai_retreat", "1");
    cvar_set("vr_agrunt_ai_firerate", "1");
    cvar_set("vr_agrunt_ai_skill", "1");
    cvar_set("vr_agrunt_rearm_range", "384");
    cvar_set("vr_agrunt_rearm_punch", "0");
    cvar_set("vr_agrunt_range", "1");
};

// The skill table (the effective knobs per skill at the default impact, then at 0): printed; Normal all 1, impact 0
// all 1 (but Nightmare's reaction: 1 there too).
void() VR_AGAIT_Table =
{
    float ok = TRUE;
    float imp;
    for(imp = 1; imp >= 0; imp = imp - 1)
    {
        cvar_set("vr_agrunt_ai_skill", ftos(imp));
        float sk;
        for(sk = 0; sk <= 3; sk++)
        {
            skill = sk;
            string row = sprintf("ai: table, impact %g, skill %g: aggressiveness %.2f accuracy %.2f reaction %.2f", imp, sk,
                VR_AGAI_Aggression(), VR_AGAI_Accuracy(), VR_AGAI_Reaction());
            VR_AGT_Print(strcat(row, sprintf(" retreat %.2f fire rate %.2f parry chance %.2f reaction %.2f", VR_AGAI_Retreat(),
                VR_AGAI_FireRate(), VR_AGPar_SkillChance(), VR_AGPar_SkillReact())));
            if((sk == 1 || imp == 0) && (VR_AGAI_Aggression() != 1 || VR_AGAI_Accuracy() != 1 || VR_AGAI_Reaction() != 1 ||
               VR_AGAI_Retreat() != 1 || VR_AGAI_FireRate() != 1 || VR_AGPar_SkillChance() != 1 || VR_AGPar_SkillReact() != 1))
            {
                ok = FALSE;
            }
        }
    }
    cvar_set("vr_agrunt_ai_skill", "1");
    VR_AGT_Check("ai: table: Normal and impact 0 every knob 1:", ok);
};

void() VR_AGAIT_Begin =
{
    entity p = vr_agt_player;
    if(vr_agt_step != 3)
    {
        vr_agait_lying = world;
        VR_AGPT_Clear();
    }
    setorigin(p, vr_agait_start);
    p.velocity = '0 0 0';
    p.angles_y = vr_agait_yaw0;
    p.v_angle_y = vr_agait_yaw0;
    p.fixangle = TRUE;
    VR_AGAIT_Setup(vr_agait_case);
    float dist = (vr_agait_kind == VR_AGAIT_REACT) ? 260 : (vr_agait_kind == VR_AGAIT_PRESS) ? 400 :
                 (vr_agait_kind == VR_AGAIT_AIM) ? 300 : (vr_agait_kind == VR_AGAIT_BACK) ? 150 : 160;
    if(vr_agait_kind == VR_AGAIT_SEEK && !vr_agait_lying)
    {
        // (a grunt dropping its shotgun 140 behind where the unarmed one stands; it lands first: VR_AGAIT_Frame)
        entity src = VR_AGT_Spawn(WID_SHOTGUN, 300);
        vr_agait_lying = VR_AG_DropHeld(src, '0 0 0');
        VR_AGT_Remove(src);
        vr_agait_t0 = time;
        return;
    }
    vr_agt_grunt = VR_AGT_Spawn(vr_agait_wid, dist);
    entity g = vr_agt_grunt;
    if(vr_agait_native)
    {
        VR_AGR_MarkNative(g); // (a replacement's own gun: its monster's own shots)
    }
    VR_AGT_Hunt(g);
    vr_agait_t0 = time;
    vr_agait_shots0 = g.vr_ag_total_shots;
    vr_agait_first = -1;
    vr_agait_dsum = 0;
    vr_agait_dn = 0;
    vr_agait_backs0 = vr_ag_range_backs;
    vr_agait_aim_sum = 0;
    vr_agait_aim_n = 0;
    vr_fire_dev_sum = 0;
    vr_fire_dev_n = 0;
    vr_agait_hits = 0;
    vr_agait_hp = p.health;
    vr_agait_dmg = vr_ag_player_dmg;
    vr_agait_went = 0;
};

void() VR_AGAIT_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        vr_agait_start = p.origin;
        vr_agait_yaw0 = p.angles_y;
        vr_agait_skill0 = skill;
        VR_AGAIT_Table();
        vr_agait_case = rint(cvar("scratch1")) > 0 ? rint(cvar("scratch1")) - 1 : 0; // (scratch1 N: case N - 1 alone)
        vr_agt_step = 1;
    }
    if(vr_agt_step == 1)
    {
        VR_AGAIT_Begin();
        vr_agt_step = vr_agait_lying ? 3 : 2;
        return;
    }
    if(vr_agt_step == 3)
    {
        // (the weapon search's shotgun landed: still, 0.5 s on; or 2 s: then the unarmed one, hunting you)
        if((vr_agait_lying.velocity != '0 0 0' || time - vr_agait_t0 < 0.5) && time - vr_agait_t0 < 2)
        {
            return;
        }
        VR_AGT_Print(sprintf("ai: the shotgun lies %.0f from you", vlen(vr_agait_lying.origin - p.origin)));
        VR_AGAIT_Begin();
        vr_agt_step = 2;
        return;
    }
    p.velocity_x = 0;
    p.velocity_y = 0;
    p.health = 500;
    float alive = (g && g.health > 0);
    float shots = alive ? g.vr_ag_total_shots - vr_agait_shots0 : 0;
    float t = time - vr_agait_t0;
    if(alive)
    {
        vr_agait_dsum = vr_agait_dsum + vlen(g.origin - p.origin);
        vr_agait_dn = vr_agait_dn + 1;
        if(shots > 0 && vr_agait_first < 0)
        {
            vr_agait_first = t;
        }
        if(shots > vr_agait_aim_n && !vr_agait_native)
        {
            vr_agait_aim_sum = vr_agait_aim_sum + vr_ag_last_aimdev; // (its last shot's aim off you)
            vr_agait_aim_n = shots;
        }
        if(vr_ag_player_dmg > vr_agait_dmg)
        {
            vr_agait_hits = vr_agait_hits + 1; // (a frame it hurt you: a volley one)
        }
        if(g.vr_ag_seek)
        {
            vr_agait_went = 1;
        }
    }
    p.health = 500;
    vr_agait_hp = p.health;
    vr_agait_dmg = vr_ag_player_dmg;
    // Each case's end: its first shot (reaction, 6 s at most), 8 shots (aim, 30 s at most), its seconds.
    float done = !alive;
    if(vr_agait_kind == VR_AGAIT_REACT) { done = (done || (shots > 0) || (t > 6)); }
    else if(vr_agait_kind == VR_AGAIT_AIM) { done = (done || (shots >= 8) || (t > 30)); }
    else if(vr_agait_kind == VR_AGAIT_PRESS) { done = (done || (t >= 8)); }
    else if(vr_agait_kind == VR_AGAIT_BACK) { done = (done || (t >= 5)); }
    else { done = (done || vr_agait_went || (t >= 4)); }
    if(!done)
    {
        return;
    }
    float c = vr_agait_case;
    float mean = vr_agait_dsum / max(1, vr_agait_dn);
    string s = strcat("ai: ", vr_agait_name);
    float ok = TRUE;
    if(vr_agait_kind == VR_AGAIT_REACT)
    {
        vr_agait_val[c] = vr_agait_first;
        float want = VR_AGAI_Reaction(); // (the case's skill still set)
        s = strcat(s, sprintf(": first shot after %.2f s (reaction %.2f s)", vr_agait_first, want));
        ok = (vr_agait_first >= want - 0.05 && vr_agait_first <= want + 2.5);
        if(c == 1) { ok = (ok && vr_agait_first < vr_agait_val[0]); }
        if(c == 2) { ok = (ok && vr_agait_first > vr_agait_val[0]); }
        if(c == 5) { ok = (ok && vr_agait_first < 0.7); }
    }
    else if(vr_agait_kind == VR_AGAIT_AIM)
    {
        float aim = vr_agait_aim_sum / max(1, vr_agait_aim_n);
        float dev = vr_fire_dev_sum / max(1, vr_fire_dev_n);
        vr_agait_val[c] = vr_agait_native ? dev : aim + dev;
        s = strcat(s, sprintf(": %g shots in %.1f s%s, aim off %.2f deg, pellets off it %.2f deg, hits %g", shots, t,
            alive ? "" : " (it died)", aim, dev, vr_agait_hits));
        ok = (shots >= 8 || vr_agait_native);
        if(c == 8) { ok = (ok && vr_agait_val[8] < 0.7 * vr_agait_val[7]); }
        if(c == 9) { ok = (ok && vr_agait_val[9] > 1.4 * vr_agait_val[7]); }
        if(c == 10) { ok = (ok && vr_agait_val[10] < vr_agait_val[7]); }
        if(c == 12) { ok = (ok && shots > 0 && vr_agait_val[12] > 1.25 * vr_agait_val[11]); }
    }
    else if(vr_agait_kind == VR_AGAIT_PRESS)
    {
        vr_agait_val[c] = mean;
        if(c == 13)
        {
            vr_agait_press_shots = shots;
        }
        s = strcat(s, sprintf(": mean distance %.0f, %g shots in 8 s", mean, shots));
        if(c == 14) { ok = (mean < vr_agait_val[13] - 20); }
        if(c == 15) { ok = (mean > vr_agait_val[13] + 20); }
        if(c == 16) { ok = shots > vr_agait_press_shots; }
    }
    else if(vr_agait_kind == VR_AGAIT_BACK)
    {
        float backs = vr_ag_range_backs - vr_agait_backs0;
        vr_agait_val[c] = mean;
        s = strcat(s, sprintf(": mean distance %.0f, %g steps backing off", mean, backs));
        if(c == 17) { ok = (backs > 0 && mean > 170); }
        if(c == 18) { ok = backs == 0; }
        if(c == 19) { ok = mean > vr_agait_val[17]; }
    }
    else
    {
        s = strcat(s, sprintf(": %s (after %.1f s)", vr_agait_went ? "went for the weapon" : "left it", t));
        ok = ((c == 21 || c == 22) ? !vr_agait_went : vr_agait_went);
    }
    VR_AGT_Check(strcat(s, ":"), ok);
    vr_agait_case = vr_agait_case + 1;
    vr_agt_step = 1;
    if(vr_agait_case >= VR_AGAIT_N || rint(cvar("scratch1")) > 0)
    {
        VR_AGPT_Clear();
        VR_AGAIT_Defaults();
        VR_AGAIT_Table(); // (again at the end: the first can be lost to the map's load in a run's log)
        skill = vr_agait_skill0;
        cvar_set("skill", ftos(vr_agait_skill0));
        setorigin(p, vr_agait_start);
        cvar_set("vr_agrunt_test", "0");
    }
};

// `vr_agrunt_test 87`: a shotgun grunt 120 ahead, side-on to nothing (facing you), its own think stopped (it stands
// in its frame), moved sideways by the test at 0, 20, ..., 100 u/s for 1.5 s each (the way turned each phase, back
// and forth about where it stood); vr_agrunt_lean_info cleared at the start and printed at the end (the engine's
// numbers: the lean, its pelvis's roll by it and its chest's as solved, by sideways speed). scratch1 1: a screenshot
// at the 80 u/s phase's middle and one at the standing phase's.
float vr_aglt_phase;
float vr_aglt_t0;
vector vr_aglt_home;
vector vr_aglt_side;
float VR_AGLT_PHASES = 7; // 0, 20, 40, 60, 80, 100 u/s, then standing again (the ease out)
float VR_AGLT_SECS = 1.5;

void() VR_AGLT_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        VR_AGPT_Clear();
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 120);
        g = vr_agt_grunt;
        p.flags = p.flags | FL_NOTARGET;
        vr_aglt_phase = 0;
        vr_aglt_t0 = time;
        vr_aglt_home = '0 0 0';
        localcmd("vr_agrunt_lean_info clear\n");
        vr_agt_step = 1;
        return;
    }
    p.velocity_x = 0;
    p.velocity_y = 0;
    if(vr_agt_step == 1)
    {
        if(!(g.flags & FL_ONGROUND) && time - vr_aglt_t0 < 2)
        {
            return; // (landed first: its spawn's drop)
        }
        g.think = SUB_Null; // (it stands; its weapon's think still poses it)
        g.nextthink = 0;
        g.velocity = '0 0 0';
        vr_aglt_home = g.origin;
        makevectors(g.angles);
        vr_aglt_side = v_right;
        vr_aglt_side_z = 0;
        vr_aglt_t0 = time;
        vr_agt_step = 2;
        return;
    }
    float t = time - vr_aglt_t0;
    float speed = vr_aglt_phase < 6 ? vr_aglt_phase * 20 : 0;
    float dir = (vr_aglt_phase & 1) ? -1 : 1;
    // (back and forth within 40 of where it stood: turned at the ends)
    vector at = g.origin + vr_aglt_side * (dir * speed * frametime);
    if(fabs((at - vr_aglt_home) * vr_aglt_side) > 40)
    {
        vr_aglt_side = -vr_aglt_side;
        at = g.origin + vr_aglt_side * (dir * speed * frametime);
    }
    setorigin(g, at);
    if(cvar("scratch1") == 1 && (vr_aglt_phase == 4 || vr_aglt_phase == 0) && t >= VR_AGLT_SECS * 0.5 &&
       t - frametime < VR_AGLT_SECS * 0.5)
    {
        localcmd("screenshot\n");
    }
    if(t < VR_AGLT_SECS)
    {
        return;
    }
    VR_AGT_Print(sprintf("lean: phase %g: %g u/s sideways done", vr_aglt_phase, speed));
    vr_aglt_phase = vr_aglt_phase + 1;
    vr_aglt_t0 = time;
    if(vr_aglt_phase < VR_AGLT_PHASES)
    {
        return;
    }
    localcmd("vr_agrunt_lean_info\n");
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    VR_AGT_Check(sprintf("lean: %g phases moved, vr_agrunt_strafe_lean %g (the numbers: aglean lines):", vr_aglt_phase,
        cvar("vr_agrunt_strafe_lean")), g != world);
    VR_AGPT_Clear();
    cvar_set("vr_agrunt_test", "0");
};
#endif
