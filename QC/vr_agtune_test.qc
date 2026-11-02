#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// vr_agtune_test.qc -- the armed humanoids' tuning of his voice notes 2026-11-01 (ROUND23.md, "Armed humanoid tuning
// batch"). `vr_agtune_test 1` (Debug > Tests > Armed Humanoid Tuning), in vrtesthall, the player kept alive, each case
// on its own end condition:
//   fire    an AD Plasma Gun grunt hunting you: its shots no closer than your own interval (AD_PLASMAGUN_REFIRE); a
//           nailgun's still every fire frame (0.1 s);
//   reload  a shotgun's reload, then a double-barrelled shotgun's: the second vr_agrunt_reload_ssg times the first;
//   speed   steps of 10 forward, back and sideways (VR_AG_Walk) at the speed sliders' 1/1/1, 0.5/1/1, 1/0.5/1, 1/1/0.5:
//           each the step times the slider for its direction;
//   swing   a sword grunt's swing at Enemy Melee Speed 0.3, its parry raised as it begins (as a guard just dropped
//           leaves it): its weapon's turn as the same swing without it from 0.2 s in (within 1 degree);
//   cast    an armed death knight with a shotgun casts (its gun's thrust): six fireballs, a tenth of a second apart, six
//           in flight after the last.
// `vr_agtune_test 2` (any map, after loading it: `vr_agrunt_repl 1; skill 3; map e1m2`): the map's own armed humanoids'
// armour: the share wearing it near vr_agarmor_skill<skill> (their rolls read the server's skill as the map spawns).
// `vr_agtune_test 3` (vrtesthall; on its own, two minutes of game time): an armed death knight with a shotgun hunting you:
// his casts against his gun's attacks as they come in play, at vr_agdknight_cast_cooldown as set: no two casts closer than
// it, the gun firing between them.
// Each `agtune: PASS|FAIL <case>: ...`; ends on `agtune: N passed, M failed`.
// ----------------------------------------------------------------------------

float vr_agtt_case;      // the case running (1 fire .. 5 cast; 0 none)
float vr_agtt_phase;     // its step
float vr_agtt_t0;        // its start
float vr_agtt_until;     // its give-up time (a safety net: FAIL "timed out")
entity vr_agtt_g;        // the armed humanoid of the case
entity vr_agtt_pl;       // the player
float vr_agtt_shots;     // fire: its gun's shots seen
float vr_agtt_last;      // ... the last one's time
float vr_agtt_gap;       // ... the least gap between two
float vr_agtt_rl0;       // reload: the shotgun's time
float vr_agtt_keep[6];   // cvars put back
float vr_agtt_k;         // swing: frames into it
float vr_agtt_dev;       // ... the most the weapon's turn strayed from the reference's (degrees, from 0.2 s)
float vr_agtt_wmax;      // ... the parry's most share from 0.2 s
float vr_agtt_dev0;      // ... the most it strayed before (the raised parry going out: the case bites)
vector vr_agtt_home;     // ... where the reference swing began, its facing
float vr_agtt_yaw;
float vr_agtt_shots0;    // cast: vr_am_cast_shots at its start
float vr_agtt_casts0;
float VR_AGTT_DKCOOL_S = 120; // dkcool: how long he is watched (s of game time)
float vr_agtt_rolls0;    // dkcool: vr_am_gun_rolls at its start
float vr_agtt_seen;      // ... its casts seen
float vr_agtt_rolls;     // ... its attacks seen
float vr_agtt_lastatk;   // ... the last one's time
vector vr_agtt_ref[512]; // swing: the reference swing's weapon forward, each frame
vector vr_agtt_refu[512];

void(string s) VR_AGTT_Print = { bprint(sprintf("agtune: %s\n", s)); };

// An armed humanoid of `kind` with `code` `dist` ahead of the player, facing him.
entity(float kind, float code, float dist) VR_AGTT_Spawn =
{
    entity p = vr_agtt_pl;
    makevectors([0, p.angles_y, 0]);
    traceline(p.origin, p.origin + v_forward * dist, TRUE, p);
    vector at = trace_endpos - v_forward * 24;
    entity oldself = self;
    self = spawn();
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    self.weapon = code;
    VR_AM_Spawn(kind);
    entity g = self;
    self = oldself;
    return g;
};

