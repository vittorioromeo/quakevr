#ifdef QVR_TESTS
// vr_attach_test.qc -- attach2: weapon attachments, the bayonet on the shotgun and the super shotgun (QC vr_attach.qc; docs/vr-port/
// ATTACHMENTS.md, "The system"). Map vrtesthall, god, notarget, on your feet in the open. Misc/quakevr/attach_test.sh runs them.
// `vr_embed_test 79` (VR, the mock headset; vr_attach_probe 1): the shotgun in the main hand, a bayonet carried in the off hand:
//   brought to the gun's mount it snaps on (the record's .wi_attach, the carried bayonet gone); the drawn bayonet's tip where
//   the server's is (VR_Attach_TipOf: the gun's Pitch/Yaw/Roll in the hand counted); the empty off hand's grip at the mount
//   pulls it off into that hand (the shell script: no two-handed grip taken by it); on again; holstered and drawn, dropped and
//   picked up by the hand, thrown at a grunt (stuck by the bayonet's point): the same record, the bayonet on it throughout;
//   stabbed into a grunt with the shotgun and with the super shotgun (hurt, stuck by its point, the hand on it), the trigger
//   then fires it where it is (one shot from the prop, the grunt hit); the shotgun with its bayonet fired in the hand.
//   Ends `attach: N passed, M failed`.
// `vr_embed_test 80` (flat-screen, -flatscreenmode): J (impulse 52) puts the bayonet the off hand carries on the shotgun, J
//   takes it off into the off hand, J on again; V's thrust sticks it in a grunt; fire shoots where it is; R reloads it there
//   (its clip back up). The super shotgun likewise: stuck, fired empty it breaks open there, R loads and shuts it.
//   Ends `attachflat: N passed, M failed`.
// `vr_embed_test 81` / `82` (persistence): 81 gives the shotgun with a bayonet in the main hand and the super shotgun with one in
//   the left hip holster (prints `attachpersist: set`); after a level change or a save and its load, 82 checks both records
//   still carry their bayonets. Ends `attachpersist: N passed, M failed`.

float VR_AT_SNAP_FRAMES = 3;  // frames between the off hand's nudges towards the mount (its move shows a frame on)
float VR_AT_SNAP_TRIES = 40;  // nudges before the snap counts as failed
float VR_AT_TIP_OFF = 1.5;    // units the drawn bayonet's tip may be off the server's
float VR_AT_HEALTH = 5000;    // the grunts' health (the stabs and shots measured, never killed)
float vr_at_s;
float vr_at_t0;
float vr_at_n;                // the nudges so far
float vr_at_after;            // the state the snap goes on to
float vr_at_count;            // a counter noted before a step (attached, detached, shots)
float vr_at_health;
float vr_at_shells;
float vr_at_flat;
float vr_at_wid;              // the gun the stab case is for
entity vr_at_rec;             // the gun's record
entity vr_at_w;               // the gun lying, thrown or stuck
vector vr_at_aim;             // the off hand's aim as the snap nudges it
float vr_at_mode;             // vr_holster_mode before the test

void(float xS) VR_AT_To =
{
    vr_at_s = xS;
    vr_at_t0 = time;
    vr_at_n = 0;
};

// A bayonet spawned and carried in `xHand` of self.
entity(float xHand) VR_AT_Bayonet =
{
    entity e = spawn();
    setorigin(e, VRGetEntHandPos(self, xHand));
    entity oldSelf = self;
    self = e;
    vr_bayonet();
    self = oldSelf;
    setorigin(e, VRGetEntHandPos(self, xHand));
    VR_Carry_Start(self, xHand, e);
    return e;
};

// The gun `xWid` in the main hand with a new record (its bayonet on if `xOn`), its clip full and plenty of shells.
void(float xWid, float xOn) VR_AT_Give =
{
    VR_EH_Give(xWid, 0);
    vr_at_rec = VRGetEntWeaponInst(self, cVR_MainHand);
    vr_at_rec.wi_attach = xOn ? VR_ATT_BAYONET : 0;
    self.ammo_shells = 60;
    vr_at_rec.wi_clip = VR_ClipSizeIn(self, cVR_MainHand);
    W_SetCurrentAmmoFor(cVR_MainHand);
    VRSetAttackFinished(cVR_MainHand, 0);
};

