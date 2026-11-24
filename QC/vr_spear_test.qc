#ifdef QVR_TESTS
// vr_spear_test.qc -- the spear (VR_SWORDKIND_SPEAR: the knight's sword of its kind held as a pole weapon on the Crystal
// Lance's systems; ROUND25.md "spear"), by the embedded blades' test machinery (vr_embed_test.qc: VR_EH_Blow, the mock
// hand's real blows; VR_ET_ThrowSword). `vr_embed_test 53` (map vrtesthall, god, notarget, on your feet in the open), the
// spear in the main hand:
//   0 a thrust (a stab) at a grunt: the pole's blow (VR_PoleHeld), it hurts it, stuck by its point, no deeper than its
//     head (its blade: the head alone, bl_from); then the hand drawn back: it comes out, back in the hand;
//   1 a swing across a grunt: it hurts it;
//   2 thrown point first at the wall: it sticks by its point;
//   3 thrown at a live grunt's chest: it hurts it (whether it stays in is printed: a note);
//   4 the flip grip (VR_RevGrip_Flip: reversed, then upright again) and no ammo (no other ammo, none to show).
// With -flatscreenmode (flat-screen play) case 5 alone: the spear held, the mouse button's punch is the spear's thrust
// (vr_flat_punch main) at a grunt ahead: it hurts it; stuck, fire presses pull it out (VR_Embed_FlatPull) back into the
// hand; B (impulse 43: no other ammo) flips its grip and back. Ends `embedspear: N passed, M failed`.

float(float xHand) VR_PoleHeld; // (vr_poleweapon.qc)
float vr_sp_hp;    // the grunt's health before the blow
float vr_sp_n;     // presses so far (the flat pull)

float() VR_SP_Flags = { return VR_ADMelee_Flags(VR_SWORDKIND_SPEAR); };

float(entity p, float h) VR_SP_InHand =
{
    return VRGetEntWeapon(p, h) == WID_SWORD && VR_ADMelee_Kind(VRGetEntWeaponFlags(p, h)) == VR_SWORDKIND_SPEAR;
};

void() VR_SP_Begin =
{
    VR_Test_BeginFlags("embedspear", VR_TEST_LOUD);
    cvar_set("vr_knockdown", "0"); // (as test 40: a blow's chance of one takes the body from the hand)
    cvar_set("vr_hit_stagger", "0"); // (a blow's own chance of a stagger moves the body off the stab)
    vr_eh_home = VR_Player().origin;
    localcmd("vr_mock_hand_to_absolute 1\n");
    vr_et_on = 53;
    vr_et_case = VR_FlatPlay(VR_Player()) ? 5 : 0; // (flat-screen play: its own case alone)
    VR_ET_To(0);
};

