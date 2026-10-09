// Dawn of the Machine weapons' tests (vr_mg3_wtest N, developer 1; Debug > Tests > Dawn of the Machine Weapons):
// docs/vr-port/MG3.md M3-11..14, docs/vr-port/TESTING.md. Prints "mg3wtest: PASS/FAIL <what>" and a total.
//   1 report: the data, the hands and holsters, cells, the Super Axe's chain
//   2 the Super Axe's blows and burst (a few seconds: monsters spawned ahead, struck by MG3_SuperAxe_Strike)
//   3 its pickup: taken into the empty main hand, its target fired, spawnflag 128's silent drop (by 16 units here)
//   4 (Dawn of the Machine's map2) the map's weapon_mjolnir is a Super Axe: taken, its secret counted, dropped 2047 units
//   5 a Super Axe in the main hand and the first holster (then changelevel or save/load and 1)
//   6 (an MG3 map with axe buttons: map6, map7, map8, secret5) each func_axe_button: shots and blasts leave it shut (and
//     it says so), then each opens to a blow, the kinds in turn: a fist's, the Super Axe's, a thrown weapon, a thrown
//     prop, a headbutt
//   7 the player put in front of the nearest closed axe button, facing it (a real punch or swing next: vr_motion_play)
//   8 whether the nearest axe button opened
//   9 the laser cannon's bolts: their damage (MG3's 15 and 20 in campaign 5, else 18 and 25), 12 fired at the floor
//     ahead and how they bounced (a bounce keeps 0.9 of it)
//  10 the nearest weapon_laser_gun taken by the empty main hand (map2b: with or without Hipnotic)
//  11 the bloody bits set (both: any campaign), 12 their report (bits, the shotgun's refire), 13 the map's bloody
//     shotguns (none: two spawned ahead) taken, one by one, by the empty main hand (bits, shells), 14 the Bloody
//     Nightmare new game flag set (serverflags 256: then changelevel), 15 the bloody bits cleared

float mg3wt_pass, mg3wt_fail;
float mg3wt_step;      // the running request's step (2's), 0: none
float mg3wt_next;      // when its next step runs
entity mg3wt_a, mg3wt_b, mg3wt_z;
float mg3wt_h;         // a health read in a step before
float mg3wt_lastdmg;   // the damage MG3_SuperAxe_Strike's victim lost
float mg3wt_gripmode;  // vr_weapon_grip_mode before request 2 (held weapons stay without a grip pressed meanwhile)

void(float ok, string label) MG3_WT_Check =
{
    if(ok) mg3wt_pass++; else mg3wt_fail++;
    dprint(sprintf("mg3wtest: %s %s\n", ok ? "PASS" : "FAIL", label));
};

entity() MG3_WT_Player = { return find(world, classname, "player"); };

// A monster `xChoice` (func_enemy_dispenser's: 0 soldier, 1 ogre, 2 zombie) `xDist` units ahead of the player, `xSide` aside.
entity(entity p, float xChoice, float xDist, float xSide) MG3_WT_Spawn =
{
    makevectors('0 1 0' * p.v_angle_y);
    entity d = spawn();
    d.weapon = xChoice;
    setorigin(d, p.origin + v_forward * xDist + v_right * xSide);
    entity oldself = self;
    self = d;
    activator = p;
    func_enemy_dispenser_use();
    entity m = self;
    self = oldself;
    remove(d);
    return m;
};

// A Super Axe blow of the main hand on `v` (base 40), its health before and after kept.
float(entity p, entity v) MG3_WT_Strike =
{
    entity oldself = self;
    self = p;
    float before = v.health;
    makevectors(p.v_angle);
    float burst = MG3_SuperAxe_Strike(cVR_MainHand, v, (v.absmin + v.absmax) * 0.5, 40);
    mg3wt_lastdmg = before - v.health;
    self = oldself;
    return burst;
};

