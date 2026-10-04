// vr_walltorch.qc -- wall torches you can take (docs/vr-port/ROUND21.md, "Wall torches you can take"; the engine's side:
// Quake/vr/vr_walltorch.cpp).
//
// With vr_walltorch on (read as the map spawns), id's wall torch (light_torch_small_walltorch) stays an entity instead
// of a static one, so that it can be taken: grip it (the fist on it, or within vr_walltorch_reach cm of its stick) and
// pull it vr_walltorch_pull cm out of its holder (a wooden scrape), or force grab it (vr_walltorch_forcegrab). The same
// entity is then the torch in the hand (saves and the entities' numbers are kept; the map's baked light stays on the
// wall, the flickering torch light goes with it). On its wall and taken it is our stick (progs/vrtorch.mdl, the wall
// torch's own skin copied in as it loads) with the wall torch's flame over its head, drawn by the engine as big as its
// fire (the stick's frame: 0 out, 1..16 sixteenths of its fire, 17 on its wall). It is carried, thrown and force grabbed as a box is (vr_carry.qc); its grip,
// fingers, weight and melee line are its Held Object Offsets (vr_props.inc, progs/vrtorch.mdl).
//
// Its fire: held, it burns for ever. Let go of (dropped, thrown), it starts dying: it goes out over vr_walltorch_die_time
// seconds; taken again before it is out, it burns up again. Its blows (a club: vr_melee.qc VR_MTHING_CLUB) use it up:
// after vr_walltorch_hits blows it dies even in the hand, and taking it again doesn't save it. Out, it is a stick (a
// club still); held in another torch's flame, in the map's other flames (light_flame_*, a lava ball) or in lava, a
// dying or burnt-out torch lights again (vr_walltorch_relight).
// In water it goes out at once.
//
// A lit torch's blow (and a thrown lit torch; with vr_burn_touch, its touch) sets what it hits on fire, a monster or a
// corpse: vr_burning.qc (Combat > Burning).

.float scale;           // (the engine's: an entity's drawn scale; a burning monster's flame)

.float wt_state;        // 0: on its wall; 1: taken (a stick with its fire, burning, dying or out)
.vector wt_home;        // where it hung (after a load, vr_walltorch.cpp puts back the map's torches a save lacks)
.float wt_hits;         // the blows it has landed since it was lit
.float wt_fire;         // its fire, 0 (out) .. 1
.float wt_dying;        // 0: not dying; 1: let go of (taken again, it burns up again); 2: its blows spent
.float wt_last;         // when its fire was last worked out
.float wt_out_time;     // when it went out (it smokes a while)
.float wt_crackle;      // its crackle is playing (the engine keeps it with it)
.entity wt_gripper;     // on its wall: the player whose hand grips it, pulling it out,
.float wt_grip_hand;    // which hand,
.vector wt_grip_from;   // where that hand was as it gripped it,
.float wt_grip_buzz;    // and the next haptic tick
.entity mainhand_wtgrip; // on a player: the torch each hand grips on its wall
.entity offhand_wtgrip;

float WT_LEVELS = 16;           // the stick's frames 1..16: its fire in sixteenths
float WT_WALL_FRAME = 17;       // the stick's frame on its wall (the engine draws id's flame on it, and its light)
vector WT_HEAD = '1.2 0 0';     // the stick's head (its pit: make_walltorch.py), in its model's axes
float WT_REGROW = 1;            // s a dying torch taken again takes to burn up again (a lit one's fire from nothing)

float(entity t) VR_WallTorch_IsTorch =
{
    return t.classname == "light_torch_small_walltorch";
};

float(entity t) VR_WallTorch_OnWall =
{
    return t.classname == "light_torch_small_walltorch" && t.wt_state == 0;
};

// ----------------------------------------------------------------------------
// Shot off its wall (vr_walltorch_shot; ROUND22.md, "Torches shot off their walls")
//
// What makes the engine meet a wall torch at all is its takedamage: Quake/world.c SV_ClipToLinks skips a
// SOLID_NOT_BUT_TOUCHABLE one unless the move is a shot's or a missile's (clip->hitgibs) and it takes damage, and
// vr_hitmodel.cpp VR_ShotTargetClip only reaches out round it when it takes damage too. With it, hitscan (the shotgun's
// pellets, the lightning), nails, rockets, lasers and grenades meet it; a body's move (a player's or a monster's), a
// gib's, a grenade's own and a melee blow's (no MOVE_HITGIBS) go through it as before. Its health stays -1 (dead
// meat): nothing alive is hurt by it and VR_Burn_Can never sets it on fire, and the damage is never dealt anyway
// (VR_WallTorch_Shot takes it off its wall first). Its shot box (vr_shot_radius) reaches past its stick and cup (its
// setsize stops 3 over its origin) into the flame id's flame draws 0..20 over it.
float VR_WALLTORCH_SHOT_RADIUS = 12; // units: the cube a shot or a missile meets a wall torch at (the engine's cap: 16)

void(entity t, float xOn) VR_WallTorch_SetShootable =
{
    if(!xOn)
    {
        t.takedamage = DAMAGE_NO;
        t.health = 0;
        t.vr_shot_radius = 0;
        return;
    }
    t.takedamage = DAMAGE_YES; // (not DAMAGE_AIM: no monster aims its shots at a torch)
    t.health = -1;             // (dead meat: it is never dealt any)
    t.vr_shot_radius = VR_WALLTORCH_SHOT_RADIUS;
};

// ----------------------------------------------------------------------------
// On its wall

