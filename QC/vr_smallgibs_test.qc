// vr_smallgibs_test.qc -- the small gibs' tests (docs/vr-port/ROUND21.md, "Small gibs"): `vr_smallgibs_test <n>` runs
// test n on the monster or corpse nearest the player (impulse 241 puts one ahead) and prints `sgibtest:` lines
// (Debug > Gore Tests). N of each: vr_smallgibs_test_n (default 200); a blow's damage: vr_smallgibs_test_dmg (35).
//   1 shotgun blasts (6 pellets, its damage and spread)    2 super shotgun blasts (14)
//   3 nails and super nails                                 4 an axe's blade, its pommel, a punch
//   5 nails under Quad Damage                               6 the chainsaw's chain for a second (a corpse: alive too)
//   7 a large gib and a head burst                          8 Most Small Gibs 10, 15 made: the oldest go
//   9 one in the off hand at Small Gibs Last 1 (the script holds the grip 3 s: vr_mock_button off grip 1, then 0):
//     there after 3 s, gone 1 s after it lands (and the fade)
//  10 the grace: one from just behind the monster through it, with the grace and without
//  11 the ones lying about: how many, their size, rigid, asleep, stuck, held
//  12 thrown into the nearest wall: a gib and a small gib at 220 u/s stick (Thrown Gibs Stick 1), a gib at 400 bursts

float vr_sgib_test;       // the test running, its step, when the step began
float vr_sgib_test_step;
float vr_sgib_test_t;
entity vr_sgib_test_m;    // its monster or corpse
entity vr_sgib_test_g;    // its small (or large) gib
entity vr_sgib_test_e;    // a third (test 12)
float vr_sgib_test_id;
float vr_sgib_test_a;     // what it counts
float vr_sgib_test_b;

entity(entity p) VR_SmallGib_TestTarget =
{
    entity best = world;
    float bestd = 0;
    entity e = findradius(p.origin, 800);
    while(e)
    {
        if(e != p && e.classname != "vr_dummy" && ((e.flags & FL_MONSTER) || e.vr_corpse == 2) && !e.vr_gib)
        {
            float d = vlen(e.origin - p.origin);
            if(best == world || d < bestd)
            {
                best = e;
                bestd = d;
            }
        }
        e = e.chain;
    }
    return best;
};

// `n` blasts of `xPellets` at `m`, from the player's eyes.
void(entity m, float xPellets, float xDmg, vector xSpread, float n, string xName) VR_SmallGib_TestBlasts =
{
    float withGibs = 0;
    float made = vr_sgib_made;
    vector src = self.origin + '0 0 22';
    m.health = 100000;
    vr_sgib_rolls = 0;
    vr_sgib_rolldmg = 0;
    for(float i = 0; i < n; i++)
    {
        vector dir = normalize((m.absmin + m.absmax) * 0.5 - src);
        float before = vr_sgib_made;
        VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
        vr_pellet_damage = xDmg;
        FireBulletsImpl(xPellets, dir, xSpread, src, 2048, TRUE);
        vr_pellet_damage = 0;
        VR_ClearHitKind();
        if(vr_sgib_made > before)
        {
            withGibs++;
        }
    }
    bprint(sprintf("sgibtest: %s: %d blasts at %s %.0f units away: %d hits weighed, %.1f damage each\n", xName, n, m.classname,
        vlen(m.origin - self.origin), vr_sgib_rolls, vr_sgib_rolldmg / max(1, vr_sgib_rolls)));
    bprint(sprintf("sgibtest: %s: %d tore small gibs out (%.0f%%), %d small gibs\n", xName, withGibs,
        100 * withGibs / max(1, n), vr_sgib_made - made));
};

// `n` blows of `xDmg` on `m` by `inflictor` (the player's hand `xKind` with weapon `xWeapon`, `xBlunt` its pommel), the
// rate that tore any out.
void(entity m, entity inflictor, float xKind, float xWeapon, float xBlunt, float xDmg, float n, string xName) VR_SmallGib_TestBlows =
{
    float withGibs = 0;
    float made = vr_sgib_made;
    float oldWeapon = self.weapon;
    vector mid = (m.absmin + m.absmax) * 0.5;
    m.health = 100000;
    if(m.vr_corpse == 2)
    {
        m.vr_corpse_hp = 100000;
    }
    for(float i = 0; i < n; i++)
    {
        float before = vr_sgib_made;
        self.weapon = xWeapon;
        VR_SetHitKind(xKind, xKind == QVR_HITKIND_MELEE ? cVR_MainHand : -1, mid, normalize(mid - self.origin));
        vr_melee_blunt = xBlunt;
        T_Damage(m, inflictor, self, xDmg);
        vr_melee_blunt = FALSE;
        VR_ClearHitKind();
        self.weapon = oldWeapon;
        if(vr_sgib_made > before)
        {
            withGibs++;
        }
    }
    bprint(sprintf("sgibtest: %s: %d of %.0f on %s, %d tore small gibs out (%.0f%%), %d small gibs\n", xName, n, xDmg,
        m.classname, withGibs, 100 * withGibs / max(1, n), vr_sgib_made - made));
};

