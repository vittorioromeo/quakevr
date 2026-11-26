//
//
//
// ----------------------------------------------------------------------------
// AD's and Quoth's own humanoid variants as armed humanoids; the recolours by weapon (ARMED_MONSTERS.md, "AD's and
// Quoth's variants", "Recolours by weapon"; ROUND25.md "humrepl")
// ----------------------------------------------------------------------------
//
// The author (2026-11-21): "replace the 'hardcoded' humanoid variants introduced by these expansions with the closest
// corresponding armed humanoid version". vr_agrunt_repl_variants 1 (with vr_agrunt_repl and AD's scope, vr_agrunt_repl_ad:
// VR_AGR_InScope): AD's spawn function of each variant (Quoth's maps play by AD's rules) calls VR_HUM_Replace first,
// after AD's own spawn exceptions (ad_monster_checkspawn: nightmare, coop, nomonsters), and the variant becomes the armed
// row of its build holding its own gun (VR_HUM_Row, VR_HUM_Weapon), with the variant's health (its map's "health", else
// AD's): the soldiers (AD's mon_soldier is our grunt's mesh) grunts; the defender, the eliminator (mon_enforcer) and the
// pyro and fumigator (mon_pyro: an enforcer's build, armoured, scaled 1.35) enforcers; the crossbow knight a knight (with
// vr_agknight). It becomes that monster (its classname, spawn: the map's plain grunts' and knights' path in AD's maps, so
// its keys, targets, counts and waiting spawns are theirs), marked a replacement (.vr_agr: its trigger-spawn keys kept)
// that rolls nothing (its gun is the variant's). AD's point knights, death knights and the rest stay AD's (the table in
// ARMED_MONSTERS.md says why).
//
// Recolours: every armed humanoid, however armed (a variant, a replacement's roll, the toolgun, a wave, a map's
// "weapon"), gets its look from the weapon it holds as it is armed (VR_HUM_Look: its row and the weapon's category,
// .vr_hum_look; its worn pack takes it from it in the engine): rockets and grenades red, energy blue, flame orange,
// poison green; shells, nails, melee and the monster's own gun as they are. The engine draws it (vr_humlook.cpp, the
// table quakevr/humlook.txt).

float VR_HUM_CAT_EXPLOSIVE = 1; // the launchers (rockets, grenades, proximity mines)
float VR_HUM_CAT_ENERGY = 2;    // lightning, plasma, lasers
float VR_HUM_CAT_FLAME = 3;     // the flamethrower, the flare gun
float VR_HUM_CAT_POISON = 4;    // the fumigator's sprayer
float VR_HUM_CAT_SHELLS = 5;
float VR_HUM_CAT_NAILS = 6;
float VR_HUM_CAT_MELEE = 7;
float VR_HUM_CAT_OWN = 8;       // the monster's own gun (the grunt's rifle, a knight's sword; the enforcer's laser rifle is energy)

float vr_hum_made; // variants replaced so far (the tests)

// The variant's armed row.
float(float v) VR_HUM_Row =
{
    if(v == VR_HUM_V_CROSSBOW) { return VR_AM_KNIGHT; }
    return v >= VR_HUM_V_DEFENDER ? VR_AM_ENFORCER : VR_AM_GRUNT;
};

// Its gun ("weapon" code). The eliminator's own (WID_ADELIMINATOR) is gone: the centurion's plasma gun is the one.
float(float v) VR_HUM_Weapon =
{
    if(v == VR_HUM_V_ROCKET)     { return WID_ADROCKET; }
    if(v == VR_HUM_V_GRENADE)    { return WID_ADGRENADE; }
    if(v == VR_HUM_V_PLASMA)     { return WID_ADPLASMA; }
    if(v == VR_HUM_V_DEFENDER)   { return WID_ADDEFENDER; }
    if(v == VR_HUM_V_ELIMINATOR) { return WID_ADCENTURION; }
    if(v == VR_HUM_V_PYRO)       { return WID_ADPYRO; }
    if(v == VR_HUM_V_FUMIGATOR)  { return WID_ADFUMIGATOR; }
    return WID_ADCROSSBOW;
};

// Its health without a map's "health" (AD's own: ad_mon_soldier.qc, ad_mon_enforcer.qc, ad_mon_pyro.qc,
// ad_mon_dcrossbow.qc).
float(float v) VR_HUM_Health =
{
    if(v == VR_HUM_V_ROCKET)     { return 45; }
    if(v == VR_HUM_V_GRENADE)    { return 60; }
    if(v == VR_HUM_V_PLASMA)     { return 75; }
    if(v == VR_HUM_V_ELIMINATOR) { return 120; }
    if(v == VR_HUM_V_CROSSBOW)   { return 75; }
    return 100; // (the defender, the pyro, the fumigator)
};

string(float v) VR_HUM_ClassName =
{
    if(v == VR_HUM_V_ROCKET)     { return "monster_army_rocket"; }
    if(v == VR_HUM_V_GRENADE)    { return "monster_army_grenade"; }
    if(v == VR_HUM_V_PLASMA)     { return "monster_army_plasma"; }
    if(v == VR_HUM_V_DEFENDER)   { return "monster_defender"; }
    if(v == VR_HUM_V_ELIMINATOR) { return "monster_eliminator"; }
    if(v == VR_HUM_V_PYRO)       { return "monster_pyro"; }
    if(v == VR_HUM_V_FUMIGATOR)  { return "monster_fumigator"; }
    return "monster_dcrossbow";
};