entity(entity xPlayer, float xHand) VR_WallTorch_Gripped =
{
    if(xHand == cVR_OffHand)
    {
        return xPlayer.offhand_wtgrip;
    }
    return xPlayer.mainhand_wtgrip;
};

void(entity xPlayer, float xHand, entity xTorch) VR_WallTorch_SetGripped =
{
    if(xHand == cVR_OffHand)
    {
        xPlayer.offhand_wtgrip = xTorch;
    }
    else
    {
        xPlayer.mainhand_wtgrip = xTorch;
    }
};

// A torch's head (world): where its fire is.
vector(entity t) VR_WallTorch_Head =
{
    if(t.wt_state == 0)
    {
        return t.origin + '0 0 2';
    }
    return t.origin + modelpoint(t, t.angles, WT_HEAD);
};

// Its frame and skin: its fire as the engine draws it (vr_walltorch.cpp).
void(entity t) VR_WallTorch_Show =
{
    if(t.wt_state == 0)
    {
        t.frame = WT_WALL_FRAME;
        t.skin = 0;
    }
    else if(t.wt_fire > 0)
    {
        t.frame = max(1, min(WT_LEVELS, ceil(t.wt_fire * WT_LEVELS)));
        t.skin = 0;
    }
    else
    {
        t.frame = 0;
        t.skin = 1;
    }
};

// Its crackle (id's torches' fire, on the torch: the engine moves it with the torch, as loud as its fire), on or off.
void(entity t, float xOn) VR_WallTorch_Crackle =
{
    if(xOn == t.wt_crackle)
    {
        return;
    }
    t.wt_crackle = xOn;
    if(xOn)
    {
        sound(t, CHAN_BODY, "ambience/fire1.wav", 0.5, ATTN_STATIC);
    }
    else
    {
        sound(t, CHAN_BODY, "misc/null.wav", 0.5, ATTN_STATIC);
    }
};

void() VR_WallTorch_Think;
void() VR_WallTorch_Touch;

// A taken torch's Quake box: round its model as it is turned now (a long stick lying any way: a hand reaching for any
// part of it finds it; the hand then takes it by its drawn shape). Box3D moves it by its own shape, not this box.
void(entity t) VR_WallTorch_Box =
{
    vector lo = modelbounds(t, 0);
    vector hi = modelbounds(t, 1);
    vector mn = '0 0 0';
    vector mx = '0 0 0';
    vector c;
    vector p;
    float i;
    for(i = 0; i < 8; ++i)
    {
        c_x = (i & 1) ? hi_x : lo_x;
        c_y = (i & 2) ? hi_y : lo_y;
        c_z = (i & 4) ? hi_z : lo_z;
        p = modelpoint(t, t.angles, c);
        if(i == 0)
        {
            mn = p;
            mx = p;
        }
        else
        {
            mn_x = min(mn_x, p_x);
            mn_y = min(mn_y, p_y);
            mn_z = min(mn_z, p_z);
            mx_x = max(mx_x, p_x);
            mx_y = max(mx_y, p_y);
            mx_z = max(mx_z, p_z);
        }
    }
    setsize(t, mn, mx);
};

// Out of its holder: our stick with its fire (the same entity), a physics prop to carry, throw and force grab as a box.
void(entity t) VR_WallTorch_Take =
{
    if(t.wt_gripper != world)
    {
        VR_WallTorch_SetGripped(t.wt_gripper, t.wt_grip_hand, world);
        t.wt_gripper = world;
    }
    t.wt_state = 1;
    t.wt_hits = 0;
    t.wt_dying = 0;
    t.wt_fire = 1;
    t.wt_last = time;
    setmodel(t, "progs/vrtorch.mdl");
    t.angles_x = 90; // upright as it hung (its +x up: an alias model's pitch is inverted)
    t.angles_z = 0;
    VR_WallTorch_Box(t);
    t.solid = SOLID_NOT_BUT_TOUCHABLE;
    t.movetype = MOVETYPE_TOSS;
    t.velocity = '0 0 0';
    t.touch = VR_WallTorch_Touch;
    t.handtouch = SUB_Null; // nothing to take (VR_Carry_Setup keeps it as carry_use)
    t.flags = t.flags | FL_FORCEGRABBABLE;
    t.throwhit = QVR_THROWHIT_HIT;
    t.owner = world;
    SetThrownWeaponDealDamage(t, FALSE);
    SetThrownWeaponDisappearOnHit(t, FALSE);
    VR_Carry_Setup(t);
    VR_WallTorch_SetShootable(t, FALSE); // (a taken torch is a prop: shots and missiles go past it as before)
    t.think = VR_WallTorch_Think;
    t.nextthink = time + 0.1;
    VR_WallTorch_Show(t);
    sound(t, CHAN_AUTO, "vr/torch_pull.wav", 0.9, ATTN_NORM);
    VR_WallTorch_Crackle(t, TRUE);
    setorigin(t, t.origin);
};

// A hand's grip on it lets go before it came out.
void(entity t) VR_WallTorch_Ungrip =
{
    if(t.wt_gripper != world)
    {
        VR_WallTorch_SetGripped(t.wt_gripper, t.wt_grip_hand, world);
    }
    t.wt_gripper = world;
};

