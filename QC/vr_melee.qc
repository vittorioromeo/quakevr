// vr_melee.qc -- melee for tracked hands: one model for swords, axes, guns and bare hands (docs/vr-port/ROUND21.md,
// "Melee, redesigned").
//
// WHAT A HAND HOLDS is laid out as a line: its near end (a sword's pommel, the end of an axe's handle, a gun's butt),
// its grip (the hand) and its far end (the muzzle point of its model: a sword's tip, an axe's head, a gun's muzzle; a
// fist's knuckles). Points along it, each of one kind (VR_MPT_*), are swept from the last pose to this one against
// what bleeds and against walls.
//
// A HIT is a point sweeping into something while the ARM drives it: the grip's speed, relative to the head (walking and
// turning are not the hand's motion), at least vr_melee_speed (a weapon's swing VR_MELEE_ARM times that), having come
// VR_MELEE_RUN towards where it goes (a blow, not a wiggle; a slow swing from further, VR_MELEE_SLOW_*) and carrying the
// hand (the wrist's speed). Any direction but back towards the hand (VR_MELEE_PULLBACK: drawn back); what a hand struck
// it strikes again only entering it (a retraction still in it strikes nothing). What kind of blow it is follows
// from which part hit and how it moved (VR_Melee_Decide): a stab or jab (the far end moving along the line, outwards), a
// slash or chop or gun swing (the far end any other way), a pommel or butt strike (the near end), a punch (a closed
// fist). Of a blade's points in the thing, the outermost strikes. The pommel's touch waits a moment for the blade
// proper (vr_melee_pommel_wait): a cut from close by is the blade's. The hilt, a handle, a gun's grip (where the hand
// is) strike nothing.
//
// THE PARRY STANCE is the held thing presented across (the parry's pose: VR_Parry_Blocks, combat.qc), or an open palm
// facing ahead. A BASH is the stance, held VR_BASH_HELD, pushed forward (both its ends going ahead), a few centimetres:
// then what is just ahead is knocked back (VR_Bash). A palm's SHOVE is the palm pushed out from the shoulder. Two hands
// (a weapon held two-handed, an open palm pushing on a one-handed weapon, both palms) bash harder.
//
// The numbers are in one place, below: tune them against the labelled takes (vr_motion_eval), not case by case. Those the
// engine's melee builtins use too (the history's length and look-back times) are in Quake/vr/vr_melee_shared.h.

#include "../Quake/vr/vr_melee_shared.h"

// ----------------------------------------------------------------------------
// The model's numbers

// Strikes.
float VR_MELEE_ARM = 1.25;         // a weapon's swing: the grip's least speed, times vr_melee_speed (a weapon is heavier than a fist;
                                   // the author's cuts move the hand 4.5-13 m/s, his sword and gun waved at the dummy 2-4)
float VR_MELEE_STAB_SPEED = 0.75;  // a stab's (jab's) least speed of the grip, times vr_melee_speed (a thrust needs less than a swing)
float VR_MELEE_STAB_ALONG = 0.4;   // cosine: the far end moving within ~66 degrees of the line, outwards, is a stab (the author's
                                   // stabs 0.4-0.87 at the contact, his cuts at most 0.36)
float VR_MELEE_UP = 0.5;           // a punch's least speed grows by this share for a blow going straight up (the author's
                                   // uppercuts go 8-10 m/s, a fist wiggled upwards 4.5-5.5; his hooks rise a little, 6.5-10),
float VR_MELEE_DOWN = 1.5;         // and by this share for each unit (sine) of steepness downwards beyond VR_MELEE_DOWN_FROM
float VR_MELEE_DOWN_FROM = 0.4;    // (a hammer blow falls easily: his overhead punches go 12 m/s, a hand waved down 6)
float VR_MELEE_PUNCH_LAND = 1.35;  // a punch lands (and whooshes) only this far over its least speed; its strength still counts from
                                   // the least speed (the author's takes of 2026-09-29: his punches 1.42-4.1 times it, his weak
                                   // punches, which he wants to miss, 1.31 at most)
float VR_MELEE_GUN_LAND = 1.5;     // likewise a gun's swing (its barrel, its muzzle): a gun is waved, aimed and reloaded all the time
                                   // (his gun swings 9.7-11.7 m/s at the grip, 1.9 times a swing's least speed; his guns wiggled and
                                   // swung down to reload hit the dummy at 5-6, 1.2 at most)
float VR_MELEE_GUN_BUTT = 1.3;     // a gun's butt strike: its grip at least this times vr_melee_speed (a sword's pommel 1: the
                                   // author's butt strikes drive the grip 5.7-9 m/s, a gun wiggled at the dummy 4.8)
float VR_MELEE_RUN = 0.2;          // m the grip came towards where it goes now, within VR_MELEE_RUN_TIME: a blow, not a wave or a
                                   // wiggle (the author's punches come 0.22-0.8 m, his fists waved fast at the dummy 0.05-0.2)
float VR_MELEE_SLOW_FROM = 2;      // a weapon's swing under this many times the speed it lands at
float VR_MELEE_SLOW_RUN = 0.4;     // must have come up to this much further (m; all of it at the least speed, none from
                                   // VR_MELEE_SLOW_FROM): a slow swing is a blow when it comes from far, not a wave or a
                                   // wiggle reaching the dummy (2026-10-12 takes: his "no hit" axe and sword swings went 1-1.6
                                   // times their least speed and came 0.22-0.51 m; his slowest cuts, 1.27-1.43 times it,
                                   // came 0.66-0.86 m, his cuts of 0.42-0.55 m went 2-2.6 times it)
float VR_MELEE_PULLBACK = -0.35;   // cosine: a blade, head or fist whose far end goes back along the line towards the hand
                                   // (within 70 degrees: a weapon drawn back after a blow, a fist pulled in) strikes nothing
                                   // (2026-10-12 "no hit" takes: their swords, axes and fists pulled back hit at -0.4 to
                                   // -1; his cuts, chops, punches and slaps go -0.26 to 1)
float VR_MELEE_WRIST_SWING = 2.3;  // a sword's, axe's or club's swing: its wrist speed, times vr_melee_wrist_speed (the arm
                                   // swings it: his cuts and chops carry the wrist 3-7.6 m/s, his "no hit" waves and wiggles
                                   // 1.3-2.8, mostly under 2.2; not a stab: his two-handed thrusts 1.7-2.3)
float VR_MELEE_WRIST_POMMEL = 2.5; // a pommel (butt, handle's end) strike's wrist speed, times vr_melee_wrist_speed: as a
                                   // punch, the hand carries it at the thing (his pommel strikes 4.6-5.6 m/s, his axe and
                                   // sword waved at the dummy flicked their near ends round with the wrist at 1.3-2)
float VR_MELEE_POMMEL_RUN = 0.35;  // m a sword's or axe's pommel (handle's end) came, as VR_MELEE_RUN (his pommel strikes
                                   // 0.52-0.76, the earlier ones 0.36 and more; a "no hit" axe waved 0.29); a gun's butt:
                                   // vr_melee_butt_run
float VR_MELEE_RELOAD_DOWN = 0.8;  // a gun whose grip went down this steeply (sine) or more over VR_MELEE_RELOAD_TIME,
float VR_MELEE_RELOAD_FROM = 0.1;  // from no higher than this over the eyes (m; aim or torso level, not an overhead chop),
float VR_MELEE_RELOAD_TIME = 0.15; // is taken down to a holster to reload: no blow (the author's fast reloads went 0.5-0.7 m
                                   // down, 0.97-0.99 steep (less their coming back), from level with the eyes to 0.3 m under;
                                   // his gun strikes downwards came 0.8-0.95 m ahead and at most 0.26 down: 0.3 steep at most)
float VR_MELEE_WRIST_BACK = 0.2;   // m behind the controller (along its own axis, the gun angle taken out) the hand turns about
                                   // when it wiggles (fitted on the author's wiggles with fists, sword, axe and gun: 17-24 cm)
float VR_MELEE_WRIST_PUNCH = 2;    // a punch's wrist speed, times vr_melee_wrist_speed: a fist has no lever, the arm carries it (the
                                   // author's punches carry the wrist 3.5 m/s and more, his fists wiggled at the dummy 1.5 at most)
float VR_MELEE_BLADE_FROM = 0.4;   // the blade proper: from this share of the way from the hand to the tip
float VR_MELEE_GOOD = 2;           // times the least speed: a blow of strength 1
float VR_MELEE_WEAKEST = 0.5;      // the strength at the least speed
float VR_MELEE_STRONGEST = 1.75;   // and the most (the author's flatter curve, round 22: was 2; the damage caps then bound
                                   // the blow's strength times its zone: vr_melee_dmg_min, vr_melee_dmg_max)
// A sword's strength is measured at its tip (the author, round 22): its far end's speed over the least speed times
// these (the detection keeps the grip's). Fitted so the author's takes keep their mean strength: his slashes swing the
// tip 2.96 times the grip (median; 1.6-4.0), his stabs 1.54 (1.0-2.7). A wrist-whipped cut now counts more than an
// arm-pushed one.
float VR_MELEE_TIP_SLASH = 3;
float VR_MELEE_TIP_STAB = 1.5;
float VR_MELEE_REARM_SLOW = 0.5;   // a hand strikes the same thing again once its part slowed below this share of the least speed,
float VR_MELEE_REARM_TURN = -0.17; // or, out of it, turned back more than 100 degrees from the blow (cosine: a backswing),
float VR_MELEE_REARM_TIME = 0.6;   // or, out of it, after this long (s)
float VR_MELEE_REARM_OUT = 0.1;    // s since the hand's parts last touched what it struck: out of it
float VR_MELEE_RADIUS = 3;         // units around a point's sweep that it strikes (times vr_melee_range_multiplier)
float VR_MELEE_JUMP = 20;          // m/s: the grip moving faster than this jumped (a teleport, the play space re-based)
float VR_MELEE_TIPJUMP = 60;       // m/s: likewise the far end (a weapon taken)
// The engine's weight spring put what a hand holds back in it at once (.handdrop's bits, vr_weight.cpp dropBits): a
// jump too (a weapon left too far behind a fast wrist snap).
float VR_HANDJUMP_OFF = 4;
float VR_HANDJUMP_MAIN = 8;
float VR_MELEE_WALL_DOWN = -0.35;  // a point going down more steeply than this (sine) doesn't strike a floor (reaching for a holster),
float VR_MELEE_FLOOR = 0.7;        // (a surface facing up: its normal's z at least this)
float VR_MELEE_WALL_INTO = 0.25;   // and strikes a wall only going into it (cosine to its normal: a chop, not a reach grazing it)
float VR_MELEE_FLOOR_AHEAD = 0.35; // m: but the floor this far ahead of the head (a chop at the ground, not a reach by the hip)
float VR_MELEE_WALL_ENTRY = 0.5;   // a part strikes a wall going into it at this share of the least speed at least (m/s along
                                   // its normal, now or a pose before: VR_Melee_WallInto); slower, a brush (round 22 meleewall)
                                   // for vr_melee_wall_hold after it the hand's parts still at (in) that wall land a blow there
                                   // (VR_Melee_WallHeld): on
float VR_MELEE_WALL_SAME = 0.7;    // the same surface (cosine of the normals),
float VR_MELEE_WALL_LEAVE = 0.5;   // not moving out of it (cosine of their way to its normal at most)
float VR_MELEE_WALL_AGAIN = 0.35;  // s: a hand strikes a wall again only this long after its last blow on one (the wall stops the hand,
                                   // which re-arms it at once: VR_Melee_Rearm; it bounced off and struck twice)
float VR_MELEE_WALL_OFF = 0.2;     // m: and the same wall only once its weapon has been this far off it since (all of it: near end,
                                   // grip, far end); a weapon lying on the wall struck it again with another part (round 22 wallhit2)
float VR_MELEE_WALL_FOLLOW = 1.2;  // s: and within this of its blow on that wall, only a blow as fast as this share of it at least
float VR_MELEE_WALL_WEAKER = 0.6;  // (the swing's own tail: its recovery poking the wall again, slow; wallhit2)
float VR_MELEE_POMMEL_SWORD = 0.6; // weights: a sword's pommel strike,
float VR_MELEE_POMMEL_AXE = 0.5;   // an axe's or Mjolnir's handle's end,
float VR_MELEE_POMMEL_CROWBAR = 0.6; // the crowbar's chisel end (a steel bar's end: as a sword's pommel),
float VR_MELEE_CROWBAR_HOOK = 1.2;   // and its hook (the claw biting: the last points, VR_CROWBAR_HOOK_F on) over its bar
float VR_CROWBAR_HOOK_F = 0.9;       // share of the way from the hand to the far end where the hook's points start
float VR_CROWBAR_NEAR = 0.78;        // the chisel's tip below the hand, a share of the hand-to-far-end line (vr_hotspot_fit:
                                     // the hand 29 cm up the bar, the far end 66)

// The stance and the bash.
float VR_BASH_AHEAD = 0.75;       // cosine: a push within ~41 degrees of the facing
float VR_BASH_END_AHEAD = 0.75;   // cosine: each end of the stance moving forward (within 41 degrees),
float VR_BASH_END_SPEED = 0.4;    // at this share of vr_bash_speed at least
float VR_BASH_TRAVEL = 0.06;      // m pushed that way before it is a bash
float VR_BASH_HELD = 0.5;         // s a weapon's stance is held before its push begins (a stance, not a swing passing through
                                  // it: the author's cuts from a guard pass it in 0-0.4 s, his parry bashes hold it 0.9-2.3)
float VR_BASH_GAP = 0.08;         // s a push may falter and go on
float VR_BASH_REACH = 0.75;       // m ahead of the stance a bash reaches
float VR_BASH_PAIR = 0.6;         // the second hand pushes at this share of vr_bash_speed
float VR_BASH_PAIR_NEAR = 0.2;    // m: an open palm this near a one-handed weapon's line pushes it with the weapon's hand
float VR_BASH_ONE_DAMAGE = 0.6;   // one hand: this share of the damage,
float VR_BASH_ONE_PUSH = 0.7;     // and of a batted projectile's speed (the knockback: vr_shove_push_onehand)
float VR_BASH_PALM = 0.6;         // cosine: an open palm facing ahead (within 53 degrees)
float VR_BASH_PALM_ALONG = 0.7;   // cosine: moving the way it faces (within 45 degrees)
float VR_BASH_PALM_RADIAL = 0.82; // cosine: a shove extends the arm: the palm moving out from its shoulder (his shoves:
                                  // 0.84-0.98; his hands waved down or round at the dummy, palm first: 0.55-0.8)
float VR_SLAP_RADIAL = 0.7;       // cosine: a slap does not extend the arm: the open hand moving out from its shoulder less
                                  // than this (a shove's palm, VR_BASH_PALM_RADIAL: 0.82 and more; between: neither)
float VR_SLAP_ACROSS = 0.5;       // cosine: and goes across the body, within 60 degrees of sideways (not a pat down or up)
float VR_SHOULDER_DOWN = 0.28;    // m: a shoulder below the eyes,
float VR_SHOULDER_SIDE = 0.2;     // and to the side
float VR_BASH_COOLDOWN = 0.8;     // s after a bash lands before the next
float VR_BASH_BAT_SPEED = 750;    // u/s a two-handed bash at vr_bash_speed bats a projectile away at (VR_Bash_Deflect)
float VR_BASH_BAT_MIN = 0.7;      // times that for a slow push,
float VR_BASH_BAT_MAX = 1.8;      // and a hard one (push speed over vr_bash_speed, clamped)
float VR_BASH_FACE_MIN = 0.25;    // a bash's point moving less than this share across the blade (pushed along it, a
                                  // thrust): what it meets goes the push's way, not off the face (VR_Bash_Direction)
float VR_STANCE_REPORT = 0.2;     // s a stance is held before it is reported (developer: "melee event: stance")

// ----------------------------------------------------------------------------
// What a hand holds

float VR_MTHING_FIST = 0;  // a fist (a box in hand too) or an open hand
float VR_MTHING_SWORD = 1; // a knight's sword
float VR_MTHING_AXE = 2;   // the axe, Mjolnir: a short handle and a head
float VR_MTHING_GUN = 3;   // a gun: blunt all along (held, or carried by its pump)
float VR_MTHING_NONE = 4;  // the empty hand steadying the other's weapon, a climbing hand: strikes nothing
float VR_MTHING_CLUB = 5;  // a carried prop with a melee tip (Held Object Offsets: Tip; a wall torch): its head strikes
float VR_MTHING_SAW = 6;   // the ogres' chainsaw (vr_chainsaw.qc): a melee weapon, its bar strikes as an axe's head
float VR_MTHING_CROWBAR = 7; // the crowbar (vr_crowbar.qc): a steel bar, blunt all along past the hands, its hook at the end

// Whether a hand holding `xThing` is armed: a weapon (a sword, an axe or Mjolnir, any gun) or a club (a carried prop
// with a melee tip: a wall torch, a whole brick, a rock or a prop given a Tip in Held Object Offsets). Only an armed
// hand bats projectiles back, by a swing (VR_Deflect) or a bash (VR_Bash_Deflect): an empty hand, a fist, an open palm,
// the flashlight, a box or a gib without a tip, a prop carried with both hands never do (the author's choice, ROUND21.md
// "Debug menu; quad sound; grenade catch default; no empty-hand deflection"); an empty hand catches a grenade instead
// (VR_Grenade_HandCatches), or the projectile flies on and hits you.
float(float xThing) VR_Melee_HoldsWeapon =
{
    return xThing == VR_MTHING_SWORD || xThing == VR_MTHING_AXE || xThing == VR_MTHING_GUN || xThing == VR_MTHING_CLUB ||
           xThing == VR_MTHING_SAW || xThing == VR_MTHING_CROWBAR;
};

// The kinds of points (what they strike as).
float VR_MPT_EDGE = 0;  // the blade, an axe's head, a gun's barrel and muzzle: moves with the far end
float VR_MPT_GRIP = 1;  // the hilt, the guard, the hand on a handle or a gun's grip: where the hand is, it strikes nothing
                        // (the author's pommel strikes land with the near end; his weapons waved at the dummy touched it
                        // with the hilt, the handle, a gun's grip); it only keeps a blow's target touched (VR_Melee_Rearm)
float VR_MPT_NEAR = 2;  // the pommel, the handle's end, a gun's butt
float VR_MPT_FIST = 3;  // the fist and the knuckles
float VR_MPT_BLUNT = 4; // a weapon carried off its handle (VR_Melee_FreeLayout): its pommel, hilt, handle or butt, struck
                        // with as its blade is (at once, at a swing's speeds) but a pommel (butt) strike

// How long the pommel's (the near end's) touch waits for the blade proper (VR_MELEE_BLADE_FROM out), s
// (vr_melee_pommel_wait): a cut from close by is the blade's (the author's close chops draw the weapon back along its
// line, as a pommel strike does). Only a pommel strike waits; a blade, a head and a fist land at once.
float() VR_Melee_PommelWait =
{
    return max(0, cvar_hget(cvarh_vr_melee_pommel_wait));
};

// Whether a point of kind `xKind` strikes (the blade, a fist) rather than being the near end. (A function of the kind
// read once. `striker = vr_mpt_kind[i] == VR_MPT_EDGE || vr_mpt_kind[i] == VR_MPT_FIST` was
// `(striker = kind == EDGE) || ...` to fteqcc (an assignment binds tighter than || in QC; docs/vr-port/CODE_STYLE.md,
// QuakeC), FALSE for a fist's points, so every punch went the pommel's way and waited
// VR_Melee_PommelWait before it landed: the author's punches landing late, NOTES.md vrfiringrange_2026-09-29_21-57-10.)
float(float xKind) VR_Melee_Strikes =
{
    return xKind == VR_MPT_EDGE || xKind == VR_MPT_FIST || xKind == VR_MPT_BLUNT;
};

// The kinds of blows.
float VR_MKIND_SLASH = 0;  // a swing's blade, an axe's head, a gun's barrel sweeping across
float VR_MKIND_STAB = 1;   // driven along the line, the far end first
float VR_MKIND_POMMEL = 2; // the near end (the blade proper not following)
float VR_MKIND_PUNCH = 3;  // a fist (any way: jab, hook, uppercut, overhead)
float VR_MKIND_SLAP = 4;   // an open hand swung across (VR_Melee_Slaps): a punch's, weaker (vr_melee_slap_mult), no gore

// The hand's state, on an entity of its own (the player's vr_blow_main / vr_blow_off).
.entity vr_blow_main;
.entity vr_blow_off;
.float mh_time;        // when the last new pose was taken
.float mh_ptime;       // its own time (the client's .vr_posetime; -1 unknown)
.float mh_dt;          // s since the one before
.float mh_thing;       // VR_MTHING_*
.float mh_wid;         // the weapon (another: nothing to sweep from)
.float mh_n;           // points (0: none yet)
.vector mh_pp[9];      // the points at the pose before (world),
.vector mh_cp[9];      // and now
// A hammer's or mace's head (vr_melee_swept: its volume strikes, VR_Melee_Ribs): its box on the line (admeleehead:
// shares of the line's length; '0 0 0' none), and the line's frame (the grip, the far end, the model's +y: unit) at
// the pose before and now.
.vector mh_hd_lo;
.vector mh_hd_hi;
.vector mh_hd_pg;
.vector mh_hd_pf;
.vector mh_hd_py;
.vector mh_hd_cg;
.vector mh_hd_cf;
.vector mh_hd_cy;
.vector mh_near;       // the near end, the grip and the far end (world, now)
.vector mh_grip;
.vector mh_pgrip;      // the grip at the pose before (world; VR_Melee_ArcRib: the points' arcs about it)
.vector mh_far;
.vector mh_rnear;      // the same from the head, in the play space (metres)
.vector mh_rgrip;
.vector mh_rfar;
.vector mh_rpunch;     // a fist's way out (the hand's forward, play space, unit): VR_Melee_PulledBack's axis for a fist
.vector mh_vnear;     // and their velocities (m/s, play space)
.vector mh_vgrip;
.vector mh_vfar;
.vector mh_vnear_last; // and a pose before (VR_Melee_WallInto)
.vector mh_vgrip_last;
.vector mh_vfar_last;
.float mh_wall_time;   // when a part last went into a wall hard enough to strike it (VR_Melee_WallEntered),
.vector mh_wall_n;     // and its normal,
.vector mh_wall_vnear; // and the line's velocities it came in at (VR_Melee_WallDecide)
.vector mh_wall_vgrip;
.vector mh_wall_vfar;
.float mh_wall_blow;   // when its last blow on a wall landed (VR_MELEE_WALL_AGAIN)
.vector mh_wall_bn;    // and that wall's plane (its normal, and the normal times a point of it: VR_Melee_WallFresh),
.float mh_wall_bd;
.float mh_wall_bspeed; // that blow's speed (m/s),
.float mh_wall_on;     // and the weapon not yet off that wall since (VR_Melee_WallLeft)
.float mh_free;      // a weapon carried off its handle: laid out along all of it (VR_Melee_FreeLayout), with
.float mh_free_hs;     // the hand's share of the way from its near end to its tip,
.float mh_free_tip;    // the far end its tip (else its near end),
.float mh_free_gw;     // the hand's half width (a share of its length),
.float mh_free_back;   // and how far its near end is behind its handle (a share of the handle to the tip)
.float mh_fast;        // the part's fastest (m/s): the far end, or the grip if faster
.float mh_bwidth;      // the blade's width: its swept points' radius's multiplier (the weapon's Blade Width; 0 taken as 1)
.float mh_bfrom;       // its blade start of its own (the weapon's Blade From: VR_Melee_LayoutFrom; 0 or below: the kind's)
.float mh_lenient;     // heavy leniency: its speed thresholds' factor, from what the hand holds (VR_Weight_Leniency)
.float mh_heavy2h;     // heavy handling: the weapon's weight class held two-handed (VR_Heavy_TwoHanded; 0: not)
.float mh_curve;       // its strength's rise over the speed needed, times this (VR_Weight_MeleeCurve)
.float mh_whooshed;    // a whoosh played for this motion (until it slows)
.float mh_overland;    // frames in a row the grip was as fast as a blow lands (VR_Melee_Whoosh: a slap's whoosh)
.entity mh_struck;     // the last thing a blow of the hand landed on, kept past the re-arm: struck again only entering it
.entity mh_hit_ent;    // the last thing struck (world: none, or a bash or a held gib spent the motion),
.vector mh_hit_dir;    // the struck part's way then (play space),
.float mh_hit_time;    // when
.float mh_hit_touch;   // the last time a part touched it (VR_MELEE_REARM_OUT)
// One swing, many things (vr_melee_multi, VR_Melee_Multi): what this motion struck, each once (VR_Melee_Armed), and
// whether it struck the world (a wall, the floor: once a motion, VR_Melee_WallArmed). Cleared as the motion is new.
.float mh_hit_n;       // how many things (VR_MELEE_MULTI_LIST at most),
.float mh_hit_cap;     // of them not loose gibs (vr_melee_multi_max of them at most: a blow's small gibs don't count)
.entity mh_hit_e0;
.entity mh_hit_e1;
.entity mh_hit_e2;
.entity mh_hit_e3;
.entity mh_hit_e4;
.entity mh_hit_e5;
.entity mh_hit_e6;
.entity mh_hit_e7;
.float mh_hit_world;   // the world struck
// The blow being dealt (VR_Melee_Contact, VRMeleeDmg, the dummy's report).
.float mh_kind;        // VR_MKIND_*
.float mh_strength;    // its damage multiplier
.float mh_speed;       // the striking part's speed (m/s)
.float mh_dmgspeed;    // the speed its strength was measured at (a sword's tip's; else mh_speed): the report's
.float mh_core;        // its strength from speed alone (VR_MELEE_WEAKEST .. STRONGEST): what the damage caps bound
                       // with the zone's multiplier (VR_Melee_CapDamage)
.string mh_part;       // its name ("the tip")
.string mh_way;        // how it moved, for the report ("overhead", "hook")
.float mh_counter;     // a counter (VR_Counter_Use): its damage multiplier (0: not a counter)
.float mh_exhaust;     // short of stamina for it (VR_Stamina_Strike): its damage multiplier (0: not short)
.float mh_stam_paid;   // this motion's stamina was spent (once a motion, however many things it goes through)
.entity mh_c_ent;      // where it lands, as a traceline's hit
.vector mh_c_pos;
.vector mh_c_dir;
.vector mh_c_normal;
.float mh_c_blunt;      // a wall's: struck by a blunt part (not VR_MPT_EDGE: a pommel, a butt, a handle; VR_Melee_WallMark)
.float mh_c_gib;        // it is a loose gib or head (it bursts: gone by VR_Melee_Landed)
// The stance and the push (VR_Bash).
.float mh_stance;      // 0: none, 1: a weapon across, 2: an open palm facing ahead
.float mh_stance_since;
.float mh_stance_said; // the stance was reported (developer)
.float mh_fresh;       // a new pose was swept this frame
.float mh_pushing;     // pushing as a bash does this frame
.float mh_push_dist;   // m pushed so far
.float mh_push_last;   // the last frame it pushed
.float mh_push_ok;     // the push began from a stance held VR_BASH_HELD (or a palm's)
.float mh_push_stance; // and which (1 a weapon's, 2 a palm's)
// The pommel's touch, waiting VR_Melee_PommelWait for the blade (VR_Melee_Sweep): the blow it would be.
.entity mh_pend_ent;
.float mh_pend_time;
.vector mh_pend_pos;
.vector mh_pend_dir;
.float mh_pend_kind;
.float mh_pend_strength;
.float mh_pend_speed;
.float mh_pend_dmgspeed;
.float mh_pend_core;
.string mh_pend_part;
.string mh_pend_way;
// The last poses' velocities (a ring of VR_MELEE_HISTORY, the newest at mh_hi; the engine's melee builtins read it:
// meleerun, meleewristspeed, meleewiggled, meleegripago): the far end's, the grip's, and each pose's time step.
.vector mh_hfar[VR_MELEE_HISTORY];
.vector mh_hgrip[VR_MELEE_HISTORY];
.vector mh_hwrist[VR_MELEE_HISTORY]; // the wrist's (VR_Melee_Wrist)
.vector mh_rwrist;     // the wrist from the head (play space, metres)
.float mh_hdt[VR_MELEE_HISTORY];
.float mh_hi;
// Batting swings (VR_Deflect, vr_juice.qc): the lines the weapon swept through.
.vector bat_a[8];
.vector bat_b[8];
.float bat_t[8];
.float bat_i;
.float bat_newest;
.vector bat_va[8];    // the lines' ends' velocities (world, u/s)
.vector bat_vb[8];
.float bat_paid_time; // when a batting swing last paid its stamina (VR_Deflect_Pay),
.float bat_exhaust;   // and what that left (as a blow's mh_exhaust: 0 paid in full)
.float bat_why;       // the last pose's swing: 0 a batting one, else why not (VR_BAT_WHY_*: VR_Deflect_SwingWhy)
.float bat_why_v;     // and its number against
.float bat_why_need;  // what it needed
.float bat_wrist;     // the newest batting line's hand speed (m/s: meleewristspeed) and how far it came (m: meleerun)
.float bat_run;

.float vr_bash_time;      // on a player: when the next bash may be
.float vr_blow_time;      // on a player: when a blow of his last landed, or he last shoved or bashed (the engine's
                          // bullet time ignores gadget screen taps a moment after: vr_bullettime_tap_blow_quiet)
.float vr_arms;           // on a player: arms crossed (the unarmed parry),
.float vr_arms_since;     // since when (0: reported)
.float vr_bash_mark;      // on a monster: found by this frame's bash (VR_Bash_Find),
.entity vr_bash_next;     // and the next one found
.float vr_bashdef_until;  // on a player: a bash's batting goes on until then (VR_Bash_Deflect),
.float vr_bashdef_hand;   // with that hand (-1: both)
.vector vr_bashdef_vel;   // the push's velocity (world, u/s; VR_Bash_DeflectStart), what it bats goes that way
.float vr_bashdef_both;   // pushed with both hands
.vector vr_bashdef_mvn;   // the main hand's weapon line's near and far ends' velocities in the push (world, u/s):
.vector vr_bashdef_mvf;   // where a bash meets a projectile on the blade, it goes the way that point moves
.vector vr_bashdef_ovn;   // (VR_Bash_Direction); and the off hand's
.vector vr_bashdef_ovf;
.float vr_push_start;     // on a player: when the push going on began (VR_Bash: its frames in a row)
.float vr_bash_paid;      // the push (its vr_push_start) whose stamina was paid (VR_Bash_Pay): once a push, batting or landing,
.float vr_bash_paid_dmg;  // and what it left (vr_bash_exhaust_dmg and _push)
.float vr_bash_paid_push;
float vr_bash_counter;    // while a bash's hits are dealt: a counter's damage multiplier (0: not a counter; vr_dummy.qc)
float vr_bash_both;       // and with two hands
float vr_bash_exhaust_dmg;  // and short of stamina: its damage multiplier (0: not short),
float vr_bash_exhaust_push; // and its knockback's and stagger's

