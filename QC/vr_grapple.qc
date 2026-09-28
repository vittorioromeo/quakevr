// Grappling hook: Rogue's (Jan '97 by ZOID <zoid@threewave.com>, under contract to id Software for Rogue
// Entertainment), reworked for VR (docs/vr-port/ROUND21.md, "Grappling hook: rope, reel on demand, props and
// monsters").
//
// The trigger fires the hook (1500 units/s, 1.8 s at most: about 2700 units). It bites what it meets first: the world,
// a door or a lift, a monster, a player, or a physics prop (a weapon, a pickup, a key, a box, a gib: missiles pass
// through those, so the flight looks for them itself). The trigger held keeps it; let go, the hook comes off at once
// (in flight too); so it does when you are teleported, change weapon or die, or what it bit dies, is taken or goes.
//
// The rope then holds at the length it bit at, measured from your body (from the gun, for a prop): you swing on it
// and walk or fall closer freely, never farther. Nothing is pulled until the hand's upper face button (B on the main
// hand, Y on the off hand: the engine's QVR_VRBITS0_*HAND_SECONDARY) is held with the trigger: the reel then winds the
// rope in for as long as it is held (taking any slack in at once), and let go, the rope keeps its new length. What the
// reel pulls:
//
//   | Bitten | The reel | Speed |
//   |---|---|---|
//   | the world, a door or lift, a player | you, to it | vr_grapple_reel_speed |
//   | a prop up to vr_grapple_prop_anchor kg | it, to the gun (it hangs there: take it with the other hand) | vr_grapple_prop_speed, times vr_grapple_prop_light / mass above that mass |
//   | a heavier prop | you, to it | vr_grapple_reel_speed |
//   | a power-up (a plain pickup, single player) | it, flying to the gun, where it is taken | as a prop |
//   | a small monster (up to vr_grapple_small_mass kg) | it, to you, staggered (vr_grapple_stagger) | vr_grapple_small_speed |
//   | a medium monster | it, to you | vr_grapple_medium_speed |
//   | a huge monster (vr_grapple_huge_mass kg and more), or one that can't move | you, to it | vr_grapple_reel_speed |
//
// A load that can't follow (a box behind a pillar, a monster against a wall) stalls the reel: the rope is never more
// than GH_STALL shorter than it is far. The rope holds what it pulls too: a prop or a monster that goes farther than the
// rope (pushed, walking away, you walking away) is pulled back to its length; a huge monster drags you.
// vr_grapple_rope 0: Rogue's grapple, pulling you in as soon as it bites, whatever it bit (as it always does for a player
// without tracked hands: a flat-screen player or a bot has no B/Y to reel with).

// New entity fields
.entity hook; // this is my hook
.float on_hook; // we're on it
.float hook_out; // it's out

.entity hook2; // (off-hand) this is my hook
.float on_hook2; // (off-hand) we're on it
.float hook_out2; // (off-hand) it's out

.float handParm;

void(float xHand, entity v) setHook =
{
    if(xHand == cVR_OffHand) self.hook2 = v;
    else                  self.hook = v;
}

void(float xHand, float v) setHookOut =
{
    if(xHand == cVR_OffHand) self.hook_out2 = v;
    else                  self.hook_out = v;
}

entity(float xHand) getHook =
{
    if(xHand == cVR_OffHand) return self.hook2;
    else                  return self.hook;
}

float(float xHand) getOnHook =
{
    if(xHand == cVR_OffHand) return self.on_hook2;
    else                  return self.on_hook;
}

float(float xHand) getHookOut =
{
    if(xHand == cVR_OffHand) return self.hook_out2;
    else                  return self.hook_out;
}

void(float xHand, float v) setOwnerOnHook =
{
    if(xHand == cVR_OffHand) self.owner.on_hook2 = v;
    else                  self.owner.on_hook = v;
}

void(float xHand, float v) setOwnerHookOut =
{
    if(xHand == cVR_OffHand) self.owner.hook_out2 = v;
    else                  self.owner.hook_out = v;
}

float(float xHand) getOwnerOnHook =
{
    if(xHand == cVR_OffHand) return self.owner.on_hook2;
    else                  return self.owner.on_hook;
}

// prototypes for WEAPONS.QC functions
float() crandom;
void(vector org, vector vel, float damage) SpawnBlood;

// ----------------------------------------------------------------------------
// The hook's state
// ----------------------------------------------------------------------------

float GH_FLYING  = 0; // not bitten yet
float GH_ANCHOR  = 1; // what it bit stays: the reel pulls the player
float GH_PROP    = 2; // a prop: the reel pulls it to the gun
float GH_MONSTER = 3; // a monster: the reel pulls it to the player

float GH_SMALL  = 1; // a monster's class (gh_class)
float GH_MEDIUM = 2;
float GH_HUGE   = 3;

float GH_STALL      = 8;    // units the rope may be shorter than the load is far: the reel can't outrun its load
float GH_RAMP       = 0.15; // seconds the reel takes to reach its speed (it starts at a fifth)
float GH_BRAKE      = 2500; // units/s/s a reeled prop or monster slows at, arriving (it stops at the rope's shortest)
float GH_HOLD_SPEED = 400;  // units/s at most the rope pulls a load back to its length when it is farther (not reeling)
float GH_HOLD_SLACK = 8;    // units past the rope's length before it pulls a load back
float GH_PROP_GAP   = 12;   // units past half its size from the muzzle a reeled prop stops at

.float gh_kind;          // GH_*
.float gh_len;           // the rope's length (units): from the player's body (a prop's: from the gun) to the hook
.vector gh_ofs;          // where it bit, on what it bit: a prop's model axes, a monster's (turned by its yaw), a door's
.float gh_class;         // a monster's class (GH_SMALL ..), 0 for the rest
.float gh_mass;          // what it bit weighs (kg; 0: the world, a door)
.float gh_speed;         // the reel's full speed for it (units/s)
.float gh_reel;          // reeling since then (0: not)
.float gh_reel_from;     // the rope's length when this reel began (the log)
.float gh_taut;          // the rope was taut last frame
.float gh_sound_at;      // the next reel click, haptic pulse, stagger, stamina payment and log line
.float gh_haptic_at;
.float gh_stagger_at;
.float gh_stamina_at;
.float gh_stamina_mult;  // 1, or a quarter with no stamina left (vr_grapple_stamina)
.float gh_log_at;
.float gh_hurt_at;       // a hooked player's next point of damage (Rogue's)
.float gh_rearm;         // on a player: the main hand's hook came off by itself, the trigger held (VR_Grapple_MustRelease)
.float gh_rearm2;        // the off hand's

