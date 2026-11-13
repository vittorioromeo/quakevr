#ifdef QVR_TESTS
// vr_stealth_test4.qc -- the stealth AI's round of the author's notes of 2026-10-08 (`vr_stealth_test 120`-`129`;
// Misc/quakevr/stealth_tests.sh seethrough|fence|chase|props; docs/vr-port/STEALTH.md, "Tests"). Each scene prints
// `stealthtest: <name> PASS|FAIL`:
//   120 a see-through wall (e1m1's door *10 or *29 put between a lit player and a grunt 300 units away): opaque, the grunt
//       doesn't see him nor hear a knock at his feet; with alpha 0.5 it sees him (its meter fills) and hears the knock;
//       with vr_stealth_seethrough 0, opaque again;
//       then the wall opaque but its textures taken for fences ('{' ones: vr_stealth_test_fence, the maps the kit has
//       have no solid fence): seen and heard through; the textures not fences: neither;
//   122 the hunt: a grunt Hostile at him sees him dart sideways, then loses him (he is put out of sight under the floor):
//       it follows his trail (to where it saw him last, then ahead along his way), gives up after vr_stealth_lose_time
//       (8 s here) and searches (Alert); then with him far (vr_stealth_lose_far 600) it gives up after a quarter of it;
//   125 his trail by the navmesh, not him (vr_nav_trail): a grunt Hostile at him sees him stand, then he is put out of
//       sight 288-768 units on (a place with a navmesh path there): it walks the navmesh to where it saw him, searches
//       about there, never comes within 160 of him, gives up at 15 s; with vr_nav_trail 0 the same, for comparison;
//   126 the trail carries on past the corner (vr_nav_trail_carry): he runs round a corner out of a hunter's sight; it goes
//       on round it along his way, searches, gives up at 15 s (VR_STT4_Corner);
//   127 noises investigated: a knock and a shot round a corner: the grunt walks there, scouts, walks back to its spot;
//       one out of hearing doesn't; a hunter that lost him hears a shot near him: its trail goes there (VR_STT4_Noises);
//   124 props in the way (vrteleporters, room T): a row of five large crates between a knight Hostile at him and him
//       (the knight 500 units off): with vr_ai_props 1 it gets round them to him (within 80 units) within 6 s; with 0 the
//       time it took (or not, in 15 s) for comparison;
//   123 a dog chasing him as he runs in circles for 30 s (it turns all the time): the script logs its drawn moves
//       (vr_debug_drawn_moves progs/dog: "drawnmove:" lines, a jump between frames each) and compares
//       vr_monster_lerp_continue 1 and 0 (stealth_tests.sh dogs);

float stt4_alpha;     // the see-through wall's alpha this stage
entity stt4_wall;
float stt4_trail;     // the hunt: the furthest step of the trail it took
float stt4_near;      // how near it came to where it saw him last
float stt4_hid;       // when he was put out of sight
vector stt4_ahead;    // where its trail's second step went
vector stt4_side;     // the way he darts
entity stt4_probe;    // 125: a box his size, for navreach
vector stt4_p2;       // 125: where he goes on to, out of sight
float stt4_min2;      // 125: the nearest it came to him there
float stt4_omni;      // 125: the frames its goal was him while it hunted him unseen
float stt4_steps0;    // 125: vr_nav_trail_steps as he went

// Him out of sight of everything: under the floor (the first solid point from 256 units down, at least `xMin` from where
// he stood), noclip; '0 0 0' if none.
vector(entity p, float xMin) VR_STT4_Hide =
{
    for(float down = max(256, xMin); down <= 4096; down = down + 64)
    {
        vector at = p.origin - '0 0 1' * down;
        if(pointcontents(at) == CONTENT_SOLID && pointcontents(at + p.view_ofs) == CONTENT_SOLID)
        {
            p.movetype = MOVETYPE_NOCLIP;
            p.velocity = '0 0 0';
            setorigin(p, at);
            return at;
        }
    }
    return '0 0 0';
};

// The props scene's crates taken away (those of the scene: within 250 units of the row's middle).
void() VR_STT4_Crates =
{
    for(entity e = find(world, classname, "vr_crate"); e; e = find(e, classname, "vr_crate"))
    {
        if(vlen(e.origin - '-1290 530 40') < 250)
        {
            setorigin(e, e.origin - '0 0 4000'); // (out of the way at once: removed next frame)
            e.solid = SOLID_NOT;
            e.think = SUB_Remove;
            e.nextthink = time;
        }
    }
};

void(entity p) VR_STT4_Unhide =
{
    p.movetype = MOVETYPE_WALK;
    p.velocity = '0 0 0';
    setorigin(p, stt2_phome);
};

// The way along +x or -x from `org` with room for `xDist` units (the first clear one); '0 0 0' none.
vector(vector org, float xDist) VR_STT4_ClearAxis =
{
    vector d;
    for(float i = 0; i < 4; i++)
    {
        d = i == 0 ? '1 0 0' : (i == 1 ? '-1 0 0' : (i == 2 ? '0 1 0' : '0 -1 0'));
        tracebox(org, '-16 -16 -24', '16 16 32', org + d * xDist, TRUE, stt2_p);
        if(trace_fraction >= 1)
        {
            return d;
        }
    }
    return '0 0 0';
};

void() VR_STT4_End =
{
    if(stt4_wall)
    {
        remove(stt4_wall);
        stt4_wall = world;
    }
    cvar_set("vr_stealth_seethrough", "1");
    cvar_set("vr_stealth_test_fence", "");
    cvar_set("vr_stealth_lose_time", "20");
    cvar_set("vr_stealth_lose_far", "1500");
    cvar_set("vr_stealth_chase_trail", "1");
    cvar_set("vr_stealth_chase_predict", "1.5");
    localcmd("tune vr_stealth_hunt_clue 256\n"); // (a tunable: by the console)
    if(stt4_probe)
    {
        remove(stt4_probe);
        stt4_probe = world;
    }
    if(stt2_p.movetype == MOVETYPE_NOCLIP)
    {
        VR_STT4_Unhide(stt2_p);
    }
    VR_STT2_RemoveAll();
    VR_STT2_End();
};

