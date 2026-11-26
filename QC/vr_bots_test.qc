#ifdef QVR_TESTS
// vr_bots_test.qc -- FrikBot bots from his voice notes 2026-11-21 (vr_dm_test 13-15; a deathmatch with the human and two
// bots: run.sh with `maxplayers 4; deathmatch 1; map dm4; impulse 100; impulse 100`, or mp_test.py dm's server).
// - 13: toned down by skill (frikbot/bot_fight.qc bot_aim_point, bot_fire_ready): a bot held 350 units from the human
//   (or nearer where the map has no room), who strafes 250 units/s across its view (128 units each way, or less), its shotgun firing at him for 10 s, at each skill 0-3; then skills 0 and 1
//   with vr_bot_aim_error 0 and vr_bot_reaction 0 (dead on, at once: as before). A table line each (its first shot, its
//   shots, its hits); checked: at skill 0 its first shot no sooner than its reaction and fewer hits than dead on.
// - 14: its weapons (bot_pick_weapon): only a revolver of its own (his revolver loadout) and shells: it draws the
//   revolver and fires it at the human; no shells and an empty clip: its melee weapon; a nail pistol and nails: that.
// - 15: its swing telegraphed (bot_windup): the second bot swings its sword at the first, held 36 units apart; at skill 1
//   its blow lands vr_bot_swing_windup x 0.75 later than with no windup; with none (the old swing) in its strike; a skill
//   0 bot (parry chance 100, its reaction 0.3 s) parries it. vr_bot_aim_error 0 meanwhile (the blade's way each case).
// Ends with `dmtest: N passed, M failed`.

float vr_bt_on;     // the running test's number
float vr_bt_s;      // its state
float vr_bt_n;      // frames the state has run
float vr_bt_case;   // the case
float vr_bt_t0;     // the case's start
float vr_bt_af;     // the bot's attack_finished last frame
float vr_bt_shots;  // its shots in the case
float vr_bt_hits;   // ... that hit the human
float vr_bt_first;  // its first shot's time after the case's start (-1: none yet)
float vr_bt_aim;    // vr_bot_aim_error before
float vr_bt_react;  // vr_bot_reaction before
float vr_bt_wind;   // vr_bot_swing_windup before
float vr_bt_pch;    // vr_bot_parry_chance before
float vr_bt_skill;  // the bot's skill before
float vr_bt_rate[6]; // test 13's hit rates by case
float vr_bt_fs[6];   // ... first shots
float vr_bt_land[2]; // test 15's blows' delays (default windup, none)
vector vr_bt_spot;  // where the bot is held
vector vr_bt_base;  // the human's middle place (13: he strafes about it)
vector vr_bt_right; // ... the way he strafes
float vr_bt_side;   // ... this far each way
float VR_BT_FRAMES = 720; // test 13: frames a case runs (10 s at 72 Hz)
float VR_BT_GIVEUP = 360; // frames a waiting state may last

void() VR_BT_End =
{
    cvar_set("vr_bot_aim_error", ftos(vr_bt_aim));
    cvar_set("vr_bot_reaction", ftos(vr_bt_react));
    cvar_set("vr_bot_swing_windup", ftos(vr_bt_wind));
    cvar_set("vr_bot_parry_chance", ftos(vr_bt_pch));
    if(vr_dmt_bot)
    {
        vr_dmt_bot.b_skill = vr_bt_skill;
        vr_dmt_bot.takedamage = DAMAGE_AIM;
    }
    if(vr_dmt_human)
    {
        vr_dmt_human.health = 100;
    }
    VR_Test_Verdict();
    cvar_set("vr_dm_test", "0");
    vr_bt_on = 0;
    vr_bt_s = -1;
};

