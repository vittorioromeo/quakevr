#ifdef QVR_TESTS
// vr_dmrules_test.qc -- deathmatch's rules with VR weapons and bots (MP_PLAN B11 part 2), run on a deathmatch server with a
// human player and a FrikBot bot in it (mp_test.py dm: `vr_dm_test 1` on the server's console).
// - Items: in deathmatch 1 a weapon pickup a bot touches is its (the inventory), gone, coming back after its wait (30 s:
//   itemTouch's SUB_regen, pulled forward here) as it was; the weapon the bot's hand let go of for it is a thrown weapon
//   with deathmatch's removal time (10 s: wpnthrow_think, pulled forward), removed then. In deathmatch 2 a weapon pickup
//   stays: taken by a bot without it (it stays there for the others), not again by the bot that has it (no ammo twice).
// - Disarms between players and bots (vr_playerdisarm.qc, his call 2026-11-16): a player's shot across a bot's held
//   weapon knocks it out (PvP, vr_player_disarm_pvp 1: the clang and the sting); PvP off: not counted; a monster's
//   (any non-player's) shot counts only with PvE (vr_player_disarm, off by default); a bot's shot across the human's
//   held weapon knocks it out too ("Your weapon is knocked out of your hand!" at his client: the harness checks it).
// - The grab (vr_playerdisarm_grab.qc, B11 part 3): the bot's hands apart (bot_setvrfields); held in front of the human,
//   his empty hand closed on its gun: running, too fast to take hold of (the grab's speed is against the other's body
//   too); still, taken hold of, then pulled away from it: the shotgun in his hand.
// Chance certain (vr_player_disarm_chance 100) while it runs. Ends with `dmtest: N passed, M failed` (to every client,
// echoed on the server's console).

float vr_dmt_on;      // the running test's number
float vr_dmt_s;       // its state
float vr_dmt_n;       // frames the state has run
float vr_dmt_wait;    // frames left before the state goes on
float vr_dmt_chance;  // vr_player_disarm_chance before the test
float vr_dmt_pve;     // vr_player_disarm before
float vr_dmt_pvp;     // vr_player_disarm_pvp before
float vr_dmt_dm;      // deathmatch before
float vr_dmt_wid;     // the pickup's weapon
float vr_dmt_solid;   // ... its solid as the map placed it
float vr_dmt_dis0;    // vr_pd_disarms at the case's start
float vr_dmt_snd0;    // vr_pd_sounds then
float vr_dmt_hits0;   // vr_pd_hits then
entity vr_dmt_bot;    // the bot
entity vr_dmt_human;  // the human player
entity vr_dmt_item;   // the weapon pickup
entity vr_dmt_drop;   // the weapon the bot's hand let go of for it
entity vr_dmt_foe;    // a stand-in non-player shooter (PvE)
vector vr_dmt_spot;   // where the bot is held in front of the human (the grab)
float vr_dmt_held;    // frames the parry's hold has run in all (its restarts as he moves bounded by VR_DMT_GIVEUP)
float VR_DMT_GIVEUP = 216; // frames a waiting state may last (3 s at 72 Hz)
float VR_DMT_HOLD = 72;    // frames the bot is held there first: the lag compensation's past places of it there too

void(float s, float frames) VR_DMT_To =
{
    vr_dmt_s = s;
    vr_dmt_n = 0;
    vr_dmt_wait = frames;
};

void() VR_DMT_End =
{
    cvar_set("vr_player_disarm_chance", ftos(vr_dmt_chance));
    cvar_set("vr_player_disarm", ftos(vr_dmt_pve));
    cvar_set("vr_player_disarm_pvp", ftos(vr_dmt_pvp));
    deathmatch = vr_dmt_dm;
    vr_dml_test_off = FALSE;
    if(vr_dmt_human && vr_dmt_human.takedamage == DAMAGE_NO)
    {
        vr_dmt_human.takedamage = DAMAGE_AIM;
    }
    if(vr_dmt_bot && vr_dmt_bot.takedamage == DAMAGE_NO)
    {
        vr_dmt_bot.takedamage = DAMAGE_AIM;
    }
    if(vr_dmt_foe)
    {
        remove(vr_dmt_foe);
        vr_dmt_foe = world;
    }
    VR_Test_Verdict();
    cvar_set("vr_dm_test", "0");
    vr_dmt_on = 0;
    vr_dmt_s = -1;
};

// `toucher` touches the pickup (its body: a bot's touch, itemTouch then weapon_touch).
void(entity toucher) VR_DMT_Touch =
{
    entity os = self, oo = other;
    self = vr_dmt_item;
    other = toucher;
    vr_wpn_dropped = world;
    // (as the engine's body touch calls it, vr_physics.cpp setHandtouchParams: the fake hand)
    toucher.touchinghand = cVR_FakeHand;
    vr_dmt_item.handtouch_hand = cVR_FakeHand;
    vr_dmt_item.handtouch_ent = toucher;
    VR_Test_Print(sprintf("dmtest: %s touches %s (handtouch %s, th_die %s)\n", toucher.netname, vr_dmt_item.classname,
        vr_dmt_item.handtouch == itemTouch ? "itemTouch" : "other", vr_dmt_item.th_die == weapon_touch ? "weapon_touch" :
        "other"));
    vr_dmt_item.handtouch();
    vr_dmt_drop = vr_wpn_dropped;
    self = os;
    other = oo;
};

// A shot from `from` across `pl`'s main hand's held weapon (its line as the melee and the parry lay it out).
float(entity from, entity pl) VR_DMT_ShotAcross =
{
    if(!VR_Parry_WeaponLine(pl, cVR_MainHand))
    {
        return FALSE;
    }
    vector mid = (vr_parry_butt + vr_parry_end) * 0.5;
    makevectors(pl.v_angle);
    vector a = mid + v_right * 48, b = mid - v_right * 48;
    trace_fraction = 1;
    trace_ent = world;
    VR_PD_Shot(from, a, b, b - a, 100, QVR_AGS_PELLET);
    return TRUE;
};

// A shotgun into `pl`'s main hand when it holds nothing (a bot's chosen weapon, the human's spawn one: as they are).
void(entity pl) VR_DMT_Arm =
{
    if(!VRIsHandWeaponless(pl, cVR_MainHand))
    {
        return;
    }
    entity os = self;
    self = pl;
    VR_WeaponUtil_EntAddWeapon(pl, WID_SHOTGUN);
    VRGiveEntWeapon(pl, cVR_MainHand, WID_SHOTGUN, 0, 5);
    self = os;
};

