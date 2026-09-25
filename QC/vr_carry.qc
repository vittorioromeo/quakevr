// vr_carry.qc -- carrying and nudging physics items (ammo and health boxes, backpacks, gibs) with
// the hands (vr_carry). See docs/vr-port/ROUND6.md, item 14.
//
// - A hand or a gun touching a box without gripping nudges it: the box takes up the hand's speed
//   along its motion, as a rigid body (it tumbles and settles).
// - Gripping a box holds it: it follows the hand, turning with it, and stops at walls (a line
//   from where it is to where the hand wants it) rather than passing through them; if it is left
//   far behind (stuck), it drops.
// - While a hand holds a box it cannot take a weapon (holsters, weapons lying about, force grab).
//   It still punches, harder (vr_carry_melee_mult): the box is an improvised weapon.
// - Taking it (its ammo or health), vr_carry_take: 0 letting go of it at a holster (hip or
//   shoulder: it goes in your pack), 1 pulling the trigger while holding it, 2 either. Walking over
//   it with body interactions on, or without tracked hands, still takes it as before.
// - Letting go leaves it with the hand's throw velocity, as a rigid body; thrown hard, it hurts
//   what it hits (vr_carry_throw_damage).
// vr_carry 0 restores the old behaviour: a hand touching a box takes it.

.entity mainhand_held;   // the box each hand carries
.entity offhand_held;
.entity carry_player;    // on a box: who carries it
.vector carry_offset;    // the box's centre in the hand's frame (forward, right, up)
.float carry_trigger;    // the carrying hand's trigger last frame
.float carry_nudgetime;
.void() carry_use;       // taking it: the box's own pickup (ammo_touch, health_touch)

entity(entity xPlayer, float xHand) VR_Carry_Held =
{
    if(xHand == cVR_OffHand)
    {
        return xPlayer.offhand_held;
    }
    return xPlayer.mainhand_held;
};

void(entity xPlayer, float xHand, entity xBox) VR_Carry_SetHeld =
{
    if(xHand == cVR_OffHand)
    {
        xPlayer.offhand_held = xBox;
    }
    else
    {
        xPlayer.mainhand_held = xBox;
    }
};

float(entity xPlayer, float xHand) VR_Carry_IsHolding =
{
    return VR_Carry_Held(xPlayer, xHand) != world;
};

// A box becomes a physics object again: a rigid body, touchable, no longer carried.
void(entity xBox) VR_Carry_Loose =
{
    xBox.carry_player = world;
    xBox.movetype = MOVETYPE_TOSS;
    xBox.solid = SOLID_NOT_BUT_TOUCHABLE;
    xBox.vr_rigid = 1;
    xBox.vr_rest = 0;
    xBox.flags = xBox.flags - (xBox.flags & FL_ONGROUND);
    setorigin(xBox, xBox.origin);
};

// ----------------------------------------------------------------------------
// Nudging

void(entity xPlayer, float xHand, entity xBox) VR_Carry_Nudge =
{
    if(time < xBox.carry_nudgetime || xBox.carry_player != world)
    {
        return;
    }

    // A gib or head struck with a swing bursts (VR_Gib_Struck).
    if(xBox.vr_gib)
    {
        VR_Gib_Struck(xPlayer, xHand, xBox);
        if(!xBox.vr_gib)
        {
            return;
        }
    }

    vector v = VRGetEntHandVel(xPlayer, xHand) * VRMetersToUnits() * cvar_hget(cvarh_vr_carry_nudge);
    float speed = vlen(v);
    if(speed < 10)
    {
        return;
    }

    // Pushed: along the hand's motion it moves at least as fast as the hand.
    vector d = v * (1 / speed);
    float along = xBox.velocity * d;
    if(along >= speed)
    {
        return;
    }
    xBox.velocity = xBox.velocity + d * (speed - along);
    VR_Carry_Loose(xBox);
    xBox.carry_nudgetime = time + 0.05;

    if(xPlayer.ishuman)
    {
        entity oldSelf = self;
        self = xPlayer;
        haptic(xHand, 0.0, 0.05, 60, 0.3);
        self = oldSelf;
    }
};

// ----------------------------------------------------------------------------
// Holding

