#ifdef QVR_TESTS
// vr_playerdisarm_test.qc -- the player disarmed (vr_playerdisarm.qc; ROUND25.md, "The player disarmed"), with the mock
// headset. `vr_pd_test 1`: the shotgun held out in the main hand (a fist in the other), vr_player_disarm_chance 100
// (certain): a grunt's full blast across the gun (two jolts) knocks it out: it drops, settles on the floor, a world weapon
// with its clip; a blast across the body: no disarm; an enforcer's laser across the gun: the warning (one jolt), a second
// knocks it out; the option off: never. `vr_pd_test 3`: PvE's rules by blow type and strength (below).
// Ends with `pdtest: N passed, M failed`.

float vr_pdt_on;     // the running test's number
float vr_pdt_s;      // its state
float vr_pdt_t0;     // when the state began (a stuck test: FAIL "timed out")
float vr_pdt_wait;   // frames left before the state goes on
float vr_pdt_n;      // frames the state has run
float vr_pdt_dis0;   // vr_pd_disarms at the case's start
float vr_pdt_hits0;  // vr_pd_hits then
float vr_pdt_chance; // vr_player_disarm_chance before the test
float vr_pdt_pve;    // vr_player_disarm (PvE) before the test
float vr_pdt_jolts;  // vr_player_disarm_jolts before the test
float vr_pdt_window; // vr_player_disarm_window before the test
float vr_pdt_warn0;  // vr_pd_warns at the case's start
entity vr_pdt_shooter; // the case's shooter (a stand-in grunt or enforcer)
entity vr_pdt_drop;  // the knocked-out weapon
entity vr_pdt_laser; // the laser case's bolt

float VR_PDT_GIVEUP = 6;      // seconds a state may last
float VR_PDT_POSE_FRAMES = 6; // frames for a mock pose and a weapon given to be in place
float VR_PDT_CLIP = 5;        // the shotgun's clip (kept by the dropped weapon)
string VR_PDT_POSE = "vr_mock_hand main 0.2 1.2 -0.4 70 0 0; vr_mock_hand off -0.25 1.0 -0.2 70 0 0";

void(float s, float frames) VR_PDT_To =
{
    vr_pdt_s = s;
    vr_pdt_wait = frames;
    vr_pdt_t0 = time;
    vr_pdt_n = 0;
};

void() VR_PDT_End =
{
    cvar_set("vr_player_disarm", ftos(vr_pdt_pve)); // (PvE as it was: off by default)
    cvar_set("vr_player_disarm_chance", ftos(vr_pdt_chance));
    cvar_set("vr_player_disarm_jolts", ftos(vr_pdt_jolts));
    cvar_set("vr_player_disarm_window", ftos(vr_pdt_window));
    if(vr_pdt_shooter)
    {
        remove(vr_pdt_shooter);
        vr_pdt_shooter = world;
    }
    localcmd("vr_mock_hand main; vr_mock_hand off\n");
    VR_Test_Verdict();
    vr_pdt_on = 0;
    cvar_set("vr_pd_test", "0");
};

void(entity p) VR_PDT_Arm =
{
    VRGiveEntWeapon(p, cVR_MainHand, WID_SHOTGUN, 0, VR_PDT_CLIP);
    VRGiveEntWeapon(p, cVR_OffHand, WID_FIST, 0, 0);
    W_SetCurrentAmmoFor(cVR_MainHand);
    VR_Scene_HoldGiven(p, cVR_MainHand); // (held with the mock's grip unpressed)
    p.health = 100;
    vr_pdt_dis0 = vr_pd_disarms;
    vr_pdt_hits0 = vr_pd_hits;
    vr_pdt_warn0 = vr_pd_warns;
    for(float h = 0; h <= 1; h++) // (a fresh grip: no jolts kept from the case before)
    {
        entity st = h == cVR_MainHand ? p.vr_pd_main : p.vr_pd_off;
        if(st)
        {
            st.vr_pd_jolts = 0;
            st.vr_pd_jnew = 0;
            st.vr_pd_sum = 0;
        }
    }
};

