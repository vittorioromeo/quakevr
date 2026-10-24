// Armed humanoids up close (ROUND22.md, "Armed humanoids at close range"; his notes vrragtriggers_2026-10-24_23-01-03
// and 23-04-09): grabbing its weapon anywhere on it starts the tug and is never taken for a blow (test 65); walking up
// to one lying with its weapon in its hand and reaching for it never knocks it away (test 66). Debug > Tests > Armed
// Grunts: Grab Anywhere, Never a Blow / Reach for a Lying One's Weapon.

float vr_agct_case;
float vr_agct_d;        // units the hand has gone forward (its reach)
float vr_agct_w;        // the most its parry was raised in the case
float vr_agct_par;      // its last parry's time at the case's start
float vr_agct_dis;      // its disarms at the case's start
float vr_agct_keep1, vr_agct_keep2, vr_agct_keep3; // settings put back

float VR_AGCT_REACH = 7; // units the hand starts off the point it reaches for (its arm reaches about 8 more)

// Test 65's cases: where the open hand reaches (its share of the way from its grip to its muzzle; less than 0: that
// share of the way back to the end of its line, 8 units behind the grip: its stock), how fast (m/s); the last a closed
// fist punching its chest instead (VR_AGCT_FIST).
float VR_AGCT_FIST = 5;
float(float i) VR_AGCT_Share = { return i == 0 ? 0.8 : i == 1 ? -0.5 : i == 2 ? 0.4 : i == 3 ? 0 : i == 4 ? 0.8 : 0; };
float(float i) VR_AGCT_Speed = { return i == 4 ? 3.5 : i == VR_AGCT_FIST ? 4.5 : 0.8; };
string(float i) VR_AGCT_Name =
{
    return i == 0 ? "its barrel, slowly" : i == 1 ? "its stock, slowly" : i == 2 ? "its body, slowly" :
           i == 3 ? "its grip, slowly" : i == 4 ? "its barrel, an open hand at 3.5 m/s" :
           "an axe swung at its chest at 4.5 m/s";
};

// The point of its weapon the case reaches for (world).
vector(entity g, float i) VR_AGCT_Point =
{
    entity w = g.vr_ag_prop;
    VR_AG_WeaponLine(w);
    if(i == VR_AGCT_FIST)
    {
        VR_AGP_Body(g);
        return g.origin + VR_AGP_ToWorld(VR_AGPar_Chest(g));
    }
    float sh = VR_AGCT_Share(i);
    return sh < 0 ? w.origin + (vr_ag_wa - w.origin) * -sh : w.origin + (vr_ag_wb - w.origin) * sh;
};

