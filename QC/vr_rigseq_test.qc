#ifdef QVR_TESTS
// vr_rigseq_test.qc -- the Rig Studio's sequences played by QC (vr_rigseq.qc; docs/vr-port/RIGSTUDIO.md, "Tests"):
// `vr_agrunt_test 80` (an open map: vrtesthall), with the sequence "rstest" of the armed grunt's model where the files
// are read (vr_rigdata_dir; the studio's test makes it: 10 frames at 10 fps, events "fire" at frame 3 and "footstep" at
// 7). A frozen armed grunt 160 units ahead plays it: its length comes back (1 s), each event reaches QC at its frame's
// time (within a server frame: 1/72 s), then "end" at 1 s and its frame field back at -1. Its last line:
// `agtest: test 80 over: <p> passed, <f> failed`.

float vr_rst_t0;      // when it was asked for
float vr_rst_end;     // its "end" (s after the start; 0 not yet)
float vr_rst_events;  // events seen (not end / stopped)
float vr_rst_timely;  // ... at their frames' times
float vr_rst_stopped; // "stopped" seen
float vr_rst_len;     // vr_rigseq_len as first seen
float vr_rst_frames;  // vr_rigseq_frame values seen climbing (0..9)
float vr_rst_lastframe;
float vr_rst_freeze;  // vr_agrunt_freeze before the test

void(string seq, string ev, float frame) VR_RST_Event =
{
    float dt = time - vr_rst_t0;
    VR_AGT_Print(sprintf("rigseq: %s: event %s at frame %g, %.3f s after the start", seq, ev, frame, dt));
    if(ev == "end")
    {
        vr_rst_end = dt;
        return;
    }
    if(ev == "stopped")
    {
        vr_rst_stopped = 1;
        return;
    }
    vr_rst_events = vr_rst_events + 1;
    // (taken at the first server frame end at or after its frame's time: within one frame of it, 1/72 s)
    if(dt >= frame / 10 - 0.001 && dt <= frame / 10 + 0.021)
    {
        vr_rst_timely = vr_rst_timely + 1;
    }
};

void() VR_RST_Frame =
{
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        vr_rst_freeze = cvar("vr_agrunt_freeze");
        cvar_set("vr_agrunt_freeze", "1");
        vr_rst_end = vr_rst_events = vr_rst_timely = vr_rst_stopped = vr_rst_len = vr_rst_frames = 0;
        vr_rst_lastframe = -1;
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 160);
        vr_agt_until = time + 0.5;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(time < vr_agt_until)
        {
            return;
        }
        g.vr_rigseq_on = VR_RST_Event;
        VR_RigSeq_Play(g, "rstest");
        vr_rst_t0 = time;
        vr_agt_until = time + 3;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2)
    {
        if(!vr_rst_len && g.vr_rigseq_len != 0)
        {
            vr_rst_len = g.vr_rigseq_len;
        }
        if(g.vr_rigseq_frame > vr_rst_lastframe)
        {
            vr_rst_frames = vr_rst_frames + 1;
            vr_rst_lastframe = g.vr_rigseq_frame;
        }
        if(vr_rst_end > 0 || vr_rst_stopped || vr_rst_len < 0 || time > vr_agt_until)
        {
            vr_agt_step = 3;
        }
        return;
    }
    VR_AGT_Check(sprintf("rigseq: its length %g s (1)", vr_rst_len), fabs(vr_rst_len - 1) < 0.001);
    VR_AGT_Check(sprintf("rigseq: %g events, %g at their frames' times (2, 2)", vr_rst_events, vr_rst_timely),
        vr_rst_events == 2 && vr_rst_timely == 2);
    VR_AGT_Check(sprintf("rigseq: its end at %.3f s (1)", vr_rst_end), vr_rst_end >= 0.999 && vr_rst_end <= 1.021 && !vr_rst_stopped);
    VR_AGT_Check(sprintf("rigseq: its frame climbed through %g values, -1 after its end (%g)", vr_rst_frames, g.vr_rigseq_frame),
        vr_rst_frames >= 8 && g.vr_rigseq_frame == -1);
    VR_AGT_Print(sprintf("rigseq: %g passed, %g failed", vr_agt_passed, vr_agt_failed));
    VR_AGT_Remove(g);
    vr_agt_grunt = world;
    cvar_set("vr_agrunt_freeze", ftos(vr_rst_freeze));
    cvar_set("vr_agrunt_test", "0");
};

