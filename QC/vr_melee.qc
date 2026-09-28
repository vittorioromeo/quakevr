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
// VR_MELEE_RUN towards where it goes (a blow, not a wiggle). The direction doesn't matter. What kind of blow it is follows
// from which part hit and how it moved (VR_Melee_Decide): a stab or jab (the far end moving along the line, outwards), a
// slash or chop or gun swing (the far end any other way), a pommel or butt strike (the near end), a punch (a closed
// fist). Of a blade's points in the thing, the outermost strikes. The pommel's touch waits a moment for the blade
// proper: a cut from close by is the blade's. The hilt, a handle, a gun's grip (where the hand is) strike nothing.
//
// THE PARRY STANCE is the held thing presented across (the parry's pose: VR_Parry_Blocks, combat.qc), or an open palm
// facing ahead. A BASH is the stance, held VR_BASH_HELD, pushed forward (both its ends going ahead), a few centimetres:
// then what is just ahead is knocked back (VR_Bash). A palm's SHOVE is the palm pushed out from the shoulder. Two hands
// (a weapon held two-handed, an open palm pushing on a one-handed weapon, both palms) bash harder.
//
// The numbers are in one place, below: tune them against the labelled takes (vr_motion_eval), not case by case.

// ----------------------------------------------------------------------------
// The model's numbers

// Strikes.
float VR_MELEE_ARM = 1.25;         // a weapon's swing: the grip's least speed, times vr_melee_speed (a weapon is heavier than a fist;
                                   // the author's cuts move the hand 4.5-13 m/s, his sword and gun waved at the dummy 2-4)
float VR_MELEE_STAB_SPEED = 0.75;  // a stab's (jab's) least speed of the grip, times vr_melee_speed (a thrust needs less than a swing)
float VR_MELEE_STAB_ALONG = 0.4;   // cosine: the far end moving within ~66 degrees of the line, outwards, is a stab (the author's
                                   // stabs 0.4-0.87 at the contact, his cuts at most 0.36)
float VR_MELEE_UP = 0.8;           // a punch's least speed grows by this share for a blow going straight up (the author's
                                   // uppercuts go 6-9 m/s, a fist wiggled upwards 4.5-5.5),
float VR_MELEE_DOWN = 1.5;         // and by this share for each unit (sine) of steepness downwards beyond VR_MELEE_DOWN_FROM
float VR_MELEE_DOWN_FROM = 0.4;    // (a hammer blow falls easily: his overhead punches go 12 m/s, a hand waved down 6)
float VR_MELEE_RUN = 0.2;          // m the grip came towards where it goes now, within VR_MELEE_RUN_TIME: a blow, not a wave or a
                                   // wiggle (the author's punches come 0.22-0.8 m, his fists waved fast at the dummy 0.05-0.2)
float VR_MELEE_RUN_TIME = 0.3;     // s looked back at most
float VR_MELEE_WRIST_BACK = 0.2;   // m behind the controller (along its own axis, the gun angle taken out) the hand turns about
                                   // when it wiggles (fitted on the author's wiggles with fists, sword, axe and gun: 17-24 cm)
float VR_MELEE_WRIST_TIME = 0.12;  // s over which vr_melee_wrist_speed averages the wrist's travel (a wiggle's half-period)
float VR_MELEE_WRIST_PUNCH = 2;    // a punch's wrist speed, times vr_melee_wrist_speed: a fist has no lever, the arm carries it (the
                                   // author's punches carry the wrist 3.5 m/s and more, his fists wiggled at the dummy 1.5 at most)
float VR_MELEE_HILT_WAIT = 0.1;    // s the pommel's (the near end's) touch waits for the blade proper (VR_MELEE_BLADE_FROM out):
                                   // a cut from close by is the blade's (the author's close chops draw the weapon back along
                                   // its line, as a pommel strike does)
float VR_MELEE_BLADE_FROM = 0.4;   // the blade proper: from this share of the way from the hand to the tip
float VR_MELEE_GOOD = 2;           // times the least speed: a blow of strength 1
float VR_MELEE_WEAKEST = 0.5;      // the strength at the least speed
float VR_MELEE_STRONGEST = 2;      // and the most
float VR_MELEE_REARM_SLOW = 0.5;   // a hand strikes the same thing again once its part slowed below this share of the least speed,
float VR_MELEE_REARM_TURN = -0.17; // or, out of it, turned back more than 100 degrees from the blow (cosine: a backswing),
float VR_MELEE_REARM_TIME = 0.6;   // or, out of it, after this long (s)
float VR_MELEE_REARM_OUT = 0.1;    // s since the hand's parts last touched what it struck: out of it
float VR_MELEE_RADIUS = 3;         // units around a point's sweep that it strikes (times vr_melee_range_multiplier)
float VR_MELEE_JUMP = 20;          // m/s: the grip moving faster than this jumped (a teleport, the play space re-based)
float VR_MELEE_TIPJUMP = 60;       // m/s: likewise the far end (a weapon taken)
float VR_MELEE_WALL_DOWN = -0.35;  // a point going down more steeply than this (sine) doesn't strike a floor (reaching for a holster),
float VR_MELEE_FLOOR = 0.7;        // (a surface facing up: its normal's z at least this)
float VR_MELEE_WALL_INTO = 0.25;   // and strikes a wall only going into it (cosine to its normal: a chop, not a reach grazing it)
float VR_MELEE_POMMEL_SWORD = 0.6; // weights: a sword's pommel strike,
float VR_MELEE_POMMEL_AXE = 0.5;   // an axe's or Mjolnir's handle's end,

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
float VR_BASH_ONE_PUSH = 0.7;     // and of the knockback
float VR_BASH_PALM = 0.6;         // cosine: an open palm facing ahead (within 53 degrees)
float VR_BASH_PALM_ALONG = 0.7;   // cosine: moving the way it faces (within 45 degrees)
float VR_BASH_PALM_RADIAL = 0.82; // cosine: a shove extends the arm: the palm moving out from its shoulder (his shoves:
                                  // 0.84-0.98; his hands waved down or round at the dummy, palm first: 0.55-0.8)
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

// The kinds of points (what they strike as).
float VR_MPT_EDGE = 0;  // the blade, an axe's head, a gun's barrel and muzzle: moves with the far end
float VR_MPT_GRIP = 1;  // the hilt, the guard, the hand on a handle or a gun's grip: where the hand is, it strikes nothing
                        // (the author's pommel strikes land with the near end; his weapons waved at the dummy touched it
                        // with the hilt, the handle, a gun's grip); it only keeps a blow's target touched (VR_Melee_Rearm)
float VR_MPT_NEAR = 2;  // the pommel, the handle's end, a gun's butt
float VR_MPT_FIST = 3;  // the fist and the knuckles