void(entity g) VR_AGTT_Remove =
{
    if(!g)
    {
        return;
    }
    if(g.vr_ag_prop)
    {
        g.vr_ag_prop.weaponinst = world;
        remove(g.vr_ag_prop);
    }
    g.vr_ag_prop = world;
    remove(g);
};

// Held still (its AI off), aware of the player.
void(entity g) VR_AGTT_Hold =
{
    g.think = SUB_Null;
    g.nextthink = -1;
    g.enemy = vr_agtt_pl;
    g.health = 1000;
    g.attack_finished = time + 100;
};

void(entity g) VR_AGTT_Hunt =
{
    entity oldself = self;
    self = g;
    self.enemy = vr_agtt_pl;
    FoundTarget();
    self = oldself;
};

void(float c) VR_AGTT_Next =
{
    VR_AGTT_Remove(vr_agtt_g);
    vr_agtt_g = world;
    vr_agtt_case = c;
    vr_agtt_phase = 0;
    vr_agtt_t0 = time;
    vr_agtt_until = time + 20;
};

void() VR_AGTT_End =
{
    VR_AGTT_Remove(vr_agtt_g);
    vr_agtt_g = world;
    cvar_set("vr_agrunt_melee_speed", ftos(vr_agtt_keep[0]));
    cvar_set("vr_agdknight", ftos(vr_agtt_keep[1]));
    cvar_set("vr_agrunt_speed", "1");
    cvar_set("vr_agrunt_speed_back", "1");
    cvar_set("vr_agrunt_speed_strafe", "1");
    vr_agtt_pl.flags = vr_agtt_pl.flags - (vr_agtt_pl.flags & FL_NOTARGET);
    vr_agtt_case = 0;
    VR_Test_Verdict();
    cvar_set("vr_agtune_test", "0");
};

// fire: an AD Plasma Gun's (then a nailgun's) shots, their least gap.
void(float code, float want, string name) VR_AGTT_Fire =
{
    if(vr_agtt_phase == 0)
    {
        if(!VR_AG_Decode(code))
        {
            VR_Test_Check(TRUE, sprintf("fire: %s: not here (no AD data): skipped", name));
            vr_agtt_phase = 9;
            return;
        }
        vr_agtt_g = VR_AGTT_Spawn(VR_AM_GRUNT, code, 300);
        vr_agtt_g.health = 1000;
        VR_AGTT_Hunt(vr_agtt_g);
        vr_agtt_shots = 0;
        vr_agtt_last = -1;
        vr_agtt_gap = 100;
        vr_agtt_phase = 1;
        return;
    }
    entity p = vr_agtt_g.vr_ag_prop;
    if(p && p.vr_ag_total_shots > vr_agtt_shots)
    {
        if(vr_agtt_last >= 0)
        {
            vr_agtt_gap = min(vr_agtt_gap, time - vr_agtt_last);
        }
        vr_agtt_shots = p.vr_ag_total_shots;
        vr_agtt_last = time;
        p.weaponinst.wi_clip = max(p.weaponinst.wi_clip, 10); // (never a reload between)
    }
    if(vr_agtt_shots >= 8 || time > vr_agtt_until)
    {
        VR_Test_Check(vr_agtt_shots >= 8 && fabs(vr_agtt_gap - want) < 0.02,
            sprintf("fire: %s: %g shots, the least gap %.3f s (want %.3f)%s", name, vr_agtt_shots, vr_agtt_gap, want,
                time > vr_agtt_until ? " (timed out)" : ""));
        vr_agtt_phase = 9;
    }
};

// reload: its clip emptied, its reload begun; the time to its clip refilled.
float(float wid) VR_AGTT_Reload =
{
    if(vr_agtt_phase == 0)
    {
        vr_agtt_g = VR_AGTT_Spawn(VR_AM_GRUNT, wid, 200);
        VR_AGTT_Hold(vr_agtt_g);
        vr_agtt_g.vr_ag_prop.weaponinst.wi_clip = 0;
        vr_agtt_g.vr_ag_reserve = 50;
        vr_agtt_shots = vr_agtt_g.vr_ag_reloads;
        entity oldself = self;
        self = vr_agtt_g;
        vr_ag_reload1();
        self = oldself;
        vr_agtt_t0 = time;
        vr_agtt_phase = 1;
        return -1;
    }
    if(vr_agtt_g.vr_ag_reloads > vr_agtt_shots)
    {
        float t = time - vr_agtt_t0;
        VR_AGTT_Remove(vr_agtt_g);
        vr_agtt_g = world;
        vr_agtt_phase = 0;
        return t;
    }
    return time > vr_agtt_until ? 0 : -1;
};