void() MG3_WT_Report =
{
    entity p = MG3_WT_Player();
    dprint(sprintf("mg3wtest: report map %s campaign %g superaxe data %g model %s\n", mapname, cvar("vr_campaign"),
        VR_Pack_WeaponAvailable(WID_SUPERAXE), WeaponIdToModel(WID_SUPERAXE, 0)));
    if(!p) return;
    dprint(sprintf("mg3wtest: hands %g/%g models %s / %s cells %g has %g\n", p.weapon, p.weapon2, p.weaponmodel, p.weaponmodel2,
        p.ammo_cells, VR_WeaponUtil_EntHasWeapon(p, WID_SUPERAXE)));
    dprint(sprintf("mg3wtest: holsters %g %g %g %g %g %g (model0 %s)\n", p.holsterweapon0, p.holsterweapon1, p.holsterweapon2,
        p.holsterweapon3, p.holsterweapon4, p.holsterweapon5, p.holsterweaponmodel0));
    dprint(sprintf("mg3wtest: chain mark %s until %g (now %g) hand %g bursts %g\n", p.vr_superaxe_mark.classname,
        p.vr_superaxe_until, time, p.vr_superaxe_hand, vr_superaxe_bursts));
};

void(entity p) MG3_WT_GiveMain =
{
    entity oldself = self;
    self = p;
    VR_WeaponUtil_EntAddWeapon(p, WID_SUPERAXE);
    VRGiveEntWeapon(p, cVR_MainHand, WID_SUPERAXE, 0, 0);
    W_SetCurrentAmmoFor(cVR_MainHand);
    self = oldself;
};

