#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_armedgrunt_downtug_test.qc (ROUND22.md, "Armed humanoids: the tug after a knockdown; shots on the weapon"):
//
// Test 74: grabbing a knocked-down armed humanoid's weapon (his note: "after an armed humanoid is shoved to the ground,
// sometimes grabbing their weapon doesn't work: I'm stuck in the tug-of-war state"). A fresh shotgun humanoid
// (vr_agrunt_test_kind) 80 ahead, you unseen; knocked down by method 0 a shove (topple), 1 a shoulder charge, 2 lifted and
// slammed down, 3 judo-thrown; then, phase 0 lying still, phase 1 as it gets up: you step in, your empty main hand put on
// its weapon (by try: 0 behind its grip, the stock; 1 at its grip; 2 the barrel), the grip pressed (the real grab:
// VR_AG_PropHandTouch), then pulled back (try 0 at 0.6 m/s, 1 at 3 m/s, 2 you step back at 0.5 m/s). Each try prints
// `downtug <method> <phase> <point> <pull>: <outcome>`: yours at once / won (a tug, pulled out) / let go (the tug lost,
// the Let Go Past rule) / STUCK (a tug or the hand's hold on still 7 s on) / nothing (the grab took nothing) / no state
// (it dropped its weapon or never got there: not counted). PASS: none stuck, 80% taken or let go (its weapon kept
// however hard it lands: vr_agrunt_fall_keep_impact 100 meanwhile). scratch1 n > 0: method n - 1
// only; scratch2 n > 0: phase n - 1 only.
//
// Test 75: a shot on the held weapon alone hurts only the weapon (his note: "if a player's shot precisely hits an armed
// humanoid's weapon only, only the weapon should be affected"). A fresh humanoid 200 ahead (5000 health, idle, its shot
// disarm rolls off), for each weapon it holds (shotgun, nailgun, axe, sword, chainsaw, super shotgun) and each shot of
// yours (a shotgun pellet, a revolver slug, a nail, a super shotgun's 14 pellets): fired from 96 units off through the
// middle of its weapon towards its body (whether its body is behind printed). With vr_agrunt_shotdisarm_block 0 (before)
// and 1 (after): the damage it took, the shots stopped. Then each shot at its head, clear of the weapon (the body hit as
// ever). PASS (block 1): a single shot through the weapon 0 damage and stopped; the super shotgun's per pellet (the
// stopped ones none, the rest as ever); the head shots hurt it.
// ----------------------------------------------------------------------------

float vr_agdt_m;        // test 74: the method
float vr_agdt_ph;       // ... the phase
float vr_agdt_try;      // ... the try (its point and pull)
float vr_agdt_t0;
float vr_agdt_still;    // ... frames lying still
float vr_agdt_wasdown;  // ... its knockdown seen
float vr_agdt_track;    // ... frames the hand followed its weapon
float vr_agdt_press;
float vr_agdt_tugged;   // ... a tug begun
float vr_agdt_ok, vr_agdt_n, vr_agdt_stuck, vr_agdt_none, vr_agdt_nost;
float vr_agdt_won, vr_agdt_yank, vr_agdt_lost;
float vr_agdt_wonsum;   // ... the won tugs' time
vector vr_agdt_home;
float vr_agdt_keep_impact; // vr_agrunt_fall_keep_impact as it was (the test: its weapon kept however hard it lands)
float vr_agdt_kd0, vr_agdt_kd1, vr_agdt_rag1; // ... its knockdown as its state was reached, at the press; a ragdoll then

string(float m) VR_AGDT_MName = { return m == 0 ? "shove" : m == 1 ? "charge" : m == 2 ? "lift" : "judo"; };
float(float t) VR_AGDT_Point = { return t == 0 ? 0.05 : t == 1 ? 0.25 : 0.85; };
string(float t) VR_AGDT_PullName = { return t == 0 ? "slow 0.6 m/s" : t == 1 ? "fast 3 m/s" : "step back 0.5 m/s"; };