// A way from the human with `d` units clear to a bot's place, and the human's strafe across it (`side` each way) seen
// from there -> vr_bt_spot, vr_bt_base, vr_bt_right; FALSE: none of 8.
float(float d, float side) VR_BT_Place =
{
    entity h = vr_dmt_human;
    for(float k = 0; k < 8; k++)
    {
        makevectors((h.angles_y + k * 45) * '0 1 0');
        vector p = h.origin + v_forward * d;
        tracebox(h.origin, VEC_HULL_MIN, VEC_HULL_MAX, p, TRUE, h);
        float ok = ((trace_fraction == 1) && !trace_startsolid);
        for(float s = -1; (s <= 1) && ok; s = s + 2)
        {
            tracebox(h.origin, VEC_HULL_MIN, VEC_HULL_MAX, h.origin + v_right * (side * s), TRUE, h);
            ok = ((trace_fraction == 1) && !trace_startsolid);
            traceline(p + '0 0 22', h.origin + v_right * (side * s), TRUE, h);
            ok = (ok && (trace_fraction == 1));
        }
        if(ok)
        {
            vr_bt_spot = p;
            vr_bt_base = h.origin;
            vr_bt_right = v_right;
            vr_bt_side = side;
            return TRUE;
        }
    }
    return FALSE;
};

// The bot and the human found (vr_dmrules_test.qc's globals: vr_dmt_bot, vr_dmt_bot2, vr_dmt_human), `two` bots.
float(float two) VR_BT_Find =
{
    vr_dmt_bot = vr_dmt_bot2 = vr_dmt_human = world;
    for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
    {
        if(p.ishuman && p.health > 0 && !vr_dmt_human) { vr_dmt_human = p; }
        if(!p.ishuman && (p.flags & FL_CLIENT) && p.health > 0)
        {
            if(!vr_dmt_bot) { vr_dmt_bot = p; }
            else if(!vr_dmt_bot2) { vr_dmt_bot2 = p; }
        }
    }
    if(two && vr_dmt_human && vr_dmt_bot && !vr_dmt_bot2 && vr_bt_n == 36)
    {
        // (a second bot added as the human's impulse 100 adds one: a run's console sends one impulse 100 only)
        entity os = self;
        self = vr_dmt_human;
        BotConnect(0, 0, cvar_hget(cvarh_skill));
        self = os;
    }
    return vr_dmt_bot && vr_dmt_human && (!two || vr_dmt_bot2);
};

// The bot held at vr_bt_spot (its AI turns and fires as it will), the human kept alive at `at`, moving `vel`, its enemy.
void(vector at, vector vel) VR_BT_Hold =
{
    entity b = vr_dmt_bot, h = vr_dmt_human;
    setorigin(b, vr_bt_spot);
    b.velocity = '0 0 0';
    b.health = max(b.health, 100);
    setorigin(h, at);
    h.velocity = vel;
    if(!b.enemy)
    {
        b.enemy = h; // (seen: its AI keeps him or loses him; held here, FrikBot's own look for players never ran)
    }
};

// A new shot by the bot since the last frame (its attack_finished moved on).
float() VR_BT_Shot =
{
    float shot = (vr_dmt_bot.attack_finished != vr_bt_af && vr_dmt_bot.attack_finished > time);
    vr_bt_af = vr_dmt_bot.attack_finished;
    return shot;
};

void(entity b, float wid, float aid, float n) VR_BT_Arm =
{
    VR_WeaponUtil_EntAddWeapon(b, wid);
    VR_AmmoUtil_SetAmmoCountByAmmoId(b, aid, n);
    VRGiveEntWeapon(b, cVR_OffHand, WID_FIST, 0, 0);
    VRGiveEntWeapon(b, cVR_MainHand, wid, 0, WeaponIdToBaseClipSize(wid));
};

