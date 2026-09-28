// vr_grenade.qc -- live grenades you can catch and throw back (vr_grenade_catch), and the lobbers' aim
// (vr_ogre_aim_height). See docs/vr-port/ROUND21.md, "Deflection by blows and bashes; catching grenades; ogre aim".
//
// A catchable grenade (an ogre's or a multi-grenade ogre's; with vr_grenade_catch 2 also the player's launcher's) is a
// physics object: a rigid body in Box3D (the engine's vr_box3d.cpp: hard and heavy, it bounces as Quake's did and rolls
// on its drawn shape, and never meets its thrower's own body), carried and thrown as the boxes and gibs are
// (vr_carry.qc), and force-grabbable (vr_wpnforcegrab.qc) even in flight. Quake's rules stay (its own touch and
// explosion run): it goes off on its fuse, or touching something it can aim at (a monster, a player; not its thrower),
// else it bounces. A VR player is its body there, not Quake's box (1.2 m across: the arms reach inside it): a grenade
// by the hands is to be caught (VR_Grenade_OnBody).
// - Caught (a hand takes it: gripped, or a force grab's catch), its fuse is set to vr_grenade_held_fuse (never
//   shortened), once a throw (vr_grenade_fuse_regrab 1: every catch), and it is lit: it fizzes, sparks, and ticks
//   faster and faster in the hand (felt too) until it goes off.
// - Thrown (let go of), it is the player's: it spares the player, goes off on a monster, and what it blows up is
//   the player's doing (T_RadiusDamage's attacker). Going off in the hand, it hurts the player as any explosion
//   there would: the thrower's (the ogre's) explosion, in full.
// - Its tick (every frame: VR_Grenade_Think) keeps its fuse (not .nextthink: a force grab's flight takes the think
//   over, and puts it back with a nextthink of its own), and plays its bounces (a prop landing on the world touches
//   nothing: a jump in its velocity is a bounce).

.float vr_grenade;       // a catchable live grenade (VR_Grenade_Make)
.float vr_gren_fuse;     // when it goes off
.void() vr_gren_boom;    // its own explosion (OgreGrenadeExplode, MultiGrenadeExplode, GrenadeExplode)
.entity vr_gren_thrower; // who threw it first (an ogre): an explosion in the hand is theirs
.void() vr_gren_touch;   // its own touch (OgreGrenadeTouch, GrenadeTouch, MultiGrenadeTouch)
.float vr_gren_held;     // in a hand at the last tick
.float vr_gren_set;      // its held fuse was set (once a throw: vr_grenade_fuse_regrab)
.float vr_gren_lit;      // caught: it fizzes, sparks and ticks until it goes off
.float vr_gren_ticknext; // the next tick
.float vr_gren_sparknext; // the next sparks
.vector vr_gren_lastvel; // its velocity at the last tick,
.vector vr_gren_lastorg; // and where it was
.float vr_gren_bouncenext; // the next bounce sound may play then
.float vr_gren_bounces;  // bounces so far (the first is reported: developer 1)

float VR_GREN_BOUNCE = 90;     // u/s: a jump in its velocity (gravity's taken off) this big is a bounce
float VR_GREN_TICK_SLOW = 0.5; // s between ticks with vr_grenade_held_fuse (or more) left,
float VR_GREN_TICK_FAST = 0.08; // down to this as it goes off
float VR_GREN_BODY_MARGIN = 3; // units round a VR player's body capsule a grenade goes off on it (VR_Grenade_OnBody)

float VR_LOB_SPEED = 632.46;   // u/s: Quake's lob (600 ahead, 200 up) on level ground: an ogre's, a zombie's
float VR_LOB_HARDER = 1.2;    // times that an out-of-reach lob may be thrown (a ledge high above: still "close to" Quake's)
float VR_LOB_LEAD = 0.3;       // share of the target's walk over the flight a lob leads it by (slightly)
float VR_LOB_STEPS = 10;       // the arc's segments traced for a wall in the way (VR_Lob_Clear)

// Whether grenades of this kind are catchable (vr_grenade_catch: 1 the ogres', 2 also the player's).
float(float xPlayers) VR_Grenade_On =
{
    float mode = cvar_hget(cvarh_vr_grenade_catch);
    if(xPlayers)
    {
        return mode >= 2;
    }
    return mode >= 1;
};