float() VR_Grapple_Debug =
{
    return cvar_hget(cvarh_vr_grapple_debug);
};

float() VR_Grapple_RopeMode =
{
    return cvar_hget(cvarh_vr_grapple_rope);
};

// A haptic pulse in the hook's hand, at vr_grapple_haptics (self: the player).
void(float xHand, float xDuration, float xFrequency, float xAmplitude) VR_Grapple_Haptic =
{
    float k = min(1, max(0, cvar_hget(cvarh_vr_grapple_haptics)));
    if(k > 0 && xAmplitude > 0)
    {
        VRGunHaptic(xHand, xDuration, xFrequency, min(1, xAmplitude * k));
    }
};

// A monster's mass (kg) for the grapple: what the reel does with it (vr_grapple_small_mass, vr_grapple_huge_mass).
// By kind; others from their box (a grunt's comes to 80 kg).
float(entity m) VR_Grapple_MonsterMass =
{
    string c = m.classname;
    if(c == "monster_fish")          { return 15; }
    if(c == "monster_spikemine")     { return 20; }
    if(c == "monster_tarbaby")       { return 25; }
    if(c == "monster_gremlin")       { return 30; }
    if(c == "monster_dog")           { return 35; }
    if(c == "monster_eel")           { return 40; }
    if(c == "monster_wizard")        { return 50; }
    if(c == "monster_zombie")        { return 70; }
    if(c == "monster_army")          { return 80; }
    if(c == "vr_dummy")              { return 80; }
    if(c == "monster_knight")        { return 90; }
    if(c == "monster_wrath")         { return 90; }
    if(c == "monster_enforcer")      { return 100; }
    if(c == "monster_mummy")         { return 140; }
    if(c == "monster_hell_knight")   { return 150; }
    if(c == "monster_sknight")       { return 150; }
    if(c == "monster_sword")         { return 150; }
    if(c == "monster_ogre")          { return 250; }
    if(c == "monster_ogre_marksman") { return 250; }
    if(c == "monster_demon1")        { return 250; }
    if(c == "monster_shalrath")      { return 250; }
    if(c == "monster_morph")         { return 300; }
    if(c == "monster_scourge")       { return 300; }
    if(c == "monster_super_wrath")   { return 450; }
    if(c == "monster_shambler")      { return 600; }
    if(c == "monster_lava_man")      { return 3000; }
    if(c == "monster_dragon")        { return 3000; }
    if(c == "monster_armagon")       { return 3000; }
    if(c == "monster_boss")          { return 5000; }
    if(c == "monster_oldone")        { return 10000; }
    // A grunt's box (24 x 24 x 51 units) is 80 kg.
    return max(10, m.size_x * m.size_y * m.size_z * (80 / (24 * 24 * 51)));
};

// A pickup that is no object (the powerups, the mission packs' shields and belts; weapons and keys with
// vr_item_objects 0): a trigger that hangs where the map put it and is taken by touch. Single player only (elsewhere
// they come back where they are taken: a pulled one would move its place).
float(entity e) VR_Grapple_IsPlainPickup =
{
    if(deathmatch || coop || !(e.flags & FL_ITEM) || e.solid != SOLID_TRIGGER || e.model == string_null)
    {
        return FALSE;
    }
    if(!e.handtouch || e.handtouch == SUB_Null)
    {
        return FALSE;
    }
    return TRUE;
};

// Whether `e` is a physics prop the hook bites (and the flight looks for): a Box3D prop, a hanging pickup, a thrown
// weapon; not while a hand holds it or a force grab pulls it. A wall torch only off its wall.
float(entity e) VR_Grapple_IsProp =
{
    if(e == world || e.model == string_null || e.solid == SOLID_NOT || e.solid == SOLID_BSP ||
       (e.flags & (FL_MONSTER | FL_CLIENT)))
    {
        return FALSE;
    }
    if(e.carry_player != world || e.fg_state != 0)
    {
        return FALSE;
    }
    if(VR_WallTorch_IsTorch(e))
    {
        return e.wt_state != 0;
    }
    if(e.vr_rigid && (e.movetype == MOVETYPE_TOSS || e.movetype == MOVETYPE_BOUNCE))
    {
        return TRUE;
    }
    if(e.flags & FL_FORCEGRABBABLE)
    {
        return TRUE;
    }
    if(e.classname == "thrown_weapon")
    {
        return TRUE;
    }
    return VR_Grapple_IsPlainPickup(e);
};

// Where on `t` the point `xAt` is, for the hook to stay there (gh_ofs): a prop's model axes, a monster's or a player's
// turned by its yaw, a door's or a lift's (and the world's) as it is.
vector(entity h, entity t, vector xAt) VR_Grapple_Local =
{
    vector d = xAt - t.origin;
    if(h.gh_kind == GH_PROP)
    {
        // modelpoint is its model's axes turned (and scaled) as drawn, off its origin: undone along each axis.
        vector o = modelpoint(t, t.angles, '0 0 0');
        vector bx = modelpoint(t, t.angles, '1 0 0') - o;
        vector by = modelpoint(t, t.angles, '0 1 0') - o;
        vector bz = modelpoint(t, t.angles, '0 0 1') - o;
        d = d - o;
        vector l;
        l_x = (d * bx) / max(0.0001, bx * bx);
        l_y = (d * by) / max(0.0001, by * by);
        l_z = (d * bz) / max(0.0001, bz * bz);
        return l;
    }
    if(t != world && t.solid != SOLID_BSP)
    {
        makevectors('0 1 0' * t.angles_y);
        return '1 0 0' * (d * v_forward) + '0 1 0' * (d * v_right) + '0 0 1' * (d * v_up);
    }
    return d;
};

// Where the hook is now, on what it bit (VR_Grapple_Local's inverse).
vector(entity h) VR_Grapple_HookPoint =
{
    entity t = h.enemy;
    if(h.gh_kind == GH_PROP)
    {
        return t.origin + modelpoint(t, t.angles, h.gh_ofs);
    }
    if(t != world && t.solid != SOLID_BSP)
    {
        makevectors('0 1 0' * t.angles_y);
        return t.origin + v_forward * h.gh_ofs_x + v_right * h.gh_ofs_y + v_up * h.gh_ofs_z;
    }
    return t.origin + h.gh_ofs;
};

// The rope's end at the player (self): the gun for a prop (it comes to the gun), else the body (the rope holds the
// player by it, whatever the arm does).
vector(entity h, float xHand) VR_Grapple_RopeEnd =
{
    if(h.gh_kind == GH_PROP)
    {
        return VRGetMuzzlePos(xHand);
    }
    return self.origin;
};

// ----------------------------------------------------------------------------
// The rope drawn
// ----------------------------------------------------------------------------

