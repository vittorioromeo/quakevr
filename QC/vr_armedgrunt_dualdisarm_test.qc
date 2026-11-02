// vr_armedgrunt_dualdisarm_test.qc -- vr_agrunt_test 203: a dual wielder disarmed of its off hand's weapon, then of the
// other (his note vrfiringrange_2026-11-02_23-10-50: "the remaining weapon just floats in the air, and then it tries to
// grab other weapons, and they keep like floating"). Each kind with a pair, you unseen, its AI on:
//   1. its left weapon knocked out (three ways: VR_AGDD_Method), the one left watched 0.8 s; 2. it goes for a fallen one;
//   both watched; 3. the right one knocked out, the other watched; 4. that one too: fists.
// Each frame not in a take: its slots agree with its weapons (.owner, wid, the left flag), each weapon's think goes on (a
// blow seen by the first's think took the second: that think stopped, nothing placed either) and each weapon is drawn on
// its own hand (weapondrawnpose 30: the weapon as drawn; 41: the hand as drawn; within VR_AGDD_GAP; the worst printed).
// The weapon it lost never knocks out the other (VR_AG_DropHeld): "holds nothing" fails it. scratch1 n: from case n - 1.
// `agtest: test 203 over: <p> passed, <f> failed`.
#ifdef QVR_TESTS
float VR_AGDD_CASES = 13;
float VR_AGDD_GAP = 14;     // units: a weapon as drawn off its hand's middle (a grip is on the palm: a few units)
float VR_AGDD_WATCH = 58;   // frames watched after each step (0.8 s)
float VR_AGDD_GIVEUP = 12;  // s: its take up (a safety net: a fail)
float vr_agdd_case;
float vr_agdd_frames;
float vr_agdd_until;
float vr_agdd_gap;          // the worst gap of the phase
float vr_agdd_undrawn;      // frames a held weapon wasn't drawn
float vr_agdd_bad;          // frames its slots didn't agree
string vr_agdd_badwhy;
entity vr_agdd_kept;        // the weapon that should be left

// Cases 0..6: knocked out by nobody (it stays calm); 7..9 by you (it turns on you, staggers); 10..12 your hand on its left
// weapon: a tug, won.
float(float c) VR_AGDD_Method = { return c < 7 ? 0 : c < 10 ? 1 : 2; };
float(float c) VR_AGDD_Kind =
{
    if(c >= 7)
    {
        c = (c - 7) % 3;
        c = c == 0 ? 0 : c == 1 ? 2 : 3;
    }
    return c == 2 ? VR_AM_ENFORCER : c == 3 ? VR_AM_KNIGHT : c == 4 ? VR_AM_DKNIGHT : c == 5 ? VR_AM_ENFORCER : VR_AM_GRUNT;
};

float(float c) VR_AGDD_Wid =
{
    if(c >= 7)
    {
        c = (c - 7) % 3;
        c = c == 0 ? 0 : c == 1 ? 2 : 3;
    }
    return c == 0 ? WID_NAILPISTOL : c == 1 ? WID_SHOTGUN : c == 2 ? WID_NAILGUN : WID_SWORD;
};

// One held weapon this frame: on its hand as drawn?
void(entity g, entity p) VR_AGDD_One =
{
    if(p.owner != g || p.classname != "vr_ag_weapon")
    {
        vr_agdd_bad = vr_agdd_bad + 1;
        vr_agdd_badwhy = "owner";
    }
    vector at = weapondrawnpose(p, 0, 30);
    vector hand = weapondrawnpose(g, p.vr_ag_left ? 1 : 0, 41);
    if(p.nextthink < time - 0.1)
    {
        vr_agdd_bad = vr_agdd_bad + 1;
        vr_agdd_badwhy = "its think stopped: nothing places it, nothing sends its pose";
    }
    if(at == '0 0 0')
    {
        vr_agdd_undrawn = vr_agdd_undrawn + 1;
        return;
    }
    if(hand != '0 0 0') // (none on a ragdoll, and just after one: the hand query's the posed body's)
    {
        vr_agdd_gap = max(vr_agdd_gap, vlen(at - hand));
    }
};

// This frame: its slots and weapons.
void(entity g) VR_AGDD_Watch =
{
    entity a = g.vr_ag_prop;
    entity b = g.vr_ag_prop2;
    if((a && g.vr_ag_wid != a.weapon) || (b && g.vr_ag_wid2 != b.weapon) || (!a && b))
    {
        vr_agdd_bad = vr_agdd_bad + 1;
        vr_agdd_badwhy = "wid";
    }
    if(a && b && a.vr_ag_left == b.vr_ag_left)
    {
        vr_agdd_bad = vr_agdd_bad + 1;
        vr_agdd_badwhy = "both one hand";
    }
    if(a && !b && a.vr_ag_left)
    {
        vr_agdd_bad = vr_agdd_bad + 1;
        vr_agdd_badwhy = "one, in its left";
    }
    if(g.vr_ag_took_at > 0)
    {
        return; // (on its way up from the floor: not on its hand's hold yet)
    }
    if(a) { VR_AGDD_One(g, a); }
    if(b) { VR_AGDD_One(g, b); }
};