// The next try (FALSE: all done).
float() VR_AGDT_Next =
{
    for(;;)
    {
        vr_agdt_try = vr_agdt_try + 1;
        if(vr_agdt_try > 2)
        {
            vr_agdt_try = 0;
            vr_agdt_ph = vr_agdt_ph + 1;
        }
        if(vr_agdt_ph > 1)
        {
            vr_agdt_ph = 0;
            vr_agdt_m = vr_agdt_m + 1;
        }
        if(vr_agdt_m > 3)
        {
            return FALSE;
        }
        float a = cvar("scratch1"), b = cvar("scratch2");
        if((a <= 0 || vr_agdt_m == a - 1) && (b <= 0 || vr_agdt_ph == b - 1))
        {
            return TRUE;
        }
    }
    return FALSE;
};

void(entity p, entity g) VR_AGDT_TryEnd =
{
    if(g.vr_ag_tug_pl)
    {
        VR_AG_TugEnd(g, FALSE, "test");
    }
    localcmd("-grabmain\n");
    cvar_set("vr_mock_hand_pull", "0");
    cvar_set("vr_mock_hand_to_absolute", "0");
    VR_AGPT_EmptyHand(p);
    vr_agt_step = VR_AGDT_Next() ? 0 : 9;
};

void() VR_AGDT_Down =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float u = VRMetersToUnits();
    if(vr_agt_step == 0)
    {
        if(vr_agdt_m == 0 && vr_agdt_ph == 0 && vr_agdt_try == 0 && vr_agdt_n + vr_agdt_nost == 0)
        {
            vr_agdt_home = p.origin;
            float a = cvar("scratch1"), b = cvar("scratch2");
            if(!((a <= 0 || a == 1) && (b <= 0 || b == 1)))
            {
                vr_agdt_try = -1; // (the first wanted)
                if(!VR_AGDT_Next())
                {
                    vr_agt_step = 9;
                    return;
                }
            }
            if(!cvar("vr_agrunt_debug"))
            {
                cvar_set("vr_agrunt_debug", "0.5"); // (the engine's reading of the fist's gap: vr_agrunt_grab_gap)
            }
            vr_agdt_keep_impact = cvar("vr_agrunt_fall_keep_impact");
            cvar_set("vr_agrunt_fall_keep_impact", "100");
        }
        p.flags = p.flags | FL_NOTARGET;
        VR_AGPT_EmptyHand(p);
        localcmd("-grabmain\n");
        cvar_set("vr_mock_hand_pull", "0");
        cvar_set("vr_mock_hand_lift", "0");
        cvar_set("vr_mock_hand_to_absolute", "0");
        cvar_set("vr_agrunt_disarm_yank", "0"); // (no grab before its press)
        p.mainhand_lastgrabtime = time - 2;
        p.mainhand_lastreleasetime = time - 1;
        VR_AGPT_Clear();
        setorigin(p, vr_agdt_home);
        p.velocity = '0 0 0';
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, 80);
        g.health = 400; // (a landing's impact damage never kills it)
        vr_agdt_t0 = time;
        vr_agdt_still = 0;
        vr_agdt_wasdown = 0;
        vr_agdt_tugged = 0;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 9)
    {
        float avg = vr_agdt_won > 0 ? vr_agdt_wonsum / vr_agdt_won : 0;
        string s1 = sprintf("downtug: %g/%g taken or let go (at once %g, won %g in %.2f s on average, ", vr_agdt_ok, vr_agdt_n,
            vr_agdt_yank, vr_agdt_won, avg);
        string s2 = sprintf("let go %g; stuck %g, nothing %g):", vr_agdt_lost, vr_agdt_stuck, vr_agdt_none);
        // (PASS: never stuck, and most tries taken; a grab that took nothing is tried again, not stuck)
        VR_AGT_Check(strcat(s1, s2), vr_agdt_stuck == 0 && vr_agdt_n > 0 && vr_agdt_ok >= vr_agdt_n * 0.8);
        if(vr_agdt_nost > 0)
        {
            VR_AGT_Print(sprintf("downtug: %g tries had no state (dropped its weapon, never got there)", vr_agdt_nost));
        }
        VR_AGPT_Clear();
        VR_AGPT_EmptyHand(p);
        localcmd("-grabmain\n");
        cvar_set("vr_mock_hand_to_absolute", "0");
        cvar_set("vr_mock_hand_pull", "0");
        if(cvar("vr_agrunt_debug") == 0.5)
        {
            cvar_set("vr_agrunt_debug", "0");
        }
        cvar_set("vr_agrunt_disarm_yank", "1");
        cvar_set("vr_agrunt_fall_keep_impact", ftos(vr_agdt_keep_impact));
        setorigin(p, vr_agdt_home);
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        vr_agdt_m = 0;
        vr_agdt_ph = 0;
        vr_agdt_try = 0;
        vr_agdt_ok = 0;
        vr_agdt_n = 0;
        vr_agdt_stuck = 0;
        vr_agdt_none = 0;
        vr_agdt_nost = 0;
        vr_agdt_won = 0;
        vr_agdt_yank = 0;
        vr_agdt_lost = 0;
        vr_agdt_wonsum = 0;
        cvar_set("vr_agrunt_test", "0");
        return;
    }
    string name = sprintf("downtug %s %s %s %s", VR_AGDT_MName(vr_agdt_m), vr_agdt_ph ? "getting-up" : "down",
        vr_agdt_try == 0 ? "stock" : vr_agdt_try == 1 ? "grip" : "barrel", VR_AGDT_PullName(vr_agdt_try));
    if(vr_agt_step == 1)
    {
        if(!g.takedamage || time < vr_agdt_t0 + 0.3)
        {
            return; // (started)
        }
        makevectors('0 1 0' * g.angles_y);
        vector back = v_forward * -1;
        float r = 0;
        entity oldself = self;
        if(vr_agdt_m == 0)
        {
            VR_Knockdown_Start(g);
            r = ragdollshovetopple(g, back, 1, FALSE, 40);
        }
        else if(vr_agdt_m == 1)
        {
            r = VR_KdTrig_Bump(VR_KDT_TACKLE, g, p, cvar("vr_kdtrig_tackle_weight"), back, 1, "the test's charge");
        }
        else if(vr_agdt_m == 2)
        {
            self = p;
            r = VR_FoeGrab_Lift(g, cvar("vr_foegrab_lift_up"), 1);
            self = oldself;
        }
        else
        {
            self = p;
            r = VR_FoeGrab_Throw(g, g.angles_y + 90, -1);
            self = oldself;
        }
        vr_agdt_t0 = time;
        vr_agt_step = 2;
        return;
    }
    float t = time - vr_agdt_t0;
    if(vr_agt_step == 2)
    {
        if(vr_agdt_m == 2 && t >= 0.35 && t - frametime < 0.35)
        {
            ragdollfling(g, '0 0 -1' * u); // (let go: down onto the floor)
        }
        if(g.vr_knockdown)
        {
            vr_agdt_wasdown = 1;
        }
        if(!g.vr_ag_prop || t > 10 || (t > 2 && !vr_agdt_wasdown))
        {
            VR_AGT_Print(sprintf("%s: no state (%s; down %g, knockdown %g, %.1f s)", name, g.vr_ag_prop ? "never got there" :
                "it dropped its weapon", vr_agdt_wasdown, g.vr_knockdown, t));
            vr_agdt_nost = vr_agdt_nost + 1;
            VR_AGDT_TryEnd(p, g);
            return;
        }
        float there = FALSE;
        if(vr_agdt_ph == 0)
        {
            vr_agdt_still = (g.vr_knockdown == 1 && vlen(g.vr_ag_fall_v) < 10 && t > 0.5) ? vr_agdt_still + 1 : 0;
            there = vr_agdt_still >= 15;
        }
        else
        {
            there = (vr_agdt_wasdown && (g.vr_knockdown == 2 || !g.vr_knockdown)); // (its get-up begun, or just up)
        }
        if(!there)
        {
            return;
        }
        vr_agdt_kd0 = g.vr_knockdown;
        // You step in beside its weapon (28 units off the point, away from its body), the hand following it.
        VR_AG_WeaponLine(g.vr_ag_prop);
        vector pt = vr_ag_wa + (vr_ag_wb - vr_ag_wa) * VR_AGDT_Point(vr_agdt_try);
        vector out = pt - g.origin;
        out_z = 0;
        if(vlen(out) < 1)
        {
            out = vr_agdt_home - g.origin;
            out_z = 0;
        }
        vector at = pt + normalize(out) * 28;
        at_z = p.origin_z;
        setorigin(p, at);
        p.velocity = '0 0 0';
        cvar_set("vr_mock_hand_to_absolute", "1");
        vr_agdt_track = 0;
        vr_agt_step = 3;
        return;
    }
    if(vr_agt_step == 3)
    {
        if(!g.vr_ag_prop)
        {
            VR_AGT_Print(sprintf("%s: no state (its weapon gone before the press)", name));
            vr_agdt_nost = vr_agdt_nost + 1;
            VR_AGDT_TryEnd(p, g);
            return;
        }
        localcmd(sprintf("vr_mock_hand_to main agweaponpalm %g 0 either\n", VR_AGDT_Point(vr_agdt_try)));
        vr_agdt_track = vr_agdt_track + 1;
        VR_AG_WeaponLine(g.vr_ag_prop);
        float near = VR_AG_SegDist(p.handpos, p.handpos, vr_ag_wa, vr_ag_wb) <= 5;
        if(vr_agdt_track < 12 || (!near && vr_agdt_track < 90))
        {
            return;
        }
        if(!near)
        {
            VR_AGT_Print(sprintf("%s: no state (the hand never got onto it: %.1f units off)", name,
                VR_AG_SegDist(p.handpos, p.handpos, vr_ag_wa, vr_ag_wb)));
            vr_agdt_nost = vr_agdt_nost + 1;
            VR_AGDT_TryEnd(p, g);
            return;
        }
        vr_agdt_kd1 = g.vr_knockdown;
        vr_agdt_rag1 = isragdoll(g);
        cvar_set("vr_agrunt_disarm_yank", "1");
        // (The grip pressed as the engine presses it: its press's time its own, every touch in that frame racing for the
        // hand. A press faked by its time (test 67's) gave the weapon to a hand whose grip wasn't down yet: dropped.)
        localcmd("+grabmain\n");
        vr_agdt_press = time;
        vr_ag_tug_why = "";
        vr_agt_step = 4;
        return;
    }
    // (step 4) Pulled back until it is yours, let go, or 7 s (stuck).
    float tp = time - vr_agdt_press;
    float tug = g.vr_ag_tug_pl == p;
    float hold = ragdollreach(p, cVR_MainHand) >= 0;
    float yours = p.weapon == WID_SHOTGUN;
    if(tug)
    {
        vr_agdt_tugged = 1;
    }
    if(cvar("vr_agrunt_debug") >= 1 && tp < 0.06)
    {
        VR_AGT_Print(sprintf("downtug %.3f s: your weapon %g, your hand holds %s, its weapon %s, tug %g (%s)", tp, p.weapon,
            p.mainhand_held ? p.mainhand_held.classname : "nothing", g.vr_ag_prop ? "held" : "gone", tug, vr_ag_tug_why));
    }
    if(tp > 0.05)
    {
        float speed = vr_agdt_try == 0 ? 0.6 : vr_agdt_try == 1 ? 3 : 0;
        cvar_set("vr_mock_hand_pull", ftos(min(1, (tp - 0.05) * speed)));
        if(vr_agdt_try == 2 && tp < 2.05)
        {
            vector away = p.origin - g.origin;
            away_z = 0;
            setorigin(p, p.origin + normalize(away) * 0.5 * frametime * u);
        }
    }
    string outcome = "";
    if(yours)
    {
        outcome = vr_agdt_tugged ? "won" : "yours at once";
    }
    else if(!tug && !hold && tp > 0.15 && vr_agdt_tugged)
    {
        outcome = "let go";
    }
    else if(!tug && !hold && tp > 0.3 && !vr_agdt_tugged)
    {
        outcome = "nothing";
    }
    else if(tp > 7)
    {
        outcome = "STUCK";
    }
    if(outcome == "")
    {
        return;
    }
    string s1 = sprintf("%s: %s after %.2f s (%s; tug %g, hold %g; ", name, outcome, tp, vr_ag_tug_why, tug, hold);
    string s2 = sprintf("knockdown %g, at the press %g, ragdoll %g; its weapon %s, ", vr_agdt_kd0, vr_agdt_kd1, vr_agdt_rag1,
        g.vr_ag_prop ? "held" : "gone");
    VR_AGT_Print(strcat(strcat(s1, s2), sprintf("your hand holds %s)", p.mainhand_held ? p.mainhand_held.classname :
        "nothing")));
    vr_agdt_n = vr_agdt_n + 1;
    if(outcome == "won") { vr_agdt_won = vr_agdt_won + 1; vr_agdt_wonsum = vr_agdt_wonsum + tp; vr_agdt_ok = vr_agdt_ok + 1; }
    else if(outcome == "yours at once") { vr_agdt_yank = vr_agdt_yank + 1; vr_agdt_ok = vr_agdt_ok + 1; }
    else if(outcome == "let go") { vr_agdt_lost = vr_agdt_lost + 1; vr_agdt_ok = vr_agdt_ok + 1; }
    else if(outcome == "STUCK") { vr_agdt_stuck = vr_agdt_stuck + 1; }
    else { vr_agdt_none = vr_agdt_none + 1; }
    VR_AGDT_TryEnd(p, g);
};