// A stand-in shooter (never thinks: the test fires for it).
entity(string cls) VR_PDT_Shooter =
{
    if(vr_pdt_shooter)
    {
        remove(vr_pdt_shooter);
    }
    vr_pdt_shooter = spawn();
    vr_pdt_shooter.classname = cls;
    vr_pdt_shooter.flags = FL_MONSTER;
    vr_pdt_shooter.takedamage = DAMAGE_AIM;
    vr_pdt_shooter.health = 100;
    return vr_pdt_shooter;
};

// Across the main hand's gun from its right (or `xLow`: across the body, at its middle, under the gun), 200 units off.
vector vr_pdt_from;
vector vr_pdt_dir;
void(entity p, float xLow) VR_PDT_Aim =
{
    VR_Parry_WeaponLine(p, cVR_MainHand);
    vector mid = vr_parry_butt + (vr_parry_end - vr_parry_butt) * 0.75; // (out ahead of your box: the laser's touch)
    vector fwd = vr_parry_end - vr_parry_butt;
    fwd_z = 0;
    fwd = normalize(fwd);
    vector side;
    side_x = fwd_y;
    side_y = -fwd_x;
    side_z = 0;
    vector at = xLow ? p.origin : mid;
    traceline(at, at + side * 200, TRUE, p); // (never inside a wall)
    vr_pdt_from = trace_endpos - side * 8;
    vr_pdt_dir = normalize(at - vr_pdt_from);
};

void(entity p) VR_PDT_Blast =
{
    entity oldSelf = self;
    self = VR_PDT_Shooter("monster_army");
    setorigin(self, vr_pdt_from);
    FireBulletsImpl(6, vr_pdt_dir, '0 0 0', vr_pdt_from, 2048, FALSE);
    self = oldSelf;
};

// The enforcer's laser case's bolt, across the gun (VR_PDT_Aim first).
void(entity p) VR_PDT_Laser =
{
    entity oldSelf = self;
    self = VR_PDT_Shooter("monster_enforcer");
    setorigin(self, vr_pdt_from);
    vr_pdt_laser = LaunchLaser(vr_pdt_from, vr_pdt_dir);
    vr_pdt_laser.velocity = vr_pdt_dir * 600; // (straight at it: no aim cone)
    self = oldSelf;
};