// Off its wall by a hit: out of its holder as it is by hand (still lit), then away from what hit it, tumbling. `by`
// what hit it (a thrown prop's own speed carries it along), `dir` from it towards the torch.
float VR_WALLTORCH_KNOCK_SPEED = 260; // units/s along the hit (about 1.8 m/s)
float VR_WALLTORCH_KNOCK_UP = 70;     // ...and up: out of its holder
float VR_WALLTORCH_KNOCK_SPIN = 6;    // rad/s of tumble (a thrown thing's own: up to vr_throw_spin_max)
float VR_WALLTORCH_CARRY_SPEED = 200; // at most, a share of what hit it that the torch takes with it
float VR_WALLTORCH_HIT_SPEED = 32;    // slower than this, the thing that hit it is not going anywhere (units/s)

// Which way a hit sends it off its wall: the way the thing that hit it was going (a spike, a rocket, a thrown prop);
// or, when the damage comes from the shooter himself (a hitscan shot: his trace, a lightning bolt), from him to it.
// A torch stopped by a hit has no speed left, so its box is not the way it came: a shot from the far side of the room
// would send it up, which is not what a hit on a wall does.
vector(entity t, entity by) VR_WallTorch_HitDir =
{
    vector vel = by == world ? '0 0 0' : by.velocity;
    if(vlen(vel) > VR_WALLTORCH_HIT_SPEED)
    {
        return vel;
    }
    vector from = (by.flags & FL_CLIENT) ? (by.origin + by.view_ofs) : 0.5 * (by.absmin + by.absmax);
    return t.origin - from;
};

void(entity t, entity by, vector dir) VR_WallTorch_KnockOff =
{
    vector head = VR_WallTorch_Head(t); // (on its wall: the foot of its flame)
    vector flat = dir;
    flat_z = 0; // (it is on a wall: what sends it off is the way across the room; the up below is its coming out)
    vector away = normalize(flat);
    if(away == '0 0 0')
    {
        away = '0 0 1'; // (nothing to aim at: it comes off straight up)
    }
    dprint(sprintf("walltorch: knocked off its wall by %s along %v\n",
        by == world ? "a hit" : (by.classname != "" ? by.classname : by.model), away));
    VR_WallTorch_Ungrip(t);
    VR_WallTorch_Take(t); // (still lit: wt_fire 1; its wall light goes with its frame: vr_emissive.cpp walltorch::onWall)
    float bySpeed = by == world ? 0 : vlen(by.velocity);
    t.velocity = away * VR_WALLTORCH_KNOCK_SPEED + '0 0 1' * VR_WALLTORCH_KNOCK_UP +
                 (bySpeed > 0 ? normalize(by.velocity) * min(VR_WALLTORCH_CARRY_SPEED, bySpeed * 0.5) : '0 0 0');
    t.avelocity = '0 0 0';
    t.vr_spin = normalize(away + '0 0 1' * 0.5 + '1 1 0' * crandom()) * VR_WALLTORCH_KNOCK_SPIN;
    setorigin(t, t.origin);
    sound(t, CHAN_AUTO, "vr/torch_hit.wav", 1.0, ATTN_NORM);
    particle2(head, away * 90, QVR_PARTICLE_PRESET_LAVASPIKE, 12); // embers off the hit
    particle2(head, away * 60, QVR_PARTICLE_PRESET_SPARKS, 6);
};

// A wall torch met by a shot, a missile, an explosion's radius damage or a thrown prop's blow (combat.qc
// T_DamageImpl): it comes off its wall. TRUE: its coming off is what the damage did (none of it is dealt to it, and it
// is never gibbed). Not a melee blow (xMelee): a fist closed on the way to a torch keeps taking it by hand, and a
// torch's own club is not a shot. Off (vr_walltorch_shot 0): as in id's Quake, every shot goes through it.
float(entity targ, entity inflictor, entity attacker, float damage, float xMelee) VR_WallTorch_Shot =
{
    if(!cvar_hget(cvarh_vr_walltorch_shot) || xMelee || damage <= 0)
    {
        return FALSE;
    }
    entity by = inflictor;
    if(by == world || by == attacker)
    {
        by = attacker; // (a hitscan shot's damage comes from the player himself)
    }
    VR_WallTorch_KnockOff(targ, by, VR_WallTorch_HitDir(targ, by));
    return TRUE;
};

// A grip that may take a torch on its wall: pressed now or a moment ago (VR_Carry_GripPressed, as a box), or held since a
// press at most vr_walltorch_grab_time ago with the hand slower than a punch: a fist closed on the way to the torch takes
// it as it gets there (NOTES.md vrfiringrange_2026-10-02_15-56-57: a grip pressed before the hand was within reach, more
// than the box's 0.2 s, never took it; the walltorch_grab_test.sh reaches pressed as they start missed 24 of 30).
float VR_WALLTORCH_PUNCH_SPEED = 2.5; // m/s of the hand (the flashlight's late grip's punch)
float(entity xPlayer, float xHand) VR_WallTorch_GripFresh =
{
    if(VR_Carry_GripPressed(xPlayer, xHand))
    {
        return TRUE;
    }
    if(!VR_HandGrabUtil_IsHandGrabbing(xPlayer, xHand) ||
       time - VR_HandGrabUtil_Impl_GetLastGrabTime(xPlayer, xHand) > cvar_hget(cvarh_vr_walltorch_grab_time) ||
       VRHasBit(xPlayer.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_CLIMBING : QVR_VRBITS0_OFFHAND_CLIMBING))
    {
        return FALSE; // (not a hand holding a ledge)
    }
    vector vel = xHand == cVR_OffHand ? xPlayer.offhandvel : xPlayer.handvel; // (m/s: the controller's, in the play space)
    return vlen(vel) < VR_WALLTORCH_PUNCH_SPEED;
};