void() VR_BT_Skill =
{
    entity b = vr_dmt_bot, h = vr_dmt_human;
    if(vr_bt_s == 1) // a case's start
    {
        float tuned = vr_bt_case < 4;
        b.b_skill = tuned ? vr_bt_case : vr_bt_case - 4;
        cvar_set("vr_bot_aim_error", tuned ? "1" : "0");
        cvar_set("vr_bot_reaction", tuned ? "1" : "0");
        VR_BT_Arm(b, WID_SHOTGUN, AID_SHELLS, 100);
        b.enemy = b.vr_botreact_of = world;
        b.v_angle = b.angles = vectoangles(vr_bt_base - vr_bt_spot);
        b.fixangle = TRUE;
        VR_BT_Hold(vr_bt_base, '0 0 0');
        vr_bt_t0 = time;
        vr_bt_af = b.attack_finished;
        vr_bt_shots = vr_bt_hits = 0;
        vr_bt_first = -1;
        h.health = 999;
        vr_bt_s = 2;
        vr_bt_n = 0;
        return;
    }
    // (2) he strafes 250 units/s, 128 each way; its shots and their hits counted
    float shot = VR_BT_Shot();
    if(shot)
    {
        vr_bt_shots++;
        vr_bt_first = vr_bt_first < 0 ? time - vr_bt_t0 : vr_bt_first;
        vr_bt_hits = vr_bt_hits + (h.health < 999);
    }
    h.health = 999;
    float tt = (time - vr_bt_t0) * 250;
    float per = 4 * vr_bt_side;
    float ph = tt - floor(tt / per) * per; // (a triangle wave: vr_bt_side each way)
    float off = ph < per / 2 ? ph - vr_bt_side : 3 * vr_bt_side - ph;
    VR_BT_Hold(vr_bt_base + vr_bt_right * off, vr_bt_right * (ph < per / 2 ? 250 : -250));
    if(vr_bt_n < VR_BT_FRAMES)
    {
        return;
    }
    float rate = vr_bt_shots > 0 ? vr_bt_hits / vr_bt_shots : 0;
    vr_bt_rate[vr_bt_case] = rate;
    vr_bt_fs[vr_bt_case] = vr_bt_first;
    VR_Test_Print(sprintf("dmtest: table: skill %g, aim error %s, reaction %s: first shot %.2f s, %g shots, %g hits (%.0f%%)\n",
        b.b_skill, ftos(cvar_hget(cvarh_vr_bot_aim_error)), ftos(cvar_hget(cvarh_vr_bot_reaction)), vr_bt_first, vr_bt_shots, vr_bt_hits,
        rate * 100));
    vr_bt_case++;
    if(vr_bt_case < 6)
    {
        vr_bt_s = 1;
        return;
    }
    VR_Test_Check(vr_bt_fs[0] >= 0.8 - 0.05 && vr_bt_fs[0] > vr_bt_fs[4], sprintf("skill 0's first shot after its reaction "
        "(%.2f s; dead on and at once: %.2f s)", vr_bt_fs[0], vr_bt_fs[4]));
    VR_Test_Check(vr_bt_rate[0] < vr_bt_rate[4], sprintf("skill 0 hits the strafing human less than dead on (%.0f%%, %.0f%%)",
        vr_bt_rate[0] * 100, vr_bt_rate[4] * 100));
    VR_Test_Check(vr_bt_rate[1] < vr_bt_rate[5], sprintf("skill 1 hits him less than dead on (%.0f%%, %.0f%%)",
        vr_bt_rate[1] * 100, vr_bt_rate[5] * 100));
    VR_Test_Check(vr_bt_rate[0] <= vr_bt_rate[3] && vr_bt_fs[3] < vr_bt_fs[0], sprintf("skill 3 sharper than skill 0 "
        "(hits %.0f%% and %.0f%%, first shots %.2f s and %.2f s)", vr_bt_rate[3] * 100, vr_bt_rate[0] * 100, vr_bt_fs[3],
        vr_bt_fs[0]));
    VR_BT_End();
};