void(entity p) VR_PDT_Run =
{
    vr_pdt_n = vr_pdt_n + 1;
    if(vr_pdt_s == 0)
    {
        VR_PDT_Arm(p);
        VR_PDT_To(1, VR_PDT_POSE_FRAMES);
        return;
    }
    if(vr_pdt_s == 1)
    {
        // A grunt's blast across the gun.
        VR_Test_Check(VR_Parry_WeaponLine(p, cVR_MainHand), "the shotgun held out in the main hand: its line");
        VR_PDT_Aim(p, FALSE);
        VR_PDT_Blast(p);
        VR_Test_Check(vr_pd_hits > vr_pdt_hits0, sprintf("a blast across the gun: hits counted on it (%g)", vr_pd_hits - vr_pdt_hits0));
        VR_PDT_To(2, 2); // (the roll: the next PlayerPostThink)
        return;
    }
    if(vr_pdt_s == 2)
    {
        VR_Test_Check(vr_pd_disarms == vr_pdt_dis0 + 1 && vr_pd_last_hand == cVR_MainHand && vr_pd_warns == vr_pdt_warn0,
            sprintf("a full blast across the gun (two jolts at once): knocked out of the main hand (%g), no warning first (%g)",
            vr_pd_disarms - vr_pdt_dis0, vr_pd_warns - vr_pdt_warn0));
        VR_Test_Check(VRIsHandWeaponless(p, cVR_MainHand), "the main hand empty");
        VR_Test_Check(vr_pd_sounds == vr_pd_disarms, sprintf("its sounds started (the clang, the sting): %g of %g disarms",
            vr_pd_sounds, vr_pd_disarms));
        vr_pdt_drop = vr_wpn_dropped;
        VR_Test_Check(vr_pdt_drop != world && vr_pdt_drop.classname == "thrown_weapon" && vr_pdt_drop.weapon == WID_SHOTGUN &&
            vr_pdt_drop.handtouch == wpnthrow_handtouch,
            sprintf("dropped as a world weapon you can take: %s, weapon %g", vr_pdt_drop.classname, vr_pdt_drop.weapon));
        VR_Test_Check(vr_pdt_drop.weaponinst.wi_clip == VR_PDT_CLIP,
            sprintf("its clip kept: %g (was %g)", vr_pdt_drop.weaponinst.wi_clip, VR_PDT_CLIP));
        VR_PDT_To(3, 0);
        return;
    }
    if(vr_pdt_s == 3)
    {
        // Settled: still for 10 frames (a frame's cap: the give-up).
        if(vr_pdt_drop == world || vlen(vr_pdt_drop.velocity) > 1)
        {
            vr_pdt_n = 0;
            return;
        }
        if(vr_pdt_n < 10)
        {
            return;
        }
        traceline(vr_pdt_drop.origin, vr_pdt_drop.origin - '0 0 32', TRUE, vr_pdt_drop);
        VR_Test_Check(trace_fraction < 1 && vr_pdt_drop.origin_z - trace_endpos_z < 16 && vr_pdt_drop.origin_z < p.origin_z,
            sprintf("it lies on the floor: %.1f units over it (you at %.1f, it at %.1f)", vr_pdt_drop.origin_z - trace_endpos_z,
            p.origin_z, vr_pdt_drop.origin_z));
        remove(vr_pdt_drop);
        vr_pdt_drop = world;
        VR_PDT_Arm(p);
        VR_PDT_To(4, VR_PDT_POSE_FRAMES);
        return;
    }
    if(vr_pdt_s == 4)
    {
        // A blast across the body, under the gun.
        VR_PDT_Aim(p, TRUE);
        VR_PDT_Blast(p);
        VR_PDT_To(5, 2);
        return;
    }
    if(vr_pdt_s == 5)
    {
        VR_Test_Check(vr_pd_disarms == vr_pdt_dis0 && vr_pd_hits == vr_pdt_hits0 && !VRIsHandWeaponless(p, cVR_MainHand),
            sprintf("a blast across the body: no hit on the gun (%g), not disarmed (%g)", vr_pd_hits - vr_pdt_hits0,
            vr_pd_disarms - vr_pdt_dis0));
        VR_PDT_Arm(p);
        // An enforcer's laser across the gun.
        VR_PDT_Aim(p, FALSE);
        VR_PDT_Laser(p);
        vr_pdt_case = 0;
        VR_PDT_To(6, 0);
        return;
    }
    if(vr_pdt_s == 6)
    {
        if(vr_pd_disarms == vr_pdt_dis0 && vr_pdt_laser.solid == SOLID_BBOX && vr_pdt_laser.touch == Laser_Touch)
        {
            return; // (on its way: 200 units at 600 a second)
        }
        if(vr_pdt_n < 2)
        {
            return; // (its jolt read: the next PlayerPostThink)
        }
        if(vr_pdt_case == 0)
        {
            VR_Test_Check(vr_pd_warns == vr_pdt_warn0 + 1 && vr_pd_disarms == vr_pdt_dis0 && !VRIsHandWeaponless(p, cVR_MainHand),
                sprintf("an enforcer's laser across the gun (15 damage: one jolt): the warning (%g), still held (%g disarms)",
                vr_pd_warns - vr_pdt_warn0, vr_pd_disarms - vr_pdt_dis0));
            vr_pdt_case = 1;
            VR_PDT_Aim(p, FALSE);
            VR_PDT_Laser(p);
            VR_PDT_To(6, 0);
            return;
        }
        VR_Test_Check(vr_pd_disarms == vr_pdt_dis0 + 1 && VRIsHandWeaponless(p, cVR_MainHand),
            sprintf("a second laser within the window: knocked out (%g)", vr_pd_disarms - vr_pdt_dis0));
        if(vr_wpn_dropped != world)
        {
            remove(vr_wpn_dropped);
            vr_wpn_dropped = world;
        }
        VR_PDT_Arm(p);
        localcmd("reset vr_player_disarm\n"); // its default: off in 1.1 (checked in state 7)
        VR_PDT_To(7, VR_PDT_POSE_FRAMES);
        return;
    }
    if(vr_pdt_s == 7)
    {
        VR_Test_Check(!cvar_hget(cvarh_vr_player_disarm), "vr_player_disarm defaults to 0 (off in 1.1)");
        VR_PDT_Aim(p, FALSE);
        VR_PDT_Blast(p);
        VR_PDT_To(8, 2);
        return;
    }
    if(vr_pdt_s == 8)
    {
        VR_Test_Check(vr_pd_disarms == vr_pdt_dis0 && vr_pd_hits == vr_pdt_hits0 && !VRIsHandWeaponless(p, cVR_MainHand),
            sprintf("vr_player_disarm 0 (the default): a blast across the gun does nothing (%g hits, %g disarms)", vr_pd_hits - vr_pdt_hits0,
            vr_pd_disarms - vr_pdt_dis0));
        VR_PDT_End();
    }
};

