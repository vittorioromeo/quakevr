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
// - Thrown or batted back by a player (vr_grenade_return_full), an ogre's goes off as the player's grenade launcher's
//   (GrenadeExplode: 120 damage, 160 units round, with the player's Quad), not the ogre's (40, 80): VR_Grenade_Return.
// - Caught in flight (ROUND21.md, "Catching versus deflecting; returned grenades; sword bash direction"): an empty hand
//   that isn't striking (a punch, a slap, a shove or a bash knocks it away: VR_Deflect, VR_Bash_Deflect) catches a
//   grenade whose path passes within vr_grenade_catch_radius of its palm, with the grip held or closing
//   (VR_Grenade_CatchCheck, VR_Grenade_HandTouch); an open palm facing it stops it (VR_Grenade_Trap) and holds it
//   vr_grenade_catch_window seconds for the grip to close, then lets it drop. A hand only reaching never knocks it
//   away: no nudge while it flies, and the hands' Box3D spheres meet only solid props (.vr_rigid 2), never a grenade.
// - Hand grenades (vr_handgrenade; ROUND21.md, "Hand grenades from the back pouch"): an empty hand gripping at the
//   grenade pouch (the engine's hotspot QVR_HS_GRENADE_POUCH, at the small of the back) takes one while the player has
//   rockets (one leaves the ammo with it). It is the launcher's grenade (GrenadeExplode, GrenadeTouch: 120 over 160,
//   the player's, with the Quad) made live here (VR_Grenade_Setup), unarmed (.vr_hgren, no fuse) until its pin is
//   pulled (the trigger, vr_handgrenade_arm 0) or it is let go of (vr_handgrenade_arm 1): then its fuse
//   (vr_handgrenade_fuse) runs, lit as a caught grenade is. Let go of unarmed at the pouch, it goes back in (the rocket
//   back); elsewhere (arming on the trigger) it drops as a dud, which can be taken again, armed or put back. Thrown,
//   it is any grenade you throw: caught, going off on a monster or its fuse, in your hand if held too long.
//   With the hand's upper face button (B/Y) held as it grips (ROUND21.md, "Multi-grenades from the pouch"), the pouch
//   gives the mission pack's multi-grenade instead while the player has multi-rockets (Dissolution of Eternity's: its
//   ammo, progs/mervup.mdl, MultiGrenadeExplode's cluster on its fuse, MultiGrenadeTouch's rule on a hit), else the
//   grenade: held, armed, thrown and caught as the grenade is (.vr_hgren_multi; muted until armed as well).
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
.float vr_gren_made;     // when it was last thrown or let go of (VR_GREN_CATCH_AGE: not caught again as it leaves the hand
                         // or your launcher)
.void() vr_gren_boom0;   // its thrower's own explosion (in the hand it is theirs, even returned)
.float vr_gren_returned; // thrown or batted back by a player: goes off as the launcher's (VR_Grenade_Return)
.entity vr_gren_trap;    // the player whose open palm holds it (VR_Grenade_Trap) for the grip to close on it,
.float vr_gren_trap_hand; // with this hand,
.float vr_gren_trap_until; // until then (vr_grenade_catch_window),
.float vr_gren_trap_time; // from then
.float vr_hgren;         // a hand grenade from the pouch (VR_HandGrenade_Take): its fuse runs only once it is armed,
.float vr_hgren_armed;   // its pin pulled (or let go of, vr_handgrenade_arm 1)
.float vr_hgren_multi;   // a multi-grenade (B/Y held at the pouch): its ammo is the multi-rockets, not the rockets
.float vr_pouch_at;      // (a player) the hands at the grenade pouch last frame (1 off hand, 2 main hand),
.float vr_pouch_tried;   // and the hands whose grip has tried the pouch (until they let go: one try a press)

float VR_GREN_FLYING = 150;     // u/s: faster, a grenade is in flight (caught within the palm's reach, never nudged);
                                // slower, it lies or rolls (touched with the grip pressed, or nudged: the carry's rule)
float VR_GREN_CATCH_AGE = 0.2;  // s after its throw a grenade can be caught (not as it leaves your hand or launcher)
float VR_GREN_PALM = 1.5;       // units from the hand's point to the palm, the way the palm faces
float VR_GREN_PALM_FACING = 0.2; // an open palm stops a grenade coming at it this square (cosine) or more: not edge-on

float VR_GREN_BOUNCE = 90;     // u/s: a jump in its velocity (gravity's taken off) this big is a bounce
float VR_GREN_TICK_SLOW = 0.5; // s between ticks with vr_grenade_held_fuse (or more) left,
float VR_GREN_TICK_FAST = 0.08; // down to this as it goes off
float VR_GREN_BODY_MARGIN = 3; // units round a VR player's body capsule a grenade goes off on it (VR_Grenade_OnBody)

float VR_LOB_SPEED = 632.46;   // u/s: Quake's lob (600 ahead, 200 up) on level ground: an ogre's, a zombie's
float VR_LOB_HARDER = 1.2;    // times that an out-of-reach lob may be thrown (a ledge high above: still "close to" Quake's)
float VR_LOB_LEAD = 0.3;       // share of the target's walk over the flight a lob leads it by (slightly)
float VR_LOB_STEPS = 10;       // the arc's segments traced for a wall in the way (VR_Lob_Clear)

