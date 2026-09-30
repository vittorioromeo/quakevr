// vr_axestick.qc -- thrown axes stick (docs/vr-port/ROUND21.md, "Thrown axes stick"; engine: vr_axestick.cpp).
//
// The engine sees a thrown axe's blade go into something blade first, fast and square enough (vr_axestick_speed,
// _angle, _incidence), puts it there with its edge in (vr_axestick_depth), keeps it there as that thing moves (a
// door, a prop, a monster's model) and calls VR_AxeStick_Stuck. Here: the blow (as its touch), the sound, then:
// - in a monster, it bleeds it: vr_axestick_bleed health a second, the thrower's damage (no pain each time), blood
//   from the wound; it falls out when the monster dies or is gibbed, or what it is in goes (a prop taken or blown up);
// - a hand gripping it takes it (its handtouch: wpnthrow_handtouch), out of what it is in;
// - a force grab tugs it (vr_axestick_tug seconds, rocking it and the hand) before it comes free and flies to the hand.
// vr_axestick 0 lets every stuck axe fall.

float AXESTICK_LEVEL = 1;       // the level or a brush entity
float AXESTICK_PROP = 2;        // a prop
float AXESTICK_MODEL = 3;       // a monster's model
float AXESTICK_MONSTERBOX = 4;  // a monster's box
float AXESTICK_OUT = -1;        // it came out: it never sticks again
float AXESTICK_BLEED_TICK = 0.5; // seconds between two bleedings

.entity vr_stick_by;      // who threw it: the bleeding's attacker
.float  vr_stick_model;   // what it is in's model when it stuck (another, or none: gibbed, gone)
.float  vr_stick_next;    // the next bleeding
.float  vr_stick_owed;    // the bleeding's damage not dealt yet (a fraction of a point)
.float  vr_stick_tug;     // a force grab's tug: when it comes free (0: none)
.float  vr_stick_tugstart;
.float  vr_stick_tughand;
.entity vr_stick_tugger;

float(entity e) VR_AxeStick_IsStuck =
{
    return e.vr_stick_kind > 0;
};

float(entity e) VR_AxeStick_Tugging =
{
    return e.vr_stick_kind > 0 && e.vr_stick_tug > 0;
};

float(entity e) VR_AxeStick_InMonster =
{
    return e.vr_stick_kind == AXESTICK_MODEL || e.vr_stick_kind == AXESTICK_MONSTERBOX;
};

// The engine: `self` has just stuck in `other` (what: xKind), its edge going into it at xSpeed units/s; self.velocity
// is still the throw's.
void(float xKind, float xSpeed) VR_AxeStick_Stuck =
{
    // The throw's blow, as its touch would have struck (damage, blood, the sound), unless it had struck already (its
    // hit box meets a monster before its blade does).
    if(other != world && other.takedamage && self.throwhit == QVR_THROWHIT_NEVER_HIT &&
       !GetThrownWeaponDisappearOnHit(self))
    {
        forcegrabbable_touch();
    }
    self.throwhit = QVR_THROWHIT_HIT;
    self.vr_stick_by = self.owner;
    self.vr_stick_model = other.modelindex;
    self.vr_stick_next = time + AXESTICK_BLEED_TICK;
    self.vr_stick_owed = 0;
    self.vr_stick_tug = 0;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT_BUT_TOUCHABLE; // (a hand's grip, a force grab)
    self.velocity = '0 0 0';
    self.avelocity = '0 0 0';
    self.vr_spin = '0 0 0';
    self.flags = self.flags - (self.flags & FL_ONGROUND);

    if(VR_AxeStick_InMonster(self))
    {
        sound(self, CHAN_AUTO, "player/axhit1.wav", 1, ATTN_NORM);
        SpawnBlood(self.vr_stick_at, self.vr_stick_dir * -60, 12);
    }
    else
    {
        sound(self, CHAN_AUTO, "player/axhit2.wav", 1, ATTN_NORM);
        particle2(self.vr_stick_at, '0 0 0', QVR_PARTICLE_PRESET_BULLETPUFF, 12);
    }
    if(cvar("vr_debug_axestick"))
    {
        dprint(sprintf("axestick: %s stuck in %s (kind %g) at %.0f u/s, thrown by %s\n", self.netname,
            other == world ? "the level" : other.classname, xKind, xSpeed, self.vr_stick_by.classname));
    }
};

// Out of what it is in: a rigid body again (it never sticks again), going at `xVel`.
void(entity e, vector xVel) VR_AxeStick_Free =
{
    e.vr_stick_kind = AXESTICK_OUT;
    e.vr_stick_ent = world;
    e.vr_stick_ofs = '0 0 0'; // (the engine's; named here so that the compiler keeps them)
    e.vr_stick_ang = '0 0 0';
    e.vr_stick_anchor = '0 0 0';
    e.vr_stick_edge = 0;
    e.vr_stick_vel = '0 0 0';
    e.vr_stick_spin = '0 0 0';
    e.vr_stick_wiggle = 0;
    e.vr_stick_tug = 0;
    e.movetype = MOVETYPE_TOSS;
    e.solid = SOLID_NOT_BUT_TOUCHABLE;
    e.throwhit = QVR_THROWHIT_HIT; // (no blow when it lands)
    e.flags = e.flags - (e.flags & FL_ONGROUND);
    e.velocity = xVel;
    e.vr_spin = '0 0 0';
    setorigin(e, e.origin);
};

