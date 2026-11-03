#ifdef QVR_TESTS
// vr_wornpack_test.qc -- worn backpacks' test (vr_wornpack.qc; Debug > Tests; ROUND23.md, "Worn backpacks").
// `vr_wornpack_test <n>` (any map with room ahead: e1m1's start; god, notarget), the tens the kind: 0 grunts, 10 knights, 20
// death knights, 30 enforcers (33 only).
// 1: its kind with a gun ahead, all wearing packs; with a sword or its fists, none (nor any loot: an armed enforcer, who
// keeps its own, the loot's control). Checks: the pack on the back, untakeable while it lives; a hit on its chest leaves it,
// damage on the pack counts, a real shot and a melee blow on it take it off (the rip, its reserve gone with it); a mild
// knockdown keeps it (the pack where it was as the ragdoll is made; the ragdoll's box), a mild landing too, a hard one tears
// it off; a wearer killed drops no other backpack, its pack takeable then, and taken it comes off. Ends on
// `wornpack: <p> passed, <f> failed`.
// 2: the eyeshot's: wearers back on, side on and knocked down. 3: lifted by the pack (below). 4: a hand through the body.
// 5: the enforcer's lift zone: on a gun's, a sword's and fists'; nothing takes it off, nothing drawn, its loot its own.
// 6: a replacement's rule: a loadout rolled unseen gives or takes the pack.

float vr_wpt_state;
float vr_wpt_until;
float vr_wpt_frames;
float vr_wpt_old_chance, vr_wpt_old_ref, vr_wpt_old_debug;
entity vr_wpt_a, vr_wpt_b, vr_wpt_c, vr_wpt_d, vr_wpt_e, vr_wpt_f, vr_wpt_g;
vector vr_wpt_pull;     // the tug's way (level, towards the player)
float vr_wpt_gapmax;    // the drawn palm's farthest off the pack's box while pulled (cm)
float vr_wpt_old_pool, vr_wpt_old_roll;

// The fit's slider of the kind under test, now (its handle for each kind).
float(float grunt, float knight, float dknight) VR_WPT_FitNow =
{
    return cvar_hget(vr_wpt_kind == VR_AM_KNIGHT ? knight : (vr_wpt_kind == VR_AM_DKNIGHT ? dknight : grunt));
};

// The fit's sliders of the kind under test: this one's name.
string(string which) VR_WPT_FitCvar =
{
    return strcat(vr_wpt_kind == VR_AM_KNIGHT ? "vr_worn_backpack_knight_" : (vr_wpt_kind == VR_AM_DKNIGHT ? "vr_worn_backpack_dknight_" :
        "vr_worn_backpack_grunt_"), which);
};
vector vr_wpt_b0;       // B's pack before its knockdown
float vr_wpt_loose0;    // loose backpacks before the deaths
float vr_wpt_rips0;
vector vr_wpt_hand;     // the mock hand's target

float vr_wpt_one;       // the gentle pull's most (N)
float vr_wpt_kind;      // the kind under test (VR_AM_*)
entity vr_wpt_s, vr_wpt_u; // its kind with a sword, with its fists: no pack
entity vr_wpt_sm;       // a wearer for the melee blow

// An armed monster of `kind` (VR_AM_*) holding `wid` ahead of the player, `side` to the right, `ahead` out, facing the
// player (or away).
entity(entity p, float kind, float wid, float side, float ahead, float away) VR_WPT_SpawnKind =
{
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    entity oldself = self;
    self = spawn();
    setorigin(self, p.origin + fw * ahead + v_right * side);
    self.angles_y = vectoyaw(p.origin - self.origin) + (away ? 180 : 0);
    self.weapon = wid;
    VR_AM_Spawn(kind);
    entity g = self;
    self = oldself;
    g.vr_ar_type = 0;
    g.vr_ar_mask = 0;
    g.health = 1000;
    return g;
};

// One of the kind under test (with a ranged weapon it wears a pack).
entity(entity p, float wid, float side, float ahead, float away) VR_WPT_Spawn =
{
    return VR_WPT_SpawnKind(p, vr_wpt_kind, wid, side, ahead, away);
};

// Backpacks lying about (not worn ones, not torn off ones).
float() VR_WPT_Loose =
{
    float n = 0;
    entity e = nextent(world);
    while(e)
    {
        n = n + (e.model == "progs/backpack.mdl" && e.classname != "vr_wornpack");
        e = nextent(e);
    }
    return n;
};

// A hit of `d` at `at` from the player.
void(entity g, entity p, vector at, float d) VR_WPT_Hit =
{
    VR_Hit_SetZone(g, 0, at);
    T_Damage(g, p, p, d);
};

void() VR_WPT_End =
{
    cvar_set("vr_worn_backpack_pool", ftos(vr_wpt_old_pool));
    cvar_set("vr_worn_backpack_hit_roll", ftos(vr_wpt_old_roll));
    localcmd("-graboff\n");
    cvar_set("vr_worn_backpack_hit_roll", ftos(vr_wpt_old_chance));
    cvar_set("vr_worn_backpack_hit_ref", ftos(vr_wpt_old_ref));
    cvar_set("vr_worn_backpack_debug", ftos(vr_wpt_old_debug));
    cvar_set("vr_mock_hand_to_absolute", "0");
    localcmd("-grabmain\n");
    vr_wpt_state = 0;
    VR_Test_Verdict();
};

void(entity g, entity p, float speed) VR_WPT_KnockDown =
{
    if(!canragdoll(g) || !VR_Knockdown_MakeRoom(g))
    {
        VR_Test_Check(FALSE, sprintf("%s can be knocked down", g.netname));
        return;
    }
    vector away = g.origin - p.origin;
    away_z = 0;
    g.velocity = normalize(away) * speed + '0 0 40';
    VR_Knockdown_Start(g);
};

void(entity p) VR_WPT_Begin =
{
    VR_Test_Begin("wornpack");
    vr_wpt_old_chance = cvar_hget(cvarh_vr_worn_backpack_hit_roll);
    vr_wpt_old_pool = cvar_hget(cvarh_vr_worn_backpack_pool);
    vr_wpt_old_roll = vr_wpt_old_chance;
    vr_wpt_old_ref = cvar_hget(cvarh_vr_worn_backpack_hit_ref);
    vr_wpt_old_debug = cvar_hget(cvarh_vr_worn_backpack_debug);
    cvar_set("vr_worn_backpack_debug", ftos(max(1, vr_wpt_old_debug)));
    vr_wpt_e = VR_WPT_SpawnKind(p, VR_AM_ENFORCER, WID_SHOTGUN, 60, 120, FALSE); // (the loot's control: an enforcer drops its own)
    cvar_set("vr_worn_backpack_pool", "30");
    cvar_set("vr_worn_backpack_hit_roll", "0");
    vr_wpt_a = VR_WPT_Spawn(p, WID_SHOTGUN, -60, 120, TRUE);
    vr_wpt_b = VR_WPT_Spawn(p, WID_SHOTGUN, 35, 70, FALSE);
    vr_wpt_c = VR_WPT_Spawn(p, WID_SHOTGUN, 25, 150, FALSE);
    vr_wpt_d = VR_WPT_Spawn(p, WID_SHOTGUN, 0, 200, FALSE);
    vr_wpt_f = VR_WPT_Spawn(p, WID_SHOTGUN, 90, 200, FALSE);
    vr_wpt_g = VR_WPT_Spawn(p, WID_NAILGUN, -50, 40, TRUE);
    vr_wpt_sm = VR_WPT_Spawn(p, WID_NAILGUN, -85, 70, TRUE);
    vr_wpt_s = VR_WPT_Spawn(p, WID_SWORD, 100, 100, FALSE);
    vr_wpt_u = VR_WPT_Spawn(p, -1, -30, 180, FALSE);
    vr_wpt_f.netname = "F";
    vr_wpt_a.netname = "A";
    vr_wpt_b.netname = "B";
    vr_wpt_c.netname = "C";
    vr_wpt_d.netname = "D";
    vr_wpt_e.netname = "E";
    vr_wpt_g.netname = "G";
    vr_wpt_sm.netname = "M";
    vr_wpt_s.netname = "S";
    vr_wpt_u.netname = "U";
    vr_wpt_state = 2;
};