void(float ask) VR_DMT_Run =
{
    if(vr_dmt_on != ask)
    {
        VR_Test_BeginFlags("dmtest", VR_TEST_LOUD | VR_TEST_ECHO);
        vr_dmt_on = ask;
        vr_dmt_chance = cvar_hget(cvarh_vr_player_disarm_chance);
        vr_dmt_pve = cvar_hget(cvarh_vr_player_disarm);
        vr_dmt_pvp = cvar_hget(cvarh_vr_player_disarm_pvp);
        vr_dmt_dm = deathmatch;
        VR_DMT_To(10, 0);
    }
    if(vr_dmt_s == 10) // the human, a living bot (one just added or respawning: waited for) and a weapon pickup
    {
        vr_dmt_n++;
        vr_dmt_bot = vr_dmt_human = vr_dmt_item = world;
        for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
        {
            if(p.ishuman && !vr_dmt_human) { vr_dmt_human = p; }
            if(!p.ishuman && (p.flags & FL_CLIENT) && p.health > 0 && !vr_dmt_bot) { vr_dmt_bot = p; }
        }
        entity e = nextent(world);
        while(e && !vr_dmt_item)
        {
            if(substring(e.classname, 0, 7) == "weapon_" && e.model != string_null && e.solid != SOLID_NOT &&
               e.th_die == weapon_touch)
            {
                vr_dmt_item = e;
            }
            e = nextent(e);
        }
        if((!vr_dmt_bot || !VR_Parry_WeaponLine(vr_dmt_bot, cVR_MainHand)) && vr_dmt_n < VR_DMT_GIVEUP)
        {
            return; // (a bot with a weapon in hand)
        }
        VR_Test_Check(deathmatch != 0 && vr_dmt_human && vr_dmt_bot && vr_dmt_item, sprintf(
            "a deathmatch (%g) with a human (%s), a bot (%s) and a weapon pickup (%s)", deathmatch, vr_dmt_human.netname,
            vr_dmt_bot.netname, vr_dmt_item.classname));
        if(deathmatch == 0 || !vr_dmt_human || !vr_dmt_bot || !vr_dmt_item)
        {
            VR_DMT_End();
            return;
        }
        // (the two kept alive while it runs: the bots' own fight killed the human between the grab's hold and its pull
        // once; a living player's takedamage is DAMAGE_AIM, put back at the end)
        vr_dmt_human.takedamage = DAMAGE_NO;
        vr_dmt_bot.takedamage = DAMAGE_NO;
        vr_dmt_wid = vr_dmt_item.weapon;
        vr_dmt_solid = vr_dmt_item.solid;
        cvar_set("vr_player_disarm_chance", "100"); // (certain)
        VR_DMT_To(0, 0);
    }
    if(vr_dmt_s < 0)
    {
        return;
    }
    vr_dmt_n++;
    if(vr_dmt_wait > 0)
    {
        vr_dmt_wait--;
        return;
    }
    entity bot = vr_dmt_bot, it = vr_dmt_item;
    if(vr_dmt_s == 0) // deathmatch 1: taken, gone, its respawn set
    {
        deathmatch = 1;
        VR_DMT_Touch(bot);
        VR_Test_Check(VR_WeaponUtil_EntHasWeapon(bot, vr_dmt_wid), sprintf("dm 1: the bot touching %s has it", it.classname));
        VR_Test_Check(it.model == string_null && it.solid == SOLID_NOT, "dm 1: the pickup is gone (no model, not solid)");
        VR_Test_Check((it.think2 == SUB_regen || it.think2 == VR_PickupObj_Regen) && fabs(it.nextthink2 - (time + it.wait)) < 0.1 && it.wait == 30,
            sprintf("dm 1: it comes back in its wait (%g s; in %.1f s)", it.wait, it.nextthink2 - time));
        VR_Test_Check(vr_dmt_drop == world || (vr_dmt_drop.classname == "thrown_weapon" &&
            fabs(GetThrownWeaponTimeout(vr_dmt_drop) - (time + 10)) < 0.1),
            sprintf("dm 1: the weapon the bot let go of for it (%s) is removed in 10 s", vr_dmt_drop.classname));
        it.nextthink2 = time + 0.05; // (its wait pulled forward)
        if(vr_dmt_drop)
        {
            SetThrownWeaponTimeout(vr_dmt_drop, time);
        }
        VR_DMT_To(1, 0);
        return;
    }
    if(vr_dmt_s == 1) // its respawn: as it was
    {
        if(it.model == string_null || (vr_dmt_drop && vr_dmt_drop.modelindex)) // (a freed edict: no model)
        {
            if(vr_dmt_n > VR_DMT_GIVEUP)
            {
                VR_Test_Check(FALSE, sprintf("timed out: the pickup back (%s), the dropped weapon removed (%s)",
                    it.model, vr_dmt_drop.classname));
                VR_DMT_End();
            }
            return;
        }
        VR_Test_Check(it.model == it.mdl && it.solid == vr_dmt_solid, sprintf(
            "dm 1: the pickup back as it was (%s, solid %g, was %g) after %g frames", it.model, it.solid, vr_dmt_solid,
            vr_dmt_n));
        VR_Test_Check(TRUE, "dm 1: the dropped weapon removed at its time");
        VR_DMT_To(2, 2);
        return;
    }
    if(vr_dmt_s == 2) // deathmatch 2: weapons stay
    {
        deathmatch = 2;
        VR_WeaponUtil_EntDelWeapon(bot, vr_dmt_wid);
        float aid = WeaponIdToAmmoId(vr_dmt_wid, 0);
        VR_DMT_Touch(bot);
        float ammo = VR_AmmoUtil_GetAmmoCountByAmmoId(bot, aid);
        VR_Test_Check(VR_WeaponUtil_EntHasWeapon(bot, vr_dmt_wid) && it.model != string_null && it.solid == vr_dmt_solid,
            "dm 2: the bot without it takes it, the pickup stays");
        VR_DMT_Touch(bot);
        VR_Test_Check(VR_AmmoUtil_GetAmmoCountByAmmoId(bot, aid) == ammo && it.model != string_null,
            sprintf("dm 2: having it, not taken again (ammo %g, %g)", ammo, VR_AmmoUtil_GetAmmoCountByAmmoId(bot, aid)));
        deathmatch = vr_dmt_dm;
        VR_DMT_To(3, 2);
        return;
    }
    if(vr_dmt_s == 3) // PvP: the human's shot across the bot's weapon
    {
        VR_DMT_Arm(bot);
        vr_dmt_dis0 = vr_pd_disarms;
        vr_dmt_snd0 = vr_pd_sounds;
        vr_dmt_hits0 = vr_pd_hits;
        float across = VR_DMT_ShotAcross(vr_dmt_human, bot);
        VR_Test_Check(across && vr_pd_hits > vr_dmt_hits0, sprintf("PvP: the human's shot across the bot's %s counts (%g hits)",
            WeaponIdToWeaponName(VRGetEntWeapon(bot, cVR_MainHand)), vr_pd_hits - vr_dmt_hits0));
        VR_DMT_To(4, 0);
        return;
    }
    if(vr_dmt_s == 4)
    {
        if(vr_pd_disarms == vr_dmt_dis0)
        {
            if(vr_dmt_n > VR_DMT_GIVEUP)
            {
                VR_Test_Check(FALSE, "timed out: the bot's weapon knocked out");
                VR_DMT_End();
            }
            return;
        }
        VR_Test_Check(TRUE, sprintf("PvP: the bot's weapon knocked out (%g frames)", vr_dmt_n));
        VR_Test_Check(vr_pd_sounds > vr_dmt_snd0, "PvP: the disarm sounds (the clang, the sting)");
        VR_DMT_To(5, 2);
        return;
    }
    if(vr_dmt_s == 5) // PvP off, then PvE (a non-player's shot): off by default, then on
    {
        VR_DMT_Arm(bot);
        cvar_set("vr_player_disarm_pvp", "0");
        vr_dmt_hits0 = vr_pd_hits;
        VR_DMT_ShotAcross(vr_dmt_human, bot);
        VR_Test_Check(vr_pd_hits == vr_dmt_hits0, "PvP off: the human's shot across the bot's weapon does nothing");
        cvar_set("vr_player_disarm_pvp", ftos(vr_dmt_pvp));
        if(!vr_dmt_foe)
        {
            vr_dmt_foe = spawn();
            vr_dmt_foe.classname = "vr_dmt_foe";
        }
        cvar_set("vr_player_disarm", "0");
        VR_DMT_ShotAcross(vr_dmt_foe, bot);
        VR_Test_Check(vr_pd_hits == vr_dmt_hits0, "PvE off (the default): a monster's shot across the bot's weapon does nothing");
        cvar_set("vr_player_disarm", "1");
        VR_DMT_ShotAcross(vr_dmt_foe, bot);
        VR_Test_Check(vr_pd_hits > vr_dmt_hits0, "PvE on: a monster's shot across it counts");
        cvar_set("vr_player_disarm", ftos(vr_dmt_pve));
        VR_DMT_To(6, 2);
        return;
    }
    if(vr_dmt_s == 6) // the bot's shot across the human's weapon
    {
        VR_DMT_Arm(vr_dmt_human);
        vr_dmt_dis0 = vr_pd_disarms;
        vr_dmt_hits0 = vr_pd_hits;
        float acrossHim = VR_DMT_ShotAcross(bot, vr_dmt_human);
        VR_Test_Check(acrossHim && vr_pd_hits > vr_dmt_hits0, sprintf("PvP: the bot's shot across the human's %s counts",
            WeaponIdToWeaponName(VRGetEntWeapon(vr_dmt_human, cVR_MainHand))));
        VR_DMT_To(7, 0);
        return;
    }
    if(vr_dmt_s == 7)
    {
        // (his own hand emptied: the disarms' count also counts a bot's from the cases before, its roll a frame late)
        if(vr_pd_disarms == vr_dmt_dis0 || !VRIsHandWeaponless(vr_dmt_human, cVR_MainHand))
        {
            if(vr_dmt_n > VR_DMT_GIVEUP)
            {
                VR_Test_Check(FALSE, "timed out: the human's weapon knocked out");
                VR_DMT_End();
            }
            return;
        }
        VR_Test_Check(VRIsHandWeaponless(vr_dmt_human, cVR_MainHand) || vr_pd_last_hand == cVR_MainHand,
            sprintf("PvP: the human's weapon knocked out by the bot (%g frames)", vr_dmt_n));
        makevectors(vr_dmt_human.angles_y * '0 1 0');
        vr_dmt_spot = vr_dmt_human.origin + v_forward * 40;
        VR_DMT_To(8, 0);
        return;
    }
    if(vr_dmt_s == 8) // the bot held in front of the human, facing him, its gun in hand (the lag compensation's past
    {                 // places of it there too: VR_DMT_HOLD frames)
        VR_DMT_Arm(bot);
        setorigin(bot, vr_dmt_spot);
        bot.velocity = '0 0 0';
        bot.v_angle = bot.angles = vectoangles(vr_dmt_human.origin - vr_dmt_spot);
        bot.fixangle = TRUE;
        if(vr_dmt_n < VR_DMT_HOLD)
        {
            return;
        }
        VR_DMT_To(9, 0);
        return;
    }
    if(vr_dmt_s == 9) // its hands apart; the human's empty hand closed on its gun: a running bot's is too fast to take
    {                 // hold of, a still one's taken hold of, then pulled away from it: his (the grab, PvP)
        makevectors(bot.v_angle);
        vector side = bot.handpos - bot.offhandpos;
        VR_Test_Check(vlen(side) > 10 && side * v_right > 0, sprintf(
            "the bot's hands apart, the main on its right (%.1f units apart, %.1f along its right)", vlen(side),
            side * v_right));
        VR_Test_Check(VRIsHandEmpty(vr_dmt_human, cVR_MainHand) && !VRIsHandWeaponless(bot, cVR_MainHand),
            "the human's main hand empty, the bot's holding its gun");
        entity os = self, him = vr_dmt_human;
        float lc = lagcompbegin(him);
        self = bot;
        float onLine = VR_Parry_WeaponLine(bot, cVR_MainHand);
        self = os;
        vector mid = (vr_parry_butt + vr_parry_end) * 0.5, chest = bot.origin + bot.view_ofs * 0.6;
        if(lc)
        {
            lagcompend();
        }
        vector wasPos = him.handpos, wasVel = him.handvel;
        float takes = vr_pdg_takes;
        him.handpos = mid;
        bot.velocity = v_forward * 320;
        float fast = onLine && VR_PDG_Try(him, cVR_MainHand);
        VR_Test_Check(onLine && !fast && him.vr_pdg_of == world, "a running bot's gun: too fast to take hold of");
        him.vr_pdg_of = world;
        bot.velocity = '0 0 0';
        float held = onLine && VR_PDG_Try(him, cVR_MainHand);
        VR_Test_Check(held && him.vr_pdg_of == bot, sprintf("the human's hand takes hold of the still bot's gun (%s)",
            him.vr_pdg_of.netname));
        vector wasBody = him.velocity;
        float slips = vr_pdg_slips;
        float gun = VRGetEntWeapon(bot, cVR_MainHand); // (the bot's chosen gun, or the shotgun VR_DMT_Arm gave it)
        string endedBy = "";
        if(held)
        {
            him.vr_pdg_since = time - 0.1; // (held a moment)
            vector away = normalize(mid - chest);
            him.handpos = mid + away;
            him.handvel = away * (cvar_hget(cvarh_vr_player_disarm_pull) + 1); // (m/s; his body still: the hand's pull)
            him.velocity = '0 0 0';
            VR_PDG_Hold(him);
            endedBy = sprintf("health %g, takes +%g, slips +%g, peak %.2f m/s, his body was at %.0f u/s, released %.2f s ago",
                him.health, vr_pdg_takes - takes, vr_pdg_slips - slips, him.vr_pdg_peak, vlen(wasBody),
                time - VR_HandGrabUtil_Impl_GetLastReleaseTime(him, cVR_MainHand));
        }
        him.handpos = wasPos;
        him.handvel = wasVel;
        him.velocity = wasBody;
        VR_Test_Check(vr_pdg_takes == takes + 1 && VRGetEntWeapon(him, cVR_MainHand) == gun &&
            VRIsHandWeaponless(bot, cVR_MainHand), sprintf("pulled away: the bot's %s in the human's hand (%s; %s)",
            WeaponIdToWeaponName(gun), WeaponIdToWeaponName(VRGetEntWeapon(him, cVR_MainHand)), endedBy));
        VR_DMT_End();
    }
};