void() VR_AGCT_Grab =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        if(vr_agct_case == 0)
        {
            vr_agct_keep1 = cvar("vr_agrunt_parry_chance");
            vr_agct_keep2 = cvar("vr_agrunt_parry_react");
            cvar_set("vr_agrunt_parry_chance", "1"); // (it sees every blow coming: a real one is always seen)
            cvar_set("vr_agrunt_parry_react", "0");
        }
        p.flags = p.flags | FL_NOTARGET;
        VR_AGPT_EmptyHand(p);
        localcmd("-grabmain\n");
        cvar_set("vr_mock_hand_pull", "0");
        cvar_set("vr_mock_hand_lift", "0");
        cvar_set("vr_agrunt_disarm_yank", "0"); // (no grab before its press)
        if(!cvar("vr_agrunt_debug"))
        {
            cvar_set("vr_agrunt_debug", "0.5"); // (the engine's reading of the fist's gap: vr_agrunt_grab_gap)
        }
        p.mainhand_lastgrabtime = time - 2;
        p.mainhand_lastreleasetime = time - 1;
        VR_AGPT_Fresh();
        vr_agt_step = 1;
        vr_agt_until = time + 0.4;
        return;
    }
    if(time < vr_agt_until) { return; }
    makevectors([0, p.angles_y, 0]);
    vector fw = v_forward;
    if(vr_agt_step == 1)
    {
        // Still, aware of you, turned side-on (its weapon across your reach: the hand meets the point it goes for).
        g.think = SUB_Null;
        g.nextthink = -1;
        g.velocity = '0 0 0';
        g.enemy = p;
        g.angles_y = p.angles_y + (vr_agct_case == VR_AGCT_FIST ? 180 : -90);
        vr_agt_step = 2;
        vr_agt_until = time + 0.1;
        return;
    }
    if(vr_agt_step == 2 || vr_agt_step == 3)
    {
        vector pt = VR_AGCT_Point(g, vr_agct_case);
        VR_AGT_HandToHeight(p, pt_z);
        vector off = p.handpos + fw * VR_AGCT_REACH - pt;
        off_z = 0;
        setorigin(g, g.origin + off);
        VR_AG_PlaceProp(g, g.vr_ag_prop);
        vr_agt_step = vr_agt_step + 1;
        vr_agt_until = time + 0.2;
        if(vr_agt_step == 4)
        {
            vr_agct_d = 0;
            vr_agct_w = 0;
            vr_agct_par = g.vr_agpar_last;
            vr_agct_dis = g.vr_ag_disarms;
            if(vr_agct_case == VR_AGCT_FIST)
            {
                // (a held melee weapon swung: a real strike; the mock's grip isn't held from QC. A weapon stays in a
                // hand not gripping with grip mode 1, as test 35's.)
                vr_agct_keep3 = cvar("vr_weapon_grip_mode");
                cvar_set("vr_weapon_grip_mode", "1");
                VRGiveEntWeapon(p, cVR_MainHand, WID_AXE, 0, 0);
                vr_agt_until = time + 0.3;
            }
        }
        return;
    }
    if(vr_agt_step == 4)
    {
        // The reach: the hand forward at the case's speed until its fist is on the weapon (the engine's reading) or
        // past where it was aimed (the punch: through its chest's place).
        vr_agct_w = max(vr_agct_w, g.vr_agpar_w);
        vector pn = VR_AGCT_Point(g, vr_agct_case);
        VR_AGT_HandToHeight(p, pn_z);
        float on = vr_agct_case != VR_AGCT_FIST && vr_agct_d > 2 && cvar("vr_agrunt_grab_gap") < -0.25; // (on it, a little in)
        if(!on && vr_agct_d < VR_AGCT_REACH + 6)
        {
            if(vr_agct_case == VR_AGCT_FIST && cvar("vr_agrunt_debug") >= 1)
            {
                VR_AGT_Print(sprintf("grab anywhere: the hand at %.1f m/s (tracked), a strike %g, its parry %.2f, bits %g",
                    VR_AG_SwingSpeed(p, cVR_MainHand), VR_AG_HandStrikes(p, cVR_MainHand), g.vr_agpar_w, p.vrbits0));
            }
            float step = VR_AGCT_Speed(vr_agct_case) * frametime;
            cvar_set("vr_mock_hand_pull", sprintf("%.4f", cvar("vr_mock_hand_pull") - step));
            vr_agct_d = vr_agct_d + step * VRMetersToUnits();
            return;
        }
        // (the arm's reach ends there: the rest of the way it is brought to the still hand, half a unit a frame, until
        // the fist is on it)
        if(!on && vr_agct_case != VR_AGCT_FIST && vr_agt_index < 60)
        {
            vr_agt_index = vr_agt_index + 1;
            setorigin(g, g.origin - fw * 0.5);
            VR_AG_PlaceProp(g, g.vr_ag_prop);
            return;
        }
        vr_agt_index = 0;
        vector pe = VR_AGCT_Point(g, vr_agct_case);
        VR_AGT_Print(sprintf("grab anywhere: the hand %.1f units from the point (%.1f ahead of it, %.1f over it)",
            vlen(pe - p.handpos), (pe - p.handpos) * fw, pe_z - p.handpos_z));
        vr_agt_step = 5;
        vr_agt_until = time + 0.25; // (still a moment: a grab is a hand at rest closing)
        return;
    }
    if(vr_agt_step == 5)
    {
        vr_agct_w = max(vr_agct_w, g.vr_agpar_w);
        if(vr_agct_case != VR_AGCT_FIST)
        {
            vr_agt_clip = cvar("vr_agrunt_grab_gap");
            cvar_set("vr_agrunt_disarm_yank", "1");
            localcmd("+grabmain\n");
            p.mainhand_lastgrabtime = time;
            p.mainhand_lastreleasetime = time - 1;
        }
        vr_agt_step = 6;
        vr_agt_until = time + 0.2;
        return;
    }
    vr_agct_w = max(vr_agct_w, g.vr_agpar_w);
    float parried = g.vr_agpar_last != vr_agct_par;
    float dis = g.vr_ag_disarms - vr_agct_dis;
    if(vr_agct_case == VR_AGCT_FIST)
    {
        VR_AGT_Check(sprintf("grab anywhere: %s: its parry raised %.2f, parried %g, disarmed %g:", VR_AGCT_Name(vr_agct_case),
            vr_agct_w, parried, dis), vr_agct_w > 0.2);
    }
    else
    {
        float taken = VR_AGPT_Taken(g, p);
        VR_AGT_Check(strcat(sprintf("grab anywhere: %s: the fist %.1f cm off it after %.1f units, taken %g (a tug %g)",
            VR_AGCT_Name(vr_agct_case), vr_agt_clip, vr_agct_d, taken, g.vr_ag_tug_pl == p),
            sprintf(", its parry raised %.2f, parried %g, disarmed %g:", vr_agct_w, parried, dis)),
            taken && vr_agct_w == 0 && !parried && dis == 0);
    }
    localcmd("-grabmain\n");
    cvar_set("vr_mock_hand_pull", "0");
    cvar_set("vr_mock_hand_lift", "0");
    VR_AGPT_EmptyHand(p);
    vr_agct_case = vr_agct_case + 1;
    vr_agt_step = 0;
    if(vr_agct_case > VR_AGCT_FIST)
    {
        vr_agct_case = 0;
        VR_AGPT_Fresh();
        VR_AGPT_Clear();
        if(cvar("vr_agrunt_debug") == 0.5)
        {
            cvar_set("vr_agrunt_debug", "0");
        }
        cvar_set("vr_agrunt_parry_chance", ftos(vr_agct_keep1));
        cvar_set("vr_agrunt_parry_react", ftos(vr_agct_keep2));
        cvar_set("vr_weapon_grip_mode", ftos(vr_agct_keep3));
        cvar_set("vr_agrunt_disarm_yank", "1");
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        cvar_set("vr_agrunt_test", "0");
    }
};