void(entity xPlayer, float xHand, entity xBox) VR_Carry_Start =
{
    xBox.carry_player = xPlayer;

    vector handPos = VRGetEntHandPos(xPlayer, xHand);
    vector handRot = VRGetEntHandRot(xPlayer, xHand);
    makevectors(handRot);
    vector d = xBox.origin - handPos;
    xBox.carry_offset_x = d * v_forward;
    xBox.carry_offset_y = d * v_right;
    xBox.carry_offset_z = d * v_up;
    carryangles(xBox, handRot, TRUE); // the engine keeps its turn in the hand from here
    xBox.carry_trigger = VRGetEntFireButtonPressed(xPlayer, xHand);

    xBox.movetype = MOVETYPE_NONE;
    xBox.solid = SOLID_NOT;
    xBox.velocity = '0 0 0';
    xBox.avelocity = '0 0 0';
    xBox.vr_spin = '0 0 0';
    xBox.throwhit = QVR_THROWHIT_HIT;
    xBox.owner = xPlayer;
    VR_Carry_SetHeld(xPlayer, xHand, xBox);

    entity oldSelf = self;
    self = xPlayer;
    sound(self, CHAN_AUTO, "knight/sword2.wav", 0.4, ATTN_NORM);
    haptic(xHand, 0.0, 0.1, 70, 0.6);
    self = oldSelf;
};

// Let go: it keeps the hand's throw (the same estimate thrown weapons use).
void(float xHand, entity xBox) VR_Carry_Release =
{
    VR_Carry_SetHeld(self, xHand, world);

    float m2u = VRMetersToUnits();
    vector throwVel = VRGetHandThrowVel(xHand);
    vector v = throwVel * m2u * VRThrowGain(vlen(throwVel)) * cvar_hget(cvarh_vr_carry_throw_mult);

    VR_Carry_Loose(xBox);
    xBox.velocity = v;
    xBox.vr_spin = VRThrowSpinCapped(VRGetHandAVel(xHand));
    if(cvar_hget(cvarh_vr_throw_gravity) > 0 && cvar_hget(cvarh_sv_gravity) > 0)
    {
        xBox.gravity = cvar_hget(cvarh_vr_throw_gravity) * m2u / cvar_hget(cvarh_sv_gravity);
    }

    // Thrown hard it is a missile: it hurts what bleeds (forcegrabbable_touch), not the thrower.
    if(vlen(v) > 250)
    {
        xBox.throwhit = QVR_THROWHIT_NEVER_HIT;
        xBox.throwdmgspeed = 700 * m2u / 120;
        SetThrownWeaponDealDamage(xBox, TRUE);
        SetThrownWeaponBaseDamage(xBox, cvar_hget(xBox.vr_gib ? cvarh_vr_gib_throw_damage : cvarh_vr_carry_throw_damage));
        xBox.owner = self;
    }
    else
    {
        xBox.owner = world;
    }
    VR_Gib_Released(xBox, vlen(v) > 250); // a gib or head thrown hard bursts on a wall
};

// Take it: its own pickup, as if the carrying hand had touched it the old way.
void(float xHand, entity xBox) VR_Carry_Take =
{
    entity oldSelf = self;
    entity oldOther = other;
    self.touchinghand = xHand;
    xBox.handtouch_hand = xHand;
    xBox.handtouch_ent = self;
    other = self;
    self = xBox;
    xBox.solid = SOLID_NOT_BUT_TOUCHABLE; // pickups ignore non-solid items; it is held as one
    xBox.carry_use();
    if(xBox.model != string_null)
    {
        xBox.solid = SOLID_NOT; // not taken (full up): still held
    }
    self = oldSelf;
    other = oldOther;

    if(xBox.model == string_null) // taken (else: full up, still held)
    {
        VR_Carry_SetHeld(self, xHand, world);
    }
};

// Whether `xHand` of `self` is at a holster (a hip, a shoulder, or an upper one).
float(float xHand) VR_Carry_AtHolster =
{
    return isHolsterHotspot(xHand == cVR_OffHand ? self.offhand_hotspot : self.mainhand_hotspot);
};

