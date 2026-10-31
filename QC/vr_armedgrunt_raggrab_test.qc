// vr_armedgrunt_raggrab_test.qc -- vr_agrunt_test 91: an active ragdoll's weapon grabbed (his note of 2026-10-30: "after
// turning an armed humanoid into an active ragdoll ... it seems impossible to grab their weapon out of their hand"; the
// grab query's arbitration: box3d grabRagdoll, physics::armedWeaponOver; QC VR_Ragdoll_Handtouch, VR_AG_RagWeaponGrab).
// A fresh shotgun grunt 80 ahead, you unseen; made an active ragdoll by trigger 0 a shove that topples it (lying still),
// 1 a shove's stagger (standing its ground, about 0.3 s); you step in and put your empty main hand (the real touches: the
// grip pressed, +grabmain) on point 0 its weapon's grip, 1 its barrel, 2 the hand holding it, 3 that hand's forearm (the
// part its gun is welded to and the bone below: ragdolllimbs 7), 4 over its barrel, the fist 5.5-7.5 cm off its drawn
// surface (lying only: where a grab took the arm, or nothing; vr_agrunt_rag_grab_weapon), then step back from it at 0.6
// m/s. Each prints `raggrab <trigger> <point>: <outcome>`: won (a tug, pulled out: yours) / yours at once / limb held
// (the hand holds its limb: the bug) / lost (the tug over, kept) / nothing / STUCK (5 s) / no state (never got there:
// not counted). Case 10: lying, the main hand on its grip, then the off hand on its forearm: the second hand joins the
// tug (two), both step back: yours. PASS: every counted case won, none "limb held". Body blows don't disarm meanwhile
// (vr_agrunt_disarm_damage 9999 by `tune`, put back before the verdict: the mock hand's jump onto it is a blow).
// Show Disarm Volumes (vr_debug_disarm 2 for point 4): `disarmviz: main hand takes weapon` for the hand 5.5-7.5 cm off.
// Ends on `agtest: test 91 over: P passed, F failed`. scratch1 n > 0: case n - 1 only.

#ifdef QVR_TESTS
float vr_agrb_case;     // 0-9: trigger (case / 5) x point (case % 5); 10 two hands
float vr_agrb_t0;
float vr_agrb_still;
float vr_agrb_track;
float vr_agrb_press;
float vr_agrb_press2;
float vr_agrb_tugged;
float vr_agrb_two;      // the second hand joined (case 10)
float vr_agrb_h;        // point 4: the hand's height over its barrel (cm)
float vr_agrb_gap;      // ... the fist's gap read at the press (cm)
float vr_agrb_rag;      // a ragdoll at the press
float vr_agrb_won, vr_agrb_n, vr_agrb_bad, vr_agrb_nost;
float vr_agrb_keep_dmg;
float vr_agrb_keep_scale, vr_agrb_keep_fight, vr_agrb_keep_impact, vr_agrb_keep_drop, vr_agrb_keep_dbg, vr_agrb_keep_dv;
vector vr_agrb_home;

string(float c) VR_AGRB_Trig = { return (c == 10 || floor(c / 5) == 0) ? "lying" : "staggering"; };
string(float c) VR_AGRB_Point =
{
    if(c == 10) { return "grip+forearm"; }
    float k = c - floor(c / 5) * 5;
    return k == 0 ? "grip" : k == 1 ? "barrel" : k == 2 ? "hand" : k == 3 ? "forearm" : "6cm-off";
};
float(float c) VR_AGRB_Pt = { return c == 10 ? 0 : c - floor(c / 5) * 5; };

// The ragdoll's part the hand goes to (points 2, 3): the hand holding its weapon (the part holding the piece it rides: the
// model's own gun, welded in that hand while down; ragdolllimbs 7, the engine's rule), or that hand's forearm (-1 none).
float(entity g, float c) VR_AGRB_Bone =
{
    entity p = g.vr_ag_prop;
    if(!p || !p.vr_ag_rb)
    {
        return -1;
    }
    float holder = ragdolllimbs(g, 7, p.vr_ag_rb - 1);
    float forearm = ragdolllimbs(g, 5, holder);
    float hand = holder;
    if(holder == ragdolllimbs(g, 6, 4) || holder == ragdolllimbs(g, 6, 5))
    {
        // (held by its forearm: the soldier's gun, welded to his left forearm; its hand the bone below)
        forearm = holder;
        float b;
        for(b = 0; b < 32; b++)
        {
            if(ragdolllimbs(g, 5, b) == forearm)
            {
                hand = b;
                break;
            }
        }
    }
    return (VR_AGRB_Pt(c) == 3 || c == 10) ? forearm : hand;
};

