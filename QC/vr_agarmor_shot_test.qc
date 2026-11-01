// vr_agarmor_shot_test.qc -- armour against real shots (vr_agarmor.qc; ROUND23.md, "Armour: the plate a shot meets").
// `vr_agarmor_test 10`: Plate Blocks All (vr_agarmor_rule 1): an armed grunt, enforcer, knight and death knight (kinds
// 0-3) ahead in full red, still, one at a time; at every plate a line that meets the plate's outer face first (a MOVE_HITMODEL trace
// from outside ends at it), and down that line each weapon in turn, fired for real by the player: a shotgun pellet, a
// revolver's slug (FireBulletsImpl), a nail (launch_spike), a plasma bolt (launch_plasma), a rocket's direct hit
// (VR_LaunchRocket). Each: no damage to the body, the plates took it. Ends on "agarmor shot test: N passed, M failed"
// (~10 s; the player in god mode meanwhile: the rockets' blasts killed him before, and the test stopped with him).
// `vr_agarmor_test 11`: the push of a shot a plate absorbs (vr_agarmor_impulse): a grunt killed (its ragdoll), shot on
// the left thigh by the same small slug bare, then in full red: the push's scale (VR_AgArmor_PushScale, what VR_ShotPush
// multiplies its momentum by) 1 bare, vr_agarmor_impulse armoured; the plate took the slug; the ragdoll's gains printed.
// Ends on its verdict.
// `vr_agarmor_test 12`: a broken plate thrown (VR_Carry_Throw, the player's throw) into a grunt hurts it (forcegrabbable_touch,
// mass and speed), and a plate in the fist has a prop's blow (its Melee Damage x).

// (vr_ars_busy: vr_agarmor_test.qc; the request running, 10-12, 0 none)
float vr_ars_step;
float vr_ars_until; // a give-up time (FAIL "timed out")
float vr_ars_rule, vr_ars_dtime, vr_ars_pdmg; // put back after
entity vr_ars_e0, vr_ars_e1, vr_ars_e2, vr_ars_e3;
float vr_ars_n;       // test 10: the shot (body x 50 + piece x 5 + weapon)
float vr_ars_kind;    // test 10: the kind standing now (vr_ars_e0; -1 none yet)
vector vr_ars_org;    // test 10: where it stands
float vr_ars_missed;  // test 10: projectiles that never met the body
float vr_ars_react;   // test 10: vr_hit_react, put back after
float vr_ars_god;     // test 10: the player's FL_GODMODE before, put back after
vector vr_ars_at;     // test 10: the aimed plate's face middle as fired (the body may move after its hit)
vector vr_ars_pre;    // test 10: the body's origin the last frame its projectile was in flight
vector vr_ars_fc0, vr_ars_fc; // test 10: the plate's face middle as fired, the last frame in flight (or settling)
float vr_ars_settle_n, vr_ars_settle; // test 10: the shot settling before it is fired, its give-up time
float vr_ars_t0;      // test 10: when it began
string vr_ars_why;    // test 10: what the face's own normal line met (VR_ARS_Line found none)
entity vr_ars_proj;   // its projectile in flight
entity vr_ars_targ;   // whom it was aimed at
float vr_ars_h0, vr_ars_hp0, vr_ars_m0; // its health, plates' pool and pieces before
float vr_ars_fails, vr_ars_noline;
float vr_ars_shots0; // vr_ar_shots before the shot
vector vr_ars_dir;    // test 11: the shot's way
float vr_ars_bone1;   // test 11: its thigh's bone
float vr_ars_ragact; // test 11: vr_ragdoll_active, put back after
float vr_ars_push, vr_ars_pspeed; // test 11: vr_shot_push, vr_shot_push_speed, put back after
vector vr_ars_v1;    // test 11: its parts' summed velocity before the shot

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
    // (its start never thinks now: what walkmonster_start_go would have made it, shootable; before, every hitscan shot
    // passed it by (TraceAttack: no takedamage) and every projectile's damage was refused: test 10's 185 failures)
    e.takedamage = DAMAGE_AIM;
    e.flags = e.flags | FL_MONSTER;
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