// The meter's verdict over a stage: `want` seen (its meter past the glimpse, or Alert, or Hostile) or not.
float(entity m) VR_STT4_Saw =
{
    return m.stl_meter >= cvar_hget(cvarh_vr_stealth_glimpse) || m.stl_state == STL_ALERT || m.enemy != world;
};

// 125: whether `c` (his origin there) is out of sight of the grunt and of where he stood (and of the spots about it a
// search may stand at: 160 units off it each way, cut short by walls).
float(vector c, vector p1, entity m) VR_STT4_Hidden =
{
    vector head = c + '0 0 22';
    traceline(m.origin + m.view_ofs, head, TRUE, m);
    if(trace_fraction >= 1) { return FALSE; }
    for(float a = -45; a < 360; a = a + 45)
    {
        vector eye = p1;
        if(a >= 0)
        {
            makevectors('0 1 0' * a);
            traceline(p1, p1 + v_forward * 160, TRUE, world);
            eye = trace_endpos - v_forward * min(16, vlen(trace_endpos - p1));
        }
        traceline(eye, head, TRUE, world);
        if(trace_fraction >= 1) { return FALSE; }
        traceline(eye, c, TRUE, world);
        if(trace_fraction >= 1) { return FALSE; }
    }
    return TRUE;
};

// 125: a place for him out of sight of where it saw him (VR_STT4_Hidden), with a navmesh path there for the grunt, the
// nearest of 288-768 units off; '0 0 0' none.
vector(entity m, vector p1) VR_STT4_Beyond =
{
    if(!stt4_probe)
    {
        stt4_probe = spawn();
        stt4_probe.classname = "stt4_probe";
    }
    setsize(stt4_probe, VEC_HULL_MIN, VEC_HULL_MAX);
    for(float r = 288; r <= 768; r = r + 48)
    {
        for(float a = 0; a < 360; a = a + 22.5)
        {
            makevectors('0 1 0' * a);
            vector c = p1 + v_forward * r;
            if(pointcontents(c) == CONTENT_SOLID) { continue; }
            traceline(c, c - '0 0 256', TRUE, world);
            if(trace_fraction >= 1 || trace_startsolid) { continue; }
            c = trace_endpos + '0 0 25';
            tracebox(c, VEC_HULL_MIN, VEC_HULL_MAX, c, FALSE, world);
            if(trace_startsolid || trace_allsolid || !VR_STT4_Hidden(c, p1, m)) { continue; }
            setorigin(stt4_probe, c);
            if(navreach(m, stt4_probe) > 0) { return c; }
        }
    }
    return '0 0 0';
};

// 125: a grunt Hostile at him sees him stand, then he is put out of sight on along the navmesh (VR_STT4_Beyond): with
// vr_nav_trail 1 it walks the navmesh to where it saw him (within 64), searches about there, never heads for him (never
// within 160 of him, its goal never him while unseen) and gives up at vr_stealth_lose_time (15 s). With vr_nav_trail 0
// (set ahead: `tune vr_nav_trail 0`; stealth_tests.sh trailnav runs both) an info line for comparison (the old trail's
// last step went on at him by the navmesh).
void(entity p, entity m, float t) VR_STT4_TrailNav =
{
    float on = cvar_hget(cvarh_vr_nav_trail);
    if(stt2_stage == 0)
    {
        stt2_dir = VR_STT4_ClearAxis(p.origin, 320);
        if(stt2_dir == '0 0 0')
        {
            VR_STT2_Report("trailnav", FALSE, "no room");
            VR_STT4_End();
            return;
        }
        cvar_set("vr_stealth_share_near", "0");
        cvar_set("vr_stealth_share_view", "0");
        cvar_set("vr_stealth_light_force", "128");
        cvar_set("vr_stealth_lose_time", "15");
        cvar_set("vr_stealth_lose_far", "0");
        cvar_set("vr_stealth_chase_trail", "1");
        cvar_set("vr_stealth_chase_predict", "1.5");
        stt2_m1 = VR_STT2_Spawn(p.origin + stt2_dir * 300 + '0 0 8', vectoyaw(-stt2_dir), 0);
        stt4_p2 = '0 0 0';
        VR_STT2_Stage(1);
        return;
    }
    // 1: the navmesh ready, a place out of sight found; spotted, he stands a second, then he is out there
    if(stt2_stage == 1)
    {
        if(stt4_p2 == '0 0 0')
        {
            if(navreach(m, p) < 0)
            {
                if(t > 20)
                {
                    VR_STT2_Report("trailnav", FALSE, "no navmesh");
                    VR_STT4_End();
                }
                return;
            }
            stt4_p2 = VR_STT4_Beyond(m, p.origin);
            if(stt4_p2 == '0 0 0')
            {
                VR_STT2_Report("trailnav", FALSE, "no place out of sight with a path there");
                VR_STT4_End();
                return;
            }
            VR_STT2_Stage(stt2_stage);
            return;
        }
        if(t < 0.1)
        {
            VR_STT2_Calm(m, m.angles_y);
            VR_Stealth_Spot(m, p);
            return;
        }
        if(t < 1.2)
        {
            return;
        }
        if(m.enemy != p)
        {
            VR_STT2_Report("trailnav", FALSE, sprintf("not hunting him (enemy %s)", m.enemy.classname));
            VR_STT4_End();
            return;
        }
        stt2_at = p.origin;
        p.velocity = '0 0 0';
        setorigin(p, stt4_p2);
        stt4_hid = time;
        stt4_trail = 0;
        stt4_near = 99999;
        stt4_min2 = 99999;
        stt4_omni = 0;
        stt4_steps0 = vr_nav_trail_steps;
        VR_STT2_Stage(2);
        return;
    }
    // 2: hunting him out of sight till it gives up (15 s)
    p.velocity = '0 0 0';
    if(m.enemy == p)
    {
        stt4_trail = max(stt4_trail, m.stl_trail);
        vector d1 = m.origin - stt2_at;
        stt4_near = min(stt4_near, vlen('1 0 0' * d1_x + '0 1 0' * d1_y));
        stt4_min2 = min(stt4_min2, vlen(m.origin - p.origin));
        if(m.goalentity == p && m.stl_trail > 0)
        {
            stt4_omni = stt4_omni + 1;
        }
        if(t < 20 && stt4_min2 > 64)
        {
            return;
        }
    }
    float gave = time - m.stl_lost_time;
    float steps = vr_nav_trail_steps - stt4_steps0;
    if(on)
    {
        VR_STT2_Report("trailnav_spot", stt4_near < 64 && steps > 0,
            sprintf("came within %.0f of where it saw him last, %g navmesh steps along his trail", stt4_near, steps));
        VR_STT2_Report("trailnav_not_him", stt4_min2 > 160 && !stt4_omni,
            sprintf("within %.0f of him (%.0f on, unseen); goal him %g frames; step %g",
                stt4_min2, vlen(stt4_p2 - stt2_at), stt4_omni, stt4_trail));
        VR_STT2_Report("trailnav_gives_up", m.enemy == world && gave > 14.6 && gave < 15.6 && m.stl_state == STL_ALERT,
            sprintf("gave up %.2f s after it last saw him (15), state %g", gave, m.stl_state));
    }
    else
    {
        bprint(sprintf("stealthtest: trailnav_off info: within %.0f of him (%s, %.0f on), %.0f of the spot; goal him %g frames\n",
            stt4_min2, stt4_min2 <= 64 ? "reached" : "not reached", vlen(stt4_p2 - stt2_at), stt4_near, stt4_omni));
    }
    bprint(sprintf("stealthtest: trailnav done, %g failed\n", stt2_fails));
    VR_STT4_End();
};

