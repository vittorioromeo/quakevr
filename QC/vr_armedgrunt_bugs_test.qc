// vr_armedgrunt_bugs_test.qc -- armed humanoids' tests of round 22's fixes (ROUND22.md, "Armed humanoids: the picked-up
// sword, drops up close, friendly fire, test 2"). `vr_agrunt_test N` in vrtesthall (Debug > Tests > Armed Grunts):
// - 61: each melee weapon picked up from the floor by a grunt and an enforcer, lying (another's drop), after its own
//   disarm, thrown by you, and after a knockdown: `agtest: pickup <kind> <weapon> <source>: model <m> (index <i>,
//   spawned <j>), effects <e>, alpha <a>, <d> from it: PASS|FAIL` (PASS: the held prop has the spawn path's model and
//   index, the held flag, no alpha, in its hand). `scratch1` N: case N alone, left standing (an eyeshot).
// - 64: a crowd of gunners hunting you (the scenario agrunt_friendly_fire).
// - 62: up close: an armed grunt hunting you while you walk into it (your body pushed at it every frame) for 10 s, each
//   weapon: `agtest: walkin <weapon>: <n> disarms, <dmg> damage to you` and `walkin: <n> drops: PASS` (0 drops).
// - 63: friendly fire: four gunners in a row far ahead, four of their friends (held still) between them and you, 15 s
//   with vr_agrunt_friendly_care 0, then again at its value: `agtest: friendly care <c>: <h> friendly hits (<d>
//   damage), <s> shots, <f> held, <p> damage to you`; PASS when the careful run's hits are fewer and it still hurt you.

float vr_agbt_case;     // the case being run
float vr_agbt_count;    // ... of how many
float vr_agbt_keep;     // test 61's scratch1: one case, left standing
entity vr_agbt_src;     // test 61: the weapon it should pick up (until picked up)
float vr_agbt_val;      // a counter at the case's start
float vr_agbt_val2;
float vr_agbt_care;     // test 63: the care's value to put back
entity vr_agbt_row[8];  // test 63: four gunners, four friends

float(float i) VR_AGBT_MeleeWid =
{
    if(i == 0) { return WID_AXE; }
    if(i == 1) { return WID_SWORD; }
    if(i == 2) { return WID_CROWBAR; }
    if(i == 3) { return WID_CHAINSAW; }
    return WID_MJOLNIR;
};

string(float s) VR_AGBT_Source =
{
    return s == 0 ? "lying" : s == 1 ? "after its disarm" : s == 2 ? "thrown by you" : "after a knockdown";
};

// Test 61's case `c`: its kind (vr_agbt_kind), weapon (vr_agbt_wid), source (vr_agbt_srcn: VR_AGBT_Source).
float VR_AGBT_SOURCES = 4;
float vr_agbt_kind;
float vr_agbt_wid;
float vr_agbt_srcn;
float vr_agbt_kd_at;    // the knockdown case: the grunt floored then
void(float c) VR_AGBT_Decode =
{
    vr_agbt_kind = floor(c / (5 * VR_AGBT_SOURCES));
    vr_agbt_wid = VR_AGBT_MeleeWid(floor(c / VR_AGBT_SOURCES) - 5 * vr_agbt_kind);
    vr_agbt_srcn = c - VR_AGBT_SOURCES * floor(c / VR_AGBT_SOURCES);
};