// The phase's verdict.
void(string what, float ok) VR_AGDD_Phase =
{
    string head = sprintf("dualdisarm %g (kind %g, %s) %s", vr_agdd_case, VR_AGDD_Kind(vr_agdd_case), VR_AGT_Name(VR_AGDD_Wid(vr_agdd_case)), what);
    VR_AGT_Check(sprintf("%s: worst gap %.1f, undrawn %g, bad slots %g%s", head, vr_agdd_gap, vr_agdd_undrawn, vr_agdd_bad,
        vr_agdd_bad ? sprintf(" (%s)", vr_agdd_badwhy) : ""), ok && vr_agdd_gap <= VR_AGDD_GAP && vr_agdd_bad == 0 && vr_agdd_undrawn < 15);
    vr_agdd_gap = 0;
    vr_agdd_undrawn = 0;
    vr_agdd_bad = 0;
    vr_agdd_frames = 0;
};

// Its weapon in hand `left` knocked out: through its first slot, as a blow on it has it (VR_AG_DisarmBy).
void(entity g, float left) VR_AGDD_Knock =
{
    if(g.vr_ag_prop2 && (g.vr_ag_prop.vr_ag_left != left))
    {
        VR_AG_Swap(g);
    }
    makevectors('0 1 0' * g.angles_y);
    VR_AG_DisarmBy(g, VR_AGDD_Method(vr_agdd_case) ? vr_agt_player : world, v_right * (left ? -1 : 1), 150, "the test's");
};

