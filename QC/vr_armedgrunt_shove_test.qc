#ifdef QVR_TESTS
// Test 84 (`vr_agrunt_test 84`; ROUND22.md, "Armed humanoids' shove"): their posed shove (vr_agrunt_shove 2: the Shove
// pose, vr_armedgrunt_pose.qc VR_AGP_Shove). For each case (a grunt with a shotgun, a nail pistol, two nail pistols, an
// axe; an enforcer with its laser rifle), one spawned 160 units ahead, hunting you; you walk up to it (3 units a frame)
// until 36 units apart, well in its reach (a gun backs off to its fighting distance: you follow it), and it shoves. Its
// phases' times are printed as it plays (the wind-up's end, the push landing: its own time of it, vr_ag_shove_s, and
// your slide's start; the end), with an eyeshot as the push lands
// (`vr_eyeshot 1`, your view up close); then it is held at the push key 64 units ahead, its keys printed (the off hand's
// hold, the gun's turn, the hands' places), and an eyeshot of its front and one of its right side. Checks per case: it
// shoved, the push landed within a frame of its push phase's end, the push moved you, the shove's length, the keys the
// spec wants (`agtest: shove ...: PASS|FAIL`); the run ends with `agtest: test 84 over: <p> passed, <f> failed` (a case
// that never shoves in 10 s fails as "timed out").

float vr_agsh_case;
float vr_agsh_step;
float vr_agsh_at;      // the step's start
float vr_agsh_ph;      // the phases logged so far
float vr_agsh_hitat;   // the shove's time when your slide began (-1 not yet)
float vr_agsh_speed;   // your speed the frame after
float vr_agsh_keep;    // vr_agrunt_shove as it was
float vr_agsh_frames;  // frames held
float vr_agsh_due;     // its push phase's end (the contact), its wind-up's end, its length (VR_AGP_Times)
float vr_agsh_wind;
float vr_agsh_len;
vector vr_agsh_home;   // where you stood, the way you faced (each case from there)
float vr_agsh_yaw;
float VR_AGSH_N = 5;  // (scratch2 1: the other classes instead, cases 5..9: a super axe, a chainsaw, two crowbars, the
                       // fists, a rocket launcher; their checks the torso's lean alone)

float(float c) VR_AGSH_Wid =
{
    if(c == 0) { return WID_SHOTGUN; }
    if((c == 1) || (c == 2)) { return WID_NAILPISTOL; }
    if(c == 3) { return WID_AXE; }
    if(c == 5) { return WID_SUPERAXE; }
    if(c == 6) { return WID_CHAINSAW; }
    if(c == 7) { return WID_CROWBAR; }
    if(c == 8) { return -1; }
    if(c == 9) { return WID_ROCKET_LAUNCHER; }
    return WID_ENFRIFLE;
};
string(float c) VR_AGSH_Name =
{
    if(c == 0) { return "grunt, shotgun"; }
    if(c == 1) { return "grunt, nail pistol"; }
    if(c == 2) { return "grunt, two nail pistols"; }
    if(c == 3) { return "grunt, axe"; }
    if(c == 5) { return "grunt, super axe"; }
    if(c == 6) { return "grunt, chainsaw"; }
    if(c == 7) { return "grunt, two crowbars"; }
    if(c == 8) { return "grunt, fists"; }
    if(c == 9) { return "grunt, rocket launcher"; }
    return "enforcer, laser rifle";
};