void() VR_SP_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    float h = cVR_MainHand;
    entity w = VR_EH_Blade();
    if(t > VR_ET_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %g state %g", vr_et_case, vr_et_s));
        VR_EH_NextCase();
        return;
    }
    if(vr_et_case != 2 && vr_et_case != 4 && vr_et_m != world && vr_et_m.health > 0)
    {
        vr_et_m.nextthink = time + 0.5; // (the body stands where it is, its blow's anger put off: as test 40's)
        vr_et_m.velocity = '0 0 0';
    }
    vr_eh_flags = VR_SP_Flags(); // (VR_EH_Blow's give; VR_EH_NextCase clears it)

    // ---- 0: a thrust at a grunt, then drawn back out
    if(vr_et_case == 0)
    {
        if(vr_et_s <= 4)
        {
            if(vr_et_s == 1 && t <= 0.1) { vr_sp_hp = 0; }
            if(vr_et_s == 2 && vr_sp_hp == 0)
            {
                vr_sp_hp = vr_et_m.health;
                VR_Test_Check(VR_PoleHeld(h), "the spear in the hand is a pole weapon (VR_PoleHeld: the Lance's blow, VR_PoleMelee)");
            }
            if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
            {
                float stuck = w != world && VR_AxeStick_InMonster(w) && w.vr_stick_embed == AXESTICK_POINT;
                VR_Test_Check(stuck, sprintf("a spear's thrust at 6 m/s is stuck by its point (depth %.1f units)", w != world ? w.vr_stick_depth : 0));
                VR_Test_Check(vr_et_m.health < vr_sp_hp, sprintf("its thrust dealt %g damage", vr_sp_hp - vr_et_m.health));
                VR_Test_Check(w == world || w.model == "progs/v_spear.mdl", sprintf("the stuck blade is the spear (%s)", w.model));
                if(stuck)
                {
                    VR_Test_Check(w.vr_stick_bladelen > 4 && w.vr_stick_bladelen < 10 && w.vr_stick_depth <= w.vr_stick_bladelen,
                        sprintf("its blade is its head alone: %.1f units (4 .. 10), %.1f in", w.vr_stick_bladelen, w.vr_stick_depth));
                    VR_EH_HandTo(vr_eh_start - vr_eh_fw * 12, 0.5); // (drawn straight back along the thrust)
                    VR_ET_To(5);
                }
                else
                {
                    VR_EH_NextCase();
                }
            }
            return;
        }
        if(vr_et_s == 5 && (w == world || t > 3))
        {
            VR_Test_Check(w == world && VR_SP_InHand(p, h), sprintf("drawn back, it comes out into the hand (stuck: %g; holds %g, kind %g)",
                w != world, VRGetEntWeapon(p, h), VR_ADMelee_Kind(VRGetEntWeaponFlags(p, h))));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 1: a swing across a grunt
    if(vr_et_case == 1)
    {
        if(vr_et_s == 2 && vr_sp_hp == 0) { vr_sp_hp = vr_et_m.health; }
        if(vr_et_s == 0) { vr_sp_hp = 0; }
        if(VR_EH_Blow(WID_SWORD, 6, 0, TRUE, t))
        {
            VR_Test_Check(vr_et_m.health < vr_sp_hp, sprintf("a spear's swing at 6 m/s hurts a grunt (%g damage)", vr_sp_hp - vr_et_m.health));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 2: thrown at the wall
    if(vr_et_case == 2)
    {
        if(vr_et_s == 0)
        {
            setorigin(p, vr_eh_home);
            vr_et_at = VR_ET_FaceWall(220);
            makevectors('0 1 0' * p.v_viewangle_y);
            vr_et_flags = VR_SP_Flags();
            vr_et_w = VR_ET_ThrowSword(WID_SWORD, vr_et_at - v_forward * 90, v_forward, v_forward, FALSE);
            vr_et_flags = 0;
            VR_ET_To(1);
        }
        else if(VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(vr_et_w.vr_stick_kind == AXESTICK_LEVEL && vr_et_w.vr_stick_embed == AXESTICK_POINT &&
                vr_et_w.model == "progs/v_spear.mdl", sprintf("a thrown spear sticks in the wall by its point (kind %g, how %g, depth %.1f, %s)",
                vr_et_w.vr_stick_kind, vr_et_w.vr_stick_embed, vr_et_w.vr_stick_depth, vr_et_w.model));
            VR_ET_NextCase();
        }
        return;
    }
    // ---- 3: thrown at a live grunt
    if(vr_et_case == 3)
    {
        if(vr_et_s == 0)
        {
            vr_et_m = VR_ET_Grunt(110);
            vr_et_m.health = vr_et_m.max_health = 400; // (a thrown spear's blow, a sword's, kills a grunt outright: gibbed, no body to stay in)
            vr_sp_hp = vr_et_m.health;
            vr_et_flags = VR_SP_Flags();
            vr_et_w = VR_ET_SwordAt(VR_ET_Chest(vr_et_m), 80, TRUE);
            vr_et_flags = 0;
            VR_ET_To(1);
        }
        else if(VR_ET_Resolved(vr_et_w))
        {
            // (Whether it stays in depends on how square the point meets the grunt's moving model: 1 run in 3 here, a
            // glancing 44 degrees off straight in skids. Reported, not checked: ROUND25.md "spear", open.)
            dprint(sprintf("embedspear: note: a spear thrown at a grunt %s (kind %g, how %g, depth %.1f)\n",
                VR_AxeStick_InMonster(vr_et_w) ? "sticks in it" : "did not stay in it", vr_et_w.vr_stick_kind,
                vr_et_w.vr_stick_embed, vr_et_w.vr_stick_depth));
            VR_Test_Check(vr_et_m.health < vr_sp_hp, sprintf("the throw hurt it (%g damage)", vr_sp_hp - vr_et_m.health));
            VR_ET_NextCase();
        }
        return;
    }
    // ---- 4: the flip grip; no ammo
    if(vr_et_case == 4)
    {
        if(vr_et_s == 0)
        {
            setorigin(p, vr_eh_home);
            localcmd("+grabmain\n"); // (the mock hand's grip closed, as VR_EH_Blow's: else the hand lets it fall)
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.1)
        {
            VR_EH_Give(WID_SWORD, VR_SP_Flags());
            VR_ET_To(6);
        }
        else if(vr_et_s == 6 && t > 0.2)
        {
            VR_Test_Check(!VR_HasOtherAmmo(h) && WeaponIdToAmmoId(WID_SWORD, VR_SP_Flags()) == AID_NONE && VR_SP_InHand(p, h),
                sprintf("no ammo: no other ammo, none to show (other ammo %g, ammo id %g)", VR_HasOtherAmmo(h), WeaponIdToAmmoId(WID_SWORD, VR_SP_Flags())));
            VR_RevGrip_Flip(h);
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && t > 0.2)
        {
            VR_Test_Check((VRGetEntWeaponFlags(p, h) & QVR_WPNFLAG_REVGRIP) && VR_SP_InHand(p, h), "the flip grip: reversed, still the spear");
            VR_RevGrip_Flip(h);
            VR_ET_To(3);
        }
        else if(vr_et_s == 3 && t > 0.2)
        {
            VR_Test_Check(!(VRGetEntWeaponFlags(p, h) & QVR_WPNFLAG_REVGRIP), "flipped again: upright");
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 5: flat-screen play: the thrust, the pull, B's flip
    if(vr_et_case == 5 && VR_FlatPlay(p))
    {
        if(vr_et_s == 0)
        {
            setorigin(p, vr_eh_home - '0 40 0'); // (back from the hall's obstacle 60 ahead of where you began)
            p.velocity = '0 0 0';
            VR_EH_Give(WID_SWORD, VR_SP_Flags());
            vr_et_m = VR_ET_Grunt(52);
            makevectors('0 1 0' * p.v_viewangle_y);
            setorigin(vr_et_m, p.origin + v_forward * 42 - '0 0 1'); // (its body's front just past the spear at rest: its tip 32 ahead, the thrust 14 on)
            vr_sp_n = 0;
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && t > 0.3)
        {
            VR_Test_Check(VR_SP_InHand(p, h), "flat screen: the spear held in the main hand");
            vr_sp_hp = vr_et_m.health;
            localcmd("vr_flatplay_status\nvr_flat_punch main\n"); // (its "spear:" line: flat play's spear pose, a pole weapon's) // (the fire button's punch: a pole weapon's is the spear's thrust)
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && t > 0.9)
        {
            VR_Test_Check(vr_et_m.health < vr_sp_hp, sprintf("flat screen: its thrust hurts a grunt ahead (%g damage; it at %v, you at %v)", vr_sp_hp - vr_et_m.health, vr_et_m.origin, p.origin));
            VR_Test_Check(w != world || VR_SP_InHand(p, h), sprintf("the spear stuck in it or still in the hand (holds %g)", VRGetEntWeapon(p, h)));
            if(w != world)
            {
                dprint("spear: flat thrust stuck, pulling\n");
            }
            VR_ET_To(3);
        }
        else if(vr_et_s == 3 && w != world && t > 0.75 && vr_sp_n < 8)
        {
            localcmd("+attack\n"); // (a fresh press of the fire button pulls a stuck blade: VR_Embed_FlatPull)
            vr_sp_n = vr_sp_n + 1;
            VR_ET_To(7);
        }
        else if(vr_et_s == 7 && t > 0.1)
        {
            localcmd("-attack\n"); // (let go: the next press is a fresh one)
            VR_ET_To(3);
        }
        else if(vr_et_s == 3 && (w == world || vr_sp_n >= 8) && t > 0.75)
        {
            VR_Test_Check(w == world && VR_SP_InHand(p, h), sprintf("flat screen: out (%g presses), back in the hand", vr_sp_n));
            localcmd("impulse 43\n"); // (B: no other ammo, so the grip flips)
            VR_ET_To(4);
        }
        else if(vr_et_s == 4 && t > 0.3)
        {
            VR_Test_Check((VRGetEntWeaponFlags(p, h) & QVR_WPNFLAG_REVGRIP) && VR_SP_InHand(p, h), "flat screen: B flips its grip, still the spear");
            localcmd("impulse 43\n");
            VR_ET_To(5);
        }
        else if(vr_et_s == 5 && t > 0.3)
        {
            VR_Test_Check(!(VRGetEntWeaponFlags(p, h) & QVR_WPNFLAG_REVGRIP), "flat screen: B again: upright");
            VR_ET_NextCase();
        }
        return;
    }
    if(vr_et_case == 5 && !VR_FlatPlay(p))
    {
        VR_ET_NextCase();
        return;
    }
    VR_Test_Verdict();
    vr_et_on = 0;
};
#endif