// `vr_pd_test 2`: melee on you holding the shotgun out (VR_PDT_POSE; flat-screen play: its own hold), up close: case 0 a
// grunt with a gun and an axe (the toolgun's pair: VR_AM_ArmAll's vr_ag_gunaxe), its axe in its left hand; case 1 an
// axe-only grunt. Each fights until 4 blows: every blow held back for your weapon near its way ends (let go, parried or
// dropped: its gun and axe's never did, his flat-screen note of 2026-11-16: no damage, never disarmed), the blows that
// land hurt you, and each roll on your weapon (vr_agrunt_vol_blow 40: the meeting's reach grown, chance certain) knocks
// it out (given back at once).
float vr_pdt_case;
float vr_pdt_h0;      // vr_drp_held_n at the case's start,
float vr_pdt_hd0;     // vr_drp_held_done,
float vr_pdt_pr0;     // vr_drp_parries,
float vr_pdt_rolls0;  // vr_pd_rolls,
float vr_pdt_strikes0; // its blows
float vr_pdt_hurt;    // the damage you took
float vr_pdt_volblow; // vr_agrunt_vol_blow before the test
entity vr_pdt_g;      // the case's grunt

entity(float wid, float wid2) VR_PDT_Grunt =
{
    entity p = VR_Player();
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    traceline(p.origin, p.origin + fw * 70, TRUE, p);
    vector at = trace_endpos - fw * 16;
    entity oldself = self;
    self = spawn();
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    self.weapon = wid;
    self.weapon2 = wid2;
    VR_AM_Spawn(VR_AM_GRUNT);
    entity g = self;
    if(g.think == walkmonster_start_go)
    {
        walkmonster_start_go();
    }
    g.enemy = p;
    FoundTarget();
    self = oldself;
    return g;
};