// The keys at the push phase as the spec wants them (VR_AGK_Keys' sums: the off hand's hold, the turns, the places).
void(entity g, float c) VR_AGSH_CheckKeys =
{
    entity p = g.vr_ag_prop;
    VR_AGK_Keys(g, p);
    VR_AGT_Print(sprintf("shove %s: push keys: off hand %g, right place %v turn %v, left place %v turn %v, torso %v",
        VR_AGSH_Name(c), vr_agk_mode, vr_agk_rp, vr_agk_rr, vr_agk_lp, vr_agk_lr, vr_agk_t));
    // (the push key's own right place: the sums above hold the stance's key too)
    vector own = weapondrawnpose(world, g.vr_ag_kind * VR_AGK_KIND + VR_AGK_Class(g, p) * 1000 + 201, 11);
    float ok;
    if((c == 0) || (c == 4))
    {
        // (a long gun: both hands on it, turned across the body, thrust out)
        ok = ((vr_agk_mode == VR_AGK_OFF_GUN) && (vr_agk_rr_z > 60) && (own_x > 4));
    }
    else if(c == 1)
    {
        // (a pistol: the off hand off it, out ahead and up, the gun drawn back)
        ok = ((vr_agk_mode == VR_AGK_OFF_FREE) && (vr_agk_lp_x > 10) && (vr_agk_lp_z > 10) && (own_x < 0));
    }
    else if(c == 2)
    {
        // (two held: both hands turned palm out, pushed out)
        ok = ((vlen(vr_agk_rr) > 90) && (vlen(vr_agk_lr) > 90) && (vr_agk_rp_x > 4) && (vr_agk_lp_x > 4));
    }
    else if(c == 3)
    {
        // (a one-handed melee weapon: the free hand pushes, the weapon drawn back)
        ok = ((vr_agk_lp_x > 8) && (own_x < 0));
    }
    else
    {
        ok = TRUE; // (the other classes, scratch2 1: the eyeshots, and the torso's lean below)
    }
    VR_AGT_Check(sprintf("shove %s: push keys as specified:", VR_AGSH_Name(c)), ok && (vr_agk_t_y > 5));
    vr_agk_on = FALSE;
};

// Held at its push key (no blow dealt), `yaw` off facing you.
void(entity g, float yaw) VR_AGSH_Hold =
{
    entity pl = vr_agt_player;
    g.nextthink = time + 1;
    g.velocity = '0 0 0';
    g.enemy = pl;
    g.vr_ag_act = VR_AGP_SHOVE;
    VR_AGP_Times(g);
    g.vr_ag_act_at = time - vr_agp_tw - vr_agp_ts - 0.0005;
    g.vr_ag_act_hit = 2;
    g.vr_ag_act_tick = time;
    g.frame = VR_AM_StandFrame(g, 0);
    g.angles_y = vectoyaw(pl.origin - g.origin) + yaw + 0.01;
    VR_AGP_ShoveAim(g);
};

