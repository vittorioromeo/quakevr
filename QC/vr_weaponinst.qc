//
//
//
// ----------------------------------------------------------------------------
// Weapon instances
// ----------------------------------------------------------------------------
//
// Every weapon is one record: an entity of its own (classname "weapon_inst", no model: never sent, never drawn) with
// its id (.wi_uid) and its magazine (.wi_clip; a chainsaw's fuel). Whatever has the weapon refers to its record: a hand
// (.weaponinst, .weaponinst2), a holster (.holsterweaponinst0..5), the prop it lies as (a thrown, dropped or placed
// weapon: .weaponinst). Taking, throwing, holstering, drawing and handing a weapon over move the reference, never a
// copy, so its magazine and its identity (the clients' blood on it, vr_wounds.cpp) go with it. The weapon's id (WID_*)
// and its flags stay where it is (the hand's, the holster's, the prop's .weapon, .weaponflags).
//
// A record nothing refers to any more (the weapon gone: discarded, used up, replaced by a cheat, a removed prop's) the
// engine frees at the end of the frame, and tells the clients its id is gone (vr_weaponinst.cpp). Saves keep the records
// as entities; a level change carries the player's (their ids and magazines: parm31..38, parm41..49).

// AD support: Arcane Dimensions' upgrades this weapon has (AD's ad_IT_UPGRADE_AXE, _SSG, _LG bits: the Shadow Axe, the
// Widowmaker, the Plasma Gun), set by the upgrade's pickup on the weapon it gives (vr_ad_bridge.qc AD_UpgradeGiven): it
// goes with the weapon (hands, holsters, drops, saves; a level change: parm64), so a plain axe found later stays plain.
.float wi_adup;

// Incandescent blades (vr_hotblade.qc): a melee weapon held in a flame or lava glows and burns what it strikes. The heat
// is the weapon's (its record's), so it goes with it held, holstered, dropped and saved, and cools on its own clock.
.float wi_heat;    // the build-up in a flame, 0..1 (as it was at wi_heat_at; it falls off out of the flame)
.float wi_heat_at; // when wi_heat was last raised
.float wi_hot_end; // when it stops being incandescent (time; past: not)

// (vr_hotblade.qc, after vr_burning.qc: the melee and the damage call it.)
float(entity p, float xHand) VR_HotBlade_Extra;
void(entity p, float xHand, entity xTarg, float xAlive, vector xAt, vector xDir) VR_HotBlade_Struck;
void() VR_HotBlade_Frame;
float(entity xThing) VR_HotBlade_ThrownExtra;
void(entity xThing, entity xTarg, entity xBy, float xAlive, vector xAt, vector xDir) VR_HotBlade_ThrownStruck;
.float vr_hot_glow; // on a weapon prop (thrown_weapon): its glow 0..1 for the engine (VR_HotBlade_Lying; U_QVR_WEAPONUID)

nosave float vr_hot_any_until; // Until when some weapon may still be incandescent (vr_hotblade.qc)
float qvr_weaponinst_next; // The next id to try (saved with the game; carried to the next level: parm49).

// Every record's id is below this; 0: not known yet (a load or a new map: the progs' globals start again), and the first
// search walks the records once for it. Ids are written only by WeaponInst_Make (WeaponInst_FreeUid's), so it only
// grows; a freed record leaves it as it was (still above every id). A search for an id at or past it finds nothing
// without walking the records (PERF_DECISIONS.md, 10: each weapon made walked every record twice; a crowd's deaths
// dropping their guns walked them ~200 times in one frame).
nosave float qvr_weaponinst_idtop;

// The record with id `xUid`, else world.
entity(float xUid) WeaponInst_Find =
{
    entity e = find(world, classname, "weapon_inst");
    while(e)
    {
        if(e.wi_uid == xUid)
        {
            return e;
        }
        e = find(e, classname, "weapon_inst");
    }
    return world;
};

// Above every record's id (qvr_weaponinst_idtop, worked out once a level or a load).
float() WeaponInst_IdTop =
{
    if(qvr_weaponinst_idtop <= 0)
    {
        float top = 1;
        for(entity e = find(world, classname, "weapon_inst"); e; e = find(e, classname, "weapon_inst"))
        {
            top = max(top, e.wi_uid + 1);
        }
        qvr_weaponinst_idtop = top;
    }
    return qvr_weaponinst_idtop;
};

