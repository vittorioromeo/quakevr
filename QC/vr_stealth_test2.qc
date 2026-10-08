// vr_stealth_test2.qc -- the stealth AI's further scenes (`vr_stealth_test <n>` for n 100 and up; vr_stealth_test.qc
// starts them; docs/vr-port/STEALTH_PLAN.md, "Tests"; Misc/quakevr/stealth_tests.sh runs them). They put their own
// monsters down (the firing range dispenser's) and take them away after, each printing `stealthtest: <name> PASS|FAIL`.
//   100 a real shot (the script pulls the trigger): each weapon heard by its loudness, walls absorbing it;
//   101 explosions by their size, walls absorbing them;
//   102 coop (two players on a listen server): the meter on the most suspicious player; each client's light and lamp;
//   103 a horde of idle monsters (vr_stealth_test_count, 60) for the profiler's "stealth" scope (vr_profile);
//   104 / 105 an investigation saved mid-walk, then (after the script's load) carried on to its end;
//   106 every kind of monster the game has (vr_stealth_test_kinds; id1's, the expansions' when installed);
//   107 infighting: a monster hit by another fights it, then is Idle again (the meter, not Quake's instant sight);
//   108 lava or slime ahead of an investigating monster's walk: it stops short.

float stt2_step;
float stt2_stage;
float stt2_t0;
float stt2_fails;
entity stt2_p;
vector stt2_phome;
entity stt2_m1;       // the scene's monsters
entity stt2_m2;
entity stt2_m3;
entity stt2_m4;
float stt2_v;         // a scene's values
float stt2_w;
float stt2_x;
float stt2_r;         // the noise's reach (units)
float stt2_s;         // the scale the scene set (a cvar's)
vector stt2_at;
vector stt2_dir;
vector stt2_mhome;    // where a scene's monster stands
float stt2_kind_i;    // the kinds scene's place in its list
float stt2_tick;

void(string xName, float xPass, string xInfo) VR_STT2_Report =
{
    if(!xPass)
    {
        stt2_fails = stt2_fails + 1;
    }
    bprint(sprintf("stealthtest: %s %s %s\n", xName, xPass ? "PASS" : "FAIL", xInfo));
};

void(float xStage) VR_STT2_Stage =
{
    stt2_stage = xStage;
    stt2_t0 = time;
};

void() VR_STT2_End =
{
    stt2_step = 0;
    cvar_set("vr_stealth_light_force", "-1");
    cvar_set("vr_stealth_share_near", "256");
    cvar_set("vr_stealth_share_view", "1000");
    cvar_set("vr_stealth_noise_guns", "1");
    cvar_set("vr_stealth_noise_blasts", "1");
};

// A monster of the dispenser's kind at `at` facing `yaw` (world: not installed).
entity(vector at, float yaw, float xKind) VR_STT2_Spawn =
{
    entity os = self;
    entity disp = spawn();
    disp.weapon = xKind;
    setorigin(disp, at);
    self = disp;
    activator = world;
    func_enemy_dispenser_use();
    entity e = self;
    self = os;
    remove(disp);
    if(e == disp)
    {
        return world;
    }
    e.angles_y = yaw;
    return e;
};

// A monster put down Idle where it is (after its start): no enemy, nothing noticed, healthy.
void(entity m, float yaw) VR_STT2_Calm =
{
    if(m == world)
    {
        return;
    }
    m.enemy = m.oldenemy = m.goalentity = m.movetarget = world;
    m.stl_state = STL_IDLE;
    m.stl_phase = 0;
    m.stl_meter = 0;
    m.stl_wary = 0;
    m.stl_lit = 0;
    m.stl_settle = 0;
    m.stl_heard = 0;
    m.stl_suspect = world;
    m.health = 500;
    m.velocity = '0 0 0';
    m.angles = '0 1 0' * yaw;
    m.ideal_yaw = yaw;
    m.attack_finished = time + 1;
    m.pain_finished = 0;
    entity os = self;
    self = m;
    self.pausetime = 99999999;
    self.th_stand();
    self = os;
};

void(entity m) VR_STT2_Remove =
{
    if(m != world && m.classname != "")
    {
        if(m.stl_marker != world && m.stl_marker.classname == "stl_marker")
        {
            remove(m.stl_marker);
        }
        remove(m);
    }
};

void() VR_STT2_RemoveAll =
{
    VR_STT2_Remove(stt2_m1);
    VR_STT2_Remove(stt2_m2);
    VR_STT2_Remove(stt2_m3);
    VR_STT2_Remove(stt2_m4);
    stt2_m1 = stt2_m2 = stt2_m3 = stt2_m4 = world;
};

// Room for a monster's box (a grunt's) at `at` (not in a wall).
float(vector at) VR_STT2_Room =
{
    tracebox(at, '-16 -16 -24', '16 16 40', at, TRUE, world);
    return !trace_startsolid && !trace_allsolid && pointcontents(at) == CONTENT_EMPTY;
};

// The longest clear way (a grunt's box fits all along it, up to xMax) from `org` of 16 directions, those more than 90
// degrees off `away` (the gun's aim: its shots and missiles go the other way) first. stt2_dir, its length returned.
float(vector org, vector away, float xMax) VR_STT2_ClearWay =
{
    float best = 0;
    for(float pass = 0; pass < 2 && best < 200; pass++)
    {
        for(float a = 0; a < 360; a = a + 22.5)
        {
            makevectors('0 1 0' * a);
            vector d = v_forward;
            if(pass == 0 && d * away > -0.1)
            {
                continue;
            }
            tracebox(org, '-16 -16 -24', '16 16 40', org + d * xMax, TRUE, world);
            float len = trace_fraction * xMax;
            if(len > best)
            {
                best = len;
                stt2_dir = d;
            }
        }
    }
    return best;
};

