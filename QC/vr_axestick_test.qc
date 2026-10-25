#ifdef QVR_TESTS
// vr_axestick's test harness (moved out of vr_axestick.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// Tests (impulse 209; Debug > Tests > Thrown Axe): an axe thrown straight ahead (the view's yaw) from the eyes at
// vr_test_axe_speed m/s, turned by vr_test_axe: 0 its blade first (the edge upright), 1 flat (the blade's side first),
// 2 handle first, 3 blade first and spinning end over end. vr_test_axe_damage 0: it doesn't hurt what it strikes;
// vr_test_axe_dist: you are moved first to that many units from what is ahead; vr_test_axe_at: at the nearest live
// monster (1), door (2), loose prop (3) or wall button (4) instead, from its side facing you, level with its middle.
// The nearest target of the axe test's kind (vr_test_axe_at: 1 a live monster, 2 a door, 3 a loose prop, 4 a wall button
// pressed by touch), or world.
entity(float xKind) VR_AxeStick_TestTarget =
{
    entity best = world;
    float bestDist = 100000;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        float ok = FALSE;
        if(xKind == 1)
        {
            ok = ((e.flags & FL_MONSTER) && e.health > 0);
        }
        else if(xKind == 2)
        {
            ok = (e.classname == "door" || e.classname == "func_door"); // (doors.qc names them "door")
        }
        else if(xKind == 3)
        {
            ok = (e.vr_rigid && (e.movetype == MOVETYPE_TOSS || e.movetype == MOVETYPE_BOUNCE) && e.classname != "thrown_weapon");
        }
        else if(xKind == 4)
        {
            ok = (e.classname == "func_button" && !e.health); // (a shot button: shot, not pressed by a throw)
        }
        float d = vlen((e.absmin + e.absmax) * 0.5 - self.origin);
        if(ok && d < bestDist)
        {
            best = e;
            bestDist = d;
        }
    }
    return best;
};