// vr_dm_test 2 and 3 (MP B11 part 3, part 3): a bot's melee swing at the human -- its hand swings its sword across its
// front (bot_setvrfields), its blade's line kept each frame (VR_Parry_GuardFrame), its blow read by the directional parry
// as a player's swing (VR_DRP_PvpApplies). 2: the human's crowbar held low at his side (mp_dm's pose): the blow lands. 3:
// held upright across its way: parried, no damage. The human kept alive (health 999), the bot held in front of him
// facing him, its own attacks held off; its swing started as its button's (W_Fire_Melee).
float vr_dmt_blows0;  // vr_melee_blows before the swing
float vr_dmt_parry0;  // vr_parry_count then
float vr_dmt_read0;   // vr_drp_pvp_n then
float vr_dmt_swung;   // the swing's start (time)
float VR_DMT_SWING_DONE = 0.5; // s after the swing's start: its blow (0.2 s) long past, its hand back at rest

void() VR_DMT_HoldBot =
{
    entity bot = vr_dmt_bot;
    setorigin(bot, vr_dmt_spot);
    bot.velocity = '0 0 0';
    bot.v_angle = bot.angles = vectoangles(vr_dmt_human.origin - vr_dmt_spot);
    bot.fixangle = TRUE;
    bot.attack_finished = time + 1; // (its own attacks held off: the test's swing only)
    bot.button0 = 0;
    // (B14d: the other bots' too -- a shot knocking his crowbar out of his hand or him off his place made the blow's
    // outcome a matter of their fight)
    for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
    {
        if(p != bot && !p.ishuman && (p.flags & FL_CLIENT))
        {
            p.attack_finished = time + 1;
            p.button0 = 0;
        }
    }
    if(VRGetEntWeapon(bot, cVR_MainHand) != WID_SWORD)
    {
        VR_WeaponUtil_EntAddWeapon(bot, WID_SWORD);
        VRGiveEntWeapon(bot, cVR_MainHand, WID_SWORD, 0, 0);
    }
};

