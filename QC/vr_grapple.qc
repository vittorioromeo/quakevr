// Grappling hook: Rogue's (Jan '97 by ZOID <zoid@threewave.com>, under contract to id Software for Rogue
// Entertainment), reworked for VR (docs/vr-port/ROUND21.md, "Grappling hook: rope, reel on demand, props and
// monsters"; "Grapple: unreel; rope drawn in one piece"; "Grappling hook: one persistent system").
//
// The hook is its own entity, and its gun is found anew every frame, wherever it is: in a hand (either, passed from one
// to the other), in a holster, or lying about as a thrown weapon (a physics prop). The gun knows its hook by a tag in its
// weapon flags (GH_TAG_UNIT and up: VR_Grapple_TagOf), which every move of a weapon carries (hand to hand, to a holster
// and back, dropped, thrown, picked up); the hook has the same tag (.gh_tag). A gun whose tag no hook has is ready to
// fire. So nothing is lost when the gun moves: the rope goes on from wherever the gun now is.
//
// The hook's states (.gh_state):
//
//   | State | What it is | Ends |
//   |---|---|---|
//   | GS_FLYING | fired: 1500 units/s for 1.8 s at most; the rope pays out behind it | it bites (GS_BITTEN); it misses, or the trigger is let go (GS_LOOSE) |
//   | GS_BITTEN | in what it bit (the world, a door, a monster, a player, a prop); the rope holds at its length | the trigger let go, what it bit gone (taken, eaten, dead, grabbed): GS_LOOSE; the quick release: GS_RETRACT |
//   | GS_LOOSE | a physics prop on the end of the rope (it can be carried) | reeled in to the gun (B/Y): back in the gun; the quick release: GS_RETRACT |
//   | GS_RETRACT | flying straight back to the gun (the quick release button on the gun, vr_grapple_quick_speed) | at the gun: back in the gun |
//
// The trigger fires a ready gun, and nothing else. Let go (it was held, the gun in the same hand), the hook comes off
// what it bit (or stops flying) after vr_grapple_drop_grace: a gun dropped or passed to the other hand within that time
// keeps its hook in. It comes loose (vr_grapple_trigger_release 0) or straight back (1). It stays loose until reeled in:
// nothing brings it back by itself (but for a player without tracked hands, and Rogue's grapple: it comes back at once).
//
// The rope is a constraint between the gun's end (the muzzle in a hand, the holster, the thrown gun's muzzle) and the
// hook's (where it bit, or the loose hook): never longer than its length. With the gun in a hand or a holster, what the
// hook bit decides who moves (the table below; B/Y reels only in a hand). With the gun lying about, the rope holds the gun
// and the hook's end together by their masses: it hangs from what the hook bit in the world, a monster drags it, it and
// a prop pull on each other; a loose hook and the gun alike.
//
// The rope then holds at the length it bit at, measured from your body (from the gun, for a prop): you swing on it
// and walk or fall closer freely, never farther. Nothing is pulled until the hand's upper face button (B on the main
// hand, Y on the off hand: the engine's QVR_VRBITS0_*HAND_SECONDARY) is held: the reel then winds the rope in for as
// long as it is held (taking any slack in at once), and let go, the rope keeps its new length. The buttons are
// symmetric: a hand whose own gun has no hook out drives the other hand's (both B and Y reel it, both A and X unreel it).
// What the reel pulls:
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
//   | nothing (the hook loose) | it, to the gun, where it goes back in | vr_grapple_prop_speed |
//
// A load that can't follow (a box behind a pillar, a monster against a wall) stalls the reel: the rope is never more
// than GH_STALL shorter than it is far. The rope holds what it pulls too: a prop or a monster that goes farther than the
// rope (pushed, walking away, you walking away) is pulled back to its length; a huge monster drags you.
//
// The unreel (docs/vr-port/ROUND21.md, "Grapple: unreel; rope drawn in one piece"): the hand's lower face button (A on
// the main hand, X on the off hand: QVR_VRBITS0_*HAND_PRIMARY) held in the air pays the rope out at
// vr_grapple_unreel_speed, up to the hook's reach (GH_MAX_ROPE): what pulls on it goes farther, no faster than that (you
// let yourself down from a ceiling; a monster walks away, a prop hanging at the gun is lowered), and let go, the rope
// keeps its new length. A reel pays out what pulls on it: a load that stays gets GH_PAYOUT_SLACK of slack, no more. On
// the ground (or swimming) the button is its key (A jumps) and pays nothing out (vr_grapple_unreel_airborne 0: anywhere),
// and a press that began there (a jump) pays nothing out until it is pressed again. B/Y held too: the reel wins.
// vr_grapple_rope 0: Rogue's grapple, pulling you in as soon as it bites, whatever it bit (as it always does for a player
// without tracked hands: a flat-screen player or a bot has no B/Y to reel with).
//
// The quick release: the button on top of the gun (the weapon button, as the rocket launcher's and the lightning gun's:
// the other hand's fingertip presses it, impulse 42/43) takes the hook off whatever it is in and brings it straight back.
//
// The hook goes (the gun back to ready) when its holder dies or is teleported, and when its gun is nowhere for
// GH_LOST_TIME (cycled away, taken into a backpack, gone with the level).

// prototypes for WEAPONS.QC functions
float() crandom;
void(vector org, vector vel, float damage) SpawnBlood;
float(entity e) VR_Thrown_Mass;

// ----------------------------------------------------------------------------
// The hook's state
// ----------------------------------------------------------------------------

float GS_FLYING  = 0; // fired, not bitten yet
float GS_BITTEN  = 1; // in what it bit
float GS_LOOSE   = 2; // a physics prop on the rope's end
float GS_RETRACT = 3; // flying straight back to the gun (the quick release)

float GH_FLYING  = 0; // (gh_kind) not bitten yet
float GH_ANCHOR  = 1; // what it bit stays: the reel pulls the player
float GH_PROP    = 2; // a prop (or the loose hook itself): the reel pulls it to the gun
float GH_MONSTER = 3; // a monster: the reel pulls it to the player

float GH_SMALL  = 1; // a monster's class (gh_class)
float GH_MEDIUM = 2;
float GH_HUGE   = 3;

float GW_NONE    = 0; // where the gun is (gg_where, .gh_where): nowhere
float GW_HAND    = 1; // in a player's hand
float GW_HOLSTER = 2; // in a player's holster
float GW_PROP    = 3; // lying about (a thrown weapon)

float GH_STALL      = 8;    // units the rope may be shorter than the load is far: the reel can't outrun its load
float GH_RAMP       = 0.15; // seconds the reel takes to reach its speed (it starts at a fifth)
float GH_BRAKE      = 2500; // units/s/s a reeled prop or monster slows at, arriving (it stops at the rope's shortest)
float GH_HOLD_SPEED = 400;  // units/s at most the rope pulls a load back to its length when it is farther (not reeling)
float GH_HOLD_SLACK = 8;    // units past the rope's length before it pulls a load back
float GH_PROP_GAP   = 12;   // units past half its size from the muzzle a reeled prop stops at
float GH_MAX_ROPE   = 2700; // units: the longest the unreel pays the rope out to (the hook's reach: 1500 u/s for 1.8 s)
float GH_PAYOUT_SLACK = 4;  // units the unreel pays out beyond the load's distance (a load that doesn't pull stays put)
float GH_TAG_UNIT   = 256;  // a gun's hook tag is its weapon flags' bits from 8 up (1 .. GH_TAG_MAX; 0: none)
float GH_TAG_MAX    = 65535;
float GH_HOOK_MASS  = 2;    // kg: the loose hook
float GH_HOOK_HALF  = 3;    // units: half the loose hook's box
float GH_HOME       = 8;    // units past the loose hook's shortest (from the muzzle) where the reel takes it back in
float GH_LOST_TIME  = 0.5;  // seconds a hook's gun may be nowhere (between two moves) before the hook goes
float GH_RETRACT_TIME = 3;  // seconds at most the quick release takes (then it is back, wherever it was)
float GH_YANK_NEAR  = 96;   // units from the gun within which a loose hook caught while reeled is yanked free,
float GH_YANK_TIME  = 0.4;  // after being stuck this long
vector GH_GUN_MUZZLE = '26 0 4'; // the grappling gun's muzzle on its model (a thrown gun's rope starts there)