// The layout of the points (VR_Melee_Layout): their place along the line (the grip 0, the far end 1; the near
// end negative), kind and name.
float vr_mpt_f[9];
float vr_mpt_fh[9]; // the place on the weapon (its handle 0, its tip 1; the near end -1 in VR_Melee_Layout's)
float vr_mpt_kind[9];
string vr_mpt_name[9];
float vr_mpt_n;

entity(float xHand) VR_Blow =
{
    entity b = xHand == cVR_OffHand ? self.vr_blow_off : self.vr_blow_main;
    if(b == world || b.classname != "vr_blow" || b.owner != self)
    {
        b = spawn();
        b.classname = "vr_blow";
        b.owner = self;
        if(xHand == cVR_OffHand)
        {
            self.vr_blow_off = b;
        }
        else
        {
            self.vr_blow_main = b;
        }
    }
    return b;
};

// The least speed of a strike (m/s).
float() VR_Melee_MinSpeed =
{
    return max(0.5, cvar_hget(cvarh_vr_melee_speed));
};

// Weight (round 21, "Spring only; Weapon Weights and Held Object Weights; weight and damage"): what `xHand` of `p` holds
// as the weight sees it: a carried prop (Held Object Weights, propvalue) or its weapon (Weapon Weights, weaponvalue; the
// empty hand's has no mass).
float(entity p, float xHand) VR_Weight_HeldMass =
{
    entity c = VR_Carry_Held(p, xHand);
    if(c != world)
    {
        return propvalue(c, "mass");
    }
    return weaponvalue(WeaponIdToModel(VRGetEntWeapon(p, xHand), VRGetEntWeaponFlags(p, xHand)), "w_mass");
};

// Blows on ragdolls and corpses (ROUND22.md, "Melee on ragdolls"): `targ` struck by the blow of `attacker`'s hand
// vr_hithand (T_Damage_VRMelee, after its damage) moves as two bodies meeting, not by vr_melee_push's knockback (a whole-
// body velocity, every part of a ragdoll given it: a strong blow's 150..300 u/s flung a corpse off): what the hand holds
// (its Mass, and vr_box3d_weapon_arm_mass of the arm behind it; an empty hand vr_box3d_hand_mass) at the blow's speed
// (its striking point's, mh_dmgspeed) along its way, with vr_melee_ragdoll_bounce of restitution. A ragdoll (a corpse's,
// one knocked down or staggering): the part struck (physicspush; its gain at most vr_melee_ragdoll_most m/s); one not a
// ragdoll yet (killed by the blow: its ragdoll is made from its motion; a corpse without one) its whole mass
// (VR_Grapple_MonsterMass). The living standing keep the knockback (FALSE).
float(entity m) VR_Grapple_MonsterMass;
float(entity targ, entity attacker) VR_Melee_RagdollPush =
{
    if(!(attacker.flags & FL_CLIENT) || vr_hitkind != QVR_HITKIND_MELEE || vr_hithand < 0 || vr_hitdir == '0 0 0')
    {
        return FALSE;
    }
    float rag = isragdoll(targ);
    if(!rag && targ.health > 0 && !targ.vr_corpse)
    {
        return FALSE;
    }
    entity oldSelf = self;
    self = attacker;
    entity b = VR_Blow(vr_hithand);
    float speed = b.mh_dmgspeed > 0 ? b.mh_dmgspeed : b.mh_speed; // m/s
    float held = max(0, VR_Weight_HeldMass(attacker, vr_hithand));
    self = oldSelf;
    float mass = held > 0 ? held + max(0, cvar_hget(cvarh_vr_box3d_weapon_arm_mass)) : max(0.1, cvar_hget(cvarh_vr_box3d_hand_mass));
    float bounce = min(1, max(0, cvar_hget(cvarh_vr_melee_ragdoll_bounce)));
    float most = max(0, cvar_hget(cvarh_vr_melee_ragdoll_most));
    vector dir = normalize(vr_hitdir);
    float m2u = VRMetersToUnits();
    if(rag)
    {
        vector at = vr_hitpos != '0 0 0' ? vr_hitpos : targ.origin;
        physicspush(targ, at, dir * speed * m2u, mass, bounce, most);
        if(cvar_hget(cvarh_vr_debug_shots))
        {
            dprint(sprintf("melee push: %s's ragdoll struck at %.1f m/s by %.1f kg (bounce %.2f, at most %.1f m/s)\n",
                targ.classname, speed, mass, bounce, most));
        }
        return TRUE;
    }
    float body = max(1, VR_Grapple_MonsterMass(targ));
    float dv = (1 + bounce) * speed * mass / (mass + body);
    if(most > 0)
    {
        dv = min(dv, most);
    }
    targ.velocity = targ.velocity + dir * dv * m2u;
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("melee push: %s's body (%.0f kg) struck at %.1f m/s by %.1f kg: %.2f m/s\n", targ.classname, body,
            speed, mass, dv));
    }
    return TRUE;
};

// Its melee damage multiplier: its own (Melee Damage x) times the weight's curve (weightdamage: heavy things more).
float(entity p, float xHand) VR_Weight_MeleeMult =
{
    entity c = VR_Carry_Held(p, xHand);
    if(c != world)
    {
        return propvalue(c, "melee_dmg") * weightdamage(propvalue(c, "mass"));
    }
    string model = WeaponIdToModel(VRGetEntWeapon(p, xHand), VRGetEntWeaponFlags(p, xHand));
    return weaponvalue(model, "w_meleedmg") * weightdamage(weaponvalue(model, "w_mass"));
};

// Heavy leniency: the factor on its strikes' speed thresholds (weightleniency: 1 but for things heavier than every
// weapon, which can't be swung as fast), times a weapon's own (Weapon Weights: Melee Speed Needed).
float(entity p, float xHand) VR_Weight_Leniency =
{
    float own = 1;
    if(VR_Carry_Held(p, xHand) == world)
    {
        own = weaponvalue(WeaponIdToModel(VRGetEntWeapon(p, xHand), VRGetEntWeaponFlags(p, xHand)), "w_meleemin");
        if(own <= 0)
        {
            own = 1; // (no such weapon: the empty hand's)
        }
    }
    return weightleniency(VR_Weight_HeldMass(p, xHand)) * own;
};

// Heavy handling (ROUND22.md, "Heavy weapons: struggle, droop, recoil, two-handed swings"): the weight class (weaponheft,
// 0 .. 1) of the weapon `xHand` of `p` holds in both hands; 0 held in one, or a carried thing.
float(entity p, float xHand) VR_Heavy_TwoHanded =
{
    if(!VRGet2HAiming(p) || VR_Carry_Held(p, xHand) != world)
    {
        return 0;
    }
    return weaponheft(WeaponIdToModel(VRGetEntWeapon(p, xHand), VRGetEntWeaponFlags(p, xHand)));
};

// Its factor on the speeds a blow needs: lower for a heavier weapon in both hands (vr_heavy_melee_2h at weight class 1),
// swung slower, deliberately (VR_Melee_Decide then asks for a smooth arc instead: meleearc).
float(float xHeavy) VR_Heavy_SpeedFactor =
{
    return 1 + (min(1, max(0.05, cvar("vr_heavy_melee_2h"))) - 1) * xHeavy;
};

// A weapon's own multiplier of how fast a blow's strength rises over the speed it needs (Weapon Weights: Melee Damage
// Curve); a prop's and the empty hand's 1.
float(entity p, float xHand) VR_Weight_MeleeCurve =
{
    if(VR_Carry_Held(p, xHand) != world)
    {
        return 1;
    }
    float own = weaponvalue(WeaponIdToModel(VRGetEntWeapon(p, xHand), VRGetEntWeaponFlags(p, xHand)), "w_meleecurve");
    return own > 0 ? own : 1;
};

// The least speed of a strike by what blow `b`'s hand holds (m/s): lower for a heavy thing.
float(entity b) VR_Melee_MinSpeedOf =
{
    return VR_Melee_MinSpeed() * (b.mh_lenient > 0 ? b.mh_lenient : 1);
};

// What `xHand` of `p` holds, as a striking thing.
float(entity p, float xHand) VR_Melee_Thing =
{
    if(VRHasBit(p.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_CLIMBING : QVR_VRBITS0_OFFHAND_CLIMBING))
    {
        return VR_MTHING_NONE; // a hand holding a ledge pulls the body
    }
    if(VR_FoeGrab_HandHolds(p, xHand))
    {
        return VR_MTHING_NONE; // a hand holding an enemy holds on (vr_foegrab.qc)
    }
    float wid = VRGetEntWeapon(p, xHand);
    if(wid == WID_FIST)
    {
        // A club carried in this hand alone (a wall torch: vr_walltorch.qc): its head strikes.
        if(VR_Melee_ClubOf(p, xHand) != world)
        {
            return VR_MTHING_CLUB;
        }
        // The empty hand steadying the other's weapon swings with it: the weapon's hand strikes.
        if(VRGet2HAiming(p) && !VRIsHandWeaponless(p, VRGetOtherHand(xHand)))
        {
            return VR_MTHING_NONE;
        }
        return VR_MTHING_FIST;
    }
    if(wid == WID_SWORD)
    {
        // AD support: the hunter ogres' hammer and mace strike with their heads, as an axe's (cutting nothing off, as
        // Mjolnir: vr_decap.qc VR_Decap_Blade; vr_ad_guns_defs.qc).
        // The death sergeant's flail: its head on its chain strikes (a body of its own: VR_Flail_Hit), its handle nothing.
        if(VR_ADMelee_Flail(VR_ADMelee_Kind(VRGetEntWeaponFlags(p, xHand))) && cvar_hget(cvarh_vr_ad_mounted))
        {
            return VR_MTHING_NONE;
        }
        return VR_ADMelee_Blunt(VR_ADMelee_Kind(VRGetEntWeaponFlags(p, xHand))) ? VR_MTHING_AXE : VR_MTHING_SWORD;
    }
    if(wid == WID_CROWBAR)
    {
        return VR_MTHING_CROWBAR;
    }
    if(wid == WID_AXE || wid == WID_MJOLNIR || wid == WID_SUPERAXE)
    {
        return VR_MTHING_AXE;
    }
    // The chainsaw: a melee weapon (the author's: NOTES.md vrfiringrange_2026-09-30_11-18-39), at a weapon's speeds, not a
    // gun's; its bar strikes as an axe's head, its whoosh leaves the engine's channel alone. (It was the axe's for a while:
    // two changes merged, the axe's branch first, and this one never ran.)
    if(wid == WID_CHAINSAW)
    {
        return VR_MTHING_SAW;
    }
    return VR_MTHING_GUN;
};

// The club `xHand` of `p` carries (world: none): a prop carried in that hand alone whose Held Object Offsets give it a
// melee tip (vr_props.inc: tip_x/y/z; butt_x/y/z its other end), into vr_mclub_tip and vr_mclub_butt (its model's axes).
vector vr_mclub_tip;
vector vr_mclub_butt;
entity(entity p, float xHand) VR_Melee_ClubOf =
{
    entity c = VR_Carry_Held(p, xHand);
    if(c == world || c.carry_player != p || c.carry_2h)
    {
        return world;
    }
    vr_mclub_tip_x = propvalue(c, "tip_x");
    vr_mclub_tip_y = propvalue(c, "tip_y");
    vr_mclub_tip_z = propvalue(c, "tip_z");
    if(vr_mclub_tip == '0 0 0')
    {
        return world;
    }
    vr_mclub_butt_x = propvalue(c, "butt_x");
    vr_mclub_butt_y = propvalue(c, "butt_y");
    vr_mclub_butt_z = propvalue(c, "butt_z");
    return c;
};

// Where the helping hand `xHelper` of `p` holds the weapon in its other hand (two-handed): the weapon's grip hotspot
// nearest it (a gun's foregrip or pump, a sword's handle or a point of its blade: the fitted hands' weaponhotspot, the
// local player's), else the hand itself.
vector(entity p, float xHelper) VR_Melee_HelpGrip =
{
    vector h = VRGetEntHandPos(p, xHelper);
    float holder = VRGetOtherHand(xHelper);
    vector best = h;
    float bestD = 24;
    float i;
    for(i = 0; i < 4; ++i)
    {
        vector at = weaponhotspot(p, holder, i);
        if(at != '0 0 0' && vlen(at - h) < bestD)
        {
            bestD = vlen(at - h);
            best = at;
        }
    }
    return best;
};

// How far behind the handle (a share of the way from the handle to the tip) the near end of a weapon held by its handle
// is: a sword's pommel, an axe's handle's end, a gun's butt.
float(float xThing) VR_Melee_NearBack =
{
    if(xThing == VR_MTHING_SWORD)
    {
        return 0.3; // the hand closes just under the guard; the grip and the pommel below it
    }
    if(xThing == VR_MTHING_AXE)
    {
        return 0.25; // the handle's end, below the hand
    }
    if(xThing == VR_MTHING_SAW)
    {
        return 0.1; // the rear handle's back (the model's origin is the handle's bar)
    }
    if(xThing == VR_MTHING_CROWBAR)
    {
        return VR_CROWBAR_NEAR; // the chisel's tip, below the tape (both hands' room)
    }
    return 0.3; // a gun: the butt behind the grip
};

// The blade's shape of what a hand holds by its Weapon Offsets slot (Edit Blade Shape; the engine's vr_blade.hpp, read
// through weapondrawnpose 7, admeleehead and admeleebend with -1 - slot: builtins.qc): its slot (-1: none, a fist, a
// club) and '<near end> <width> <flags>' (the near end's share behind the hand, -1 the kind's own; the swept points'
// radius's multiplier; 1 a curve of its own, 2 a head's box of its own). Every default keeps the shape as it was before
// (the kind's near end, the model's curve and head).
// '<blade start> <off> <side>' (Blade From, admeleehead 3): the share of the line where its striking part starts (-1 the
// kind's own: VR_Melee_Layout's first striking point) and that point off the line (its bend: VR_Melee_BendLine).
// The line's start off the hand and its tip off the muzzle (Blade Org, Blade Tip, admeleehead 4 and 5; flag 8): units along
// the hand's forward, right, up ('0 0 0': the hand and the muzzle).
float vr_mline_slot;
vector vr_mline_shape;
vector vr_mline_from;
vector vr_mline_org;
vector vr_mline_tipofs;
void(entity p, float xHand, float xThing) VR_Melee_ShapeOf =
{
    vr_mline_slot = -1;
    vr_mline_shape = '-1 1 0';
    vr_mline_from = '-1 0 0';
    vr_mline_org = vr_mline_tipofs = '0 0 0';
    if(xThing == VR_MTHING_FIST)
    {
        vr_mline_shape_y = max(0.1, cvar_hget(cvarh_vr_melee_fist_width)); // (its blow line's width: vr_melee_fist_width)
        return;
    }
    if(xThing == VR_MTHING_NONE || xThing == VR_MTHING_CLUB)
    {
        return;
    }
    vector s = weapondrawnpose(p, xHand, 7);
    vr_mline_slot = s_x;
    if(vr_mline_slot >= 0)
    {
        vr_mline_shape = admeleehead(-1 - vr_mline_slot, 2);
        if(vr_mline_shape_z & 4)
        {
            vr_mline_from = admeleehead(-1 - vr_mline_slot, 3);
        }
        if(vr_mline_shape_z & 8)
        {
            vr_mline_org = admeleehead(-1 - vr_mline_slot, 4);
            vr_mline_tipofs = admeleehead(-1 - vr_mline_slot, 5);
        }
    }
};

// The near end's share behind the handle of what the hand VR_Melee_ShapeOf last saw holds (`xThing`): its weapon's own
// (Blade Near End), else the kind's (VR_Melee_NearBack).
float(float xThing) VR_Melee_NearBackOf =
{
    return vr_mline_shape_x >= 0 ? vr_mline_shape_x : VR_Melee_NearBack(xThing);
};

// The head's box (shares of the line, as admeleehead's) of what the hand VR_Melee_Line last laid out holds (`xThing`), into
// vr_mhead_lo, _hi ('0 0 0': none): its weapon's own (Blade Head), else an AD hammer's or mace's made head. None carried off
// its handle.
vector vr_mhead_lo;
vector vr_mhead_hi;
void(entity p, float xHand, float xThing) VR_Melee_HeadOf =
{
    vr_mhead_lo = vr_mhead_hi = '0 0 0';
    if(vr_mline_free)
    {
        return;
    }
    if(vr_mline_slot >= 0 && (vr_mline_shape_z & 2))
    {
        vr_mhead_lo = admeleehead(-1 - vr_mline_slot, 0);
        vr_mhead_hi = admeleehead(-1 - vr_mline_slot, 1);
        return;
    }
    if(xThing == VR_MTHING_AXE)
    {
        float hk = VR_ADMelee_Kind(VRGetEntWeaponFlags(p, xHand));
        if(hk > 0 && VR_ADMelee_Blunt(hk))
        {
            vr_mhead_lo = admeleehead(hk, 0);
            vr_mhead_hi = admeleehead(hk, 1);
        }
    }
};

// The line of what `xHand` of `p` holds, into vr_mline_near, _grip, _far (world): FALSE for nothing to strike with.
vector vr_mline_near;
vector vr_mline_grip;
vector vr_mline_far;
// A weapon carried off its handle (VR_Melee_FreeLine; FALSE: held by its handle, or not a weapon): the near end and the
// far end are its two ends, the grip where the hand is on the line between them, and its points are laid out along all
// of it (VR_Melee_FreeLayout, from these).
float vr_mline_free;
float vr_mline_hs;   // the hand's share of the way from its near end (its pommel, butt) to its tip
float vr_mline_tip;  // the far end is its tip (the hand nearer its near end), else its near end
float vr_mline_gw;   // the hand's half width, a share of its length
float vr_mline_back; // its near end behind its handle (VR_Melee_NearBack)
// A curved blade (AD's death fury's sword: the engine's admeleebend, vr_admelee.cpp): its points (VR_Melee_At) follow
// the blade's spine off the straight line from the hand to its tip, so its sweeps are where the curved blade goes (the
// line alone cut across the curve's inside: it struck sooner than the blade, and missed with its outer bow). Its kind
// (0: straight) and, in the world, the line's normal in the blade's plane and the flat's normal, each the line's length.
float vr_mline_bend;
vector vr_mline_bend_p;
vector vr_mline_bend_y;

// The hand's half width along a weapon held anywhere on it (m): the points there are the hand's, they strike nothing.
float VR_MELEE_FREE_GRIP = 0.05;

// A weapon `xHand` of `p` carries off its handle (by a hotspot, the hand-off, anywhere on it: NOTES.md
// vrfiringrange_2026-10-01_22-35-54, a sword held upside down by its blade, struck with its hilt; _22-37-04, the rocket
// launcher and the laser cannon held backwards, struck with their butts) as the local player's view draws it
// (weapondrawnpose): its line from its near end, VR_Melee_NearBack behind its handle, to its tip. Before, the line was
// the hand's to the tip, as if the hand were on the handle: past the hand, away from the tip, only a little of it struck.
// FALSE: not known (another player; not drawn lately).
float(entity p, float xHand, float xThing, vector xHandPos) VR_Melee_FreeLine =
{
    if(weapondrawnpose(p, xHand, 2) == '0 0 0')
    {
        return FALSE;
    }
    // Its handle and tip as drawn in the hand, placed on the hand of this move (as drawn, the weapon's place lags the
    // hand by a frame: a thrust's far end stood still while the hand went on).
    vector lg = weapondrawnpose(p, xHand, 4);
    vector lt = weapondrawnpose(p, xHand, 5);
    makevectors(VRGetEntHandRot(p, xHand));
    vector g = xHandPos + v_forward * lg_x + v_right * lg_y + v_up * lg_z;
    vector t = xHandPos + v_forward * lt_x + v_right * lt_y + v_up * lt_z;
    float len = vlen(t - g);
    if(len < 2 || len > 120)
    {
        return FALSE;
    }
    vr_mline_back = VR_Melee_NearBackOf(xThing);
    vector e1 = g - (t - g) * vr_mline_back;
    vector ax = t - e1;
    float l2 = ax * ax;
    vr_mline_hs = min(1, max(0, ((xHandPos - e1) * ax) / l2));
    vr_mline_grip = e1 + ax * vr_mline_hs;
    vr_mline_tip = (vr_mline_hs <= 0.5);
    if(vr_mline_tip)
    {
        vr_mline_far = t;
        vr_mline_near = e1;
    }
    else
    {
        vr_mline_far = e1;
        vr_mline_near = t;
    }
    vr_mline_gw = VR_MELEE_FREE_GRIP * VRMetersToUnits() / sqrt(l2);
    vr_mline_free = TRUE;
    return TRUE;
};

// A sword held by its handle along `xLine` (the hand to its tip, world): if it is a curved AD sword (admeleebend), its
// bend's axes into vr_mline_bend_*. The model's +y (the blade's flat's normal: the knight's sword's slot draws it with no
// roll) is the hand's left; a mirrored model (the off hand's) keeps the bend, which lies in the blade's plane.
// A weapon whose slot has a curve of its own (Blade Spine: VR_Melee_ShapeOf's flags) bends along it instead, whatever it
// is (vr_mline_bend -1 - its slot: VR_Melee_At's admeleebend reads its curve).
void(entity p, float xHand, float xThing, vector xLine) VR_Melee_BendLine =
{
    if(!cvar_hget(cvarh_vr_melee_curved_blades))
    {
        return;
    }
    float kind = xThing == VR_MTHING_SWORD ? VR_ADMelee_Kind(VRGetEntWeaponFlags(p, xHand)) : 0;
    if(vr_mline_slot >= 0 && (vr_mline_shape_z & 5)) // (a curve or a blade start of its own: admeleebend adds them)
    {
        kind = -1 - vr_mline_slot;
    }
    else if(kind <= 0)
    {
        return;
    }
    else
    {
        vector curved = admeleebend(kind, 0.5);
        if(curved_z == 0)
        {
            return;
        }
    }
    vector oldF = v_forward, oldR = v_right, oldU = v_up;
    makevectors(VRGetEntHandRot(p, xHand));
    float len = vlen(xLine);
    vector a = xLine * (1 / max(0.001, len));
    vector y = v_right * -1;
    y = normalize(y - a * (y * a));
    vr_mline_bend = kind;
    vr_mline_bend_y = y * len;
    vector pn;
    pn_x = y_y * a_z - y_z * a_y; // (y x a: the model's +y crossed with the line, admelee::bend's normal)
    pn_y = y_z * a_x - y_x * a_z;
    pn_z = y_x * a_y - y_y * a_x;
    vr_mline_bend_p = normalize(pn) * len;
    v_forward = oldF;
    v_right = oldR;
    v_up = oldU;
};

// The model's +y of what `xHand` of `p` holds along `xLine` (VR_Melee_BendLine's: the hand's left, square to the line;
// unit), for a heavy head's frame (VR_Melee_Ribs).
vector(entity p, float xHand, vector xLine) VR_Melee_ModelY =
{
    vector oldF = v_forward, oldR = v_right, oldU = v_up;
    makevectors(VRGetEntHandRot(p, xHand));
    vector a = normalize(xLine);
    vector y = v_right * -1;
    y = normalize(y - a * (y * a));
    v_forward = oldF;
    v_right = oldR;
    v_up = oldU;
    return y;
};

float(entity p, float xHand, float xThing) VR_Melee_Line =
{
    vector h = VRGetEntHandPos(p, xHand);
    vr_mline_grip = h;
    vr_mline_free = FALSE;
    vr_mline_bend = 0;
    VR_Melee_ShapeOf(p, xHand, xThing);
    if(xThing == VR_MTHING_NONE)
    {
        return FALSE;
    }
    if(xThing == VR_MTHING_FIST)
    {
        // Its blow line (vr_melee_fist_*: the two ends in the hand's frame, edited by Edit Blade Shape with the hand
        // empty): the layout's fist (0) to its knuckles (1), the near end a quarter of it behind. The defaults are the
        // hand to 4 units ahead (the near end 1 behind), as exactly as before.
        vector hr = VRGetEntHandRot(p, xHand);
        vector fwd = VRForwardOf(hr);
        vector fa, fb;
        fa_x = cvar_hget(cvarh_vr_melee_fist_from_x);
        fa_y = cvar_hget(cvarh_vr_melee_fist_from_y);
        fa_z = cvar_hget(cvarh_vr_melee_fist_from_z);
        fb_x = cvar_hget(cvarh_vr_melee_fist_to_x);
        fb_y = cvar_hget(cvarh_vr_melee_fist_to_y);
        fb_z = cvar_hget(cvarh_vr_melee_fist_to_z);
        if(xHand == cVR_OffHand && cvar_hget(cvarh_vr_melee_fist_mirror))
        {
            fa_y = -fa_y;
            fb_y = -fb_y;
        }
        vector fn = fa - (fb - fa) * 0.25;
        vr_mline_grip = h + fwd * fa_x;
        vr_mline_far = h + fwd * fb_x;
        vr_mline_near = h + fwd * fn_x;
        if(fa_y || fa_z || fb_y || fb_z)
        {
            vector oldF = v_forward, oldR = v_right, oldU = v_up;
            makevectors(hr);
            vr_mline_grip = vr_mline_grip + v_right * fa_y + v_up * fa_z;
            vr_mline_far = vr_mline_far + v_right * fb_y + v_up * fb_z;
            vr_mline_near = vr_mline_near + v_right * fn_y + v_up * fn_z;
            v_forward = oldF;
            v_right = oldR;
            v_up = oldU;
        }
        return TRUE;
    }
    if(xThing == VR_MTHING_CLUB)
    {
        // Its tip and butt where the hand holds it now (as VR_Carry_HandFrame places it: its place and turn in the hand),
        // not where it was put last frame.
        entity c = VR_Melee_ClubOf(p, xHand);
        if(c == world)
        {
            return FALSE;
        }
        vector handRot = VRGetEntHandRot(p, xHand);
        vector ang = carryangles(c, handRot, FALSE);
        makevectors(handRot);
        vector org = h + v_forward * c.carry_offset_x + v_right * c.carry_offset_y + v_up * c.carry_offset_z;
        vr_mline_far = org + modelpoint(c, ang, vr_mclub_tip);
        vr_mline_near = vr_mclub_butt != '0 0 0' ? org + modelpoint(c, ang, vr_mclub_butt) : h - (vr_mline_far - h) * 0.25;
        return TRUE;
    }
    // Carried off its handle (by a hotspot, the hand-off, anywhere on it): all of it, wherever the hand is on it.
    if(VRIsHandCarryingByForegrip(p, xHand) && VR_Melee_FreeLine(p, xHand, xThing, h))
    {
        return TRUE;
    }
    vector m = xHand == cVR_OffHand ? p.offmuzzlepos : p.muzzlepos;
    if(m == '0 0 0' || vlen(m - h) > 80 || vlen(m - h) < 2)
    {
        m = h + VRForwardOf(VRGetEntHandRot(p, xHand)) * 8;
    }
    if(vr_mline_org != '0 0 0' || vr_mline_tipofs != '0 0 0')
    {
        // Its line's own ends (Edit Blade Shape's Line Start, a gun's Tip): off the hand and the muzzle in the hand's
        // frame, its right mirrored in the off hand (as the model is drawn).
        vector oldF = v_forward, oldR = v_right, oldU = v_up;
        makevectors(VRGetEntHandRot(p, xHand));
        float side = xHand == cVR_OffHand ? -1 : 1;
        vr_mline_grip = h + v_forward * vr_mline_org_x + v_right * (vr_mline_org_y * side) + v_up * vr_mline_org_z;
        m = m + v_forward * vr_mline_tipofs_x + v_right * (vr_mline_tipofs_y * side) + v_up * vr_mline_tipofs_z;
        v_forward = oldF;
        v_right = oldR;
        v_up = oldU;
        if(vlen(m - vr_mline_grip) < 2)
        {
            m = vr_mline_grip + VRForwardOf(VRGetEntHandRot(p, xHand)) * 8;
        }
    }
    vector a = m - vr_mline_grip;
    vr_mline_far = m;
    VR_Melee_BendLine(p, xHand, xThing, a);
    float back = VR_Melee_NearBackOf(xThing);
    if(xThing == VR_MTHING_GUN && VRIsHandCarryingByForegrip(p, xHand))
    {
        back = 0.8; // held by its pump: the stock is well behind the hand
    }
    vr_mline_near = vr_mline_grip - a * back;
    return TRUE;
};

// A point of the layout (a macro: the constant index is a plain global).
#define VR_MPT(xIndex, xF, xKind, xName) \
    vr_mpt_f[xIndex] = xF;     vr_mpt_fh[xIndex] = xF; \
    vr_mpt_kind[xIndex] = xKind; \
    vr_mpt_name[xIndex] = xName; \
    vr_mpt_n = xIndex + 1

// The points of a thing (see the top): the whole blade strikes, from the guard to the tip.
void(float xThing) VR_Melee_Layout =
{
    vr_mpt_n = 0;
    if(xThing == VR_MTHING_FIST)
    {
        VR_MPT(0, 0, VR_MPT_FIST, "the fist");
        VR_MPT(1, 1, VR_MPT_FIST, "the knuckles");
    }
    else if(xThing == VR_MTHING_SWORD)
    {
        VR_MPT(0, -1, VR_MPT_NEAR, "the pommel");
        VR_MPT(1, 0, VR_MPT_GRIP, "the hilt");
        VR_MPT(2, 0.12, VR_MPT_GRIP, "the guard");
        VR_MPT(3, 0.27, VR_MPT_EDGE, "the blade's root");
        VR_MPT(4, 0.42, VR_MPT_EDGE, "mid-blade");
        VR_MPT(5, 0.57, VR_MPT_EDGE, "mid-blade");
        VR_MPT(6, 0.72, VR_MPT_EDGE, "the blade");
        VR_MPT(7, 0.86, VR_MPT_EDGE, "the blade");
        VR_MPT(8, 1, VR_MPT_EDGE, "the tip");
    }
    else if(xThing == VR_MTHING_AXE)
    {
        VR_MPT(0, -1, VR_MPT_NEAR, "the handle's end");
        VR_MPT(1, 0, VR_MPT_GRIP, "the handle");
        VR_MPT(2, 0.45, VR_MPT_GRIP, "the handle");
        VR_MPT(3, 0.75, VR_MPT_EDGE, "the head");
        VR_MPT(4, 1, VR_MPT_EDGE, "the head");
    }
    else if(xThing == VR_MTHING_CLUB)
    {
        // (A wall torch: its head is most of what is past the hand.)
        VR_MPT(0, -1, VR_MPT_NEAR, "the club's end");
        VR_MPT(1, 0, VR_MPT_GRIP, "the handle");
        VR_MPT(2, 0.3, VR_MPT_EDGE, "the head");
        VR_MPT(3, 0.55, VR_MPT_EDGE, "the head");
        VR_MPT(4, 0.8, VR_MPT_EDGE, "the head");
        VR_MPT(5, 1, VR_MPT_EDGE, "the head's top");
    }
    else if(xThing == VR_MTHING_SAW)
    {
        // The engine block (0 .. 0.55 of the way to the tip: vr_chainsaw.qc VR_SAW_BAR_START) is where the hands are;
        // the bar strikes, as an axe's head.
        VR_MPT(0, -1, VR_MPT_NEAR, "the rear handle");
        VR_MPT(1, 0, VR_MPT_GRIP, "the grip");
        VR_MPT(2, 0.3, VR_MPT_GRIP, "the engine");
        VR_MPT(3, 0.55, VR_MPT_EDGE, "the bar's root");
        VR_MPT(4, 0.78, VR_MPT_EDGE, "the bar");
        VR_MPT(5, 1, VR_MPT_EDGE, "the bar's tip");
    }
    else if(xThing == VR_MTHING_CROWBAR)
    {
        // The hand's point at the tape's top (its fist below it on the tape); the bar past it strikes (a blunt steel bar
        // all along, as a gun's barrel), the hook at its end hardest (VR_MELEE_CROWBAR_HOOK); the far end is the hook's
        // back, on the bar's line (the claw curls out ahead of it, the way a chop goes).
        VR_MPT(0, -1, VR_MPT_NEAR, "the chisel end");
        VR_MPT(1, 0, VR_MPT_GRIP, "the grip");
        VR_MPT(2, 0.2, VR_MPT_EDGE, "the bar");
        VR_MPT(3, 0.38, VR_MPT_EDGE, "the bar");
        VR_MPT(4, 0.56, VR_MPT_EDGE, "the bar");
        VR_MPT(5, 0.73, VR_MPT_EDGE, "the bar");
        VR_MPT(6, VR_CROWBAR_HOOK_F, VR_MPT_EDGE, "the hook");
        VR_MPT(7, 1, VR_MPT_EDGE, "the hook");
    }
    else if(xThing == VR_MTHING_GUN)
    {
        VR_MPT(0, -1, VR_MPT_NEAR, "the butt");
        VR_MPT(1, 0, VR_MPT_GRIP, "the grip");
        VR_MPT(2, 0.5, VR_MPT_EDGE, "the barrel");
        VR_MPT(3, 1, VR_MPT_EDGE, "the muzzle");
    }
};