void(entity p) VR_PDT_Melee =
{
    vr_pdt_n = vr_pdt_n + 1;
    if(vr_pdt_s == 0)
    {
        VR_PDT_Arm(p);
        p.health = 1000;
        vr_pdt_hurt = 0;
        vr_pdt_g = VR_PDT_Grunt(vr_pdt_case == 0 ? WID_SHOTGUN : WID_AXE, vr_pdt_case == 0 ? WID_AXE : 0);
        vr_pdt_h0 = vr_drp_held_n;
        vr_pdt_hd0 = vr_drp_held_done;
        vr_pdt_pr0 = vr_drp_parries;
        vr_pdt_rolls0 = vr_pd_rolls;
        vr_pdt_strikes0 = vr_pdt_g.vr_ag_strikes;
        VR_Test_Check(vr_pdt_case == 1 || (vr_pdt_g.vr_ag_gunaxe && vr_pdt_g.vr_ag_wid == WID_SHOTGUN && vr_pdt_g.vr_ag_wid2 == WID_AXE),
            sprintf("case %g: spawned holding %g and %g (gun and axe %g)", vr_pdt_case, vr_pdt_g.vr_ag_wid, vr_pdt_g.vr_ag_wid2,
            vr_pdt_g.vr_ag_gunaxe));
        VR_PDT_To(1, 0);
        return;
    }
    // (fighting: you kept alive and armed; the damage summed)
    if(p.health < 1000)
    {
        vr_pdt_hurt = vr_pdt_hurt + 1000 - p.health;
        p.health = 1000;
    }
    if(VRIsHandWeaponless(p, cVR_MainHand))
    {
        if(vr_wpn_dropped != world)
        {
            remove(vr_wpn_dropped);
            vr_wpn_dropped = world;
        }
        VRGiveEntWeapon(p, cVR_MainHand, WID_SHOTGUN, 0, VR_PDT_CLIP);
        W_SetCurrentAmmoFor(cVR_MainHand);
        VR_Scene_HoldGiven(p, cVR_MainHand);
    }
    entity g = vr_pdt_g;
    g.health = max(g.health, 500); // (your parries' staggers never kill it)
    float blows = g.vr_ag_strikes - vr_pdt_strikes0;
    if(blows < 4 || g.vr_drp_pend > 0)
    {
        return;
    }
    float held = vr_drp_held_n - vr_pdt_h0;
    float ended = vr_drp_held_done - vr_pdt_hd0;
    float rolls = vr_pd_rolls - vr_pdt_rolls0;
    float dis = vr_pd_disarms - vr_pdt_dis0;
    string what = vr_pdt_case == 0 ? "gun and axe" : "axe only";
    VR_Test_Check(held == ended, sprintf("%s: %g blows; every one held back for your weapon near its way ended (%g of %g)", what,
        blows, ended, held));
    VR_Test_Check(vr_pdt_hurt > 0, sprintf("%s: the blows that landed hurt you (%g damage; %g parried)", what, vr_pdt_hurt,
        vr_drp_parries - vr_pdt_pr0));
    VR_Test_Check(rolls > 0 && dis == rolls, sprintf("%s: blows landing on your weapon jolted it (%g warnings), the jolts "
        "reached knocked it out each time at a certain chance (%g of %g)", what, vr_pd_warns - vr_pdt_warn0, dis, rolls));
    remove(g.vr_ag_prop);
    if(g.vr_ag_prop2)
    {
        remove(g.vr_ag_prop2);
    }
    remove(g);
    vr_pdt_g = world;
    vr_pdt_case = vr_pdt_case + 1;
    if(vr_pdt_case < 2)
    {
        VR_PDT_To(0, VR_PDT_POSE_FRAMES);
        return;
    }
    cvar_set("vr_agrunt_vol_blow", ftos(vr_pdt_volblow));
    VR_PDT_End();
};

// `vr_pd_test 3`: PvE's rules one by one (vr_playerdisarm.qc, "PvE's own rule"), each blow as VR_Parry hands it over
// landed (VR_PD_Blow) from a stand-in attacker, the shotgun in your main hand, a fist in the other, the jolts kept 0.25 s:
// a blade on your weapon warns, a second knocks it out, a heavy one at once; an armed humanoid's fist on your body warns,
// again after the window only warns; a bite, a blade on your body: nothing; a heavy body blow warns; the option off:
// nothing; vr_player_disarm_jolts 1: the first knocks it out.
float vr_pdt_k;    // the case
float vr_pdt_d0;   // vr_pd_disarms before its blow,
float vr_pdt_w0;   // vr_pd_warns,
float vr_pdt_j0;   // vr_pd_jolted
float VR_PDT_RULES = 10;