// The rope from the gun to the hook: the player's beam, id 2 + hand (the client keeps its start at the gun as drawn,
// every frame); slack (0 .. 1: the share of its length that hangs), ids 4 + hand + 2 k, k 1 .. 125 (the client draws
// it hanging: vr_client.cpp VR_RopeCurve).
void(float xHand, vector hookPos, vector muzzlePos, float xSlack) GrappleTrail =
{
    float k = rint(min(1, max(0, xSlack)) * 125);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_BEAM);
    WriteEntity(MSG_BROADCAST, self);
    if(k >= 1)
    {
        WriteByte(MSG_BROADCAST, 4 + xHand + 2 * k);
    }
    else
    {
        WriteByte(MSG_BROADCAST, 2 + xHand); /* disambiguator */
    }
    WriteVec3(MSG_BROADCAST, muzzlePos);
    WriteVec3(MSG_BROADCAST, hookPos);
};

// ----------------------------------------------------------------------------
// Letting go
// ----------------------------------------------------------------------------

void() GrappleResetFromHook =
{
    float xHand;
    xHand = self.handParm;

    // Missed (its time out, the sky) or glanced off with the trigger held: fired again on a new pull only.
    if(VRGetEntFireButtonPressed(self.owner, xHand))
    {
        if(xHand == cVR_OffHand) self.owner.gh_rearm2 = TRUE;
        else                     self.owner.gh_rearm = TRUE;
    }

    setOwnerOnHook(xHand, FALSE);
    setOwnerHookOut(xHand, FALSE);
    VRSetWeaponFrame(xHand, 0);
    VRSetAttackFinished(xHand, time + 0.25);

    remove(self);
};

void(float xHand) GrappleResetFromPlayer =
{
    setOwnerOnHook(xHand, FALSE);
    setOwnerHookOut(xHand, FALSE);
    VRSetWeaponFrame(xHand, 0);
    VRSetAttackFinished(xHand, time + 0.25);

    remove(self);
};

// A hand whose hook came off by itself, the trigger still held (what it bit died, was taken or went): it fires again
// only once the trigger is let go (Rogue's fired again at once, over and over).
float(float xHand) VR_Grapple_MustRelease =
{
    return VR_IMPL_SELECT(xHand, self.gh_rearm2, self.gh_rearm);
};

// Every frame (client.qc): the trigger let go re-arms the hand.
void(float xHand) VR_Grapple_Rearm =
{
    if(!VRGetFireButtonPressed(xHand))
    {
        if(xHand == cVR_OffHand) self.gh_rearm2 = FALSE;
        else                     self.gh_rearm = FALSE;
    }
};

// The hook of `self`'s `xHand` comes off (`xWhy`: for the log; `xByItself`: not the trigger let go).
void(float xHand, string xWhy, float xByItself) VR_Grapple_Release =
{
    entity h = getHook(xHand);
    if(xByItself)
    {
        if(xHand == cVR_OffHand) self.gh_rearm2 = TRUE;
        else                     self.gh_rearm = TRUE;
    }
    if(VR_Grapple_Debug() && h.gh_kind != GH_FLYING)
    {
        dprint(sprintf("grapple: hand %g lets go (%s), rope %.0f\n", xHand, xWhy, h.gh_len));
    }
    entity oldSelf = self;
    self = h;
    GrappleResetFromPlayer(xHand);
    self = oldSelf;
};

// ----------------------------------------------------------------------------
// Biting
// ----------------------------------------------------------------------------

// Keeps a bitten hook alive while its player services it (GrappleService, every frame); left alone (the player gone),
// it goes.
void() VR_Grapple_HookThink =
{
    if(self.owner.classname != "player" || !getOwnerOnHook(self.handParm) || self.owner.health <= 0)
    {
        GrappleResetFromHook();
        return;
    }
    self.nextthink = time + 0.5;
};

// The hook `h` (of the player `h.owner`) bites `t` (world: the world) at `xAt`: what it is decides what the reel does
// (GH_*), and the rope is as long as it is now.
void(entity h, entity t, vector xAt) VR_Grapple_Bite =
{
    float xHand = h.handParm;
    entity p = h.owner;
    vector flight = normalize(h.velocity);

    h.velocity = '0 0 0';
    h.avelocity = '0 0 0';
    h.movetype = MOVETYPE_NONE;
    h.solid = SOLID_NOT;
    h.touch = SUB_Null;
    h.frame = 2; // anchored
    h.enemy = t;
    h.gh_class = 0;
    h.gh_mass = 0;
    h.gh_speed = cvar_hget(cvarh_vr_grapple_reel_speed);
    h.gh_reel = 0;
    h.gh_taut = FALSE;
    h.gh_stamina_mult = 1;
    setorigin(h, xAt);

    string what = "the world";
    if(t != world && (t.flags & FL_MONSTER))
    {
        h.gh_mass = VR_Grapple_MonsterMass(t);
        what = t.classname;
        if(h.gh_mass >= cvar_hget(cvarh_vr_grapple_huge_mass))
        {
            h.gh_class = GH_HUGE;
        }
        else if(h.gh_mass <= cvar_hget(cvarh_vr_grapple_small_mass))
        {
            h.gh_class = GH_SMALL;
        }
        else
        {
            h.gh_class = GH_MEDIUM;
        }
        // One that can't be moved (the firing range's dummies stand on posts; Chthon in his lava) pulls you, as a huge one.
        float movable = t.movetype == MOVETYPE_STEP || t.movetype == MOVETYPE_WALK || t.movetype == MOVETYPE_TOSS ||
                        t.movetype == MOVETYPE_BOUNCE || t.movetype == MOVETYPE_FLY;
        if(h.gh_class == GH_HUGE || !movable)
        {
            h.gh_kind = GH_ANCHOR;
        }
        else
        {
            h.gh_kind = GH_MONSTER;
            if(h.gh_class == GH_SMALL)
            {
                h.gh_speed = cvar_hget(cvarh_vr_grapple_small_speed);
            }
            else
            {
                h.gh_speed = cvar_hget(cvarh_vr_grapple_medium_speed);
            }
        }
    }
    else if(VR_Grapple_IsProp(t) || (t.vr_rigid && t.solid == SOLID_BBOX))
    {
        h.gh_mass = max(0.05, propvalue(t, "mass"));
        what = t.classname;
        if(what == string_null)
        {
            what = t.model; // (ammo and health boxes have none)
        }
        if(h.gh_mass >= cvar_hget(cvarh_vr_grapple_prop_anchor))
        {
            h.gh_kind = GH_ANCHOR;
        }
        else
        {
            h.gh_kind = GH_PROP;
            float light = max(0.01, cvar_hget(cvarh_vr_grapple_prop_light));
            h.gh_speed = cvar_hget(cvarh_vr_grapple_prop_speed) * min(1, light / h.gh_mass);
            // A pickup hanging where the map put it is knocked loose (a physics object from now on), a little along the
            // hook's flight.
            if(VR_Grapple_IsPlainPickup(t))
            {
                // A plain pickup floats (as it hangs): the reel flies it in, and it stays where the reel leaves it.
                t.movetype = MOVETYPE_FLY;
                t.velocity = '0 0 0';
                t.flags = t.flags - (t.flags & FL_ONGROUND);
            }
            else if(t.movetype == MOVETYPE_NONE)
            {
                VR_Carry_Loose(t);
                t.velocity = t.velocity + flight * 60;
            }
        }
    }
    else
    {
        // The world, a door or a lift, a player, a corpse: it stays.
        h.gh_kind = GH_ANCHOR;
        if(t != world)
        {
            what = t.classname;
        }
    }
    h.gh_ofs = VR_Grapple_Local(h, t, xAt);

    entity oldSelf = self;
    self = p;
    h.gh_len = vlen(VR_Grapple_HookPoint(h) - VR_Grapple_RopeEnd(h, xHand));
    VR_Grapple_Haptic(xHand, 0.1, 120, 0.8);
    if(VR_Grapple_Debug())
    {
        string kind = "stays: the reel pulls you";
        if(h.gh_kind == GH_PROP)
        {
            kind = sprintf("comes to the gun at %.0f u/s", h.gh_speed);
        }
        else if(h.gh_kind == GH_MONSTER)
        {
            kind = sprintf("%s, comes to you at %.0f u/s", h.gh_class == GH_SMALL ? "small" : "medium", h.gh_speed);
        }
        else if(h.gh_class == GH_HUGE)
        {
            kind = "huge: the reel pulls you";
        }
        dprint(sprintf("grapple: hand %g bit %s (%.1f kg) at %v, the gun at %v: %s; rope %.0f\n", xHand, what, h.gh_mass,
            xAt, VRGetMuzzlePos(xHand), kind, h.gh_len));
    }
    self = oldSelf;

    if(xHand == cVR_OffHand) p.on_hook2 = TRUE;
    else                     p.on_hook = TRUE;
    h.think = VR_Grapple_HookThink;
    h.nextthink = time + 0.5;
};