// A line to plate `i` of `e`: its outer face's middle (limbpiece 14) seen from outside, along the face's own outward
// normal there (limbpiece 15), else out from the body, from ahead, from either side, from above, whichever is within 45
// degrees of that normal; the way out ('0 0 0': none clear). A MOVE_HITMODEL trace down it ends at that point (within
// the guns' tolerance and the plate's own thickness): nothing of the body in front of the plate there. (Before, the
// first clear one of those, normal or not: a wrapped limb plate seen from the side has its face's middle on its outline,
// and a projectile a fifth of a unit off the line passed the plate by: test 10's last 12 "leaks".)
// Whether the line to `at` along `dir` meets plate `i` first, as the shots' own query sees it, and so do the lines half a
// unit beside it four ways (a projectile flies up to that far off the aimed line). (Without: the 15% and 85% points
// lie near a plate's end, and a line tilted toward it passed the plate's edge: 7 "leaks" that met no plate at all.)
float(entity e, float i, vector at, vector dir) VR_ARS_Meets =
{
    vector su = fabs(dir_z) < 0.9 ? normalize([dir_y, -dir_x, 0]) : normalize([0, dir_z, -dir_y]);
    vector sw = [dir_y * su_z - dir_z * su_y, dir_z * su_x - dir_x * su_z, dir_x * su_y - dir_y * su_x];
    float j;
    for(j = 0; j < 5; j++)
    {
        vector off = j == 0 ? '0 0 0' : j == 1 ? su * 0.5 : j == 2 ? su * -0.5 : j == 3 ? sw * 0.5 : sw * -0.5;
        if(ragdollbone(e, at + off, QVR_ARMOR_SHOT, dir) != i)
        {
            return FALSE;
        }
    }
    return TRUE;
};

// The point it aims at: vr_ars_lineat. (Its face's middle first, then the points 15% and 85% along it (limbpiece 12, 13:
// the arc's middle there too) with the middle's normal: a grunt's gun before its cuirass, an arm or the other leg before a
// limb plate took every line to the middle of 12 of 40 plates.)
vector vr_ars_lineat;

