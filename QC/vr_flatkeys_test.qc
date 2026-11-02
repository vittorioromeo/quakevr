// vr_flatkeys_test [1|2]: the flatscreen keys (Left Ctrl, F, G) as the game code sees them. Only with -flatscreenmode: a
// game without the flag says so, because the keys are the flatscreen's and a headset player has his own ways.
//   1 (the console, or Debug > Tests > Flat Keys): two lines about where the crouch put him (vr_flatplay.cpp
//       puts his head down vr_flat_crouch_drop; the engine reports how upright that left him as .vr_stand), what each hand
//       holds, and what the flashlight's switch has kept (the packed state the engine saves each frame, bit 0 lit).
//   2: G, sent the way the engine sends it (impulse 47, weapons.qc): what the main hand held, and a few frames on, whether
//       it is gone (the drop itself prints its own flat drop: line in vr_wpnforcegrab.qc).
// A crouch cannot be pressed from the game code: for 1, hold Left Ctrl (or +flatcrouch) before asking for it.

#ifdef QVR_TESTS

float flatkeysTestStep; // 0: nothing armed; the frames left until 2 reports (the impulse needs a player frame to run)
float flatkeysTestWas;  // what the main hand held when G was sent

void() VR_FlatKeysTest_Frame =
{
    float mode = cvar_hget(cvarh_vr_flatkeys_test);
    if(!mode)
    {
        return;
    }
    entity player = VR_Player();
    if(player == world)
    {
        cvar_set("vr_flatkeys_test", "0");
        return;
    }
    if(!VR_FlatPlay(player))
    {
        bprint("flatkeys: flat play off: run with -flatscreenmode (Left Ctrl, F and G belong to the flatscreen)\n");
        cvar_set("vr_flatkeys_test", "0");
        return;
    }

    if(mode == 2)
    {
        if(flatkeysTestStep <= 0)
        {
            flatkeysTestWas = VRIsHandWeaponless(player, cVR_MainHand) ? 0 : VRGetEntWeapon(player, cVR_MainHand);
            player.impulse = 47; // G, through the game's own impulse path
            flatkeysTestStep = 3;
            bprint(sprintf("flatkeys: G sent (impulse 47), main hand held weapon %g\n", flatkeysTestWas));
            return;
        }
        if(--flatkeysTestStep > 0)
        {
            return;
        }
        entity left = VR_Carry_Held(player, cVR_MainHand);
        bprint(sprintf("flatkeys: dropped %g: main hand weaponless %g, hand now %s\n", flatkeysTestWas,
            VRIsHandWeaponless(player, cVR_MainHand) ? 1 : 0, left == world ? "empty" : left.classname));
        cvar_set("vr_flatkeys_test", "0");
        return;
    }

    float eye = player.headpos == '0 0 0' ? 0 : player.headpos_z - player.absmin_z;
    float state = player.vr_flashlight_state;
    float lit = state - floor(state / 2) * 2;             // bit 0: the lamp lit
    float worn = floor(state / 2) - floor(state / 16) * 8; // bits 1..3: mounted, held, returning, on the gun, on the head
    entity cMain = VR_Carry_Held(player, cVR_MainHand);
    entity cOff = VR_Carry_Held(player, cVR_OffHand);
    bprint(sprintf("flatkeys: crouched %g stand %g eye %.1f origin %.1f hands %g %g\n",
        VRHasBit(player.vrbits0, QVR_VRBITS0_CROUCHED) ? 1 : 0, player.vr_stand, eye, player.origin_z,
        VRGetEntWeapon(player, cVR_MainHand), VRGetEntWeapon(player, cVR_OffHand)));
    bprint(sprintf("flatkeys: carry %s %s torch lit %g worn %g\n", cMain == world ? "-" : cMain.classname,
        cOff == world ? "-" : cOff.classname, lit, worn));
    cvar_set("vr_flatkeys_test", "0");
};

#endif
