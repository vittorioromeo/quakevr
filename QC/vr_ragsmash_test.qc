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
    vr_rst_old_debug = cvar_hget(cvarh_vr_knockdown_debug);
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

// ---- 2: wall glue (animfix2; his note vrtutorial 2026-11-23 23:14:32: "when enemies are shoved into walls ... they
// glue themselves to the walls ... not only humanoids, but also dogs"). A grunt and a dog, each shoved (two hands, as
// in a fight: its enemy you, facing you) into the nearest wall from 8 units off it, three times under each of: everything
// on; the active ragdoll off (vr_ragdoll_active, _stagger, _getup, _env 0: the plain knockdown); the knockdown's wiggle
// off (vr_knockdown_wiggle 0). Each trial watched until it has been on its feet (no ragdoll) 1 s, at most 10 s: the
// seconds a ragdoll (`rag`), a ragdoll with its middle (its first part's) within its half width + 12 of the wall's plane
// (`wall`), and of those propped up off the floor against it (`propped`: its middle over a third of its standing box's
// height). Prints `wallglue: <kind> <variant> <rep>: ...` per trial and the means per cell; checks every trial ran
// and each was up again within 10 s, never propped against the wall past VR_RWG_MOST seconds at a time (stuck). A
// fourth variant is the control: all on, shoved away from the wall onto the open floor (its middle's height lying).
// Every shove a knockdown (vr_knockdown_chance 100 while it runs). Ends on `wallglue: <p> passed, <f> failed`.
// (2026-11-24, vrfiringrange, sv_random_seed 1: against the wall every kind and variant lay propped on it for nearly its
// whole knockdown, its middle 19-27 units up against 9-11 on the open floor, the active ragdoll off and the wiggle off
// alike: the plain knockdown ragdoll leaning on the wall, not the active ragdoll's lean; ROUND25.md, "animfix2".)
// (wallglue, 2026-11-24: the cause was the shove's drives pushing it on into the wall, held up by that much friction;
// they now end where a wall stops it, vr_knockdown_wall_stop; ROUND25.md, "wallglue". `vr_ragsmash_test 3`: the dogs only.)
float vr_rwg_state;    // 0 idle, 1 spawned, 2 placed, 3 shoved (watching)
float vr_rwg_trial;    // trial index: kind * 12 + variant * 3 + rep
float vr_rwg_hsum;     // its middle's height over the floor, summed over its ragdoll frames
float vr_rwg_frames;
float vr_rwg_up;       // frames on its feet in a row since the shove
float vr_rwg_rag, vr_rwg_wallf, vr_rwg_prop, vr_rwg_kd, vr_rwg_stg, vr_rwg_floor, vr_rwg_h, vr_rwg_half;
float vr_rwg_wallmost; // frames: the longest run propped against the wall this trial
float vr_rwg_run;      // frames propped in a row now
vector vr_rwg_spot;    // where every trial's monster spawns (128 units ahead of you as it began)
float vr_rwg_keep_chance;
float vr_rwg_keep_active, vr_rwg_keep_stagger, vr_rwg_keep_getup, vr_rwg_keep_env, vr_rwg_keep_wiggle;
float vr_rwg_sum_n[8]; // trials a ragdoll a while
float vr_rwg_sum_rag[8], vr_rwg_sum_wall[8], vr_rwg_sum_prop[8], vr_rwg_sum_most[8], vr_rwg_sum_kd[8], vr_rwg_sum_h[8];
entity vr_rwg_g;
float VR_RWG_TRIALS = 24;
float VR_RWG_MOST = 7; // s: propped against the wall longer than this in one run is stuck (a knockdown lasts 2-6 s)

// (variant 3, the control: all on, from 120 units off the wall shoved away from it, onto the open floor)
string(float v) VR_RWG_Variant =
{
    return v == 0 ? "all on" : v == 1 ? "active ragdoll off" : v == 2 ? "wiggle off" : "all on, open floor";
};
float() VR_RWG_Var = { return floor(vr_rwg_trial / 3) - 4 * floor(vr_rwg_trial / 12); };