// The bots' off hands held off too (tests 2 and 3, the sword's blow alone): no punch and no reach for his weapon begun
// (B13d: the held bot's off hand punched him as its sword's blow was parried, its punch's damage read as the blow's
// landing: vr_dm_test 3's flake).
void() VR_DMT_HoldOffHands =
{
    for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
    {
        if(!p.ishuman && (p.flags & FL_CLIENT))
        {
            p.vr_botpunch_end = max(p.vr_botpunch_end, time);
            p.vr_botreach_next = max(p.vr_botreach_next, time + 1);
        }
    }
};

// Weapon `xWid` into `pl`'s main hand: another weapon there let go of first (a frame before: a weapon swapped in the
// hand is let go of at once), then it given into the empty hand, as VR_DMT_Arm gives.
void(entity pl, float xWid) VR_DMT_ArmWith =
{
    float wid = VRGetEntWeapon(pl, cVR_MainHand);
    if(wid == xWid)
    {
        return;
    }
    entity os = self;
    self = pl;
    if(!VRIsHandWeaponless(pl, cVR_MainHand))
    {
        DropWeaponInHand(pl, cVR_MainHand, wid, VRGetEntWeaponFlags(pl, cVR_MainHand));
    }
    else
    {
        VR_WeaponUtil_EntAddWeapon(pl, xWid);
        VRGiveEntWeapon(pl, cVR_MainHand, xWid, 0, 0);
        W_SetCurrentAmmoFor(cVR_MainHand);
    }
    self = os;
};
void(entity pl) VR_DMT_ArmCrowbar = { VR_DMT_ArmWith(pl, WID_CROWBAR); };

