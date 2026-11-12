// vr_armedgrunt_second_test.qc -- vr_agrunt_test 211: Pick Up a Second (vr_agrunt_dual_pickup 1) goes for a second weapon
// only when it is cheap (VR_AG_SecondOk; ROUND24.md, "Pick Up a Second, smarter"). A shotgun grunt, a nail gun lying `dist`
// units to its side:
//   0 idle (you unseen), 120 away: goes for it and takes it;  1 idle, 300 away (past vr_agrunt_second_range): leaves it;
//   2 hunting you (in sight), 110 away (past vr_agrunt_second_combat_range): leaves it, keeps shooting;
//   3 hunting you, 52 away (very near): takes it, the trip's downtime in seconds printed (the combat time limits the
//     walk only, not the reach down, VR_AG_SECOND_TAKE: so his 1 s default lets a grunt that sees you fetch it);
//   4 idle, 140 away, the weapon taken away once it is on its way: gives up, no pick-up, nothing held in the left hand.
// `agtest: test 211 over: <p> passed, <f> failed`.
#ifdef QVR_TESTS
float vr_agsn_case;
entity vr_agsn_g;
entity vr_agsn_w;
vector vr_agsn_home;
float vr_agsn_t0;          // when it set out (first frame going for it)
float vr_agsn_t1;          // when the pick-up was done
float vr_agsn_gave;        // case 4: it had set out, the weapon was taken away
float vr_agsn_keep_pickup;

float(float c) VR_AGSN_Dist = { return c == 0 ? 120 : c == 1 ? 300 : c == 2 ? 110 : c == 3 ? 52 : 140; };
float(float c) VR_AGSN_Combat = { return c == 2 || c == 3; };

void() VR_AGSN_Frame =
{
    entity p = vr_agt_player;
    float c = vr_agsn_case;
    p.flags = p.flags | FL_GODMODE;
    if(vr_agt_step == 0)
    {
        if(c >= 5)
        {
            cvar_set("vr_agrunt_dual_pickup", ftos(vr_agsn_keep_pickup));
            p.flags = p.flags - (p.flags & (FL_GODMODE | FL_NOTARGET));
            VR_Test_VerdictPart("test 211 over:");
            vr_agsn_case = 0;
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        if(c == 0)
        {
            vr_agsn_keep_pickup = cvar_hget(cvarh_vr_agrunt_dual_pickup);
            cvar_set("vr_agrunt_dual_pickup", "1");
        }
        VR_AGT_ClearWeapons();
        if(VR_AGSN_Combat(c)) { p.flags = p.flags - (p.flags & FL_NOTARGET); }
        else { p.flags = p.flags | FL_NOTARGET; }
        vr_agsn_g = VR_AGT_Spawn(WID_SHOTGUN, 260);
        entity src = VR_AGT_Spawn(WID_NAILGUN, 220);
        vector at = src.origin;
        VR_AG_DropHeld(src, '0 0 0');
        VR_AGT_Remove(src);
        vr_agsn_w = VR_AGT_LyingNear(at);
        makevectors(vr_agsn_g.angles);
        setorigin(vr_agsn_w, vr_agsn_g.origin + v_right * VR_AGSN_Dist(c) + '0 0 6');
        vr_agsn_w.velocity = '0 0 0';
        vr_agsn_home = vr_agsn_g.origin;
        vr_agsn_t0 = 0;
        vr_agsn_t1 = 0;
        vr_agsn_gave = 0;
        if(VR_AGSN_Combat(c))
        {
            VR_AGT_Hunt(vr_agsn_g); // (at once: before it can look for a weapon as an idle one)
        }
        vr_agt_until = time + 8; // (a take's safety net; the others' window)
        vr_agt_step = 2;
        return;
    }
    entity g = vr_agsn_g;
    if(vr_agt_step == 2)
    {
        if(g.vr_ag_seek && !vr_agsn_t0)
        {
            vr_agsn_t0 = time;
            if(c == 4)
            {
                vr_agsn_gave = TRUE;
                vr_agsn_w.weaponinst = world;
                vr_agsn_w.classname = "taken"; // (a freed entity keeps its classname: what a pick-up by another does)
                remove(vr_agsn_w); // (taken away by someone else)
            }
        }
        if(c == 4)
        {
            if(vr_agsn_gave && !g.vr_ag_seek && time > vr_agsn_t0 + 0.2)
            {
                vr_agt_until = time + 3; // (nothing else to go for: watched a while)
                vr_agt_step = 3;
            }
            else if(time >= vr_agt_until)
            {
                VR_AGT_Check("second 4: never set out", FALSE);
                vr_agsn_case = c + 1;
                vr_agt_step = 0;
            }
            return;
        }
        if(c == 1 || c == 2)
        {
            if(c == 2)
            {
                setorigin(g, vr_agsn_home); // (a hunter walks about and would pass the weapon: held where it stands, fire only)
                g.velocity = '0 0 0';
            }
            if(time >= vr_agt_until - 3) // (5 s: ten looks of its 0.5 s)
            {
                VR_AGT_Check(sprintf("second %g: %s weapon %.0f away left: set out %s, pickups %g, left hand %s", c,
                    c == 1 ? "idle, far" : "in a fight, past the combat range", VR_AGSN_Dist(c), vr_agsn_t0 ? "yes" : "no",
                    g.vr_ag_pickups, g.vr_ag_prop2 ? "full" : "empty"), !vr_agsn_t0 && g.vr_ag_pickups == 0 && !g.vr_ag_prop2);
                VR_AGT_Remove(g);
                vr_agsn_case = c + 1;
                vr_agt_step = 0;
            }
            return;
        }
        // 0, 3: it takes it up
        if(g.vr_ag_prop2 && g.vr_ag_took_at <= 0)
        {
            vr_agsn_t1 = time;
            float trip = vr_agsn_t1 - vr_agsn_t0;
            VR_AGT_Check(sprintf("second %g: %s, %.0f away: took it up, the trip (downtime, not firing) %.2f s, left hand %s",
                c, c == 0 ? "idle" : "in a fight", VR_AGSN_Dist(c), trip, g.vr_ag_prop2.netname),
                g.vr_ag_pickups == 1 && (c == 0 || trip < cvar_hget(cvarh_vr_agrunt_second_combat_time) + VR_AG_SECOND_TAKE + 0.5));
            VR_AGT_Remove(g);
            vr_agsn_case = c + 1;
            vr_agt_step = 0;
        }
        else if(time >= vr_agt_until)
        {
            VR_AGT_Check(sprintf("second %g: %.0f away: not taken in time", c, VR_AGSN_Dist(c)), FALSE);
            VR_AGT_Remove(g);
            vr_agsn_case = c + 1;
            vr_agt_step = 0;
        }
        return;
    }
    if(vr_agt_step == 3)
    {
        if(time < vr_agt_until) { return; }
        VR_AGT_Check(sprintf("second 4: weapon taken away on the way: gave up (pickups %g, left hand %s, seek %s)", g.vr_ag_pickups,
            g.vr_ag_prop2 ? "full" : "empty", g.vr_ag_seek ? "still set" : "cleared"),
            g.vr_ag_pickups == 0 && !g.vr_ag_prop2 && !g.vr_ag_seek);
        VR_AGT_Remove(g);
        vr_agsn_case = c + 1;
        vr_agt_step = 0;
    }
};
#endif
