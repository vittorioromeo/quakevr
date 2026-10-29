#ifdef QVR_TESTS
// Grabbing an active armed humanoid's weapon (ROUND22.md, "Armed humanoids: grabs, holding on, running chainsaws";
// his notes vrragtriggers_2026-10-27_01-38-30 .. 01-41-43): test 67, the grab table. Each weapon class (axe, sword,
// crowbar, chainsaw, shotgun, nailgun, a dual wielder's left shotgun, a dual wielder's left axe) on a humanoid idle (it
// hasn't seen you), running at you, ready (aiming a gun, a blade raised) and mid-strike (a swing under way, a shot just
// fired), with your empty main hand put on its weapon at 10 points along it (5% .. 95% from the end behind its grip to
// its muzzle or tip: vr_mock_hand_to agweaponpalm), following the weapon a few frames as a hand catching it would, then
// closing: a tug (or the weapon at once) is a success. In and out of bullet time (vr_timescale 0.3). Each group prints
// "grabtable: <weapon> <state> <normal|bullet time>: <n>/<tries> (tug, at once; refused too fast, the fist off it,
// knocked away, nothing; state not reached)", then the totals and the verdict: 90% of the tries taken or more (each time scale).
// scratch1: n > 0 only weapon case n - 1; scratch2: n > 0 only state n - 1; scratch3: 1 only normal time, 2 only bullet.
float vr_aggt_case;     // the weapon case
float vr_aggt_state;    // 0 idle, 1 running, 2 ready, 3 striking
float vr_aggt_pt;       // the point along it (0..9)
float vr_aggt_bt;       // 0 normal time, 1 bullet time
float vr_aggt_t0;       // the try's start, its press
float vr_aggt_press;
float vr_aggt_track;    // frames the hand has followed the weapon
vector vr_aggt_last;    // the humanoid's place last frame (its speed)
float vr_aggt_fast0;    // vr_ag_grab_fast at the press
float vr_aggt_gap;      // the engine's fist gap reading at the press (cm)
float vr_aggt_dis0;     // its disarms at the press
float vr_aggt_line;     // ... the hand's point from the weapon's line (units)
float vr_aggt_line1;    // ... from its line a frame before
float vr_aggt_hunted;   // the try's humanoid set hunting you (once started)
float vr_aggt_reach;    // ... from your eyes (units)
float vr_aggt_lt, vr_aggt_dt; // ... the frame before's time, the frame's
float vr_aggt_tug, vr_aggt_yank, vr_aggt_fast, vr_aggt_off, vr_aggt_knock, vr_aggt_none, vr_aggt_nost;
float vr_aggt_all_ok[2], vr_aggt_all_n[2];
float vr_aggt_keepscale;

float VR_AGGT_CASES = 8;
float VR_AGGT_POINTS = 10;
float VR_AGGT_TRACK = 12; // frames from your step in to the press (the hand following the weapon from the 4th)
float VR_AGGT_TRACK_MOST = 90; // ... at most, the hand not there yet (pressed anyway)
vector vr_aggt_home;      // where you stood as the test began (back there each try)
float VR_AGGT_ROAM = 64;  // units from there you step in at most
float vr_aggt_far;        // the try's weapon went farther: not counted (state not reached)

float(float c) VR_AGGT_Wid =
{
    return c == 0 ? WID_AXE : c == 1 ? WID_SWORD : c == 2 ? WID_CROWBAR : c == 3 ? WID_CHAINSAW : c == 4 ? WID_SHOTGUN :
           c == 5 ? WID_NAILGUN : c == 6 ? WID_SHOTGUN : WID_AXE;
};
float(float c) VR_AGGT_Dual = { return c >= 6; };
string(float c) VR_AGGT_Name =
{
    return c == 6 ? "dual-shotgun(left)" : c == 7 ? "dual-axe(left)" : VR_AGT_Name(VR_AGGT_Wid(c));
};
string(float s) VR_AGGT_StateName = { return s == 0 ? "idle" : s == 1 ? "running" : s == 2 ? "ready" : "striking"; };

// The weapon the hand goes for (a dual wielder's left one).
entity(entity g) VR_AGGT_Target =
{
    if(VR_AGGT_Dual(vr_aggt_case))
    {
        return g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop : (g.vr_ag_prop2.vr_ag_left ? g.vr_ag_prop2 : world);
    }
    return g.vr_ag_prop;
};