void() VR_AGDD_Frame =
{
    entity pl = vr_agt_player;
    entity g = vr_agt_grunt;
    float c = vr_agdd_case;
    pl.flags = pl.flags | FL_GODMODE | FL_NOTARGET;
    if(vr_agt_step == 0)
    {
        if(c >= VR_AGDD_CASES)
        {
            pl.flags = pl.flags - (pl.flags & (FL_GODMODE | FL_NOTARGET));
            cvar_set("vr_agrunt_test_kind", "0");
            VR_Test_VerdictPart("test 203 over:");
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        if(c == 0 && cvar_hget(cvarh_scratch1) > 0)
        {
            vr_agdd_case = c = cvar_hget(cvarh_scratch1) - 1; // (scratch1 n: from case n - 1)
        }
        VR_AGT_ClearWeapons();
        cvar_set("vr_agrunt_test_kind", ftos(VR_AGDD_Kind(c)));
        vr_agt_grunt = VR_AGT_SpawnPair(VR_AGDD_Wid(c), VR_AGDD_Wid(c), VR_AGDD_Method(c) ? 60 : 200);
        vr_agaw_home = vr_agt_grunt.origin;
        vr_agdd_frames = 0;
        vr_agdd_gap = 0;
        vr_agdd_undrawn = 0;
        vr_agdd_bad = 0;
        vr_agt_step = 1;
        return;
    }
    vr_agdd_frames = vr_agdd_frames + 1;
    if(vr_agt_step == 1)
    {
        if(vr_agdd_frames < 20) { return; } // (its arms drawn a few frames)
        VR_AGDD_Watch(g);
        if(vr_agdd_frames < 30) { return; }
        VR_AGDD_Phase("both, before", g.vr_ag_prop && g.vr_ag_prop2);
        vr_agdd_kept = g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop2 : g.vr_ag_prop;
        if(VR_AGDD_Method(c) == 2)
        {
            // Your empty main hand on its left weapon (test 68's way), closing: a tug; pulled hard: torn out, yours.
            cvar_set("vr_mock_hand_to_absolute", "1");
            VR_AGPT_EmptyHand(pl);
            localcmd("-grabmain\n");
            cvar_set("vr_agrunt_disarm_yank", "0");
            pl.mainhand_lastgrabtime = time - 2;
            pl.mainhand_lastreleasetime = time - 1;
            vr_agt_step = 10;
            return;
        }
        if(VR_AGDD_Method(c) == 1)
        {
            vr_agdd_until = time + VR_AGDD_GIVEUP;
            vr_agt_step = 13;
            return;
        }
        VR_AGDD_Knock(g, 1);
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 13)
    {
        // Your fist through its left weapon (test 200's blow), seen in the frame by the first weapon's think, as the frame
        // runs it: nothing else asks it to think again (the engine clears .nextthink before a think).
        entity lw = g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop : world;
        if(g.vr_ag_prop2 && g.vr_ag_prop2.vr_ag_left)
        {
            lw = g.vr_ag_prop2;
        }
        if(lw && time < vr_agdd_until)
        {
            vr_agaw_aim = lw;
            g.vr_ag_prop.nextthink = 0;
            VR_AGAW_DualBlow(g);
            return;
        }
        pl.vrbits0 = pl.vrbits0 - (pl.vrbits0 & QVR_VRBITS0_MAINHAND_GRABBING); // (the blow's fist: your grip's own now)
        vr_agdd_frames = 0;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 10)
    {
        localcmd("vr_mock_hand_to main agweaponpalm 0.5 0 left\n");
        if(vr_agdd_frames < 12) { return; }
        cvar_set("vr_agrunt_disarm_yank", "1");
        localcmd("+grabmain\n");
        pl.mainhand_lastgrabtime = time;
        pl.mainhand_lastreleasetime = time - 1;
        vr_agdd_until = time + 3;
        vr_agt_step = 11;
        vr_agdd_frames = 0;
        return;
    }
    if(vr_agt_step == 11)
    {
        if(g.vr_ag_tug_pl != pl && vr_agdd_frames < 11) { return; }
        VR_AGT_Check(sprintf("dualdisarm %g: a tug %g over its %s (left %g)", c, g.vr_ag_tug_pl == pl, g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing",
            g.vr_ag_prop.vr_ag_left), g.vr_ag_tug_pl == pl && g.vr_ag_prop.vr_ag_left);
        vr_ag_tug_test_force = max(1, cvar_hget(cvarh_vr_agrunt_tug_break_force)) * 1.3;
        vr_agt_step = 12;
        return;
    }
    if(vr_agt_step == 12)
    {
        if(g.vr_ag_tug_pl == pl && time < vr_agdd_until) { return; }
        vr_ag_tug_test_force = 0;
        VR_AGT_Print(sprintf("dualdisarm %g: tug over (%s): yours %s, it holds %s", c, vr_ag_tug_why, VR_AGT_Name(pl.weapon),
            g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"));
        vr_agdd_frames = 0;
        vr_agt_step = 2;
        return;
    }
    VR_AGDD_Watch(g);
    if(vr_agt_step == 2)
    {
        if(vr_agdd_frames < VR_AGDD_WATCH) { return; }
        VR_AGDD_Phase(sprintf("its left knocked out: holds %s%s (kept: %g)", g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing", g.vr_ag_prop2 ? " and more" : "",
            g.vr_ag_prop == vr_agdd_kept),
            g.vr_ag_prop == vr_agdd_kept && !g.vr_ag_prop2 && !vr_agdd_kept.vr_ag_left);
        vr_agdd_until = time + VR_AGDD_GIVEUP;
        vr_agt_step = 3;
        if(VR_AGDD_Method(c))
        {
            // (it was hunting you: calm again, for it to go for a weapon; a tug's went into your hand: another lying by it)
            localcmd("-grabmain\n");
            VR_AGPT_EmptyHand(pl);
            cvar_set("vr_mock_hand_to_absolute", "0");
            VR_AGAW_Calm(g);
            if(VR_AGDD_Method(c) == 2)
            {
                entity src = VR_AGT_Spawn(VR_AGDD_Wid(c), 25);
                VR_AG_DropHeld(src, '0 0 0');
                VR_AGT_Remove(src);
            }
        }
        return;
    }
    if(vr_agt_step == 3)
    {
        if(!(g.vr_ag_prop2 && g.vr_ag_took_at <= 0) && time < vr_agdd_until) { return; }
        entity lh = g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop : g.vr_ag_prop2;
        VR_AGDD_Phase(sprintf("going for the fallen one: left hand %s", lh ? lh.netname : "empty"), g.vr_ag_prop && g.vr_ag_prop2 && lh);
        vr_agt_step = 4;
        return;
    }
    if(vr_agt_step == 4)
    {
        if(vr_agdd_frames < VR_AGDD_WATCH) { return; }
        VR_AGDD_Phase("both again", g.vr_ag_prop && g.vr_ag_prop2);
        if(!g.vr_ag_prop2)
        {
            VR_AGT_RemovePair(g);
            vr_agdd_case = c + 1;
            vr_agt_step = 0;
            return;
        }
        vr_agdd_kept = g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop : g.vr_ag_prop2;
        VR_AGDD_Knock(g, 0);
        vr_agt_step = 5;
        return;
    }
    if(vr_agt_step == 5)
    {
        if(vr_agdd_frames < VR_AGDD_WATCH) { return; }
        VR_AGDD_Phase(sprintf("its right knocked out: holds %s", g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"),
            g.vr_ag_prop == vr_agdd_kept && !g.vr_ag_prop2 && !vr_agdd_kept.vr_ag_left);
        VR_AGDD_Knock(g, 0);
        vr_agt_step = 6;
        return;
    }
    if(vr_agdd_frames < 10) { return; }
    VR_AGDD_Phase("the last knocked out", !g.vr_ag_prop && !g.vr_ag_prop2 && g.vr_ag_wid == -1 && g.vr_ag_wid2 == -1);
    VR_AGT_RemovePair(g);
    vr_agdd_case = c + 1;
    vr_agt_step = 0;
};
#endif