void() VR_AT_Finish =
{
    localcmd("-grabmain\n-graboff\n-attack\nvr_test_handcontact 1 0\nvr_mock_button main trigger 0\nvr_mock_hand_aim off off\nvr_mock_hand_aim main off\n");
    if(!vr_at_flat)
    {
        localcmd("vr_mock_hand_to_absolute 0\n");
    }
    cvar_set("vr_attach_probe", "0");
    cvar_set("vr_holster_mode", ftos(vr_at_mode));
    VR_PE_Finish();
};

// One nudge of the off hand (carrying a bayonet) towards the main hand's gun's mount, pointing along the gun: TRUE once it is on.
float() VR_AT_Snap =
{
    entity p = self;
    if(vr_at_rec.wi_attach == VR_ATT_BAYONET)
    {
        return TRUE;
    }
    entity c = VR_Carry_Held(p, cVR_OffHand);
    if(c == world)
    {
        return FALSE;
    }
    if(vr_at_n == 0 || floor((time - vr_at_t0) * 72 + 0.5) >= vr_at_n * VR_AT_SNAP_FRAMES)
    {
        vector at = VR_Attach_HandPoint(p, cVR_MainHand, VR_ATT_BAYONET, 1);
        // (The hand's aim nudged until the carried bayonet points the way it sits on the gun: its hold turns it off the aim.)
        vector want = VR_Attach_Way(p, cVR_MainHand, VR_ATT_BAYONET);
        vector ca = c.angles;
        ca_x = -ca_x;
        makevectors(ca);
        vr_at_aim = vr_at_n == 0 ? want : normalize(vr_at_aim + want - v_forward);
        localcmd(sprintf("vr_mock_hand_aim off %v\nvr_mock_hand_to off by %v\n", VRGetEntHandPos(p, cVR_OffHand) + vr_at_aim * 8000,
            at - c.origin));
        if(cvar_hget(cvarh_developer) && !(vr_at_n & 7))
        {
            dprint(sprintf("attach: snap nudge %g: bayonet %v, mount %v (%.1f off); ", vr_at_n, c.origin, at, vlen(at - c.origin)));
            dprint(sprintf("bayonet angles %v, gun hand %v, off hand %v\n", c.angles, VRGetEntHandRot(p, cVR_MainHand), VRGetEntHandRot(p, cVR_OffHand)));
        }
        vr_at_n = vr_at_n + 1;
    }
    return FALSE;
};

// The drawn bayonet's tip (the client's, vr_attach_probe_tip) against the server's.
void(string xWhat) VR_AT_TipCheck =
{
    float on = cvar_hget(cvarh_vr_attach_probe_drawn);
    vector drawn = [cvar_hget(cvarh_vr_attach_probe_x), cvar_hget(cvarh_vr_attach_probe_y), cvar_hget(cvarh_vr_attach_probe_z)];
    vector server = VR_Attach_TipOf(self, cVR_MainHand);
    if(cvar_hget(cvarh_developer))
    {
        dprint(sprintf("attach: tip drawn %v, server %v; mount %v; muzzle %v; hand %v %v\n", drawn, server,
            VR_Attach_HandPoint(self, cVR_MainHand, VR_ATT_BAYONET, 1), self.muzzlepos, VRGetEntHandPos(self, cVR_MainHand),
            VRGetEntHandRot(self, cVR_MainHand)));
    }
    VR_Test_Check(on && vlen(drawn - server) <= VR_AT_TIP_OFF, sprintf("%s: the drawn bayonet's tip where the server's is (%.2f units off)",
        xWhat, on ? vlen(drawn - server) : -1));
};

// The gun lying or flying whose record is `r`.
entity(entity r) VR_AT_Lying =
{
    for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        if(e.weaponinst == r)
        {
            return e;
        }
    }
    return world;
};

