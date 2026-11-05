// vr_flatmove_test.qc -- flat-screen movement as the game measures it (-flatscreenmode; docs/vr-port/FLATPLAY.md,
// "Crouched, slower" and "Swimming"). The moves themselves come from the client's keys (Misc/quakevr/flatmove_test.sh
// holds +forward, +flatcrouch, +jump and sets the look with setpos); these only measure, print and end:
//   1: his top level speed, once it has held within 0.5 for 24 frames: `flatmove: speed S crouched C eye E`.
//   2: a swim of half a second (36 frames: the measure's span) from where he is when asked:
//      `flatmove: swim dz D dxy H waterlevel W`.
//   3: out of the water: once he stands on dry ground (waterlevel 0, on the ground):
//      `flatmove: out z Z waterjumped J` (J: Quake's jump out of the water lifted him, FL_WATERJUMP).
//   8: a test aid: deep water within 512 units level and 256 up or down of him, on a 32-unit grid: his box under water
//      with 128 units of open water over and under it: `flatmove: deep X Y Z` (the first 4), then `flatmove: deep spots N`.
//   9: a test aid (Debug > Tests > Flat Ledges): from where he floats (waterlevel 2), the spots within 512 units, on a
//      16-unit grid at his height, where Quake's jump out of the water lifts him, each with the yaw that faces its ledge
//      and room to swim 48 units up to it: `flatmove: ledge X Y Z yaw A` (the first 8), then `flatmove: ledges N`.
// Each gives up after 6 s with a `timed out` line (a safety net: never the expected end).

#ifdef QVR_TESTS

float vr_fmt_on;    // the mode running (0: none)
float vr_fmt_n;     // frames counted in it
float vr_fmt_same;  // (1) frames the speed has held
float vr_fmt_last;  // (1) the speed last frame
vector vr_fmt_from; // (2) where the swim started; (9) where the scan is centred
float vr_fmt_jumped; // (3) FL_WATERJUMP seen
float vr_fmt_found; // (9) ledges found
float VR_FMT_GIVEUP = 432; // 6 s of frames
float VR_FMT_SCAN = 512;

void() VR_FMT_End =
{
    vr_fmt_on = 0;
    cvar_set("vr_flatmove_test", "0");
};

// A spot at `p` where Quake's jump out of the water (WaterJumpTry, client.qc) lifts him along `d`, with room for his
// box to swim the last 48 units up to it.
float(entity pl, vector p, vector d) VR_FMT_Ledge =
{
    traceline(p + '0 0 8', p + '0 0 8' + d * 24, TRUE, pl);
    if(trace_fraction == 1)
    {
        return FALSE;
    }
    traceline(p + '0 0 32', p + '0 0 32' + d * 24, TRUE, pl);
    if(trace_fraction < 1)
    {
        return FALSE;
    }
    vector back = p - d * 48;
    if(pointcontents(back) != CONTENT_WATER)
    {
        return FALSE;
    }
    tracebox(back, pl.mins, pl.maxs, p, TRUE, pl);
    return !trace_startsolid && !trace_allsolid && trace_fraction == 1;
};

void(entity pl) VR_FMT_ScanRow =
{
    float x = vr_fmt_from_x - VR_FMT_SCAN + vr_fmt_n * 16;
    for(float y = vr_fmt_from_y - VR_FMT_SCAN; y <= vr_fmt_from_y + VR_FMT_SCAN && vr_fmt_found < 8; y += 16)
    {
        vector p = [x, y, vr_fmt_from_z];
        if(pointcontents(p + '0 0 4') != CONTENT_WATER || pointcontents(p + '0 0 22') == CONTENT_WATER)
        {
            continue; // not waterlevel 2
        }
        tracebox(p, pl.mins, pl.maxs, p, TRUE, pl);
        if(trace_startsolid || trace_allsolid)
        {
            continue;
        }
        for(float a = 0; a < 360; a += 22.5)
        {
            vector d = [cos(a * 3.14159265 / 180), sin(a * 3.14159265 / 180), 0];
            if(VR_FMT_Ledge(pl, p, d))
            {
                vr_fmt_found++;
                bprint(sprintf("flatmove: ledge %.0f %.0f %.0f yaw %g\n", p_x, p_y, p_z, a));
                break;
            }
        }
    }
};