// The hook touches something (self: the hook): it bites it, if the trigger is still held.
void() GrappleAnchor =
{
    float xHand;

    xHand = self.handParm;

    if(other == self.owner) // don't hook the guy that fired it
        return;

    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        // TODO VR: (P2) see below
        // This does happen, but only in certain maps. Not sure if the problem
        // is map support, or something with `pointcontents`.
        GrappleResetFromHook();
        return;
    }

    if(other.classname == "player")
    {
        // glance off of teammates
        if(other.steam == self.owner.steam)
        {
            GrappleResetFromHook();        // PGM - fix drift after teammate hit  01/20/97
            return;
        }

        sound(self, CHAN_WEAPON, "player/axhit1.wav", 1, ATTN_NORM);
        T_Damage(other, self, self.owner, 10);
    } else {
        sound(self, CHAN_WEAPON, "player/axhit2.wav", 1, ATTN_NORM);

        // One point of damage inflicted upon impact. Subsequent
        // damage will only be done to PLAYERS... this way secret
        // doors and triggers will only be damaged once.
        if(other.takedamage)
        {
            T_Damage(other, self, self.owner, 1);
        }
    }

    sound(self.owner, CHAN_WEAPON, "weapons/tink1.wav", 1, ATTN_NORM);
    particle2(self.origin, '0 0 0', QVR_PARTICLE_PRESET_BULLETPUFF, 48);

    if(!VRGetEntFireButtonPressed(self.owner, xHand) || self.owner.health <= 0 || (other != world && other.health <= 0 &&
       (other.flags & FL_MONSTER)))
    {
        GrappleResetFromHook();
        return;
    }

    VR_Grapple_Bite(self, other, self.origin);
};

// The flying hook of `self`'s `xHand` meets a prop on its way this frame (missiles pass through them): the first one
// before whatever it hits (a wall, a monster: its touch), padded a little. TRUE when it bit one.
float(entity h) VR_Grapple_FlightProps =
{
    vector a = h.origin;
    vector b = a + h.velocity * frametime;
    float len = vlen(b - a);
    if(len < 0.1)
    {
        return FALSE;
    }
    traceline(a, b, FALSE, h);
    float before = trace_fraction;
    entity best = world;
    float bestT = before;
    entity e = findradius((a + b) * 0.5, len * 0.5 + 64);
    while(e)
    {
        if(e != h && e != self && VR_Grapple_IsProp(e))
        {
            float t = VR_SegmentBox(a, b, e.absmin - '3 3 3', e.absmax + '3 3 3');
            if(t >= 0 && t < bestT)
            {
                bestT = t;
                best = e;
            }
        }
        e = e.chain;
    }
    if(best == world)
    {
        return FALSE;
    }
    vector at = a + (b - a) * bestT;
    sound(h, CHAN_WEAPON, "player/axhit2.wav", 0.7, ATTN_NORM);
    sound(self, CHAN_WEAPON, "weapons/tink1.wav", 1, ATTN_NORM);
    particle2(at, '0 0 0', QVR_PARTICLE_PRESET_BULLETPUFF, 16);
    VR_Grapple_Bite(h, best, at);
    return TRUE;
};

// ----------------------------------------------------------------------------
// Firing
// ----------------------------------------------------------------------------

void (float xHand) W_FireGrapple =
{
    entity newmis;

    if(getHookOut(xHand))// reject subsequent calls from player.qc
        return;

    VRGunHaptic(xHand, 0.55, 75, 1.0);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 8);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);

    self.punchangle_x = -2; // bump him

    // chain out sound(loops)
    sound(self, CHAN_WEAPON, "weapons/chain1.wav", 1, ATTN_NORM);

    newmis = spawn();
    newmis.handParm = xHand;
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid = SOLID_BBOX;
    newmis.owner = self;        // newmis belongs to me
    setHook(xHand, newmis);            // This is my newmis
    newmis.classname = "hook";
    newmis.gh_kind = GH_FLYING;

    makevectors(VRGetWeaponFireRot(xHand));
    newmis.velocity = v_forward * 1500;
    newmis.velocity += vrProjectileVelocity(xHand, 0.6);