// Whether this case, state and time are run (scratch1..3).
float() VR_AGGT_Wanted =
{
    float c = cvar("scratch1"), s = cvar("scratch2"), b = cvar("scratch3");
    return (c <= 0 || vr_aggt_case == c - 1) && (s <= 0 || vr_aggt_state == s - 1) && (b <= 0 || vr_aggt_bt == b - 1);
};

void() VR_AGGT_GroupEnd =
{
    float n = vr_aggt_tug + vr_aggt_yank + vr_aggt_fast + vr_aggt_off + vr_aggt_knock + vr_aggt_none;
    if(n + vr_aggt_nost <= 0)
    {
        vr_aggt_nost = 0; // (a group not run)
        return;
    }
    // (sprintf: 8 arguments at most)
    string a = sprintf("grabtable: %s %s %s: %g/%g", VR_AGGT_Name(vr_aggt_case), VR_AGGT_StateName(vr_aggt_state),
        vr_aggt_bt ? "bullet" : "normal", vr_aggt_tug + vr_aggt_yank, n);
    string b = sprintf(" (tug %g, at once %g; too fast %g, fist off %g, knocked %g, nothing %g; state not reached %g)",
        vr_aggt_tug, vr_aggt_yank, vr_aggt_fast, vr_aggt_off, vr_aggt_knock, vr_aggt_none, vr_aggt_nost);
    VR_AGT_Print(strcat(a, b));
    vr_aggt_all_ok[vr_aggt_bt] = vr_aggt_all_ok[vr_aggt_bt] + vr_aggt_tug + vr_aggt_yank;
    vr_aggt_all_n[vr_aggt_bt] = vr_aggt_all_n[vr_aggt_bt] + n;
    vr_aggt_tug = 0;
    vr_aggt_yank = 0;
    vr_aggt_fast = 0;
    vr_aggt_off = 0;
    vr_aggt_knock = 0;
    vr_aggt_none = 0;
    vr_aggt_nost = 0;
};

// The next try (FALSE: all done).
float() VR_AGGT_Next =
{
    vr_aggt_pt = vr_aggt_pt + 1;
    if(vr_aggt_pt < VR_AGGT_POINTS)
    {
        return TRUE;
    }
    vr_aggt_pt = 0;
    VR_AGGT_GroupEnd();
    for(;;)
    {
        vr_aggt_state = vr_aggt_state + 1;
        if(vr_aggt_state > 3)
        {
            vr_aggt_state = 0;
            vr_aggt_case = vr_aggt_case + 1;
        }
        if(vr_aggt_case >= VR_AGGT_CASES)
        {
            vr_aggt_case = 0;
            vr_aggt_bt = vr_aggt_bt + 1;
            cvar_set("vr_timescale", vr_aggt_bt ? "0.3" : ftos(vr_aggt_keepscale));
        }
        if(vr_aggt_bt > 1)
        {
            return FALSE;
        }
        if(VR_AGGT_Wanted())
        {
            return TRUE;
        }
    }
    return FALSE;
};

// Its state reached (the try's hand goes to its weapon now).
float(entity g, entity p) VR_AGGT_InState =
{
    float sp = vlen((g.origin - vr_aggt_last) - '0 0 1' * (g.origin_z - vr_aggt_last_z)) / max(0.001, frametime);
    float melee = VR_AG_Class(g.vr_ag_wid) == VR_AG_CLASS_MELEE;
    float d = vlen(g.origin - p.origin);
    if(vr_aggt_state == 0)
    {
        return time > vr_aggt_t0 + 0.4;
    }
    if(vr_aggt_state == 1)
    {
        return sp > 40 && d < 90 && !VR_AG_Striking(g); // (near enough to meet: you step in at most VR_AGGT_ROAM)
    }
    if(vr_aggt_state == 2)
    {
        return time > vr_aggt_t0 + 0.5 && d < 120 && sp < 20 && !VR_AG_Striking(g) && (melee || g.vr_ag_aimw > 0.5);
    }
    return melee ? (g.vr_ag_act != 0 && time - g.vr_ag_act_at > 0.03) : (time - g.vr_ag_kick_at < 0.05 || g.vr_ag_act != 0);
};

