#ifdef QVR_TESTS
// vr_dagger_test.qc -- the dagger (VR_SWORDKIND_DAGGER: the knight's sword of its kind; ROUND25.md, "The dagger"), by the
// embedded blades' test machinery (vr_embed_test.qc: VR_EH_Blow, the mock hand's real blows; VR_ET_ThrowSword).
// `vr_embed_test 40` (map vrtesthall, god, notarget, on your feet in the open), the dagger in the main hand:
//   0 a swing across a grunt: it hurts it, the dagger still in the hand;
//   1 a stab: stuck by its point in the body, the body hurt; how deep;
//   2 the same stab with the knight's sword: the dagger's blade (how deep a held stab can go: vr_embed_hold_sword_most of
//     it) is about a third of the sword's;
//   3 thrown point first at the wall: it sticks, its tip where its depth says;
//   4 thrown at a live grunt's chest: it sticks in it and hurts it;
//   6, 7 skewered gibs (skewer_depth, skewer_most): a head, a limb, a second gib on the dagger (shallower, one at most) and on
//     the knight's sword (as ever: 8 units from the tip, up to vr_embed_skewer_most).
// With -flatscreenmode (flat-screen play) case 5 alone: the dagger held in the main hand, the mouse button's swing
// (vr_flat_punch main) at a grunt ahead hurts it, the dagger still in the hand.
// Each blow's damage is printed (the dagger's vr_dagger_damage of the sword's). Ends `embeddagger: N passed, M failed`.

float vr_dg_blade;  // case 1: the dagger's stuck blade's length (units)
float vr_dg_hp;     // case 4, 5: the grunt's health before the throw, the swing

float() VR_DG_Flags = { return VR_ADMelee_Flags(VR_SWORDKIND_DAGGER); };

void() VR_DG_Begin =
{
    VR_Test_BeginFlags("embeddagger", VR_TEST_LOUD);
    cvar_set("vr_knockdown", "0"); // (as test 7: a blow's chance of one takes the body from the hand)
    cvar_set("vr_hit_stagger", "0"); // (as test 11: a blow's own chance of a stagger moves the body off the stab)
    vr_eh_home = VR_Player().origin;
    localcmd("vr_mock_hand_to_absolute 1\n");
    vr_et_on = 40;
    vr_et_case = VR_FlatPlay(VR_Player()) ? 5 : 0; // (flat-screen play: its swing alone)
    VR_ET_To(0);
};

