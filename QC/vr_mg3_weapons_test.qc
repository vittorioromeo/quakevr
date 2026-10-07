// Dawn of the Machine weapons' tests (vr_mg3_wtest N, developer 1; Debug > Tests > Dawn of the Machine Weapons):
// docs/vr-port/MG3_PLAN.md M3-11..14, docs/vr-port/TESTING.md. Prints "mg3wtest: PASS/FAIL <what>" and a total.
//   1 report: the data, the hands and holsters, cells, the Super Axe's chain
//   2 the Super Axe's blows and burst (a few seconds: monsters spawned ahead, struck by MG3_SuperAxe_Strike)
//   3 its pickup: taken into the empty main hand, its target fired, spawnflag 128's silent drop (by 16 units here)
//   4 (Dawn of the Machine's map2) the map's weapon_mjolnir is a Super Axe: taken, its secret counted, dropped 2047 units
//   5 a Super Axe in the main hand and the first holster (then changelevel or save/load and 1)

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

void() MG3_WeaponsTestFrame =
{
    if(mg3wt_step == 10 && time >= mg3wt_next)
    {
        entity os1 = self, oo1 = other, oa1 = activator;
        mg3wt_step = 0;
        MG3_WT_Pickup(); // (request 3: its pickup settled first)
        self = os1; other = oo1; activator = oa1;
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
    if(request != 1 && !VR_Pack_WeaponAvailable(WID_SUPERAXE))
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
    self = oldself; other = oldother; activator = oldact;
};