void() VR_AxeStick_TestThrow =
{
    makevectors('0 1 0' * self.v_viewangle_y);
    vector F = v_forward;
    float dist = cvar("vr_test_axe_dist");
    float atKind = cvar("vr_test_axe_at");
    entity target = world;
    vector eye = self.origin + '0 0 22';
    if(atKind > 0)
    {
        // At a target: from its side facing you (a door: square to it), level with its middle, `dist` away (or where
        // you are).
        target = VR_AxeStick_TestTarget(atKind);
        if(target == world)
        {
            dprint("axe test: no target of that kind\n");
            return;
        }
        vector c = (target.absmin + target.absmax) * 0.5 + '0 0 1' * cvar("vr_test_axe_up"); // (_up: above its middle)
        F = c - eye;
        if(atKind == 4 && (target.movedir_x || target.movedir_y))
        {
            F = target.movedir; // (into the wall: the way it is pressed)
        }
        else if(atKind == 2)
        {
            if(target.size_x <= target.size_y)
            {
                F = '1 0 0' * (F_x >= 0 ? 1 : -1);
            }
            else
            {
                F = '0 1 0' * (F_y >= 0 ? 1 : -1);
            }
        }
        F_z = 0;
        F = normalize(F);
        if(dist <= 0)
        {
            dist = vlen(c - eye);
        }
        eye = c - F * dist;
        setorigin(self, eye - '0 0 22');
        self.velocity = '0 0 0';
        dist = 0;
    }
    vector U = '0 0 1';
    vector L;
    L_x = -F_y;
    L_y = F_x;
    L_z = 0;
    float mode = cvar("vr_test_axe");
    // The model's forward (x) from where its blade faces (`bf`) and its handle points (`hp`), square to each other: the
    // blade faces (0.94, 0, -0.34) in its axes, the handle (0.34, 0, 0.94), the head thin along y.
    vector bf = F;
    vector hp = U;
    if(mode == 2)
    {
        bf = U;
        hp = F * -1; // the handle's end ahead
    }
    vector a = vectoangles(bf * 0.9398 + hp * 0.3417);
    if(mode == 2)
    {
        a_z = 180; // (its forward tilted back past the upright: vectoangles turns it round, the roll turns it back)
    }
    if(mode == 1)
    {
        a = vectoangles(L * -1); // its forward to the right: its left (y, across the blade) ahead
    }
    a_x = a_x * -1; // (MakeThrown turns a view's pitch into a model's)
    // A hand's throw (4 overhand, 5 sidearm, 6 sloppy, 8 upright, 9 knife-style, 10 spear-like, 11 flat, 12 handle
    // first: at random): from blade first (8-12: from their own hold), turned (roll about the way it goes, pitch about
    // your left, its head forward, yaw about the upright) and spun (rad/s about the way it goes, your left: end over
    // end, its top going ahead, the upright). vr_test_axe_loft: aimed up to meet what is ahead level with your eyes.
    // (7, every turn and spin set by cvars, was removed 2026-10-09: the spin investigation it served is done.)
    vector spin = '0 0 0';
    if(mode >= 4)
    {
        vector t = '0 0 0'; // roll, pitch, yaw
        vector w = '0 0 0'; // about F, L, U
        if(mode == 4) // overhand: end over end
        {
            t = randomVec(1) * 15;
            t_y = crandom() * 30;
            w_y = 8 + random() * 8;
            w_x = crandom() * 1.5;
            w_z = crandom() * 1.5;
        }
        else if(mode == 5) // sidearm: the blade level, spun about the upright either way
        {
            t_x = 90 + crandom() * 15;
            t_y = crandom() * 10;
            t_z = crandom() * 30;
            w_z = (random() < 0.5 ? -1 : 1) * (8 + random() * 8);
            w_x = crandom() * 1.5;
            w_y = crandom() * 1.5;
        }
        else if(mode == 6) // sloppy: turned and spun any way, end over end the most
        {
            t = randomVec(1) * 45;
            w = randomVec(1) * 6;
            w_y = w_y + 4 + random() * 8;
        }
        else if(mode == 8) // upright: the handle upright, the blade ahead, pushed (a little turn either way)
        {
            t = randomVec(1) * 12;
            w = randomVec(1) * 1.5;
            w_y = w_y + crandom() * 3;
        }
        else if(mode == 9) // knife-style (a throwing knife's blade grip): the handle ahead, the edge up, flicked end over end
        {
            bf = U;
            hp = F * -1;
            t = randomVec(1) * 15;
            t_y = crandom() * 20;
            w_y = 6 + random() * 8;
            w_x = crandom() * 1.5;
            w_z = crandom() * 1.5;
        }
        else if(mode == 10) // spear-like: the head ahead, the handle pointing at it, the blade turned any way about it
        {
            hp = F;
            bf = VR_AxeStick_Turn(U, F, random() * 360);
            t = randomVec(1) * 12;
            w = randomVec(1) * 1.5;
        }
        else if(mode == 11) // flat: the blade's side ahead, its plane square to the way it goes, spun in that plane
        {
            bf = L * -1;
            hp = U;
            t = randomVec(1) * 15;
            w = randomVec(1) * 1.5;
            w_x = crandom() * 10;
        }
        else if(mode == 12) // handle first: the handle's end ahead, the edge up, hardly turning
        {
            bf = U;
            hp = F * -1;
            t = randomVec(1) * 15;
            w = randomVec(1) * 1.5;
        }
        vector fw = bf * 0.9398 + hp * 0.3417;
        vector up = hp * 0.9398 - bf * 0.3417;
        fw = VR_AxeStick_Turn(VR_AxeStick_Turn(VR_AxeStick_Turn(fw, F, t_x), L, t_y), U, t_z);
        up = VR_AxeStick_Turn(VR_AxeStick_Turn(VR_AxeStick_Turn(up, F, t_x), L, t_y), U, t_z);
        a = VR_AxeStick_Angles(fw, up);
        spin = F * w_x + L * w_y + U * w_z;
        dprint(sprintf("axe test: hand throw roll %.0f pitch %.0f yaw %.0f, spin %.1f %.1f %.1f (along, left, up)\n", t_x, t_y,
            t_z, w_x, w_y, w_z));
    }
    // vr_test_axe_dist: you moved along so that what is ahead of your eyes is that far (units) from you.
    if(dist > 0)
    {
        traceline(self.origin + '0 0 22', self.origin + '0 0 22' + F * 4000, TRUE, self);
        if(trace_fraction < 1)
        {
            setorigin(self, self.origin + F * (4000 * trace_fraction - dist));
            self.velocity = '0 0 0';
        }
    }
    vector from = self.origin + '0 0 22' + F * 40 + L * cvar("vr_test_axe_side"); // (clear of your body)
    float speed = cvar("vr_test_axe_speed") * VRMetersToUnits();
    float hurt = cvar("vr_test_axe_damage") != 0;
    // vr_test_axe_weapon: that weapon instead, hurting as a hand's throw does (its full-damage speed: DropWeaponInHandScaled).
    float wid = cvar("vr_test_axe_weapon");
    if(wid <= 0)
    {
        wid = WID_AXE;
    }
    entity axe = CreateThrownWeapon(self, wid, 0, WeaponInst_New(0), from, a, F * speed - self.velocity * VR_Throw_Inherit(self), '0 0 0',
        FALSE, hurt, WeaponIdToThrowDamage(wid));
    if(cvar("vr_test_axe_weapon") > 0)
    {
        axe.throwdmgspeed = 700 * VRMetersToUnits() / 120;
    }
    if(mode >= 4 && cvar("vr_test_axe_loft") && speed > 0)
    {
        traceline(from, from + F * 4000, TRUE, self);
        float g = (axe.gravity ? axe.gravity : 1) * cvar("sv_gravity");
        if(trace_fraction < 1) // (nothing ahead: not lofted, or it went up at 140 m/s)
        {
            axe.velocity_z = axe.velocity_z + 0.5 * g * (4000 * trace_fraction / speed);
        }
    }
    if(mode == 3)
    {
        axe.vr_spin = L * 12; // end over end (rad/s), its top going ahead, the blade coming down
    }
    if(mode >= 4)
    {
        axe.vr_spin = spin;
    }
    // vr_test_axe_what (the spin in the air's tests): 1 a gib, 2 an explosive box (that never blows up) thrown instead,
    // the same way (the axe's place, velocity and spin; the axe itself removed).
    float what = cvar("vr_test_axe_what");
    if(what == 1 || what == 2)
    {
        entity thing = world;
        if(what == 1)
        {
            thing = spawn();
            thing.classname = "test_gib";
            setmodel(thing, "progs/gib1.mdl");
            setsize(thing, '0 0 0', '0 0 0');
            thing.movetype = MOVETYPE_BOUNCE;
            thing.solid = SOLID_NOT;
            thing.vr_rigid = 1;
            setorigin(thing, from);
        }
        else
        {
            entity oldSelf = self;
            self = spawn();
            setorigin(self, from - '16 16 32');
            misc_explobox();
            thing = self;
            self = oldSelf;
            thing.takedamage = DAMAGE_NO;
            setorigin(thing, from - '16 16 32');
            thing.flags = thing.flags - (thing.flags & FL_ONGROUND);
        }
        thing.angles = axe.angles;
        thing.velocity = axe.velocity;
        thing.vr_spin = axe.vr_spin;
        thing.owner = self;
        remove(axe);
        axe = thing;
    }
    traceline(from, from + F * 2000, FALSE, self);
    dprint(sprintf("axe test: thrown %s at %.1f m/s (%.0f u/s) from %v, angles %v; ahead: %s %.0f units away\n",
        mode == 1 ? "flat" : mode == 2 ? "handle first" : mode == 3 ? "spinning" : "blade first",
        cvar("vr_test_axe_speed"), speed, from, axe.angles, trace_ent == world ? "the level" : trace_ent.classname,
        2000 * trace_fraction));
};

