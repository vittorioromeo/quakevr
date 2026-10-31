#ifdef QVR_TESTS
// Test 85 (`vr_agrunt_test 85`; ROUND23.md, "Dual guns: each shot from its own muzzle"): every dual-gun pair of a grunt
// and an enforcer (two shotguns, super shotguns, nail guns, super nail guns, nail pistols, grenade launchers,
// thunderbolts, laser rifles, AD's plasma guns with AD's data; the grunt's and the enforcer's own guns as a
// replacement holds them: vr_ag_native, their monster's own shot), one spawned 300 units ahead, its AI held, you unseen
// by its friends' lines: each gun fires three times in turn (VR_AG_ShootProp, as its attack frames do). Each shot's
// origin (its projectile's place as launched, its pellets' tracer start, the beam's start) is compared with the drawn
// muzzles of both guns (VR_AG_DrawnMuzzle): within 14 units of its own (VR_ShotPlace may start it behind what
// its muzzle is in) and nearer its own than the other's
// (`agtest: muzzle <case>: <n> shots, right off <r> / other <o>, left off <l> / other <o>: PASS|FAIL`; a pair it can't
// hold: SKIP). The run ends on `agtest: test 85 over: <p> passed, <f> failed`.

.float vr_agmz_seen;  // an entity of the shooter's there before the shot
float VR_AGMZ_N = 21;
float vr_agmz_kind;   // VR_AGMZ_Case's: the kind,
float vr_agmz_native; // ... its guns its monster's own (vr_ag_native)
float VR_AGMZ_BARREL = 12; // degrees a drawn gun's barrel may point off its aim (at you) before its shots

// Case `c`'s weapon (both hands), its kind (vr_agmz_kind) and whether its guns are native (vr_agmz_native). 16..20: the
// two-pistol (and two-shotgun) holds of every other kind: the enforcer's, the knight's, the death knight's (their grips:
// vr_aggrip.cpp).
float(float c) VR_AGMZ_Case =
{
    vr_agmz_kind = (c >= 19) ? VR_AM_DKNIGHT : (c >= 17) ? VR_AM_KNIGHT : (c >= 10) ? VR_AM_ENFORCER : 0;
    vr_agmz_native = ((c == 9) || (c == 10));
    if(c == 0 || c == 11 || c == 18 || c == 20) { return WID_SHOTGUN; }
    if(c == 1) { return WID_SUPER_SHOTGUN; }
    if(c == 2 || c == 12) { return WID_NAILGUN; }
    if(c == 3 || c == 13) { return WID_SUPER_NAILGUN; }
    if(c == 4 || c == 16 || c == 17 || c == 19) { return WID_NAILPISTOL; }
    if(c == 5 || c == 14) { return WID_GRENADE_LAUNCHER; }
    if(c == 6) { return WID_LIGHTNING; }
    if(c == 7 || c == 15) { return WID_ENFRIFLE; }
    if(c == 8) { return WID_ADPLASMA; }
    if(c == 9) { return WID_GRUNTGUN; }
    return WID_ENFRIFLE; // (10: the enforcer's own)
};

// `g`'s gun `pr` fired once: its shot's origin ('0 0 -99999': none seen).
vector(entity g, entity pr) VR_AGMZ_Fire =
{
    if(!pr || !g)
    {
        VR_AGT_Print(sprintf("muzzle: a gun gone (%s %s)", pr ? pr.netname : "none", g ? g.classname : "no shooter"));
        return '0 0 -99999';
    }
    entity e = nextent(world);
    while(e)
    {
        e.vr_agmz_seen = e.owner == g;
        e = nextent(e);
    }
    entity oldself = self;
    self = g;
    g.enemy = vr_agt_player;
    if(pr.weaponinst)
    {
        pr.weaponinst.wi_clip = max(pr.weaponinst.wi_clip, 8);
    }
    pr.vr_ag_hk = 0;
    vector none = '0 0 -99999';
    vr_tracer_from = none;
    g.muzzlepos = none;
    float before = pr.vr_ag_total_shots;
    VR_AG_ShootProp(pr);
    self = oldself;
    if(pr.vr_ag_total_shots == before)
    {
        return none; // (held: its friend, its blast)
    }
    e = nextent(world);
    while(e)
    {
        if(e.owner == g && !e.vr_agmz_seen && vlen(e.velocity) > 200)
        {
            return e.origin; // (its projectile)
        }
        e = nextent(e);
    }
    if(vr_tracer_from != none)
    {
        return vr_tracer_from; // (its pellets', its rounds')
    }
    return g.muzzlepos; // (the beam's)
};

