// vr_ad_melee.qc -- AD support: Arcane Dimensions' melee monsters drop their weapons as they die, as the knights drop
// their swords (VR_DropKnightSword): the death fury its two swords (one for each hand), the death knight, the zombie
// knight and the death guard their swords, the hunter ogres their hammer and mace. Each is the knight's sword (WID_SWORD)
// of its kind (the weapon flags; the kinds, models and names: vr_ad_guns_defs.qc), swung as it is (W_SwordMelee: its own
// damage, vr_admelee_*); the hammer and the mace heavy (their mass, Weapon Weights slots 45-46: one hand swings them
// slowly; vr_admelee_heavy_1h of their damage, two hands all of it) and blunt. Their models are AD's own weapon models
// (their gibs), laid as the knight's sword by the engine (vr_admelee.cpp); AD's gibs throw no second one, and their
// corpses and ragdolls hold none (vr_monstermods.cpp hides them in their death frames; the zombie knight's rig, whose falls
// are its knockdown's too). vr_admelee_drops 0: none dropped.
//
// `vr_ad_test 131`..`134`: the same for the four monster swords (Rogue's Phantom Swordsman, which falls as its very heavy
// sword and leaves no body; AD's Dark Prince, justice knight, judicator); `135` their holsters (as 127: the Phantom's and
// the judicator's too big); `136`, `137` two of them held.
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
    if(t == ad_CT_MONDPRINCE)  { return VR_ADMELEE_DPRINCE; }
    if(t == ad_CT_MONJUSTICE)  { return VR_ADMELEE_JUSTICE; }
    if(t == ad_CT_MONJUDGE)    { return VR_ADMELEE_JUDGE; }
    if(t == ad_CT_MONDSERG)    { return VR_ADMELEE_FLAIL; }
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
    if(xKind == VR_ADMELEE_FLAIL && cvar("vr_ad_mounted"))
    {
        precache_sound("gibs/gibchain.wav"); // (its chain's rattle: VR_Flail_Rattle)
    }
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

// The Phantom Swordsman (Rogue's monster_sword; AD's own, which AD sets up as its knight) is the sword itself: killed,
// it falls as the weapon (VR_ADMELEE_PHANTOM) and its body goes (rogue_invis_sw.qc sword_die removes it). TRUE: dropped.
float() VR_ADMelee_PhantomDrop =
{
    if(!cvar("vr_admelee_drops") || self.classname != "monster_sword")
    {
        return FALSE;
    }
    VR_ADMelee_DropOne(VR_ADMELEE_PHANTOM, 1);
    dprint(sprintf("admelee: %s drops its %s\n", self.classname, VR_ADMelee_Name(VR_ADMELEE_PHANTOM)));
    return TRUE;
};