// Every player frame, for each hand (self is the player).
void(float xHand) VR_Carry_HandFrame =
{
    entity box = VR_Carry_Held(self, xHand);
    if(box == world)
    {
        return;
    }

    if(box.model == string_null || box.carry_player != self || self.health <= 0 || !cvar_hget(cvarh_vr_carry))
    {
        dprint("carry: lost\n");
        VR_Carry_SetHeld(self, xHand, world);
        if(box.carry_player == self)
        {
            VR_Carry_Loose(box);
        }
        return;
    }

    // A held gib or backpack does not vanish.
    if(box.think == SUB_Remove && box.nextthink < time + 5)
    {
        box.nextthink = time + 5;
    }
    if(box.think2 == SUB_Remove && box.nextthink2 < time + 5)
    {
        box.nextthink2 = time + 5;
    }

    float takeMode = cvar_hget(cvarh_vr_carry_take);
    if(!VR_HandGrabUtil_IsHandGrabbing(self, xHand))
    {
        // Let go at a holster: into the pack (if full up, it drops there).
        if(takeMode != 1 && VR_Carry_AtHolster(xHand))
        {
            dprint("carry: into the pack\n");
            VR_Carry_Take(xHand, box);
            if(box.model == string_null)
            {
                return;
            }
            VR_Carry_SetHeld(self, xHand, world);
            VR_Carry_Loose(box);
            box.velocity = '0 0 0';
            return;
        }
        dprint("carry: let go\n");
        VR_Carry_Release(xHand, box);
        return;
    }

    float trigger = VRGetFireButtonPressed(xHand);
    if(takeMode != 0 && trigger && !box.carry_trigger)
    {
        box.carry_trigger = trigger;
        VR_Carry_Take(xHand, box);
        if(box.model == string_null)
        {
            return; // taken (VR_Carry_Take let go of it)
        }
    }
    box.carry_trigger = trigger;

    // Follow the hand, turning with it; not through walls or monsters.
    vector handRot = VRGetHandRot(xHand);
    makevectors(handRot);
    vector target = VRGetHandPos(xHand) + v_forward * box.carry_offset_x + v_right * box.carry_offset_y +
                    v_up * box.carry_offset_z;

    traceline(box.origin, target, FALSE, self);
    vector dest = trace_endpos;
    if(trace_fraction < 1)
    {
        dest = dest + trace_plane_normal * max(1, box.maxs_x);
    }
    if(vlen(dest - target) > 32)
    {
        dprint("carry: stuck at "); dprint(vtos(dest)); dprint(", the hand wants "); dprint(vtos(target)); dprint("\n");
        VR_Carry_Release(xHand, box); // stuck behind something
        return;
    }

    if(frametime > 0)
    {
        box.velocity = (dest - box.origin) * (1 / frametime);
    }
    setorigin(box, dest);
    box.angles = carryangles(box, handRot, FALSE);
};

// ----------------------------------------------------------------------------
// The box's touches

// A hand touching the box (handtouch): grip to hold it, else nudge it.
void() VR_Carry_Handtouch =
{
    if(!cvar_hget(cvarh_vr_carry) || other.classname != "player" || VRIsTouchingHandFake(other))
    {
        self.carry_use(); // the old way: touching takes it
        return;
    }

    if(self.carry_player != world || self.model == string_null)
    {
        return;
    }

    float hand = VRGetTouchingHand(other);
    if(VR_HandGrabUtil_IsHandGrabbing(other, hand) && VRIsHandEmpty(other, hand) && !VR_Carry_IsHolding(other, hand) &&
       !VR_Forcegrab_IsHandBusy(other, hand))
    {
        VR_Carry_Start(other, hand, self);
        return;
    }

    VR_Carry_Nudge(other, hand, self);
};

// A gun poking the box (vr_wpntouch): nudge it.
void() VR_Carry_Weapontouch =
{
    if(!cvar_hget(cvarh_vr_carry) || other.classname != "player")
    {
        return;
    }
    VR_Carry_Nudge(other, VRGetTouchingHand(other), self);
};

// ----------------------------------------------------------------------------
// Gibs and heads (vr_grab_gibs): carried, thrown and force-grabbed like the boxes, with nothing to
// take. Their box is the model's; thrown hard they hurt (forcegrabbable_touch) and splatter.

