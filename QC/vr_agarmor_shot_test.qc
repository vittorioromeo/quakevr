// vr_agarmor_shot_test.qc -- armour against real shots (vr_agarmor.qc; ROUND23.md, "Armour: the plate a shot meets").
// `vr_agarmor_test 10`: Plate Blocks All (vr_agarmor_rule 1): an armed grunt, enforcer, knight and death knight (kinds
// 0-3) ahead in full red, still; at every plate a line that meets the plate's outer face first (a MOVE_HITMODEL trace
// from outside ends at it), and down that line each weapon in turn, fired for real by the player: a shotgun pellet, a
// revolver's slug (FireBulletsImpl), a nail (launch_spike), a plasma bolt (launch_plasma), a rocket's direct hit
// (VR_LaunchRocket). Each: no damage to the body, the plates took it. Ends on "agarmor shot test: N passed, M failed".
// `vr_agarmor_test 11`: the push of a shot a plate absorbs (vr_agarmor_impulse): a bare and a fully armoured grunt
// killed (their ragdolls), each shot on the left thigh by the same small slug: the armoured thigh's part gains about
// vr_agarmor_impulse of the bare one's speed (between 0.3 and 0.8 of it at the default 0.5).
// `vr_agarmor_test 12`: a broken plate thrown (VR_Carry_Throw, the player's throw) into a grunt hurts it (forcegrabbable_touch,
// mass and speed), and a plate in the fist has a prop's blow (its Melee Damage x).

// (vr_ars_busy: vr_agarmor_test.qc; the request running, 10-12, 0 none)
float vr_ars_step;
float vr_ars_until; // a give-up time (FAIL "timed out")
float vr_ars_rule, vr_ars_dtime, vr_ars_pdmg; // put back after
entity vr_ars_e0, vr_ars_e1, vr_ars_e2, vr_ars_e3;
float vr_ars_n;       // test 10: the shot (body x 50 + piece x 5 + weapon)
entity vr_ars_proj;   // its projectile in flight
entity vr_ars_targ;   // whom it was aimed at
float vr_ars_h0, vr_ars_hp0, vr_ars_m0; // its health, plates' pool and pieces before
float vr_ars_fails, vr_ars_noline;
float vr_ars_shots0; // vr_ar_shots before the shot
vector vr_ars_dir;    // test 11: the shot's way
float vr_ars_bone0, vr_ars_bone1;
vector vr_ars_v0, vr_ars_v1;

void(entity attacker, float damage) VR_ARS_NoPain = {};

entity(float i) VR_ARS_Body =
{
    return i == 0 ? vr_ars_e0 : i == 1 ? vr_ars_e1 : i == 2 ? vr_ars_e2 : vr_ars_e3;
};

// Still: no thinking, no pain frames (its drawn pose is its stand).
void(entity e) VR_ARS_Still =
{
    e.think = SUB_Null;
    e.nextthink = 0;
    e.th_pain = VR_ARS_NoPain;
    e.health = 1000;
};

float(entity e) VR_ARS_Pool =
{
    float i, s = 0;
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        s = s + e.vr_ar_hp[i];
    }
    return s;
};

// A line to plate `i` of `e`: its outer face's middle (limbpiece 14) seen from outside, out from the body, from ahead,
// from either side, from above; the way out ('0 0 0': none clear). A MOVE_HITMODEL trace down it ends at that point (within
// the guns' tolerance and the plate's own thickness): nothing of the body in front of the plate there.
vector(entity e, float i) VR_ARS_Line =
{
    vector at = limbpiece(e, i, 14);
    makevectors([0, e.angles_y, 0]);
    vector radial = at - e.origin;
    radial_z = 0;
    radial = normalize(radial);
    entity pl = VR_Player();
    float k;
    for(k = 0; k < 6; k++)
    {
        vector out = k == 0 ? radial : k == 1 ? v_forward : k == 2 ? v_right : k == 3 ? -v_right : k == 4 ? '0 0 1' : -v_forward;
        traceline(at + out * 48, at - out * 8, MOVE_HITMODEL, pl);
        if((trace_ent == e) && (vlen(trace_endpos - at) < 3.5))
        {
            return out;
        }
    }
    return '0 0 0';
};

