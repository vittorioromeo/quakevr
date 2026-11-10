#ifdef QVR_TESTS
// vr_systems_test.qc -- the shared systems' tests and dumps (vr_systems_test <n>, Debug > Systems Tests;
// DYNAMIC_SPAWNS.md, "Prerequisites available"):
//   1 every entity's material answers, one line each (the material table's consumers: hits, fire, impacts), then
//     "systems_test: matdump <n> entities": the before/after proof that the table changed no answer;
//   2 every entity's status (vr_status.qc) and wall mount (vr_mount.qc), then "systems_test: status <n> entities".

// One entity's consumers' answers.
void(entity e) VR_SysTest_MatLine =
{
    float m = VR_MaterialOf(e);
    dprint(sprintf("matdump: %s %s at %v: mat %g fleshy %g", e.classname, e.model, e.origin, m, VR_Material_Fleshy(m)));
    dprint(sprintf(" burncan %g burnwood %g hitmetal %g cratewood %g\n", VR_Burn_Can(e), VR_Burn_Wood(e), VR_Hit_Metal(e),
        VR_Crate_Wooden(e)));
};

void() VR_SysTest_MatDump =
{
    float n = 0;
    entity e = nextent(world);
    while(e)
    {
        if(e.classname != "")
        {
            VR_SysTest_MatLine(e);
            n = n + 1;
        }
        e = nextent(e);
    }
    dprint(sprintf("systems_test: matdump %g entities\n", n));
};

// ----------------------------------------------------------------------------
// The scenes (10..): each sets its stage ahead of the player (an open floor: vrfiringrange), runs a state machine a
// frame at a time and ends with "systems_test: PASS|FAIL <case>: <why>", then "systems_test: N passed, M failed".
// One scene a map load: a scene's leftovers (crates' pieces, bodies) would meet the next one's blast.
// 10 fire spreads: a burning grunt lights the crate it touches; a burning crate lights the grunt it touches (flesh's chance)
// 11 a blast lights the crate within half its radius, not the one past it nor the grunt in it (vr_burn_blast_flesh 0)
// 12 floor buttons: a monster on one presses it only with vr_monsters_press_buttons 1
// 13 the status component: a grunt and you burning (the fire's own state), doused, soaked, dried
// 14 a grunt's thrown thing knocks another grunt's weapon away, and the disarmed one turns on the thrower
// 15 the author's chain: a blast kills a grunt and flings its body into a wall torch, the torch falls lit onto crates,
//    a crate catches
// 16 a crate dropped (no hand) on a grunt and the same crate thrown by the player: the same stagger chance

float vr_st_case;  // the running scene (0 none)
float vr_st_step;
float vr_st_t0;    // its step's start
float vr_st_n;     // a count the scene keeps

entity vr_st_a, vr_st_b, vr_st_c, vr_st_d;

vector(float xAhead, float xLeft, float xUp) VR_ST_At =
{
    entity p = VR_Player();
    makevectors('0 1 0' * p.angles_y);
    return p.origin + v_forward * xAhead - v_right * xLeft + '0 0 1' * xUp;
};

entity(void() xSpawn, string xClass, vector xAt, float xFlags) VR_ST_Make =
{
    entity e = spawn();
    e.classname = xClass;
    e.spawnflags = xFlags;
    setorigin(e, xAt);
    entity oself = self;
    self = e;
    xSpawn();
    self = oself;
    return e;
};

// A grunt that stands still (its AI off: it neither runs from its fire nor walks away from the stage).
entity(vector xAt, float xFreeze) VR_ST_Grunt =
{
    entity p = VR_Player();
    entity oself = self;
    self = spawn();
    setorigin(self, xAt);
    self.angles_y = p.angles_y + 180;
    self.weapon = WID_SHOTGUN; // (an armed grunt: VR_AGT_Spawn's way)
    VR_AM_Spawn(VR_AM_GRUNT);
    VR_AgArmor_Give(self, 0, 0); // (no plates: a kill is one blow, its material flesh)
    entity g = self;
    if(self.think == walkmonster_start_go)
    {
        walkmonster_start_go(); // (its spawn's last step now: takedamage, its eyes)
    }
    if(xFreeze)
    {
        self.think = SUB_Null;
        self.nextthink = 0;
    }
    self = oself;
    return g;
};

void(float xOk, string xWhy) VR_ST_End =
{
    VR_Test_Check(xOk, sprintf("%g: %s", vr_st_case, xWhy));
    vr_st_case = 0;
    VR_Test_Verdict();
};

void(float xStep) VR_ST_Next = { vr_st_step = xStep; vr_st_t0 = time; };