// (8) Deep water in this row: his box fits and is under water, with 128 units of open water over it and under it.
void(entity pl) VR_FMT_DeepRow =
{
    float x = vr_fmt_from_x - VR_FMT_SCAN + vr_fmt_n * 32;
    for(float y = vr_fmt_from_y - VR_FMT_SCAN; y <= vr_fmt_from_y + VR_FMT_SCAN && vr_fmt_found < 4; y += 32)
    {
        for(float z = vr_fmt_from_z - 256; z <= vr_fmt_from_z + 256 && vr_fmt_found < 4; z += 32)
        {
            vector p = [x, y, z];
            if(pointcontents(p + '0 0 150') != CONTENT_WATER || pointcontents(p - '0 0 152') != CONTENT_WATER ||
               pointcontents(p) != CONTENT_WATER)
            {
                continue;
            }
            tracebox(p, pl.mins, pl.maxs, p + '0 0 128', TRUE, pl);
            if(trace_startsolid || trace_allsolid || trace_fraction < 1)
            {
                continue;
            }
            tracebox(p, pl.mins, pl.maxs, p - '0 0 128', TRUE, pl);
            if(trace_fraction < 1)
            {
                continue;
            }
            vr_fmt_found++;
            bprint(sprintf("flatmove: deep %.0f %.0f %.0f\n", p_x, p_y, p_z));
        }
    }
};

void() VR_FlatMoveTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_flatmove_test);
    if(!ask)
    {
        vr_fmt_on = 0;
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    if(vr_fmt_on != ask)
    {
        vr_fmt_on = ask;
        vr_fmt_n = 0;
        vr_fmt_same = 0;
        vr_fmt_last = -1;
        vr_fmt_from = p.origin;
        vr_fmt_jumped = 0;
        vr_fmt_found = 0;
        if(!VR_FlatPlay(p))
        {
            bprint("flatmove: flat play off: run with -flatscreenmode\n");
        }
    }
    vr_fmt_n++;
    if(ask == 8)
    {
        VR_FMT_DeepRow(p);
        if(vr_fmt_n * 32 > VR_FMT_SCAN * 2 || vr_fmt_found >= 4)
        {
            bprint(sprintf("flatmove: deep spots %g\n", vr_fmt_found));
            VR_FMT_End();
        }
        return;
    }
    if(ask == 9)
    {
        VR_FMT_ScanRow(p);
        if(vr_fmt_n * 16 > VR_FMT_SCAN * 2 || vr_fmt_found >= 8)
        {
            bprint(sprintf("flatmove: ledges %g\n", vr_fmt_found));
            VR_FMT_End();
        }
        return;
    }
    if(vr_fmt_n > VR_FMT_GIVEUP)
    {
        bprint(sprintf("flatmove: timed out: mode %g, speed %.1f, waterlevel %g, z %.1f, waterjumped %g\n", ask,
            vlen([p.velocity_x, p.velocity_y, 0]), p.waterlevel, p.origin_z, vr_fmt_jumped));
        VR_FMT_End();
        return;
    }
    if(ask == 1)
    {
        float s = vlen([p.velocity_x, p.velocity_y, 0]);
        vr_fmt_same = ((s > 50) && (fabs(s - vr_fmt_last) < 0.5)) ? vr_fmt_same + 1 : 0;
        vr_fmt_last = s;
        if(vr_fmt_same >= 24)
        {
            bprint(sprintf("flatmove: speed %.1f crouched %g eye %.1f\n", s,
                VRHasBit(p.vrbits0, QVR_VRBITS0_CROUCHED) ? 1 : 0, p.headpos_z - p.absmin_z));
            VR_FMT_End();
        }
        return;
    }
    if(ask == 2)
    {
        if(vr_fmt_n >= 36)
        {
            vector d = p.origin - vr_fmt_from;
            bprint(sprintf("flatmove: swim dz %.1f dxy %.1f waterlevel %g\n", d_z, vlen([d_x, d_y, 0]), p.waterlevel));
            VR_FMT_End();
        }
        return;
    }
    if(ask == 3)
    {
        if(p.flags & FL_WATERJUMP)
        {
            vr_fmt_jumped = 1;
        }
        if(p.waterlevel == 0 && (p.flags & FL_ONGROUND))
        {
            bprint(sprintf("flatmove: out z %.1f waterjumped %g\n", p.origin_z, vr_fmt_jumped));
            VR_FMT_End();
        }
        return;
    }
    bprint(sprintf("flatmove: no mode %g (1 speed, 2 swim, 3 out of the water, 8 deep water, 9 ledges)\n", ask));
    VR_FMT_End();
};

#endif