// Pulled out (a hand, a force grab): blood out of a monster, dust out of a wall.
void(entity e) VR_AxeStick_PulledOut =
{
    if(VR_AxeStick_InMonster(e))
    {
        SpawnBlood(e.vr_stick_at, e.vr_stick_dir * -120, 20);
        sound(e, CHAN_AUTO, "player/axhit1.wav", 0.5, ATTN_NORM);
    }
    else
    {
        particle2(e.vr_stick_at, e.vr_stick_dir * -20, QVR_PARTICLE_PRESET_BULLETPUFF, 8);
    }
    if(cvar("vr_debug_axestick"))
    {
        dprint(sprintf("axestick: %s pulled out of %s\n", e.netname, e.vr_stick_ent == world ? "the level" :
            e.vr_stick_ent.classname));
    }
};

// What it is in has let go of it (died, gibbed, gone; or vr_axestick 0): it falls, with that thing's motion.
void(entity e) VR_AxeStick_Fall =
{
    entity h = e.vr_stick_ent;
    if(cvar("vr_debug_axestick"))
    {
        dprint(sprintf("axestick: %s falls out of %s (its health %g)\n", e.netname, h == world ? "the level" : h.classname,
            h.health));
    }
    vector v = '0 0 0';
    if(h != world && h.modelindex == e.vr_stick_model)
    {
        v = h.velocity;
    }
    setorigin(e, e.origin - e.vr_stick_dir * 2); // (its edge out of the surface)
    VR_AxeStick_Free(e, v - e.vr_stick_dir * 40);
};

// A force grab's pull of a stuck axe (its flick, by `xPlayer`'s hand `xHand`): it tugs a moment first.
void(entity e, entity xPlayer, float xHand) VR_AxeStick_Tug =
{
    e.vr_stick_tugstart = time;
    e.vr_stick_tug = time + max(0, cvar_hget(cvarh_vr_axestick_tug));
    e.vr_stick_tugger = xPlayer;
    e.vr_stick_tughand = xHand;
    haptic(xHand, 0.0, max(0.05, e.vr_stick_tug - time), 40, 0.8);
    if(cvar("vr_debug_axestick"))
    {
        dprint(sprintf("axestick: %s tugged by a force grab (%.2f s)\n", e.netname, e.vr_stick_tug - time));
    }
};

// While stuck (its think, every 0.02 s): what it is in still holds it; a tug; the bleeding.
void() VR_AxeStick_Think =
{
    entity h = self.vr_stick_ent;
    if(!cvar_hget(cvarh_vr_axestick) ||
       (h != world && (h.modelindex != self.vr_stick_model || h.model == string_null)) ||
       (VR_AxeStick_InMonster(self) && (h.health <= 0 || h.solid == SOLID_NOT)))
    {
        VR_AxeStick_Fall(self);
        return;
    }

    if(self.vr_stick_tug)
    {
        if(time < self.vr_stick_tug)
        {
            // Rocked about its edge, harder towards the end.
            float k = (time - self.vr_stick_tugstart) / max(0.01, self.vr_stick_tug - self.vr_stick_tugstart);
            self.vr_stick_wiggle = sin((time - self.vr_stick_tugstart) * 75) * (3 + 7 * k);
            return;
        }
        // Free: it flies to the hand (the force grab's flight, from here).
        entity axe = self;
        entity p = self.vr_stick_tugger;
        float hand = self.vr_stick_tughand;
        VR_AxeStick_PulledOut(axe);
        VR_AxeStick_Free(axe, '0 0 0');
        if(p.classname == "player" && p.health > 0 && VRIsHandEmpty(p, hand))
        {
            self = p;
            VR_Forcegrab_Launch(axe, hand);
            VR_Forcegrab_SetPulled(hand, axe);
            self = axe;
        }
        return;
    }

    // The bleeding: the thrower's damage, without a flinch each time (it bleeds, it isn't struck).
    float rate = cvar_hget(cvarh_vr_axestick_bleed);
    if(VR_AxeStick_InMonster(self) && time >= self.vr_stick_next)
    {
        self.vr_stick_next = max(self.vr_stick_next + AXESTICK_BLEED_TICK, time); // (its cadence kept: the rate exact)
        // Whole points (Quake's damage is rounded up): the fraction owed until the next.
        self.vr_stick_owed = self.vr_stick_owed + max(0, rate) * AXESTICK_BLEED_TICK;
        float dmg = floor(self.vr_stick_owed);
        if(dmg >= 1 && h.takedamage)
        {
            self.vr_stick_owed = self.vr_stick_owed - dmg;
            SpawnBlood(self.vr_stick_at - self.vr_stick_dir * 2, self.vr_stick_dir * -40 - '0 0 20', rate * 2);
            entity by = self.vr_stick_by;
            if(by == world || by.classname != "player")
            {
                by = self; // (the thrower gone)
            }
            float before = h.health;
            void(entity attacker, float damage) pain = h.th_pain;
            h.th_pain = SUB_NullPain;
            VR_SetHitKind(QVR_HITKIND_BLEED, -1, self.vr_stick_at, self.vr_stick_dir);
            T_Damage(h, self, by, dmg);
            VR_ClearHitKind();
            if(h.th_pain == SUB_NullPain)
            {
                h.th_pain = pain;
            }
            if(cvar("vr_debug_axestick"))
            {
                dprint(sprintf("axestick: %s bleeds %s at %.2f s: health %g -> %g (%g a second, by %s)\n", self.netname,
                    h.classname, time, before, h.health, rate, by.classname));
            }
        }
    }
};