void(float ask) VR_DMT_Parry =
{
    if(vr_dmt_on != ask)
    {
        VR_Test_BeginFlags("dmtest", VR_TEST_LOUD | VR_TEST_ECHO);
        vr_dmt_on = ask;
        vr_dmt_chance = cvar_hget(cvarh_vr_player_disarm_chance);
        vr_dmt_pve = cvar_hget(cvarh_vr_player_disarm);
        vr_dmt_pvp = cvar_hget(cvarh_vr_player_disarm_pvp);
        vr_dmt_dm = deathmatch;
        cvar_set("vr_player_disarm_pvp", "0"); // (the swing's outcome alone: no roll on his crowbar)
        VR_DMT_To(20, 0);
    }
    if(vr_dmt_s == 20) // a living human and bot
    {
        vr_dmt_n++;
        vr_dmt_bot = vr_dmt_human = world;
        for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
        {
            if(p.ishuman && p.health > 0 && !vr_dmt_human) { vr_dmt_human = p; }
            if(!p.ishuman && (p.flags & FL_CLIENT) && p.health > 0 && !vr_dmt_bot) { vr_dmt_bot = p; }
        }
        if((!vr_dmt_bot || !vr_dmt_human) && vr_dmt_n < VR_DMT_GIVEUP)
        {
            return;
        }
        VR_Test_Check(deathmatch != 0 && vr_dmt_human && vr_dmt_bot, sprintf("a deathmatch (%g) with a living human (%s) "
            "and bot (%s)", deathmatch, vr_dmt_human.netname, vr_dmt_bot.netname));
        if(deathmatch == 0 || !vr_dmt_human || !vr_dmt_bot)
        {
            VR_DMT_End();
            return;
        }
        vr_dmt_human.health = 999; // (alive through the blow and the bots' own fight)
        vr_dmt_bot.takedamage = DAMAGE_NO;
        makevectors(vr_dmt_human.angles_y * '0 1 0');
        vr_dmt_spot = vr_dmt_human.origin + v_forward * 40;
        vr_dmt_held = 0;
        VR_DMT_To(21, 0);
    }
    if(vr_dmt_s < 0)
    {
        return;
    }
    vr_dmt_n++;
    if(vr_dmt_s == 21) // the bot held in front of him with its sword, his crowbar drawn in his hand's pose
    {
        vr_dmt_human.health = max(vr_dmt_human.health, 999);
        // (B14d: the spot follows him -- his pose's change can move his body a little after the spot was taken, a blow
        // from the old spot then reaching nothing -- and the hold starts again until he has stood still through it)
        makevectors(vr_dmt_human.angles_y * '0 1 0');
        vector spot = vr_dmt_human.origin + v_forward * 40;
        if(vlen(spot - vr_dmt_spot) > 1 && vr_dmt_held < VR_DMT_GIVEUP)
        {
            vr_dmt_n = 0;
        }
        vr_dmt_spot = spot;
        vr_dmt_held++;
        VR_DMT_HoldBot();
        VR_DMT_HoldOffHands();
        VR_DMT_ArmCrowbar(vr_dmt_human);
        if(vr_dmt_n < VR_DMT_HOLD || VRGetEntWeapon(vr_dmt_human, cVR_MainHand) != WID_CROWBAR)
        {
            return;
        }
        vr_dmt_blows0 = vr_melee_blows;
        vr_dmt_parry0 = vr_parry_count;
        vr_dmt_read0 = vr_drp_pvp_n;
        entity os = self;
        self = vr_dmt_bot;
        // (its button's swing: its hand swings it, the blow at player_axe*3)
        W_Fire_Melee(cVR_MainHand, &vr_dmt_bot.attack_finished);
        self = os;
        vr_dmt_swung = time;
        VR_Test_Check(vr_dmt_bot.vr_botswing == time, "the bot's sword swing begun (its hand swings it)");
        // (its walk this frame through the moves a player's walk runs: botmovethink, VR_PredictThink's flag)
        VR_Test_Check(vr_dmt_bot.vr_movejump == (cvar_hget(cvarh_vr_pmove_bots) != 0), sprintf(
            "the bot walks through the players' moves as vr_pmove_bots says (%g)", cvar_hget(cvarh_vr_pmove_bots)));
        VR_DMT_To(22, 0);
        return;
    }
    if(vr_dmt_s == 22)
    {
        VR_DMT_HoldBot();
        VR_DMT_HoldOffHands();
        if(time - vr_dmt_bot.vr_botswing > 0.19 && time - vr_dmt_bot.vr_botswing < 0.21)
        {
            // (at its blow: his crowbar still in his hand, and its blade's line ahead of it)
            makevectors(vr_dmt_bot.v_angle_y * '0 1 0');
            handlineat(vr_dmt_bot, cVR_MainHand, 0);
            VR_Test_Print(sprintf("dmtest: at its blow (%.3f s) its blade %.1f .. %.1f units ahead; his main hand's %s\n",
                time - vr_dmt_bot.vr_botswing, (vr_blow_a - vr_dmt_bot.origin) * v_forward,
                (vr_blow_b - vr_dmt_bot.origin) * v_forward, WeaponIdToWeaponName(VRGetEntWeapon(vr_dmt_human, cVR_MainHand))));
        }
        if(time - vr_dmt_swung < VR_DMT_SWING_DONE)
        {
            return;
        }
        // (its blow's own damage: the other bot's shots at him meanwhile not counted)
        float landed = vr_melee_blows > vr_dmt_blows0 && vr_melee_blow_by == vr_dmt_bot && vr_melee_blow_on == vr_dmt_human;
        float lost = landed ? vr_melee_blow_dmg : 0;
        float read = vr_drp_pvp_n > vr_dmt_read0;
        float parried = vr_parry_count > vr_dmt_parry0 && vr_parry_count_by == vr_dmt_bot;
        string got = sprintf("damage %g, read by its line %g, parried %g", lost, vr_drp_pvp_n - vr_dmt_read0, parried);
        VR_Test_Check(read, sprintf("its blow read by its blade's line as a player's swing (%s)", got));
        if(ask == 2)
        {
            VR_Test_Check(landed && lost > 0 && !parried, sprintf("his crowbar held at his side: the blow lands (%s)", got));
        }
        else
        {
            VR_Test_Check(parried && !landed, sprintf("his crowbar held upright across its way: parried, no damage (%s)",
                got));
        }
        vr_dmt_human.health = 100;
        VR_DMT_End();
    }
};

// vr_dm_test 4, 5 and 6 (MP B11 part 4): a bot's off hand as a player's, its AI deciding (frikbot/bot_fight.qc
// bot_offhand_ai), its hand through the players' code. 4: the bot held 40 units in front of the human, PvP disarms off:
// it punches him -- its fist thrust (bot_setvrfields), struck by the melee's sweep (PlayerVRMeleeImpl: a blow of its off
// hand on him, damage). 5: held 120 units off him, a nailgun lying 20 units in front of it: its open off hand goes to
// it and closes (the grip's press), the hand's touch takes it (wpnthrow_handtouch): the nailgun in its off hand. 6: held
// 40 units in front of him, PvP disarms on, his shotgun in his main hand: its off hand closes on his shotgun and pulls it
// away (vr_playerdisarm_grab.qc): the shotgun in its off hand, his hand empty.
float vr_dmt_takes0;  // vr_pdg_takes before
float vr_dmt_began;   // the bot's punch or reach seen begun
entity vr_dmt_gun;    // test 5's nailgun lying there