// Whether `xPlayer`'s hand `xHand` may take hold of torch `t` on its wall now (its grip fresh, nothing in it).
float(entity t, entity xPlayer, float xHand) VR_WallTorch_CanGrip =
{
    return t.wt_state == 0 && t.wt_gripper == world && cvar_hget(cvarh_vr_carry) &&
           VR_WallTorch_GripFresh(xPlayer, xHand) && VRIsHandEmpty(xPlayer, xHand) &&
           !VR_Forcegrab_IsHandBusy(xPlayer, xHand) && VR_WallTorch_Gripped(xPlayer, xHand) == world;
};

// `xPlayer`'s hand `xHand` takes hold of torch `t` on its wall (`xHow`: the log's); pulled far enough, it comes out
// (VR_WallTorch_HandFrame).
void(entity t, entity xPlayer, float xHand, string xHow) VR_WallTorch_Grip =
{
    t.wt_gripper = xPlayer;
    t.wt_grip_hand = xHand;
    t.wt_grip_from = VRGetEntHandPos(xPlayer, xHand);
    t.wt_grip_buzz = 0;
    VR_WallTorch_SetGripped(xPlayer, xHand, t);
    dprint(sprintf("walltorch: gripped on its wall (%s)\n", xHow));
    if(xPlayer.ishuman)
    {
        entity oldSelf = self;
        self = xPlayer;
        haptic(xHand, 0.0, 0.06, 60, 0.45);
        self = oldSelf;
    }
};

// A hand touching it on its wall (handtouch: the fist on its drawn stick): the grip pressed on it takes hold of it.
void() VR_WallTorch_Handtouch =
{
    if(other.classname != "player" || VRIsTouchingHandFake(other))
    {
        return;
    }
    float hand = VRGetTouchingHand(other);
    if(VR_WallTorch_CanGrip(self, other, hand))
    {
        VR_WallTorch_Grip(self, other, hand, "touched");
    }
};

// The torch on its wall nearest `self`'s hand `xHand` within vr_walltorch_reach cm of its stick (from its butt to the
// flame's foot), world if none: a grip near it takes it though the fist isn't quite on it (NOTES.md
// vrfiringrange_2026-09-30_11-22-44: torches were hard to take by hand). Measured from the hand's point (the game's,
// behind the fist: at the wrist's end of the palm) or the fist's middle, whichever is nearer: a fist closed round the
// stick from in front has its point a palm's length short of it. `vr_wt_near_dist`: how far it was (cm).
float VR_WALLTORCH_FIST_MIDDLE = 0.065; // m from the hand's point along where it points (the fist's middle sphere)
float vr_wt_near_dist;
float(entity t, vector xAt) VR_WallTorch_StickDist =
{
    // Its stick upright: the butt 14 under its origin, the flame's foot 3 over it.
    vector p = t.origin;
    p_z = min(t.origin_z + 3, max(t.origin_z - 14, xAt_z));
    return vlen(xAt - p);
};
entity(float xHand) VR_WallTorch_NearHand =
{
    float reach = max(0, cvar_hget(cvarh_vr_walltorch_reach)) * VRMetersToUnits() / 100;
    if(reach <= 0)
    {
        return world;
    }
    vector hand = VRGetHandPos(xHand);
    vector oldForward = v_forward, oldRight = v_right, oldUp = v_up; // (the caller's, kept)
    makevectors(VRGetHandRot(xHand));
    vector fist = hand + v_forward * (VR_WALLTORCH_FIST_MIDDLE * VRMetersToUnits());
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    entity best = world;
    float bestD = reach;
    entity t = findradius(hand, reach + 20);
    while(t != world)
    {
        if(t.classname == "light_torch_small_walltorch" && t.wt_state == 0)
        {
            float d = min(VR_WallTorch_StickDist(t, hand), VR_WallTorch_StickDist(t, fist));
            if(d <= bestD)
            {
                bestD = d;
                best = t;
            }
        }
        t = t.chain;
    }
    vr_wt_near_dist = bestD * 100 / VRMetersToUnits();
    return best;
};

// Every player frame, for each hand (self is the player; W_Frame): a hand gripping a torch on its wall pulls it out.
void(float xHand) VR_WallTorch_HandFrame =
{
    entity t = VR_WallTorch_Gripped(self, xHand);
    if(t == world)
    {
        // Not quite on one: a grip near a torch takes it (vr_walltorch_reach), or one closed on the way to it
        // (vr_walltorch_grab_time).
        if(self.health > 0 && VR_WallTorch_GripFresh(self, xHand) && VRIsHandEmpty(self, xHand))
        {
            t = VR_WallTorch_NearHand(xHand);
            if(t != world && VR_WallTorch_CanGrip(t, self, xHand))
            {
                VR_WallTorch_Grip(t, self, xHand, sprintf("near, %.1f cm; %.2f s after the press, the hand at %.1f m/s", vr_wt_near_dist, time - VR_HandGrabUtil_Impl_GetLastGrabTime(self, xHand), vlen(xHand == cVR_OffHand ? self.offhandvel : self.handvel)));
            }
        }
        return;
    }
    if(t.wt_state != 0 || t.wt_gripper != self || t.wt_grip_hand != xHand || self.health <= 0 ||
       !VR_HandGrabUtil_IsHandGrabbing(self, xHand) || !VRIsHandEmpty(self, xHand))
    {
        dprint("walltorch: let go of on its wall\n");
        if(t.wt_gripper == self && t.wt_grip_hand == xHand)
        {
            VR_WallTorch_Ungrip(t);
        }
        else
        {
            VR_WallTorch_SetGripped(self, xHand, world);
        }
        return;
    }
    float pulled = vlen(VRGetHandPos(xHand) - t.wt_grip_from) * 100 / VRMetersToUnits(); // cm
    float need = max(0, cvar_hget(cvarh_vr_walltorch_pull));
    if(pulled >= need)
    {
        dprint(sprintf("walltorch: pulled out by hand %g (%.1f cm)\n", xHand, pulled));
        VR_WallTorch_Take(t);
        vr_carry_quiet = TRUE;
        VR_Carry_Start(self, xHand, t);
        vr_carry_quiet = FALSE;
        haptic(xHand, 0.0, 0.08, 80, 0.8);
        return;
    }
    // Held in its holder: a grating buzz, stronger as it comes.
    if(time >= t.wt_grip_buzz)
    {
        t.wt_grip_buzz = time + 0.05;
        haptic(xHand, 0.0, 0.05, 45 + pulled * 3, 0.15 + 0.45 * pulled / max(1, need));
    }
};

