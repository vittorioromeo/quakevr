#ifdef QVR_TESTS
// vr_waterzap_test.qc -- lightning into water, as a player sees it (ROUND24.md, "Lightning on water"). This only sets the
// stage; the verdict is Misc/quakevr/waterzap_test.sh's, from pictures: each case runs twice, vr_lg_water_arcs 1 and 0, on the
// same frames, and the changed pixels of the two screenshots are the effect (waterzap_diff.py).
//
// `vr_waterzap_test <case>`, on any map with open water (vrfiringrange, vrstart), flat-screen or in the mock headset:
// over the water with the widest stretch ahead, looking along it:
//   1 a thunderbolt fired by the trigger, steeply down (+attack: W_Attack, player_light, W_FireLightning)
//   2 the same at a glancing angle
//   3 Mjolnir swung (the flat punch by +attack, the mock's swing), landing on a knight over the water (vr_test_spawn 5 before
//     the map: impulse 241): VR_Melee_Deal, HIP_FireMjolnir
//   4 a shambler's bolt (CastLightning) from above, at you in the water
// The screenshot is taken VR_WZ_FRAME_LIT frames after the client first draws the effect (watershockdrawn: it counts
// with the arcs' width at 0 too), so both runs of a case shoot the same frame; `waterzap: <case>: the screenshot's
// taken` ends the run (run.sh -QuitOn).

float vr_wz_case;     // the running case (0: none)
float vr_wz_frame;    // its frames so far
vector vr_wz_surface; // the water's surface under the stage
float vr_wz_yaw;      // the way the water stretches from there
vector vr_wz_eye;     // where the player's eyes hover
entity vr_wz_caster;  // case 4's shambler
float vr_wz_drawn;    // watershockdrawn as the trigger went
float vr_wz_lit;      // the frame the client first drew the effect (0: not yet)
float vr_wz_shot;     // the screenshot's frame (0: not yet)
vector vr_wz_knight;  // where case 3's knight is held

float VR_WZ_FRAME_GIVE = 2;     // all the weapons (a frame takes one impulse: these are apart)
float VR_WZ_FRAME_HAND = 8;     // the case's weapon into the main hand
float VR_WZ_FRAME_FIRE = 40;    // the aim settled: the trigger
float VR_WZ_FRAME_LIT = 12;     // frames after the client first draws the effect: the screenshot (it fades in over 0.04 s)
float VR_WZ_FRAME_GIVEUP = 120; // frames after the trigger: a safety net: the screenshot, with nothing on it
float VR_WZ_FRAME_END = 4;      // frames after the screenshot, for it to be written

float VR_WZ_REACH = 320; // how far along the water it looks for a stretch

// Whether `xAt` is a point of an open surface: liquid under it, open over it.
float(vector xAt) VR_WZ_WaterAt =
{
    return VR_LGWater_IsLiquid(pointcontents(xAt - '0 0 8')) && pointcontents(xAt + '0 0 8') == CONTENT_EMPTY;
};

// How far the water goes on from `xAt` along `xDir`.
float(vector xAt, vector xDir) VR_WZ_Stretch =
{
    float d = 32;
    while(d <= VR_WZ_REACH && VR_WZ_WaterAt(xAt + xDir * d))
    {
        d = d + 32;
    }
    return d - 32;
};

vector(float xYaw) VR_WZ_Dir =
{
    vector dir = '0 0 0';
    dir_x = xYaw == 0 ? 1 : (xYaw == 180 ? -1 : 0);
    dir_y = xYaw == 90 ? 1 : (xYaw == 270 ? -1 : 0);
    return dir;
};

// The stage: the surface's point, over the map's columns, with water all round it, the longest stretch of it in a straight
// line and room over it for the shots.
float() VR_WZ_FindWater =
{
    float best = -1;
    for(float x = -4032; x <= 4032; x = x + 64)
    {
        for(float y = -4032; y <= 4032; y = y + 64)
        {
            vector top = '0 0 4032';
            top_x = x;
            top_y = y;
            vector at = liquidentry(top, top - '0 0 8064');
            if(at == top || !VR_WZ_WaterAt(at) || !VR_WZ_WaterAt(at + '96 0 0') || !VR_WZ_WaterAt(at - '96 0 0')
                || !VR_WZ_WaterAt(at + '0 96 0') || !VR_WZ_WaterAt(at - '0 96 0'))
            {
                continue; // (not at its edge: the stage has room all round)
            }
            traceline(at, at + '0 0 170', TRUE, world);
            if(trace_fraction != 1)
            {
                continue;
            }
            for(float yaw = 0; yaw < 360; yaw = yaw + 90)
            {
                float along = VR_WZ_Stretch(at, VR_WZ_Dir(yaw));
                if(along > best)
                {
                    best = along;
                    vr_wz_surface = at;
                    vr_wz_yaw = yaw;
                }
            }
        }
    }
    return best >= 96;
};

// How far down each case looks and aims.
float(float xCase) VR_WZ_Pitch =
{
    if(xCase == 1) { return 55; }
    if(xCase == 2) { return 14; }
    if(xCase == 3) { return 0; }
    return 30;
};