void() VR_Gib_Touch =
{
    float speed = vlen(self.velocity);
    float fast = speed > 220;
    if(fast && other.solid == SOLID_BSP)
    {
        sound(self, CHAN_AUTO, "zombie/z_miss.wav", 0.7, ATTN_NORM);

        // With vr_gib_blood the engine splats and spurts blood where a gib hits the world (not doors
        // or lifts, which get this).
        if(!cvar_hget(cvarh_vr_gib_blood) || other != world)
        {
            particle2(self.origin, self.velocity * -0.1, QVR_PARTICLE_PRESET_BLOOD, 12);
        }
    }

    // Quake's bounce flings it from the body; from its first landing it is a rigid body (vr_rigid.cpp),
    // which lies on the floor by its turned box. Quake's box does not turn with the model (and a
    // gib's collides as a player-sized one), so gibs came to rest up to half their size sunk into
    // the floor. vr_rest -1: settle even if Quake stops it this frame.
    if(!self.vr_rigid && other.solid == SOLID_BSP)
    {
        self.vr_rigid = 1;
        self.vr_rest = -1;
        self.vr_spin = VRThrowSpinCapped(self.avelocity * 0.0087); // half its tumble, in radians
        self.avelocity = '0 0 0';
    }

    // Thrown hard into a monster: it hurts it (forcegrabbable_touch, which ends the throw; not the
    // thrower, whose body it leaves through), and bursts.
    float thrown = self.throwhit == QVR_THROWHIT_NEVER_HIT;
    vector dir = self.velocity;
    forcegrabbable_touch();
    if(thrown && self.throwhit != QVR_THROWHIT_NEVER_HIT && other.takedamage &&
       speed >= cvar_hget(cvarh_vr_gib_splat_speed) && VR_Gib_IsDestroyable(self))
    {
        dprint("gib: struck "); dprint(other.classname); dprint(" at "); dprint(ftos(speed)); dprint("\n");
        VR_Gib_Burst(self, dir);
    }
};

void(entity xGib) VR_MakeGibGrabbable =
{
    if(!cvar_hget(cvarh_vr_grab_gibs) || xGib.model == string_null)
    {
        return;
    }

    // The model's own box (Quake gave gibs none, a head a fixed one).
    setsize(xGib, modelbounds(xGib, 0), modelbounds(xGib, 1));
    xGib.solid = SOLID_NOT_BUT_TOUCHABLE;
    xGib.touch = VR_Gib_Touch;
    xGib.handtouch = SUB_Null; // nothing to take: VR_Carry_Setup keeps it as carry_use
    // A head is the monster itself: no longer one (not thrown by the killing blow's push, not a
    // target). Killed restores its solid after the death code (which often clears it).
    xGib.flags = (xGib.flags - (xGib.flags & FL_MONSTER)) | FL_FORCEGRABBABLE;
    xGib.vr_gib = TRUE;
    xGib.throwhit = QVR_THROWHIT_HIT;
    xGib.owner = world;
    SetThrownWeaponDealDamage(xGib, FALSE);
    SetThrownWeaponDisappearOnHit(xGib, FALSE);
    VR_Carry_Setup(xGib);
    xGib.vr_rigid = 0; // Quake's bounce until it first lands (VR_Gib_Touch) or is picked up; then a rigid body

    // They lie about longer, to be played with.
    if(xGib.think == SUB_Remove)
    {
        xGib.nextthink = time + 30 + random() * 15;
    }
};

// ----------------------------------------------------------------------------
// Destroying gibs and heads (vr_gib_destroy): shot, blown up, struck, or thrown hard at a wall or a
// monster, a gib or head bursts in a mist of blood and is gone. It has a little health
// (vr_gib_health); its damage never reaches Quake's death code (T_Damage hands it to
// VR_Gib_Damage), so nothing is counted or killed again. Shots and missiles stop at it (the
// engine's MOVE_HITGIBS: it is not solid, and never blocks anything else). Only a grabbable one
// (vr_grab_gibs) can be destroyed: it has a box to hit.

// Added to a traceline's "nomonsters" (FALSE or TRUE): it also stops at gibs and heads that can be
// destroyed (the engine's MOVE_HITGIBS). Missiles always do.
float MOVE_HITGIBS = 256;

.float vr_gib_healthmult; // 1 a gib, 1.5 a head
.float vr_gib_hp;         // what damage it takes yet (not .health: a head is a dead monster, which must stay dead)
.float vr_gib_armtime;    // when it can first be hit (not by the rest of the blow that threw it)
.float vr_gib_flight;     // thrown: until when a hard hit on a wall bursts it
.vector vr_gib_lastvel;   // thrown: its velocity at the last check, to see it strike a wall
.float vr_gib_releasetime; // when a hand last let go of it

float(entity xGib) VR_Gib_IsDestroyable =
{
    return xGib.th_die == VR_Gib_Die && cvar_hget(cvarh_vr_gib_destroy);
};