//    newmis.avelocity = '0 0 -500';
    newmis.angles = vectoangles(v_forward);

    newmis.touch = GrappleAnchor;
    newmis.think = GrappleResetFromHook;
    // grapple only lives for two seconds, this gives max range on it
    newmis.nextthink = time + 1.8;
    newmis.frame = 1; // hook spread

    setmodel(newmis,"progs/hook.mdl");
    setorigin(newmis, VRGetMuzzlePos(xHand) + v_forward * 6);
    setsize(newmis, '0 0 0' , '0 0 0 ');

    setHookOut(xHand, TRUE);
};

// ----------------------------------------------------------------------------
// The rope
// ----------------------------------------------------------------------------

// The reel's speed now for `h` (it starts at a fifth and reaches it in GH_RAMP).
float(entity h) VR_Grapple_ReelSpeed =
{
    float ramp = min(1, 0.2 + 0.8 * (time - h.gh_reel) / GH_RAMP);
    return h.gh_speed * ramp * h.gh_stamina_mult;
};

// The rope shortened by the reel this frame, from `xDist` (how far the load is): taking any slack in at once, never
// shorter than `xShortest` nor GH_STALL shorter than the load is far (the reel can't outrun its load; a stuck one stalls
// it).
void(entity h, float xDist, float xSpeed, float xShortest) VR_Grapple_Wind =
{
    float len = min(h.gh_len, xDist) - xSpeed * frametime;
    len = max(len, xDist - GH_STALL);
    h.gh_len = max(min(h.gh_len, max(xShortest, len)), 0);
};

// The rope snapping taut at `xSpeed` units/s (the load or the player going away from the other end): a twang, a jolt
// in the hand (self: the player).
void(entity h, float xHand, float xSpeed) VR_Grapple_Taut =
{
    if(xSpeed < 120)
    {
        return;
    }
    float k = min(1, (xSpeed - 120) / 500);
    sound(self, CHAN_AUTO, "vr/grapple_taut.wav", 0.35 + 0.65 * k, ATTN_NORM);
    VR_Grapple_Haptic(xHand, 0.08, 70, 0.3 + 0.7 * k);
    if(VR_Grapple_Debug())
    {
        dprint(sprintf("grapple: rope taut at %.0f u/s\n", xSpeed));
    }
};

// The player (self) held by the rope: never farther from `xAt` than its length (put back on it, swept: a wall stops
// that, the rope then stalls), no speed away from it, and reeling (`xReeling`) `xReel` units/s towards it, no faster
// (arriving, it slows down). Swinging is what's left: gravity along the rope's circle.
void(entity h, float xHand, vector xAt, float xReel, float xReeling) VR_Grapple_HoldPlayer =
{
    vector toA = xAt - self.origin;
    float d = vlen(toA);
    if(xReeling && d >= 1)
    {
        float closing = self.velocity * toA * (1 / d);
        if(closing > xReel + 20)
        {
            self.velocity = self.velocity - toA * ((closing - xReel) / d);
        }
    }
    if(d <= h.gh_len || d < 1)
    {
        h.gh_taut = FALSE;
        return;
    }
    vector n = toA * (1 / d);
    float along = self.velocity * n;
    if(!h.gh_taut)
    {
        VR_Grapple_Taut(h, xHand, -along);
    }
    h.gh_taut = TRUE;

    float excess = d - h.gh_len;
    vector move = n * excess;
    tracebox(self.origin, self.mins, self.maxs, self.origin + move, FALSE, self);
    float moved = trace_fraction;
    if(!trace_startsolid && !trace_allsolid)
    {
        setorigin(self, trace_endpos);
        if(trace_fraction < 1)
        {
            // Against something (a ceiling's beam, a wall): the rest slid along it, as a body on a rope swings round.
            vector rest = move * (1 - trace_fraction);
            rest = rest - trace_plane_normal * (rest * trace_plane_normal);
            tracebox(self.origin, self.mins, self.maxs, self.origin + rest, FALSE, self);
            if(!trace_startsolid && !trace_allsolid)
            {
                setorigin(self, trace_endpos);
            }
        }
    }
    if(VR_Grapple_Debug() >= 3)
    {
        dprint(sprintf("grapple: hold %.1f over, moved %.2f of it (then slid %.2f), along %.0f, on ground %g\n", excess,
            moved, trace_fraction, along, self.flags & FL_ONGROUND));
    }
    if(along < xReel)
    {
        self.velocity = self.velocity + n * (xReel - along);
    }
    // Lifted: off the floor (Quake's walking would hold it there, with its friction).
    if((xReel > 0 && n_z > 0.1) || excess * n_z > 0.25)
    {
        self.flags = self.flags - (self.flags & FL_ONGROUND);
    }
};

// A prop pulled at `xAt` (the hook on it) so that the point goes at least `xVel`: Box3D's push (its mass and turn: a
// box pulled by a corner swings round), or, not a body (yet), its velocity.
void(entity t, vector xAt, vector xVel) VR_Grapple_PullProp =
{
    float speed = vlen(xVel);
    if(speed < 0.5)
    {
        return;
    }
    if(physicspush(t, xAt, xVel))
    {
        return;
    }
    vector dir = xVel * (1 / speed);
    float along = t.velocity * dir;
    if(along < speed)
    {
        t.velocity = t.velocity + dir * (speed - along);
    }
    t.flags = t.flags - (t.flags & FL_ONGROUND);
};

// A monster pulled along `xDir` at `xSpeed` units/s, the way it can move: flying and swimming ones swept to where they
// get this frame (a wall stops them; a fish stays in its water), walking ones slid over the floor (combat.qc's
// shove slide: its feet just clear, so its AI can't walk it back; it goes over edges and lands hurt), lifted only when
// small and the rope pulls up steeply.
void(entity h, entity m, vector xDir, float xSpeed) VR_Grapple_MoveMonster =
{
    if(m.flags & (FL_FLY | FL_SWIM))
    {
        tracebox(m.origin, m.mins, m.maxs, m.origin + xDir * xSpeed * frametime, FALSE, m);
        if(trace_startsolid || trace_allsolid)
        {
            return;
        }
        if((m.flags & FL_SWIM) && pointcontents(trace_endpos) > CONTENT_WATER)
        {
            return;
        }
        setorigin(m, trace_endpos);
        return;
    }

    float g = cvar_hget(cvarh_sv_gravity) * (m.gravity ? m.gravity : 1) * frametime;
    if(h.gh_class == GH_SMALL && xDir_z > 0.6)
    {
        m.velocity = xDir * xSpeed + '0 0 1' * g;
        m.flags = m.flags - (m.flags & FL_ONGROUND);
        return;
    }
    vector flat = xDir;
    flat_z = 0;
    float f = vlen(flat);
    if(f < 0.05)
    {
        return;
    }
    flat = flat * (1 / f);
    float speed = xSpeed * f;
    m.velocity_x = flat_x * speed;
    m.velocity_y = flat_y * speed;
    if(m.flags & FL_ONGROUND)
    {
        m.velocity_z = g; // its feet off the floor this frame (as the slide keeps them)
        m.flags = m.flags - FL_ONGROUND;
    }
    if(m.movetype == MOVETYPE_STEP)
    {
        VR_Shove_Start(m, flat, speed);
    }
};

