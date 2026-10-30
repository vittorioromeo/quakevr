// vr_wornpack_test.qc -- worn backpacks' test (vr_wornpack.qc; Debug > Tests; ROUND23.md, "Worn backpacks").
// `vr_wornpack_test 1` (any map with room ahead: e1m1's start; god, notarget): armed grunts ahead with
// vr_worn_backpacks 1 (one with it 0 for the control). Checks: the pack on the back, untakeable while it lives; a hit on
// its chest leaves it, a hit on the pack tears it off (the rip, its reserve gone with it); a mild knockdown keeps it (the
// pack where it was as the ragdoll is made; the ragdoll's box), a mild landing too, a hard one tears it off; a hand takes
// the ragdoll by its pack, drags it slowly (kept), yanks (torn off into the hand); a wearer killed drops no other backpack
// (the control drops one), its pack takeable then, and taken it comes off. Ends on `wornpack: <p> passed, <f> failed`.
// `vr_wornpack_test 2`: the eyeshot's: grunts wearing packs, back on, side on and knocked down.

float vr_wpt_state;
float vr_wpt_until;
float vr_wpt_frames;
float vr_wpt_old_on, vr_wpt_old_chance, vr_wpt_old_ref, vr_wpt_old_debug;
entity vr_wpt_a, vr_wpt_b, vr_wpt_c, vr_wpt_d, vr_wpt_e, vr_wpt_f, vr_wpt_g, vr_wpt_h;
vector vr_wpt_pull;     // the tug's way (level, towards the player)
float vr_wpt_gapmax;    // the drawn palm's farthest off the pack's box while pulled (cm)
float vr_wpt_one;       // one hand's tug time (s)
float vr_wpt_old_pool, vr_wpt_old_roll;
vector vr_wpt_b0;       // B's pack before its knockdown
float vr_wpt_loose0;    // loose backpacks before the deaths
float vr_wpt_rips0;
vector vr_wpt_hand;     // the mock hand's target

// An armed grunt holding `wid` ahead of the player, `side` to the right, `ahead` out, facing the player (or away).
entity(entity p, float wid, float side, float ahead, float away) VR_WPT_Spawn =
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
    VR_AM_Spawn(VR_AM_GRUNT);
    entity g = self;
    self = oldself;
    g.vr_ar_type = 0;
    g.vr_ar_mask = 0;
    g.health = 1000;
    return g;
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
    cvar_set("vr_worn_backpacks", ftos(vr_wpt_old_on));
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
    vr_wpt_old_on = cvar("vr_worn_backpacks");
    vr_wpt_old_chance = cvar("vr_worn_backpack_hit_roll");
    vr_wpt_old_pool = cvar("vr_worn_backpack_pool");
    vr_wpt_old_roll = vr_wpt_old_chance;
    vr_wpt_old_ref = cvar("vr_worn_backpack_hit_ref");
    vr_wpt_old_debug = cvar("vr_worn_backpack_debug");
    cvar_set("vr_worn_backpack_debug", ftos(max(1, vr_wpt_old_debug)));
    cvar_set("vr_worn_backpacks", "0");
    vr_wpt_e = VR_WPT_Spawn(p, WID_SHOTGUN, 60, 120, FALSE); // (the control: no pack)
    cvar_set("vr_worn_backpacks", "1");
    cvar_set("vr_worn_backpack_pool", "30");
    cvar_set("vr_worn_backpack_hit_roll", "0");
    vr_wpt_a = VR_WPT_Spawn(p, WID_SHOTGUN, -60, 120, TRUE);
    vr_wpt_b = VR_WPT_Spawn(p, -1, -20, 80, FALSE);
    vr_wpt_c = VR_WPT_Spawn(p, -1, 25, 150, FALSE);
    vr_wpt_d = VR_WPT_Spawn(p, WID_SHOTGUN, 0, 200, FALSE);
    vr_wpt_f = VR_WPT_Spawn(p, -1, 90, 200, FALSE);
    vr_wpt_f.netname = "F";
    vr_wpt_a.netname = "A";
    vr_wpt_b.netname = "B";
    vr_wpt_c.netname = "C";
    vr_wpt_d.netname = "D";
    vr_wpt_e.netname = "E";
    vr_wpt_state = 2;
};