// Test 61: case `c` (kind, weapon, source) set up: the grunt (vr_agt_grunt) and the weapon it is to take.
void(float c) VR_AGBT_PickupBegin =
{
    entity p = vr_agt_player;
    VR_AGBT_Decode(c);
    float wid = vr_agbt_wid;
    float src = vr_agbt_srcn;
    cvar_set("vr_agrunt_test_kind", ftos(vr_agbt_kind));
    VR_AGT_ClearWeapons();
    p.flags = p.flags | FL_NOTARGET;
    entity g;
    vr_agbt_kd_at = 0;
    if(src == 1 || src == 3)
    {
        g = VR_AGT_Spawn(wid, 160);
        if(src == 1)
        {
            VR_AG_Disarm(g, '0 1 0', 60, "the test");
        }
        else
        {
            vr_agbt_kd_at = time + 0.5; // (floored then: it lets go, gets up, takes it back)
        }
        vr_agbt_src = world;
    }
    else
    {
        g = VR_AGT_Spawn(-1, 200);
        makevectors(p.angles);
        vector fw = v_forward;
        fw_z = 0;
        fw = normalize(fw);
        if(src == 0)
        {
            p.angles_y = p.angles_y + 25;
            entity other = VR_AGT_Spawn(wid, 150); // (another dropping it: it lies there)
            p.angles_y = p.angles_y - 25;
            vr_agbt_src = VR_AG_DropHeld(other, '0 0 0');
            VR_AGT_Remove(other);
        }
        else
        {
            vr_agbt_src = CreateThrownWeapon(p, wid, 0, WeaponInst_New(VR_AG_ClipSize(wid)), p.origin + fw * 60 + '0 0 20',
                p.angles, fw * 160 + '0 0 120', '0 0 0', FALSE, FALSE, WeaponIdToThrowDamage(wid));
        }
    }
    vr_agt_grunt = g;
    vr_agbt_val = g.vr_ag_pickups;
};

void() VR_AGBT_Pickup =
{
    entity g = vr_agt_grunt;
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        vr_agbt_count = 10 * VR_AGBT_SOURCES;
        vr_agbt_keep = cvar("scratch1");
        vr_agbt_case = vr_agbt_keep > 0 ? vr_agbt_keep - 1 : 0;
        vr_agbt_val2 = cvar("vr_agrunt_test_kind");
        vr_agt_step = 1;
    }
    if(vr_agt_step == 1)
    {
        if(vr_agbt_case >= vr_agbt_count || (vr_agbt_keep > 0 && vr_agbt_case != vr_agbt_keep - 1))
        {
            VR_AGT_Print(sprintf("pickups: %g passed, %g failed", vr_agt_passed, vr_agt_failed));
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            cvar_set("vr_agrunt_test_kind", ftos(vr_agbt_val2));
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        VR_AGBT_PickupBegin(vr_agbt_case);
        vr_agt_step = 2;
        vr_agt_until = time + 15;
        return;
    }
    if(vr_agt_step == 2)
    {
        if(vr_agbt_kd_at > 0 && time >= vr_agbt_kd_at)
        {
            vr_agbt_kd_at = 0;
            VR_Knockdown_Start(g); // (floored: VR_AG_FallDrop lets it go)
        }
        // Taken up, its hand closed on it a while (its lift: vr_armedgrunt_pose.qc VR_AG_LIFT_TIME).
        if((g.vr_ag_pickups <= vr_agbt_val || time < g.vr_ag_took_grab + 0.8) && time < vr_agt_until && g.health > 0)
        {
            return;
        }
        VR_AGBT_Decode(vr_agbt_case);
        float kind = vr_agbt_kind;
        float wid = vr_agbt_wid;
        entity pr = g.vr_ag_prop;
        entity fresh = VR_AGT_Spawn(wid, 320); // (the spawn path's prop, the same weapon: its model's index)
        float spawned = fresh.vr_ag_prop.modelindex;
        string want = fresh.vr_ag_prop.model;
        VR_AGT_Remove(fresh);
        float d = pr ? vlen(pr.origin - g.origin) : -1;
        string what = sprintf("pickup %s %s %s: %g pickups, model %s", kind ? "enforcer" : "grunt", WeaponNameOf(wid, 0),
            VR_AGBT_Source(vr_agbt_srcn), g.vr_ag_pickups - vr_agbt_val,
            pr ? pr.model : "none"); // (sprintf: 8 values at most)
        VR_AGT_Check(sprintf("%s (index %g, spawned %g), effects %g, alpha %g, %.0f from it:", what, pr.modelindex,
            spawned, pr.effects, pr.alpha, d), pr && pr.modelindex > 0 && pr.modelindex == spawned && pr.model == want &&
            (pr.effects & VR_AG_EF_HELD) && (pr.alpha == 0 || pr.alpha >= 0.5) && d < 64 && g.vr_ag_pickups > vr_agbt_val);
        if(vr_agbt_keep > 0)
        {
            vr_agbt_case = vr_agbt_count; // (left standing for an eyeshot)
            vr_agt_step = 1;
            return;
        }
        VR_AGT_Remove(g);
        vr_agbt_case = vr_agbt_case + 1;
        vr_agt_step = 1;
    }
};