void() VR_AGSH_Frame =
{
    entity pl = vr_agt_player;
    entity g = vr_agt_grunt;
    float c = vr_agsh_case;
    if((vr_agsh_step == 0) && (c == 0) && cvar("scratch2"))
    {
        vr_agsh_case = VR_AGSH_N; // (scratch2 1: the other classes)
        c = vr_agsh_case;
        vr_agsh_home = pl.origin;
        vr_agsh_yaw = pl.angles_y;
        vr_agsh_keep = cvar("vr_agrunt_shove");
        cvar_set("vr_agrunt_shove", "2");
    }
    else if((vr_agsh_step == 0) && (c == 0))
    {
        vr_agsh_home = pl.origin;
        vr_agsh_yaw = pl.angles_y;
        vr_agsh_keep = cvar("vr_agrunt_shove");
        cvar_set("vr_agrunt_shove", "2");
    }
    makevectors([0, vr_agsh_yaw, 0]);
    vector fw = v_forward; // (the way you faced at the start)
    if(vr_agsh_step == 0)
    {
        if(g && g.classname != "" && g.health > 0) { VR_AGT_Remove(g); }
        VR_AGT_ClearWeapons();
        pl.angles_y = vr_agsh_yaw;
        pl.v_angle_y = vr_agsh_yaw;
        pl.fixangle = TRUE;
        setorigin(pl, vr_agsh_home);
        pl.velocity = '0 0 0';
        pl.vr_eshove_speed = 0;
        cvar_set("vr_agrunt_test_kind", c == 4 ? "1" : "0");
        vr_agt_weapon2 = (c == 2) ? WID_NAILPISTOL : ((c == 7) ? WID_CROWBAR : 0);
        vr_agt_grunt = VR_AGT_Spawn(VR_AGSH_Wid(c), 160);
        vr_agt_weapon2 = 0;
        VR_AGT_Hunt(vr_agt_grunt);
        vr_agsh_at = time;
        vr_agsh_hitat = -1;
        vr_agsh_ph = 0;
        vr_agsh_step = 1;
        return;
    }
    if(!g || (g.health <= 0))
    {
        VR_AGT_Check(sprintf("shove %s: its monster gone:", VR_AGSH_Name(c)), FALSE);
        vr_agsh_step = 9;
    }
    vector to = g.origin - pl.origin;
    to_z = 0;
    if((vr_agsh_step == 1) || (vr_agsh_step == 2))
    {
        pl.angles_y = vectoyaw(to); // (you face it as you follow it: the eyeshot as the push lands sees it)
        pl.v_angle_y = pl.angles_y;
        pl.fixangle = TRUE;
    }
    if(vr_agsh_step == 1)
    {
        // You walk up to it (3 units a frame: 216 units/s), to 36 units apart.
        if(vlen(to) > 36)
        {
            setorigin(pl, pl.origin + normalize(to) * min(3, vlen(to) - 36));
            return;
        }
        VR_AGT_Print(sprintf("shove %s: you stand %.0f units from it at %.2f s", VR_AGSH_Name(c), vlen(to), time - vr_agsh_at));
        vr_agsh_step = 2;
        vr_agsh_at = time;
        return;
    }
    if(vr_agsh_step == 2)
    {
        if((vlen(to) > 36) && (g.vr_ag_act != VR_AGP_SHOVE))
        {
            setorigin(pl, pl.origin + normalize(to) * min(3, vlen(to) - 36)); // (it backs off to its fighting distance: you follow)
        }
        if(g.vr_ag_act == VR_AGP_SHOVE)
        {
            VR_AGT_Print(sprintf("shove %s: it starts its shove %.2f s after you stopped (%.0f units apart)",
                VR_AGSH_Name(c), time - vr_agsh_at, vlen(to)));
            VR_AGP_Times(g);
            vr_agsh_due = vr_agp_tw + vr_agp_ts;
            vr_agsh_wind = vr_agp_tw;
            vr_agsh_len = vr_agp_tw + vr_agp_ts + vr_agp_th + vr_agp_tr;
            vr_agsh_step = 3;
        }
        else if(cvar("scratch1") && floor((time - vr_agsh_at) * 2) != floor((time - vr_agsh_at - frametime) * 2))
        {
            VR_AGT_Print(sprintf("shove debug: act %g frame %g near %g next %g enemy %s time %g", g.vr_ag_act, g.frame,
                g.vr_eshove_near, g.vr_eshove_next, g.enemy.classname, time));
        }
        else if(time - vr_agsh_at > 10)
        {
            VR_AGT_Check(sprintf("shove %s: timed out (no shove in 10 s; act %g):", VR_AGSH_Name(c), g.vr_ag_act), FALSE);
            vr_agsh_step = 9;
            return;
        }
        else
        {
            return;
        }
    }
    if(vr_agsh_step == 3)
    {
        float s = time - g.vr_ag_act_at;
        float due = vr_agsh_due;
        if(vr_agsh_ph < 1 && s >= vr_agsh_wind)
        {
            vr_agsh_ph = 1;
            VR_AGT_Print(sprintf("shove %s: wind-up over at %.3f s (its %.2f), the push begins", VR_AGSH_Name(c), s, vr_agsh_wind));
        }
        if(vr_agsh_hitat < 0 && pl.vr_eshove_speed > 0)
        {
            // (seen a frame after it landed: the shove's own time of it, vr_ag_shove_s)
            vr_agsh_hitat = g.vr_ag_shove_s;
            vr_agsh_ph = 2;
            cvar_set("vr_eyeshot", "1"); // (your view as the push lands)
            VR_AGT_Print(sprintf("shove %s: the push landed at %.3f s (its push phase ends at %.2f), your slide %.0f units/s",
                VR_AGSH_Name(c), vr_agsh_hitat, due, pl.vr_eshove_speed));
            return;
        }
        if(vr_agsh_ph == 2)
        {
            vr_agsh_ph = 3;
            vr_agsh_speed = vlen([pl.velocity_x, pl.velocity_y, 0]);
            VR_AGT_Print(sprintf("shove %s: your speed the frame after %.0f units/s", VR_AGSH_Name(c), vr_agsh_speed));
        }
        if(g.vr_ag_act == VR_AGP_SHOVE)
        {
            return;
        }
        VR_AGT_Print(sprintf("shove %s: over at %.3f s (its %.2f s; its frames' think every 0.1 s)", VR_AGSH_Name(c), s,
            vr_agsh_len));
        VR_AGT_Check(sprintf("shove %s: the push at its push phase's end (%.3f s, due %.2f):", VR_AGSH_Name(c), vr_agsh_hitat,
            due), (vr_agsh_hitat >= due - 0.002) && (vr_agsh_hitat <= due + 1 / 72));
        VR_AGT_Check(sprintf("shove %s: the push moved you (%.0f units/s):", VR_AGSH_Name(c), vr_agsh_speed), vr_agsh_speed > 100);
        VR_AGT_Check(sprintf("shove %s: its length %.3f s (0.4 .. 0.65):", VR_AGSH_Name(c), s), (s >= 0.4) && (s <= 0.68));
        // (you back where you began, it 64 units ahead: the hall's open floor, not where the push slid you)
        setorigin(pl, vr_agsh_home);
        pl.velocity = '0 0 0';
        pl.vr_eshove_speed = 0;
        setorigin(g, vr_agsh_home + fw * 64);
        pl.angles_y = vr_agsh_yaw;
        pl.v_angle_y = vr_agsh_yaw;
        pl.fixangle = TRUE;
        vr_agsh_frames = 0;
        vr_agsh_step = 4;
        return;
    }
    if((vr_agsh_step == 4) || (vr_agsh_step == 5))
    {
        // Held at its push key: its front (step 4), its right side (5); 0.4 s each for the client's eases, then the eyes.
        VR_AGSH_Hold(g, vr_agsh_step == 5 ? 90 : 0);
        vr_agsh_frames = vr_agsh_frames + 1;
        if(vr_agsh_frames == 2 && vr_agsh_step == 4)
        {
            VR_AGSH_CheckKeys(g, c);
        }
        if(vr_agsh_frames == 29)
        {
            cvar_set("vr_eyeshot", "1");
            dprint(sprintf("agtest: shove %s: eyeshot, held at its push key, %s\n", VR_AGSH_Name(c),
                vr_agsh_step == 5 ? "side" : "front"));
        }
        if(vr_agsh_frames >= 31)
        {
            vr_agsh_frames = 0;
            vr_agsh_step = vr_agsh_step + 1;
            if(vr_agsh_step == 6)
            {
                vr_agsh_step = 9;
            }
        }
        return;
    }
    if(vr_agsh_step == 9)
    {
        if(g && g.classname != "" && g.health > 0) { VR_AGT_Remove(g); }
        vr_agt_grunt = world;
        vr_agsh_case = vr_agsh_case + 1;
        vr_agsh_step = 0;
        if(vr_agsh_case >= VR_AGSH_N * (cvar("scratch2") ? 2 : 1))
        {
            vr_agsh_case = 0;
            cvar_set("vr_agrunt_shove", ftos(vr_agsh_keep));
            cvar_set("vr_agrunt_test_kind", "0");
            cvar_set("vr_agrunt_test", "0");
        }
    }
};
#endif
