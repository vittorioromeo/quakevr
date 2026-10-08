// vr_stealth_test4.qc -- the stealth AI's round of the author's notes of 2026-10-08 (`vr_stealth_test 120`-`129`;
// Misc/quakevr/stealth_tests.sh seethrough|fence|chase|props; docs/vr-port/STEALTH_PLAN.md, "Tests"). Each scene prints
// `stealthtest: <name> PASS|FAIL`:
//   120 a see-through wall (e1m1's door *10 or *29 put between a lit player and a grunt 300 units away): opaque, the grunt
//       doesn't see him nor hear a knock at his feet; with alpha 0.5 it sees him (its meter fills) and hears the knock;
//       with vr_stealth_seethrough 0, opaque again;
//       then the wall opaque but its textures taken for fences ('{' ones: vr_stealth_test_fence, the maps the kit has
//       have no solid fence): seen and heard through; the textures not fences: neither;
//   122 the hunt: a grunt Hostile at him sees him dart sideways, then loses him (he is put out of sight under the floor):
//       it follows his trail (to where it saw him last, then ahead along his way), gives up after vr_stealth_lose_time
//       (8 s here) and searches (Alert); then with him far (vr_stealth_lose_far 600) it gives up after a quarter of it;
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
    return m.stl_meter >= cvar("vr_stealth_glimpse") || m.stl_state == STL_ALERT || m.enemy != world;
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