// A small monster reeled in, staggered: its pain, forced (as a shove's: vr_melee.qc VR_Bash_Hit), cutting off its
// attack, and no attack for a second. Zombies get a light pain (a hard one knocks them down).
void(entity m) VR_Grapple_Stagger =
{
    if(m.health <= 0 || !m.th_pain)
    {
        return;
    }
    float amount = 70; // over every pain's chance to shrug it off (a scrag's is random() * 70)
    if(m.classname == "monster_zombie")
    {
        amount = 20;
    }
    entity oldSelf = self;
    self = m;
    m.pain_finished = 0;
    m.th_pain(oldSelf, amount);
    m.attack_finished = time + 1;
    self = oldSelf;
    if(VR_Grapple_Debug())
    {
        dprint(sprintf("grapple: %s staggered\n", m.classname));
    }
};

// ----------------------------------------------------------------------------
// Every frame
// ----------------------------------------------------------------------------

// Whether what the hook bit is gone (freed, taken, dead, held, force grabbed): the hook comes off.
float(entity h) VR_Grapple_TargetGone =
{
    entity t = h.enemy;
    if(t == world)
    {
        return FALSE;
    }
    if(t.model == string_null)
    {
        return TRUE; // freed, or taken (a pickup's model goes)
    }
    if(t.classname == "player")
    {
        return t.health <= 0 || t.teleport_time > time;
    }
    if(h.gh_kind == GH_MONSTER || h.gh_class)
    {
        return t.health <= 0 || !(t.flags & FL_MONSTER);
    }
    if(h.gh_mass > 0)
    {
        return t.solid == SOLID_NOT || t.carry_player != world || t.fg_state != 0;
    }
    return FALSE;
};

// Rogue's grapple (vr_grapple_rope 0): pulls the player in as soon as it bites, whatever it bit.
void(entity h, float xHand) VR_Grapple_AutoPull =
{
    vector vel = VR_Grapple_HookPoint(h) - VRGetMuzzlePos(xHand);
    float v = vlen(vel);
    if(v >= 350)
    {
        vel = normalize(vel) * 350;
    }
    float dot = vel * self.velocity;
    if(dot < 0 || vlen(self.velocity) < 800)
    {
        self.velocity = self.velocity + (vel * 8 * frametime);
    }
    self.flags = self.flags - (self.flags & FL_ONGROUND);
};

// The reel's sound and buzz while it winds (`xLoad` 0 .. 1: how hard it pulls; `xMoving`: FALSE when it stalls).
void(entity h, float xHand, float xLoad, float xMoving) VR_Grapple_ReelFeel =
{
    if(time >= h.gh_sound_at)
    {
        // Clicks a little slower under a heavy load, none when it stalls (only the strain in the hand).
        if(xMoving)
        {
            sound(self, CHAN_AUTO, "vr/grapple_reel.wav", 0.55, ATTN_NORM);
        }
        h.gh_sound_at = time + 0.25 * (1 + 0.6 * xLoad);
    }
    if(time >= h.gh_haptic_at)
    {
        float amp = 0.12 + 0.35 * xLoad;
        if(!xMoving)
        {
            amp = 0.55;
        }
        VR_Grapple_Haptic(xHand, 0.06, 90 + 60 * (1 - xLoad), amp);
        h.gh_haptic_at = time + 0.05;
    }
};

// The monster reel's stamina (vr_grapple_stamina a second, paid four times a second): short of it, a quarter as fast.
void(entity h) VR_Grapple_Stamina =
{
    float cost = cvar_hget(cvarh_vr_grapple_stamina);
    if(cost <= 0 || !VR_Stamina_On())
    {
        h.gh_stamina_mult = 1;
        return;
    }
    if(time < h.gh_stamina_at)
    {
        return;
    }
    h.gh_stamina_at = time + 0.25;
    float share = VR_Stamina_Effort(cost * 0.25, "grapple reel");
    h.gh_stamina_mult = 1;
    if(share < 1)
    {
        h.gh_stamina_mult = 0.25;
    }
};

// A reel's progress in the log (vr_grapple_debug), four times a second: how far, the rope, how fast it closes (the
// distance's change since the last line: flying monsters are moved, not given a velocity), the load's velocity along
// the rope, the reel's speed now.
.float gh_log_d; // the distance at the last line
void(entity h, float xHand, float xDist, float xSpeed, string xWho) VR_Grapple_Log =
{
    if(!VR_Grapple_Debug() || time < h.gh_log_at)
    {
        return;
    }
    float rate = 0;
    if(h.gh_log_at > 0 && h.gh_log_d > 0)
    {
        rate = (h.gh_log_d - xDist) / (time - (h.gh_log_at - 0.25));
    }
    h.gh_log_at = time + 0.25;
    h.gh_log_d = xDist;
    dprint(sprintf("grapple: hand %g reels %s: %.0f away, rope %.0f, closing %.0f u/s (velocity %.0f, reel %.0f)\n", xHand,
        xWho, xDist, h.gh_len, rate, xSpeed, VR_Grapple_ReelSpeed(h)));
};

// The hook of a prop: it comes to the gun while reeling (slowing to stop at the rope's shortest, where it hangs to be
// taken), and the rope holds it at its length.
// A plain pickup (VR_Grapple_IsPlainPickup) reeled in: flown to the gun (it has no body: its velocity, set), still
// when the reel stops; at the gun, you take it (its hand touch, as if you had walked into it).
void(entity h, float xHand, float xReeling) VR_Grapple_ServicePlain =
{
    entity t = h.enemy;
    vector at = VR_Grapple_HookPoint(h);
    vector toGun = VRGetMuzzlePos(xHand) - at;
    float d = vlen(toGun);
    vector n = toGun * (1 / max(0.5, d));
    float shortest = GH_PROP_GAP + 0.5 * max(t.size_x, max(t.size_y, t.size_z));
    float want = 0;
    if(xReeling)
    {
        float speed = VR_Grapple_ReelSpeed(h);
        VR_Grapple_Wind(h, d, speed, shortest);
        want = min(speed, sqrt(2 * GH_BRAKE * max(0, d - shortest)));
        VR_Grapple_ReelFeel(h, xHand, 0.2, TRUE);
        VR_Grapple_Log(h, xHand, d, t.velocity * n, t.classname);
    }
    float over = d - h.gh_len - GH_HOLD_SLACK;
    if(over > 0)
    {
        want = max(want, min(GH_HOLD_SPEED, over * 8));
    }
    // Pulled: towards the gun, and along with you; not: still where it is.
    t.velocity = '0 0 0';
    if(want > 0)
    {
        t.velocity = self.velocity + n * want;
    }
    if(d <= shortest + 8)
    {
        dprint(sprintf("grapple: %s reeled in: taken\n", t.classname));
        // As your body walking into it takes it (its hand touch, by the "fake hand": vr_util.qc).
        entity oldSelf = self;
        entity oldOther = other;
        t.velocity = '0 0 0';
        self.touchinghand = cVR_FakeHand;
        t.handtouch_hand = cVR_FakeHand;
        t.handtouch_ent = self;
        other = self;
        self = t;
        self.handtouch();
        self = oldSelf;
        other = oldOther;
    }
};