// A real shot from behind `g` at its pack's middle: its hit there (the trace's, then the damage as a shot's).
void(entity g, entity p, float d) VR_WPT_Shot =
{
    makevectors([0, g.angles_y, 0]);
    vector mid = limbpiece(g, 0, 22);
    traceline(mid - v_forward * 24, mid + v_forward * 20, MOVE_HITMODEL, p);
    VR_Test_Check(trace_ent == g && ragdollbone(g, trace_endpos, QVR_WORNPACK_AT) <= VR_WP_SHOT_REACH,
        sprintf("a shot from behind meets %s's pack (met %s, %.2f units off its box)", g.netname, trace_ent.classname,
        ragdollbone(g, trace_endpos, QVR_WORNPACK_AT)));
    VR_Hit_SetZone(g, 0, trace_endpos);
    VR_Hit_BeginAs(QVR_HITKIND_SHOT, FALSE);
    T_Damage(g, p, p, d);
    VR_Hit_End();
};

// State 2 (its first frame placed): the standing checks, A's hits, B and C knocked down.
void(entity p) VR_WPT_Standing =
{
    entity a = vr_wpt_a;
    entity pk = a.vr_wp_pack;
    VR_Test_Check(pk && a.vr_wp_worn == 1 && pk.model == "progs/backpack.mdl" && pk.vr_wp_state == VR_WP_WORN &&
        a.vr_wp_had && !vr_wpt_s.vr_wp_pack && !vr_wpt_u.vr_wp_pack && !vr_wpt_s.vr_wp_worn && !vr_wpt_u.vr_wp_worn &&
        !vr_wpt_s.vr_wp_had && !vr_wpt_u.vr_wp_had, sprintf("A (a gun) wears a pack (worn %g, state %g); S (a sword) and U (its fists) none", a.vr_wp_worn, pk.vr_wp_state));
    vector mid = limbpiece(a, 0, 22);
    makevectors([0, a.angles_y, 0]);
    float back = (mid - a.origin) * v_forward;
    float up = mid_z - a.origin_z;
    // (Its entity on the pack's place: limbpiece's is the .mdl frame pose's, the pack rides the chest bone as drawn since
    // drawnpose::bone, a fraction of a unit off it: within test 99's 3 units.)
    // (Its rear face: where its middle lies depends on the fit (turned 180 it sits further in) and on the kind (a knight's middle is
    // further forward of its origin): the pack is on its back if its rear stands out behind its middle.)
    vector packExt = limbpiece(a, 0, 23);
    float rear = back - fabs(v_forward_x) * packExt_x - fabs(v_forward_y) * packExt_y;
    VR_Test_Check(rear < -4 && rear > -24 && up > 4 && up < 30 && vlen(pk.origin - limbpiece(a, 0, 20)) < 3,
        sprintf("on its back: its rear %.1f units behind its middle (its own middle %.1f), %.1f up, its entity %.2f units off the pack's frame place", rear, back, up,
        vlen(pk.origin - limbpiece(a, 0, 20))));
    VR_Test_Check(pk.solid == SOLID_NOT && !(pk.flags & FL_FORCEGRABBABLE) && !(pk.flags & FL_ITEM) &&
        pk.handtouch != VR_Carry_Handtouch, "not takeable from the living (not touchable, not force-grabbable)");
    // Its kind's Up moves it now; another kind's doesn't.
    float up0 = mid_z;
    string upName = VR_WPT_FitCvar("z");
    string otherName = vr_wpt_kind == VR_AM_KNIGHT ? "vr_worn_backpack_grunt_z" : "vr_worn_backpack_knight_z";
    float otherWas = vr_wpt_kind == VR_AM_KNIGHT ? cvar_hget(cvarh_vr_worn_backpack_grunt_z) : cvar_hget(cvarh_vr_worn_backpack_knight_z);
    float upWas = VR_WPT_FitNow(cvarh_vr_worn_backpack_grunt_z, cvarh_vr_worn_backpack_knight_z, cvarh_vr_worn_backpack_dknight_z);
    cvar_set(otherName, ftos(otherWas + 5));
    vector atOther = limbpiece(a, 0, 22);
    float upOther = atOther_z;
    cvar_set(otherName, ftos(otherWas));
    cvar_set(upName, ftos(upWas + 4));
    vector atMine = limbpiece(a, 0, 22);
    float upMine = atMine_z;
    cvar_set(upName, ftos(upWas));
    VR_Test_Check(fabs(upOther - up0) < 0.01 && upMine - up0 > 3 && upMine - up0 < 5, sprintf("its kind's Up 4 moves the pack %.2f up at once; another kind's 5 moves it %.2f",
        upMine - up0, upOther - up0));
    // A hit on its chest: the pack stays.
    float r0 = a.vr_ag_reserve;
    float s0 = a.ammo_shells;
    VR_WPT_Hit(a, p, a.origin + v_forward * 12 + '0 0 18', 10);
    VR_Test_Check(pk.vr_wp_state == VR_WP_WORN, sprintf("a hit on its chest leaves the pack on (%g units from it)",
        ragdollbone(a, a.origin + v_forward * 12 + '0 0 18', QVR_WORNPACK_AT)));
    // On its back as its fit says, turned with it (not spinning: the engine keeps a worn pack's angles), its box the model's.
    float fitYaw = VR_WPT_FitNow(cvarh_vr_worn_backpack_grunt_yaw, cvarh_vr_worn_backpack_knight_yaw, cvarh_vr_worn_backpack_dknight_yaw);
    float fitPitch = VR_WPT_FitNow(cvarh_vr_worn_backpack_grunt_pitch, cvarh_vr_worn_backpack_knight_pitch, cvarh_vr_worn_backpack_dknight_pitch);
    float dyaw = pk.angles_y - a.angles_y - fitYaw;
    dyaw = dyaw - 360 * floor((dyaw + 180) / 360);
    vector sz = pk.maxs - pk.mins;
    VR_Test_Check(fabs(dyaw) < 25 && fabs(pk.angles_x) < 30 + fabs(fitPitch) && fabs(pk.angles_z) < 25 && pk.vr_wp_state == VR_WP_WORN,
        sprintf("on its back as its fit says, turned with it: yaw %.1f off its own + %g, pitch %.1f (fit %g + its chest's lean), roll %.1f", dyaw, fitYaw,
        pk.angles_x, fitPitch, pk.angles_z));
    VR_Test_Check(sz_x < 17 && sz_y < 17 && sz_z < 20 && sz_z > 10, sprintf("its box round its model as turned: %.1f x %.1f x %.1f",
        sz_x, sz_y, sz_z));
    // Shots across it from behind: they meet it where its model's box is, and nowhere off it.
    float onIn = 0;
    float offOut = 0;
    float bad = 0;
    float y = -14;
    makevectors([0, a.angles_y, 0]);
    while(y <= 14)
    {
        vector st = mid - v_forward * 60 + v_right * y;
        vector en = mid + v_forward * 60 + v_right * y;
        float near = 99;
        float k = 0;
        while(k <= 120)
        {
            near = min(near, ragdollbone(a, st + v_forward * k, QVR_WORNPACK_AT));
            k = k + 0.5;
        }
        traceline(st, en, MOVE_HITMODEL, p);
        float onPack = trace_ent == a && ragdollbone(a, trace_endpos, QVR_WORNPACK_AT) <= VR_WP_SHOT_REACH;
        float wrong = (near <= 0) ? !onPack : ((near > 0.6) ? onPack : FALSE);
        if(wrong)
        {
            dprint(sprintf("wornpack: shot %g units across: %.2f from the box, met %s %.2f from it\n", y, near, trace_ent.classname,
                ragdollbone(a, trace_endpos, QVR_WORNPACK_AT)));
        }
        if(near <= 0)
        {
            onIn = onIn + onPack;
            bad = bad + !onPack;
        }
        else if(near > 0.6)
        {
            offOut = offOut + !onPack;
            bad = bad + onPack;
        }
        y = y + 1;
    }
    VR_Test_Check(bad == 0 && onIn >= 6, sprintf("shots across it meet the pack's box only: %g on it, %g past it, %g wrong", onIn,
        offOut, bad));
    // Damage on the pack counts to vr_worn_backpack_pool (30): a hit there 12, a blast 10, poison none, fire 10: off.
    vr_wpt_rips0 = vr_wp_rips;
    a.enemy = world;
    float dmg0 = pk.vr_wp_dmg;
    pk.vr_wp_dmg = 0;
    VR_WPT_Hit(a, p, mid, 12);
    VR_Test_Check(pk.vr_wp_state == VR_WP_WORN && pk.vr_wp_dmg >= 12 && pk.vr_wp_dmg == vr_wp_hit_counted && a.enemy == p,
        sprintf("a hit of 12 on the pack counts its damage (%g of 30; %g before), still on; it spotted you (%s)", pk.vr_wp_dmg,
        dmg0, a.enemy.classname));
    float d1 = pk.vr_wp_dmg;
    entity blast = spawn();
    setorigin(blast, a.origin + v_right * 40);
    VR_Hit_BeginAs(QVR_HITKIND_SPLASH, FALSE);
    T_Damage(a, blast, p, 8);
    VR_Hit_End();
    float d2 = pk.vr_wp_dmg;
    VR_Test_Check(pk.vr_wp_state == VR_WP_WORN && d2 > d1 && d2 < 30, sprintf("an explosion's damage counts (%g of 30)", d2));
    blast.classname = "vr_adpoison";
    T_Damage(a, blast, p, 10);
    VR_Test_Check(pk.vr_wp_state == VR_WP_WORN && pk.vr_wp_dmg == d2, sprintf("poison doesn't (%g of 30)", pk.vr_wp_dmg));
    blast.classname = "vr_adflame";
    VR_Hit_BeginAs(-1, TRUE);
    T_Damage(a, blast, p, 10);
    VR_Hit_End();
    remove(blast);
    VR_Test_Check(pk.vr_wp_state == VR_WP_OFF && vr_wp_rips == vr_wpt_rips0 + 1 && vr_wp_rip_why == "fire" &&
        vr_wp_rip_sound == VR_WP_RIP_SOUND && a.vr_wp_worn == 0 && (pk.flags & FL_FORCEGRABBABLE),
        sprintf("the flamethrower's fire takes it to the pool: torn off (%s, %g), takeable now", vr_wp_rip_why, pk.vr_wp_dmg));
    VR_Test_Check(r0 > 0 && a.vr_ag_reserve == 0 && pk.ammo_shells >= r0 + s0, sprintf("its spare rounds went with it: reserve %g -> %g, the pack's shells %g",
        r0, a.vr_ag_reserve, pk.ammo_shells));
    // A real shot on G's pack, a melee blow on M's: each past the pool, off (their spare rounds with them).
    float rg = vr_wpt_g.vr_ag_reserve;
    VR_WPT_Shot(vr_wpt_g, p, 35);
    VR_Test_Check(vr_wpt_g.vr_wp_pack.vr_wp_state == VR_WP_OFF && vr_wp_rip_why == "hit" && vr_wp_rip_sound == VR_WP_RIP_SOUND &&
        vr_wpt_g.vr_ag_reserve == 0 && vr_wpt_g.vr_wp_pack.ammo_nails >= rg, sprintf("a shot of 35 on it takes G's pack off (%s; its reserve %g in it: nails %g)",
        vr_wp_rip_why, rg, vr_wpt_g.vr_wp_pack.ammo_nails));
    entity sm = vr_wpt_sm;
    makevectors([0, sm.angles_y, 0]);
    vector smid = limbpiece(sm, 0, 22);
    VR_Hit_SetZone(sm, 0, smid);
    VR_Hit_BeginAs(QVR_HITKIND_MELEE, TRUE);
    T_Damage(sm, p, p, 35);
    VR_Hit_End();
    VR_Test_Check(sm.vr_wp_pack.vr_wp_state == VR_WP_OFF && vr_wp_rip_why == "hit", sprintf("a melee blow of 35 on it takes M's pack off (%s)", vr_wp_rip_why));
    vr_wpt_b0 = limbpiece(vr_wpt_b, 0, 22);
    VR_WPT_KnockDown(vr_wpt_b, p, 60);
    VR_WPT_KnockDown(vr_wpt_c, p, 60);
    vr_wpt_frames = 0;
    vr_wpt_state = 3;
};

