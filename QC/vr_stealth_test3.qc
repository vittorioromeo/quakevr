// vr_stealth_test3.qc -- the stealth AI through seamless teleporters (`vr_stealth_test 110` on vrteleporters, the player
// standing in room U at -1100 1408: Misc/quakevr/stealth_tests.sh gates; docs/vr-port/STEALTH_PLAN.md, "Teleporters").
// Room T's north gate comes out of U's east wall (a 90-degree turn): a monster in T sees and hears U through it. The
// scenes run one after the other, each printing `stealthtest: <name> PASS|FAIL`:
//   110 a grunt sees him through the gate: Alert at his image (not at him, behind T's wall), walks into the gate and
//       through, on to where he stood, searches, and back through the gate to its post (two crossings: no ping-pong);
//   111 a knock in U, out of a grunt's reach the straight way through the walls but not through the gate: heard, the same
//       walk there and back; with vr_stealth_gates 0 not heard; one reaching 0.85 of the way through the gate not heard;
//   112 a dog (a melee monster) sees him through the gate (its meter fills) and goes through to him; with
//       vr_stealth_gates 0 it doesn't see him;
//   113 vr_ai_enhanced 0 (Quake's AI): the dog doesn't see him through the gate, the grunt does (vr_portals_ai).

float stt3_gate;      // T's north gate (portal_ai_gateinfo's numbering)
vector stt3_image;    // what the monster should go to, as seen from T
vector stt3_spot;     // and where it really is, in U
vector stt3_post;
float stt3_room;      // the monster's last room (1 T, 2 U)
float stt3_cross;     // its crossings between them
float stt3_near;      // how near the spot it came in U
float stt3_hostile;

float(vector o) VR_STT3_Room =
{
    if(o_x > -1600 && o_x < -960 && o_y > 300 && o_y < 768)
    {
        return 1;
    }
    if(o_x > -1700 && o_x < -960 && o_y > 1100)
    {
        return 2;
    }
    return 0;
};

// The gate whose face's middle is at `mid` (0: none).
float(vector mid) VR_STT3_Gate =
{
    float n = portal_ai_gateinfo(0, 0);
    for(float g = 1; g <= n; g++)
    {
        if(vlen(portal_ai_gate(g, 0, VEC_ORIGIN) - mid) < 8)
        {
            return g;
        }
    }
    return 0;
};

void() VR_STT3_End =
{
    cvar_set("vr_stealth_gates", "1");
    cvar_set("vr_ai_enhanced", "1");
    stt2_p.flags = stt2_p.flags - (stt2_p.flags & FL_NOTARGET);
    VR_STT2_RemoveAll();
    VR_STT2_End();
};

void(float xNext) VR_STT3_Next =
{
    VR_STT2_RemoveAll();
    stt2_step = xNext;
    stt2_v = 0;
    VR_STT2_Stage(0);
};

// The walk there and back (110, 111): its room each frame, its crossings, how near the spot it came; TRUE when done.
float(entity m, string xName, float t) VR_STT3_Walk =
{
    float r = VR_STT3_Room(m.origin);
    if(r && stt3_room && r != stt3_room)
    {
        stt3_cross = stt3_cross + 1;
    }
    if(r)
    {
        stt3_room = r;
    }
    if(r == 2)
    {
        vector d = m.origin - stt3_spot;
        d_z = 0;
        stt3_near = min(stt3_near, vlen(d));
    }
    if(m.enemy != world)
    {
        stt3_hostile = TRUE;
    }
    if(t < 60 && m.health > 0 && m.stl_state == STL_ALERT)
    {
        return FALSE;
    }
    vector home = m.origin - stt3_post;
    home_z = 0;
    VR_STT2_Report(sprintf("%s_walk", xName), stt3_near < 96 && !stt3_hostile,
        sprintf("in U came within %.0f of the spot (96)%s", stt3_near, stt3_hostile ? "; went hostile" : ""));
    VR_STT2_Report(sprintf("%s_back", xName), stt3_cross == 2 && r == 1 && vlen(home) < 48 && m.stl_state == STL_IDLE,
        sprintf("%g crossings (2), back in room %g (1), %.0f from its post (48), state %g, after %.1f s", stt3_cross, r,
        vlen(home), m.stl_state, t));
    return TRUE;
};