void(float xAsk) VR_AT_Begin =
{
    vr_at_flat = xAsk == 80;
    VR_Test_BeginFlags(vr_at_flat ? "attachflat" : "attach", VR_TEST_LOUD);
    vr_pe_saved_kd = cvar_hget(cvarh_vr_knockdown);
    vr_pe_saved_stag = cvar_hget(cvarh_vr_hit_stagger);
    vr_pe_saved_push = cvar_hget(cvarh_vr_melee_push);
    cvar_set("vr_knockdown", "0");
    cvar_set("vr_hit_stagger", "0");
    cvar_set("vr_melee_push", "0");
    cvar_set("vr_embed", "1");
    cvar_set("vr_embed_fire", "1");
    vr_at_mode = cvar_hget(cvarh_vr_holster_mode);
    cvar_set("vr_holster_mode", ftos(QVR_WEAPONMODE_IMMERSIVE)); // (holstered: the hand's record itself, not a quick slot's copy)
    if(!vr_at_flat)
    {
        localcmd("vr_mock_hand_to_absolute 1\n");
        cvar_set("vr_attach_probe", "1");
    }
    vr_eh_home = VR_Player().origin;
    vr_et_on = xAsk;
    vr_et_case = 0;
    VR_ET_To(0);
    VR_AT_To(0);
};

// The stuck gun's shot (the trigger pressed: from the prop, the grunt hit).
void(string xWhat) VR_AT_ShotCheck =
{
    float shots = vr_pfire_shots - vr_at_count;
    float hurt = vr_et_m != world ? vr_at_health - vr_et_m.health : 0;
    VR_Test_Check(shots >= 1 && hurt > 0, sprintf("%s: fired where it is stuck (shots from the prop %g, the grunt hurt %g)", xWhat, shots, hurt));
};

void(float xOn) VR_AT_Trigger =
{
    if(vr_at_flat)
    {
        localcmd(xOn ? "+attack\n" : "-attack\n");
    }
    else
    {
        localcmd(sprintf("vr_mock_button main trigger %g\n", xOn));
    }
};

void() VR_AT_HoldGrunt =
{
    VR_PE_HoldStill();
    if(vr_et_m != world && vr_et_m.health > 0 && vr_et_m.health < 1000) // (low: a spawned grunt may stand at 4000, and a hit reads against it)
    {
        vr_et_m.health = VR_AT_HEALTH;
    }
};