// State 3: B and C ragdolls; the pack where it was as the ragdoll is made, kept through the knockdown and a mild landing;
// C's hard landing tears its off.
void(entity p) VR_WPT_Down =
{
    entity b = vr_wpt_b;
    entity c = vr_wpt_c;
    vr_wpt_frames = vr_wpt_frames + 1;
    if(vr_wpt_frames == 1)
    {
        VR_Test_Check(isragdoll(b) && ragdollbone(b, '0 0 0', QVR_WORNPACK) == 1,
            sprintf("B a ragdoll (%g) with the pack's box on its chest (%g)", isragdoll(b), ragdollbone(b, '0 0 0', QVR_WORNPACK)));
        float moved = vlen(limbpiece(b, 0, 22) - vr_wpt_b0);
        VR_Test_Check(moved < 6, sprintf("the ragdoll's pack where the standing one's was (%.1f units)", moved));
        return;
    }
    if(vr_wpt_frames < 36)
    {
        return; // (half a second down)
    }
    VR_Test_Check(b.vr_wp_pack.vr_wp_state == VR_WP_WORN && ragdollbone(b, '0 0 0', QVR_WORNPACK) == 1,
        "kept through a mild knockdown");
    VR_RagImpact_Hit(c, limbpiece(c, 0, 22), 4, 20, 4, 0, '0 0 -1');
    VR_Test_Check(c.vr_wp_pack.vr_wp_state == VR_WP_WORN, "kept through a mild landing (4 m/s)");
    VR_RagImpact_Hit(c, limbpiece(c, 0, 22), 12, 20, 12, 0, '0 0 -1');
    VR_Test_Check(c.vr_wp_pack.vr_wp_state == VR_WP_OFF && vr_wp_rip_why == "landing" && ragdollbone(c, '0 0 0', QVR_WORNPACK) == 0,
        sprintf("torn off by a hard landing (12 m/s: %s), its box off the ragdoll", vr_wp_rip_why));
    vr_wpt_loose0 = VR_WPT_Loose();
    // D (packed), S and U (none) and E (the control) killed.
    vr_wp_deathloots = 0;
    vr_wpt_d.health = 1;
    vr_wpt_e.health = 1;
    vr_wpt_s.health = 1;
    vr_wpt_u.health = 1;
    T_Damage(vr_wpt_d, p, p, 20);
    T_Damage(vr_wpt_e, p, p, 20);
    T_Damage(vr_wpt_s, p, p, 20);
    T_Damage(vr_wpt_u, p, p, 20);
    vr_wpt_until = time + 5;
    vr_wpt_state = 7;
};