// Force grabbed: off its wall as it flies (VR_Forcegrab_Launch); lying about, as a box. Never while a hand pulls it.
float(entity e) VR_WallTorch_ForceGrabbable =
{
    if(!cvar_hget(cvarh_vr_walltorch_forcegrab) || !propvalue(e, "forcegrab"))
    {
        return FALSE;
    }
    if(e.wt_state == 0)
    {
        return e.wt_gripper == world;
    }
    return e.solid != SOLID_NOT && e.carry_player == world &&
           ((e.flags & FL_ONGROUND) || vlen(e.velocity) < 300);
};

// ----------------------------------------------------------------------------
// Its fire

void(entity t, float xWhy) VR_WallTorch_StartDying =
{
    if(t.wt_fire <= 0 || t.wt_dying >= xWhy)
    {
        return;
    }
    t.wt_dying = xWhy;
    dprint(sprintf("walltorch: dying (%s) at %.2f s, fire %.2f, gone in %.1f s\n", xWhy == 2 ? "its blows spent" : "let go of", time,
        t.wt_fire, t.wt_fire * cvar_hget(cvarh_vr_walltorch_die_time)));
};

void(entity t) VR_WallTorch_GoOut =
{
    t.wt_fire = 0;
    t.wt_dying = 0;
    t.wt_out_time = time;
    VR_WallTorch_Crackle(t, FALSE);
    sound(t, CHAN_AUTO, "vr/torch_out.wav", 0.8, ATTN_NORM);
    particle2(VR_WallTorch_Head(t), '0 0 20', QVR_PARTICLE_PRESET_SMOKE, 4);
    VR_WallTorch_Show(t);
    dprint(sprintf("walltorch: out at %.2f s\n", time));
};

void(entity t, string xFrom) VR_WallTorch_Relight =
{
    t.wt_fire = max(t.wt_fire, 1 / WT_LEVELS);
    t.wt_hits = 0;
    t.wt_dying = 0;
    t.wt_last = time;
    sound(t, CHAN_AUTO, "vr/torch_light.wav", 0.8, ATTN_NORM);
    VR_WallTorch_Crackle(t, TRUE);
    VR_WallTorch_Show(t);
    dprint(sprintf("walltorch: lit again from %s\n", xFrom));
};

// The nearest point of the segment a..b to p, how far.
float(vector p, vector a, vector b) VR_WallTorch_SegmentDist =
{
    vector ab = b - a;
    float l2 = ab * ab;
    float k = l2 > 0 ? min(1, max(0, ((p - a) * ab) / l2)) : 0;
    return vlen(p - (a + ab * k));
};

// The map's flames (light_flame_large_yellow, light_flame_small_yellow, light_flame_small_white) are static entities,
// gone from the entity list: each leaves its place here as it spawns, for VR_WallTorch_FireAt (not entities: those
// would move the map's edict numbers). Set at each map's spawn, which a loaded save runs too (before its own globals).
float VR_FIRE_SPOTS_MAX = 128;
vector vr_fire_spot[128];
float vr_fire_spot_large[128]; // flame2.mdl's frame: 0 the small flame, 1 the large one
float vr_fire_spots;

void(float xLarge) VR_FireSpot =
{
    if(vr_fire_spots >= VR_FIRE_SPOTS_MAX)
    {
        return;
    }
    vr_fire_spot[vr_fire_spots] = self.origin;
    vr_fire_spot_large[vr_fire_spots] = xLarge;
    vr_fire_spots = vr_fire_spots + 1;
};