// You step in within arm's reach of the try's point on its weapon: 22 units off it (flat), not in its body.
void(entity p, entity g, entity w) VR_AGGT_StepIn =
{
    VR_AG_WeaponLine(w);
    vector pt = vr_ag_wa + (vr_ag_wb - vr_ag_wa) * (0.05 + vr_aggt_pt * 0.1);
    vector to = pt - p.origin;
    to_z = 0;
    if(vlen(to) <= 26)
    {
        return;
    }
    vector at = p.origin + normalize(to) * (vlen(to) - 22);
    vector out = at - g.origin;
    out_z = 0;
    if(vlen(out) < 36)
    {
        at = g.origin + normalize(out) * 36;
        at_z = p.origin_z;
    }
    vector fromHome = at - vr_aggt_home;
    fromHome_z = 0;
    if(vlen(fromHome) > VR_AGGT_ROAM)
    {
        vr_aggt_far = 1; // (it went off: not followed; the try isn't counted. Walking about, the hands pressed the hall's
        return;          // wall buttons: its VR Calibration button opened the calibration and the run stood still an hour)
    }
    setorigin(p, at);
};

void() VR_AGGT_Run =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        if(vr_aggt_case == 0 && vr_aggt_state == 0 && vr_aggt_pt == 0 && vr_aggt_bt == 0)
        {
            vr_aggt_keepscale = 1; // (normal time between, whatever a run before left)
            vr_aggt_home = p.origin;
            vr_aggt_all_ok[0] = 0;
            vr_aggt_all_ok[1] = 0;
            vr_aggt_all_n[0] = 0;
            vr_aggt_all_n[1] = 0;
            if(!VR_AGGT_Wanted())
            {
                vr_aggt_pt = VR_AGGT_POINTS - 1;
                vr_aggt_nost = -1; // (not run: not printed)
                if(!VR_AGGT_Next())
                {
                    vr_agt_step = 9;
                    return;
                }
            }
            cvar_set("vr_timescale", vr_aggt_bt ? "0.3" : ftos(vr_aggt_keepscale));
            cvar_set("vr_mock_hand_to_absolute", "1"); // (the hand put each frame on the moving weapon follows it)
            if(!cvar("vr_agrunt_debug"))
            {
                cvar_set("vr_agrunt_debug", "0.5"); // (the engine's reading of the fist's gap: vr_agrunt_grab_gap)
            }
        }
        VR_AGPT_EmptyHand(p);
        localcmd("-grabmain\n");
        cvar_set("vr_mock_hand_pull", "0");
        cvar_set("vr_mock_hand_lift", "0");
        cvar_set("vr_agrunt_disarm_yank", "0"); // (no grab before its press)
        p.mainhand_lastgrabtime = time - 2;
        p.mainhand_lastreleasetime = time - 1;
        VR_AGPT_Clear();
        setorigin(p, vr_aggt_home); // (the steps in, a try's shoves: never wandered off over the map)
        p.velocity = '0 0 0';
        vr_agt_weapon2 = VR_AGGT_Dual(vr_aggt_case) ? VR_AGGT_Wid(vr_aggt_case) : 0;
        float dist = vr_aggt_state == 0 ? 60 : vr_aggt_state == 1 ? 300 : vr_aggt_state == 2 ? 90 : 60;
        p.flags = vr_aggt_state == 0 ? p.flags | FL_NOTARGET : p.flags - (p.flags & FL_NOTARGET);
        vr_agt_grunt = g = VR_AGT_Spawn(VR_AGGT_Wid(vr_aggt_case), dist);
        vr_agt_weapon2 = 0;
        vr_aggt_hunted = 0; // (hunting you once it has started: walkmonster_start_go, its takedamage)
        vr_aggt_far = 0;
        vr_aggt_t0 = time;
        vr_aggt_last = g.origin;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 9)
    {
        cvar_set("vr_timescale", ftos(vr_aggt_keepscale));
        cvar_set("vr_mock_hand_to_absolute", "0");
        float i;
        for(i = 0; i < 2; i++)
        {
            if(vr_aggt_all_n[i] > 0)
            {
                // (At least 90% taken: the table is a measure; every try taken never held, not even when it was
                // written: 198/214 normal, 173/178 bullet time; 2026-10-28 184/198, 160/170. ROUND23.md, "Armed-grunt
                // tests fixed".)
                VR_AGT_Check(sprintf("grabtable %s: %g/%g taken (90%% or more):", i ? "bullet time" : "normal time",
                    vr_aggt_all_ok[i], vr_aggt_all_n[i]), vr_aggt_all_ok[i] >= 0.9 * vr_aggt_all_n[i]);
            }
        }
        VR_AGPT_Clear();
        VR_AGPT_EmptyHand(p);
        localcmd("-grabmain\n");
        if(cvar("vr_agrunt_debug") == 0.5)
        {
            cvar_set("vr_agrunt_debug", "0");
        }
        cvar_set("vr_agrunt_disarm_yank", "1");
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        vr_aggt_case = 0;
        vr_aggt_state = 0;
        vr_aggt_pt = 0;
        vr_aggt_bt = 0;
        cvar_set("vr_agrunt_test", "0");
        return;
    }
    entity w = VR_AGGT_Target(g);
    if(vr_agt_step == 1)
    {
        if(!w || g.health <= 0)
        {
            vr_aggt_nost = vr_aggt_nost + 1;
            vr_agt_step = VR_AGGT_Next() ? 0 : 9;
            return;
        }
        if(!vr_aggt_hunted)
        {
            if(!g.takedamage)
            {
                return; // (not started yet)
            }
            vr_aggt_hunted = 1;
            if(vr_aggt_state > 0)
            {
                VR_AGT_Hunt(g);
            }
            vr_aggt_t0 = time;
        }
        float in = VR_AGGT_InState(g, p);
        vr_aggt_last = g.origin;
        if(!in)
        {
            if(time > vr_aggt_t0 + 6)
            {
                vr_aggt_nost = vr_aggt_nost + 1;
                vr_agt_step = VR_AGGT_Next() ? 0 : 9;
            }
            return;
        }
        VR_AGGT_StepIn(p, g, w);
        vr_aggt_track = 0;
        vr_agt_step = 2;
    }
    else if(vr_agt_step == 2)
    {
        if(!w)
        {
            // (knocked out of its hand as your hand went to it, before the press: a blow, not a grab)
            vr_aggt_knock = vr_aggt_knock + 1;
            VR_AGT_Print(sprintf("grabtry %s %s %s: its weapon knocked away before the press", VR_AGGT_Name(vr_aggt_case),
                VR_AGGT_StateName(vr_aggt_state), vr_aggt_bt ? "bullet" : "normal"));
            localcmd("-grabmain\n");
            vr_agt_step = VR_AGGT_Next() ? 0 : 9;
            return;
        }
        VR_AGGT_StepIn(p, g, w); // (it moved off: you follow)
    }
    // The hand on it at the try's point, following it (after 3 frames: the client has your step in).
    float f = 0.05 + vr_aggt_pt * 0.1;
    if(vr_agt_step == 2 && vr_aggt_track == VR_AGGT_TRACK - 1)
    {
        vr_aggt_lt = time;
    }
    if(vr_aggt_track >= 3)
    {
        localcmd(sprintf("vr_mock_hand_to main agweaponpalm %g 0 %s\n", f, VR_AGGT_Dual(vr_aggt_case) ? "left" : "either"));
    }
    vr_aggt_track = vr_aggt_track + 1;
    if(vr_agt_step == 2 && vr_aggt_far)
    {
        vr_aggt_nost = vr_aggt_nost + 1;
        vr_agt_step = VR_AGGT_Next() ? 0 : 9;
        return;
    }
    if(vr_agt_step == 2)
    {
        // (pressed once the hand is there: on its line within 5 units, a palm's breadth; in bullet time the drawn hand
        // takes longer to catch up with its controller, timescale::filterHands)
        VR_AG_WeaponLine(w);
        float near = VR_AG_SegDist(p.handpos, p.handpos, vr_ag_wa, vr_ag_wb) <= 5;
        if(vr_aggt_track < VR_AGGT_TRACK || (!near && vr_aggt_track < VR_AGGT_TRACK_MOST))
        {
            return;
        }
        if(!near)
        {
            // (the hand never got onto it: beyond your reach, a gun firing from across the room; not a grab's try)
            vr_aggt_nost = vr_aggt_nost + 1;
            vr_agt_step = VR_AGGT_Next() ? 0 : 9;
            return;
        }
        vr_aggt_gap = cvar("vr_agrunt_grab_gap");
        VR_AG_WeaponLine(w);
        vr_aggt_line = VR_AG_SegDist(p.handpos, p.handpos, vr_ag_wa, vr_ag_wb);
        if(cvar("vr_agrunt_debug") >= 1)
        {
            VR_AGT_Print(strcat(sprintf("grabpress: hand %v, its line %v .. %v, the weapon at %v;", p.handpos,
                vr_ag_wa, vr_ag_wb, w.origin), sprintf(" you at %v, it at %v (%v .. %v)", p.origin, g.origin, g.absmin, g.absmax)));
            tracebox(p.origin + '0 0 8', '-1 -1 -1', '1 1 1', w.origin, FALSE, p);
            VR_AGT_Print(sprintf("grabpress: a box from your chest to its weapon stops at %.2f on %s (takedamage %g, solid %g, it %g)",
                trace_fraction, trace_ent.classname, trace_ent.takedamage, trace_ent.solid, trace_ent == g));
        }
        vr_aggt_reach = vlen(p.handpos - (p.origin + p.view_ofs));
        // (the weapon's last frame's move, and the hand from its line as it was then)
        vr_aggt_dt = vlen(w.origin - w.vr_ag_was_o1);
        vector ko = w.origin, ka = w.angles;
        w.origin = w.vr_ag_was_o1;
        w.angles = w.vr_ag_was_a1;
        VR_AG_WeaponLine(w);
        vr_aggt_line1 = VR_AG_SegDist(p.handpos, p.handpos, vr_ag_wa, vr_ag_wb);
        w.origin = ko;
        w.angles = ka;
        vr_aggt_fast0 = vr_ag_grab_fast;
        vr_aggt_dis0 = g.vr_ag_disarms;
        cvar_set("vr_agrunt_disarm_yank", "1");
        localcmd("+grabmain\n");
        p.mainhand_lastgrabtime = time; // (the grip pressed now: VR_HandGrabUtil_StartedHandGrabbing)
        p.mainhand_lastreleasetime = time - 1;
        vr_aggt_press = time;
        vr_agt_step = 3;
        return;
    }
    // (step 3) Taken: a tug over it, or it in your hand.
    float tug = g.vr_ag_tug_pl == p;
    float yank = p.weapon == VR_AGGT_Wid(vr_aggt_case);
    float knocked = !tug && !yank && g.vr_ag_disarms > vr_aggt_dis0;
    if(!tug && !yank && time < vr_aggt_press + 0.15) // (yanked: in your hand a frame after it left its)
    {
        return;
    }
    if(tug) { vr_aggt_tug = vr_aggt_tug + 1; }
    else if(yank) { vr_aggt_yank = vr_aggt_yank + 1; }
    else if(knocked) { vr_aggt_knock = vr_aggt_knock + 1; }
    else if(vr_ag_grab_fast > vr_aggt_fast0) { vr_aggt_fast = vr_aggt_fast + 1; }
    else if(vr_aggt_gap > 0) { vr_aggt_off = vr_aggt_off + 1; }
    else { vr_aggt_none = vr_aggt_none + 1; }
    if(cvar("vr_agrunt_debug") >= 1 || (!tug && !yank))
    {
        string a = sprintf("grabtry %s %s %s %.2f: %s", VR_AGGT_Name(vr_aggt_case), VR_AGGT_StateName(vr_aggt_state),
            vr_aggt_bt ? "bullet" : "normal", f, tug ? "tug" : yank ? "at once" : knocked ? "knocked" :
            vr_ag_grab_fast > vr_aggt_fast0 ? "too fast" : vr_aggt_gap > 0 ? "fist off" : "nothing");
        string b = sprintf(" (fist %.1f cm off it at the press; the hand %.1f units off its line (%.1f off its last), %.1f from you; act %g, moved %.1f; you at %.0f u/s)",
            vr_aggt_gap, vr_aggt_line, vr_aggt_line1, vr_aggt_reach, g.vr_ag_act, vr_aggt_dt, vlen(p.velocity));
        VR_AGT_Print(strcat(a, b));
    }
    if(g.vr_ag_tug_pl)
    {
        VR_AG_TugEnd(g, FALSE, "test");
    }
    localcmd("-grabmain\n");
    vr_agt_step = VR_AGGT_Next() ? 0 : 9;
};