float(entity g) VR_SmallGib_TestInBox =
{
    entity m = vr_sgib_test_m;
    vector o = (g.absmin + g.absmax) * 0.5;
    return o_x > m.absmin_x && o_x < m.absmax_x && o_y > m.absmin_y && o_y < m.absmax_y && o_z > m.absmin_z && o_z < m.absmax_z;
};

void(float xCase) VR_SmallGib_TestStart =
{
    vr_sgib_test = xCase;
    vr_sgib_test_step = 0;
    vr_sgib_test_t = time;
    vr_sgib_test_m = VR_SmallGib_TestTarget(self);
    float n = cvar("vr_smallgibs_test_n");
    n = n > 0 ? n : 200;
    float dmg = cvar("vr_smallgibs_test_dmg");
    dmg = dmg > 0 ? dmg : 35;
    entity m = vr_sgib_test_m;
    if(m == world && xCase != 8 && xCase != 9 && xCase != 11 && xCase != 12)
    {
        bprint("sgibtest: no monster or corpse near\n");
        vr_sgib_test = 0;
        return;
    }
    bprint(sprintf("sgibtest: test %d on %s (corpse %d)\n", xCase, m.classname, m.vr_corpse));
    if(m != world && m.vr_corpse != 2 && (m.flags & FL_MONSTER))
    {
        // Still, standing (its first frame): the same pose every run (a pose with the arms ahead takes the pellets
        // as limbs', 0.6 of their damage).
        m.think = SUB_Null;
        m.nextthink = -1;
        m.frame = 0;
        m.velocity = '0 0 0';
    }
    if(xCase == 1)
    {
        VR_SmallGib_TestBlasts(m, 6, cvar_hget(cvarh_vr_dmg_shotgun), '0.01 0.01 0', n, "shotgun");
    }
    else if(xCase == 2)
    {
        VR_SmallGib_TestBlasts(m, 14, cvar_hget(cvarh_vr_dmg_super_shotgun), '0.035 0.025 0', n, "super shotgun");
    }
    else if(xCase == 3 || xCase == 5)
    {
        if(xCase == 5)
        {
            n = ceil(n / 4); // (each makes 1 to 4: one frame's work)
        }
        entity spk = spawn();
        spk.classname = "spike";
        setorigin(spk, (m.absmin + m.absmax) * 0.5 + normalize(self.origin - m.origin) * 16);
        if(xCase == 5)
        {
            self.super_damage_finished = time + 30;
        }
        VR_SmallGib_TestBlows(m, spk, QVR_HITKIND_NONE, self.weapon, FALSE, cvar_hget(cvarh_vr_dmg_nail), n, xCase == 5 ? "nails, quad" : "nails");
        VR_SmallGib_TestBlows(m, spk, QVR_HITKIND_NONE, self.weapon, FALSE, cvar_hget(cvarh_vr_dmg_super_nail), n,
            xCase == 5 ? "super nails, quad" : "super nails");
        if(xCase == 5)
        {
            VR_SmallGib_TestBlows(m, self, QVR_HITKIND_MELEE, WID_FIST, FALSE, 8, n, "a light punch, quad");
            self.super_damage_finished = 0;
        }
        remove(spk);
    }
    else if(xCase == 4)
    {
        VR_SmallGib_TestBlows(m, self, QVR_HITKIND_MELEE, WID_AXE, FALSE, dmg, n, "axe blade");
        VR_SmallGib_TestBlows(m, self, QVR_HITKIND_MELEE, WID_AXE, TRUE, dmg, n, "axe pommel");
        VR_SmallGib_TestBlows(m, self, QVR_HITKIND_MELEE, WID_SWORD, FALSE, dmg, n, "sword blade");
        VR_SmallGib_TestBlows(m, self, QVR_HITKIND_MELEE, WID_FIST, FALSE, 25, n, "a strong punch");
    }
    else if(xCase == 6)
    {
        m.health = max(m.health, 100000);
        vr_sgib_test_a = vr_sgib_made;
        vr_sgib_test_b = 0;
        if(m.vr_corpse == 2)
        {
            m.vr_corpse_hp = 100000;
        }
    }
    else if(xCase == 7)
    {
        makevectors('0 1 0' * self.v_viewangle_y);
        vector at = self.origin + v_forward * 48;
        vr_sgib_test_a = vr_sgib_made;
        entity oself = self;
        self = spawn();
        setorigin(self, at);
        ThrowGib("progs/gib2.mdl", -10);
        ThrowHead("progs/h_ogre.mdl", -10); // (the spawned thing itself made a head)
        vr_sgib_test_g = self;
        self = oself;
    }
    else if(xCase == 8)
    {
        cvar_set("vr_smallgibs_max", "10");
        vr_sgib_test_a = vr_sgib_serial + 1; // the first one's serial
        makevectors('0 1 0' * self.v_viewangle_y);
        for(float k = 0; k < 15; k++)
        {
            VR_SmallGib_Spawn(self.origin + v_forward * 64 + v_right * (k * 4 - 28) + '0 0 8', '0 0 0', world);
        }
        float lowest = 0;
        float firstFive = 0;
        for(float s = 0; s < 256; s++)
        {
            if(VR_SmallGib_SlotLive(s))
            {
                if(!lowest || vr_sgib_ringid[s] < lowest)
                {
                    lowest = vr_sgib_ringid[s];
                }
                if(vr_sgib_ringid[s] < vr_sgib_test_a + 5)
                {
                    firstFive++;
                }
            }
        }
        bprint(sprintf("sgibtest: cap: made 15 at most 10: %d lie about, the oldest #%d (the first was #%d), %d of the first five left\n",
            VR_SmallGib_Count(), lowest, vr_sgib_test_a, firstFive));
        cvar_set("vr_smallgibs_max", "40");
        vr_sgib_test = 0;
    }
    else if(xCase == 9)
    {
        cvar_set("vr_smallgibs_time", "1");
        entity g = VR_SmallGib_Spawn(VRGetHandPos(cVR_OffHand), '0 0 0', world);
        vr_sgib_test_g = g;
        vr_sgib_test_id = g.vr_sgib_id;
        if(g != world && VRIsHandEmpty(self, cVR_OffHand))
        {
            VR_Carry_Start(self, cVR_OffHand, g);
        }
        bprint(sprintf("sgibtest: held: #%d in the off hand %d\n", g.vr_sgib_id, g.carry_player == self));
    }
    else if(xCase == 12)
    {
        // Thrown into the nearest wall (of four ways round): a gib at 220 u/s and a small gib at 220 (stick: Thrown Gibs
        // Stick 1 for the test), a gib at 400 (faster than Gib Splat Speed: it bursts).
        float bestFrac = 2;
        vector wallDir = '0 0 0';
        vector eye = self.origin + '0 0 16';
        for(float yaw = 0; yaw < 360; yaw = yaw + 90)
        {
            makevectors('0 1 0' * yaw);
            traceline(eye, eye + v_forward * 400, TRUE, self);
            if(trace_fraction < bestFrac && !trace_startsolid)
            {
                bestFrac = trace_fraction;
                wallDir = v_forward;
            }
        }
        if(bestFrac >= 1)
        {
            bprint("sgibtest: no wall within 400 units\n");
            vr_sgib_test = 0;
            return;
        }
        cvar_set("vr_gore_stick_thrown", "1");
        vector wall = eye + wallDir * (400 * bestFrac);
        makevectors(vectoangles(wallDir));
        for(float w = 0; w < 3; w++)
        {
            vector from = wall - wallDir * 48 + v_right * (w * 24 - 24);
            entity tg;
            if(w == 1)
            {
                cvar_set("vr_smallgibs_grace", "0");
                tg = VR_SmallGib_Spawn(from, wallDir * 220, world);
                cvar_set("vr_smallgibs_grace", "0.15");
            }
            else
            {
                tg = spawn();
                setmodel(tg, "progs/gib1.mdl");
                setorigin(tg, from);
                tg.think = SUB_Remove;
                tg.nextthink = time + 30;
                VR_MakeGibGrabbable(tg);
                VR_Gib_MakeDestroyable(tg, 1);
                tg.vr_gib_armtime = time;
                tg.vr_rigid = 1;
                tg.velocity = wallDir * (w == 0 ? 220 : 400);
            }
            tg.movetype = MOVETYPE_TOSS;
            VR_Gib_Released(tg, TRUE, cVR_MainHand);
            if(w == 0) { vr_sgib_test_g = tg; }
            else if(w == 1) { vr_sgib_test_m = tg; }
            else { vr_sgib_test_e = tg; }
        }
        vr_sgib_test_t = time;
    }
    else if(xCase == 11)
    {
        // The ones lying about: how many, their size, mass, how many rigid bodies, asleep, stuck, held.
        float cnt = 0, rigid = 0, asleep = 0, stuck = 0, held = 0;
        float sz = 0, szMin = 1000, szMax = 0, last = 0;
        for(float r = 0; r < 256; r++)
        {
            if(VR_SmallGib_SlotLive(r))
            {
                entity q = vr_sgib_ring[r];
                vector qd = q.absmax - q.absmin;
                float qs = max(qd_x, max(qd_y, qd_z));
                cnt++;
                last = r;
                sz = sz + qs;
                szMin = min(szMin, qs);
                szMax = max(szMax, qs);
                rigid = rigid + (q.vr_rigid != 0);
                asleep = asleep + ((q.flags & FL_ONGROUND) != 0);
                stuck = stuck + (q.vr_gib_stuck != 0);
                held = held + (q.carry_player != world);
            }
        }
        bprint(sprintf("sgibtest: %d small gibs (%d made): size %.1f..%.1f (mean %.1f) units, a rock %.1f\n",
            cnt, vr_sgib_made, szMin, szMax, sz / max(1, cnt), VR_SmallGib_RockSize()));
        bprint(sprintf("sgibtest: %d rigid, %d asleep, %d stuck, %d held; mass %.2f kg\n", rigid, asleep, stuck, held,
            cnt > 0 ? propvalue(vr_sgib_ring[last], "mass") : 0));
        vr_sgib_test = 0;
    }
    else if(xCase == 10)
    {
        m.think = SUB_Null; // (still: its AI stopped)
        m.nextthink = -1;
        m.velocity = '0 0 0';
    }
    if(xCase <= 5)
    {
        vr_sgib_test = 0;
    }
};

