#ifdef QVR_TESTS
// vr_dm_test 21 (vr_deathdrop_test.qc): a dying player's and bot's weapons dropped as they were (VR_DeathDrops,
// vr_wpnholster.qc; his note 2026-11-21: "Bots/players in MP who had a dagger in their holster actually drop a sword as
// a prop"). A bot, then the player, each with the dagger in both hands and five holsters (the off hand's turned blade
// down) and a knight's sword in the last holster, killed (PlayerDie): each weapon lies as a prop of its own kind (the
// daggers daggers, the sword a sword), with its own record (its id), the hand's state (the reverse grip) kept as the
// weapon's; the dead one's references cleared. Needs a free client slot and deathmatch or coop: run with
// `maxplayers 2; deathmatch 1` before the map. Verdict "deathdrops: N passed, M failed".

float vr_ddt_uid[8]; // the eight weapons' record ids, by slot (0, 1: the hands; 2..7: the holsters)

// The holster of slot `s` (2..7).
float(float s) VR_DDT_Holster =
{
    if(s == 2) { return QVR_HS_LEFT_SHOULDER_HOLSTER; }
    if(s == 3) { return QVR_HS_RIGHT_SHOULDER_HOLSTER; }
    if(s == 4) { return QVR_HS_LEFT_HIP_HOLSTER; }
    if(s == 5) { return QVR_HS_RIGHT_HIP_HOLSTER; }
    if(s == 6) { return QVR_HS_LEFT_UPPER_HOLSTER; }
    return QVR_HS_RIGHT_UPPER_HOLSTER;
};

// The flags slot `s` is given: the dagger (the off hand's reversed), the last holster a knight's sword (0).
float(float s) VR_DDT_Flags =
{
    if(s == 7) { return 0; }
    if(s == 1) { return VR_ADMelee_Flags(VR_SWORDKIND_DAGGER) | QVR_WPNFLAG_REVGRIP; }
    return VR_ADMelee_Flags(VR_SWORDKIND_DAGGER);
};

// `p` armed: the dagger in both hands and five holsters, a knight's sword in the last; their record ids kept.
void(entity p) VR_DDT_Arm =
{
    entity oself = self;
    self = p;
    VRGiveEntWeapon(p, cVR_MainHand, WID_SWORD, VR_DDT_Flags(0), 0);
    VRGiveEntWeapon(p, cVR_OffHand, WID_SWORD, VR_DDT_Flags(1), 0);
    vr_ddt_uid[0] = p.weaponinst.wi_uid;
    vr_ddt_uid[1] = p.weaponinst2.wi_uid;
    float s = 2;
    while(s < 8)
    {
        float hs = VR_DDT_Holster(s);
        entity r = WeaponInst_New(0);
        setHolsterWeapon(hs, WID_SWORD);
        setHolsterWeaponFlags(hs, VR_DDT_Flags(s));
        setHolsterWeaponModel(hs, WeaponIdToModel(WID_SWORD, VR_DDT_Flags(s)));
        VRSetEntHolsterWeaponInst(p, hs, r);
        vr_ddt_uid[s] = r.wi_uid;
        s = s + 1;
    }
    self = oself;
};

// `p` killed (PlayerDie), then each slot's weapon looked for among the props dropped this frame.
void(entity p, string who) VR_DDT_KillAndCheck =
{
    VR_DDT_Arm(p);
    entity oself = self;
    self = p;
    p.health = 0;
    PlayerDie();
    self = oself;

    float s = 0;
    while(s < 8)
    {
        entity w = find(world, classname, "thrown_weapon");
        entity hit = world;
        while(w)
        {
            if(w.vr_drop_born == time && w.weaponinst.wi_uid == vr_ddt_uid[s])
            {
                hit = w;
            }
            w = find(w, classname, "thrown_weapon");
        }
        float want = VR_ADMelee_Kind(VR_DDT_Flags(s));
        float rev = (VR_DDT_Flags(s) & QVR_WPNFLAG_REVGRIP) != 0;
        string what = want == VR_SWORDKIND_DAGGER ? "the dagger" : "the knight's sword";
        string from = s == 0 ? "the main hand" : (s == 1 ? "the off hand" : sprintf("holster %g", VR_DDT_Holster(s)));
        VR_Test_Check(hit != world && hit.weapon == WID_SWORD && VR_ADMelee_Kind(hit.weaponflags) == want &&
            ((hit.weaponflags & QVR_WPNFLAG_REVGRIP) != 0) == rev,
            sprintf("%s: %s from %s dropped as itself, its own record (#%g)", who, what, from, vr_ddt_uid[s]));
        if(hit == world || VR_ADMelee_Kind(hit.weaponflags) != want)
        {
            bprint(sprintf("deathdrops: %s slot %g: prop %g weapon %g flags %g\n", who, s, hit != world, hit.weapon,
                hit.weaponflags));
        }
        s = s + 1;
    }
    VR_Test_Check(!p.weaponinst && !p.weaponinst2 && !p.holsterweaponinst0 && !p.holsterweaponinst1 &&
        !p.holsterweaponinst2 && !p.holsterweaponinst3 && !p.holsterweaponinst4 && !p.holsterweaponinst5,
        sprintf("%s: the dead one's records moved to the drops (none left on him)", who));
};

void() VR_DeathDropTest_Run =
{
    entity pl = self;
    VR_Test_BeginFlags("deathdrops", VR_TEST_LOUD);
    VR_Test_Check(deathmatch || coop, "deathmatch or coop (a death drops the weapons)");
    float f = ClientNextAvailable();
    VR_Test_Check(f != -1, "a client slot free for a bot (maxplayers 2 or more)");
    if(f != -1)
    {
        BotConnect(0, 0, 1);
        entity bot = GetClientEntity(f);
        VR_DDT_KillAndCheck(bot, "the bot");
    }
    VR_DDT_KillAndCheck(pl, "the player");
    self = pl;
    VR_Test_Verdict();
};

void() VR_DeathDropTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_dm_test);
    if(ask == 21)
    {
        if(VR_Player() == world)
        {
            return; // (kept until the player is in)
        }
        cvar_set("vr_dm_test", "0");
        entity oself = self;
        self = VR_Player();
        VR_DeathDropTest_Run();
        self = oself;
    }
};
#endif