// ---- 126 / 127: a corner (the notes of 2026-11-13: the trail carries on past the corner; noises investigated) ----

vector stt4_pp;      // 126/127: where he stands, seen (P)
vector stt4_c;       // the corner's pivot at the far end of his way (C)
vector stt4_s;       // the side way round the corner
vector stt4_d;       // out of sight round it (D)
vector stt4_mpos;    // where the grunt stands (M)
vector stt4_fpos;    // 127: the far grunt's spot
float stt4_cand;     // the corner search's next candidate (another after a place with no navmesh path)
float stt4_prog;     // 126: the farthest it went round the corner (along the side way, past C)
float stt4_aprog;    // 126: its trail's step 2 spot along the side way
float stt4_trial;    // 127: the trial
float stt4_nt;       // 127: when the noise was made
float stt4_minD;     // 127: the nearest it came to the noise (xy)
float stt4_r;        // 127: the noise's reach
vector stt4_rej;     // the corner search's rejects: no wall ahead, no side way, seen round it
float stt4_n;        // and the places it tried

float(vector a, float h) VR_STT4_Floor =
{
    traceline(a, a - '0 0 1' * h, TRUE, world);
    return trace_fraction < 1 && !trace_startsolid;
};

// A corner near him (the places he can walk to from his spot, navplace): from P a straight way along an axis to a wall
// 160-640 units on, a side way round its end (C, 40 short of the wall) clear for 192 units with a floor along it, a
// grunt's spot M 160 or 256 behind P (or 48 toward the side) that sees C but not 128 or 160 units round the corner (D,
// 160 round). The walk from his spot, then (e1m1) from a few spots farther in. From candidate `stt4_cand` on; FALSE none.
float stt4_seed;     // the walk's start: 0 his spot, then e1m1's (VR_STT4_Seed)

vector(float i) VR_STT4_Seed =
{
    if(i == 0) { return stt2_phome; }
    if(mapname != "e1m1") { return '0 0 0'; }
    if(i == 1) { return '-32 2000 -29'; }  // (nav_tele_metric's start)
    if(i == 2) { return '-466 2662 -96'; } // (nav_lift's, below the lift)
    if(i == 3) { return '-544 2540 58'; }  // (and above it)
    return '0 0 0';
};

float() VR_STT4_CornerFind =
{
    stt4_rej = '0 0 0';
    stt4_n = 0;
    for(; VR_STT4_Seed(stt4_seed) != '0 0 0'; stt4_seed++)
    {
        vector walked = navplace(-1, VR_STT4_Seed(stt4_seed), 48, 600);
        float n = walked_x;
        stt4_n = stt4_n + n;
        for(; stt4_cand < n * 8; stt4_cand++)
        {
            vector pp = navplace(floor(stt4_cand / 8));
            float k = stt4_cand - floor(stt4_cand / 8) * 8;
            float a = floor(k / 2);
            vector dir = a == 0 ? '1 0 0' : (a == 1 ? '-1 0 0' : (a == 2 ? '0 1 0' : '0 -1 0'));
            vector side = '1 0 0' * (0 - dir_y) + '0 1 0' * dir_x;
            if(k - a * 2 > 0) { side = '0 0 0' - side; }
            tracebox(pp, VEC_HULL_MIN, VEC_HULL_MAX, pp + dir * 640, TRUE, world);
            float len = vlen(trace_endpos - pp);
            if(trace_startsolid || trace_fraction >= 1 || len < 160) { stt4_rej_x = stt4_rej_x + 1; continue; }
            vector c = pp + dir * (len - 40);
            tracebox(c, VEC_HULL_MIN, VEC_HULL_MAX, c + side * 192, TRUE, world);
            if(trace_fraction < 1) { stt4_rej_y = stt4_rej_y + 1; continue; }
            if(!VR_STT4_Floor(c + side * 96, 80) || !VR_STT4_Floor(c + side * 160, 80))
            {
                continue;
            }
            // (the grunt 160 or 256 behind, on the line or 48 toward the corner's side: hugging that wall it sees less round it)
            float hid = FALSE;
            vector mp = '0 0 0';
            for(float b = 0; b < 4 && !hid; b++)
            {
                mp = pp - dir * (b < 2 ? 160 : 256) + side * ((b == 1 || b == 3) ? 48 : 0);
                tracebox(pp, VEC_HULL_MIN, VEC_HULL_MAX, mp, TRUE, world);
                if(trace_fraction < 1 || !VR_STT4_Floor(mp, 80)) { continue; }
                vector eye = mp + '0 0 22';
                traceline(eye, c + '0 0 22', TRUE, world);
                if(trace_fraction < 1) { continue; }
                hid = TRUE;
                for(float j = 128; j <= 160; j = j + 32)
                {
                    traceline(eye, c + side * j + '0 0 22', TRUE, world);
                    if(trace_fraction >= 1) { hid = FALSE; }
                    traceline(eye, c + side * j, TRUE, world);
                    if(trace_fraction >= 1) { hid = FALSE; }
                }
            }
            if(!hid) { stt4_rej_z = stt4_rej_z + 1; continue; }
            stt4_pp = pp;
            stt4_c = c;
            stt4_s = side;
            stt4_d = c + side * 160;
            stt4_mpos = mp + '0 0 8';
            stt2_dir = dir;
            stt4_cand++;
            return TRUE;
        }
        stt4_cand = 0;
    }
    return FALSE;
};