// Request 2, one step a call (MG3_WeaponsTestFrame runs it when mg3wt_next comes).
void() MG3_WT_BurstStep =
{
    entity p = MG3_WT_Player();
    float window = cvar_hget(cvarh_vr_superaxe_burst_window);
    if(mg3wt_step == 1)
    {
        mg3wt_pass = mg3wt_fail = 0;
        p.flags = p.flags | FL_GODMODE;
        mg3wt_gripmode = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1");
        MG3_WT_GiveMain(p);
        p.ammo_cells = 50;
        mg3wt_a = MG3_WT_Spawn(p, 1, 120, -40); // two ogres (200 health) and a zombie
        mg3wt_b = MG3_WT_Spawn(p, 1, 120, 40);
        mg3wt_z = MG3_WT_Spawn(p, 2, 90, 0);
        MG3_WT_Check(p.weapon == WID_SUPERAXE && p.weaponmodel == "owned/mg3/progs/v_hammer.mdl", "the Super Axe in the main hand, its model MG3's");
        MG3_WT_Check(mg3wt_a.classname == "monster_ogre" && mg3wt_z.classname == "monster_zombie", "two ogres and a zombie ahead");
        mg3wt_step = 2; mg3wt_next = time + 0.5;
        return;
    }
    if(mg3wt_step == 2)
    {
        float burst = MG3_WT_Strike(p, mg3wt_a);
        dprint(sprintf("mg3wtest: first blow: %g damage, burst %g, mark %s, model %s\n", mg3wt_lastdmg, burst, p.vr_superaxe_mark.classname, p.weaponmodel));
        MG3_WT_Check(!burst && mg3wt_lastdmg >= 40 && p.vr_superaxe_mark == mg3wt_a, "a first blow: its damage, the ogre marked");
        MG3_WT_Check(p.weaponmodel == "owned/mg3/progs/v_hammer_glow.mdl", "the head glows (the burst ready, 50 cells)");
        MG3_WT_Check(fabs(p.vr_superaxe_until - (time + window)) < 0.01, sprintf("the window %g s", window));
        mg3wt_h = mg3wt_a.health;
        mg3wt_step = 3; mg3wt_next = time + min(0.8, window * 0.5);
        return;
    }
    if(mg3wt_step == 3)
    {
        float cells = p.ammo_cells;
        float bursts = vr_superaxe_bursts;
        float burst3 = MG3_WT_Strike(p, mg3wt_a);
        dprint(sprintf("mg3wtest: second blow: burst %g, cells %g -> %g, damage at once %g\n", burst3, cells, p.ammo_cells, mg3wt_lastdmg));
        MG3_WT_Check(burst3 && vr_superaxe_bursts == bursts + 1, "a second blow on it within the window fires the burst");
        MG3_WT_Check(p.ammo_cells == cells - 15, "the burst spends 15 cells");
        MG3_WT_Check(mg3wt_lastdmg == 0, "the burst's blow does no damage itself (MG3: the lightning's instead)");
        MG3_WT_Check(p.weaponmodel == "owned/mg3/progs/v_hammer.mdl" && !p.vr_superaxe_until, "the chain ends, the glow out");
        mg3wt_step = 4; mg3wt_next = time + 0.6;
        return;
    }
    if(mg3wt_step == 4)
    {
        dprint(sprintf("mg3wtest: the lightning: the ogre %g -> %g\n", mg3wt_h, mg3wt_a.health));
        MG3_WT_Check(mg3wt_a.health < mg3wt_h, "the lightning struck the ogre");
        // A first blow, then the window let pass: no burst.
        mg3wt_a.health = 200;
        MG3_WT_Strike(p, mg3wt_a);
        mg3wt_step = 5; mg3wt_next = time + window + 0.2;
        return;
    }
    if(mg3wt_step == 5)
    {
        MG3_WT_Check(p.weaponmodel == "owned/mg3/progs/v_hammer.mdl" && !p.vr_superaxe_until, "the window closed: the glow out, no chain");
        float late = MG3_WT_Strike(p, mg3wt_a);
        MG3_WT_Check(!late && mg3wt_lastdmg >= 40 && p.vr_superaxe_mark == mg3wt_a, "a blow after the window: a first blow again");
        // Another monster in between: its damage, the chain ended.
        float other1 = MG3_WT_Strike(p, mg3wt_b);
        MG3_WT_Check(!other1 && mg3wt_lastdmg >= 40 && !p.vr_superaxe_until, "a blow on another monster: its damage, the chain ended");
        float again = MG3_WT_Strike(p, mg3wt_a);
        MG3_WT_Check(!again && p.vr_superaxe_mark == mg3wt_a, "then the first again: a first blow");
        // Under water: no burst.
        p.waterlevel = 2;
        float wet = MG3_WT_Strike(p, mg3wt_a);
        p.waterlevel = 0;
        MG3_WT_Check(!wet && mg3wt_lastdmg >= 40, "under water: the second blow's damage, no burst");
        // Too few cells: no glow, no burst.
        p.ammo_cells = 10;
        MG3_WT_Strike(p, mg3wt_a);
        MG3_WT_Check(p.weaponmodel == "owned/mg3/progs/v_hammer.mdl", "under 15 cells: no glow");
        float poor = MG3_WT_Strike(p, mg3wt_a);
        MG3_WT_Check(!poor && p.ammo_cells == 10, "under 15 cells: no burst");
        // The zombie: three times; a killing blow on an ogre: twice.
        mg3wt_z.health = 1000;
        MG3_WT_Strike(p, mg3wt_z);
        dprint(sprintf("mg3wtest: zombie blow %g\n", mg3wt_lastdmg));
        MG3_WT_Check(mg3wt_lastdmg >= 120, "a zombie takes three times the blow");
        mg3wt_b.health = 30;
        float hb = mg3wt_b.health;
        MG3_WT_Strike(p, mg3wt_b);
        dprint(sprintf("mg3wtest: killing blow from %g: %g (health %g)\n", hb, mg3wt_lastdmg, mg3wt_b.health));
        MG3_WT_Check(mg3wt_b.health <= -50 || mg3wt_lastdmg >= 80, "a killing blow does twice (it gibs)");
        p.flags = p.flags - (p.flags & FL_GODMODE);
        cvar_set("vr_weapon_grip_mode", ftos(mg3wt_gripmode));
        dprint(sprintf("mg3wtest: superaxe %g passed, %g failed\n", mg3wt_pass, mg3wt_fail));
        mg3wt_step = 0;
        return;
    }
};