// State 2 (its first frame placed): the standing checks, A's hits, B and C knocked down.
void(entity p) VR_WPT_Standing =
{
    entity a = vr_wpt_a;
    entity pk = a.vr_wp_pack;
    VR_Test_Check(pk && a.vr_wp_worn == 1 && pk.model == "progs/backpack.mdl" && pk.vr_wp_state == VR_WP_WORN &&
        !vr_wpt_e.vr_wp_pack, sprintf("A wears a pack (worn %g, state %g), the control E none", a.vr_wp_worn, pk.vr_wp_state));
    vector mid = limbpiece(a, 0, 22);
    makevectors([0, a.angles_y, 0]);
    float back = (mid - a.origin) * v_forward;
    float up = mid_z - a.origin_z;
    VR_Test_Check(back < -4 && back > -20 && up > 4 && up < 30 && vlen(pk.origin - limbpiece(a, 0, 20)) < 0.1,
        sprintf("on its back: %.1f units behind its middle, %.1f up", back, up));
    VR_Test_Check(pk.solid == SOLID_NOT && !(pk.flags & FL_FORCEGRABBABLE) && !(pk.flags & FL_ITEM) &&
        pk.handtouch != VR_Carry_Handtouch, "not takeable from the living (not touchable, not force-grabbable)");
    // A hit on its chest: the pack stays.
    float r0 = a.vr_ag_reserve;
    float s0 = a.ammo_shells;
    VR_WPT_Hit(a, p, a.origin + v_forward * 12 + '0 0 18', 10);
    VR_Test_Check(pk.vr_wp_state == VR_WP_WORN, sprintf("a hit on its chest leaves the pack on (%g units from it)",
        ragdollbone(a, a.origin + v_forward * 12 + '0 0 18', QVR_WORNPACK_AT)));
    // Upright on its back, turned with it (not spinning: the engine keeps a worn pack's angles), its box the model's.
    float dyaw = pk.angles_y - a.angles_y;
    dyaw = dyaw - 360 * floor((dyaw + 180) / 360);
    vector sz = pk.maxs - pk.mins;
    VR_Test_Check(fabs(dyaw) < 25 && fabs(pk.angles_x) < 30 && fabs(pk.angles_z) < 25 && pk.vr_wp_state == VR_WP_WORN,
        sprintf("upright on its back, turned with it: yaw %.1f off its own, pitch %.1f, roll %.1f", dyaw, pk.angles_x, pk.angles_z));
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
    vr_hitkind = QVR_HITKIND_SPLASH;
    T_Damage(a, blast, p, 8);
    vr_hitkind = QVR_HITKIND_NONE;
    float d2 = pk.vr_wp_dmg;
    VR_Test_Check(pk.vr_wp_state == VR_WP_WORN && d2 > d1 && d2 < 30, sprintf("an explosion's damage counts (%g of 30)", d2));
    blast.classname = "vr_adpoison";
    T_Damage(a, blast, p, 10);
    VR_Test_Check(pk.vr_wp_state == VR_WP_WORN && pk.vr_wp_dmg == d2, sprintf("poison doesn't (%g of 30)", pk.vr_wp_dmg));
    blast.classname = "vr_adflame";
    vr_hit_nobleed = TRUE;
    T_Damage(a, blast, p, 10);
    vr_hit_nobleed = FALSE;
    remove(blast);
    VR_Test_Check(pk.vr_wp_state == VR_WP_OFF && vr_wp_rips == vr_wpt_rips0 + 1 && vr_wp_rip_why == "fire" &&
        vr_wp_rip_sound == VR_WP_RIP_SOUND && a.vr_wp_worn == 0 && (pk.flags & FL_FORCEGRABBABLE),
        sprintf("the flamethrower's fire takes it to the pool: torn off (%s, %g), takeable now", vr_wp_rip_why, pk.vr_wp_dmg));
    VR_Test_Check(r0 > 0 && a.vr_ag_reserve == 0 && pk.ammo_shells >= r0 + s0, sprintf("its spare rounds went with it: reserve %g -> %g, the pack's shells %g",
        r0, a.vr_ag_reserve, pk.ammo_shells));
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
    // The player beside B (within reach), the hand onto its pack.
    vr_wpt_hand = limbpiece(b, 0, 22);
    vector out = p.origin - vr_wpt_hand;
    out_z = 0;
    vector at = vr_wpt_hand + normalize(out) * 20;
    at_z = p.origin_z;
    setorigin(p, at);
    p.velocity = '0 0 0';
    cvar_set("vr_mock_hand_to_absolute", "1");
    localcmd(sprintf("vr_mock_hand_to main %g %g %g\n", vr_wpt_hand_x, vr_wpt_hand_y, vr_wpt_hand_z));
    vr_wpt_frames = 0;
    vr_wpt_state = 4;
};

