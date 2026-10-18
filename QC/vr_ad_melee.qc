// vr_ad_melee.qc -- AD support: Arcane Dimensions' melee monsters drop their weapons as they die, as the knights drop
// their swords (VR_DropKnightSword): the death fury its two swords (one for each hand), the death knight, the zombie
// knight and the death guard their swords, the hunter ogres their hammer and mace. Each is the knight's sword (WID_SWORD)
// of its kind (the weapon flags; the kinds, models and names: vr_ad_guns_defs.qc), swung as it is (W_SwordMelee: its own
// damage, vr_admelee_*); the hammer and the mace heavy (their mass, Weapon Weights slots 45-46: one hand swings them
// slowly; vr_admelee_heavy_1h of their damage, two hands all of it) and blunt. Their models are AD's own weapon models
// (their gibs), laid as the knight's sword by the engine (vr_admelee.cpp); AD's gibs throw no second one, and their
// corpses and ragdolls hold none (vr_monstermods.cpp hides them in their death frames; not the zombie knight's: its falls
// are its knockdown's too). vr_admelee_drops 0: none dropped.
//
// `vr_ad_test 110`..`115` (the fury, the death knight, the zombie knight, the death guard, the hammer's ogre, the mace's):
// the nearest such monster (the toolgun's, or `vr_test_spawn <n>; impulse 241`) is killed; its weapon (the fury's two)
// lies, is taken (the fury's into both hands) and swung once with one hand and once with two at a target. "admelee:"
// lines, then PASS or FAIL. `120`..`122`: AD's hell knight, the death lord, Quoth's death guard (they drop Quake VR's
// hell knight's sword; AD's knight its knight's sword: VR_ADMelee_KnightSwordOf). `116`..`119`: two of them put in
// your hands (to look at them; 119 the mace and the knight's own sword).

float vr_admelee_dropped; // the weapons the monsters dropped (the tests)

// The kind of weapon an AD monster of class `t` drops (0: none).
float(float t) VR_ADMelee_ForClass =
{
    if(t == ad_CT_MONDFURY)    { return VR_ADMELEE_FURY; }
    if(t == ad_CT_MONDKNIGHT)  { return VR_ADMELEE_DKNIGHT; }
    if(t == ad_CT_MONZOMBIEK)  { return VR_ADMELEE_ZKNIGHT; }
    if(t == ad_CT_MONDGUARD)   { return VR_ADMELEE_DGUARD; }
    if(t == ad_CT_MONHOGREHAM) { return VR_ADMELEE_HAMMER; }
    if(t == ad_CT_MONHOGREMAC) { return VR_ADMELEE_MACE; }
    return 0;
};

// A kind's model made and precached now (setmodel precaches one late: the hands' models are sent by their precache
// index; the model's making, from AD's file, takes a moment).
void(float xKind) VR_ADMelee_Ensure =
{
    if(xKind <= 0)
    {
        return;
    }
    entity e = spawn();
    setmodel(e, VR_ADMelee_Model(xKind));
    remove(e);
};

void(float xClass) VR_ADMelee_Prepare =
{
    if(cvar("vr_admelee_drops"))
    {
        VR_ADMelee_Ensure(VR_ADMelee_ForClass(xClass));
    }
};

void(float wid, float xFlags) VR_ADMelee_Placed =
{
    if(wid == WID_SWORD)
    {
        VR_ADMelee_Ensure(VR_ADMelee_Kind(xFlags));
    }
};

// DecodeLevelParms (after the weapons' flags): p's carried AD melee weapons (hands, holsters) brought from the last level
// made and precached in this one (VR_ADGun_EnsureCarried's).
void(entity p) VR_ADMelee_EnsureCarried =
{
    VR_ADMelee_Placed(p.weapon, p.weaponflags);
    VR_ADMelee_Placed(p.weapon2, p.weaponflags2);
    VR_ADMelee_Placed(p.holsterweapon0, p.holsterweaponflags0);
    VR_ADMelee_Placed(p.holsterweapon1, p.holsterweaponflags1);
    VR_ADMelee_Placed(p.holsterweapon2, p.holsterweaponflags2);
    VR_ADMelee_Placed(p.holsterweapon3, p.holsterweaponflags3);
    VR_ADMelee_Placed(p.holsterweapon4, p.holsterweaponflags4);
    VR_ADMelee_Placed(p.holsterweapon5, p.holsterweaponflags5);
};