void() VR_DMT_HandsHold =
{
    VR_DMT_HoldBot();
    vr_dmt_bot.enemy = vr_dmt_on == 5 ? world : vr_dmt_human;
    vr_dmt_human.health = max(vr_dmt_human.health, 999);
};

void(float ask) VR_DMT_Hands =
{
    if(vr_dmt_on != ask)
    {
        VR_Test_BeginFlags("dmtest", VR_TEST_LOUD | VR_TEST_ECHO);
        vr_dmt_on = ask;
        vr_dmt_chance = cvar_hget(cvarh_vr_player_disarm_chance);
        vr_dmt_pve = cvar_hget(cvarh_vr_player_disarm);
        vr_dmt_pvp = cvar_hget(cvarh_vr_player_disarm_pvp);
        vr_dmt_dm = deathmatch;
        cvar_set("vr_player_disarm_pvp", ask == 6 ? "1" : "0");
        cvar_set("vr_player_disarm_chance", "0"); // (no shot's roll: the grab alone)
        vr_dmt_gun = world;
        VR_DMT_To(30, 0);
    }
    if(vr_dmt_s == 30) // a living human and bot
    {
        vr_dmt_n++;
        vr_dmt_bot = vr_dmt_human = world;
        for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
        {
            if(p.ishuman && p.health > 0 && !vr_dmt_human) { vr_dmt_human = p; }
            if(!p.ishuman && (p.flags & FL_CLIENT) && p.health > 0 && !vr_dmt_bot) { vr_dmt_bot = p; }
        }
        if((!vr_dmt_bot || !vr_dmt_human) && vr_dmt_n < VR_DMT_GIVEUP)
        {
            return;
        }
        VR_Test_Check(deathmatch != 0 && vr_dmt_human && vr_dmt_bot, sprintf("a deathmatch (%g) with a living human (%s) "
            "and bot (%s)", deathmatch, vr_dmt_human.netname, vr_dmt_bot.netname));
        if(deathmatch == 0 || !vr_dmt_human || !vr_dmt_bot)
        {
            VR_DMT_End();
            return;
        }
        vr_dmt_bot.takedamage = DAMAGE_NO;
        vr_dmt_held = 0;
        VR_DMT_To(31, 0);
    }
    if(vr_dmt_s < 0)
    {
        return;
    }
    vr_dmt_n++;
    entity bot = vr_dmt_bot;
    if(vr_dmt_s == 31) // held there, its off hand empty and at rest, his weapon in his hand (6)
    {
        makevectors(vr_dmt_human.angles_y * '0 1 0');
        vector spot = vr_dmt_human.origin + v_forward * (ask == 5 ? 120 : 40);
        if(vlen(spot - vr_dmt_spot) > 1 && vr_dmt_held < VR_DMT_GIVEUP)
        {
            vr_dmt_n = 0;
        }
        vr_dmt_spot = spot;
        vr_dmt_held++;
        VR_DMT_HandsHold();
        entity os = self;
        self = bot;
        if(!VRIsHandWeaponless(bot, cVR_OffHand))
        {
            DropWeaponInHand(bot, cVR_OffHand, VRGetEntWeapon(bot, cVR_OffHand), VRGetEntWeaponFlags(bot, cVR_OffHand));
        }
        self = os;
        bot.vr_botreach_next = time + 1; // (nothing begun before the test's start)
        bot.vr_botpunch_end = time;
        if(ask == 6)
        {
            VR_DMT_ArmWith(vr_dmt_human, WID_SHOTGUN);
        }
        if(vr_dmt_n < VR_DMT_HOLD || bot.vr_botreach || bot.vr_botpunch > 0 ||
           (ask == 6 && VRGetEntWeapon(vr_dmt_human, cVR_MainHand) != WID_SHOTGUN))
        {
            return;
        }
        if(ask == 5)
        {
            // (a nailgun let go of by his off hand, laid 20 units in front of the bot, towards him)
            os = self;
            self = vr_dmt_human;
            VR_WeaponUtil_EntAddWeapon(vr_dmt_human, WID_NAILGUN);
            VRGiveEntWeapon(vr_dmt_human, cVR_OffHand, WID_NAILGUN, 0, 0);
            DropWeaponInHand(vr_dmt_human, cVR_OffHand, WID_NAILGUN, VRGetEntWeaponFlags(vr_dmt_human, cVR_OffHand));
            self = os;
            vr_dmt_gun = world;
            for(entity g = nextent(world); g; g = nextent(g))
            {
                if((g.handtouch == wpnthrow_handtouch) && (g.weapon == WID_NAILGUN))
                {
                    vr_dmt_gun = g;
                }
            }
            VR_Test_Check(vr_dmt_gun != world, "a nailgun let go of, to lie in front of the bot");
            if(!vr_dmt_gun)
            {
                VR_DMT_End();
                return;
            }
            makevectors(bot.angles_y * '0 1 0');
            setorigin(vr_dmt_gun, bot.origin + v_forward * 20);
            vr_dmt_gun.velocity = '0 0 0';
        }
        vr_dmt_blows0 = vr_melee_blows;
        vr_dmt_takes0 = vr_pdg_takes;
        vr_dmt_began = 0;
        bot.vr_botreach_next = 0;
        bot.vr_botpunch_end = -1;
        vr_dmt_swung = time;
        VR_DMT_To(32, 0);
        return;
    }
    if(vr_dmt_s == 32)
    {
        VR_DMT_HandsHold();
        if(ask == 6)
        {
            float hw = VRGetEntWeapon(vr_dmt_human, cVR_MainHand);
            VR_DMT_ArmWith(vr_dmt_human, ((hw == WID_FIST) && (vr_pdg_takes == vr_dmt_takes0)) ? WID_SHOTGUN : hw);
        }
        if(bot.vr_botpunch > 0 && !vr_dmt_began)
        {
            vr_dmt_began = time;
            VR_Test_Print(sprintf("dmtest: the bot punches %s (%.3f s in)\n", bot.vr_botpunch_of.netname, time - vr_dmt_swung));
        }
        if(bot.vr_botreach && !vr_dmt_began)
        {
            vr_dmt_began = time;
            VR_Test_Print(sprintf("dmtest: the bot's off hand reaches for %s (%.3f s in)\n", bot.vr_botreach.classname,
                time - vr_dmt_swung));
        }
        float done = FALSE;
        if(ask == 4) { done = ((vr_dmt_began != 0) && (bot.vr_botpunch <= 0)); }
        if(ask == 5) { done = VRGetEntWeapon(bot, cVR_OffHand) == WID_NAILGUN; }
        if(ask == 6) { done = ((vr_pdg_takes > vr_dmt_takes0) && (VRGetEntWeapon(bot, cVR_OffHand) == WID_SHOTGUN)); }
        if(!done && time - vr_dmt_swung < 4)
        {
            return;
        }
        VR_Test_Check(vr_dmt_began != 0, sprintf("its off hand %s", ask == 4 ? "punches him" :
            (ask == 5 ? "reaches for the nailgun" : "reaches for his shotgun")));
        if(ask == 4)
        {
            float landed = (vr_melee_blows > vr_dmt_blows0) && (vr_melee_blow_by == bot) && (vr_melee_blow_on == vr_dmt_human);
            VR_Test_Check(landed && (vr_melee_blow_dmg > 0) && (vr_melee_blow_hand == cVR_OffHand + 1), sprintf(
                "its fist's blow lands on him through the melee's sweep (blows %g, damage %g, hand %g)",
                vr_melee_blows - vr_dmt_blows0, landed ? vr_melee_blow_dmg : 0, vr_melee_blow_hand - 1));
        }
        if(ask == 5)
        {
            VR_Test_Check(done && !vr_dmt_gun.modelindex, sprintf("its hand takes it, the gun gone from the floor (off hand: %s)",
                WeaponIdToWeaponName(VRGetEntWeapon(bot, cVR_OffHand))));
            if(vr_dmt_gun.modelindex)
            {
                remove(vr_dmt_gun);
            }
        }
        if(ask == 6)
        {
            VR_Test_Check(done && VRIsHandWeaponless(vr_dmt_human, cVR_MainHand), sprintf(
                "its hand closes on his shotgun and pulls it away (taken %g; its off hand: %s; his main hand: %s)",
                vr_pdg_takes - vr_dmt_takes0, WeaponIdToWeaponName(VRGetEntWeapon(bot, cVR_OffHand)),
                WeaponIdToWeaponName(VRGetEntWeapon(vr_dmt_human, cVR_MainHand))));
        }
        vr_dmt_human.health = 100;
        VR_DMT_End();
    }
};

