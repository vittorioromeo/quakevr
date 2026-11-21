#ifdef QVR_TESTS
// vr_stealth_test6.qc -- `vr_stealth_test 131` on e1m1: riders on a moving lift don't sink into it (his note of
// 2026-11-21, riders "compressed by the lift"; vr_debug_lift measures them: the boxes on the server, the drawn feet,
// the drawn eye, the drawn monsters). Two plats (*7 at -544 2656, *22 at 792 512), each: the player and a grunt put on
// it at its bottom ride it up; at the top he steps off, it comes down with the grunt and he is put back on it under way:
// both ride it down. Each ride's worst sink of each rider (vr_debug_lift_player / _monster) is held to 0.5 units.
// Prints `lifttest: <lift>_<way>_<rider> PASS|FAIL <sink>`, then `lifttest: N passed, M failed`. Run it flat and in the
// headset: Misc/quakevr/lift_ride_test.sh.

float stt6_lift;   // 0 the first plat, 1 the second
float stt6_stage;
float stt6_t0;
float stt6_wait;   // frames counted since a ride ended (the client draws the ride's end, then its worst is read)
entity stt6_plat;
float stt6_speed;  // the plat's speed (units a second)
entity stt6_m;

// Each plat's middle, the player's and the grunt's places on it (x off its middle) and the floor beside it.
vector(float i) VR_STT6_Mid = { return i == 0 ? '-544 2656 0' : '792 512 0'; };
float(float i) VR_STT6_PlayerX = { return i == 0 ? -28 : -20; };
float(float i) VR_STT6_GruntX = { return i == 0 ? 24 : 20; };
vector(float i) VR_STT6_Beside = { return i == 0 ? '-460 2656 -104' : '860 512 -312'; };
string(float i) VR_STT6_Name = { return i == 0 ? "plat7" : "plat22"; };

void(float xStage) VR_STT6_Stage =
{
    stt6_stage = xStage;
    stt6_t0 = time;
    stt6_wait = 0;
};

// The plat nearest the lift's middle (world: none within 96 units across).
entity(float i) VR_STT6_FindPlat =
{
    vector mid = VR_STT6_Mid(i);
    for(entity e = find(world, classname, "plat"); e; e = find(e, classname, "plat"))
    {
        vector c = (e.absmin + e.absmax) * 0.5;
        if(fabs(c_x - mid_x) < 96 && fabs(c_y - mid_y) < 96)
        {
            return e;
        }
    }
    return world;
};

// A rider's place on the plat: its top under the box's feet, 1 unit up.
vector(float xOff, float xFeet) VR_STT6_On =
{
    vector mid = VR_STT6_Mid(stt6_lift);
    return [mid_x + xOff, mid_y, stt6_plat.absmax_z - 1 + xFeet + 1];
};

void(entity p, vector at) VR_STT6_Put =
{
    p.velocity = '0 0 0';
    p.flags = p.flags - (p.flags & FL_ONGROUND);
    setorigin(p, at);
};

void(string xWay) VR_STT6_Read =
{
    float ps = cvar_hget(cvarh_vr_debug_lift_player);
    float ms = cvar_hget(cvarh_vr_debug_lift_monster);
    float es = cvar_hget(cvarh_vr_debug_lift_eye);
    // (the headset's eye is placed from his place a frame old, the drawn lift from this frame's: a frame of its travel)
    float eyemax = 0.5 + (cvar_hget(cvarh_vr_enabled) ? stt6_speed * frametime : 0);
    string lift = VR_STT6_Name(stt6_lift);
    VR_Test_Check(ps > -900 && ps <= 0.5, sprintf("%s_%s_player %s", lift, xWay, ps > -900 ? sprintf("%.2f", ps) : "not measured"));
    VR_Test_Check(es > -900 && es <= eyemax, sprintf("%s_%s_eye %s (at most %.2f)", lift, xWay, es > -900 ? sprintf("%.2f", es) : "not measured", eyemax));
    VR_Test_Check(ms > -900 && ms <= 0.5, sprintf("%s_%s_monster %s", lift, xWay, ms > -900 ? sprintf("%.2f", ms) : "not measured"));
    cvar_set("vr_debug_lift_player", "-999");
    cvar_set("vr_debug_lift_monster", "-999");
    cvar_set("vr_debug_lift_eye", "-999");
};

void(string xWhy) VR_STT6_Fail =
{
    VR_Test_Check(FALSE, sprintf("%s %s", VR_STT6_Name(stt6_lift), xWhy));
};

