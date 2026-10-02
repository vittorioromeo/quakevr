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

float qvr_weaponinst_next; // The next id to try (saved with the game; carried to the next level: parm49).

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

// An id no record has: `xWant` if free (a weapon carried from the last level keeps its own), else the next free one.
float(float xWant) WeaponInst_FreeUid =
{
    if(xWant < 1 || WeaponInst_Find(xWant))
    {
        xWant = max(1, qvr_weaponinst_next);
        while(WeaponInst_Find(xWant))
        {
            xWant = xWant + 1;
        }
    }
    qvr_weaponinst_next = max(qvr_weaponinst_next, xWant + 1);
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
    return WeaponInst_New(xFrom.wi_clip);
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

// The hand's magazine set. A hand holding a weapon with no record yet (a save from before records) gets one.
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

// A record for a weapon `xWid` that has none (a save from before records): magazine `xClip`.
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

// After a load: every weapon a player holds or keeps, and every weapon prop, has its record (a save from before
// records: their magazines empty, reloaded as ever).
void() WeaponInst_EnsureAll =
{
    entity e = nextent(world);
    while(e)
    {
        if(e.classname == "player")
        {
            e.weaponinst = WeaponInst_Ensure(e.weaponinst, e.weapon, 0);
            e.weaponinst2 = WeaponInst_Ensure(e.weaponinst2, e.weapon2, 0);
            e.holsterweaponinst0 = WeaponInst_Ensure(e.holsterweaponinst0, e.holsterweapon0, 0);
            e.holsterweaponinst1 = WeaponInst_Ensure(e.holsterweaponinst1, e.holsterweapon1, 0);
            e.holsterweaponinst2 = WeaponInst_Ensure(e.holsterweaponinst2, e.holsterweapon2, 0);
            e.holsterweaponinst3 = WeaponInst_Ensure(e.holsterweaponinst3, e.holsterweapon3, 0);
            e.holsterweaponinst4 = WeaponInst_Ensure(e.holsterweaponinst4, e.holsterweapon4, 0);
            e.holsterweaponinst5 = WeaponInst_Ensure(e.holsterweaponinst5, e.holsterweapon5, 0);
        }
        else if(e.classname == "thrown_weapon" && !e.weaponinst)
        {
            e.weaponinst = WeaponInst_New(0);
        }
        e = nextent(e);
    }
};

// For debug lines: what a hand holds ("weapon 3 #12 clip 8": its id, its record's id, its magazine).
string(entity e, float xHand) VRWeaponInstDesc =
{
    entity r = VRGetEntWeaponInst(e, xHand);
    return sprintf("weapon %g #%g clip %g", xHand == cVR_OffHand ? e.weapon2 : e.weapon, r.wi_uid, r.wi_clip);
};