// Request 3: a pickup ahead (spawned half a second before), taken by the empty main hand as a hand's grab takes it.
void() MG3_WT_Pickup =
{
    entity p = MG3_WT_Player();
    mg3wt_pass = mg3wt_fail = 0;
    entity oldself = self, oldother = other;
    self = p;
    VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
    W_SetCurrentAmmoFor(cVR_MainHand);
    self = oldself;
    entity it = find(world, classname, "weapon_superaxe");
    while(it && it.solid == SOLID_NOT) { it = find(it, classname, "weapon_superaxe"); }
    MG3_WT_Check(it != world && it.weapon == WID_SUPERAXE, "a weapon_superaxe spawned");
    if(!it) { return; }
    // Its target, and the silent drop by 16 units.
    entity t = spawn();
    t.targetname = "mg3wtest_target";
    t.use = SUB_Null;
    t.classname = "mg3wtest_target";
    it.target = "mg3wtest_target";
    it.spawnflags = it.spawnflags | 128;
    it.height = -16;
    float z0 = p.origin_z;
    p.touchinghand = cVR_MainHand;
    it.handtouch_hand = cVR_MainHand;
    it.handtouch_ent = p;
    other = p;
    self = it;
    float oldCatching = forcegrabCatching;
    forcegrabCatching = TRUE;
    activator = world;
    it.handtouch();
    forcegrabCatching = oldCatching;
    self = oldself;
    other = oldother;
    dprint(sprintf("mg3wtest: pickup: hand %g, item solid %g, dz %g, activator %s\n", p.weapon, it.solid, p.origin_z - z0, activator.classname));
    MG3_WT_Check(p.weapon == WID_SUPERAXE && VR_WeaponUtil_EntHasWeapon(p, WID_SUPERAXE), "taken into the main hand");
    MG3_WT_Check(it.solid == SOLID_NOT, "the pickup gone");
    MG3_WT_Check(fabs(p.origin_z - z0 + 15) < 0.5, "spawnflag 128: dropped by its height (16, less MG3's 1)");
    MG3_WT_Check(activator == p, "its target fired by the taker");
    dprint(sprintf("mg3wtest: pickup %g passed, %g failed\n", mg3wt_pass, mg3wt_fail));
};

// Request 4 (Dawn of the Machine's map2): the map's own Super Axe.
void() MG3_WT_Map2 =
{
    entity p = MG3_WT_Player();
    mg3wt_pass = mg3wt_fail = 0;
    entity it = find(world, classname, "weapon_superaxe");
    MG3_WT_Check(find(world, classname, "weapon_mjolnir") == world, "no weapon_mjolnir left (remapped)");
    MG3_WT_Check(it != world && it.weapon == WID_SUPERAXE && (it.spawnflags & 128) && it.height == -2048 && it.target == "axe_secret",
        "map2's weapon_mjolnir is a Super Axe: spawnflag 128, height -2048, target axe_secret");
    if(!it) { return; }
    entity oldself = self, oldother = other;
    self = p;
    VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
    W_SetCurrentAmmoFor(cVR_MainHand);
    self = oldself;
    setorigin(p, it.origin + '0 0 24');
    float z0 = p.origin_z;
    float secrets = found_secrets;
    p.touchinghand = cVR_MainHand;
    it.handtouch_hand = cVR_MainHand;
    it.handtouch_ent = p;
    other = p;
    self = it;
    float oldCatching = forcegrabCatching;
    forcegrabCatching = TRUE;
    activator = world;
    it.handtouch();
    forcegrabCatching = oldCatching;
    self = oldself;
    other = oldother;
    dprint(sprintf("mg3wtest: map2 pickup: hand %g, dz %g, secrets %g -> %g\n", p.weapon, p.origin_z - z0, secrets, found_secrets));
    MG3_WT_Check(p.weapon == WID_SUPERAXE, "taken into the main hand");
    MG3_WT_Check(fabs(p.origin_z - z0 + 2047) < 0.5, "dropped 2047 units (MG3's silent teleport)");
    MG3_WT_Check(found_secrets == secrets + 1, "its secret counted (trigger_secret axe_secret)");
    dprint(sprintf("mg3wtest: map2 %g passed, %g failed\n", mg3wt_pass, mg3wt_fail));
};

// Request 5: a Super Axe in the main hand and the first holster.
void() MG3_WT_Holster =
{
    entity p = MG3_WT_Player();
    MG3_WT_GiveMain(p);
    p.holsterweapon0 = WID_SUPERAXE;
    p.holsterweaponmodel0 = WeaponIdToModel(WID_SUPERAXE, 0);
    p.ammo_cells = 40;
    dprint("mg3wtest: a Super Axe in the main hand and holster 0, 40 cells\n");
    MG3_WT_Report();
};