void(float v) VR_RWG_SetVariant =
{
    cvar_set("vr_ragdoll_active", ftos(v == 1 ? 0 : vr_rwg_keep_active));
    cvar_set("vr_ragdoll_active_stagger", ftos(v == 1 ? 0 : vr_rwg_keep_stagger));
    cvar_set("vr_ragdoll_active_getup", ftos(v == 1 ? 0 : vr_rwg_keep_getup));
    cvar_set("vr_ragdoll_active_env", ftos(v == 1 ? 0 : vr_rwg_keep_env));
    cvar_set("vr_knockdown_wiggle", ftos(v == 2 ? 0 : vr_rwg_keep_wiggle));
};

void(entity p) VR_RWG_Spawn =
{
    float kind = vr_rwg_trial >= 12 ? 7 : 0; // (the dispenser's: 0 a grunt, 7 a dog)
    VR_RWG_SetVariant(VR_RWG_Var());
    vr_rwg_g = VR_Test_Spawn(p, kind, vr_rwg_spot, vectoyaw(p.origin - vr_rwg_spot));
    vr_rwg_state = 1;
    vr_rwg_frames = 0;
};

void(entity p, float xFirst) VR_RWG_Begin =
{
    VR_Test_Begin("wallglue");
    vr_rwg_keep_active = cvar_hget(cvarh_vr_ragdoll_active);
    vr_rwg_keep_stagger = cvar_hget(cvarh_vr_ragdoll_active_stagger);
    vr_rwg_keep_getup = cvar_hget(cvarh_vr_ragdoll_active_getup);
    vr_rwg_keep_env = cvar_hget(cvarh_vr_ragdoll_active_env);
    vr_rwg_keep_wiggle = cvar_hget(cvarh_vr_knockdown_wiggle);
    vr_rwg_keep_chance = cvar_hget(cvarh_vr_knockdown_chance);
    cvar_set("vr_knockdown_chance", "100"); // (every shove a knockdown, or the active stagger's fall)
    for(float i = 0; i < 8; i++)
    {
        vr_rwg_sum_rag[i] = 0;
        vr_rwg_sum_wall[i] = 0;
        vr_rwg_sum_prop[i] = 0;
        vr_rwg_sum_most[i] = 0;
        vr_rwg_sum_kd[i] = 0;
        vr_rwg_sum_h[i] = 0;
        vr_rwg_sum_n[i] = 0;
    }
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    vr_rwg_spot = p.origin + normalize(fw) * 128;
    vr_rwg_trial = xFirst; // (vr_ragsmash_test 3: the dogs only, from trial 12)
    VR_RWG_Spawn(p);
};

void() VR_RWG_Finish =
{
    VR_RWG_SetVariant(0);
    cvar_set("vr_knockdown_chance", ftos(vr_rwg_keep_chance));
    for(float c = 0; c < 8; c++)
    {
        string cell = sprintf("%s, %s", c >= 4 ? "dog" : "grunt", VR_RWG_Variant(c - 4 * floor(c / 4)));
        string height = sprintf("its middle %.1f units up while a ragdoll", vr_rwg_sum_h[c] / max(1, vr_rwg_sum_n[c]));
        VR_Test_Print(sprintf("wallglue: mean %s: rag %.2f s, at the wall %.2f s, propped %.2f s (longest %.2f s), knocked down %g of 3, %s\n",
            cell, vr_rwg_sum_rag[c] / 3, vr_rwg_sum_wall[c] / 3, vr_rwg_sum_prop[c] / 3, vr_rwg_sum_most[c] / 3,
            vr_rwg_sum_kd[c], height));
    }
    vr_rwg_state = 0;
    VR_Test_Verdict();
};