void() VR_DG_Frame =
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
    if(vr_et_case <= 2 && vr_et_m != world && vr_et_m.health > 0)
    {
        vr_et_m.nextthink = time + 0.5; // (the body stands where it is, its blow's anger put off: as test 7's)
        vr_et_m.velocity = '0 0 0';
    }
    vr_eh_flags = (vr_et_case == 2 || vr_et_case == 7) ? 0 : VR_DG_Flags(); // (VR_EH_Blow's give; VR_EH_NextCase clears it)

    // ---- 0: a swing across a grunt
    if(vr_et_case == 0)
    {
        if(VR_EH_Blow(WID_SWORD, 6, 0, TRUE, t))
        {
            float kind = VR_ADMelee_Kind(VRGetEntWeaponFlags(p, h));
            VR_Test_Check(vr_et_m.health < vr_et_m.max_health, sprintf("a dagger's swing at 6 m/s hurts a grunt (%g damage)",
                vr_et_m.max_health - vr_et_m.health));
            VR_Test_Check(w != world || (VRGetEntWeapon(p, h) == WID_SWORD && kind == VR_SWORDKIND_DAGGER),
                sprintf("the dagger is still in the hand (holds %g, kind %g)", VRGetEntWeapon(p, h), kind));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 1, 2: a stab (the dagger's, then the knight's sword's)
    if(vr_et_case == 1 || vr_et_case == 2)
    {
        if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
        {
            string name = vr_et_case == 1 ? "dagger" : "knight's sword";
            float stuck = w != world && VR_AxeStick_InMonster(w) && w.vr_stick_embed == AXESTICK_POINT;
            float depth = w != world ? w.vr_stick_depth : 0;
            float blade = w != world ? w.vr_stick_bladelen : 0;
            VR_Test_Check(stuck, sprintf("a %s's stab at 6 m/s is stuck by its point (depth %.1f units)", name, depth));
            VR_Test_Check(vr_et_m.health < vr_et_m.max_health, sprintf("its blow dealt %g damage", vr_et_m.max_health - vr_et_m.health));
            if(vr_et_case == 1)
            {
                VR_Test_Check(w == world || w.model == "progs/v_dagger.mdl", sprintf("the stuck blade is the dagger (%s)", w.model));
                vr_dg_blade = blade;
            }
            else if(stuck)
            {
                VR_Test_Check(vr_dg_blade > blade * 0.25 && vr_dg_blade < blade * 0.45, sprintf("the dagger's blade (a held stab's "
                    "reach in) is a third of the sword's: %.1f units of %.1f", vr_dg_blade, blade));
            }
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 3: thrown at the wall
    if(vr_et_case == 3)
    {
        if(vr_et_s == 0)
        {
            setorigin(p, vr_eh_home);
            vr_et_at = VR_ET_FaceWall(220);
            makevectors('0 1 0' * p.v_viewangle_y);
            vr_et_flags = VR_DG_Flags();
            vr_et_w = VR_ET_ThrowSword(WID_SWORD, vr_et_at - v_forward * 90, v_forward, v_forward, FALSE);
            vr_et_flags = 0;
            VR_ET_To(1);
        }
        else if(VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(vr_et_w.vr_stick_kind == AXESTICK_LEVEL && vr_et_w.model == "progs/v_dagger.mdl",
                sprintf("a thrown dagger sticks in the wall (kind %g, how %g, depth %.1f, %s)", vr_et_w.vr_stick_kind,
                vr_et_w.vr_stick_embed, vr_et_w.vr_stick_depth, vr_et_w.model));
            if(vr_et_w.vr_stick_embed == AXESTICK_POINT)
            {
                VR_ET_CheckTip(vr_et_w);
            }
            VR_ET_NextCase();
        }
        return;
    }
    // ---- 4: thrown at a live grunt
    if(vr_et_case == 4)
    {
        if(vr_et_s == 0)
        {
            vr_et_m = VR_ET_Grunt(110);
            vr_dg_hp = vr_et_m.health;
            vr_et_flags = VR_DG_Flags();
            vr_et_w = VR_ET_SwordAt(VR_ET_Chest(vr_et_m), 80, TRUE);
            vr_et_flags = 0;
            VR_ET_To(1);
        }
        else if(VR_ET_Resolved(vr_et_w))
        {
            VR_Test_Check(VR_AxeStick_InMonster(vr_et_w), sprintf("a dagger thrown at a grunt sticks in it (kind %g, how %g, depth %.1f)",
                vr_et_w.vr_stick_kind, vr_et_w.vr_stick_embed, vr_et_w.vr_stick_depth));
            VR_Test_Check(vr_et_m.health < vr_dg_hp, sprintf("the throw hurt it (%g damage)", vr_dg_hp - vr_et_m.health));
            VR_ET_NextCase();
        }
        return;
    }
    // ---- 5: flat-screen play: held, the mouse button's swing
    if(vr_et_case == 5 && VR_FlatPlay(p))
    {
        if(vr_et_s == 0)
        {
            setorigin(p, vr_eh_home);
            p.velocity = '0 0 0';
            VR_EH_Give(WID_SWORD, VR_DG_Flags());
            vr_et_m = VR_ET_Grunt(34);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && t > 0.3)
        {
            float k = VR_ADMelee_Kind(VRGetEntWeaponFlags(p, h));
            VR_Test_Check(VRGetEntWeapon(p, h) == WID_SWORD && k == VR_SWORDKIND_DAGGER, sprintf("flat screen: the dagger held in "
                "the main hand (holds %g, kind %g)", VRGetEntWeapon(p, h), k));
            vr_dg_hp = vr_et_m.health;
            localcmd("vr_flat_punch main\n");
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && (vr_et_m.health < vr_dg_hp || t > 2.5))
        {
            VR_Test_Check(vr_et_m.health < vr_dg_hp, sprintf("flat screen: its swing hurts a grunt ahead (%g damage)",
                vr_dg_hp - vr_et_m.health));
            VR_Test_Check(VRGetEntWeapon(p, h) == WID_SWORD, sprintf("the dagger still in the hand (holds %g)", VRGetEntWeapon(p, h)));
            VR_ET_NextCase();
        }
        return;
    }
    // ---- 6, 7: skewered gibs (his 2026-11-13: too deep on the dagger, one at most): a head, then a limb, then a second gib, on the
    // dagger (6) and, as it is, on the knight's sword (7). Where each sits: from the tip, and from the hand end of the blade's line.
    if((vr_et_case == 6 || vr_et_case == 7) && !VR_FlatPlay(p))
    {
        float dag = vr_et_case == 6;
        float want = cvar_hget(cvarh_vr_embed_skewer_depth) * (dag ? 0.5 : 1);
        if(vr_et_s == 0)
        {
            setorigin(p, vr_eh_home);
            p.velocity = '0 0 0';
            cvar_set("vr_embed", "1");
            cvar_set("vr_embed_skewer_most", "3");
            localcmd("+grabmain\n");
            VR_ET_To(10);
        }
        else if(vr_et_s == 10 && VR_HandGrabUtil_IsHandGrabbing(p, h))
        {
            VR_EH_Give(WID_SWORD, dag ? VR_DG_Flags() : 0);
            VR_Blow(h).mh_far = '0 0 0';
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && VR_Blow(h).mh_far != '0 0 0')
        {
            makevectors(p.v_angle);
            vr_ets_g[0] = VR_ETS_Head(p, VR_ETS_OnLine(0));
            vr_ets_g[1] = VR_ETS_Gib(p, VR_ETS_OnLine(0));
            vr_ets_g[1].vr_prop_mass = 12.4; // (a soldier's leg)
            vr_ets_g[2] = VR_ETS_Gib(p, VR_ETS_OnLine(0));
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && vr_ets_g[0].takedamage && vr_ets_g[1].takedamage && vr_ets_g[2].takedamage)
        {
            // The head
            entity g = vr_ets_g[0];
            VR_ETS_PutOn(g, 0);
            VR_ETS_Blow(p, g, VR_MKIND_STAB, 6);
            float len = VR_Skewer_Line(g);
            float back = VR_Skewer_Back(g, len);
            VR_Test_Check(g.vr_skewer_p == p, sprintf("%s: a head skewered", dag ? "dagger" : "sword"));
            VR_Test_Check(fabs(back - want) < 0.5, sprintf("%s: the head's middle %.1f from the tip (wanted %.1f); %.1f from the hilt end of the blade's line (%.1f long)",
                dag ? "dagger" : "sword", back, want, len - back, len));
            VR_Test_Check(len - back > len * 0.5, sprintf("well short of the hilt: %.0f%% of the line from it", 100 * (len - back) / len));
            // A second gib (a limb) while one is on
            g = vr_ets_g[1];
            VR_ETS_PutOn(g, 0);
            VR_ETS_Blow(p, g, VR_MKIND_STAB, 6);
            if(dag)
            {
                VR_Test_Check(g.vr_skewer_p == world, "a second gib is not taken (blade full: one at most)");
            }
            else
            {
                VR_Test_Check(g.vr_skewer_p == p, "a second gib is taken, as before");
            }
            VR_ET_To(3);
        }
        else if(vr_et_s == 3)
        {
            // The limb alone on the blade (the head taken off it)
            if(vr_ets_g[0].vr_skewer_p == p)
            {
                remove(vr_ets_g[0]);
            }
            entity g = vr_ets_g[2];
            if(dag && vr_ets_g[1].vr_skewer_p == p)
            {
                remove(vr_ets_g[1]);
            }
            if(!dag)
            {
                g = vr_ets_g[1]; // (the sword: the limb that is on it)
            }
            if(g.vr_skewer_p != p)
            {
                VR_ETS_PutOn(g, 0);
                g.vr_prop_mass = 12.4;
                VR_ETS_Blow(p, g, VR_MKIND_STAB, 6);
            }
            float len2 = VR_Skewer_Line(g);
            float back2 = VR_Skewer_Back(g, len2);
            VR_Test_Check(g.vr_skewer_p == p, sprintf("%s: a limb skewered", dag ? "dagger" : "sword"));
            float shift = dag ? 0 : cvar_hget(cvarh_vr_embed_skewer_gap) * g.vr_skewer_slot;
            VR_Test_Check(fabs(back2 - (want + shift)) < 0.5, sprintf("%s: the limb's middle %.1f from the tip (wanted %.1f); %.1f from the hilt end (%.1f long)",
                dag ? "dagger" : "sword", back2, want + shift, len2 - back2, len2));
            VR_EH_NextCase();
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