// One weapon of `xKind` falling from self's hand on side `xSide` (1 right, -1 left).
void(float xKind, float xSide) VR_ADMelee_DropOne =
{
    makevectors(self.angles);
    vector at = self.origin + v_forward * 10 + v_right * (8 * xSide) + (VR_ADMelee_Heavy(xKind) ? '0 0 16' : '0 0 8');
    vector vel = v_forward * 60 + v_right * (xSide * 30 + crandom() * 20) + '0 0 120';
    entity w = CreateThrownWeapon(self, WID_SWORD, VR_ADMelee_Flags(xKind), WeaponInst_New(0),
        at, self.angles, vel, randomVec(60),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(WID_SWORD));
    if(w != world)
    {
        vr_admelee_dropped++;
    }
};

// AD's other sword users drop Quake VR's own knight's sword (AD's knight: 1) or hell knight's (2: AD's hell knight, the
// death lord, Quoth's death guard; their meshes id's and Quoth's hell knight's); 0: none.
float(float t) VR_ADMelee_KnightSwordOf =
{
    if(t == ad_CT_MONKNIGHT) { return 1; }
    if(t == ad_CT_MONHELLK || t == ad_CT_MONDLORD || t == ad_CT_MONDGUARDQ) { return 2; }
    return 0;
};

void() VR_ADMelee_Drop =
{
    if(!cvar("vr_admelee_drops"))
    {
        return;
    }
    float ks = VR_ADMelee_KnightSwordOf(self.ad_classtype);
    if(ks)
    {
        VR_DropKnightSword(ks == 2);
        vr_admelee_dropped++;
        if(self.ad_classtype == ad_CT_MONDLORD)
        {
            self.ad_gib2mdl = ad_string_null; // (the death lord's sword is its second gib, its head the first)
        }
        else
        {
            self.ad_gib1mdl = ad_string_null;
        }
        dprint(sprintf("admelee: %s drops a %s\n", self.classname, ks == 2 ? "hell knight's sword" : "knight's sword"));
        return;
    }
    float kind = VR_ADMelee_ForClass(self.ad_classtype);
    if(!kind)
    {
        return;
    }
    VR_ADMelee_DropOne(kind, 1);
    self.ad_gib1mdl = ad_string_null; // (it fell as ours: AD's gibs throw no second one)
    if(kind == VR_ADMELEE_FURY)
    {
        VR_ADMelee_DropOne(kind, -1); // (its left hand's)
        self.ad_gib2mdl = ad_string_null;
    }
    dprint(sprintf("admelee: %s drops its %s%s\n", self.classname, VR_ADMelee_Name(kind), kind == VR_ADMELEE_FURY ? " (two)" : ""));
};

// ---------------------------------------------------------------------------------------------------------------------
// Tests

float vr_admelee_step;
float vr_admelee_ok;
float vr_admelee_class; // the monster class tested
float vr_admelee_flags; // and the weapon flags of the sword it drops
float vr_admelee_count; // the drops seen before the kill
float vr_admelee_grip;  // vr_weapon_grip_mode before the test

void(string xWhat, float xOk) VR_ADMelee_Report =
{
    bprint(sprintf("admelee: %s: %s\n", xWhat, xOk ? "PASS" : "FAIL"));
    vr_admelee_ok = (vr_admelee_ok && xOk);
};

// `w` taken into `xHand` of `p` (VR_ADWeapons_Take's, either hand).
void(entity p, entity w, float xHand) VR_ADMelee_Take =
{
    VRGiveEntWeapon(p, xHand, WID_FIST, 0, 0);
    entity was = self;
    entity wasOther = other;
    p.touchinghand = xHand;
    w.handtouch_hand = xHand;
    w.handtouch_ent = p;
    other = p;
    self = w;
    float catching = forcegrabCatching;
    forcegrabCatching = TRUE;
    w.handtouch();
    forcegrabCatching = catching;
    self = was;
    other = wasOther;
};