// Test 66: one lying with its weapon in its hand (a shove's gentle fall: vr_agrunt_fall_keep), you walking up to it
// and reaching for its weapon with an open hand at 0.8 m/s (your body against it, your hand on it): it keeps it. Case 0
// as shipped; case 1 the old rules (vr_agrunt_disarm_deliberate 0, vr_agrunt_fall_keep_window 0: printed, not checked).
void() VR_AGCT_Lying =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float u = VRMetersToUnits();
    if(vr_agt_step == 0)
    {
        if(vr_agct_case == 0)
        {
            vr_agct_keep1 = cvar("vr_agrunt_disarm_deliberate");
            vr_agct_keep2 = cvar("vr_agrunt_fall_keep_window");
            vr_agct_keep3 = cvar("vr_agrunt_disarm_yank");
        }
        cvar_set("vr_agrunt_disarm_deliberate", vr_agct_case == 0 ? ftos(vr_agct_keep1) : "0");
        cvar_set("vr_agrunt_fall_keep_window", vr_agct_case == 0 ? ftos(vr_agct_keep2) : "0");
        cvar_set("vr_agrunt_disarm_yank", "0"); // (a grab would take it: only blows and falls here)
        p.flags = p.flags | FL_NOTARGET;
        VR_AGPT_EmptyHand(p);
        localcmd("-grabmain\n");
        cvar_set("vr_mock_hand_pull", "0");
        cvar_set("vr_mock_hand_lift", "0");
        VR_AGPT_Clear();
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 110);
        vr_agt_step = 1;
        vr_agt_until = time + 0.5;
        return;
    }
    if(time < vr_agt_until && vr_agt_step != 3) { return; }
    if(vr_agt_step == 1)
    {
        VR_Knockdown_Start(g);
        makevectors('0 1 0' * g.angles_y);
        ragdollshovetopple(g, v_forward * -1, 1, FALSE, 40);
        vr_agct_dis = g.vr_ag_disarms;
        vr_agt_step = 2;
        vr_agt_until = time + 1.6; // (down and still)
        return;
    }
    if(vr_agt_step == 2)
    {
        VR_AGT_Print(sprintf("lying: down (knockdown %g), it holds %s", g.vr_knockdown, g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"));
        vr_agct_d = 0;
        vr_agt_step = 3;
        vr_agt_until = time + 3;
        return;
    }
    if(vr_agt_step == 3)
    {
        // You walk up to it (pushed at its weapon, your body against it), the hand lowered to its weapon and reaching.
        vector at = g.vr_ag_prop ? g.vr_ag_prop.origin : g.origin;
        vector to = at - p.origin;
        to_z = 0;
        if(vlen(to) > 20 && (p.flags & FL_ONGROUND))
        {
            to = normalize(to) * 150;
            p.velocity_x = to_x;
            p.velocity_y = to_y;
        }
        p.angles_y = vectoyaw(to);
        p.v_angle_y = p.angles_y;
        p.fixangle = TRUE;
        VR_AGT_HandToHeight(p, at_z);
        if(vr_agct_d < 24)
        {
            float step = 0.8 * frametime;
            cvar_set("vr_mock_hand_pull", sprintf("%.4f", cvar("vr_mock_hand_pull") - step));
            vr_agct_d = vr_agct_d + step * u;
        }
        if(g.vr_ag_prop && time < vr_agt_until)
        {
            return; // (on until the 3 s are up, or it lets go)
        }
    }
    float dis = g.vr_ag_disarms - vr_agct_dis;
    string s = sprintf("lying (%s): walked up and reached for its weapon: it holds %s, %g dropped (down %g, its fall moving %.1f m/s, the hand reached %.0f units, you %.0f units off):",
        vr_agct_case == 0 ? "as shipped" : "the old rules", g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing", dis,
        g.vr_knockdown, g.vr_ag_fall_fling / u, vr_agct_d, vlen(p.origin - g.origin));
    if(vr_agct_case == 0)
    {
        VR_AGT_Check(s, g.vr_ag_prop != world && dis == 0);
    }
    else
    {
        VR_AGT_Print(s);
    }
    cvar_set("vr_mock_hand_pull", "0");
    cvar_set("vr_mock_hand_lift", "0");
    vr_agct_case = vr_agct_case + 1;
    vr_agt_step = 0;
    if(vr_agct_case > 1)
    {
        vr_agct_case = 0;
        VR_AGPT_Clear();
        cvar_set("vr_agrunt_disarm_deliberate", ftos(vr_agct_keep1));
        cvar_set("vr_agrunt_fall_keep_window", ftos(vr_agct_keep2));
        cvar_set("vr_agrunt_disarm_yank", ftos(vr_agct_keep3));
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        cvar_set("vr_agrunt_test", "0");
    }
};

void(float t) VR_AGCT_Frame =
{
    if(t == 65) { VR_AGCT_Grab(); }
    else if(t == 66) { VR_AGCT_Lying(); }
};