// State 7: their loot counted; no second backpack from D, one from E; D's pack takeable, and taken it comes off.
void(entity p) VR_WPT_Deaths =
{
    entity d = vr_wpt_d;
    entity pk = d.vr_wp_pack;
    float loose = VR_WPT_Loose();
    if((vr_wp_deathloots < 1 || loose < vr_wpt_loose0 + 1 || vr_wpt_s.solid != SOLID_NOT || vr_wpt_u.solid != SOLID_NOT) && time < vr_wpt_until)
    {
        return;
    }
    VR_Test_Check(vr_wp_deathloots == 1 && loose == vr_wpt_loose0 + 1, sprintf("killed: the wearer drops no other backpack, sword's and fists' none, the control one (loose %g -> %g)",
        vr_wpt_loose0, loose));
    VR_Test_Check(pk.vr_wp_state == VR_WP_DEAD && (pk.flags & FL_FORCEGRABBABLE) && pk.handtouch == VR_Carry_Handtouch &&
        pk.ammo_shells > 0, sprintf("its pack still on, takeable now, its loot in it (shells %g)", pk.ammo_shells));
    VR_Carry_Start(p, cVR_OffHand, pk);
    VR_Test_Check(pk.vr_wp_state == VR_WP_OFF && vr_wp_rip_why == "taken" && d.vr_wp_worn == 0 && p.offhand_held == pk,
        sprintf("taken: off its back into the hand (%s)", vr_wp_rip_why));
    // F gibbed: its pack comes off (VR_WP_Think, the frame after).
    vr_wpt_f.health = 1;
    T_Damage(vr_wpt_f, p, p, 200);
    localcmd("-graboff\n");
    vr_wpt_frames = 0;
    vr_wpt_state = 8;
};

// State 8: F's pack off (gibbed).
void(entity p) VR_WPT_Gibbed =
{
    VR_Test_Check(vr_wpt_f.vr_wp_pack.vr_wp_state == VR_WP_OFF && vr_wpt_f.vr_wp_worn == 0,
        sprintf("gibbed: its pack comes off (%s)", vr_wpt_f.vr_wp_pack.vr_wp_state == VR_WP_OFF ? vr_wp_rip_why : "on"));
    VR_WPT_End();
};

// The hand(s) onto `g`'s pack middle (both: `two`, each 2 units to a side), the grip(s) pressed.
void(entity p, entity g, float two) VR_WPT_HandsOnPack =
{
    makevectors([0, g.angles_y, 0]);
    vr_wpt_hand = limbpiece(g, 0, 22) - v_forward * 3; // (by its outer face: the fist closing on the pack from behind)
    vector side = two ? v_right * 2 : '0 0 0';
    localcmd(sprintf("vr_mock_hand_to main %g %g %g\n", vr_wpt_hand_x + side_x, vr_wpt_hand_y + side_y, vr_wpt_hand_z));
    if(two)
    {
        localcmd(sprintf("vr_mock_hand_to off %g %g %g\n", vr_wpt_hand_x - side_x, vr_wpt_hand_y - side_y, vr_wpt_hand_z));
    }
};

// The hands pulled back (towards the player, level) `step` units.
void(float step, float two) VR_WPT_HandsBack =
{
    vr_wpt_hand = vr_wpt_hand + vr_wpt_pull * step;
    makevectors([0, vectoyaw(vr_wpt_pull), 0]);
    vector side = two ? v_right * 2 : '0 0 0';
    localcmd(sprintf("vr_mock_hand_to main %g %g %g\n", vr_wpt_hand_x + side_x, vr_wpt_hand_y + side_y, vr_wpt_hand_z));
    if(two)
    {
        localcmd(sprintf("vr_mock_hand_to off %g %g %g\n", vr_wpt_hand_x - side_x, vr_wpt_hand_y - side_y, vr_wpt_hand_z));
    }
};

// ---- `vr_wornpack_test 3` (13, 23, 33: its kind): lifted by the pack. Ends on `wornpull: <p> passed, <f> failed`. G standing,
// held by its pack: a gentle pull neither lifts nor tears; a strong one lifts it (a ragdoll, the pack still on its chest
// part, the hand holding it by the pack); carried sideways and up, then thrown: it goes with the hand, the pack on its chest
// throughout (its middle within 3 units of where it was from the chest bone). B knocked down, held by its pack: a slow drag
// drags it (kept, not lifted), a yank lifts it, never tears it. Then A's pack hit (a shot's or a blow's damage) and D's
// (dead) force grab still tear theirs off (an enforcer's: nothing takes it off, and a dead one's zone is gone).
float vr_wpt_d0;     // the lifted one's pack's middle from its chest bone as it was lifted (units)
float vr_wpt_lifts0; // lifts before a step
float vr_wpt_off;    // frames the lifted one was not held by its pack

void() VR_WPL_End =
{
    localcmd("-grabmain\n");
    VR_WPT_End();
};

void(entity p) VR_WPL_Begin =
{
    VR_Test_Begin("wornpull");
    vr_wpt_old_chance = cvar_hget(cvarh_vr_worn_backpack_hit_roll);
    vr_wpt_old_pool = cvar_hget(cvarh_vr_worn_backpack_pool);
    vr_wpt_old_roll = vr_wpt_old_chance;
    vr_wpt_old_ref = cvar_hget(cvarh_vr_worn_backpack_hit_ref);
    vr_wpt_old_debug = cvar_hget(cvarh_vr_worn_backpack_debug);
    cvar_set("vr_worn_backpack_debug", ftos(max(1, vr_wpt_old_debug)));
    cvar_set("vr_worn_backpack_pool", "30");
    cvar_set("vr_worn_backpack_hit_roll", "0");
    cvar_set("vr_mock_hand_to_absolute", "1");
    vr_wpt_rips0 = vr_wp_rips;
    vr_wpt_frames = 0;
    vr_wpt_state = 20;
};

// The lifted one's pack on its chest: how far its middle is now from where it was from the chest bone (the most so far
// in vr_wpt_gapmax), and whether the hand holds it by the pack (else counted in vr_wpt_off).
void(entity g, entity p) VR_WPL_Track =
{
    float d = ragdollbone(g, '9 0 0', QVR_WORNPACK);
    vr_wpt_gapmax = max(vr_wpt_gapmax, d < 0 ? 99 : fabs(d - vr_wpt_d0));
    if(p.mainhand_held == g && ragdollbone(g, [2, 1, cVR_MainHand], QVR_WORNPACK) < 0)
    {
        vr_wpt_off = vr_wpt_off + 1;
    }
};