// A place for a monster xDist units from `org` with a wall in the line between (not in sight of `org`).
float(vector org, float xDist) VR_STT2_Walled =
{
    for(float a = 0; a < 360; a = a + 11.25)
    {
        makevectors('0 1 0' * a);
        vector at = org + v_forward * xDist;
        traceline(org, at, TRUE, world);
        if(trace_fraction >= 1 || !VR_STT2_Room(at))
        {
            continue;
        }
        stt2_at = at;
        return TRUE;
    }
    return FALSE;
};

// Each monster's way from the noise and its distance as the noise was made (they may move after: a hostile one runs).
vector stt2_way1;
vector stt2_way2;
vector stt2_way3;

vector(entity m, vector org, entity src) VR_STT2_Way =
{
    if(m == world)
    {
        return '0 0 0';
    }
    return '1 0 0' * VR_Stealth_NoisePath(org, m, src) + '0 1 0' * vlen(m.origin + m.view_ofs - org);
};

void(vector org, entity src) VR_STT2_Ways =
{
    stt2_way1 = VR_STT2_Way(stt2_m1, org, src);
    stt2_way2 = VR_STT2_Way(stt2_m2, org, src);
    stt2_way3 = VR_STT2_Way(stt2_m3, org, src);
};

// Whether m heard the scene's noise (since stt2_x, of that kind, from that source) and whether it should have: the
// noise's reach by its way there (xWay: VR_Stealth_NoisePath, the distance) against its distance.
float(entity m, string xName, vector xWay, entity src, float xKind, float xWant) VR_STT2_Heard =
{
    float heard = m.stl_heard >= stt2_x && m.stl_heard_kind == xKind && m.stl_heard_src == src;
    float d = xWay_y;
    float f = xWay_x;
    float reach = stt2_r * VR_Stealth_Hearing(m) * f;
    float should = reach >= d;
    bprint(sprintf("stealthtest:   %s %.0f units off: heard %g (its way x%.2f: reach %.0f; wanted %g)\n", xName, d,
        heard, f, reach, xWant));
    return heard == should && (xWant < 0 || heard == xWant);
};

// The player's shots' missiles (rockets, grenades) taken away at once: only the shot's own noise is heard.
void(entity p) VR_STT2_NoMissiles =
{
    for(entity e = find(world, classname, "missile"); e; e = find(e, classname, "missile"))
    {
        if(e.owner == p) { remove(e); }
    }
    for(entity e = find(world, classname, "grenade"); e; e = find(e, classname, "grenade"))
    {
        if(e.owner == p) { remove(e); }
    }
};

float(float xWeapon) VR_STT2_GunReach =
{
    if(xWeapon == WID_SHOTGUN) { return 1100; }
    if(xWeapon == WID_SUPER_SHOTGUN) { return 1300; }
    if(xWeapon == WID_NAILGUN) { return 800; }
    if(xWeapon == WID_SUPER_NAILGUN) { return 900; }
    if(xWeapon == WID_GRENADE_LAUNCHER || xWeapon == WID_PROXIMITY_GUN) { return 600; }
    if(xWeapon == WID_ROCKET_LAUNCHER || xWeapon == WID_LIGHTNING || xWeapon == WID_LASER_CANNON) { return 900; }
    if(xWeapon == WID_CHAINSAW) { return 700; }
    if(xWeapon == WID_GRAPPLE) { return 250; }
    return 1000;
};

// The list of kinds scene 106 goes through (the dispenser's numbers).
float(float i) VR_STT2_Kind =
{
    float n = cvar("vr_stealth_test_kinds");
    if(n > 0)
    {
        return i == 0 ? n : -1;
    }
    if(i == 0) { return 0; } if(i == 1) { return 1; } if(i == 2) { return 2; } if(i == 3) { return 3; }
    if(i == 4) { return 4; } if(i == 5) { return 5; } if(i == 6) { return 6; } if(i == 7) { return 7; }
    if(i == 8) { return 8; } if(i == 9) { return 9; } if(i == 10) { return 10; } if(i == 11) { return 11; }
    if(i == 12) { return 12; } if(i == 13) { return 13; } if(i == 14) { return 14; } if(i == 15) { return 15; }
    if(i == 16) { return 16; } if(i == 17) { return 17; } if(i == 18) { return 18; } if(i == 19) { return 20; }
    if(i == 20) { return 30; } if(i == 21) { return 31; } if(i == 22) { return 32; } if(i == 23) { return 33; }
    if(i == 24) { return 34; } if(i == 25) { return 35; } if(i == 26) { return 36; }
    return -1;
};

void(float xAsk) VR_STT2_Start =
{
    stt2_p = find(world, classname, "player");
    stt2_phome = stt2_p.origin;
    stt2_fails = 0;
    stt2_step = xAsk;
    stt2_kind_i = 0;
    stt2_v = 0;
    VR_STT2_Stage(0);
};