// Degrees gun `pr`'s barrel is drawn off its aim at `at` (its drawn forward: the server's turned by weapondrawnpose 21);
// vr_agmz_sv: the server's own forward's (its pose's) degrees off it; vr_agmz_turn: the drawn barrel's off the server's.
float vr_agmz_sv;
float vr_agmz_turn;
float vr_agmz_restR, vr_agmz_restL; // ... each gun's at rest (before its shots)
float vr_agmz_shot;                 // vr_agmz_frames at its shots
float vr_agmz_lastR, vr_agmz_lastL; // ... each gun's the frame before (settled: alike)
float(entity pr, vector at) VR_AGMZ_Barrel =
{
    makevectors([-pr.angles_x, pr.angles_y, pr.angles_z]); // (a model's pitch is up)
    vector fw = v_forward;
    vector aim = at - pr.origin;
    vr_agmz_sv = VR_DegreesBetween(fw, aim);
    vector turn = weapondrawnpose(pr, 0, 21);
    float ang = vlen(turn);
    vr_agmz_turn = ang;
    if(ang > 0.01)
    {
        fw = VR_AG_Rotate(fw, turn * (1 / ang), ang);
    }
    return VR_DegreesBetween(fw, aim);
};

float vr_agmz_frames; // frames its guns have been drawn (the drawn muzzles: VR_AG_DrawnMuzzle)
float vr_agmz_keep_sd; // vr_agrunt_shotdisarm as it was (0 meanwhile: one gun's beam through the other knocked it out)