// From ThrowGib and ThrowHead, after VR_MakeGibGrabbable: `xHealthMult` of vr_gib_health.
void(entity xGib, float xHealthMult) VR_Gib_MakeDestroyable =
{
    // Not a player's head (the player itself, who comes back).
    if(!xGib.vr_gib || (xGib.flags & FL_CLIENT) || !cvar_hget(cvarh_vr_gib_destroy))
    {
        return;
    }

    xGib.vr_gib_healthmult = xHealthMult;
    xGib.vr_gib_hp = max(1, cvar_hget(cvarh_vr_gib_health) * xHealthMult);
    if(xGib.health >= 0)
    {
        xGib.health = -1; // dead meat: a rocket's direct hit looks for some health (T_MissileTouch), a headbutt for more
    }
    xGib.th_die = VR_Gib_Die; // a head's own death code (it is the monster) never runs again
    xGib.th_pain = SUB_NullPain;
    xGib.takedamage = DAMAGE_NO; // until armed: VR_Gib_Think2
    xGib.vr_gib_armtime = time + 0.2;
    xGib.think2 = VR_Gib_Think2;
    xGib.nextthink2 = xGib.vr_gib_armtime;
};

// Arms it, and follows a thrown one's flight (VR_Gib_Released). A thrown gib is a rigid body, whose
// landing on the world touches nothing: a hard hit is seen in its velocity, turned sharply between
// two frames.
void() VR_Gib_Think2 =
{
    if(!cvar_hget(cvarh_vr_gib_destroy))
    {
        self.takedamage = DAMAGE_NO;
        self.vr_gib_flight = 0;
        return;
    }

    float next = 0;
    if(!self.takedamage)
    {
        if(time >= self.vr_gib_armtime && self.carry_player == world)
        {
            self.takedamage = DAMAGE_YES; // not DAMAGE_AIM: no autoaim at it
        }
        else
        {
            next = time + 0.1; // picked up at once: armed once let go of
        }
    }

    if(self.vr_gib_flight && (time >= self.vr_gib_flight || self.carry_player != world))
    {
        self.vr_gib_flight = 0;
    }
    if(self.vr_gib_flight)
    {
        float splat = cvar_hget(cvarh_vr_gib_splat_speed);
        vector change = self.velocity - self.vr_gib_lastvel;
        if(vlen(self.vr_gib_lastvel) >= splat && vlen(change) >= splat)
        {
            dprint("gib: struck the world at "); dprint(ftos(vlen(self.vr_gib_lastvel))); dprint(", now ");
            dprint(vtos(self.velocity)); dprint("\n");
            VR_Gib_Burst(self, self.vr_gib_lastvel);
            return;
        }
        self.vr_gib_lastvel = self.velocity;
        next = time + 0.01; // the next frame
    }

    if(next)
    {
        self.nextthink2 = next;
    }
};

// Let go of (VR_Carry_Release): the hand that let go does not strike it as it follows through
// (VR_Gib_Struck). Thrown hard (`xHard`), for a few seconds a hard hit on a wall bursts it.
void(entity xGib, float xHard) VR_Gib_Released =
{
    if(!VR_Gib_IsDestroyable(xGib))
    {
        return;
    }
    xGib.vr_gib_releasetime = time;
    if(!xHard)
    {
        return;
    }
    dprint("gib: thrown at "); dprint(vtos(xGib.velocity)); dprint("\n");
    xGib.vr_gib_flight = time + 3;
    xGib.vr_gib_lastvel = xGib.velocity;
    xGib.think2 = VR_Gib_Think2;
    xGib.nextthink2 = time + 0.01;
};

// Its damage, from T_Damage (instead of Quake's). Pushed by the blow, burst at no health. Held, it
// takes none.
float(entity targ, entity inflictor, entity attacker, float damage) VR_Gib_Damage =
{
    if(!cvar_hget(cvarh_vr_gib_destroy))
    {
        targ.takedamage = DAMAGE_NO;
        return FALSE;
    }
    if(targ.carry_player != world || damage <= 0)
    {
        return FALSE;
    }

    vector dir = '0 0 0';
    if(inflictor != world && inflictor != targ)
    {
        dir = normalize((targ.absmin + targ.absmax) * 0.5 - (inflictor.absmin + inflictor.absmax) * 0.5);
    }

    targ.vr_gib_hp = targ.vr_gib_hp - damage;
    dprint("gib: hit by "); dprint(inflictor.classname); dprint(" for "); dprint(ftos(damage)); dprint(", ");
    dprint(ftos(max(0, targ.vr_gib_hp))); dprint(" left\n");
    if(targ.vr_gib_hp <= 0)
    {
        VR_Gib_Burst(targ, dir);
        return TRUE;
    }

    // Knocked away (a rigid body lying asleep wakes as it moves).
    targ.velocity = targ.velocity + dir * min(400, damage * 12) + '0 0 1' * min(150, damage * 4);
    targ.flags = targ.flags - (targ.flags & FL_ONGROUND);
    return TRUE;
};