// State 4: the hand on B's pack, the grip pressed; 5: held by the pack, dragged slowly; 6: yanked.
void(entity p) VR_WPT_Pull =
{
    entity b = vr_wpt_b;
    entity pk = b.vr_wp_pack;
    vr_wpt_frames = vr_wpt_frames + 1;
    if(vr_wpt_state == 4)
    {
        // (The hand kept on the pack's middle as the body settles; the grip pressed once it is there.)
        float gap = ragdollbone(b, p.handpos, QVR_WORNPACK_AT);
        if(!vr_wpt_until)
        {
            vr_wpt_hand = limbpiece(b, 0, 22);
            localcmd(sprintf("vr_mock_hand_to main %g %g %g\n", vr_wpt_hand_x, vr_wpt_hand_y, vr_wpt_hand_z));
            if(gap < 0 || gap > 2)
            {
                if(vr_wpt_frames < 90)
                {
                    return;
                }
                VR_Test_Check(FALSE, sprintf("the hand gets onto B's pack (%.1f units off, the pack %.1f units over your feet)", gap,
                    vr_wpt_hand_z - p.origin_z + 24));
                VR_WPT_End();
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
        vr_wpt_until = 0;
        float f = ragdollbone(b, [2, 1, cVR_MainHand], QVR_WORNPACK);
        VR_Test_Check(p.mainhand_held == b && f >= 0, sprintf("the hand holds B by its pack (holds %s, pull %.0f N; hand %.1f units from the pack)",
            p.mainhand_held.classname, f, ragdollbone(b, p.handpos, QVR_WORNPACK_AT)));
        if(f < 0)
        {
            VR_WPT_End();
            return;
        }
        vr_wpt_frames = 0;
        vr_wpt_state = 5;
        return;
    }
    vector away = p.origin - b.origin;
    away_z = 0;
    away = normalize(away);
    if(vr_wpt_state == 5)
    {
        vr_wpt_hand = vr_wpt_hand + away * 0.6; // (about 43 units/s: a slow drag)
        localcmd(sprintf("vr_mock_hand_to main %g %g %g\n", vr_wpt_hand_x, vr_wpt_hand_y, vr_wpt_hand_z));
        if(vr_wpt_frames < 48)
        {
            return;
        }
        VR_Test_Check(pk.vr_wp_state == VR_WP_WORN && p.mainhand_held == b, sprintf("dragged slowly by the pack: kept (hardest pull %.0f N)",
            pk.vr_wp_peak));
        vr_wpt_hand = vr_wpt_hand + away * 30 + '0 0 20';
        localcmd(sprintf("vr_mock_hand_to main %g %g %g\n", vr_wpt_hand_x, vr_wpt_hand_y, vr_wpt_hand_z));
        vr_wpt_frames = 0;
        vr_wpt_state = 6;
        return;
    }
    if(pk.vr_wp_state != VR_WP_OFF && vr_wpt_frames < 36)
    {
        return;
    }
    VR_Test_Check(pk.vr_wp_state == VR_WP_OFF && vr_wp_rip_why == "pulled" && p.mainhand_held == pk && b.vr_wp_worn == 0,
        sprintf("yanked: torn off into the hand (%s, hand holds %s, hardest pull %.0f N)", vr_wp_rip_why,
        p.mainhand_held.classname, pk.vr_wp_peak));
    localcmd("-grabmain\n");
    // D (packed) and E (the control) killed.
    vr_wpt_loose0 = VR_WPT_Loose();
    vr_wp_deathloots = 0;
    vr_wpt_d.health = 1;
    vr_wpt_e.health = 1;
    T_Damage(vr_wpt_d, p, p, 20);
    T_Damage(vr_wpt_e, p, p, 20);
    vr_wpt_until = time + 5;
    vr_wpt_state = 7;
};

// State 7: their loot counted; no second backpack from D, one from E; D's pack takeable, and taken it comes off.
void(entity p) VR_WPT_Deaths =
{
    entity d = vr_wpt_d;
    entity pk = d.vr_wp_pack;
    float loose = VR_WPT_Loose();
    if((vr_wp_deathloots < 1 || loose < vr_wpt_loose0 + 1) && time < vr_wpt_until)
    {
        return;
    }
    VR_Test_Check(vr_wp_deathloots == 1 && loose == vr_wpt_loose0 + 1, sprintf("killed: the wearer drops no other backpack, the control one (loose %g -> %g)",
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

// State 8: F's pack off (gibbed); G spawned facing away (unaware: notarget), its back to the player, for the tug.
// 9: the hand on G's pack, the grip pressed; held by the pack (it spotted you), the palm on the pack's face;
// 10: pulled back at 0.5 m/s: torn off into the hand, G toppled the way of the pull; 11-12: H by both hands: sooner.
void(entity p) VR_WPT_Tug =
{
    vr_wpt_frames = vr_wpt_frames + 1;
    float two = vr_wpt_state >= 11;
    entity g = two ? vr_wpt_h : vr_wpt_g;
    if(vr_wpt_state == 8 || vr_wpt_state == 11)
    {
        if(vr_wpt_frames == 1)
        {
            if(vr_wpt_state == 8)
            {
                VR_Test_Check(vr_wpt_f.vr_wp_pack.vr_wp_state == VR_WP_OFF && vr_wpt_f.vr_wp_worn == 0,
                    sprintf("gibbed: its pack comes off (%s)", vr_wpt_f.vr_wp_pack.vr_wp_state == VR_WP_OFF ? vr_wp_rip_why : "on"));
            }
            g = VR_WPT_Spawn(p, WID_SHOTGUN, 0, 44, TRUE);
            g.netname = two ? "H" : "G";
            g.enemy = world;
            if(two)
            {
                vr_wpt_h = g;
            }
            else
            {
                vr_wpt_g = g;
            }
            return;
        }
        VR_WPT_HandsOnPack(p, g, two);
        if(vr_wpt_frames < 4)
        {
            return;
        }
        if(vr_wpt_frames == 4)
        {
            VR_Test_Check(g.enemy == world, sprintf("%s unaware before the grab (its enemy %s)", g.netname, g.enemy.classname));
            p.flags = p.flags - (p.flags & FL_NOTARGET); // (seen again: the grab's alert is to be seen)
            localcmd(two ? "+grabmain;+graboff\n" : "+grabmain\n");
            return;
        }
        float sm = ragdollbone(g, [5, 1, cVR_MainHand], QVR_WORNPACK);
        float so = ragdollbone(g, [5, 1, cVR_OffHand], QVR_WORNPACK);
        if((sm < 0 || (two && so < 0)) && vr_wpt_frames < 30)
        {
            return;
        }
        float gap = ragdollbone(g, [7, 1, cVR_MainHand], QVR_WORNPACK);
        VR_Test_Check(sm >= 0 && (!two || so >= 0) && g.vr_wp_pack.vr_wp_state == VR_WP_WORN && !isragdoll(g),
            sprintf("%s's pack held by %s while it stands (stretch main %.1f, off %.1f units)", g.netname, two ? "both hands" : "the hand",
            sm, so));
        VR_Test_Check(g.enemy == p, sprintf("%s alerted by the grab (its enemy %s)", g.netname, g.enemy.classname));
        VR_Test_Check(gap >= 2 && gap <= 5, sprintf("the drawn palm on the pack's face (%.1f cm off its box: the palm's middle)", gap));
        if(sm < 0)
        {
            VR_WPT_End();
            return;
        }
        vr_wpt_pull = p.origin - g.origin;
        vr_wpt_pull_z = 0;
        vr_wpt_pull = normalize(vr_wpt_pull);
        vr_wpt_gapmax = 0;
        vr_wpt_frames = 0;
        vr_wpt_state = vr_wpt_state + 1;
        return;
    }
    entity pk = g.vr_wp_pack;
    if(pk.vr_wp_state == VR_WP_WORN && vr_wpt_frames < 216)
    {
        float gp = ragdollbone(g, [7, 1, cVR_MainHand], QVR_WORNPACK);
        vr_wpt_gapmax = max(vr_wpt_gapmax, gp);
        VR_WPT_HandsBack(0.5 * VRMetersToUnits() / 72, two); // (0.5 m/s)
        return;
    }
    float secs = vr_wpt_frames / 72;
    VR_Test_Check(pk.vr_wp_state == VR_WP_OFF && vr_wp_rip_why == "pulled" && p.mainhand_held == pk && vr_wp_two_hands == 1 + two,
        sprintf("%s pulled at 0.5 m/s by %s: torn off into the hand after %.2f s (counted %.2f s; holds %s)", g.netname,
        two ? "both hands" : "one hand", secs, vr_wp_tug_secs, p.mainhand_held.classname));
    VR_Test_Check(vr_wpt_gapmax <= 5, sprintf("the drawn palm kept on the pack as it pulled (at most %.1f cm off its box)", vr_wpt_gapmax));
    VR_Test_Check(g.vr_knockdown && vr_wp_topple_dir * vr_wpt_pull > 0.9, sprintf("%s toppled the way of the tear (knocked down %g, along %.2f: %v, the pull %v)",
        g.netname, g.vr_knockdown, vr_wp_topple_dir * vr_wpt_pull, vr_wp_topple_dir, vr_wpt_pull));
    localcmd("-grabmain;-graboff\n");
    p.flags = p.flags | FL_NOTARGET;
    if(!two)
    {
        vr_wpt_one = secs;
        vr_wpt_frames = 0;
        vr_wpt_state = 11;
        return;
    }
    VR_Test_Check(secs < vr_wpt_one * 0.85, sprintf("two hands tear it sooner: %.2f s against one's %.2f s (x%g)", secs, vr_wpt_one,
        cvar("vr_worn_backpack_two_hand_mult")));
    VR_WPT_End();
};

void(entity p) VR_WPT_Eyeshot =
{
    cvar_set("vr_worn_backpacks", "1");
    entity g1 = VR_WPT_Spawn(p, WID_SHOTGUN, -22, 64, TRUE);
    entity g2 = VR_WPT_Spawn(p, WID_SHOTGUN, 22, 72, FALSE);
    g2.angles_y = g2.angles_y + 90;
    entity g3 = VR_WPT_Spawn(p, -1, 60, 110, FALSE);
    VR_WPT_KnockDown(g3, p, 40);
    g1.netname = g2.netname = g3.netname = "eyeshot";
};

void() VR_WornPack_TestFrame =
{
    entity p = VR_Player();
    if(vr_wpt_state == 0)
    {
        float request = cvar("vr_wornpack_test");
        if(!request || !p)
        {
            return;
        }
        cvar_set("vr_wornpack_test", "0");
        if(request == 1)
        {
            VR_WPT_Begin(p);
        }
        if(request == 2)
        {
            VR_WPT_Eyeshot(p);
        }
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
    if(vr_wpt_state >= 4 && vr_wpt_state <= 6)
    {
        VR_WPT_Pull(p);
        return;
    }
    if(vr_wpt_state == 7)
    {
        VR_WPT_Deaths(p);
        return;
    }
    if(vr_wpt_state >= 8 && vr_wpt_state <= 12)
    {
        VR_WPT_Tug(p);
    }
};