void() VR_BT_Weapons =
{
    entity b = vr_dmt_bot, h = vr_dmt_human;
    entity os = self;
    if(vr_bt_s == 1) // only a revolver and shells: drawn
    {
        for(float w = WID_AXE; w <= WID_LAST; w++)
        {
            if(w != WID_SWORD && w != WID_AXE) { VR_WeaponUtil_EntDelWeapon(b, w); }
        }
        VR_WeaponUtil_EntAddWeapon(b, WID_REVOLVER);
        VR_AmmoUtil_SetAmmoCountByAmmoId(b, AID_SHELLS, 25);
        VR_AmmoUtil_SetAmmoCountByAmmoId(b, AID_NAILS, 0);
        VRGiveEntWeapon(b, cVR_OffHand, WID_FIST, 0, 0);
        VRGiveEntWeapon(b, cVR_MainHand, WID_AXE, 0, 0);
        self = b;
        float pick = bot_pick_weapon(-1);
        bot_weapon_switch(-1);
        self = os;
        VR_Test_Check(pick == WID_REVOLVER && b.weapon == WID_REVOLVER, sprintf("only a revolver and shells of its own: it "
            "draws the revolver (%s, in hand %s)", WeaponIdToWeaponName(pick), WeaponIdToWeaponName(b.weapon)));
        b.enemy = b.vr_botreact_of = world;
        b.v_angle = b.angles = vectoangles(vr_bt_base - vr_bt_spot);
        b.fixangle = TRUE;
        vr_bt_af = b.attack_finished;
        vr_bt_t0 = time;
        vr_bt_first = -1;
        vr_bt_s = 2;
        vr_bt_n = 0;
        return;
    }
    if(vr_bt_s == 2) // it fires the revolver at him
    {
        h.health = 999;
        VR_BT_Hold(vr_bt_base, '0 0 0');
        // (Slugs, not trigger pulls: its shells spent, each a hammer's fall on a live chamber. His voice note
        // 2026-11-22: a revolver bot aimed and never shot. Three slugs: the trigger let go between them.)
        float slugs = 25 - VR_AmmoUtil_GetAmmoCountByAmmoId(b, AID_SHELLS);
        if(slugs >= 1 && vr_bt_first < 0)
        {
            vr_bt_first = time - vr_bt_t0;
        }
        float fired = (slugs >= 3 && b.weapon == WID_REVOLVER);
        if(!fired && vr_bt_n < VR_BT_GIVEUP * 2)
        {
            return;
        }
        VR_Test_Check(fired, sprintf("its AI fires the revolver at the human: %g slugs in %.2f s (the first at %.2f s; "
            "in hand %s)", slugs, time - vr_bt_t0, vr_bt_first, WeaponIdToWeaponName(b.weapon)));
        // no shells and its clip empty: its melee weapon; a nail pistol and nails: that
        VR_AmmoUtil_SetAmmoCountByAmmoId(b, AID_SHELLS, 0);
        VRSetEntWeaponClip(b, cVR_MainHand, 0);
        self = b;
        float melee = bot_pick_weapon(-1);
        VR_WeaponUtil_EntAddWeapon(b, WID_NAILPISTOL);
        VR_AmmoUtil_SetAmmoCountByAmmoId(b, AID_NAILS, 50);
        float np = bot_pick_weapon(-1);
        self = os;
        VR_Test_Check(VR_AG_Class(melee) == VR_AG_CLASS_MELEE, sprintf("no shells, its revolver empty: its melee weapon (%s)",
            WeaponIdToWeaponName(melee)));
        VR_Test_Check(np == WID_NAILPISTOL, sprintf("a nail pistol and nails: the nail pistol (%s)", WeaponIdToWeaponName(np)));
        VR_WeaponUtil_EntDelWeapon(b, WID_NAILPISTOL);
        VR_AmmoUtil_SetAmmoCountByAmmoId(b, AID_NAILS, 0);
        vr_bt_case = 0;
        vr_bt_s = 3;
        vr_bt_n = 0;
        return;
    }
    if(vr_bt_s == 3) // a revolver taken (his, by a disarm: its clip loaded): case 0 in its main hand, no shells of its
    {                // own; case 1 in its off hand beside its shotgun, shells of its own
        VR_WeaponUtil_EntAddWeapon(b, WID_REVOLVER);
        if(vr_bt_case == 0)
        {
            VR_AmmoUtil_SetAmmoCountByAmmoId(b, AID_SHELLS, 0);
            VRGiveEntWeapon(b, cVR_OffHand, WID_FIST, 0, 0);
            VRGiveEntWeapon(b, cVR_MainHand, WID_REVOLVER, 0, WeaponIdToBaseClipSize(WID_REVOLVER));
        }
        else
        {
            VR_WeaponUtil_EntAddWeapon(b, WID_SHOTGUN);
            VR_AmmoUtil_SetAmmoCountByAmmoId(b, AID_SHELLS, 25);
            VRGiveEntWeapon(b, cVR_MainHand, WID_SHOTGUN, 0, 0);
            VRGiveEntWeapon(b, cVR_OffHand, WID_REVOLVER, 0, WeaponIdToBaseClipSize(WID_REVOLVER));
        }
        b.weapon = VRGetEntWeapon(b, cVR_MainHand);
        self = b;
        W_SetCurrentAmmo();
        self = os;
        b.enemy = b.vr_botreact_of = world;
        b.v_angle = b.angles = vectoangles(vr_bt_base - vr_bt_spot);
        b.fixangle = TRUE;
        vr_rv_bot_slugs = 0;
        vr_bt_t0 = time;
        vr_bt_first = -1;
        vr_bt_s = 4;
        vr_bt_n = 0;
        return;
    }
    if(vr_bt_s == 4) // it fires the taken revolver at him: three slugs
    {
        h.health = 999;
        VR_BT_Hold(vr_bt_base, '0 0 0');
        float rh = vr_bt_case == 0 ? cVR_MainHand : cVR_OffHand;
        if(vr_rv_bot_slugs >= 1 && vr_bt_first < 0)
        {
            vr_bt_first = time - vr_bt_t0;
        }
        float ok = (vr_rv_bot_slugs >= 3 && VRGetEntWeapon(b, rh) == WID_REVOLVER);
        if(!ok && vr_bt_n < VR_BT_GIVEUP * 2)
        {
            return;
        }
        VR_Test_Check(ok, sprintf("a revolver it took (its clip loaded), %s: %g slugs in %.2f s (the first at %.2f s; "
            "in that hand %s, shells %g)", vr_bt_case == 0 ? "in its main hand, no shells of its own" :
            "in its off hand beside its shotgun", vr_rv_bot_slugs, time - vr_bt_t0, vr_bt_first,
            WeaponIdToWeaponName(VRGetEntWeapon(b, rh)), VR_AmmoUtil_GetAmmoCountByAmmoId(b, AID_SHELLS)));
        VRGiveEntWeapon(b, cVR_OffHand, WID_FIST, 0, 0);
        vr_bt_case++;
        vr_bt_s = vr_bt_case < 2 ? 3 : 5;
        vr_bt_n = 0;
        if(vr_bt_s == 5)
        {
            VR_BT_End();
        }
    }
};