// speed: a step of 10 at `rel` degrees from its facing; its length.
float(entity g, float rel) VR_AGTT_Step =
{
    vector o = g.origin;
    entity oldself = self;
    self = g;
    VR_AG_Walk(g.angles_y + rel, 10);
    self = oldself;
    float d = vlen(g.origin - o);
    setorigin(g, o);
    return d;
};

void(float s, float b, float st) VR_AGTT_SpeedSet =
{
    cvar_set("vr_agrunt_speed", ftos(s));
    cvar_set("vr_agrunt_speed_back", ftos(b));
    cvar_set("vr_agrunt_speed_strafe", ftos(st));
    entity g = vr_agtt_g;
    float f = VR_AGTT_Step(g, 0);
    float bk = VR_AGTT_Step(g, 180);
    float sd = VR_AGTT_Step(g, 90);
    VR_Test_Check(fabs(f - 10 * s) < 0.05 && fabs(bk - 10 * s * b) < 0.05 && fabs(sd - 10 * s * st) < 0.05,
        strcat(sprintf("speed: sliders %g/%g/%g: forward %.2f, back %.2f, sideways %.2f", s, b, st, f, bk, sd),
            sprintf(" (want %.2f, %.2f, %.2f)", 10 * s, 10 * s * b, 10 * s * st)));
};

// swing: its swing (kind 0, as varied 0.5) begun now, its parry raised (`raised`) or not.
void(entity g, float raised) VR_AGTT_SwingBegin =
{
    entity oldself = self;
    self = g;
    VR_AGP_Begin(VR_AGP_SWING);
    self = oldself;
    g.vr_ag_act_kind = 0;
    g.vr_ag_act_var = 0.5;
    if(raised)
    {
        g.vr_agpar_w = 1;
        g.vr_agpar_t = VR_AGPar_Chest(g) + '17 10 4'; // (a blow coming at its chest from its left)
        g.vr_agpar_seen = time;
        g.vr_agpar_ready = -1; // (no new parry: the one raised going out)
    }
    vr_agtt_k = 0;
};

// The angle (degrees) between the turn `f`, `u` and the reference's at frame `k`.
float(vector f, vector u, float k) VR_AGTT_Off =
{
    // (the chord between the unit vectors: the angle in radians, near enough for a few degrees)
    return max(vlen(f - vr_agtt_ref[k]), vlen(u - vr_agtt_refu[k])) * 57.29578;
};