// The name of the part of a weapon of `xThing` at `xFh` (its handle 0, its tip 1) and whether it is blunt there (into
// vr_mpart_blunt: VR_MPT_BLUNT, held anywhere on it): as VR_Melee_Layout's points.
float vr_mpart_blunt;
string(float xThing, float xFh) VR_Melee_PartAt =
{
    vr_mpart_blunt = FALSE;
    if(xThing == VR_MTHING_SWORD)
    {
        vr_mpart_blunt = (xFh < 0.2);
        return xFh < -0.15 ? "the pommel" : (xFh < 0.06 ? "the hilt" : (xFh < 0.2 ? "the guard" : (xFh > 0.93 ? "the tip" : "the blade")));
    }
    if(xThing == VR_MTHING_AXE)
    {
        vr_mpart_blunt = (xFh < 0.6);
        return xFh < -0.12 ? "the handle's end" : (xFh < 0.6 ? "the handle" : "the head");
    }
    if(xThing == VR_MTHING_SAW)
    {
        vr_mpart_blunt = (xFh < 0.45);
        return xFh < -0.05 ? "the rear handle" : (xFh < 0.45 ? "the engine" : (xFh > 0.93 ? "the bar's tip" : "the bar"));
    }
    if(xThing == VR_MTHING_CROWBAR)
    {
        vr_mpart_blunt = (xFh < -0.6);
        return xFh < -0.6 ? "the chisel end" : (xFh < VR_CROWBAR_HOOK_F ? "the bar" : "the hook");
    }
    vr_mpart_blunt = (xFh < -0.1);
    return xFh < -0.1 ? "the butt" : (xFh < 0.1 ? "the grip" : (xFh < 0.9 ? "the barrel" : "the muzzle"));
};

// The corpse (or a body still dying) the thing of `xThing` strikes along the line VR_Melee_Line left (vr_mline_*; a test
// sets them itself), going `xDir` (combat.qc VR_Corpse_StrikeFrame; `p` the player, whom the traces skip): from the hand
// out to its far end and back to its near end, a little ahead along the swing, all of it as the living's blows have it
// (a weapon carried off its handle, a sword by its blade, end to end; a pommel behind the hand; a club's head). World for
// none; where into vr_cstrike_at; vr_cstrike_blunt: a sword's or an axe's blunt part (VR_Melee_PartAt: its pommel, hilt,
// guard, an axe's handle). (Clubs, guns, the crowbar and fists are blunt all along: VR_Corpse_DamageKind's own.)
entity(entity p, float xThing, vector xDir) VR_Corpse_StrikeAlong =
{
    vr_cstrike_blunt = FALSE;
    vector lead = xDir * 4;
    vector grip = vr_mline_grip;
    entity c = world;
    traceline(grip, vr_mline_far + lead, MOVE_HITGIBS | MOVE_HITMODEL_MELEE, p);
    if(trace_ent.vr_corpse == 2 || trace_ent.vr_corpse_dying)
    {
        c = trace_ent;
    }
    else
    {
        traceline(grip, vr_mline_near + lead, MOVE_HITGIBS | MOVE_HITMODEL_MELEE, p);
        if(trace_ent.vr_corpse == 2 || trace_ent.vr_corpse_dying)
        {
            c = trace_ent;
        }
    }
    if(c == world)
    {
        return world;
    }
    vr_cstrike_at = trace_endpos;
    if(xThing == VR_MTHING_SWORD || xThing == VR_MTHING_AXE)
    {
        // Its handle and its tip: the part struck as a share of the way from one to the other (VR_Melee_PartAt's).
        vector h = grip;
        vector t = vr_mline_far;
        if(vr_mline_free)
        {
            vector e1 = vr_mline_tip ? vr_mline_near : vr_mline_far; // its near end (the pommel)
            t = vr_mline_tip ? vr_mline_far : vr_mline_near;
            h = (e1 + t * vr_mline_back) * (1 / (1 + vr_mline_back)); // (e1 = h - (t - h) * back)
        }
        vector ax = t - h;
        float fh = ((vr_cstrike_at - h) * ax) / max(0.001, ax * ax);
        vr_cstrike_fh = fh;
        string part = VR_Melee_PartAt(xThing, fh);
        vr_cstrike_blunt = vr_mpart_blunt;
        if(cvar_hget(cvarh_developer) >= 2)
        {
            dprint(sprintf("corpse strike: %s with %s (%.2f of the way from the handle to the tip)%s\n", c.classname, part, fh,
                vr_cstrike_blunt ? ", blunt" : ""));
        }
    }
    return c;
};

entity(entity p, float xHand, vector xDir) VR_Corpse_StrikeLine =
{
    float thing = VR_Melee_Thing(p, xHand);
    if(!VR_Melee_Line(p, xHand, thing))
    {
        vr_cstrike_blunt = FALSE;
        return world;
    }
    return VR_Corpse_StrikeAlong(p, thing, xDir);
};

// The points of a weapon carried off its handle (VR_Melee_FreeLine): all of it, evenly from its near end to its tip (as a
// prop held anywhere is all of it), each as VR_Melee_PartAt has it; those at the hand strike nothing. Their place on the
// line (vr_mpt_f) is from the grip (the hand) to the far end, which may be either end: past the hand, away from the far
// end, they are negative.
void(float xThing, float xHs, float xTip, float xGw, float xBack) VR_Melee_FreeLayout =
{
    float i;
    for(i = 0; i < 9; ++i)
    {
        float sh = i / 8;
        float fh = sh * (1 + xBack) - xBack;
        vr_mpt_f[i] = (sh - xHs) / ((xTip ? 1 : 0) - xHs);
        vr_mpt_fh[i] = fh;
        vr_mpt_name[i] = VR_Melee_PartAt(xThing, fh);
        if(fabs(sh - xHs) <= xGw)
        {
            vr_mpt_kind[i] = VR_MPT_GRIP;
        }
        else
        {
            vr_mpt_kind[i] = vr_mpart_blunt ? VR_MPT_BLUNT : VR_MPT_EDGE;
        }
    }
    vr_mpt_n = 9;
};

// The layout's points moved for a blade start of its own at share `xFrom` (Blade From, Edit Blade Shape): its striking
// part (from its first striking point to the tip) laid from `xFrom` to the tip, the points before it (the handle's)
// squeezed or stretched into the hand .. `xFrom`; the near end (-1) and the hand (0) stay. Their names and parts stay.
void(float xFrom) VR_Melee_LayoutFrom =
{
    if(xFrom <= 0 || xFrom >= 1)
    {
        return;
    }
    float i;
    float f0 = -1;
    for(i = 0; i < vr_mpt_n && f0 < 0; ++i)
    {
        if(VR_Melee_Strikes(vr_mpt_kind[i]) && vr_mpt_f[i] > 0 && vr_mpt_f[i] < 1)
        {
            f0 = vr_mpt_f[i];
        }
    }
    if(f0 <= 0)
    {
        return;
    }
    for(i = 0; i < vr_mpt_n; ++i)
    {
        float f = vr_mpt_f[i];
        if(f > 0 && f < 1)
        {
            vr_mpt_f[i] = f >= f0 ? xFrom + (f - f0) * (1 - xFrom) / (1 - f0) : f * xFrom / f0;
        }
    }
};

// The points of what `b`'s hand holds (as it was last tracked).
void(entity b) VR_Melee_LayoutOf =
{
    if(b.mh_free)
    {
        VR_Melee_FreeLayout(b.mh_thing, b.mh_free_hs, b.mh_free_tip, b.mh_free_gw, b.mh_free_back);
    }
    else
    {
        VR_Melee_Layout(b.mh_thing);
        VR_Melee_LayoutFrom(b.mh_bfrom);
    }
};

// A point's place on the line (the layout's -1: the near end; laid out along all of a weapon held anywhere, where the
// line goes on past the grip).
vector(float xF) VR_Melee_At =
{
    if(xF < 0 && !vr_mline_free)
    {
        return vr_mline_near;
    }
    vector at = vr_mline_grip + (vr_mline_far - vr_mline_grip) * xF;
    if(vr_mline_bend && xF > 0)
    {
        vector k = admeleebend(vr_mline_bend, xF); // (a curved blade: on its spine; below 0 its weapon's own curve)
        at = at + vr_mline_bend_p * k_x + vr_mline_bend_y * k_y;
    }
    return at;
};

// `xVec` of the world turned into the play space (the player's turning undone). The turn's cosine and sine are kept
// for the last yaw (it changes only as the player turns).
float vr_ps_set;
float vr_ps_yaw;
float vr_ps_cos;
float vr_ps_sin;
vector(vector xVec) VR_Blow_ToPlaySpace =
{
    if(self.vryaw != vr_ps_yaw || !vr_ps_set)
    {
        float a = -self.vryaw * (3.14159265 / 180);
        vr_ps_cos = cos(a);
        vr_ps_sin = sin(a);
        vr_ps_yaw = self.vryaw;
        vr_ps_set = TRUE;
    }
    vector r;
    r_x = xVec_x * vr_ps_cos - xVec_y * vr_ps_sin;
    r_y = xVec_x * vr_ps_sin + xVec_y * vr_ps_cos;
    r_z = xVec_z;
    return r;
};

// The play space's way back to the world.
vector(vector xVec) VR_Blow_ToWorld =
{
    VR_Blow_ToPlaySpace('0 0 0'); // (the turn's cosine and sine for this yaw)
    vector r;
    r_x = xVec_x * vr_ps_cos + xVec_y * vr_ps_sin;
    r_y = -xVec_x * vr_ps_sin + xVec_y * vr_ps_cos;
    r_z = xVec_z;
    return r;
};

// The head's facing, flat, in the play space.
vector() VR_Blow_Facing =
{
    return VRForwardOf('0 1 0' * (self.v_viewangle_y - self.vryaw));
};

// Where `xHand`'s palm faces (the main hand's palm is its left, the off hand's its right, as held).
vector(float xHand) VR_Melee_Palm =
{
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;
    makevectors(VRGetHandRot(xHand));
    vector palm = xHand == cVR_OffHand ? v_right : v_right * -1;
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return palm;
};

// Whether `xHand` of `p` holds the flashlight (the engine's QVR_VRBITS0_*HAND_BUSY). The torch is the client's: the
// game sees an empty hand whose grip is not held (the torch takes the grip's press, vr_flashlight.cpp), but the hand is
// a fist closed round the torch.
float(entity p, float xHand) VR_Melee_HoldsTorch =
{
    return VRHasBit(p.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_BUSY : QVR_VRBITS0_OFFHAND_BUSY);
};

// Whether `xHand` of `self` is a closed fist: the grip held, or closed round the flashlight.
float(float xHand) VR_Melee_ClosedFist =
{
    return VRIsEntHandGrabbingBit(self, xHand) || VR_Melee_HoldsTorch(self, xHand);
};

// Whether `xHand` of `self` is an open hand: empty, the grip not held, not holding the flashlight, not steadying the
// other's weapon, not carrying or climbing.
float(float xHand) VR_Melee_OpenHand =
{
    if(!VRIsHandEmpty(self, xHand) || VR_Melee_ClosedFist(xHand))
    {
        return FALSE;
    }
    if(VRGet2HAiming(self) && !VRIsHandWeaponless(self, VRGetOtherHand(xHand)))
    {
        return FALSE; // steadying the other's weapon
    }
    return !VRHasBit(self.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_CLIMBING : QVR_VRBITS0_OFFHAND_CLIMBING) &&
           !VR_FoeGrab_HandHolds(self, xHand);
};

// Where `xHand`'s grip is from its shoulder (play space, normalized; `xFwd`: the facing, play space): a shove extends the
// arm along it (VR_Bash_Extends), a slap goes across it (VR_Melee_Slaps).
vector(float xHand, entity b, vector xFwd) VR_Melee_ArmOut =
{
    vector right = '0 0 0';
    right_x = xFwd_y;
    right_y = -xFwd_x;
    float side = VR_SHOULDER_SIDE;
    if(xHand == cVR_OffHand) // (the left controller, left-handed too)
    {
        side = -side;
    }
    return normalize(b.mh_rgrip - (right * side - '0 0 1' * VR_SHOULDER_DOWN));
};

// Whether blow `b`'s grip is in a liquid (water, slime, lava): a hand under water swims (vr_physics.cpp
// VR_AfterWaterMove, which gives the strokes their sound): an open hand there slaps nothing (VR_Melee_Slaps), and no
// swing whooshes there (VR_Melee_Whoosh: no air to swish); a closed fist's punch still lands, with its sound
// (vrfiringrange_2026-10-08_22-26-37).
float(entity b) VR_Melee_HandUnderwater =
{
    float c = pointcontents(b.mh_grip);
    return c == CONTENT_WATER || c == CONTENT_SLIME || c == CONTENT_LAVA;
};

// SLAPS (vr_melee_slap; ROUND21.md, "Slaps"): whether `xHand`'s open hand, swung as fast as a punch (VR_Melee_Decide),
// slaps: an open hand (VR_Melee_OpenHand) not in a shove's push (mh_pushing: VR_Bash), going across the body (within
// 60 degrees of sideways: VR_SLAP_ACROSS) and not out from its shoulder (VR_SLAP_RADIAL, below a shove's
// VR_BASH_PALM_RADIAL: an open palm pushed out ahead, with one hand or both, is always a shove, never a slap).
float(float xHand, entity b) VR_Melee_Slaps =
{
    if(!cvar_hget(cvarh_vr_melee_slap) || b.mh_pushing || !VR_Melee_OpenHand(xHand) || VR_Melee_HandUnderwater(b))
    {
        return FALSE;
    }
    vector v = b.mh_vgrip;
    float sp = vlen(v);
    if(sp <= 0)
    {
        return FALSE;
    }
    vector fwd = VR_Blow_Facing();
    vector right = '0 0 0';
    right_x = fwd_y;
    right_y = -fwd_x;
    float across = fabs(v * right) / sp;
    float outward = (v * VR_Melee_ArmOut(xHand, b, fwd)) / sp;
    if(cvar_hget(cvarh_developer) >= 2)
    {
        dprint(sprintf("melee debug: hand %g open, %.1f m/s, across %.2f out %.2f\n", xHand, sp, across, outward));
    }
    return across >= VR_SLAP_ACROSS && outward < VR_SLAP_RADIAL;
};

// Prints a melee event for the logs and the motion evaluation (developer): "melee event: <what> ...".
void(string xWhat) VR_Melee_Event =
{
    dprint("melee event: ");
    dprint(xWhat);
    dprint("\n");
};
.float vr_motion_pushseen; // on a player: the last frame of a push (the motion recorder's "push" event: once a push)

// ----------------------------------------------------------------------------
// Following a hand

// Takes `xHand`'s new pose (once a frame, PlayerVRMelee): the points swept from the last pose to this one, the line's
// velocities. FALSE when there is no new pose (a server frame between two client ones: the next pose's sweep spans
// both) or nothing to sweep from (the first pose, a jump, another weapon).
// Where the hand of `p` turns about when it wiggles (world): VR_MELEE_WRIST_BACK behind the controller along its own
// axis. The hand's angles carry the gun angle (vr_gunangle/vr_gunyaw, vr_offhandpitch/vr_offhandyaw: the engine's
// withHandOffsets turns the controller by the yaw, then by minus the pitch): taken out here. The rest of the hand
// calibration (its move and roll, hands::calibration) is the real hand's place on the controller: the wrist goes with
// the hand, as it is here. The off hand's pitch and yaw are the main hand's mirrored with vr_handcal_off_mirror. A
// weapon held two-handed turns with the hands' line, not with one controller: its hand itself.
vector(entity p, float xHand) VR_Melee_Wrist =
{
    vector h = VRGetEntHandPos(p, xHand);
    if(VRGet2HAiming(p))
    {
        return h;
    }
    float ownOff = xHand == cVR_OffHand && !cvar_hget(cvarh_vr_handcal_off_mirror);
    float pitch = cvar_hget(ownOff ? cvarh_vr_offhandpitch : cvarh_vr_gunangle) * 0.0174533;
    float yaw = cvar_hget(ownOff ? cvarh_vr_offhandyaw : cvarh_vr_gunyaw) * 0.0174533;
    if(xHand == cVR_OffHand && !ownOff)
    {
        yaw = -yaw;
    }
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;
    makevectors(VRGetEntHandRot(p, xHand));
    float d = VR_MELEE_WRIST_BACK * VRMetersToUnits();
    float cy = cos(yaw);
    vector w = h - v_right * (d * sin(yaw)) - v_up * (d * cy * sin(pitch)) - v_forward * (d * cy * cos(pitch));
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return w;
};

// The player's own clock: `time`, ahead by the engine's vr_player_time_offset (slow motion's Sandevistan, where the
// player runs at full speed in a slowed world; 0 otherwise). Its blows, rearming, pommel wait, bash stance and cooldown
// are timed on it.
float() VR_PlayerTime =
{
    return time + vr_player_time_offset;
};

// A point of the line's velocity about the grip (`xR0` to `xR1`, from the grip, over `xSpan` s), as its chord over
// VR_MELEE_VEL_SPAN would read it. A chord's way is the arc's at the chord's middle: over a longer span (a tick's pose
// two headset frames on, a dropped frame) it points further back into the turn (a blade's tip swung round read as drawn
// back along the line: VR_MELEE_PULLBACK) and reads slower. Turning uniformly about the grip (the line is rigid), the
// chord is turned on to the window's middle and scaled to its length: every span reads as a 72 Hz tick's chord did.
// The strength's (and the damage's) speed: the measured speed times this. Measured over the poses' times (meleefps,
// 2026-10-19) a blow's speed is its own; before, at the author's 120 Hz, the ticks beating against the frames read the
// first hit's speed 1.15-1.2 times too fast on average, and his blows' strength was tuned on those. The gates (land,
// slow, wiggle) read the true speed; only how hard a landed blow strikes keeps the old feel.
float VR_MELEE_STRENGTH_SPEED = 1.13;
float VR_MELEE_VEL_SPAN = 0.013889; // s (a 72 Hz tick: at 72 Hz every chord is this, as before)
vector(vector xR0, vector xR1, float xSpan) VR_Melee_TurnVel =
{
    vector chord = (xR1 - xR0) * (1 / xSpan);
    vector n = VR_Cross(xR0, xR1);
    float sn = vlen(n);
    float th = atan2(sn, xR0 * xR1);
    if(sn < 0.000001 || th < 0.001)
    {
        return chord;
    }
    n = n * (1 / sn);
    float k = VR_MELEE_VEL_SPAN / xSpan;
    float a = th * 0.5 * (1 - k);
    float hw = min(th * k * 0.5, 1.5);
    float hs = th * 0.5;
    chord = chord * (sin(hw) / hw) * (hs / sin(hs));
    return chord * cos(a) + VR_Cross(n, chord) * sin(a) + n * (n * chord) * (1 - cos(a));
};

float(float xHand, entity b) VR_Melee_Track =
{
    // The pose is the client's as this server frame ends (time + frametime): QC's time is still the frame's start. On
    // the player's own clock (the engine's vr_player_time_offset: 0 but in slow motion's Sandevistan, where the player
    // runs at full speed in a slowed world, and frametime is the player's), so its blows' speeds are its own.
    float now = time + vr_player_time_offset + frametime;
    float dt = now - b.mh_time;
    // Over the poses' own times when the client sends them (.vr_posetime, the headset's frames' times): the server's
    // 72 Hz ticks beat against the headset's frames, so a tick's pose is one or two frames newer than the last tick's
    // (at 90 Hz 1/90 or 2/90 s for the tick's 1/72: a steady swing read at 0.8 and 1.6 times its speed by turns; a
    // dropped frame likewise). Measured over the poses' times every headset rate reads a swing's own speed.
    float ptime = self.vr_posetime;
    if(ptime >= 0 && b.mh_ptime >= 0)
    {
        float pdt = ptime - b.mh_ptime;
        if(pdt < -0.5 * VR_POSE_CLOCK_WRAP)
        {
            pdt = pdt + VR_POSE_CLOCK_WRAP;
        }
        if(pdt == 0 && dt > 0 && dt < 0.05)
        {
            return FALSE; // (the same frame's pose again: no news, not a stop)
        }
        if(pdt > 0 && pdt <= 0.25 && dt > 0 && dt <= 0.25)
        {
            dt = pdt;
        }
    }
    float thing = VR_Melee_Thing(self, xHand);
    float wid = VRGetEntWeapon(self, xHand);
    if(!VR_Melee_Line(self, xHand, thing) || !VRHandsTracked(self))
    {
        b.mh_n = 0;
        b.mh_thing = thing;
        b.mh_free = FALSE;
        b.mh_heavy2h = VR_Heavy_TwoHanded(self, xHand);
        b.mh_lenient = VR_Weight_Leniency(self, xHand) * VR_Heavy_SpeedFactor(b.mh_heavy2h);
        b.mh_curve = VR_Weight_MeleeCurve(self, xHand);
        b.mh_vnear = b.mh_vgrip = b.mh_vfar = '0 0 0';
        b.mh_fast = 0;
        b.mh_time = now;
        b.mh_ptime = ptime;
        return FALSE;
    }

    float m2u = VRMetersToUnits();
    vector head = VRGetHeadPos(self);
    vector rnear = VR_Blow_ToPlaySpace(vr_mline_near - head) * (1 / m2u);
    vector rgrip = VR_Blow_ToPlaySpace(vr_mline_grip - head) * (1 / m2u);
    vector rfar = VR_Blow_ToPlaySpace(vr_mline_far - head) * (1 / m2u);
    vector rwrist = VR_Blow_ToPlaySpace(VR_Melee_Wrist(self, xHand) - head) * (1 / m2u);
    vector vwrist = '0 0 0';

    // The same pose again: no news, not a stop.
    if(rgrip == b.mh_rgrip && rfar == b.mh_rfar && dt > 0 && dt < 0.05 && thing == b.mh_thing)
    {
        return FALSE;
    }

    float same = b.mh_n > 0 && thing == b.mh_thing && wid == b.mh_wid && dt > 0 && dt <= 0.1;
    // (Taken anywhere, let go of its handle, the far end the other end: its points are other points.)
    if(vr_mline_free != b.mh_free || (vr_mline_free && (vr_mline_tip != b.mh_free_tip || fabs(vr_mline_hs - b.mh_free_hs) > 0.05)))
    {
        same = FALSE;
    }
    // What the hand holds jumped, not the hand: put back in it at once by the weight (left too far behind it), or its far
    // end jumping (a weapon taken). No speed of its points from the jump and no blow, but the hand's own way is still its
    // way: the grip's history goes on (round 21 wigglefix: a gun wiggled at the dummy, lagging the wrist's roll, is put
    // back in the hand at about every turn of the wiggle; each put-back forgot the wiggle, and the next flick passed for a
    // fresh blow, a butt strike).
    float jumped = FALSE;
    vector vgripOwn = '0 0 0';
    b.mh_vnear_last = b.mh_vnear;
    b.mh_vgrip_last = b.mh_vgrip;
    b.mh_vfar_last = b.mh_vfar;
    if(same)
    {
        // A jump (a teleport, the play space re-based) is faster than a hand goes.
        if(vlen(rgrip - b.mh_rgrip) > max(0.3, VR_MELEE_JUMP * dt))
        {
            same = FALSE;
        }
        else if(vlen(rfar - b.mh_rfar) > max(0.6, VR_MELEE_TIPJUMP * dt) ||
                (self.handdrop & (xHand == cVR_MainHand ? VR_HANDJUMP_MAIN : VR_HANDJUMP_OFF)))
        {
            jumped = TRUE;
        }
    }
    if(same && !jumped)
    {
        b.mh_vgrip = (rgrip - b.mh_rgrip) * (1 / dt);
        b.mh_vnear = b.mh_vgrip + VR_Melee_TurnVel(b.mh_rnear - b.mh_rgrip, rnear - rgrip, dt);
        b.mh_vfar = b.mh_vgrip + VR_Melee_TurnVel(b.mh_rfar - b.mh_rgrip, rfar - rgrip, dt);
        vwrist = (rwrist - b.mh_rwrist) * (1 / dt);
    }
    else
    {
        b.mh_vnear = b.mh_vgrip = b.mh_vfar = '0 0 0';
        if(jumped)
        {
            vgripOwn = (rgrip - b.mh_rgrip) * (1 / dt);
        }
    }
    b.mh_rnear = rnear;
    b.mh_rgrip = rgrip;
    b.mh_rfar = rfar;
    // (A fist's blow line is its knuckles' surface as edited (vr_melee_fist_*: the author's runs down the hand, not out
    // ahead of it): drawn back or struck is judged along the hand's own forward, the punch's way, not along that line.)
    b.mh_rpunch = normalize(VR_Blow_ToPlaySpace(VRForwardOf(VRGetEntHandRot(self, xHand))));
    b.mh_rwrist = rwrist;
    b.mh_near = vr_mline_near;
    b.mh_pgrip = same ? b.mh_grip : vr_mline_grip;
    b.mh_grip = vr_mline_grip;
    b.mh_far = vr_mline_far;
    b.mh_dt = (same && !jumped) ? dt : 0;
    b.mh_time = now;
    b.mh_ptime = ptime;
    b.mh_thing = thing;
    b.mh_wid = wid;
    b.mh_heavy2h = VR_Heavy_TwoHanded(self, xHand);
    b.mh_lenient = VR_Weight_Leniency(self, xHand) * VR_Heavy_SpeedFactor(b.mh_heavy2h);
    b.mh_curve = VR_Weight_MeleeCurve(self, xHand);
    if(!jumped)
    {
        b.mh_fast = max(vlen(b.mh_vfar), vlen(b.mh_vgrip)); // (a jump: as it was, no news)
    }
    if(same)
    {
        b.mh_hi = b.mh_hi + 1;
        if(b.mh_hi >= VR_MELEE_HISTORY)
        {
            b.mh_hi = 0;
        }
        b.mh_hfar[b.mh_hi] = b.mh_vfar; // (a jump: none)
        b.mh_hgrip[b.mh_hi] = jumped ? vgripOwn : b.mh_vgrip;
        b.mh_hwrist[b.mh_hi] = vwrist;
        b.mh_hdt[b.mh_hi] = dt;
    }
    else
    {
        b.mh_hdt[b.mh_hi] = 0; // (nothing to look back through)
    }

    if(vr_mline_free && (!b.mh_free || vr_mline_tip != b.mh_free_tip) && cvar_hget(cvarh_developer))
    {
        dprint(sprintf("melee: hand %g holds its weapon off its handle, %.2f of the way from its near end to its tip (%.1f units long); "
            "it strikes with all of it, the far end its %s (the hand at %v, the near end %v, the far end %v)\n", xHand,
            vr_mline_hs, vlen(vr_mline_far - vr_mline_near), vr_mline_tip ? "tip" : "near end", vr_mline_grip, vr_mline_near,
            vr_mline_far));
    }
    b.mh_free = vr_mline_free;
    b.mh_free_hs = vr_mline_hs;
    b.mh_free_tip = vr_mline_tip;
    b.mh_free_gw = vr_mline_gw;
    b.mh_free_back = vr_mline_back;
    b.mh_bfrom = vr_mline_free ? -1 : vr_mline_from_x;

    // The points: kept from the last pose, laid out now.
    VR_Melee_LayoutOf(b);
    // (Unrolled, constant indices: plain fields and globals, not the array accessors a variable index calls.)
#define VR_MELEE_KEEP(k) \
    if(k < vr_mpt_n) \
    { \
        b.mh_pp[k] = same ? b.mh_cp[k] : VR_Melee_At(vr_mpt_f[k]); \
        b.mh_cp[k] = VR_Melee_At(vr_mpt_f[k]); \
    }
    VR_MELEE_KEEP(0)
    VR_MELEE_KEEP(1)
    VR_MELEE_KEEP(2)
    VR_MELEE_KEEP(3)
    VR_MELEE_KEEP(4)
    VR_MELEE_KEEP(5)
    VR_MELEE_KEEP(6)
    VR_MELEE_KEEP(7)
    VR_MELEE_KEEP(8)
#undef VR_MELEE_KEEP
    b.mh_n = vr_mpt_n;
    // A hammer's or mace's head: its box (made from AD's model) and the line's frame, kept from the last pose.
    // (Or the weapon's own head and width: Edit Blade Shape.)
    VR_Melee_HeadOf(self, xHand, thing);
    b.mh_hd_lo = vr_mhead_lo;
    b.mh_hd_hi = vr_mhead_hi;
    b.mh_bwidth = vr_mline_shape_y;
    vector hy = VR_Melee_ModelY(self, xHand, vr_mline_far - vr_mline_grip);
    b.mh_hd_pg = same ? b.mh_hd_cg : vr_mline_grip;
    b.mh_hd_pf = same ? b.mh_hd_cf : vr_mline_far;
    b.mh_hd_py = same ? b.mh_hd_cy : hy;
    b.mh_hd_cg = vr_mline_grip;
    b.mh_hd_cf = vr_mline_far;
    b.mh_hd_cy = hy;
    return same;
};

// The velocity of a point of the layout at `xF` (play space, m/s): the line moves as a rigid thing.
vector(entity b, float xF) VR_Melee_PointVel =
{
    if(xF < 0 && !b.mh_free)
    {
        return b.mh_vnear;
    }
    return b.mh_vgrip + (b.mh_vfar - b.mh_vgrip) * xF;
};