// Makes the grenade `g` (just thrown: its model, origin and velocity set) catchable, if vr_grenade_catch says so:
// `xBoom` its explosion, `xFuse` seconds to it, `xPlayers` a player's launcher's.
void(entity g, void() xBoom, float xFuse, float xPlayers) VR_Grenade_Make =
{
    if(!VR_Grenade_On(xPlayers))
    {
        return;
    }
    g.vr_grenade = TRUE;
    g.vr_gren_boom = xBoom;
    g.vr_gren_fuse = time + xFuse;
    g.vr_gren_thrower = g.owner;

    // The model's own box (Quake gave it none): what a hand touches and grips.
    setsize(g, modelbounds(g, 0), modelbounds(g, 1));
    g.solid = SOLID_NOT_BUT_TOUCHABLE;
    g.vr_gren_touch = g.touch;
    g.touch = VR_Grenade_Touch;
    g.flags = g.flags | FL_FORCEGRABBABLE;
    g.handtouch = SUB_Null; // nothing to take: VR_Carry_Setup keeps it as carry_use
    g.throwhit = QVR_THROWHIT_HIT;
    SetThrownWeaponDealDamage(g, FALSE);
    SetThrownWeaponDisappearOnHit(g, FALSE);
    VR_Carry_Setup(g); // (a rigid body: vr_rigid)
    g.vr_spin = VRThrowSpinCapped(g.avelocity * 0.0174533); // Quake's tumble, in radians
    g.avelocity = '0 0 0';
    g.vr_gren_lastvel = g.velocity;
    g.vr_gren_lastorg = g.origin;
    g.think = VR_Grenade_Think;
    g.nextthink = time;
    setorigin(g, g.origin);
};

// Lets go of the grenade `g` wherever it is held (it went off).
void(entity g) VR_Grenade_Detach =
{
    entity p = g.carry_player;
    if(p == world)
    {
        return;
    }
    if(p.mainhand_held == g)
    {
        p.mainhand_held = world;
    }
    if(p.offhand_held == g)
    {
        p.offhand_held = world;
    }
    g.carry_player = world;
    g.carry_2h = FALSE;
};

// It goes off (self is the grenade): its own explosion, let go of first; in a hand, the thrower's explosion.
void() VR_Grenade_Explode =
{
    entity holder = self.carry_player;
    if(holder != world)
    {
        VR_Grenade_Detach(self);
        if(self.vr_gren_thrower.flags & FL_MONSTER)
        {
            self.owner = self.vr_gren_thrower; // (the player's own attack would be halved and scaled by Self Damage)
        }
        dprint(sprintf("grenade: %s went off in %s's hand at %v\n", self.classname, holder.netname, self.origin));
    }
    else
    {
        dprint(sprintf("grenade: %s goes off at %v (%s's)\n", self.classname, self.origin, self.owner.classname));
    }
    VR_Grenade_Spent(self);
    self.movetype = MOVETYPE_NONE; // (not falling out of the hand)
    self.vr_gren_boom();
    if(holder != world)
    {
        dprint(sprintf("grenade: %s has %g health left\n", holder.netname, holder.health));
    }
};

// A bounce's sound, `xSpeed` its jump in speed.
void(float xSpeed) VR_Grenade_BounceSound =
{
    if(time < self.vr_gren_bouncenext)
    {
        return;
    }
    self.vr_gren_bouncenext = time + 0.08;
    sound(self, CHAN_VOICE, "weapons/bounce.wav", min(1, 0.25 + xSpeed / 400), ATTN_NORM);
    self.vr_gren_bounces = self.vr_gren_bounces + 1;
    if(self.vr_gren_bounces == 1)
    {
        dprint(sprintf("grenade: %s first bounces at %v, %.0f u/s\n", self.classname, self.origin, xSpeed));
    }
};