// `vr_agrunt_test 81`: the taunt (vr_agrunt_taunt on for the test): an armed grunt 200 units ahead sees you, stands and
// plays its taunt (its length 1.6 s; its think the taunt's wait meanwhile, facing you), then comes for you (its run).
float vr_rst_taunt;   // vr_agrunt_taunt before the test
float vr_rst_waiting; // frames it waited in its taunt
float vr_rst_shout;   // its "shout" event (s after it saw you; 0 none)
float vr_rst_tend;    // its "end" (s after it saw you; 0 none)
void(string seq, string ev, float frame) VR_AG_SeqEvent;
// (its events seen, then handed on to its own handler: the shout's sound, its run after the end)
void(string seq, string ev, float frame) VR_RST_TauntEvent =
{
    float dt = time - self.vr_rigseq_t0;
    VR_AGT_Print(sprintf("taunt: event %s at frame %g, %.3f s after its start", ev, frame, dt));
    if(ev == "shout" && !vr_rst_shout)
    {
        vr_rst_shout = dt;
    }
    if(ev == "end")
    {
        vr_rst_tend = dt;
    }
    VR_AG_SeqEvent(seq, ev, frame);
};
void() VR_RST_TauntFrame =
{
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        vr_rst_taunt = cvar("vr_agrunt_taunt");
        cvar_set("vr_agrunt_taunt", "1");
        vr_rst_waiting = vr_rst_len = vr_rst_shout = vr_rst_tend = 0;
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 200);
        vr_agt_until = time + 0.3;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(time < vr_agt_until)
        {
            return;
        }
        VR_AGT_Hunt(g); // (it sees you: its th_run, the taunt first)
        vr_rst_t0 = time;
        vr_agt_until = time + 5;
        vr_agt_step = 2;
        VR_AGT_Print("taunt: it sees you");
        return;
    }
    if(vr_agt_step == 2)
    {
        if(g.vr_rigseq_on == VR_AG_SeqEvent)
        {
            g.vr_rigseq_on = VR_RST_TauntEvent; // (its events seen: its taunt started by its th_run)
        }
        if(!vr_rst_len && g.vr_rigseq_len != 0)
        {
            vr_rst_len = g.vr_rigseq_len;
            VR_AGT_Print(sprintf("taunt: its sequence %g s long", vr_rst_len));
        }
        if(g.think == vr_ag_taunt_wait)
        {
            vr_rst_waiting = vr_rst_waiting + 1;
        }
        else if(vr_rst_waiting > 0 || time > vr_agt_until)
        {
            vr_rst_end = time - vr_rst_t0;
            vr_agt_step = 3;
        }
        return;
    }
    VR_AGT_Check(sprintf("taunt: its taunt %g s long (1.6)", vr_rst_len), fabs(vr_rst_len - 1.6) < 0.001);
    VR_AGT_Check(sprintf("taunt: it stood %g frames in it, then went on after %.2f s", vr_rst_waiting, vr_rst_end),
        vr_rst_waiting > 50 && vr_rst_end >= 1.5 && vr_rst_end <= 2.1);
    VR_AGT_Check(sprintf("taunt: taunted once (%g), hunting you now (%s)", g.vr_ag_taunted, g.enemy.classname),
        g.vr_ag_taunted == 1 && g.enemy == vr_agt_player);
    VR_AGT_Check(sprintf("taunt: its shout at %.3f s (frame 6: 0.6), its end at %.3f s (1.6)", vr_rst_shout, vr_rst_tend),
        vr_rst_shout >= 0.599 && vr_rst_shout <= 0.621 && vr_rst_tend >= 1.599 && vr_rst_tend <= 1.621);
    VR_AGT_Print(sprintf("taunt: %g passed, %g failed", vr_agt_passed, vr_agt_failed));
    VR_AGT_Remove(g);
    vr_agt_grunt = world;
    cvar_set("vr_agrunt_taunt", ftos(vr_rst_taunt));
    cvar_set("vr_agrunt_test", "0");
};

// `vr_agrunt_test 82` (vr_rig2 1): the Rig Studio's override keys on a live grunt drawn on the authored rig (RIGSTUDIO.md,
// "Animating the authored rig"; its keys those vr_rigstudio_test_rig2anim made: `load`'s chest and right upper arm at
// frames 3 and 7). An armed grunt 160 units ahead, its AI held (its think holds its frame), at load's frame 3 then 7;
// at each the engine's `vr_rig2anim_check` keeps its bones as drawn with the keys off (vr_rigseq 0), then measures each
// keyed bone drawn with them on against its key. Its frames: vrgrunt.mdl's load1..load11 are 29..39 (the check
// says if it is not at that frame). Ends on the engine's `rig2anim check: N passed, M failed`.
float vr_rst_hold;      // the frame its think holds
float vr_rst_keysframe; // load's frame measured now (3, 7)
void() VR_RST_HoldThink =
{
    self.frame = vr_rst_hold;
    self.velocity = '0 0 0';
    self.nextthink = time + 0.05;
};
void() VR_RST_KeysFrame =
{
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 160);
        g = vr_agt_grunt;
        g.enemy = world;
        g.think = VR_RST_HoldThink;
        g.nextthink = time + 0.05;
        vr_rst_keysframe = 3;
        vr_rst_hold = 29 + vr_rst_keysframe;
        g.frame = vr_rst_hold;
        localcmd("vr_rigseq 0\n");
        vr_agt_until = time + 0.6; // (its frame's lerp over, the upper body's hold settled)
        vr_agt_step = 1;
        return;
    }
    if(time < vr_agt_until)
    {
        return;
    }
    if(vr_agt_step == 1)
    {
        localcmd(vr_rst_keysframe == 3 ? "screenshot\nvr_rig2anim_check keep\nvr_rigseq 1\n" : "vr_rig2anim_check keep\nvr_rigseq 1\n");
        vr_agt_until = time + 0.3;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2)
    {
        localcmd(sprintf("vr_rig2anim_check load %g\n", vr_rst_keysframe));
        if(vr_rst_keysframe == 3)
        {
            localcmd("screenshot\n"); // (the eyeshots: its frame 3 unkeyed, then keyed)
            vr_rst_keysframe = 7;
            vr_rst_hold = 29 + vr_rst_keysframe;
            localcmd("vr_rigseq 0\n");
            vr_agt_until = time + 0.6;
            vr_agt_step = 1;
            return;
        }
        vr_agt_until = time + 0.1;
        vr_agt_step = 3;
        return;
    }
    VR_AGT_Print("rig2anim: test 82 over");
    localcmd("vr_rig2anim_check end\n");
    VR_AGT_Remove(g);
    vr_agt_grunt = world;
    cvar_set("vr_agrunt_test", "0");
};
#endif