// Whether `e` can be struck by the hands of `self`: it bleeds (or breaks), it is solid, and it is not the player's
// own (held or thrown; thrown with both hands a moment ago, VR_Throw_SparedBlows).
float(entity e) VR_Blow_Strikable =
{
    return e != self && e != world && e.takedamage && e.solid != SOLID_NOT && e.solid != SOLID_TRIGGER &&
           e.owner != self && e.carry_player != self && !VR_Throw_SparedBlows(e, self);
};

// ----------------------------------------------------------------------------
// Deciding a blow

// The way a part moved, named for the report (the play space: the head's facing ahead).
string(entity b, vector xVel, float xThing) VR_Melee_Way =
{
    float speed = vlen(xVel);
    if(speed <= 0)
    {
        return "";
    }
    vector d = xVel * (1 / speed);
    vector fwd = VR_Blow_Facing();
    float ahead = d * fwd;
    float up = d_z;
    float side = sqrt(max(0, 1 - ahead * ahead - up * up));
    if(xThing == VR_MTHING_FIST)
    {
        if(up > 0.6)
        {
            return "uppercut";
        }
        if(up < -0.6)
        {
            return "overhead";
        }
        if(side > fabs(ahead))
        {
            return "hook";
        }
        return ahead >= 0 ? "straight" : "backhand";
    }
    if(up < -0.8)
    {
        return "overhead";
    }
    if(up > 0.8)
    {
        return "rising";
    }
    if(fabs(up) < 0.4)
    {
        return "horizontal";
    }
    return up < 0 ? "diagonal, down" : "diagonal, up";
};

// Whether `b`'s weapon is wiggled, not swung (meleewiggled). Never a fist's.
float(entity b) VR_Melee_Wiggled =
{
    if(b.mh_thing == VR_MTHING_FIST)
    {
        return FALSE;
    }
    return meleewiggled(b);
};

// Whether `b`'s gun is being taken down to a holster to reload, not struck with (NOTES.md vrfiringrange_2026-09-30_02-03-28:
// a fast reload next to a monster hit it): its grip gone steeply down from aim or torso level (VR_MELEE_RELOAD_*), coming
// back towards the body as it drops or not. A gun chopped down from overhead, or swung down and ahead at something, is
// still a blow.
float(entity b) VR_Melee_Reloading =
{
    if(b.mh_thing != VR_MTHING_GUN || b.mh_vgrip_z >= 0)
    {
        return FALSE;
    }
    vector from = meleegripago(b, VR_MELEE_RELOAD_TIME);
    if(from_z > VR_MELEE_RELOAD_FROM)
    {
        return FALSE;
    }
    // Its way, less any coming back (a holster is by the hip, under and behind a gun held out).
    vector moved = b.mh_rgrip - from;
    vector fwd = VR_Blow_Facing();
    float ahead = moved * fwd;
    if(ahead < 0)
    {
        moved = moved - fwd * ahead;
    }
    return -moved_z >= VR_MELEE_RELOAD_DOWN * vlen(moved);
};

// How a weapon's blow is struck, for VR_Melee_WristMoved and VR_Melee_CameFar: a swing of a sword, an axe or a club (not a
// gun's: its own numbers, VR_MELEE_GUN_*), a pommel strike (the near end), anything else.
float VR_MHOW_OTHER = 0;
float VR_MHOW_SWING = 1;
float VR_MHOW_POMMEL = 2;

// Whether the hand itself moved as a blow's does (vr_melee_wrist_speed; a punch VR_MELEE_WRIST_PUNCH times that, a swing
// VR_MELEE_WRIST_SWING, a pommel strike VR_MELEE_WRIST_POMMEL: `xHow`, VR_MHOW_*), not only turned.
float(entity b, float xHow) VR_Melee_WristMoved =
{
    float need = cvar_hget(cvarh_vr_melee_wrist_speed) * (b.mh_lenient > 0 ? b.mh_lenient : 1);
    if(b.mh_thing == VR_MTHING_FIST)
    {
        need = need * VR_MELEE_WRIST_PUNCH;
    }
    else if(xHow == VR_MHOW_SWING)
    {
        need = need * VR_MELEE_WRIST_SWING;
    }
    else if(xHow == VR_MHOW_POMMEL)
    {
        need = need * VR_MELEE_WRIST_POMMEL;
    }
    return meleewristspeed(b) >= need;
};

// Whether `b`'s motion came from far enough to be a blow (VR_MELEE_RUN): a swing (`xHow`, VR_MHOW_*) at `xOver` times the
// speed it lands at, the further the slower (VR_MELEE_SLOW_*); a pommel strike VR_MELEE_POMMEL_RUN.
float(entity b, float xHow, float xOver) VR_Melee_CameFar =
{
    float need = VR_MELEE_RUN;
    if(xHow == VR_MHOW_SWING)
    {
        need = need + VR_MELEE_SLOW_RUN * min(1, max(0, (VR_MELEE_SLOW_FROM - xOver) / (VR_MELEE_SLOW_FROM - 1)));
    }
    else if(xHow == VR_MHOW_POMMEL)
    {
        need = max(need, VR_MELEE_POMMEL_RUN);
    }
    return meleerun(b, TRUE) >= need;
};

// Whether `b`'s far end goes back along the line towards the hand (VR_MELEE_PULLBACK): drawn back, not struck with. A
// fist's: back against the hand's forward (mh_rpunch), whatever way its edited blow line runs (vr_melee_fist_*).
float(entity b) VR_Melee_PulledBack =
{
    float vfar = vlen(b.mh_vfar);
    vector axis = b.mh_thing == VR_MTHING_FIST ? b.mh_rpunch : normalize(b.mh_rfar - b.mh_rgrip);
    return vfar > 0 && (b.mh_vfar * axis) / vfar <= VR_MELEE_PULLBACK;
};

// The weight of a pommel (butt) strike with `xThing`.
float(float xThing) VR_Melee_PommelWeight =
{
    if(xThing == VR_MTHING_SWORD)
    {
        return VR_MELEE_POMMEL_SWORD;
    }
    if(xThing == VR_MTHING_AXE || xThing == VR_MTHING_CLUB || xThing == VR_MTHING_SAW)
    {
        return VR_MELEE_POMMEL_AXE;
    }
    if(xThing == VR_MTHING_CROWBAR)
    {
        return VR_MELEE_POMMEL_CROWBAR;
    }
    return 1;
};

// Whether the blow of `xHand` (point `xI`, of kind `xKind`, on `xEnt`) lands, and as what: sets mh_kind, mh_speed,
// mh_strength, mh_part and mh_way. FALSE: nothing (too slow, or no blow).
float vr_msw_decide; // (the multi-target test, vr_melee_sweep_test 6: every blow lands, a cut at this strength)
float(float xHand, entity b, float xI, float xKind) VR_Melee_Decide =
{
    if(vr_msw_decide > 0)
    {
        b.mh_part = vr_mpt_name[xI];
        b.mh_kind = xKind == VR_MPT_EDGE ? VR_MKIND_SLASH : VR_MKIND_POMMEL;
        b.mh_speed = 6;
        b.mh_dmgspeed = 6;
        b.mh_core = vr_msw_decide;
        b.mh_strength = vr_msw_decide;
        b.mh_way = "down";
        return TRUE;
    }
    float minSpeed = VR_Melee_MinSpeedOf(b);
    float f = vr_mpt_f[xI];
    vector vp = VR_Melee_PointVel(b, f);
    vector axis = normalize(b.mh_rfar - b.mh_rgrip);
    float vfar = vlen(b.mh_vfar);
    float vgrip = vlen(b.mh_vgrip);
    float kind = VR_MKIND_SLASH;
    float speed = vgrip; // the arm's: the grip's speed
    float need = minSpeed;
    float land = 1; // the share over `need` it lands at (VR_MELEE_PUNCH_LAND, VR_MELEE_GUN_LAND)
    float weight = 1;
    vector way = b.mh_vfar;
    b.mh_part = vr_mpt_name[xI];
    float along = vfar > 0 ? (b.mh_vfar * axis) / vfar : 0;
    float lead = 0;

    if(xKind == VR_MPT_FIST)
    {
        // A punch is a closed fist (the grip held, or the flashlight in it): an open hand only shoves (VR_Bash). (The
        // author: every punch of his with the grip held; his open hands waved, flicked and patted at the dummy, some at
        // 7-9 m/s, all "no hit".)
        // Swung across, an open hand slaps (VR_Melee_Slaps): as a punch, weaker.
        kind = VR_MKIND_PUNCH;
        if(!VR_Melee_ClosedFist(xHand))
        {
            if(!VR_Melee_Slaps(xHand, b))
            {
                return FALSE;
            }
            kind = VR_MKIND_SLAP;
            b.mh_part = "the open hand";
        }
        way = b.mh_vgrip;
        // The blow's way: rising and steeply falling blows are thrown faster for the same effort.
        need = VR_Melee_SwingNeed(xHand, b, b.mh_vgrip);
        land = VR_MELEE_PUNCH_LAND;
        weight = cvar_hget(cvarh_vr_melee_punch_mult);
        if(kind == VR_MKIND_SLAP)
        {
            weight = weight * cvar_hget(cvarh_vr_melee_slap_mult);
        }
        else if(VR_Carry_IsHolding(self, xHand))
        {
            weight = weight * VR_Carry_PunchMult(self, xHand); // a box, a rock, a brick in hand: an improvised weapon
        }
    }
    else if(xKind == VR_MPT_EDGE || xKind == VR_MPT_BLUNT)
    {
        // The far end driven point first, along the line (its outer half striking): a stab (a jab). Else a swing: the
        // arm must drive it (VR_MELEE_ARM), however fast the head goes round. (Held anywhere on it, its pommel, hilt,
        // handle or butt: struck with as a swing is, a pommel (butt) strike.)
        if(xKind == VR_MPT_EDGE && along >= VR_MELEE_STAB_ALONG && f >= 0.5 && b.mh_thing != VR_MTHING_AXE &&
           b.mh_thing != VR_MTHING_CROWBAR)
        {
            kind = VR_MKIND_STAB;
            need = minSpeed * VR_MELEE_STAB_SPEED;
        }
        else
        {
            need = minSpeed * VR_MELEE_ARM;
            if(b.mh_thing == VR_MTHING_GUN)
            {
                land = VR_MELEE_GUN_LAND;
            }
            if(b.mh_thing == VR_MTHING_CROWBAR && vr_mpt_fh[xI] >= VR_CROWBAR_HOOK_F)
            {
                weight = VR_MELEE_CROWBAR_HOOK; // the hook: a hooked blow
            }
            if(xKind == VR_MPT_BLUNT)
            {
                kind = VR_MKIND_POMMEL;
                way = vp;
                weight = VR_Melee_PommelWeight(b.mh_thing);
            }
        }
    }
    else
    {
        // The pommel, the handle's end, a gun's butt: a pommel (butt) strike, unless the blade proper follows it into
        // the thing at once (VR_Melee_Sweep: a cut from close by).
        float sp = vlen(vp);
        lead = sp > 0 ? (vp * axis) / -sp : 0;
        kind = VR_MKIND_POMMEL;
        way = vp;
        speed = sp;
        if(vgrip < minSpeed * (b.mh_thing == VR_MTHING_GUN ? VR_MELEE_GUN_BUTT : 1))
        {
            return FALSE; // the arm drives it too (a sword waved round swings its guard fast)
        }
        // A gun's butt is struck with by carrying the gun at the thing (vr_melee_butt_run): a gun wiggled turns about the
        // hand, which hardly travels, and flings its butt round faster than the hand (round 21 wigglefix). Not a sword's
        // pommel: turned hilt first from close by, the author's pommel strikes come 0.36-0.54 m.
        if(b.mh_thing == VR_MTHING_GUN && meleerun(b, TRUE) < cvar_hget(cvarh_vr_melee_butt_run))
        {
            if(cvar_hget(cvarh_developer) >= 2)
            {
                dprint(sprintf("melee debug: the butt came %.2f m, under vr_melee_butt_run: no butt strike\n", meleerun(b, TRUE)));
            }
            return FALSE;
        }
        weight = VR_Melee_PommelWeight(b.mh_thing);
    }
    if(cvar_hget(cvarh_developer) >= 2)
    {
        dprint(sprintf("melee debug: hand %g %s at %s (point %g, %.2f on the weapon): grip %.1f far %.1f", xHand,
            VR_Melee_KindName(kind, b.mh_thing), vr_mpt_name[xI], xI, vr_mpt_fh[xI], vgrip, vfar)); // (8 at most)
        dprint(sprintf(" along %.2f lead %.2f need %.1f run %.2f wrist %.2f\n", along, lead, need, meleerun(b, TRUE),
            meleewristspeed(b)));
    }
    if(speed < need * land)
    {
        return FALSE;
    }
    // Heavy handling: a heavy weapon in both hands is swung slower than a light one (its weight), so its blows need less
    // speed (mh_lenient: VR_Heavy_SpeedFactor); one slower than a light weapon's blow strikes only as a deliberate swing,
    // a smooth arc of its far end (meleearc: a steady way, at least vr_heavy_swing_arc m over vr_heavy_swing_time s), not
    // a slow reposition or a wave. Such a swing need not come from far (VR_Melee_CameFar) and isn't a wiggle (its arc is
    // steady); its strength is the arc's and the weight's (below).
    float heavyArc = 0;
    float deliberate = FALSE;
    float arcNeed = max(0.05, cvar("vr_heavy_swing_arc"));
    if(b.mh_heavy2h > 0 && xKind != VR_MPT_FIST)
    {
        heavyArc = meleearc(b, need * 0.5);
        if(speed < need * land / VR_Heavy_SpeedFactor(b.mh_heavy2h))
        {
            if(heavyArc < arcNeed)
            {
                if(cvar_hget(cvarh_developer) >= 2)
                {
                    dprint(sprintf("melee debug: heavy two-handed, slow (%.2f m/s) and no deliberate arc (%.2f m of %.2f): no blow\n",
                        speed, heavyArc, arcNeed));
                }
                return FALSE;
            }
            deliberate = TRUE;
        }
    }
    float how = VR_MHOW_OTHER;
    if(b.mh_thing != VR_MTHING_GUN && b.mh_thing != VR_MTHING_FIST)
    {
        if(xKind == VR_MPT_EDGE && kind != VR_MKIND_STAB)
        {
            how = VR_MHOW_SWING;
        }
        else if(xKind == VR_MPT_NEAR)
        {
            how = VR_MHOW_POMMEL;
        }
    }
    // Not drawn back: a blade, a head or a fist going back towards the hand (after a blow, still touching).
    if((xKind == VR_MPT_EDGE || xKind == VR_MPT_FIST) && VR_Melee_PulledBack(b))
    {
        if(cvar_hget(cvarh_developer) >= 2)
        {
            dprint("melee debug: pulled back, no blow\n");
        }
        return FALSE;
    }
    // A blow, not a wiggle: the arm came from some way off (a slow swing from further).
    // (A heavy two-handed blow fast enough without its arc: from as far as a light weapon's at that speed, so a short jerk
    // of it doesn't strike either.)
    if(!deliberate && !VR_Melee_CameFar(b, how, speed * VR_Heavy_SpeedFactor(b.mh_heavy2h) / (need * land)))
    {
        return FALSE;
    }
    // And the hand itself moved, not only turned in place (a wiggle flicks the grip and the tip fast).
    if(!VR_Melee_WristMoved(b, how))
    {
        return FALSE;
    }
    // Nor a weapon waved back and forth.
    if(!deliberate && VR_Melee_Wiggled(b))
    {
        if(cvar_hget(cvarh_developer) >= 2)
        {
            dprint("melee debug: wiggled, no blow\n");
        }
        return FALSE;
    }
    // Nor a gun taken down to reload.
    if(VR_Melee_Reloading(b))
    {
        if(cvar_hget(cvarh_developer) >= 2)
        {
            dprint("melee debug: taken down to reload, no blow\n");
        }
        return FALSE;
    }
    if(cvar_hget(cvarh_developer) >= 2)
    {
        dprint(deliberate ? sprintf("melee debug: a blow (heavy two-handed, deliberate: arc %.2f m)\n", heavyArc) : "melee debug: a blow\n");
    }

    // The strength: the speed over the least. A sword's cut or stab measured at its tip (VR_MELEE_TIP_*), not the
    // grip that decided it landed (a sword carried off its handle, its near end out: the grip's, as before).
    float sspeed = speed;
    float sneed = need;
    if(b.mh_thing == VR_MTHING_SWORD && (kind == VR_MKIND_SLASH || kind == VR_MKIND_STAB) && (!b.mh_free || b.mh_free_tip))
    {
        sspeed = vfar;
        sneed = need * (kind == VR_MKIND_STAB ? VR_MELEE_TIP_STAB : VR_MELEE_TIP_SLASH);
    }
    sspeed = sspeed * VR_MELEE_STRENGTH_SPEED;
    float s = VR_MELEE_WEAKEST + (1 - VR_MELEE_WEAKEST) * (sspeed - sneed) / (sneed * (VR_MELEE_GOOD - 1)) *
              (b.mh_curve > 0 ? b.mh_curve : 1);
    b.mh_kind = kind;
    b.mh_speed = speed;
    b.mh_dmgspeed = sspeed;
    b.mh_core = min(VR_MELEE_STRONGEST, max(VR_MELEE_WEAKEST, s));
    // Heavy handling: a deliberate two-handed arc of a heavy weapon strikes hard however slow (its weight does the work:
    // weightdamage on top), vr_heavy_swing_strength at its weight class 1 for the arc needed, up to 1.3 times that for one
    // twice as long; a faster blow keeps its own if stronger.
    if(b.mh_heavy2h > 0 && heavyArc >= arcNeed)
    {
        float arcStrength = cvar("vr_heavy_swing_strength") * min(1.3, 0.7 + 0.3 * heavyArc / arcNeed);
        arcStrength = VR_MELEE_WEAKEST + (arcStrength - VR_MELEE_WEAKEST) * b.mh_heavy2h;
        b.mh_core = min(VR_MELEE_STRONGEST, max(b.mh_core, arcStrength));
    }
    if(b.mh_heavy2h > 0 && cvar_hget(cvarh_developer) >= 2)
    {
        dprint(sprintf("melee debug: heavy two-handed strength %.2f (by its speed %.2f, its arc %.2f m)\n", b.mh_core,
            min(VR_MELEE_STRONGEST, max(VR_MELEE_WEAKEST, s)), heavyArc));
    }
    b.mh_strength = b.mh_core * weight;
    b.mh_way = VR_Melee_Way(b, way, b.mh_thing);
    return TRUE;
};

// Whether the blow just decided (VR_Melee_Decide) lands on `xEnt`: a slap only on something that takes damage, not on a
// wall or a loose gib or head (it would burst it: VR_Gib_Blow); every other blow anywhere.
float(entity b, entity xEnt) VR_Melee_SlapLands =
{
    return b.mh_kind != VR_MKIND_SLAP || (xEnt.takedamage && !xEnt.vr_gib);
};

// One swing, many things (vr_melee_multi; ROUND22.md "One swing hits everything its edge passes through"): a swing of a
// weapon (not a bare fist: a punch strikes one thing) strikes each thing its striking part passes through once (at
// most vr_melee_multi_max of them), each its own blow by its own point and speed, and the world (a wall, the floor)
// once as well, in the order its parts meet them: a blow on the floor before a body lying on it no longer spends the
// swing (his note vrtesthall_2026-10-24_01-17-02), nor one on a body the wall beyond it.
float VR_MELEE_MULTI_MOST = 6; // things a swing strikes at most (vr_melee_multi_max's top)
float VR_MELEE_MULTI_LIST = 8; // and loose gibs with them (the list's fields: mh_hit_e0..e7; a full list: no more)
float(entity b) VR_Melee_Multi =
{
    return (cvar("vr_melee_multi") != 0) && (b.mh_thing != VR_MTHING_FIST) && (b.mh_thing != VR_MTHING_NONE);
};

float(entity b, entity xEnt) VR_Melee_HitHas =
{
    float n = b.mh_hit_n;
    return ((n > 0) && (b.mh_hit_e0 == xEnt)) || ((n > 1) && (b.mh_hit_e1 == xEnt)) ||
           ((n > 2) && (b.mh_hit_e2 == xEnt)) || ((n > 3) && (b.mh_hit_e3 == xEnt)) ||
           ((n > 4) && (b.mh_hit_e4 == xEnt)) || ((n > 5) && (b.mh_hit_e5 == xEnt)) ||
           ((n > 6) && (b.mh_hit_e6 == xEnt)) || ((n > 7) && (b.mh_hit_e7 == xEnt));
};

void(entity b, entity xEnt, float xGib) VR_Melee_HitAdd =
{
    if(VR_Melee_HitHas(b, xEnt) || (b.mh_hit_n >= VR_MELEE_MULTI_LIST))
    {
        return;
    }
    if(b.mh_hit_n == 0)      b.mh_hit_e0 = xEnt;
    else if(b.mh_hit_n == 1) b.mh_hit_e1 = xEnt;
    else if(b.mh_hit_n == 2) b.mh_hit_e2 = xEnt;
    else if(b.mh_hit_n == 3) b.mh_hit_e3 = xEnt;
    else if(b.mh_hit_n == 4) b.mh_hit_e4 = xEnt;
    else if(b.mh_hit_n == 5) b.mh_hit_e5 = xEnt;
    else if(b.mh_hit_n == 6) b.mh_hit_e6 = xEnt;
    else                     b.mh_hit_e7 = xEnt;
    b.mh_hit_n = b.mh_hit_n + 1;
    if(!xGib)
    {
        b.mh_hit_cap = b.mh_hit_cap + 1;
    }
};

// (A new motion: nothing struck yet.)
void(entity b) VR_Melee_HitClear =
{
    b.mh_hit_n = 0;
    b.mh_hit_cap = 0;
    b.mh_hit_world = FALSE;
};

// Whether a swing of `b` may still strike another thing: it struck something already (none: a bash or a held gib
// spent it, setting the hand's blow landed without one), fewer than vr_melee_multi_max things (loose gibs aside) and
// its list not full.
float(entity b) VR_Melee_MultiRoom =
{
    float most = min(VR_MELEE_MULTI_MOST, max(1, floor(cvar("vr_melee_multi_max"))));
    return ((b.mh_hit_n > 0) || b.mh_hit_world) && (b.mh_hit_cap < most) && (b.mh_hit_n < VR_MELEE_MULTI_LIST);
};

// Whether `xHand` may strike `xEnt` now: once per motion (see VR_MELEE_REARM_*); another thing at once (a swing
// through two monsters). A bash or a held gib struck spends the motion for everything (mh_hit_ent world). Never a
// ragdoll it holds or just let go of (VR_Ragdoll_HandSpares: a throw is not a blow). One swing, many things
// (VR_Melee_Multi): each thing once, after the world too.
float(float xHand, entity b, entity xEnt) VR_Melee_Armed =
{
    if(VR_Ragdoll_HandSpares(xEnt, self, xHand))
    {
        return FALSE;
    }
    if(!VRGetMeleeHitSoundPlayed(xHand))
    {
        return TRUE;
    }
    if(VR_Melee_Multi(b))
    {
        return VR_Melee_MultiRoom(b) && !VR_Melee_HitHas(b, xEnt);
    }
    return b.mh_hit_ent != world && xEnt != b.mh_hit_ent;
};

// Whether `xHand` may strike the world (a wall, the floor) now: a motion's first blow; one swing, many things
// (VR_Melee_Multi), once a motion after the things it struck too.
float(float xHand, entity b) VR_Melee_WallArmed =
{
    if(!VRGetMeleeHitSoundPlayed(xHand))
    {
        return TRUE;
    }
    return VR_Melee_Multi(b) && !b.mh_hit_world && b.mh_hit_n > 0;
};

// Re-arms `xHand` (each frame) once its motion slowed down since the last blow, or, its parts out of what it struck,
// turned back (a backswing) or went on long enough: a swing whose arc curves on through a big monster is one blow.
void(float xHand, entity b) VR_Melee_Rearm =
{
    if(!VRGetMeleeHitSoundPlayed(xHand) || VR_PlayerTime() < b.mh_hit_time + 0.1)
    {
        return;
    }
    float rearm = b.mh_fast < VR_Melee_MinSpeedOf(b) * VR_MELEE_REARM_SLOW;
    if(!rearm && VR_PlayerTime() - b.mh_hit_touch > VR_MELEE_REARM_OUT)
    {
        if(VR_PlayerTime() - b.mh_hit_time > VR_MELEE_REARM_TIME)
        {
            rearm = TRUE;
        }
        else if(b.mh_hit_dir != '0 0 0')
        {
            vector v = b.mh_vfar;
            if(vlen(b.mh_vgrip) > vlen(v))
            {
                v = b.mh_vgrip;
            }
            rearm = normalize(v) * b.mh_hit_dir < VR_MELEE_REARM_TURN;
        }
    }
    if(rearm)
    {
        VRSetMeleeHitSoundPlayed(xHand, FALSE);
        b.mh_hit_ent = world;
        VR_Melee_HitClear(b);
    }
};

// Whether a point swept from `xP0` to `xP1` strikes the wall (world, brush) it meets at `xAt`, whose normal is
// `xNormal`: not going down steeply onto the floor by the body (a hand reaching for a holster: a chop at the ground
// ahead does, VR_MELEE_FLOOR_AHEAD), nor down past a wall, grazing it; a blow chopped down into a wall does.
float vr_melee_wallwin; // vr_melee_wall_hold (s; 0: a wall struck only in the frame a part crosses it, as before)
float(vector xP0, vector xP1, vector xNormal, vector xAt) VR_Melee_WallTaken =
{
    vector d = normalize(xP1 - xP0);
    if(d_z >= VR_MELEE_WALL_DOWN)
    {
        return TRUE;
    }
    float taken;
    if(vr_melee_wallwin <= 0 && xNormal_z >= VR_MELEE_FLOOR)
    {
        taken = FALSE;
    }
    else if(xNormal_z >= VR_MELEE_FLOOR)
    {
        vector off = xAt - VRGetHeadPos(self);
        off_z = 0;
        taken = off * VRForwardOf('0 1 0' * self.v_viewangle_y) >= VR_MELEE_FLOOR_AHEAD * VRMetersToUnits();
    }
    else
    {
        taken = d * xNormal <= -VR_MELEE_WALL_INTO;
    }
    if(cvar_hget(cvarh_developer) >= 2)
    {
        dprint(sprintf("melee debug: going down at %.2f onto a surface facing %v (into it %.2f): %s\n", d_z, xNormal,
            -(d * xNormal), taken ? "strikes" : "passes"));
    }
    return taken;
};

// The speed (m/s) point `xI` of `b`, swept from `xP0` to `xP1`, goes into a surface facing `xNormal` (world): now (the
// line's velocity, or the sweep's in the world), or a pose before, the most. The hand's slide (vr_handpose.cpp) holds a
// weapon off the wall it meets, along its surface, so the pose it arrives in keeps little of the speed it came in at;
// a pose whose line jumped (VR_Melee_Track: a grip taken) has no velocity.
float(entity b, float xI, vector xP0, vector xP1, vector xNormal) VR_Melee_WallInto =
{
    vector nrm = VR_Blow_ToPlaySpace(xNormal);
    float f = vr_mpt_f[xI];
    vector before = b.mh_vgrip_last + (b.mh_vfar_last - b.mh_vgrip_last) * f;
    if(f < 0 && !b.mh_free)
    {
        before = b.mh_vnear_last;
    }
    float swept = 0;
    if(b.mh_dt > 0)
    {
        swept = (xP1 - xP0) * xNormal / (b.mh_dt * VRMetersToUnits());
    }
    return min(min(VR_Melee_PointVel(b, f) * nrm, before * nrm), swept) * -1;
};

// Whether point `xI` of `b`, swept from `xP0` to `xP1`, goes into the wall it met (VR_Melee_WallTaken) at `xAt`, facing
// `xNormal`, hard enough to strike it (VR_MELEE_WALL_ENTRY; a brush along it doesn't): then the hand may land a blow on
// that wall for vr_melee_wall_hold (VR_Melee_WallHeld).
float(entity b, float xI, vector xP0, vector xP1, vector xNormal, vector xAt) VR_Melee_WallEntered =
{
    if(!VR_Melee_WallTaken(xP0, xP1, xNormal, xAt))
    {
        return FALSE;
    }
    float into = VR_Melee_WallInto(b, xI, xP0, xP1, xNormal);
    if(vr_melee_wallwin <= 0)
    {
        return TRUE;
    }
    if(into < VR_MELEE_WALL_ENTRY * VR_Melee_MinSpeedOf(b))
    {
        if(cvar_hget(cvarh_developer) >= 2)
        {
            dprint(sprintf("melee debug: %s brushes a wall, into it at %.1f m/s\n", vr_mpt_name[xI], into));
        }
        return FALSE;
    }
    // The motion it came in at: this pose's, or the one before if faster (the slide slowed the hand meeting the wall);
    // going on into it (another of its parts, the next pose), the fastest since it met it.
    float again = VR_PlayerTime() - b.mh_wall_time <= vr_melee_wallwin;
    b.mh_wall_time = VR_PlayerTime();
    b.mh_wall_n = xNormal;
    if(again && vlen(b.mh_wall_vgrip) >= max(vlen(b.mh_vgrip), vlen(b.mh_vgrip_last)))
    {
        return TRUE;
    }
    b.mh_wall_vnear = b.mh_vnear;
    b.mh_wall_vgrip = b.mh_vgrip;
    b.mh_wall_vfar = b.mh_vfar;
    if(vlen(b.mh_vgrip_last) > vlen(b.mh_vgrip))
    {
        b.mh_wall_vnear = b.mh_vnear_last;
        b.mh_wall_vgrip = b.mh_vgrip_last;
        b.mh_wall_vfar = b.mh_vfar_last;
    }
    return TRUE;
};

// VR_Melee_Decide for a blow of `xHand` on a wall with point `xI`: at the speeds the hand came into it at
// (VR_Melee_WallEntered), not those the wall left it: held off by the hand's slide (vr_handpose.cpp), the weapon and the
// hand slow down on it, and a swing decided a frame or two after it met the wall was taken for a slow one.
float(float xHand, entity b, float xI) VR_Melee_WallDecide =
{
    vector vnear = b.mh_vnear;
    vector vgrip = b.mh_vgrip;
    vector vfar = b.mh_vfar;
    if(vr_melee_wallwin <= 0)
    {
        return VR_Melee_Decide(xHand, b, xI, vr_mpt_kind[xI]);
    }
    if(vlen(b.mh_wall_vgrip) > vlen(vgrip))
    {
        b.mh_vnear = b.mh_wall_vnear;
        b.mh_vgrip = b.mh_wall_vgrip;
        b.mh_vfar = b.mh_wall_vfar;
    }
    if(cvar_hget(cvarh_developer) >= 2)
    {
        dprint(sprintf("melee debug: a wall met %.2f s ago, come in at grip %.1f m/s (now %.1f)\n",
            VR_PlayerTime() - b.mh_wall_time, vlen(b.mh_wall_vgrip), vlen(vgrip)));
    }
    float landed = VR_Melee_Decide(xHand, b, xI, vr_mpt_kind[xI]);
    b.mh_vnear = vnear;
    b.mh_vgrip = vgrip;
    b.mh_vfar = vfar;
    return landed;
};