// ---- vr_embed_test 79 (VR)
void() VR_AT_Frame =
{
    entity p = self;
    float u = time - vr_at_t0;
    float h = cVR_MainHand;
    if(u > VR_ET_GIVEUP && vr_at_s != 99)
    {
        VR_Test_Check(FALSE, sprintf("timed out: state %g/%g", vr_at_s, vr_et_s));
        VR_AT_To(99);
    }
    VR_AT_HoldGrunt();
    if(vr_at_s == 0)
    {
        setorigin(p, vr_eh_home);
        VRSetHandEmpty(p, cVR_OffHand);
        VRSetHandEmpty(p, cVR_MainHand);
        localcmd("+grabmain\n+graboff\n");
        VR_AT_To(1);
    }
    else if(vr_at_s == 1 && u > 0.1 && VR_HandGrabUtil_IsHandGrabbing(p, h) && VR_HandGrabUtil_IsHandGrabbing(p, cVR_OffHand))
    {
        VR_AT_Give(WID_SHOTGUN, FALSE);
        VR_AT_Bayonet(cVR_OffHand);
        vr_at_count = vr_att_attached;
        vr_at_after = 3;
        VR_AT_To(2);
    }
    // The snap (2), then on to vr_at_after.
    else if(vr_at_s == 2 && u > 0.15 && VR_AT_Snap())
    {
        VR_Test_Check(vr_at_rec.wi_attach == VR_ATT_BAYONET && VR_Carry_Held(p, cVR_OffHand) == world && vr_att_attached == vr_at_count + 1,
            sprintf("brought to the shotgun's mount, the bayonet snaps on (attach %g, after %g nudges)", vr_at_rec.wi_attach, vr_at_n));
        VR_AT_To(vr_at_after);
    }
    else if(vr_at_s == 2 && vr_at_n > VR_AT_SNAP_TRIES)
    {
        VR_Test_Check(FALSE, sprintf("the bayonet never snapped on (%.1f units off the mount)", VR_Carry_Held(p, cVR_OffHand) != world ?
            vlen(VR_Carry_Held(p, cVR_OffHand).origin - VR_Attach_HandPoint(p, h, VR_ATT_BAYONET, 1)) : -1));
        VR_AT_To(99);
    }
    else if(vr_at_s == 3 && u > 0.2)
    {
        VR_AT_TipCheck("the shotgun in the hand");
        localcmd("-graboff\nvr_mock_hand_aim off off\n");
        VR_AT_To(4);
    }
    else if(vr_at_s == 4 && u > 0.1)
    {
        localcmd(sprintf("vr_mock_hand_to off %v\n", VR_Attach_HandPoint(p, h, VR_ATT_BAYONET, 1)));
        VR_AT_To(5);
    }
    else if(vr_at_s == 5 && u > 0.15)
    {
        bprint("attach: the pull begins\n"); // (the shell script: no two-handed grip from here to its end)
        vr_at_count = vr_att_detached;
        localcmd("+graboff\n");
        VR_AT_To(6);
    }
    else if(vr_at_s == 6 && (vr_att_detached > vr_at_count || u > 0.5))
    {
        entity c = VR_Carry_Held(p, cVR_OffHand);
        VR_Test_Check(vr_at_rec.wi_attach == 0 && c != world && c.classname == "vr_bayonet",
            sprintf("the empty hand's grip at the mount pulls it off into that hand (attach %g, carried %s)", vr_at_rec.wi_attach,
                c != world ? c.classname : "nothing"));
        VR_AT_To(7);
    }
    else if(vr_at_s == 7 && u > 0.3)
    {
        bprint("attach: the pull ends\n");
        VR_Test_Check(vr_at_rec.wi_attach == 0, "held at the mount just after the pull, it doesn't snap straight back on");
        localcmd("vr_mock_hand_to off by 0 0 -16\n"); // (taken away from the mount first: then it may go on again)
        VR_AT_To(70);
    }
    else if(vr_at_s == 70 && u > 0.3)
    {
        vr_at_count = vr_att_attached;
        vr_at_after = 8;
        VR_AT_To(2); // (on again)
    }
    // Holstered and drawn; dropped from the holster.
    else if(vr_at_s == 8 && u > 0.1)
    {
        float hs = QVR_HS_RIGHT_HIP_HOLSTER;
        setHolsterWeapon(hs, WID_FIST); // (empty: the map's start may have put a gun there)
        setHolsterWeaponModel(hs, WeaponIdToModel(WID_FIST, 0));
        setHolsterWeaponFlags(hs, 0);
        VRSetEntHolsterWeaponInst(p, hs, world);
        VRLetGoAtHolster(p, h, hs, WID_SHOTGUN, 0, TRUE);
        entity hr = VRGetEntHolsterWeaponInst(p, hs);
        VR_Test_Check(getHolsterWeapon(hs) == WID_SHOTGUN && hr == vr_at_rec && hr.wi_attach == VR_ATT_BAYONET,
            sprintf("holstered: the same record, the bayonet on it (attach %g)", hr != world ? hr.wi_attach : -1));
        VRDrawHolsterWeapon(p, h, hs);
        entity dr = VRGetEntWeaponInst(p, h);
        VR_Test_Check(VRGetEntWeapon(p, h) == WID_SHOTGUN && dr == vr_at_rec && dr.wi_attach == VR_ATT_BAYONET,
            sprintf("drawn again: the bayonet on it (attach %g)", dr != world ? dr.wi_attach : -1));
        VRLetGoAtHolster(p, h, hs, WID_SHOTGUN, 0, TRUE);
        VR_FlatHolster_Drop(hs);
        vr_at_w = VR_AT_Lying(vr_at_rec);
        VR_Test_Check(vr_at_w != world && vr_at_rec.wi_attach == VR_ATT_BAYONET, sprintf("dropped: lying with its record, the bayonet on it (%s)",
            vr_at_w != world ? "lying" : "not found"));
        localcmd("-grabmain\n");
        VR_AT_To(9);
    }
    else if(vr_at_s == 9 && u > 1)
    {
        vr_at_n = 0;
        VR_AT_To(10);
    }
    else if(vr_at_s == 10)
    {
        // The open hand onto it first, following it a few frames (it lands short of a point by the floor at first: the
        // mock's note), then the grip pressed there: a press, not a closed hand brought to it.
        vr_at_n = vr_at_n + 1;
        if(vr_at_n % 3 == 1)
        {
            localcmd("vr_mock_hand_to main weaponpalm 0.5\n");
        }
        if(vr_at_n >= 12)
        {
            // (the gun against the hand stood in for: the mock's fist stays a few units over a gun lying on the floor, out
            // of its grip's reach without a bayonet too; vr_pickup_test 17's aid, physics.cpp weaponAgainstHand)
            localcmd("vr_test_handcontact 1 1\n+grabmain\n");
            VR_AT_To(11);
        }
    }
    else if(vr_at_s == 11 && VRGetEntWeaponInst(p, h) != vr_at_rec && u <= 1)
    {
        vr_at_n = vr_at_n + 1; // (waiting for the press's take)
    }
    else if(vr_at_s == 11)
    {
        VR_Test_Check(VRGetEntWeaponInst(p, h) == vr_at_rec && vr_at_rec.wi_attach == VR_ATT_BAYONET,
            sprintf("picked up by the hand: the same record, the bayonet on it (weapon %g)", VRGetEntWeapon(p, h)));
        localcmd("vr_test_handcontact 1 0\n");
        // Thrown at a grunt, point first: stuck by the bayonet's point.
        setorigin(p, vr_eh_home);
        vr_et_m = VR_ET_Grunt(80);
        makevectors('0 1 0' * p.v_viewangle_y);
        vector go = normalize(VR_ET_Chest(vr_et_m) - (p.origin + '0 0 16'));
        VRSetHandEmpty(p, h);
        vr_at_w = CreateThrownWeapon(p, WID_SHOTGUN, 0, vr_at_rec, p.origin + '0 0 16' + go * 24, vectoangles(go), go * 900, '0 0 0',
            FALSE, TRUE, WeaponIdToThrowDamage(WID_SHOTGUN));
        VR_AT_To(12);
    }
    else if(vr_at_s == 12 && (vr_at_w == world || vr_at_w.vr_stick_kind != 0 || u > 1.5))
    {
        VR_Test_Check(vr_at_w != world && VR_AxeStick_IsStuck(vr_at_w) && vr_at_w.vr_stick_embed == AXESTICK_POINT &&
            vr_at_w.weaponinst == vr_at_rec && vr_at_rec.wi_attach == VR_ATT_BAYONET,
            sprintf("thrown point first: stuck by the bayonet's point, the bayonet on it (stick %g, embed %g)", vr_at_w != world ? vr_at_w.vr_stick_kind : -1,
                vr_at_w != world ? vr_at_w.vr_stick_embed : -1));
        if(vr_at_w != world)
        {
            remove(vr_at_w);
        }
        if(vr_et_m != world)
        {
            remove(vr_et_m);
            vr_et_m = world;
        }
        vr_at_wid = WID_SHOTGUN;
        VR_ET_To(0);
        VR_AT_To(20);
    }
    // Stabbed in a grunt (the shotgun, then the super shotgun): stuck, then fired where it is.
    else if(vr_at_s == 20)
    {
        if(vr_et_s == 2 && VRGetEntWeapon(p, h) == vr_at_wid && VRGetEntWeaponInst(p, h).wi_attach == 0)
        {
            VR_AT_Give(vr_at_wid, TRUE); // (VR_EH_Blow's gun: its bayonet on before the aim lays its line)
            vr_et_m.health = VR_AT_HEALTH;
            vr_at_health = VR_AT_HEALTH;
        }
        if(VR_EH_Blow(vr_at_wid, 5, 0, FALSE, time - vr_et_t0))
        {
            vr_at_w = VR_EH_Blade();
            string what = vr_at_wid == WID_SHOTGUN ? "the shotgun" : "the super shotgun";
            float hurt = vr_et_m != world ? vr_at_health - vr_et_m.health : 0;
            VR_Test_Check(vr_at_w != world && hurt > 0 && vr_at_w.vr_stick_embed == AXESTICK_POINT,
                sprintf("%s stabbed with its bayonet: the grunt hurt (%g) and the gun stuck by its point (embed %g)", what, hurt,
                    vr_at_w != world ? vr_at_w.vr_stick_embed : -1));
            VR_Test_Check(vr_at_w != world && vr_at_w.weaponinst.wi_attach == VR_ATT_BAYONET && VR_PropFire_Held(p, h) == vr_at_w,
                sprintf("%s stuck: the bayonet on it, its hand drives it", what));
            if(vr_at_w == world)
            {
                VR_AT_To(29);
            }
            else
            {
                VR_AT_To(21);
            }
        }
    }
    else if(vr_at_s == 21 && u > 0.2)
    {
        vr_at_count = vr_pfire_shots;
        vr_at_health = vr_et_m != world ? vr_et_m.health : 0;
        VRSetAttackFinished(h, 0);
        VR_AT_Trigger(TRUE);
        VR_AT_To(22);
    }
    else if(vr_at_s == 22 && (vr_pfire_shots != vr_at_count || u > 0.5))
    {
        VR_AT_Trigger(FALSE);
        VR_AT_To(23);
    }
    else if(vr_at_s == 23 && u > 0.1)
    {
        VR_AT_ShotCheck(vr_at_wid == WID_SHOTGUN ? "the shotgun" : "the super shotgun");
        VR_AT_To(29);
    }
    else if(vr_at_s == 29)
    {
        VR_EH_NextCase(); // (lets go of it, the hand emptied, the grunt cleared)
        if(vr_at_wid == WID_SHOTGUN)
        {
            vr_at_wid = WID_SUPER_SHOTGUN;
            VR_ET_To(0);
            VR_AT_To(20);
        }
        else
        {
            VR_AT_To(30);
        }
    }
    // The shotgun with its bayonet fired in the hand.
    else if(vr_at_s == 30 && u > 0.2)
    {
        setorigin(p, vr_eh_home);
        localcmd("+grabmain\nvr_mock_hand_aim main off\n");
        VR_AT_To(31);
    }
    else if(vr_at_s == 31 && u > 0.1 && VR_HandGrabUtil_IsHandGrabbing(p, h))
    {
        VR_AT_Give(WID_SHOTGUN, TRUE);
        vr_at_shells = p.ammo_shells + VRGetEntWeaponClip(p, h);
        VR_AT_Trigger(TRUE);
        VR_AT_To(32);
    }
    else if(vr_at_s == 32 && u > 0.1)
    {
        VR_AT_Trigger(FALSE);
        float left = p.ammo_shells + VRGetEntWeaponClip(p, h);
        VR_Test_Check(left == vr_at_shells - 1 && vr_at_rec.wi_attach == VR_ATT_BAYONET,
            sprintf("the shotgun with its bayonet fires in the hand (%g shell spent)", vr_at_shells - left));
        VR_AT_To(99);
    }
    else if(vr_at_s == 99)
    {
        VR_AT_Finish();
    }
};

