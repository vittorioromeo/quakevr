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
entity vr_wpt_a, vr_wpt_b, vr_wpt_c, vr_wpt_d, vr_wpt_e;
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
    cvar_set("vr_worn_backpack_hit_chance", ftos(vr_wpt_old_chance));
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
    vr_wpt_old_chance = cvar("vr_worn_backpack_hit_chance");
    vr_wpt_old_ref = cvar("vr_worn_backpack_hit_ref");
    vr_wpt_old_debug = cvar("vr_worn_backpack_debug");
    cvar_set("vr_worn_backpack_debug", ftos(max(1, vr_wpt_old_debug)));
    cvar_set("vr_worn_backpacks", "0");
    vr_wpt_e = VR_WPT_Spawn(p, WID_SHOTGUN, 60, 120, FALSE); // (the control: no pack)
    cvar_set("vr_worn_backpacks", "1");
    cvar_set("vr_worn_backpack_hit_chance", "1");
    cvar_set("vr_worn_backpack_hit_ref", "10");
    vr_wpt_a = VR_WPT_Spawn(p, WID_SHOTGUN, -60, 120, TRUE);
    vr_wpt_b = VR_WPT_Spawn(p, -1, -20, 80, FALSE);
    vr_wpt_c = VR_WPT_Spawn(p, -1, 25, 150, FALSE);
    vr_wpt_d = VR_WPT_Spawn(p, WID_SHOTGUN, 0, 200, FALSE);
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
    // A hit on the pack: torn off, its reserve with it.
    vr_wpt_rips0 = vr_wp_rips;
    VR_WPT_Hit(a, p, mid, 12);
    VR_Test_Check(pk.vr_wp_state == VR_WP_OFF && vr_wp_rips == vr_wpt_rips0 + 1 && vr_wp_rip_why == "hit" &&
        vr_wp_rip_sound == VR_WP_RIP_SOUND && a.vr_wp_worn == 0 && (pk.flags & FL_FORCEGRABBABLE),
        sprintf("a hit on the pack tears it off (%s, %s), takeable now", vr_wp_rip_why, vr_wp_rip_sound));
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
    VR_WPT_End();
};

void(entity p) VR_WPT_Eyeshot =
{
    cvar_set("vr_worn_backpacks", "1");
    entity g1 = VR_WPT_Spawn(p, WID_SHOTGUN, -45, 150, TRUE);
    entity g2 = VR_WPT_Spawn(p, WID_SHOTGUN, 5, 175, FALSE);
    g2.angles_y = g2.angles_y + 90;
    entity g3 = VR_WPT_Spawn(p, -1, 55, 150, FALSE);
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
    }
};