// Whether the grenade (self) touching `xWho` is on its body. A player in VR is not the box Quake gives it (1.2 m
// across: the arms work inside it), but the body's capsule (vr_box3d_player_radius, what props bounce off round its
// origin): a grenade within the hands' reach of it is to be caught, not gone off. Anything else: its box, as Quake's.
float(entity xWho) VR_Grenade_OnBody =
{
    if(!(xWho.flags & FL_CLIENT) || !xWho.ishuman || !VRHandsTracked(xWho))
    {
        return TRUE;
    }
    vector d = self.origin + (self.mins + self.maxs) * 0.5 - xWho.origin;
    d_z = 0;
    float r = max(1, cvar("vr_box3d_player_radius")) * 0.01 * VRMetersToUnits() + self.maxs_x + VR_GREN_BODY_MARGIN;
    return vlen(d) <= r;
};

// No longer a live grenade (it went off): not a rigid body, nothing to grab.
void(entity g) VR_Grenade_Spent =
{
    g.vr_gren_fuse = 0;
    g.vr_gren_lit = FALSE;
    g.vr_rigid = 0;
    g.vr_spin = '0 0 0';
    g.flags = g.flags - (g.flags & FL_FORCEGRABBABLE);
    g.handtouch = SUB_Null;
    g.vr_wpntouch = SUB_Null;
};

// Its own touch (OgreGrenadeTouch, GrenadeTouch, MultiGrenadeTouch: Quake's rules, it goes off on what it can aim at,
// not its thrower, else it bounces), except while held, and on a VR player's hands (VR_Grenade_OnBody).
void() VR_Grenade_Touch =
{
    if(other == self.owner || self.carry_player != world || !self.vr_gren_fuse)
    {
        return;
    }
    if(other.takedamage != DAMAGE_AIM)
    {
        self.vr_gren_touch(); // (a bounce off a door, a lift, another prop: its sound)
        return;
    }
    if(!VR_Grenade_OnBody(other))
    {
        return; // (in a VR player's box, but by the hands: to be caught)
    }
    dprint(sprintf("grenade: %s hits %s at %v\n", self.classname, other.classname, self.origin));
    float fuse = self.vr_gren_fuse;
    float oldFlags = self.flags;
    VR_Grenade_Spent(self);
    self.vr_gren_touch();
    if(self.touch == VR_Grenade_Touch)
    {
        // (It didn't go off: Honey's monsters only a weapon of their own hurts.)
        self.vr_gren_fuse = fuse;
        self.vr_rigid = 1;
        self.flags = oldFlags;
        self.handtouch = VR_Carry_Handtouch;
        self.vr_wpntouch = VR_Carry_Weapontouch;
    }
};

// A hand has taken it (self is the grenade): the fuse set (once a throw), and lit.
void() VR_Grenade_Caught =
{
    entity p = self.carry_player;
    float hand = cVR_MainHand;
    if(p.offhand_held == self && p.mainhand_held != self)
    {
        hand = cVR_OffHand;
    }
    float left = self.vr_gren_fuse - time;
    if(!self.vr_gren_set || cvar_hget(cvarh_vr_grenade_fuse_regrab))
    {
        float fuse = min(5, max(1, cvar_hget(cvarh_vr_grenade_held_fuse)));
        self.vr_gren_fuse = max(self.vr_gren_fuse, time + fuse);
        self.vr_gren_set = TRUE;
        dprint(sprintf("grenade: caught in hand %g, the fuse %.2f s -> %.2f s\n", hand, left, self.vr_gren_fuse - time));
    }
    else
    {
        dprint(sprintf("grenade: caught again in hand %g, the fuse left at %.2f s\n", hand, left));
    }
    if(!self.vr_gren_lit)
    {
        self.vr_gren_lit = TRUE;
        sound(self, CHAN_BODY, "vr/grenade_fuse.wav", 1, ATTN_NORM);
    }
    self.vr_gren_ticknext = time + 0.15;
};

// Let go of (VR_Carry_Release; self is the player, `xBox` what the hand let go of): a grenade is the player's now.
void(entity xBox, float xHand) VR_Grenade_Released =
{
    if(!xBox.vr_grenade)
    {
        return;
    }
    xBox.owner = self;
    SetThrownWeaponDealDamage(xBox, FALSE); // (it hurts by going off, not as a thrown box)
    dprint(sprintf("grenade: let go of at %v (%.0f u/s), %.2f s left\n", xBox.velocity, vlen(xBox.velocity),
        xBox.vr_gren_fuse - time));
};