.float gh_state;         // GS_*
.float gh_tag;           // its gun's tag (1 .. GH_TAG_MAX)
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
.float gh_unreel;        // unreeling since then (0: not)
.float gh_payout;        // units/s the rope pays out at now: ramping up while A/X unreels, braking to 0 after
.float gh_unreel_from;   // the rope's length when this unreel began (the log)
.float gh_unreel_held;   // the hand's A/X was held last frame
.float gh_unreel_armed;  // and that press began in the air (it may unreel)
.float gh_where;         // where its gun was last frame (GW_*), and which:
.entity gh_wplayer;      //   the player (GW_HAND, GW_HOLSTER)
.float gh_whand;         //   the hand (GW_HAND)
.float gh_wslot;         //   the holster (GW_HOLSTER: QVR_HS_*)
.entity gh_went;         //   the thrown gun (GW_PROP)
.float gh_lost_at;       // since then its gun is nowhere (0: found)
.float gh_served;        // the frame (time) it was serviced last (once a frame: VR_Grapple_Service)
.float gh_trig;          // the holding hand's trigger last frame (the gun in that hand)
.float gh_release_at;    // the trigger let go: the hook comes off then (vr_grapple_drop_grace; 0: not)
.float gh_retract_at;    // the quick release began then
.float gh_state_at;      // the state began then (the log)
.float gh_prev_d;        // a loose hook's distance from the gun last frame (reeled: is it stuck?)
.float gh_stall_at;      // since then it is stuck, reeled (0: not)
.float gh_rearm;         // on a player: the main hand's hook came back with the trigger held (VR_Grapple_MustRelease)
.float gh_rearm2;        // the off hand's

// Where the gun of the hook being serviced is this frame (VR_Grapple_FindGun).
float gg_where;          // GW_*
entity gg_player;        // GW_HAND, GW_HOLSTER: the player
float gg_hand;           // GW_HAND: the hand
float gg_slot;           // GW_HOLSTER: the holster
entity gg_ent;           // GW_PROP: the thrown gun
vector gg_at;            // the rope's end at the gun: the muzzle in a hand, the holster, the thrown gun's muzzle
float gg_moved;          // it is elsewhere than last frame

float gh_nexttag;        // the last tag given (VR_Grapple_NewTag)

float() VR_Grapple_Debug =
{
    return cvar_hget(cvarh_vr_grapple_debug);
};

float() VR_Grapple_RopeMode =
{
    return cvar_hget(cvarh_vr_grapple_rope);
};

