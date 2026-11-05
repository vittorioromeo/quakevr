#ifdef QVR_TESTS
// vr_shockarcs_test.qc -- struck by lightning, your own arms' arcs as a player sees them (ROUND24.md, "The arcs on your arms on
// a flat screen"). This only sets the stage; the verdict is Misc/quakevr/sa_shockarcs_test.sh's, from pictures: each case runs
// twice on the same frames, the arcs on (vr_shock_self_time as shipped) and off (0), and the changed pixels of the two
// screenshots are the arcs (sa_shockarcs_diff.py).
//
// `vr_shockarcs_test <case>`, on any map, flat-screen (-flatscreenmode) or in the mock headset: the real trigger, a shambler's
// bolts (CastLightning: VR_LightningHit, playershock, QVR_SVC_SHOCK) from behind you, so the beam is out of the view:
//   1 looking level
//   2 looking down
//   3 crouched a little (+flatcrouch, vr_flat_crouch_drop 8)
//   4 the look still swinging down as the screenshot's taken: the arcs on the arms of this frame, not the last one's
// The screenshot is taken VR_SA_FRAME_SHOT frames after the first bolt, `shockarcs: <case>: the screenshot's taken` ends the
// run (run.sh -QuitOn).

float vr_sa_case;     // the running case (0: none)
float vr_sa_frame;    // its frames so far
float vr_sa_yaw;      // the way it looks (open ahead and behind)
float vr_sa_pitch;    // how far down it looks
float vr_sa_swing;    // how far further down, each frame from the first bolt
entity vr_sa_caster;  // the shambler behind you

float VR_SA_FRAME_GIVE = 2;   // all the weapons (a frame takes one impulse: these are apart)
float VR_SA_FRAME_HAND = 8;   // the thunderbolt into the main hand
float VR_SA_FRAME_FIRE = 40;  // the look and a crouch settled: the first bolt
float VR_SA_BOLT_EVERY = 7;   // frames between its bolts
float VR_SA_BOLTS = 3;        // a shambler's attack
float VR_SA_FRAME_SHOT = 34;  // frames after the first bolt: the last's beam gone (0.2 s), its arcs still going (0.8 s)
float VR_SA_SHOTS = 5;        // screenshots (the arcs are reshaped every frame, some frames have few)
float VR_SA_SHOT_EVERY = 3;   // frames between them
float VR_SA_FRAME_END = 4;    // frames after the last, for it to be written
float VR_SA_BEHIND = 140;     // units behind you the shambler stands
float VR_SA_AHEAD = 96;       // units open ahead wanted, so that the view is not a wall against the hands

// The way with VR_SA_BEHIND open behind the eyes and the most open ahead.
float(vector xEye) VR_SA_FindYaw =
{
    float best = -1;
    float bestYaw = 0;
    for(float yaw = 0; yaw < 360; yaw = yaw + 90)
    {
        vector dir = VR_WZ_Dir(yaw);
        traceline(xEye, xEye - dir * VR_SA_BEHIND, TRUE, world);
        if(trace_fraction != 1)
        {
            continue;
        }
        traceline(xEye, xEye + dir * (VR_SA_AHEAD * 4), TRUE, world);
        if(trace_fraction > best)
        {
            best = trace_fraction;
            bestYaw = yaw;
        }
    }
    return bestYaw;
};

void() VR_ShockArcs_TestFrame =
{
    float ask = cvar_hget(cvarh_vr_shockarcs_test);
    if(ask)
    {
        cvar_set("vr_shockarcs_test", "0");
        vr_sa_pitch = 0;
        vr_sa_swing = 0;
        if(ask == 1)
        {
            vr_sa_pitch = 0;
        }
        if(ask == 2)
        {
            vr_sa_pitch = 35;
        }
        if(ask == 3)
        {
            // (Shallow: at the full crouch, 18, the hands and the gun are posed behind the eyes: ROUND24.md.)
            localcmd("tune vr_flat_crouch_drop 8\n+flatcrouch\n");
        }
        if(ask == 4)
        {
            vr_sa_swing = 1;
        }
        vr_sa_case = ask;
        vr_sa_frame = 0;
        vr_sa_caster = world;
        entity who = VR_Player();
        vr_sa_yaw = who == world ? 0 : VR_SA_FindYaw(who.origin + who.view_ofs);
        bprint(sprintf("shockarcs: case %g: looking along yaw %g, %g down\n", ask, vr_sa_yaw, vr_sa_pitch));
    }
    if(!vr_sa_case)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    vr_sa_frame = vr_sa_frame + 1;
    float since = vr_sa_frame - VR_SA_FRAME_FIRE; // (frames from the first bolt)

    p.flags = p.flags | FL_GODMODE;
    vector look = '0 0 0';
    look_x = vr_sa_pitch + vr_sa_swing * (since > 0 ? since : 0);
    look_y = vr_sa_yaw;
    p.angles = look;
    p.v_angle = look;
    p.fixangle = TRUE;

    if(vr_sa_frame == VR_SA_FRAME_GIVE)
    {
        localcmd("impulse 9\n");
    }
    else if(vr_sa_frame == VR_SA_FRAME_HAND)
    {
        localcmd("impulse 161\n");
    }

    // Its bolts, as its magic frames cast them.
    if(since >= 0 && since < VR_SA_BOLTS * VR_SA_BOLT_EVERY && since == floor(since / VR_SA_BOLT_EVERY) * VR_SA_BOLT_EVERY)
    {
        if(vr_sa_caster == world)
        {
            vr_sa_caster = spawn();
            vr_sa_caster.classname = "monster_shambler";
            vr_sa_caster.flags = FL_MONSTER;
            vr_sa_caster.takedamage = DAMAGE_NO;
            setorigin(vr_sa_caster, p.origin - VR_WZ_Dir(vr_sa_yaw) * VR_SA_BEHIND);
            vr_sa_caster.enemy = p;
        }
        // (Facing you, so that the aim cone leaves the bolt on you.)
        vr_sa_caster.angles_y = vectoyaw(p.origin - vr_sa_caster.origin);
        vr_sa_caster.ideal_yaw = vr_sa_caster.angles_y;
        vr_sa_caster.yaw_speed = 360;
        entity oself = self;
        self = vr_sa_caster;
        CastLightning();
        self = oself;
    }

    float lastShot = VR_SA_FRAME_SHOT + (VR_SA_SHOTS - 1) * VR_SA_SHOT_EVERY;
    if(since >= VR_SA_FRAME_SHOT && since <= lastShot && (since - VR_SA_FRAME_SHOT) == floor((since - VR_SA_FRAME_SHOT) / VR_SA_SHOT_EVERY) * VR_SA_SHOT_EVERY)
    {
        localcmd("screenshot\nvr_shock_self_info\n");
    }
    else if(since == lastShot + VR_SA_FRAME_END)
    {
        localcmd("-flatcrouch\n");
        bprint(sprintf("shockarcs: %g: crouched %g, pitch %g: the screenshot's taken\n", vr_sa_case, VR_Stealth_CrouchShare(p), look_x));
        vr_sa_case = 0;
        if(vr_sa_caster != world)
        {
            remove(vr_sa_caster);
            vr_sa_caster = world;
        }
    }
};
#endif