// Whether point `xI` of `b` (swept from `xP0` to `xP1`, `xR` thick) is still at the wall a part of the hand went into
// (VR_Melee_WallEntered) within vr_melee_wall_hold: in it (behind its surface, seen from the grip), or held off it by
// the hand's slide within `xR` and a unit, and not leaving it. The contact in the trace_ globals (fraction 1). A wall
// gave a blow one frame, the one the point crossed its surface: a blow decided a frame late (the wrist not yet up to
// speed: VR_Melee_WristMoved) never landed, while one in a monster's box is decided again every frame it is in it.
float(entity b, float xI, vector xP0, vector xP1, float xR) VR_Melee_WallHeld =
{
    if(vr_melee_wallwin <= 0 || vr_mpt_kind[xI] == VR_MPT_GRIP || VR_PlayerTime() - b.mh_wall_time > vr_melee_wallwin ||
       normalize(xP1 - xP0) * b.mh_wall_n > VR_MELEE_WALL_LEAVE)
    {
        return FALSE;
    }
    traceline(b.mh_grip, xP1, TRUE, self);
    if(trace_startsolid || trace_fraction >= 1)
    {
        if(trace_startsolid)
        {
            return FALSE; // (the hand itself in it)
        }
        traceline(xP1, xP1 - b.mh_wall_n * (xR + 1), TRUE, self);
        if(trace_startsolid || trace_fraction >= 1)
        {
            return FALSE;
        }
    }
    if(trace_plane_normal * b.mh_wall_n < VR_MELEE_WALL_SAME || VR_Blow_Strikable(trace_ent))
    {
        return FALSE;
    }
    trace_fraction = 1;
    return TRUE;
};

// Each frame (VR_Melee_Strike): whether the weapon of `b` is now off the wall it last struck (VR_MELEE_WALL_OFF, every
// part in front of that wall's plane by that much); then that wall may be struck again (VR_Melee_WallFresh).
void(entity b) VR_Melee_WallLeft =
{
    if(!b.mh_wall_on)
    {
        return;
    }
    float off = min(min(b.mh_near * b.mh_wall_bn, b.mh_grip * b.mh_wall_bn), b.mh_far * b.mh_wall_bn) - b.mh_wall_bd;
    if(off >= VR_MELEE_WALL_OFF * VRMetersToUnits())
    {
        b.mh_wall_on = FALSE;
    }
};

// Whether a contact at `xAt` on a wall facing `xNormal` is a new blow of `b` (its speed in mh_speed: after
// VR_Melee_WallDecide) and not the swing that struck that wall last again: one wall blow a swing. The same wall (its
// plane: the normals within VR_MELEE_WALL_SAME, the contact within 4 units of it) is struck again only once the weapon
// has left it (VR_Melee_WallLeft: a weapon lying on the wall, swept along it, struck it again with another part), and
// within VR_MELEE_WALL_FOLLOW of the last blow only as fast as VR_MELEE_WALL_WEAKER of it (the swing's recovery poking
// the wall). Two real blows in a row (the weapon drawn back and swung again) both land. Another wall: at once.
float(entity b, vector xNormal, vector xAt) VR_Melee_WallFresh =
{
    if(vr_melee_wallwin <= 0 || b.mh_wall_blow <= 0 || xNormal * b.mh_wall_bn < VR_MELEE_WALL_SAME ||
       fabs(xAt * b.mh_wall_bn - b.mh_wall_bd) > 4)
    {
        return TRUE;
    }
    float fresh = TRUE;
    string why = "";
    if(b.mh_wall_on)
    {
        fresh = FALSE;
        why = "the weapon still on it";
    }
    else if(VR_PlayerTime() - b.mh_wall_blow < VR_MELEE_WALL_FOLLOW && b.mh_speed < VR_MELEE_WALL_WEAKER * b.mh_wall_bspeed)
    {
        fresh = FALSE;
        why = sprintf("%.1f m/s after %.1f, %.2f s ago", b.mh_speed, b.mh_wall_bspeed, VR_PlayerTime() - b.mh_wall_blow);
    }
    if(!fresh && cvar_hget(cvarh_developer) >= 2)
    {
        dprint(sprintf("melee debug: the wall struck again by the same swing (%s): no blow\n", why));
    }
    return fresh;
};

// Whether the hands' blows find `e` by its box (VR_Melee_Sweep, after VR_Blow_Strikable): a solid one; a living monster
// knocked down (vr_knockdown.qc: not solid, its ragdoll struck as drawn, vr_hit_precise; a blow on it is a live one's:
// its head cut off, its limbs, by the same rules as standing; his note 2026-10-09); a loose gib or head, an exploding
// crate's piece (not solid: the traces pass them), not one held nor a small gib spared.
float(entity e) VR_Melee_BoxTarget =
{
    return e.solid == SOLID_SLIDEBOX || e.solid == SOLID_BBOX ||
           (e.solid == SOLID_NOT_BUT_TOUCHABLE && e.vr_knockdown == 1 && e.health > 0 && (e.flags & FL_MONSTER)) ||
           (e.solid == SOLID_NOT_BUT_TOUCHABLE && (e.vr_gib || e.vr_cratekind == 3) && e.carry_player == world &&
            (!VR_SmallGib_BlowSpared(e) || VR_SmallGib_BlowBursts(e)));
};

// One swing, many things (VR_Melee_Multi): a corpse or a body still dying (not solid to the traces: a ragdoll lying on the
// floor) is struck by the sweep as a box target is, as drawn (vr_hit_precise), and dealt as VR_Corpse_StrikeFrame's
// blow (VR_Corpse_Struck: client.qc VR_Melee_Deal). That one only traced the weapon's line as it is, from the hand: a
// blade whose tip met the floor first stopped there, short of the limb under it (his note vrtesthall_2026-10-24_01-17-02).
float(entity e) VR_Melee_CorpseTarget =
{
    return (e.solid == SOLID_NOT_BUT_TOUCHABLE) && e.takedamage && ((e.vr_corpse == 2) || e.vr_corpse_dying) &&
           !e.vr_gib && (e.health <= 0) && cvar_hget(cvarh_vr_corpse_gib);
};

// Whether a point swept from `xP0` to `xP1` (`xR` thick) starts in `e` (a box target: its model, vr_hit_precise, else its
// box, as VR_Melee_Sweep's boxes measure it): it doesn't enter it.
float(entity e, vector xP0, vector xP1, float xR) VR_Melee_StartsIn =
{
    if(!VR_Melee_BoxTarget(e))
    {
        return FALSE;
    }
    if(hitmodel_target(e))
    {
        return hitmodel_segment(e, xP0, xP1, xR, HITCLASS_MELEE) == 0;
    }
    vector pad = '1 1 1' * xR;
    return VR_SegmentBox(xP0, xP1, e.origin + e.mins - pad, e.origin + e.maxs + pad) == 0;
};

// The lines a hand's sweep strikes with (VR_Melee_Sweep, from VR_Melee_Ribs): each swept from the pose before to this one
// (vr_rib_p0 to vr_rib_p1), vr_rib_r thick, striking as the layout's point vr_rib_i (VR_Melee_Decide's); the box round
// them all, their thickness in (vr_rib_lo, _hi); a wall's crossing by each (VR_Melee_Sweep: how far past its surface,
// where, its normal).
float VR_MELEE_RIBS = 48; // (the arrays below as long)
float VR_MELEE_RIB_GAP = 1.5; // vr_melee_swept: neighbouring striking points' lines at most this many thicknesses apart
float VR_MELEE_RIB_MORE = 3;  // (at most this many lines more between two points)
float VR_MELEE_HEAD_MOST = 4; // a hammer's or mace's head: its lines along each of its two long sides, at most
vector vr_rib_p0[48];
vector vr_rib_p1[48];
float vr_rib_i[48];
float vr_rib_r[48];
float vr_rib_wd[48];
vector vr_rib_wat[48];
vector vr_rib_wn[48];
float vr_rib_n;
float vr_rib_rmax;
vector vr_rib_lo;
vector vr_rib_hi;

void(vector xP0, vector xP1, float xI, float xR) VR_Melee_RibAdd =
{
    if(vr_rib_n >= VR_MELEE_RIBS)
    {
        return;
    }
    vr_rib_p0[vr_rib_n] = xP0;
    vr_rib_p1[vr_rib_n] = xP1;
    vr_rib_i[vr_rib_n] = xI;
    vr_rib_r[vr_rib_n] = xR;
    vr_rib_n = vr_rib_n + 1;
    vr_rib_rmax = max(vr_rib_rmax, xR);
    vr_rib_lo_x = min(vr_rib_lo_x, min(xP0_x, xP1_x) - xR);
    vr_rib_lo_y = min(vr_rib_lo_y, min(xP0_y, xP1_y) - xR);
    vr_rib_lo_z = min(vr_rib_lo_z, min(xP0_z, xP1_z) - xR);
    vr_rib_hi_x = max(vr_rib_hi_x, max(xP0_x, xP1_x) + xR);
    vr_rib_hi_y = max(vr_rib_hi_y, max(xP0_y, xP1_y) + xR);
    vr_rib_hi_z = max(vr_rib_hi_z, max(xP0_z, xP1_z) + xR);
};

// A heavy head's lines along one of its sides, from `xLo` to `xHi` (shares of the line), each reaching `xIn` (its
// thickness, a share) to the faces: a thickness apart at most.
float(float xLo, float xHi, float xIn) VR_Melee_HeadLines =
{
    float span = xHi - xLo - 2 * xIn;
    if(span <= 0 || xIn <= 0)
    {
        return 1;
    }
    return min(VR_MELEE_HEAD_MOST, 1 + ceil(span / xIn));
};

// The place of the `xK`th of `xN` such lines.
float(float xLo, float xHi, float xIn, float xK, float xN) VR_Melee_HeadAt =
{
    if(xN <= 1)
    {
        return (xLo + xHi) * 0.5;
    }
    return xLo + xIn + (xHi - xLo - 2 * xIn) * xK / (xN - 1);
};

// A point's line from the pose before to this one along its arc round the grip, not the chord, when the chord cuts the
// arc's inside by more than VR_MELEE_ARC_SAG of the line's thickness: a fast turn, or a pose two headset frames on (a
// dropped frame; the 72 Hz ticks beating against the headset's frames) cuts further in. Then two lines, through the
// arc's middle (the grip's middle, the point's way from it turned half way, its distance the mean).
float VR_MELEE_ARC_SAG = 0.5;
void(entity b, vector xP0, vector xP1, float xI, float xR) VR_Melee_ArcRib =
{
    vector r0 = xP0 - b.mh_pgrip;
    vector r1 = xP1 - b.mh_grip;
    float l0 = vlen(r0);
    float l1 = vlen(r1);
    if(l0 >= 1 && l1 >= 1 && r0 * r1 > -0.9 * l0 * l1)
    {
        vector mid = (b.mh_pgrip + b.mh_grip) * 0.5 + normalize(r0 * (1 / l0) + r1 * (1 / l1)) * ((l0 + l1) * 0.5);
        if(vlen(mid - (xP0 + xP1) * 0.5) > VR_MELEE_ARC_SAG * xR)
        {
            VR_Melee_RibAdd(xP0, mid, xI, xR);
            VR_Melee_RibAdd(mid, xP1, xI, xR);
            return;
        }
    }
    VR_Melee_RibAdd(xP0, xP1, xI, xR);
};

// The lines of `b`'s sweep, `xR` thick: its layout's points (VR_Melee_Track); with vr_melee_swept (`xSwept`) the surface
// its whole blade sweeps: between two neighbouring striking points (a blade's, an axe's head's, the crowbar's bar), as
// many lines more as keep them at most VR_MELEE_RIB_GAP thicknesses apart all along the sweep (his note: a long swing
// passed by a thing its blade went through between two points), each striking as the nearer point; and a hammer's or
// mace's head as its volume (its box, admeleehead): a grid of lines over its middle, as thick as half its depth and
// reaching its faces, each striking as the head's point.
void(entity b, float xR, float xSwept) VR_Melee_Ribs =
{
    float n = b.mh_n;
    float i;
    vr_rib_n = 0;
    vr_rib_rmax = 0;
    vr_rib_lo = '1 1 1' * 1000000;
    vr_rib_hi = '-1 -1 -1' * 1000000;
    for(i = 0; i < n; ++i)
    {
        vector p0 = b.mh_pp[i];
        vector p1 = b.mh_cp[i];
        VR_Melee_ArcRib(b, p0, p1, i, xR);
        if(!xSwept || i + 1 >= n || !VR_Melee_Strikes(vr_mpt_kind[i]) || !VR_Melee_Strikes(vr_mpt_kind[i + 1]))
        {
            continue;
        }
        vector q0 = b.mh_pp[i + 1];
        vector q1 = b.mh_cp[i + 1];
        float more = min(VR_MELEE_RIB_MORE, ceil(max(vlen(q0 - p0), vlen(q1 - p1)) / (VR_MELEE_RIB_GAP * xR)) - 1);
        float j;
        for(j = 1; j <= more; ++j)
        {
            float u = j / (more + 1);
            VR_Melee_RibAdd(p0 + (q0 - p0) * u, p1 + (q1 - p1) * u, u < 0.5 ? i : i + 1, xR);
        }
    }
    if(!xSwept || n <= 0 || b.mh_hd_hi == '0 0 0')
    {
        return;
    }
    vector lo = b.mh_hd_lo;
    vector hi = b.mh_hd_hi;
    float len = vlen(b.mh_hd_cf - b.mh_hd_cg);
    float plen = vlen(b.mh_hd_pf - b.mh_hd_pg);
    if(len < 1 || plen < 1)
    {
        return;
    }
    float thick = max(xR, hi_z * len);
    float inset = thick / len;
    float na = VR_Melee_HeadLines(lo_x, hi_x, inset);
    float ny = VR_Melee_HeadLines(lo_y, hi_y, inset);
    vector cn = normalize(VR_Cross(b.mh_hd_cy, b.mh_hd_cf - b.mh_hd_cg)) * len;
    vector pn = normalize(VR_Cross(b.mh_hd_py, b.mh_hd_pf - b.mh_hd_pg)) * plen;
    float ia;
    float iy;
    for(ia = 0; ia < na; ++ia)
    {
        float sa = VR_Melee_HeadAt(lo_x, hi_x, inset, ia, na);
        for(iy = 0; iy < ny; ++iy)
        {
            float sy = VR_Melee_HeadAt(lo_y, hi_y, inset, iy, ny);
            VR_Melee_RibAdd(b.mh_hd_pg + (b.mh_hd_pf - b.mh_hd_pg) * sa + pn * sy,
                            b.mh_hd_cg + (b.mh_hd_cf - b.mh_hd_cg) * sa + cn * sy, n - 1, thick);
        }
    }
};

// vr_melee_swept: how squarely a line of `b` swept from `xP0` to `xP1` crosses `e` (lower is better): its nearest to the
// thing's middle, a share of its half size; the blade proper's lines (from VR_MELEE_BLADE_FROM out: a blade cuts with
// its outer part, a fist with its knuckles) before those nearer the hand, and a line already in it (`xIn`: it didn't
// enter it) after any that entered.
float(entity b, entity e, vector xP0, vector xP1, float xIn) VR_Melee_RibScore =
{
    vector c = (e.absmin + e.absmax) * 0.5;
    vector line = b.mh_far - b.mh_grip;
    float inner = (xP1 - b.mh_grip) * line < VR_MELEE_BLADE_FROM * (line * line);
    return (xIn ? 2 : 0) + (inner ? 1 : 0) + VR_PointSegmentDistance(c, xP0, xP1) / max(1, vlen(e.absmax - e.absmin) * 0.5);
};

// Show Melee Blades (vr_debug_melee_blades, Debug > Views; vr_meleeshow.hpp). Fast enough: VR_Melee_Decide would land a blow of its outermost striking point now (the speed its kind needs, the arm
// driving it, not drawn back, wiggled or turned in place); what it sets on `b` is put back (the blow's own comes after).
float(float xHand, entity b) VR_Melee_ShowFast =
{
    float i;
    float tip = -1;
    for(i = 0; i < vr_mpt_n; ++i)
    {
        if(VR_Melee_Strikes(vr_mpt_kind[i]))
        {
            tip = i;
        }
    }
    if(tip < 0 || b.mh_fast < VR_Melee_MinSpeedOf(b) * 0.5)
    {
        return FALSE;
    }
    float kind = b.mh_kind, strength = b.mh_strength, speed = b.mh_speed, dmgspeed = b.mh_dmgspeed, core = b.mh_core;
    string part = b.mh_part, way = b.mh_way;
    float fast = VR_Melee_Decide(xHand, b, tip, vr_mpt_kind[tip]);
    b.mh_kind = kind;
    b.mh_strength = strength;
    b.mh_speed = speed;
    b.mh_dmgspeed = dmgspeed;
    b.mh_core = core;
    b.mh_part = part;
    b.mh_way = way;
    return fast;
};

// The sweep of `xHand` this frame to the view: its blow line, whether it is fast enough to strike (VR_Melee_ShowFast),
// the lines VR_Melee_Ribs made, a head's box. Only while the view is on.
void(float xHand, entity b) VR_Melee_Show =
{
    meleeshow(xHand, 0, b.mh_grip, b.mh_far, VR_Melee_ShowFast(xHand, b));
    float k;
    vector from = '0 0 0'; // (its first striking point: where its blade starts, Edit Blade Shape's Blade Start)
    for(k = 0; k < vr_mpt_n && from == '0 0 0'; ++k)
    {
        if(VR_Melee_Strikes(vr_mpt_kind[k]) && vr_mpt_f[k] > 0)
        {
            from = b.mh_cp[k];
        }
    }
    meleeshow(xHand, 1, b.mh_near, from, 0);
    for(k = 0; k < vr_rib_n; ++k)
    {
        meleeshow(xHand, 2, vr_rib_p0[k], vr_rib_p1[k], vr_rib_r[k]);
    }
    if(b.mh_hd_hi != '0 0 0' && cvar_hget(cvarh_vr_melee_swept))
    {
        meleeshow(xHand, 3, b.mh_hd_lo, b.mh_hd_hi, 0);
        meleeshow(xHand, 4, b.mh_hd_cg, b.mh_hd_cf, 0);
        meleeshow(xHand, 5, b.mh_hd_cy, '0 0 0', 0);
    }
};

// VR_Melee_SweepTest's probe: VR_Melee_Sweep finds the contacts and stops (no speeds, nothing armed or decided), into
// vr_msw_*.
float vr_msw_probe;
float vr_msw_more; // (VR_Melee_StrikeMore's sweep: the frame's blades already shown)
entity vr_msw_edge;
vector vr_msw_edge_at;
float vr_msw_edge_i;
vector vr_msw_wall_at;
float vr_msw_wall_i;

// Sweeps the lines of `xHand` (VR_Melee_Ribs) from the last pose to this one (VR_Melee_Track) and decides the blow: the
// contact to land on into mh_c_* and TRUE, or FALSE. Things that bleed first: of the lines striking this frame, the
// blade's (or head's, fist's) over the pommel's; with vr_melee_swept the thing the blade crossed most squarely, struck
// where the line that crossed it nearest its middle met it, as its outermost striking point in it (its kind, a stab's
// or a cut's: VR_Melee_Decide); else (0) the outermost point's. The pommel's: the earliest along the sweep. A wall only
// when nothing that bleeds is struck, and not by a point going down (a hand reaching for a holster); with
// vr_melee_swept where the blade went deepest into it (the middle of the lines as deep), else the first touch.
float(float xHand, entity b) VR_Melee_Sweep =
{
    float n = b.mh_n;
    if(n <= 0)
    {
        return FALSE;
    }
    VR_Melee_LayoutOf(b); // (this hand's: the globals hold the last hand tracked)
    vr_melee_wallwin = cvar_hget(cvarh_vr_melee_wall_hold);
    float r = VR_MELEE_RADIUS * max(0.25, cvar_hget(cvarh_vr_melee_range_multiplier)) * (b.mh_bwidth > 0 ? b.mh_bwidth : 1);
    float swept = cvar_hget(cvarh_vr_melee_swept);
    vector hand = b.mh_grip;
    VR_Melee_Ribs(b, r, swept);
    if(!vr_msw_probe && !vr_msw_more && cvar_hget(cvarh_vr_debug_melee_blades))
    {
        VR_Melee_Show(xHand, b);
    }

    // The best contact of each class: the striking part's (edge, fist) and the near end's (pommel, butt).
    float edgeT = 2;
    float edgeI = -1;
    entity edgeEnt = world;
    vector edgeAt = '0 0 0';
    vector edgeDir = '0 0 0';
    float edgeScore = 1000000;
    float hiltT = 2;
    float hiltI = -1;
    entity hiltEnt = world;
    vector hiltAt = '0 0 0';
    vector hiltDir = '0 0 0';
    float wallT = 2;
    float wallI = -1;
    entity wallEnt = world;
    vector wallAt = '0 0 0';
    vector wallNormal = '0 0 0';
    vector wallDir = '0 0 0';
    float wallDepth = -1000000;
    entity traceEnt = world; // (vr_melee_swept: a box target the lines' traces met first, its best line, its outermost)
    float traceOuter = -1;
    float traceScore = 1000000;
    vector traceAt = '0 0 0';
    vector traceDir = '0 0 0';
    float k;
    float i;
    vector p0;
    vector p1;
    float rr;
    float t;
    float striker;

    // Walls and brush entities (a line each, exact against the BSP; it finds monsters too). A striking point strikes a
    // wall within `r` past its sweep, as it does a monster's box widened by `r`: the hand and what it holds are stopped
    // short of walls (vr_handpose.cpp: the far end, a blade's tip or an axe's head, a unit off), so an axe swung into a
    // wall never reached it, while a fist's knuckles, 4 units ahead of the stopped hand, went in. Held off it, the hand
    // slows down at once: the wall is struck over the frames it stays against it (vr_melee_wall_hold, VR_Melee_WallHeld),
    // at the speed it came in at (VR_Melee_WallDecide), as a monster's box is over the frames a blow is in it.
    for(k = 0; k < vr_rib_n; ++k)
    {
        vr_rib_wd[k] = -1000000;
        p0 = vr_rib_p0[k];
        p1 = vr_rib_p1[k];
        if(p0 == p1)
        {
            continue;
        }
        i = vr_rib_i[k];
        rr = vr_rib_r[k];
        striker = VR_Melee_Strikes(vr_mpt_kind[i]);
        traceline(p0, p1, MOVE_HITMODEL_MELEE, self); // (a monster: its model, vr_hit_precise)
        if(trace_fraction >= 1 && !trace_startsolid)
        {
            float past = FALSE;
            if(striker)
            {
                traceline(p1, p1 + normalize(p1 - p0) * rr, TRUE, self);
                if(trace_fraction < 1 && !trace_startsolid)
                {
                    past = TRUE;
                }
            }
            trace_fraction = past ? 1 : 2; // (a wall within reach past the sweep's end: there)
        }
        // Nothing met on the way (or the sweep starts in a wall): a point still at, or in, the wall a part of this hand
        // went into a moment ago (VR_Melee_WallHeld) lands there, a frame later, as one through a monster's box does.
        float held = FALSE;
        if(trace_startsolid || trace_fraction > 1)
        {
            if(!VR_Melee_WallHeld(b, i, p0, p1, rr))
            {
                continue;
            }
            held = TRUE;
        }
        if(!held && VR_Blow_Strikable(trace_ent))
        {
            if(trace_ent == b.mh_hit_ent)
            {
                b.mh_hit_touch = VR_PlayerTime();
            }
            if(vr_mpt_kind[i] == VR_MPT_GRIP || (trace_ent == b.mh_struck && VR_Melee_StartsIn(trace_ent, p0, p1, rr)))
            {
                continue; // (a blow enters what it strikes: below)
            }
            if(swept && VR_Melee_BoxTarget(trace_ent))
            {
                // (Measured with its box's below: kept for the one thing a line found first.)
                if(striker && (traceEnt == world || traceEnt == trace_ent) && (vr_msw_probe || VR_Melee_Armed(xHand, b, trace_ent)))
                {
                    float tsc = VR_Melee_RibScore(b, trace_ent, p0, p1, FALSE);
                    traceEnt = trace_ent;
                    if(traceOuter < 0 || fabs(vr_mpt_f[i]) > fabs(vr_mpt_f[traceOuter]))
                    {
                        traceOuter = i;
                    }
                    if(tsc < traceScore)
                    {
                        traceScore = tsc;
                        traceAt = trace_endpos;
                        traceDir = normalize(p1 - p0);
                    }
                }
                continue;
            }
            float sc = swept ? VR_Melee_RibScore(b, trace_ent, p0, p1, FALSE) : 0;
            float first = trace_fraction < edgeT;
            if(swept)
            {
                first = sc < edgeScore;
            }
            if(striker && first &&
               (vr_msw_probe || VR_Melee_Armed(xHand, b, trace_ent)))
            {
                edgeT = trace_fraction;
                edgeScore = sc;
                edgeI = i;
                edgeEnt = trace_ent;
                edgeAt = trace_endpos;
                edgeDir = normalize(p1 - p0);
            }
            else if(!striker && trace_fraction < hiltT && (vr_msw_probe || VR_Melee_Armed(xHand, b, trace_ent)))
            {
                hiltT = trace_fraction;
                hiltI = i;
                hiltEnt = trace_ent;
                hiltAt = trace_endpos;
                hiltDir = normalize(p1 - p0);
            }
        }
        else if(vr_mpt_kind[i] != VR_MPT_GRIP)
        {
            // How far past the surface the line went (held off it, within reach: under 0).
            float depth = (trace_endpos - p1) * trace_plane_normal;
            float better = trace_fraction < wallT;
            if(swept)
            {
                // (As deep: a striking part's over the pommel's, then the first.)
                better = depth > wallDepth + 0.01;
                if(!better && depth > wallDepth - 0.01)
                {
                    better = ((striker && wallI >= 0 && !VR_Melee_Strikes(vr_mpt_kind[wallI])) ||
                              (striker == VR_Melee_Strikes(vr_mpt_kind[max(0, wallI)]) && trace_fraction < wallT));
                }
            }
            float entered = FALSE;
            if(better || swept)
            {
                entered = (held || vr_msw_probe);
                if(!entered)
                {
                    entered = VR_Melee_WallEntered(b, i, p0, p1, trace_plane_normal, trace_endpos);
                }
            }
            if(entered)
            {
                vr_rib_wd[k] = depth;
                vr_rib_wat[k] = trace_endpos;
                vr_rib_wn[k] = trace_plane_normal;
                if(better)
                {
                    wallT = trace_fraction;
                    wallDepth = depth;
                    wallI = i;
                    wallEnt = trace_ent;
                    wallAt = trace_endpos;
                    wallNormal = trace_plane_normal;
                    wallDir = normalize(p1 - p0);
                }
            }
        }
    }
    // The wall where the blade crossed it: the middle of the lines that went as deep into the same surface (a flat
    // chop at the floor: the blade's middle, not its first point).
    if(swept && wallI >= 0)
    {
        vector sum = '0 0 0';
        float cnt = 0;
        for(k = 0; k < vr_rib_n; ++k)
        {
            if(vr_rib_wd[k] >= wallDepth - 1 && vr_rib_wn[k] * wallNormal > 0.9 &&
               VR_Melee_Strikes(vr_mpt_kind[vr_rib_i[k]]) == VR_Melee_Strikes(vr_mpt_kind[wallI]))
            {
                sum = sum + vr_rib_wat[k];
                cnt = cnt + 1;
            }
        }
        if(cnt > 1)
        {
            wallAt = sum * (1 / cnt);
        }
    }

    // Monsters' boxes, widened by the lines' thickness (and not through a wall); and loose gibs and heads (on the
    // floor, flying): not solid (SOLID_NOT_BUT_TOUCHABLE: the traces above pass them; only shots trace with
    // MOVE_HITGIBS), struck as a monster's box, and burst (VR_Gib_Blow). A gib held in a hand is VR_Gib_HeldFrame's.
    // Precise hits (vr_hit_precise): a monster's (or corpse's) box, grown by VR_HITMODEL_REACH, is only the broad phase:
    // a line strikes where its sweep meets the model as drawn, grown by its thickness and the melee tolerance
    // (hitmodel_segment); through the box beside the model it misses.
    vector mid = (b.mh_pp[0] + b.mh_cp[0] + b.mh_pp[n - 1] + b.mh_cp[n - 1]) * 0.25;
    float reach = max(max(vlen(b.mh_pp[0] - mid), vlen(b.mh_cp[0] - mid)),
                      max(vlen(b.mh_pp[n - 1] - mid), vlen(b.mh_cp[n - 1] - mid)));
    entity e = findradius(mid, reach + vr_rib_rmax + 96);
    float corpses = VR_Melee_Multi(b); // (corpses too: VR_Melee_CorpseTarget)
    while(e)
    {
        // A small gib just torn out: struck only to burst (vr_smallgibs_blow_burst), never batted (VR_SmallGib_BlowSpared).
        if((VR_Blow_Strikable(e) || VR_SmallGib_BlowBursts(e)) &&
           (VR_Melee_BoxTarget(e) || (corpses && VR_Melee_CorpseTarget(e))))
        {
            vector lo = e.origin + e.mins;
            vector hi = e.origin + e.maxs;
            float armed = vr_msw_probe || VR_Melee_Armed(xHand, b, e);
            float precise = hitmodel_target(e);
            float boxPad = 0;
            if(precise)
            {
                boxPad = VR_HITMODEL_REACH;
            }
            // (The box round all the lines first: most things near are nowhere near them.)
            float near = !swept || (lo_x - boxPad <= vr_rib_hi_x && hi_x + boxPad >= vr_rib_lo_x &&
                                    lo_y - boxPad <= vr_rib_hi_y && hi_y + boxPad >= vr_rib_lo_y &&
                                    lo_z - boxPad <= vr_rib_hi_z && hi_z + boxPad >= vr_rib_lo_z);
            vector ctr = (lo + hi) * 0.5;
            float eScore = 1000000;
            float eOuter = -1;
            vector eAt = '0 0 0';
            vector eDir = '0 0 0';
            for(k = 0; near && k < vr_rib_n; ++k)
            {
                p0 = vr_rib_p0[k];
                p1 = vr_rib_p1[k];
                i = vr_rib_i[k];
                rr = vr_rib_r[k];
                float pad = rr + boxPad;
                t = VR_SegmentBox(p0, p1, lo - '1 1 1' * pad, hi + '1 1 1' * pad);
                if(t < 0)
                {
                    continue;
                }
                if(precise)
                {
                    t = hitmodel_segment(e, p0, p1, rr, HITCLASS_MELEE);
                    if(t < 0)
                    {
                        continue;
                    }
                }
                if(!armed || vr_mpt_kind[i] == VR_MPT_GRIP)
                {
                    if(e == b.mh_hit_ent)
                    {
                        b.mh_hit_touch = VR_PlayerTime(); // still in what it struck
                    }
                    if(!armed)
                    {
                        break;
                    }
                    continue;
                }
                // A blow strikes again what it struck only entering it: a point whose sweep starts in it (a fist or a
                // blade still in the thing after a blow, drawn back or turned about in it: his note 2026-10-12, a
                // retraction striking again) strikes nothing; its sweep from outside, even through it in one frame,
                // does. (Not a first blow's: his punches and thrusts start their last frame within a point's reach.)
                if(t <= 0 && e == b.mh_struck)
                {
                    if(e == b.mh_hit_ent)
                    {
                        b.mh_hit_touch = VR_PlayerTime();
                    }
                    continue;
                }
                striker = VR_Melee_Strikes(vr_mpt_kind[i]);
                float rsc = 0;
                if(swept && striker)
                {
                    // The line that crossed it most squarely is where it lands; the outermost striking point in it
                    // what struck (its kind).
                    rsc = VR_Melee_RibScore(b, e, p0, p1, t <= 0);
                    if(rsc >= eScore && eOuter >= 0 && fabs(vr_mpt_f[i]) <= fabs(vr_mpt_f[eOuter]))
                    {
                        continue;
                    }
                }
                // Of the striking points in it, the outermost (a blade strikes with its outer part, its root may have been
                // in the thing for a while: the furthest from the grip, either way along a weapon held anywhere); of the
                // near end's, the first along the sweep.
                else if((striker && edgeI >= 0 && fabs(vr_mpt_f[i]) < fabs(vr_mpt_f[edgeI])) || (!striker && t >= hiltT))
                {
                    continue;
                }
                // On the box itself (precise: where the sweep met the model; a head's line: its face, not its middle).
                vector at = p0 + (p1 - p0) * t;
                if(!precise)
                {
                    at_x = min(hi_x, max(lo_x, at_x));
                    at_y = min(hi_y, max(lo_y, at_y));
                    at_z = min(hi_z, max(lo_z, at_z));
                }
                else if(rr > r)
                {
                    at = at + normalize(ctr - at) * min(rr - r, vlen(ctr - at));
                }
                traceline(hand, at, TRUE, self);
                if(trace_fraction < 1)
                {
                    continue;
                }
                if(striker && swept)
                {
                    if(eOuter < 0 || fabs(vr_mpt_f[i]) > fabs(vr_mpt_f[eOuter]))
                    {
                        eOuter = i;
                    }
                    if(rsc < eScore)
                    {
                        eScore = rsc;
                        eAt = at;
                        eDir = normalize(p1 - p0);
                    }
                }
                else if(striker)
                {
                    edgeT = t;
                    edgeI = i;
                    edgeEnt = e;
                    edgeAt = at;
                    edgeDir = normalize(p1 - p0);
                }
                else
                {
                    hiltT = t;
                    hiltI = i;
                    hiltEnt = e;
                    hiltAt = at;
                    hiltDir = normalize(p1 - p0);
                }
            }
            if(eOuter >= 0 && eScore < edgeScore)
            {
                edgeScore = eScore;
                edgeT = 0;
                edgeI = eOuter;
                edgeEnt = e;
                edgeAt = eAt;
                edgeDir = eDir;
            }
        }
        e = e.chain;
    }
    // (The thing the traces met: its outermost striking point and best line with its box's.)
    if(traceEnt != world && (edgeEnt == traceEnt || traceScore < edgeScore))
    {
        if(edgeEnt != traceEnt || fabs(vr_mpt_f[traceOuter]) > fabs(vr_mpt_f[edgeI]))
        {
            edgeI = traceOuter;
        }
        if(edgeEnt != traceEnt || traceScore < edgeScore)
        {
            edgeScore = traceScore;
            edgeAt = traceAt;
            edgeDir = traceDir;
        }
        edgeEnt = traceEnt;
        edgeT = 0;
    }

    if(vr_msw_probe)
    {
        vr_msw_edge = edgeEnt;
        vr_msw_edge_at = edgeAt;
        vr_msw_edge_i = edgeI;
        vr_msw_wall_at = wallAt;
        vr_msw_wall_i = wallI;
        return FALSE;
    }

    // The striking part first. The pommel's touch waits VR_Melee_PommelWait for the blade proper to reach the
    // same thing: then the blow is the blade's (a cut whose hands arrive first); else it lands as the pommel strike it
    // was.
    if(edgeI >= 0 && VR_Melee_Decide(xHand, b, edgeI, vr_mpt_kind[edgeI]) && VR_Melee_SlapLands(b, edgeEnt))
    {
        if(b.mh_pend_ent == world || b.mh_pend_ent != edgeEnt || vr_mpt_f[edgeI] >= VR_MELEE_BLADE_FROM)
        {
            b.mh_pend_ent = world;
            b.mh_c_ent = edgeEnt;
            b.mh_c_pos = edgeAt;
            b.mh_c_dir = edgeDir;
            b.mh_c_normal = b.mh_c_dir * -1;
            return TRUE;
        }
    }
    if(hiltI >= 0 && b.mh_pend_ent == world && VR_Melee_Decide(xHand, b, hiltI, vr_mpt_kind[hiltI]))
    {
        b.mh_c_ent = hiltEnt;
        b.mh_c_pos = hiltAt;
        b.mh_c_dir = hiltDir;
        b.mh_c_normal = b.mh_c_dir * -1;
        b.mh_pend_ent = hiltEnt;
        b.mh_pend_time = VR_PlayerTime();
        b.mh_pend_pos = hiltAt;
        b.mh_pend_dir = b.mh_c_dir;
        b.mh_pend_kind = b.mh_kind;
        b.mh_pend_strength = b.mh_strength;
        b.mh_pend_speed = b.mh_speed;
        b.mh_pend_dmgspeed = b.mh_dmgspeed;
        b.mh_pend_core = b.mh_core;
        b.mh_pend_part = b.mh_part;
        b.mh_pend_way = b.mh_way;
    }
    if(b.mh_pend_ent != world && VR_PlayerTime() - b.mh_pend_time >= VR_Melee_PommelWait())
    {
        // The blade never came: the pommel strike.
        b.mh_c_ent = b.mh_pend_ent;
        b.mh_c_pos = b.mh_pend_pos;
        b.mh_c_dir = b.mh_pend_dir;
        b.mh_c_normal = b.mh_c_dir * -1;
        b.mh_kind = b.mh_pend_kind;
        b.mh_strength = b.mh_pend_strength;
        b.mh_speed = b.mh_pend_speed;
        b.mh_dmgspeed = b.mh_pend_dmgspeed;
        b.mh_core = b.mh_pend_core;
        b.mh_part = b.mh_pend_part;
        b.mh_way = b.mh_pend_way;
        b.mh_pend_ent = world;
        return b.mh_c_ent.takedamage && VR_Melee_Armed(xHand, b, b.mh_c_ent);
    }
    if(edgeI < 0 && hiltI < 0 && wallI >= 0 && VR_Melee_WallArmed(xHand, b) &&
       (vr_melee_wallwin <= 0 || VR_PlayerTime() - b.mh_wall_blow >= VR_MELEE_WALL_AGAIN) &&
       VR_Melee_WallDecide(xHand, b, wallI) && VR_Melee_SlapLands(b, wallEnt) && VR_Melee_WallFresh(b, wallNormal, wallAt))
    {
        b.mh_c_ent = wallEnt;
        b.mh_c_pos = wallAt;
        b.mh_c_dir = wallDir;
        b.mh_c_normal = wallNormal;
        b.mh_c_blunt = vr_mpt_kind[wallI] != VR_MPT_EDGE;
        return TRUE;
    }
    return FALSE;
};