// Tests (impulse 209; Debug > Tests > Thrown Axe): an axe thrown straight ahead (the view's yaw) from the eyes at
// vr_test_axe_speed m/s, turned by vr_test_axe: 0 its blade first (the edge upright), 1 flat (the blade's side first),
// 2 handle first, 3 blade first and spinning end over end. vr_test_axe_damage 0: it doesn't hurt what it strikes;
// vr_test_axe_dist: you are moved first to that many units from what is ahead; vr_test_axe_at: at the nearest live
// monster (1), door (2) or loose prop (3) instead, from its side facing you, level with its middle.
// The nearest target of the axe test's kind (vr_test_axe_at: 1 a live monster, 2 a door, 3 a loose prop), or world.
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
        float d = vlen((e.absmin + e.absmax) * 0.5 - self.origin);
        if(ok && d < bestDist)
        {
            best = e;
            bestDist = d;
        }
    }
    return best;
};

// `v` turned `deg` degrees about the unit `k` (right-handed).
vector(vector v, vector k, float deg) VR_AxeStick_Turn =
{
    float c = cos(deg * (3.14159265 / 180));
    float s = sin(deg * (3.14159265 / 180));
    vector kv;
    kv_x = k_y * v_z - k_z * v_y;
    kv_y = k_z * v_x - k_x * v_z;
    kv_z = k_x * v_y - k_y * v_x;
    return v * c + kv * s + k * ((k * v) * (1 - c));
};

// A model's angles (as a view's pitch: MakeThrown turns it) for its forward `fw` and up `up` (square to each other).
vector(vector fw, vector up) VR_AxeStick_Angles =
{
    vector a = vectoangles(fw);
    a_x = a_x * -1;
    a_z = 0;
    makevectors(a);
    vector c;
    c_x = v_up_y * up_z - v_up_z * up_y;
    c_y = v_up_z * up_x - v_up_x * up_z;
    c_z = v_up_x * up_y - v_up_y * up_x;
    float roll = atan2(c * fw, v_up * up) * (180 / 3.14159265);
    a_z = roll;
    makevectors(a);
    if(v_up * up < 0.999)
    {
        a_z = -roll; // (the other way round)
    }
    return a;
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
        vector c = (target.absmin + target.absmax) * 0.5;
        F = c - eye;
        if(atKind == 2)
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
    // A hand's throw (4 overhand, 5 sidearm, 6 sloppy: at random; 7 as set): from blade first, turned (vr_test_axe_roll
    // about the way it goes, _pitch about your left, its head forward, _yaw about the upright) and spun (rad/s about
    // the way it goes, your left: end over end, its top going ahead, the upright). vr_test_axe_loft: aimed up to meet
    // what is ahead level with your eyes.
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
        else
        {
            t_x = cvar("vr_test_axe_roll");
            t_y = cvar("vr_test_axe_pitch");
            t_z = cvar("vr_test_axe_yaw");
            w_x = cvar("vr_test_axe_spin_f");
            w_y = cvar("vr_test_axe_spin_l");
            w_z = cvar("vr_test_axe_spin_u");
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
    entity axe = CreateThrownWeapon(self, WID_AXE, 0, 0, from, a, F * speed - self.velocity, '0 0 0',
        FALSE, hurt, WeaponIdToThrowDamage(WID_AXE));
    if(mode >= 4 && cvar("vr_test_axe_loft") && speed > 0)
    {
        traceline(from, from + F * 4000, TRUE, self);
        float g = (axe.gravity ? axe.gravity : 1) * cvar("sv_gravity");
        axe.velocity_z = axe.velocity_z + 0.5 * g * (4000 * trace_fraction / speed);
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

// The stuck axe nearest `xAt` (world: none).
entity(vector xAt) VR_AxeStick_Nearest =
{
    entity best = world;
    float bestDist = 100000;
    for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        if(VR_AxeStick_IsStuck(e) && vlen(e.origin - xAt) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - xAt);
        }
    }
    return best;
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

// Tests (impulse 208): prints each stuck axe: what it is in, where, its blade.
void() VR_AxeStick_TestReport =
{
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