// Tests (impulse 207): you moved so that an empty hand (the main one first) is on the nearest stuck axe's handle
// (where a hand holds it), to grip it.
void() VR_AxeStick_TestReach =
{
    entity e = VR_AxeStick_Nearest(self.origin);
    if(e == world)
    {
        dprint("axe test: no stuck axe\n");
        return;
    }
    float hand = VRIsHandEmpty(self, cVR_MainHand) ? cVR_MainHand : cVR_OffHand; // (an empty one takes it)
    vector d = forcegrabpoint(e) - VRGetHandPos(hand);
    // Along the floor, as far as your body fits (the hand's height, and its reach if you stop short, are the hand's:
    // vr_mock_hand).
    float up = d_z;
    d_z = 0;
    tracebox(self.origin, self.mins, self.maxs, self.origin + d, FALSE, self);
    d_z = up;
    setorigin(self, trace_endpos);
    self.velocity = '0 0 0';
    dprint(sprintf("axe test: you moved %v (%.0f%% of the way), your %s hand on the handle of %s stuck in %s at %v\n", d,
        trace_fraction * 100, hand == cVR_MainHand ? "main" : "off", e.netname, e.vr_stick_ent == world ? "the level" : e.vr_stick_ent.classname, forcegrabpoint(e)));
};