// ----------------------------------------------------------------------------
// Shooting grenades (vr_grenade_shoot; ROUND21.md, "Shooting grenades")
//
// A grenade (the launcher's, an ogre's or a multi-grenade ogre's, rogue's multi-grenade, a hand grenade, live or a dud
// lying about) is set off by whatever hits it: a shot (the shotguns', a grunt's, the lightning), a missile (nails,
// rockets, lasers, spit), a thrown thing, a very strong melee blow (vr_grenade_shoot_melee; VR_GrenShot_BlowSetsOff: a
// full-speed one, or a running chainsaw; any other blow bats it: VR_GrenShot_Knock, or VR_Deflect for a monster's in
// flight; an empty hand catches one in flight), and another blast (a chain, each a moment after the last). Quake's
// grenades are points no shot meets: the engine meets a cube round its middle, its model's half size and
// vr_grenade_shoot_pad (.vr_shot_radius: vr_hitmodel.cpp VR_ShotTargetClip), its owner's own shots too once it has left
// his launcher or hand (VR_GRENSHOT_OWN_GRACE). Not one in a hand or an open palm. What it blows up is the doing of who
// set it off (its owner then: T_RadiusDamage's attacker), with its own blast (an ogre's stays an ogre's). Hipnotic's
// proximity mines were shootable already (once they land).

float VR_GRENSHOT_CHAIN = 0.12;     // s a grenade a blast sets off waits (a chain ripples, not one bang)
float VR_GRENSHOT_OWN_GRACE = 0.15; // s after it leaves its owner's launcher or hand before his own shots meet it

// Whether `g` is a grenade shots set off now (VR_GrenShot_Make, not set off yet).
float(entity g) VR_GrenShot_Is =
{
    return g.vr_shootnade && !g.vr_shot_off;
};

// Whether shots meet the grenade `g` now (set by VR_GrenShot_Update): vr_grenade_shoot on, not set off or gone off,
// not in a hand or an open palm.
void(entity g) VR_GrenShot_Update =
{
    if(!VR_GrenShot_Is(g) || !cvar_hget(cvarh_vr_grenade_shoot) || g.carry_player != world || g.vr_gren_trap != world ||
       g.solid == SOLID_NOT)
    {
        g.takedamage = DAMAGE_NO;
        g.vr_shot_radius = 0;
        return;
    }
    g.takedamage = DAMAGE_YES; // (not DAMAGE_AIM: no autoaim at it, and Quake's grenades bounce off it)
    g.health = 1; // (what some touches test: Hipnotic's laser)
    g.vr_shot_radius = max(0.5, g.vr_shot_half + max(0, cvar_hget(cvarh_vr_grenade_shoot_pad)));
};

// Makes the grenade `g` (its model set, just thrown) one that shots and blasts set off.
void(entity g) VR_GrenShot_Make =
{
    vector lo = modelbounds(g, 0);
    vector hi = modelbounds(g, 1);
    g.vr_shootnade = TRUE;
    g.vr_shot_off = FALSE;
    g.vr_shot_half = max(hi_x - lo_x, max(hi_y - lo_y, hi_z - lo_z)) * 0.5;
    g.vr_shot_ownfrom = time + VR_GRENSHOT_OWN_GRACE;
    VR_GrenShot_Update(g);
};

// No longer one shots meet (it went off).
void(entity g) VR_GrenShot_Spent =
{
    g.vr_shot_off = TRUE;
    VR_GrenShot_Update(g);
};

// A Quake grenade set off (self; not a catchable one, which its tick sets off: VR_Grenade_Think) goes off: its blast.
void() VR_GrenShot_Boom =
{
    dprint(sprintf("grenade: %s goes off at %v (%s's)\n", self.classname, self.origin, self.owner.classname));
    self.think = self.vr_shot_boom;
    self.think();
};

// Sets the grenade `g` off, `xBy` its doing (the kills are theirs), `xDelay` s from now (`xHow` for the log): the next
// tick at the soonest (a blast's damage, a shotgun's pellets: not amid the damage's own loops).
void(entity g, entity xBy, float xDelay, string xHow) VR_GrenShot_SetOff =
{
    VR_GrenShot_Spent(g);
    if(xBy != world && xBy != g && (xBy.flags & (FL_CLIENT | FL_MONSTER)))
    {
        g.owner = xBy;
    }
    string fuse = "a dud";
    if(!g.vr_hgren || g.vr_hgren_armed)
    {
        fuse = sprintf("%.2f s before its fuse", (g.vr_grenade ? g.vr_gren_fuse : g.nextthink) - time);
    }
    dprint(sprintf("grenade: %s set off by %s (%s) at %v, %s's (%s, %.0f u/s)\n", g.classname, xBy.classname, xHow,
        g.origin, g.owner.classname, fuse, vlen(g.velocity)));
    if(g.vr_grenade)
    {
        if(g.vr_hgren && !g.vr_hgren_armed)
        {
            g.vr_hgren_armed = TRUE; // (a dud: no pin, no fuse; it goes off all the same)
            g.skin = 0;
        }
        g.vr_gren_fuse = time + xDelay; // (its tick: VR_Grenade_Think)
        return;
    }
    g.vr_shot_boom = g.think; // (Quake's grenade: its think is its blast)
    g.think = VR_GrenShot_Boom;
    g.nextthink = time + xDelay;
};

// Melee blows on grenades (vrfiringrange_2026-10-01_16-55-46, _17-14-36; ROUND21.md, "Grenades: blows bat, strong
// ones set off"): a blow bats a grenade; only a very strong one sets it off: the blow's speed (VR_Blow's mh_speed, the
// arm's; a pommel's its own) at least vr_grenade_melee_speed (a punch, a gun's swing, a pommel, a butt) or
// vr_grenade_melee_speed_weapon (a blade's or a head's: a sword, the axe, the crowbar, a club, a stopped chainsaw: they
// go round faster), and its damage at least vr_grenade_melee_damage. A running chainsaw always does (vr_grenshot_saw:
// its chain cutting, VR_Chainsaw_Cut, or swung running, W_ChainsawMelee). A headbutt only bats.
float vr_grenshot_saw; // set while a running chainsaw's blow or cut deals its damage
float VR_GRENSHOT_KNOCK_MIN = 300;  // u/s a grenade a blow bats flies at least (VR_GrenShot_Knock),
float VR_GRENSHOT_KNOCK_LIFT = 0.25; // and one lying on the floor rises at least this share of that (not a skid)
float VR_GRENSHOT_BATTED = 0.3;     // s after it was batted (VR_Deflect_Send) a blow leaves it be (the same swing)