// A haptic pulse in the hook's hand, at vr_grapple_haptics (self: the player; `xHand` -1: the gun in no hand, none).
void(float xHand, float xDuration, float xFrequency, float xAmplitude) VR_Grapple_Haptic =
{
    if(xHand < 0)
    {
        return;
    }
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
    if(e.carry_player != world || e.fg_state != 0 || e.classname == "hook") // (a loose hook: not bitten by another)
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

// Whether a thing on the rope's end can be moved by it now: not held in a hand, not flying to one (a force grab).
float(entity t) VR_Grapple_Movable =
{
    return t.carry_player == world && t.fg_state == 0;
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

// The rope's end at the gun's side: the gun for a prop (it comes to the gun) and for a gun lying about, else the body of
// the player holding the gun (the rope holds the player by it, whatever the arm does). (VR_Grapple_FindGun first.)
vector(entity h) VR_Grapple_RopeEnd =
{
    if(h.gh_kind == GH_PROP || gg_player == world)
    {
        return gg_at;
    }
    return gg_player.origin;
};

// ----------------------------------------------------------------------------
// The gun and its tag
// ----------------------------------------------------------------------------

// A gun's hook tag (0: none), from its weapon flags, and the flags with another tag.
float(float xFlags) VR_Grapple_TagOf =
{
    return (xFlags - (xFlags & (GH_TAG_UNIT - 1))) / GH_TAG_UNIT;
};

float(float xFlags, float xTag) VR_Grapple_WithTag =
{
    return (xFlags & (GH_TAG_UNIT - 1)) + xTag * GH_TAG_UNIT;
};

// A weapon's flags as a new level has them: a hook doesn't cross levels (DecodeLevelParms).
float(float xFlags) VR_Grapple_StripTag =
{
    return VR_Grapple_WithTag(xFlags, 0);
};

// The hook of the tag (world: none, the gun is ready).
entity(float xTag) VR_Grapple_HookOf =
{
    if(xTag <= 0)
    {
        return world;
    }
    entity h = find(world, classname, "hook");
    while(h)
    {
        if(h.gh_tag == xTag)
        {
            return h;
        }
        h = find(h, classname, "hook");
    }
    return world;
};

// A tag no hook has.
float() VR_Grapple_NewTag =
{
    float n = 0;
    while(n < 16)
    {
        gh_nexttag = gh_nexttag + 1;
        if(gh_nexttag > GH_TAG_MAX)
        {
            gh_nexttag = 1;
        }
        if(VR_Grapple_HookOf(gh_nexttag) == world)
        {
            return gh_nexttag;
        }
        n = n + 1;
    }
    return gh_nexttag;
};

// A holster's weapon and flags (the holster: QVR_HS_*), of any player.
float(entity p, float s) VR_Grapple_HolsterWeapon =
{
    if(s == QVR_HS_LEFT_SHOULDER_HOLSTER)  { return p.holsterweapon0; }
    if(s == QVR_HS_RIGHT_SHOULDER_HOLSTER) { return p.holsterweapon1; }
    if(s == QVR_HS_LEFT_HIP_HOLSTER)       { return p.holsterweapon2; }
    if(s == QVR_HS_RIGHT_HIP_HOLSTER)      { return p.holsterweapon3; }
    if(s == QVR_HS_LEFT_UPPER_HOLSTER)     { return p.holsterweapon4; }
    if(s == QVR_HS_RIGHT_UPPER_HOLSTER)    { return p.holsterweapon5; }
    return 0;
};

float(entity p, float s) VR_Grapple_HolsterFlags =
{
    if(s == QVR_HS_LEFT_SHOULDER_HOLSTER)  { return p.holsterweaponflags0; }
    if(s == QVR_HS_RIGHT_SHOULDER_HOLSTER) { return p.holsterweaponflags1; }
    if(s == QVR_HS_LEFT_HIP_HOLSTER)       { return p.holsterweaponflags2; }
    if(s == QVR_HS_RIGHT_HIP_HOLSTER)      { return p.holsterweaponflags3; }
    if(s == QVR_HS_LEFT_UPPER_HOLSTER)     { return p.holsterweaponflags4; }
    if(s == QVR_HS_RIGHT_UPPER_HOLSTER)    { return p.holsterweaponflags5; }
    return 0;
};

void(entity p, float s, float v) VR_Grapple_SetHolsterFlags =
{
    if(s == QVR_HS_LEFT_SHOULDER_HOLSTER)  { p.holsterweaponflags0 = v; }
    if(s == QVR_HS_RIGHT_SHOULDER_HOLSTER) { p.holsterweaponflags1 = v; }
    if(s == QVR_HS_LEFT_HIP_HOLSTER)       { p.holsterweaponflags2 = v; }
    if(s == QVR_HS_RIGHT_HIP_HOLSTER)      { p.holsterweaponflags3 = v; }
    if(s == QVR_HS_LEFT_UPPER_HOLSTER)     { p.holsterweaponflags4 = v; }
    if(s == QVR_HS_RIGHT_UPPER_HOLSTER)    { p.holsterweaponflags5 = v; }
};

// Where a holster is, near enough for the rope (the client alone knows the body's pose): the hips at the waist, the upper
// holsters on the chest, the shoulders' on the back, left or right of the player's facing.
vector(entity p, float s) VR_Grapple_HolsterPos =
{
    makevectors('0 1 0' * p.v_angle_y);
    float side = 1;
    if(s == QVR_HS_LEFT_SHOULDER_HOLSTER || s == QVR_HS_LEFT_HIP_HOLSTER || s == QVR_HS_LEFT_UPPER_HOLSTER)
    {
        side = -1;
    }
    if(s == QVR_HS_LEFT_HIP_HOLSTER || s == QVR_HS_RIGHT_HIP_HOLSTER)
    {
        return p.origin + v_right * side * 9 - '0 0 2';
    }
    if(s == QVR_HS_LEFT_UPPER_HOLSTER || s == QVR_HS_RIGHT_UPPER_HOLSTER)
    {
        return p.origin + v_right * side * 6 + v_forward * 4 + '0 0 12';
    }
    return p.origin + v_right * side * 7 - v_forward * 6 + '0 0 18';
};

float(entity p, float xHand, float xTag) VR_Grapple_HandHas =
{
    return p.classname == "player" && VRGetEntWeapon(p, xHand) == WID_GRAPPLE &&
           VR_Grapple_TagOf(VRGetEntWeaponFlags(p, xHand)) == xTag;
};

float(entity p, float s, float xTag) VR_Grapple_HolsterHas =
{
    return p.classname == "player" && VR_Grapple_HolsterWeapon(p, s) == WID_GRAPPLE &&
           VR_Grapple_TagOf(VR_Grapple_HolsterFlags(p, s)) == xTag;
};

float(entity e, float xTag) VR_Grapple_PropHas =
{
    // (A freed thrown weapon keeps its class, weapon and flags: its model is gone.)
    return e.classname == "thrown_weapon" && e.model != string_null && e.weapon == WID_GRAPPLE &&
           VR_Grapple_TagOf(e.weaponflags) == xTag;
};

// The holsters a gun can be in (QVR_HS_*), in turn.
float(float s) VR_Grapple_NextHolster =
{
    if(s < QVR_HS_LEFT_SHOULDER_HOLSTER) { return QVR_HS_LEFT_SHOULDER_HOLSTER; }
    if(s < QVR_HS_RIGHT_HIP_HOLSTER)     { return s + 1; }
    if(s < QVR_HS_LEFT_UPPER_HOLSTER)    { return QVR_HS_LEFT_UPPER_HOLSTER; }
    if(s < QVR_HS_RIGHT_UPPER_HOLSTER)   { return s + 1; }
    return 0;
};

void(entity p, float xHand) VR_Grapple_GunInHand =
{
    gg_where = GW_HAND;
    gg_player = p;
    gg_hand = xHand;
    entity oldSelf = self;
    self = p;
    makevectors(VRGetHandRot(xHand));
    gg_at = VRGetMuzzlePos(xHand) + v_forward * 1.5;
    self = oldSelf;
};

// Where the hook's gun is this frame (gg_*; FALSE: nowhere): where it was last frame, else looked for in every
// player's hands and holsters and among the thrown weapons. gg_moved: elsewhere than last frame.
float(entity h) VR_Grapple_FindGun =
{
    float tag = h.gh_tag;
    gg_where = GW_NONE;
    gg_player = world;
    gg_ent = world;
    gg_hand = 0;
    gg_slot = 0;
    gg_at = h.origin;
    if(h.gh_where == GW_HAND && VR_Grapple_HandHas(h.gh_wplayer, h.gh_whand, tag))
    {
        VR_Grapple_GunInHand(h.gh_wplayer, h.gh_whand);
    }
    else if(h.gh_where == GW_HOLSTER && VR_Grapple_HolsterHas(h.gh_wplayer, h.gh_wslot, tag))
    {
        gg_where = GW_HOLSTER;
        gg_player = h.gh_wplayer;
        gg_slot = h.gh_wslot;
    }
    else if(h.gh_where == GW_PROP && VR_Grapple_PropHas(h.gh_went, tag))
    {
        gg_where = GW_PROP;
        gg_ent = h.gh_went;
    }
    else
    {
        entity p = find(world, classname, "player");
        while(p && gg_where == GW_NONE)
        {
            if(VR_Grapple_HandHas(p, cVR_MainHand, tag))
            {
                VR_Grapple_GunInHand(p, cVR_MainHand);
            }
            else if(VR_Grapple_HandHas(p, cVR_OffHand, tag))
            {
                VR_Grapple_GunInHand(p, cVR_OffHand);
            }
            else
            {
                float s = VR_Grapple_NextHolster(0);
                while(s && gg_where == GW_NONE)
                {
                    if(VR_Grapple_HolsterHas(p, s, tag))
                    {
                        gg_where = GW_HOLSTER;
                        gg_player = p;
                        gg_slot = s;
                    }
                    s = VR_Grapple_NextHolster(s);
                }
            }
            p = find(p, classname, "player");
        }
        entity e = find(world, classname, "thrown_weapon");
        while(e && gg_where == GW_NONE)
        {
            if(VR_Grapple_PropHas(e, tag))
            {
                gg_where = GW_PROP;
                gg_ent = e;
            }
            e = find(e, classname, "thrown_weapon");
        }
    }
    if(gg_where == GW_HOLSTER)
    {
        gg_at = VR_Grapple_HolsterPos(gg_player, gg_slot);
    }
    else if(gg_where == GW_PROP)
    {
        gg_at = gg_ent.origin + modelpoint(gg_ent, gg_ent.angles, GH_GUN_MUZZLE);
    }

    gg_moved = gg_where != h.gh_where || gg_player != h.gh_wplayer || gg_ent != h.gh_went ||
               (gg_where == GW_HAND && gg_hand != h.gh_whand) || (gg_where == GW_HOLSTER && gg_slot != h.gh_wslot);
    if(gg_where == GW_NONE)
    {
        return FALSE;
    }
    if(gg_where == GW_NONE)
    {
        return FALSE;
    }
    return TRUE;
};

// The gun's place found (VR_Grapple_FindGun) kept as the hook's: next frame's is compared with it (gg_moved).
void(entity h) VR_Grapple_Commit =
{
    h.gh_where = gg_where;
    h.gh_wplayer = gg_player;
    h.gh_whand = gg_hand;
    h.gh_wslot = gg_slot;
    h.gh_went = gg_ent;
    h.gh_lost_at = 0;
};

// The hook of the gun in `xHand` of `self` (world: none; the gun ready, or no grappling gun there).
entity(float xHand) getHook =
{
    if(VRGetWeapon(xHand) != WID_GRAPPLE)
    {
        return world;
    }
    return VR_Grapple_HookOf(VR_Grapple_TagOf(VRGetWeaponFlags(xHand)));
};

// The gun in `xHand` has its hook out (in any state: it fires only once it is back).
float(float xHand) getHookOut =
{
    return getHook(xHand) != world;
};

// And the hook is in something.
float(float xHand) getOnHook =
{
    return getHook(xHand).gh_state == GS_BITTEN;
};

// Whether the hand's face buttons drive a hook (its own gun's, or, its own gun having none out, the other hand's): they
// don't cycle its weapon then (weapons.qc).
float(float xHand) VR_Grapple_ButtonsBusy =
{
    return getHookOut(xHand) || getHookOut(1 - xHand);
};

// ----------------------------------------------------------------------------
// The rope drawn
// ----------------------------------------------------------------------------

// The rope from the gun to the hook. In a hand: the player's beam, id 2 + hand (the client keeps its start at the gun as
// drawn, every frame); elsewhere (a holster, lying about): the hook's own beam, id 2. Slack (0 .. 1: the share of its
// length that hangs): ids 4 + hand + 2 k, k 1 .. 125 (the client draws it hanging: vr_client.cpp VR_RopeCurve).
void(entity h, vector hookPos, float xSlack) GrappleTrail =
{
    entity owner = h;
    float hand = 0;
    if(gg_where == GW_HAND)
    {
        owner = gg_player;
        hand = gg_hand;
    }
    float k = rint(min(1, max(0, xSlack)) * 125);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_BEAM);
    WriteEntity(MSG_BROADCAST, owner);
    if(k >= 1)
    {
        WriteByte(MSG_BROADCAST, 4 + hand + 2 * k);
    }
    else
    {
        WriteByte(MSG_BROADCAST, 2 + hand); /* disambiguator */
    }
    WriteVec3(MSG_BROADCAST, gg_at);
    WriteVec3(MSG_BROADCAST, hookPos);
};

// ----------------------------------------------------------------------------
// The hook's states
// ----------------------------------------------------------------------------

string(float s) VR_Grapple_StateName =
{
    if(s == GS_FLYING) { return "flying"; }
    if(s == GS_BITTEN) { return "in"; }
    if(s == GS_LOOSE)  { return "loose"; }
    return "coming back";
};

void(entity h, float xState, string xWhy) VR_Grapple_SetState =
{
    if(VR_Grapple_Debug())
    {
        dprint(sprintf("grapple: hook %g %s -> %s (%s) after %.2f s, rope %.0f, at %v\n", h.gh_tag,
            VR_Grapple_StateName(h.gh_state), VR_Grapple_StateName(xState), xWhy, time - h.gh_state_at, h.gh_len,
            h.origin));
    }
    h.gh_state = xState;
    h.gh_state_at = time;
    h.gh_reel = 0;
    h.gh_unreel = 0;
    h.gh_payout = 0;
    h.gh_taut = FALSE; // (a trigger let go in flight still takes the hook off once it bites: gh_release_at kept)
};

// A hand whose hook came back with the trigger held: it fires again only once the trigger is let go (Rogue's fired
// again at once, over and over).
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

// The hook back in its gun (reeled in, the quick release home, its holder dead or teleported, its gun gone): the gun
// ready again (its tag cleared), the hook gone.
void(entity h, string xWhy) VR_Grapple_Home =
{
    if(VR_Grapple_Debug())
    {
        dprint(sprintf("grapple: hook %g back in the gun (%s), was %s\n", h.gh_tag, xWhy, VR_Grapple_StateName(h.gh_state)));
    }
    if(VR_Grapple_FindGun(h))
    {
        if(gg_where == GW_HAND)
        {
            entity oldSelf = self;
            self = gg_player;
            VRSetEntWeaponFlags(self, gg_hand, VR_Grapple_StripTag(VRGetWeaponFlags(gg_hand)));
            VRSetWeaponFrame(gg_hand, 0);
            VRSetAttackFinished(gg_hand, time + 0.25);
            if(VRGetFireButtonPressed(gg_hand))
            {
                if(gg_hand == cVR_OffHand) self.gh_rearm2 = TRUE;
                else                       self.gh_rearm = TRUE;
            }
            sound(self, CHAN_AUTO, "weapons/tink1.wav", 0.6, ATTN_NORM);
            VR_Grapple_Haptic(gg_hand, 0.08, 120, 0.6);
            self = oldSelf;
        }
        else if(gg_where == GW_HOLSTER)
        {
            VR_Grapple_SetHolsterFlags(gg_player, gg_slot, VR_Grapple_StripTag(VR_Grapple_HolsterFlags(gg_player, gg_slot)));
        }
        else
        {
            gg_ent.weaponflags = VR_Grapple_StripTag(gg_ent.weaponflags);
            gg_ent.frame = 0;
            sound(gg_ent, CHAN_AUTO, "weapons/tink1.wav", 0.6, ATTN_NORM);
        }
    }
    VR_Carry_Forget(h);
    h.gh_tag = 0;
    remove(h);
};

// The hook comes off what it bit (or stops flying) and is a physics prop on the rope's end: it falls, it can be carried,
// the reel brings it in. Where it was, going on as what it was in went (a thrown weapon, a monster), a flying one slowed
// by the rope stopping it.
void(entity h, string xWhy) VR_Grapple_Loosen =
{
    vector vel = h.velocity * 0.25;
    if(h.gh_state == GS_BITTEN)
    {
        vel = '0 0 0';
        if(h.enemy != world && h.enemy.model != string_null)
        {
            vel = h.enemy.velocity;
        }
    }
    VR_Grapple_SetState(h, GS_LOOSE, xWhy);
    h.enemy = h;
    h.gh_kind = GH_PROP;
    h.gh_ofs = '0 0 0';
    h.gh_class = 0;
    h.gh_mass = GH_HOOK_MASS;
    h.gh_speed = cvar_hget(cvarh_vr_grapple_prop_speed);
    h.movetype = MOVETYPE_TOSS;
    h.solid = SOLID_NOT_BUT_TOUCHABLE;
    h.touch = SUB_Null;
    h.think = SUB_Null;
    h.nextthink = 0;
    h.handtouch = SUB_Null; // nothing to take: carried only (VR_Carry_Setup keeps it as carry_use)
    VR_Carry_Setup(h);      // a rigid body (Box3D) a hand can carry
    h.vr_spin = '0 0 0';
    h.frame = 1;
    setsize(h, '-1 -1 -1' * GH_HOOK_HALF, '1 1 1' * GH_HOOK_HALF);
    setorigin(h, h.origin);
    h.velocity = vel;
    h.avelocity = '0 0 0';
    h.flags = h.flags - (h.flags & FL_ONGROUND);
    if(VR_Grapple_FindGun(h))
    {
        h.gh_len = min(GH_MAX_ROPE, max(h.gh_len, vlen(h.origin - gg_at)));
    }
};

// The quick release: the hook comes off whatever it is in (or lets go of the hand carrying it) and flies straight back
// to its gun at vr_grapple_quick_speed, through anything.
void(entity h, string xWhy) VR_Grapple_Retract =
{
    if(h.gh_state == GS_RETRACT)
    {
        return;
    }
    VR_Carry_Forget(h);
    VR_Grapple_SetState(h, GS_RETRACT, xWhy);
    h.gh_retract_at = time;
    h.enemy = world;
    h.movetype = MOVETYPE_NONE;
    h.solid = SOLID_NOT;
    h.vr_rigid = 0;
    h.touch = SUB_Null;
    h.handtouch = SUB_Null;
    h.vr_wpntouch = SUB_Null;
    h.think = SUB_Null;
    h.nextthink = 0;
    h.velocity = '0 0 0';
    h.avelocity = '0 0 0';
    h.frame = 1;
    sound(h, CHAN_WEAPON, "weapons/chain1.wav", 0.7, ATTN_NORM);
};

// The hook comes off by itself (it missed, what it bit went, the trigger let go): loose on its rope, or, for a player
// who has no reel (no tracked hands) and with Rogue's grapple, back at once (as the quick release). `xTrigger`: the
// trigger let go (vr_grapple_trigger_release 1: back at once).
void(entity h, string xWhy, float xTrigger) VR_Grapple_Free =
{
    float back = FALSE;
    if(!VR_Grapple_RopeMode())
    {
        back = TRUE;
    }
    else if(VR_Grapple_FindGun(h) && gg_player != world && !VRHandsTracked(gg_player))
    {
        back = TRUE;
    }
    else if(xTrigger && cvar_hget(cvarh_vr_grapple_trigger_release))
    {
        back = TRUE;
    }
    if(back)
    {
        VR_Grapple_Retract(h, xWhy);
    }
    else
    {
        VR_Grapple_Loosen(h, xWhy);
    }
};

// The quick release button on the gun in `xHand` of `self` (impulse 42/43: the other hand's fingertip on it).
void(float xHand) VR_Grapple_QuickRelease =
{
    entity h = getHook(xHand);
    VR_Grapple_Haptic(xHand, 0.05, 160, 0.5);
    if(h == world)
    {
        sound(self, CHAN_AUTO, "weapons/tink1.wav", 0.3, ATTN_NORM); // (nothing out: a click)
        if(VR_Grapple_Debug())
        {
            dprint(sprintf("grapple: hand %g quick release: no hook out\n", xHand));
        }
        return;
    }
    VR_Grapple_Retract(h, "the quick release");
};

// ----------------------------------------------------------------------------
// Biting
// ----------------------------------------------------------------------------

// A flying hook's time out (1.8 s: its reach): it missed, and comes loose where it is.
void() VR_Grapple_FlightOver =
{
    if(self.gh_state == GS_FLYING)
    {
        VR_Grapple_Free(self, "missed", FALSE);
    }
};

// The hook `h` bites `t` (world: the world) at `xAt`: what it is decides what the reel does (GH_*), and the rope is as
// long as it is now (from wherever its gun is).
void(entity h, entity t, vector xAt) VR_Grapple_Bite =
{
    vector flight = normalize(h.velocity);
    VR_Grapple_SetState(h, GS_BITTEN, "bit");
    h.think = SUB_Null;
    h.nextthink = 0;

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

    VR_Grapple_FindGun(h);
    h.gh_len = min(GH_MAX_ROPE, vlen(VR_Grapple_HookPoint(h) - VR_Grapple_RopeEnd(h)));
    float xHand = -1;
    entity oldSelf = self;
    if(gg_where == GW_HAND)
    {
        xHand = gg_hand;
        self = gg_player;
    }
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
        dprint(sprintf("grapple: hook %g (hand %g) bit %s (%.1f kg) at %v, ", h.gh_tag, xHand, what, h.gh_mass, xAt));
        dprint(sprintf("the gun at %v: %s; rope %.0f\n", gg_at, kind, h.gh_len));
    }
    self = oldSelf;
};

