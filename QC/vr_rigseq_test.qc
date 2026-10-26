#ifdef QVR_TESTS
// vr_rigseq_test.qc -- the Rig Studio's sequences played by QC (vr_rigseq.qc; docs/vr-port/RIGSTUDIO.md, "Tests"):
// `vr_agrunt_test 80` (an open map: vrtesthall), with the sequence "rstest" of the armed grunt's model where the files
// are read (vr_rigdata_dir; the studio's test makes it: 10 frames at 10 fps, events "fire" at frame 3 and "footstep" at
// 7). A frozen armed grunt 160 units ahead plays it: its length comes back (1 s), each event reaches QC at its frame's
// time (within a server frame: 1/72 s), then "end" at 1 s and its frame field back at -1. Its last line: `agtest: test 80 over:
// <p> passed, <f> failed`.

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
    // (taken at the first server frame end at or after its frame's time: within one frame of it)
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
#endif