// States 20-24: G standing, held by its pack: a gentle pull (21), a strong one (22: lifted), carried (23), thrown (24).
void(entity p) VR_WPL_Standing =
{
    vr_wpt_frames = vr_wpt_frames + 1;
    entity g = vr_wpt_g;
    entity pk = g.vr_wp_pack;
    float stiff = cvar_hget(cvarh_vr_worn_backpack_lift_stiffness);
    if(vr_wpt_state == 20)
    {
        if(vr_wpt_frames == 1)
        {
            g = VR_WPT_Spawn(p, WID_SHOTGUN, 0, 44, TRUE);
            g.netname = "G";
            g.enemy = world;
            vr_wpt_g = g;
            return;
        }
        VR_WPT_HandsOnPack(p, g, FALSE);
        if(vr_wpt_frames < 4)
        {
            return;
        }
        if(vr_wpt_frames == 4)
        {
            localcmd("+grabmain\n");
            return;
        }
        float s0 = ragdollbone(g, [5, 1, cVR_MainHand], QVR_WORNPACK);
        if(s0 < 0 && vr_wpt_frames < 30)
        {
            return;
        }
        VR_Test_Check(s0 >= 0 && pk.vr_wp_state == VR_WP_WORN && !isragdoll(g), sprintf("G's pack held by the hand while it stands (stretch %.1f units)", s0));
        if(s0 < 0)
        {
            VR_WPL_End();
            return;
        }
        vr_wpt_pull = p.origin - g.origin;
        vr_wpt_pull_z = 0;
        vr_wpt_pull = normalize(vr_wpt_pull);
        vr_wpt_one = 0;
        vr_wpt_lifts0 = vr_wp_lifts;
        vr_wpt_frames = 0;
        vr_wpt_state = 21;
        return;
    }
    float s = ragdollbone(g, [5, 1, cVR_MainHand], QVR_WORNPACK);
    if(vr_wpt_state == 21)
    {
        if(vr_wpt_frames <= 18)
        {
            VR_WPT_HandsBack(4 / 18, FALSE); // (4 units back, then held there)
        }
        vr_wpt_one = max(vr_wpt_one, s * stiff);
        if(vr_wpt_frames < 72)
        {
            return;
        }
        VR_Test_Check(!isragdoll(g) && pk.vr_wp_state == VR_WP_WORN && vr_wp_lifts == vr_wpt_lifts0 && vr_wp_rips == vr_wpt_rips0 && s >= 0,
            sprintf("a gentle pull (4 units back, at most %.0f N of %g) neither lifts nor tears; still held", vr_wpt_one,
            cvar_hget(cvarh_vr_worn_backpack_lift_force)));
        vr_wpt_frames = 0;
        vr_wpt_state = 22;
        return;
    }
    if(vr_wpt_state == 22)
    {
        if(vr_wp_lifts == vr_wpt_lifts0 && vr_wpt_frames < 108)
        {
            VR_WPT_HandsBack(1.5 * VRMetersToUnits() / 72, FALSE); // (1.5 m/s)
            return;
        }
        VR_Test_Check(vr_wp_lifts == vr_wpt_lifts0 + 1 && isragdoll(g) && g.vr_knockdown && g.health > 0,
            sprintf("a strong pull (1.5 m/s) lifts it after %.2f s at %.0f N (a ragdoll %g, knocked down %g)", vr_wpt_frames / 72,
            vr_wp_lift_pull, isragdoll(g), g.vr_knockdown));
        float f = ragdollbone(g, [2, 1, cVR_MainHand], QVR_WORNPACK);
        VR_Test_Check(pk.vr_wp_state == VR_WP_WORN && ragdollbone(g, '0 0 0', QVR_WORNPACK) == 1 && p.mainhand_held == g && f >= 0 &&
            vr_wp_rips == vr_wpt_rips0, sprintf("lifted: the pack still on its ragdoll's chest, the hand holding it by the pack (%.0f N; holds %s)", f,
            p.mainhand_held.classname));
        if(!isragdoll(g) || p.mainhand_held != g)
        {
            VR_WPL_End();
            return;
        }
        vr_wpt_d0 = ragdollbone(g, '9 0 0', QVR_WORNPACK);
        vr_wpt_gapmax = 0;
        vr_wpt_off = 0;
        vr_wpt_b0 = limbpiece(g, 0, 22);
        vr_wpt_frames = 0;
        vr_wpt_state = 23;
        return;
    }
    makevectors([0, vectoyaw(vr_wpt_pull), 0]);
    VR_WPL_Track(g, p);
    if(vr_wpt_state == 23)
    {
        if(vr_wpt_frames <= 72)
        {
            vr_wpt_hand = vr_wpt_hand + v_right * (30 / 72) + '0 0 1' * (24 / 72); // (30 units aside and 24 up in a second: a death knight's feet off the floor)
            localcmd(sprintf("vr_mock_hand_to main %g %g %g\n", vr_wpt_hand_x, vr_wpt_hand_y, vr_wpt_hand_z));
            return;
        }
        vector went = limbpiece(g, 0, 22) - vr_wpt_b0;
        VR_Test_Check(went * v_right >= 15 && p.mainhand_held == g && vr_wpt_off == 0, sprintf("carried: it went %.1f of the hand's 30 units aside, %.1f of 24 up, held by the pack throughout (%g frames not)",
            went * v_right, went_z, vr_wpt_off));
        vr_wpt_b0 = limbpiece(g, 0, 22);
        vr_wpt_frames = 0;
        vr_wpt_state = 24;
        return;
    }
    // 24: swung away from you at 4 m/s for 10 frames and let go.
    if(vr_wpt_frames <= 10)
    {
        vr_wpt_hand = vr_wpt_hand - vr_wpt_pull * (4 * VRMetersToUnits() / 72) + '0 0 0.5';
        localcmd(sprintf("vr_mock_hand_to main %g %g %g\n", vr_wpt_hand_x, vr_wpt_hand_y, vr_wpt_hand_z));
        if(vr_wpt_frames == 10)
        {
            localcmd("-grabmain\n");
        }
        return;
    }
    if(vr_wpt_frames < 46)
    {
        return;
    }
    vector flew = limbpiece(g, 0, 22) - vr_wpt_b0;
    // (A throw: it goes with the swing and is let go of. At least half the hand's own swing: on the fit of 2026-11-03, five runs each, grunts 33.7-45.7
    // units, knights 14.6-27.6, death knights 36.9-44, enforcers 40.9-47; the swing about 22. The old 20 came from the old fit.)
    float swing = 10 * 4 * VRMetersToUnits() / 72;
    VR_Test_Check(flew * (vr_wpt_pull * -1) >= swing * 0.5 && p.mainhand_held != g, sprintf("thrown: it went %.1f units the way of the throw (the hand's swing %.1f), let go of (holds %s)",
        flew * (vr_wpt_pull * -1), swing, p.mainhand_held.classname));
    VR_Test_Check(vr_wpt_gapmax <= 3 && pk.vr_wp_state == VR_WP_WORN && vr_wp_rips == vr_wpt_rips0,
        sprintf("the pack on its chest throughout: at most %.2f units off its place from the chest bone (%.1f), never torn (%g rips)", vr_wpt_gapmax,
        vr_wpt_d0, vr_wp_rips - vr_wpt_rips0));
    vr_wpt_frames = 0;
    vr_wpt_state = 25;
};