// The variants are replaced here now (vr_agrunt_repl_variants, vr_agrunt_repl, AD's scope; a knight's with vr_agknight;
// the crossbow knight's only with vr_agrunt_repl_crossbow: his note 2026-11-26, off until it is a proper armed humanoid).
float(float v) VR_HUM_On =
{
    if(!cvar_hget(cvarh_vr_agrunt_repl_variants) || !cvar_hget(cvarh_vr_agrunt_repl) || !VR_AGR_InScope())
    {
        return FALSE;
    }
    if(v == VR_HUM_V_CROSSBOW && !cvar_hget(cvarh_vr_agrunt_repl_crossbow))
    {
        return FALSE;
    }
    return VR_HUM_Row(v) != VR_AM_KNIGHT || cvar_hget(cvarh_vr_agknight);
};

// AD's spawn function of variant `v` (self): its armed humanoid in its place. FALSE: not replaced (off, out of scope,
// VR_AM_Spawn's own call, a dummy's hold, MG3's infected), AD's own spawn goes on.
float(float v) VR_HUM_Replace =
{
    if(vr_am_spawning || vr_agr_hold > 0 || self.vr_mg3_infected || v < 1 || v >= VR_HUM_V_COUNT || !VR_HUM_On(v))
    {
        return FALSE;
    }
    float kind = VR_HUM_Row(v);
    float hp = self.health > 0 ? self.health : VR_HUM_Health(v); // (the map's "health", as AD reads it)
    VR_AM_Row(kind);
    self.classname = vr_am_classname; // (its row's monster from now on: monster_army, monster_enforcer, monster_knight)
    self.vr_agr = 1;                  // (a replacement: its map's trigger-spawn keys kept, VR_AM_Spawn)
    self.vr_hum_variant = v;
    self.weapon = VR_HUM_Weapon(v);
    self.weaponflags = 0;
    self.weapon2 = 0;
    self.weaponflags2 = 0;
    entity me = self;
    VR_AM_Spawn(kind);
    if(me.vr_ag)
    {
        me.health = hp;
        if(me.max_health > 0)
        {
            me.max_health = hp;
        }
        vr_hum_made = vr_hum_made + 1;
    }
    return TRUE;
};

// The category of weapon `wid` (flags `fl`) held by `e`.
float(entity e, float wid, float fl) VR_HUM_Category =
{
    if(wid < 0)
    {
        return 0;
    }
    // (fbmisc1123; his note 2026-11-23: an enforcer with its own laser rifle stayed undyed "although the weapon uses
    // cells") Energy first: a cell weapon is blue on anyone, the enforcer's own rifle too; only the other own guns (the
    // grunt's rifle, a knight's sword) keep their look.
    if(wid == WID_LIGHTNING || wid == WID_LASER_CANNON || wid == WID_MJOLNIR || wid == WID_ADPLASMA ||
       wid == WID_ADCENTURION || wid == WID_ADELIMINATOR || wid == WID_ENFRIFLE)
    {
        return VR_HUM_CAT_ENERGY;
    }
    if(VR_AKN_IsKnight(e.vr_ag_kind) ? wid == WID_SWORD && VR_ADMelee_Kind(fl) == 0 : wid == VR_AGR_NativeOf(e.vr_ag_kind))
    {
        return VR_HUM_CAT_OWN;
    }
    if(wid == WID_ROCKET_LAUNCHER || wid == WID_GRENADE_LAUNCHER || wid == WID_PROXIMITY_GUN || wid == WID_ADROCKET ||
       wid == WID_ADGRENADE)
    {
        return VR_HUM_CAT_EXPLOSIVE;
    }
    if(wid == WID_ADPYRO || wid == WID_FLAREGUN)
    {
        return VR_HUM_CAT_FLAME;
    }
    if(wid == WID_ADFUMIGATOR)
    {
        return VR_HUM_CAT_POISON;
    }
    if(wid == WID_NAILGUN || wid == WID_SUPER_NAILGUN || wid == WID_NAILPISTOL)
    {
        return VR_HUM_CAT_NAILS;
    }
    float c = VR_AG_Class(wid);
    if(c == VR_AG_CLASS_MELEE || c == VR_AG_CLASS_FISTS)
    {
        return VR_HUM_CAT_MELEE;
    }
    return VR_HUM_CAT_SHELLS; // (the shotguns, the grunt's rifle, the defender's, the revolver, the crossbow)
};

// `e`'s look by the weapons it holds now (as it is armed): the right hand's, or the left's ranged one beside a blade.
void(entity e) VR_HUM_Look =
{
    float c1 = e.vr_ag_prop ? VR_HUM_Category(e, e.vr_ag_wid, e.vr_ag_wflags) : 0;
    float c2 = e.vr_ag_prop2 ? VR_HUM_Category(e, e.vr_ag_wid2, e.vr_ag_wflags2) : 0;
    float c = c1;
    if(c == 0 || (c == VR_HUM_CAT_MELEE && c2 != 0 && c2 != VR_HUM_CAT_MELEE))
    {
        c = c2;
    }
    e.vr_hum_look = c > 0 ? e.vr_ag_kind * 16 + c : 0;
};
