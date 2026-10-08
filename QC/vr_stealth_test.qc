// vr_stealth_test.qc -- `vr_stealth_test 1`: the stealth AI's scenes on the monster nearest the player (a grunt put
// ahead of him: `god; vr_test_spawn 0; vr_test_spawn_dist 150; impulse 241`), one after the other, each printing
// `stealthtest: <name> PASS|FAIL <numbers>` (docs/vr-port/STEALTH_PLAN.md, "Tests"). Debug > Tests > Stealth AI.
// `vr_stealth_test 2`: only the crate drop (a real prop's knock through the physics sounds).

entity stt_m;      // the monster
entity stt_p;      // the player
vector stt_home;   // the monster's place and yaw (facing the player)
float stt_yaw;
vector stt_phome;  // the player's place
float stt_step;    // the scene and its stage (0: none running)
float stt_stage;
float stt_t0;      // when the stage began
float stt_n;       // a scene's number
vector stt_at;     // a scene's point
float stt_v;       // a scene's value
float stt_fails;
float stt_tick;    // a scene's next progress line
entity stt_gib;    // the body scene's gib

void(string xName, float xPass, string xInfo) VR_STT_Report =
{
    if(!xPass)
    {
        stt_fails = stt_fails + 1;
    }
    bprint(sprintf("stealthtest: %s %s %s\n", xName, xPass ? "PASS" : "FAIL", xInfo));
};

// The monster back at its place, Idle, facing the player (or away: xAway), healthy, the meter empty.
void(float xAway) VR_STT_Reset =
{
    entity m = stt_m;
    m.enemy = m.oldenemy = m.goalentity = m.movetarget = world;
    m.stl_state = STL_IDLE;
    m.stl_phase = 0;
    m.stl_meter = 0;
    m.stl_wary = 0;
    m.stl_lit = 0;
    m.stl_settle = 0;
    m.stl_body = world;
    m.health = 500;
    m.velocity = '0 0 0';
    setorigin(m, stt_home);
    m.angles = '0 1 0' * (xAway ? anglemod(stt_yaw + 180) : stt_yaw);
    m.ideal_yaw = m.angles_y;
    m.attack_finished = time + 1;
    m.pain_finished = 0;
    entity os = self;
    self = m;
    droptofloor();
    self.flags = self.flags | FL_ONGROUND;
    self.pausetime = 99999999;
    self.th_stand();
    self = os;
    stt_p.velocity = '0 0 0';
    setorigin(stt_p, stt_phome);
    sight_entity = world;
    sight_entity_time = -100;
    cvar_set("vr_stealth_light_force", "0");
};

void(float xStage) VR_STT_Stage =
{
    stt_stage = xStage;
    stt_t0 = time;
};

void(float xStep) VR_STT_Next =
{
    stt_step = xStep;
    VR_STT_Stage(0);
};

// A point stt_v units from the monster's eyes, behind a wall in line (traced blocked) and out of the wall: the first
// direction (of 16) that has one.
float() VR_STT_FindWalled =
{
    vector eye = stt_m.origin + stt_m.view_ofs;
    for(float a = 0; a < 360; a = a + 22.5)
    {
        makevectors('0 1 0' * a);
        traceline(eye, eye + v_forward * stt_v, TRUE, stt_m);
        if(trace_fraction >= 1 || trace_fraction * stt_v > stt_v - 40)
        {
            continue;
        }
        vector p = eye + v_forward * stt_v;
        if(pointcontents(p) != CONTENT_EMPTY)
        {
            continue;
        }
        stt_at = p;
        return TRUE;
    }
    return FALSE;
};