// A blow of kind `k` on button `b` (0 a fist's melee, 1 the Super Axe's melee, 2 a thrown weapon, 3 a thrown prop,
// 4 a headbutt), or a shot (5) or a blast (6).
void(entity p, entity b, float k) MG3_WT_HitButton =
{
    entity oldself = self;
    self = p;
    entity thing = world;
    if(k == 2 || k == 3)
    {
        thing = spawn();
        thing.classname = k == 2 ? "thrown_weapon" : "mg3wtest_prop";
        thing.vr_rigid = k == 3;
        thing.movetype = MOVETYPE_BOUNCE;
        thing.owner = p;
    }
    if(k == 0 || k == 1) VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, '0 0 0', '0 0 0');
    else if(k == 4) VR_SetHitKind(QVR_HITKIND_HEADBUTT, -1, '0 0 0', '0 0 0');
    else if(k == 5) VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', '0 0 0');
    else if(k == 6) VR_SetHitKind(QVR_HITKIND_SPLASH, -1, '0 0 0', '0 0 0');
    else VR_ClearHitKind();
    if(k == 0 || k == 1) T_Damage_VRMelee(b, p, p, 20);
    else T_Damage(b, thing != world ? thing : p, p, 20);
    VR_ClearHitKind();
    if(thing) remove(thing);
    self = oldself;
};

// Request 6: every axe button of the map.
void() MG3_WT_AxeButtons =
{
    entity p = MG3_WT_Player();
    mg3wt_pass = mg3wt_fail = 0;
    float n = 0, shut = 0, opened = 0;
    entity b = find(world, classname, "func_axe_button");
    while(b)
    {
        n++;
        MG3_WT_HitButton(p, b, 5);
        MG3_WT_HitButton(p, b, 6);
        if(b.state == STATE_BOTTOM && b.health > 0) shut++;
        b = find(b, classname, "func_axe_button");
    }
    dprint(sprintf("mg3wtest: %s: %g axe buttons, %g shut after a shot and a blast each\n", mapname, n, shut));
    MG3_WT_Check(n > 0, "the map has axe buttons");
    MG3_WT_Check(shut == n, "shots and blasts open none");
    float k = 0;
    b = find(world, classname, "func_axe_button");
    while(b)
    {
        MG3_WT_HitButton(p, b, k);
        if(b.state != STATE_BOTTOM) opened++;
        else dprint(sprintf("mg3wtest: button at %s (target %s) stayed shut to blow kind %g\n", vtos((b.absmin + b.absmax) * 0.5), b.target, k));
        k = k == 4 ? 0 : k + 1;
        b = find(b, classname, "func_axe_button");
    }
    dprint(sprintf("mg3wtest: %g of %g opened by blows (fist, Super Axe, thrown weapon, thrown prop, headbutt in turn)\n", opened, n));
    MG3_WT_Check(opened == n, "every one opens to a blow");
    dprint(sprintf("mg3wtest: axe buttons %g passed, %g failed\n", mg3wt_pass, mg3wt_fail));
};

entity(entity p, float xClosed) MG3_WT_NearestAxeButton =
{
    entity best = world;
    float bestDist = 999999;
    entity b = find(world, classname, "func_axe_button");
    while(b)
    {
        vector mid = (b.absmin + b.absmax) * 0.5;
        if((!xClosed || b.state == STATE_BOTTOM) && vlen(mid - p.origin) < bestDist)
        {
            best = b;
            bestDist = vlen(mid - p.origin);
        }
        b = find(b, classname, "func_axe_button");
    }
    return best;
};