// States 25-27: B knocked down, the player beside it on its floor, the hand on its pack (25); dragged slowly (26); yanked (27).
void(entity p) VR_WPL_Down =
{
    vr_wpt_frames = vr_wpt_frames + 1;
    entity b = vr_wpt_b;
    entity pk = b.vr_wp_pack;
    if(vr_wpt_state == 25)
    {
        if(vr_wpt_frames == 1)
        {
            b = VR_WPT_Spawn(p, WID_SHOTGUN, 60, 90, FALSE); // (not where G was thrown)
            b.netname = "B";
            vr_wpt_b = b;
            return;
        }
        if(vr_wpt_frames == 2)
        {
            VR_WPT_KnockDown(b, p, 40);
            return;
        }
        if(vr_wpt_frames == 38)
        {
            vr_wpt_hand = limbpiece(b, 0, 22);
            vector out = p.origin - vr_wpt_hand;
            out_z = 0;
            vector at = vr_wpt_hand + normalize(out) * 20;
            traceline(b.origin, b.origin - '0 0 256', TRUE, b);
            at_z = trace_endpos_z + 25; // (on its floor, as vr_wornpack_test 1's)
            setorigin(p, at);
            p.velocity = '0 0 0';
            vr_wpt_until = 0;
        }
        if(vr_wpt_frames < 38)
        {
            return;
        }
        float gap = ragdollbone(b, p.handpos, QVR_WORNPACK_AT);
        if(!vr_wpt_until)
        {
            vr_wpt_hand = limbpiece(b, 0, 22);
            localcmd(sprintf("vr_mock_hand_to main %g %g %g\n", vr_wpt_hand_x, vr_wpt_hand_y, vr_wpt_hand_z));
            if(gap < 0 || gap > 2)
            {
                if(vr_wpt_frames < 128)
                {
                    return;
                }
                VR_Test_Check(FALSE, sprintf("the hand gets onto B's pack (%.1f units off; B a ragdoll %g)", gap, isragdoll(b)));
                VR_WPL_End();
                return;
            }
            localcmd("+grabmain\n");
            vr_wpt_until = vr_wpt_frames;
            return;
        }
        if(vr_wpt_frames < vr_wpt_until + 6)
        {
            return;
        }
        float f = ragdollbone(b, [2, 1, cVR_MainHand], QVR_WORNPACK);
        VR_Test_Check(p.mainhand_held == b && f >= 0 && b.health > 0, sprintf("the hand holds knocked-down B by its pack (pull %.0f N)", f));
        if(f < 0)
        {
            VR_WPL_End();
            return;
        }
        vr_wpt_lifts0 = vr_wp_lifts;
        pk.vr_wp_peak = 0;
        vr_wpt_frames = 0;
        vr_wpt_state = 26;
        return;
    }
    vector away = p.origin - b.origin;
    away_z = 0;
    away = normalize(away);
    if(vr_wpt_state == 26)
    {
        vr_wpt_hand = vr_wpt_hand + away * 0.6; // (about 43 units/s: a slow drag)
        localcmd(sprintf("vr_mock_hand_to main %g %g %g\n", vr_wpt_hand_x, vr_wpt_hand_y, vr_wpt_hand_z));
        if(vr_wpt_frames < 48)
        {
            return;
        }
        // (A slow drag runs at about the lift force, past it as the body catches on the floor: any kind may be lifted by it: what
        // a pull does; ROUND23.md, "Lift only". It stays on and held.)
        VR_Test_Check(pk.vr_wp_state == VR_WP_WORN && p.mainhand_held == b && 1 &&
            vr_wp_rips == vr_wpt_rips0, sprintf("B dragged slowly by the pack: kept (lifted %g; hardest pull %.0f N of %g)", vr_wp_lifts - vr_wpt_lifts0,
            pk.vr_wp_peak, cvar_hget(cvarh_vr_worn_backpack_lift_force)));
        vr_wpt_hand = vr_wpt_hand + away * 30 + '0 0 20';
        localcmd(sprintf("vr_mock_hand_to main %g %g %g\n", vr_wpt_hand_x, vr_wpt_hand_y, vr_wpt_hand_z));
        vr_wpt_frames = 0;
        vr_wpt_state = 27;
        return;
    }
    if(vr_wp_lifts == vr_wpt_lifts0 && vr_wpt_frames < 36)
    {
        return;
    }
    VR_Test_Check(vr_wp_lifts == vr_wpt_lifts0 + 1 && pk.vr_wp_state == VR_WP_WORN && ragdollbone(b, '0 0 0', QVR_WORNPACK) == 1 &&
        p.mainhand_held == b && vr_wp_rips == vr_wpt_rips0, sprintf("B yanked: lifted by its pack at %.0f N (hardest %.0f), never torn (%g rips), still held",
        vr_wp_lift_pull, pk.vr_wp_peak, vr_wp_rips - vr_wpt_rips0));
    localcmd("-grabmain\n");
    vr_wpt_frames = 0;
    vr_wpt_state = 28;
};

// States 28-29: A's pack hit (its pool's 30), D killed and its pack force-grabbed: both still come off (an enforcer's zone:
// the hit doesn't take it off, its death ends it).
void(entity p) VR_WPL_Others =
{
    vr_wpt_frames = vr_wpt_frames + 1;
    if(vr_wpt_frames == 1)
    {
        vr_wpt_a = VR_WPT_Spawn(p, WID_SHOTGUN, -60, 120, TRUE);
        vr_wpt_a.netname = "A";
        vr_wpt_d = VR_WPT_Spawn(p, WID_SHOTGUN, 0, 200, FALSE);
        vr_wpt_d.netname = "D";
        return;
    }
    entity pk = vr_wpt_a.vr_wp_pack;
    entity pd = vr_wpt_d.vr_wp_pack;
    float baked = vr_wpt_kind == VR_AM_ENFORCER;
    if(vr_wpt_state == 28)
    {
        if(vr_wpt_frames < 3)
        {
            return;
        }
        vr_wpt_rips0 = vr_wp_rips;
        VR_WPT_Hit(vr_wpt_a, p, limbpiece(vr_wpt_a, 0, 22), 40);
        if(baked)
        {
            VR_Test_Check(pk.vr_wp_state == VR_WP_WORN && vr_wp_rips == vr_wpt_rips0, "a hit of 40 on A's model's pack takes nothing off");
        }
        else
        {
            VR_Test_Check(pk.vr_wp_state == VR_WP_OFF && vr_wp_rips == vr_wpt_rips0 + 1, sprintf("a hit of 40 on A's pack still tears it off (%s)",
                vr_wp_rip_why));
        }
        vr_wpt_d.health = 1;
        T_Damage(vr_wpt_d, p, p, 20);
        vr_wpt_until = time + 2;
        vr_wpt_state = 29;
        return;
    }
    if(baked)
    {
        if(vr_wpt_d.vr_wp_pack && time < vr_wpt_until)
        {
            return;
        }
        VR_Test_Check(!vr_wpt_d.vr_wp_pack && !vr_wpt_d.vr_wp_worn, "a dead enforcer's lift zone is gone");
        VR_WPL_End();
        return;
    }
    if(pd.vr_wp_state != VR_WP_DEAD && time < vr_wpt_until)
    {
        return;
    }
    VR_Forcegrab_Launch(pd, cVR_OffHand);
    VR_Test_Check(pd.vr_wp_state == VR_WP_OFF && vr_wp_rip_why == "taken", sprintf("a dead one's pack force-grabbed still comes off (%s)", vr_wp_rip_why));
    VR_WPL_End();
};

