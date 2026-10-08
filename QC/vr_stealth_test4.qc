// vr_stealth_test4.qc -- the stealth AI's round of the author's notes of 2026-10-08 (`vr_stealth_test 120`-`129`;
// Misc/quakevr/stealth_tests.sh seethrough|fence|chase|props; docs/vr-port/STEALTH_PLAN.md, "Tests"). Each scene prints
// `stealthtest: <name> PASS|FAIL`:
//   120 a see-through wall (e1m1's door *10 or *29 put between a lit player and a grunt 300 units away): opaque, the grunt
//       doesn't see him nor hear a knock at his feet; with alpha 0.5 it sees him (its meter fills) and hears the knock;
//       with vr_stealth_seethrough 0, opaque again;
//       then the wall opaque but its textures taken for fences ('{' ones: vr_stealth_test_fence, the maps the kit has
//       have no solid fence): seen and heard through; the textures not fences: neither;

float stt4_alpha;     // the see-through wall's alpha this stage
entity stt4_wall;

// The way along +x or -x from `org` with room for `xDist` units (the first clear one); '0 0 0' none.
vector(vector org, float xDist) VR_STT4_ClearAxis =
{
    vector d;
    for(float i = 0; i < 4; i++)
    {
        d = i == 0 ? '1 0 0' : (i == 1 ? '-1 0 0' : (i == 2 ? '0 1 0' : '0 -1 0'));
        tracebox(org, '-16 -16 -24', '16 16 32', org + d * xDist, TRUE, stt2_p);
        if(trace_fraction >= 1)
        {
            return d;
        }
    }
    return '0 0 0';
};

void() VR_STT4_End =
{
    if(stt4_wall)
    {
        remove(stt4_wall);
        stt4_wall = world;
    }
    cvar_set("vr_stealth_seethrough", "1");
    cvar_set("vr_stealth_test_fence", "");
    VR_STT2_RemoveAll();
    VR_STT2_End();
};

// The meter's verdict over a stage: `want` seen (its meter past the glimpse, or Alert, or Hostile) or not.
float(entity m) VR_STT4_Saw =
{
    return m.stl_meter >= cvar("vr_stealth_glimpse") || m.stl_state == STL_ALERT || m.enemy != world;
};

void() VR_STT4_Frame =
{
    entity p = stt2_p;
    float t = time - stt2_t0;
    entity m = stt2_m1;

    // 120: a see-through wall.
    if(stt2_step == 120)
    {
        if(stt2_stage == 0)
        {
            stt2_dir = VR_STT4_ClearAxis(p.origin, 320);
            if(stt2_dir == '0 0 0')
            {
                VR_STT2_Report("seethrough_wall", FALSE, "no room");
                VR_STT4_End();
                return;
            }
            cvar_set("vr_stealth_share_near", "0");
            cvar_set("vr_stealth_share_view", "0");
            cvar_set("vr_stealth_light_force", "128");
            cvar_set("vr_stealth_seethrough", "1");
            stt2_m1 = VR_STT2_Spawn(p.origin + stt2_dir * 300 + '0 0 8', vectoyaw(-stt2_dir), 0);
            // (the wall across the line: *10 is 14 units thick along x, 126 wide and tall; *29 along y, 142 by 110)
            stt4_wall = spawn();
            stt4_wall.classname = "stealthtest_wall";
            stt4_wall.solid = SOLID_BSP;
            stt4_wall.movetype = MOVETYPE_PUSH;
            setmodel(stt4_wall, stt2_dir_y != 0 ? "*29" : "*10");
            stt4_alpha = 0;
            stt2_v = 0;
            VR_STT2_Stage(1);
            return;
        }
        // 1 opaque, 2 alpha 0.5, 3 alpha 0.5 with the switch off, 4 opaque but a fence (its textures taken for '{'
        // ones), 5 not: 3 s each, the grunt calm at the start
        if(stt2_stage >= 1 && stt2_stage <= 5)
        {
            if(t < 0.1)
            {
                // (across the line between their eyes, the grunt landed)
                vector mid = (p.origin + p.view_ofs + m.origin + m.view_ofs) * 0.5;
                setorigin(stt4_wall, mid - (stt4_wall.mins + stt4_wall.maxs) * 0.5);
                stt4_wall.alpha = (stt2_stage == 2 || stt2_stage == 3) ? 0.5 : 0;
                cvar_set("vr_stealth_seethrough", stt2_stage == 3 ? "0" : "1");
                cvar_set("vr_stealth_test_fence", stt2_stage == 4 ? "tech0 key01" : "");
                VR_STT2_Calm(m, m.angles_y);
                stt2_w = 0;
                return;
            }
            if(t >= 0.5 && !stt2_w)
            {
                stt2_w = 1;
                stt2_x = m.stl_heard;
                // a knock where he stands, heard (in the open) a little farther than the grunt is
                VR_Stealth_Noise(p.origin, vlen(m.origin + m.view_ofs - p.origin) * 1.15, world, STL_N_PROP);
            }
            if(t < 3)
            {
                return;
            }
            float saw = VR_STT4_Saw(m);
            float heard = m.stl_heard > stt2_x;
            float want = stt2_stage == 2 || stt2_stage == 4;
            string name = stt2_stage == 1 ? "seethrough_opaque" : (stt2_stage == 2 ? "seethrough_alpha" :
                (stt2_stage == 3 ? "seethrough_off" : (stt2_stage == 4 ? "seethrough_fence" : "seethrough_nofence")));
            VR_STT2_Report(name, saw == want && heard == want, sprintf("meter %.2f state %g hostile %g heard %g (want %g)",
                m.stl_meter, m.stl_state, m.enemy != world, heard, want));
            VR_STT2_Stage(stt2_stage + 1);
            return;
        }
        bprint("stealthtest: seethrough done\n");
        VR_STT4_End();
        return;
    }

    VR_STT4_End();
};
