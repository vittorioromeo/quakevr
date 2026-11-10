#ifdef QVR_TESTS
// vr_embed_1108_test.qc -- `vr_embed_test 28` (embed1108): his notes of 2026-11-08 on embedded blades (ROUND24.md, "Embedded blades:
// his 11-08 notes"), by the mock hand's real stabs (vr_embed_test.qc's VR_EH_Blow):
// 0  a stab that kills the grunt (17:16): the blade stays in its model (not its box) and, let go of, lies with the body as it falls,
//    not floating where it stood.
// 1, 2 a stab held, you running on into it (17:17), Brake on and Brake 0.
// 3  a blade left in a body (17:09): a force grab finds it from in front, through its own host.
// 4  a blade's strike on a dead ragdoll (17:07): its wound where it struck, not from its box's middle.
// 5  a shot across a gib skewered on the blade in the hand (17:10): it hits it, off the blade.
// 6  an axe swung into a dead ragdoll (2026-11-10): it stays in, the ragdoll not gibbed.
// 7-10 an axe swung and a sword's stab that stay in the training dummy, then in a grunt, pulled out: their blood (the
//    client's: Misc/quakevr/embed_blood_test.sh judges it).
// Ends `embed1108: N passed, M failed`.

float vr_e8_v;     // a number kept across states
vector vr_e8_at;
float vr_e8_brake; // Running Stab: Brake as the test began
entity vr_e8_g;    // case 5's gib
float vr_e8_rag;   // case 6's step: 1 the grunt killed, 2 the ragdoll struck
float vr_e8_limbs; // and the settings it turns off meanwhile
float vr_e8_decap;

void() VR_E8_Begin =
{
    VR_Test_BeginFlags("embed1108", VR_TEST_LOUD);
    cvar_set("vr_knockdown", "0"); // (as test 7: a blow's chance of one takes the body from the hand)
    vr_e8_brake = cvar_hget(cvarh_vr_embed_brake);
    vr_eh_home = VR_Player().origin;
    localcmd("vr_mock_hand_to_absolute 1\n");
    vr_et_on = 28;
    vr_et_case = 0;
    VR_ET_To(0);
};

void() VR_E8_Next =
{
    localcmd(sprintf("tune vr_embed_brake %g\n", vr_e8_brake));
    VR_EH_NextCase();
};