void(entity h, float xHand, float xReeling) VR_Grapple_ServiceProp =
{
    entity t = h.enemy;
    if(t.movetype == MOVETYPE_FLY && VR_Grapple_IsPlainPickup(t))
    {
        VR_Grapple_ServicePlain(h, xHand, xReeling);
        return;
    }
    vector at = VR_Grapple_HookPoint(h);
    vector gun = VRGetMuzzlePos(xHand);
    vector toGun = gun - at;
    float d = vlen(toGun);
    if(d < 0.5)
    {
        return;
    }
    vector n = toGun * (1 / d);
    // It comes right to the gun, hanging GH_PROP_GAP past half its size from the muzzle (to take with the other hand; a
    // big box stays further).
    float shortest = GH_PROP_GAP + 0.5 * max(t.size_x, max(t.size_y, t.size_z));
    float closing = (t.velocity - self.velocity) * n; // how fast it comes, the player's own motion aside
    float want = 0;
    float speed = 0;
    if(xReeling)
    {
        speed = VR_Grapple_ReelSpeed(h);
        VR_Grapple_Wind(h, d, speed, shortest);
        want = min(speed, sqrt(2 * GH_BRAKE * max(0, d - shortest)));
    }
    float over = d - h.gh_len - GH_HOLD_SLACK;
    if(over > 0)
    {
        if(!h.gh_taut)
        {
            VR_Grapple_Taut(h, xHand, -closing);
        }
        h.gh_taut = TRUE;
        want = max(want, min(GH_HOLD_SPEED, over * 8));
    }
    else if(d < h.gh_len)
    {
        h.gh_taut = FALSE;
    }

    if(closing < want)
    {
        float absWant = want + self.velocity * n;
        if(absWant > 0)
        {
            VR_Grapple_PullProp(t, at, n * absWant);
        }
    }
    else if(xReeling && closing > want + 40)
    {
        // Arriving: it slows to stop at the gun (a push can't slow it: its velocity).
        t.velocity = t.velocity - n * (closing - want);
    }
    // Hanging at the gun, the rope not pulling it this frame: its swing dies down. (Its velocity set now would undo the
    // pull's push: Box3D takes a changed .velocity over what the body has.)
    else if(h.gh_len <= shortest + 1 && d <= h.gh_len + GH_HOLD_SLACK)
    {
        t.velocity = t.velocity + (self.velocity - t.velocity) * min(1, 3 * frametime);
    }

    if(xReeling)
    {
        float load = min(1, h.gh_mass / max(1, cvar_hget(cvarh_vr_grapple_prop_anchor)));
        float moving = FALSE; // (not `x = a || b`: see the fteqcc notes)
        if(closing > 20 || d < shortest + 8)
        {
            moving = TRUE;
        }
        VR_Grapple_ReelFeel(h, xHand, 0.2 + 0.8 * load, moving);
        VR_Grapple_Log(h, xHand, d, closing, sprintf("%s (%.1f kg)", t.classname != string_null ? t.classname : t.model,
            h.gh_mass));
    }
};

// The hook of a monster that comes (small or medium): pulled to the player while reeling (a small one staggered),
// held at the rope's length.
void(entity h, float xHand, float xReeling) VR_Grapple_ServiceMonster =
{
    entity m = h.enemy;
    vector toP = self.origin - VR_Grapple_HookPoint(h); // the rope: from the hook on it to your body
    float d = vlen(toP);
    if(d < 0.5)
    {
        return;
    }
    vector n = toP * (1 / d);
    // It stops a step from you: the hook on it (its side facing you) your half-width and a little more from your middle,
    // beyond the rope's shortest.
    float apart = 24;
    float shortest = max(0, cvar_hget(cvarh_vr_grapple_min_length)) + apart;
    float closing = (m.velocity - self.velocity) * n;
    float want = 0;
    float reel = 0;
    if(xReeling)
    {
        VR_Grapple_Stamina(h);
        reel = VR_Grapple_ReelSpeed(h);
        VR_Grapple_Wind(h, d, reel, shortest);
        want = min(reel, sqrt(2 * GH_BRAKE * max(0, d - shortest)));
        if(h.gh_class == GH_SMALL && cvar_hget(cvarh_vr_grapple_stagger) && time >= h.gh_stagger_at && want > 40)
        {
            VR_Grapple_Stagger(m);
            h.gh_stagger_at = time + 1;
        }
    }
    float over = d - h.gh_len - GH_HOLD_SLACK;
    if(over > 0)
    {
        if(!h.gh_taut)
        {
            VR_Grapple_Taut(h, xHand, -closing);
        }
        h.gh_taut = TRUE;
        want = max(want, min(GH_HOLD_SPEED, over * 6));
    }
    else if(d < h.gh_len)
    {
        h.gh_taut = FALSE;
    }
    if(want > 1)
    {
        VR_Grapple_MoveMonster(h, m, n, want);
    }
    else if(xReeling && d <= shortest + 4 && !(m.flags & (FL_FLY | FL_SWIM)))
    {
        // Arrived: it stops where it is (the slide would carry it into you).
        m.velocity_x = 0;
        m.velocity_y = 0;
        if(m.vr_shove != world)
        {
            m.vr_shove.speed = 0;
        }
    }

    if(xReeling)
    {
        float load = 0.45;
        if(h.gh_class == GH_MEDIUM)
        {
            load = 0.85;
        }
        float moving = FALSE;
        if(closing > 20 || d < shortest + 8)
        {
            moving = TRUE;
        }
        VR_Grapple_ReelFeel(h, xHand, load, moving);
        VR_Grapple_Log(h, xHand, d, closing, sprintf("%s (%s, %.0f kg)", m.classname, h.gh_class == GH_SMALL ? "small" :
            "medium", h.gh_mass));
    }
};