void(entity p) VR_RWG_Frame =
{
    entity g = vr_rwg_g;
    vr_rwg_frames = vr_rwg_frames + 1;
    if(!g || g.health <= 0 || (vr_rwg_state < 3 && vr_rwg_frames > 360))
    {
        VR_Test_Check(FALSE, sprintf("trial %g: its monster spawned and lived (state %g)", vr_rwg_trial, vr_rwg_state));
        VR_RWG_Finish();
        return;
    }
    if(vr_rwg_state == 1)
    {
        if(vr_rwg_frames < 10)
        {
            return; // (its spawn done: its health set as it starts)
        }
        // 8 units off the nearest wall, facing you, you 40 units off its front; its enemy you (aware: no surprise). The
        // control: 120 off it, you between it and the wall, shoved away from it.
        vr_rst_g = g;
        float w = VR_RST_FindWall();
        if(w >= 1000)
        {
            VR_Test_Check(FALSE, sprintf("trial %g: a wall within 256 units", vr_rwg_trial));
            VR_RWG_Finish();
            return;
        }
        float open = VR_RWG_Var() == 3;
        setorigin(g, g.origin + vr_rst_way * (w - g.maxs_x - (open ? 120 : 8)));
        g.angles_y = vectoyaw(vr_rst_wn * (open ? -1 : 1));
        g.ideal_yaw = g.angles_y;
        g.health = 1000;
        vr_rwg_half = g.maxs_x;
        vr_rwg_h = g.maxs_z - g.mins_z;
        vr_rwg_floor = g.origin_z + g.mins_z;
        setorigin(p, g.origin + vr_rst_wn * (g.maxs_x + 16 + 40) * (open ? -1 : 1));
        p.velocity = '0 0 0';
        g.enemy = p;
        vr_rwg_state = 2;
        vr_rwg_frames = 0;
        return;
    }
    if(vr_rwg_state == 2)
    {
        if(vr_rwg_frames < 18)
        {
            return;
        }
        vr_rwg_rag = vr_rwg_wallf = vr_rwg_prop = vr_rwg_kd = vr_rwg_stg = vr_rwg_wallmost = vr_rwg_run = vr_rwg_up = 0;
        g.enemy = p;
        entity oldSelf = self;
        self = p;
        vr_bash_exhaust_dmg = 0;
        vr_bash_exhaust_push = 0;
        VR_Bash_Hit(g, g.origin + '0 0 16', vr_rst_way * (VR_RWG_Var() == 3 ? -1 : 1), QVR_HITKIND_SHOVE, -1, 0,
            VR_Shove_HandsPush(TRUE));
        self = oldSelf;
        vr_rwg_hsum = 0;
        vr_rwg_state = 3;
        vr_rwg_frames = 0;
        return;
    }
    // Watching: a ragdoll, near the wall, propped up against it.
    float rag = isragdoll(g);
    vr_rwg_kd = (vr_rwg_kd || (g.vr_knockdown == 1)) ? 1 : 0;
    vr_rwg_stg = (vr_rwg_stg || g.vr_kd_stagger) ? 1 : 0;
    float near = FALSE;
    float propped = FALSE;
    if(rag)
    {
        vector mid = ragdollpoint(g, 0, '0 0 0', 2);
        near = (((mid - vr_rst_wall) * vr_rst_wn) <= (vr_rwg_half + 12));
        propped = (near && ((mid_z - vr_rwg_floor) > (vr_rwg_h / 3)));
        vr_rwg_hsum = vr_rwg_hsum + (mid_z - vr_rwg_floor);
        vr_rwg_rag++;
    }
    vr_rwg_wallf = vr_rwg_wallf + (near ? 1 : 0);
    vr_rwg_prop = vr_rwg_prop + (propped ? 1 : 0);
    vr_rwg_run = propped ? vr_rwg_run + 1 : 0;
    vr_rwg_wallmost = max(vr_rwg_wallmost, vr_rwg_run);
    vr_rwg_up = (rag || g.vr_knockdown) ? 0 : vr_rwg_up + 1;
    if(vr_rwg_up < 72 && vr_rwg_frames < 720)
    {
        return;
    }
    float c = floor(vr_rwg_trial / 3);
    float tick = 1 / 72; // (fast mode: a frame is 1/72 s of game time)
    vr_rwg_sum_rag[c] = vr_rwg_sum_rag[c] + vr_rwg_rag * tick;
    vr_rwg_sum_wall[c] = vr_rwg_sum_wall[c] + vr_rwg_wallf * tick;
    vr_rwg_sum_prop[c] = vr_rwg_sum_prop[c] + vr_rwg_prop * tick;
    vr_rwg_sum_most[c] = vr_rwg_sum_most[c] + vr_rwg_wallmost * tick;
    vr_rwg_sum_kd[c] = vr_rwg_sum_kd[c] + (vr_rwg_kd ? 1 : 0);
    float hmean = vr_rwg_hsum / max(1, vr_rwg_rag);
    if(vr_rwg_rag >= 36)
    {
        vr_rwg_sum_h[c] = vr_rwg_sum_h[c] + hmean;
        vr_rwg_sum_n[c] = vr_rwg_sum_n[c] + 1;
    }
    string how = sprintf("%s%s%s, its middle %.1f up", vr_rwg_kd ? "knocked down" : "kept its feet",
        vr_rwg_stg ? ", staggered" : "", vr_rwg_up < 72 ? ", NOT UP after 10 s" : "", hmean);
    string cell = sprintf("%s, %s, %g", vr_rwg_trial >= 12 ? "dog" : "grunt", VR_RWG_Variant(VR_RWG_Var()),
        vr_rwg_trial - 3 * c);
    VR_Test_Print(sprintf("wallglue: %s: rag %.2f s, at the wall %.2f s, propped %.2f s (longest %.2f s), %s\n", cell,
        vr_rwg_rag * tick, vr_rwg_wallf * tick, vr_rwg_prop * tick, vr_rwg_wallmost * tick, how));
    // (how long propped is too long is his call: the numbers are the evidence; checked: never stuck for good)
    VR_Test_Check(vr_rwg_up >= 72 && vr_rwg_wallmost * tick <= VR_RWG_MOST,
        sprintf("trial %g: up again within 10 s, propped against the wall at most %g s in one run (%.2f s)", vr_rwg_trial,
        VR_RWG_MOST, vr_rwg_wallmost * tick));
    remove(g);
    vr_rwg_g = world;
    vr_rwg_trial = vr_rwg_trial + 1;
    if(vr_rwg_trial >= VR_RWG_TRIALS)
    {
        VR_RWG_Finish();
        return;
    }
    VR_RWG_Spawn(p);
};