// The lit grenade's cues: sparks, and ticks faster as it goes off (felt in the hand that holds it).
void(float xHeld) VR_Grenade_Cue =
{
    if(time >= self.vr_gren_sparknext)
    {
        self.vr_gren_sparknext = time + 0.06;
        particle2(self.origin + '0 0 2', '0 0 60', QVR_PARTICLE_PRESET_SPARKS, 2);
    }
    if(time < self.vr_gren_ticknext)
    {
        return;
    }
    float left = self.vr_gren_fuse - time;
    float full = min(5, max(1, cvar_hget(cvarh_vr_grenade_held_fuse)));
    float gap = VR_GREN_TICK_FAST + (VR_GREN_TICK_SLOW - VR_GREN_TICK_FAST) * min(1, max(0, left / full));
    self.vr_gren_ticknext = time + gap;
    sound(self, CHAN_AUTO, "vr/grenade_tick.wav", 0.8, ATTN_NORM);
    if(xHeld && self.carry_player.ishuman)
    {
        entity p = self.carry_player;
        entity oldSelf = self;
        self = p;
        if(p.mainhand_held == oldSelf)
        {
            haptic(cVR_MainHand, 0.0, 0.03, 140, 0.35);
        }
        if(p.offhand_held == oldSelf)
        {
            haptic(cVR_OffHand, 0.0, 0.03, 140, 0.35);
        }
        self = oldSelf;
    }
};

// Every frame (self is the grenade).
void() VR_Grenade_Think =
{
    self.nextthink = time;
    float held = self.carry_player != world;
    if(held && !self.vr_gren_held)
    {
        VR_Grenade_Caught();
    }
    float wasHeld = self.vr_gren_held;
    self.vr_gren_held = held;
    if(time >= self.vr_gren_fuse)
    {
        VR_Grenade_Explode();
        return;
    }
    if(self.vr_gren_lit)
    {
        VR_Grenade_Cue(held);
    }

    // A bounce: its velocity jumped (the fall this frame taken off), not by a throw, a bat, or a move (a test's
    // placing, a teleporter).
    float moved = vlen(self.origin - self.vr_gren_lastorg);
    if(!held && !wasHeld && frametime > 0 && time - self.vr_deflect_time > 0.05 &&
       moved < vlen(self.vr_gren_lastvel) * frametime * 2 + 16)
    {
        vector dv = self.velocity - self.vr_gren_lastvel;
        dv_z = dv_z + VR_Deflect_Gravity(self) * frametime;
        if(vlen(dv) > VR_GREN_BOUNCE && dv * self.vr_gren_lastvel < 0)
        {
            VR_Grenade_BounceSound(vlen(dv));
        }
    }
    self.vr_gren_lastvel = self.velocity;
    self.vr_gren_lastorg = self.origin;
};

// ----------------------------------------------------------------------------
// Lobbing at the target's height (vr_ogre_aim_height)

// Whether the arc from `xFrom` at `xVel` (gravity `xG`) reaches `xTo` without a wall in the way (the level only, in
// VR_LOB_STEPS segments over its flight `xTime`; `xSkip` is not hit).
float(vector xFrom, vector xVel, float xG, float xTime, vector xTo, entity xSkip) VR_Lob_Clear =
{
    vector last = xFrom;
    for(float i = 1; i <= VR_LOB_STEPS; ++i)
    {
        float t = xTime * i / VR_LOB_STEPS;
        vector at = xFrom + xVel * t - '0 0 0.5' * xG * t * t;
        traceline(last, at, TRUE, xSkip);
        if(trace_fraction < 1)
        {
            return vlen(trace_endpos - xTo) < 40; // (it hits the floor by the target: as good)
        }
        last = at;
    }
    return TRUE;
};