// The hook of what stays (the world, a door, a player, a heavy prop, a huge monster): the rope holds the player,
// reeling pulls him in.
void(entity h, float xHand, float xReeling) VR_Grapple_ServiceAnchor =
{
    vector at = VR_Grapple_HookPoint(h);
    float d = vlen(at - self.origin);
    float speed = 0;
    if(xReeling)
    {
        speed = VR_Grapple_ReelSpeed(h);
        VR_Grapple_Wind(h, d, speed, max(0, cvar_hget(cvarh_vr_grapple_min_length)));
        speed = min(speed, sqrt(2 * GH_BRAKE * max(0, d - max(0, cvar_hget(cvarh_vr_grapple_min_length)))));
    }
    VR_Grapple_HoldPlayer(h, xHand, at, speed, xReeling);
    if(xReeling)
    {
        float moving = FALSE;
        if(self.velocity * normalize(at - self.origin) > 20 ||
           vlen(at - self.origin) < max(0, cvar_hget(cvarh_vr_grapple_min_length)) + 8)
        {
            moving = TRUE;
        }
        VR_Grapple_ReelFeel(h, xHand, 0.6, moving);
        VR_Grapple_Log(h, xHand, vlen(at - self.origin), self.velocity * normalize(at - self.origin), "you");
    }

    // Rogue's: a hooked player bleeds.
    entity t = h.enemy;
    if(t.classname == "player" && time >= h.gh_hurt_at)
    {
        h.gh_hurt_at = time + 0.1;
        sound(h, CHAN_WEAPON, "pendulum/hit.wav", 1, ATTN_NORM);
        T_Damage(t, h, self, 1);
        vector spray;
        spray_x = 100 * crandom();
        spray_y = 100 * crandom();
        spray_z = 100 * crandom() + 50;
        SpawnBlood(h.origin, spray, 20);
    }
};

// called each frame by CLIENT.QC if client has hook_out
void(float xHand) GrappleService =
{
    entity h = getHook(xHand);
    if(h == world || h.classname != "hook" || h.owner != self)
    {
        setHookOut(xHand, FALSE);
        if(xHand == cVR_OffHand) self.on_hook2 = FALSE;
        else                     self.on_hook = FALSE;
        return;
    }

    // Let go of the trigger (in flight too), teleported, another weapon, dead: it comes off.
    if(!VRGetFireButtonPressed(xHand) || self.teleport_time > time || VRGetWeapon(xHand) != WID_GRAPPLE || self.health <= 0)
    {
        VR_Grapple_Release(xHand, "trigger, teleport, weapon or death", VRGetFireButtonPressed(xHand));
        return;
    }

    makevectors_mdl(h.angles);
    vector hookfwd = v_forward;
    makevectors(VRGetHandRot(xHand));
    vector gun = VRGetMuzzlePos(xHand) + v_forward * 1.5;

    if(!getOnHook(xHand))
    {
        // Flying: a prop on its way this frame?
        if(!VR_Grapple_FlightProps(h))
        {
            GrappleTrail(xHand, h.origin - hookfwd * 6.5, gun, 0);
            return;
        }
    }

    if(VR_Grapple_TargetGone(h))
    {
        VR_Grapple_Release(xHand, "what it bit is gone", TRUE);
        return;
    }

    // The hook stays where it bit (following what moves).
    vector at = VR_Grapple_HookPoint(h);
    setorigin(h, at);

    // Rogue's pull: chosen, or for a player without tracked hands (flat screen, a bot: no B/Y to reel with).
    if(!VR_Grapple_RopeMode() || !VRHandsTracked(self))
    {
        if(VR_Grapple_Debug() >= 2 && time >= h.gh_log_at)
        {
            h.gh_log_at = time + 0.25;
            dprint(sprintf("grapple: hand %g pulls you (Rogue's): %.0f away, at %v moving %v\n", xHand, vlen(at - self.origin),
                self.origin, self.velocity));
        }
        VR_Grapple_AutoPull(h, xHand);
        GrappleTrail(xHand, at - hookfwd * 6.5, gun, 0);
        return;
    }

    float reeling = VRGetEntSecondaryPressed(self, xHand);
    if(reeling && !h.gh_reel)
    {
        h.gh_reel = time;
        h.gh_reel_from = h.gh_len;
        h.gh_sound_at = time;
        h.gh_haptic_at = time;
        h.gh_log_at = time;
        h.gh_log_d = 0;
        h.gh_stagger_at = time;
        if(VR_Grapple_Debug())
        {
            dprint(sprintf("grapple: hand %g reel on, rope %.0f\n", xHand, h.gh_len));
        }
    }
    else if(!reeling && h.gh_reel)
    {
        if(VR_Grapple_Debug())
        {
            dprint(sprintf("grapple: hand %g reel off after %.2f s: rope %.0f -> %.0f\n", xHand, time - h.gh_reel,
                h.gh_reel_from, h.gh_len));
        }
        h.gh_reel = 0;
    }

    if(h.gh_kind == GH_PROP)
    {
        VR_Grapple_ServiceProp(h, xHand, reeling);
    }
    else if(h.gh_kind == GH_MONSTER)
    {
        VR_Grapple_ServiceMonster(h, xHand, reeling);
    }
    else
    {
        VR_Grapple_ServiceAnchor(h, xHand, reeling);
    }
    if(VR_Grapple_TargetGone(h))
    {
        VR_Grapple_Release(xHand, "what it bit is gone", TRUE); // (a pickup reeled in, taken)
        return;
    }

    // vr_grapple_debug 2: the rope's state four times a second, reeling or not (a hook that holds: nothing moves).
    if(VR_Grapple_Debug() >= 2 && !reeling && time >= h.gh_log_at)
    {
        h.gh_log_at = time + 0.25;
        entity load = h.enemy;
        vector loadAt = load.origin;
        if(h.gh_kind == GH_ANCHOR)
        {
            load = self;
            loadAt = self.origin;
        }
        dprint(sprintf("grapple: hand %g holds: rope %.0f, %.0f away, %s at %v moving %v\n", xHand, h.gh_len,
            vlen(VR_Grapple_HookPoint(h) - VR_Grapple_RopeEnd(h, xHand)), load.classname, loadAt, load.velocity));
    }

    // Drawn slack: the rope's length less how far its ends are, as a share of it.
    float slack = 0;
    if(h.gh_len > 1)
    {
        slack = max(0, h.gh_len - vlen(VR_Grapple_HookPoint(h) - VR_Grapple_RopeEnd(h, xHand))) / h.gh_len;
    }
    GrappleTrail(xHand, VR_Grapple_HookPoint(h) - hookfwd * 6.5, gun, slack);
};