void() VR_WaterZap_TestFrame =
{
    float ask = cvar_hget(cvarh_vr_waterzap_test);
    if(ask)
    {
        cvar_set("vr_waterzap_test", "0");
        if(!VR_WZ_FindWater())
        {
            bprint("waterzap: no open water on this map: the screenshot's taken\n");
            return;
        }
        vr_wz_case = ask;
        vr_wz_frame = 0;
        vr_wz_lit = 0;
        vr_wz_shot = 0;
        vr_wz_eye = vr_wz_surface + '0 0 80';
        if(ask == 3) { vr_wz_eye = vr_wz_surface + '0 0 50'; }
        if(ask == 4) { vr_wz_eye = vr_wz_surface + '0 0 6'; }
        bprint(sprintf("waterzap: case %g: the surface at %v, looking along yaw %g\n", ask, vr_wz_surface, vr_wz_yaw));
        cvar_set("vr_weapon_grip_mode", "1"); // (a weapon stays in the mock's hand, not gripping)
        // (The mock: the head's pitch, the hand's: 70 is level, less is down.)
        localcmd(sprintf("vr_mock_look %g 0\nvr_mock_hand main 0.2 1.2 -0.4 %g 0 0\n", VR_WZ_Pitch(ask), 70 - VR_WZ_Pitch(ask)));
    }
    if(!vr_wz_case)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    vr_wz_frame = vr_wz_frame + 1;

    // Hovering there, looking along the water.
    p.movetype = MOVETYPE_NOCLIP;
    p.velocity = '0 0 0';
    p.flags = p.flags | FL_GODMODE;
    if(vr_wz_frame == VR_WZ_FRAME_FIRE)
    {
        vr_wz_drawn = watershockdrawn();
    }
    if(vr_wz_frame > VR_WZ_FRAME_FIRE && !vr_wz_lit && watershockdrawn() > vr_wz_drawn)
    {
        vr_wz_lit = vr_wz_frame;
        if(vr_wz_case == 3)
        {
            // Mjolnir's blow has landed: up and back, off the knight it fills the view with, to see the water.
            vr_wz_eye = vr_wz_surface + '0 0 150' - VR_WZ_Dir(vr_wz_yaw) * 90;
            localcmd("vr_mock_look 35 0\n");
        }
    }
    if(!vr_wz_shot && (vr_wz_lit ? vr_wz_frame >= vr_wz_lit + VR_WZ_FRAME_LIT : vr_wz_frame >= VR_WZ_FRAME_FIRE + VR_WZ_FRAME_GIVEUP))
    {
        vr_wz_shot = vr_wz_frame;
        localcmd("screenshot\n");
    }
    setorigin(p, vr_wz_eye - p.view_ofs);
    vector look = '0 0 0';
    look_x = (vr_wz_case == 3 && vr_wz_lit) ? 35 : VR_WZ_Pitch(vr_wz_case);
    look_y = vr_wz_yaw;
    p.angles = look;
    p.v_angle = look;
    p.fixangle = TRUE;

    if(vr_wz_frame == VR_WZ_FRAME_GIVE)
    {
        localcmd("impulse 9\n");
    }
    else if(vr_wz_frame == VR_WZ_FRAME_HAND)
    {
        localcmd(vr_wz_case == 3 ? "impulse 153\n" : "impulse 161\n");
    }
    else if(vr_wz_frame == VR_WZ_FRAME_HAND + 6 && vr_wz_case == 3)
    {
        localcmd("impulse 241\n");
    }
    else if(vr_wz_frame == VR_WZ_FRAME_FIRE && vr_wz_case == 3)
    {
        localcmd("+attack\nvr_mock_swing 0.4\n");
    }
    else if(vr_wz_frame == VR_WZ_FRAME_FIRE && vr_wz_case != 4)
    {
        localcmd("+attack\n");
    }
    else if(vr_wz_shot && vr_wz_frame == vr_wz_shot + VR_WZ_FRAME_END)
    {
        localcmd("-attack\n");
        bprint(sprintf("waterzap: %g: the screenshot's taken\n", vr_wz_case));
        vr_wz_case = 0;
    }

    // Case 3's knight: held right before you, over the water, to be struck.
    if(vr_wz_case == 3 && vr_wz_frame > VR_WZ_FRAME_HAND + 6)
    {
        entity m = findflags(world, flags, FL_MONSTER);
        if(m)
        {
            m.movetype = MOVETYPE_NONE;
            m.health = 5000;
            m.nextthink = time + 1;
            // Ahead of the hammer's head at rest, before the swing, where the hand is aimed: the mock's way and height
            // needn't be the stage's.
            if(vr_wz_frame <= VR_WZ_FRAME_FIRE)
            {
                entity held = self;
                self = p;
                vector ahead = VRGetWeaponFireRotFwd(cVR_MainHand);
                vector head = VRGetMuzzlePos(cVR_MainHand);
                self = held;
                ahead_z = 0;
                vr_wz_knight = head + normalize(ahead) * 14 - '0 0 12';
            }
            setorigin(m, vr_wz_knight);
        }
    }

    // The shambler's bolts, as its magic frames cast them: from above, ahead, at you.
    if(vr_wz_case == 4 && vr_wz_frame >= VR_WZ_FRAME_FIRE && !vr_wz_shot)
    {
        if(vr_wz_caster == world)
        {
            vr_wz_caster = spawn();
            vr_wz_caster.classname = "monster_shambler";
            vr_wz_caster.flags = FL_MONSTER;
            setorigin(vr_wz_caster, vr_wz_surface + VR_WZ_Dir(vr_wz_yaw) * 150 + '0 0 110');
            vr_wz_caster.enemy = p;
        }
        if(vr_wz_frame - VR_WZ_FRAME_FIRE == floor((vr_wz_frame - VR_WZ_FRAME_FIRE) / 7) * 7)
        {
            entity oself = self;
            self = vr_wz_caster;
            CastLightning();
            self = oself;
        }
    }
};
#endif