// The flying hook touches something (self: the hook): it bites it (it glances off a teammate: loose).
void() GrappleAnchor =
{
    if(self.gh_state != GS_FLYING)
    {
        return;
    }
    if(other == self.owner || other.classname == "hook") // don't hook the guy that fired it (nor another hook)
    {
        return;
    }
    VR_Grapple_FindGun(self);
    if(other == gg_player && gg_player != world) // nor who holds its gun now
    {
        return;
    }
    entity holder = self;
    if(gg_player != world)
    {
        holder = gg_player;
    }

    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        // The sky: nothing to bite; it comes loose, back out of it.
        setorigin(self, self.origin - normalize(self.velocity) * 8);
        VR_Grapple_Free(self, "the sky", FALSE);
        return;
    }

    if(other.classname == "player")
    {
        // glance off of teammates
        if(other.steam == self.owner.steam)
        {
            VR_Grapple_Free(self, "glanced off a teammate", FALSE); // PGM - fix drift after teammate hit  01/20/97
            return;
        }

        sound(self, CHAN_WEAPON, "player/axhit1.wav", 1, ATTN_NORM);
        if(T_Damage(other, self, self.owner, 10))
        {
            VR_QuadSound(self.owner, "the hook bit");
        }
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

    sound(holder, CHAN_WEAPON, "weapons/tink1.wav", 1, ATTN_NORM);
    particle2(self.origin, '0 0 0', QVR_PARTICLE_PRESET_BULLETPUFF, 48);

    if(other != world && other.health <= 0 && (other.flags & FL_MONSTER))
    {
        VR_Grapple_Free(self, "what it hit is dead", FALSE);
        return;
    }

    VR_Grapple_Bite(self, other, self.origin);
};