// ---- `vr_wornpack_test 5`: the enforcer's lift zone. States 50 (standing), 51 (down), 52 (dead).
void(entity p) VR_WPB_Begin =
{
    VR_Test_Begin("wornpack");
    vr_wpt_old_chance = cvar_hget(cvarh_vr_worn_backpack_hit_roll);
    vr_wpt_old_pool = cvar_hget(cvarh_vr_worn_backpack_pool);
    vr_wpt_old_roll = vr_wpt_old_chance;
    vr_wpt_old_ref = cvar_hget(cvarh_vr_worn_backpack_hit_ref);
    vr_wpt_old_debug = cvar_hget(cvarh_vr_worn_backpack_debug);
    cvar_set("vr_worn_backpack_pool", "30");
    cvar_set("vr_worn_backpack_hit_roll", "1");
    vr_wpt_a = VR_WPT_SpawnKind(p, VR_AM_ENFORCER, WID_SHOTGUN, 0, 100, TRUE);
    vr_wpt_s = VR_WPT_SpawnKind(p, VR_AM_ENFORCER, WID_SWORD, 90, 100, FALSE);
    vr_wpt_u = VR_WPT_SpawnKind(p, VR_AM_ENFORCER, -1, -90, 100, FALSE);
    vr_wpt_a.netname = "A";
    vr_wpt_frames = 0;
    vr_wpt_rips0 = vr_wp_rips;
    vr_wpt_state = 50;
};

void(entity p) VR_WPB_Frame =
{
    vr_wpt_frames = vr_wpt_frames + 1;
    entity a = vr_wpt_a;
    entity pk = a.vr_wp_pack;
    if(vr_wpt_state == 50)
    {
        if(vr_wpt_frames < 2)
        {
            return;
        }
        VR_Test_Check(pk && pk.vr_wp_baked && a.vr_wp_worn == 1 && pk.model == "" && !a.vr_wp_had && pk.solid == SOLID_NOT,
            sprintf("A (a gun): a lift zone, nothing drawn (worn %g, its model '%s')", a.vr_wp_worn, pk.model));
        VR_Test_Check(vr_wpt_s.vr_wp_pack.vr_wp_baked && vr_wpt_u.vr_wp_pack.vr_wp_baked && vr_wpt_s.vr_wp_worn == 1 && vr_wpt_u.vr_wp_worn == 1 &&
            !vr_wpt_s.vr_wp_had && !vr_wpt_u.vr_wp_had, "a sword's and fists' enforcers: a lift zone too");
        vector mid = limbpiece(a, 0, 22);
        makevectors([0, a.angles_y, 0]);
        float back = (mid - a.origin) * v_forward;
        VR_Test_Check(back < -12 && back > -22, sprintf("the zone at the drawn pack, %.1f units behind its middle", back));
        // A shot, a blow, a blast on it: nothing off, nothing counted.
        VR_Hit_SetZone(a, 0, mid);
        VR_Hit_BeginAs(QVR_HITKIND_SHOT, FALSE);
        T_Damage(a, p, p, 60);
        VR_Hit_End();
        VR_Hit_SetZone(a, 0, mid);
        VR_Hit_BeginAs(QVR_HITKIND_MELEE, TRUE);
        T_Damage(a, p, p, 60);
        VR_Hit_End();
        entity blast = spawn();
        setorigin(blast, a.origin + v_right * 40);
        VR_Hit_BeginAs(QVR_HITKIND_SPLASH, FALSE);
        T_Damage(a, blast, p, 60);
        VR_Hit_End();
        remove(blast);
        VR_Test_Check(pk.vr_wp_state == VR_WP_WORN && a.vr_wp_worn == 1 && vr_wp_rips == vr_wpt_rips0 && !pk.vr_wp_dmg,
            sprintf("a shot, a blow and a blast of 60: its zone stays, nothing counted (%g)", pk.vr_wp_dmg));
        VR_WPT_KnockDown(a, p, 60);
        vr_wpt_frames = 0;
        vr_wpt_state = 51;
        return;
    }
    if(vr_wpt_state == 51)
    {
        if(vr_wpt_frames < 36)
        {
            return;
        }
        VR_Test_Check(isragdoll(a) && ragdollbone(a, '0 0 0', QVR_WORNPACK) == 1, "down: its ragdoll has the zone's box");
        VR_RagImpact_Hit(a, limbpiece(a, 0, 22), 20, 20, 20, 0, '0 0 -1');
        VR_Test_Check(pk.vr_wp_state == VR_WP_WORN && a.vr_wp_worn == 1 && vr_wp_rips == vr_wpt_rips0, "a hard landing (20 m/s) takes nothing off");
        vr_wpt_loose0 = VR_WPT_Loose();
        a.health = 1;
        T_Damage(a, p, p, 20);
        vr_wpt_until = time + 5;
        vr_wpt_state = 52;
        return;
    }
    if(VR_WPT_Loose() < vr_wpt_loose0 + 1 && time < vr_wpt_until)
    {
        return;
    }
    VR_Test_Check(VR_WPT_Loose() == vr_wpt_loose0 + 1 && !a.vr_wp_worn, sprintf("killed: its own backpack dropped as before (loose %g -> %g), its zone gone",
        vr_wpt_loose0, VR_WPT_Loose()));
    VR_WPT_End();
};

// ---- `vr_wornpack_test 6`: a replacement's loadout rolled unseen (VR_AGR_Resolve: VR_WP_Refit) gives or takes the pack.
void(entity p) VR_WPR_Run =
{
    VR_Test_Begin("wornpack");
    vr_wpt_old_chance = cvar_hget(cvarh_vr_worn_backpack_hit_roll);
    vr_wpt_old_pool = cvar_hget(cvarh_vr_worn_backpack_pool);
    vr_wpt_old_roll = vr_wpt_old_chance;
    vr_wpt_old_ref = cvar_hget(cvarh_vr_worn_backpack_hit_ref);
    vr_wpt_old_debug = cvar_hget(cvarh_vr_worn_backpack_debug);
    entity g = VR_WPT_SpawnKind(p, VR_AM_GRUNT, WID_SHOTGUN, 0, 120, FALSE);
    entity k = VR_WPT_SpawnKind(p, VR_AM_KNIGHT, WID_SWORD, 60, 120, FALSE);
    VR_Test_Check(g.vr_wp_worn == 1 && !k.vr_wp_worn, "a gun's wears one, a sword's none");
    entity oldself = self;
    self = g;
    VR_AGR_Unmake(self.vr_ag_prop);
    self.vr_ag_prop = world;
    self.vr_ag_wid = -1;
    self.vr_ag_wid = -1;
    self.vr_ag_prop = world;
    VR_AM_ArmAll(WID_SWORD, 0, world, 0, 0);
    VR_WP_Refit(self);
    self = k;
    VR_AGR_Unmake(self.vr_ag_prop);
    self.vr_ag_prop = world;
    self.vr_ag_wid = -1;
    VR_AM_ArmAll(WID_SHOTGUN, 0, world, 0, 0);
    VR_WP_Refit(self);
    self = oldself;
    VR_Test_Check(!g.vr_wp_pack && !g.vr_wp_worn && !g.vr_wp_had, "rolled a sword: its pack gone");
    VR_Test_Check(k.vr_wp_pack && k.vr_wp_worn == 1 && k.vr_wp_had, "rolled a gun: it wears one");
    VR_WPT_End();
};

void(entity p) VR_WPT_Eyeshot =
{
    float near = vr_wpt_kind == VR_AM_DKNIGHT ? 96 : 64; // (he stands taller)
    entity g1 = VR_WPT_Spawn(p, WID_SHOTGUN, -24, near, TRUE);
    entity g2 = VR_WPT_Spawn(p, WID_SHOTGUN, 24, near, FALSE);
    g2.angles_y = g2.angles_y + 90;
    entity g3 = VR_WPT_Spawn(p, WID_SHOTGUN, 60, 110, FALSE);
    VR_WPT_KnockDown(g3, p, 40);
    g1.netname = g2.netname = g3.netname = "eyeshot";
};