// A fire the head `xHead` of torch `t` is in: another torch's flame (on its wall, or taken and lit), one of the map's
// flames, a lava ball, or lava. "" none.
string(entity t, vector xHead) VR_WallTorch_FireAt =
{
    if(pointcontents(xHead) == CONTENT_LAVA)
    {
        return "lava";
    }
    float i;
    for(i = 0; i < vr_fire_spots; i++)
    {
        // flame2.mdl's ball: the small one about 6 units wide, up to 16 over its origin; the large one 13 wide, to 39.
        // A little more all round: held into it, the torch's head is in it.
        vector o = vr_fire_spot[i];
        if(vr_fire_spot_large[i])
        {
            if(VR_WallTorch_SegmentDist(xHead, o + '0 0 -6', o + '0 0 32') < 12)
            {
                return "a large flame";
            }
        }
        else if(VR_WallTorch_SegmentDist(xHead, o + '0 0 -6', o + '0 0 12') < 9)
        {
            return "a flame";
        }
    }
    entity f;
    for(f = find(world, classname, "fireball"); f; f = find(f, classname, "fireball"))
    {
        if(f.model != "" && vlen(f.origin - xHead) < 16) // (misc_fireball's spawner, in the lava, has no model)
        {
            return "a lava ball";
        }
    }
    // Every torch by its class, not findradius: that skips SOLID_NOT entities, and a torch in a hand is one
    // (VR_Carry_Start), so a lit torch held in the other hand never lit this one (only once it was let go of).
    entity e;
    for(e = find(world, classname, "light_torch_small_walltorch"); e; e = find(e, classname, "light_torch_small_walltorch"))
    {
        if(e == t || vlen(e.origin - xHead) > 64) // (a torch's head is well within it of its origin)
        {
            continue;
        }
        if(e.wt_state == 0)
        {
            // Id's flame: from the cup up (the lower two thirds of it are hot enough).
            if(VR_WallTorch_SegmentDist(xHead, e.origin + '0 0 2', e.origin + '0 0 20') < 6)
            {
                return "a torch on its wall";
            }
        }
        else if(e.wt_fire >= 0.25)
        {
            vector h = VR_WallTorch_Head(e);
            if(VR_WallTorch_SegmentDist(xHead, h, h + '0 0 1' * (16 * e.wt_fire)) < 5)
            {
                return "a burning torch";
            }
        }
    }
    return "";
};

// Ten times a second while it is taken: its fire burns, dies, grows back, goes out in water, lights again in a fire.
void() VR_WallTorch_Think =
{
    self.nextthink = time + 0.1;
    entity t = self;
    if(t.wt_state != 1)
    {
        return;
    }
    float dt = min(0.5, max(0, time - t.wt_last));
    t.wt_last = time;
    vector head = VR_WallTorch_Head(t);
    float held = t.carry_player != world || t.fg_state == 1;
    if(!held)
    {
        held = pointcontents(head) == CONTENT_LAVA; // (lying in lava, it burns as if held)
    }
    if(t.carry_player == world && t.fg_state != 1)
    {
        VR_WallTorch_Box(t); // (where it lies, as it lies)
    }

    if(t.wt_fire > 0)
    {
        float c = pointcontents(head);
        if(c == CONTENT_WATER || c == CONTENT_SLIME)
        {
            dprint("walltorch: into the water\n");
            VR_WallTorch_GoOut(t);
            particle2(head, '0 0 30', QVR_PARTICLE_PRESET_BIGSMOKE, 6);
            return;
        }
        if(t.wt_dying == 1 && held)
        {
            t.wt_dying = 0;
            dprint(sprintf("walltorch: taken again at fire %.2f: it burns up again\n", t.wt_fire));
        }
        else if(!t.wt_dying && !held)
        {
            VR_WallTorch_StartDying(t, 1);
        }
        if(t.wt_dying)
        {
            t.wt_fire = t.wt_fire - dt / max(0.1, cvar_hget(cvarh_vr_walltorch_die_time));
            if(t.wt_fire <= 0)
            {
                VR_WallTorch_GoOut(t);
                return;
            }
        }
        else if(t.wt_fire < 1)
        {
            t.wt_fire = min(1, t.wt_fire + dt / WT_REGROW);
        }
    }
    else if(time - t.wt_out_time < 3 && random() < 0.25)
    {
        particle2(head, '0 0 12', QVR_PARTICLE_PRESET_SMOKE, 1); // out: a wisp of smoke a while
    }

    // Lit, its head touching a monster or a corpse sets it on fire (vr_burn_touch).
    if(t.wt_fire > 0)
    {
        VR_Burn_TorchTouch(t, head, t.carry_player != world ? t.carry_player : VR_Throw_Thrower(t));
    }
    // Held lit, its flame on you burns you, upside down on the hand holding it drops it (vr_burn_self, vr_burn_drop).
    VR_Burn_OnYou(t, dt);

    // In a fire: a dying (its blows spent) or burnt-out torch lights again, as new.
    if(cvar_hget(cvarh_vr_walltorch_relight) && (t.wt_fire <= 0 || t.wt_dying == 2))
    {
        string from = VR_WallTorch_FireAt(t, head);
        if(from != "")
        {
            VR_WallTorch_Relight(t, from);
        }
    }
    VR_WallTorch_Show(t);
};

// A blow landed (VR_WallTorch_Melee): once a motion.
void(entity t) VR_WallTorch_Used =
{
    if(t.wt_fire <= 0)
    {
        return;
    }
    t.wt_hits = t.wt_hits + 1;
    float most = cvar_hget(cvarh_vr_walltorch_hits);
    dprint(sprintf("walltorch: blow %g of %g\n", t.wt_hits, most));
    if(most > 0 && t.wt_hits >= most)
    {
        VR_WallTorch_StartDying(t, 2);
    }
};

// ----------------------------------------------------------------------------
// Burning what it hits (vr_burning.qc)

