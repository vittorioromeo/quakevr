#ifdef QVR_TESTS
// vr_botgrab_test.qc -- the disarm grab at another's weapon where it is drawn (bots3; his notes 2026-11-21: "if I try to grab
// their weapon where it visually is, nothing happens"). Driven by mp_test.py botgrab on a deathmatch server with the human
// A, a remote player B and a bot: the harness reads where A's client draws the victim's weapon (vr_mp_avatars'
// mp_drawnwpn: its butt and muzzle) and asks the server for A's grab there.
// - 16: hold. The victim (vr_dm_test_who: 0 the first bot, 1 the second living human): a bot is held 64 units in front of the human,
//   facing him, a shotgun in its hand if it holds none, its own attacks off, unhurt; a player is given a shotgun if his hand
//   holds none (the harness keeps him still). `dmtest: hold: ... ready` once it has been held a second (the clients' copies
//   there), held on until vr_dm_test changes.
// - 17: the human's empty main hand closes at the point (vr_dm_test_x, _y, _z: world) and is pulled away hard: the victim's
//   weapon is taken (its hold, its weapon in his hand). 18: the same point, where nothing must be taken hold of.
// - 23: the hold of 16 with every holster of the human and the victim filled, the same gun in the same holster
//   (fbmisc1123, mp_test.py botgear: the copies' holstered guns against the human's own).
// Each grab ends with `dmtest: N passed, M failed` (the victim still held there until then).

float vr_bg_on;      // the running test's number
float vr_bg_n;       // frames it has run
float vr_bg_ready;   // 16: ready printed
float vr_bg_filled;  // 23: the holsters filled
entity vr_bg_victim; // the victim (a bot or a player)
float VR_BG_GIVEUP = 360; // frames the hold may wait for its victim

// The human (vr_dmt_human) and the victim (vr_bg_victim) found; FALSE: not (yet).
float() VR_BG_Find =
{
    vr_dmt_human = world;
    vr_bg_victim = world;
    float who = cvar_hget(cvarh_vr_dm_test_who);
    for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
    {
        if(p.ishuman && p.health > 0)
        {
            if(!vr_dmt_human) { vr_dmt_human = p; }
            else if(who == 1 && !vr_bg_victim) { vr_bg_victim = p; }
        }
        else if(who == 0 && (p.flags & FL_CLIENT) && p.health > 0 && !vr_bg_victim) { vr_bg_victim = p; }
    }
    return vr_dmt_human && vr_bg_victim;
};

// A bot victim held in front of the human, armed, still, its attacks off (each frame).
void() VR_BG_Hold =
{
    entity v = vr_bg_victim;
    if(vr_bg_n <= 1 && !VR_BT_Place(64, 8))
    {
        vr_bt_spot = vr_dmt_human.origin; // (no room: the test's grab still runs, at its drawn weapon wherever it is)
    }
    // (a player too: put there by the server each frame, facing the human; his weapon is the harness's, held by his grip)
    setorigin(v, vr_bt_spot);
    v.velocity = '0 0 0';
    v.v_angle = v.angles = vectoangles(vr_dmt_human.origin - vr_bt_spot);
    v.fixangle = TRUE;
    if(v.ishuman)
    {
        if(VRIsHandWeaponless(v, cVR_MainHand))
        {
            VR_DMT_ArmWith(v, WID_SHOTGUN); // (held by his grip; its model his client draws: W_SetCurrentAmmoFor)
        }
        return;
    }
    VR_DMT_Arm(v);
    v.attack_finished = time + 1;
    v.button0 = 0;
    v.takedamage = DAMAGE_NO;
};