// Request 7: in front of the nearest closed axe button (its face: the way it moves when pressed, reversed), 28 units
// from its middle (request 7), facing it, the eyes level with it.
void() MG3_WT_FaceAxeButton =
{
    entity p = MG3_WT_Player();
    entity b = MG3_WT_NearestAxeButton(p, TRUE);
    if(!b) { dprint("mg3wtest: no closed axe button\n"); return; }
    vector mid = (b.absmin + b.absmax) * 0.5;
    vector out = normalize(b.movedir) * -1;
    out_z = 0;
    out = normalize(out);
    vector at = mid + out * 28 - '0 0 22';
    setorigin(p, at);
    p.angles = vectoangles(out * -1);
    p.v_angle = p.angles;
    p.fixangle = TRUE;
    p.velocity = '0 0 0';
    dprint(sprintf("mg3wtest: facing axe button at %s (target %s) at %v from %v, yaw %g\n", vtos((b.absmin + b.absmax) * 0.5), b.target, mid, at, p.angles_y));
};

void() MG3_WT_AxeButtonState =
{
    entity p = MG3_WT_Player();
    entity b = MG3_WT_NearestAxeButton(p, FALSE);
    if(!b) return;
    dprint(sprintf("mg3wtest: nearest axe button at %s state %g (1 shut) health %g\n", vtos((b.absmin + b.absmax) * 0.5), b.state, b.health));
};

.float mg3wt_bolt;     // a test's laser bolt (request 9)
float mg3wt_bolts_bounced, mg3wt_bolts_seen; // HIP_LaserTouch's counts as the bolts were fired

// Request 9, at once: the damage of a bolt, then 12 bolts at the wall ahead (MG3_WT_LaserStep counts them after).
void() MG3_WT_Laser =
{
    entity p = MG3_WT_Player();
    mg3wt_pass = mg3wt_fail = 0;
    entity oldself = self;
    self = p;
    float plain = VR_LaserBoltDamage(FALSE), lit = VR_LaserBoltDamage(TRUE);
    float want = MG3_Campaign() ? 15 : 18, wantLit = MG3_Campaign() ? 20 : 25;
    float scale = cvar_hget(cvarh_vr_dmg_laser) / 18;
    dprint(sprintf("mg3wtest: laser bolt %g, lit %g (campaign %g, Weapon Damage x%g)\n", plain, lit, cvar("vr_campaign"), scale));
    MG3_WT_Check(fabs(plain - want * scale) < 0.01 && fabs(lit - wantLit * scale) < 0.01,
        sprintf("a bolt %g, a lit one %g", want * scale, wantLit * scale));
    // At the floor ahead, 40 degrees down, fanned (the floor: world, near, whatever the map; monsters seldom on it).
    makevectors(p.v_angle_y * '0 1 0');
    vector from = p.origin + v_forward * 28 + '0 0 16'; // (out of your box: a bolt starting in it strikes you)
    makevectors(p.v_angle_y * '0 1 0' + '40 0 0');
    dprint(sprintf("mg3wtest: bolts at the floor ahead (yaw %g)\n", p.v_angle_y));
    mg3wt_bolts_bounced = vr_laser_bounces; // (the counts before: weapons.qc HIP_LaserTouch)
    mg3wt_bolts_seen = vr_laser_stops;
    vr_laser_bounce_ratio = 0;
    for(float i = 0; i < 12; i++)
    {
        vector dir = normalize(v_forward + v_right * (i - 5.5) * 0.05);
        entity bolt = HIP_LaunchLaser(from, dir, '0 0 0', 0);
        bolt.mg3wt_bolt = 1;
    }
    self = oldself;
    mg3wt_step = 20;
    mg3wt_next = time + 1; // (their bounces and stops counted by then)
};

void() MG3_WT_LaserStep =
{
    float alive = 0;
    entity e = find(world, classname, "hiplaser");
    while(e)
    {
        if(e.mg3wt_bolt)
        {
            alive++;
            remove(e);
        }
        e = find(e, classname, "hiplaser");
    }
    float bounces = vr_laser_bounces - mg3wt_bolts_bounced, stops = vr_laser_stops - mg3wt_bolts_seen;
    dprint(sprintf("mg3wtest: of 12 bolts: %g bounces (each keeping %g of the damage), %g stopped, %g still flying\n",
        bounces, vr_laser_bounce_ratio, stops, alive));
    MG3_WT_Check(bounces >= 1 && vr_laser_bounce_ratio == 0.9, "bolts bounce off the floor, keeping 0.9 of their damage");
    MG3_WT_Check(stops >= 1, "bolts stop (the one in seven, the third touch, or what they hit)");
    dprint(sprintf("mg3wtest: laser %g passed, %g failed\n", mg3wt_pass, mg3wt_fail));
    mg3wt_step = 0;
};