// Case `k`'s blow: by a stand-in of class `cls` (`ag`: an armed humanoid's), `dmg` damage, on the weapon (`onW`) or the
// body, with a line or not; its name.
string(entity p, float k) VR_PDT_RuleBlow =
{
    string cls = "monster_knight";
    float dmg = 5;
    float onW = TRUE;
    float line = TRUE;
    float ag = FALSE;
    string name = "a light blade blow (5) on your weapon";
    if(k == 1) { name = "a second one within the window"; }
    if(k == 2) { dmg = 20; name = "a heavy blade blow (20) on your weapon"; }
    if(k == 3 || k == 4)
    {
        cls = "monster_army";
        ag = TRUE;
        onW = FALSE;
        line = FALSE;
        name = k == 3 ? "an armed humanoid's fist (5) on your body" : "its fist again after the window (0.25 s)";
    }
    if(k == 5) { cls = "monster_dog"; dmg = 8; onW = FALSE; line = FALSE; name = "a dog's bite (8) on your body"; }
    if(k == 6) { dmg = 8; onW = FALSE; name = "a blade blow (8) on your body, not your weapon"; }
    if(k == 7) { cls = "monster_shambler"; dmg = 20; onW = FALSE; line = FALSE; name = "a heavy claw (20) on your body"; }
    if(k == 8) { dmg = 20; name = "vr_player_disarm 0: a heavy blade blow on your weapon"; }
    if(k == 9) { name = "vr_player_disarm_jolts 1: a light blade blow on your weapon"; }
    if(k == 8) { cvar_set("vr_player_disarm", "0"); }
    if(k == 9) { cvar_set("vr_player_disarm_jolts", "1"); }
    entity by = VR_PDT_Shooter(cls);
    by.vr_ag = ag;
    setorigin(by, p.origin + '40 0 0');
    vr_pdt_d0 = vr_pd_disarms;
    vr_pdt_w0 = vr_pd_warns;
    vr_pdt_j0 = vr_pd_jolted;
    VR_PD_Blow(p, by, onW ? cVR_MainHand : -1, dmg, 0, line);
    return name;
};

string vr_pdt_name; // the case's name
void(entity p) VR_PDT_Rules =
{
    vr_pdt_n = vr_pdt_n + 1;
    float k = vr_pdt_k;
    if(vr_pdt_s == 0)
    {
        if(k != 1 && k != 4) // (those build on the case before)
        {
            VR_PDT_Arm(p);
            VR_PDT_To(1, VR_PDT_POSE_FRAMES);
            return;
        }
        VR_PDT_To(1, k == 4 ? 30 : 0); // (case 4: the 0.25 s window over)
        return;
    }
    if(vr_pdt_s == 1)
    {
        vr_pdt_name = VR_PDT_RuleBlow(p, k);
        VR_PDT_To(2, 2); // (its jolts read: the next PlayerPostThink)
        return;
    }
    float d = vr_pd_disarms - vr_pdt_d0;
    float w = vr_pd_warns - vr_pdt_w0;
    float j = vr_pd_jolted - vr_pdt_j0;
    float held = !VRIsHandWeaponless(p, cVR_MainHand);
    // Each case's outcome: knocked out (d), warned (w), jolts given (j).
    float wantD = (k == 1 || k == 2 || k == 9);
    float wantW = (k == 0 || k == 3 || k == 4 || k == 7);
    float wantJ = (k == 2) ? 2 : ((k == 5 || k == 6 || k == 8) ? 0 : 1);
    VR_Test_Check(d == wantD && w == wantW && j == wantJ && held == !wantD,
        sprintf("%s: %s (jolts %g, warnings %g, knocked out %g, held %g)", vr_pdt_name,
        wantD ? "knocked out" : (wantW ? "the warning, still held" : "nothing"), j, w, d, held));
    if(k == 8) { cvar_set("vr_player_disarm", "1"); }
    if(k == 9) { cvar_set("vr_player_disarm_jolts", ftos(vr_pdt_jolts)); }
    if(vr_wpn_dropped != world && !held)
    {
        remove(vr_wpn_dropped);
        vr_wpn_dropped = world;
    }
    vr_pdt_k = k + 1;
    if(vr_pdt_k >= VR_PDT_RULES)
    {
        VR_PDT_End();
        return;
    }
    VR_PDT_To(0, 0);
};