// Whether the melee blow dealing `damage` to a grenade now (vr_hithand's, VR_Blow) sets it off; vr_grenshot_why says
// how strong it was against what it needed (for the log).
string vr_grenshot_why;
float(float damage) VR_GrenShot_BlowSetsOff =
{
    if(vr_grenshot_saw)
    {
        vr_grenshot_why = "a running chainsaw";
        return TRUE;
    }
    float hand = vr_hithand;
    if(hand != cVR_MainHand && hand != cVR_OffHand)
    {
        vr_grenshot_why = "not a hand's blow";
        return FALSE;
    }
    entity b = VR_Blow(hand);
    float heavy = ((b.mh_thing == VR_MTHING_SWORD || b.mh_thing == VR_MTHING_AXE || b.mh_thing == VR_MTHING_CLUB ||
                    b.mh_thing == VR_MTHING_SAW || b.mh_thing == VR_MTHING_CROWBAR) && b.mh_kind != VR_MKIND_POMMEL);
    float need = max(0, cvar_hget(heavy ? cvarh_vr_grenade_melee_speed_weapon : cvarh_vr_grenade_melee_speed));
    float least = max(0, cvar_hget(cvarh_vr_grenade_melee_damage));
    vr_grenshot_why = sprintf("%.1f m/s of %.1f, %.1f damage of %.1f", b.mh_speed, need, damage, least);
    return b.mh_speed >= need && damage >= least;
};

// A melee blow of the player (self) that doesn't set the grenade `g` off bats it the way the struck part was going (the
// fist or grip for a punch or a pommel, the far end for a swing or a stab), at that speed (VR_GRENSHOT_KNOCK_MIN ..
// VR_BAT_MAX_SPEED): one lying on the floor lifted a little. It is the player's then (VR_Deflect_Send).
void(entity g) VR_GrenShot_Knock =
{
    float hand = vr_hithand;
    vector v = '0 0 0';
    if(hand == cVR_MainHand || hand == cVR_OffHand)
    {
        entity b = VR_Blow(hand);
        v = b.mh_vfar;
        if(b.mh_kind == VR_MKIND_POMMEL || b.mh_kind == VR_MKIND_PUNCH)
        {
            v = b.mh_vgrip;
        }
        v = VR_Blow_ToWorld(v) * VRMetersToUnits() + self.velocity;
    }
    else
    {
        v = vr_hitdir * VR_GRENSHOT_KNOCK_MIN;
    }
    float speed = vlen(v);
    if(speed < 1)
    {
        v = g.origin - self.origin; // (no way known: away from you)
        speed = 1;
    }
    float want = min(VR_BAT_MAX_SPEED, max(VR_GRENSHOT_KNOCK_MIN, speed));
    v = v * (want / max(1, vlen(v)));
    if((g.flags & FL_ONGROUND) && v_z < want * VR_GRENSHOT_KNOCK_LIFT)
    {
        v_z = want * VR_GRENSHOT_KNOCK_LIFT;
    }
    dprint(sprintf("grenade: %s batted by a blow (%s): %.0f u/s\n", g.classname, vr_grenshot_why, vlen(v)));
    VR_Deflect_Send(g, v, hand < 0 ? cVR_MainHand : hand, "blow");
};

// T_Damage on a grenade shots set off (T_DamageImpl; `xMelee` a VR melee blow's): set off, by `xAttacker`. Not by its
// own blast; a melee blow only a very strong one (VR_GrenShot_BlowSetsOff), any other bats it (a monster's in flight
// batted by the swing, VR_Deflect, or caught by an empty hand, as before; the rest VR_GrenShot_Knock). TRUE if it was
// set off.
float(entity targ, entity inflictor, entity attacker, float damage, float xMelee) VR_GrenShot_Damage =
{
    if(!VR_GrenShot_Is(targ) || !targ.takedamage || targ.solid == SOLID_NOT || inflictor == targ || damage <= 0)
    {
        return FALSE;
    }
    if(xMelee)
    {
        if(!cvar_hget(cvarh_vr_grenade_shoot_melee))
        {
            return FALSE;
        }
        if(!VR_GrenShot_BlowSetsOff(damage))
        {
            if(time < targ.vr_deflect_time + VR_GRENSHOT_BATTED)
            {
                return FALSE; // (this swing batted it already)
            }
            if(cvar_hget(cvarh_vr_deflect) && vlen(targ.velocity) >= VR_GREN_FLYING && VR_Deflect_IsProjectile(targ))
            {
                dprint(sprintf("grenade: %s in flight met by a blow (%s): batted by the swing or caught, not set off\n",
                    targ.classname, vr_grenshot_why));
                return FALSE; // (VR_Deflect bats it this frame; an empty hand catches it)
            }
            if(attacker.flags & FL_CLIENT)
            {
                VR_GrenShot_Knock(targ);
            }
            return FALSE;
        }
    }
    string how = "a hit";
    float delay = 0;
    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        how = sprintf("a blast, %s's", inflictor.classname);
        delay = VR_GRENSHOT_CHAIN;
    }
    else if(xMelee)
    {
        how = sprintf("a blow: %s", vr_grenshot_why);
    }
    else if(inflictor != attacker)
    {
        how = inflictor.classname;
    }
    VR_GrenShot_SetOff(targ, attacker, delay, how);
    return TRUE;
};