vector(entity e, float i) VR_ARS_Line =
{
    vector nrm = limbpiece(e, i, 15);
    makevectors([0, e.angles_y, 0]);
    entity pl = VR_Player();
    // (and the normal tilted 25 and 40 degrees eight ways round: the normal's own line was blocked at 20 of 40 plates)
    vector tu = fabs(nrm_z) < 0.9 ? normalize([nrm_y, -nrm_x, 0]) : normalize([0, nrm_z, -nrm_y]); // (nrm x up, nrm x x)
    vector tw = [nrm_y * tu_z - nrm_z * tu_y, nrm_z * tu_x - nrm_x * tu_z, nrm_x * tu_y - nrm_y * tu_x];
    float k;
    vector mid = limbpiece(e, i, 14), lo = limbpiece(e, i, 12), hi = limbpiece(e, i, 13);
    for(k = 0; k < 115; k++)
    {
        // (the middle, its ends' 15% and 85% points, and halfway to each: the arc's middle runs about straight along it)
        float pt = floor(k / 23);
        vector at = pt == 0 ? mid : pt == 1 ? (mid + lo) * 0.5 : pt == 2 ? (mid + hi) * 0.5 : pt == 3 ? lo : hi;
        vr_ars_lineat = at;
        vector radial = at - e.origin;
        radial_z = 0;
        radial = normalize(radial);
        float kk = k - floor(k / 23) * 23;
        vector out;
        if(kk >= 1 && kk <= 16)
        {
            float az = (kk - 1) * 45 * 0.0174533, tilt = (kk <= 8 ? 25 : 40) * 0.0174533; // (radians)
            out = nrm * cos(tilt) + (tu * cos(az) + tw * sin(az)) * sin(tilt);
        }
        else
        {
            float k2 = kk == 0 ? 0 : kk - 16;
            out = k2 == 0 ? nrm : k2 == 1 ? radial : k2 == 2 ? v_forward : k2 == 3 ? v_right : k2 == 4 ? -v_right : k2 == 5 ? '0 0 1' : -v_forward;
        }
        if(out * nrm < 0.7)
        {
            continue;
        }
        // (clear from 56 units out, past where the shots start (48, 44); before, 80, and a wall behind a hell knight's
        // bracer took its every line there)
        // (and no wall or floor on it: a line from below a shin's plate began under the floor, a trace from inside it
        // passed, and the nails and bolts fired from there met the floor: most of the "missed the body")
        traceline(at + out * 56, at, TRUE, pl); // (nomonsters)
        float wayClear = (!trace_startsolid && (trace_fraction == 1));
        traceline(at + out * 56, at - out * 8, MOVE_HITMODEL, pl);
        if(wayClear && (trace_ent == e) && (vlen(trace_endpos - at) < 3.5) && VR_ARS_Meets(e, i, at, -out))
        {
            return out;
        }
        if(k == 0)
        {
            vr_ars_why = sprintf("its normal %v met %s %.1f from it", nrm, trace_ent.classname, vlen(trace_endpos - at));
        }
    }
    vr_ars_lineat = limbpiece(e, i, 14);
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
    if(!ok && w >= 2 && vr_proj_target != e)
    {
        // (a projectile that never met the body: no leak, counted apart; its blast's splash on the body, if any, is a
        // blast's (the plates facing it take their shares), not a hit on the plate)
        vr_ars_missed = vr_ars_missed + 1;
        VR_Test_Print(sprintf("agarmor shot test: kind %g piece %g %s: missed the body (met %s; its splash: body %g, plates %g)\n",
            b, i, VR_ARS_WeaponName(w), vr_proj_target.classname, took, plates));
        return;
    }
    if(!ok)
    {
        vr_ars_fails = vr_ars_fails + 1;
        VR_Test_Count(FALSE);
        string head = sprintf("agarmor shot test: FAIL kind %g piece %g %s: the body took %g, the plates %g (pieces %g -> %g;",
            b, i, VR_ARS_WeaponName(w), took, plates, vr_ars_m0, e.vr_ar_mask);
        // (points from the aimed face's middle as it was fired at: a blast's knockback may have moved the body since)
        head = sprintf("%s shot asked %g, its piece %g, %s at %v;", head, vr_ar_shots - vr_ars_shots0, vr_ar_shot_piece,
            (vr_ar_shot_ent == e) ? "on it" : "elsewhere", vr_ar_shot_at - vr_ars_at);
        VR_Test_Print(sprintf("%s projectile met %s at %v, %.1f along the line, its flight from %v;\n", head,
            vr_proj_target.classname, vr_proj_at - vr_ars_at, (vr_proj_at - vr_ars_at) * vr_ars_dir, vr_ar_fly_from - vr_ars_at));
        // (the plate asked again now from that point, and from its face's middle down the aimed line; the body moved)
        VR_Test_Print(sprintf("agarmor shot test:   asked now %g, from the face %g; the body moved %v, %v by its hit's frame; velocity %v, movetype %g, flags %g)\n",
            ragdollbone(e, vr_proj_at, QVR_ARMOR_SHOT, vr_proj_dir),
            ragdollbone(e, vr_ars_at, QVR_ARMOR_SHOT, vr_ars_dir), e.origin - vr_ars_org, vr_ars_pre - vr_ars_org,
            e.velocity, e.movetype, e.flags));
        VR_Test_Print(sprintf("agarmor shot test:   its face moved %v by its hit's frame, %v now; frame %g)\n",
            vr_ars_fc - vr_ars_fc0, limbpiece(e, i, 14) - vr_ars_fc0, e.frame));
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
        if(b != vr_ars_kind)
        {
            // One body at a time (a lighter frame: before, all four stood there through every shot, and the test ran
            // past its 1500 s timeout on a loaded machine): the last one gone, this kind spawned, its pose drawn first.
            if(vr_ars_e0 != world && vr_ars_e0.modelindex != 0)
            {
                remove(vr_ars_e0);
            }
            vr_ars_e0 = VR_ART_SpawnKind(p, 0, b, b == 3 ? 180 : 160);
            VR_ARS_Still(vr_ars_e0);
            vr_ars_org = vr_ars_e0.origin;
            vr_ars_kind = b;
            VR_Test_Print(sprintf("agarmor shot test: kind %g (%s) at %.1f s\n", b, vr_ars_e0.classname, time - vr_ars_t0));
            vr_ars_proj = world; // (the last kind's last projectile, gone, is not this shot's)
            return TRUE; // (no projectile: the next frame fires)
        }
        entity e = vr_ars_e0;
        // (at rest: a blast's knockback carried it on between shots, and the next shot's line went to a plate that was no
        // longer there by the time its projectile arrived; where it is now is where this shot's line is drawn to)
        e.velocity = '0 0 0';
        // (and on the floor: a blast's knockback lifted it off (no FL_ONGROUND), and it sank half a unit while the next
        // projectile flew: its plate below the line that had met it, 3 "leaks" (body moved 0 0 -0.5 by its hit's frame))
        entity oldself0 = self;
        self = e;
        droptofloor();
        self = oldself0;
        e.flags = e.flags | FL_ONGROUND;
        vr_ars_org = vr_ars_pre = e.origin;
        // (and the last shot's broken plates gone: one falling across the line took the next projectile)
        entity deb = find(world, classname, "vr_ar_debris");
        while(deb != world)
        {
            entity debNext = find(deb, classname, "vr_ar_debris");
            remove(deb);
            deb = debNext;
        }
        VR_AgArmor_Give(e, 3, 1023);
        VR_ARS_Still(e);
        // (and its pose still: the aimed plate's face where it was last frame. The last shot's hit swings the limb for a
        // few frames (the knight's pauldron 2 units by the time the next projectile, fired the same frame, arrived), so
        // its line went to where the plate no longer was: the last "leaks", a nail and a rocket. Up to 1 s, then fired.)
        vector fc = limbpiece(e, i, 14);
        if(vr_ars_settle_n != vr_ars_n)
        {
            vr_ars_settle_n = vr_ars_n;
            vr_ars_settle = time + 1;
            vr_ars_fc = fc;
            return TRUE;
        }
        if((vlen(fc - vr_ars_fc) > 0.01) && (time < vr_ars_settle))
        {
            vr_ars_fc = fc;
            return TRUE;
        }
        vector out = VR_ARS_Line(e, i);
        if(out == '0 0 0')
        {
            if(w == 0)
            {
                vr_ars_noline = vr_ars_noline + 1;
                VR_Test_Print(sprintf("agarmor shot test: kind %g piece %g: no clear line to its face (skipped; %s)\n", b, i,
                    vr_ars_why));
            }
            vr_ars_n = vr_ars_n + 1;
            continue;
        }
        vector at = vr_ars_lineat; // (the face point VR_ARS_Line found a clear line to)
        vr_ars_at = at;
        vr_ars_fc0 = vr_ars_fc = limbpiece(e, i, 14);
        vr_ars_dir = -out;
        vr_ars_targ = e;
        vr_ars_h0 = e.health;
        vr_ars_hp0 = VR_ARS_Pool(e);
        vr_ars_m0 = e.vr_ar_mask;
        vr_ars_shots0 = vr_ar_shots;
        vr_ar_shot_ent = world;
        vr_proj_target = world;
        vr_ars_proj = world;
        entity oldself = self;
        self = p;
        if(w <= 1)
        {
            vr_pellet_damage = w == 0 ? 4 : 35;
            FireBulletsImpl(1, -out, '0 0 0', at + out * 48, 96, 0);
            vr_pellet_damage = vr_ars_pdmg;
        }
        // (projectiles from 44 units out, past a monster's box as a missile meets it (15 units grown): a real shot's
        // touch, short of the model, not one inside the box from its first move)
        else if(w == 2)
        {
            vr_ars_proj = launch_spike(at + out * 44, -out);
        }
        else if(w == 3)
        {
            vr_ars_proj = launch_plasma(at + out * 44, -out);
            // (its flight at once: PlasmaLaunch's 0.1 s at rest first let the line go stale)
            entity launcher = self;
            self = vr_ars_proj;
            PlasmaLaunch();
            self = launcher;
        }
        else
        {
            vr_ars_proj = VR_LaunchRocket(p, at + out * 44, -out);
        }
        self = oldself;
        vr_ars_until = time + 2; // (a projectile that never lands: given up, a miss)
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
    if(vr_ars_busy == 11)
    {
        cvar_set("vr_ragdoll_active", ftos(vr_ars_ragact));
        cvar_set("vr_shot_push", ftos(vr_ars_push));
        cvar_set("vr_shot_push_speed", ftos(vr_ars_pspeed));
    }
    if(vr_ars_busy == 10)
    {
        cvar_set("vr_hit_react", ftos(vr_ars_react));
        entity pl = VR_Player();
        pl.flags = pl.flags - (pl.flags & FL_GODMODE) + vr_ars_god;
    }
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

// Test 11: the sum of e's ragdoll parts' velocities (u/s). Its gain along a shot is the push it took, whichever part
// took it (box3d::push gives it to the part whose box is nearest, and a lying body's boxes overlap: the bone nearest the
// plate was another part in some runs, and its gain read 0.1 or 22 u/s for the same push).
vector(entity e) VR_ARS_PartsVel =
{
    vector v = '0 0 0';
    float bone;
    for(bone = 0; bone < 32; bone++)
    {
        v = v + limbplace(e, bone, 2);
    }
    return v;
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
        vr_ars_missed = 0;
        vr_ars_until = time + 8;
        if(request == 10)
        {
            vr_ars_kind = -1; // (VR_ARS_Fire spawns each kind in turn)
            vr_ars_settle_n = -1;
            // (no flinch: a hit's kick moves the drawn pose, and so the plates, for half a second; the next shot's line,
            // aimed at the plate where it was, met skin by the time its projectile arrived)
            vr_ars_react = cvar("vr_hit_react");
            cvar_set("vr_hit_react", "0");
            // (the player in god mode: forty rockets' blasts 160 units off, a few points each, killed him by the last
            // kind, and the test, run from his frame, stopped with him: no verdict, the run's timeout)
            vr_ars_god = p.flags & FL_GODMODE;
            p.flags = p.flags | FL_GODMODE;
            vr_ars_t0 = time;
        }
        else if(request == 11)
        {
            vr_ars_e1 = VR_ART_SpawnKind(p, 60, 0, 120);
            VR_AgArmor_Give(vr_ars_e1, 3, 1023); // (worn as it spawns, as a map's; off for its death: step 0)
            // (limp ragdolls: an active one's death throes kept its thigh moving past the give-up time)
            vr_ars_ragact = cvar("vr_ragdoll_active");
            cvar_set("vr_ragdoll_active", "0");
            // (the slug's push and speed a hundred times over: the same light pusher (0.6 kg), so its impulse is about its
            // momentum and the two pushes' ratio about vr_agarmor_impulse (a heavy one's tends to 1); at 1x the thigh
            // gained 0.1-3 u/s, under a resting limp ragdoll's own jitter (up to 8 u/s))
            vr_ars_push = cvar("vr_shot_push");
            vr_ars_pspeed = cvar("vr_shot_push_speed");
            cvar_set("vr_shot_push", ftos(max(1, vr_ars_push) * 100));
            cvar_set("vr_shot_push_speed", ftos(max(1, vr_ars_pspeed) * 100));
            vr_ars_until = time + 15;
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
            // (judged the frame after its touch (VR_Proj_Begin's vr_proj_target), not once a rocket's explosion sprite
            // is done: the blast's knockback carried the body on meanwhile; or once it is gone, a wall's)
            if((vr_proj_target == world) && (vr_ars_proj.modelindex != 0) && (time < vr_ars_until))
            {
                vr_ars_pre = vr_ars_targ.origin;
                vr_ars_fc = limbpiece(vr_ars_targ, floor(vr_ars_n / 5) - floor(vr_ars_n / 50) * 10, 14);
                return; // (in flight)
            }
            if((vr_proj_target == world) && (vr_ars_proj.modelindex != 0))
            {
                // (never landed in 2 s: a miss, the next shot; before, a FAIL each, and the run went on)
                float gb = floor(vr_ars_n / 50), gi = floor(vr_ars_n / 5) - gb * 10;
                vr_ars_missed = vr_ars_missed + 1;
                VR_Test_Print(sprintf("agarmor shot test: kind %g piece %g %s: never landed (given up after 2 s, a miss; at %v from the face)\n",
                    gb, gi, VR_ARS_WeaponName(vr_ars_n - floor(vr_ars_n / 5) * 5), vr_ars_proj.origin - vr_ars_at));
                remove(vr_ars_proj);
            }
            else
            {
                VR_ARS_Judge();
            }
            vr_ars_n = vr_ars_n + 1;
            vr_ars_proj = world; // (judged: the next shot may wait for its pose to settle first)
        }
        if(VR_ARS_Fire())
        {
            return;
        }
        VR_Test_Print(sprintf("agarmor shot test: %g shots, %g let damage through; %g plates with no clear line\n",
            vr_ars_n, vr_ars_fails, vr_ars_noline));
        VR_Test_Print(sprintf("agarmor shot test: %g projectiles missed the body; %.1f s of game time\n", vr_ars_missed,
            time - vr_ars_t0));
        VR_ART_Check(vr_ars_noline <= 4, sprintf("a clear line to most plates' faces (%g of 40 without)", vr_ars_noline));
        VR_ART_Check(vr_ars_missed <= 12, sprintf("most projectiles met the body (%g missed)", vr_ars_missed));
        VR_ARS_End();
        return;
    }
    if(vr_ars_busy == 11)
    {
        // One grunt's ragdoll shot twice on its left thigh plate's face, bare then in full red: the same body lying the
        // same way, so the two pushes go into the same part (two bodies lay differently each run, and box3d::push gives a
        // shot to the part whose box is nearest: the ratio read 0.06 to 4.4 from run to run).
        entity b = vr_ars_e1;
        if(vr_ars_step == 0)
        {
            // (killed bare: under Plate Blocks All its plates took the killing blow, it lived on, beat the player to
            // death, and the test, the player's, ended with him: "timed out, no output at all")
            VR_AgArmor_Give(b, 0, 0);
            b.health = 1;
            T_Damage(b, p, p, 5);
            VR_ART_Check(b.health <= 0, sprintf("killed (health %g)", b.health));
            vr_ars_step = 1;
            return;
        }
        if(vr_ars_step == 1)
        {
            // Its ragdoll, come to rest (its left thigh slower than 8 u/s).
            vector pb = limbpiece(b, 6, 14);
            vr_ars_bone1 = ragdollbone(b, pb);
            float ready = isragdoll(b) && ((b.vr_corpse == 2) || b.vr_corpse_dying) && (vlen(limbplace(b, vr_ars_bone1, 2)) < 8);
            if(!ready && time < vr_ars_until)
            {
                return;
            }
            if(!ready)
            {
                VR_ART_Check(FALSE, sprintf("timed out: the ragdoll at rest (shot %g)", vr_ars_n));
                VR_ARS_End();
                return;
            }
            if(vr_ars_n == 1)
            {
                VR_AgArmor_Give(b, 3, 1023); // (the second shot: its plates on, on its ragdoll)
                if(cvar("vr_agarmor_debug"))
                {
                    localcmd("screenshot\n"); // (the debug view on the ragdoll at rest: Debug > Show Armour Plates)
                }
            }
            // The slug down a clear line to its thigh plate's face (VR_ARS_Line; before, level from the player's side: an
            // arm or the other leg lying across it took it in some runs), else level.
            vr_ars_dir = -VR_ARS_Line(b, 6);
            pb = vr_ars_lineat;
            vr_ars_dir = vr_ars_dir == '0 0 0' ? normalize([pb_x - p.origin_x, pb_y - p.origin_y, 0]) : vr_ars_dir;
            vr_ars_v1 = VR_ARS_PartsVel(b);
            vr_ars_hp0 = VR_ARS_Pool(b);
            // (the push's own scale: the plate query on its line: 1 bare, vr_agarmor_impulse armoured)
            float sb = VR_AgArmor_PushScale(b, pb, vr_ars_dir);
            float want = vr_ars_n == 1 ? cvar("vr_agarmor_impulse") : 1;
            VR_ART_Check(sb == want, sprintf("shot %g: the push's scale %g (%g)", vr_ars_n, sb, want));
            entity oldself = self;
            self = p;
            vr_pellet_damage = 6;
            FireBulletsImpl(1, vr_ars_dir, '0 0 0', pb - vr_ars_dir * 48, 96, 0);
            vr_pellet_damage = vr_ars_pdmg;
            self = oldself;
            vr_ars_step = 2;
            return;
        }
        // (a frame on: the physics step has taken the push)
        float g = (VR_ARS_PartsVel(b) - vr_ars_v1) * vr_ars_dir;
        if(vr_ars_n == 0)
        {
            vr_ars_h0 = g; // (the bare thigh's)
            vr_ars_n = 1;
            vr_ars_step = 1;
            vr_ars_until = time + 15;
            return;
        }
        float k = cvar("vr_agarmor_impulse");
        VR_Test_Print(sprintf("agarmor shot test: the parts' gain: bare %.1f u/s, armoured %.1f u/s (%.2f; vr_agarmor_impulse %g); plates took %g\n",
            vr_ars_h0, g, g / max(0.01, vr_ars_h0), k, vr_ars_hp0 - VR_ARS_Pool(b)));
        VR_ART_Check(vr_ars_hp0 - VR_ARS_Pool(b) > 0, "the armoured thigh's plate took the slug");
        // (the parts' gains are printed, not judged: the push goes to the part whose box is nearest (a lying body's boxes
        // overlap), a part driven into the floor stops, and the ratio read -490 to 4.4 from run to run; the push itself,
        // its momentum, is what vr_agarmor_impulse scales, checked above: VR_ShotPush's 6.0 N s bare, 3.0 armoured)
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