void() VR_E8_Step =
{
    entity p = self;
    float t = time - vr_et_t0;
    entity w = VR_EH_Blade();
    // ---- 4: a blade's strike on a dead ragdoll (17:07): its wound and dent where it struck, not from its box's middle
    if(vr_et_case == 4)
    {
        if(vr_et_s == 0)
        {
            setorigin(p, vr_eh_home);
            p.velocity = '0 0 0';
            vr_et_m = VR_ET_Grunt(54);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && vr_et_m.takedamage && t > 0.1)
        {
            vr_et_m.health = 1;
            T_Damage(vr_et_m, p, p, 2);
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && t > 2)
        {
            // (A limb: off the box's middle as the body lies.)
            vector at = limbplace(vr_et_m, 0, 3);
            float b = ragdolllimbs(vr_et_m, 0);
            for(float k = 0; k < VR_LIMB_JOINTS; k++)
            {
                if(b & VR_AgArmor_Bit(k))
                {
                    at = limbplace(vr_et_m, k, 3);
                }
            }
            vector mid = (vr_et_m.absmin + vr_et_m.absmax) * 0.5;
            vector eye = p.origin + p.view_ofs;
            hit_where = '0 0 0';
            VR_Corpse_Struck(cVR_MainHand, vr_et_m, at, normalize(at - eye), 400, FALSE);
            VR_Test_Check(vlen(hit_where - at) < 1,
                sprintf("case 4: a blade's strike on a dead ragdoll wounds where it struck (%.1f units off it; its box's middle %.1f off)", vlen(hit_where - at), vlen(mid - at)));
            VR_E8_Next();
        }
        return;
    }
    // ---- 5: a shot through a gib skewered on the blade in the hand (17:10): it hits it, off the blade, not through it
    if(vr_et_case == 5)
    {
        if(vr_et_s == 0)
        {
            setorigin(p, vr_eh_home);
            p.velocity = '0 0 0';
            localcmd("+grabmain\n");
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && VR_HandGrabUtil_IsHandGrabbing(p, cVR_MainHand) && t > 0.1)
        {
            VR_EH_Give(WID_SWORD, 0);
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && t > 0.3)
        {
            vr_e8_g = VR_ETS_Gib(p, VR_ETS_OnLine(0));
            VR_ETS_PutOn(vr_e8_g, 0);
            VR_ETS_Blow(p, vr_e8_g, VR_MKIND_STAB, 6);
            VR_ET_To(3);
        }
        else if(vr_et_s == 3 && t > 0.2)
        {
            VR_Test_Check(vr_e8_g.vr_skewer_p == p, "case 5: a gib skewered on the blade in the hand");
            if(vr_e8_g.vr_skewer_p == p)
            {
                makevectors(p.angles);
                vector from = vr_e8_g.vr_skewer_c + v_right * 60;
                vector to = vr_e8_g.vr_skewer_c - v_right * 60;
                traceline(from, to, MOVE_HITGIBS | MOVE_HITMODEL, p);
                float before = trace_ent == vr_e8_g;
                // (As if the pellet's trace met nothing: it is the gib alone that is asked about, not this spot's walls.)
                trace_fraction = 1;
                trace_ent = world;
                VR_Gib_TraceHeld(from, to); // (FireBulletsImpl's, after its traceline)
                VR_Test_Check(trace_ent == vr_e8_g && vr_e8_g.vr_skewer_p == world && vr_e8_g.takedamage,
                    sprintf("a shot across it hits it, off the blade (the trace alone met it %g; it hit %s)", before, trace_ent.classname));
            }
            remove(vr_e8_g); // (loose now, or still on the blade: nothing removed it)
            VR_E8_Next();
        }
        return;
    }
    // ---- 6: an axe swung into a dead ragdoll (his note 2026-11-10: "still completely gibs it instead of embedding the axe"):
    // it stays in, the ragdoll whole (its blow the living's: VR_Corpse_BlowDamage)
    if(vr_et_case == 6)
    {
        if(vr_et_s == 0 && vr_e8_rag == 0)
        {
            setorigin(p, vr_eh_home);
            p.velocity = '0 0 0';
            vr_et_m = VR_ET_Grunt(44);
            vr_e8_rag = 1;
            // (Limb and head cuts off for it: a ragdoll's random pose may put an arm or the neck under the blade, and a
            // cut goes through, rightly unstuck; this case is the gib or the stick.)
            vr_e8_limbs = cvar_hget(cvarh_vr_limbs_corpses);
            vr_e8_decap = cvar_hget(cvarh_vr_decap_corpses);
            cvar_set("vr_limbs_corpses", "0");
            cvar_set("vr_decap_corpses", "0");
            vr_et_t0 = time;
            return;
        }
        if(vr_et_s == 0 && vr_e8_rag == 1)
        {
            if(vr_et_m.health > 0 && t > 0.1)
            {
                vr_et_m.health = 1;
                T_Damage(vr_et_m, p, p, 2);
                vr_et_t0 = time;
            }
            else if(vr_et_m.health <= 0 && t > 2.5)
            {
                VR_Test_Check(isragdoll(vr_et_m) && !vr_et_m.vr_gib, "case 6: the grunt lies dead, a ragdoll");
                vr_ee_target = vr_et_m;
                vr_eh_down = 1;
                vr_e8_rag = 2;
                vr_corpse_last_hit = -1;
            }
            return;
        }
        if(VR_EH_Blow(WID_AXE, 7, 0, TRUE, t))
        {
            VR_Test_Check(!vr_et_m.vr_gib && vr_et_m.model != string_null, sprintf("an axe swung into it doesn't gib it (its blow %.1f on its corpse health, overkill %g)", vr_corpse_last_hit, cvar_hget(cvarh_vr_corpse_gib_overkill)));
            VR_Test_Check(VR_EH_Blade() != world, "the axe stays in it, held");
            vr_ee_target = world;
            vr_eh_down = 0;
            vr_e8_rag = 0;
            cvar_set("vr_limbs_corpses", ftos(vr_e8_limbs));
            cvar_set("vr_decap_corpses", ftos(vr_e8_decap));
            VR_E8_Next();
        }
        return;
    }
    // ---- 7-10: a blow that stays in, pulled out (his note 2026-11-10: "if I stab the dummy with the axe, then the axe
    // doesn't get bloody at all on the blade"): 7 an axe swung into the training dummy, 8 a sword's stab into it, 9 and 10
    // the same into a grunt. The blood is the client's (vr_wounds.cpp): `embedblood:` marks and vr_gore_hands_info's gear
    // lines, stuck and back in the hand, for the script (Misc/quakevr/embed_blood_test.sh) to judge.
    if(vr_et_case >= 7 && vr_et_case <= 10)
    {
        float dummy = vr_et_case <= 8;
        float wid = (vr_et_case == 7 || vr_et_case == 9) ? WID_AXE : WID_SWORD;
        if(vr_et_s == 0 && vr_e8_rag == 0)
        {
            setorigin(p, vr_eh_home);
            p.velocity = '0 0 0';
            localcmd("vr_wounds_debug 1\n");
            entity g = VR_ET_Grunt(wid == WID_AXE ? 44 : 54);
            if(dummy)
            {
                entity d = spawn();
                d.classname = "monster_army"; // (as the map's: the dummy stands as a grunt)
                setorigin(d, g.origin);
                d.angles = g.angles;
                remove(g);
                entity os = self;
                self = d;
                vr_dummy();
                self = os;
                g = d;
            }
            else
            {
                g.health = 500;
            }
            vr_et_m = g;
            vr_e8_rag = 1;
            vr_et_t0 = time;
            return;
        }
        if(vr_et_s == 0 && vr_e8_rag == 1)
        {
            if(t > 0.6)
            {
                vr_ee_target = vr_et_m;
                vr_e8_rag = 2;
            }
            return;
        }
        if(vr_et_s < 10)
        {
            if(!dummy && vr_et_m.health > 0)
            {
                vr_et_m.nextthink = time + 0.5; // (stands still for the blow)
                vr_et_m.velocity = '0 0 0';
            }
            if(VR_EH_Blow(wid, wid == WID_AXE ? 7 : 6, 0, wid == WID_AXE, t))
            {
                VR_Test_Check(VR_EH_Blade() != world, sprintf("case %g: the %s stays in the %s", vr_et_case,
                    wid == WID_AXE ? "axe swung" : "sword's stab", dummy ? "dummy" : "grunt"));
                VR_ET_To(10);
            }
            return;
        }
        if(vr_et_s == 10 && t > 0.2) // (before the stuck blade's own bleeding adds to it)
        {
            localcmd(sprintf("echo embedblood: %g stuck\nvr_gore_hands_info\n", vr_et_case));
            if(w != world)
            {
                VR_EH_HandTo(VRGetHandPos(cVR_MainHand) - w.vr_stick_dir * 50, 1.6);
            }
            VR_ET_To(11);
        }
        else if(vr_et_s == 11 && ((w == world && VRGetEntWeapon(p, cVR_MainHand) == wid) || t > 4))
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, cVR_MainHand) == wid, "pulled out: back in the hand");
            VR_ET_To(12);
        }
        else if(vr_et_s == 12 && t > 0.5)
        {
            // (Asked while the hand still holds it: the next case empties the hand.)
            localcmd(sprintf("echo embedblood: %g in hand\nvr_gore_hands_info\nvr_wounds_debug 0\n", vr_et_case));
            VR_ET_To(13);
        }
        else if(vr_et_s == 13 && t > 0.3)
        {
            vr_ee_target = world;
            vr_e8_rag = 0;
            if(dummy)
            {
                remove(vr_et_m); // (the dummy: not a grunt the cases clean up)
                vr_et_m = world;
            }
            VR_E8_Next();
        }
        return;
    }
    if(vr_et_case > 10)
    {
        VR_Test_Verdict();
        vr_et_on = 0;
        return;
    }
    // (The body stands still for the blow, as test 7's; case 0's dies of it.)
    if(vr_et_m != world && vr_et_m.health > 0)
    {
        vr_et_m.nextthink = time + 0.5;
        vr_et_m.velocity = '0 0 0';
        if(vr_et_case <= 2)
        {
            vr_et_m.health = 1; // (1, 2: a body the stab kills is no longer solid: nothing but the blade stops you running on, his case)
        }
    }
    if(vr_et_s < 10)
    {
        if(vr_et_case == 2)
        {
            localcmd("tune vr_embed_brake 0\n"); // (a tunable: set ahead, it takes a frame or so)
        }
        if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
        {
            VR_ET_To(10);
        }
        return;
    }

    // ---- 0: the stab kills
    if(vr_et_case == 0 && vr_et_s == 10)
    {
        VR_Test_Check(w != world && vr_et_m.health <= 0, sprintf("case 0: a stab that kills is stuck (blade %g, health %g)", w != world, vr_et_m.health));
        if(w == world)
        {
            VR_E8_Next();
            return;
        }
        vr_et_w = w;
        VR_Test_Check(w.vr_stick_kind == AXESTICK_MODEL, sprintf("in the dying body's model, not its box (kind %g)", w.vr_stick_kind));
        localcmd("-grabmain\n"); // (let go: it stays in the body)
        VR_ET_To(11);
        return;
    }
    if(vr_et_case == 0 && vr_et_s == 11 && t > 2)
    {
        vector torso = limbplace(vr_et_m, 0, 3);
        float off = vlen(vr_et_w.vr_stick_at - torso);
        VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && !VR_Embed_Held(vr_et_w), sprintf("let go: it stays in the body (kind %g)", vr_et_w.vr_stick_kind));
        VR_Test_Check(off < 32, sprintf("the body fallen, the blade with it: its wound %.1f units from the torso (at %v, the torso %v)", off, vr_et_w.vr_stick_at, torso));
        VR_E8_Next();
        return;
    }

    // ---- 1, 2: running on into a stab held, braked, then Brake 0
    if((vr_et_case == 1 || vr_et_case == 2) && vr_et_s == 10)
    {
        VR_Test_Check(w != world, sprintf("case %g: stuck", vr_et_case));
        if(w == world)
        {
            VR_E8_Next();
            return;
        }
        if(vr_et_case == 2)
        {

        }
        vr_et_w = w;
        vr_e8_at = vr_eh_fw; // (the way in, level)
        p.velocity = vr_e8_at * 320; // (running at it)
        vr_embed_braked = 0;
        vr_e8_v = 0;
        VR_ET_To(12);
        return;
    }
    if((vr_et_case == 1 || vr_et_case == 2) && vr_et_s == 12)
    {
        // (One frame: the blade's think after your move. Your speed itself can't tell it here: with no stick held, the mock's
        // player stops from a run within 5 frames either way, 2.4 m/s left, braked or not.)
        float along = p.velocity * vr_e8_at / VRMetersToUnits();
        if(vr_et_case == 1)
        {
            VR_Test_Check(vr_embed_braked > 1.5, sprintf("running on into it (8.4 m/s): the blade pushes back, %.1f m/s off in a frame (%.1f m/s left)", vr_embed_braked, along));
        }
        else
        {
            VR_Test_Check(vr_embed_braked == 0, sprintf("Brake 0: nothing taken off (%.1f m/s; %.1f m/s left)", vr_embed_braked, along));
        }
        p.velocity = '0 0 0';
        VR_E8_Next();
        return;
    }

    // ---- 3: a force grab through its own host
    if(vr_et_case == 3 && vr_et_s == 10)
    {
        VR_Test_Check(w != world, "case 3: stuck");
        if(w == world)
        {
            VR_E8_Next();
            return;
        }
        vr_et_w = w;
        localcmd("-grabmain\n");
        VR_ET_To(13);
        return;
    }
    if(vr_et_case == 3 && vr_et_s == 13 && t > 0.3)
    {
        // From before the body, the side the blade went in: the line to the blade's middle meets the body first.
        vector c = VR_Forcegrab_Centre(vr_et_w);
        vector from = c - vr_eh_fw * 60 + '0 0 4';
        traceline(from, c, MOVE_PORTALS, p);
        float behind = trace_ent == vr_et_m;
        entity found = VR_Forcegrab_FindTargetAlong(from, normalize(c - from));
        VR_Test_Check(VR_AxeStick_InMonster(vr_et_w) && found == vr_et_w, sprintf("a force grab finds the blade in its own host (found %s, the line meets the body first %g)", found.classname, behind));
        VR_E8_Next();
        return;
    }
};
#endif