// The next case (FALSE: all done).
float() VR_AGRB_Next =
{
    float a = cvar("scratch1");
    for(;;)
    {
        vr_agrb_case = vr_agrb_case + 1;
        if(vr_agrb_case > 10)
        {
            return FALSE;
        }
        if((a <= 0 && vr_agrb_case != 9) || vr_agrb_case == a - 1)
        {
            return TRUE; // (not 9, staggering 6 cm off: up again before the search ends; scratch1 10 runs it)
        }
    }
    return FALSE;
};

void(entity p, entity g) VR_AGRB_End =
{
    if(g.vr_ag_tug_pl)
    {
        VR_AG_TugEnd(g, FALSE, "test");
    }
    localcmd("-grabmain\n-graboff\n");
    ragdollrelease(p, cVR_MainHand, '0 0 0');
    ragdollrelease(p, cVR_OffHand, '0 0 0');
    cvar_set("vr_mock_hand_pull", "0");
    cvar_set("vr_mock_hand_pull_off", "0");
    cvar_set("vr_mock_hand_to_absolute", "0");
    cvar_set("vr_debug_disarm", ftos(vr_agrb_keep_dv));
    VR_AGPT_EmptyHand(p);
    p.weapon2 = WID_FIST;
    vr_agt_step = VR_AGRB_Next() ? 0 : 9;
};

