#ifdef QVR_TESTS
// vr_v1116_test.qc -- his notes of 2026-11-16 (ROUND25.md, "vr1116a"): looping sounds that played on for ever, and the
// wet shine on things never wet.
// `vr_v1116_test 1` (any map with room ahead; god, notarget): a knight with a chainsaw, hunting you (its engine idling: a
//   loop), five times: disarmed (its chainsaw dropped running), killed, gibbed, shoved down then killed, removed
//   outright. Each time the server's loops (vr_snd_loops, vr_server.cpp) are counted as it idles (at least one) and
//   vr_chainsaw_drop_run + 1.5 s after (none). Ends with the engine's `sawloops: N passed, M failed`.
// `vr_v1116_test 2` (e1m1 by the slime bridge's grunts: setpos 835 1910 -380): the monsters within 700 units killed;
//   2 s on nothing wet (vr_drips_check v16wet_before), then saved (v1116w) and loaded, 2 s on nothing wet again. Ends
//   with the engine's `v16wet: N passed, M failed`.

float vr_v16_on;      // the running test's number (saved: test 2 goes on after its load)
float vr_v16_s;       // its state
float vr_v16_case;    // test 1's transition
float vr_v16_t0;      // when the state began (game time)
entity vr_v16_g;      // the knight
nosave float vr_v16_frames; // frames in the state (a load resets it)

void(float s) VR_V16_To =
{
    vr_v16_s = s;
    vr_v16_t0 = time;
    vr_v16_frames = 0;
};

string(float k) VR_V16_Case =
{
    if(k == 0) { return "disarmed"; }
    if(k == 1) { return "killed"; }
    if(k == 2) { return "gibbed"; }
    if(k == 3) { return "shoved_then_killed"; }
    return "removed";
};

// A knight with a chainsaw 140 units ahead of `p`, hunting him.
entity(entity p) VR_V16_Knight =
{
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    traceline(p.origin, p.origin + fw * 140, TRUE, p);
    vector at = trace_endpos - fw * 24;
    entity oldself = self;
    self = spawn();
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    self.weapon = WID_CHAINSAW;
    VR_AM_Spawn(VR_AM_KNIGHT);
    entity g = self;
    self = oldself;
    vr_agt_player = p;
    VR_AGT_Hunt(g);
    return g;
};

void(entity p) VR_V16_Saw =
{
    string n = VR_V16_Case(vr_v16_case);
    if(vr_v16_s == 0)
    {
        vr_v16_g = VR_V16_Knight(p);
        VR_V16_To(1);
        return;
    }
    if(vr_v16_s == 1)
    {
        if(time < vr_v16_t0 + 1)
        {
            return; // (its engine's idle started: its pose's first frames)
        }
        localcmd(sprintf("vr_snd_loops check %s_running 9 1\n", n)); // (read next frame: before the transition)
        VR_V16_To(4);
        return;
    }
    if(vr_v16_s == 4)
    {
        if(vr_v16_frames++ < 3)
        {
            return;
        }
        entity g = vr_v16_g;
        if(vr_v16_case == 0)
        {
            VR_AG_DropHeld(g, '0 0 120');
        }
        else if(vr_v16_case == 1)
        {
            T_Damage(g, p, p, g.health + 5);
        }
        else if(vr_v16_case == 2)
        {
            T_Damage(g, p, p, g.health + 200);
        }
        else if(vr_v16_case == 3)
        {
            vector dir = g.origin - p.origin;
            dir_z = 0;
            VR_Bash_Hit(g, g.origin, normalize(dir), QVR_HITKIND_SHOVE, -1, 1, 1);
        }
        else
        {
            remove(g);
            vr_v16_g = world;
        }
        VR_V16_To(2);
        return;
    }
    if(vr_v16_s == 2)
    {
        if(vr_v16_case == 3 && vr_v16_g && vr_v16_g.health > 0 && time >= vr_v16_t0 + 0.5)
        {
            T_Damage(vr_v16_g, p, p, vr_v16_g.health + 5); // (down: killed as it lies)
        }
        if(time < vr_v16_t0 + cvar_hget(cvarh_vr_chainsaw_drop_run) + 1.5)
        {
            return; // (a chainsaw dropped running runs on, then stalls)
        }
        localcmd(sprintf("vr_snd_loops check %s_after 0\n", n));
        if(vr_v16_g && !VR_AG_Freed(vr_v16_g))
        {
            remove(vr_v16_g); // (its body out of the next one's way)
        }
        vr_v16_g = world;
        vr_v16_case++;
        VR_V16_To(vr_v16_case <= 4 ? 0 : 3);
        return;
    }
    localcmd("vr_snd_loops end sawloops\n");
    vr_v16_on = 0;
    cvar_set("vr_v1116_test", "0");
};

void(entity p) VR_V16_Wet =
{
    vr_v16_frames++;
    if(vr_v16_s == 0)
    {
        float n = 0;
        entity e = findradius(p.origin, 700);
        while(e)
        {
            if((e.flags & FL_MONSTER) && e.health > 0)
            {
                T_Damage(e, e, e, e.health + 5); // (its own blow: no push, killed where it stands)
                n++;
            }
            e = e.chain;
        }
        dprint(sprintf("v16wet: %g monsters killed\n", n));
        VR_V16_To(1);
        return;
    }
    if(vr_v16_s == 1)
    {
        if(vr_v16_frames < 144)
        {
            return; // (2 s: the drips' scans, ten a second, have looked at the bodies)
        }
        vr_v16_s = 2; // (saved so: the load goes on in state 2)
        vr_v16_frames = 0;
        localcmd("vr_drips_check v16wet_before 0 grunt\nsave v1116w\nload v1116w\n"); // (grunts: a dropped pack may roll into the slime)
        return;
    }
    if(vr_v16_frames < 144)
    {
        return; // (after the load: its first scans)
    }
    localcmd("vr_drips_check v16wet 0 grunt\n");
    vr_v16_on = 0;
    cvar_set("vr_v1116_test", "0");
};

void() VR_V1116Test_Frame =
{
    float ask = cvar_hget(cvarh_vr_v1116_test);
    if(ask && !vr_v16_on)
    {
        vr_v16_on = ask;
        vr_v16_case = 0;
        VR_V16_To(0);
    }
    entity p = VR_Player();
    if(!vr_v16_on || !p)
    {
        return;
    }
    if(time > vr_v16_t0 + 12)
    {
        localcmd(vr_v16_on == 1 ? "echo \"sawloops: FAIL timed out\"\nvr_snd_loops end sawloops\n" : "echo \"v16wet: 0 passed, 1 failed (timed out)\"\n");
        vr_v16_on = 0;
        cvar_set("vr_v1116_test", "0");
        return;
    }
    if(vr_v16_on == 1)
    {
        VR_V16_Saw(p);
    }
    else if(vr_v16_on == 2)
    {
        VR_V16_Wet(p);
    }
};
#endif