void() VR_STT2_Frame =
{
    entity p = stt2_p;
    float t = time - stt2_t0;
    entity os = self;
    entity m;

    // 100: a real shot of the main hand's weapon (the script pulls the trigger once the scene says "gun ready"). Three
    // grunts behind him: in the open at 0.8 and 1.2 times the weapon's reach, behind a wall at 0.8 times; the reach
    // scaled down (vr_stealth_noise_guns) to the room the place has.
    if(stt2_step == 100)
    {
        if(stt2_stage == 0)
        {
            makevectors(VRGetWeaponFireRot(cVR_MainHand));
            vector aim = v_forward;
            aim_z = 0;
            float base = VR_STT2_GunReach(p.weapon);
            float room = VR_STT2_ClearWay(p.origin, normalize(aim), 1400);
            stt2_s = min(1, (room - 48) / (1.25 * base));
            cvar_set("vr_stealth_noise_guns", sprintf("%.3f", stt2_s));
            stt2_s = cvar("vr_stealth_noise_guns");
            if(stt2_s < 0.08)
            {
                VR_STT2_Report(sprintf("gun_%g", p.weapon), FALSE, sprintf("no room (%.0f units)", room));
                VR_STT2_End();
                return;
            }
            stt2_r = base * stt2_s;
            cvar_set("vr_stealth_share_near", "0");
            cvar_set("vr_stealth_share_view", "0");
            cvar_set("vr_stealth_light_force", "0");
            float yaw = vectoyaw(-stt2_dir);
            stt2_m1 = VR_STT2_Spawn(p.origin + stt2_dir * 0.8 * stt2_r + '0 0 8', yaw, 0);
            stt2_m2 = VR_STT2_Spawn(p.origin + stt2_dir * 1.2 * stt2_r + '0 0 8', yaw, 0);
            stt2_m3 = world;
            if(VR_STT2_Walled(p.origin, 0.8 * stt2_r))
            {
                stt2_m3 = VR_STT2_Spawn(stt2_at + '0 0 8', yaw, 0);
            }
            stt2_w = p.stl_shot_time;
            bprint(sprintf("stealthtest: gun %g: reach %.0f x%.2f = %.0f, room %.0f\n", p.weapon, base, stt2_s, stt2_r, room));
            VR_STT2_Stage(1);
            return;
        }
        if(stt2_stage == 1 && t > 0.5)
        {
            VR_STT2_Calm(stt2_m1, stt2_m1.angles_y);
            VR_STT2_Calm(stt2_m2, stt2_m2.angles_y);
            VR_STT2_Calm(stt2_m3, stt2_m3.angles_y);
            stt2_x = time;
            bprint("stealthtest: gun ready\n");
            VR_STT2_Stage(2);
            return;
        }
        if(stt2_stage == 2)
        {
            if(p.stl_shot_time > stt2_w)
            {
                stt2_v = time;
                VR_STT2_Ways(p.origin, p);
                VR_STT2_NoMissiles(p);
                VR_STT2_Stage(3);
            }
            else if(t > 5)
            {
                VR_STT2_Report(sprintf("gun_%g", p.weapon), FALSE, "no shot (the trigger pulled?)");
                VR_STT2_RemoveAll();
                VR_STT2_End();
            }
            return;
        }
        if(stt2_stage == 3)
        {
            VR_STT2_NoMissiles(p);
            if(t < 0.3)
            {
                return;
            }
            float loud = p.stl_loud / stt2_s;
            bprint(sprintf("stealthtest:   the shot's noise: reach %.0f (x%.2f: %.0f; the weapon's %.0f)\n", p.stl_loud, stt2_s, loud,
                VR_STT2_GunReach(p.weapon)));
            float ok = fabs(loud - VR_STT2_GunReach(p.weapon)) < 2;
            ok = (VR_STT2_Heard(stt2_m1, "near (0.8)", stt2_way1, p, STL_N_GUN, 1) && ok);
            ok = (VR_STT2_Heard(stt2_m2, "far (1.2)", stt2_way2, p, STL_N_GUN, 0) && ok);
            if(stt2_m3 != world)
            {
                ok = (VR_STT2_Heard(stt2_m3, "behind a wall (0.8)", stt2_way3, p, STL_N_GUN, 0) && ok);
            }
            else
            {
                bprint("stealthtest:   no walled place found\n");
                ok = FALSE;
            }
            VR_STT2_Report(sprintf("gun_%g", p.weapon), ok, sprintf("weapon %g, reach %.0f", p.weapon, loud));
            VR_STT2_RemoveAll();
            VR_STT2_End();
        }
        return;
    }

    // 101: explosions (T_RadiusDamage, as every explosion) of 50 (heard at 800) and 200 (1600) 64 units ahead of him:
    // grunts in the open at 0.8 and 1.2 times the small one's reach, behind a wall at 0.8 times (vr_stealth_noise_blasts
    // scaled to the room).
    if(stt2_step == 101)
    {
        if(stt2_stage == 0)
        {
            float room1 = VR_STT2_ClearWay(p.origin, '0 0 0', 1400);
            stt2_s = min(1, (room1 - 48) / (1.25 * 800));
            cvar_set("vr_stealth_noise_blasts", sprintf("%.3f", stt2_s));
            stt2_s = cvar("vr_stealth_noise_blasts");
            stt2_at = p.origin;
            stt2_r = 800 * stt2_s;
            cvar_set("vr_stealth_share_near", "0");
            cvar_set("vr_stealth_share_view", "0");
            cvar_set("vr_stealth_light_force", "0");
            float yaw1 = vectoyaw(-stt2_dir);
            stt2_m1 = VR_STT2_Spawn(stt2_at + stt2_dir * 0.8 * stt2_r + '0 0 8', yaw1, 0);
            stt2_m2 = VR_STT2_Spawn(stt2_at + stt2_dir * 1.2 * stt2_r + '0 0 8', yaw1, 0);
            stt2_m3 = world;
            vector blastAt = stt2_at;
            if(VR_STT2_Walled(blastAt, 0.8 * stt2_r))
            {
                stt2_m3 = VR_STT2_Spawn(stt2_at + '0 0 8', yaw1, 0);
            }
            stt2_at = blastAt;
            bprint(sprintf("stealthtest: blasts: reach x%.2f, room %.0f\n", stt2_s, room1));
            VR_STT2_Stage(1);
            return;
        }
        if(stt2_stage == 1 && t > 0.5)
        {
            VR_STT2_Calm(stt2_m1, stt2_m1.angles_y);
            VR_STT2_Calm(stt2_m2, stt2_m2.angles_y);
            VR_STT2_Calm(stt2_m3, stt2_m3.angles_y);
            stt2_x = time;
            entity b = spawn();
            setorigin(b, stt2_at);
            VR_STT2_Ways(stt2_at, world);
            T_RadiusDamage(b, world, 50, world);
            remove(b);
            VR_STT2_Stage(2);
            return;
        }
        if(stt2_stage == 2 && t > 0.3)
        {
            stt2_r = 800 * stt2_s;
            float okSmall = VR_STT2_Heard(stt2_m1, "small: near (0.8)", stt2_way1, world, STL_N_BLAST, 1);
            okSmall = (VR_STT2_Heard(stt2_m2, "small: far (1.2)", stt2_way2, world, STL_N_BLAST, 0) && okSmall);
            if(stt2_m3 != world)
            {
                okSmall = (VR_STT2_Heard(stt2_m3, "small: behind a wall (0.8)", stt2_way3, world, STL_N_BLAST, 0) && okSmall);
            }
            VR_STT2_Report("blast_small", okSmall && stt2_m3 != world, sprintf("damage 50, reach %.0f", stt2_r));
            stt2_x = time;
            p.velocity = '0 0 0';
            setorigin(p, stt2_phome);
            entity b2 = spawn();
            setorigin(b2, stt2_at);
            VR_STT2_Ways(stt2_at, world);
            T_RadiusDamage(b2, world, 200, world);
            remove(b2);
            VR_STT2_Stage(3);
            return;
        }
        if(stt2_stage == 3 && t > 0.3)
        {
            stt2_r = 1600 * stt2_s;
            float okBig = VR_STT2_Heard(stt2_m1, "big: near", stt2_way1, world, STL_N_BLAST, 1);
            okBig = (VR_STT2_Heard(stt2_m2, "big: far", stt2_way2, world, STL_N_BLAST, 1) && okBig);
            if(stt2_m3 != world)
            {
                okBig = (VR_STT2_Heard(stt2_m3, "big: behind a wall", stt2_way3, world, STL_N_BLAST, -1) && okBig);
            }
            VR_STT2_Report("blast_big", okBig, sprintf("damage 200, reach %.0f", stt2_r));
            p.velocity = '0 0 0';
            setorigin(p, stt2_phome);
            VR_STT2_RemoveAll();
            VR_STT2_End();
        }
        return;
    }

    // 102: coop (a listen server, a second player in): the light each client measures, each one's lamp; one grunt
    // facing both players 200 units off: one lit, the other dark, then the other way round: it spots the lit one, as
    // fast as one player alone (the meter on the most suspicious player, not shared out between them).
    if(stt2_step == 102)
    {
        entity p2 = find(p, classname, "player");
        if(stt2_stage == 0)
        {
            if(p2 == world)
            {
                VR_STT2_Report("coop", FALSE, "no second player");
                VR_STT2_End();
                return;
            }
            bprint(sprintf("stealthtest: coop: host light %.1f (the host's view %.1f), client light %.1f (the host's view %.1f)\n",
                clientlight(p), stealthlight(p.origin), clientlight(p2), stealthlight(p2.origin)));
            VR_STT2_Report("coop_client_light", clientlight(p) >= 0 && clientlight(p2) >= 0,
                sprintf("host %.1f, client %.1f", clientlight(p), clientlight(p2)));
            vector beam = flashlightbeam(p2, 2);
            vector beam1 = flashlightbeam(p, 2);
            VR_STT2_Report("coop_client_lamp_known", beam_x == 1 && beam_y > 100 && beam1_x == 0,
                sprintf("client's lamp on %g range %.0f cone %.2f; host's on %g", beam_x, beam_y, beam_z, beam1_x));
            // The grunt 200 units ahead of the host, facing him; the client beside him. The client's lamp off meanwhile
            // (the server has his console: its glare would light him, whichever is lit).
            float room2 = VR_STT2_ClearWay(p.origin, '0 0 0', 400);
            stt2_mhome = p.origin + stt2_dir * min(220, room2 - 40) + '0 0 8';
            stt2_m1 = VR_STT2_Spawn(stt2_mhome, vectoyaw(-stt2_dir), 0);
            stt2_at = stt2_dir;
            stuffcmd(p2, "echo MARK lamp toggled by the server;vr_flashlight_toggle\n");
            cvar_set("vr_stealth_light_force", "-1");
            cvar_set("vr_stealth_share_near", "0");
            cvar_set("vr_stealth_share_view", "0");
            VR_STT2_Stage(1);
            stt2_kind_i = 0;
            return;
        }
        m = stt2_m1;
        // (the lights pinned: one lit, the other dark)
        float lit2 = stt2_kind_i == 0;
        p.stl_light = lit2 ? 0 : 128;
        p2.stl_light = lit2 ? 128 : 0;
        p.stl_light_time = p2.stl_light_time = time + 0.5;
        if(stt2_stage == 1 && t > 1)
        {
            if(stt2_kind_i == 0)
            {
                vector beamOff = flashlightbeam(p2, 2);
                VR_STT2_Report("coop_client_lamp_off_known", beamOff_x == 0, sprintf("client's lamp on %g", beamOff_x));
            }
            setorigin(m, stt2_mhome);
            self = m;
            droptofloor();
            self = os;
            makevectors('0 1 0' * vectoyaw(-stt2_at));
            setorigin(p, m.origin - stt2_at * 200 + v_right * 32 + '0 0 8');
            setorigin(p2, m.origin - stt2_at * 200 - v_right * 32 + '0 0 8');
            p.velocity = p2.velocity = '0 0 0';
            VR_STT2_Calm(m, vectoyaw(-stt2_at));
            stt2_v = -1;
            VR_STT2_Stage(2);
            return;
        }
        if(stt2_stage == 2)
        {
            if(m.enemy != world && stt2_v < 0)
            {
                stt2_v = t;
            }
            if(t < 6 && m.enemy == world)
            {
                return;
            }
            entity want = lit2 ? p2 : p;
            VR_STT2_Report(lit2 ? "coop_meter_client_lit" : "coop_meter_host_lit", m.enemy == want && stt2_v > 0 && stt2_v < 4.5,
                sprintf("spotted %s after %.2f s (the lit one: %s)", m.enemy == p ? "the host" : (m.enemy == p2 ? "the client" : "nobody"),
                stt2_v, lit2 ? "the client" : "the host"));
            if(stt2_kind_i == 0)
            {
                stt2_kind_i = 1;
                VR_STT2_Stage(1);
                return;
            }
            stuffcmd(p2, "vr_flashlight_toggle\n"); // (his lamp on again)
            p.stl_light_time = p2.stl_light_time = 0;
            setorigin(p, stt2_phome);
            VR_STT2_Calm(m, m.angles_y);
            VR_STT2_Stage(4);
            return;
        }
        if(stt2_stage == 4)
        {
            p.stl_light_time = p2.stl_light_time = 0;
            if(t < 1)
            {
                return;
            }
            // The client's lamp: the grunt put 160 units down its beam, its back to the lens: Alert at the lens.
            vector lens = flashlightbeam(p2, 0);
            vector dir = flashlightbeam(p2, 1);
            p.stl_light_time = p2.stl_light_time = 0;
            setorigin(p, stt2_phome);
            VR_STT2_Calm(m, vectoyaw(dir));
            traceline(lens, lens + dir * 160, TRUE, p2);
            setorigin(m, trace_endpos - dir * 24 - '0 0 8');
            self = m;
            droptofloor();
            self = os;
            cvar_set("vr_stealth_light_force", "0");
            stt2_at = lens;
            VR_STT2_Stage(3);
            return;
        }
        if(stt2_stage == 3)
        {
            p.stl_light_time = p2.stl_light_time = 0;
            if(t < 1.5)
            {
                return;
            }
            vector beam2 = flashlightbeam(p2, 2);
            VR_STT2_Report("coop_client_lamp_lights_it", m.stl_state == STL_ALERT && vlen(m.stl_point - stt2_at) < 48,
                sprintf("state %g, its point %.0f from the client's lens (lamp on %g)", m.stl_state, vlen(m.stl_point - stt2_at),
                beam2_x));
            bprint(sprintf("stealthtest: done, %g failed\n", stt2_fails));
            if(cvar("vr_stealth_test_quitclient"))
            {
                stuffcmd(p2, "echo MARK told to quit;disconnect;quit\n"); // (stealth_mp_test.sh sets it: its client waits for this)
            }
            VR_STT2_RemoveAll();
            VR_STT2_End();
        }
        return;
    }

    // 103: vr_stealth_test_count (60) grunts round him, Idle, most in sight of him, in a light just over Dark (their
    // meters at work, hardly filling); one in six killed (bodies to notice); a knock near him every 2 s (some walk to
    // it); for 20 s (vr_profile's "stealth" scope).
    if(stt2_step == 103)
    {
        if(stt2_stage == 0)
        {
            float want = cvar("vr_stealth_test_count");
            want = want > 0 ? want : 60;
            float n = 0;
            for(float ring = 80; ring < 1600 && n < want; ring = ring + 56)
            {
                for(float a2 = 0; a2 < 360 && n < want; a2 = a2 + max(5, 3500 / ring))
                {
                    makevectors('0 1 0' * a2);
                    vector at2 = p.origin + v_forward * ring + '0 0 8';
                    if(!VR_STT2_Room(at2))
                    {
                        continue;
                    }
                    traceline(p.origin, at2, TRUE, p);
                    if(trace_fraction < 1 && random() < 0.3)
                    {
                        continue; // (most in his sight)
                    }
                    entity g = VR_STT2_Spawn(at2, a2 + 180, 0);
                    if(g)
                    {
                        g.stl_notice = 77; // (the scene's: taken away after)
                        n++;
                    }
                }
            }
            cvar_set("vr_stealth_light_force", ftos(cvar("vr_stealth_light_dark") + 0.5));
            bprint(sprintf("stealthtest: horde: %g grunts\n", n));
            stt2_v = n;
            stt2_tick = 0;
            VR_STT2_Stage(1);
            return;
        }
        if(stt2_stage == 1 && t > 0.6)
        {
            // (one in six killed where it stands: bodies the others may notice)
            float k = 0;
            for(entity h = findflags(world, flags, FL_MONSTER); h; h = findflags(h, flags, FL_MONSTER))
            {
                if(h.stl_notice == 77)
                {
                    VR_STT2_Calm(h, h.angles_y);
                    h.stl_notice = 78;
                    k++;
                    if(k % 6 == 0)
                    {
                        T_Damage(h, world, world, h.health + 1);
                    }
                }
            }
            bprint("stealthtest: horde ready\n");
            VR_STT2_Stage(2);
            return;
        }
        if(stt2_stage == 2)
        {
            if(time >= stt2_tick)
            {
                // (a knock somewhere near him every 2 s: a few investigate, walking)
                stt2_tick = time + 2;
                makevectors('0 1 0' * random() * 360);
                traceline(p.origin, p.origin + v_forward * 400, TRUE, p);
                VR_Stealth_Noise(trace_endpos - v_forward * 16, 350, world, STL_N_PROP);
            }
            if(t < 20)
            {
                return;
            }
            float awake = 0, alert = 0, idle = 0;
            for(entity h2 = nextent(world); h2; h2 = nextent(h2))
            {
                if(h2.stl_notice != 78)
                {
                    continue;
                }
                if(h2.enemy != world) { awake++; } else if(h2.stl_state == STL_ALERT) { alert++; } else { idle++; }
            }
            bprint(sprintf("stealthtest: horde done: %g idle, %g alert, %g hostile\n", idle, alert, awake));
            for(entity h3 = nextent(world); h3; h3 = nextent(h3))
            {
                if(h3.stl_notice == 78)
                {
                    VR_STT2_Remove(h3);
                }
            }
            VR_STT2_End();
        }
        return;
    }

    // 104: a grunt put Alert at a point 250 units behind it, walking there (the script then saves, waits, loads and runs
    // 105).
    if(stt2_step == 104)
    {
        if(stt2_stage == 0)
        {
            makevectors('0 1 0' * p.v_viewangle_y);
            stt2_m1 = VR_STT2_Spawn(p.origin + v_forward * 150 + '0 0 8', p.v_viewangle_y + 180, 0);
            cvar_set("vr_stealth_light_force", "0");
            VR_STT2_Stage(1);
            return;
        }
        if(stt2_stage == 1 && t > 0.5)
        {
            m = stt2_m1;
            VR_STT2_Calm(m, m.angles_y);
            makevectors(m.angles);
            traceline(m.origin, m.origin - v_forward * 250, TRUE, m);
            VR_Stealth_Alert(m, trace_endpos + v_forward * 24, STL_N_PROP, 1, FALSE);
            VR_STT2_Stage(2);
            return;
        }
        if(stt2_stage == 2 && t > 2)
        {
            m = stt2_m1;
            bprint(sprintf("stealthtest: saveload: before: state %g phase %g at %v, post %v, point %v, goal %s\n", m.stl_state,
                m.stl_phase, m.origin, m.stl_post, m.stl_point, m.goalentity.classname));
            stt2_step = 0; // (the light stays forced dark: the load restores the cvar's as it was)
        }
        return;
    }
    // 105: after the load: the Alert grunt goes on (its walk, search, the way back) to Idle at its post.
    if(stt2_step == 105)
    {
        if(stt2_stage == 0)
        {
            stt2_m1 = world;
            for(entity e = findflags(world, flags, FL_MONSTER); e; e = findflags(e, flags, FL_MONSTER))
            {
                if(e.stl_state == STL_ALERT && e.health > 0)
                {
                    stt2_m1 = e;
                }
            }
            m = stt2_m1;
            if(m == world)
            {
                VR_STT2_Report("saveload", FALSE, "no Alert monster after the load");
                VR_STT2_End();
                return;
            }
            bprint(sprintf("stealthtest: saveload: after: state %g phase %g at %v, post %v, point %v, goal %s (its marker %g)\n",
                m.stl_state, m.stl_phase, m.origin, m.stl_post, m.stl_point, m.goalentity.classname, m.goalentity == m.stl_marker));
            stt2_v = m.stl_phase;
            stt2_w = 9999;
            cvar_set("vr_stealth_light_force", "0");
            VR_STT2_Stage(1);
            return;
        }
        m = stt2_m1;
        stt2_w = min(stt2_w, vlen(m.origin - m.stl_point));
        if(m.stl_state == STL_IDLE || t > 45 || m.health <= 0)
        {
            VR_STT2_Report("saveload", m.stl_state == STL_IDLE && m.enemy == world && vlen(m.origin - m.stl_post) < 40 && stt2_w < 90,
                sprintf("phase after the load %g; came within %.0f of the point; %s %.0f from its post after %.1f s", stt2_v, stt2_w,
                m.stl_state == STL_IDLE ? "idle" : "still alert", vlen(m.origin - m.stl_post), t));
            VR_STT2_RemoveAll();
            VR_STT2_End();
        }
        return;
    }

    // 106: each kind (vr_stealth_test_kinds <n>: that one only): put down 200 units ahead, facing him: the rules apply
    // to it (or it is one they leave out: a boss, a flyer, a swimmer); dark in front: Idle; a knock 160 units to its side:
    // Alert, and its investigation runs; lit: it spots him.
    if(stt2_step == 106)
    {
        float kind = VR_STT2_Kind(stt2_kind_i);
        if(kind < 0)
        {
            bprint(sprintf("stealthtest: kinds done, %g failed\n", stt2_fails));
            VR_STT2_End();
            return;
        }
        if(stt2_stage == 0)
        {
            makevectors('0 1 0' * p.v_viewangle_y);
            setorigin(p, stt2_phome);
            p.velocity = '0 0 0';
            stt2_m1 = VR_STT2_Spawn(stt2_phome + v_forward * 200 + '0 0 16', p.v_viewangle_y + 180, kind);
            if(stt2_m1 == world)
            {
                bprint(sprintf("stealthtest: kind %g: not installed\n", kind));
                stt2_kind_i++;
                return;
            }
            cvar_set("vr_stealth_light_force", "0");
            cvar_set("vr_stealth_share_near", "0");
            cvar_set("vr_stealth_share_view", "0");
            VR_STT2_Stage(1);
            return;
        }
        m = stt2_m1;
        if(m.classname == "" || m.health <= 0)
        {
            VR_STT2_Report(sprintf("kind_%g", kind), FALSE, "it died or went");
            stt2_m1 = world;
            stt2_kind_i++;
            VR_STT2_Stage(0);
            return;
        }
        if(stt2_stage == 1 && t > 0.8)
        {
            VR_STT2_Calm(m, m.angles_y);
            float applies = VR_Stealth_Applies(m);
            if(!applies)
            {
                float fly = (m.flags & (FL_FLY | FL_SWIM)) != 0;
                float dormant = (m.takedamage == DAMAGE_NO || m.solid == SOLID_NOT);
                VR_STT2_Report(sprintf("kind_%g", kind), m.stl_class < 0 || dormant,
                    sprintf("%s: left to Quake's AI (class %g, flyer or swimmer %g, dormant until its map wakes it %g)", m.classname,
                    m.stl_class, fly, dormant));
                VR_STT2_Remove(m);
                stt2_m1 = world;
                stt2_kind_i++;
                VR_STT2_Stage(0);
                return;
            }
            VR_STT2_Stage(2);
            return;
        }
        if(stt2_stage == 2 && t > 2)
        {
            stt2_v = (m.enemy == world && m.stl_state == STL_IDLE);
            makevectors(m.angles);
            traceline(m.origin, m.origin + v_right * 160, TRUE, m);
            stt2_at = trace_endpos - v_right * 16;
            VR_Stealth_Noise(stt2_at, 400, world, STL_N_PROP);
            VR_STT2_Stage(3);
            return;
        }
        if(stt2_stage == 3 && t > 0.5)
        {
            stt2_w = m.stl_state == STL_ALERT ? 1 : 0;
            stt2_x = m.stl_phase;
            VR_STT2_Stage(4);
            return;
        }
        if(stt2_stage == 4 && t > 4)
        {
            stt2_x = max(stt2_x, m.stl_phase);
            VR_STT2_Calm(m, vectoyaw(p.origin - m.origin));
            cvar_set("vr_stealth_light_force", "128");
            stt2_r = -1;
            VR_STT2_Stage(5);
            return;
        }
        if(stt2_stage == 5)
        {
            if(m.enemy == p && stt2_r < 0)
            {
                stt2_r = t;
            }
            if(t < 8 && m.enemy != p)
            {
                return;
            }
            VR_STT2_Report(sprintf("kind_%g", kind), stt2_v && stt2_w && stt2_r > 0,
                sprintf("%s: dark in front idle %g; a knock: alert %g (phase %g); lit: spotted after %.2f s", m.classname, stt2_v,
                stt2_w, stt2_x, stt2_r));
            VR_STT2_Remove(m);
            stt2_m1 = world;
            cvar_set("vr_stealth_light_force", "0");
            stt2_kind_i++;
            VR_STT2_Stage(0);
        }
        return;
    }

    // 107: infighting: grunt 1 shot by grunt 2 fights it (Hostile at a monster: nothing shared); grunt 2 killed: grunt 1
    // goes back to standing, and he in the dark in front of it stays unseen (the meter, not Quake's sight); his shots'
    // stand-in: the grunt's own shots heard by a third (Alert, not Hostile: not his).
    if(stt2_step == 107)
    {
        if(stt2_stage == 0)
        {
            makevectors('0 1 0' * p.v_viewangle_y);
            stt2_m1 = VR_STT2_Spawn(p.origin + v_forward * 220 + '0 0 8', p.v_viewangle_y + 180, 0);
            stt2_m2 = VR_STT2_Spawn(p.origin + v_forward * 220 + v_right * 96 + '0 0 8', p.v_viewangle_y + 180, 0);
            stt2_m3 = VR_STT2_Spawn(p.origin + v_forward * 220 - v_right * 96 + '0 0 8', p.v_viewangle_y + 180, 0);
            cvar_set("vr_stealth_light_force", "0");
            VR_STT2_Stage(1);
            return;
        }
        if(stt2_stage == 1 && t > 0.6)
        {
            VR_STT2_Calm(stt2_m1, stt2_m1.angles_y);
            VR_STT2_Calm(stt2_m2, stt2_m2.angles_y);
            VR_STT2_Calm(stt2_m3, stt2_m3.angles_y);
            stt2_m2.health = 30;
            stt2_x = time;
            T_Damage(stt2_m1, stt2_m2, stt2_m2, 5);
            VR_STT2_Stage(2);
            return;
        }
        if(stt2_stage == 2 && t > 1.5)
        {
            stt2_v = stt2_m1.enemy == stt2_m2;
            stt2_w = stt2_m3.enemy == world;
            T_Damage(stt2_m2, p, p, 1000);
            VR_STT2_Stage(3);
            return;
        }
        if(stt2_stage == 3 && t > 6)
        {
            m = stt2_m1;
            bprint(sprintf("stealthtest:   the third heard %g, kind %g from %s (its state %g)\n", stt2_m3.stl_heard >= stt2_x,
                stt2_m3.stl_heard_kind, stt2_m3.stl_heard_src.classname, stt2_m3.stl_state));
            VR_STT2_Report("infighting", stt2_v && stt2_w && m.enemy == world && m.health > 0,
                sprintf("fought the other %g (the third not drawn in %g); after: enemy %s, state %g, meter %.2f", stt2_v, stt2_w,
                m.enemy.classname, m.stl_state, m.stl_meter));
            VR_STT2_RemoveAll();
            VR_STT2_End();
        }
        return;
    }

    // 108: lava or slime near him (the nearest within 1200 units, found on a grid): a grunt on the dry floor beside it,
    // Alert at a point across it: it stops at the edge (its walk ends; it never stands in it).
    if(stt2_step == 108)
    {
        if(stt2_stage == 0)
        {
            // The map scanned for it a slab a frame (64 units apart; nearest him), then a place beside it.
            if(stt2_v == 0)
            {
                stt2_v = 1;
                stt2_dir = p.origin - '3072 3072 1024'; // (round him: the world's bounds aren't set for QC)
                stt2_x = stt2_dir_x;
                stt2_w = 99999;
                stt2_at = '0 0 0';
                return;
            }
            if(stt2_x <= p.origin_x + 3072)
            {
                for(float gy = stt2_dir_y; gy <= stt2_dir_y + 6144; gy = gy + 64)
                {
                    for(float gz = stt2_dir_z; gz <= stt2_dir_z + 2048; gz = gz + 32)
                    {
                        vector q = '1 0 0' * stt2_x + '0 1 0' * gy + '0 0 1' * gz;
                        float c = pointcontents(q);
                        if((c == CONTENT_LAVA || c == CONTENT_SLIME) && vlen(q - p.origin) < stt2_w &&
                           pointcontents(q + '0 0 40') == CONTENT_EMPTY)
                        {
                            stt2_w = vlen(q - p.origin);
                            stt2_at = q;
                        }
                    }
                }
                stt2_x = stt2_x + 64;
                return;
            }
            if(stt2_w >= 99999)
            {
                VR_STT2_Report("liquid_edge", TRUE, "SKIP: no lava or slime on this map");
                VR_STT2_End();
                return;
            }
            vector pool = stt2_at;
            // Dry floor beside it: walk out from the pool in 16 directions until room for a grunt over a floor that is
            // not liquid.
            float placed = FALSE;
            for(float a3 = 0; a3 < 360 && !placed; a3 = a3 + 22.5)
            {
                makevectors('0 1 0' * a3);
                for(float dd = 48; dd < 400 && !placed; dd = dd + 16)
                {
                    vector at3 = pool + v_forward * dd;
                    at3_z = pool_z;
                    traceline(at3 + '0 0 64', at3 - '0 0 64', TRUE, world);
                    vector floor = trace_endpos;
                    float fc = pointcontents(floor + '0 0 1');
                    if(trace_fraction >= 1 || fc != CONTENT_EMPTY || !VR_STT2_Room(floor + '0 0 26'))
                    {
                        continue;
                    }
                    // (72 units farther back, room all the way: it walks up to the edge first)
                    vector back = floor + '0 0 30' + v_forward * 72;
                    tracebox(floor + '0 0 30', '-16 -16 -24', '16 16 40', back, TRUE, world);
                    float roomBack = trace_fraction >= 1;
                    traceline(back, back - '0 0 64', TRUE, world);
                    roomBack = (roomBack && trace_fraction < 1 && fabs(trace_endpos_z - floor_z) < 4 &&
                        pointcontents(trace_endpos + '0 0 1') == CONTENT_EMPTY);
                    if(!roomBack)
                    {
                        break; // (another way out of the pool)
                    }
                    stt2_dir = floor + '0 0 30';
                    stt2_m1 = VR_STT2_Spawn(back, a3 + 180, 0);
                    stt2_at = pool - v_forward * 96;
                    stt2_at_z = floor_z + 24;
                    placed = TRUE;
                }
            }
            if(!placed)
            {
                VR_STT2_Report("liquid_edge", TRUE, "SKIP: no dry floor found beside the pool");
                VR_STT2_End();
                return;
            }
            bprint(sprintf("stealthtest: liquid (%g) at %v, the grunt at %v, the point across %v\n", pointcontents(pool), pool,
                stt2_m1.origin, stt2_at));
            cvar_set("vr_stealth_light_force", "0");
            stt2_v = FALSE;
            VR_STT2_Stage(1);
            return;
        }
        m = stt2_m1;
        if(stt2_stage == 1 && t > 0.6)
        {
            VR_STT2_Calm(m, m.angles_y);
            VR_Stealth_Alert(m, stt2_at, STL_N_PROP, 1, FALSE);
            VR_STT2_Stage(2);
            return;
        }
        if(stt2_stage == 2)
        {
            float fc2 = pointcontents(m.origin + '0 0 1' * (m.mins_z + 2));
            if(fc2 == CONTENT_LAVA || fc2 == CONTENT_SLIME)
            {
                stt2_v = TRUE;
            }
            if(t < 12 && m.health > 0 && m.stl_phase != STL_SEARCH && m.stl_state == STL_ALERT)
            {
                return;
            }
            VR_STT2_Report("liquid_edge", !stt2_v && m.health > 0, sprintf("in it %g; health %g, phase %g after %.1f s, %.0f from the point, %.0f from the edge's first dry place",
                stt2_v, m.health, m.stl_phase, t, vlen(m.origin - stt2_at), vlen(m.origin - stt2_dir)));
            VR_STT2_RemoveAll();
            VR_STT2_End();
        }
        return;
    }
};