// vr_dm_test 7 (MP B13b): deathmatch's caps on loose gibs and dropped weapons (vr_dmloose.qc). Under caps of 4 gibs and 3
// weapons, 8 gibs (6 made long ago, the oldest of all, and 2 now) and 5 weapons (3 going soonest, 2 with a long time
// left) laid by the human: out of deathmatch nothing goes (single player and coop as they were); in it the oldest gibs go,
// the youngest stay, at most the cap are left, and the weapons the same.
// vr_dm_test 8: a respawn with every start taken goes beside one (client.qc VR_DMBesideSpot). Only the first start
// counted (the others renamed for the test), the human standing on it: the bot respawned (PutClientInServer) is put 48-144
// units beside it, clear of him, and nobody is telefragged.
entity vr_dmt_gibs[8];
entity vr_dmt_wpns[5];
float vr_dmt_gmax;  // vr_dm_gibs_max before
float vr_dmt_wmax;  // vr_dm_weapons_max before
float vr_dmt_wgone; // vr_dml_weapons_gone at the start
entity vr_dmt_start; // test 8's start
float vr_dmt_hp;     // the human's health then

void() VR_DMT_LayLoose =
{
    makevectors(vr_dmt_human.angles_y * '0 1 0');
    vector at = vr_dmt_human.origin + v_forward * 64;
    for(float i = 0; i < 8; i++)
    {
        entity g = spawn();
        setmodel(g, "progs/gib1.mdl");
        setorigin(g, at + v_right * (i * 8 - 28));
        g.movetype = MOVETYPE_BOUNCE;
        g.think = SUB_Remove;
        g.nextthink = time + 30;
        VR_MakeGibGrabbable(g);
        g.vr_gib_made = time; // (6 the oldest of all, 2 the youngest)
        if(i < 6)
        {
            g.vr_gib_made = time - 1000 + i;
        }
        vr_dmt_gibs[i] = g;
    }
    for(float k = 0; k < 5; k++)
    {
        entity w = spawn();
        MakeThrown(w, vr_dmt_human, WID_SHOTGUN, 0, world, at + v_forward * 24 + v_right * (k * 12 - 24), '0 0 0',
            '0 0 0', '0 0 0', FALSE, FALSE, 0);
        SetThrownWeaponTimeout(w, time + 100);
        if(k < 3)
        {
            SetThrownWeaponTimeout(w, time + 0.5 + k * 0.01);
        }
        vr_dmt_wpns[k] = w;
    }
};

// How many of test 7's gibs (from `a` to `b`) and weapons (`wa` to `wb`) are still there.
float(float a, float b) VR_DMT_GibsLeft =
{
    float n = 0;
    for(float i = a; i < b; i++)
    {
        if(vr_dmt_gibs[i].modelindex != 0 && vr_dmt_gibs[i].vr_gib)
        {
            n++;
        }
    }
    return n;
};

float(float a, float b) VR_DMT_WeaponsLeft =
{
    float n = 0;
    for(float i = a; i < b; i++)
    {
        if(vr_dmt_wpns[i].modelindex != 0 && vr_dmt_wpns[i].classname == "thrown_weapon")
        {
            n++;
        }
    }
    return n;
};