// Every server frame (StartFrame): a test asked for (vr_smallgibs_test), or the next step of one running.
void() VR_SmallGib_TestFrame =
{
    float ask = cvar("vr_smallgibs_test");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    if(ask)
    {
        cvar_set("vr_smallgibs_test", "0");
        VR_SmallGib_TestStart(ask);
    }
    else if(vr_sgib_test)
    {
        float t = time - vr_sgib_test_t;
        entity m = vr_sgib_test_m;
        entity g = vr_sgib_test_g;
        if(vr_sgib_test == 6)
        {
            // The chain running through it a second (VR_Saw's cut: a tick's damage each frame).
            if(t < 1)
            {
                float oldWeapon = self.weapon;
                self.weapon = WID_CHAINSAW;
                vector mid = (m.absmin + m.absmax) * 0.5;
                VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, mid, normalize(mid - self.origin));
                T_DamageImpl(m, self, self, cvar_hget(cvarh_vr_chainsaw_damage) * frametime, 1);
                VR_ClearHitKind();
                self.weapon = oldWeapon;
                vr_sgib_test_b++;
            }
            else
            {
                bprint(sprintf("sgibtest: chainsaw on %s (corpse %d): %d ticks in %.2f s, %d small gibs (one each %.2f s at most)\n",
                    m.classname, m.vr_corpse, vr_sgib_test_b, t, vr_sgib_made - vr_sgib_test_a,
                    cvar_hget(cvarh_vr_smallgibs_saw_interval)));
                vr_sgib_test = 0;
            }
        }
        else if(vr_sgib_test == 7)
        {
            if(vr_sgib_test_step == 0 && t >= 0.4)
            {
                // Both armed: shot to bursting.
                vr_sgib_test_a = vr_sgib_made;
                entity e = findradius(g.origin, 400);
                float burst = 0;
                while(e)
                {
                    if(e.vr_gib && !e.vr_sgib && e.th_die == VR_Gib_Die)
                    {
                        T_Damage(e, self, self, 100);
                        burst++;
                    }
                    e = e.chain;
                }
                bprint(sprintf("sgibtest: burst %d large gibs and heads: %d small gibs (Burst Into %.0f, a head half as many again)\n",
                    burst, vr_sgib_made - vr_sgib_test_a, cvar_hget(cvarh_vr_smallgibs_burst)));
                vr_sgib_test = 0;
            }
        }
        else if(vr_sgib_test == 9)
        {
            // Held by the off hand's grip (the script holds it: vr_mock_button off grip 1), then let go of (grip 0).
            float live = g != world && g.vr_sgib_id == vr_sgib_test_id;
            if(vr_sgib_test_step == 0 && t >= 2.9)
            {
                bprint(sprintf("sgibtest: held: after %.1f s (Small Gibs Last 1 s) there %d, held %d, alpha %.2f\n", t, live,
                    live && g.carry_player != world, g.alpha));
                vr_sgib_test_step = 1;
                vr_sgib_test_a = 0;
                vr_sgib_test_b = 0;
            }
            else if(vr_sgib_test_step == 1)
            {
                if(!vr_sgib_test_a && live && g.carry_player == world)
                {
                    vr_sgib_test_a = t; // let go
                }
                if(vr_sgib_test_a && !vr_sgib_test_b && live && (g.flags & FL_ONGROUND))
                {
                    vr_sgib_test_b = t; // landed
                }
                if(!live || t > 20)
                {
                    bprint(sprintf("sgibtest: held: let go at %.2f s, landed at %.2f s, gone at %.2f s (%.2f s after landing: Last 1 s and the fade)\n",
                        vr_sgib_test_a, vr_sgib_test_b, t, t - vr_sgib_test_b));
                    cvar_set("vr_smallgibs_time", "20");
                    vr_sgib_test = 0;
                }
            }
        }
        else if(vr_sgib_test == 12)
        {
            if(t >= 1)
            {
                entity g1 = vr_sgib_test_g;
                entity g2 = vr_sgib_test_m;
                entity g3 = vr_sgib_test_e;
                bprint(sprintf("sgibtest: thrown into a wall: a gib at 220 stuck %d burst %d; a small gib at 220 stuck %d\n",
                    g1.vr_gib_stuck != 0, !g1.vr_gib, g2.vr_gib_stuck != 0));
                bprint(sprintf("sgibtest: thrown into a wall: a gib at 400 (Gib Splat Speed %.0f) stuck %d burst %d\n",
                    cvar_hget(cvarh_vr_gib_splat_speed), g3.vr_gib_stuck != 0, !g3.vr_gib));
                cvar_set("vr_gore_stick_thrown", "0.5");
                vr_sgib_test = 0;
            }
        }
        else if(vr_sgib_test == 10)
        {
            if(vr_sgib_test_step == 0 || vr_sgib_test_step == 2)
            {
                if(vr_sgib_test_step == 2)
                {
                    cvar_set("vr_smallgibs_grace", "0");
                }
                // Just behind it, going through it towards you (300 units/s: across it in the grace).
                vector mid2 = (m.absmin + m.absmax) * 0.5;
                vector out = normalize(self.origin - m.origin);
                out_z = 0;
                g = VR_SmallGib_Spawn(mid2 - out * (m.maxs_x + 6), out * 300 + '0 0 60', m);
                vr_sgib_test_g = g;
                vr_sgib_test_id = g.vr_sgib_id;
                vr_sgib_test_t = time;
                vr_sgib_test_a = 0;
                vr_sgib_test_b = 0;
                vr_sgib_test_step++;
            }
            else if(t >= 0.05 && t < 0.07)
            {
                vr_sgib_test_b = g.owner == m;
            }
            else if(t >= 0.3)
            {
                vector o = (g.absmin + g.absmax) * 0.5 - (m.absmin + m.absmax) * 0.5;
                vector way = normalize(self.origin - m.origin);
                way_z = 0;
                bprint(sprintf("sgibtest: grace %.2f: from behind it at 300 u/s: passes through at 0.05 s %d; at 0.3 s %.0f units %s it (going %.0f u/s %s)\n",
                    cvar_hget(cvarh_vr_smallgibs_grace), vr_sgib_test_b, fabs(o * way), o * way > 0 ? "in front of" : "behind",
                    fabs(g.velocity * way), g.velocity * way > 0 ? "on" : "back"));
                vr_sgib_test_step++;
                if(vr_sgib_test_step >= 4)
                {
                    cvar_set("vr_smallgibs_grace", "0.15");
                    vr_sgib_test = 0;
                }
            }
        }
    }
    self = oself;
};