// The next lift, or the end.
void() VR_STT6_NextLift =
{
    VR_STT2_Remove(stt6_m);
    stt6_m = world;
    VR_STT6_Put(stt2_p, VR_STT6_Beside(stt6_lift));
    stt6_lift = stt6_lift + 1;
    if(stt6_lift > 1)
    {
        cvar_set("vr_debug_lift", "0");
        VR_Test_Verdict();
        VR_STT2_End();
        return;
    }
    VR_STT6_Stage(0);
};

void() VR_STT6_Frame =
{
    entity p = stt2_p;
    float t = time - stt6_t0;
    // 0: the plat found, resting at its bottom; the player beside it.
    if(stt6_stage == 0)
    {
        stt6_plat = VR_STT6_FindPlat(stt6_lift);
        if(stt6_plat == world)
        {
            VR_STT6_Fail("no plat");
            VR_STT6_NextLift();
            return;
        }
        VR_STT6_Put(p, VR_STT6_Beside(stt6_lift));
        stt6_speed = stt6_plat.speed;
        cvar_set("vr_debug_lift_player", "-999");
        cvar_set("vr_debug_lift_eye", "-999");
        cvar_set("vr_debug_lift_monster", "-999");
        VR_STT6_Stage(1);
        return;
    }
    // 1: at its bottom, he standing beside it 10 frames (his eye's height at rest measured: vr_debug_lift); then the
    // player and a grunt on it in the same frame (each calls it up).
    if(stt6_stage == 1)
    {
        if((p.flags & FL_ONGROUND) && p.velocity_z == 0)
        {
            stt6_wait = stt6_wait + 1;
        }
        else
        {
            stt6_wait = 0;
        }
        if(stt6_plat.state != STATE_BOTTOM || stt6_plat.velocity != '0 0 0' || stt6_wait < 10)
        {
            if(t > 15) { VR_STT6_Fail("timed out: never at its bottom"); VR_STT6_NextLift(); }
            return;
        }
        stt6_m = VR_STT2_Spawn(VR_STT6_On(VR_STT6_GruntX(stt6_lift), 24), 180, 0);
        if(stt6_m == world)
        {
            VR_STT6_Fail("no grunt");
            VR_STT6_NextLift();
            return;
        }
        VR_STT2_Calm(stt6_m, 180);
        stt6_m.flags = stt6_m.flags - (stt6_m.flags & FL_ONGROUND);
        VR_STT6_Put(p, VR_STT6_On(VR_STT6_PlayerX(stt6_lift), -p.mins_z));
        VR_STT6_Stage(2);
        return;
    }
    // 2: riding up to its top; there, the ride's end drawn (3 frames), its worst read; he steps off.
    if(stt6_stage == 2)
    {
        if(stt6_plat.state != STATE_TOP)
        {
            if(t > 15) { VR_STT6_Fail("timed out: never at its top"); VR_STT6_NextLift(); }
            return;
        }
        stt6_wait = stt6_wait + 1;
        if(stt6_wait < 3)
        {
            return;
        }
        VR_STT6_Read("up");
        VR_STT6_Put(p, VR_STT6_Beside(stt6_lift));
        VR_STT6_Stage(3);
        return;
    }
    // 3: it comes down by itself with the grunt on it; 24 units down, he is put back on it.
    if(stt6_stage == 3)
    {
        if(stt6_plat.state != STATE_DOWN || stt6_plat.origin_z > stt6_plat.pos1_z - 24)
        {
            if(t > 15) { VR_STT6_Fail("timed out: never came down"); VR_STT6_NextLift(); }
            return;
        }
        VR_STT6_Put(p, VR_STT6_On(VR_STT6_PlayerX(stt6_lift), -p.mins_z));
        VR_STT6_Stage(4);
        return;
    }
    // 4: riding down to its bottom; the ride's end drawn, its worst read.
    if(stt6_stage == 4)
    {
        if(stt6_plat.state == STATE_DOWN)
        {
            if(t > 15) { VR_STT6_Fail("timed out: never at its bottom"); VR_STT6_NextLift(); }
            return;
        }
        stt6_wait = stt6_wait + 1;
        if(stt6_wait < 3)
        {
            return;
        }
        VR_STT6_Read("down");
        VR_STT6_NextLift();
        return;
    }
};

void() VR_STT6_Start =
{
    stt6_lift = 0;
    VR_Test_BeginFlags("lifttest", VR_TEST_LOUD);
    stt6_plat = world;
    stt6_m = world;
    if(!cvar_hget(cvarh_vr_debug_lift))
    {
        cvar_set("vr_debug_lift", "1"); // (2 kept: every frame logged)
    }
    VR_STT6_Stage(0);
};
#endif