// The contact the blow of `xHand` lands on (VR_Melee_Sweep), in the trace_ globals and v_forward (the striking
// point's way), as a traceline's hit (VRDoMeleeAttackTracelineFor, weapons.qc).
float(float xHand) VR_Blow_Contact =
{
    entity b = VR_Blow(xHand);
    if(b.mh_c_ent == world && b.mh_c_pos == '0 0 0')
    {
        return FALSE;
    }
    trace_ent = b.mh_c_ent;
    trace_endpos = b.mh_c_pos;
    trace_plane_normal = b.mh_c_normal;
    v_forward = b.mh_c_dir;
    trace_fraction = 0.5;
    trace_startsolid = FALSE;
    trace_allsolid = FALSE;
    return TRUE;
};

// The damage multiplier of the blow `xHand` is dealing (VRMeleeDmg).
float(float xHand) VR_Blow_Strength =
{
    return VR_Blow(xHand).mh_strength;
};

// And a counter's (VR_Counter_Use; VRMeleeDmg): 1 for a blow that isn't one.
float(float xHand) VR_Blow_Counter =
{
    float c = VR_Blow(xHand).mh_counter;
    return c > 0 ? c : 1;
};

// And for want of stamina (VR_Stamina_Strike; VRMeleeDmg, VR_Gib_Blow): 1 for a blow paid in full.
float(float xHand) VR_Blow_Exhaust =
{
    float c = VR_Blow(xHand).mh_exhaust;
    return c > 0 ? c : 1;
};

string(float xKind, float xThing) VR_Melee_KindName =
{
    if(xKind == VR_MKIND_STAB)
    {
        return xThing == VR_MTHING_SWORD ? "stab" : "jab";
    }
    if(xKind == VR_MKIND_POMMEL)
    {
        return xThing == VR_MTHING_GUN ? "butt strike" : (xThing == VR_MTHING_CROWBAR ? "chisel strike" : "pommel strike");
    }
    if(xKind == VR_MKIND_PUNCH)
    {
        return "punch";
    }
    if(xKind == VR_MKIND_SLAP)
    {
        return "slap";
    }
    if(xThing == VR_MTHING_SWORD)
    {
        return "slash";
    }
    if(xThing == VR_MTHING_AXE)
    {
        return "chop";
    }
    if(xThing == VR_MTHING_SAW)
    {
        return "cut";
    }
    if(xThing == VR_MTHING_CROWBAR)
    {
        return "strike";
    }
    return "swing";
};

// The sound of a blow landing on something that takes damage (weapons.qc: the weapons' melee): `xSound`, the weapon's
// own, or for a pommel, hilt or butt strike a blunt knock of its own (vr/pommel1..3.wav, a little apart in pitch; made by
// make_sounds.py; one of the two not played last), apart from the blades' cuts and the punches. vr_melee_blunt is set
// around the weapons' melee by PlayerVRMeleeImpl (client.qc) for the blow being struck. `developer` prints which.
float vr_melee_blunt;
float vr_melee_pommel_last; // 1..3 (0: none yet)
float vr_melee_slap_last;   // 1..3 (0: none yet)
string(string xSound) VR_Melee_HitSound =
{
    if(vr_melee_slap)
    {
        float j = min(3, 1 + floor(random() * 3));
        if(vr_melee_slap_last)
        {
            j = vr_melee_slap_last + (random() < 0.5 ? 1 : 2);
            if(j > 3)
            {
                j = j - 3;
            }
        }
        vr_melee_slap_last = j;
        xSound = j == 1 ? "vr/slap1.wav" : (j == 2 ? "vr/slap2.wav" : "vr/slap3.wav");
        dprint(sprintf("melee sound: %s\n", xSound));
        return xSound;
    }
    if(vr_melee_blunt)
    {
        float k = min(3, 1 + floor(random() * 3));
        if(vr_melee_pommel_last)
        {
            k = vr_melee_pommel_last + (random() < 0.5 ? 1 : 2);
            if(k > 3)
            {
                k = k - 3;
            }
        }
        vr_melee_pommel_last = k;
        xSound = k == 1 ? "vr/pommel1.wav" : (k == 2 ? "vr/pommel2.wav" : "vr/pommel3.wav");
    }
    dprint(sprintf("melee sound: %s\n", xSound));
    return xSound;
};

// The category of a blow for the motion evaluation: slash, stab, pommel, punch, gun.
string(float xKind, float xThing) VR_Melee_Category =
{
    if(xThing == VR_MTHING_GUN)
    {
        return "gun";
    }
    if(xKind == VR_MKIND_STAB)
    {
        return "stab";
    }
    if(xKind == VR_MKIND_POMMEL)
    {
        return "pommel";
    }
    if(xKind == VR_MKIND_PUNCH)
    {
        return "punch";
    }
    if(xKind == VR_MKIND_SLAP)
    {
        return "slap";
    }
    return "slash";
};

// The least speed of a blow by `xHand`'s far end moving along `xVel` (a punch's by its way; a weapon's swing).
float(float xHand, entity b, vector xVel) VR_Melee_SwingNeed =
{
    float minSpeed = VR_Melee_MinSpeedOf(b);
    if(b.mh_thing != VR_MTHING_FIST)
    {
        return minSpeed * VR_MELEE_ARM;
    }
    float s = vlen(xVel);
    float vz = s > 0 ? xVel_z / s : 0;
    return minSpeed * (1 + VR_MELEE_UP * max(0, vz) + VR_MELEE_DOWN * max(0, -vz - VR_MELEE_DOWN_FROM));
};

// The speed at which a blow by `xHand` along `xVel` lands (and whooshes): its least speed, more for a punch or a gun's swing
// (VR_MELEE_PUNCH_LAND, VR_MELEE_GUN_LAND: too weak to count below).
float(float xHand, entity b, vector xVel) VR_Melee_LandNeed =
{
    float need = VR_Melee_SwingNeed(xHand, b, xVel);
    if(b.mh_thing == VR_MTHING_FIST)
    {
        return need * VR_MELEE_PUNCH_LAND;
    }
    if(b.mh_thing == VR_MTHING_GUN)
    {
        return need * VR_MELEE_GUN_LAND;
    }
    return need;
};

float vr_melee_whoosh_alt; // the swish played last (VR_Melee_Whoosh): the knights' two swings taken in turn

// ----------------------------------------------------------------------------
// Swing sounds by speed (vr_melee_swish; docs/vr-port/ROUND22.md, "Swing sounds by speed")
//
// Any motion of a fist, an open hand or a melee weapon that moves air whooshes, by its speed as a share of the speed its
// blow would land at (VR_Swish_Share: VR_Melee_LandNeed, the hand's and the blow's way; a weapon's far end, which the
// wrist whips round, counted at 1 / VR_MELEE_TIP_SLASH of its speed). Under vr_melee_swish_min: silent. Up to the
// landing speed: softly, at most vr_melee_swish_sub, as the motion is at its fastest (it slowed to VR_SWISH_PEAK_FALL of
// it: VR_Swish_Frame), a little lower the softer. A blow (VR_Melee_Whoosh, as it lands, as before) at 1, up to
// vr_melee_swish_gain at vr_melee_swish_full times its landing speed (louder than full: protocol.h SND_LOUD), a little
// higher too. Once a motion: again once the part slowed under VR_SWISH_REARM of the least share, a soft one not within
// VR_SWISH_COOLDOWN of the hand's last. A gun keeps its blow's whoosh only, at 1 (waved, aimed and reloaded all the time).
float VR_SWISH_PEAK_FALL = 0.9; // a soft swish plays once the motion slowed to this share of its fastest
float VR_SWISH_REARM = 0.7;     // and the next once the part slowed under this share of vr_melee_swish_min
float VR_SWISH_COOLDOWN = 0.2;  // s: a soft swish not sooner than this after the hand's last (a wave back and forth)
float VR_SWISH_LEAST = 0.04;    // volume: a softer swish isn't played
float VR_SWISH_PITCH = 6;       // percent: higher at a blow's most gain, lower at the softest swish
// (A blow's volume is by its speed as it lands, not a guess at its fastest to come: the author's takes of 2026-10-12
// land their punches at 1.37-1.6 times the landing speed and go 1.63-1.72 at most, his overhead cuts 1.9-2.7 and
// 2.4-3.1 (the far end's), but the frames' rise as they land, 0.3-0.8 a frame, foretells it at 2.6-5.9.) Its speed is
// this pose's or the last's, the faster, as the blow's own is a pose's (its four poses' mean lags: 0.85 for a punch
// landing at 1.43). A soft swish's is the mean of the last four poses' (VR_Swish_Mean): a swing's speed jumps from pose
// to pose (the takes' 0.66, 1.24, 1.16; where the frames beat, a mock punch at a steady 0.77 of its landing speed reads
// 1.23, 0.62, 0.62, 0.62), and such a dip is no slowing down (it played a soft swish just before the blow).

.float mh_sw_h1, mh_sw_h2, mh_sw_h3; // the shares of the three poses before
.float mh_sw_armed; // a soft swish may play (VR_Swish_Frame: the part slowed since the last)
.float mh_sw_peak;  // the motion's fastest share of its landing speed since
.float mh_sw_time;  // the hand's last swish (its own time: VR_Blow's mh_time)
.float mh_sw_blown; // the motion was a blow (its fastest is printed as the part slows: developer 1)
float vr_swish_blew; // VR_Melee_Whoosh played a blow's whoosh for the hand this frame (VR_Swish_Frame)
float vr_swish_chan; // VR_Swish_Sample's channel

float(float t) VR_Swish_Ease =
{
    t = clamp(t, 0, 1);
    return t * t * (3 - 2 * t);
};

// How fast `xHand`'s striking part goes, as a share of the speed its blow would land at.
float(float xHand, entity b) VR_Swish_Share =
{
    float need = VR_Melee_LandNeed(xHand, b, b.mh_vgrip);
    if(need <= 0)
    {
        return 0;
    }
    float s = vlen(b.mh_vgrip);
    if(b.mh_thing != VR_MTHING_FIST && b.mh_thing != VR_MTHING_GUN)
    {
        s = max(s, vlen(b.mh_vfar) / VR_MELEE_TIP_SLASH);
    }
    return s / need;
};

// That share over the last four poses (the frames' beat evened out).
float(float xHand, entity b) VR_Swish_Mean =
{
    return (VR_Swish_Share(xHand, b) + b.mh_sw_h1 + b.mh_sw_h2 + b.mh_sw_h3) * 0.25;
};

// The swish of what `xHand` swings, the knights' two swings (or a slap's two) in turn, its channel in vr_swish_chan.
string(float xHand, entity b, float slap) VR_Swish_Sample =
{
    vr_melee_whoosh_alt = !vr_melee_whoosh_alt;
    vr_swish_chan = VRGetHandChannel(xHand); // (any free one of the hand's: a blow's hit, on its gun channel or another, never cuts it off)
    if(slap)
    {
        return vr_melee_whoosh_alt ? "vr/slap_whoosh2.wav" : "vr/slap_whoosh1.wav";
    }
    if(b.mh_thing == VR_MTHING_AXE || b.mh_thing == VR_MTHING_CLUB || b.mh_thing == VR_MTHING_SAW ||
       b.mh_thing == VR_MTHING_CROWBAR)
    {
        return "weapons/ax1.wav"; // (the saw's engine sound is on the weapon's channel, vr_chainsaw.qc: the swish isn't)
    }
    return vr_melee_whoosh_alt ? "knight/sword2.wav" : "knight/sword1.wav";
};

// Plays `s` at `vol` (over 1: louder than full), `more` percent higher (and a little higher or lower each time).
void(string s, float chan, float vol, float more) VR_Swish_Play =
{
    if(vol == 1 && more == 0)
    {
        VR_SoundVaried(self, chan, s, 1, ATTN_NORM);
        return;
    }
    float j = clamp(cvar_hget(cvarh_vr_snd_pitch_jitter), 0, 25);
    sound_pitch(self, chan, s, clamp(vol, 0, 4), ATTN_NORM, 100 + more + crandom() * j);
};

// A blow's whoosh (VR_Melee_Whoosh): louder and a little higher the faster it lands.
void(float xHand, entity b, string s) VR_Swish_Blow =
{
    vr_swish_blew = TRUE;
    if(!cvar_hget(cvarh_vr_melee_swish) || b.mh_thing == VR_MTHING_GUN)
    {
        VR_Swish_Play(s, vr_swish_chan, 1, 0);
        return;
    }
    float x = max(VR_Swish_Share(xHand, b), b.mh_sw_h1); // (this pose's or the last: VR_Swish_Mean lags a blow)
    float u = VR_Swish_Ease((x - 1) / (max(1.05, cvar_hget(cvarh_vr_melee_swish_full)) - 1));
    float vol = 1 + (clamp(cvar_hget(cvarh_vr_melee_swish_gain), 1, 4) - 1) * u;
    dprint(sprintf("melee swish: blow, hand %g, %.2f of its landing speed, volume %.2f, %s\n", xHand, x, vol, s));
    VR_Swish_Play(s, vr_swish_chan, vol, VR_SWISH_PITCH * u);
};

// Every pose of `xHand` after VR_Melee_Whoosh: a soft swish for a motion short of a blow, at its fastest.
void(float xHand, entity b) VR_Swish_Frame =
{
    float x = VR_Swish_Mean(xHand, b);
    b.mh_sw_h3 = b.mh_sw_h2;
    b.mh_sw_h2 = b.mh_sw_h1;
    b.mh_sw_h1 = VR_Swish_Share(xHand, b);    float blew = vr_swish_blew;
    vr_swish_blew = FALSE;
    float least = clamp(cvar_hget(cvarh_vr_melee_swish_min), 0.05, 0.95);
    if(x < least * VR_SWISH_REARM)
    {
        if(b.mh_sw_blown)
        {
            dprint(sprintf("melee swish: the blow's motion, hand %g, went %.2f of its landing speed at most\n", xHand,
                b.mh_sw_peak));
        }
        b.mh_sw_armed = TRUE;
        b.mh_sw_blown = FALSE;
        b.mh_sw_peak = 0;
    }
    b.mh_sw_peak = max(b.mh_sw_peak, x);
    if(blew)
    {
        b.mh_sw_armed = FALSE;
        b.mh_sw_blown = TRUE;
        b.mh_sw_time = b.mh_time;
        return;
    }
    if(!b.mh_sw_armed || !cvar_hget(cvarh_vr_melee_swish) || b.mh_thing == VR_MTHING_GUN || b.mh_pushing)
    {
        return;
    }
    if(b.mh_sw_peak < least || x > b.mh_sw_peak * VR_SWISH_PEAK_FALL)
    {
        return;
    }
    b.mh_sw_armed = FALSE; // (once a motion)
    float t = VR_Swish_Ease((min(1, b.mh_sw_peak) - least) / (1 - least));
    float vol = clamp(cvar_hget(cvarh_vr_melee_swish_sub), 0, 1) * t;
    if(vol < VR_SWISH_LEAST || b.mh_time - b.mh_sw_time < VR_SWISH_COOLDOWN || VR_Melee_HandUnderwater(b))
    {
        return;
    }
    // An open hand whooshes only swung across, as a slap goes (VR_Melee_Slaps): its wave, reach or pat was heard as the
    // slap's swish, more often than the fist's (the author, 2026-10-19: "the slap seems to register too often").
    float open = b.mh_thing == VR_MTHING_FIST && !VR_Melee_ClosedFist(xHand);
    if(open && !VR_Melee_Slaps(xHand, b))
    {
        return;
    }
    b.mh_sw_time = b.mh_time;
    string s = VR_Swish_Sample(xHand, b, open);
    dprint(sprintf("melee swish: soft, hand %g, %.2f of its landing speed at most, volume %.2f, %s\n", xHand,
        b.mh_sw_peak, vol, s));
    VR_Swish_Play(s, vr_swish_chan, vol, -VR_SWISH_PITCH * (1 - t));
};

// The weapon's whoosh as `xHand`'s striking part gets going as a blow would (once a motion): as fast as a blow needs,
// come from some way off (and a fist closed). The motion recorder's "stroke". A blade's, a fist's or a gun's swish is the
// knights' two swings in turn (knight/sword1, sword2), as their own attacks pick them: not one recording every time; an
// open hand's slap, its own lighter swish (vr/slap_whoosh1, 2: make_sounds.py), in turn too.
void(float xHand, entity b) VR_Melee_Whoosh =
{
    float minSpeed = VR_Melee_MinSpeedOf(b);
    if(b.mh_fast < minSpeed * VR_MELEE_REARM_SLOW)
    {
        b.mh_whooshed = FALSE;
        b.mh_overland = 0;
        return;
    }
    float fastNow = vlen(b.mh_vgrip) >= VR_Melee_LandNeed(xHand, b, b.mh_vgrip);
    b.mh_overland = fastNow ? b.mh_overland + 1 : 0;
    if(b.mh_whooshed || b.mh_thing == VR_MTHING_NONE || !fastNow)
    {
        return;
    }
    // An open hand: no blow, but swung across as a slap is (VR_Melee_Slaps), the slap's own lighter whoosh, landing or
    // not (vrfiringrange_2026-10-08_10-48-14: "no sound during the attack, even if it doesn't connect").
    float slap = FALSE;
    if(b.mh_thing == VR_MTHING_FIST && !VR_Melee_ClosedFist(xHand))
    {
        // As fast two frames in a row: a frame's jitter (a slow wave's speed doubled where the frames beat) is no slap.
        if(b.mh_overland < 2 || !VR_Melee_Slaps(xHand, b))
        {
            return;
        }
        slap = TRUE;
    }
    // (As a blow's: VR_Melee_Decide. A weapon's far end going out along the line is a stab's: no slow swing's run-up.)
    float how = VR_MHOW_OTHER;
    if(b.mh_thing != VR_MTHING_FIST && b.mh_thing != VR_MTHING_GUN && vlen(b.mh_vfar) > 0 &&
       (b.mh_vfar * normalize(b.mh_rfar - b.mh_rgrip)) / vlen(b.mh_vfar) < VR_MELEE_STAB_ALONG)
    {
        how = VR_MHOW_SWING;
    }
    if(!VR_Melee_CameFar(b, how, vlen(b.mh_vgrip) / VR_Melee_LandNeed(xHand, b, b.mh_vgrip)) ||
       VR_Melee_PulledBack(b) ||
       !VR_Melee_WristMoved(b, how) || VR_Melee_Wiggled(b) || VR_Melee_Reloading(b))
    {
        return;
    }
    b.mh_whooshed = TRUE;
    self.show_hostile = time + 1; // a real swing or punch wakes monsters up (a hand merely closing doesn't: W_AttackImpl)
    float kind = VR_MKIND_SLASH;
    if(slap)
    {
        kind = VR_MKIND_SLAP;
    }
    else if(b.mh_thing == VR_MTHING_FIST)
    {
        kind = VR_MKIND_PUNCH;
    }
    VR_Motion_Event("stroke", VR_Melee_Category(kind, b.mh_thing), xHand, vlen(b.mh_vfar), b.mh_far, world, "");
    if(!slap && b.mh_thing == VR_MTHING_GUN && b.mh_vfar_z < -0.6 * vlen(b.mh_vfar))
    {
        vr_melee_whoosh_alt = !vr_melee_whoosh_alt;
        return; // a gun swung down: reaching for a holster to reload is fast too
    }
    if(!slap && b.mh_thing == VR_MTHING_FIST && VR_Melee_OpenHand(xHand))
    {
        vr_melee_whoosh_alt = !vr_melee_whoosh_alt;
        return; // (an open hand that doesn't slap: VR_Swish_Frame's soft swish)
    }
    string s = VR_Swish_Sample(xHand, b, slap);
    if(slap)
    {
        dprint(sprintf("melee sound: %s (a slap's whoosh, %.1f m/s, lands from %.1f)\n", s, vlen(b.mh_vgrip),
            VR_Melee_LandNeed(xHand, b, b.mh_vgrip)));
    }
    if(VR_Melee_HandUnderwater(b))
    {
        vr_swish_blew = TRUE;
        dprint(sprintf("melee sound: none (%s under water)\n", s));
        return; // no air to swish: the swing (a punch, a weapon) still counts
    }
    VR_Swish_Blow(xHand, b, s); // (louder the faster; a little higher or lower each time)
};

// ----------------------------------------------------------------------------
// Parry stamina and counter-attacks (docs/vr-port/ROUND21.md, "Parry stamina and counter-attacks")
//
// PARRY STAMINA (vr_parry_stamina; off as shipped): one pool for both hands -- your arms' and body's, not the
// weapon's, so switching hands doesn't rest you. A weapon's parry costs vr_parry_stamina_cost, held in two hands
// vr_parry_stamina_cost_2h; the parry that leaves none knocks the parrying weapon out of the hand (VR_Parry,
// combat.qc), one hand or two, and any parry while you have none does too. Stamina comes back at
// vr_parry_stamina_regen a second once you haven't parried for vr_parry_stamina_delay. On, it replaces the drop
// chance (vr_parry_drop_chance), which is not used. Crossed arms (the unarmed parry) cost nothing: there's no weapon
// to lose. The warning (vr_parry_stamina_warn): a breath and a throb in the hand when one more one-handed parry would
// knock the weapon away, a gasp and a long buzz when it is. What's left shows on the wrist gadget (vr_gadget_stamina):
// VR_Melee_Hud hands it and the counter's window to the engine (STAT_QVR_MELEE, vr_meleehud.cpp).
//
// COUNTER-ATTACKS (vr_counter): any parry (a weapon's or crossed arms) opens a window of vr_counter_window seconds in
// which your next melee attack -- a blow with either hand, a bash or a shove -- is a counter: vr_counter_damage times
// the damage (and a bash's or shove's knockback). One counter a parry. It is what the parry-bash was (a weapon bash
// 1.3x within 1.5 s of a weapon's parry), for every attack. The window opens with a blade's shing, the hands feel
// soft pulses while it lasts (vr_counter_haptic), fading as it closes, and what they hold may glow gold (vr_counter_glow:
// drawn by the engine from the window's share left, vr_meleehud.cpp); a counter lands with a heavy strike over the
// attack's own sound (vr_counter_sound).
//
// SHOVE AND STRIKE STAMINA (vr_shove_stamina, vr_strike_stamina; ROUND21.md, "Stamina for shoves and strikes; thrown
// damage on gibs"): the same pool pays for shoves and bashes that land (vr_shove_stamina_cost, two hands _2h) and for
// blows that land on something that takes damage (vr_strike_stamina_punch; a weapon's vr_strike_stamina_cost, in two
// hands _2h), once a motion however many things it goes through. Blows on walls cost nothing. What they can't pay they
// do weaker: with none left for it, a blow's, a shove's or a bash's damage is vr_stamina_exhausted_damage times as
// much, a shove's or a bash's knockback and stagger vr_stamina_exhausted_push times; with part of its cost left, in
// between (VR_Stamina_Mult). What counts as a blow or a shove is unchanged: only their damage and knockback scale.
// The rest before stamina comes back (vr_parry_stamina_delay) counts from the last parry, shove or blow that spent
// any. Each feature is its own switch; the pool is used when any of the three is on.
//
// With developer 1, "stamina:" and "counter:" lines trace both.

float VR_COUNTER_PULSE_EVERY = 0.25; // s between the pulses in the hands
float VR_COUNTER_PULSE_FIRST = 0.35; // s after the parry the first (the parry's own buzz lasts 0.3)