// The launch velocity of a lob from `xFrom` that falls on `xTo` at `xSpeed` (sv_gravity): the lower arc, or the higher
// if a wall is in the way of the lower but not of it. Out of reach at `xSpeed`, it is thrown harder, as hard as it needs
// (a little more) up to VR_LOB_HARDER times; out of reach even so, as far as it goes towards it (45 degrees up, plus
// half the slope to it). Into vr_lob_time its flight to it, vr_lob_how the arc taken, vr_lob_speed the speed.
float vr_lob_time;
float vr_lob_how;   // 0 the lower arc, 1 the higher, 2 out of reach
float vr_lob_speed;
vector(vector xFrom, vector xTo, float xSpeed, entity xSkip) VR_Lob_Velocity =
{
    float g = cvar_hget(cvarh_sv_gravity);
    vector d = xTo - xFrom;
    vector flat = d;
    flat_z = 0;
    float x = vlen(flat);
    float y = d_z;
    float speed = xSpeed;
    vr_lob_how = 0;
    vr_lob_time = 0;
    vr_lob_speed = speed;
    if(g <= 0 || x < 1)
    {
        vr_lob_time = vlen(d) / max(1, speed);
        return normalize(d) * speed;
    }
    vector ahead = flat * (1 / x);
    float v2 = speed * speed;
    float disc = v2 * v2 - g * (g * x * x + 2 * y * v2);
    if(disc < 0)
    {
        // The least speed that reaches it (at 45 degrees plus half the slope), and a little more.
        speed = min(xSpeed * VR_LOB_HARDER, sqrt(g * (y + vlen(d))) * 1.03);
        v2 = speed * speed;
        disc = v2 * v2 - g * (g * x * x + 2 * y * v2);
        vr_lob_speed = speed;
    }
    float angle;
    if(disc >= 0)
    {
        float low = atan2(v2 - sqrt(disc), g * x);
        float high = atan2(v2 + sqrt(disc), g * x);
        angle = low;
        vector vlow = ahead * (speed * cos(low)) + '0 0 1' * (speed * sin(low));
        float tlow = x / (speed * cos(low));
        if(!VR_Lob_Clear(xFrom, vlow, g, tlow, xTo, xSkip))
        {
            vector vhigh = ahead * (speed * cos(high)) + '0 0 1' * (speed * sin(high));
            float thigh = x / max(1, speed * cos(high));
            if(VR_Lob_Clear(xFrom, vhigh, g, thigh, xTo, xSkip))
            {
                angle = high;
                vr_lob_how = 1;
            }
        }
    }
    else
    {
        angle = 0.785398 + 0.5 * atan2(y, x); // (the furthest along the slope to it)
        vr_lob_how = 2;
    }
    vr_lob_time = x / max(1, speed * cos(angle));
    return ahead * (speed * cos(angle)) + '0 0 1' * (speed * sin(angle));
};

// A lobber (self: an ogre, a zombie) throws `xMissile` from `xFrom` at its enemy: with vr_ogre_aim_height, on the arc
// at Quake's lob's speed (VR_LOB_SPEED) that falls on the enemy where it is (its middle), leading it a little
// (VR_LOB_LEAD); else Quake's lob (the missile's velocity as the caller set it) stays.
void(entity xMissile, vector xFrom) VR_Lob_Aim =
{
    entity e = self.enemy;
    if(!cvar_hget(cvarh_vr_ogre_aim_height) || e == world)
    {
        return;
    }
    vector quake = xMissile.velocity;
    vector at = e.origin + (e.mins + e.maxs) * 0.5;
    vector vel = VR_Lob_Velocity(xFrom, at, VR_LOB_SPEED, self);
    vector walk = e.velocity;
    walk_z = 0;
    if(vlen(walk) > 10 && vr_lob_time > 0)
    {
        vel = VR_Lob_Velocity(xFrom, at + walk * (vr_lob_time * VR_LOB_LEAD), VR_LOB_SPEED, self);
    }
    xMissile.velocity = vel;
    vector flat = at - xFrom;
    flat_z = 0;
    string how = "the lower arc";
    if(vr_lob_how == 1)
    {
        how = "the higher arc (a wall in the way)";
    }
    else if(vr_lob_how == 2)
    {
        how = "out of reach: the furthest";
    }
    dprint(sprintf("lob: %s at %s, %.0f across, %.0f up: %s, %.1f deg, %.2f s;", self.classname, e.classname,
        vlen(flat), at_z - xFrom_z, how, atan2(vel_z, vlen(vel - '0 0 1' * vel_z)) * 57.29578, vr_lob_time));
    dprint(sprintf(" %.0f u/s %v (Quake's %v)\n", vr_lob_speed, vel, quake));
};