// Whether a record has id `xUid` (WeaponInst_Find's answer, without its walk past every id).
float(float xUid) WeaponInst_Taken =
{
    return xUid < WeaponInst_IdTop() && WeaponInst_Find(xUid) != world;
};

// An id no record has: `xWant` if free (a weapon carried from the last level keeps its own), else the next free one.
float(float xWant) WeaponInst_FreeUid =
{
    if(xWant < 1 || WeaponInst_Taken(xWant))
    {
        xWant = max(1, qvr_weaponinst_next);
        while(WeaponInst_Taken(xWant))
        {
            xWant = xWant + 1;
        }
    }
    qvr_weaponinst_next = max(qvr_weaponinst_next, xWant + 1);
    qvr_weaponinst_idtop = max(WeaponInst_IdTop(), xWant + 1);
    return xWant;
};

// A new weapon's record: magazine `xClip`, id `xUid` if free (0: a new one).
entity(float xClip, float xUid) WeaponInst_Make =
{
    entity r = spawn();
    r.classname = "weapon_inst";
    r.wi_uid = WeaponInst_FreeUid(xUid);
    r.wi_clip = xClip;
    return r;
};

entity(float xClip) WeaponInst_New =
{
    return WeaponInst_Make(xClip, 0);
};

// Another weapon like `xFrom` (its magazine, a new id): a quick slot's (QVR_WEAPONMODE_CYCLEQUICKSLOT) holster keeps a
// copy of the hand's weapon, and gives one back.
entity(entity xFrom) WeaponInst_Clone =
{
    entity r = WeaponInst_New(xFrom.wi_clip);
    r.wi_adup = xFrom.wi_adup; // (AD support: its upgrades too)
    r.wi_heat = xFrom.wi_heat; // (its heat: vr_hotblade.qc)
    r.wi_heat_at = xFrom.wi_heat_at;
    r.wi_hot_end = xFrom.wi_hot_end;
    return r;
};

// The magazine of record `xInst` set (none: nothing; world is never written).
void(entity xInst, float xClip) WeaponInst_SetClip =
{
    if(xInst)
    {
        xInst.wi_clip = xClip;
    }
};

// ---

entity(entity e, float xHand) VRGetEntWeaponInst =
{
    if(xHand == cVR_OffHand) return e.weaponinst2;
    else                     return e.weaponinst;
};

void(entity e, float xHand, entity xInst) VRSetEntWeaponInst =
{
    if(xHand == cVR_OffHand) e.weaponinst2 = xInst;
    else                     e.weaponinst = xInst;
};

// The hand's magazine (0: no weapon). Reading world's field is harmless (0).
float(entity e, float xHand) VRGetEntWeaponClip =
{
    if(xHand == cVR_OffHand) return e.weaponinst2.wi_clip;
    else                     return e.weaponinst.wi_clip;
};

// The hand's magazine set. A hand holding a weapon with no record yet gets one.
void(entity e, float xHand, float xClip) VRSetEntWeaponClip =
{
    entity r = VRGetEntWeaponInst(e, xHand);
    if(!r)
    {
        if((xHand == cVR_OffHand ? e.weapon2 : e.weapon) == WID_FIST)
        {
            return;
        }
        r = WeaponInst_New(0);
        VRSetEntWeaponInst(e, xHand, r);
    }
    r.wi_clip = xClip;
};

float(float xHand) VRGetWeaponClip =
{
    return VRGetEntWeaponClip(self, xHand);
};

void(float xHand, float xClip) VRSetWeaponClip =
{
    VRSetEntWeaponClip(self, xHand, xClip);
};

// A new weapon `xWid` in `xHand` (a cheat, a pickup of a weapon item, a replay's): its own record, magazine `xClip`;
// whatever the hand held is gone. WID_FIST: the hand emptied.
void(entity e, float xHand, float xWid, float xFlags, float xClip) VRGiveEntWeapon =
{
    if(xHand == cVR_OffHand) { e.weapon2 = xWid; e.weaponflags2 = xFlags; }
    else                     { e.weapon = xWid; e.weaponflags = xFlags; }
    VRSetEntWeaponInst(e, xHand, xWid == WID_FIST ? world : WeaponInst_New(xClip));
};

// The hand's weapon cycled to another, `xWid` (weapon cycling, W_ChangeWeapon: the weapons as an inventory): a weapon of
// its own (its own blood), its magazine the hand's as ever.
void(entity e, float xHand, float xWid) VRCycleEntWeaponInst =
{
    VRSetEntWeaponInst(e, xHand, xWid == WID_FIST ? world : WeaponInst_Clone(VRGetEntWeaponInst(e, xHand)));
};

