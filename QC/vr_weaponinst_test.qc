//
//
//
// ----------------------------------------------------------------------------
// Weapon instances: test steps (impulse 120; vr_weaponinst.qc)
// ----------------------------------------------------------------------------
//
// `set vr_test_weaponinst <step>; impulse 120` moves the weapons by the game's own functions, as the hands do (Debug >
// Tests > Weapon Instances): 0 lists what has which weapon (each weapon's id and magazine), 1 the main hand's weapon
// into a holster (`vr_test_weaponinst_slot`, a hotspot id: 0 the right hip), 2 drawn from it into the main hand, 3 the
// main hand's dropped, 4 the weapon lying nearest taken into the main hand, 5 handed off to the off hand (carried there),
// 6 taken back by the main hand, 7 the main hand's into the off hand (the hand switch), 8 back, 9 the off hand's
// dropped.

string(entity xInst, float xWid) VR_WeaponInst_TestDesc =
{
    if(xWid == WID_FIST)
    {
        return "-";
    }
    return sprintf("weapon %g #%g clip %g", xWid, xInst.wi_uid, xInst.wi_clip);
};

void() VR_WeaponInst_TestList =
{
    string s = sprintf("weaponinst: main %s | off %s", VR_WeaponInst_TestDesc(self.weaponinst, self.weapon),
        VR_WeaponInst_TestDesc(self.weaponinst2, self.weapon2));
    float hs;
    for(hs = QVR_HS_LEFT_SHOULDER_HOLSTER; hs <= QVR_HS_RIGHT_UPPER_HOLSTER; hs++)
    {
        if(hs != QVR_HS_HAND_SWITCH && getHolsterWeapon(hs) != WID_FIST)
        {
            s = strcat(s, sprintf(" | holster %g %s", hs, VR_WeaponInst_TestDesc(VRGetEntHolsterWeaponInst(self, hs),
                getHolsterWeapon(hs))));
        }
    }
    float records = 0;
    entity e = find(world, classname, "weapon_inst");
    while(e)
    {
        records++;
        e = find(e, classname, "weapon_inst");
    }
    e = find(world, classname, "thrown_weapon");
    while(e)
    {
        if(vlen(e.origin - self.origin) < 512)
        {
            s = strcat(s, sprintf(" | lying %s %.0fu", VR_WeaponInst_TestDesc(e.weaponinst, e.weapon), vlen(e.origin - self.origin)));
        }
        e = find(e, classname, "thrown_weapon");
    }
    bprint(sprintf("%s | records %g\n", s, records));
};

void() VR_WeaponInst_Test =
{
    float step = cvar("vr_test_weaponinst");
    float slot = cvar("vr_test_weaponinst_slot");
    if(!slot)
    {
        slot = QVR_HS_RIGHT_HIP_HOLSTER;
    }
    if(step == 1)
    {
        if(VRIsHandWeaponless(self, cVR_MainHand) || getHolsterWeapon(slot) != WID_FIST)
        {
            bprint("weaponinst test: no weapon in the main hand, or the holster is full\n");
            return;
        }
        VRHolsterHandWeapon(self, cVR_MainHand, slot);
    }
    else if(step == 2)
    {
        if(!VRIsHandEmpty(self, cVR_MainHand) || getHolsterWeapon(slot) == WID_FIST)
        {
            bprint("weaponinst test: the main hand isn't empty, or the holster is\n");
            return;
        }
        VRDrawHolsterWeapon(self, cVR_MainHand, slot);
    }
    else if(step == 3 || step == 9)
    {
        float hand = step == 3 ? cVR_MainHand : cVR_OffHand;
        DropWeaponInHand(self, hand, VRGetEntWeapon(self, hand), VRGetEntWeaponFlags(self, hand));
    }
    else if(step == 4)
    {
        entity best = world;
        float bestDist = 999999;
        entity e = find(world, classname, "thrown_weapon");
        while(e)
        {
            if(vlen(e.origin - self.origin) < bestDist)
            {
                best = e;
                bestDist = vlen(e.origin - self.origin);
            }
            e = find(e, classname, "thrown_weapon");
        }
        if(!best || !VRIsHandEmpty(self, cVR_MainHand))
        {
            bprint("weaponinst test: no weapon lying about, or the main hand isn't empty\n");
            return;
        }
        wpnthrow_handtouch_impl(cVR_MainHand, best, self, &self.weapon, &self.weaponflags);
    }
    else if(step == 5 || step == 7)
    {
        if(VRIsHandWeaponless(self, cVR_MainHand) || !VRIsHandEmpty(self, cVR_OffHand))
        {
            bprint("weaponinst test: no weapon in the main hand, or the off hand isn't empty\n");
            return;
        }
        if(step == 5) VRHandOffWeapon(self, cVR_MainHand);
        else          VRSwitchWeaponHands(self, cVR_MainHand);
    }
    else if(step == 6 || step == 8)
    {
        if(VRIsHandWeaponless(self, cVR_OffHand) || !VRIsHandEmpty(self, cVR_MainHand))
        {
            bprint("weaponinst test: no weapon in the off hand, or the main hand isn't empty\n");
            return;
        }
        if(step == 6) VRTakeCarriedWeapon(self, cVR_MainHand);
        else          VRSwitchWeaponHands(self, cVR_OffHand);
    }
    VR_WeaponInst_TestList();
};