void() VR_AGMZ_Frame =
{
    entity pl = vr_agt_player;
    pl.flags = pl.flags | FL_GODMODE;
    float c = vr_agt_index;
    entity g = vr_agt_grunt;
    float wid = VR_AGMZ_Case(c);
    string kn = vr_agmz_kind == VR_AM_DKNIGHT ? "death knight" : vr_agmz_kind == VR_AM_KNIGHT ? "knight"
        : vr_agmz_kind == VR_AM_ENFORCER ? "enforcer" : "grunt";
    string name = sprintf("%s, two %s%s", kn, VR_AGT_Name(wid), vr_agmz_native ? " (its own)" : "");
    if(c >= VR_AGMZ_N)
    {
        VR_AGT_ClearWeapons();
        cvar_set("vr_agrunt_test_kind", "0");
        pl.flags = pl.flags - (pl.flags & FL_GODMODE);
        cvar_set("vr_agrunt_shotdisarm", ftos(vr_agmz_keep_sd));
        localcmd("vr_agrunt_grip_fit\n"); // (every kind's hands drawn: the knights' grip fits, vr_aggrip.cpp gripFits)
        cvar_set("vr_agrunt_test", "0");
        return;
    }
    if(vr_agt_step == 0)
    {
        if(c == 0)
        {
            vr_agmz_keep_sd = cvar("vr_agrunt_shotdisarm");
            cvar_set("vr_agrunt_shotdisarm", "0");
        }
        cvar_set("vr_agrunt_test_kind", ftos(vr_agmz_kind));
        VR_AGT_ClearWeapons();
        vr_agt_grunt = VR_AGT_SpawnPair(wid, wid, 300);
        g = vr_agt_grunt;
        if(!g.vr_ag_prop || !g.vr_ag_prop2)
        {
            VR_AGT_Print(sprintf("muzzle %s: SKIP (not held in both hands)", name));
            VR_AGT_RemovePair(g);
            vr_agt_index = c + 1;
            return;
        }
        g.takedamage = DAMAGE_NO;
        g.enemy = pl;
        vr_agmz_frames = 0;
        vr_agt_step = 1;
    }
    g.nextthink = time + 1; // (its AI held: it fires as told)
    vr_agmz_frames = vr_agmz_frames + 1;
    if(vr_agmz_frames < 10)
    {
        return; // (its arms drawn a few frames: the guns' drawn places known, weapondrawnpose 21 and 22)
    }
    entity right = g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop2 : g.vr_ag_prop;
    entity left = g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop : g.vr_ag_prop2;
    vector at = VR_AI_Origin(pl);
    if(vr_agt_step == 2)
    {
        // Each barrel as drawn against the server's (its pose's) and its aim at you: at rest, then in the pose its shots
        // left once its arms have settled there (the hands' goals eased after a jump, vr_agarms_jump_ease: two frames
        // alike; a safety net of 2 s) (a hand's grip turned wrong for its kind: that gun drawn pointing elsewhere, its
        // wrist past vr_agarms_wrist_most)
        float bR = VR_AGMZ_Barrel(right, at);
        float sR = vr_agmz_sv;
        float aR = vr_agmz_turn;
        float bL = VR_AGMZ_Barrel(left, at);
        float sL = vr_agmz_sv;
        float aL = vr_agmz_turn;
        float moved = max(fabs(aR - vr_agmz_lastR), fabs(aL - vr_agmz_lastL));
        vr_agmz_lastR = aR;
        vr_agmz_lastL = aL;
        if(vr_agmz_frames < vr_agmz_shot + 3 || (moved > 0.05 && vr_agmz_frames < vr_agmz_shot + 144))
        {
            return;
        }
        VR_AGT_Print(sprintf("barrel %s: %g frames after its shots, off you: right %.1f deg (its pose %.1f), left %.1f "
            "(%.1f)", name, vr_agmz_frames - vr_agmz_shot, bR, sR, bL, sL));
        VR_AGT_Check(sprintf("barrel %s: drawn off its pose: at rest right %.1f deg, left %.1f; aimed right %.1f, left "
            "%.1f:", name, vr_agmz_restR, vr_agmz_restL, aR, aL),
            max(aR, vr_agmz_restR) <= VR_AGMZ_BARREL && max(aL, vr_agmz_restL) <= VR_AGMZ_BARREL);
        entity e = nextent(world);
        while(e)
        {
            entity nx = nextent(e);
            if(e.owner == g && e != g.vr_ag_prop && e != g.vr_ag_prop2 && vlen(e.velocity) > 0)
            {
                remove(e); // (its shots still flying: a grenade bursting by the next one)
            }
            e = nx;
        }
        VR_AGT_RemovePair(g);
        vr_agt_grunt = world;
        vr_agt_index = c + 1;
        vr_agt_step = 0;
        return;
    }
    {
        if(vr_agmz_native)
        {
            right.vr_ag_native = 1;
            left.vr_ag_native = 1;
        }
        VR_AGMZ_Barrel(right, at);
        vr_agmz_restR = vr_agmz_turn;
        VR_AGMZ_Barrel(left, at);
        vr_agmz_restL = vr_agmz_turn;
        float n = 0;
        float bad = 0;
        float offR = 0;
        float othR = 999;
        float offL = 0;
        float othL = 999;
        float k;
        for(k = 0; k < 6; k++)
        {
            entity pr = (k & 1) ? left : right;
            entity oth = (k & 1) ? right : left;
            vector o = VR_AGMZ_Fire(g, pr);
            if(o == '0 0 -99999')
            {
                continue;
            }
            vector dir = normalize(VR_AI_Origin(pl) - pr.origin);
            vector mo = VR_AG_DrawnMuzzle(oth, dir); // (each where it is: this one as it fired, the other as it was)
            vector mp = VR_AG_DrawnMuzzle(pr, dir);
            float dOwn = vlen(o - mp);
            float dOther = vlen(o - mo);
            n = n + 1;
            if(dOwn > 14 || dOwn >= dOther) // (a shot from behind what its muzzle is in: VR_ShotPlace, along its own gun)
            {
                bad = bad + 1;
            }
            if(k & 1)
            {
                offL = max(offL, dOwn);
                othL = min(othL, dOther);
            }
            else
            {
                offR = max(offR, dOwn);
                othR = min(othR, dOther);
            }
        }
        VR_AGT_Check(sprintf("muzzle %s: %g shots, right off %.1f / other %.1f, left off %.1f / other %.1f:", name, n,
            offR, othR, offL, othL), n >= 4 && bad == 0);
        vr_agmz_shot = vr_agmz_frames;
        vr_agmz_lastR = -99;
        vr_agmz_lastL = -99;
        vr_agt_step = 2;
    }
};
#endif