// ---- vr_embed_test 80 (flat-screen)
void() VR_AT_FlatFrame =
{
    entity p = self;
    float u = time - vr_at_t0;
    float h = cVR_MainHand;
    entity w = VR_EH_Blade();
    if(u > VR_ET_GIVEUP && vr_at_s != 99)
    {
        VR_Test_Check(FALSE, sprintf("timed out: state %g", vr_at_s));
        VR_AT_To(99);
    }
    VR_AT_HoldGrunt();
    if(vr_at_s == 0)
    {
        setorigin(p, vr_eh_home - '0 40 0');
        p.velocity = '0 0 0';
        VRSetHandEmpty(p, cVR_OffHand);
        VRSetHandEmpty(p, h);
        VR_AT_Give(WID_SHOTGUN, FALSE);
        VR_AT_Bayonet(cVR_OffHand);
        vr_at_count = vr_att_attached;
        localcmd("impulse 52\n"); // (J: vr_flatplay.cpp sends the same impulse; weapons.qc runs VR_Attach_Key)
        VR_AT_To(1);
    }
    else if(vr_at_s == 1 && u > 0.1)
    {
        VR_Test_Check(vr_at_rec.wi_attach == VR_ATT_BAYONET && VR_Carry_Held(p, cVR_OffHand) == world && vr_att_attached == vr_at_count + 1,
            "J: the bayonet in the off hand goes on the shotgun");
        localcmd("impulse 52\n");
        VR_AT_To(2);
    }
    else if(vr_at_s == 2 && u > 0.1)
    {
        entity c = VR_Carry_Held(p, cVR_OffHand);
        VR_Test_Check(vr_at_rec.wi_attach == 0 && c != world && c.classname == "vr_bayonet", "J again: taken off into the empty off hand");
        localcmd("impulse 52\n");
        VR_AT_To(3);
    }
    else if(vr_at_s == 3 && u > 0.1)
    {
        VR_Test_Check(vr_at_rec.wi_attach == VR_ATT_BAYONET, "J: on again");
        vr_at_wid = WID_SHOTGUN;
        VR_AT_To(10);
    }
    // Stuck by V's thrust (the shotgun, then the super shotgun), fired there, reloaded there.
    else if(vr_at_s == 10 && u > 0.2)
    {
        if(vr_at_wid == WID_SUPER_SHOTGUN)
        {
            VRSetHandEmpty(p, cVR_OffHand);
            VR_AT_Give(WID_SUPER_SHOTGUN, TRUE);
        }
        VR_PE_Grunt(42);
        vr_et_m.health = VR_AT_HEALTH;
        vr_at_health = VR_AT_HEALTH;
        VR_AT_To(11);
    }
    else if(vr_at_s == 11 && u > 0.6)
    {
        vr_at_health = vr_et_m != world ? vr_et_m.health : 0; // (now: the spawned grunt's own setup may have changed it since)
        localcmd("vr_flat_punch v\n");
        VR_AT_To(12);
    }
    else if(vr_at_s == 12 && (w != world || u > 0.9))
    {
        string what = vr_at_wid == WID_SHOTGUN ? "the shotgun" : "the super shotgun";
        float hurt = vr_et_m != world ? vr_at_health - vr_et_m.health : 0;
        VR_Test_Check(w != world && hurt > 0 && VR_PropFire_Held(p, h) == w, sprintf("V: %s's bayonet thrust in: hurt %g, stuck, the hand drives it", what, hurt));
        vr_at_w = w;
        VR_AT_To(w != world ? 13 : 29);
    }
    else if(vr_at_s == 13 && u > 0.2)
    {
        vr_at_count = vr_pfire_shots;
        vr_at_health = vr_et_m != world ? vr_et_m.health : 0;
        vr_at_shells = vr_at_rec.wi_clip;
        VRSetAttackFinished(h, 0);
        VR_AT_Trigger(TRUE);
        VR_AT_To(14);
    }
    else if(vr_at_s == 14 && (vr_pfire_shots != vr_at_count || u > 0.5))
    {
        VR_AT_Trigger(FALSE);
        VR_AT_To(15);
    }
    else if(vr_at_s == 15 && u > 0.1)
    {
        string what2 = vr_at_wid == WID_SHOTGUN ? "the shotgun" : "the super shotgun";
        VR_AT_ShotCheck(what2);
        VR_Test_Check(w == vr_at_w && w != world, sprintf("%s: still stuck after its shot (no pull)", what2));
        if(vr_at_wid == WID_SUPER_SHOTGUN && w != world)
        {
            // (the other barrel: fired empty, it breaks open where it is)
            vr_at_count = vr_pfire_shots;
            VRSetAttackFinished(h, 0);
            VR_AT_Trigger(TRUE);
            VR_AT_To(16);
        }
        else
        {
            VR_AT_To(18);
        }
    }
    else if(vr_at_s == 16 && (vr_pfire_shots != vr_at_count || u > 0.5))
    {
        VR_AT_Trigger(FALSE);
        VR_AT_To(17);
    }
    else if(vr_at_s == 17 && ((w != world && (w.weaponflags & QVR_WPNFLAG_SSG_OPEN)) || u > 1.5))
    {
        VR_Test_Check(w != world && (w.weaponflags & QVR_WPNFLAG_SSG_OPEN) && vr_at_rec.wi_clip == 0,
            sprintf("the super shotgun fired empty while stuck breaks open there (clip %g)", vr_at_rec.wi_clip));
        VR_AT_To(18);
    }
    else if(vr_at_s == 18 && u > 0.1)
    {
        vr_at_shells = vr_at_rec.wi_clip;
        localcmd("impulse 55\n");
        VR_AT_To(19);
    }
    else if(vr_at_s == 19 && ((vr_at_rec.wi_clip > vr_at_shells && !(p.vr_flatrl & 2)) || u > 3))
    {
        string what3 = vr_at_wid == WID_SHOTGUN ? "the shotgun" : "the super shotgun";
        VR_Test_Check(vr_at_rec.wi_clip > vr_at_shells && w == vr_at_w && w != world,
            sprintf("R: %s reloaded where it is stuck (clip %g -> %g), still stuck", what3, vr_at_shells, vr_at_rec.wi_clip));
        if(vr_at_wid == WID_SUPER_SHOTGUN)
        {
            VR_Test_Check(w != world && !(w.weaponflags & QVR_WPNFLAG_SSG_OPEN), "and shut again");
        }
        VR_AT_To(28);
    }
    else if(vr_at_s == 28 && u > 0.3)
    {
        VR_Test_Check(w == vr_at_w && w != world && VR_PropFire_Held(p, h) == w, "and still stuck, the hand on it, after the reload");
        VR_AT_To(29);
    }
    else if(vr_at_s == 29)
    {
        if(w != world)
        {
            VR_Embed_DropHands(w, "test");
            remove(w);
        }
        if(vr_et_m != world)
        {
            remove(vr_et_m);
            vr_et_m = world;
        }
        if(vr_at_wid == WID_SHOTGUN)
        {
            vr_at_wid = WID_SUPER_SHOTGUN;
            VR_AT_To(10);
        }
        else
        {
            VR_AT_To(99);
        }
    }
    else if(vr_at_s == 99)
    {
        VR_AT_Finish();
    }
};