void() VR_Stealth_TestFrame =
{
    float ask = cvar("vr_stealth_test");
    if(ask >= 100)
    {
        cvar_set("vr_stealth_test", "0");
        VR_STT2_Start(ask); // (the further scenes: vr_stealth_test2.qc)
        ask = 0;
    }
    if(stt2_step)
    {
        VR_STT2_Frame();
    }
    if(ask > 0)
    {
        cvar_set("vr_stealth_test", "0");
        stt_p = find(world, classname, "player");
        stt_m = world;
        float nearest = 1000;
        for(entity e = findflags(world, flags, FL_MONSTER); e; e = findflags(e, flags, FL_MONSTER))
        {
            if(e.health > 0 && vlen(e.origin - stt_p.origin) < nearest)
            {
                nearest = vlen(e.origin - stt_p.origin);
                stt_m = e;
            }
        }
        if(stt_m == world)
        {
            bprint("stealthtest: no monster near\n");
            return;
        }
        stt_home = stt_m.origin;
        stt_phome = stt_p.origin;
        stt_yaw = vectoyaw(stt_p.origin - stt_m.origin);
        stt_fails = 0;
        bprint(sprintf("stealthtest: %s at %v, player at %v (%.0f units), light there %.1f\n", stt_m.classname, stt_m.origin,
            stt_p.origin, nearest, stealthlight(stt_p.origin)));
        VR_STT_Next(ask == 2 ? 60 : (ask >= 3 ? ask - 2 : 1));
    }
    if(!stt_step || stt_m == world)
    {
        return;
    }
    entity m = stt_m;
    entity p = stt_p;
    float t = time - stt_t0;
    entity os = self;

    // 1: dark, still, in front of it: stays Idle.
    if(stt_step == 1)
    {
        if(stt_stage == 0) { VR_STT_Reset(FALSE); VR_STT_Stage(1); return; }
        if(t < 4) { return; }
        VR_STT_Report("dark_in_front", m.enemy == world && m.stl_meter == 0, sprintf("enemy %s meter %.2f state %g", m.enemy.classname, m.stl_meter, m.stl_state));
        VR_STT_Next(2);
        return;
    }
    // 2: lit: the meter fills, Hostile.
    if(stt_step == 2)
    {
        if(stt_stage == 0) { VR_STT_Reset(FALSE); cvar_set("vr_stealth_light_force", "128"); VR_STT_Stage(1); stt_v = -1; return; }
        if(m.enemy == p && stt_v < 0) { stt_v = t; }
        if(t < 4) { return; }
        VR_STT_Report("lit_in_front", m.enemy == p, sprintf("hostile after %.2f s", stt_v));
        VR_STT_Next(3);
        return;
    }
    // 3: half lit: a glimpse (Alert) before Hostile.
    if(stt_step == 3)
    {
        if(stt_stage == 0) { VR_STT_Reset(FALSE); cvar_set("vr_stealth_light_force", "45"); VR_STT_Stage(1); stt_v = -1; return; }
        if(m.stl_state == STL_ALERT && stt_v < 0) { stt_v = t; }
        if(t < 6) { return; }
        VR_STT_Report("half_lit_glimpse", stt_v >= 0, sprintf("alert after %.2f s, then %s (meter %.2f)", stt_v, m.enemy == p ? "hostile" : "not hostile", m.stl_meter));
        VR_STT_Next(4);
        return;
    }
    // 4: dark, behind it, he runs (his running steps' noise; it sees him: Hostile).
    if(stt_step == 4)
    {
        if(stt_stage == 0) { VR_STT_Reset(TRUE); VR_STT_Stage(1); return; }
        if(stt_stage == 1 && t > 0.5) { VR_Stealth_Noise(p.origin, cvar_hget(cvarh_vr_stealth_noise_run), p, STL_N_STEP); VR_STT_Stage(2); return; }
        if(stt_stage == 2 && t > 1)
        {
            VR_STT_Report("run_noise_behind", m.enemy == p, sprintf("enemy %s", m.enemy.classname));
            VR_STT_Next(5);
        }
        return;
    }
    // 5: a running step's noise 300 units off, clear air (not him: a prop's knock as loud): Alert; the same behind a
    // wall: Idle.
    if(stt_step == 5)
    {
        if(stt_stage == 0)
        {
            VR_STT_Reset(FALSE);
            stt_v = 300;
            if(!VR_STT_FindWalled()) { VR_STT_Report("noise_wall", FALSE, "no walled point found"); VR_STT_Next(6); return; }
            VR_Stealth_Noise(stt_at, 400, world, STL_N_PROP);
            VR_STT_Stage(1);
            return;
        }
        if(stt_stage == 1 && t > 1)
        {
            float walled = m.stl_state;
            float pvs = pvsvisible(m.origin + m.view_ofs, stt_at);
            VR_STT_Reset(FALSE);
            makevectors('0 1 0' * stt_yaw);
            vector open = m.origin + m.view_ofs + v_forward * 300;
            traceline(m.origin + m.view_ofs, open, TRUE, m);
            stt_at = trace_endpos;
            VR_Stealth_Noise(stt_at, 400, world, STL_N_PROP);
            stt_n = walled;
            stt_v = pvs;
            VR_STT_Stage(2);
            return;
        }
        if(stt_stage == 2 && t > 1)
        {
            VR_STT_Report("noise_wall", stt_n == STL_IDLE && m.stl_state == STL_ALERT, sprintf("behind the wall: state %g (pvs %g); in the open (%.0f units): state %g",
                stt_n, stt_v, vlen(stt_at - m.origin), m.stl_state));
            VR_STT_Next(6);
        }
        return;
    }
    // 6: touching it in the dark: Hostile.
    if(stt_step == 6)
    {
        if(stt_stage == 0) { VR_STT_Reset(TRUE); VR_STT_Stage(1); return; }
        if(stt_stage == 1 && t > 0.3)
        {
            vector d = normalize(stt_phome - m.origin);
            d_z = 0;
            setorigin(p, m.origin + normalize(d) * (m.maxs_x - p.mins_x + 2) + '0 0 1' * (p.origin_z - m.origin_z));
            VR_STT_Stage(2);
            return;
        }
        if(stt_stage == 2 && t > 0.5)
        {
            VR_STT_Report("touch", m.enemy == p, sprintf("enemy %s, %.0f units apart", m.enemy.classname, vlen(p.origin - m.origin)));
            VR_STT_Next(7);
        }
        return;
    }
    // 7: used by the map (a trigger's monster_use) in the dark: Hostile at once (Quake's).
    if(stt_step == 7)
    {
        if(stt_stage == 0) { VR_STT_Reset(TRUE); VR_STT_Stage(1); return; }
        if(stt_stage == 1 && t > 0.3)
        {
            self = m;
            activator = p;
            monster_use();
            self = os;
            VR_STT_Stage(2);
            return;
        }
        if(stt_stage == 2 && t > 0.4)
        {
            VR_STT_Report("map_trigger_wakes", m.enemy == p, sprintf("enemy %s", m.enemy.classname));
            VR_STT_Next(8);
        }
        return;
    }
    // 8: a sneak hit (Idle) against a hit on it Hostile: vr_stealth_sneak times the damage.
    if(stt_step == 8)
    {
        if(stt_stage == 0) { VR_STT_Reset(TRUE); VR_STT_Stage(1); return; }
        if(stt_stage == 1 && t > 0.3)
        {
            float h0 = m.health;
            T_Damage(m, p, p, 20);
            float sneak = h0 - m.health;
            h0 = m.health;
            T_Damage(m, p, p, 20);
            float plain = h0 - m.health;
            VR_STT_Report("sneak_attack", plain > 0 && fabs(sneak / plain - cvar_hget(cvarh_vr_stealth_sneak)) < 0.06,
                sprintf("sneak %g, then %g (x%.2f)", sneak, plain, plain > 0 ? sneak / plain : 0));
            VR_STT_Next(9);
        }
        return;
    }
    // 9: a shot's line passing 40 units beside it (from far): Alert; 250 units off: Idle.
    if(stt_step == 9)
    {
        if(stt_stage == 0) { VR_STT_Reset(TRUE); VR_STT_Stage(1); return; }
        if(stt_stage == 1 && t > 0.3)
        {
            makevectors('0 1 0' * stt_yaw);
            vector c = (m.absmin + m.absmax) * 0.5;
            VR_Stealth_Graze(c - v_right * 250 - v_forward * 2000, c - v_right * 250 + v_forward * 600, p, world);
            stt_n = m.stl_state;
            VR_Stealth_Graze(c - v_right * 40 - v_forward * 2000, c - v_right * 40 + v_forward * 600, p, world);
            VR_STT_Report("graze", stt_n == STL_IDLE && m.stl_state == STL_ALERT, sprintf("250 units off: state %g; 40 off: state %g", stt_n, m.stl_state));
            VR_STT_Next(10);
        }
        return;
    }
    // 10: a body in front of it: Alert, walks to it.
    if(stt_step == 10)
    {
        if(stt_stage == 0)
        {
            VR_STT_Reset(FALSE);
            makevectors('0 1 0' * stt_yaw);
            entity g = spawn();
            setmodel(g, "progs/gib1.mdl");
            setsize(g, '-4 -4 -4', '4 4 4');
            traceline(m.origin, m.origin + v_forward * 120, TRUE, m);
            setorigin(g, trace_endpos - v_forward * 16);
            g.stl_notice = STL_NOTICE_BODY;
            stt_gib = g;
            g.think = SUB_Remove;
            g.nextthink = time + 20;
            stt_at = g.origin;
            VR_STT_Stage(1);
            return;
        }
        if(t < 1.5) { return; }
        VR_STT_Report("body_seen", m.stl_state == STL_ALERT, sprintf("state %g phase %g point %v (gib at %v)", m.stl_state, m.stl_phase, m.stl_point, stt_at));
        remove(stt_gib);
        VR_STT_Next(11);
        return;
    }
    // 11: the umbrella off mid-alert: back to Quake's AI (Idle, no investigation); in the dark in front: Quake's
    // sight (Hostile); on again: dark is safe again.
    if(stt_step == 11)
    {
        if(stt_stage == 0)
        {
            VR_STT_Reset(TRUE);
            VR_Stealth_Alert(m, stt_phome + '0 0 0', STL_A_GRAZE, 2, FALSE);
            stt_n = m.stl_state;
            cvar_set("vr_ai_enhanced", "0");
            VR_STT_Stage(1);
            return;
        }
        if(stt_stage == 1 && t > 0.2)
        {
            stt_v = m.stl_state;
            VR_STT_Reset(FALSE);
            VR_STT_Stage(2);
            return;
        }
        if(stt_stage == 2 && t > 2)
        {
            stt_at_x = m.enemy == p;
            cvar_set("vr_ai_enhanced", "1");
            VR_STT_Reset(FALSE);
            VR_STT_Stage(3);
            return;
        }
        if(stt_stage == 3 && t > 3)
        {
            VR_STT_Report("umbrella_switch", stt_n == STL_ALERT && stt_v == STL_IDLE && stt_at_x && m.enemy == world,
                sprintf("alert %g -> off: %g; off, dark, in front: hostile %g; on again: enemy %s", stt_n, stt_v, stt_at_x, m.enemy.classname));
            VR_STT_Next(12);
        }
        return;
    }
    // 12: investigating a point 250 units behind it: turns, walks there, searches, comes back.
    if(stt_step == 12)
    {
        if(stt_stage == 0)
        {
            VR_STT_Reset(FALSE);
            makevectors('0 1 0' * stt_yaw);
            traceline(m.origin, m.origin - v_forward * 250, TRUE, m);
            stt_at = trace_endpos + v_forward * 24;
            stt_v = 9999;
            stt_n = 0;
            VR_Stealth_Alert(m, stt_at, STL_N_PROP, 1, FALSE);
            VR_STT_Stage(1);
            return;
        }
        float d2 = vlen(m.origin - stt_at);
        if(stt_stage == 1)
        {
            if(cvar("vr_stealth_debug") && time >= stt_n) { stt_n = time + 0.5; bprint(sprintf("stealthtest: t %.1f phase %g at %v yaw %.0f ideal %.0f goal %s flags %g\n", t, m.stl_phase, m.origin, m.angles_y, m.ideal_yaw, m.goalentity.classname, m.flags)); }
            stt_v = min(stt_v, d2);
            if(m.stl_phase == STL_RETURN || m.stl_state == STL_IDLE) { stt_n = t; VR_STT_Stage(2); }
            else if(t > 20) { VR_STT_Report("investigate", FALSE, sprintf("stuck in phase %g, nearest %.0f of %.0f", m.stl_phase, stt_v, vlen(stt_home - stt_at))); VR_STT_Next(13); }
            return;
        }
        if(stt_stage == 2 && time >= stt_tick && m.stl_state != STL_IDLE)
        {
            stt_tick = time + 2;
            bprint(sprintf("stealthtest: returning t %.0f phase %g at %v, %.0f from its post, goal %s at %v\n", t, m.stl_phase,
                m.origin, vlen(m.origin - stt_home), m.goalentity.classname, m.goalentity.origin));
        }
        if(stt_stage == 2 && (m.stl_state == STL_IDLE || t > 40))
        {
            VR_STT_Report("investigate", stt_v < 90 && m.stl_state == STL_IDLE && vlen(m.origin - stt_home) < 40,
                sprintf("came within %.0f of the point (%.0f away), searched until %.1f s, back %.0f from its post (state %g)",
                stt_v, vlen(stt_home - stt_at), stt_n, vlen(m.origin - stt_home), m.stl_state));
            VR_STT_Next(13);
        }
        return;
    }
    // 13: Hostile, then he hides (behind a wall) for 7 s: it gives up the hunt, Alert, to where it saw him last (the
    // hunt as it was: 6 s, no trail: vr_stealth_lose_time 6, vr_stealth_chase_trail 0; test 122 has the trail).
    if(stt_step == 13)
    {
        if(stt_stage == 0)
        {
            cvar_set("vr_stealth_lose_time", "6");
            cvar_set("vr_stealth_lose_far", "0");
            cvar_set("vr_stealth_chase_trail", "0");
            VR_STT_Reset(FALSE);
            self = m;
            self.enemy = p;
            FoundTarget();
            self = os;
            VR_STT_Stage(1);
            return;
        }
        if(stt_stage == 1 && t > 0.5)
        {
            // (far off, out of its sight: a pickup 800 units or more away it can't see)
            entity hide = world;
            for(entity it = findflags(world, flags, FL_ITEM); it; it = findflags(it, flags, FL_ITEM))
            {
                traceline(m.origin + m.view_ofs, it.origin + '0 0 24', TRUE, m);
                if(vlen(it.origin - m.origin) > 800 && trace_fraction < 1)
                {
                    hide = it;
                    break;
                }
            }
            if(!hide) { VR_STT_Report("lose_him", FALSE, "no hiding place found"); VR_STT_Next(14); return; }
            setorigin(p, hide.origin + '0 0 24');
            VR_STT_Stage(2);
            return;
        }
        if(stt_stage == 2 && t > 7.5)
        {
            VR_STT_Report("lose_him", m.enemy == world && m.stl_state == STL_ALERT && vlen(m.stl_point - stt_phome) < 80,
                sprintf("enemy %s, state %g, looking %.0f from where it saw him last", m.enemy.classname, m.stl_state, vlen(m.stl_point - stt_phome)));
            cvar_set("vr_stealth_lose_time", "20");
            cvar_set("vr_stealth_lose_far", "1500");
            cvar_set("vr_stealth_chase_trail", "1");
            VR_STT_Next(14);
        }
        return;
    }
    if(stt_step == 14)
    {
        VR_STT_Reset(FALSE);
        bprint(sprintf("stealthtest: done, %g failed\n", stt_fails));
        stt_step = 0;
        cvar_set("vr_stealth_light_force", "-1");
        return;
    }
    // 20 (vr_stealth_test 22): the player's numbers for 4 s (his speed, light, crouch; the gem's visibility).
    if(stt_step == 20)
    {
        if(stt_stage == 0) { cvar_set("vr_stealth_light_force", "-1"); VR_STT_Stage(1); stt_n = 0; return; }
        if(time >= stt_n)
        {
            stt_n = time + 0.5;
            vector hv = p.velocity;
            hv_z = 0;
            bprint(sprintf("stealthtest: probe t %.1f speed %.0f light %.1f crouch %.2f", t, vlen(hv), VR_Stealth_LightOf(p),
                VR_Stealth_CrouchShare(p)));
            bprint(sprintf(" eye %.1f stand %.1f vis %.2f move %.2f\n", p.headpos_z - p.absmin_z, p.stl_stand_eye, p.stl_vis,
                VR_Stealth_MoveShare(p)));
        }
        if(t > 4) { stt_step = 0; }
        return;
    }
    // 30 (vr_stealth_test 32): the flashlight (on, clipped to the head, aimed at it: the script's) on its back in the
    // dark: Alert at the lens; then facing him, lit by it: Hostile.
    if(stt_step == 30)
    {
        if(stt_stage == 0) { VR_STT_Reset(TRUE); VR_STT_Stage(1); return; }
        if(stt_stage == 1 && t > 1.5)
        {
            vector bm = flashlightbeam(p, 2);
            VR_STT_Report("beam_on_its_back", m.stl_state == STL_ALERT && vlen(m.stl_point - flashlightbeam(p, 0)) < 1,
                sprintf("lamp on %g range %.0f; state %g, point %.0f from the lens", bm_x, bm_y, m.stl_state, vlen(m.stl_point - flashlightbeam(p, 0))));
            bprint(sprintf("stealthtest: lens %v dir %v cos to it %.3f (cone %.3f)\n", flashlightbeam(p, 0), flashlightbeam(p, 1),
                normalize((m.absmin + m.absmax) * 0.5 - flashlightbeam(p, 0)) * flashlightbeam(p, 1), bm_z));
            VR_STT_Reset(FALSE);
            VR_STT_Stage(2);
            return;
        }
        if(stt_stage == 2 && t > 4)
        {
            VR_STT_Report("beam_in_its_face", m.enemy == p, sprintf("enemy %s (the lens's glare in the dark)", m.enemy.classname));
            stt_step = 0;
            cvar_set("vr_stealth_light_force", "-1");
        }
        return;
    }
    // 24 (vr_stealth_test 26): the light where the map's monsters and items are, as a histogram (the thresholds'
    // calibration: vr_stealth_light_dark, vr_stealth_light_bright).
    if(stt_step == 24)
    {
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, b7 = 0, nall = 0;
        for(entity h = nextent(world); h; h = nextent(h))
        {
            if(!(h.flags & (FL_MONSTER | FL_ITEM)))
            {
                continue;
            }
            float l = stealthlight(h.origin + '0 0 8');
            nall = nall + 1;
            if(l < 8) { b0++; } else if(l < 16) { b1++; } else if(l < 32) { b2++; } else if(l < 48) { b3++; }
            else if(l < 64) { b4++; } else if(l < 96) { b5++; } else if(l < 128) { b6++; } else { b7++; }
        }
        bprint(sprintf("stealthtest: light %s: %g places: <8 %g, <16 %g, <32 %g, <48 %g", mapname, nall, b0, b1, b2, b3));
        bprint(sprintf(", <64 %g, <96 %g, <128 %g, more %g\n", b4, b5, b6, b7));
        stt_step = 0;
        return;
    }
    // 60: a crate dropped 200 units behind it: its knock is heard (the physics sounds), it walks to it.
    if(stt_step == 60)
    {
        if(stt_stage == 0)
        {
            if(time < 4.5)
            {
                return; // (a new map's first seconds: props settling, their knocks unheard)
            }
            VR_STT_Reset(FALSE);
            makevectors('0 1 0' * stt_yaw);
            traceline(m.origin, m.origin - v_forward * 200, TRUE, m);
            stt_at = trace_endpos + v_forward * 24;
            stt_gib = VR_Crate_TestSpawn(stt_at, 1, 0, FALSE);
            stt_v = 9999;
            VR_STT_Stage(1);
            return;
        }
        if(stt_stage == 1 && t > 1.5) { physicsblast((stt_gib.absmin + stt_gib.absmax) * 0.5 + '0 0 -6' + v_forward * 24, 300); VR_STT_Stage(2); return; }
        if(stt_stage >= 3) { stt_v = min(stt_v, vlen(m.origin - stt_at)); }
        if(stt_stage == 2 && t > 1.5 && (m.stl_state == STL_ALERT || t > 5)) { stt_n = m.stl_state; stt_at = m.stl_point; VR_STT_Stage(3); return; } // (the crate may fly a while before it lands)
        if(stt_stage == 3 && (t > 45 || m.stl_phase == STL_SEARCH))
        {
            VR_STT_Report("crate_drop", stt_n == STL_ALERT && stt_v < 100, sprintf("state after the knock %g; came within %.0f of the knock it heard (the crate now %.0f from there)", stt_n, stt_v, vlen(stt_gib.origin - stt_at)));
            stt_step = 0;
            cvar_set("vr_stealth_light_force", "-1");
        }
        return;
    }
};