// Test 68: a dual wielder's left-hand weapon (his note vrragtriggers_2026-10-27_01-32-59: "I can only grab the one in
// the main hand"). A humanoid with a shotgun in its right hand and a nailgun in its left, idle: your hand on the left one
// (vr_mock_hand_to agweaponpalm 0.5 0 left) closes: a tug over the nailgun (the right still holds the shotgun); pulled
// (vr_ag_tug_test_force 1.3 x the break force) it is torn out, yours; it keeps the shotgun. Then your empty hand on the
// shotgun: a tug over it too.
float vr_aggt_leg;      // test 68's leg: 0 the left weapon, 1 the right

void() VR_AGGT_Dual68 =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        if(vr_aggt_leg == 0)
        {
            cvar_set("vr_mock_hand_to_absolute", "1");
            p.flags = p.flags | FL_NOTARGET;
            VR_AGPT_Clear();
            vr_agt_weapon2 = WID_NAILGUN;
            vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, 60);
            vr_agt_weapon2 = 0;
        }
        VR_AGPT_EmptyHand(p);
        localcmd("-grabmain\n");
        cvar_set("vr_agrunt_disarm_yank", "0");
        p.mainhand_lastgrabtime = time - 2;
        p.mainhand_lastreleasetime = time - 1;
        vr_ag_tug_test_force = 0;
        vr_aggt_t0 = time;
        vr_aggt_track = 0;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        // (started, and (the right leg) up again after the first tug)
        if(!g.takedamage || g.vr_knockdown || time < vr_aggt_t0 + 0.4)
        {
            if(time > vr_aggt_t0 + 6)
            {
                VR_AGT_Check("dual tug: it never stood ready:", FALSE);
                vr_agt_step = 9;
            }
            return;
        }
        localcmd(sprintf("vr_mock_hand_to main agweaponpalm 0.5 0 %s\n", vr_aggt_leg == 0 ? "left" : "either"));
        vr_aggt_track = vr_aggt_track + 1;
        if(vr_aggt_track < VR_AGGT_TRACK)
        {
            return;
        }
        cvar_set("vr_agrunt_disarm_yank", "1");
        localcmd("+grabmain\n");
        p.mainhand_lastgrabtime = time;
        p.mainhand_lastreleasetime = time - 1;
        vr_aggt_press = time;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2)
    {
        float tug = g.vr_ag_tug_pl == p;
        if(!tug && time < vr_aggt_press + 0.15)
        {
            return;
        }
        float want = vr_aggt_leg == 0 ? WID_NAILGUN : WID_SHOTGUN;
        float other = vr_aggt_leg == 0 ? (g.vr_ag_prop2 ? g.vr_ag_prop2.weapon : -1) : -1;
        VR_AGT_Check(sprintf("dual tug %s: a tug %g over its %s (the left %g), its other hand holding %s:", vr_aggt_leg == 0 ? "left" : "right",
            tug, g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing", g.vr_ag_prop.vr_ag_left, other >= 0 ? VR_AGT_Name(other) : "nothing"),
            tug && g.vr_ag_prop.weapon == want && (vr_aggt_leg == 1 || (other == WID_SHOTGUN && g.vr_ag_prop.vr_ag_left)));
        if(!tug)
        {
            vr_agt_step = 9;
            return;
        }
        // (pulled hard: torn out after vr_agrunt_tug_pull_time)
        vr_ag_tug_test_force = max(1, cvar_hget(cvarh_vr_agrunt_tug_break_force)) * 1.3;
        vr_agt_step = 3;
        return;
    }
    if(vr_agt_step == 3)
    {
        if(g.vr_ag_tug_pl == p && time < vr_aggt_press + 3)
        {
            return;
        }
        vr_ag_tug_test_force = 0;
        float yours = vr_aggt_leg == 0 ? WID_NAILGUN : WID_SHOTGUN;
        VR_AGT_Check(sprintf("dual tug %s: won (%s): yours %s, it holds %s%s:", vr_aggt_leg == 0 ? "left" : "right", vr_ag_tug_why,
            VR_AGT_Name(p.weapon), g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing", g.vr_ag_prop2 ? " and more" : ""),
            p.weapon == yours && (vr_aggt_leg == 1 ? !g.vr_ag_prop : (g.vr_ag_prop.weapon == WID_SHOTGUN && !g.vr_ag_prop2)));
        vr_aggt_leg = vr_aggt_leg + 1;
        vr_agt_step = vr_aggt_leg > 1 ? 9 : 0;
        return;
    }
    // (step 9) done
    vr_ag_tug_test_force = 0;
    vr_aggt_leg = 0;
    localcmd("-grabmain\n");
    VR_AGPT_EmptyHand(p);
    VR_AGPT_Clear();
    cvar_set("vr_mock_hand_to_absolute", "0");
    cvar_set("vr_agrunt_disarm_yank", "1");
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    cvar_set("vr_agrunt_test", "0");
};

// Test 69: holding on (his notes vrragtriggers_2026-10-27_01-43-08, 01-45-10). A fresh shotgun humanoid 80 ahead, you
// unseen, each case once: 0 shoved over (a topple), 1 lifted (vr_foegrab_lift_up) and let go, 2 lifted then slammed down
// (8 m/s), 3 judo-thrown (VR_FoeGrab_Throw), 4 a shoulder charge (strike 1), 5 a hard one (strike 2), 6 down and flung
// at 8 m/s (test 38's). Each until it lies still: whether it kept its weapon, its hardest impact against
// vr_agrunt_fall_keep_impact, and how fast it was flung and fell (the old rules let go above 5 and 4.5 m/s). PASS: kept
// while in the air and held, let go only at an impact over the threshold (never before it).
float vr_aggt_case69;
float vr_aggt_drop_at;  // when its weapon left its hand (0: kept)
float vr_aggt_still;    // frames it has lain still
float VR_AGGT_CASES69 = 7;

// Test 69's last three (his note vrragtriggers_2026-10-27_01-35-21): a chainsaw humanoid fighting you killed (case 7),
// disarmed by a blow (8): the chainsaw it drops lies running (QVR_WPNFLAG_SAW_RUNNING, its idle heard); one that never
// saw you killed (9): it lies off.
void() VR_AGGT_Saw69 =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float c = vr_aggt_case69 - VR_AGGT_CASES69;
    if(vr_agt_step == 0)
    {
        p.flags = c == 2 ? p.flags | FL_NOTARGET : p.flags - (p.flags & FL_NOTARGET);
        VR_AGPT_Clear();
        vr_agt_grunt = g = VR_AGT_Spawn(WID_CHAINSAW, 120);
        vr_aggt_t0 = time;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(!g.takedamage || time < vr_aggt_t0 + 0.3)
        {
            return;
        }
        if(c < 2)
        {
            VR_AGT_Hunt(g);
        }
        vr_aggt_t0 = time;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2)
    {
        if(time < vr_aggt_t0 + 0.2)
        {
            return; // (a moment hunting you)
        }
        if(c == 1)
        {
            VR_AG_Disarm(g, g.origin - p.origin, 120, "the test's blow");
        }
        else
        {
            T_Damage(g, p, p, g.health + 1); // (not gibbed: its gibs flew into the next one's weapon and disarmed it)
        }
        vr_agt_step = 3;
        return;
    }
    entity w = find(world, classname, "thrown_weapon");
    while(w && w.weapon != WID_CHAINSAW)
    {
        w = find(w, classname, "thrown_weapon");
    }
    if(!w && time < vr_aggt_t0 + 3)
    {
        return; // (dropped in its death frames)
    }
    float running = w && (w.weaponflags & QVR_WPNFLAG_SAW_RUNNING) != 0;
    VR_AGT_Check(sprintf("holdon chainsaw %s: dropped %g, running %g (fuel %g):", c == 0 ? "of one fighting you, killed" :
        c == 1 ? "of one fighting you, disarmed" : "of one that never saw you, killed", w != world, running,
        w ? w.weaponinst.wi_clip : -1), w != world && running == (c < 2));
    vr_aggt_case69 = vr_aggt_case69 + 1;
    vr_agt_step = vr_aggt_case69 >= VR_AGGT_CASES69 + 3 ? 9 : 0;
};
string(float c) VR_AGGT_Name69 =
{
    return c == 0 ? "shoved over" : c == 1 ? "lifted, let go" : c == 2 ? "lifted, slammed down" : c == 3 ? "judo-thrown" :
           c == 4 ? "shoulder-charged" : c == 5 ? "shoulder-charged hard" : "flung while down";
};