// The flying hook `h` meets a prop on its way this frame (missiles pass through them): the first one before whatever it
// hits (a wall, a monster: its touch), padded a little; never its own gun. TRUE when it bit one. (VR_Grapple_FindGun
// first.)
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
        if(e != h && e != gg_ent && e != gg_player && VR_Grapple_IsProp(e))
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
    if(gg_player != world)
    {
        sound(gg_player, CHAN_WEAPON, "weapons/tink1.wav", 1, ATTN_NORM);
    }
    particle2(at, '0 0 0', QVR_PARTICLE_PRESET_BULLETPUFF, 16);
    VR_Grapple_Bite(h, best, at);
    return TRUE;
};

// ----------------------------------------------------------------------------
// Firing
// ----------------------------------------------------------------------------

// The trigger on a ready gun in `xHand` (self: the player): the hook flies, and the gun takes a new tag, the hook's.
void (float xHand) W_FireGrapple =
{
    entity newmis;

    if(getHookOut(xHand)) // (its hook out: it fires only once the hook is back)
        return;

    VRGunHaptic(xHand, 0.55, 75, 1.0);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 8);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);

    self.punchangle_x = -2; // bump him

    // chain out sound(loops)
    sound(self, CHAN_WEAPON, "weapons/chain1.wav", 1, ATTN_NORM);

    float tag = VR_Grapple_NewTag();
    VRSetEntWeaponFlags(self, xHand, VR_Grapple_WithTag(VRGetWeaponFlags(xHand), tag));

    newmis = spawn();
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid = SOLID_BBOX;
    newmis.owner = self;        // newmis belongs to me
    newmis.classname = "hook";
    newmis.vr_hitclass = HITCLASS_GRAPPLE; // hooks only on a monster's model (vr_hit_precise; vr_hit_tolerance_grapple)
    newmis.gh_tag = tag;
    newmis.gh_state = GS_FLYING;
    newmis.gh_state_at = time;
    newmis.gh_kind = GH_FLYING;
    newmis.gh_where = GW_HAND;
    newmis.gh_wplayer = self;
    newmis.gh_whand = xHand;
    newmis.gh_trig = VRGetFireButtonPressed(xHand);
    newmis.gh_unreel_held = VRGetEntPrimaryPressed(self, xHand); // (held already: not a press for the unreel)

    makevectors(VRGetWeaponFireRot(xHand));
    newmis.velocity = v_forward * 1500;
    newmis.velocity += vrProjectileVelocity(xHand, 0.6);
//    newmis.avelocity = '0 0 -500';
    newmis.angles = vectoangles(v_forward);

    newmis.touch = GrappleAnchor;
    newmis.think = VR_Grapple_FlightOver;
    // grapple only lives for two seconds, this gives max range on it
    newmis.nextthink = time + 1.8;
    newmis.frame = 1; // hook spread

    setmodel(newmis,"progs/hook.mdl");
    setorigin(newmis, VRGetMuzzlePos(xHand) + v_forward * 6);
    setsize(newmis, '0 0 0' , '0 0 0 ');

    if(VR_Grapple_Debug())
    {
        dprint(sprintf("grapple: hand %g fires hook %g\n", xHand, tag));
    }
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
        VR_Grapple_Taut(h, xHand, -along + min(0, xReel)); // (paying out: how much faster than the rope runs)
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
        // (Carried, or flying to a hand: still there; the rope then pulls on the other end.)
        return t.solid == SOLID_NOT && VR_Grapple_Movable(t);
    }
    return FALSE;
};