// Thrown lit into a monster or a corpse, it sets it on fire too (and hurts it as a thrown box does: forcegrabbable_touch).
// On its wall (wt_state 0) only a thing thrown into it hard knocks it off (VR_WallTorch_KnockOff).
void() VR_WallTorch_Touch =
{
    if(self.wt_state == 0)
    {
        // On its wall. Its box is touched by everything that moves past it (a SOLID_NOT_BUT_TOUCHABLE one is linked,
        // and VR_TouchLinks takes it: a player's or a monster's body, a prop resting against it, one carried or pushed
        // slowly); a hand takes it by its handtouch, never here. What knocks it off is a thing thrown into it at the
        // speed a throw of that thing counts as a throw (VR_Thrown_HitSpeed): what has already hit something is not in
        // a throw, and what is not in one is resting against it.
        if(other == world || other == self.wt_gripper || other.classname == "player" ||
           (other.flags & FL_MONSTER))
        {
            return;
        }
        float need = VR_Thrown_HitSpeed(other);
        float near = VR_Prop_Approach(other, self, other.velocity);
        if(other.throwhit != QVR_THROWHIT_NEVER_HIT || near < need)
        {
            dprint(sprintf("walltorch: %s past it at %g u/s (a throw of it from %g), throwhit %g\n",
                other.classname, near, need, other.throwhit));
            return;
        }
        VR_WallTorch_KnockOff(self, other, VR_WallTorch_HitDir(self, other));
        return;
    }
    if(self.wt_state == 1 && self.wt_fire > 0 && self.throwhit == QVR_THROWHIT_NEVER_HIT && other.takedamage &&
       other != self.owner)
    {
        VR_Burn_Ignite(other, VR_Throw_Thrower(self), self.origin, self.velocity, VR_BURN_BLOW); // (yours, past the grace too)
    }
    forcegrabbable_touch();
};

// ----------------------------------------------------------------------------
// Blows

// W_FistMelee (weapons.qc), for a hand carrying a taken torch: a club (its line: vr_melee.qc VR_MTHING_CLUB). A knock of
// wood; lit, a burst of fire, and what it hits burns. Each blow (a motion's first contact, a wall's too) uses its fire.
// FALSE: the hand carries no torch.
float(float xHand) VR_WallTorch_Melee =
{
    entity t = VR_Carry_Held(self, xHand);
    if(t == world || !VR_WallTorch_IsTorch(t) || t.wt_state != 1)
    {
        return FALSE;
    }
    float xdmg = VRMeleeDmg(xHand, cvar_hget(cvarh_vr_walltorch_damage));
    if(!VRDoMeleeAttackTracelineFor(xHand, 16, 8))
    {
        return TRUE;
    }
    vector org = trace_endpos - v_forward * 4;
    float lit = t.wt_fire > 0;
    float first = VRGetMeleeHitSoundPlayed(xHand) == FALSE;
    if(first)
    {
        VRMeleeHapticByHand(xHand);
        vr_melee_blunt = TRUE; // a wooden knock (VR_Melee_HitSound's); on an explosive box a metal one
        sound(self, VRGetGunChannel(xHand), VR_Melee_HitSoundOn(trace_ent, "fisthit.wav", FALSE), 1.0, ATTN_NORM);
        vr_melee_blunt = FALSE;
        if(lit)
        {
            sound(t, CHAN_AUTO, "vr/torch_hit.wav", 0.9, ATTN_NORM);
        }
        VRSetMeleeHitSoundPlayed(xHand, TRUE);
        VR_WallTorch_Used(t);
    }
    if(trace_ent.takedamage)
    {
        trace_ent.axhitme = 1;
        if(lit && VR_CratePiece_Is(trace_ent))
        {
            VR_Burn_Ignite(trace_ent, self, org, v_forward, VR_BURN_BLOW); // (first: a piece set on fire isn't burst by the blow)
        }
        entity victim = trace_ent; // (T_Damage may trace: an explosive box blowing up)
        if(T_Damage_VRMelee(trace_ent, self, self, xdmg))
        {
            VR_HitBloodSplash(victim, org, xdmg);
        }
        if(lit)
        {
            VR_Burn_Ignite(victim, self, org, v_forward, VR_BURN_BLOW);
            particle2(org, v_forward * -40, QVR_PARTICLE_PRESET_LAVASPIKE, 8);
        }
        return TRUE;
    }
    // A wall: sparks and embers knocked off it (lit), splinters' dust (out).
    if(lit)
    {
        particle2(org, trace_plane_normal * 60, QVR_PARTICLE_PRESET_LAVASPIKE, 10);
        particle2(org, trace_plane_normal * 40, QVR_PARTICLE_PRESET_SPARKS, 6);
    }
    else
    {
        BroadcastGunshotEffect(org);
    }
    return TRUE;
};

// ----------------------------------------------------------------------------
// Spawning (light_torch_small_walltorch, misc.qc)

void() VR_WallTorch_Setup =
{
    precache_model("progs/vrtorch.mdl");
    precache_model("progs/flame2.mdl");
    setmodel(self, "progs/vrtorch.mdl"); // our stick with id's skin, the engine drawing id's flame on it (vr_walltorch.cpp)
    self.angles_x = 90; // upright (its +x up: an alias model's pitch is inverted)
    precache_sound("vr/torch_pull.wav");
    precache_sound("vr/torch_out.wav");
    precache_sound("vr/torch_light.wav");
    precache_sound("vr/torch_hit.wav");
    precache_sound("vr/nail_sizzle1.wav"); // a nail through its flame (vr_burning.qc)
    precache_sound("vr/nail_sizzle2.wav");
    precache_sound("ambience/fire1.wav");
    precache_sound("misc/null.wav");
    self.wt_state = 0;
    self.wt_home = self.origin;
    self.wt_fire = 1;
    self.solid = SOLID_NOT_BUT_TOUCHABLE;
    self.movetype = MOVETYPE_NONE;
    self.flags = self.flags | FL_FORCEGRABBABLE; // (the hand takes it by its drawn shape: vr_physics.cpp handOn)
    self.handtouch = VR_WallTorch_Handtouch;
    self.touch = VR_WallTorch_Touch; // (what is thrown into it: the wt_state 0 branch)
    setsize(self, '-3 -3 -14', '3 3 3'); // its stick and cup (not its flame)
    setorigin(self, self.origin);
    VR_WallTorch_SetShootable(self, cvar_hget(cvarh_vr_walltorch_shot) != 0);
    VR_WallTorch_Show(self);
};