void() VR_ADMelee_Drop =
{
    if(!cvar("vr_admelee_drops"))
    {
        return;
    }
    if(self.classname == "monster_sword") // (AD's: its class is its knight's, its sword no knight's sword)
    {
        if(VR_ADMelee_PhantomDrop())
        {
            self.ad_gib1mdl = self.ad_gib2mdl = self.ad_gib3mdl = ad_string_null;
            setmodel(self, ad_string_null);
        }
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
// The death sergeant's flail (VR_ADMELEE_FLAIL): its handle in the hand strikes nothing (VR_Melee_Thing); its spiked head
// hangs from the handle's end on a chain, a body of its own in the physics (the engine's vr_box3d_flail.cpp: a rope
// vr_flail_chain m long, the head vr_flail_mass kg), swung by the hand: a lazy wave barely lifts it, a full swing or a
// whirl lands it hard. Each frame its hardest hit on something (a monster, a ragdoll, a prop, the level) comes here at
// the head's speed into it (m/s, real time's: bullet time's slowing taken out) and its mass (and, held in both hands,
// vr_flail_2h more): dealt as the hand's sword blow (W_SwordMelee: blood, a blunt blow's sounds and wounds, dents, the
// impact rule for a prop, knockdowns), its damage vr_flail_damage x mass x (speed - vr_flail_min_speed), at most
// vr_flail_max_damage. vr_flail_debug 1: "flail:" lines.

float vr_flail_hits;      // the head's blows dealt (the tests)
float vr_flail_rattles;   // the chain's rattles played (the tests)
string VR_FLAIL_RATTLE = "gibs/gibchain.wav"; // (AD's: its flail's gib's)

// The chain rattling (the engine's, its head swung faster than 6 m/s, at most every 0.3 s; and each blow): AD's chain
// sound from `p`, louder the faster (`xSpeed` m/s).
void(entity p, float xHand, float xSpeed) VR_Flail_Rattle =
{
    if(!cvar("vr_flail_rattle") || !cvar("vr_ad_mounted"))
    {
        return;
    }
    vr_flail_rattles++;
    sound(p, CHAN_AUTO, VR_FLAIL_RATTLE, min(1, cvar("vr_flail_rattle") * (0.25 + xSpeed / 25)), ATTN_NORM);
};
float vr_flail_last;      // and the last one's damage
float vr_flail_last_speed;

void(entity p, float xHand, entity t, vector xAt, vector xDir, float xSpeed, float xMass) VR_Flail_Hit =
{
    float least = max(0, cvar("vr_flail_min_speed"));
    if(xSpeed <= least || p.health <= 0 || !(p.flags & FL_CLIENT))
    {
        return;
    }
    float xBoth = VRGet2HAiming(p) && VRIsHandEmpty(p, VRGetOtherHand(xHand)); // (the other hand on its handle)
    if(t != world && !t.takedamage && t.solid != SOLID_BSP)
    {
        return; // (a prop that takes no damage: the head knocks it, in the physics)
    }
    float dmg = cvar("vr_flail_damage") * xMass * (xSpeed - least) * (xBoth ? max(0, cvar("vr_flail_2h")) : 1);
    dmg = min(dmg, max(0, cvar("vr_flail_max_damage")));
    if(dmg <= 0)
    {
        return;
    }
    entity was = self;
    self = p;
    entity b = VR_Blow(xHand);
    entity cEnt = b.mh_c_ent;
    vector cPos = b.mh_c_pos;
    vector cDir = b.mh_c_dir;
    vector cNormal = b.mh_c_normal;
    float strength = b.mh_strength;
    float counter = b.mh_counter;
    float speed = b.mh_speed;
    float dmgSpeed = b.mh_dmgspeed;
    b.mh_c_ent = t;
    b.mh_c_pos = xAt;
    b.mh_c_dir = normalize(xDir);
    b.mh_c_normal = -b.mh_c_dir;
    b.mh_strength = 1;
    b.mh_counter = 0;
    b.mh_speed = xSpeed;
    b.mh_dmgspeed = xSpeed;
    VRSetMeleeHitSoundPlayed(xHand, FALSE);
    VR_SetHitKind(QVR_HITKIND_MELEE, xHand, '0 0 0', '0 0 0');
    vr_mdmg_core = 0; // (no strength cap: its damage is its own)
    vr_melee_blunt = TRUE;
    vr_sgib_at = xAt;
    vr_melee_blowhand = xHand + 1;
    vr_flail_damage = dmg;
    float before = t.health;
    VR_Flail_Rattle(p, xHand, xSpeed);
    W_SwordMelee(xHand);
    vr_flail_damage = 0;
    vr_melee_blowhand = 0;
    vr_melee_blunt = FALSE;
    vr_sgib_at = '0 0 0';
    VR_ClearHitKind();
    b.mh_c_ent = cEnt;
    b.mh_c_pos = cPos;
    b.mh_c_dir = cDir;
    b.mh_c_normal = cNormal;
    b.mh_strength = strength;
    b.mh_counter = counter;
    b.mh_speed = speed;
    b.mh_dmgspeed = dmgSpeed;
    self = was;
    vr_flail_hits++;
    vr_flail_last = dmg;
    vr_flail_last_speed = xSpeed;
    if(cvar("vr_flail_debug"))
    {
        bprint(sprintf("flail: hand %g hit %s at %.1f m/s, %.1f kg%s: damage %.1f (health %g before)\n", xHand,
            t == world ? "the level" : t.classname, xSpeed, xMass, xBoth ? ", two hands" : "", dmg, before));
    }
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
    vr_melee_blunt = VR_ADMelee_Blunt(VR_ADMelee_Kind(VRGetEntWeaponFlags(p, xHand))); // (as client.qc's blow)
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
    if(xKind == VR_ADMELEE_PHANTOM) { return -1; } // (Rogue's: found by its classname, monster_sword)
    if(xKind == VR_ADMELEE_DPRINCE) { return ad_CT_MONDPRINCE; }
    if(xKind == VR_ADMELEE_JUSTICE) { return ad_CT_MONJUSTICE; }
    if(xKind == VR_ADMELEE_JUDGE)   { return ad_CT_MONJUDGE; }
    if(xKind == VR_ADMELEE_FLAIL)   { return ad_CT_MONDSERG; }
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
            if((vr_admelee_class < 0 ? e.classname == "monster_sword" : e.ad_classtype == vr_admelee_class) && e.health > 0 &&
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
        if(k == VR_ADMELEE_PHANTOM)
        {
            VR_ADMelee_Report(sprintf("its body gone (model index %g)", m.modelindex), m.modelindex == 0);
        }
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
    if(VR_ADMelee_Flail(k))
    {
        VR_ADMelee_Report(sprintf("its handle strikes nothing (thing %g)", thing), thing == VR_MTHING_NONE);
        VR_ADMelee_Report(sprintf("no holster takes it (%g)", WeaponIdToHolsterable(WID_SWORD, vr_admelee_flags)),
            !WeaponIdToHolsterable(WID_SWORD, vr_admelee_flags));
        bprint(sprintf("admelee: %s test %s\n", WeaponNameOf(WID_SWORD, vr_admelee_flags), vr_admelee_ok ? "PASS" : "FAIL"));
        cvar_set("vr_weapon_grip_mode", ftos(vr_admelee_grip));
        remove(self);
        return;
    }
    VR_ADMelee_Report(sprintf("strikes as %s", thing == VR_MTHING_AXE ? "a head (blunt)" : "a sword"),
        thing == (VR_ADMelee_Blunt(k) ? VR_MTHING_AXE : VR_MTHING_SWORD));
    VR_ADMelee_Report(sprintf("holsterable %g, very heavy %g", VR_ADMelee_Holsterable(k), VR_ADMelee_VeryHeavy(p, cVR_MainHand)),
        VR_ADMelee_VeryHeavy(p, cVR_MainHand) == VR_ADMelee_Heavy(k));
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

// A hand's or a holster's weapon told: its kind and model; TRUE if it is an AD melee weapon on its own model.
float(string xWhere, float wid, float xFlags, string xModel) VR_ADMelee_Carried =
{
    float k = wid == WID_SWORD ? VR_ADMelee_Kind(xFlags) : 0;
    float ok = k > 0 && xModel == VR_ADMelee_Model(k);
    bprint(sprintf("admelee: %s: weapon %g, kind %g, model %s%s\n", xWhere, wid, k, xModel, ok ? " (its own)" : ""));
    return ok;
};

// `vr_ad_test 123` (the toolgun's path): the nearest AD melee weapon lying about (the toolgun's Weapons tab, a drop) is
// taken into the main hand, holstered at the left hip and drawn into the off hand (`125`: holstered at the right hip and
// kept there). `124`: what is carried now (after a save's load or a level change: the kinds kept, each on its own model).
// (Set vr_weapon_grip_mode 1 first: the mock's hands don't grip, and would drop what they hold.)
void(entity p, float xKeep) VR_ADMelee_TakeNearest =
{
    entity best = world;
    float bestD = 1000;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.classname == "thrown_weapon" && e.weapon == WID_SWORD && VR_ADMelee_Kind(e.weaponflags) > 0 &&
           vlen(e.origin - p.origin) < bestD)
        {
            best = e;
            bestD = vlen(e.origin - p.origin);
        }
    }
    if(!best)
    {
        bprint("admelee: no AD melee weapon lying near you: FAIL\n");
        return;
    }
    float flags = best.weaponflags;
    float ok = VR_ADMelee_Carried(sprintf("lying (%s)", best.classname), best.weapon, best.weaponflags, best.model);
    VR_ADMelee_Take(p, best, cVR_MainHand);
    ok = (VR_ADMelee_Carried("main hand", VRGetEntWeapon(p, cVR_MainHand), VRGetEntWeaponFlags(p, cVR_MainHand),
        p.weaponmodel) && ok);
    entity was = self;
    float hs = xKeep ? QVR_HS_RIGHT_HIP_HOLSTER : QVR_HS_LEFT_HIP_HOLSTER;
    self = p;
    setHolsterWeapon(hs, WID_FIST);
    setHolsterWeaponModel(hs, WeaponIdToModel(WID_FIST, 0));
    setHolsterWeaponFlags(hs, 0);
    VRSetEntHolsterWeaponInst(p, hs, world);
    VRHolsterHandWeapon(p, cVR_MainHand, hs);
    float kept = getHolsterWeaponFlags(hs);
    self = was;
    if(xKeep)
    {
        bprint(sprintf("admelee: %s taken, holstered at the right hip (kind %g): %s\n", WeaponNameOf(WID_SWORD, flags),
            VR_ADMelee_Kind(kept), (ok && kept == flags) ? "PASS" : "FAIL"));
        return;
    }
    ok = (VR_ADMelee_Carried("left hip holster", p.holsterweapon2, p.holsterweaponflags2, p.holsterweaponmodel2) && ok);
    self = p;
    VRSetHandEmpty(p, cVR_OffHand);
    VRDrawHolsterWeapon(p, cVR_OffHand, QVR_HS_LEFT_HIP_HOLSTER);
    self = was;
    ok = (VR_ADMelee_Carried("off hand (drawn)", VRGetEntWeapon(p, cVR_OffHand), VRGetEntWeaponFlags(p, cVR_OffHand),
        p.weaponmodel2) && VRGetEntWeaponFlags(p, cVR_OffHand) == flags && ok);
    bprint(sprintf("admelee: %s taken, holstered, drawn: %s\n", WeaponNameOf(WID_SWORD, flags), ok ? "PASS" : "FAIL"));
};

void(entity p) VR_ADMelee_ReportCarried =
{
    float n = 0;
    n = n + VR_ADMelee_Carried("main hand", p.weapon, p.weaponflags, p.weaponmodel);
    n = n + VR_ADMelee_Carried("off hand", p.weapon2, p.weaponflags2, p.weaponmodel2);
    n = n + VR_ADMelee_Carried("holster 0", p.holsterweapon0, p.holsterweaponflags0, p.holsterweaponmodel0);
    n = n + VR_ADMelee_Carried("holster 1", p.holsterweapon1, p.holsterweaponflags1, p.holsterweaponmodel1);
    n = n + VR_ADMelee_Carried("holster 2", p.holsterweapon2, p.holsterweaponflags2, p.holsterweaponmodel2);
    n = n + VR_ADMelee_Carried("holster 3", p.holsterweapon3, p.holsterweaponflags3, p.holsterweaponmodel3);
    n = n + VR_ADMelee_Carried("holster 4", p.holsterweapon4, p.holsterweaponflags4, p.holsterweaponmodel4);
    n = n + VR_ADMelee_Carried("holster 5", p.holsterweapon5, p.holsterweaponflags5, p.holsterweaponmodel5);
    bprint(sprintf("admelee: carried AD melee weapons on their own models: %g\n", n));
};

// `vr_ad_test 126` (the death fury's sword in the main hand: `vr_ad_test 116` first, a few frames before): its melee
// points on its curve (VR_Melee_At, vr_melee_curved_blades) against the straight line from the hand to its tip, and a
// sweep: the blade's points moved 24 units across its flat in one frame past a small box on the curve's bow, halfway
// (the points' own thickness, VR_MELEE_RADIUS, in): the curved points' sweeps strike it, the straight line's pass it by.
// "admelee curve:" lines (each share's point; the engine's drawn spine for the same shares: vr_admelee_spine), then
// PASS or FAIL.
void(entity p) VR_ADMelee_CurveTest =
{
    entity was = self;
    self = p;
    bprint(sprintf("admelee curve: main hand %s, curved blades %g\n", p.weaponmodel, cvar_hget(cvarh_vr_melee_curved_blades)));
    float ok = VR_Melee_Line(p, cVR_MainHand, VR_MTHING_SWORD);
    float kind = VR_ADMelee_Kind(VRGetEntWeaponFlags(p, cVR_MainHand));
    vector grip = vr_mline_grip, far = vr_mline_far;
    float bend = vr_mline_bend;
    vector across = normalize(vr_mline_bend_y);
    if(!bend)
    {
        makevectors(VRGetEntHandRot(p, cVR_MainHand));
        across = v_right * -1;
    }
    float most = 0;
    vector mostAt = grip;
    float f;
    for(f = 0.27; f < 1.01; f = f + 0.1)
    {
        vector at = VR_Melee_At(min(1, f));
        vector line = grip + (far - grip) * min(1, f);
        if(vlen(at - line) > most)
        {
            most = vlen(at - line);
            mostAt = at;
        }
    }
    float s;
    for(s = 0; s < 4; s++)
    {
        f = s == 0 ? 0.27 : (s == 1 ? 0.57 : (s == 2 ? 0.86 : 1));
        vector pt = VR_Melee_At(f);
        vector off = pt - (grip + (far - grip) * f);
        bprint(sprintf("admelee curve: %.2f at %.2f %.2f %.2f, off its line %.2f %.2f %.2f\n", f, pt_x, pt_y, pt_z, off_x,
            off_y, off_z));
    }
    // Two small boxes (1 unit, grown by the points' thickness) halfway along a 24-unit sweep across the flat: one just
    // outside the curve's bow (the blade strikes it, the straight line passed it by), one just inside it, off the line
    // the other way (the straight line struck it before the blade got there: "it hits sooner").
    vector d = across * 24;
    vector out = normalize(mostAt - (grip + (far - grip) * ((mostAt - grip) * (far - grip) / max(0.001, (far - grip) * (far - grip)))));
    vector outer = mostAt + out * 1.5 + d * 0.5;
    vector inner = mostAt - out * (most + 1.5) + d * 0.5;
    vector half = '1 1 1' * (1 + VR_MELEE_RADIUS);
    float curvedOuter = 0, curvedInner = 0, straightOuter = 0, straightInner = 0;
    VR_Melee_Layout(VR_MTHING_SWORD);
    for(float i = 0; i < vr_mpt_n; i++)
    {
        if(vr_mpt_kind[i] != VR_MPT_EDGE)
        {
            continue;
        }
        vector c0 = VR_Melee_At(vr_mpt_f[i]);
        vector l0 = grip + (far - grip) * vr_mpt_f[i];
        curvedOuter = curvedOuter + (VR_SegmentBox(c0, c0 + d, outer - half, outer + half) >= 0);
        curvedInner = curvedInner + (VR_SegmentBox(c0, c0 + d, inner - half, inner + half) >= 0);
        straightOuter = straightOuter + (VR_SegmentBox(l0, l0 + d, outer - half, outer + half) >= 0);
        straightInner = straightInner + (VR_SegmentBox(l0, l0 + d, inner - half, inner + half) >= 0);
    }
    self = was;
    float pass = (ok && (kind == VR_ADMELEE_FURY) && bend && (most > 3) && (curvedOuter > 0) && (curvedInner == 0) &&
                  (straightOuter == 0) && (straightInner > 0));
    bprint(sprintf("admelee curve: kind %g bent %g, %.2f units off the line at most\n", kind, bend, most));
    bprint(sprintf("admelee curve: the bow's outside struck by the curved points %g, the line's %g; its inside %g, %g: %s\n",
        curvedOuter, straightOuter, curvedInner, straightInner, pass ? "PASS" : "FAIL"));
};

// `vr_ad_test 127` (Weapon Mode Immersive): the hammer, the mace, then the fury's sword in the main hand let go at the
// empty left hip holster (VRLetGoAtHolster, as the hand's letting go there): the hammer and the mace are refused (the
// holster stays empty, the weapon falls: the hand empty, a weapon lying about), the sword goes in. "admelee holster:"
// lines, then PASS or FAIL.
void(entity p, float xNew) VR_ADMelee_HolsterTest =
{
    float pass = TRUE;
    float hs = QVR_HS_LEFT_HIP_HOLSTER;
    for(float k = 0; k < (xNew ? 4 : 3); k++)
    {
        float kind = (k == 0) ? VR_ADMELEE_HAMMER : ((k == 1) ? VR_ADMELEE_MACE : VR_ADMELEE_FURY);
        if(xNew) // (`vr_ad_test 135`: the four monster swords)
        {
            kind = VR_ADMELEE_PHANTOM + k;
        }
        float flags = VR_ADMelee_Flags(kind);
        VR_ADMelee_Ensure(kind);
        VR_ADMelee_Hand(p, cVR_MainHand, flags);
        entity was = self;
        self = p;
        setHolsterWeapon(hs, WID_FIST);
        setHolsterWeaponModel(hs, WeaponIdToModel(WID_FIST, 0));
        setHolsterWeaponFlags(hs, 0);
        VRSetEntHolsterWeaponInst(p, hs, world);
        float refused = vr_holster_refused;
        VRLetGoAtHolster(p, cVR_MainHand, hs, VRGetEntWeapon(p, cVR_MainHand), VRGetEntWeaponFlags(p, cVR_MainHand), TRUE);
        float inHolster = ((getHolsterWeapon(hs) == WID_SWORD) && (VR_ADMelee_Kind(getHolsterWeaponFlags(hs)) == kind));
        float handEmpty = VRIsHandEmpty(p, cVR_MainHand);
        entity lying = world;
        for(entity e = findradius(p.origin, 96); e; e = e.chain)
        {
            if((e.classname == "thrown_weapon") && (e.weapon == WID_SWORD) && (VR_ADMelee_Kind(e.weaponflags) == kind))
            {
                lying = e;
            }
        }
        self = was;
        float want = VR_ADMelee_Holsterable(kind);
        float good = want ? (inHolster && handEmpty) : (!inHolster && handEmpty && (lying != world) && (vr_holster_refused > refused));
        bprint(sprintf("admelee holster: %s: holsterable %g, in the holster %g, hand empty %g, lying %g, refused %g: %s\n",
            VR_ADMelee_Name(kind), want, inHolster, handEmpty, lying != world, vr_holster_refused - refused,
            good ? "ok" : "WRONG"));
        pass = (pass && good);
        if(lying != world)
        {
            remove(lying);
        }
    }
    bprint(sprintf("admelee holster: %s\n", pass ? "PASS" : "FAIL"));
};

void(float request, entity p) AD_MeleeTest =
{
    if(!cvar("vr_ad_mounted"))
    {
        bprint("admelee: Arcane Dimensions' data is not loaded\n");
        return;
    }
    if(request == 123 || request == 125) { VR_ADMelee_TakeNearest(p, request == 125); return; }
    if(request == 124) { VR_ADMelee_ReportCarried(p); return; }
    if(request == 126) { VR_ADMelee_CurveTest(p); return; }
    if(request == 127 || request == 135) { VR_ADMelee_HolsterTest(p, request == 135); return; }
    if(request >= 136 && request <= 139)
    {
        // The four monster swords held, two at a time (to look at them; the main hand's for vr_melee_sweep_test): 136
        // the Phantom Swordsman's and the justice knight's, 137 the Dark Prince's and the judicator's, 138 and 139 the
        // other way round.
        float m1 = request == 136 ? VR_ADMELEE_PHANTOM : (request == 137 ? VR_ADMELEE_DPRINCE :
            (request == 138 ? VR_ADMELEE_JUSTICE : VR_ADMELEE_JUDGE));
        float m2 = request == 136 ? VR_ADMELEE_JUSTICE : (request == 137 ? VR_ADMELEE_JUDGE :
            (request == 138 ? VR_ADMELEE_PHANTOM : VR_ADMELEE_DPRINCE));
        VR_ADMelee_Ensure(m1);
        VR_ADMelee_Ensure(m2);
        VR_ADMelee_Hand(p, cVR_MainHand, VR_ADMelee_Flags(m1));
        VR_ADMelee_Hand(p, cVR_OffHand, VR_ADMelee_Flags(m2));
        VR_ADMelee_Hold(p, cVR_MainHand);
        VR_ADMelee_Hold(p, cVR_OffHand);
        return;
    }
    if(request == 147)
    {
        VR_ADMelee_Hold(p, cVR_MainHand); // (the hold as drawn now: the tip from the hand)
        return;
    }
    if(request == 146)
    {
        // The flail in the main hand alone (its head's mock swings: vr_flail_test.cfg).
        VR_ADMelee_Ensure(VR_ADMELEE_FLAIL);
        VRSetHandEmpty(p, cVR_OffHand);
        VR_ADMelee_Hand(p, cVR_MainHand, VR_ADMelee_Flags(VR_ADMELEE_FLAIL));
        VR_ADMelee_Hold(p, cVR_MainHand);
        return;
    }
    if(request == 128 || request == 129)
    {
        // The hammer (128) or the mace (129) in the main hand alone (heavy handling's mock swings: vr_heavy_test.cfg).
        float heavy = request == 128 ? VR_ADMELEE_HAMMER : VR_ADMELEE_MACE;
        VR_ADMelee_Ensure(heavy);
        VR_ADMelee_Hand(p, cVR_MainHand, VR_ADMelee_Flags(heavy));
        VR_ADMelee_Hold(p, cVR_MainHand);
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
    if(request >= 120 && request <= 122)
    {
        vr_admelee_class = request == 120 ? ad_CT_MONHELLK : (request == 121 ? ad_CT_MONDLORD : ad_CT_MONDGUARDQ);
        vr_admelee_flags = QVR_WPNFLAG_USE_SECONDARY_AMMO; // (Quake VR's hell knight's sword)
    }
    else if(request == 145)
    {
        vr_admelee_class = ad_CT_MONDSERG; // (the death sergeant's flail)
        vr_admelee_flags = VR_ADMelee_Flags(VR_ADMELEE_FLAIL);
    }
    else
    {
        float kind = request >= 131 ? request - 124 : request - 109; // (131-134: the four monster swords, kinds 7-10)
        vr_admelee_class = VR_ADMelee_ClassOf(kind);
        vr_admelee_flags = VR_ADMelee_Flags(kind);
    }
    vr_admelee_step = 0;
    vr_admelee_ok = TRUE;
    vr_admelee_grip = cvar("vr_weapon_grip_mode");
    cvar_set("vr_weapon_grip_mode", "1"); // (the weapons stay in hands not gripping; as it was again at the end)
    t.think = VR_ADMelee_Think;
    t.nextthink = time + 0.1;
};