void() VR_ST_Run =
{
    entity p = VR_Player();
    float t = time - vr_st_t0;
    if(vr_st_case == 10)
    {
        if(vr_st_step == 0)
        {
            vr_st_a = VR_ST_Grunt(VR_ST_At(160, 0, 0), TRUE);
            vr_st_b = VR_ST_Make(vr_crate, "vr_crate", VR_ST_At(160, 31, 0), 0);
            VR_Burn_Ignite(vr_st_a, p, vr_st_a.origin, '0 0 1', VR_BURN_TOUCH);
            VR_ST_Next(1);
        }
        else if(vr_st_step == 1)
        {
            if(vr_st_b.burn_fire != world)
            {
                dprint(sprintf("systems_test: 10: the burning grunt lit the crate after %.1f s\n", t));
                vr_st_c = VR_ST_Make(vr_crate, "vr_crate", VR_ST_At(320, 0, 0), 0);
                vr_st_d = VR_ST_Grunt(VR_ST_At(320, 31, 0), TRUE);
                VR_Burn_Ignite(vr_st_c, p, vr_st_c.origin, '0 0 1', VR_BURN_TOUCH);
                VR_ST_Next(2);
            }
            else if(t > 8)
            {
                VR_ST_End(FALSE, sprintf("the crate by the burning grunt never caught (grunt burning %g, health %g; boxes %v-%v and %v-%v)",
                    vr_st_a.burn_fire != world, vr_st_a.health, vr_st_a.absmin, vr_st_a.absmax, vr_st_b.absmin, vr_st_b.absmax));
            }
        }
        else if(vr_st_d.burn_fire != world)
        {
            VR_ST_End(TRUE, sprintf("the burning crate lit the grunt after %.1f s", t));
        }
        else if(t > 15 || vr_st_c.burn_fire == world)
        {
            VR_ST_End(FALSE, "the grunt by the burning crate never caught");
        }
        return;
    }
    if(vr_st_case == 11)
    {
        if(vr_st_step == 0)
        {
            vr_st_a = VR_ST_Make(vr_crate, "vr_crate", VR_ST_At(220, 40, 0), 0);  // 40 from the blast: inside half its radius (70)
            vr_st_b = VR_ST_Make(vr_crate, "vr_crate", VR_ST_At(220, -110, 0), 0); // 110: past half, inside the radius (140)
            vr_st_c = VR_ST_Grunt(VR_ST_At(260, 0, 0), TRUE);
            VR_ST_Next(1);
        }
        else if(vr_st_step == 1 && t > 0.3)
        {
            vr_st_n = vr_burn_blasted;
            entity blast = spawn();
            setorigin(blast, VR_ST_At(220, 0, 0));
            T_RadiusDamage(blast, p, 100, world);
            remove(blast);
            float lit = vr_burn_blasted - vr_st_n;
            VR_ST_End(lit == 1 && vr_burn_blast_last == vr_st_a && vr_st_c.burn_fire == world,
                sprintf("%g lit by the blast: %s; the grunt %s", lit, vr_burn_blast_last == vr_st_a ? "the near crate" : "not the near crate",
                    vr_st_c.burn_fire == world ? "not" : "burning"));
        }
        return;
    }
    if(vr_st_case == 12)
    {
        entity b = find(world, classname, "func_button");
        while(b != world && (b.state != STATE_BOTTOM || b.touch != button_touch_any))
        {
            b = find(b, classname, "func_button");
        }
        if(b == world)
        {
            VR_ST_End(FALSE, "no touch button resting in this map");
            return;
        }
        entity g = VR_ST_Grunt(VR_ST_At(160, 0, 0), TRUE);
        vector md = b.movedir;
        b.movedir = '0 0 -8'; // (as a floor button)
        entity oself = self;
        entity oother = other;
        cvar_set("vr_monsters_press_buttons", "0");
        self = b;
        other = g;
        button_touch_any();
        float off = b.state;
        cvar_set("vr_monsters_press_buttons", "1");
        button_touch_any();
        float on = b.state;
        cvar_set("vr_monsters_press_buttons", "0");
        self = oself;
        other = oother;
        b.movedir = md;
        VR_ST_End(off == STATE_BOTTOM && on != STATE_BOTTOM, sprintf("off: %s, on: %s", off == STATE_BOTTOM ? "not pressed" : "pressed",
            on != STATE_BOTTOM ? "pressed" : "not pressed"));
        return;
    }
    if(vr_st_case == 14)
    {
        if(vr_st_step == 0)
        {
            vr_st_a = VR_ST_Grunt(VR_ST_At(200, 70, 0), FALSE);
            vr_st_b = VR_ST_Grunt(VR_ST_At(200, -70, 0), FALSE);
            VR_ST_Next(1);
        }
        else if(vr_st_step == 1 && t > 0.5)
        {
            if(!vr_st_b.vr_ag_prop)
            {
                VR_ST_End(FALSE, "the second grunt holds no weapon");
                return;
            }
            VR_AG_WeaponLine(vr_st_b);
            vector mid = (vr_ag_wa + vr_ag_wb) * 0.5;
            vr_st_c = VR_ST_Make(item_rockets, "item_rockets", vr_st_a.origin + '0 0 200', 0);

            vr_st_n = 0;
            VR_ST_Next(3);
        }
        else if(vr_st_step == 3 && t > 0.3)
        {
            VR_AG_WeaponLine(vr_st_b);
            vector mid2 = (vr_ag_wa + vr_ag_wb) * 0.5;
            vector dir = normalize(mid2 - vr_st_a.origin);
            setorigin(vr_st_c, mid2 - dir * 60);
            vr_st_c.owner = vr_st_a; // (thrown by the other grunt)
            vr_st_c.flags = vr_st_c.flags - (vr_st_c.flags & FL_ONGROUND);
            vr_st_c.velocity = dir * 900;
            VR_ST_Next(2);
        }
        else if(vr_st_step == 2)
        {
            if(!vr_st_b.vr_ag_prop)
            {
                // (the thrower credited: VR_AG_Thrower names the grunt; grunts of one kind don't fight each other, Quake's rule)
                VR_ST_End(vr_ag_last_try_by == vr_st_a, sprintf("disarmed by a box thrown by %s",
                    vr_ag_last_try_by == vr_st_a ? "the other grunt (credited)" : "nobody credited"));
            }
            else if(t > 2)
            {
                VR_ST_End(FALSE, "the thrown crate never knocked its weapon away");
            }
        }
        return;
    }
    if(vr_st_case == 13)
    {
        if(vr_st_step == 0)
        {
            vr_st_a = VR_ST_Grunt(VR_ST_At(160, 0, 0), TRUE);
            VR_Status_Ignite(vr_st_a, p);
            float lit = (VR_Status(vr_st_a) & VR_STATUS_BURNING) && vr_st_a.burn_fire != world;
            VR_Status_Douse(vr_st_a, "the status test");
            float out = !(VR_Status(vr_st_a) & VR_STATUS_BURNING) && vr_st_a.burn_fire == world;
            float dry0 = !(VR_Status(vr_st_a) & VR_STATUS_WET) && !(VR_Status(p) & VR_STATUS_WET);
            VR_Status_Soak(vr_st_a, 1);
            VR_Status_Soak(p, 2);
            float wet = VR_Status_Wet(vr_st_a) == 1 && (VR_Status(p) & VR_STATUS_WET);
            if(!(lit && out && dry0 && wet))
            {
                VR_ST_End(FALSE, sprintf("lit %g, doused %g, dry at first %g, soaked %g", lit, out, dry0, wet));
                return;
            }
            vr_st_n = cvar_hget(cvarh_vr_wounds_wet_time);
            cvar_set("vr_wounds_wet_time", "0.4");
            VR_ST_Next(1);
        }
        else if(t > 0.5)
        {
            float left = VR_Status_Wet(vr_st_a) + VR_Status_Wet(p);
            cvar_set("vr_wounds_wet_time", ftos(vr_st_n));
            VR_ST_End(left == 0, sprintf("lit, doused, soaked; dried after 0.5 s of 0.4: wetness left %g", left));
        }
        return;
    }
    if(vr_st_case == 16)
    {
        if(vr_st_step == 0)
        {
            vr_st_a = VR_ST_Grunt(VR_ST_At(200, 60, 0), TRUE);
            vr_st_b = VR_ST_Grunt(VR_ST_At(200, -60, 0), TRUE);
            vr_st_a.health = vr_st_b.health = vr_st_a.max_health = vr_st_b.max_health = 100; // (alive after the hit: its chance is rolled)
            vr_st_a.vr_hst_last_chance = vr_st_b.vr_hst_last_chance = -1;
            vr_st_c = VR_ST_Make(vr_crate, "vr_crate", vr_st_a.origin + '0 0 120', 0); // dropped: no hand, no owner
            vr_st_d = VR_ST_Make(vr_crate, "vr_crate", vr_st_b.origin + '0 0 120', 0); // let go of by your hand
            vr_st_d.carry_letgo = p;
            vr_st_d.carry_letgo_time = time;
            vr_st_c.flags = vr_st_c.flags - (vr_st_c.flags & FL_ONGROUND);
            vr_st_d.flags = vr_st_d.flags - (vr_st_d.flags & FL_ONGROUND);
            VR_ST_Next(1);
        }
        else if(vr_st_step == 1)
        {
            if(t > 0.3)
            {
                vr_st_d.carry_letgo_time = time;
                setorigin(vr_st_c, vr_st_a.origin + '0 0 120');
                setorigin(vr_st_d, vr_st_b.origin + '0 0 120');
                vr_st_c.velocity = vr_st_d.velocity = '0 0 -250';
                VR_ST_Next(2);
            }
        }
        else
        {
            float ca = vr_st_a.vr_hst_last_chance;
            float cb = vr_st_b.vr_hst_last_chance;
            if((ca >= 0 && cb >= 0) || t > 3)
            {
                VR_ST_End(ca > 0 && cb > 0 && fabs(ca - cb) < 0.01, sprintf("the dropped crate's stagger chance %.3f, the one let go of by your hand %.3f",
                    ca, cb));
            }
        }
        return;
    }
    if(vr_st_case == 15)
    {
        if(vr_st_step == 0)
        {
            vr_st_a = VR_ST_Grunt(VR_ST_At(220, 0, 0), TRUE);
            vr_st_b = VR_ST_Make(light_torch_small_walltorch, "light_torch_small_walltorch", VR_ST_At(300, 0, 6), 0);
            vr_st_c = VR_ST_Make(vr_crate, "vr_crate", VR_ST_At(350, 0, 0), 1);
            vr_st_d = VR_ST_Make(vr_crate, "vr_crate", VR_ST_At(392, 0, 0), 1);
            VR_ST_Next(1);
        }
        else if(vr_st_step == 1 && t > 0.3)
        {
            // A rocket's blast (120) between you and the grunt kills it and flings its body at the torch
            // (vr_kill_blast_fling: its ragdoll made as it dies). The torch is kept out of the blast's own damage, so what
            // knocks it off here is the body alone.
            vr_mount_last_by = world;
            entity bl = spawn();
            setorigin(bl, VR_ST_At(185, 0, -8));
            float td = vr_st_b.takedamage;
            vr_st_b.takedamage = DAMAGE_NO;
            T_RadiusDamage(bl, p, 120, world);
            vr_st_b.takedamage = td;
            remove(bl);
            VR_ST_Next(3);
        }
        else if(vr_st_step == 3)
        {
            if(time >= vr_st_n)
            {
                vr_st_n = time + 0.1;
                dprint(sprintf("systems_test: 15: at %.2f s the body at %v (ragdoll %g), the torch at %v\n", t, vr_st_a.origin,
                    isragdoll(vr_st_a), vr_st_b.origin));
            }
            if(vr_st_b.wt_state != 0)
            {
                if(vr_mount_last_by == world || vr_mount_last_by.classname != vr_st_a.classname) // (the corpse: its ragdoll's entity)
                {
                    VR_ST_End(FALSE, sprintf("the torch was knocked off by %s, not the flung corpse",
                        vr_mount_last_by == world ? "the blast" : vtos(vr_mount_last_by.origin)));
                    dprint(sprintf("systems_test: 15: the corpse %s, the torch %s\n", vtos(vr_st_a.origin), vtos(vr_st_b.origin)));
                    return;
                }
                dprint(sprintf("systems_test: 15: the flung corpse knocked the torch off after %.2f s\n", t));
                VR_ST_Next(4);
            }
            else if(t > 4)
            {
                VR_ST_End(FALSE, sprintf("the blast's body never knocked the torch off (corpse at %v, torch at %v)", vr_st_a.origin, vr_st_b.origin));
            }
        }
        else if(vr_st_c.burn_fire != world || vr_st_d.burn_fire != world)
        {
            VR_ST_End(TRUE, sprintf("the fallen torch lit a crate after %.1f s", t));
        }
        else if(t > 8)
        {
            VR_ST_End(FALSE, sprintf("the torch (lit %g, at %v) lit no crate", vr_st_b.wt_fire, vr_st_b.origin));
        }
        return;
    }
};

void() VR_SysTest_Frame =
{
    if(vr_st_case)
    {
        VR_ST_Run();
    }
    float ask = cvar_hget(cvarh_vr_systems_test);
    if(!ask)
    {
        return;
    }
    cvar_set("vr_systems_test", "0");
    if(ask == 1)
    {
        VR_SysTest_MatDump();
    }
    if(ask >= 10 && ask <= 16)
    {
        VR_Test_Begin("systems_test");
        vr_st_case = ask;
        VR_ST_Next(0);
    }
};
#endif