// `vr_wornpack_test 4` (his note 2026-11-01 18:55: "If I push my hand hard enough into an enemy grunt, it can actually get
// through the grunt and then I'm able to grab the backpack while the grunt is facing me"): a packed grunt facing
// you, your empty main hand glided slowly (about 1.4 m/s: no melee phasing) from before its chest through its body to
// its pack's outer face, the grip pressed there: the hand went into its body (vr_modelcollide.cpp Through), drawn held
// out of it, and the pack not taken. Then one with its back to you (in its place), the hand glided onto its pack from
// behind: taken. Ends on `wornpack: <p> passed, <f> failed`. States 40 (facing), 41 (its back).
float vr_wpp_glide = 0.8; // s each glide takes
vector vr_wpp_start, vr_wpp_end;

void(entity p) VR_WPP_Begin =
{
    VR_Test_Begin("wornpack");
    vr_wpt_old_chance = cvar_hget(cvarh_vr_worn_backpack_hit_roll);
    vr_wpt_old_pool = cvar_hget(cvarh_vr_worn_backpack_pool);
    vr_wpt_old_roll = vr_wpt_old_chance;
    vr_wpt_old_ref = cvar_hget(cvarh_vr_worn_backpack_hit_ref);
    vr_wpt_old_debug = cvar_hget(cvarh_vr_worn_backpack_debug);
    cvar_set("vr_worn_backpack_debug", ftos(max(1, vr_wpt_old_debug)));
    cvar_set("vr_worn_backpack_pool", "30");
    cvar_set("vr_worn_backpack_hit_roll", "0");
    cvar_set("vr_mock_hand_to_absolute", "1");
    p.flags = p.flags | FL_GODMODE | FL_NOTARGET;
    p.weapon = WID_FIST; // (an empty main hand)
    p.weaponinst = world;
    p.weaponflags = 0;
    vr_wpt_a = world;
    vr_wpt_state = 40;
    vr_wpt_frames = 0;
};

// The frame of state 40 / 41: spawned, the hand put before it, glided to its pack's outer face, the grip pressed there.
void(entity p) VR_WPP_Frame =
{
    float behind = vr_wpt_state == 41;
    vr_wpt_frames = vr_wpt_frames + 1;
    float f = vr_wpt_frames;
    float glided = 8 + ceil(vr_wpp_glide * 72) + 6; // (the glide's frames, and a few to settle)
    if(f == 1)
    {
        if(vr_wpt_a)
        {
            remove(vr_wpt_a.vr_wp_pack); // (the one facing you gone: the next where it stood, in reach)
            remove(vr_wpt_a);
        }
        vr_wpt_a = VR_WPT_SpawnKind(p, VR_AM_GRUNT, -1, 0, 44, behind);
        vr_wpt_a.vr_ag_main_wid = WID_SHOTGUN; // (its hands empty as this test's bodies always were: the pack by a gun stowed)
        VR_WP_Spawn(vr_wpt_a);
        vr_wpt_a.netname = behind ? "its back to you" : "facing you";
        vr_wpt_a.enemy = world;
        return;
    }
    entity g = vr_wpt_a;
    if(f == 3)
    {
        // From 30 units before the side that faces you (its chest, or its pack) to the pack's outer face.
        makevectors([0, g.angles_y, 0]);
        vector pack = limbpiece(g, 0, 22);
        vector toYou = behind ? -v_forward : v_forward;
        vr_wpp_start = g.origin + toYou * 30;
        vr_wpp_start_z = pack_z;
        vr_wpp_end = pack - v_forward * 3;
        localcmd(sprintf("vr_mock_hand_glide 0\nvr_mock_hand_to main %g %g %g\n", vr_wpp_start_x, vr_wpp_start_y,
            vr_wpp_start_z));
        return;
    }
    if(f == 8)
    {
        localcmd(sprintf("vr_mock_hand_glide %g\nvr_mock_hand_to main %g %g %g\n", vr_wpp_glide, vr_wpp_end_x, vr_wpp_end_y,
            vr_wpp_end_z));
        return;
    }
    if(f == glided)
    {
        vector th = weapondrawnpose(p, cVR_MainHand, 40);
        if(behind)
        {
            VR_Test_Check(th_x == 0, sprintf("%s: the hand on its pack went into no body (%g; drawn %.1f units off)",
                g.netname, th_x, th_y));
        }
        else
        {
            VR_Test_Check(th_x > 0 && th_y > 10, sprintf("%s: the hand pushed through its body is drawn held out of it (in %g, "
                "drawn %.1f units off the tracked hand)", g.netname, th_x, th_y));
        }
        localcmd("+grabmain\n");
        return;
    }
    if(f < glided + 12)
    {
        return;
    }
    float sm = ragdollbone(g, [5, 1, cVR_MainHand], QVR_WORNPACK);
    if(behind)
    {
        VR_Test_Check(sm >= 0 && g.vr_wp_worn == 1, sprintf("%s: the hand on its pack takes it (stretch %.1f, worn %g)",
            g.netname, sm, g.vr_wp_worn));
    }
    else
    {
        VR_Test_Check(sm < 0 && g.vr_wp_worn == 1, sprintf("%s: the hand pushed through its body doesn't take its pack "
            "(held %.1f, worn %g)", g.netname, sm, g.vr_wp_worn));
    }
    localcmd("-grabmain\nvr_mock_hand_glide 0\n");
    if(behind)
    {
        p.flags = p.flags - (p.flags & (FL_GODMODE | FL_NOTARGET));
        VR_WPT_End();
        return;
    }
    vr_wpt_state = 41;
    vr_wpt_frames = 0;
};


void() VR_WornPack_TestFrame =
{
    entity p = VR_Player();
    if(vr_wpt_state == 0)
    {
        float request = cvar_hget(cvarh_vr_wornpack_test);
        if(!request || !p)
        {
            return;
        }
        cvar_set("vr_wornpack_test", "0");
        float tens = floor(request / 10);
        vr_wpt_kind = tens == 1 ? VR_AM_KNIGHT : (tens == 2 ? VR_AM_DKNIGHT : (tens == 3 ? VR_AM_ENFORCER : VR_AM_GRUNT));
        if(request == 1 || request == 11 || request == 21)
        {
            VR_WPT_Begin(p);
        }
        if(request == 2 || request == 12 || request == 22)
        {
            VR_WPT_Eyeshot(p);
        }
        if(request == 3 || request == 13 || request == 23 || request == 33)
        {
            VR_WPL_Begin(p);
        }
        if(request == 4)
        {
            VR_WPP_Begin(p);
        }
        if(request == 5)
        {
            VR_WPB_Begin(p);
        }
        if(request == 6)
        {
            VR_WPR_Run(p);
        }
        return;
    }
    if(vr_wpt_state == 40 || vr_wpt_state == 41)
    {
        VR_WPP_Frame(p);
        return;
    }
    if(vr_wpt_state >= 50)
    {
        VR_WPB_Frame(p);
        return;
    }
    if(vr_wpt_state >= 20 && vr_wpt_state <= 24)
    {
        VR_WPL_Standing(p);
        return;
    }
    if(vr_wpt_state >= 25 && vr_wpt_state <= 27)
    {
        VR_WPL_Down(p);
        return;
    }
    if(vr_wpt_state >= 28)
    {
        VR_WPL_Others(p);
        return;
    }
    if(vr_wpt_state == 2)
    {
        VR_WPT_Standing(p);
        return;
    }
    if(vr_wpt_state == 3)
    {
        VR_WPT_Down(p);
        return;
    }
    if(vr_wpt_state == 7)
    {
        VR_WPT_Deaths(p);
        return;
    }
    if(vr_wpt_state == 8)
    {
        VR_WPT_Gibbed(p);
    }
};
#endif