void() VR_AGTune_TestFrame =
{
    float req = cvar_hget(cvarh_vr_agtune_test);
    if(!req)
    {
        return;
    }
    if(!vr_agtt_case && vr_agtt_phase != -1)
    {
        vr_agtt_pl = VR_Player();
        if(!vr_agtt_pl)
        {
            return;
        }
        VR_Test_BeginFlags("agtune", VR_TEST_LOUD);
        vr_agtt_keep[0] = cvar_hget(cvarh_vr_agrunt_melee_speed);
        vr_agtt_keep[1] = cvar_hget(cvarh_vr_agdknight);
        if(req == 2)
        {
            // The map's own armed humanoids (spawned with it): their armour by the skill.
            float sk = cvar_hget(cvarh_skill);
            float want = VR_AgArmor_SkillValue(min(3, max(0, rint(sk))), 0);
            float n = 0;
            float worn = 0;
            entity e = nextent(world);
            while(e)
            {
                if(e.vr_ag && e.vr_ag_kind <= 3)
                {
                    n = n + 1;
                    worn = worn + (e.vr_ar_type > 0);
                }
                e = nextent(e);
            }
            VR_Test_Check(n >= 5 && fabs(worn / max(1, n) - want) <= 0.2,
                sprintf("armour: %s on skill %g: %g of its %g armed humanoids wear armour (%.2f; vr_agarmor_skill%g %g)",
                    mapname, sk, worn, n, worn / max(1, n), rint(sk), want));
            VR_Test_Verdict();
            cvar_set("vr_agtune_test", "0");
            return;
        }
        vr_agtt_pl.flags = vr_agtt_pl.flags | FL_NOTARGET;
        vr_agtt_k = 0;
        vr_agtt_rl0 = 0;
        if(req == 3)
        {
            VR_AGTT_Next(6); // (dkcool, on its own)
            return;
        }
        VR_AGTT_Next(1);
    }
    entity pl = vr_agtt_pl;
    pl.health = max(pl.health, 1000);
    float c = vr_agtt_case;
    entity g = vr_agtt_g;
    entity oldself = self;
    if(c == 1)
    {
        // fire: the Plasma Gun (its interval), then the nailgun (every fire frame).
        pl.flags = pl.flags - (pl.flags & FL_NOTARGET);
        if(vr_agtt_k == 0)
        {
            VR_AGTT_Fire(VR_AG_CODE_PLASMAGUN, AD_PLASMAGUN_REFIRE, "AD Plasma Gun");
            if(vr_agtt_phase == 9)
            {
                VR_AGTT_Next(1);
                vr_agtt_k = 1;
            }
            return;
        }
        VR_AGTT_Fire(WID_NAILGUN, 0.1, "nailgun");
        if(vr_agtt_phase == 9)
        {
            vr_agtt_k = 0;
            pl.flags = pl.flags | FL_NOTARGET;
            VR_AGTT_Next(2);
        }
        return;
    }
    if(c == 2)
    {
        float t = VR_AGTT_Reload(vr_agtt_rl0 > 0 ? WID_SUPER_SHOTGUN : WID_SHOTGUN);
        if(t < 0)
        {
            return;
        }
        if(vr_agtt_rl0 <= 0)
        {
            vr_agtt_rl0 = max(0.001, t);
            vr_agtt_until = time + 20;
            return;
        }
        float ssg = cvar_hget(cvarh_vr_agrunt_reload_ssg);
        VR_Test_Check(t > 0 && fabs(t / vr_agtt_rl0 - ssg) < 0.03,
            sprintf("reload: the shotgun's %.3f s, the double-barrelled shotgun's %.3f s: x%.3f (vr_agrunt_reload_ssg %g)",
                vr_agtt_rl0, t, t / vr_agtt_rl0, ssg));
        vr_agtt_rl0 = 0;
        VR_AGTT_Next(3);
        return;
    }
    if(c == 3)
    {
        if(vr_agtt_phase == 0)
        {
            vr_agtt_g = VR_AGTT_Spawn(VR_AM_GRUNT, WID_SHOTGUN, 200);
            VR_AGTT_Hold(vr_agtt_g);
            vr_agtt_phase = 1;
            return;
        }
        if(!(vr_agtt_g.flags & FL_ONGROUND) && time < vr_agtt_until)
        {
            return; // (on its feet first: walkmove's)
        }
        VR_AGTT_SpeedSet(1, 1, 1);
        VR_AGTT_SpeedSet(0.5, 1, 1);
        VR_AGTT_SpeedSet(1, 0.5, 1);
        VR_AGTT_SpeedSet(1, 1, 0.5);
        cvar_set("vr_agrunt_speed", "1");
        cvar_set("vr_agrunt_speed_back", "1");
        cvar_set("vr_agrunt_speed_strafe", "1");
        VR_AGTT_Next(4);
        return;
    }
    if(c == 4)
    {
        // swing: the reference (phase 1), then with its parry raised as it begins (phase 2).
        if(vr_agtt_phase == 0)
        {
            cvar_set("vr_agrunt_melee_speed", "0.3");
            vr_agtt_g = VR_AGTT_Spawn(VR_AM_GRUNT, WID_SWORD, 60);
            VR_AGTT_Hold(vr_agtt_g);
            vr_agtt_phase = 0.5;
            vr_agtt_dev = 0;
            vr_agtt_wmax = 0;
            vr_agtt_dev0 = 0;
            return;
        }
        if(vr_agtt_phase == 0.5)
        {
            if(time - vr_agtt_t0 > 0.3) // (settled, its weapon posed a while)
            {
                vr_agtt_home = g.origin;
                vr_agtt_yaw = g.angles_y;
                VR_AGTT_SwingBegin(g, FALSE);
                vr_agtt_phase = 1;
            }
            return;
        }
        g.health = 1000;
        g.attack_finished = time + 100; // (its act's think its own: VR_AGP_Begin set it)
        entity p = g.vr_ag_prop;
        makevectors(p.angles);
        float k = vr_agtt_k;
        vr_agtt_k = vr_agtt_k + 1;
        if(g.vr_ag_act && k < 512)
        {
            if(vr_agtt_phase == 1)
            {
                vr_agtt_ref[k] = v_forward;
                vr_agtt_refu[k] = v_up;
            }
            else if(time - vr_agtt_t0 >= 0.2)
            {
                vr_agtt_dev = max(vr_agtt_dev, VR_AGTT_Off(v_forward, v_up, k));
                vr_agtt_wmax = max(vr_agtt_wmax, g.vr_agpar_w);
            }
            else
            {
                vr_agtt_dev0 = max(vr_agtt_dev0, VR_AGTT_Off(v_forward, v_up, k)); // (the parry going out: it was in)
            }
            return;
        }
        if(vr_agtt_phase == 1)
        {
            vr_agtt_phase = 2;
            vr_agtt_t0 = time;
            setorigin(g, vr_agtt_home); // (as the reference began: its step in undone)
            g.angles_y = vr_agtt_yaw;
            VR_AGTT_SwingBegin(g, TRUE);
            return;
        }
        VR_Test_Check(vr_agtt_dev < 1 && vr_agtt_wmax < 0.01 && vr_agtt_dev0 > 5 && k > 30,
            sprintf("swing: at Enemy Melee Speed 0.3, its parry raised as its swing began (%.0f deg off at first): from "
                "0.2 s in its weapon %.2f deg off the same swing's without it, the parry's share %.2f at most (%g frames)",
                vr_agtt_dev0, vr_agtt_dev, vr_agtt_wmax, k));
        cvar_set("vr_agrunt_melee_speed", ftos(vr_agtt_keep[0]));
        VR_AGTT_Next(5);
        return;
    }
    if(c == 5)
    {
        // cast: an armed death knight with a shotgun casts (its gun's thrust: the volley from its muzzle).
        if(vr_agtt_phase == 0)
        {
            cvar_set("vr_agdknight", "1");
            makevectors([0, pl.angles_y, 0]);
            self = spawn();
            self.classname = "monster_hell_knight";
            setorigin(self, pl.origin + v_forward * 380);
            self.angles_y = pl.angles_y + 180;
            self.weapon = WID_SHOTGUN;
            monster_hell_knight();
            vr_agtt_g = self;
            self = oldself;
            vr_agtt_phase = 1;
            return;
        }
        if(vr_agtt_phase == 1)
        {
            if(!g.vr_ag || !g.vr_ag_prop)
            {
                VR_Test_Check(FALSE, "cast: no armed death knight with a gun");
                VR_AGTT_End();
                return;
            }
            pl.flags = pl.flags - (pl.flags & FL_NOTARGET);
            g.enemy = pl;
            g.health = 1000;
            vr_agtt_shots0 = vr_am_cast_shots;
            vr_agtt_casts0 = vr_am_casts;
            oldself = self;
            self = g;
            VR_AGP_Cast();
            self = oldself;
            g.attack_finished = time + 100;
            vr_agtt_phase = 2;
            vr_agtt_gap = -1;
            return;
        }
        g.attack_finished = time + 100;
        float shots = vr_am_cast_shots - vr_agtt_shots0;
        float flying = 0;
        entity e2 = find(world, classname, "knightspike");
        while(e2)
        {
            flying = flying + (e2.owner == g);
            e2 = find(e2, classname, "knightspike");
        }
        if(vr_agtt_phase == 2 && shots >= 1)
        {
            vr_agtt_phase = 3;
            vr_agtt_last = time;
        }
        if(vr_agtt_phase == 3 && shots >= 6 && vr_agtt_gap < 0)
        {
            vr_agtt_gap = time - vr_agtt_last; // (the sweep's span, first to last)
            vr_agtt_dev = flying;              // (in flight as the last leaves)
        }
        if((vr_agtt_gap >= 0 && !find(world, classname, "vr_ag_caster")) || time > vr_agtt_until)
        {
            VR_Test_Check(shots == 6 && vr_agtt_dev == 6 && vr_am_casts - vr_agtt_casts0 == 1 && fabs(vr_agtt_gap - 0.5) < 0.03,
                sprintf("cast: a death knight's gun thrust: %g fireballs over %.2f s, %g in flight as the last left%s",
                    shots, vr_agtt_gap, vr_agtt_dev, time > vr_agtt_until ? " (timed out)" : ""));
            VR_AGTT_End();
        }
        return;
    }
    if(c == 6)
    {
        // dkcool: a death knight with a shotgun hunting you for two minutes: his casts, his gun's attacks.
        if(vr_agtt_phase == 0)
        {
            cvar_set("vr_agdknight", "1");
            makevectors([0, pl.angles_y, 0]);
            self = spawn();
            self.classname = "monster_hell_knight";
            setorigin(self, pl.origin + v_forward * 380);
            self.angles_y = pl.angles_y + 180;
            self.weapon = WID_SHOTGUN;
            monster_hell_knight();
            vr_agtt_g = self;
            self = oldself;
            pl.flags = pl.flags - (pl.flags & FL_NOTARGET);
            VR_AGTT_Hunt(vr_agtt_g);
            vr_agtt_rolls0 = vr_am_gun_rolls;
            vr_agtt_casts0 = vr_am_gun_casts;
            vr_agtt_t0 = time;
            vr_agtt_last = -1;
            vr_agtt_gap = 999999;
            vr_agtt_dev = 0;
            vr_agtt_seen = 0;
            vr_agtt_rolls = 0;
            vr_agtt_lastatk = time;
            vr_agtt_phase = 1;
            return;
        }
        g.health = 1000;
        if(time >= vr_agtt_until)
        {
            vr_agtt_until = time + 15; // (its log, for a reader: where he has got to)
            dprint(sprintf("agtune: dkcool: %.0f s: %.0f from you, enemy %s, attack_finished in %.1f, act %g\n",
                time - vr_agtt_t0, vlen(g.origin - pl.origin), g.enemy.classname, g.attack_finished - time, g.vr_ag_act));
        }
        float seen = vr_am_gun_casts - vr_agtt_casts0;
        if(vr_am_gun_rolls - vr_agtt_rolls0 > vr_agtt_rolls)
        {
            vr_agtt_rolls = vr_am_gun_rolls - vr_agtt_rolls0;
            vr_agtt_lastatk = time;
        }
        if(seen > vr_agtt_seen)
        {
            if(vr_agtt_last >= 0)
            {
                vr_agtt_gap = min(vr_agtt_gap, time - vr_agtt_last);
            }
            else
            {
                vr_agtt_dev = time - vr_agtt_t0; // (his first)
            }
            vr_agtt_last = time;
            vr_agtt_seen = seen;
        }
        if(time - vr_agtt_t0 >= VR_AGTT_DKCOOL_S)
        {
            float cd = cvar_hget(cvarh_vr_agdknight_cast_cooldown);
            float gun = vr_am_gun_rolls - vr_agtt_rolls0 - seen;
            float span = max(1, vr_agtt_lastatk - vr_agtt_t0); // (to his last attack: he can run out of a clear shot at you)
            dprint(sprintf("agtune: dkcool: cooldown %g s, over the %.0f s to his last attack: %g casts (one per %.1f s, the closest %.1f s apart, his first at %.1f s)\n",
                cd, span, seen, span / max(1, seen), vr_agtt_gap, vr_agtt_dev));
            dprint(sprintf("agtune: dkcool: %g gun attacks (%.0f%% of his ranged attacks are casts), holding %g (class %g), his health %g\n",
                gun, 100 * seen / max(1, seen + gun), g.vr_ag_wid, VR_AG_Class(g.vr_ag_wid), g.health));
            VR_Test_Check(g.vr_ag_prop && VR_AG_Class(g.vr_ag_wid) != VR_AG_CLASS_MELEE && seen >= 1 && gun >= seen * 0.5 &&
                vr_agtt_gap >= cd - 0.02, "dkcool: he still holds his gun, casts no closer than the cooldown, and fires between");
            VR_AGTT_End();
        }
        return;
    }
};
#endif