// The weapon a hand holds moved to the other hand (its record with it): the other hand's weapon and flags set here.
void(entity e, float xFrom, float xTo) VRMoveEntWeaponInst =
{
    VRSetEntWeaponInst(e, xTo, VRGetEntWeaponInst(e, xFrom));
    VRSetEntWeaponInst(e, xFrom, world);
};

// The holster's record (world: none).
entity(entity e, float xHolster) VRGetEntHolsterWeaponInst =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  { return e.holsterweaponinst0; }
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) { return e.holsterweaponinst1; }
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       { return e.holsterweaponinst2; }
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      { return e.holsterweaponinst3; }
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     { return e.holsterweaponinst4; }
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    { return e.holsterweaponinst5; }
    return world;
};

void(entity e, float xHolster, entity xInst) VRSetEntHolsterWeaponInst =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  { e.holsterweaponinst0 = xInst; return; }
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) { e.holsterweaponinst1 = xInst; return; }
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       { e.holsterweaponinst2 = xInst; return; }
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      { e.holsterweaponinst3 = xInst; return; }
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     { e.holsterweaponinst4 = xInst; return; }
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    { e.holsterweaponinst5 = xInst; return; }
    bprint(sprintf("`VRSetEntHolsterWeaponInst`: invalid holster ID: %g\n", xHolster));
};

// A record for a weapon `xWid` that has none: magazine `xClip`.
entity(entity xInst, float xWid, float xClip) WeaponInst_Ensure =
{
    if(xInst || xWid == WID_FIST)
    {
        return xInst;
    }
    return WeaponInst_New(xClip);
};

// A weapon carried from the last level (DecodeLevelParms): its record again, magazine `xClip`, id `xUid` if free.
entity(float xWid, float xClip, float xUid) WeaponInst_Carried =
{
    return xWid == WID_FIST ? world : WeaponInst_Make(xClip, xUid);
};

// For debug lines: what a hand holds ("weapon 3 #12 clip 8": its id, its record's id, its magazine).
string(entity e, float xHand) VRWeaponInstDesc =
{
    entity r = VRGetEntWeaponInst(e, xHand);
    return sprintf("weapon %g #%g clip %g", xHand == cVR_OffHand ? e.weapon2 : e.weapon, r.wi_uid, r.wi_clip);
};

// Incandescent blades across a level change (parm65..72: the two hands' records, then the six holsters'): the seconds a
// blade has left to glow, in tenths, 0 = cold (the new level's clock restarts, so the end is carried as a time left).
float(entity r) WeaponInst_HotParm =
{
    return (r != world && r.wi_hot_end > time) ? ceil((r.wi_hot_end - time) * 10) : 0;
};

void(entity p) WeaponInst_PackHot =
{
    parm65 = WeaponInst_HotParm(p.weaponinst);
    parm66 = WeaponInst_HotParm(p.weaponinst2);
    parm67 = WeaponInst_HotParm(p.holsterweaponinst0);
    parm68 = WeaponInst_HotParm(p.holsterweaponinst1);
    parm69 = WeaponInst_HotParm(p.holsterweaponinst2);
    parm70 = WeaponInst_HotParm(p.holsterweaponinst3);
    parm71 = WeaponInst_HotParm(p.holsterweaponinst4);
    parm72 = WeaponInst_HotParm(p.holsterweaponinst5);
};

void(entity r, float xTenths) WeaponInst_UnpackHotOne =
{
    if(r != world && xTenths > 0)
    {
        r.wi_hot_end = time + xTenths / 10;
        r.wi_heat = 0;
        vr_hot_any_until = max(vr_hot_any_until, r.wi_hot_end);
    }
};

void(entity p) WeaponInst_UnpackHot =
{
    WeaponInst_UnpackHotOne(p.weaponinst, parm65);
    WeaponInst_UnpackHotOne(p.weaponinst2, parm66);
    WeaponInst_UnpackHotOne(p.holsterweaponinst0, parm67);
    WeaponInst_UnpackHotOne(p.holsterweaponinst1, parm68);
    WeaponInst_UnpackHotOne(p.holsterweaponinst2, parm69);
    WeaponInst_UnpackHotOne(p.holsterweaponinst3, parm70);
    WeaponInst_UnpackHotOne(p.holsterweaponinst4, parm71);
    WeaponInst_UnpackHotOne(p.holsterweaponinst5, parm72);
};