// ----------------------------------------------------------------------------
// `vr_test_walltorch_shot <kind>` (Debug > Tests; polled in your own context from W_Frame, weapons.qc)
//
// 10 a shotgun's pellets, 1 a nail, 0 a rocket, 2 a grenade, 3 a super nail, 4 an enforcer's laser: what VR_Test_Fire
// fires (weapons.qc) from your eyes at the wall torch nearest you. 20: the loose prop nearest you, set 40 units from it
// on the wall's own line past it (not at it from where you stand, so you are not in the way) and sent at it at
// vr_test_fling_speed m/s. 21: a lightning bolt at it (LightningDamage, as W_FireLightning). 22: a blast at it
// (T_RadiusDamage, as a rocket's or a grenade's). -1 (the default) asks for nothing. With developer 1 it prints what
// it did (walltorch:).
entity(float xKind, vector xAt) VR_Test_Fire; // (weapons.qc)
void(vector p1, vector p2, entity from, float damage) LightningDamage; // (weapons.qc)
float VR_WALLTORCH_TEST_THROW = 20;  // the kind that throws a prop instead of firing a gun
float VR_WALLTORCH_TEST_BOLT = 21;   // a lightning bolt along the line from your eyes to it
float VR_WALLTORCH_TEST_BLAST = 22;  // a blast at it, of a rocket's or a grenade's radius damage
float VR_WALLTORCH_TEST_REACH = 1024; // how near a wall torch it will take
float VR_WALLTORCH_TEST_PROPS = 512;  // how near a loose prop to throw
float VR_WALLTORCH_TEST_BLAST_RADIUS = 150; // a rocket's (weapons.qc T_MissileTouch)

void() VR_WallTorch_TestShot =
{
    float kind = cvar("vr_test_walltorch_shot");
    if(kind < 0)
    {
        return;
    }
    cvar_set("vr_test_walltorch_shot", "-1");
    if(self.health <= 0)
    {
        return;
    }
    entity t = world;
    float best = VR_WALLTORCH_TEST_REACH;
    float all = 0; // (how many this map has at all: so that no wall torch within reach says the map has none)
    for(entity e = nextent(world); e != world; e = nextent(e))
    {
        if(e.classname != "light_torch_small_walltorch" || e.wt_state != 0)
        {
            continue;
        }
        all = all + 1;
        float d = vlen(e.origin - self.origin);
        if(d < best)
        {
            best = d;
            t = e;
        }
    }
    if(t == world)
    {
        dprint(sprintf("walltorch test: no wall torch on a wall within reach (%g in this map)\n", all));
        return;
    }
    vector mid = 0.5 * (t.absmin + t.absmax);
    dprint(sprintf("walltorch test: kind %g at the wall torch at %v\n", kind, t.origin));
    if(kind >= VR_WALLTORCH_TEST_BOLT)
    {
        if(kind == VR_WALLTORCH_TEST_BOLT)
        {
            dprint("walltorch test: a lightning bolt at it\n");
            LightningDamage(self.origin + self.view_ofs, mid, self, cvar_hget(cvarh_vr_dmg_lightning));
            return;
        }
        entity blast = spawn();
        setorigin(blast, t.origin);
        dprint("walltorch test: a blast at it\n");
        T_RadiusDamage(blast, self, VR_WALLTORCH_TEST_BLAST_RADIUS, world);
        remove(blast);
        return;
    }
    if(kind >= VR_WALLTORCH_TEST_THROW)
    {
        entity p = world;
        best = VR_WALLTORCH_TEST_PROPS;
        for(entity e = nextent(world); e != world; e = nextent(e))
        {
            if(!e.vr_rigid || e.carry_player != world || e == t ||
               !(e.touch == forcegrabbable_touch || e.vr_impact))
            {
                continue;
            }
            float d = vlen(e.origin - self.origin);
            if(d < best)
            {
                best = d;
                p = e;
            }
        }
        if(p == world)
        {
            dprint("walltorch test: no loose prop to throw\n");
            return;
        }
        vector to = normalize(mid - (self.origin + self.view_ofs)); // (the line from your eyes to it)
        vector dir; // (along the wall past it, not at it from where you stand: you are not in the way of the throw)
        dir_x = -to_y;
        dir_y = to_x;
        dir_z = 0;
        float speed = cvar("vr_test_fling_speed") * VRMetersToUnits();
        p.owner = world;
        p.carry_player = world;
        p.carry_letgo = world;
        p.throwhit = QVR_THROWHIT_NEVER_HIT; // (a throw, not a prop resting where it fell)
        p.vr_fling_hit = 0;
        p.fg_state = 0;
        p.flags = p.flags - (p.flags & FL_ONGROUND);
        setorigin(p, mid - dir * (0.5 * max(t.size_x, t.size_y) + 40) - 0.5 * (p.mins + p.maxs));
        p.velocity = dir * speed;
        dprint(sprintf("walltorch test: %s sent at it along %v at %g m/s\n", p.classname, dir,
            cvar("vr_test_fling_speed")));
        return;
    }
    VR_Test_Fire(kind, mid);
};
