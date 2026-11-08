#ifdef QVR_TESTS
// vr_embed_1108_test.qc -- `vr_embed_test 28` (embed1108): his notes of 2026-11-08 on embedded blades (ROUND24.md, "Embedded blades:
// his 11-08 notes"), by the mock hand's real stabs (vr_embed_test.qc's VR_EH_Blow):
// 0  a stab that kills the grunt (17:16): the blade stays in its model (not its box) and, let go of, lies with the body as it falls,
//    not floating where it stood.
// 1  a stab held, you running on into it (17:17): Running Stab: Brake cuts your speed along the blade; with Brake 0, only friction.
// 2  a blade left in a body (17:09): a force grab finds it from in front, through its own host.
// Ends `embed1108: N passed, M failed`.

float vr_e8_v;     // a number kept across states
vector vr_e8_at;
float vr_e8_brake; // Running Stab: Brake as the test began

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
    if(vr_et_case > 4)
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