// ---- vr_embed_test 81 / 82 (persistence)
void() VR_AT_PersistSet =
{
    entity p = VR_Player(); // (StartFrame runs the tests: self is whatever ran last, after a level change not the player)
    entity oldself = self;
    self = p;
    VRGiveEntWeapon(p, cVR_MainHand, WID_SHOTGUN, 0, 0);
    if(VRGetEntWeaponInst(p, cVR_MainHand) == world)
    {
        VRSetEntWeaponInst(p, cVR_MainHand, WeaponInst_New(0));
    }
    VRGetEntWeaponInst(p, cVR_MainHand).wi_attach = VR_ATT_BAYONET;
    p.items = p.items | IT_SHOTGUN | IT_SUPER_SHOTGUN;
    p.ammo_shells = max(p.ammo_shells, 20);
    float hs = QVR_HS_LEFT_HIP_HOLSTER;
    setHolsterWeapon(hs, WID_SUPER_SHOTGUN);
    setHolsterWeaponModel(hs, WeaponIdToModel(WID_SUPER_SHOTGUN, 0));
    setHolsterWeaponFlags(hs, 0);
    entity r = WeaponInst_New(0);
    r.wi_attach = VR_ATT_BAYONET;
    VRSetEntHolsterWeaponInst(p, hs, r);
    W_SetCurrentAmmoFor(cVR_MainHand);
    bprint("attachpersist: set\n");
    self = oldself;
};

void() VR_AT_PersistCheck =
{
    entity p = VR_Player();
    entity oldself = self;
    self = p; // (getHolsterWeapon reads self)
    VR_Test_BeginFlags("attachpersist", VR_TEST_LOUD);
    entity m = VRGetEntWeaponInst(p, cVR_MainHand);
    VR_Test_Check(VRGetEntWeapon(p, cVR_MainHand) == WID_SHOTGUN && m != world && m.wi_attach == VR_ATT_BAYONET,
        sprintf("the shotgun in the hand still has its bayonet (weapon %g, attach %g)", VRGetEntWeapon(p, cVR_MainHand), m != world ? m.wi_attach : -1));
    float hs = QVR_HS_LEFT_HIP_HOLSTER;
    entity r = VRGetEntHolsterWeaponInst(p, hs);
    VR_Test_Check(getHolsterWeapon(hs) == WID_SUPER_SHOTGUN && r != world && r.wi_attach == VR_ATT_BAYONET,
        sprintf("the super shotgun in the left hip holster still has its bayonet (weapon %g, attach %g)", getHolsterWeapon(hs), r != world ? r.wi_attach : -1));
    VR_Test_Verdict();
    self = oldself;
};
#endif