void() VR_AGGT_Hold69 =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float u = VRMetersToUnits();
    if(vr_aggt_case69 >= VR_AGGT_CASES69 && vr_aggt_case69 < VR_AGGT_CASES69 + 3 && vr_agt_step < 9)
    {
        VR_AGGT_Saw69();
        return;
    }
    if(vr_agt_step == 0)
    {
        if(cvar("scratch1") > 0)
        {
            vr_aggt_case69 = cvar("scratch1") - 1; // (that case alone)
        }
        p.flags = p.flags | FL_NOTARGET;
        VR_AGPT_EmptyHand(p);
        VR_AGPT_Clear();
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, 80);
        g.health = 400; // (a throw's or a landing's impact damage, vr_ragimpact.qc, never kills it: a death drops it anyway)
        vr_aggt_t0 = time;
        vr_aggt_drop_at = 0;
        vr_aggt_still = 0;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(!g.takedamage || time < vr_aggt_t0 + 0.3)
        {
            return; // (started)
        }
        makevectors('0 1 0' * g.angles_y);
        vector back = v_forward * -1;
        float c = vr_aggt_case69;
        float r = 0;
        entity oldself = self;
        if(c == 0 || c == 6)
        {
            VR_Knockdown_Start(g);
            r = ragdollshovetopple(g, back, 1, FALSE, 40);
        }
        else if(c == 1 || c == 2)
        {
            self = p;
            r = VR_FoeGrab_Lift(g, cvar("vr_foegrab_lift_up"), 2);
            self = oldself;
        }
        else if(c == 3)
        {
            self = p;
            r = VR_FoeGrab_Throw(g, g.angles_y + 90, -1);
            self = oldself;
        }
        else
        {
            r = VR_KdTrig_Bump(VR_KDT_TACKLE, g, p, cvar("vr_kdtrig_tackle_weight"), back, c == 4 ? 1 : 2, "the test's charge");
        }
        VR_AGT_Print(sprintf("holdon %s: begun (%g), down %g", VR_AGGT_Name69(c), r, g.vr_knockdown));
        vr_aggt_t0 = time;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2)
    {
        float t = time - vr_aggt_t0;
        if(!vr_aggt_drop_at && !g.vr_ag_prop)
        {
            vr_aggt_drop_at = time;
        }
        if((vr_aggt_case69 == 2 || vr_aggt_case69 == 6) && t >= 0.35 && t - frametime < 0.35)
        {
            makevectors('0 1 0' * g.angles_y);
            ragdollfling(g, vr_aggt_case69 == 2 ? '0 0 -8' * u : (v_forward * -1 + '0 0 0.3') * 8 * u);
        }
        vr_aggt_still = (g.vr_knockdown && vlen(g.vr_ag_fall_v) < 10 && t > 0.5) ? vr_aggt_still + 1 : 0;
        if(vr_aggt_still < 15 && t < 4 && g.vr_knockdown)
        {
            return;
        }
        float most = cvar_hget(cvarh_vr_agrunt_fall_keep_impact) * u;
        float dropped = vr_aggt_drop_at > 0;
        float hard = g.vr_ag_fall_hit >= most;
        float ok = dropped == hard && (!dropped || vr_aggt_drop_at >= g.vr_ag_fall_hit_at);
        string a = sprintf("holdon %s: %s; hardest impact %.1f m/s (at %.2f s; lets go at %g), flung %.1f, fell %.1f m/s",
            VR_AGGT_Name69(vr_aggt_case69), dropped ? sprintf("dropped at %.2f s", vr_aggt_drop_at - vr_aggt_t0) : "kept",
            g.vr_ag_fall_hit / u, g.vr_ag_fall_hit_at ? g.vr_ag_fall_hit_at - vr_aggt_t0 : -1, most / u, g.vr_ag_fall_fling / u,
            g.vr_ag_fall_down / u);
        VR_AGT_Check(strcat(a, sprintf(" (the old rules: %s):", (g.vr_ag_fall_fling > 5 * u || g.vr_ag_fall_down > 4.5 * u) ? "dropped" : "kept")), ok);
        vr_aggt_case69 = vr_aggt_case69 + 1;
        vr_agt_step = cvar("scratch1") > 0 ? 9 : 0; // (after the falls, the chainsaw's three: VR_AGGT_Saw69)
        return;
    }
    vr_aggt_case69 = 0;
    VR_AGPT_Clear();
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    cvar_set("vr_agrunt_test", "0");
};

void(float t) VR_AGGT_Frame =
{
    if(t == 67) { VR_AGGT_Run(); }
    else if(t == 68) { VR_AGGT_Dual68(); }
    else if(t == 69) { VR_AGGT_Hold69(); }
};
#endif
