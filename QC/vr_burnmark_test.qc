#ifdef QVR_TESTS
// vr_burnmark_test.qc -- lightning leaves a lasting burn where it strikes (ROUND24.md, "Lightning burn marks"). This only
// sets the stage; the verdict is Misc/quakevr/burnmark_test.sh's, from pictures of the wall before the bolts and after they
// and their light have gone. `map vrtesthall; r_drawentities 0; vr_burnmark_test <case>`, flat-screen or in the mock headset:
//   1 a thunderbolt held on the wall, by the trigger (+attack: W_Attack, player_light, W_FireLightning)
//   2 a shambler's bolts (CastLightning, on a bare entity) at the same spot
// Three screenshots: at the frames below, the last after a save and a load (the frames go on from the save's; the cvar, which a
// load keeps, at -1 says it has been loaded). `burnmark: loaded` ends the run (run.sh -QuitOn).

float vr_bmt_case;   // the running case (0: none)
float vr_bmt_frame;  // its frames so far
entity vr_bmt_caster; // case 2's shambler
entity vr_bmt_mark;   // what it aims at

vector VR_BMT_EYE = '-400 -350 70';  // vrtesthall: before the plain south wall
vector VR_BMT_SPOT = '-400 -480 70'; // (the case's aim, for the shambler)

float VR_BMT_FRAME_GIVE = 2;    // all the weapons (a frame takes one impulse: these are apart)
float VR_BMT_FRAME_HAND = 8;    // the thunderbolt into the main hand
float VR_BMT_FRAME_BEFORE = 40; // settled: the wall as it was
float VR_BMT_FRAME_FIRE = 46;   // the trigger
float VR_BMT_FRAME_STOP = 106;  // held 60 frames: 0.83 s, eight bolts
float VR_BMT_FRAME_AFTER = 400; // the beam's light, smoke and arcs gone (smoulder 5.5 s isn't on the wall): the wall now
float VR_BMT_FRAME_SAVE = 404;  // for the screenshot to be written: a save
float VR_BMT_FRAME_LOAD = 434;  // and a load
float VR_BMT_FRAME_LOADED = 524; // the map back and settled: the wall again
float VR_BMT_FRAME_END = 528;   // for the screenshot to be written

void() VR_BurnMark_TestFrame =
{
    float ask = cvar_hget(cvarh_vr_burnmark_test);
    if(ask > 0)
    {
        cvar_set("vr_burnmark_test", "0");
        vr_bmt_case = ask;
        vr_bmt_frame = 0;
        cvar_set("vr_weapon_grip_mode", "1"); // (a weapon stays in the mock's hand, not gripping)
        localcmd("vr_mock_look 0 0\nvr_mock_hand main 0.2 1.2 -0.4 70 0 0\n");
    }
    if(!vr_bmt_case)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    vr_bmt_frame = vr_bmt_frame + 1;

    // Hovering there, looking at the wall.
    p.movetype = MOVETYPE_NOCLIP;
    p.velocity = '0 0 0';
    p.flags = p.flags | FL_GODMODE;
    setorigin(p, VR_BMT_EYE - p.view_ofs);
    p.angles = '0 270 0';
    p.v_angle = '0 270 0';
    p.fixangle = TRUE;

    if(vr_bmt_frame == VR_BMT_FRAME_GIVE)
    {
        localcmd("impulse 9\n");
    }
    else if(vr_bmt_frame == VR_BMT_FRAME_HAND)
    {
        localcmd("impulse 161\n");
    }
    else if(vr_bmt_frame == VR_BMT_FRAME_BEFORE || vr_bmt_frame == VR_BMT_FRAME_AFTER)
    {
        localcmd("screenshot\n");
    }
    else if(vr_bmt_frame == VR_BMT_FRAME_FIRE && vr_bmt_case == 1)
    {
        localcmd("+attack\n");
    }
    else if(vr_bmt_frame == VR_BMT_FRAME_STOP && vr_bmt_case == 1)
    {
        localcmd("-attack\n");
    }
    else if(vr_bmt_frame == VR_BMT_FRAME_SAVE && ask == 0)
    {
        bprint(sprintf("burnmark: %g: cells %g\n", vr_bmt_case, p.ammo_cells));
        localcmd("vr_permdecal_info\nsave bm_test\n");
    }
    else if(vr_bmt_frame == VR_BMT_FRAME_LOAD && ask == 0)
    {
        cvar_set("vr_burnmark_test", "-1");
        localcmd("load bm_test\n");
    }
    else if(vr_bmt_frame == VR_BMT_FRAME_LOADED && ask == -1)
    {
        localcmd("vr_permdecal_info\nscreenshot\n");
    }
    else if(vr_bmt_frame == VR_BMT_FRAME_END && ask == -1)
    {
        cvar_set("vr_burnmark_test", "0");
        bprint("burnmark: loaded\n");
        vr_bmt_case = 0;
    }

    // The shambler's bolts, as its magic frames cast them, from beside you at the wall.
    if(vr_bmt_case == 2 && vr_bmt_frame >= VR_BMT_FRAME_FIRE && vr_bmt_frame < VR_BMT_FRAME_STOP)
    {
        if(vr_bmt_caster == world)
        {
            vr_bmt_mark = spawn();
            setorigin(vr_bmt_mark, VR_BMT_SPOT - '0 0 16');
            vr_bmt_caster = spawn();
            vr_bmt_caster.classname = "monster_shambler";
            vr_bmt_caster.flags = FL_MONSTER;
            setorigin(vr_bmt_caster, VR_BMT_EYE - '0 0 40');
            vr_bmt_caster.enemy = vr_bmt_mark;
        }
        if(vr_bmt_frame - VR_BMT_FRAME_FIRE == floor((vr_bmt_frame - VR_BMT_FRAME_FIRE) / 7) * 7)
        {
            entity oself = self;
            self = vr_bmt_caster;
            CastLightning();
            self = oself;
        }
    }
};
#endif