// 126/127: the scene set (stage 0): a corner found, the grunt at M facing P; FALSE (reported) none.
float(string xName) VR_STT4_CornerScene =
{
    cvar_set("vr_stealth_share_near", "0");
    cvar_set("vr_stealth_share_view", "0");
    cvar_set("vr_stealth_light_force", "128");
    cvar_set("vr_stealth_lose_time", "15");
    cvar_set("vr_stealth_lose_far", "0");
    cvar_set("vr_stealth_chase_trail", "1");
    if(!VR_STT4_CornerFind())
    {
        VR_STT2_Report(xName, FALSE, sprintf("no corner near his spot (%g places; rejects %v)", stt4_n, stt4_rej));
        VR_STT4_End();
        return FALSE;
    }
    if(stt2_m1 == world)
    {
        // (the map's own monsters out of the way: e1m1's far end has a few, who hunt him too)
        for(entity e = nextent(world); e; e = nextent(e))
        {
            if((e.flags & FL_MONSTER) && e.classname != "player")
            {
                e.flags = e.flags - FL_MONSTER;
                e.solid = SOLID_NOT;
                e.health = 0;
                e.think = SUB_Remove;
                e.nextthink = time;
            }
        }
        stt2_m1 = VR_STT2_Spawn(stt4_mpos, vectoyaw(stt2_dir), 0);
        bprint(sprintf("stealthtest: %s info: the grunt at %v, he at %v, the corner %v, round it %v\n", xName, stt4_mpos,
            stt4_pp, stt4_c, stt4_d));
    }
    setorigin(stt2_m1, stt4_mpos);
    stt2_m1.angles_y = vectoyaw(stt2_dir); // (put down Idle once its start has run: VR_STT4_CornerReady)
    if(!stt4_probe)
    {
        stt4_probe = spawn();
        stt4_probe.classname = "stt4_probe";
    }
    setsize(stt4_probe, VEC_HULL_MIN, VEC_HULL_MAX);
    setorigin(stt4_probe, stt4_d);
    return TRUE;
};

// 126/127 stage 1: the navmesh ready and a path from M round the corner to D (else the next corner): 1 ready, 0 wait,
// -1 given up (reported).
float(string xName, entity m, float t) VR_STT4_CornerReady =
{
    if(t < 0.5) { return 0; } // (its start first: a monster put down before it never takes damage, nor the rules)
    float r = navreach(m, stt4_probe);
    if(r < 0)
    {
        if(t > 20)
        {
            VR_STT2_Report(xName, FALSE, "no navmesh");
            VR_STT4_End();
            return -1;
        }
        return 0;
    }
    if(r == 0)
    {
        if(!VR_STT4_CornerScene(xName)) { return -1; }
        return 0;
    }
    return 1;
};

