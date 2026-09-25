// vr_carry.qc -- carrying and nudging physics items (ammo and health boxes) with the hands
// (vr_carry). See docs/vr-port/ROUND6.md, item 14.
//
// - A hand or a gun touching a box without gripping nudges it: the box takes up the hand's speed
//   along its motion, as a rigid body (it tumbles and settles).
// - Gripping a box holds it: it follows the hand, turning with it, and stops at walls (a line
//   from where it is to where the hand wants it) rather than passing through them; if it is left
//   far behind (stuck), it drops.
// - While a hand holds a box it cannot take a weapon (holsters, weapons lying about, force grab).
//   It still punches, harder (vr_carry_melee_mult): the box is an improvised weapon.
// - Pulling the trigger while holding it takes it (its ammo or health). Walking over it with body
//   interactions on, or without tracked hands, still takes it as before.
// - Letting go leaves it with the hand's throw velocity, as a rigid body; thrown hard, it hurts
//   what it hits (vr_carry_throw_damage).
// vr_carry 0 restores the old behaviour: a hand touching a box takes it.

.entity mainhand_held;   // the box each hand carries
.entity offhand_held;
.entity carry_player;    // on a box: who carries it
.float carry_hand;
.vector carry_offset;    // the box's centre in the hand's frame (forward, right, up)
.vector carry_angles;    // its angles relative to the hand's
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

// The hand's velocity (m/s) and angles of `xPlayer` (not only `self`).
vector(entity xPlayer, float xHand) VR_Carry_HandVel =
{
    if(xHand == cVR_OffHand)
    {
        return xPlayer.offhandvel;
    }
    return xPlayer.handvel;
};

vector(entity xPlayer, float xHand) VR_Carry_HandRot =
{
    if(xHand == cVR_OffHand)
    {
        return xPlayer.offhandrot;
    }
    return xPlayer.handrot;
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

    vector v = VR_Carry_HandVel(xPlayer, xHand) * VRMetersToUnits() * cvar_hget(cvarh_vr_carry_nudge);
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
    xBox.carry_hand = xHand;

    vector handPos = VRGetEntHandPos(xPlayer, xHand);
    vector handRot = VR_Carry_HandRot(xPlayer, xHand);
    makevectors(handRot);
    vector d = xBox.origin - handPos;
    xBox.carry_offset_x = d * v_forward;
    xBox.carry_offset_y = d * v_right;
    xBox.carry_offset_z = d * v_up;
    xBox.carry_angles = xBox.angles - handRot;
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
        SetThrownWeaponBaseDamage(xBox, cvar_hget(cvarh_vr_carry_throw_damage));
        xBox.owner = self;
    }
    else
    {
        xBox.owner = world;
    }
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

    if(!VR_HandGrabUtil_IsHandGrabbing(self, xHand))
    {
        dprint("carry: let go\n");
        VR_Carry_Release(xHand, box);
        return;
    }

    float trigger = VRGetFireButtonPressed(xHand);
    if(trigger && !box.carry_trigger)
    {
        box.carry_trigger = trigger;
        VR_Carry_Take(xHand, box);
        if(box.model == string_null)
        {
            VR_Carry_SetHeld(self, xHand, world);
            return;
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
    box.angles = handRot + box.carry_angles;
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
