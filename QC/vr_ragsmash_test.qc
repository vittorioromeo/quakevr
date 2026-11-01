#ifdef QVR_TESTS
// vr_ragsmash_test.qc -- the smash sound and blood's test (vr_ragimpact.qc VR_RagSmash, vr_ragsmash; ROUND23.md, "Smash
// sound and blood"). `vr_ragsmash_test 1` (vrfiringrange; god, notarget): a grunt spawned ahead, moved 48 units off
// the nearest wall and killed whole (a corpse ragdoll), let settle; flung at the wall at 9 m/s as if you threw it: it
// strikes, sounds and bleeds, the first smash at the wall (within 24 units of its plane); settled against it, flung
// softly (1 m/s, 2.4 up): nothing sounds or bleeds; then the engine's own call (VR_RagImpact_Hit) on the wall there: a
// slow touch (2 m/s, under the least speed 2.5) nothing, a held slam at 5 m/s a sound and blood, again at once nothing
// (the body's cooldown). Ends on
// `ragsmash: <p> passed, <f> failed`. vr_knockdown_debug 1 while it runs: the client's own line for the blood's mark
// ("ragsmash: blood <size> units at <x y z> ...: placed", vr_gore.cpp smash).

float vr_rst_state;     // 0 idle, 1 spawned, 2 the corpse settling, 3 flung hard, 4 flung softly
float vr_rst_frames;    // frames in this state
float vr_rst_still;     // frames its middle has kept still
vector vr_rst_last;     // its middle last frame
entity vr_rst_g;
vector vr_rst_way;      // towards the wall (level)
vector vr_rst_wall;     // a point on the wall,
vector vr_rst_wn;       // and its normal
float vr_rst_s0, vr_rst_b0, vr_rst_k0; // the smashes sounded, bled and the strikes before the fling
float vr_rst_first;     // the fling's first smash's distance from the wall's plane (-1: none yet)
float vr_rst_old_debug;

float VR_RST_GIVEUP = 720; // frames (10 s): a state not done by then fails "timed out"

// Whether the corpse has kept still (its middle under 0.3 units a frame) for 36 frames.
float() VR_RST_Settled =
{
    vector at = (vr_rst_g.absmin + vr_rst_g.absmax) * 0.5;
    float moved = vlen(at - vr_rst_last);
    vr_rst_last = at;
    vr_rst_still = moved < 0.6 ? vr_rst_still + 1 : 0; // (a corpse's twitches: its box a few tenths a frame)
    return isragdoll(vr_rst_g) && vr_rst_still >= 36;
};

// The nearest wall level with the corpse's middle (8 ways, within 256 units): vr_rst_way, vr_rst_wall, vr_rst_wn.
// Returns its distance (1000: none).
float() VR_RST_FindWall =
{
    vector from = (vr_rst_g.absmin + vr_rst_g.absmax) * 0.5;
    float near = 2;
    for(float k = 0; k < 8; k++)
    {
        makevectors('0 1 0' * k * 45);
        traceline(from, from + v_forward * 256, TRUE, vr_rst_g);
        if(trace_fraction < near && fabs(trace_plane_normal_z) < 0.3)
        {
            near = trace_fraction;
            vr_rst_way = v_forward;
            vr_rst_wall = trace_endpos;
            vr_rst_wn = trace_plane_normal;
        }
    }
    return near > 1 ? 1000 : near * 256;
};

// Flung at the wall at xSpeed m/s and xUp m/s up, as if you threw it; the counts kept.
void(float xSpeed, float xUp) VR_RST_Fling =
{
    float u = VRMetersToUnits();
    vr_rst_g.velocity = vr_rst_way * xSpeed * u + '0 0 1' * xUp * u;
    vr_rst_g.vr_rag_letgo = VR_Player();
    vr_rst_g.vr_rag_letgo_hand = 0;
    vr_rst_g.vr_rag_letgo_time = time;
    vr_rst_s0 = vr_rsmash_sounds;
    vr_rst_b0 = vr_rsmash_bloods;
    vr_rst_k0 = vr_rsmash_strikes;
    vr_rsmash_fastest = 0;
    vr_rst_first = -1;
    vr_rst_still = 0;
    vr_rst_frames = 0;
    VR_Test_Print(sprintf("ragsmash: flung at the wall at %g m/s (and %g up) from %v\n", xSpeed, xUp,
        (vr_rst_g.absmin + vr_rst_g.absmax) * 0.5));
};