// 126: the trail carries on past the corner (vr_nav_trail_carry): a grunt Hostile at him sees him at P, he runs on to C
// (300 units a second) and turns round the corner (32 units round it he is put under the floor: gone as he turned). It walks to where it saw him
// last, on round the corner along his way (its step 2 spot at least 128 units round, it at least 96), searches (step 3),
// its goal never him, and gives up at 15 s. With `tune vr_nav_trail_carry 0` (stealth_tests.sh corner runs both) an
// info line for comparison (the old straight guess, cut short by the corner's wall).
void(entity p, entity m, float t) VR_STT4_Corner =
{
    float carry = cvar_hget(cvarh_vr_nav_trail_carry);
    if(stt2_stage == 0)
    {
        stt4_cand = 0;
        stt4_seed = 0;
        if(!VR_STT4_CornerScene("corner")) { return; }
        m = stt2_m1;
        p.movetype = MOVETYPE_NOCLIP;
        p.velocity = '0 0 0';
        setorigin(p, stt4_pp);
        VR_STT2_Stage(1);
        return;
    }
    if(stt2_stage == 1)
    {
        float ok = VR_STT4_CornerReady("corner", m, t);
        if(ok <= 0) { return; }
        setorigin(p, stt4_pp);
        VR_STT2_Calm(m, vectoyaw(stt2_dir));
        VR_Stealth_Spot(m, p);
        VR_STT2_Stage(2);
        return;
    }
    // 2: seen standing a second, then he runs to C and round the corner to D
    if(stt2_stage == 2)
    {
        p.velocity = '0 0 0';
        if(t < 1) { return; }
        if(m.enemy != p)
        {
            VR_STT2_Report("corner", FALSE, sprintf("not hunting him (enemy %s)", m.enemy.classname));
            VR_STT4_End();
            return;
        }
        float step = 300 * frametime;
        vector to = (vlen(p.origin - stt4_c) > 1 && (p.origin - stt4_c) * stt4_s < 1) ? stt4_c : stt4_c + stt4_s * 32;
        if(vlen(to - p.origin) <= step)
        {
            setorigin(p, to);
            if(to != stt4_c)
            {
                VR_STT4_Hide(p, 256);
                stt4_hid = time;
                stt4_trail = 0;
                stt4_prog = -9999;
                stt4_aprog = -9999;
                stt4_omni = 0;
                VR_STT2_Stage(3);
            }
            return;
        }
        setorigin(p, p.origin + normalize(to - p.origin) * step);
        return;
    }
    // 3: hunting him out of sight till it gives up
    p.velocity = '0 0 0';
    if(m.enemy == p)
    {
        stt4_trail = max(stt4_trail, m.stl_trail);
        stt4_prog = max(stt4_prog, (m.origin - stt4_c) * stt4_s);
        if(m.stl_trail == 2 && m.stl_marker != world)
        {
            stt4_aprog = max(stt4_aprog, (m.stl_marker.origin - stt4_c) * stt4_s);
        }
        if(m.goalentity == p && m.stl_trail > 0) { stt4_omni = stt4_omni + 1; }
        if(t < 25) { return; }
    }
    float gave = time - m.stl_lost_time;
    string info = sprintf("its step 2 spot %.0f round the corner, it went %.0f round; step %g; goal him %g frames; gave up %.2f s after it lost him (15)",
        stt4_aprog, stt4_prog, stt4_trail, stt4_omni, gave);
    if(carry > 0)
    {
        VR_STT2_Report("corner_carry", stt4_aprog >= 128 && stt4_prog >= 96 && stt4_trail >= 3 && !stt4_omni, info);
        VR_STT2_Report("corner_gives_up", m.enemy == world && gave > 14.6 && gave < 15.6 && m.stl_state == STL_ALERT,
            sprintf("gave up %.2f s after it last saw him (15), state %g", gave, m.stl_state));
    }
    else
    {
        bprint(sprintf("stealthtest: corner_off info: %s\n", info));
    }
    bprint(sprintf("stealthtest: corner done, %g failed\n", stt2_fails));
    VR_STT4_End();
};

// 127 trial start: both grunts Idle at their spots, the noise made out of sight round the corner (D).
void(entity p, float xTrial) VR_STT4_NoiseTrial =
{
    stt4_trial = xTrial;
    setorigin(stt2_m1, stt4_mpos);
    VR_STT2_Calm(stt2_m1, vectoyaw(stt2_dir));
    stt2_m1.stl_scouted = 0;
    stt4_minD = 99999;
    stt4_steps0 = vr_nav_trail_steps;
    stt4_omni = 0;
    stt4_trail = 0; // (Alert seen)
    stt4_nt = 0;
    VR_STT2_Stage(10 + xTrial);
};

