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

void() VR_DMRulesTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_dm_test);
    if(ask == 1) { VR_DMT_Run(ask); }
};
#endif