// Request 10: the nearest weapon_laser_gun into the empty main hand.
void() MG3_WT_LaserPickup =
{
    entity p = MG3_WT_Player();
    mg3wt_pass = mg3wt_fail = 0;
    entity best = world;
    float bestDist = 999999;
    entity e = find(world, classname, "weapon_laser_gun");
    while(e)
    {
        if(e.model != string_null && vlen(e.origin - p.origin) < bestDist) { best = e; bestDist = vlen(e.origin - p.origin); }
        e = find(e, classname, "weapon_laser_gun");
    }
    MG3_WT_Check(best != world, sprintf("a weapon_laser_gun in %s", mapname));
    if(!best) return;
    dprint(sprintf("mg3wtest: laser pickup %s (Hipnotic %g), its model %s; v_laserg %g lasrspik %g\n", vtos(best.origin),
        VR_Pack_Hipnotic(), best.model, fileexists("progs/v_laserg.mdl"), fileexists("progs/lasrspik.mdl")));
    entity oldself = self, oldother = other;
    self = p;
    VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
    W_SetCurrentAmmoFor(cVR_MainHand);
    self = oldself;
    setorigin(p, best.origin + '0 0 24');
    p.touchinghand = cVR_MainHand;
    best.handtouch_hand = cVR_MainHand;
    best.handtouch_ent = p;
    other = p;
    self = best;
    float oldCatching = forcegrabCatching;
    forcegrabCatching = TRUE;
    best.handtouch();
    forcegrabCatching = oldCatching;
    self = oldself;
    other = oldother;
    dprint(sprintf("mg3wtest: hand %g model %s cells %g\n", p.weapon, p.weaponmodel, p.ammo_cells));
    MG3_WT_Check(p.weapon == WID_LASER_CANNON && p.weaponmodel == "progs/v_laserg.mdl", "taken: the laser cannon in the main hand");
    dprint(sprintf("mg3wtest: laser pickup %g passed, %g failed\n", mg3wt_pass, mg3wt_fail));
};

// Requests 11..15: the bloody shotguns.
void() MG3_WT_BloodyReport =
{
    entity p = MG3_WT_Player();
    dprint(sprintf("mg3wtest: bloody bits %g (parm56 %g) serverflags %g; shotgun refire %g; shells %g; hands %g/%g\n",
        MG3_BloodyBits(), parm56, serverflags, MG3_BloodyShotgunRefire(0.5), p.ammo_shells, p.weapon, p.weapon2));
};

entity() MG3_WT_NextBloody =
{
    entity e = nextent(world);
    while(e)
    {
        if(e.vr_bloody_bit && e.model != string_null && (e.classname == "weapon_shotgun" || e.classname == "weapon_supershotgun"))
        {
            return e;
        }
        e = nextent(e);
    }
    return world;
};

void() MG3_WT_BloodyTake =
{
    entity p = MG3_WT_Player();
    mg3wt_pass = mg3wt_fail = 0;
    float taken = 0;
    entity it = MG3_WT_NextBloody();
    while(it && taken < 4)
    {
        float bit = it.vr_bloody_bit, wid = it.weapon, shells = p.ammo_shells, bits = MG3_BloodyBits();
        entity oldself = self, oldother = other;
        self = p;
        VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
        W_SetCurrentAmmoFor(cVR_MainHand);
        self = oldself;
        p.touchinghand = cVR_MainHand;
        it.handtouch_hand = cVR_MainHand;
        it.handtouch_ent = p;
        other = p;
        self = it;
        float oldCatching = forcegrabCatching;
        forcegrabCatching = TRUE;
        it.handtouch();
        forcegrabCatching = oldCatching;
        self = oldself;
        other = oldother;
        float clip = p.weaponclipsize;
        dprint(sprintf("mg3wtest: took %s: hand %g, bits %g -> %g, shells %g -> %g (clip %g)\n", it.netname, p.weapon, bits,
            MG3_BloodyBits(), shells, p.ammo_shells, clip));
        MG3_WT_Check(p.weapon == wid && (MG3_BloodyBits() & bit) && it.model == string_null, sprintf("%s taken: its bit set", it.netname));
        MG3_WT_Check(p.ammo_shells + clip >= min(shells + 30, VR_MaxAmmo(p, AID_SHELLS)) - 0.5, "30 shells (to the cap)");
        taken++;
        it = MG3_WT_NextBloody();
    }
    MG3_WT_Check(taken > 0, sprintf("bloody shotguns taken: %g", taken));
    MG3_WT_BloodyReport();
    MG3_WT_Check(!(MG3_BloodyBits() & MG3_BLOODY_SG) || MG3_BloodyShotgunRefire(0.5) == 0.28, "the bloody shotgun refires in 0.28 s");
    dprint(sprintf("mg3wtest: bloody %g passed, %g failed\n", mg3wt_pass, mg3wt_fail));
};