// The kinds of blows.
float VR_MKIND_SLASH = 0;  // a swing's blade, an axe's head, a gun's barrel sweeping across
float VR_MKIND_STAB = 1;   // driven along the line, the far end first
float VR_MKIND_POMMEL = 2; // the near end (the blade proper not following)
float VR_MKIND_PUNCH = 3;  // a fist (any way: jab, hook, uppercut, overhead)

// The hand's state, on an entity of its own (the player's vr_blow_main / vr_blow_off).
.entity vr_blow_main;
.entity vr_blow_off;
.float mh_time;        // when the last new pose was taken
.float mh_dt;          // s since the one before
.float mh_thing;       // VR_MTHING_*
.float mh_wid;         // the weapon (another: nothing to sweep from)
.float mh_n;           // points (0: none yet)
.vector mh_pp[9];      // the points at the pose before (world),
.vector mh_cp[9];      // and now
.vector mh_near;       // the near end, the grip and the far end (world, now)
.vector mh_grip;
.vector mh_far;
.vector mh_rnear;      // the same from the head, in the play space (metres)
.vector mh_rgrip;
.vector mh_rfar;
.vector mh_vnear;      // and their velocities (m/s, play space)
.vector mh_vgrip;
.vector mh_vfar;
.float mh_fast;        // the part's fastest (m/s): the far end, or the grip if faster
.float mh_lenient;     // heavy leniency: its speed thresholds' factor, from what the hand holds (VR_Weight_Leniency)
.float mh_whooshed;    // a whoosh played for this motion (until it slows)
.entity mh_hit_ent;    // the last thing struck (world: none, or a bash or a held gib spent the motion),
.vector mh_hit_dir;    // the struck part's way then (play space),
.float mh_hit_time;    // when
.float mh_hit_touch;   // the last time a part touched it (VR_MELEE_REARM_OUT)
// The blow being dealt (VR_Melee_Contact, VRMeleeDmg, the dummy's report).
.float mh_kind;        // VR_MKIND_*
.float mh_strength;    // its damage multiplier
.float mh_speed;       // the striking part's speed (m/s)
.string mh_part;       // its name ("the tip")
.string mh_way;        // how it moved, for the report ("overhead", "hook")
.float mh_counter;     // a counter (VR_Counter_Use): its damage multiplier (0: not a counter)
.float mh_exhaust;     // short of stamina for it (VR_Stamina_Strike): its damage multiplier (0: not short)
.float mh_stam_paid;   // this motion's stamina was spent (once a motion, however many things it goes through)
.entity mh_c_ent;      // where it lands, as a traceline's hit
.vector mh_c_pos;
.vector mh_c_dir;
.vector mh_c_normal;
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
// The pommel's touch, waiting VR_MELEE_HILT_WAIT for the blade (VR_Melee_Sweep): the blow it would be.
.entity mh_pend_ent;
.float mh_pend_time;
.vector mh_pend_pos;
.vector mh_pend_dir;
.float mh_pend_kind;
.float mh_pend_strength;
.float mh_pend_speed;
.string mh_pend_part;
.string mh_pend_way;
// The last poses' velocities (a ring: VR_Melee_Run): the far end's, the grip's, and each pose's time step.
.vector mh_hfar[24];
.vector mh_hgrip[24];
.vector mh_hwrist[24]; // the wrist's (VR_Melee_Wrist)
.vector mh_rwrist;     // the wrist from the head (play space, metres)
.float mh_hdt[24];
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