// A sword of weapon flags `xFlags` made at `p` and taken into `xHand` (as a pickup: the hand's model and grip).
void(entity p, float xHand, float xFlags) VR_ADMelee_Hand =
{
    entity w = CreateThrownWeapon(p, WID_SWORD, xFlags, WeaponInst_New(0), p.origin, p.angles, '0 0 0', '0 0 0',
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        0);
    if(w != world)
    {
        VR_ADMelee_Take(p, w, xHand);
    }
};

// One blow of `xHand` of `p` at the test target, at full strength (`xBoth`: the other hand on its grip): the damage done.
float(entity p, float xHand, float xBoth) VR_ADMelee_Blow =
{
    entity t = vr_adgun_test_target;
    t.health = 100000;
    entity was = self;
    self = p;
    entity b = VR_Blow(xHand);
    makevectors(p.v_angle);
    b.mh_c_ent = t;
    b.mh_c_pos = t.origin + '0 0 16';
    b.mh_c_dir = v_forward;
    b.mh_c_normal = -v_forward;
    b.mh_strength = 1;
    b.mh_counter = 0;
    float bits = p.vrbits0;
    p.vrbits0 = xBoth ? (p.vrbits0 | QVR_VRBITS0_2H_AIMING) : (p.vrbits0 - (p.vrbits0 & QVR_VRBITS0_2H_AIMING));
    VRSetMeleeHitSoundPlayed(xHand, FALSE);
    vr_melee_blunt = VR_ADMelee_Heavy(VR_ADMelee_Kind(VRGetEntWeaponFlags(p, xHand))); // (as client.qc's blow)
    W_SwordMelee(xHand);
    vr_melee_blunt = FALSE;
    p.vrbits0 = bits;
    b.mh_c_ent = world;
    b.mh_c_pos = '0 0 0';
    self = was;
    return 100000 - t.health;
};

// The hold of `xHand`: its tip from the hand (the weapon's MuzzleAnchor), along its aim, right and up.
void(entity p, float xHand) VR_ADMelee_Hold =
{
    entity was = self;
    self = p;
    vector d = VRGetMuzzlePos(xHand) - VRGetHandPos(xHand);
    makevectors(VRGetWeaponFireRot(xHand));
    self = was;
    bprint(sprintf("admelee: hold (hand %g, %s): tip %.1f from the hand (%.1f ahead, %.1f right, %.1f up)\n", xHand,
        (xHand == cVR_OffHand ? p.weaponmodel2 : p.weaponmodel), vlen(d), d * v_forward, d * v_right, d * v_up));
};

float(float xKind) VR_ADMelee_ClassOf =
{
    if(xKind == VR_ADMELEE_FURY)    { return ad_CT_MONDFURY; }
    if(xKind == VR_ADMELEE_DKNIGHT) { return ad_CT_MONDKNIGHT; }
    if(xKind == VR_ADMELEE_ZKNIGHT) { return ad_CT_MONZOMBIEK; }
    if(xKind == VR_ADMELEE_DGUARD)  { return ad_CT_MONDGUARD; }
    if(xKind == VR_ADMELEE_HAMMER)  { return ad_CT_MONHOGREHAM; }
    return ad_CT_MONHOGREMAC;
};

// The last step: the holds, and the verdict.
void() VR_ADMelee_Finish =
{
    entity p = self.owner;
    entity was = self;
    self = p;
    float reach = vlen(VRGetMuzzlePos(cVR_MainHand) - VRGetHandPos(cVR_MainHand));
    self = was;
    VR_ADMelee_Report(sprintf("held a moment on (%s, its tip %.1f from the hand)", p.weaponmodel, reach),
        p.weaponmodel == WeaponIdToModel(WID_SWORD, vr_admelee_flags) && reach > 15);
    VR_ADMelee_Hold(p, cVR_MainHand);
    if(VR_ADMelee_Kind(vr_admelee_flags) == VR_ADMELEE_FURY)
    {
        VR_ADMelee_Hold(p, cVR_OffHand);
    }
    cvar_set("vr_weapon_grip_mode", ftos(vr_admelee_grip));
    bprint(sprintf("admelee: %s test %s\n", WeaponNameOf(WID_SWORD, vr_admelee_flags), vr_admelee_ok ? "PASS" : "FAIL"));
    remove(self);
};