// 127: noises investigated. A grunt at M (Idle); a crate's knock (trial 1) and a gunshot (2) out of its sight round the
// corner (D, the noise's reach 1.3 times what it takes to hear it there): it walks there (the navmesh), scouts about it
// (vr_stealth_scout_time: at least one spot walked to), walks back to its spot (within 48) and is Idle again; a second
// grunt where the same noise is out of its hearing stays Idle. Trial 3: the grunt Hostile at him loses him (he is put
// under the floor at C); a gunshot at D, near him (vr_stealth_hunt_clue 4000 here: he is under the floor): its trail goes to D (a heard clue),
// it walks there, its goal never him.
void(entity p, entity m, float t) VR_STT4_Noises =
{
    if(stt2_stage == 0)
    {
        stt4_cand = 0;
        stt4_seed = 0;
        if(!VR_STT4_CornerScene("noises")) { return; }
        p.movetype = MOVETYPE_NOCLIP;
        VR_STT4_Hide(p, 256);
        VR_STT2_Stage(1);
        return;
    }
    if(stt2_stage == 1)
    {
        float ok = VR_STT4_CornerReady("noises", m, t);
        if(ok <= 0) { return; }
        // (the reach: 1.3 times what it takes to hear it at D, walls and all)
        float d = vlen(m.origin + m.view_ofs - stt4_d);
        stt4_r = 1.3 * d / max(0.01, VR_Stealth_NoisePath(stt4_d, m, world) * VR_Stealth_Hearing(m));
        // (the far grunt: a place it can't hear that from, the farthest within 1.6 times the reach, else any)
        vector walked = navplace(-1, VR_STT4_Seed(stt4_seed), 48, 600);
        float n = walked_x;
        stt4_fpos = '0 0 0';
        float best = 0;
        for(float i = 0; i < n; i++)
        {
            vector f = navplace(i) + '0 0 8';
            float df = vlen(f + '0 0 22' - stt4_d);
            float reach = stt4_r * VR_Stealth_Hearing(m) * VR_Stealth_NoiseLine(stt4_d, f + '0 0 22', m, world);
            traceline(f + '0 0 22', stt4_pp + '0 0 22', TRUE, world);
            if(reach < df * 0.9 && df < stt4_r * 1.6 && df > best && vlen(f - stt4_mpos) > 128)
            {
                best = df;
                stt4_fpos = f;
            }
        }
        if(stt4_fpos != '0 0 0')
        {
            stt2_m2 = VR_STT2_Spawn(stt4_fpos, 0, 0);
        }
        bprint(sprintf("stealthtest: noises info: reach %.0f, the near grunt %.0f off, the far one %.0f%s\n", stt4_r, d, best,
            stt2_m2 == world ? " (none: not checked)" : ""));
        VR_STT4_NoiseTrial(p, 1);
        return;
    }
    // 11, 12: a knock, a shot; 13: a heard clue
    float mk = stt4_trial == 1 ? STL_N_PROP : STL_N_GUN;
    string nm = stt4_trial == 1 ? "noises_knock" : (stt4_trial == 2 ? "noises_shot" : "noises_clue");
    if(stt4_trial == 3 && stt2_stage == 13)
    {
        // he stands at C, seen; then under the floor there; a second on, the shot at D
        if(t < 0.1)
        {
            VR_STT2_Remove(stt2_m2); // (the far one done with: not to see him at C)
            stt2_m2 = world;
            p.movetype = MOVETYPE_NOCLIP;
            setorigin(p, stt4_c);
            VR_Stealth_Spot(m, p);
            localcmd("tune vr_stealth_hunt_clue 4000\n"); // (he is under the floor, maybe far below: the clue's rule, not its range)
            return;
        }
        if(t < 1.2) { return; }
        if(stt4_nt == 0)
        {
            VR_STT4_Hide(p, 256);
            stt4_nt = -1;
            return;
        }
        if(stt4_nt < 0 && t > 2.2)
        {
            stt4_nt = time;
            VR_Stealth_Noise(stt4_d, stt4_r * 4, world, mk); // (louder: heard from wherever its trail took it)
            bprint(sprintf("stealthtest: noises info: the clue shot: its trail step %g, clue range %g, he %.0f from it, it %.0f\n",
                m.stl_trail, cvar_hget(cvarh_vr_stealth_hunt_clue), vlen(stt4_d - p.origin), vlen(stt4_d - m.origin)));
            return;
        }
        if(stt4_nt <= 0) { return; }
        vector dd = m.origin - stt4_d;
        stt4_minD = min(stt4_minD, vlen('1 0 0' * dd_x + '0 1 0' * dd_y));
        if(m.goalentity == p && m.stl_trail > 0) { stt4_omni = stt4_omni + 1; }
        if(m.enemy == p && stt4_minD > 64 && time - stt4_nt < 20) { return; }
        float steps = vr_nav_trail_steps - stt4_steps0;
        VR_STT2_Report(nm, m.enemy == p && vlen(m.stl_lost_at - stt4_d) < 1 && stt4_minD <= 64 && !stt4_omni && steps > 0,
            sprintf("its trail at %.0f from the shot, it came within %.0f of it in %.1f s; %g navmesh steps; goal him %g frames; enemy %s",
                vlen(m.stl_lost_at - stt4_d), stt4_minD, time - stt4_nt, steps, stt4_omni, m.enemy.classname));
        bprint(sprintf("stealthtest: noises done, %g failed\n", stt2_fails));
        VR_STT4_End();
        return;
    }
    if(t < 1)
    {
        return;
    }
    if(stt4_nt == 0)
    {
        // (both put down Idle at their spots again, the far one's start run by now)
        setorigin(m, stt4_mpos);
        VR_STT2_Calm(m, vectoyaw(stt2_dir));
        if(stt2_m2 != world)
        {
            setorigin(stt2_m2, stt4_fpos);
            VR_STT2_Calm(stt2_m2, stt2_m2.angles_y);
        }
        stt4_nt = -2;
        return;
    }
    if(stt4_nt == -2)
    {
        if(t < 1.5) { return; }
        stt4_nt = time;
        VR_Stealth_Noise(stt4_d, stt4_r, world, mk);
        return;
    }
    vector dn = m.origin - stt4_d;
    stt4_minD = min(stt4_minD, vlen('1 0 0' * dn_x + '0 1 0' * dn_y));
    if(m.stl_state == STL_ALERT) { stt4_trail = 1; }
    float far = stt2_m2 != world && (stt2_m2.stl_state != STL_IDLE || stt2_m2.stl_heard >= stt4_nt || stt2_m2.enemy != world);
    float back = stt4_trail && m.stl_state == STL_IDLE && m.enemy == world && vlen(m.origin - stt4_mpos) < 48;
    if(!back && m.enemy == world && time - stt4_nt < 90) { return; }
    float nsteps = vr_nav_trail_steps - stt4_steps0;
    VR_STT2_Report(nm, back && stt4_minD <= 64 && m.stl_scouted >= 1 && stt2_m2 != world && !far,
        strcat(sprintf("came within %.0f of the noise, scouted %g spots, back %.0f off its spot after %.1f s (state %g, enemy %s); ",
            stt4_minD, m.stl_scouted, vlen(m.origin - stt4_mpos), time - stt4_nt, m.stl_state, m.enemy.classname),
            sprintf("%g navmesh steps; the far grunt %s", nsteps, stt2_m2 == world ? "none" : (far ? "heard it" : "Idle, didn't hear"))));
    stt4_nt = 0;
    VR_STT4_NoiseTrial(p, stt4_trial + 1);
};