// Test 62: walking into an armed grunt's blows.
float(float i) VR_AGBT_WalkWid =
{
    if(i < 5) { return VR_AGBT_MeleeWid(i); }
    if(i == 5) { return -1; }
    if(i == 6) { return WID_SHOTGUN; }
    return WID_NAILGUN;
};

void() VR_AGBT_WalkIn =
{
    entity g = vr_agt_grunt;
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        vr_agbt_count = 8;
        vr_agbt_case = 0;
        vr_agbt_val2 = 0;
        vr_agt_step = 1;
    }
    if(vr_agt_step == 1)
    {
        if(vr_agbt_case >= vr_agbt_count)
        {
            VR_AGT_Check(sprintf("walkin: %g drops:", vr_agbt_val2), vr_agbt_val2 == 0);
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        VR_AGT_ClearWeapons();
        vr_agt_grunt = VR_AGT_Spawn(VR_AGBT_WalkWid(vr_agbt_case), 90);
        vr_agbt_val = vr_ag_player_dmg;
        vr_agt_step = 2;
        vr_agt_until = time + 0.4;
        return;
    }
    if(vr_agt_step == 2)
    {
        if(time < vr_agt_until) { return; }
        VR_AGT_Hunt(g);
        vr_agt_step = 3;
        vr_agt_until = time + 10;
        return;
    }
    if(time < vr_agt_until && g.health > 0)
    {
        // You walk into it (pushed at it, as a step or the stick's walk would: your body against it, your hands idle).
        vector to = g.origin - p.origin;
        to_z = 0;
        if(vlen(to) > 24 && (p.flags & FL_ONGROUND))
        {
            to = normalize(to) * 200;
            p.velocity_x = to_x;
            p.velocity_y = to_y;
        }
        return;
    }
    VR_AGT_Print(sprintf("walkin %s: %g disarms, %g blows, %g damage to you", VR_AGT_Name(VR_AGBT_WalkWid(vr_agbt_case)),
        g.vr_ag_disarms, g.vr_ag_strikes, vr_ag_player_dmg - vr_agbt_val));
    vr_agbt_val2 = vr_agbt_val2 + g.vr_ag_disarms;
    VR_AGT_Remove(g);
    vr_agbt_case = vr_agbt_case + 1;
    vr_agt_step = 1;
};

// Test 63: friendly fire.
float(float i) VR_AGBT_GunWid =
{
    if(i == 0) { return WID_SHOTGUN; }
    if(i == 1) { return WID_NAILGUN; }
    if(i == 2) { return WID_SUPER_SHOTGUN; }
    return WID_GRUNTGUN;
};

void() VR_AGBT_RowRemove =
{
    float k;
    for(k = 0; k < 8; k++)
    {
        if(vr_agbt_row[k] && vr_agbt_row[k].vr_ag && vr_agbt_row[k].health > 0)
        {
            VR_AGT_Remove(vr_agbt_row[k]);
        }
        vr_agbt_row[k] = world;
    }
};