void() VR_AGRB_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float u = VRMetersToUnits();
    float c = vr_agrb_case;
    if(vr_agt_step == 0)
    {
        if(c == 0 && vr_agrb_n + vr_agrb_nost == 0)
        {
            vr_agrb_home = p.origin;
            vr_agrb_keep_scale = cvar("vr_agrunt_tug_downed_scale");
            vr_agrb_keep_fight = cvar("vr_agrunt_tug_fight");
            vr_agrb_keep_impact = cvar("vr_agrunt_fall_keep_impact");
            vr_agrb_keep_drop = cvar("vr_agrunt_fall_drop");
            vr_agrb_keep_dbg = cvar("vr_agrunt_debug");
            vr_agrb_keep_dv = cvar("vr_debug_disarm");
            vr_agrb_keep_dmg = cvar_hget(cvarh_vr_agrunt_disarm_damage);
            localcmd("tune vr_agrunt_disarm_damage 9999\n"); // (a tunable: the mock hand's jump onto it is no blow that disarms)
            cvar_set("vr_agrunt_tug_downed_scale", "0.5"); // (as shipped: a downed one's weapon needs a tug)
            cvar_set("vr_agrunt_tug_fight", "0");          // (up again mid-tug it fights back: not while it is measured)
            cvar_set("vr_agrunt_fall_keep_impact", "100"); // (its weapon kept however hard it lands)
            cvar_set("vr_agrunt_fall_drop", "0");
            if(!vr_agrb_keep_dbg)
            {
                cvar_set("vr_agrunt_debug", "0.5"); // (the engine's reading of the fist's gap: vr_agrunt_grab_gap)
            }
            float a = cvar("scratch1");
            if(a > 0)
            {
                vr_agrb_case = -1;
                if(!VR_AGRB_Next())
                {
                    vr_agt_step = 9;
                    return;
                }
                c = vr_agrb_case;
            }
        }
        p.flags = p.flags | FL_NOTARGET;
        VR_AGPT_EmptyHand(p);
        p.weapon2 = WID_FIST;
        localcmd("-grabmain\n-graboff\n");
        cvar_set("vr_mock_hand_pull", "0");
        cvar_set("vr_mock_hand_pull_off", "0");
        cvar_set("vr_mock_hand_lift", "0");
        cvar_set("vr_mock_hand_to_absolute", "0");
        cvar_set("vr_agrunt_disarm_yank", "0"); // (no grab before the press)
        p.mainhand_lastgrabtime = time - 2;
        p.mainhand_lastreleasetime = time - 1;
        p.offhand_lastgrabtime = time - 2;
        p.offhand_lastreleasetime = time - 1;
        VR_AGPT_Clear();
        setorigin(p, vr_agrb_home);
        p.velocity = '0 0 0';
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, 80);
        g.health = 400; // (a landing's impact damage never kills it)
        vr_agrb_t0 = time;
        vr_agrb_still = 0;
        vr_agrb_tugged = 0;
        vr_agrb_two = 0;
        vr_agrb_h = 3;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 9)
    {
        // (The tunable back first, a frame before the verdict ends the run: never left in tunables.cfg.)
        localcmd(sprintf("tune vr_agrunt_disarm_damage %g\n", vr_agrb_keep_dmg));
        vr_agt_step = 11;
        return;
    }
    if(vr_agt_step == 11)
    {
        VR_AGT_Check(sprintf("raggrab: %g/%g won (limb held or kept %g; %g no state):", vr_agrb_won, vr_agrb_n, vr_agrb_bad,
            vr_agrb_nost), vr_agrb_n > 0 && vr_agrb_won == vr_agrb_n && vr_agrb_bad == 0);
        VR_AGPT_Clear();
        VR_AGPT_EmptyHand(p);
        localcmd("-grabmain\n-graboff\n");
        cvar_set("vr_mock_hand_to_absolute", "0");
        cvar_set("vr_mock_hand_pull", "0");
        cvar_set("vr_mock_hand_pull_off", "0");
        cvar_set("vr_agrunt_debug", ftos(vr_agrb_keep_dbg));
        cvar_set("vr_agrunt_disarm_yank", "1");
        cvar_set("vr_agrunt_tug_downed_scale", ftos(vr_agrb_keep_scale));
        cvar_set("vr_agrunt_tug_fight", ftos(vr_agrb_keep_fight));
        cvar_set("vr_agrunt_fall_keep_impact", ftos(vr_agrb_keep_impact));
        cvar_set("vr_agrunt_fall_drop", ftos(vr_agrb_keep_drop));
        cvar_set("vr_debug_disarm", ftos(vr_agrb_keep_dv));
        setorigin(p, vr_agrb_home);
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        vr_agrb_case = 0;
        vr_agrb_won = 0;
        vr_agrb_n = 0;
        vr_agrb_bad = 0;
        vr_agrb_nost = 0;
        cvar_set("vr_agrunt_test", "0");
        return;
    }
    string name = sprintf("raggrab %s %s", VR_AGRB_Trig(c), VR_AGRB_Point(c));
    float lying = (VR_AGRB_Trig(c) == "lying");
    float pt = VR_AGRB_Pt(c);
    if(vr_agt_step == 1)
    {
        if(!g.takedamage || time < vr_agrb_t0 + 0.3)
        {
            return; // (started)
        }
        makevectors('0 1 0' * g.angles_y);
        vector back = v_forward * -1;
        if(lying)
        {
            VR_Knockdown_Start(g);
            ragdollshovetopple(g, back, 1, FALSE, 40);
        }
        else
        {
            VR_Knockdown_Stagger(g, back, 0.5, 0.95);
        }
        vr_agrb_t0 = time;
        vr_agt_step = 2;
        return;
    }
    float t = time - vr_agrb_t0;
    if(vr_agt_step == 2)
    {
        if(!g.vr_ag_prop || t > 6)
        {
            VR_AGT_Print(sprintf("%s: no state (%s; knockdown %g, ragdoll %g)", name, g.vr_ag_prop ? "never got there" :
                "it dropped its weapon", g.vr_knockdown, isragdoll(g)));
            vr_agrb_nost = vr_agrb_nost + 1;
            VR_AGRB_End(p, g);
            return;
        }
        float there = FALSE;
        if(lying)
        {
            vr_agrb_still = (g.vr_knockdown == 1 && !g.vr_kd_stagger && vlen(g.vr_ag_fall_v) < 10 && t > 0.5) ?
                vr_agrb_still + 1 : 0;
            there = (vr_agrb_still >= 15 && g.vr_ag_prop.vr_ag_rb);
        }
        else
        {
            there = (isragdoll(g) && g.vr_kd_stagger && g.vr_ag_prop.vr_ag_rb && t > 0.05);
        }
        if(!there)
        {
            return;
        }
        float rb = g.vr_ag_prop.vr_ag_rb - 1;
        VR_AGT_Print(sprintf("%s: its weapon rides part %g; its hand %g, forearm %g", name, rb, VR_AGRB_Bone(g, 2),
            VR_AGRB_Bone(g, 3)));
        // You step in beside its weapon (28 units off its middle, on your side).
        VR_AG_WeaponLine(g.vr_ag_prop);
        vector mid = (vr_ag_wa + vr_ag_wb) * 0.5;
        vector out = vr_agrb_home - g.origin;
        out_z = 0;
        vector at = mid + normalize(out) * 28;
        at_z = p.origin_z;
        setorigin(p, at);
        p.velocity = '0 0 0';
        cvar_set("vr_mock_hand_to_absolute", "1");
        if(pt == 4)
        {
            cvar_set("vr_debug_disarm", "2"); // (Show Disarm Volumes: its hover printed)
        }
        vr_agrb_track = 0;
        vr_agt_step = 3;
        return;
    }
    if(vr_agt_step == 3)
    {
        if(!g.vr_ag_prop || !isragdoll(g))
        {
            VR_AGT_Print(sprintf("%s: no state (%s before the press, %.2f s on)", name, g.vr_ag_prop ? "up again" :
                "its weapon gone", t));
            vr_agrb_nost = vr_agrb_nost + 1;
            VR_AGRB_End(p, g);
            return;
        }
        float bone = VR_AGRB_Bone(g, c);
        float near = FALSE;
        vr_agrb_track = vr_agrb_track + 1;
        p.vr_ag_knock_at = time + 0.3; // (the hand put there is no blow on its weapon or hand: a grab's approach)
        p.vr_ag_knock_off = time + 0.3;
        if(pt == 2 || pt == 3)
        {
            localcmd(sprintf("vr_mock_hand_to main ragdoll %g\n", bone));
            near = (bone >= 0 && ragdollbone(g, p.handpos) == bone);
        }
        else if(pt == 4)
        {
            // Raised over its barrel until the engine reads the fist 5.5-7.5 cm off its drawn surface.
            if(vr_agrb_track >= 12 && (vr_agrb_track - floor(vr_agrb_track / 3) * 3) == 0)
            {
                float gp = cvar("vr_agrunt_grab_gap");
                if(gp >= 5.5 && gp <= 7.5)
                {
                    near = TRUE;
                }
                else if(gp < 5.5)
                {
                    vr_agrb_h = vr_agrb_h + 0.5;
                }
                else
                {
                    vr_agrb_h = max(0, vr_agrb_h - 0.5);
                }
            }
            if(!near)
            {
                localcmd(sprintf("vr_mock_hand_to main agweaponpalm 0.85 %g\n", vr_agrb_h));
            }
        }
        else
        {
            localcmd(sprintf("vr_mock_hand_to main agweaponpalm %g 0 either\n", (pt == 0) ? 0.25 : 0.85));
            VR_AG_WeaponLine(g.vr_ag_prop);
            near = (VR_AG_SegDist(p.handpos, p.handpos, vr_ag_wa, vr_ag_wb) <= 5);
        }
        if(vr_agrb_track < (lying ? 12 : 4) || (!near && vr_agrb_track < 150))
        {
            return;
        }
        if(!near)
        {
            VR_AGT_Print(sprintf("%s: no state (the hand never got there: bone %g, nearest %g, gap %.1f cm)", name, bone,
                ragdollbone(g, p.handpos), cvar("vr_agrunt_grab_gap")));
            vr_agrb_nost = vr_agrb_nost + 1;
            VR_AGRB_End(p, g);
            return;
        }
        vr_agrb_gap = cvar("vr_agrunt_grab_gap");
        vr_agrb_rag = isragdoll(g);
        cvar_set("vr_agrunt_disarm_yank", "1");
        localcmd("+grabmain\n"); // (the grip pressed as the engine presses it: every touch that frame racing for it)
        vr_agrb_press = time;
        vr_agrb_press2 = 0;
        vr_ag_tug_why = "";
        vr_agt_step = 4;
        return;
    }
    // (step 4) Pulled back until it is yours, the tug lost, the limb held, or 5 s (stuck).
    float tp = time - vr_agrb_press;
    float tug = (g.vr_ag_tug_pl == p);
    float limb = !tug && ((ragdollheld(p, cVR_MainHand) == 1) || (ragdollheld(p, cVR_OffHand) == 1)); // (not the physical tug's hold)
    // (Case 10, both hands on: won, it goes to the hand placed last, the off hand: VR_AG_TugWinHand, d6ae5f30a.)
    float yours = (p.weapon == WID_SHOTGUN) || (c == 10 && p.weapon2 == WID_SHOTGUN);
    p.vr_ag_knock_at = time + 0.3; // (the grab and the pull are no blow on its weapon or hand)
    p.vr_ag_knock_off = time + 0.3;
    if(tug)
    {
        vr_agrb_tugged = 1;
        vr_agrb_two = max(vr_agrb_two, g.vr_ag_tug_two);
    }
    if(c == 10 && tug && !vr_agrb_press2 && tp >= 0.1)
    {
        // The off hand onto its forearm, its grip pressed: it joins.
        localcmd(sprintf("vr_mock_hand_to off ragdoll %g\n", VR_AGRB_Bone(g, c)));
        if(tp >= 0.3)
        {
            localcmd("+graboff\n");
            vr_agrb_press2 = time;
        }
        return;
    }
    if(tp > 0.05 && (c != 10 || (vr_agrb_press2 && time > vr_agrb_press2 + 0.1)))
    {
        // You step back at 0.6 m/s, away from it (the tug's own way: its hand to your grab), your hands drawn with you.
        vector away = g.vr_ag_tug_away;
        if(!tug)
        {
            away = p.handpos - g.origin;
            away_z = 0;
            away = normalize(away);
        }
        setorigin(p, p.origin + away * 0.6 * frametime * u);
        p.velocity = '0 0 0';
    }
    string outcome = "";
    if(yours)
    {
        outcome = vr_agrb_tugged ? "won" : "yours at once";
    }
    else if(limb && tp > 0.1)
    {
        outcome = "limb held";
    }
    else if(!tug && tp > 0.15 && vr_agrb_tugged)
    {
        outcome = "lost";
    }
    else if(!tug && tp > 0.3 && !vr_agrb_tugged)
    {
        outcome = "nothing";
    }
    else if(tp > 5)
    {
        outcome = "STUCK";
    }
    if(outcome == "")
    {
        return;
    }
    string s1 = sprintf("%s: %s after %.2f s (%s; ragdoll at the press %g, gap %.1f cm", name, outcome, tp, vr_ag_tug_why,
        vr_agrb_rag, vr_agrb_gap);
    string s2 = sprintf("; two hands %g; your hand holds %s, its weapon %s)", vr_agrb_two, p.mainhand_held ?
        p.mainhand_held.classname : "nothing", g.vr_ag_prop ? "held" : "gone");
    VR_AGT_Print(strcat(s1, s2));
    if(outcome != "won")
    {
        VR_AGT_Print(sprintf("    tug: stretch %.1f units, progress %.2f, eased %g, knockdown %g stagger %g; raggrabs %g", vr_ag_tug_stretch,
            g.vr_ag_tug_prog, g.vr_ag_tug_down, g.vr_knockdown, g.vr_kd_stagger, vr_ag_raggrabs));
    }
    vr_agrb_n = vr_agrb_n + 1;
    if(outcome == "won" && (c != 10 || vr_agrb_two))
    {
        vr_agrb_won = vr_agrb_won + 1;
    }
    else if(outcome == "limb held" || outcome == "lost" || outcome == "STUCK")
    {
        vr_agrb_bad = vr_agrb_bad + 1;
    }
    VR_AGRB_End(p, g);
};
#endif
