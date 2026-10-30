// vr_melee_params.qc -- melee for tracked hands, first part: the model's numbers (weights, speeds, reaches) and
// what a hand holds (its line and points). Split from vr_melee.qc (the model is described at its top); compiled
// right before it (progs.src).

// The numbers the engine's melee builtins use too (the history's length and look-back times).
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