void() VR_AGBT_Friendly =
{
    entity p = vr_agt_player;
    float k;
    if(vr_agt_step == 0)
    {
        vr_agbt_care = cvar("vr_agrunt_friendly_care");
        vr_agbt_case = 0;
        vr_agt_step = 1;
    }
    if(vr_agt_step == 1)
    {
        // The row: gunners 420 ahead, 70 apart; a friend in each one's line, 200 ahead of you.
        VR_AGT_ClearWeapons();
        cvar_set("vr_agrunt_friendly_care", vr_agbt_case == 0 ? "0" : ftos(vr_agbt_care));
        makevectors(p.angles);
        vector side = v_right;
        side_z = 0;
        side = normalize(side);
        vector keep = p.origin;
        for(k = 0; k < 4; k++)
        {
            setorigin(p, keep + side * ((k - 1.5) * 70));
            vr_agbt_row[k] = VR_AGT_Spawn(VR_AGBT_GunWid(k), 420);
            vr_agbt_row[k + 4] = VR_AGT_Spawn(-1, 200);
            vr_agbt_row[k + 4].health = 1000; // (they stand there: never killed by a stray shot)
        }
        setorigin(p, keep);
        vr_agbt_val = vr_ag_ff_hits;
        vr_agbt_val2 = vr_ag_ff_dmg;
        vr_agt_dmg0 = vr_ag_player_dmg;
        vr_agt_clip = vr_ag_shots_fired;
        vr_agt_index = vr_ag_ff_held;
        vr_agt_step = 2;
        vr_agt_until = time + 0.4;
        return;
    }
    if(vr_agt_step == 2)
    {
        if(time < vr_agt_until) { return; }
        for(k = 0; k < 4; k++)
        {
            VR_AGT_Hunt(vr_agbt_row[k]);
        }
        vr_agt_step = 3;
        vr_agt_until = time + 15;
        return;
    }
    // The friends held still (their thinking put off), always in the way.
    for(k = 4; k < 8; k++)
    {
        entity f = vr_agbt_row[k];
        if(f.health > 0)
        {
            f.nextthink = time + 0.2;
            f.enemy = world;
            f.velocity = '0 0 0';
        }
    }
    if(time < vr_agt_until)
    {
        return;
    }
    float hits = vr_ag_ff_hits - vr_agbt_val;
    float pdmg = vr_ag_player_dmg - vr_agt_dmg0;
    VR_AGT_Print(sprintf("friendly care %g: %g friendly hits (%g damage), %g shots, %g held, %g damage to you",
        cvar("vr_agrunt_friendly_care"), hits, vr_ag_ff_dmg - vr_agbt_val2, vr_ag_shots_fired - vr_agt_clip,
        vr_ag_ff_held - vr_agt_index, pdmg));
    VR_AGBT_RowRemove();
    if(vr_agbt_case == 0)
    {
        vr_agt_failed = hits; // (the first run's hits)
        vr_agbt_case = 1;
        vr_agt_step = 1;
        return;
    }
    VR_AGT_Print(sprintf("friendly fire: %g hits careless, %g careful, %g damage to you: %s", vr_agt_failed, hits, pdmg,
        ((hits < vr_agt_failed || vr_agt_failed == 0) && pdmg > 0) ? "PASS" : "FAIL"));
    cvar_set("vr_agrunt_friendly_care", ftos(vr_agbt_care));
    cvar_set("vr_agrunt_test", "0");
};

// Test 64 (the scenario agrunt_friendly_fire): a crowd of armed gunners hunting you, two loose rows, the back row
// behind the front one (their friends in the way: vr_agrunt_friendly_care).
void() VR_AGBT_Crowd =
{
    entity p = vr_agt_player;
    makevectors(p.angles);
    vector side = v_right;
    side_z = 0;
    side = normalize(side);
    vector keep = p.origin;
    float k;
    for(k = 0; k < 8; k++)
    {
        setorigin(p, keep + side * ((k - 4 * floor(k / 4) - 1.5) * 64 + (k >= 4 ? 20 : 0)));
        VR_AGT_Hunt(VR_AGT_Spawn(VR_AGBT_GunWid(k - 4 * floor(k / 4)), k >= 4 ? 460 : 300));
    }
    setorigin(p, keep);
    cvar_set("vr_agrunt_test", "0");
};

// VR_AGTest_Frame's tests 61-64.
void(float t) VR_AGBT_Frame =
{
    if(t == 61) { VR_AGBT_Pickup(); }
    else if(t == 62) { VR_AGBT_WalkIn(); }
    else if(t == 63) { VR_AGBT_Friendly(); }
    else if(t == 64) { VR_AGBT_Crowd(); }
};