// Rogue's grapple (vr_grapple_rope 0): pulls the player in as soon as it bites, whatever it bit.
void(entity h, float xHand) VR_Grapple_AutoPull =
{
    vector vel = VR_Grapple_HookPoint(h) - gg_at;
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

// ----------------------------------------------------------------------------
// The unreel
// ----------------------------------------------------------------------------

// Whether the hook's A/X unreels this frame (self: the player; `xHeld`: the hook's hand's A/X or the other hand's that
// drives it, VR_Grapple_UnreelHeld; `xReeling`: its B/Y reels, which wins): held, in
// the air (not on the ground nor swimming, where it is the button's key: A jumps) unless vr_grapple_unreel_airborne is
// 0, and pressed there (a press that jumped does not unreel once in the air: let go and press again).
float(entity h, float xHand, float xHeld, float xReeling) VR_Grapple_UnreelButton =
{
    float held = xHeld;
    float grounded = FALSE;
    if(cvar_hget(cvarh_vr_grapple_unreel_airborne))
    {
        if(self.flags & FL_ONGROUND)
        {
            grounded = TRUE;
        }
        else if(self.waterlevel >= 2)
        {
            grounded = TRUE; // (A swims up)
        }
    }
    if(!held)
    {
        h.gh_unreel_armed = FALSE;
    }
    else if(!h.gh_unreel_held)
    {
        h.gh_unreel_armed = FALSE;
        if(!grounded)
        {
            h.gh_unreel_armed = TRUE;
        }
        if(VR_Grapple_Debug() && grounded)
        {
            dprint(sprintf("grapple: hand %g unreel button on the ground: its key (jump), no unreel\n", xHand));
        }
    }
    h.gh_unreel_held = held;
    if(!held || !h.gh_unreel_armed || grounded || xReeling || cvar_hget(cvarh_vr_grapple_unreel_speed) <= 0)
    {
        return FALSE;
    }
    return TRUE;
};

// The unreel's speed this frame (h.gh_payout): held (`xUnreeling`), it starts at a fifth of vr_grapple_unreel_speed
// and reaches it in GH_RAMP, as the reel; let go, it brakes to a stop at GH_BRAKE (from 300 u/s: 0.12 s, 18 units) rather
// than stopping what it lets down dead; the reel (B/Y) stops it at once.
void(entity h, float xUnreeling, float xReeling) VR_Grapple_Payout =
{
    float full = max(0, cvar_hget(cvarh_vr_grapple_unreel_speed));
    if(xReeling)
    {
        h.gh_payout = 0;
    }
    else if(xUnreeling)
    {
        h.gh_payout = max(0.2 * full, min(full, h.gh_payout + 0.8 * full / GH_RAMP * frametime));
    }
    else
    {
        h.gh_payout = max(0, h.gh_payout - GH_BRAKE * frametime);
    }
};

// The rope paid out this frame: longer at the unreel's speed (h.gh_payout), up to GH_MAX_ROPE, and no more than
// GH_PAYOUT_SLACK longer than the load is far (what pulls on it takes it out; a load that stays gets a little slack).
// Its clicks and buzz while A/X is held (`xUnreeling`) and it pays out; the log four times a second.
void(entity h, float xHand, float xUnreeling) VR_Grapple_Unwind =
{
    float d = vlen(VR_Grapple_HookPoint(h) - VR_Grapple_RopeEnd(h));
    float speed = h.gh_payout;
    float was = h.gh_len;
    float len = min(h.gh_len + speed * frametime, max(h.gh_len, d + GH_PAYOUT_SLACK));
    h.gh_len = max(h.gh_len, min(len, GH_MAX_ROPE));
    float rate = (h.gh_len - was) / max(0.001, frametime); // paid out now (units/s)

    // The freewheel's ticks (vr/grapple_unreel.wav: the reel's ratchet run backwards, lighter and quicker) while the
    // rope runs out; a fine buzz in the hand, stronger with a heavier load. Nothing while it doesn't turn.
    float moving = FALSE;
    if(xUnreeling && rate > 0.25 * speed && rate > 10)
    {
        moving = TRUE;
    }
    float load = 0.6; // you
    if(h.gh_kind == GH_PROP)
    {
        load = 0.2 + 0.8 * min(1, h.gh_mass / max(1, cvar_hget(cvarh_vr_grapple_prop_anchor)));
    }
    else if(h.gh_kind == GH_MONSTER)
    {
        load = 0.45;
        if(h.gh_class == GH_MEDIUM)
        {
            load = 0.85;
        }
    }
    if(time >= h.gh_sound_at)
    {
        if(moving)
        {
            sound(self, CHAN_AUTO, "vr/grapple_unreel.wav", 0.5, ATTN_NORM);
        }
        h.gh_sound_at = time + 0.25;
    }
    if(time >= h.gh_haptic_at)
    {
        if(moving)
        {
            VR_Grapple_Haptic(xHand, 0.025, 200 + 60 * (1 - load), 0.08 + 0.25 * load);
        }
        h.gh_haptic_at = time + 0.035;
    }
    if(VR_Grapple_Debug() && (time >= h.gh_log_at || (!xUnreeling && h.gh_payout <= 0)))
    {
        h.gh_log_at = time + 0.25;
        dprint(sprintf("grapple: hand %g unreels: rope %.1f, %.1f away, paying out %.0f u/s (unreel %.0f), on ground %g, "
            "at %v\n", xHand, h.gh_len, d, rate, speed, self.flags & FL_ONGROUND, self.origin));
    }
};

// The hook of a prop: it comes to the gun while reeling (slowing to stop at the rope's shortest, where it hangs to be
// taken), and the rope holds it at its length.
// A plain pickup (VR_Grapple_IsPlainPickup) reeled in: flown to the gun (it has no body: its velocity, set), still
// when the reel stops; at the gun, you take it (its hand touch, as if you had walked into it).
void(entity h, float xHand, float xReeling) VR_Grapple_ServicePlain =
{
    entity t = h.enemy;
    vector at = VR_Grapple_HookPoint(h);
    vector toGun = gg_at - at;
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
    if(!VR_Grapple_Movable(t))
    {
        // Carried (by a hand), or flying to one: the rope pays out.
        h.gh_len = min(GH_MAX_ROPE, max(h.gh_len, vlen(VR_Grapple_HookPoint(h) - gg_at)));
        return;
    }
    if(t.movetype == MOVETYPE_FLY && VR_Grapple_IsPlainPickup(t))
    {
        VR_Grapple_ServicePlain(h, xHand, xReeling);
        return;
    }
    vector at = VR_Grapple_HookPoint(h);
    vector gun = gg_at;
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
    // pull's push: Box3D takes a changed .velocity over what the body has.) Not while the unreel pays out: it is let
    // down, falling as the rope runs out.
    else if(h.gh_len <= shortest + 1 && d <= h.gh_len + GH_HOLD_SLACK && h.gh_payout <= 0)
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
    else if(h.gh_payout > 0)
    {
        // Paying out: you go away from the hook (let down, swinging out) no faster than the rope comes off the drum.
        speed = -h.gh_payout;
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

// ----------------------------------------------------------------------------
// The rope between two things (the gun lying about)
// ----------------------------------------------------------------------------

// `t` pulled at its point `xAt` so that the point goes at least `xSpeed` along `xDir` (a unit vector): Box3D's push (a
// gun pulled by its muzzle swings round), or, not a body (or the speed not a pull), its velocity.
void(entity t, vector xAt, vector xDir, float xSpeed) VR_Grapple_PullAlong =
{
    float along = t.velocity * xDir;
    if(along >= xSpeed)
    {
        return;
    }
    if(xSpeed > 0.5 && physicspush(t, xAt, xDir * xSpeed))
    {
        return;
    }
    t.velocity = t.velocity + xDir * (xSpeed - along);
    t.flags = t.flags - (t.flags & FL_ONGROUND);
};

// The rope between `a` (its point `xA`) and `b` (`xB`) holds them at its length: farther apart, they are pulled back
// together, each by its share (`xShareA`, `xShareB`: the other's share of their masses; 0: it doesn't move). Nothing
// can move: the rope pays out (a hand carrying one end, and the other flying to a hand).
void(entity h, entity a, vector xA, float xShareA, entity b, vector xB, float xShareB) VR_Grapple_HoldTwo =
{
    vector ab = xB - xA;
    float d = vlen(ab);
    if(d < 0.5)
    {
        return;
    }
    if(xShareA <= 0 && xShareB <= 0)
    {
        h.gh_len = min(GH_MAX_ROPE, max(h.gh_len, d));
        return;
    }
    vector n = ab * (1 / d);
    float over = d - h.gh_len - GH_HOLD_SLACK;
    if(over <= 0)
    {
        if(d < h.gh_len)
        {
            h.gh_taut = FALSE;
        }
        return;
    }
    float closing = a.velocity * n - b.velocity * n;
    if(!h.gh_taut && closing < -120)
    {
        sound(h, CHAN_AUTO, "vr/grapple_taut.wav", 0.35 + 0.65 * min(1, (-closing - 120) / 500), ATTN_NORM);
    }
    h.gh_taut = TRUE;
    float want = min(GH_HOLD_SPEED, over * 8);
    if(closing >= want)
    {
        return;
    }
    float deficit = want - closing;
    if(xShareA > 0)
    {
        VR_Grapple_PullAlong(a, xA, n, a.velocity * n + deficit * xShareA);
    }
    if(xShareB > 0)
    {
        VR_Grapple_PullAlong(b, xB, '0 0 0' - n, b.velocity * ('0 0 0' - n) + deficit * xShareB);
    }
};

// The hook's gun lying about (self: the hook): the rope holds the gun and the hook's end together. The gun hangs from
// what the hook is in when that stays (the world, a door, a heavy prop, a player, a huge monster), is dragged by a
// monster, and it and a prop (or the loose hook) pull on each other by their masses.
void(entity h) VR_Grapple_ServiceLying =
{
    entity g = gg_ent;
    g.frame = 2; // (the hook out of it)
    float gm = max(0.1, VR_Thrown_Mass(g));
    float gShare = 1;
    if(!VR_Grapple_Movable(g))
    {
        gShare = 0;
    }
    entity t = h.enemy;
    vector at = h.origin;
    if(h.gh_state == GS_BITTEN)
    {
        at = VR_Grapple_HookPoint(h);
        setorigin(h, at);
    }
    if(gg_moved)
    {
        h.gh_len = min(GH_MAX_ROPE, max(h.gh_len, vlen(at - gg_at)));
    }
    if(h.gh_state == GS_FLYING)
    {
        h.gh_len = min(GH_MAX_ROPE, vlen(h.origin - gg_at)); // (paying out behind it)
        GrappleTrail(h, h.origin, 0);
        return;
    }
    if(h.gh_state == GS_BITTEN && h.gh_kind == GH_PROP && VR_Grapple_Movable(t))
    {
        float tm = max(0.05, h.gh_mass);
        VR_Grapple_HoldTwo(h, g, gg_at, gShare * tm / (gm + tm), t, at, gm / (gm + tm));
    }
    else if(h.gh_state == GS_LOOSE && VR_Grapple_Movable(h))
    {
        VR_Grapple_HoldTwo(h, g, gg_at, gShare * GH_HOOK_MASS / (gm + GH_HOOK_MASS), h, at, gm / (gm + GH_HOOK_MASS));
    }
    else
    {
        // What stays (or a monster, or a hand carrying the loose hook): the gun alone moves.
        VR_Grapple_HoldTwo(h, g, gg_at, gShare, t, at, 0);
    }
    float slack = 0;
    if(h.gh_len > 1)
    {
        slack = max(0, h.gh_len - vlen(at - gg_at)) / h.gh_len;
    }
    GrappleTrail(h, at, slack);
    if(VR_Grapple_Debug() >= 2 && time >= h.gh_log_at)
    {
        h.gh_log_at = time + 0.25;
        dprint(sprintf("grapple: hook %g (%s) holds the gun lying about: rope %.0f, %.0f away, the gun at %v moving %v\n",
            h.gh_tag, VR_Grapple_StateName(h.gh_state), h.gh_len, vlen(at - gg_at), g.origin, g.velocity));
    }
};

// ----------------------------------------------------------------------------
// The quick release, flying back
// ----------------------------------------------------------------------------

void(entity h) VR_Grapple_ServiceRetract =
{
    vector to = gg_at - h.origin;
    float d = vlen(to);
    float step = max(100, cvar_hget(cvarh_vr_grapple_quick_speed)) * frametime;
    if(d <= step + 4 || time - h.gh_retract_at > GH_RETRACT_TIME)
    {
        VR_Grapple_Home(h, "the quick release");
        return;
    }
    vector dir = to * (1 / d);
    setorigin(h, h.origin + dir * step);
    h.angles = vectoangles('0 0 0' - dir);
    h.gh_len = d - step;
    GrappleTrail(h, h.origin, 0);
};

// ----------------------------------------------------------------------------
// The reel and the unreel's buttons
// ----------------------------------------------------------------------------

// The hand whose face buttons drive `xHand`'s hook too (-1: none): the other hand, when its own gun has no hook out.
float(float xHand) VR_Grapple_OtherDriver =
{
    if(getHookOut(1 - xHand))
    {
        return -1;
    }
    return 1 - xHand;
};

// Whether the reel's button is held for the hook in `xHand` (B/Y on that hand or the other that drives it).
float(float xHand) VR_Grapple_ReelHeld =
{
    if(VRGetEntSecondaryPressed(self, xHand))
    {
        return TRUE;
    }
    float o = VR_Grapple_OtherDriver(xHand);
    return o >= 0 && VRGetEntSecondaryPressed(self, o);
};

float(float xHand) VR_Grapple_UnreelHeld =
{
    if(VRGetEntPrimaryPressed(self, xHand))
    {
        return TRUE;
    }
    float o = VR_Grapple_OtherDriver(xHand);
    return o >= 0 && VRGetEntPrimaryPressed(self, o);
};

// The reel and the unreel this frame for the hook `h` in `xHand` (self: the player): the reel's state (h.gh_reel), the
// unreel's (h.gh_payout: the rope paid out), TRUE while the reel is held.
float(entity h, float xHand) VR_Grapple_Buttons =
{
    float reeling = VR_Grapple_ReelHeld(xHand);
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
            dprint(sprintf("grapple: hand %g reel on, rope %.0f (hook %s)\n", xHand, h.gh_len,
                VR_Grapple_StateName(h.gh_state)));
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

    float unreeling = VR_Grapple_UnreelButton(h, xHand, VR_Grapple_UnreelHeld(xHand), reeling);
    VR_Grapple_Payout(h, unreeling, reeling);
    if(unreeling && !h.gh_unreel)
    {
        h.gh_unreel = time;
        h.gh_unreel_from = h.gh_len;
        h.gh_sound_at = time;
        h.gh_haptic_at = time;
        h.gh_log_at = time;
        if(VR_Grapple_Debug())
        {
            dprint(sprintf("grapple: hand %g unreel on, rope %.0f\n", xHand, h.gh_len));
        }
    }
    else if(!unreeling && h.gh_unreel && h.gh_payout <= 0)
    {
        if(VR_Grapple_Debug())
        {
            dprint(sprintf("grapple: hand %g unreel off (braked) after %.2f s: rope %.0f -> %.0f\n", xHand,
                time - h.gh_unreel, h.gh_unreel_from, h.gh_len));
        }
        h.gh_unreel = 0;
    }
    if(h.gh_payout > 0)
    {
        VR_Grapple_Unwind(h, xHand, unreeling);
    }
    return reeling;
};

// ----------------------------------------------------------------------------
// The gun held (in a hand or a holster)
// ----------------------------------------------------------------------------

// The trigger of the hand holding the gun (self: the player): let go (held last frame, the gun in the same hand), the
// hook comes off after vr_grapple_drop_grace, unless the trigger is pulled again or the gun leaves the hand by then (a
// drop, a pass to the other hand, a holster: the hook stays in). TRUE: it came off.
float(entity h, float xHand) VR_Grapple_Trigger =
{
    float trig = VRGetFireButtonPressed(xHand);
    if(gg_moved)
    {
        h.gh_trig = trig;
        h.gh_release_at = 0;
        return FALSE;
    }
    if(h.gh_state != GS_FLYING && h.gh_state != GS_BITTEN)
    {
        h.gh_trig = trig;
        h.gh_release_at = 0;
        return FALSE;
    }
    if(trig)
    {
        h.gh_release_at = 0;
    }
    else if(h.gh_trig)
    {
        h.gh_release_at = time + max(0, cvar_hget(cvarh_vr_grapple_drop_grace));
        if(VR_Grapple_Debug())
        {
            dprint(sprintf("grapple: hook %g: the trigger let go (%s)\n", h.gh_tag, VR_Grapple_StateName(h.gh_state)));
        }
    }
    h.gh_trig = trig;
    // Letting go of the gun, not of the hook: the hand's grip let go too (the gun is dropped or handed over as soon as
    // the other hand lets go of it: vr_weapon_grip_mode 0), or the other hand, empty, grips it (a pass, or both hands
    // on it: the gun goes to it when the hands part).
    if(h.gh_release_at)
    {
        float o = 1 - xHand;
        if((cvar_hget(cvarh_vr_weapon_grip_mode) != 1 && !VRIsEntHandGrabbingBit(self, xHand)) ||
           (VRIsEntHandGrabbingBit(self, o) && VRIsHandEmpty(self, o)))
        {
            if(VR_Grapple_Debug())
            {
                dprint(sprintf("grapple: hook %g stays in: the trigger let go with the gun (a drop or a pass)\n", h.gh_tag));
            }
            h.gh_release_at = 0;
        }
    }
    if(h.gh_release_at && VR_Grapple_Debug() >= 3)
    {
        dprint(sprintf("grapple: hook %g release pending: %.3f, now %.3f\n", h.gh_tag, h.gh_release_at, time));
    }
    if(h.gh_release_at && time >= h.gh_release_at)
    {
        h.gh_release_at = 0;
        VR_Grapple_Free(h, "the trigger let go", TRUE);
        return TRUE;
    }
    return FALSE;
};

// The hook's gun in `xHand` of `self` (-1: in a holster: no buttons, no trigger, no haptics).
void(entity h, float xHand) VR_Grapple_ServiceHeld =
{
    if(xHand >= 0 && VR_Grapple_Trigger(h, xHand))
    {
        if(!VR_Grapple_FindGun(h))
        {
            return;
        }
    }
    if(h.gh_state == GS_RETRACT)
    {
        VR_Grapple_ServiceRetract(h);
        return;
    }

    vector hookfwd = '0 0 0';
    if(h.gh_state == GS_FLYING)
    {
        h.gh_len = min(GH_MAX_ROPE, vlen(h.origin - gg_at)); // (paying out behind it)
        // Flying: a prop on its way this frame?
        if(!VR_Grapple_FlightProps(h))
        {
            makevectors_mdl(h.angles);
            GrappleTrail(h, h.origin - v_forward * 6.5, 0);
            return;
        }
    }

    if(h.gh_state == GS_BITTEN)
    {
        if(VR_Grapple_TargetGone(h))
        {
            VR_Grapple_Free(h, "what it bit is gone", FALSE);
        }
        else
        {
            // The hook stays where it bit (following what moves).
            setorigin(h, VR_Grapple_HookPoint(h));
            makevectors_mdl(h.angles);
            hookfwd = v_forward;
        }
    }
    vector at = VR_Grapple_HookPoint(h);
    if(gg_moved)
    {
        // The rope from the gun's new place: long enough to reach (never pulling anything at once).
        h.gh_len = min(GH_MAX_ROPE, max(h.gh_len, vlen(at - VR_Grapple_RopeEnd(h))));
    }

    // Rogue's pull: chosen, or for a player without tracked hands (flat screen, a bot: no B/Y to reel with).
    if(h.gh_state == GS_BITTEN && xHand >= 0 && (!VR_Grapple_RopeMode() || !VRHandsTracked(self)))
    {
        if(VR_Grapple_Debug() >= 2 && time >= h.gh_log_at)
        {
            h.gh_log_at = time + 0.25;
            dprint(sprintf("grapple: hand %g pulls you (Rogue's): %.0f away, at %v moving %v\n", xHand, vlen(at - self.origin),
                self.origin, self.velocity));
        }
        VR_Grapple_AutoPull(h, xHand);
        GrappleTrail(h, at - hookfwd * 6.5, 0);
        return;
    }

    float reeling = FALSE;
    if(xHand >= 0)
    {
        reeling = VR_Grapple_Buttons(h, xHand);
    }

    if(h.gh_state == GS_LOOSE)
    {
        float d = vlen(at - gg_at);
        float home = GH_PROP_GAP + GH_HOOK_HALF + GH_HOME;
        if(!VR_Grapple_Movable(h))
        {
            // Carried (by a hand) or flying to one: the rope pays out; reeled with it at the gun, it goes back in.
            h.gh_len = min(GH_MAX_ROPE, max(h.gh_len, d));
        }
        else
        {
            VR_Grapple_ServiceProp(h, xHand, reeling);
        }
        if(reeling && d <= home)
        {
            VR_Grapple_Home(h, "reeled in");
            return;
        }
        // Caught near the gun (on a step's edge, under the gun through a floor's lip) while it is reeled: the reel yanks it
        // free, and it flies the rest of the way in. (Farther off, a caught hook stalls the reel: walk, or the quick
        // release.)
        if(reeling && d < GH_YANK_NEAR && h.gh_prev_d - d < 20 * frametime)
        {
            if(!h.gh_stall_at)
            {
                h.gh_stall_at = time;
            }
            if(time - h.gh_stall_at >= GH_YANK_TIME)
            {
                h.gh_stall_at = 0;
                VR_Grapple_Retract(h, "the reel yanks it free");
                h.gh_prev_d = 0;
                GrappleTrail(h, h.origin, 0);
                return;
            }
        }
        else
        {
            h.gh_stall_at = 0;
        }
        h.gh_prev_d = d;
    }
    else if(h.gh_kind == GH_PROP)
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
    if(h.gh_state == GS_BITTEN && VR_Grapple_TargetGone(h))
    {
        VR_Grapple_Free(h, "what it bit is gone", FALSE); // (a pickup reeled in, taken)
    }
    at = VR_Grapple_HookPoint(h);

    // vr_grapple_debug 2: the rope's state four times a second, reeling or not (a hook that holds: nothing moves).
    if(VR_Grapple_Debug() >= 2 && !reeling && h.gh_payout <= 0 && time >= h.gh_log_at)
    {
        h.gh_log_at = time + 0.25;
        entity load = h.enemy;
        vector loadAt = load.origin;
        if(h.gh_kind == GH_ANCHOR)
        {
            load = self;
            loadAt = self.origin;
        }
        dprint(sprintf("grapple: hand %g holds (hook %g %s): rope %.0f, %.0f away, ", xHand, h.gh_tag,
            VR_Grapple_StateName(h.gh_state), h.gh_len, vlen(at - VR_Grapple_RopeEnd(h))));
        dprint(sprintf("%s at %v moving %v\n", load.classname, loadAt, load.velocity));
    }

    // Drawn slack: the rope's length less how far its ends are, as a share of it.
    float slack = 0;
    if(h.gh_len > 1)
    {
        slack = max(0, h.gh_len - vlen(at - VR_Grapple_RopeEnd(h))) / h.gh_len;
    }
    GrappleTrail(h, at - hookfwd * 6.5, slack);
};

// ----------------------------------------------------------------------------
// Every frame
// ----------------------------------------------------------------------------

// The hook `h`, once a frame, wherever its gun is.
void(entity h) VR_Grapple_Service =
{
    if(h.gh_served == time)
    {
        return;
    }
    h.gh_served = time;
    float found = VR_Grapple_FindGun(h);
    if(gg_moved && VR_Grapple_Debug())
    {
        string where = "nowhere";
        if(gg_where == GW_HAND)
        {
            where = gg_hand == cVR_OffHand ? "in the off hand" : "in the main hand";
        }
        else if(gg_where == GW_HOLSTER)
        {
            where = sprintf("in holster %g", gg_slot);
        }
        else if(gg_where == GW_PROP)
        {
            where = "lying about";
        }
        dprint(sprintf("grapple: hook %g's gun is %s now (hook %s, rope %.0f)\n", h.gh_tag, where,
            VR_Grapple_StateName(h.gh_state), h.gh_len));
    }
    if(found)
    {
        VR_Grapple_Commit(h);
    }
    if(!found)
    {
        if(!h.gh_lost_at)
        {
            h.gh_lost_at = time;
        }
        if(time - h.gh_lost_at >= GH_LOST_TIME)
        {
            VR_Grapple_Home(h, "its gun is gone");
        }
        return;
    }
    if(gg_player != world)
    {
        if(gg_player.health <= 0 || gg_player.teleport_time > time)
        {
            VR_Grapple_Home(h, "its holder died or was teleported");
            return;
        }
        entity oldSelf = self;
        self = gg_player;
        if(gg_where == GW_HAND)
        {
            VR_Grapple_ServiceHeld(h, gg_hand);
        }
        else
        {
            VR_Grapple_ServiceHeld(h, -1);
        }
        self = oldSelf;
        return;
    }

    entity oldSelf2 = self;
    self = h;
    if(h.gh_state == GS_RETRACT)
    {
        VR_Grapple_ServiceRetract(h);
    }
    else
    {
        if(h.gh_state == GS_FLYING)
        {
            VR_Grapple_FlightProps(h);
        }
        else if(h.gh_state == GS_BITTEN && VR_Grapple_TargetGone(h))
        {
            VR_Grapple_Free(h, "what it bit is gone", FALSE);
        }
        if(VR_Grapple_FindGun(h))
        {
            VR_Grapple_ServiceLying(h);
        }
    }
    self = oldSelf2;
};

// Every player frame (client.qc PlayerPreThink; self: the player): the hooks of the guns in its hands and holsters (the
// rope may hold the player: before the move).
void() VR_Grapple_PlayerFrame =
{
    VR_Grapple_Rearm(cVR_OffHand);
    VR_Grapple_Rearm(cVR_MainHand);
    float hand = cVR_OffHand;
    while(hand <= cVR_MainHand)
    {
        entity h = getHook(hand);
        if(h)
        {
            if(VRGetWeaponFrame(hand) == 0)
            {
                VRSetWeaponFrame(hand, 2); // (a gun taken with its hook out)
            }
            VR_Grapple_Service(h);
        }
        hand = hand + 1;
    }
    float s = VR_Grapple_NextHolster(0);
    while(s)
    {
        if(VR_Grapple_HolsterWeapon(self, s) == WID_GRAPPLE)
        {
            entity h2 = VR_Grapple_HookOf(VR_Grapple_TagOf(VR_Grapple_HolsterFlags(self, s)));
            if(h2)
            {
                VR_Grapple_Service(h2);
            }
        }
        s = VR_Grapple_NextHolster(s);
    }
};

// Every frame (world.qc StartFrame): the hooks whose gun no living player has (lying about, gone), and those of the dead.
// The players' own come in their frames (VR_Grapple_PlayerFrame).
void() VR_Grapple_WorldFrame =
{
    entity h = find(world, classname, "hook");
    while(h)
    {
        entity next = find(h, classname, "hook");
        if(h.gh_tag > 0)
        {
            float found = VR_Grapple_FindGun(h);
            if(!found || gg_player == world || gg_player.health <= 0)
            {
                VR_Grapple_Service(h);
            }
        }
        h = next;
    }
};

// impulse 239: every hook, its state, its gun's place and its rope (tests; the console).
void() VR_Grapple_Report =
{
    float n = 0;
    entity h = find(world, classname, "hook");
    while(h)
    {
        n = n + 1;
        float found = VR_Grapple_FindGun(h);
        string where = "nowhere";
        if(gg_where == GW_HAND)
        {
            where = gg_hand == cVR_OffHand ? "off hand" : "main hand";
        }
        else if(gg_where == GW_HOLSTER)
        {
            where = sprintf("holster %g", gg_slot);
        }
        else if(gg_where == GW_PROP)
        {
            where = sprintf("lying at %v", gg_ent.origin);
        }
        vector at = h.origin;
        if(h.gh_state == GS_BITTEN)
        {
            at = VR_Grapple_HookPoint(h);
        }
        dprint(sprintf("grapple report: hook %g %s (%s), gun %s, ", h.gh_tag, VR_Grapple_StateName(h.gh_state),
            h.enemy == world ? "the world" : h.enemy == h ? "itself" : h.enemy.classname, where));
        dprint(sprintf("rope %.1f, gun to hook %.1f, hook at %v\n", h.gh_len, found ? vlen(at - gg_at) : -1, at));
        h = find(h, classname, "hook");
    }
    dprint(sprintf("grapple report: %g hooks; main hand %g flags %g, off hand %g flags %g\n", n, self.weapon, self.weaponflags,
        self.weapon2, self.weaponflags2));
    dprint(sprintf("grapple report: you at %v, the main hand at %v, the off hand at %v\n", self.origin, self.handpos,
        self.offhandpos));
};

// impulse 233 (tests): the player takes what every hook is in, as walking into it (a pickup is taken).
void() VR_Grapple_TestTouch =
{
    entity h = find(world, classname, "hook");
    while(h)
    {
        entity t = h.enemy;
        if(h.gh_state == GS_BITTEN && t != world && (t.carry_use || t.handtouch))
        {
            dprint(sprintf("grapple test: you touch %s (hook %g)\n", t.classname != string_null ? t.classname : t.model, h.gh_tag));
            entity oldSelf = self;
            entity oldOther = other;
            // As the body's touch takes it (the "fake hand": vr_util.qc), by its pickup (a carried box's carry_use); a
            // thrown weapon as an empty hand catching it (a force grab's catch: into the hand).
            float hand = cVR_FakeHand;
            if(t.classname == "thrown_weapon")
            {
                if(VRIsHandEmpty(self, cVR_MainHand))     { hand = cVR_MainHand; }
                else if(VRIsHandEmpty(self, cVR_OffHand)) { hand = cVR_OffHand; }
            }
            self.touchinghand = hand;
            t.handtouch_hand = hand;
            t.handtouch_ent = self;
            other = self;
            self = t;
            float oldCatching = forcegrabCatching;
            forcegrabCatching = TRUE;
            if(t.carry_use)
            {
                t.carry_use();
            }
            else
            {
                t.handtouch();
            }
            forcegrabCatching = oldCatching;
            self = oldSelf;
            other = oldOther;
        }
        h = find(h, classname, "hook");
    }
};