.float vr_stamina_used;     // on a player: the stamina spent (0: rested; out of vr_parry_stamina_max),
.float vr_stamina_time;     // when the last parry, shove or blow spent some,
.float vr_stamina_said;     // its coming back was traced (developer)
.float vr_climb_drain;      // on a player: 1 while hanging from a hold spends it (the engine's vr_climb.cpp, "Climbing stamina")
.float vr_counter_until;    // on a player: a counter may land until then (0: no window),
.float vr_counter_since;    // the window opened then,
.float vr_counter_pulse_at; // the next pulse
.float vr_melee_hud;        // on a player: what the gadget shows of both (VR_Melee_Hud; the engine's STAT_QVR_MELEE)

float() VR_Stamina_Max =
{
    return max(1, cvar_hget(cvarh_vr_parry_stamina_max));
};

// Whether climbing spends it: hanging from a hold (the engine drains it: vr_climb.cpp, "Climbing stamina").
float() VR_Stamina_Climbing =
{
    if(cvar_hget(cvarh_vr_climb) && cvar_hget(cvarh_vr_climb_stamina))
    {
        return TRUE;
    }
    return FALSE;
};

// Whether the pool is used: parries, shoves, blows or hanging from a hold spend it.
float() VR_Stamina_On =
{
    if(cvar_hget(cvarh_vr_parry_stamina) || cvar_hget(cvarh_vr_shove_stamina) || cvar_hget(cvarh_vr_strike_stamina))
    {
        return TRUE;
    }
    return VR_Stamina_Climbing();
};

// Stamina left at or under which it is low (the gadget blinks): one more of the dearest of what spends it (a
// one-handed parry, which knocks the weapon away; a one-handed shove; a one-handed weapon's blow) would empty it.
float() VR_Stamina_LowAt =
{
    float at = 0;
    if(cvar_hget(cvarh_vr_parry_stamina))
    {
        at = max(at, cvar_hget(cvarh_vr_parry_stamina_cost));
    }
    if(cvar_hget(cvarh_vr_shove_stamina))
    {
        at = max(at, cvar_hget(cvarh_vr_shove_stamina_cost));
    }
    if(cvar_hget(cvarh_vr_strike_stamina))
    {
        at = max(at, cvar_hget(cvarh_vr_strike_stamina_cost));
    }
    if(VR_Stamina_Climbing())
    {
        at = max(at, 3 * cvar_hget(cvarh_vr_climb_stamina_rate)); // three seconds' hanging from one hand
    }
    return at;
};

// Stamina left, 0 .. vr_parry_stamina_max.
float(entity p) VR_Stamina_Left =
{
    float most = VR_Stamina_Max();
    return min(most, max(0, most - p.vr_stamina_used));
};

// A weapon's parry by `p`'s `hand` (VR_Parry) spends stamina and warns: TRUE when it leaves none (the weapon is
// knocked away).
float(entity p, float hand, float twoHanded) VR_Stamina_Parry =
{
    float most = VR_Stamina_Max();
    float oneCost = max(0, cvar_hget(cvarh_vr_parry_stamina_cost));
    float cost = twoHanded ? max(0, cvar_hget(cvarh_vr_parry_stamina_cost_2h)) : oneCost;
    float before = VR_Stamina_Left(p);
    float left = max(0, before - cost);
    p.vr_stamina_used = most - left;
    p.vr_stamina_time = time;
    p.vr_stamina_said = FALSE;
    float knocked = FALSE;
    float low = FALSE;
    if(cost > 0 && left <= 0)
    {
        knocked = TRUE;
    }
    else if(oneCost > 0 && left <= oneCost)
    {
        low = TRUE; // one more one-handed parry knocks it away
    }

    entity oldSelf = self;
    self = p;
    float warn = min(1, max(0, cvar_hget(cvarh_vr_parry_stamina_warn)));
    if(warn > 0 && knocked)
    {
        // Out of breath: a gasp and a long, low buzz in the hand losing its weapon.
        sound(self, CHAN_AUTO, "player/gasp1.wav", warn, ATTN_NORM);
        haptic(hand, 0.0, 0.5, 30, warn);
    }
    else if(warn > 0 && low)
    {
        // Tiring: a breath, and a throb in the hand once the parry's buzz is over.
        sound(self, CHAN_AUTO, "player/gasp2.wav", 0.8 * warn, ATTN_NORM);
        haptic(hand, 0.32, 0.18, 30, 0.8 * warn);
    }
    self = oldSelf;

    dprint(sprintf("stamina: %g of %g left (%s, -%g)%s\n", rint(left), rint(most), twoHanded ? "two hands" : "one hand",
        cost, knocked ? ": none, the weapon is knocked away" : (low ? ": low" : "")));
    return knocked;
};

// The multiplier of an effort that could pay `xShare` (0 .. 1) of its cost: 1 paid in full, `xPenalty` (a cvar: 1 no
// penalty) with nothing, in between in proportion.
float(float xShare, float xPenalty) VR_Stamina_Mult =
{
    float pen = min(1, max(0, xPenalty));
    return pen + (1 - pen) * min(1, max(0, xShare));
};

// A shove's, a bash's or a blow's effort by `self` (`xWhat`, for the trace), costing `xCost`: spends what it can and
// returns the share of the cost paid (1: all, 0: none was left). The rest counts from now. Tiring
// (vr_parry_stamina_warn): the breath as it leaves stamina low (or empty) from above.
float(float xCost, string xWhat) VR_Stamina_Effort =
{
    float cost = max(0, xCost);
    float most = VR_Stamina_Max();
    float before = VR_Stamina_Left(self);
    float paid = min(before, cost);
    float left = before - paid;
    self.vr_stamina_used = most - left;
    self.vr_stamina_time = time;
    self.vr_stamina_said = FALSE;
    float share = 1;
    if(cost > 0)
    {
        share = paid / cost;
    }

    float lowAt = VR_Stamina_LowAt();
    float wasLow = FALSE;
    float isLow = FALSE;
    if(before <= 0 || (lowAt > 0 && before <= lowAt))
    {
        wasLow = TRUE; // (not `x = a || b`: see the fteqcc notes)
    }
    if(left <= 0 || (lowAt > 0 && left <= lowAt))
    {
        isLow = TRUE;
    }
    float warn = min(1, max(0, cvar_hget(cvarh_vr_parry_stamina_warn)));
    if(warn > 0 && isLow && !wasLow)
    {
        sound(self, CHAN_AUTO, "player/gasp2.wav", 0.8 * warn, ATTN_NORM); // (the hand feels the blow's own buzz)
    }
    string state = "";
    if(share < 1)
    {
        state = sprintf(": short, paid %.1f", paid);
    }
    else if(left <= 0)
    {
        state = ": none left";
    }
    else if(isLow)
    {
        state = ": low";
    }
    dprint(sprintf("stamina: %g of %g left (%s, -%g)%s\n", rint(left), rint(most), xWhat, cost, state));
    return share;
};

// Whether `p`'s hand `xHand` holds a very heavy melee weapon: the hunter ogres' hammer or mace (AD's: vr_ad_melee.qc).
// Their blows cost vr_admelee_heavy_stamina times as much stamina; holding one you move at vr_admelee_heavy_speed
// (VR_HeavyMelee_Frame).
float(entity p, float xHand) VR_ADMelee_VeryHeavy =
{
    float wid = VR_HAND_FIELD(p, xHand, weapon, weapon2);
    float flags = VR_HAND_FIELD(p, xHand, weaponflags, weaponflags2);
    return wid == WID_SWORD && VR_ADMelee_Heavy(VR_ADMelee_Kind(flags));
};

// Each frame (VR_Stamina_Frame's caller): the move speed's share for the very heavy melee weapon in either hand (.vr_heavy_move:
// the engine's SV_AirMove reads it; 0 or 1: full speed).
.float vr_heavy_move;
void() VR_HeavyMelee_Frame =
{
    float slow = VR_ADMelee_VeryHeavy(self, cVR_MainHand) || VR_ADMelee_VeryHeavy(self, cVR_OffHand);
    self.vr_heavy_move = slow ? clamp(cvar_hget(cvarh_vr_admelee_heavy_speed), 0.05, 1) : 1;
};

// A blow of `xHand` of `self` (VR_Melee_Strike) landing on something that takes damage: its motion's stamina (once a
// motion), and the damage multiplier that leaves it (mh_exhaust; 0 when paid in full or not counted).
void(float xHand, entity b) VR_Stamina_Strike =
{
    if(b.mh_stam_paid)
    {
        return; // (this motion paid as it struck the first thing)
    }
    b.mh_stam_paid = TRUE;
    b.mh_exhaust = 0;
    if(!cvar_hget(cvarh_vr_strike_stamina))
    {
        return;
    }
    float cost = cvar_hget(cvarh_vr_strike_stamina_cost);
    string what = "a blow, one hand";
    if(b.mh_thing == VR_MTHING_FIST)
    {
        cost = cvar_hget(cvarh_vr_strike_stamina_punch);
        what = "a punch";
    }
    else if(VRGet2HAiming(self) && VRIsHandEmpty(self, VRGetOtherHand(xHand)))
    {
        cost = cvar_hget(cvarh_vr_strike_stamina_cost_2h);
        what = "a blow, two hands";
    }
    // (The very heavy ones, the hunter ogres' hammer and mace: more; author's note 2026-10-22.)
    if(b.mh_thing != VR_MTHING_FIST && VR_ADMelee_VeryHeavy(self, xHand))
    {
        cost = cost * max(0, cvar_hget(cvarh_vr_admelee_heavy_stamina));
        what = strcat(what, ", very heavy");
    }
    float share = VR_Stamina_Effort(cost, what);
    if(share < 1)
    {
        b.mh_exhaust = VR_Stamina_Mult(share, cvar_hget(cvarh_vr_stamina_exhausted_damage));
        dprint(sprintf("stamina: the blow deals x%.2f\n", b.mh_exhaust));
    }
};

// Each frame (VR_Melee_Frame): stamina comes back once no parry, shove or blow has spent any for
// vr_parry_stamina_delay.
void() VR_Stamina_Frame =
{
    if(!VR_Stamina_On())
    {
        self.vr_stamina_used = 0; // off: rested when turned on
        return;
    }
    if(self.vr_stamina_used <= 0 || time - self.vr_stamina_time < max(0, cvar_hget(cvarh_vr_parry_stamina_delay)))
    {
        return;
    }
    if(!self.vr_stamina_said)
    {
        self.vr_stamina_said = TRUE;
        dprint(sprintf("stamina: %g left, coming back (%.2f s after the last parry, shove or blow)\n", rint(VR_Stamina_Left(self)),
            time - self.vr_stamina_time));
    }
    self.vr_stamina_used = max(0, self.vr_stamina_used - max(0, cvar_hget(cvarh_vr_parry_stamina_regen)) * frametime);
    if(self.vr_stamina_used <= 0)
    {
        dprint(sprintf("stamina: rested, %g (%.2f s after the last parry, shove or blow)\n", rint(VR_Stamina_Max()),
            time - self.vr_stamina_time));
    }
};

// A parry by `p` (VR_Parry; `hand` -1: crossed arms) opens a counter's window.
void(entity p, float hand) VR_Counter_Open =
{
    float window = cvar_hget(cvarh_vr_counter_window);
    if(!cvar_hget(cvarh_vr_counter) || window <= 0)
    {
        return;
    }
    p.vr_counter_since = time;
    p.vr_counter_until = time + window;
    p.vr_counter_pulse_at = time + VR_COUNTER_PULSE_FIRST;
    float volume = min(1, max(0, cvar_hget(cvarh_vr_counter_sound)));
    if(volume > 0)
    {
        sound(p, hand >= 0 ? VRGetHandChannel(hand) : CHAN_AUTO, "vr/counter_open.wav", volume, ATTN_NORM);
    }
    dprint(sprintf("counter: open for %.2f s (%s)\n", window, hand < 0 ? "crossed arms" : sprintf("hand %g", hand)));
};

// Whether `self`'s next melee attack is a counter.
float() VR_Counter_Ready =
{
    return self.vr_counter_until > time && cvar_hget(cvarh_vr_counter);
};

// `self`'s melee attack `xWhat` landing on `xOn` in the window is the counter (one a parry): its sound; its damage
// multiplier.
float(string xWhat, entity xOn) VR_Counter_Use =
{
    float mult = max(0.1, cvar_hget(cvarh_vr_counter_damage));
    dprint(sprintf("counter: %s on %s, x%.2f, %.2f s after the parry\n", xWhat, xOn.classname, mult,
        time - self.vr_counter_since));
    self.vr_counter_until = 0;
    VR_Highlight_Counter(xWhat, xOn);
    float volume = min(1, max(0, cvar_hget(cvarh_vr_counter_sound)));
    if(volume > 0)
    {
        sound(self, CHAN_AUTO, "vr/counter.wav", volume, ATTN_NORM);
    }
    return mult;
};

// The share of `p`'s counter window left: 1 as it opens .. 0 (0: none open).
float(entity p) VR_Counter_Left =
{
    if(p.vr_counter_until <= time)
    {
        return 0;
    }
    return min(1, (p.vr_counter_until - time) / max(0.01, p.vr_counter_until - p.vr_counter_since));
};

// Each frame (VR_Melee_Frame): while the window is open, the pulses, fading as it closes.
void() VR_Counter_Frame =
{
    if(self.vr_counter_until <= 0)
    {
        return;
    }
    if(time >= self.vr_counter_until || !cvar_hget(cvarh_vr_counter))
    {
        self.vr_counter_until = 0;
        dprint(sprintf("counter: closed unused (%.2f s after the parry)\n", time - self.vr_counter_since));
        return;
    }
    float left = VR_Counter_Left(self);
    float strength = min(1, max(0, cvar_hget(cvarh_vr_counter_haptic)));
    if(strength > 0 && time >= self.vr_counter_pulse_at)
    {
        self.vr_counter_pulse_at = time + VR_COUNTER_PULSE_EVERY;
        float amp = strength * (0.15 + 0.25 * left);
        haptic(cVR_MainHand, 0.0, 0.04, 160, amp);
        haptic(cVR_OffHand, 0.0, 0.04, 160, amp);
    }
};

// Each frame (VR_Melee_Frame), after both: what the wrist gadget shows of them (vr_gadget_stamina) and the counter
// glow (vr_counter_glow), for the engine (.vr_melee_hud, sent as STAT_QVR_MELEE; vr_meleehud.cpp): the stamina left in
// percent, rounded up (0 only when none is), + 128 with the pool on (parry, shove or strike stamina), + 256 when low
// (VR_Stamina_LowAt; with parry stamina, one more one-handed parry would knock the weapon away), + 512 while it comes
// back, + 1024 x the counter window's share left in 63rds (0: none
// open), + 65536 while hanging from a hold drains it (.vr_climb_drain). Integers under 2^17, exact in a float.
void() VR_Melee_Hud =
{
    float hud = 0;
    if(VR_Stamina_On())
    {
        float left = VR_Stamina_Left(self);
        float oneCost = VR_Stamina_LowAt();
        hud = 128 + min(100, ceil(100 * left / VR_Stamina_Max()));
        if(left > 0 && oneCost > 0 && left <= oneCost)
        {
            hud = hud + 256;
        }
        if(self.vr_stamina_used > 0 && time - self.vr_stamina_time >= max(0, cvar_hget(cvarh_vr_parry_stamina_delay)))
        {
            hud = hud + 512;
        }
        if(self.vr_climb_drain)
        {
            hud = hud + 65536; // hanging drains it
        }
    }
    self.vr_melee_hud = hud + 1024 * ceil(63 * VR_Counter_Left(self));
};

// Tests (impulse 242, single player): the nearest monster that strikes in melee, within the parry's reach (150 units),
// deals `self` a blow of 10 now, as its own attack would -- parried or not as the pose is (VR_Parry). With `notarget`
// the monster stands still and blows come only when a script says.
void() VR_Parry_TestBlow =
{
    entity best = world;
    float bestDist = 150;
    entity e = findradius(self.origin, 150);
    while(e)
    {
        if(e.health > 0 && (e.flags & FL_MONSTER) && VR_Parry_MeleeMonster(e) && vlen(e.origin - self.origin) <= bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - self.origin);
        }
        e = e.chain;
    }
    if(best == world)
    {
        dprint("parry test: no monster that strikes in melee within 150 units\n");
        return;
    }
    dprint(sprintf("parry test: %s strikes, %.0f units away\n", best.classname, bestDist));
    T_Damage(self, best, best, 10);
};

// ----------------------------------------------------------------------------
// The stance and the bash

// The stance of `xHand` of `self` facing `xForward` (world, flat): 1, a weapon held across (the parry's pose: the
// weapon's line, or the hands' line held two-handed, level, across and in front); 2, an open palm facing ahead;
// 0, none.
float(float xHand, entity b, vector xForward) VR_Bash_Stance =
{
    if(b.mh_thing == VR_MTHING_NONE)
    {
        return 0;
    }
    if(b.mh_thing == VR_MTHING_FIST)
    {
        if(VR_Melee_OpenHand(xHand) && VR_Melee_Palm(xHand) * xForward > VR_BASH_PALM)
        {
            return 2;
        }
        return 0;
    }
    return VR_Parry_Blocks(self, xHand, xForward) ? 1 : 0;
};

// The stance's velocity (play space, m/s): a weapon's middle, or the palm.
vector(entity b) VR_Bash_PushVel =
{
    if(b.mh_stance == 2)
    {
        return b.mh_vgrip;
    }
    return (b.mh_vnear + b.mh_vfar) * 0.5;
};

// Whether `xHand`'s grip moving at `v` (play space) extends the arm: out from its shoulder (a hand waved down or round,
// palm first, is no shove). `xFwd`: the facing (play space).
float(float xHand, entity b, vector v, vector xFwd) VR_Bash_Extends =
{
    return v * VR_Melee_ArmOut(xHand, b, xFwd) >= vlen(v) * VR_BASH_PALM_RADIAL;
};

// Whether the flashlight's hand `xHand` pushes along with the other's open palm (a two-handed shove, the torch in one
// fist): its grip going forward (play space `xFwd`) at `xSpeed` (m/s, a bash's: VR_Bash_Pushing's share for a shove
// is taken here) or more, extending the arm as a shove's palm does. Alone it is no shove: no palm (VR_Bash_Stance).
float(float xHand, entity b, vector xFwd, float xSpeed) VR_Bash_TorchPushing =
{
    if(!b.mh_fresh || !VR_Melee_HoldsTorch(self, xHand) || b.mh_thing != VR_MTHING_FIST)
    {
        return FALSE;
    }
    vector v = b.mh_vgrip;
    if(!VR_Bash_Extends(xHand, b, v, xFwd))
    {
        return FALSE;
    }
    xSpeed = xSpeed * max(0.1, cvar_hget(cvarh_vr_shove_speed)) / max(0.1, cvar_hget(cvarh_vr_bash_speed));
    float ahead = v * xFwd;
    return ahead >= xSpeed && ahead >= vlen(v) * VR_BASH_AHEAD;
};

// Whether `xHand`'s stance is pushed as a bash is this frame, at `xSpeed` (m/s) or more: forward (play space
// `xFwd`), as a whole (a weapon's two ends moving alike; a palm moving the way it faces).
float(float xHand, entity b, vector xFwd, float xSpeed) VR_Bash_Pushing =
{
    vector v;
    if(b.mh_stance == 2)
    {
        v = b.mh_vgrip;
        vector palm = VR_Blow_ToPlaySpace(VR_Melee_Palm(xHand));
        if(v * palm < vlen(v) * VR_BASH_PALM_ALONG)
        {
            return FALSE;
        }
        if(!VR_Bash_Extends(xHand, b, v, xFwd))
        {
            return FALSE;
        }
        xSpeed = xSpeed * max(0.1, cvar_hget(cvarh_vr_shove_speed)) / max(0.1, cvar_hget(cvarh_vr_bash_speed));
    }
    else
    {
        // Both ends advance: a cut through the stance sweeps its far end across or down, not forward. (One end may go
        // faster: the author's pushes with one hand swing the tip forward with the wrist, 2-3 times the pommel's speed.)
        v = (b.mh_vnear + b.mh_vfar) * 0.5;
        float an = b.mh_vnear * xFwd;
        float af = b.mh_vfar * xFwd;
        if(an < vlen(b.mh_vnear) * VR_BASH_END_AHEAD || af < vlen(b.mh_vfar) * VR_BASH_END_AHEAD ||
           min(an, af) < xSpeed * VR_BASH_END_SPEED)
        {
            return FALSE;
        }
    }
    float ahead = v * xFwd;
    return ahead >= xSpeed && ahead >= vlen(v) * VR_BASH_AHEAD;
};

// What is just ahead of the stance of `xHand` (VR_BASH_REACH, along `xForward`): the weapon's line (or the palm)
// swept forward, against monsters' boxes. Into vr_bash_victims (a chain), how many.
entity vr_bash_victims;
float(float xHand, entity b, vector xForward) VR_Bash_Find =
{
    vector a = b.mh_near;
    vector c = b.mh_far;
    if(b.mh_stance == 2)
    {
        a = b.mh_grip - xForward * 2;
        c = b.mh_grip + xForward * 2;
    }
    float reach = VR_BASH_REACH * VRMetersToUnits();
    float pad = 6;
    float hits = 0;
    vr_bash_victims = world;
    entity e = findradius((a + c) * 0.5 + xForward * reach * 0.5, reach + vlen(c - a) * 0.5 + 64);
    while(e)
    {
        if(e != self && e.takedamage && e.health > 0 && (e.flags & FL_MONSTER) && e.vr_bash_mark != time)
        {
            // Precise hits (vr_hit_precise): the box (grown by VR_HITMODEL_REACH) the broad phase, the model as drawn
            // (grown by the pad and the melee tolerance) what is shoved.
            float precise = hitmodel_target(e);
            float boxPad = pad;
            if(precise)
            {
                boxPad = pad + VR_HITMODEL_REACH;
            }
            vector lo = e.origin + e.mins - '1 1 1' * boxPad;
            vector hi = e.origin + e.maxs + '1 1 1' * boxPad;
            float k;
            for(k = 0; k <= 3; ++k)
            {
                vector off = xForward * (reach * k / 3);
                if(VR_SegmentBox(a + off, c + off, lo, hi) >= 0 &&
                   (!precise || hitmodel_segment(e, a + off, c + off, pad, HITCLASS_MELEE) >= 0))
                {
                    e.vr_bash_mark = time;
                    e.vr_bash_next = vr_bash_victims;
                    vr_bash_victims = e;
                    hits = hits + 1;
                    break;
                }
            }
        }
        e = e.chain;
    }
    return hits;
};

// A shove's or a bash's push by its hands (vr_shove_push_onehand, _twohand; times vr_bash_push in VR_Bash_Hit).
float(float xBoth) VR_Shove_HandsPush =
{
    return max(0, cvar_hget(xBoth ? cvarh_vr_shove_push_twohand : cvarh_vr_shove_push_onehand));
};

// A shove's surprise (vr_knockdown_surprise; vr_knockdown.qc VR_Knockdown_Try): 1, `e` unaware of `by` (its enemy not
// him: asleep, idle, fighting another), 2, shoved from behind (the shove's way within vr_knockdown_surprise_angle of
// where it faces), 0 neither. Taken as the blow finds it, before its damage wakes it (VR_Bash_Hit: vr_kd_surprise).
float vr_kd_surprise;
float vr_kd_stagger_far;       // VR_Bash_Hit's: its stagger's travel limit times this (vr_knockdown.qc VR_Knockdown_Stagger; 0: 1)
float VR_SHOVE_RUN_MIN = 40;   // u/s along the shove: slower is standing (vr_shove_run_carry carries nothing)
float vr_test_shove_run;       // u/s: vr_knockdown_test 21/22's run into the shove (vr_test_shove_run; 0: your own velocity)
float(entity e, entity by, vector xForward) VR_Knockdown_Surprised =
{
    if(!cvar_hget(cvarh_vr_knockdown_surprise) || !(e.flags & FL_MONSTER) || e.health <= 0)
    {
        return 0;
    }
    if(e.enemy != by)
    {
        return 1;
    }
    float half = cvar_hget(cvarh_vr_knockdown_surprise_angle);
    vector f = xForward;
    f_z = 0;
    if(half <= 0 || f == '0 0 0')
    {
        return 0;
    }
    return fabs(anglemod(vectoyaw(f) - e.angles_y + 180) - 180) <= half ? 2 : 0;
};

// Knocks `e` back along `xForward`: `xScale` of vr_bash_damage and `xPush` of vr_bash_push, and a stagger (its pain,
// forced).
void(entity e, vector xAt, vector xForward, float xKind, float xHand, float xScale, float xPush) VR_Bash_Hit =
{
    vr_kd_surprise = VR_Knockdown_Surprised(e, self, xForward);
    if(self.flags & FL_CLIENT)
    {
        self.vr_blow_time = time; // (bullet time's screen tap: vr_bullettime_tap_blow_quiet)
    }
    float push = cvar_hget(cvarh_vr_bash_push) * xPush;
    float dmg = cvar_hget(cvarh_vr_bash_damage) * xScale;
    float stagger = xScale;
    if(vr_bash_exhaust_dmg > 0)
    {
        // Short of stamina (VR_Bash): weaker, thrown back less, staggered less long.
        dmg = dmg * vr_bash_exhaust_dmg;
        push = push * vr_bash_exhaust_push;
        stagger = stagger * vr_bash_exhaust_push;
    }
    if(e.flags & FL_MONSTER)
    {
        push = push * max(0, monstergroupget(e, MGP_PUSH)); // (its monster group's Shove Push: Combat > Monster Groups)
    }
    VR_SetHitKind(xKind, xHand, xAt, xForward);
    if(T_Damage(e, self, self, dmg) && dmg > 0)
    {
        VR_QuadSound(self, "a bash or shove hurt");
    }
    VR_ClearHitKind();
    // Running into it (vr_shove_run_carry): your own speed along the shove carried into it on top of the push (yours as
    // you move, not scaled by Knockback), times its group's Shove Push over its weight (VR_Push_Mass), at most all of it:
    // a running shove sends it farther and faster; standing (under VR_SHOVE_RUN_MIN) as before.
    float run = 0;
    vector way = xForward;
    way_z = 0;
    if((self.flags & FL_CLIENT) && (e.flags & FL_MONSTER) && push > 0 && vlen(way) > 0)
    {
        vector runVel = self.velocity;
        if(vr_test_shove_run > 0)
        {
            runVel = normalize(way) * vr_test_shove_run; // (vr_knockdown_test 21/22's vr_test_shove_run)
        }
        runVel_z = 0;
        run = runVel * normalize(way);
        run = run >= VR_SHOVE_RUN_MIN ? run * max(0, cvar_hget(cvarh_vr_shove_run_carry)) : 0;
        run = run * min(1, max(0, monstergroupget(e, MGP_PUSH)) / VR_Push_Mass(e));
    }
    float knock = max(0.01, cvar_hget(cvarh_vr_push));
    float pushSpeed = 520 * push;
    float carried = run * VR_Push_Mass(e) / knock; // (VR_Push's units: times Knockback, over its weight)
    // Its stagger's travel limit (ragdollstagger's `far`): the hands' push over 1 as much farther as its slide (the
    // square of its speed: two hands' 1.12, 1.25 times), times the run's speed-up (linear: 160 u/s still short of 320's).
    float speedUp = pushSpeed > 0 ? (pushSpeed + carried) / pushSpeed : 1;
    float hands = max(1, VR_Shove_HandsPush(xHand == -1));
    vr_kd_stagger_far = hands * hands * speedUp;
    VR_Push(e, xForward, pushSpeed + carried, 140 * push);
    dprint(sprintf("bash hit: %s, %.1f damage, knocked back at %.0f u/s (%.0f of it your run's), staggered %.2f s\n",
        e.classname, dmg, vlen(e.velocity), run, stagger));
    if(e.health > 0 && e.th_pain && !e.vr_knockdown)
    {
        entity oldSelf = self;
        self = e;
        e.pain_finished = 0;
        e.th_pain(oldSelf, dmg);
        e.attack_finished = time + stagger;
        self = oldSelf;
    }
    else if(e.health > 0 && e.vr_ad_mon && !e.vr_knockdown)
    {
        VR_AD_Pain(e, self, dmg, stagger); // AD support: AD's pain function (vr_ad_monsters.qc)
    }
    // A knockdown (vr_knockdown.qc): by chance, the shove knocks it down as a ragdoll.
    VR_Knockdown_Try(e, xForward, xKind, xHand, push);
    vr_kd_surprise = 0;
    vr_kd_stagger_far = 0;
    if(!(e.flags & (FL_MONSTER | FL_CLIENT)) || VR_Hit_Metal(e))
    {
        // Wood's splinters, metal's sparks; never blood: a shove or a bash draws none (combat.qc VR_HitBloodless).
        VR_HitBloodSplash(e, e.origin + '0 0 16', 4);
    }
};

// A push's stamina (vr_shove_stamina), once a push (`xBoth`: two hands; `xArmed`: a bash), whether it lands or bats a
// projectile first (VR_Bash_Deflect): the exhaustion's multipliers into vr_bash_exhaust_dmg and _push (0: paid in full).
void(float xBoth, float xArmed) VR_Bash_Pay =
{
    if(self.vr_bash_paid == self.vr_push_start && self.vr_push_start > 0)
    {
        vr_bash_exhaust_dmg = self.vr_bash_paid_dmg;
        vr_bash_exhaust_push = self.vr_bash_paid_push;
        return;
    }
    vr_bash_exhaust_dmg = 0;
    vr_bash_exhaust_push = 0;
    if(cvar_hget(cvarh_vr_shove_stamina))
    {
        float share = VR_Stamina_Effort(cvar_hget(xBoth ? cvarh_vr_shove_stamina_cost_2h : cvarh_vr_shove_stamina_cost),
            sprintf("a %s, %s", xArmed ? "bash" : "shove", xBoth ? "two hands" : "one hand"));
        if(share < 1)
        {
            vr_bash_exhaust_dmg = VR_Stamina_Mult(share, cvar_hget(cvarh_vr_stamina_exhausted_damage));
            vr_bash_exhaust_push = VR_Stamina_Mult(share, cvar_hget(cvarh_vr_stamina_exhausted_push));
            dprint(sprintf("stamina: the %s deals x%.2f, knocks back x%.2f\n", xArmed ? "bash" : "shove",
                vr_bash_exhaust_dmg, vr_bash_exhaust_push));
        }
    }
    self.vr_bash_paid = self.vr_push_start;
    self.vr_bash_paid_dmg = vr_bash_exhaust_dmg;
    self.vr_bash_paid_push = vr_bash_exhaust_push;
};