// Tests (impulse 208 with vr_test_axe_host 1 or 2, once): what each axe is stuck in that is a prop pushed up and
// across at 200 u/s (1: the axe follows it), or broken (2: an explosive box or a crate dealt 1000, a pickup taken by
// you, your health lowered for it: the axe falls).
void() VR_AxeStick_TestHost =
{
    float what = cvar("vr_test_axe_host");
    cvar_set("vr_test_axe_host", "0");
    entity p = self;
    for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        entity h = e.vr_stick_ent;
        if(e.weapon != WID_AXE || e.vr_stick_kind != AXESTICK_PROP || h == world)
        {
            continue;
        }
        if(what == 1)
        {
            makevectors('0 1 0' * p.v_viewangle_y);
            float pushed = physicspush(h, (h.absmin + h.absmax) * 0.5, v_right * 140 + '0 0 140');
            dprint(sprintf("axe test: %s pushed (%g) from %v\n", h.classname, pushed, h.origin));
        }
        else if(what == 2)
        {
            dprint(sprintf("axe test: breaking %s (health %g, takedamage %g)\n", h.classname, h.health, h.takedamage));
            if(h.takedamage)
            {
                T_Damage(h, p, p, 1000);
            }
            else if(h.touch)
            {
                p.health = 10;
                other = p;
                self = h;
                self.touch();
                self = p;
                p.health = 100;
            }
        }
    }
};

// Tests (impulse 208): prints each stuck axe: what it is in, where, its blade.
void() VR_AxeStick_TestReport =
{
    if(cvar("vr_test_axe_host"))
    {
        VR_AxeStick_TestHost();
    }
    dprint(sprintf("axe test: you at %v, head %v, view %v\n", self.origin, VRGetHeadPos(self), self.v_viewangle));
    for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        if(e.weapon != WID_AXE)
        {
            continue;
        }
        // (sprintf takes 7 values at most.)
        dprint(sprintf("axe test: kind %g in %s (health %g, at %v), origin %v angles %v, movetype %g\n",
            e.vr_stick_kind, e.vr_stick_ent == world ? "the level" : e.vr_stick_ent.classname,
            e.vr_stick_ent.health, e.vr_stick_ent.origin, e.origin, e.angles, e.movetype));
        dprint(sprintf("axe test:   its edge at %v facing %v\n", e.vr_stick_at, e.vr_stick_dir));
    }
};
#endif