void() VR_STT4_Frame =
{
    entity p = stt2_p;
    float t = time - stt2_t0;
    entity m = stt2_m1;

    // 120: a see-through wall.
    if(stt2_step == 120)
    {
        if(stt2_stage == 0)
        {
            stt2_dir = VR_STT4_ClearAxis(p.origin, 320);
            if(stt2_dir == '0 0 0')
            {
                VR_STT2_Report("seethrough_wall", FALSE, "no room");
                VR_STT4_End();
                return;
            }
            cvar_set("vr_stealth_share_near", "0");
            cvar_set("vr_stealth_share_view", "0");
            cvar_set("vr_stealth_light_force", "128");
            cvar_set("vr_stealth_seethrough", "1");
            stt2_m1 = VR_STT2_Spawn(p.origin + stt2_dir * 300 + '0 0 8', vectoyaw(-stt2_dir), 0);
            // (the wall across the line: *10 is 14 units thick along x, 126 wide and tall; *29 along y, 142 by 110)
            stt4_wall = spawn();
            stt4_wall.classname = "stealthtest_wall";
            stt4_wall.solid = SOLID_BSP;
            stt4_wall.movetype = MOVETYPE_PUSH;
            setmodel(stt4_wall, stt2_dir_y != 0 ? "*29" : "*10");
            stt4_alpha = 0;
            stt2_v = 0;
            VR_STT2_Stage(1);
            return;
        }
        // 1 opaque, 2 alpha 0.5, 3 alpha 0.5 with the switch off, 4 opaque but a fence (its textures taken for '{'
        // ones), 5 not: 3 s each, the grunt calm at the start
        if(stt2_stage >= 1 && stt2_stage <= 5)
        {
            if(t < 0.1)
            {
                // (across the line between their eyes, the grunt landed)
                vector mid = (p.origin + p.view_ofs + m.origin + m.view_ofs) * 0.5;
                setorigin(stt4_wall, mid - (stt4_wall.mins + stt4_wall.maxs) * 0.5);
                stt4_wall.alpha = (stt2_stage == 2 || stt2_stage == 3) ? 0.5 : 0;
                cvar_set("vr_stealth_seethrough", stt2_stage == 3 ? "0" : "1");
                cvar_set("vr_stealth_test_fence", stt2_stage == 4 ? "tech0 key01" : "");
                VR_STT2_Calm(m, m.angles_y);
                stt2_w = 0;
                return;
            }
            if(t >= 0.5 && !stt2_w)
            {
                stt2_w = 1;
                stt2_x = m.stl_heard;
                // a knock where he stands, heard (in the open) a little farther than the grunt is
                VR_Stealth_Noise(p.origin, vlen(m.origin + m.view_ofs - p.origin) * 1.15, world, STL_N_PROP);
            }
            if(t < 3)
            {
                return;
            }
            float saw = VR_STT4_Saw(m);
            float heard = m.stl_heard > stt2_x;
            float want = stt2_stage == 2 || stt2_stage == 4;
            string name = stt2_stage == 1 ? "seethrough_opaque" : (stt2_stage == 2 ? "seethrough_alpha" :
                (stt2_stage == 3 ? "seethrough_off" : (stt2_stage == 4 ? "seethrough_fence" : "seethrough_nofence")));
            VR_STT2_Report(name, saw == want && heard == want, sprintf("meter %.2f state %g hostile %g heard %g (want %g)",
                m.stl_meter, m.stl_state, m.enemy != world, heard, want));
            VR_STT2_Stage(stt2_stage + 1);
            return;
        }
        bprint("stealthtest: seethrough done\n");
        VR_STT4_End();
        return;
    }

    // 124: props in the way (vrteleporters, room T: the script loads it).
    if(stt2_step == 124)
    {
        if(stt2_stage == 0 || stt2_stage == 2)
        {
            cvar_set("vr_stealth_light_force", "128");
            cvar_set("vr_ai_props", stt2_stage == 0 ? "1" : "0");
            VR_STT2_RemoveAll();
            setorigin(p, '-1030 530 24');
            p.velocity = '0 0 0';
            stt2_phome = p.origin;
            // (the row across the room's middle, 44 units apart: large crates; again for the second run, where they stood)
            VR_STT4_Crates();
            {
                for(float k = -2; k <= 2; k++)
                {
                    entity os = self;
                    entity c = spawn();
                    c.origin = '-1290 530 40' + '0 44 0' * k;
                    c.spawnflags = 1;
                    c.classname = "stealthtest_crate";
                    self = c;
                    vr_crate();
                    self = os;
                }
            }
            stt2_m1 = VR_STT2_Spawn('-1540 530 32', 0, 5);
            if(!stt2_m1)
            {
                VR_STT2_Report("props", FALSE, "no knight");
                VR_STT4_End();
                return;
            }
            stt2_m1.health = 5000;
            stt2_v = 99999;
            VR_STT2_Stage(stt2_stage + 1);
            return;
        }
        if(stt2_stage == 1 || stt2_stage == 3)
        {
            if(t < 0.1)
            {
                VR_Stealth_Spot(m, p);
                return;
            }
            stt2_v = min(stt2_v, vlen(m.origin - p.origin));
            if(stt2_v > 80 && t < 15)
            {
                return;
            }
            if(stt2_stage == 1)
            {
                VR_STT2_Report("props_round", stt2_v <= 80 && t < 6, sprintf("vr_ai_props 1: the knight came within %.0f units in %.1f s",
                    stt2_v, t));
            }
            else
            {
                bprint(sprintf("stealthtest: props_quake vr_ai_props 0: the knight came within %.0f units in %.1f s (for comparison)\n",
                    stt2_v, t));
            }
            VR_STT2_Stage(stt2_stage + 1);
            if(stt2_stage == 4)
            {
                bprint("stealthtest: props done\n");
                cvar_set("vr_ai_props", "1");
                VR_STT4_Crates();
                VR_STT4_End();
            }
            return;
        }
        VR_STT4_End();
        return;
    }

    // 123: a dog chasing him as he runs in circles (the drawn moves are the engine's log).
    if(stt2_step == 123)
    {
        if(stt2_stage == 0)
        {
            stt2_dir = VR_STT4_ClearAxis(p.origin, 320);
            if(stt2_dir == '0 0 0')
            {
                VR_STT2_Report("dog_steps", FALSE, "no room");
                VR_STT4_End();
                return;
            }
            cvar_set("vr_stealth_light_force", "128");
            stt2_m1 = VR_STT2_Spawn(p.origin + stt2_dir * 300 + '0 0 8', vectoyaw(-stt2_dir), 7);
            if(!stt2_m1)
            {
                VR_STT2_Report("dog_steps", FALSE, "no dog");
                VR_STT4_End();
                return;
            }
            stt2_m1.health = 5000; // (his blows don't land: he only runs)
            VR_Stealth_Spot(stt2_m1, p);
            VR_STT2_Stage(1);
            return;
        }
        if(stt2_stage == 1)
        {
            if(t < 30)
            {
                // round a circle 280 units across about his first spot (round once every 1.8 s), pulled back to it
                float a = t * 200;
                vector want = stt2_phome + '1 0 0' * (140 * cos(a * (3.14159 / 180))) + '0 1 0' * (140 * sin(a * (3.14159 / 180)));
                vector v = '-1 0 0' * sin(a * (3.14159 / 180)) + '0 1 0' * cos(a * (3.14159 / 180));
                v = v * 250 + (want - p.origin) * 4;
                v_z = p.velocity_z;
                p.velocity = v;
                return;
            }
            bprint(sprintf("stealthtest: dog_steps the dog %s, %.0f units off\n", stt2_m1.health > 0 ? "alive" : "dead",
                vlen(stt2_m1.origin - p.origin)));
            bprint("stealthtest: dogs done\n");
            VR_STT4_End();
            return;
        }
        VR_STT4_End();
        return;
    }

    // 125: his trail by the navmesh, not him.
    if(stt2_step == 125)
    {
        VR_STT4_TrailNav(p, m, t);
        return;
    }

    // 126: the trail carries on past the corner; 127: noises investigated.
    if(stt2_step == 126)
    {
        VR_STT4_Corner(p, m, t);
        return;
    }
    if(stt2_step == 127)
    {
        VR_STT4_Noises(p, m, t);
        return;
    }

    // 122: the hunt.
    if(stt2_step == 122)
    {
        if(stt2_stage == 0)
        {
            stt2_dir = VR_STT4_ClearAxis(p.origin, 320);
            if(stt2_dir == '0 0 0')
            {
                VR_STT2_Report("hunt", FALSE, "no room");
                VR_STT4_End();
                return;
            }
            cvar_set("vr_stealth_share_near", "0");
            cvar_set("vr_stealth_share_view", "0");
            cvar_set("vr_stealth_light_force", "128");
            cvar_set("vr_stealth_lose_time", "8");
            cvar_set("vr_stealth_lose_far", "0");
            cvar_set("vr_stealth_chase_trail", "1");
            cvar_set("vr_stealth_chase_predict", "1.5");
            stt2_m1 = VR_STT2_Spawn(p.origin + stt2_dir * 300 + '0 0 8', vectoyaw(-stt2_dir), 0);
            // (he darts a way with room for his box: to a side, back, or on)
            vector side = '-1 0 0' * stt2_dir_y + '0 1 0' * stt2_dir_x;
            stt4_side = '0 0 0';
            for(float k = 0; k < 4 && stt4_side == '0 0 0'; k++)
            {
                vector w = k == 0 ? side : (k == 1 ? -side : (k == 2 ? -stt2_dir : stt2_dir));
                tracebox(p.origin, '-16 -16 -16', '16 16 32', p.origin + w * 200, TRUE, p);
                if(trace_fraction >= 1)
                {
                    stt4_side = w;
                }
            }
            if(stt4_side == '0 0 0')
            {
                VR_STT2_Report("hunt", FALSE, "no room to dart");
                VR_STT4_End();
                return;
            }
            VR_STT2_Stage(1);
            return;
        }
        // 1: spotted, he darts sideways (1.0-1.4 s), out of sight at once
        if(stt2_stage == 1)
        {
            if(t < 0.1)
            {
                VR_STT2_Calm(m, m.angles_y);
                VR_Stealth_Spot(m, p);
                return;
            }
            if(t < 1.4)
            {
                if(t >= 1)
                {
                    p.velocity = stt4_side * 300;
                }
                return;
            }
            // (out of sight while still darting: round a corner at a run)
            stt2_at = p.origin;
            if(m.enemy != p || VR_STT4_Hide(p, 256) == '0 0 0')
            {
                VR_STT2_Report("hunt", FALSE, sprintf("not hunting him (enemy %s) or no place out of sight", m.enemy.classname));
                VR_STT4_End();
                return;
            }
            stt4_hid = time;
            stt4_trail = 0;
            stt4_near = 99999;
            stt4_ahead = '0 0 0';
            VR_STT2_Stage(2);
            return;
        }
        // 2: hunting him out of sight; it gives up at 8 s
        if(stt2_stage == 2)
        {
            if(m.enemy == p)
            {
                if(m.stl_trail > stt4_trail)
                {
                    stt4_trail = m.stl_trail;
                    if(stt4_trail == 2)
                    {
                        stt4_ahead = m.stl_marker.origin;
                    }
                }
                stt4_near = min(stt4_near, vlen((m.origin - m.stl_lost_at) - '0 0 1' * (m.origin_z - m.stl_lost_at_z)));
                if(t < 20)
                {
                    return;
                }
            }
            float gave = time - m.stl_lost_time;
            float aheadWay = (stt4_ahead - m.stl_lost_at) * stt4_side;
            VR_STT2_Report("hunt_trail", stt4_near < 64 && stt4_trail >= 2 && aheadWay > 150,
                sprintf("came within %.0f of where it saw him last; trail step %g; its guess %.0f units on along his way",
                    stt4_near, stt4_trail, aheadWay));
            VR_STT2_Report("hunt_gives_up", m.enemy == world && gave > 7.6 && gave < 8.6 && m.stl_state == STL_ALERT,
                sprintf("gave up %.2f s after it last saw him (8), state %g", gave, m.stl_state));
            VR_STT4_Unhide(p);
            cvar_set("vr_stealth_lose_far", "600");
            VR_STT2_Stage(3);
            return;
        }
        // 3: again, but he goes far (over 600 units): it gives up after a quarter of the time
        if(stt2_stage == 3)
        {
            if(t < 0.1)
            {
                VR_STT2_Calm(m, m.angles_y);
                VR_Stealth_Spot(m, p);
                return;
            }
            if(t < 1)
            {
                return;
            }
            if(m.enemy != p || VR_STT4_Hide(p, 900) == '0 0 0')
            {
                VR_STT2_Report("hunt_far", FALSE, "not hunting him or no place far out of sight");
                VR_STT4_End();
                return;
            }
            VR_STT2_Stage(4);
            return;
        }
        if(stt2_stage == 4)
        {
            if(m.enemy == p && t < 10)
            {
                return;
            }
            float gave2 = time - m.stl_lost_time;
            VR_STT2_Report("hunt_far", m.enemy == world && gave2 > 1.6 && gave2 < 2.6,
                sprintf("him %.0f units off: gave up %.2f s after it last saw him (2)", vlen(p.origin - m.origin), gave2));
            bprint("stealthtest: hunt done\n");
            VR_STT4_End();
            return;
        }
        VR_STT4_End();
        return;
    }

    VR_STT4_End();
};
#endif