void() VR_RagSmash_TestFrame =
{
    entity p = VR_Player();
    if(vr_rwg_state > 0)
    {
        if(p)
        {
            VR_RWG_Frame(p);
        }
        return;
    }
    if(vr_rst_state == 0)
    {
        float request = cvar_hget(cvarh_vr_ragsmash_test);
        if(!request || !p)
        {
            return;
        }
        cvar_set("vr_ragsmash_test", "0");
        if(request == 1)
        {
            VR_RST_Begin(p);
        }
        if(request == 2)
        {
            VR_RWG_Begin(p, 0);
        }
        if(request == 3)
        {
            VR_RWG_Begin(p, 12);
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
        // Impact Multiplier (the shipped one, over 1): the held slam's damage against the same strike's without it (its piece's multiplier
        // in, under the cap; the wear toward popping is that damage's). Held: the least speed vr_ragimpact_held_speed.
        float mult = cvar_hget(cvarh_vr_ragimpact_mult);
        float d1 = VR_RagImpact_Scale(VR_RagImpact_Amount(30, 5 - max(0.25, cvar_hget(cvarh_vr_ragimpact_held_speed))),
            VR_RagImpact_Piece(vr_rst_g, vr_rst_wall));
        VR_RagImpact_Hit(vr_rst_g, vr_rst_wall, 5, 30, 5, 1, vr_rst_wn);
        VR_Test_Check(mult > 1 && d1 > 0.5 && fabs(vr_rimp_last - d1 * mult) < 0.01, // (the shipped default: 1.25 since fc3f51a52)
            sprintf("Impact Multiplier %.2f: the strike %.2f -> %.2f damage", mult, d1, vr_rimp_last));
        VR_RST_Finish();
    }
};
#endif