void() VR_STT3_Frame =
{
    entity p = stt2_p;
    float t = time - stt2_t0;
    entity m = stt2_m1;
    if(stt2_stage == 0)
    {
        stt3_gate = VR_STT3_Gate('-1280 768 48');
        if(!stt3_gate || VR_STT3_Room(p.origin) != 2)
        {
            VR_STT2_Report("gates", FALSE, sprintf("SKIP: not on vrteleporters in room U (gate %g, at %v), or teleporters off",
                stt3_gate, p.origin));
            VR_STT3_End();
            return;
        }
        p.flags = p.flags | FL_NOTARGET;
        cvar_set("vr_stealth_light_force", "128");
        cvar_set("vr_stealth_gates", "1");
        cvar_set("vr_ai_enhanced", "1");
        stt3_spot = p.origin;
        stt3_post = '-1280 450 24';
        stt3_room = 1;
        stt3_cross = 0;
        stt3_near = 9999;
        stt3_hostile = FALSE;
        stt2_m1 = VR_STT2_Spawn(stt3_post, (stt2_step == 111) ? 270 : 90, (stt2_step == 112 || stt2_step == 113) ? 7 : 0);
        if(stt2_step == 113)
        {
            stt2_m2 = VR_STT2_Spawn(stt3_post + '96 0 0', 90, 0);
        }
        VR_STT2_Stage(1);
        return;
    }
    if(stt2_stage == 1)
    {
        if(t < 0.6)
        {
            return;
        }
        VR_STT2_Calm(m, m.angles_y);
        VR_STT2_Calm(stt2_m2, 90);
        if(stt2_step == 113)
        {
            cvar_set("vr_ai_enhanced", "0");
        }
        if(stt2_step == 111)
        {
            stt3_image = portal_ai_map(stt3_gate, stt3_spot, FALSE);
            VR_Stealth_Noise(stt3_spot, 600, world, STL_N_PROP);
        }
        else
        {
            stt3_image = portal_ai_map(stt3_gate, stt3_spot, FALSE);
            p.flags = p.flags - (p.flags & FL_NOTARGET);
        }
        VR_STT2_Stage(2);
        return;
    }

    // 110, 111: Alert at the image through the gate; there and back.
    if(stt2_step == 110 || stt2_step == 111)
    {
        string name = stt2_step == 110 ? "gate_seen" : "gate_noise";
        if(stt2_stage == 2)
        {
            if(m.stl_state != STL_ALERT && t < 10)
            {
                return;
            }
            p.flags = p.flags | FL_NOTARGET; // (out of its sight from now: it investigates)
            vector miss = m.stl_point - stt3_image;
            miss_z = 0;
            VR_STT2_Report(sprintf("%s_point", name), m.stl_state == STL_ALERT && vlen(miss) < 40 && m.stl_via == stt3_gate,
                sprintf("state %g after %.1f s, at %v (the image %v: %.0f off, 40), via %g (gate %g)", m.stl_state, t,
                m.stl_point, stt3_image, vlen(miss), m.stl_via, stt3_gate));
            if(m.stl_state != STL_ALERT)
            {
                VR_STT3_Next(stt2_step + 1);
                return;
            }
            VR_STT2_Stage(3);
            return;
        }
        if(stt2_stage == 3)
        {
            if(!VR_STT3_Walk(m, name, t))
            {
                return;
            }
            if(stt2_step == 111)
            {
                // the control: the same knock with vr_stealth_gates 0, the grunt back at its post
                setorigin(m, stt3_post);
                VR_STT2_Calm(m, 270);
                cvar_set("vr_stealth_gates", "0");
                VR_Stealth_Noise(stt3_spot, 600, world, STL_N_PROP);
                VR_STT2_Stage(4);
                return;
            }
            VR_STT3_Next(111);
            return;
        }
        if(stt2_stage == 4 && t > 1)
        {
            VR_STT2_Report("gate_noise_off", m.stl_state == STL_IDLE && m.stl_heard == 0,
                sprintf("vr_stealth_gates 0: state %g, heard at %.1f (0)", m.stl_state, m.stl_heard));
            cvar_set("vr_stealth_gates", "1");
            // the way through the gate is the noise's distance: a knock reaching 0.85 of it isn't heard
            stt2_w = vlen(stt3_image - (m.origin + m.view_ofs));
            VR_Stealth_Noise(stt3_spot, stt2_w * 0.85, world, STL_N_PROP);
            VR_STT2_Stage(5);
            return;
        }
        if(stt2_stage == 5 && t > 1)
        {
            VR_STT2_Report("gate_noise_reach", m.stl_state == STL_IDLE && m.stl_heard == 0,
                sprintf("a knock reaching %.0f of the %.0f units through the gate: state %g, heard at %.1f (0)",
                stt2_w * 0.85, stt2_w, m.stl_state, m.stl_heard));
            VR_STT3_Next(112);
        }
        return;
    }

    // 112: the dog sees him through the gate and goes through to him; then vr_stealth_gates 0: it doesn't see him.
    if(stt2_step == 112)
    {
        if(stt2_stage == 2)
        {
            if(m.stl_meter > 0 || m.stl_state == STL_ALERT || m.enemy == p)
            {
                stt2_v = TRUE;
            }
            if(VR_STT3_Room(m.origin) != 2 && t < 25)
            {
                return;
            }
            VR_STT2_Report("gate_dog", stt2_v && VR_STT3_Room(m.origin) == 2,
                sprintf("saw him %g, in U after %.1f s (25), state %g, enemy %s, at %v", stt2_v, t, m.stl_state,
                m.enemy.classname, m.origin));
            p.flags = p.flags | FL_NOTARGET;
            setorigin(m, stt3_post);
            VR_STT2_Calm(m, 90);
            cvar_set("vr_stealth_gates", "0");
            VR_STT2_Stage(3);
            return;
        }
        if(stt2_stage == 3 && t > 0.3)
        {
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            VR_STT2_Stage(4);
            return;
        }
        if(stt2_stage == 4)
        {
            if(m.stl_meter > 0 || m.enemy != world || m.stl_state == STL_ALERT)
            {
                stt2_v = 2;
            }
            if(t < 5 && stt2_v != 2)
            {
                return;
            }
            VR_STT2_Report("gate_dog_off", stt2_v != 2 && VR_STT3_Room(m.origin) == 1,
                sprintf("vr_stealth_gates 0: meter %.2f, state %g, enemy %s, in room %g (1)", m.stl_meter, m.stl_state,
                m.enemy.classname, VR_STT3_Room(m.origin)));
            cvar_set("vr_stealth_gates", "1");
            VR_STT3_Next(113);
        }
        return;
    }

    // 113: Quake's AI: the dog doesn't see him through the gate (only ranged monsters do), the grunt does.
    if(stt2_step == 113 && stt2_stage == 2)
    {
        if(t < 5 && stt2_m2.enemy == world)
        {
            return;
        }
        VR_STT2_Report("gate_quake_ai", m.enemy == world && m.vr_ai_gate == 0 && VR_STT3_Room(m.origin) == 1 &&
            stt2_m2.enemy == p, sprintf("vr_ai_enhanced 0: the dog's enemy %s (none), gate %g, room %g; the grunt's enemy %s "
            "(player) after %.1f s", m.enemy.classname, m.vr_ai_gate, VR_STT3_Room(m.origin), stt2_m2.enemy.classname, t));
        bprint(sprintf("stealthtest: gates done, %g failed\n", stt2_fails));
        VR_STT3_End();
    }
};