void() MG3_WeaponsTestFrame =
{
    if(mg3wt_step == 10 && time >= mg3wt_next)
    {
        entity os1 = self, oo1 = other, oa1 = activator;
        mg3wt_step = 0;
        MG3_WT_Pickup(); // (request 3: its pickup settled first)
        self = os1; other = oo1; activator = oa1;
    }
    if(mg3wt_step == 20 && time >= mg3wt_next)
    {
        MG3_WT_LaserStep();
    }
    if(mg3wt_step == 30 && time >= mg3wt_next)
    {
        entity os3 = self, oo3 = other, oa3 = activator;
        mg3wt_step = 0;
        MG3_WT_BloodyTake(); // (request 13: the pickups settled first)
        self = os3; other = oo3; activator = oa3;
    }
    if(mg3wt_step && time >= mg3wt_next)
    {
        entity os = self, oo = other;
        MG3_WT_BurstStep();
        self = os; other = oo;
    }
    float request = cvar("vr_mg3_wtest");
    if(!request) return;
    cvar_set("vr_mg3_wtest", "0");
    if(!MG3_WT_Player()) return;
    if(request != 1 && request < 6 && !VR_Pack_WeaponAvailable(WID_SUPERAXE))
    {
        dprint("mg3wtest: the Dawn of the Machine data is not there (no Super Axe)\n");
        return;
    }
    entity oldself = self, oldother = other, oldact = activator;
    if(request == 1) MG3_WT_Report();
    else if(request == 2) { mg3wt_step = 1; mg3wt_next = time; }
    else if(request == 3) { MG3_SuperAxe_SpawnAhead(MG3_WT_Player(), 48); mg3wt_step = 10; mg3wt_next = time + 0.5; }
    else if(request == 4) MG3_WT_Map2();
    else if(request == 5) MG3_WT_Holster();
    else if(request == 6) MG3_WT_AxeButtons();
    else if(request == 7) MG3_WT_FaceAxeButton();
    else if(request == 8) MG3_WT_AxeButtonState();
    else if(request == 9) MG3_WT_Laser();
    else if(request == 10) MG3_WT_LaserPickup();
    else if(request == 11) { MG3_SetBloodyBits(MG3_BLOODY_SG | MG3_BLOODY_SSG); MG3_WT_BloodyReport(); }
    else if(request == 12) MG3_WT_BloodyReport();
    else if(request == 13)
    {
        if(!MG3_WT_NextBloody())
        {
            entity p13 = MG3_WT_Player();
            makevectors(p13.v_angle_y * '0 1 0');
            entity os13 = self;
            self = spawn(); setorigin(self, p13.origin + v_forward * 48 + v_right * 24); weapon_bloody_sg();
            self = spawn(); setorigin(self, p13.origin + v_forward * 48 - v_right * 24); weapon_bloody_ssg();
            self = os13;
            dprint("mg3wtest: two bloody shotguns spawned ahead\n");
        }
        mg3wt_step = 30; mg3wt_next = time + 0.7;
    }
    else if(request == 14) { serverflags = serverflags | MG3_SF_BN_NEWGAME; MG3_WT_BloodyReport(); }
    else if(request == 15) { MG3_SetBloodyBits(0); MG3_WT_BloodyReport(); }
    self = oldself; other = oldother; activator = oldact;
};