void() VR_RST_Finish =
{
    cvar_set("vr_knockdown_debug", ftos(vr_rst_old_debug));
    vr_rimp_test = 0;
    vr_rst_state = 0;
    VR_Test_Verdict();
};

void(entity p) VR_RST_Begin =
{
    VR_Test_Begin("ragsmash");
    vr_rst_old_debug = cvar("vr_knockdown_debug");
    cvar_set("vr_knockdown_debug", ftos(max(1, vr_rst_old_debug))); // (2: the engine's every strike too)
    vr_rimp_test = 1;
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    vr_rst_g = VR_Test_Spawn(p, 0, p.origin + fw * 128, vectoyaw(fw * -1));
    if(vr_rst_g == world)
    {
        VR_Test_Check(FALSE, "a grunt spawned");
        VR_RST_Finish();
        return;
    }
    vr_rst_state = 1;
    vr_rst_frames = 0;
    vr_rst_still = 0;
};

void() VR_RagSmash_TestFrame =
{
    entity p = VR_Player();
    if(vr_rst_state == 0)
    {
        float request = cvar("vr_ragsmash_test");
        if(!request || !p)
        {
            return;
        }
        cvar_set("vr_ragsmash_test", "0");
        if(request == 1)
        {
            VR_RST_Begin(p);
        }
        return;
    }
    vr_rst_frames = vr_rst_frames + 1;
    if(vr_rst_frames > VR_RST_GIVEUP || !vr_rst_g || vr_rst_g.vr_gib)
    {
        VR_Test_Check(FALSE, sprintf("timed out (or the corpse gone) in state %g (%s ragdoll %g health %g, still %g frames)",
            vr_rst_state, vr_rst_g.classname, isragdoll(vr_rst_g), vr_rst_g.health, vr_rst_still));
        VR_RST_Finish();
        return;
    }
    if(vr_rst_state == 1)
    {
        // Its spawn done (its health set as it starts: a few frames): 48 units off the nearest wall (as
        // vr_knockdown_test 58 moves one), then killed whole.
        if(vr_rst_frames < 10)
        {
            return;
        }
        float w = VR_RST_FindWall();
        if(w < 1000)
        {
            setorigin(vr_rst_g, vr_rst_g.origin + vr_rst_way * (w - 48 - vr_rst_g.maxs_x));
        }
        T_Damage(vr_rst_g, world, p, vr_rst_g.health + 1);
        vr_rst_state = 2;
        vr_rst_frames = 0;
        vr_rst_still = 0;
        return;
    }
    if(vr_rst_state >= 3 && vr_rst_first < 0 && vr_rsmash_sounds > vr_rst_s0)
    {
        vr_rst_first = fabs((vr_rsmash_at - vr_rst_wall) * vr_rst_wn);
    }
    if(!VR_RST_Settled() || vr_rst_frames < 36)
    {
        return;
    }
    if(vr_rst_state == 2)
    {
        VR_Test_Check(isragdoll(vr_rst_g) && vr_rst_g.health <= 0, "the grunt is a corpse ragdoll");
        float d = VR_RST_FindWall();
        VR_Test_Check(d < 160, sprintf("a wall near it (%.0f units)", d));
        if(d >= 160)
        {
            VR_RST_Finish();
            return;
        }
        VR_RST_Fling(9, 0.5);
        vr_rst_state = 3;
        return;
    }
    if(vr_rst_state == 3)
    {
        VR_Test_Check(vr_rsmash_strikes > vr_rst_k0, sprintf("the hard fling struck (%g strikes, fastest %.1f m/s)",
            vr_rsmash_strikes - vr_rst_k0, vr_rsmash_fastest));
        VR_Test_Check(vr_rsmash_sounds > vr_rst_s0, sprintf("it sounded (%g)", vr_rsmash_sounds - vr_rst_s0));
        VR_Test_Check(vr_rsmash_bloods > vr_rst_b0, sprintf("it bled (%g)", vr_rsmash_bloods - vr_rst_b0));
        VR_Test_Check(vr_rst_first >= 0 && vr_rst_first <= 24, sprintf("the first smash at the wall (%.1f units off its plane)",
            vr_rst_first));
        VR_Test_Print(sprintf("ragsmash: the wall %.0f units off its middle\n", VR_RST_FindWall()));
        VR_RST_Fling(1, 2.4); // (up off the floor, a new touch: it lands back on it as softly)
        vr_rst_state = 4;
        return;
    }
    if(vr_rst_state == 4)
    {
        // (Against the wall a soft fling's touches are mostly under the engine's reporting speed, 1 m/s: its strikes
        // printed, not checked.)
        VR_Test_Print(sprintf("ragsmash: settled at %v (%g strikes reported, fastest %.1f m/s)\n",
            (vr_rst_g.absmin + vr_rst_g.absmax) * 0.5, vr_rsmash_strikes - vr_rst_k0, vr_rsmash_fastest));
        VR_Test_Check(vr_rsmash_sounds == vr_rst_s0 && vr_rsmash_bloods == vr_rst_b0,
            sprintf("the soft fling neither sounded nor bled (%g, %g)", vr_rsmash_sounds - vr_rst_s0, vr_rsmash_bloods - vr_rst_b0));
        // The velocity rule through the engine's own call (VR_RagImpact_Hit), its strike on the wall there: a slow touch
        // (2 m/s, not held: under the least speed, 2.5) nothing; a held part slammed at 5 m/s sounds and bleeds; the
        // same again at once nothing (the body's cooldown).
        float s1 = vr_rsmash_sounds;
        float b1 = vr_rsmash_bloods;
        VR_RagImpact_Hit(vr_rst_g, vr_rst_wall, 2, 15, 2, 0, vr_rst_wn);
        VR_Test_Check(vr_rsmash_sounds == s1 && vr_rsmash_bloods == b1, "a slow touch (2 m/s): no sound, no blood");
        VR_RagImpact_Hit(vr_rst_g, vr_rst_wall, 5, 30, 5, 1, vr_rst_wn);
        VR_Test_Check(vr_rsmash_sounds == s1 + 1 && vr_rsmash_bloods == b1 + 1, "a held slam (5 m/s): a sound and blood");
        VR_RagImpact_Hit(vr_rst_g, vr_rst_wall, 5, 30, 5, 1, vr_rst_wn);
        VR_Test_Check(vr_rsmash_sounds == s1 + 1 && vr_rsmash_bloods == b1 + 1, "the same again at once: none (cooldown)");
        // Smash Volume (1.35: his note 2026-11-01, "30, 40% louder") on the held slam (full strength): past full volume.
        float sv = cvar_hget(cvarh_vr_ragsmash_volume);
        VR_Test_Check(fabs(sv - 1.35) < 0.001 && fabs(vr_rsmash_vol - min(4, (0.25 + 0.75 * vr_rsmash_t) * sv)) < 0.001 &&
            vr_rsmash_vol > 1, sprintf("the held slam's volume %.3f (strength %.2f, Smash Volume %.2f: louder than full)",
            vr_rsmash_vol, vr_rsmash_t, sv));
        // Impact Multiplier (1.15): the held slam's damage against the same strike's without it (its piece's multiplier
        // in, under the cap; the wear toward popping is that damage's). Held: the least speed vr_ragimpact_held_speed.
        float mult = cvar_hget(cvarh_vr_ragimpact_mult);
        float d1 = VR_RagImpact_Scale(VR_RagImpact_Amount(30, 5 - max(0.25, cvar_hget(cvarh_vr_ragimpact_held_speed))),
            VR_RagImpact_Piece(vr_rst_g, vr_rst_wall));
        VR_RagImpact_Hit(vr_rst_g, vr_rst_wall, 5, 30, 5, 1, vr_rst_wn);
        VR_Test_Check(fabs(mult - 1.15) < 0.001 && d1 > 0.5 && fabs(vr_rimp_last - d1 * mult) < 0.01,
            sprintf("Impact Multiplier %.2f: the strike %.2f -> %.2f damage", mult, d1, vr_rimp_last));
        VR_RST_Finish();
    }
};
#endif
