#ifdef QVR_TESTS
// vr_attach_test.qc -- attach2: weapon attachments, the bayonet on the shotgun and the super shotgun (QC vr_attach.qc; docs/vr-port/
// ATTACHMENTS.md, "The system"). Map vrtesthall, god, notarget, on your feet in the open. Misc/quakevr/attach_test.sh runs them.
// `vr_embed_test 79` (VR, the mock headset; vr_attach_probe 1): the shotgun in the main hand, a bayonet carried in the off hand:
//   brought to the gun's mount it snaps on (the record's .wi_attach, the carried bayonet gone); the drawn bayonet's tip where
//   the server's is (VR_Attach_TipOf: the gun's Pitch/Yaw/Roll in the hand counted); the empty off hand's grip at the mount
//   alone leaves it on, its B/Y then pulls it off into that hand (bayonet2); on again; holstered and drawn, dropped and
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
float VR_AT_SHOT_PUSH = 60;   // bayonet2: u/s at most a stuck gun's shot may shove the grunt it is in
float vr_at_push;             // the most the stuck gun's shot shoved it (u/s, across the floor)

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
        localcmd("vr_mock_button off secondary 0\nvr_mock_hand_to_absolute 0\n");
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
        // (bayonet2: held In the Palm it may lie reversed in the hand, pointing back up the arm, where adding the difference
        // never turns it round: the aim is turned by the turn that takes the bayonet's way onto the mounted way.)
        vector bf = v_forward;
        vector k = VR_Cross(bf, want);
        if(vlen(k) < 0.01)
        {
            k = VR_Cross(bf, '0 0 1');
        }
        k = normalize(k);
        float cosA = max(-1, min(1, bf * want));
        float sinA = sqrt(max(0, 1 - cosA * cosA));
        makevectors(VRGetEntHandRot(p, cVR_OffHand));
        vector a0 = vr_at_n == 0 ? v_forward : vr_at_aim;
        vr_at_aim = normalize(a0 * cosA + VR_Cross(k, a0) * sinA + k * ((k * a0) * (1 - cosA)));
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

// The HUD while the hand drives the stuck gun (bayonetfix; his flat note 2026-11-27: the ammo count went away): the server's
// readouts are the gun's (the engine's STAT_QVR_PROPHUD from .vr_pfire_hud, the counter, the ammo type, the clip size);
// the client's (vr_flathud_report after the marker: the shell script) show it.
void(string xWhat) VR_AT_HudCheck =
{
    entity p = self;
    float aid = WeaponIdToAmmoId(vr_at_wid, 0);
    float main = floor(p.vr_pfire_hud / 256);
    VR_Test_Check(main == vr_at_wid && p.currentammo == aid && p.ammocounter == VR_AmmoUtil_GetAmmoCountByAmmoId(p, aid) &&
        p.ammocounter > 0 && p.weaponclipsize == VR_ClipSizeOf(vr_at_wid, vr_at_rec) && VRIsHandWeaponless(p, cVR_MainHand),
        sprintf("%s stuck: the HUD shows its ammo (prop weapon %g, ammo type %g, counter %g, clip size %g, its clip %g)", xWhat, main,
            p.currentammo, p.ammocounter, p.weaponclipsize, vr_at_rec.wi_clip));
    bprint(sprintf("%s: hud probe: weapon %g clip %g\n", vr_at_flat ? "attachflat" : "attach", vr_at_wid, vr_at_rec.wi_clip));
    localcmd("vr_flathud_report\n");
};