// ----------------------------------------------------------------------------
// Test 75
float vr_agws_w;        // the held weapon's case
float vr_agws_s;        // the shot's case (4: at its head)
float vr_agws_block;    // 0 before, 1 after
float vr_agws_h0;
float vr_agws_ab0;
float vr_agws_body;     // its body behind the weapon on the shot's way
float vr_agws_until;
float vr_agws_keep_block, vr_agws_keep_chance, vr_agws_keep_scale;
float vr_agws_pass, vr_agws_fail;
entity vr_agws_nail; // the nail fired (scratch3 1: its flight printed)

float(float c) VR_AGWS_Wid =
{
    return c == 0 ? WID_SHOTGUN : c == 1 ? WID_NAILGUN : c == 2 ? WID_AXE : c == 3 ? WID_SWORD : c == 4 ? WID_CHAINSAW :
        WID_SUPER_SHOTGUN;
};
string(float s) VR_AGWS_SName =
{
    return s == 0 ? "pellet" : s == 1 ? "revolver slug" : s == 2 ? "nail" : s == 3 ? "super shotgun (14 pellets)" :
        s == 4 ? "pellet at its head" : "nail at its head";
};

// The shot's case fired by you from `from` along `dir`.
void(vector from, vector dir) VR_AGWS_Fire =
{
    entity pl = vr_agt_player;
    entity oldself = self;
    self = pl;
    if(vr_agws_s == 1)
    {
        VR_Revolver_SlugFrom(from, dir, 400);
    }
    else if(vr_agws_s == 2 || vr_agws_s == 5)
    {
        vr_agws_nail = launch_spike(from, dir);
        if(cvar("scratch3") > 0)
        {
            traceline(from, from + dir * 30, FALSE, pl);
            VR_AGT_Print(sprintf("weaponshot nail from %v dir %v: at %v, a line ahead stops at %.2f on %s (startsolid %g)", from, dir,
                vr_agws_nail.origin, trace_fraction, trace_ent.classname, trace_startsolid));
        }
    }
    else
    {
        float n = vr_agws_s == 3 ? 14 : 1;
        vector spread = vr_agws_s == 3 ? '0.035 0.025 0' : '0 0 0';
        vr_pellet_damage = cvar_hget(vr_agws_s == 3 ? cvarh_vr_dmg_super_shotgun : cvarh_vr_dmg_shotgun);
        VR_DamageCredit_FromHand(pl, pl, cVR_MainHand);
        VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
        vr_tracer_hand = cVR_MainHand;
        vr_tracer_from = from;
        FireBulletsImpl(n, dir, spread, from, 400, FALSE);
        VR_ClearHitKind();
        vr_pellet_damage = 0;
    }
    self = oldself;
};