void() VR_BT_Windup =
{
    entity d = vr_dmt_bot, a = vr_dmt_bot2;
    if(vr_bt_s == 1) // held apart, then the swing (case 0, 1: the blow lands, windup default / none; 2: parried)
    {
        cvar_set("vr_bot_swing_windup", vr_bt_case == 1 ? "0" : ftos(vr_bt_wind));
        cvar_set("vr_bot_parry_chance", vr_bt_case == 2 ? "100" : "0");
        cvar_set("vr_bot_aim_error", "0"); // (its blade's way the same each case: test 13 measures the aim's error)
        a.b_skill = 1;
        d.b_skill = 0;
        if(vr_bt_n == 1 && vr_bt_case == 0 && !VR_DMT_PairFree())
        {
            VR_Test_Check(FALSE, "a free place for the swinger 36 units from the other bot (8 ways tried)");
        }
        VR_DMT_HoldPair();
        if(vr_bt_n < VR_DMT_HOLD)
        {
            return;
        }
        vr_dmt_blows0 = vr_melee_blows;
        vr_dmt_parry0 = vr_parry_count;
        a.attack_finished = 0;
        a.vr_botswing = 0;
        entity os = self;
        self = a;
        W_Fire_Melee(cVR_MainHand, &a.attack_finished);
        self = os;
        vr_bt_t0 = time;
        vr_bt_first = -1;
        vr_bt_s = 2;
        vr_bt_n = 0;
        return;
    }
    VR_DMT_HoldPair();
    float landed = (vr_melee_blows > vr_dmt_blows0 && vr_melee_blow_by == a && vr_melee_blow_on == d);
    if(landed && vr_bt_first < 0)
    {
        vr_bt_first = time - vr_bt_t0;
    }
    if(time - vr_bt_t0 < BOT_SWING_LEN + a.vr_botwindup + 0.1)
    {
        return;
    }
    float parried = (vr_parry_count > vr_dmt_parry0 && vr_parry_count_by == a);
    if(vr_bt_case < 2)
    {
        vr_bt_land[vr_bt_case] = vr_bt_first;
        VR_Test_Print(sprintf("dmtest: skill 1's blow, windup %s: lands %.3f s after its start (held raised %.3f s more)\n",
            ftos(cvar_hget(cvarh_vr_bot_swing_windup)), vr_bt_first, a.vr_botwindup));
        vr_bt_case++;
        vr_bt_s = 1;
        vr_bt_n = 0;
        if(vr_bt_case == 2)
        {
            // (a player's swing strikes from 0.32 s: vr_flat_swing_wind + _top's defaults. With no windup the old swing:
            // its blow lost while the bot's hands were posed from its drawn head, 6 units higher, bots2)
            float want = vr_bt_wind * 0.75;
            VR_Test_Check(vr_bt_land[0] >= 0.32 + want, sprintf("skill 1's blow lands %.3f s after its swing's start, held "
                "raised %.3f s past a player's strike at 0.32 s", vr_bt_land[0], want));
            VR_Test_Check(vr_bt_land[1] >= 0.32 && vr_bt_land[1] < 0.55, sprintf("vr_bot_swing_windup 0: the old swing, "
                "its blow lands %.3f s after its start (from 0.32 s, in its strike)", vr_bt_land[1]));
        }
        return;
    }
    VR_Test_Check(parried && vr_bt_first < 0, sprintf("a skill 0 bot (its guard up 0.3 s after it sees the blow) parries "
        "skill 1's telegraphed swing (parried %g, landed %g)", parried, vr_bt_first >= 0));
    VR_BT_End();
};