string(float w) VR_ARS_WeaponName =
{
    return w == 0 ? "pellet" : w == 1 ? "revolver" : w == 2 ? "nail" : w == 3 ? "plasma" : "rocket";
};

// Test 10's result of shot vr_ars_n (just landed, or its projectile gone).
void() VR_ARS_Judge =
{
    entity e = vr_ars_targ;
    float b = floor(vr_ars_n / 50), i = floor(vr_ars_n / 5) - b * 10, w = vr_ars_n - floor(vr_ars_n / 5) * 5;
    float took = vr_ars_h0 - e.health;
    float plates = vr_ars_hp0 - VR_ARS_Pool(e);
    float ok = (took <= 0) && ((plates > 0) || (e.vr_ar_mask != vr_ars_m0));
    if(!ok)
    {
        vr_ars_fails = vr_ars_fails + 1;
        VR_Test_Count(FALSE);
        VR_Test_Print(sprintf("agarmor shot test: FAIL kind %g piece %g %s: the body took %g, the plates %g (pieces %g -> %g; shot asked %g, its piece %g, %s at %v)\n",
            b, i, VR_ARS_WeaponName(w), took, plates, vr_ars_m0, e.vr_ar_mask, vr_ars_shots - vr_ars_shots0, vr_ar_shot_piece,
            vr_ar_shot_ent == e ? "on it" : "elsewhere", vr_ar_shot_at - limbpiece(e, i, 14)));
    }
    else
    {
        VR_Test_Count(TRUE);
    }
};

// Test 10: the next shot fired (FALSE: none left).
float() VR_ARS_Fire =
{
    entity p = VR_Player();
    while(vr_ars_n < 200)
    {
        float b = floor(vr_ars_n / 50), i = floor(vr_ars_n / 5) - b * 10, w = vr_ars_n - floor(vr_ars_n / 5) * 5;
        entity e = VR_ARS_Body(b);
        VR_AgArmor_Give(e, 3, 1023);
        VR_ARS_Still(e);
        vector out = VR_ARS_Line(e, i);
        if(out == '0 0 0')
        {
            if(w == 0)
            {
                vr_ars_noline = vr_ars_noline + 1;
                VR_Test_Print(sprintf("agarmor shot test: kind %g piece %g: no clear line to its face (skipped)\n", b, i));
            }
            vr_ars_n = vr_ars_n + 1;
            continue;
        }
        vector at = limbpiece(e, i, 14);
        vr_ars_targ = e;
        vr_ars_h0 = e.health;
        vr_ars_hp0 = VR_ARS_Pool(e);
        vr_ars_m0 = e.vr_ar_mask;
        vr_ars_shots0 = vr_ar_shots;
        vr_ar_shot_ent = world;
        vr_ars_proj = world;
        entity oldself = self;
        self = p;
        if(w <= 1)
        {
            vr_pellet_damage = w == 0 ? 4 : 35;
            FireBulletsImpl(1, -out, '0 0 0', at + out * 48, 96, 0);
            vr_pellet_damage = vr_ars_pdmg;
        }
        else if(w == 2)
        {
            vr_ars_proj = launch_spike(at + out * 24, -out);
        }
        else if(w == 3)
        {
            vr_ars_proj = launch_plasma(at + out * 24, -out);
        }
        else
        {
            vr_ars_proj = VR_LaunchRocket(p, at + out * 24, -out);
        }
        self = oldself;
        vr_ars_until = time + 1;
        if(vr_ars_proj == world)
        {
            VR_ARS_Judge();
            vr_ars_n = vr_ars_n + 1;
            continue; // (hitscan: judged at once; the next shot this frame too)
        }
        return TRUE;
    }
    return FALSE;
};