float() VR_PDT_IsMelee = { return vr_pdt_on == 2; }; // (the melee test: vr_pd_test 2)

void() VR_PlayerDisarmTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_pd_test);
    if(!ask)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    if(vr_pdt_on != ask)
    {
        VR_Test_BeginFlags("pdtest", VR_TEST_LOUD);
        vr_pdt_on = ask;
        vr_pdt_chance = cvar_hget(cvarh_vr_player_disarm_chance);
        vr_pdt_pve = cvar_hget(cvarh_vr_player_disarm);
        vr_pdt_jolts = cvar_hget(cvarh_vr_player_disarm_jolts);
        vr_pdt_window = cvar_hget(cvarh_vr_player_disarm_window);
        vr_pdt_k = 0;
        if(vr_pdt_on == 3)
        {
            cvar_set("vr_player_disarm_window", "0.25"); // (case 4: the grip recovered after it)
        }
        cvar_set("vr_player_disarm", "1"); // (PvE: the grunts' and enforcers' shots and blows)
        cvar_set("vr_player_disarm_chance", "100"); // (certain)
        localcmd(sprintf("%s\n", VR_PDT_POSE));
        VR_PDT_To(0, VR_PDT_POSE_FRAMES);
        vr_pdt_case = 0;
        vr_pdt_volblow = cvar_hget(cvarh_vr_agrunt_vol_blow);
        if(VR_PDT_IsMelee())
        {
            cvar_set("vr_agrunt_vol_blow", "40");
        }
    }
    if(vr_pdt_wait > 0)
    {
        vr_pdt_wait = vr_pdt_wait - 1;
        return;
    }
    if(time - vr_pdt_t0 > (VR_PDT_IsMelee() ? 25 : VR_PDT_GIVEUP)) // (test 2: four blows, each a swing's while)
    {
        VR_Test_Check(FALSE, sprintf("timed out: state %g, case %g, rule case %g%s", vr_pdt_s, vr_pdt_case, vr_pdt_k, !vr_pdt_g ? "" :
            sprintf(" (blows %g, held %g, ended %g, pending %g, act %g, its weapon %g, %.0f away)",
            vr_pdt_g.vr_ag_strikes - vr_pdt_strikes0, vr_drp_held_n - vr_pdt_h0, vr_drp_held_done - vr_pdt_hd0,
            vr_pdt_g.vr_drp_pend, vr_pdt_g.vr_ag_act, vr_pdt_g.vr_ag_wid, vlen(vr_pdt_g.origin - VR_Player().origin))));
        if(VR_PDT_IsMelee())
        {
            cvar_set("vr_agrunt_vol_blow", ftos(vr_pdt_volblow));
        }
        VR_PDT_End();
        return;
    }
    entity oldSelf = self;
    self = p; // (the hands' getters read self)
    if(ask == 1) { VR_PDT_Run(p); }
    else if(ask == 2) { VR_PDT_Melee(p); }
    else if(ask == 3) { VR_PDT_Rules(p); }
    else { VR_PDT_End(); }
    self = oldSelf;
};

#endif