void(float ask) VR_BT_Run =
{
    if(vr_bt_on != ask)
    {
        VR_Test_BeginFlags("dmtest", VR_TEST_LOUD | VR_TEST_ECHO);
        vr_bt_on = ask;
        vr_bt_aim = cvar_hget(cvarh_vr_bot_aim_error);
        vr_bt_react = cvar_hget(cvarh_vr_bot_reaction);
        vr_bt_wind = cvar_hget(cvarh_vr_bot_swing_windup);
        vr_bt_pch = cvar_hget(cvarh_vr_bot_parry_chance);
        vr_bt_case = 0;
        vr_bt_s = 0;
        vr_bt_n = 0;
    }
    if(vr_bt_s < 0)
    {
        return;
    }
    vr_bt_n++;
    if(vr_bt_s == 0) // the human and living bots (two for 15), a place for the bot (13, 14)
    {
        float ok = VR_BT_Find((ask == 15));
        if(!ok && (vr_bt_n < VR_BT_GIVEUP))
        {
            return;
        }
        vr_bt_skill = vr_dmt_bot.b_skill;
        ok = (ok && ((ask == 15) || VR_BT_Place(350, 128) || VR_BT_Place(300, 96) || VR_BT_Place(250, 64) ||
                       VR_BT_Place(200, 48)));
        VR_Test_Check(ok, sprintf("a human (%s), living bots (%s, %s) and room for them (%.0f units, strafing %.0f)",
            vr_dmt_human.netname, vr_dmt_bot.netname, vr_dmt_bot2.netname, vlen(vr_bt_spot - vr_bt_base), vr_bt_side));
        if(!ok)
        {
            VR_BT_End();
            return;
        }
        if(ask != 15)
        {
            vr_dmt_bot.takedamage = DAMAGE_NO; // (15: its blow lands on it)
        }
        vr_bt_s = 1;
        vr_bt_n = 0;
    }
    if(ask == 13) { VR_BT_Skill(); }
    if(ask == 14) { VR_BT_Weapons(); }
    if(ask == 15) { VR_BT_Windup(); }
};

void() VR_BotsTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_dm_test);
    if(ask >= 13 && ask <= 15) { VR_BT_Run(ask); }
};
// vr_stealth_test 134 (vr_stealth_test7.qc): the bot's goal. what 0: none (its targets and ai flags cleared); 1: a temp
// waypoint at `at`; 2: that, kept on (FrikBot gives a goal up after 5 s, or when it stands still a while).
void(entity b, float what, vector at) VR_STT7_BotGoal =
{
    if(what == 0)
    {
        b.target1 = b.target2 = b.target3 = b.target4 = b.enemy = world;
        b.b_aiflags = 0;
        return;
    }
    entity os = self;
    self = b;
    SpawnTempWaypoint(at);
    self = os;
    if(what == 2)
    {
        b.search_time = time + 5;
    }
};
#endif