.float vr_bash_time;      // on a player: when the next bash may be
.float vr_arms;           // on a player: arms crossed (the unarmed parry),
.float vr_arms_since;     // since when (0: reported)
.float vr_bash_mark;      // on a monster: found by this frame's bash (VR_Bash_Find),
.entity vr_bash_next;     // and the next one found
.float vr_bashdef_until;  // on a player: a bash's (or an armed shove's) batting goes on until then (VR_Bash_Deflect),
.float vr_bashdef_hand;   // with that hand (-1: both)
.vector vr_bashdef_vel;   // the push's velocity (world, u/s; VR_Bash_DeflectStart), what it bats goes that way
.float vr_bashdef_both;   // pushed with both hands
.float vr_bashdef_armed;  // a bash (a weapon's guard), not a shove
.float vr_bashdef_gren;   // an unarmed shove: it pushes away only a grenade (VR_Grenade_Is; a palm can't bat a spike)
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
// weapon, which can't be swung as fast).
float(entity p, float xHand) VR_Weight_Leniency =
{
    return weightleniency(VR_Weight_HeldMass(p, xHand));
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
    float wid = VRGetEntWeapon(p, xHand);
    if(wid == WID_FIST)
    {
        // A club carried in this hand alone (a wall torch: vr_walltorch.qc): its head strikes.
        if(VR_Melee_ClubOf(p, xHand) != world)
        {
            return VR_MTHING_CLUB;
        }
        // The empty hand steadying the other's weapon swings with it: the weapon's hand strikes.
        if(VRGet2HAiming(p) && !VRIsHandEmpty(p, VRGetOtherHand(xHand)))
        {
            return VR_MTHING_NONE;
        }
        return VR_MTHING_FIST;
    }
    if(wid == WID_SWORD)
    {
        return VR_MTHING_SWORD;
    }
    if(wid == WID_AXE || wid == WID_MJOLNIR)
    {
        return VR_MTHING_AXE;
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

// The line of what `xHand` of `p` holds, into vr_mline_near, _grip, _far (world): FALSE for nothing to strike with.
vector vr_mline_near;
vector vr_mline_grip;
vector vr_mline_far;
float(entity p, float xHand, float xThing) VR_Melee_Line =
{
    vector h = VRGetEntHandPos(p, xHand);
    vr_mline_grip = h;
    if(xThing == VR_MTHING_NONE)
    {
        return FALSE;
    }
    if(xThing == VR_MTHING_FIST)
    {
        vector fwd = VRForwardOf(VRGetEntHandRot(p, xHand));
        vr_mline_near = h - fwd * 1;
        vr_mline_far = h + fwd * 4;
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
    vector m = xHand == cVR_OffHand ? p.offmuzzlepos : p.muzzlepos;
    if(m == '0 0 0' || vlen(m - h) > 80 || vlen(m - h) < 2)
    {
        m = h + VRForwardOf(VRGetEntHandRot(p, xHand)) * 8;
    }
    vector a = m - h;
    vr_mline_far = m;
    if(xThing == VR_MTHING_SWORD)
    {
        vr_mline_near = h - a * 0.3; // the hand closes just under the guard; the grip and the pommel below it
    }
    else if(xThing == VR_MTHING_AXE)
    {
        vr_mline_near = h - a * 0.25; // the handle's end, below the hand
    }
    else if(VRIsHandCarryingByForegrip(p, xHand))
    {
        vr_mline_near = h - a * 0.8; // held by its pump: the stock is well behind the hand
    }
    else
    {
        vr_mline_near = h - a * 0.3; // the butt behind the grip
    }
    return TRUE;
};

// A point of the layout (a macro: the constant index is a plain global).
#define VR_MPT(xIndex, xF, xKind, xName) \
    vr_mpt_f[xIndex] = xF; \
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
    else if(xThing == VR_MTHING_GUN)
    {
        VR_MPT(0, -1, VR_MPT_NEAR, "the butt");
        VR_MPT(1, 0, VR_MPT_GRIP, "the grip");
        VR_MPT(2, 0.5, VR_MPT_EDGE, "the barrel");
        VR_MPT(3, 1, VR_MPT_EDGE, "the muzzle");
    }
};

// A point's place on the line (the layout's -1: the near end).
vector(float xF) VR_Melee_At =
{
    if(xF < 0)
    {
        return vr_mline_near;
    }
    return vr_mline_grip + (vr_mline_far - vr_mline_grip) * xF;
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
    if(!VRIsHandEmpty(self, xHand) || VR_Melee_ClosedFist(xHand) || VR_Carry_IsHolding(self, xHand))
    {
        return FALSE;
    }
    if(VRGet2HAiming(self) && !VRIsHandEmpty(self, VRGetOtherHand(xHand)))
    {
        return FALSE; // steadying the other's weapon
    }
    return !VRHasBit(self.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_CLIMBING : QVR_VRBITS0_OFFHAND_CLIMBING);
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
    float ownOff = xHand == cVR_OffHand && !cvar("vr_handcal_off_mirror");
    float pitch = cvar(ownOff ? "vr_offhandpitch" : "vr_gunangle") * 0.0174533;
    float yaw = cvar(ownOff ? "vr_offhandyaw" : "vr_gunyaw") * 0.0174533;
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

float(float xHand, entity b) VR_Melee_Track =
{
    // The pose is the client's as this server frame ends (time + frametime): QC's time is still the frame's start.
    float now = time + frametime;
    float dt = now - b.mh_time;
    float thing = VR_Melee_Thing(self, xHand);
    float wid = VRGetEntWeapon(self, xHand);
    if(!VR_Melee_Line(self, xHand, thing) || !VRHandsTracked(self))
    {
        b.mh_n = 0;
        b.mh_thing = thing;
        b.mh_lenient = VR_Weight_Leniency(self, xHand);
        b.mh_vnear = b.mh_vgrip = b.mh_vfar = '0 0 0';
        b.mh_fast = 0;
        b.mh_time = now;
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
    if(same)
    {
        // A jump (a teleport, the play space re-based, a weapon taken) is faster than a hand goes.
        if(vlen(rgrip - b.mh_rgrip) > max(0.3, VR_MELEE_JUMP * dt) || vlen(rfar - b.mh_rfar) > max(0.6, VR_MELEE_TIPJUMP * dt))
        {
            same = FALSE;
        }
    }
    if(same)
    {
        b.mh_vnear = (rnear - b.mh_rnear) * (1 / dt);
        b.mh_vgrip = (rgrip - b.mh_rgrip) * (1 / dt);
        b.mh_vfar = (rfar - b.mh_rfar) * (1 / dt);
        vwrist = (rwrist - b.mh_rwrist) * (1 / dt);
    }
    else
    {
        b.mh_vnear = b.mh_vgrip = b.mh_vfar = '0 0 0';
    }
    b.mh_rnear = rnear;
    b.mh_rgrip = rgrip;
    b.mh_rfar = rfar;
    b.mh_rwrist = rwrist;
    b.mh_near = vr_mline_near;
    b.mh_grip = vr_mline_grip;
    b.mh_far = vr_mline_far;
    b.mh_dt = same ? dt : 0;
    b.mh_time = now;
    b.mh_thing = thing;
    b.mh_wid = wid;
    b.mh_lenient = VR_Weight_Leniency(self, xHand);
    b.mh_fast = max(vlen(b.mh_vfar), vlen(b.mh_vgrip));
    if(same)
    {
        b.mh_hi = b.mh_hi + 1;
        if(b.mh_hi >= 24)
        {
            b.mh_hi = 0;
        }
        b.mh_hfar[b.mh_hi] = b.mh_vfar;
        b.mh_hgrip[b.mh_hi] = b.mh_vgrip;
        b.mh_hwrist[b.mh_hi] = vwrist;
        b.mh_hdt[b.mh_hi] = dt;
    }
    else
    {
        b.mh_hdt[b.mh_hi] = 0; // (nothing to look back through)
    }

    // The points: kept from the last pose, laid out now.
    VR_Melee_Layout(thing);
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
    return same;
};

// The velocity of a point of the layout at `xF` (play space, m/s): the line moves as a rigid thing.
vector(entity b, float xF) VR_Melee_PointVel =
{
    if(xF < 0)
    {
        return b.mh_vnear;
    }
    return b.mh_vgrip + (b.mh_vfar - b.mh_vgrip) * xF;
};

// Whether `e` can be struck by the hands of `self`: it bleeds (or breaks), it is solid, and it is not the player's
// own (held or thrown).
float(entity e) VR_Blow_Strikable =
{
    return e != self && e != world && e.takedamage && e.solid != SOLID_NOT && e.solid != SOLID_TRIGGER &&
           e.owner != self && e.carry_player != self;
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

// How far the far end (or, `xGrip`, the grip) of what `b`'s hand holds has come towards where it goes now (m): the
// most of its displacement along its way now, looking back VR_MELEE_RUN_TIME at most (a wave's back and forth cancels).
float(entity b, float xGrip) VR_Melee_Run =
{
    vector now = normalize(xGrip ? b.mh_vgrip : b.mh_vfar);
    vector moved = '0 0 0';
    float run = 0;
    float back = 0;
    float k = b.mh_hi;
    float n;
    for(n = 0; n < 24; ++n)
    {
        float dt = b.mh_hdt[k];
        if(dt <= 0)
        {
            break;
        }
        moved = moved + (xGrip ? b.mh_hgrip[k] : b.mh_hfar[k]) * dt;
        run = max(run, moved * now);
        back = back + dt;
        if(back > VR_MELEE_RUN_TIME)
        {
            break;
        }
        k = k - 1;
        if(k < 0)
        {
            k = 23;
        }
    }
    return run;
};

// The wrist's own speed (m/s): its net travel over the last VR_MELEE_WRIST_TIME, over that time. The hand turned in
// place (a wiggle: the controller and the tip flick fast) barely moves it; a swing, a stab, a punch carry it along.
float(entity b) VR_Melee_WristSpeed =
{
    vector moved = '0 0 0';
    float back = 0;
    float k = b.mh_hi;
    float n;
    for(n = 0; n < 24; ++n)
    {
        float dt = b.mh_hdt[k];
        if(dt <= 0)
        {
            break;
        }
        moved = moved + b.mh_hwrist[k] * dt;
        back = back + dt;
        if(back >= VR_MELEE_WRIST_TIME)
        {
            break;
        }
        k = k - 1;
        if(k < 0)
        {
            k = 23;
        }
    }
    return vlen(moved) / max(back, VR_MELEE_WRIST_TIME);
};

// Whether the hand itself moved as a blow's does (vr_melee_wrist_speed; a punch VR_MELEE_WRIST_PUNCH times that), not
// only turned.
float(entity b) VR_Melee_WristMoved =
{
    float need = cvar_hget(cvarh_vr_melee_wrist_speed) * (b.mh_lenient > 0 ? b.mh_lenient : 1);
    if(b.mh_thing == VR_MTHING_FIST)
    {
        need = need * VR_MELEE_WRIST_PUNCH;
    }
    return VR_Melee_WristSpeed(b) >= need;
};

// Whether the blow of `xHand` (point `xI`, of kind `xKind`, on `xEnt`) lands, and as what: sets mh_kind, mh_speed,
// mh_strength, mh_part and mh_way. FALSE: nothing (too slow, or no blow).
float(float xHand, entity b, float xI, float xKind) VR_Melee_Decide =
{
    float minSpeed = VR_Melee_MinSpeedOf(b);
    float f = vr_mpt_f[xI];
    vector vp = VR_Melee_PointVel(b, f);
    vector axis = normalize(b.mh_rfar - b.mh_rgrip);
    float vfar = vlen(b.mh_vfar);
    float vgrip = vlen(b.mh_vgrip);
    float kind = VR_MKIND_SLASH;
    float speed = vgrip; // the arm's: the grip's speed
    float need = minSpeed;
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
        if(!VR_Melee_ClosedFist(xHand))
        {
            return FALSE;
        }
        kind = VR_MKIND_PUNCH;
        way = b.mh_vgrip;
        // The blow's way: rising and steeply falling blows are thrown faster for the same effort.
        need = VR_Melee_SwingNeed(xHand, b, b.mh_vgrip);
        weight = cvar_hget(cvarh_vr_melee_punch_mult);
        if(VR_Carry_IsHolding(self, xHand))
        {
            weight = weight * VR_Carry_PunchMult(self, xHand); // a box, a rock, a brick in hand: an improvised weapon
        }
    }
    else if(xKind == VR_MPT_EDGE)
    {
        // The far end driven point first, along the line (its outer half striking): a stab (a jab). Else a swing: the
        // arm must drive it (VR_MELEE_ARM), however fast the head goes round.
        if(along >= VR_MELEE_STAB_ALONG && f >= 0.5 && b.mh_thing != VR_MTHING_AXE)
        {
            kind = VR_MKIND_STAB;
            need = minSpeed * VR_MELEE_STAB_SPEED;
        }
        else
        {
            need = minSpeed * VR_MELEE_ARM;
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
        if(vgrip < minSpeed)
        {
            return FALSE; // the arm drives it too (a sword waved round swings its guard fast)
        }
        if(b.mh_thing == VR_MTHING_SWORD)
        {
            weight = VR_MELEE_POMMEL_SWORD;
        }
        else if(b.mh_thing == VR_MTHING_AXE || b.mh_thing == VR_MTHING_CLUB)
        {
            weight = VR_MELEE_POMMEL_AXE;
        }
    }
    if(cvar("developer") >= 2)
    {
        dprint(sprintf("melee debug: hand %g %s at %s: grip %.1f far %.1f", xHand, VR_Melee_KindName(kind, b.mh_thing),
            vr_mpt_name[xI], vgrip, vfar));
        dprint(sprintf(" along %.2f lead %.2f need %.1f run %.2f wrist %.2f\n", along, lead, need, VR_Melee_Run(b, TRUE),
            VR_Melee_WristSpeed(b)));
    }
    if(speed < need)
    {
        return FALSE;
    }
    // A blow, not a wiggle: the arm came from some way off.
    if(VR_Melee_Run(b, TRUE) < VR_MELEE_RUN)
    {
        return FALSE;
    }
    // And the hand itself moved, not only turned in place (a wiggle flicks the grip and the tip fast).
    if(!VR_Melee_WristMoved(b))
    {
        return FALSE;
    }

    float s = VR_MELEE_WEAKEST + (1 - VR_MELEE_WEAKEST) * (speed - need) / (need * (VR_MELEE_GOOD - 1));
    b.mh_kind = kind;
    b.mh_speed = speed;
    b.mh_strength = min(VR_MELEE_STRONGEST, max(VR_MELEE_WEAKEST, s)) * weight;
    b.mh_way = VR_Melee_Way(b, way, b.mh_thing);
    return TRUE;
};

// Whether `xHand` may strike `xEnt` now: once per motion (see VR_MELEE_REARM_*); another thing at once (a swing
// through two monsters). A bash or a held gib struck spends the motion for everything (mh_hit_ent world).
float(float xHand, entity b, entity xEnt) VR_Melee_Armed =
{
    if(!VRGetMeleeHitSoundPlayed(xHand))
    {
        return TRUE;
    }
    return b.mh_hit_ent != world && xEnt != b.mh_hit_ent;
};

// Re-arms `xHand` (each frame) once its motion slowed down since the last blow, or, its parts out of what it struck,
// turned back (a backswing) or went on long enough: a swing whose arc curves on through a big monster is one blow.
void(float xHand, entity b) VR_Melee_Rearm =
{
    if(!VRGetMeleeHitSoundPlayed(xHand) || time < b.mh_hit_time + 0.1)
    {
        return;
    }
    float rearm = b.mh_fast < VR_Melee_MinSpeedOf(b) * VR_MELEE_REARM_SLOW;
    if(!rearm && time - b.mh_hit_touch > VR_MELEE_REARM_OUT)
    {
        if(time - b.mh_hit_time > VR_MELEE_REARM_TIME)
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
    }
};

// Whether a point swept from `xP0` to `xP1` strikes the wall (world, brush) it meets, whose normal is `xNormal`: not
// going down steeply onto a floor (a hand reaching for a holster), nor down past a wall, grazing it; a blow chopped
// down into a wall does.
float(vector xP0, vector xP1, vector xNormal) VR_Melee_WallTaken =
{
    vector d = normalize(xP1 - xP0);
    if(d_z >= VR_MELEE_WALL_DOWN)
    {
        return TRUE;
    }
    float taken = xNormal_z < VR_MELEE_FLOOR && d * xNormal <= -VR_MELEE_WALL_INTO;
    if(cvar("developer") >= 2)
    {
        dprint(sprintf("melee debug: going down at %.2f onto a surface facing %v (into it %.2f): %s\n", d_z, xNormal,
            -(d * xNormal), taken ? "strikes" : "passes"));
    }
    return taken;
};

// Sweeps the points of `xHand` from the last pose to this one (VR_Melee_Track) and decides the blow: the contact to
// land on into mh_c_* and TRUE, or FALSE. Things that bleed first: of the points striking this frame, the blade's
// (or head's, fist's) over the pommel's, then the earliest along the sweep. A wall only when nothing that
// bleeds is struck, and not by a point going down (a hand reaching for a holster).
float(float xHand, entity b) VR_Melee_Sweep =
{
    float n = b.mh_n;
    if(n <= 0)
    {
        return FALSE;
    }
    VR_Melee_Layout(b.mh_thing); // (this hand's: the globals hold the last hand tracked)
    float r = VR_MELEE_RADIUS * max(0.25, cvar_hget(cvarh_vr_melee_range_multiplier));
    vector hand = b.mh_grip;

    // The best contact of each class: the striking part's (edge, fist) and the near end's (pommel, butt).
    float edgeT = 2;
    float edgeI = -1;
    entity edgeEnt = world;
    vector edgeAt = '0 0 0';
    float hiltT = 2;
    float hiltI = -1;
    entity hiltEnt = world;
    vector hiltAt = '0 0 0';
    float wallT = 2;
    float wallI = -1;
    entity wallEnt = world;
    vector wallAt = '0 0 0';
    vector wallNormal = '0 0 0';
    float i;
    vector p0;
    vector p1;
    float t;
    float striker;

    // Walls and brush entities (a line each, exact against the BSP; it finds monsters too). A striking point strikes a
    // wall within `r` past its sweep, as it does a monster's box widened by `r`: the hand and what it holds are stopped
    // short of walls (vr_handpose.cpp: the far end, a blade's tip or an axe's head, a unit off), so an axe swung into a
    // wall never reached it, while a fist's knuckles, 4 units ahead of the stopped hand, went in.
    for(i = 0; i < n; ++i)
    {
        p0 = b.mh_pp[i];
        p1 = b.mh_cp[i];
        if(p0 == p1)
        {
            continue;
        }
        striker = vr_mpt_kind[i] == VR_MPT_EDGE || vr_mpt_kind[i] == VR_MPT_FIST;
        traceline(p0, p1, FALSE, self);
        if(trace_fraction >= 1 && !trace_startsolid)
        {
            float past = FALSE;
            if(striker)
            {
                traceline(p1, p1 + normalize(p1 - p0) * r, TRUE, self);
                if(trace_fraction < 1 && !trace_startsolid)
                {
                    past = TRUE;
                }
            }
            trace_fraction = past ? 1 : 2; // (a wall within reach past the sweep's end: there)
        }
        if(trace_startsolid || trace_fraction > 1)
        {
            continue;
        }
        if(VR_Blow_Strikable(trace_ent))
        {
            if(trace_ent == b.mh_hit_ent)
            {
                b.mh_hit_touch = time;
            }
            if(vr_mpt_kind[i] == VR_MPT_GRIP)
            {
                continue;
            }
            if(striker && trace_fraction < edgeT && VR_Melee_Armed(xHand, b, trace_ent))
            {
                edgeT = trace_fraction;
                edgeI = i;
                edgeEnt = trace_ent;
                edgeAt = trace_endpos;
            }
            else if(!striker && trace_fraction < hiltT && VR_Melee_Armed(xHand, b, trace_ent))
            {
                hiltT = trace_fraction;
                hiltI = i;
                hiltEnt = trace_ent;
                hiltAt = trace_endpos;
            }
        }
        else if(trace_fraction < wallT && vr_mpt_kind[i] != VR_MPT_GRIP && VR_Melee_WallTaken(p0, p1, trace_plane_normal))
        {
            wallT = trace_fraction;
            wallI = i;
            wallEnt = trace_ent;
            wallAt = trace_endpos;
            wallNormal = trace_plane_normal;
        }
    }

    // Monsters' boxes, widened by the points' thickness (and not through a wall); and loose gibs and heads (on the
    // floor, flying): not solid (SOLID_NOT_BUT_TOUCHABLE: the traces above pass them; only shots trace with
    // MOVE_HITGIBS), struck as a monster's box, and burst (VR_Gib_Blow). A gib held in a hand is VR_Gib_HeldFrame's.
    vector mid = (b.mh_pp[0] + b.mh_cp[0] + b.mh_pp[n - 1] + b.mh_cp[n - 1]) * 0.25;
    float reach = max(max(vlen(b.mh_pp[0] - mid), vlen(b.mh_cp[0] - mid)),
                      max(vlen(b.mh_pp[n - 1] - mid), vlen(b.mh_cp[n - 1] - mid)));
    entity e = findradius(mid, reach + r + 96);
    while(e)
    {
        if(VR_Blow_Strikable(e) && (e.solid == SOLID_SLIDEBOX || e.solid == SOLID_BBOX ||
                                    (e.solid == SOLID_NOT_BUT_TOUCHABLE && e.vr_gib && e.carry_player == world)))
        {
            vector lo = e.origin + e.mins;
            vector hi = e.origin + e.maxs;
            float armed = VR_Melee_Armed(xHand, b, e);
            for(i = 0; i < n; ++i)
            {
                p0 = b.mh_pp[i];
                p1 = b.mh_cp[i];
                t = VR_SegmentBox(p0, p1, lo - '1 1 1' * r, hi + '1 1 1' * r);
                if(t < 0)
                {
                    continue;
                }
                if(!armed || vr_mpt_kind[i] == VR_MPT_GRIP)
                {
                    if(e == b.mh_hit_ent)
                    {
                        b.mh_hit_touch = time; // still in what it struck
                    }
                    if(!armed)
                    {
                        break;
                    }
                    continue;
                }
                striker = vr_mpt_kind[i] == VR_MPT_EDGE || vr_mpt_kind[i] == VR_MPT_FIST;
                // Of the striking points in it, the outermost (a blade strikes with its outer part, its root may have been
                // in the thing for a while); of the near end's, the first along the sweep.
                if((striker && i < edgeI) || (!striker && t >= hiltT))
                {
                    continue;
                }
                // On the box itself.
                vector at = p0 + (p1 - p0) * t;
                at_x = min(hi_x, max(lo_x, at_x));
                at_y = min(hi_y, max(lo_y, at_y));
                at_z = min(hi_z, max(lo_z, at_z));
                traceline(hand, at, TRUE, self);
                if(trace_fraction < 1)
                {
                    continue;
                }
                if(striker)
                {
                    edgeT = t;
                    edgeI = i;
                    edgeEnt = e;
                    edgeAt = at;
                }
                else
                {
                    hiltT = t;
                    hiltI = i;
                    hiltEnt = e;
                    hiltAt = at;
                }
            }
        }
        e = e.chain;
    }

    // The striking part first. The pommel's touch waits VR_MELEE_HILT_WAIT for the blade proper to reach the
    // same thing: then the blow is the blade's (a cut whose hands arrive first); else it lands as the pommel strike it
    // was.
    if(edgeI >= 0 && VR_Melee_Decide(xHand, b, edgeI, vr_mpt_kind[edgeI]))
    {
        if(b.mh_pend_ent == world || b.mh_pend_ent != edgeEnt || vr_mpt_f[edgeI] >= VR_MELEE_BLADE_FROM)
        {
            b.mh_pend_ent = world;
            b.mh_c_ent = edgeEnt;
            b.mh_c_pos = edgeAt;
            b.mh_c_dir = normalize(b.mh_cp[edgeI] - b.mh_pp[edgeI]);
            b.mh_c_normal = b.mh_c_dir * -1;
            return TRUE;
        }
    }
    if(hiltI >= 0 && b.mh_pend_ent == world && VR_Melee_Decide(xHand, b, hiltI, vr_mpt_kind[hiltI]))
    {
        b.mh_c_ent = hiltEnt;
        b.mh_c_pos = hiltAt;
        b.mh_c_dir = normalize(b.mh_cp[hiltI] - b.mh_pp[hiltI]);
        b.mh_c_normal = b.mh_c_dir * -1;
        b.mh_pend_ent = hiltEnt;
        b.mh_pend_time = time;
        b.mh_pend_pos = hiltAt;
        b.mh_pend_dir = b.mh_c_dir;
        b.mh_pend_kind = b.mh_kind;
        b.mh_pend_strength = b.mh_strength;
        b.mh_pend_speed = b.mh_speed;
        b.mh_pend_part = b.mh_part;
        b.mh_pend_way = b.mh_way;
    }
    if(b.mh_pend_ent != world && time - b.mh_pend_time >= VR_MELEE_HILT_WAIT)
    {
        // The blade never came: the pommel strike.
        b.mh_c_ent = b.mh_pend_ent;
        b.mh_c_pos = b.mh_pend_pos;
        b.mh_c_dir = b.mh_pend_dir;
        b.mh_c_normal = b.mh_c_dir * -1;
        b.mh_kind = b.mh_pend_kind;
        b.mh_strength = b.mh_pend_strength;
        b.mh_speed = b.mh_pend_speed;
        b.mh_part = b.mh_pend_part;
        b.mh_way = b.mh_pend_way;
        b.mh_pend_ent = world;
        return b.mh_c_ent.takedamage && VR_Melee_Armed(xHand, b, b.mh_c_ent);
    }
    if(edgeI < 0 && hiltI < 0 && wallI >= 0 && !VRGetMeleeHitSoundPlayed(xHand) &&
       VR_Melee_Decide(xHand, b, wallI, vr_mpt_kind[wallI]))
    {
        b.mh_c_ent = wallEnt;
        b.mh_c_pos = wallAt;
        b.mh_c_dir = normalize(b.mh_cp[wallI] - b.mh_pp[wallI]);
        b.mh_c_normal = wallNormal;
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
        return xThing == VR_MTHING_GUN ? "butt strike" : "pommel strike";
    }
    if(xKind == VR_MKIND_PUNCH)
    {
        return "punch";
    }
    if(xThing == VR_MTHING_SWORD)
    {
        return "slash";
    }
    if(xThing == VR_MTHING_AXE)
    {
        return "chop";
    }
    return "swing";
};

// The sound of a blow landing on something that takes damage (weapons.qc: the weapons' melee): `xSound`, the weapon's
// own, or for a pommel, hilt or butt strike a blunt knock of its own (vr/pommel1..3.wav, a little apart in pitch; made by
// make_sounds.py; one of the two not played last), apart from the blades' cuts and the punches. vr_melee_blunt is set
// around the weapons' melee by PlayerVRMeleeImpl (client.qc) for the blow being struck. `developer` prints which.
float vr_melee_blunt;
float vr_melee_pommel_last; // 1..3 (0: none yet)
string(string xSound) VR_Melee_HitSound =
{
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

// The weapon's whoosh as `xHand`'s striking part gets going as a blow would (once a motion): as fast as a blow needs,
// come from some way off (and a fist closed). The motion recorder's "stroke".
void(float xHand, entity b) VR_Melee_Whoosh =
{
    float minSpeed = VR_Melee_MinSpeedOf(b);
    if(b.mh_fast < minSpeed * VR_MELEE_REARM_SLOW)
    {
        b.mh_whooshed = FALSE;
        return;
    }
    if(b.mh_whooshed || b.mh_thing == VR_MTHING_NONE || vlen(b.mh_vgrip) < VR_Melee_SwingNeed(xHand, b, b.mh_vgrip))
    {
        return;
    }
    if(b.mh_thing == VR_MTHING_FIST && !VR_Melee_ClosedFist(xHand))
    {
        return; // an open hand: no blow
    }
    if(VR_Melee_Run(b, TRUE) < VR_MELEE_RUN || !VR_Melee_WristMoved(b))
    {
        return;
    }
    b.mh_whooshed = TRUE;
    VR_Motion_Event("stroke", VR_Melee_Category(b.mh_thing == VR_MTHING_FIST ? VR_MKIND_PUNCH : VR_MKIND_SLASH, b.mh_thing), xHand, vlen(b.mh_vfar), b.mh_far, world, "");
    string s = "knight/sword1.wav";
    if(b.mh_thing == VR_MTHING_AXE || b.mh_thing == VR_MTHING_CLUB)
    {
        s = "weapons/ax1.wav";
    }
    else if(b.mh_thing == VR_MTHING_GUN && b.mh_vfar_z < -0.6 * vlen(b.mh_vfar))
    {
        return; // a gun swung down: reaching for a holster to reload is fast too
    }
    else if(b.mh_thing == VR_MTHING_FIST && VR_Melee_OpenHand(xHand))
    {
        return;
    }
    sound(self, xHand == cVR_OffHand ? CHAN_WEAPON2 : CHAN_WEAPON, s, 1, ATTN_NORM);
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
.float vr_counter_until;    // on a player: a counter may land until then (0: no window),
.float vr_counter_since;    // the window opened then,
.float vr_counter_pulse_at; // the next pulse
.float vr_melee_hud;        // on a player: what the gadget shows of both (VR_Melee_Hud; the engine's STAT_QVR_MELEE)

float() VR_Stamina_Max =
{
    return max(1, cvar_hget(cvarh_vr_parry_stamina_max));
};

// Whether the pool is used: parries, shoves or blows spend it.
float() VR_Stamina_On =
{
    if(cvar_hget(cvarh_vr_parry_stamina) || cvar_hget(cvarh_vr_shove_stamina) || cvar_hget(cvarh_vr_strike_stamina))
    {
        return TRUE;
    }
    return FALSE;
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
        sound(p, CHAN_AUTO, "vr/counter_open.wav", volume, ATTN_NORM);
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
// open). Integers under 2^16, exact in a float.
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
    vector right = '0 0 0';
    right_x = xFwd_y;
    right_y = -xFwd_x;
    float side = VR_SHOULDER_SIDE;
    if((xHand == cVR_OffHand) != !!cvar("vr_lefthanded"))
    {
        side = -side;
    }
    vector out = b.mh_rgrip - (right * side - '0 0 1' * VR_SHOULDER_DOWN);
    return v * normalize(out) >= vlen(v) * VR_BASH_PALM_RADIAL;
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
            vector lo = e.origin + e.mins - '1 1 1' * pad;
            vector hi = e.origin + e.maxs + '1 1 1' * pad;
            float k;
            for(k = 0; k <= 3; ++k)
            {
                vector off = xForward * (reach * k / 3);
                if(VR_SegmentBox(a + off, c + off, lo, hi) >= 0)
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

// Knocks `e` back along `xForward`: `xScale` of vr_bash_damage and `xPush` of vr_bash_push, and a stagger (its pain,
// forced).
void(entity e, vector xAt, vector xForward, float xKind, float xHand, float xScale, float xPush) VR_Bash_Hit =
{
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
    VR_SetHitKind(xKind, xHand, xAt, xForward);
    T_Damage(e, self, self, dmg);
    VR_ClearHitKind();
    VR_Push(e, xForward, 520 * push, 140 * push);
    dprint(sprintf("bash hit: %s, %.1f damage, knocked back at %.0f u/s, staggered %.2f s\n", e.classname, dmg,
        vlen(e.velocity), stagger));
    if(e.health > 0 && e.th_pain)
    {
        entity oldSelf = self;
        self = e;
        e.pain_finished = 0;
        e.th_pain(oldSelf, dmg);
        e.attack_finished = time + stagger;
        self = oldSelf;
    }
    SpawnBloodSplash(e.origin + '0 0 16', 4);
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
// the frame's end) comes within vr_bash_deflect_radius of the guard -- each hand's weapon line, each empty hand, and
// the line between the hands while they are together -- and is ahead of the facing `xForward`, is sent the way the
// hands push (vr_bashdef_vel), as fast as the bash is strong: VR_BASH_BAT_SPEED times the push's speed over
// vr_bash_speed (VR_BASH_BAT_MIN .. VR_BASH_BAT_MAX), one hand's VR_BASH_ONE_PUSH of it, short of stamina its
// Exhausted Knockback (the push's stamina, VR_Bash_Pay). Met by a weapon's line, off the blade's face
// (VR_Bash_Direction). Then the aim assist (VR_Deflect_Assist, vr_juice.qc). An unarmed shove (vr_bashdef_gren) pushes
// only grenades.
void(vector xForward) VR_Bash_Deflect =
{
    float radius = max(0, cvar_hget(cvarh_vr_bash_deflect_radius));
    float dt = max(frametime, 0.011);
    entity bm = VR_Blow(cVR_MainHand);
    entity bo = VR_Blow(cVR_OffHand);
    vector mh = VRGetHandPos(cVR_MainHand);
    vector oh = VRGetHandPos(cVR_OffHand);
    vector ma = mh;
    vector mb = mh;
    vector oa = oh;
    vector ob = oh;
    if(bm.mh_thing != VR_MTHING_FIST && bm.mh_thing != VR_MTHING_NONE)
    {
        ma = bm.mh_near;
        mb = bm.mh_far;
    }
    if(bo.mh_thing != VR_MTHING_FIST && bo.mh_thing != VR_MTHING_NONE)
    {
        oa = bo.mh_near;
        ob = bo.mh_far;
    }
    float together = vlen(mh - oh) < 0.7 * VRMetersToUnits();
    vector head = VRGetHeadPos(self);

    entity e = findradius((mh + oh) * 0.5, 200);
    while(e)
    {
        if(VR_Deflect_IsProjectile(e) && time >= e.vr_deflect_time + 0.3 && (e.origin - head) * xForward > -8 &&
           (!self.vr_bashdef_gren || VR_Grenade_Is(e)))
        {
            vector from = e.origin - e.velocity * max(dt, VR_BAT_LATE * 2);
            vector to = e.origin + e.velocity * dt;
            // (Where it met: a hand's line, how far along it; -1 the line between the hands.)
            float near = VR_SegmentDistance(from, to, ma, mb);
            float onHand = cVR_MainHand;
            float onT = vr_seg_t;
            float nearOff = VR_SegmentDistance(from, to, oa, ob);
            if(nearOff < near)
            {
                near = nearOff;
                onHand = cVR_OffHand;
                onT = vr_seg_t;
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
                dprint(sprintf("deflect: %s of %s, %.0f units from the bash's guard, %.2f s into the push\n",
                    e.classname, e.owner.classname, near,
                    time - (self.vr_bashdef_until - max(0, cvar_hget(cvarh_vr_bash_deflect_window)))));
                vector pushVel = self.vr_bashdef_vel;
                float strength = vlen(pushVel) / (max(0.1, cvar_hget(cvarh_vr_bash_speed)) * VRMetersToUnits());
                float batSpeed = VR_BASH_BAT_SPEED * min(VR_BASH_BAT_MAX, max(VR_BASH_BAT_MIN, strength));
                if(!self.vr_bashdef_both)
                {
                    batSpeed = batSpeed * VR_BASH_ONE_PUSH;
                }
                VR_Bash_Pay(self.vr_bashdef_both, self.vr_bashdef_armed);
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
void(float xHand, vector xForward, vector xPushVel, float xBoth, float xArmed, float xGrenadesOnly)
    VR_Bash_DeflectStart =
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
        self.vr_bashdef_armed = xArmed;
        self.vr_bashdef_gren = xGrenadesOnly;
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
            b.mh_stance_since = time;
            b.mh_stance_said = FALSE;
        }
        if(stance == 1 && !b.mh_stance_said && time - b.mh_stance_since >= VR_STANCE_REPORT)
        {
            // (The parry poses are reported: a weapon across, crossed arms; not a palm facing ahead.)
            b.mh_stance_said = TRUE;
            VR_Melee_Event(sprintf("stance weapon, hand %g%s", h, VRGet2HAiming(self) ? ", two hands" : ""));
            VR_Motion_Event("stance", "weapon", h, 0, VRGetHandPos(h), world, VRGet2HAiming(self) ? "two hands" : "");
        }
        if(stance && cvar("developer") >= 2 && VR_Bash_PushVel(b) * pfwd > speed * 0.5)
        {
            dprint(sprintf("melee debug: hand %g stance %g push %v ends %v %v dist %.3f\n", h, stance,
                VR_Bash_PushVel(b), b.mh_vnear, b.mh_vfar, b.mh_push_dist));
            dprint(sprintf("melee debug: held %.2f ok %g pushing %g\n", time - b.mh_stance_since, b.mh_push_ok,
                VR_Bash_Pushing(h, b, pfwd, speed)));
        }
        // A push begun from the stance goes on while it pushes, the stance tilting or turning as it goes (a push with one
        // hand swings the far end forward).
        float going = b.mh_push_ok && b.mh_push_stance == 1 && time - b.mh_push_last <= VR_BASH_GAP;
        if(!stance && going)
        {
            b.mh_stance = 1; // (for the push's test: a weapon's)
        }
        if((stance || going) && VR_Bash_Pushing(h, b, pfwd, speed))
        {
            if(b.mh_push_dist <= 0 && time - b.mh_push_last > VR_BASH_GAP)
            {
                // A push begins: from a stance held (a weapon's; a palm's needs none).
                b.mh_push_ok = FALSE;
                b.mh_push_stance = stance;
                if(stance == 2)
                {
                    b.mh_push_ok = TRUE;
                }
                else if(time - b.mh_stance_since >= VR_BASH_HELD)
                {
                    b.mh_push_ok = TRUE;
                }
            }
            b.mh_pushing = b.mh_push_ok;
            b.mh_push_dist = b.mh_push_dist + (VR_Bash_PushVel(b) * pfwd) * b.mh_dt;
            b.mh_push_last = time;
        }
        else if(time - b.mh_push_last > VR_BASH_GAP)
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
        self.vr_arms_since = time;
    }
    else if(arms && self.vr_arms_since > 0 && time - self.vr_arms_since >= VR_STANCE_REPORT)
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

    // The motion recorder (vr_motion.qc): a push, whatever it meets; once a push (its frames in a row).
    if(time - self.vr_motion_pushseen > 0.15)
    {
        self.vr_push_start = time;
        VR_Motion_Event("push", armed ? "bash" : "shove", both ? -1 : readyHand, 0, VRGetHandPos(readyHand), world,
                        both ? "two hands" : "one hand");
    }
    self.vr_motion_pushseen = time;

    // The hands in the push strike nothing else in this motion.
    VRSetMeleeHitSoundPlayed(readyHand, TRUE);
    b.mh_hit_ent = world;
    b.mh_hit_time = time;
    b.mh_hit_touch = time;
    if(both)
    {
        VRSetMeleeHitSoundPlayed(other, TRUE);
        ob.mh_hit_ent = world;
        ob.mh_hit_time = time;
        ob.mh_hit_touch = time;
    }

    // Batting: a bash, or a shove while a hand holds a weapon; what it bats goes the way the hands push (off a blade's
    // face: VR_Bash_Direction, from the weapon lines' motion in the push). An unarmed shove pushes a grenade away (a
    // palm can't bat a spike).
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
        float weapon = armed || !VRIsHandEmpty(self, cVR_MainHand) || !VRIsHandEmpty(self, cVR_OffHand);
        VR_Bash_DeflectStart(both ? -1 : readyHand, fwd, pushVel, both, armed, !weapon);
    }

    if(time < self.vr_bash_time)
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
    float push = both ? 1 : VR_BASH_ONE_PUSH;
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
    self.vr_bash_time = time + VR_BASH_COOLDOWN;

    // Each its own sound (vr_bash_sound; round 18), apart from the blows'; 0: the old ones.
    float bashSound = min(1, cvar_hget(cvarh_vr_bash_sound));
    if(bashSound > 0)
    {
        sound(self, CHAN_AUTO, (counter && armed) ? "vr/bash_parry.wav" : (armed ? "vr/bash.wav" : "vr/shove.wav"),
              bashSound * (both ? 1 : 0.7), ATTN_NORM);
    }
    else
    {
        sound(self, CHAN_AUTO, armed ? "player/axhit2.wav" : "fisthit.wav", 1, ATTN_NORM);
        sound(self, CHAN_AUTO, "demon/dland2.wav", both ? 0.7 : 0.45, ATTN_NORM);
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
        if(fresh && cvar("developer") >= 3 && b.mh_thing != VR_MTHING_NONE)
        {
            dprint(sprintf("melee trace: %.3f hand %g thing %g wid %g bits %g", time, h, b.mh_thing, VRGetEntWeapon(self, h),
                self.vrbits0));
            dprint(sprintf(" grip %v far %v vfar %v vgrip %v wrist %v\n", b.mh_rgrip, b.mh_rfar, b.mh_vfar, b.mh_vgrip, b.mh_rwrist));
        }
    }
    VR_Stamina_Frame();
    VR_Counter_Frame();
    VR_Melee_Hud();
};

// The blow of `xHand` this frame, if any (PlayerVRMeleeImpl): FALSE, or TRUE with the contact in mh_c_*, the kind and
// strength set.
float(float xHand) VR_Melee_Strike =
{
    entity b = VR_Blow(xHand);
    VR_Melee_Rearm(xHand, b);
    if(!b.mh_fresh || b.mh_thing == VR_MTHING_NONE)
    {
        return FALSE;
    }
    VR_Melee_Whoosh(xHand, b);
    if(b.mh_pushing)
    {
        return FALSE; // a bash's push
    }
    float struck = VR_Melee_Sweep(xHand, b);
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

// After a blow of `xHand` landed on mh_c_ent: once per motion (VR_Melee_Rearm).
void(float xHand) VR_Melee_Landed =
{
    entity b = VR_Blow(xHand);
    b.mh_hit_ent = b.mh_c_ent;
    b.mh_hit_time = time;
    b.mh_hit_touch = time;
    vector v = b.mh_vfar;
    if(b.mh_kind == VR_MKIND_POMMEL)
    {
        v = b.mh_vgrip;
    }
    b.mh_hit_dir = normalize(v);
    string what = sprintf("%s, hand %g, %s (%s) with %s", VR_Melee_Category(b.mh_kind, b.mh_thing), xHand,
        VR_Melee_KindName(b.mh_kind, b.mh_thing), b.mh_way, b.mh_part);
    VR_Melee_Event(sprintf("%s, %.1f m/s, x%.2f, %s", what, b.mh_speed, b.mh_strength,
        b.mh_c_gib ? "a gib" : (b.mh_c_ent.takedamage ? b.mh_c_ent.classname : "a wall")));
};