// The way a bash sends a projectile it meets (ROUND21.md, "Catching versus deflecting; returned grenades; sword bash
// direction"). Met by a weapon's line (`xHand`'s, `xT` along it from its near end): off the blade's face, the way the
// blade's point moves across the blade (its velocity in the push, vr_bashdef_mvn/_mvf, mixed along the rigid line)
// with its share along the blade taken off (it slides along a blade, which can't push it that way). The point: two
// hands on one weapon, the blade's middle (between the grip and the far end), so neither hand's push pulls it aside;
// one hand, where it met the blade. So a blade held level and square in front of the torso sends it straight ahead,
// whichever hand pushes harder, and tilting the blade turns it (20 degrees, 20 degrees). A palm, a fist, the line
// between the hands, or a push along the blade (a thrust: less than VR_BASH_FACE_MIN across it): the push's way
// `xPush`, as before. `xForward`: the facing.
vector(float xHand, float xT, vector xPush, vector xForward) VR_Bash_Direction =
{
    vector pushDir = normalize(xPush);
    if(vlen(xPush) < 1)
    {
        pushDir = xForward;
    }
    if(xHand < 0)
    {
        return pushDir;
    }
    entity b = VR_Blow(xHand);
    if(b.mh_thing == VR_MTHING_FIST || b.mh_thing == VR_MTHING_NONE || vlen(b.mh_far - b.mh_near) < 1)
    {
        return pushDir;
    }
    vector axis = normalize(b.mh_far - b.mh_near);
    vector vn = xHand == cVR_OffHand ? self.vr_bashdef_ovn : self.vr_bashdef_mvn;
    vector vf = xHand == cVR_OffHand ? self.vr_bashdef_ovf : self.vr_bashdef_mvf;
    float t = xT;
    float two = VRGet2HAiming(self) && VRIsHandEmpty(self, VRGetOtherHand(xHand));
    if(two)
    {
        float grip = vlen(b.mh_grip - b.mh_near) / vlen(b.mh_far - b.mh_near);
        t = grip + (1 - grip) * 0.5;
    }
    vector v = vn + (vf - vn) * t;
    if(vlen(v) < 1)
    {
        v = xPush;
    }
    vector across = v - axis * (v * axis);
    if(vlen(across) < VR_BASH_FACE_MIN * vlen(v) || across * xForward <= 0)
    {
        dprint(sprintf("deflect: the point %.2f along the blade %v moves %v, along it: the push's way\n", t, axis, v));
        return pushDir;
    }
    vector dir = normalize(across);
    dprint(sprintf("deflect: off the blade %v (%s), its point %.2f along moving %v: sent %v, %.1f deg from the push %v\n", axis,
        two ? "two hands: its middle" : "where it met", t, v, dir, VRAngleBetween(dir, pushDir), pushDir));
    return dir;
};

// Batting with a bash (vr_deflect; round 20, ROUND21.md "Deflection by blows and bashes"): for vr_bash_deflect_window
// seconds from the push, a monster's projectile whose path this frame (from a little behind it to where it will be at
// the frame's end) comes within vr_bash_deflect_radius of the guard -- each armed hand's weapon or club line
// (VR_Melee_HoldsWeapon), and the line between the hands while they are together and both armed; never an empty
// hand -- and is ahead of the facing `xForward`, is sent the way the hands push (vr_bashdef_vel), as fast as the bash
// is strong: VR_BASH_BAT_SPEED times the push's speed over
// vr_bash_speed (VR_BASH_BAT_MIN .. VR_BASH_BAT_MAX), one hand's VR_BASH_ONE_PUSH of it, short of stamina its
// Exhausted Knockback (the push's stamina, VR_Bash_Pay). Met by a weapon's line, off the blade's face
// (VR_Bash_Direction). Then the aim assist (VR_Deflect_Assist, vr_juice.qc). A shove (open palms) bats nothing.
void(vector xForward) VR_Bash_Deflect =
{
    float radius = max(0, cvar_hget(cvarh_vr_bash_deflect_radius));
    float dt = max(frametime, 0.011);
    entity bm = VR_Blow(cVR_MainHand);
    entity bo = VR_Blow(cVR_OffHand);
    vector mh = VRGetHandPos(cVR_MainHand);
    vector oh = VRGetHandPos(cVR_OffHand);
    float mArmed = VR_Melee_HoldsWeapon(bm.mh_thing);
    float oArmed = VR_Melee_HoldsWeapon(bo.mh_thing);
    if(!mArmed && !oArmed)
    {
        return; // (the weapon let go of during the push)
    }
    float together = mArmed && oArmed && vlen(mh - oh) < 0.7 * VRMetersToUnits();
    vector head = VRGetHeadPos(self);

    entity e = findradius((mh + oh) * 0.5, 200);
    while(e)
    {
        if(VR_Deflect_IsProjectile(e) && time >= e.vr_deflect_time + 0.3 && (e.origin - head) * xForward > -8)
        {
            vector from = e.origin - e.velocity * max(dt, VR_BAT_LATE * 2);
            vector to = e.origin + e.velocity * dt;
            // (Where it met: a hand's line, how far along it; -1 the line between the hands.)
            float near = 9999;
            float onHand = cVR_MainHand;
            float onT = 0;
            if(mArmed)
            {
                near = VR_SegmentDistance(from, to, bm.mh_near, bm.mh_far);
                onT = vr_seg_t;
            }
            if(oArmed)
            {
                float nearOff = VR_SegmentDistance(from, to, bo.mh_near, bo.mh_far);
                if(nearOff < near)
                {
                    near = nearOff;
                    onHand = cVR_OffHand;
                    onT = vr_seg_t;
                }
            }
            if(together)
            {
                float nearBoth = VR_SegmentDistance(from, to, mh, oh);
                if(nearBoth < near)
                {
                    near = nearBoth;
                    onHand = -1;
                }
            }
            if(near < radius + e.maxs_x)
            {
                dprint(sprintf("deflect: accepted %s of %s, %.0f units from the bash's guard, %.2f s into the push\n",
                    e.classname, e.owner.classname, near,
                    time - (self.vr_bashdef_until - max(0, cvar_hget(cvarh_vr_bash_deflect_window)))));
                vector pushVel = self.vr_bashdef_vel;
                float strength = vlen(pushVel) / (max(0.1, cvar_hget(cvarh_vr_bash_speed)) * VRMetersToUnits());
                float batSpeed = VR_BASH_BAT_SPEED * min(VR_BASH_BAT_MAX, max(VR_BASH_BAT_MIN, strength));
                if(!self.vr_bashdef_both)
                {
                    batSpeed = batSpeed * VR_BASH_ONE_PUSH;
                }
                VR_Bash_Pay(self.vr_bashdef_both, TRUE); // (a bash's: only a bash bats)
                if(vr_bash_exhaust_push > 0)
                {
                    batSpeed = batSpeed * vr_bash_exhaust_push;
                }
                dprint(sprintf("deflect: the push %v (%.0f u/s, x%.2f the bash speed), %s\n", pushVel, vlen(pushVel),
                    strength, self.vr_bashdef_both ? "two hands" : "one hand"));
                vector pushDir = VR_Bash_Direction(onHand, onT, pushVel, xForward);
                VR_Deflect_Send(e, pushDir * batSpeed, self.vr_bashdef_hand, "bash");
            }
        }
        e = e.chain;
    }
};

// Starts a bash's batting (vr_deflect), with `xHand` (-1: both), unless it is going on already; the push's velocity
// `xPushVel` (play space, m/s) is kept while it goes on (what it bats goes that way).
void(float xHand, vector xForward, vector xPushVel, float xBoth) VR_Bash_DeflectStart =
{
    if(!cvar_hget(cvarh_vr_deflect))
    {
        return;
    }
    self.vr_bashdef_vel = VR_Blow_ToWorld(xPushVel) * VRMetersToUnits();
    if(time > self.vr_bashdef_until)
    {
        self.vr_bashdef_hand = xHand;
        self.vr_bashdef_both = xBoth;
        self.vr_bashdef_until = time + max(0, cvar_hget(cvarh_vr_bash_deflect_window));
        VR_Bash_Deflect(xForward);
    }
};

// Follows each hand's stance and push; a push of a stance far enough is a bash (or a shove, open palms): what is just
// ahead is knocked back, once. Runs after the hands are tracked and before their blows: the hands in a push don't
// also strike.
void() VR_Bash =
{
    entity bm = VR_Blow(cVR_MainHand);
    entity bo = VR_Blow(cVR_OffHand);
    bm.mh_pushing = FALSE;
    bo.mh_pushing = FALSE;
    if(!cvar_hget(cvarh_vr_bash) || !self.ishuman || self.health <= 0 || !VRHandsTracked(self))
    {
        bm.mh_stance = bo.mh_stance = 0;
        bm.mh_push_dist = bo.mh_push_dist = 0;
        return;
    }

    // The body's facing is the head's (.v_angle is the main hand's aim: a weapon held across in front aims sideways).
    vector fwd = VRForwardOf('0 1 0' * self.v_viewangle_y);
    vector pfwd = VR_Blow_Facing();
    if(time <= self.vr_bashdef_until)
    {
        VR_Bash_Deflect(fwd); // a bash's batting goes on
    }

    float speed = max(0.1, cvar_hget(cvarh_vr_bash_speed));
    float h;
    entity b;
    float readyHand = -1;
    for(h = 0; h < 2; ++h)
    {
        b = h == cVR_OffHand ? bo : bm;
        float stance = VR_Bash_Stance(h, b, fwd);
        if(stance != b.mh_stance)
        {
            b.mh_stance = stance;
            b.mh_stance_since = VR_PlayerTime();
            b.mh_stance_said = FALSE;
        }
        if(stance == 1 && !b.mh_stance_said && VR_PlayerTime() - b.mh_stance_since >= VR_STANCE_REPORT)
        {
            // (The parry poses are reported: a weapon across, crossed arms; not a palm facing ahead.)
            b.mh_stance_said = TRUE;
            VR_Melee_Event(sprintf("stance weapon, hand %g%s", h, VRGet2HAiming(self) ? ", two hands" : ""));
            VR_Motion_Event("stance", "weapon", h, 0, VRGetHandPos(h), world, VRGet2HAiming(self) ? "two hands" : "");
        }
        if(stance && cvar_hget(cvarh_developer) >= 2 && VR_Bash_PushVel(b) * pfwd > speed * 0.5)
        {
            dprint(sprintf("melee debug: hand %g stance %g push %v ends %v %v dist %.3f\n", h, stance,
                VR_Bash_PushVel(b), b.mh_vnear, b.mh_vfar, b.mh_push_dist));
            dprint(sprintf("melee debug: held %.2f ok %g pushing %g\n", VR_PlayerTime() - b.mh_stance_since, b.mh_push_ok,
                VR_Bash_Pushing(h, b, pfwd, speed)));
        }
        // A push begun from the stance goes on while it pushes, the stance tilting or turning as it goes (a push with one
        // hand swings the far end forward).
        float going = b.mh_push_ok && b.mh_push_stance == 1 && VR_PlayerTime() - b.mh_push_last <= VR_BASH_GAP;
        if(!stance && going)
        {
            b.mh_stance = 1; // (for the push's test: a weapon's)
        }
        if((stance || going) && VR_Bash_Pushing(h, b, pfwd, speed))
        {
            if(b.mh_push_dist <= 0 && VR_PlayerTime() - b.mh_push_last > VR_BASH_GAP)
            {
                // A push begins: from a stance held (a weapon's; a palm's needs none).
                b.mh_push_ok = FALSE;
                b.mh_push_stance = stance;
                if(stance == 2)
                {
                    b.mh_push_ok = TRUE;
                }
                else if(VR_PlayerTime() - b.mh_stance_since >= VR_BASH_HELD)
                {
                    b.mh_push_ok = TRUE;
                }
            }
            b.mh_pushing = b.mh_push_ok;
            b.mh_push_dist = b.mh_push_dist + (VR_Bash_PushVel(b) * pfwd) * b.mh_dt;
            b.mh_push_last = VR_PlayerTime();
        }
        else if(VR_PlayerTime() - b.mh_push_last > VR_BASH_GAP)
        {
            b.mh_push_dist = 0;
        }
        if(b.mh_pushing && b.mh_push_dist >= VR_BASH_TRAVEL && readyHand < 0 && b.mh_push_ok)
        {
            readyHand = h;
        }
    }
    // Crossed arms (the unarmed parry, VR_Parry_ArmsCrossed): reported as a stance too.
    float arms = VR_Parry_ArmsCrossed(self, fwd);
    if(arms != self.vr_arms)
    {
        self.vr_arms = arms;
        self.vr_arms_since = VR_PlayerTime();
    }
    else if(arms && self.vr_arms_since > 0 && VR_PlayerTime() - self.vr_arms_since >= VR_STANCE_REPORT)
    {
        self.vr_arms_since = 0;
        VR_Melee_Event("stance arms, hand -1");
        VR_Motion_Event("stance", "arms", -1, 0, VRGetHandPos(cVR_MainHand), world, "");
    }
    if(readyHand < 0)
    {
        return;
    }
    // An open palm's push beside a weapon's (the palm on the blade) is the weapon's.
    entity rb = readyHand == cVR_OffHand ? bo : bm;
    entity rob = readyHand == cVR_OffHand ? bm : bo;
    if(rb.mh_stance == 2 && rob.mh_stance == 1 && rob.mh_pushing)
    {
        readyHand = VRGetOtherHand(readyHand);
    }

    b = readyHand == cVR_OffHand ? bo : bm;
    float other = VRGetOtherHand(readyHand);
    entity ob = other == cVR_OffHand ? bo : bm;
    float armed = b.mh_stance == 1;

    // Two hands: the weapon held two-handed; an open palm pushing on a one-handed weapon; both palms (or both weapons)
    // pushing.
    float both = FALSE;
    if(armed && VRGet2HAiming(self) && VRIsHandEmpty(self, other))
    {
        both = TRUE;
    }
    else if(ob.mh_stance && (ob.mh_pushing || VR_Bash_Pushing(other, ob, pfwd, speed * VR_BASH_PAIR)))
    {
        if(ob.mh_stance == b.mh_stance)
        {
            both = TRUE;
        }
        else if(armed && VR_SegmentDistance(ob.mh_grip, ob.mh_grip, b.mh_near, b.mh_far) <
                         VR_BASH_PAIR_NEAR * VRMetersToUnits())
        {
            both = TRUE; // the palm on the blade
        }
    }
    else if(b.mh_stance == 2 && VR_Bash_TorchPushing(other, ob, pfwd, speed * VR_BASH_PAIR))
    {
        both = TRUE; // an open palm, the other hand pushing along with the flashlight in its fist
    }

    self.vr_blow_time = time; // (bullet time's screen tap: vr_bullettime_tap_blow_quiet)

    // The motion recorder (vr_motion.qc): a push, whatever it meets; once a push (its frames in a row).
    if(VR_PlayerTime() - self.vr_motion_pushseen > 0.15)
    {
        self.vr_push_start = VR_PlayerTime();
        VR_Motion_Event("push", armed ? "bash" : "shove", both ? -1 : readyHand, 0, VRGetHandPos(readyHand), world,
                        both ? "two hands" : "one hand");
    }
    self.vr_motion_pushseen = VR_PlayerTime();

    // The hands in the push strike nothing else in this motion.
    VRSetMeleeHitSoundPlayed(readyHand, TRUE);
    b.mh_hit_ent = world;
    VR_Melee_HitClear(b); // (one swing, many things: spent all the same)
    b.mh_hit_time = VR_PlayerTime();
    b.mh_hit_touch = VR_PlayerTime();
    if(both)
    {
        VRSetMeleeHitSoundPlayed(other, TRUE);
        ob.mh_hit_ent = world;
        VR_Melee_HitClear(ob);
        ob.mh_hit_time = VR_PlayerTime();
        ob.mh_hit_touch = VR_PlayerTime();
    }

    // Batting: a bash (a weapon or a club across, pushed: VR_Melee_HoldsWeapon); what it bats goes the way the hands
    // push (off a blade's face: VR_Bash_Direction, from the weapon lines' motion in the push). A shove (open palms,
    // whatever the other hand holds) bats nothing: an empty hand never does.
    if(armed)
    {
        vector pushVel = VR_Bash_PushVel(b);
        if(both && ob.mh_stance)
        {
            pushVel = (pushVel + VR_Bash_PushVel(ob)) * 0.5;
        }
        float m2u = VRMetersToUnits();
        self.vr_bashdef_mvn = VR_Blow_ToWorld(bm.mh_vnear) * m2u;
        self.vr_bashdef_mvf = VR_Blow_ToWorld(bm.mh_vfar) * m2u;
        self.vr_bashdef_ovn = VR_Blow_ToWorld(bo.mh_vnear) * m2u;
        self.vr_bashdef_ovf = VR_Blow_ToWorld(bo.mh_vfar) * m2u;
        VR_Bash_DeflectStart(both ? -1 : readyHand, fwd, pushVel, both);
    }

    if(VR_PlayerTime() < self.vr_bash_time)
    {
        return;
    }
    float hits = VR_Bash_Find(readyHand, b, fwd);
    if(both && !hits && ob.mh_stance)
    {
        hits = VR_Bash_Find(other, ob, fwd);
    }
    if(!hits)
    {
        return; // a push at the air: nothing yet (the push may still reach something)
    }

    // Stamina (vr_shove_stamina): a shove or a bash that lands spends it (once a push: not again if it batted a
    // projectile first); short of it, it is weaker (VR_Bash_Hit).
    VR_Bash_Pay(both, armed);

    // A bash or a shove in a counter's window after a parry (VR_Counter_Use): the counter, harder and further.
    float counter = 0;
    if(VR_Counter_Ready())
    {
        counter = VR_Counter_Use(armed ? "bash" : "shove", vr_bash_victims);
    }
    float scale = both ? 1 : VR_BASH_ONE_DAMAGE;
    float push = VR_Shove_HandsPush(both);
    if(counter)
    {
        scale = scale * counter;
        push = push * counter;
    }
    vector at = both ? (VRGetHandPos(cVR_MainHand) + VRGetHandPos(cVR_OffHand)) * 0.5 : VRGetHandPos(readyHand);
    if(armed)
    {
        at = (b.mh_near + b.mh_far) * 0.5;
    }
    vr_bash_counter = counter;
    vr_bash_both = both;
    entity e = vr_bash_victims;
    while(e)
    {
        entity next = e.vr_bash_next;
        vector hitAt = at;
        hitAt_x = min(e.absmax_x, max(e.absmin_x, hitAt_x));
        hitAt_y = min(e.absmax_y, max(e.absmin_y, hitAt_y));
        hitAt_z = min(e.absmax_z, max(e.absmin_z, hitAt_z));
        VR_Bash_Hit(e, hitAt, fwd, armed ? QVR_HITKIND_BASH : QVR_HITKIND_SHOVE, both ? -1 : readyHand, scale, push);
        e = next;
    }
    vr_bash_counter = FALSE;
    vr_bash_both = FALSE;
    vr_bash_exhaust_dmg = 0;
    vr_bash_exhaust_push = 0;
    self.vr_bash_time = VR_PlayerTime() + VR_BASH_COOLDOWN;

    // Each its own sound (vr_bash_sound, its volume; round 18), apart from the blows'; 0: silent.
    float bashSound = min(1, cvar_hget(cvarh_vr_bash_sound));
    float bashChan = both ? CHAN_AUTO : VRGetHandChannel(readyHand); // (both hands: in front of the chest)
    if(bashSound > 0)
    {
        sound(self, bashChan, (counter && armed) ? "vr/bash_parry.wav" : (armed ? "vr/bash.wav" : "vr/shove.wav"),
              bashSound * (both ? 1 : 0.7), ATTN_NORM);
    }
    if(both)
    {
        haptic(cVR_MainHand, 0.0, 0.25, 70, 1.0);
        haptic(cVR_OffHand, 0.0, 0.25, 70, 1.0);
        handimpact(cVR_MainHand, 3, fwd * -1);
        handimpact(cVR_OffHand, 3, fwd * -1);
    }
    else
    {
        haptic(readyHand, 0.0, 0.2, 70, 0.8);
        handimpact(readyHand, 2, fwd * -1);
    }
    particle2(at + fwd * 12, fwd * 80, QVR_PARTICLE_PRESET_BULLETPUFF, both ? 16 : 10);
    VR_Melee_Event(sprintf("%s, hand %g, %s, %g hit%s, push %.1f m/s over %.0f cm", counter ? (armed ? "parrybash" :
        "counter shove") : (armed ? "bash" : "shove"), readyHand, both ? "two hands" : "one hand", hits, hits == 1 ? "" : "s",
        VR_Bash_PushVel(b) * pfwd, b.mh_push_dist * 100));
};

// ----------------------------------------------------------------------------
// Each frame

// Follows both hands (PlayerVRMelee, before the bash and the blows).
void() VR_Melee_Frame =
{
    float h;
    for(h = 0; h < 2; ++h)
    {
        entity b = VR_Blow(h);
        b.mh_c_ent = world;
        b.mh_c_pos = '0 0 0';
        float fresh = VR_Melee_Track(h, b);
        if(!fresh)
        {
            b.mh_dt = 0; // (no new pose, or nothing to sweep from: no motion counted this frame)
        }
        b.mh_fresh = fresh;
        if(fresh && cvar_hget(cvarh_developer) >= 3 && b.mh_thing != VR_MTHING_NONE)
        {
            dprint(sprintf("melee trace: %.3f hand %g thing %g wid %g bits %g", time, h, b.mh_thing, VRGetEntWeapon(self, h),
                self.vrbits0));
            dprint(sprintf(" grip %v far %v vfar %v vgrip %v wrist %v\n", b.mh_rgrip, b.mh_rfar, b.mh_vfar, b.mh_vgrip, b.mh_rwrist));
        }
    }
    VR_Stamina_Frame();
    VR_HeavyMelee_Frame();
    VR_Counter_Frame();
    VR_Parry_GuardFrame(); // (when each guard came up: a parry's timing, combat.qc)
    VR_Melee_Hud();
};

float(float xHand, entity b, float xStruck) VR_Melee_StrikeAfter; // (below)

// The blow of `xHand` this frame, if any (PlayerVRMeleeImpl): FALSE, or TRUE with the contact in mh_c_*, the kind and
// strength set.
float(float xHand) VR_Melee_Strike =
{
    entity b = VR_Blow(xHand);
    VR_Melee_Rearm(xHand, b);
    if(!VRGetMeleeHitSoundPlayed(xHand))
    {
        VR_Melee_HitClear(b); // (a new motion, however it was re-armed)
    }
    if(!b.mh_fresh || b.mh_thing == VR_MTHING_NONE)
    {
        return FALSE;
    }
    VR_Melee_WallLeft(b);
    if(!VR_Throw_HandMayStrike(self, xHand))
    {
        return FALSE; // a two-handed throw's follow-through, not a blow (vr_throw_2h_nomelee)
    }
    VR_Melee_Whoosh(xHand, b);
    VR_Swish_Frame(xHand, b); // (swings short of a blow: vr_melee_swish)
    if(b.mh_pushing)
    {
        return FALSE; // a bash's push
    }
    AD_ShadowAxeChop(xHand, b); // AD support: the Shadow Axe chops the bodies lying on the floor (vr_ad_bridge.qc)
    float struck = VR_Melee_Sweep(xHand, b);
    VR_HoseCut_Melee(xHand, b); // AD support: a blade cuts a hose lying about, its sweep's lines (vr_ad_enemyguns.qc)
    return VR_Melee_StrikeAfter(xHand, b, struck);
};

// The rest of a blow found by VR_Melee_Sweep (`xStruck`): what it struck, the stamina it costs, a counter.
float(float xHand, entity b, float xStruck) VR_Melee_StrikeAfter =
{
    float struck = xStruck;
    b.mh_c_gib = FALSE;
    if(struck && b.mh_c_ent.vr_gib)
    {
        b.mh_c_gib = TRUE; // (not a && in a field: fteqcc stores 0)
    }
    // Stamina (vr_strike_stamina): once a motion, as it lands on the first thing that takes damage. A motion is new
    // until it lands (VRGetMeleeHitSoundPlayed; re-armed by VR_Melee_Rearm).
    if(!VRGetMeleeHitSoundPlayed(xHand))
    {
        b.mh_stam_paid = FALSE;
        b.mh_exhaust = 0;
        if(time - b.bat_paid_time < VR_BAT_PAID_KEEP)
        {
            b.mh_stam_paid = TRUE; // (this swing paid as it batted a projectile: VR_Deflect_Pay)
            b.mh_exhaust = b.bat_exhaust;
        }
    }
    if(struck && b.mh_c_ent.takedamage)
    {
        VR_Stamina_Strike(xHand, b);
    }
    // In a counter's window after a parry, a blow on something alive is the counter (VRMeleeDmg).
    b.mh_counter = 0;
    if(struck && b.mh_c_ent.takedamage && b.mh_c_ent.health > 0 && VR_Counter_Ready())
    {
        b.mh_counter = VR_Counter_Use(VR_Melee_KindName(b.mh_kind, b.mh_thing), b.mh_c_ent);
    }
    return struck;
};

// One swing, many things (VR_Melee_Multi): after a blow of `xHand` landed this frame (PlayerVRMeleeImpl), another the
// same sweep finds on something not struck yet in this motion, or the world: as VR_Melee_Strike, the contact in mh_c_*.
// (A body and the floor under it crossed in one frame: both.)
float(float xHand) VR_Melee_StrikeMore =
{
    entity b = VR_Blow(xHand);
    if(!VRGetMeleeHitSoundPlayed(xHand) || !VR_Melee_Multi(b) || (!b.mh_fresh && !vr_msw_decide))
    {
        return FALSE;
    }
    if(!VR_Melee_MultiRoom(b) && (b.mh_hit_world || b.mh_hit_n == 0))
    {
        return FALSE; // (no thing more, and the world struck)
    }
    b.mh_c_ent = world;
    b.mh_c_pos = '0 0 0';
    vr_msw_more = TRUE;
    float struck = VR_Melee_Sweep(xHand, b);
    vr_msw_more = FALSE;
    return VR_Melee_StrikeAfter(xHand, b, struck);
};

// After a blow of `xHand` landed on mh_c_ent: once per motion (VR_Melee_Rearm).
void(float xHand) VR_Melee_Landed =
{
    entity b = VR_Blow(xHand);
    self.vr_blow_time = time; // (bullet time's screen tap: vr_bullettime_tap_blow_quiet)
    self.show_hostile = time + 1; // a blow that landed wakes monsters up (as a gun's shot: W_AttackImpl)
    // QVR: heard by its strength and the held thing's weight (a fist's 200 units at strength 1; vr_stealth.qc)
    VR_Stealth_Noise(self.origin, 200 * b.mh_strength * (1 + max(0, VR_Weight_HeldMass(self, xHand)) / 3) *
        cvar_hget(cvarh_vr_stealth_noise_melee), self, STL_N_MELEE);
    float wall = !b.mh_c_ent.takedamage && !b.mh_c_gib;
    float multi = VR_Melee_Multi(b);
    if(wall)
    {
        b.mh_hit_world = TRUE;
    }
    else
    {
        VR_Melee_HitAdd(b, b.mh_c_ent, b.mh_c_gib);
    }
    // (One swing, many things: the world struck keeps the thing struck last, and a thing struck keeps the wall a part
    // went into a moment ago, VR_Melee_WallHeld: each gives its blow after the other.)
    if(!multi || !wall)
    {
        b.mh_hit_ent = b.mh_c_ent;
        b.mh_struck = b.mh_c_ent;
    }
    if(cvar_hget(cvarh_vr_debug_melee_blades))
    {
        meleeshow(xHand, 6, b.mh_c_pos, '0 0 0', -1); // (Show Melee Blades: where it landed)
    }
    b.mh_hit_time = VR_PlayerTime();
    b.mh_hit_touch = VR_PlayerTime();
    vector v = b.mh_vfar;
    if(b.mh_kind == VR_MKIND_POMMEL)
    {
        v = b.mh_vgrip;
    }
    b.mh_hit_dir = normalize(v);
    if(!multi || wall)
    {
        b.mh_wall_time = -1; // (a blow landed: the wall it went into gives no more, VR_Melee_WallHeld; only a new one)
    }
    if(wall)
    {
        b.mh_wall_blow = VR_PlayerTime();
        b.mh_wall_bn = b.mh_c_normal; // (VR_Melee_WallFresh: that wall, until the weapon is off it)
        b.mh_wall_bd = b.mh_c_normal * b.mh_c_pos;
        b.mh_wall_bspeed = b.mh_speed;
        b.mh_wall_on = TRUE;
    }
    string what =sprintf("%s, hand %g, %s (%s) with %s", VR_Melee_Category(b.mh_kind, b.mh_thing), xHand,
        VR_Melee_KindName(b.mh_kind, b.mh_thing), b.mh_way, b.mh_part);
    VR_Melee_Event(sprintf("%s, %.1f m/s, x%.2f, %s", what, b.mh_speed, b.mh_strength,
        b.mh_c_gib ? "a gib" : (b.mh_c_ent.takedamage ? b.mh_c_ent.classname : "a wall")));
};

// The engine's melee marks (vr/vr_decals.hpp EventMeleeMark; vr_decal_melee): a particle2 preset past the gore's.
float QVR_DECAL_MELEEMARK = 43;

// A melee blow of `xHand` struck a wall or the floor (the weapons' wall branches, weapons.qc and the others; the trace_
// globals and v_forward as VRDoMeleeAttackTracelineFor left them), dealing `xDmg` had it been something that bleeds:
// the mark it carves, once a blow (before its hit sound is played). Blunt (`xBlunt`, or a blunt part: a pommel, a
// handle, a gun, the crowbar, a club, a fist holding a prop) a round dent; a blade (a sword's, an axe's head, the
// chainsaw's bar) a groove along the swing on the surface, or, driven straight in, along the blade (its edge's line);
// a stab's point a short one. A bare fist leaves none.
void(float xHand, float xDmg, float xBlunt) VR_Melee_WallMark =
{
    if(VRGetMeleeHitSoundPlayed(xHand) || trace_fraction >= 1 || trace_ent.takedamage)
    {
        return;
    }
    float tracked = VRHandsTracked(self) && self.ishuman;
    entity b = VR_Blow(xHand);
    float thing = tracked ? b.mh_thing : VR_Melee_Thing(self, xHand);
    float kind = tracked ? b.mh_kind : VR_MKIND_SLASH;
    if(thing == VR_MTHING_NONE || (thing == VR_MTHING_FIST && VR_Carry_Held(self, xHand) == world))
    {
        return;
    }
    float blade = !xBlunt && !(tracked && b.mh_c_blunt) && (kind == VR_MKIND_SLASH || kind == VR_MKIND_STAB) &&
                  (thing == VR_MTHING_SWORD || thing == VR_MTHING_AXE || thing == VR_MTHING_SAW);
    vector n = trace_plane_normal;
    vector way = normalize(v_forward);
    float mark = 0;
    if(blade)
    {
        mark = kind == VR_MKIND_STAB ? 2 : (thing == VR_MTHING_AXE ? 3 : 1);
        vector t = way - n * (way * n);
        if(vlen(t) < 0.35 && tracked)
        {
            vector ax = b.mh_far - b.mh_grip; // (straight in: the edge's line, along the blade)
            t = ax - n * (ax * n);
        }
        way = normalize(t);
    }
    particle2(trace_endpos + n * 2, way * 7, QVR_DECAL_MELEEMARK, min(999, max(1, rint(xDmg * 4))) + 1000 * mark);
};