// The bayonet just taken off into the off hand: the client's report of what that hand is drawn holding (the shell script).
void() VR_AT_HeldProbe =
{
    entity c = VR_Carry_Held(self, cVR_OffHand);
    bprint(sprintf("%s: held probe: the off hand carries %s\n", vr_at_flat ? "attachflat" : "attach", c != world ? c.classname : "nothing"));
    localcmd("vr_held_report\n");
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
    // (bayonet2: the shove the stuck gun's shot gave the grunt, read before it is stilled.)
    if((vr_at_s == 22 || vr_at_s == 23) && vr_et_m != world)
    {
        vr_at_push = max(vr_at_push, vlen([vr_et_m.velocity_x, vr_et_m.velocity_y, 0]));
    }
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
        bprint("attach: the pull begins\n");
        vr_at_count = vr_att_detached;
        localcmd("+graboff\n");
        VR_AT_To(60);
    }
    // bayonet2 (his VR note 2026-11-27: a grip alone pulled it off, against the shotguns' two-handed grips): the grip at the
    // mount alone leaves it on (vr_attach_pull_button); the grip held and B/Y pressed pulls it off, as the head torch comes off.
    else if(vr_at_s == 60 && (vr_att_detached > vr_at_count || u > 0.3))
    {
        VR_Test_Check(vr_at_rec.wi_attach == VR_ATT_BAYONET && vr_att_detached == vr_at_count,
            sprintf("the empty hand's grip at the mount alone leaves the bayonet on (attach %g)", vr_at_rec.wi_attach));
        localcmd("vr_mock_button off secondary 1\n");
        VR_AT_To(6);
    }
    else if(vr_at_s == 6 && (vr_att_detached > vr_at_count || u > 0.5))
    {
        localcmd("vr_mock_button off secondary 0\n");
        entity c = VR_Carry_Held(p, cVR_OffHand);
        VR_Test_Check(vr_at_rec.wi_attach == 0 && c != world && c.classname == "vr_bayonet",
            sprintf("gripping at the mount, its B/Y pulls it off into that hand (attach %g, carried %s)", vr_at_rec.wi_attach,
                c != world ? c.classname : "nothing"));
        VR_AT_To(7);
    }
    else if(vr_at_s == 7 && u > 0.3)
    {
        bprint("attach: the pull ends\n");
        VR_AT_HeldProbe();
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
        // Thrown at a grunt, point first: stuck by the bayonet's point. (From 160 units back: at home the grunt stands on the
        // hall's first mantle block, 32 high from y 352, and the gun's stock struck its face first: attach4.)
        setorigin(p, vr_eh_home - '0 160 0');
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
        float thrownStuck = vr_at_w != world && VR_AxeStick_IsStuck(vr_at_w) && vr_at_w.vr_stick_embed == AXESTICK_POINT &&
            vr_at_w.weaponinst == vr_at_rec && vr_at_rec.wi_attach == VR_ATT_BAYONET;
        string thrownWhat = sprintf("thrown point first: stuck by the bayonet's point, the bayonet on it (stick %g, embed %g)",
            vr_at_w != world ? vr_at_w.vr_stick_kind : -1, vr_at_w != world ? vr_at_w.vr_stick_embed : -1);
        VR_Test_Check(thrownStuck, thrownWhat); // (its Box3D body has the bayonet's hull: vr_box3d_mesh.cpp attachmentHull)
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
        localcmd("-graboff\n"); // (the stabs one-handed: the off hand resting by the gun takes no two-handed grip on it)
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
        VR_AT_HudCheck(vr_at_wid == WID_SHOTGUN ? "the shotgun" : "the super shotgun");
        vr_at_count = vr_pfire_shots;
        vr_at_health = vr_et_m != world ? vr_et_m.health : 0;
        vr_at_push = 0;
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
        string sw = vr_at_wid == WID_SHOTGUN ? "the shotgun" : "the super shotgun";
        VR_AT_ShotCheck(sw);
        // bayonet2 (his VR note 2026-11-27: a gun stuck by its bayonet and fired almost always lost the grip): its shot barely
        // shoves the grunt it is in (read before the test's hold stills it; Weapon Hits' full push, 144 u/s for the shotgun's
        // 24 and 336 for the super shotgun's 56, took the body out of the arm's reach: vr_embed_fire_push), and the hand on
        // it is given the firing margin (vr_embed_fire_letgo for vr_embed_fire_hold_time: the shot's flinch carries it).
        VR_Test_Check(vr_at_push <= VR_AT_SHOT_PUSH, sprintf("%s stuck: its shot barely shoves the grunt it is in (%.0f u/s, at most %g)",
            sw, vr_at_push, VR_AT_SHOT_PUSH));
        float margin = vr_at_w != world ? VR_Embed_FireMargin(vr_at_w) : -1;
        VR_Test_Check(margin == max(0, cvar_hget(cvarh_vr_embed_fire_letgo)) && VR_PropFire_Held(p, h) == vr_at_w,
            sprintf("%s stuck and fired: its hand still on it, with %g cm more to let go", sw, margin));
        localcmd("+graboff\n"); // (the off hand's grip again: it holds the round it brings)
        VR_AT_To((vr_at_w != world && VR_Reload_Immersive()) ? 24 : 29);
    }
    // Reloaded where it is stuck (attach4), its hand still on it: the super shotgun broken open by its B (the hand's frame
    // on the stuck gun: vr_reload.qc VR_Reload_SsgFrameWhere), then the off hand takes a round from the pouch and brings
    // it to the stuck gun's load point (the lying guns' path): it goes in, and the gun stays stuck.
    else if(vr_at_s == 24 && u > 0.1)
    {
        if(vr_at_wid == WID_SUPER_SHOTGUN && !(vr_at_w.weaponflags & QVR_WPNFLAG_SSG_OPEN))
        {
            localcmd("vr_mock_button main secondary 1\n");
            VR_AT_To(25);
        }
        else
        {
            VR_AT_To(26);
        }
    }
    else if(vr_at_s == 25 && ((vr_at_w.weaponflags & QVR_WPNFLAG_SSG_OPEN) || u > 0.5))
    {
        localcmd("vr_mock_button main secondary 0\n");
        VR_Test_Check(vr_at_w.weaponflags & QVR_WPNFLAG_SSG_OPEN, sprintf("the super shotgun stuck: broken open where it is by "
            "its B (flags %g, still stuck %g)", vr_at_w.weaponflags, VR_AxeStick_IsStuck(vr_at_w)));
        VR_AT_To(26);
    }
    else if(vr_at_s == 26 && u > 0.4)
    {
        vr_at_count = vr_at_w.weaponinst.wi_clip;
        if(VR_Carry_Held(p, cVR_OffHand) == world)
        {
            VR_Reload_Take(cVR_OffHand);
        }
        vr_at_n = 0;
        VR_AT_To(27);
    }
    else if(vr_at_s == 27 && vr_at_w.weaponinst.wi_clip == vr_at_count && u < 1.5)
    {
        // The off hand brought (every third frame, the mock's move from the hand's place) so its round's load end is at the
        // stuck gun's load point as it lies.
        vr_at_n = vr_at_n + 1;
        entity rd = VR_Carry_Held(p, cVR_OffHand);
        float ssgOpen = vr_at_wid == WID_SUPER_SHOTGUN && (vr_at_w.weaponflags & QVR_WPNFLAG_SSG_OPEN);
        if(vr_at_n % 3 == 1 && rd != world && loadportof(vr_at_w, ssgOpen ? cvar_hget(cvarh_vr_reload_ssg_open_angle) : 0))
        {
            vector to = vr_at_w.loadportpos + (VRGetEntHandPos(p, cVR_OffHand) - VR_Reload_RoundRef(rd));
            localcmd(sprintf("vr_mock_hand_to off %g %g %g\n", to_x, to_y, to_z));
        }
    }
    else if(vr_at_s == 27)
    {
        string what = vr_at_wid == WID_SHOTGUN ? "the shotgun" : "the super shotgun";
        VR_Test_Check(vr_at_w.weaponinst.wi_clip > vr_at_count && VR_AxeStick_IsStuck(vr_at_w) && VR_PropFire_Held(p, h) == vr_at_w,
            sprintf("%s stuck: the off hand's round goes in where it is (clip %g -> %g), still stuck %g, its hand on it %g", what,
                vr_at_count, vr_at_w.weaponinst.wi_clip, VR_AxeStick_IsStuck(vr_at_w), VR_PropFire_Held(p, h) == vr_at_w));
        if(VR_Carry_Held(p, cVR_OffHand) != world && VR_Reload_IsRound(VR_Carry_Held(p, cVR_OffHand)))
        {
            VR_Reload_Refund(p, VR_Carry_Held(p, cVR_OffHand), cVR_OffHand); // (a pair's other shell)
        }
        localcmd("vr_mock_hand off -0.15 1.2 -0.45 70 0 0\n");
        VR_AT_To(29);
    }
    else if(vr_at_s == 29)
    {
        VR_EH_NextCase(); // (lets go of it, the hand emptied, the grunt cleared)
        if(vr_at_wid == WID_SHOTGUN)
        {
            vr_at_wid = WID_SUPER_SHOTGUN;
            localcmd("-graboff\n");
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
        VR_AT_To(5);
    }
    else if(vr_at_s == 5 && u > 0.3)
    {
        VR_AT_HeldProbe(); // (eased into the hand by now: drawn there, blade up, on the screen)
        VR_AT_To(6);
    }
    else if(vr_at_s == 6 && u > 0.1)
    {
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
        VR_AT_HudCheck(vr_at_wid == WID_SHOTGUN ? "the shotgun" : "the super shotgun");
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

// attach5 (mp_test.py attach): the server's side of the multiplayer test, one-shot asks (the cvar set back to 0 after):
//   vr_dm_test 25: a bayonet spawned into the empty off hand of the player whose entity number is vr_dm_test_who
//     (coop's test impulses are off for clients, deathmatch has none); `attachmp: bayonet in player N's off hand`.
//   vr_dm_test 27: player vr_dm_test_who put at vr_dm_test_x/y/z, facing the nearest other living player (deathmatch has
//     no setpos); `attachmp: player N put at ...`.
//   vr_dm_test 26: every player's carried records' attachments and every thrown weapon's (dropped, thrown, stuck):
//     `attachmp: player ent=N main=<wid>,<attach> off=<wid>,<attach> holsters=a,b,c,d,e,f`, `attachmp: lying wid=<wid>
//     attach=<kind> stuck=<0|1> speed=<u/s> at=<x y z>`, the live monsters within 160 units of a player (`attachmp:
//     monster`), then `attachmp: report done`.
// (An entity's number: its place among the used edicts, exact for players, whose edicts are never freed.)
float(entity e) VR_AT_EntNum =
{
    float n = 0;
    for(entity f = world; f != e && n < 8192; f = nextent(f))
    {
        n++;
    }
    return n;
};

void() VR_AttachMPTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_dm_test);
    if(ask == 25)
    {
        cvar_set("vr_dm_test", "0");
        float who = cvar_hget(cvarh_vr_dm_test_who);
        entity oldself = self;
        for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
        {
            if(VR_AT_EntNum(p) == who && p.health > 0 && VRIsHandEmpty(p, cVR_OffHand) && VR_Carry_Held(p, cVR_OffHand) == world)
            {
                self = p;
                VR_AT_Bayonet(cVR_OffHand);
                self = oldself;
                bprint(sprintf("attachmp: bayonet in player %g's off hand\n", who));
                return;
            }
        }
        bprint(sprintf("attachmp: no player %g with an empty off hand\n", who));
    }
    if(ask == 27)
    {
        cvar_set("vr_dm_test", "0");
        float mover = cvar_hget(cvarh_vr_dm_test_who);
        vector to = [cvar_hget(cvarh_vr_dm_test_x), cvar_hget(cvarh_vr_dm_test_y), cvar_hget(cvarh_vr_dm_test_z)];
        for(entity pm = VR_Player(); pm; pm = VR_NextPlayer(pm))
        {
            if(VR_AT_EntNum(pm) == mover && pm.health > 0)
            {
                entity near = world;
                for(entity po = VR_Player(); po; po = VR_NextPlayer(po))
                {
                    if(po != pm && po.health > 0 && (near == world || vlen(po.origin - to) < vlen(near.origin - to)))
                    {
                        near = po;
                    }
                }
                setorigin(pm, to);
                pm.velocity = '0 0 0';
                pm.flags = pm.flags - (pm.flags & FL_ONGROUND);
                if(near != world)
                {
                    vector look = near.origin - to;
                    look_z = 0;
                    pm.v_angle = pm.angles = vectoangles(look);
                    pm.fixangle = TRUE;
                }
                bprint(sprintf("attachmp: player %g put at %v\n", mover, to));
                return;
            }
        }
        bprint(sprintf("attachmp: no player %g to put\n", mover));
    }
    if(ask == 26)
    {
        cvar_set("vr_dm_test", "0");
        for(entity q = VR_Player(); q; q = VR_NextPlayer(q))
        {
            string hol = "";
            for(float i = 2; i < 8; i++)
            {
                hol = sprintf("%s%s%g", hol, i > 2 ? "," : "", VR_Attach_Of(VR_Attach_Carried(q, i)));
            }
            bprint(sprintf("attachmp: player ent=%g main=%g,%g off=%g,%g holsters=%s\n", VR_AT_EntNum(q), VRGetEntWeapon(q, cVR_MainHand),
                VR_Attach_Of(VRGetEntWeaponInst(q, cVR_MainHand)), VRGetEntWeapon(q, cVR_OffHand),
                VR_Attach_Of(VRGetEntWeaponInst(q, cVR_OffHand)), hol));
        }
        for(entity w = find(world, classname, "thrown_weapon"); w; w = find(w, classname, "thrown_weapon"))
        {
            bprint(sprintf("attachmp: lying wid=%g attach=%g stuck=%g speed=%.0f at=%.0f %.0f %.0f\n", w.weapon,
                VR_Attach_Of(w.weaponinst), VR_AxeStick_IsStuck(w) ? 1 : 0, vlen(w.velocity), w.origin_x, w.origin_y, w.origin_z));
        }
        for(entity mo = nextent(world); mo; mo = nextent(mo))
        {
            if((mo.flags & FL_MONSTER) && mo.health > 0)
            {
                for(entity pp = VR_Player(); pp; pp = VR_NextPlayer(pp))
                {
                    if(vlen(mo.origin - pp.origin) < 160)
                    {
                        bprint(sprintf("attachmp: monster ent=%g class=%s health=%g\n", VR_AT_EntNum(mo), mo.classname, mo.health));
                        break;
                    }
                }
            }
        }
        bprint("attachmp: report done\n");
    }
};
#endif