// 17, 18: the human's main hand closes at the point, pulled away if it took hold.
void(float ask) VR_BG_Grab =
{
    entity him = vr_dmt_human, v = vr_bg_victim;
    vector at;
    at_x = cvar_hget(cvarh_vr_dm_test_x);
    at_y = cvar_hget(cvarh_vr_dm_test_y);
    at_z = cvar_hget(cvarh_vr_dm_test_z);
    VRGiveEntWeapon(him, cVR_MainHand, WID_FIST, 0, 0); // (his hand empty: what a grab takes goes there)
    him.vr_pdg_of = world;
    float gun = VRGetEntWeapon(v, cVR_MainHand);
    float pvp = cvar_hget(cvarh_vr_player_disarm_pvp), grab = cvar_hget(cvarh_vr_player_disarm_grab);
    cvar_set("vr_player_disarm_pvp", "1");
    cvar_set("vr_player_disarm_grab", "1");
    vector wasPos = him.handpos, wasVel = him.handvel, wasBody = him.velocity;
    float lc = lagcompbegin(him);
    float gap = VR_PDG_Gap(v, cVR_MainHand, at);
    if(lc)
    {
        lagcompend();
    }
    him.handpos = at;
    him.handvel = '0 0 0';
    him.velocity = '0 0 0';
    float takes = vr_pdg_takes;
    float held = VR_PDG_Try(him, cVR_MainHand) && him.vr_pdg_of == v;
    if(held)
    {
        him.vr_pdg_since = time - 0.1; // (held a moment)
        vector away = normalize(at - (v.origin + v.view_ofs * 0.6));
        him.handpos = at + away;
        him.handvel = away * (cvar_hget(cvarh_vr_player_disarm_pull) + 1); // (m/s: the hand's pull, his body still)
        VR_PDG_Hold(him);
    }
    him.vr_pdg_of = world;
    him.handpos = wasPos;
    him.handvel = wasVel;
    him.velocity = wasBody;
    cvar_set("vr_player_disarm_pvp", ftos(pvp));
    cvar_set("vr_player_disarm_grab", ftos(grab));
    string where = sprintf("%s's %s, the grab %.1f units from its weapon as the server has it (reach %.1f)", v.netname,
        WeaponIdToWeaponName(gun), gap, cvar_hget(cvarh_vr_player_disarm_reach) * 0.01 * VRMetersToUnits());
    if(ask == 17)
    {
        VR_Test_Check(held, sprintf("a grab at the drawn weapon takes hold: %s", where));
        VR_Test_Check(vr_pdg_takes == takes + 1 && VRGetEntWeapon(him, cVR_MainHand) == gun &&
            VRIsHandWeaponless(v, cVR_MainHand), sprintf("pulled away: the %s in the human's hand (%s)",
            WeaponIdToWeaponName(gun), WeaponIdToWeaponName(VRGetEntWeapon(him, cVR_MainHand))));
    }
    else
    {
        VR_Test_Check(!held && vr_pdg_takes == takes && !VRIsHandWeaponless(v, cVR_MainHand),
            sprintf("a grab off the drawn weapon takes nothing: %s", where));
    }
};

void() VR_BG_End =
{
    if(vr_bg_victim && !vr_bg_victim.ishuman)
    {
        vr_bg_victim.takedamage = DAMAGE_AIM;
    }
    cvar_set("vr_dm_test", "0");
    vr_bg_on = 0;
};

void(float ask) VR_BG_Run =
{
    if(vr_bg_on != ask)
    {
        vr_bg_on = ask;
        vr_bg_n = 0;
        vr_bg_ready = FALSE;
        vr_bg_filled = FALSE;
        VR_Test_BeginFlags("dmtest", VR_TEST_LOUD | VR_TEST_ECHO); // (16's hold line too: to every client)
    }
    vr_bg_n++;
    if(!VR_BG_Find())
    {
        if(vr_bg_n >= VR_BG_GIVEUP)
        {
            VR_Test_Print("dmtest: hold: FAIL no human and victim (timed out)\n");
            if(ask != 16)
            {
                VR_Test_Check(FALSE, "a human and the victim (timed out)");
                VR_Test_Verdict();
            }
            VR_BG_End();
        }
        return;
    }
    VR_BG_Hold();
    if(ask == 23 && !vr_bg_filled)
    {
        // (fbmisc1123) every holster of both filled, the same gun in the same holster: the copies' holstered guns are
        // compared with the human's own (vr_mp_gearcheck's vsown)
        vr_bg_filled = TRUE;
        VR_FNK_SetHolsters(vr_dmt_human, WID_NAILPISTOL, WID_REVOLVER, WID_SHOTGUN, WID_NAILGUN, WID_SUPER_SHOTGUN,
            WID_GRENADE_LAUNCHER);
        VR_FNK_SetHolsters(vr_bg_victim, WID_NAILPISTOL, WID_REVOLVER, WID_SHOTGUN, WID_NAILGUN, WID_SUPER_SHOTGUN,
            WID_GRENADE_LAUNCHER);
    }
    if(ask == 16 || ask == 23)
    {
        if(!vr_bg_ready && vr_bg_n >= VR_DMT_HOLD)
        {
            vr_bg_ready = TRUE;
            VR_Test_Print(sprintf("dmtest: hold: %s (human %g) held by %s, its %s: ready\n", vr_bg_victim.netname,
                vr_bg_victim.ishuman ? 1 : 0, vr_dmt_human.netname, WeaponIdToWeaponName(VRGetEntWeapon(vr_bg_victim,
                cVR_MainHand))));
        }
        return;
    }
    VR_BG_Grab(ask);
    VR_Test_Verdict();
    VR_BG_End();
};

void() VR_BotGrabTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_dm_test);
    if((ask >= 16 && ask <= 18) || ask == 23) { VR_BG_Run(ask); }
    else if(vr_bg_on) { VR_BG_End(); }
};
#endif