void() VR_ARS_End =
{
    cvar_set("vr_agarmor_rule", ftos(vr_ars_rule));
    cvar_set("vr_agarmor_debris_time", ftos(vr_ars_dtime));
    vr_pellet_damage = vr_ars_pdmg;
    float i;
    for(i = 0; i < 4; i++)
    {
        entity e = VR_ARS_Body(i);
        if(e != world && e.modelindex != 0)
        {
            remove(e);
        }
    }
    vr_ars_e0 = vr_ars_e1 = vr_ars_e2 = vr_ars_e3 = world;
    vr_ars_busy = 0;
    VR_Test_Verdict();
};

// Test 11: the struck part's speed along the shot (u/s) gained since `v0`.
float(entity e, float bone, vector v0) VR_ARS_Gain =
{
    return (limbplace(e, bone, 2) - v0) * vr_ars_dir;
};

void(float request) VR_ARS_Frame =
{
    entity p = VR_Player();
    float i;
    if(request)
    {
        VR_Test_BeginFlags("agarmor shot test", VR_TEST_LOUD);
        vr_ars_rule = cvar("vr_agarmor_rule");
        vr_ars_dtime = cvar("vr_agarmor_debris_time");
        vr_ars_pdmg = vr_pellet_damage;
        cvar_set("vr_agarmor_rule", "1");
        cvar_set("vr_agarmor_debris_time", "0.5");
        vr_ars_busy = request;
        vr_ars_step = 0;
        vr_ars_n = 0;
        vr_ars_fails = 0;
        vr_ars_noline = 0;
        vr_ars_until = time + 8;
        if(request == 10)
        {
            vr_ars_e0 = VR_ART_SpawnKind(p, -150, 0, 160);
            vr_ars_e1 = VR_ART_SpawnKind(p, -50, 1, 160);
            vr_ars_e2 = VR_ART_SpawnKind(p, 50, 2, 160);
            vr_ars_e3 = VR_ART_SpawnKind(p, 150, 3, 180);
            for(i = 0; i < 4; i++)
            {
                VR_ARS_Still(VR_ARS_Body(i));
            }
        }
        else if(request == 11)
        {
            vr_ars_e0 = VR_ART_SpawnKind(p, -60, 0, 120);
            vr_ars_e1 = VR_ART_SpawnKind(p, 60, 0, 120);
            VR_AgArmor_Give(vr_ars_e1, 3, 1023);
        }
        else
        {
            vr_ars_e0 = VR_ART_SpawnKind(p, 0, 0, 96);
            VR_ARS_Still(vr_ars_e0);
            vr_ars_e0.health = 100;
        }
        return; // (their poses first drawn)
    }
    if(vr_ars_busy == 10)
    {
        if(vr_ars_proj != world)
        {
            if((vr_ars_proj.modelindex != 0) && (time < vr_ars_until))
            {
                return; // (in flight)
            }
            if(time >= vr_ars_until)
            {
                VR_ART_Check(FALSE, sprintf("shot %g: timed out (its projectile never landed)", vr_ars_n));
            }
            else
            {
                VR_ARS_Judge();
            }
            vr_ars_n = vr_ars_n + 1;
        }
        if(VR_ARS_Fire())
        {
            return;
        }
        VR_Test_Print(sprintf("agarmor shot test: %g shots, %g let damage through; %g plates with no clear line\n",
            vr_ars_n, vr_ars_fails, vr_ars_noline));
        VR_ART_Check(vr_ars_noline <= 4, sprintf("a clear line to most plates' faces (%g of 40 without)", vr_ars_noline));
        VR_ARS_End();
        return;
    }
    if(vr_ars_busy == 11)
    {
        entity a = vr_ars_e0, b = vr_ars_e1;
        if(vr_ars_step == 0)
        {
            a.health = 1;
            b.health = 1;
            T_Damage(a, p, p, 5);
            T_Damage(b, p, p, 5);
            vr_ars_step = 1;
            return;
        }
        if(vr_ars_step == 1)
        {
            // Both ragdolls, come to rest (each one's left thigh slower than 8 u/s).
            float ready = isragdoll(a) && isragdoll(b) && ((a.vr_corpse == 2) || a.vr_corpse_dying) && ((b.vr_corpse == 2) || b.vr_corpse_dying);
            vector pa = limbpiece(a, 6, 14);
            vector pb = limbpiece(b, 6, 14);
            vr_ars_bone0 = ragdollbone(a, pa);
            vr_ars_bone1 = ragdollbone(b, pb);
            ready = (ready && (vlen(limbplace(a, vr_ars_bone0, 2)) < 8) && (vlen(limbplace(b, vr_ars_bone1, 2)) < 8));
            if(!ready && time < vr_ars_until)
            {
                return;
            }
            if(!ready)
            {
                VR_ART_Check(FALSE, "timed out: the two ragdolls at rest");
                VR_ARS_End();
                return;
            }
            // The same slug at each one's left thigh, down the same way (from the player's side, level).
            vr_ars_dir = normalize([pa_x - p.origin_x, pa_y - p.origin_y, 0]);
            vr_ars_v0 = limbplace(a, vr_ars_bone0, 2);
            vr_ars_v1 = limbplace(b, vr_ars_bone1, 2);
            vr_ars_hp0 = VR_ARS_Pool(b);
            entity oldself = self;
            self = p;
            vr_pellet_damage = 6;
            FireBulletsImpl(1, vr_ars_dir, '0 0 0', pa - vr_ars_dir * 48, 96, 0);
            FireBulletsImpl(1, vr_ars_dir, '0 0 0', pb - vr_ars_dir * 48, 96, 0);
            vr_pellet_damage = vr_ars_pdmg;
            self = oldself;
            vr_ars_step = 2;
            return;
        }
        // (a frame on: the physics step has taken the pushes)
        float ga = VR_ARS_Gain(a, vr_ars_bone0, vr_ars_v0);
        float gb = VR_ARS_Gain(b, vr_ars_bone1, vr_ars_v1);
        float k = cvar("vr_agarmor_impulse");
        VR_Test_Print(sprintf("agarmor shot test: the thigh's gain: bare %.1f u/s, armoured %.1f u/s (%.2f; vr_agarmor_impulse %g); plates took %g\n",
            ga, gb, gb / max(0.01, ga), k, vr_ars_hp0 - VR_ARS_Pool(b)));
        VR_ART_Check(vr_ars_hp0 - VR_ARS_Pool(b) > 0, "the armoured thigh's plate took the slug");
        VR_ART_Check(ga > 1, sprintf("the bare thigh pushed (%.1f u/s)", ga));
        VR_ART_Check((gb > ga * (k - 0.2)) && (gb < ga * (k + 0.3)), sprintf("the armoured thigh pushed about %g of that (%.1f u/s)", k, gb));
        VR_ARS_End();
        return;
    }
    // 12: a plate thrown into a grunt; a plate's blow in the fist.
    entity g = vr_ars_e0;
    if(vr_ars_step == 0)
    {
        makevectors(p.angles);
        vector from = p.origin + p.view_ofs;
        vector to = (g.absmin + g.absmax) * 0.5;
        vr_ars_proj = VR_AgArmor_DebrisAt(g, 0, from + normalize(to - from) * 32, '0 0 0', '0 0 0');
        if(vr_ars_proj == world)
        {
            VR_ART_Check(FALSE, "a plate's debris made");
            VR_ARS_End();
            return;
        }
        VR_ART_Check(propvalue(vr_ars_proj, "melee_dmg") > 0, sprintf("a plate in the fist has a prop's blow (Melee Damage x %g, %g kg)",
            propvalue(vr_ars_proj, "melee_dmg"), propvalue(vr_ars_proj, "mass")));
        entity oldself2 = self;
        self = p;
        VR_Carry_Throw(vr_ars_proj, normalize(to - from) * 700);
        self = oldself2;
        vr_ars_h0 = g.health;
        vr_ars_step = 1;
        vr_ars_until = time + 2;
        return;
    }
    if((g.health >= vr_ars_h0) && (time < vr_ars_until))
    {
        return;
    }
    VR_ART_Check(g.health < vr_ars_h0, sprintf("the thrown plate hurt the grunt (%g, else timed out)", vr_ars_h0 - g.health));
    VR_ARS_End();
};