// Where a shot or a blow meets a grenade: sparks, not blood (TraceAttack, spawn_touchblood, VR_HitBlood).
void(vector xAt) VR_GrenShot_Sparks =
{
    particle2(xAt, '0 0 40', QVR_PARTICLE_PRESET_SPARKS, 8);
};

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

// Makes the grenade `g` (its model, origin and velocity set) a live grenade you can catch: `xBoom` its explosion,
// `xFuse` seconds to it.
void(entity g, void() xBoom, float xFuse) VR_Grenade_Setup =
{
    g.vr_grenade = TRUE;
    g.vr_gren_boom = xBoom;
    g.vr_gren_boom0 = xBoom;
    g.vr_gren_fuse = time + xFuse;
    g.vr_gren_made = time;
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

// Makes the grenade `g` (just thrown: its model, origin and velocity set) catchable, if vr_grenade_catch says so:
// `xBoom` its explosion, `xFuse` seconds to it, `xPlayers` a player's launcher's.
void(entity g, void() xBoom, float xFuse, float xPlayers) VR_Grenade_Make =
{
    if(VR_Grenade_On(xPlayers))
    {
        VR_Grenade_Setup(g, xBoom, xFuse);
    }
    VR_GrenShot_Make(g); // (caught or not, a shot sets it off: vr_grenade_shoot)
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
            self.vr_gren_boom = self.vr_gren_boom0; // (the thrower's own blast, even if it was returned once)
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
    VR_GrenShot_Spent(g);
};

// Its own touch (OgreGrenadeTouch, GrenadeTouch, MultiGrenadeTouch: Quake's rules, it goes off on what it can aim at,
// not its thrower, else it bounces), except while held, and on a VR player's hands (VR_Grenade_OnBody).
void() VR_Grenade_Touch =
{
    if(other == self.owner || self.carry_player != world || !self.vr_gren_fuse || self.vr_gren_trap != world)
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
        self.vr_shot_off = FALSE; // (shots meet it again: VR_GrenShot_Update at its tick)
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
    if(!VR_HandGrenade_Armed(self))
    {
        dprint(sprintf("grenade: an unarmed hand grenade in hand %g\n", hand));
        return; // (no fuse to set: its pin, VR_HandGrenade_Arm)
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

// Whether `e` is a monster's grenade in flight (a catchable one, or Quake's ogre and multi-grenades): what an unarmed
// shove can push away (VR_Bash_Deflect; a palm can't bat a spike).
float(entity e) VR_Grenade_Is =
{
    return e.vr_grenade || e.think == OgreGrenadeExplode || e.think == MultiGrenadeExplode;
};

// Thrown or batted back by the player (self; `xHow` for the log): with vr_grenade_return_full an ogre's grenade goes off
// as the player's grenade launcher's (GrenadeExplode, GrenadeTouch: 120 damage over 160 units, T_RadiusDamage's
// attacker the player, so the player's Quad counts as it would for a launcher's), not the ogre's (OgreGrenadeExplode:
// 40 over 80). Only once it is the player's: in the ogre's flight it keeps the ogre's, and going off in the hand it is
// the ogre's again (VR_Grenade_Explode). A multi-grenade keeps rogue's rule (it goes off as a launcher's grenade on
// what it hits, as a cluster on its fuse), and the player's own is the launcher's already.
void(entity g, string xHow) VR_Grenade_Return =
{
    if(!cvar_hget(cvarh_vr_grenade_return_full) || g.vr_gren_returned)
    {
        return;
    }
    if(g.vr_grenade)
    {
        if(g.vr_gren_boom != OgreGrenadeExplode)
        {
            return;
        }
        g.vr_gren_boom = GrenadeExplode;
        g.vr_gren_touch = GrenadeTouch;
    }
    else if(g.think == OgreGrenadeExplode)
    {
        g.think = GrenadeExplode; // (Quake's grenade, vr_grenade_catch 0: its fuse and touch)
        g.touch = GrenadeTouch;
    }
    else
    {
        return;
    }
    g.vr_gren_returned = TRUE;
    dprint(sprintf("grenade: %s %s: goes off as your launcher's (120 damage, 160 units round), not its thrower's (40, 80)\n",
        g.classname, xHow));
};

// Where `xHand` of `p`'s palm faces (as VR_Carry_Start fits a held thing to it: a right hand's palm is its left).
vector(entity p, float xHand) VR_Grenade_PalmDir =
{
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;
    makevectors(VRGetEntHandRot(p, xHand));
    vector palm = xHand == cVR_MainHand ? v_right * -1 : v_right; // (cVR_MainHand: the right controller)
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return palm;
};

// Whether `xHand` of self (a player) may catch: an empty hand (not the flashlight's, not steadying the other's weapon,
// not climbing, carrying or force-grabbing) that isn't striking: not in a push (a shove, a bash) or its batting, and
// slower than a punch (VR_Deflect_Need: faster, it bats: VR_Deflect).
float(float xHand, entity b) VR_Grenade_HandCatches =
{
    if(!self.ishuman || self.health <= 0 || !VRHandsTracked(self))
    {
        return FALSE;
    }
    if(VR_Melee_Thing(self, xHand) != VR_MTHING_FIST || !VRIsHandEmpty(self, xHand) ||
       VR_Melee_HoldsTorch(self, xHand) || VR_Forcegrab_IsHandBusy(self, xHand))
    {
        return FALSE;
    }
    if(b.mh_pushing || (time <= self.vr_bashdef_until && (self.vr_bashdef_hand == xHand || self.vr_bashdef_hand < 0)))
    {
        return FALSE;
    }
    return vlen(b.mh_vfar) < VR_Deflect_Need(b);
};

// The hand `xHand` of the player `p` takes the grenade `g` (`xHow` for the log): carried (VR_Carry_Start; its fuse is
// set and it is lit at its next tick, VR_Grenade_Caught).
void(entity p, float xHand, entity g, string xHow) VR_Grenade_Take =
{
    dprint(sprintf("grenade: %s caught in flight by hand %g (%s), at %.0f u/s\n", g.classname, xHand, xHow,
        vlen(g.velocity)));
    g.vr_gren_trap = world;
    VR_Carry_Start(p, xHand, g);
};

// Keeps the grenade `g` in the open palm that stopped it (VR_Grenade_Trap): at the palm, moving with the hand.
void(entity g) VR_Grenade_TrapHold =
{
    entity p = g.vr_gren_trap;
    float h = g.vr_gren_trap_hand;
    vector at = VRGetEntHandPos(p, h) + VR_Grenade_PalmDir(p, h) * (VR_GREN_PALM + g.maxs_x);
    setorigin(g, at - (g.mins + g.maxs) * 0.5);
    g.velocity = VRGetEntHandVel(p, h) * VRMetersToUnits();
};

// An open palm (`xHand` of self) facing the grenade `g` stops it: it rests there vr_grenade_catch_window seconds for the
// grip to close on it (VR_Grenade_Think), not a rigid body meanwhile (MOVETYPE_NONE), then drops from the hand. With no
// window, it isn't stopped (the grip must be closing as it arrives).
float(float xHand, entity g) VR_Grenade_Trap =
{
    float window = min(1, max(0, cvar_hget(cvarh_vr_grenade_catch_window)));
    if(window <= 0)
    {
        return FALSE;
    }
    vector palm = VR_Grenade_PalmDir(self, xHand);
    if(palm * normalize(g.velocity * -1) < VR_GREN_PALM_FACING)
    {
        return FALSE; // (the edge or back of the hand: it flies on)
    }
    dprint(sprintf("grenade: %s stopped by the open palm of hand %g at %.0f u/s (facing it %.2f): %.2f s for the grip\n",
        g.classname, xHand, vlen(g.velocity), palm * normalize(g.velocity * -1), window));
    g.vr_gren_trap = self;
    g.vr_gren_trap_hand = xHand;
    g.vr_gren_trap_time = time;
    g.vr_gren_trap_until = time + window;
    g.movetype = MOVETYPE_NONE;
    g.flags = g.flags - (g.flags & FL_ONGROUND);
    VR_Grenade_TrapHold(g);
    sound(g, CHAN_VOICE, "weapons/bounce.wav", 0.35, ATTN_NORM); // (a soft slap into the palm)
    haptic(xHand, 0.0, 0.06, 60, 0.5);
    return TRUE;
};

// The trap's end (self is the grenade), at its tick: taken if the grip closed, else it drops from the open hand with the
// hand's motion. TRUE while it goes on.
float() VR_Grenade_TrapTick =
{
    entity p = self.vr_gren_trap;
    float h = self.vr_gren_trap_hand;
    float lost = p.health <= 0 || !VRIsHandEmpty(p, h);
    if(!lost && VR_HandGrabUtil_IsHandGrabbing(p, h))
    {
        VR_Grenade_Take(p, h, self, sprintf("the palm's grip, %.2f s after it met it", time - self.vr_gren_trap_time));
        return FALSE;
    }
    if(!lost && time <= self.vr_gren_trap_until)
    {
        VR_Grenade_TrapHold(self);
        return TRUE;
    }
    self.vr_gren_trap = world;
    VR_Carry_Loose(self); // (a rigid body again: movetype, solid, vr_rigid)
    self.velocity = VRGetEntHandVel(p, h) * VRMetersToUnits();
    dprint(sprintf("grenade: %s drops from the open hand %g (no grip in %.2f s)\n", self.classname, h,
        time - self.vr_gren_trap_time));
    return FALSE;
};

// A grenade in flight `g` meeting `xHand` of self, not striking (VR_Grenade_HandCatches): the grip held or closing
// takes it; an open palm facing it stops it (VR_Grenade_Trap); its edge or back lets it fly on. TRUE if it was
// taken or stopped.
float(float xHand, entity g, string xHow) VR_Grenade_Meet =
{
    if(VR_HandGrabUtil_IsHandGrabbing(self, xHand))
    {
        VR_Grenade_Take(self, xHand, g, xHow);
        return TRUE;
    }
    return VR_Grenade_Trap(xHand, g);
};

// Whether `g` is a live grenade in flight (not lying or rolling).
float(entity g) VR_Grenade_InFlight =
{
    return g.vr_grenade && g.vr_gren_fuse && g.carry_player == world && vlen(g.velocity) >= VR_GREN_FLYING;
};

// Whether `g` is a live grenade in flight a hand may catch now.
float(entity g) VR_Grenade_Catchable =
{
    return g.vr_grenade && g.vr_gren_fuse > time && g.carry_player == world && g.vr_gren_trap == world &&
           time - g.vr_gren_made >= VR_GREN_CATCH_AGE && vlen(g.velocity) >= VR_GREN_FLYING;
};

// Each frame, for `xHand` of self (before its batting, VR_Deflect; `b` its blow): a grenade in flight whose path this
// frame (from where it was a frame ago to half a frame ahead) passes within vr_grenade_catch_radius of the palm is
// met by it (VR_Grenade_Meet), if the hand is empty and not striking. Swept, so a fast grenade doesn't skip the hand.
void(float xHand, entity b) VR_Grenade_CatchCheck =
{
    if(!VR_Grenade_On(FALSE) || !VR_Grenade_HandCatches(xHand, b))
    {
        return;
    }
    vector palm = VRGetHandPos(xHand) + VR_Grenade_PalmDir(self, xHand) * VR_GREN_PALM;
    float reach = max(0, cvar_hget(cvarh_vr_grenade_catch_radius)) * 0.01 * VRMetersToUnits();
    float dt = max(frametime, 0.011);
    for(entity g = findradius(palm, 160); g; g = g.chain)
    {
        if(!VR_Grenade_Catchable(g))
        {
            continue;
        }
        vector mid = g.origin + (g.mins + g.maxs) * 0.5;
        float d = VR_SegmentDistance(mid - g.velocity * dt, mid + g.velocity * (dt * 0.5), palm, palm);
        if(d <= reach + g.maxs_x)
        {
            if(VR_Grenade_Meet(xHand, g, sprintf("the palm's reach, %.1f units off", d)))
            {
                return;
            }
        }
    }
};

// A hand's touch on a live grenade in flight (VR_Carry_Handtouch; self is the grenade, `xPlayer` the player): never a
// nudge (a reaching hand knocked it away before); met as by the palm's reach (VR_Grenade_Meet) if the hand is empty and
// not striking (a striking hand's batting is VR_Deflect's). TRUE if it was handled here (in flight or in a palm).
float(entity xPlayer, float xHand) VR_Grenade_HandTouch =
{
    if(!self.vr_grenade || !self.vr_gren_fuse || (vlen(self.velocity) < VR_GREN_FLYING && self.vr_gren_trap == world))
    {
        return FALSE;
    }
    if(!VR_Grenade_Catchable(self))
    {
        return TRUE;
    }
    entity g = self;
    self = xPlayer;
    if(VR_Grenade_HandCatches(xHand, VR_Blow(xHand)))
    {
        VR_Grenade_Meet(xHand, g, "touched");
    }
    self = g;
    return TRUE;
};

// Let go of (VR_Carry_Release; self is the player, `xBox` what the hand let go of): a grenade is the player's now.
void(entity xBox, float xHand) VR_Grenade_Released =
{
    if(!xBox.vr_grenade)
    {
        return;
    }
    if(!VR_HandGrenade_Armed(xBox))
    {
        if(cvar_hget(cvarh_vr_handgrenade_arm) == 1)
        {
            VR_HandGrenade_Arm(xBox, xHand, "the lever flies off as it leaves the hand");
        }
        else
        {
            dprint("grenade: a hand grenade let go of unarmed: a dud (take it again to arm it, or put it in the pouch)\n");
        }
    }
    xBox.owner = self;
    xBox.vr_gren_made = time; // (not caught again by the hand it leaves)
    xBox.vr_shot_ownfrom = time + VR_GRENSHOT_OWN_GRACE; // (nor shot by you as it leaves the hand)
    SetThrownWeaponDealDamage(xBox, FALSE); // (it hurts by going off, not as a thrown box)
    if(VR_HandGrenade_Armed(xBox))
    {
        dprint(sprintf("grenade: let go of at %v (%.0f u/s), %.2f s left\n", xBox.velocity, vlen(xBox.velocity),
            xBox.vr_gren_fuse - time));
    }
    else
    {
        dprint(sprintf("grenade: let go of at %v (%.0f u/s), unarmed\n", xBox.velocity, vlen(xBox.velocity)));
    }
    if(vlen(xBox.velocity) >= VR_GREN_FLYING)
    {
        VR_Grenade_Return(xBox, "thrown back"); // (thrown; dropped, it keeps its thrower's blast)
    }
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
    VR_GrenShot_Update(self); // (not in a hand or a palm)
    if(time >= self.vr_gren_fuse && VR_HandGrenade_Armed(self))
    {
        VR_Grenade_Explode();
        return;
    }
    if(self.vr_gren_trap != world && VR_Grenade_TrapTick())
    {
        self.vr_gren_lastvel = self.velocity;
        self.vr_gren_lastorg = self.origin;
        return; // (in an open palm)
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
// Hand grenades from the back pouch (vr_handgrenade; ROUND21.md, "Hand grenades from the back pouch")

float VR_HGREN_MAX_ROCKETS = 100; // rockets (or multi-rockets) a player carries (bound_other_ammo): one put back
                                  // beyond it doesn't fit
float VR_HGREN_SKIN_UNARMED = 1;  // progs/grenade.mdl's (and progs/mervup.mdl's) skin with the pin in: muted, its band
                                  // unlit (make_grenade_skins.py); the engine gives it no smoke trail (VR_GrenadeTrail)

// The ammo the hand grenade `g` came out of: its multi-rockets (a multi-grenade) or its rockets, of the player `p`.
float(entity p, entity g) VR_HandGrenade_Ammo =
{
    return g.vr_hgren_multi ? p.ammo_multi_rockets : p.ammo_rockets;
};

// The hand grenade `g` back in the pouch of `p`: its ammo one more.
void(entity p, entity g) VR_HandGrenade_AmmoBack =
{
    if(g.vr_hgren_multi)
    {
        p.ammo_multi_rockets = p.ammo_multi_rockets + 1;
    }
    else
    {
        p.ammo_rockets = p.ammo_rockets + 1;
    }
};

// Whether `xHand` of `p` holds its upper face button (B/Y) down: the pouch gives a multi-grenade then (if it has one).
float(entity p, float xHand) VR_HandGrenade_WantsMulti =
{
    return VRHasBit(p.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_SECONDARY : QVR_VRBITS0_OFFHAND_SECONDARY);
};

// Whether the grenade `g` goes off on its fuse: any live grenade but a hand grenade whose pin is still in.
float(entity g) VR_HandGrenade_Armed =
{
    return !g.vr_hgren || g.vr_hgren_armed;
};

// Whether `g` is a hand grenade with its pin in (held, or a dud lying about).
float(entity g) VR_HandGrenade_Unarmed =
{
    return g.vr_hgren && !g.vr_hgren_armed;
};

// Whether `xHand` of `p` is at the grenade pouch.
float(entity p, float xHand) VR_HandGrenade_AtPouch =
{
    if(!cvar_hget(cvarh_vr_handgrenade))
    {
        return FALSE;
    }
    return (xHand == cVR_MainHand ? p.mainhand_hotspot : p.offhand_hotspot) == QVR_HS_GRENADE_POUCH;
};

// A haptic in `xHand` of the player `p`.
void(entity p, float xHand, float xDuration, float xFrequency, float xAmplitude) VR_HandGrenade_Haptic =
{
    if(!p.ishuman)
    {
        return;
    }
    entity oldSelf = self;
    self = p;
    haptic(xHand, 0.0, xDuration, xFrequency, xAmplitude);
    self = oldSelf;
};

// The rockets of `p` changed: its ammo counters (the hands' screens, the HUD).
void(entity p) VR_HandGrenade_AmmoChanged =
{
    entity oldSelf = self;
    self = p;
    W_SetCurrentAmmo();
    self = oldSelf;
};

// Arms the hand grenade `g` (`xHand` the hand holding or letting go of it, `xHow` for the log): its fuse
// (vr_handgrenade_fuse) runs from now, and it is lit as a caught grenade is (the lever's clink and the fizz, sparks, the
// ticks faster and faster: VR_Grenade_Cue). The trigger's pin: a ring's ping first, and a burst of sparks.
void(entity g, float xHand, string xHow) VR_HandGrenade_Arm =
{
    float fuse = min(5, max(1, cvar_hget(cvarh_vr_handgrenade_fuse)));
    g.vr_hgren_armed = TRUE;
    g.skin = 0; // the live grenade's look: its red band, and its smoke trail
    g.vr_gren_fuse = time + fuse;
    g.vr_gren_set = TRUE; // (the pin set it: a catch doesn't, unless Fuse Resets Every Catch)
    g.vr_gren_lit = TRUE;
    g.vr_gren_ticknext = time + 0.15;
    g.vr_gren_sparknext = time + 0.06;
    if(cvar_hget(cvarh_vr_handgrenade_arm) != 1)
    {
        sound(g, CHAN_AUTO, "vr/grenade_pin.wav", 1, ATTN_NORM);
    }
    sound(g, CHAN_BODY, "vr/grenade_fuse.wav", 1, ATTN_NORM);
    particle2(g.origin + '0 0 2', '0 0 90', QVR_PARTICLE_PRESET_SPARKS, 6);
    if(g.carry_player != world)
    {
        VR_HandGrenade_Haptic(g.carry_player, xHand, 0.08, 180, 0.7);
    }
    dprint(sprintf("grenade: hand grenade armed (%s): goes off in %.2f s\n", xHow, fuse));
};

// Takes a grenade from the pouch into `xHand` of self (a player; the hand empty, at the pouch, gripping): the
// launcher's grenade, unarmed, held in the hand; a rocket leaves the ammo with it. With the hand's B/Y held and a
// multi-rocket or more: the multi-grenade launcher's, a multi-rocket leaving the ammo (no multi-rockets: the grenade).
// Nothing to take: a dull knock in the hand.
void(float xHand) VR_HandGrenade_Take =
{
    float wantsMulti = VR_HandGrenade_WantsMulti(self, xHand);
    float multi = wantsMulti && self.ammo_multi_rockets >= 1;
    if(!multi && self.ammo_rockets < 1)
    {
        VR_HandGrenade_Haptic(self, xHand, 0.12, 30, 0.8);
        dprint(wantsMulti ? "grenade: the pouch is empty (no multi-rockets, no rockets)\n" :
                            "grenade: the pouch is empty (no rockets)\n");
        return;
    }
    if(wantsMulti && !multi)
    {
        dprint("grenade: no multi-rockets: a grenade instead\n");
    }
    entity p = self;
    entity g = spawn();
    g.classname = multi ? "MultiGrenade" : "grenade";
    g.owner = p;
    g.movetype = MOVETYPE_BOUNCE;
    g.solid = SOLID_BBOX;
    setmodel(g, multi ? "progs/mervup.mdl" : "progs/grenade.mdl");
    setsize(g, '0 0 0', '0 0 0');
    setorigin(g, VRGetEntHandPos(p, xHand));
    g.angles = VRGetEntHandRot(p, xHand);
    if(multi)
    {
        g.touch = MultiGrenadeTouch; // (rogue's: a launcher's grenade on what it hits, its cluster on its fuse)
        VR_Grenade_Setup(g, MultiGrenadeExplode, 0);
        g.vr_hgren_multi = TRUE;
    }
    else
    {
        g.touch = GrenadeTouch;
        VR_Grenade_Setup(g, GrenadeExplode, 0);
    }
    g.vr_hgren = TRUE;
    g.vr_gren_fuse = 0; // (the pin: VR_HandGrenade_Arm)
    g.skin = VR_HGREN_SKIN_UNARMED; // muted, no smoke trail, until it is armed

    if(multi)
    {
        p.ammo_multi_rockets = p.ammo_multi_rockets - 1;
    }
    else
    {
        p.ammo_rockets = p.ammo_rockets - 1;
    }
    VR_HandGrenade_AmmoChanged(p);
    vr_carry_quiet = TRUE; // (the pouch's own sound)
    VR_Carry_Start(p, xHand, g);
    vr_carry_quiet = FALSE;
    VR_GrenShot_Make(g); // (shots meet it once it leaves the hand: a dud lying about too)
    // Its turn in the hand as it comes out (vr_grenade_pouch_hold_*), on top of its grip (vr_grip.cpp serverFromPouch).
    g.carry_offset = carrypouch(g, VRGetEntHandRot(p, xHand), g.carry_offset);
    sound(p, CHAN_AUTO, "vr/grenade_pouch.wav", 0.9, ATTN_NORM);
    VR_HandGrenade_Haptic(p, xHand, 0.1, 90, 0.8);
    dprint(sprintf("grenade: hand %s taken from the pouch by hand %g, %g %s left\n", multi ? "multi-grenade" : "grenade",
        xHand, VR_HandGrenade_Ammo(p, g), multi ? "multi-rockets" : "rockets"));
};

// `xHand` of self lets go of the hand grenade `g` (VR_Carry_HandFrame): unarmed at the pouch, it goes back in (the
// rocket back, or the multi-rocket, if it fits) and TRUE; else FALSE (let go of as any carried thing: VR_Carry_Release, then
// VR_Grenade_Released arms it or leaves it a dud).
float(float xHand, entity g) VR_HandGrenade_LetGo =
{
    if(!VR_HandGrenade_Unarmed(g) || !VR_HandGrenade_AtPouch(self, xHand))
    {
        return FALSE;
    }
    if(VR_HandGrenade_Ammo(self, g) + 1 > VR_HGREN_MAX_ROCKETS)
    {
        VR_HandGrenade_Haptic(self, xHand, 0.12, 30, 0.8);
        dprint("grenade: the pouch is full: the grenade drops\n");
        return FALSE;
    }
    VR_Carry_SetHeld(self, xHand, world);
    g.carry_player = world;
    VR_HandGrenade_AmmoBack(self, g);
    VR_HandGrenade_AmmoChanged(self);
    sound(self, CHAN_AUTO, "vr/grenade_pouch.wav", 0.6, ATTN_NORM);
    VR_HandGrenade_Haptic(self, xHand, 0.08, 90, 0.6);
    dprint(sprintf("grenade: hand %s put back in the pouch by hand %g, %g %s\n", g.vr_hgren_multi ? "multi-grenade" :
        "grenade", xHand, VR_HandGrenade_Ammo(self, g), g.vr_hgren_multi ? "multi-rockets" : "rockets"));
    remove(g);
    return TRUE;
};

// The trigger's press (not held) while `xHand` of self holds the hand grenade `g`: its pin pulled (vr_handgrenade_arm
// 0). TRUE if it is a hand grenade (the trigger is its own, not the carry's take).
float(float xHand, entity g, float xPressed) VR_HandGrenade_Trigger =
{
    if(!g.vr_hgren)
    {
        return FALSE;
    }
    if(xPressed && VR_HandGrenade_Unarmed(g) && cvar_hget(cvarh_vr_handgrenade_arm) != 1)
    {
        VR_HandGrenade_Arm(g, xHand, "the pin pulled");
    }
    return TRUE;
};

// A level's end (SetChangeParms; self is the player): an unarmed hand grenade in a hand goes back in the pouch, so that
// its rocket (or multi-rocket) goes on with the player (the grenade itself stays behind with the level).
void() VR_HandGrenade_LevelEnd =
{
    for(float h = 0; h < 2; h++)
    {
        float hand = h ? cVR_MainHand : cVR_OffHand;
        entity g = VR_Carry_Held(self, hand);
        if(g != world && VR_HandGrenade_Unarmed(g) && VR_HandGrenade_Ammo(self, g) < VR_HGREN_MAX_ROCKETS)
        {
            VR_HandGrenade_AmmoBack(self, g);
            VR_Carry_SetHeld(self, hand, world);
            g.carry_player = world;
            remove(g);
            dprint("grenade: an unarmed hand grenade back in the pouch at the level's end\n");
        }
    }
};

// Every player frame, for `xHand` of self (W_Frame, before the weapons' hands: DoHandImpl): at the grenade pouch, a
// tap as the hand arrives (with a grenade to take, or an unarmed one to put back), and a grip pressed there by an empty
// hand takes a grenade (VR_HandGrenade_Take; a multi-grenade with the hand's B/Y held).
void(float xHand) VR_HandGrenade_HandFrame =
{
    float bit = xHand == cVR_MainHand ? 2 : 1;
    float at = VR_HandGrenade_AtPouch(self, xHand);
    if(self.health <= 0)
    {
        at = FALSE;
    }
    float was = (self.vr_pouch_at & bit) != 0;
    self.vr_pouch_at = self.vr_pouch_at - (self.vr_pouch_at & bit);
    if(at)
    {
        self.vr_pouch_at = self.vr_pouch_at | bit;
    }
    if(!VR_HandGrabUtil_IsHandGrabbing(self, xHand))
    {
        self.vr_pouch_tried = self.vr_pouch_tried - (self.vr_pouch_tried & bit);
    }
    if(!at || !self.ishuman || (!deathmatch && time < readytime))
    {
        return;
    }
    float climbing = VRHasBit(self.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_CLIMBING :
                                                                    QVR_VRBITS0_OFFHAND_CLIMBING);
    float emptyHand = VRIsHandEmpty(self, xHand) &&
                      !VR_Melee_HoldsTorch(self, xHand) && !VR_Forcegrab_IsHandBusy(self, xHand) && !climbing;
    if(!was)
    {
        entity held = VR_Carry_Held(self, xHand);
        if(emptyHand && (self.ammo_rockets >= 1 || self.ammo_multi_rockets >= 1))
        {
            VR_HandGrenade_Haptic(self, xHand, 0.04, 160, 0.35); // (as a holster with a gun in it)
        }
        else if(held != world && VR_HandGrenade_Unarmed(held))
        {
            VR_HandGrenade_Haptic(self, xHand, 0.04, 160, 0.35); // (to put it back)
        }
    }
    if(emptyHand && VR_HandGrabUtil_StartedHandGrabbing(self, xHand) && !(self.vr_pouch_tried & bit))
    {
        self.vr_pouch_tried = self.vr_pouch_tried | bit;
        VR_HandGrenade_Take(xHand);
    }
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