void() VR_ADMelee_Think =
{
    entity p = self.owner;
    float k = VR_ADMelee_Kind(vr_admelee_flags);
    float want = k == VR_ADMELEE_FURY ? 2 : 1;
    if(vr_admelee_step == 0)
    {
        entity m = world;
        for(entity e = nextent(world); e; e = nextent(e))
        {
            if(e.ad_classtype == vr_admelee_class && e.health > 0 &&
               (!m || vlen(e.origin - p.origin) < vlen(m.origin - p.origin)))
            {
                m = e;
            }
        }
        VR_ADMelee_Report(sprintf("a living %s near you", m ? m.classname : "monster"), m != world);
        if(!m)
        {
            remove(self);
            return;
        }
        vr_admelee_count = vr_admelee_dropped;
        T_Damage(m, p, p, m.health + 5);
        VR_ADMelee_Report(sprintf("killed (health %g); dropped %g", m.health, vr_admelee_dropped - vr_admelee_count),
            m.health <= 0 && vr_admelee_dropped - vr_admelee_count == want);
        VR_ADMelee_Report(sprintf("AD's gibs throw none (gib1 '%s', gib2 '%s')", m.ad_gib1mdl, m.ad_gib2mdl),
            (vr_admelee_class == ad_CT_MONDLORD) ? (m.ad_gib2mdl == ad_string_null) :
                (m.ad_gib1mdl == ad_string_null && (want == 1 || m.ad_gib2mdl == ad_string_null)));
        vr_admelee_step = 1;
        self.nextthink = time + 1.5; // (the drop lands)
        return;
    }
    entity w1 = world;
    entity w2 = world;
    float n = 0;
    for(entity t = find(world, classname, "thrown_weapon"); t; t = find(t, classname, "thrown_weapon"))
    {
        if(t.weapon == WID_SWORD && t.weaponflags == vr_admelee_flags)
        {
            n++;
            w2 = w1;
            w1 = t;
        }
    }
    VR_ADMelee_Report(sprintf("its %s lying (%g, %s, model index %g, named %s)", WeaponNameOf(WID_SWORD, vr_admelee_flags), n,
        w1 ? w1.model : "none", w1 ? w1.modelindex : 0, w1 ? w1.netname : "-"), n >= want && w1.modelindex > 0 &&
        w1.model == WeaponIdToModel(WID_SWORD, vr_admelee_flags) && w1.netname == WeaponNameOf(WID_SWORD, vr_admelee_flags));
    if(!w1)
    {
        remove(self);
        return;
    }
    VRSetHandEmpty(p, cVR_OffHand);
    VR_ADMelee_Take(p, w1, cVR_MainHand);
    VR_ADMelee_Report(sprintf("taken into the main hand (weapon %g, kind %g, model %s)", VRGetEntWeapon(p, cVR_MainHand),
        VR_ADMelee_Kind(VRGetEntWeaponFlags(p, cVR_MainHand)), p.weaponmodel),
        VRGetEntWeapon(p, cVR_MainHand) == WID_SWORD &&
        (VRGetEntWeaponFlags(p, cVR_MainHand) & (QVR_WPNFLAG_SWORD_KIND | QVR_WPNFLAG_USE_SECONDARY_AMMO)) == vr_admelee_flags);
    float thing = VR_Melee_Thing(p, cVR_MainHand);
    VR_ADMelee_Report(sprintf("strikes as %s", thing == VR_MTHING_AXE ? "a head (blunt)" : "a sword"),
        thing == (VR_ADMelee_Heavy(k) ? VR_MTHING_AXE : VR_MTHING_SWORD));
    VR_ADGun_TestPlace(p, 32);
    float one = VR_ADMelee_Blow(p, cVR_MainHand, FALSE);
    float two = VR_ADMelee_Blow(p, cVR_MainHand, TRUE);
    if(VR_ADMelee_Heavy(k))
    {
        float share = cvar("vr_admelee_heavy_1h");
        VR_ADMelee_Report(sprintf("one hand %.1f, two hands %.1f (one hand's share %.2f, set %.2f)", one, two,
            two > 0 ? one / two : 0, share), two > 0 && fabs(one / two - share) < 0.02);
    }
    else
    {
        VR_ADMelee_Report(sprintf("one hand %.1f, two hands %.1f (a sword: the same)", one, two), one > 0 && one == two);
    }
    if(k == VR_ADMELEE_FURY)
    {
        VR_ADMelee_Report("its second sword lying", w2 != world);
        if(w2)
        {
            VR_ADMelee_Take(p, w2, cVR_OffHand);
            VR_ADMelee_Report(sprintf("dual-wielded (main %g kind %g, off %g kind %g)", VRGetEntWeapon(p, cVR_MainHand),
                VR_ADMelee_Kind(VRGetEntWeaponFlags(p, cVR_MainHand)), VRGetEntWeapon(p, cVR_OffHand),
                VR_ADMelee_Kind(VRGetEntWeaponFlags(p, cVR_OffHand))),
                VRGetEntWeapon(p, cVR_MainHand) == WID_SWORD && VRGetEntWeapon(p, cVR_OffHand) == WID_SWORD &&
                VR_ADMelee_Kind(VRGetEntWeaponFlags(p, cVR_OffHand)) == k);
            float off = VR_ADMelee_Blow(p, cVR_OffHand, FALSE);
            VR_ADMelee_Report(sprintf("the off hand's blow %.1f", off), off > 0);
        }
    }
    remove(vr_adgun_test_target);
    vr_adgun_test_target = world;
    self.think = VR_ADMelee_Finish;
    self.nextthink = time + 0.3; // (the hands' new weapons placed: their tips)
};