void() VR_AGWS_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        if(vr_agws_w == 0 && vr_agws_s == 0 && vr_agws_block == 0)
        {
            vr_agws_keep_block = cvar("vr_agrunt_shotdisarm_block");
            vr_agws_keep_chance = cvar("vr_agrunt_shotdisarm_chance");
            vr_agws_keep_scale = cvar("vr_agrunt_shotdisarm_chance_scale");
            cvar_set("vr_agrunt_shotdisarm_chance", "0"); // (its weapon stays in its hand: every shot on it)
            cvar_set("vr_agrunt_shotdisarm_chance_scale", "0");
            vr_agws_pass = 0;
            vr_agws_fail = 0;
        }
        if(vr_agws_w > 5)
        {
            cvar_set("vr_agrunt_shotdisarm_block", ftos(vr_agws_keep_block));
            cvar_set("vr_agrunt_shotdisarm_chance", ftos(vr_agws_keep_chance));
            cvar_set("vr_agrunt_shotdisarm_chance_scale", ftos(vr_agws_keep_scale));
            VR_AGT_Print(sprintf("weaponshot: %g passed, %g failed", vr_agws_pass, vr_agws_fail));
            VR_AGPT_Clear();
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            vr_agws_w = 0;
            vr_agws_s = 0;
            vr_agws_block = 0;
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        p.flags = p.flags | FL_NOTARGET;
        cvar_set("vr_agrunt_shotdisarm_block", ftos(vr_agws_block));
        VR_AGPT_Clear();
        vr_agt_grunt = g = VR_AGT_Spawn(VR_AGWS_Wid(vr_agws_w), 200);
        g.health = 5000;
        vr_agws_until = time + 0.4;
        vr_agt_step = 1;
        return;
    }
    if(cvar("scratch3") > 0 && vr_agws_nail && vr_agws_nail.classname == "spike" && vr_agt_step == 2)
    {
        VR_AGT_Print(sprintf("weaponshot nail at %v (%.1f from its weapon, %.1f from it), touched %s", vr_agws_nail.origin,
            vlen(vr_agws_nail.origin - g.vr_ag_prop.origin), vlen(vr_agws_nail.origin - g.origin), vr_agws_nail.enemy.classname));
    }
    if(time < vr_agws_until)
    {
        return;
    }
    entity w = g.vr_ag_prop;
    if(vr_agt_step == 1)
    {
        if(!w || !g.takedamage)
        {
            VR_AGT_Print(sprintf("weaponshot %s: no weapon", VR_AGT_Name(VR_AGWS_Wid(vr_agws_w))));
            vr_agws_fail = vr_agws_fail + 1;
            vr_agws_w = vr_agws_w + 1;
            vr_agt_step = 0;
            return;
        }
        VR_AG_WeaponLine(w);
        vector mid = vr_ag_wa + (vr_ag_wb - vr_ag_wa) * 0.7; // (past its grip hand, short of its muzzle)
        vector chest = g.origin + '0 0 1' * (g.maxs_z * 0.35);
        vector dir = chest - mid;
        if(vlen(dir) < 4)
        {
            dir = g.origin - p.origin;
            dir_z = 0;
        }
        dir = normalize(dir);
        vector from = mid - dir * 96;
        traceline(from, mid, TRUE, p);
        if(trace_startsolid || trace_fraction < 1)
        {
            // (its chest above a weapon held low: from under the floor; your eyes' way to it instead)
            dir = normalize(mid - (p.origin + p.view_ofs));
            from = mid - dir * 96;
        }
        if(vr_agws_s >= 4)
        {
            vector head = g.origin + '0 0 1' * (g.maxs_z - 5);
            from = p.origin + p.view_ofs;
            dir = normalize(head - from);
            from = head - dir * 96;
        }
        float bt = hitmodel_segment(g, from, from + dir * 300, 0, HITCLASS_GUNS);
        vr_agws_body = bt < 0 ? 999 : bt * 300 - 96; // (its body met this far beyond the weapon's line: negative before it)
        vr_agws_h0 = g.health;
        vr_agws_ab0 = vr_ags_absorbed;
        VR_AGWS_Fire(from, dir);
        vr_agws_until = time + ((vr_agws_s == 2 || vr_agws_s == 5) ? 0.3 : 0.1);
        vr_agt_step = 2;
        return;
    }
    // (step 2) Its result.
    float dmg = vr_agws_h0 - g.health;
    float stopped = vr_ags_absorbed - vr_agws_ab0;
    string a = sprintf("weaponshot %s, %s, block %g: damage %g, stopped %g", VR_AGT_Name(VR_AGWS_Wid(vr_agws_w)),
        VR_AGWS_SName(vr_agws_s), vr_agws_block, dmg, stopped);
    // Its body on the way: none (999), beyond the weapon's line (> 1 unit: the weapon first), or before it (< -1: its hand
    // or arm in front, hurt as ever); within a unit of it: either (not judged).
    float first = (vr_agws_body < -1 && vr_agws_body > -90) ? 2 : ((vr_agws_body > 1 || vr_agws_body <= -90) ? 1 : 0);
    string b = sprintf(" (its body %s: %.1f units past the weapon's line)", first == 1 ? "behind it" : first == 2 ?
        "in front (a hand or arm)" : "at it", vr_agws_body);
    float ok = TRUE;
    float judged = vr_agws_block == 1 || vr_agws_s >= 4;
    if(vr_agws_s >= 4)
    {
        ok = (dmg > 0 && stopped == 0); // (clear of the weapon: the body as ever)
    }
    else if(vr_agws_block == 1 && vr_agws_s == 3)
    {
        // (per pellet: the stopped ones hurt nothing, the rest as ever)
        ok = (stopped >= 1 && (stopped == 14 || dmg < (14 - stopped + 1) * cvar_hget(cvarh_vr_dmg_super_shotgun) * 3));
    }
    else if(vr_agws_block == 1 && first == 1)
    {
        ok = (dmg == 0 && stopped == 1);
    }
    else if(vr_agws_block == 1 && first == 2)
    {
        ok = (stopped == 0 && dmg > 0); // (its hand first: the body's, hurt)
    }
    else if(vr_agws_block == 1)
    {
        judged = FALSE;
    }
    if(judged)
    {
        VR_AGT_Print(strcat(strcat(a, b), ok ? ": PASS" : ": FAIL"));
        if(ok) { vr_agws_pass = vr_agws_pass + 1; } else { vr_agws_fail = vr_agws_fail + 1; }
    }
    else
    {
        VR_AGT_Print(strcat(strcat(a, b), " (before)"));
    }
    // The next: block 0 then 1 for each shot, then the head shot once; then the next weapon.
    if(vr_agws_s < 4 && vr_agws_block == 0)
    {
        vr_agws_block = 1;
    }
    else
    {
        vr_agws_block = 0;
        vr_agws_s = vr_agws_s + 1;
        if(vr_agws_s > 5)
        {
            vr_agws_s = 0;
            vr_agws_w = vr_agws_w + 1;
        }
    }
    if(vr_agws_s >= 4)
    {
        vr_agws_block = 1;
    }
    vr_agt_step = 0;
};

void(float t) VR_AGDT_Frame =
{
    if(t == 74) { VR_AGDT_Down(); }
    else if(t == 75) { VR_AGWS_Frame(); }
};
#endif