void(float ask) VR_DMT_Loose =
{
    if(vr_dmt_on != ask)
    {
        VR_Test_BeginFlags("dmtest", VR_TEST_LOUD | VR_TEST_ECHO);
        vr_dmt_on = ask;
        vr_dmt_chance = cvar_hget(cvarh_vr_player_disarm_chance);
        vr_dmt_pve = cvar_hget(cvarh_vr_player_disarm);
        vr_dmt_pvp = cvar_hget(cvarh_vr_player_disarm_pvp);
        vr_dmt_dm = deathmatch;
        vr_dmt_gmax = cvar_hget(cvarh_vr_dm_gibs_max);
        vr_dmt_wmax = cvar_hget(cvarh_vr_dm_weapons_max);
        vr_dmt_start = world;
        VR_DMT_To(40, 0);
    }
    if(vr_dmt_s == 40) // a living human and bot
    {
        vr_dmt_n++;
        vr_dmt_bot = vr_dmt_human = world;
        for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
        {
            if(p.ishuman && p.health > 0 && !vr_dmt_human) { vr_dmt_human = p; }
            if(!p.ishuman && (p.flags & FL_CLIENT) && p.health > 0 && !vr_dmt_bot) { vr_dmt_bot = p; }
        }
        if((!vr_dmt_bot || !vr_dmt_human) && vr_dmt_n < VR_DMT_GIVEUP)
        {
            return;
        }
        VR_Test_Check(vr_dmt_dm != 0 && vr_dmt_human && vr_dmt_bot, sprintf("a deathmatch (%g) with a living human (%s) "
            "and bot (%s)", vr_dmt_dm, vr_dmt_human.netname, vr_dmt_bot.netname));
        if(vr_dmt_dm == 0 || !vr_dmt_human || !vr_dmt_bot)
        {
            VR_DMT_End();
            return;
        }
        if(ask == 7)
        {
            cvar_set("vr_dm_gibs_max", "4");
            cvar_set("vr_dm_weapons_max", "3");
            vr_dml_test_off = TRUE; // (the caps as out of deathmatch first: nothing goes; B25b: not `deathmatch` 0, a
                                    // respawn then restarted the level)
            VR_DMT_LayLoose();
            VR_DMT_To(41, 3);
        }
        else
        {
            VR_DMT_To(50, 0);
        }
        return;
    }
    if(vr_dmt_s < 0)
    {
        return;
    }
    vr_dmt_n++;
    if(vr_dmt_wait > 0)
    {
        vr_dmt_wait--;
        return;
    }
    if(vr_dmt_s == 41) // out of deathmatch: all there
    {
        VR_Test_Check(VR_DMT_GibsLeft(0, 8) == 8 && VR_DMT_WeaponsLeft(0, 5) == 5, sprintf("out of deathmatch the caps "
            "take nothing: %g of 8 gibs and %g of 5 weapons there", VR_DMT_GibsLeft(0, 8), VR_DMT_WeaponsLeft(0, 5)));
        vr_dml_test_off = FALSE;
        vr_dmt_wgone = vr_dml_weapons_gone;
        VR_DMT_To(42, 3);
        return;
    }
    if(vr_dmt_s == 42) // in deathmatch: the oldest gone
    {
        VR_DMLoose_OldestGib();
        float gibs = vr_dml_count;
        VR_Test_Check(VR_DMT_GibsLeft(0, 4) == 0 && VR_DMT_GibsLeft(6, 8) == 2 && gibs <= 4, sprintf("in deathmatch "
            "the 4 oldest gibs go, the 2 youngest stay, %g left of the cap 4 (the 6 old ones: %g left)", gibs,
            VR_DMT_GibsLeft(0, 6)));
        VR_DMLoose_OldestWeapon();
        float weapons = vr_dml_count;
        VR_Test_Check(VR_DMT_WeaponsLeft(3, 5) == 2 && weapons <= 3 && vr_dml_weapons_gone - vr_dmt_wgone >= 2,
            sprintf("... the weapons going soonest go first (%g gone), the 2 youngest stay, %g left of the cap 3",
            vr_dml_weapons_gone - vr_dmt_wgone, weapons));
        cvar_set("vr_dm_gibs_max", ftos(vr_dmt_gmax));
        cvar_set("vr_dm_weapons_max", ftos(vr_dmt_wmax));
        VR_DMT_End();
        return;
    }
    if(vr_dmt_s == 50) // test 8: only the first start counted, the human on it
    {
        vr_dmt_start = find(world, classname, "info_player_deathmatch");
        for(entity e = find(vr_dmt_start, classname, "info_player_deathmatch"); e;
            e = find(vr_dmt_start, classname, "info_player_deathmatch"))
        {
            e.classname = "info_player_deathmatch_vrtest";
        }
        setorigin(vr_dmt_human, vr_dmt_start.origin + '0 0 1');
        vr_dmt_human.velocity = '0 0 0';
        vr_dmt_human.health = max(vr_dmt_human.health, 999); // (the other bots' shots meanwhile; a telefrag's 50000 kills)
        vr_dmt_hp = vr_dmt_human.health;
        lastspawn = vr_dmt_start;
        entity os = self;
        self = vr_dmt_bot;
        PutClientInServer();
        self = os;
        vector at = vr_dmt_bot.origin - '0 0 1';
        float far = vlen(at - vr_dmt_start.origin);
        VR_Test_Check(vr_dm_beside && vr_dm_beside.classname == "vr_dm_beside" && vlen(at - vr_dm_beside_at) < 2 &&
            far >= 47 && far <= 145 && vlen(at - vr_dmt_human.origin) >= 64, sprintf("every start taken: the bot "
            "respawned beside it, %.0f units from it and %.0f from the human (at %s)", far, vlen(at -
            vr_dmt_human.origin), vtos(at)));
        VR_DMT_To(51, 36); // (0.5 s: the spawn's teledeath lasts 0.2 s)
        return;
    }
    if(vr_dmt_s == 51)
    {
        VR_Test_Check(vr_dmt_human.health > 0 && vr_dmt_human.deadflag == DEAD_NO && vr_dmt_bot.health > 0,
            sprintf("... nobody telefragged (the human's health %g, was %g; the bot's %g)", vr_dmt_human.health,
            vr_dmt_hp, vr_dmt_bot.health));
        for(entity e = find(world, classname, "info_player_deathmatch_vrtest"); e;
            e = find(world, classname, "info_player_deathmatch_vrtest"))
        {
            e.classname = "info_player_deathmatch";
        }
        vr_dmt_human.health = 100;
        VR_DMT_End();
    }
};

void() VR_DMRulesTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_dm_test);
    if(ask == 1) { VR_DMT_Run(ask); }
    if(ask == 2 || ask == 3) { VR_DMT_Parry(ask); }
    if(ask >= 4 && ask <= 6) { VR_DMT_Hands(ask); }
    if(ask == 7 || ask == 8) { VR_DMT_Loose(ask); }
};
#endif