void(float request, entity p) AD_MeleeTest =
{
    if(!cvar("vr_ad_mounted"))
    {
        bprint("admelee: Arcane Dimensions' data is not loaded\n");
        return;
    }
    if(request >= 116 && request <= 119)
    {
        float a = request == 116 ? VR_ADMELEE_FURY : (request == 117 ? VR_ADMELEE_DKNIGHT : (request == 118 ?
            VR_ADMELEE_DGUARD : VR_ADMELEE_MACE));
        float b = request == 116 ? VR_ADMELEE_FURY : (request == 117 ? VR_ADMELEE_ZKNIGHT : (request == 118 ?
            VR_ADMELEE_HAMMER : 0));
        VR_ADMelee_Ensure(a);
        VR_ADMelee_Ensure(b);
        VR_ADMelee_Hand(p, cVR_MainHand, VR_ADMelee_Flags(a));
        if(b)
        {
            VR_ADMelee_Hand(p, cVR_OffHand, VR_ADMelee_Flags(b));
        }
        else
        {
            VR_ADMelee_Hand(p, cVR_OffHand, 0); // (the knight's own sword beside it, to compare)
        }
        VR_ADMelee_Hold(p, cVR_MainHand);
        if(b)
        {
            VR_ADMelee_Hold(p, cVR_OffHand);
        }
        return;
    }
    entity t = spawn();
    t.classname = "vr_admelee_test";
    t.owner = p;
    if(request >= 120)
    {
        vr_admelee_class = request == 120 ? ad_CT_MONHELLK : (request == 121 ? ad_CT_MONDLORD : ad_CT_MONDGUARDQ);
        vr_admelee_flags = QVR_WPNFLAG_USE_SECONDARY_AMMO; // (Quake VR's hell knight's sword)
    }
    else
    {
        vr_admelee_class = VR_ADMelee_ClassOf(request - 109);
        vr_admelee_flags = VR_ADMelee_Flags(request - 109);
    }
    vr_admelee_step = 0;
    vr_admelee_ok = TRUE;
    vr_admelee_grip = cvar("vr_weapon_grip_mode");
    cvar_set("vr_weapon_grip_mode", "1"); // (the weapons stay in hands not gripping; as it was again at the end)
    t.think = VR_ADMelee_Think;
    t.nextthink = time + 0.1;
};