// Quake's death code, should anything run it (Killed): it bursts.
void() VR_Gib_Die =
{
    VR_Gib_Burst(self, '0 0 0');
};

float vr_gib_burstsoundtime;

// Bursts in a mist of blood and is gone: soft clouds of blood through where it was, a spray of blood
// all round (splatting the walls and floor near by: the particles' decals), more along the blow
// (`xDir`, if any), and the gib sound.
void(entity xGib, vector xDir) VR_Gib_Burst =
{
    vector org = (xGib.absmin + xGib.absmax) * 0.5;
    dprint("gib: burst at "); dprint(vtos(org)); dprint("\n");
    float size = max(1, xGib.vr_gib_healthmult);
    vector d;
    float i;

    // The mist.
    for(i = 0; i < 3; i++)
    {
        d_x = crandom() * 6;
        d_y = crandom() * 6;
        d_z = crandom() * 6;
        particle2(org + d, d * 0.5, QVR_PARTICLE_PRESET_BLOOD, 12 * size);
    }

    // The spray, every way.
    for(i = 0; i < 4; i++)
    {
        d_x = crandom();
        d_y = crandom();
        d_z = crandom() * 0.7;
        particle2(org, normalize(d) * 22, QVR_PARTICLE_PRESET_BLOOD, 8 * size);
    }

    // Along the blow.
    if(xDir != '0 0 0')
    {
        particle2(org, normalize(xDir) * 30, QVR_PARTICLE_PRESET_BLOOD, 16 * size);
    }

    // Once a frame (a rocket bursts many).
    if(vr_gib_burstsoundtime != time)
    {
        vr_gib_burstsoundtime = time;
        sound(xGib, CHAN_AUTO, "player/udeath.wav", min(1, 0.55 * size), ATTN_NORM);
    }

    // Gone. Removed the next frame, not now: what struck it may still use it this frame.
    xGib.takedamage = DAMAGE_NO;
    xGib.th_die = SUB_Null;
    xGib.vr_gib_hp = 0;
    xGib.touch = SUB_Null;
    xGib.handtouch = SUB_Null;
    xGib.vr_wpntouch = SUB_Null;
    xGib.vr_gib = FALSE;
    xGib.vr_gib_flight = 0;
    xGib.flags = xGib.flags - (xGib.flags & FL_FORCEGRABBABLE);
    xGib.movetype = MOVETYPE_NONE;
    xGib.solid = SOLID_NOT;
    xGib.velocity = '0 0 0';
    xGib.avelocity = '0 0 0';
    xGib.vr_spin = '0 0 0';
    xGib.vr_rigid = 0;
    xGib.effects = 0;
    xGib.model = string_null;
    xGib.modelindex = 0;
    setorigin(xGib, xGib.origin);
    xGib.think = SUB_Remove;
    xGib.nextthink = time + 0.1;
    xGib.think2 = SUB_Null;
    xGib.nextthink2 = 0;
};

// A hand or a gun striking it at a swing's speed (vr_melee_speed): the blow of a swing, from the
// base of a punch at that speed up to three times it. Not just after a hand let go of it: a throw's
// follow-through is not a blow.
void(entity xPlayer, float xHand, entity xGib) VR_Gib_Struck =
{
    float threshold = max(0.1, cvar_hget(cvarh_vr_melee_speed));
    float speed = vlen(VRGetEntHandVel(xPlayer, xHand));
    if(speed < threshold || !VR_Gib_IsDestroyable(xGib) || !xGib.takedamage ||
       time < xGib.vr_gib_releasetime + 0.4)
    {
        return;
    }

    float damage = 20 * min(3, speed / threshold) * cvar_hget(cvarh_vr_melee_dmg_multiplier);
    T_Damage(xGib, xPlayer, xPlayer, damage);
};

// From MakeItemGrabbable: the box's pickup moves to carry_use; its hand and gun touches carry.
void(entity xBox) VR_Carry_Setup =
{
    if(xBox.handtouch != VR_Carry_Handtouch)
    {
        xBox.carry_use = xBox.handtouch;
    }
    xBox.handtouch = VR_Carry_Handtouch;
    xBox.vr_wpntouch = VR_Carry_Weapontouch;
    xBox.vr_rigid = 1;
};
