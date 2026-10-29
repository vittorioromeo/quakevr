#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_armedgrunt_ragweapon_test.qc (ROUND23.md, "Armed humanoids: the held weapon on an active ragdoll's hand"):
//
// Test 88: an armed humanoid's held weapon stays on its drawn hand as it becomes an active ragdoll (his notes
// 2026-10-29: "once they are transformed into active ragdolls, the position gets out of sync and the weapon just
// floats in the wrong position"; "the weapon and the hand overlap or get behind the active ragdoll's torso"). For the
// grunt and the enforcer (vr_agrunt_test_kind 0, 1), each holding a shotgun 80 ahead of you (unseen; its landings never
// cost it its weapon): case 0 shoved into a stagger it recovers from (balance 1), case 1 shoved over (a topple), lying
// there and getting up. From the shove until it stands again the engine measures every drawn frame
// (vr_agrunt_ragweapon_check): its weapon's grip against its hand's grip fitted to the hand's drawn vertices, and its
// hand's vertices and its weapon's line inside its torso (vr_agbody's proxies). Each case prints
// `ragweapon <kind> <case>: ...: PASS|FAIL` (vr_agrunt_ragweapon_err: the grip within 1.5 units and 12 degrees, hand and
// weapon within 1.5 units of the torso's surface), then `ragweapon: N passed, M failed` (run.sh -QuitOn on it).
// A case that never gets there (no ragdoll, its weapon dropped, still down 15 s on) FAILs as such.
// ----------------------------------------------------------------------------

float vr_agrw_case;      // 0 .. 3: kind (case / 2), stagger or topple (case % 2)
float vr_agrw_t0;
float vr_agrw_down;      // its knockdown seen
float vr_agrw_kind0;     // vr_agrunt_test_kind as it was
float vr_agrw_keep;      // vr_agrunt_fall_keep_impact as it was
float vr_agrw_check;     // vr_agrunt_ragweapon_check as it was
vector vr_agrw_home;
float VR_AGRW_CASES = 4;

string(float c) VR_AGRW_Name =
{
    return strcat(floor(c / 2) == 0 ? "grunt" : "enforcer", (c & 1) ? " topple" : " stagger");
};

void() VR_AGRW_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        if(vr_agrw_case == 0)
        {
            vr_agrw_home = p.origin;
            vr_agrw_kind0 = cvar("vr_agrunt_test_kind");
            vr_agrw_keep = cvar("vr_agrunt_fall_keep_impact");
            vr_agrw_check = cvar("vr_agrunt_ragweapon_check");
            cvar_set("vr_agrunt_fall_keep_impact", "100");
            if(!vr_agrw_check)
            {
                cvar_set("vr_agrunt_ragweapon_check", "1");
            }
        }
        if(vr_agrw_case >= VR_AGRW_CASES)
        {
            localcmd("vr_agrunt_ragweapon_err done\n");
            cvar_set("vr_agrunt_test_kind", ftos(vr_agrw_kind0));
            cvar_set("vr_agrunt_fall_keep_impact", ftos(vr_agrw_keep));
            cvar_set("vr_agrunt_ragweapon_check", ftos(vr_agrw_check));
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            vr_agrw_case = 0;
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        cvar_set("vr_agrunt_test_kind", ftos(floor(vr_agrw_case / 2)));
        p.flags = p.flags | FL_NOTARGET;
        setorigin(p, vr_agrw_home);
        p.velocity = '0 0 0';
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, 80);
        g.health = 400; // (a landing's impact damage never kills it)
        vr_agrw_t0 = time;
        vr_agrw_down = 0;
        vr_agt_step = 1;
        return;
    }
    string name = VR_AGRW_Name(vr_agrw_case);
    if(vr_agt_step == 1)
    {
        if(!g.takedamage || time < vr_agrw_t0 + 0.3)
        {
            return; // (its arms posed and sent first)
        }
        localcmd("vr_agrunt_ragweapon_err clear\n");
        makevectors('0 1 0' * g.angles_y);
        vector back = v_forward * -1;
        if(vr_agrw_case & 1)
        {
            VR_Knockdown_Start(g);
            ragdollshovetopple(g, back, 1, FALSE, 40);
        }
        else
        {
            VR_Knockdown_Stagger(g, back, 1, 1);
        }
        vr_agrw_t0 = time;
        vr_agt_step = 2;
        return;
    }
    float t = time - vr_agrw_t0;
    if(g.vr_knockdown)
    {
        vr_agrw_down = 1;
    }
    string why = "";
    if(!g.vr_ag_prop)
    {
        why = "it dropped its weapon";
    }
    else if(!vr_agrw_down && t > 1)
    {
        why = "never became a ragdoll";
    }
    else if(t > 15)
    {
        why = "timed out (still down 15 s on)";
    }
    if(why != "")
    {
        VR_AGT_Check(sprintf("ragweapon %s: %s:", name, why), FALSE);
    }
    else if(!vr_agrw_down || g.vr_knockdown)
    {
        return; // (staggering, down or getting up)
    }
    else
    {
        VR_AGT_Print(sprintf("ragweapon %s: up again after %.2f s", name, t));
    }
    localcmd(sprintf("vr_agrunt_ragweapon_err \"%s\"%s\n", name, why != "" ? " -1" : "")); // (-1: counted failed)
    VR_AGT_Remove(g);
    vr_agt_grunt = world;
    vr_agrw_case = vr_agrw_case + 1;
    vr_agt_step = 0;
};
#endif
