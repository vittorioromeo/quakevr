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
// A lit torch's blow (and a thrown lit torch) sets what it hits burning: vr_walltorch_burn damage a second for
// vr_walltorch_burn_time seconds, a small flame on it (flame2.mdl, which lights it), burns painted where the flame is
// (the wounds' QVR_WOUND_BURN).

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
.entity wt_burner;      // on a monster or a player: the fire a torch lit on it
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

// Whether `xPlayer`'s hand `xHand` may take hold of torch `t` on its wall now (its grip just pressed, nothing in it).
float(entity t, entity xPlayer, float xHand) VR_WallTorch_CanGrip =
{
    return t.wt_state == 0 && t.wt_gripper == world && cvar_hget(cvarh_vr_carry) &&
           VR_Carry_GripPressed(xPlayer, xHand) && VRIsHandEmpty(xPlayer, xHand) &&
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
// vrfiringrange_2026-09-30_11-22-44: torches were hard to take by hand). `vr_wt_near_dist`: how far it was (cm).
float vr_wt_near_dist;
entity(float xHand) VR_WallTorch_NearHand =
{
    float reach = max(0, cvar_hget(cvarh_vr_walltorch_reach)) * VRMetersToUnits() / 100;
    if(reach <= 0)
    {
        return world;
    }
    vector hand = VRGetHandPos(xHand);
    entity best = world;
    float bestD = reach;
    entity t = findradius(hand, reach + 20);
    while(t != world)
    {
        if(t.classname == "light_torch_small_walltorch" && t.wt_state == 0)
        {
            // Its stick upright: the butt 14 under its origin, the flame's foot 3 over it.
            vector p = t.origin;
            p_z = min(t.origin_z + 3, max(t.origin_z - 14, hand_z));
            float d = vlen(hand - p);
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
        // Not quite on one: a grip near a torch takes it (vr_walltorch_reach).
        if(self.health > 0 && VR_Carry_GripPressed(self, xHand) && VRIsHandEmpty(self, xHand))
        {
            t = VR_WallTorch_NearHand(xHand);
            if(t != world && VR_WallTorch_CanGrip(t, self, xHand))
            {
                VR_WallTorch_Grip(t, self, xHand, sprintf("near, %.1f cm", vr_wt_near_dist));
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
// Burning what it hits

// What fire doesn't hurt (VR_Liquid_Immune's: what lives in lava or water, the bosses), but the training dummy burns.
float(entity e) VR_WallTorch_Fireproof =
{
    return e.classname != "vr_dummy" && VR_Liquid_Immune(e);
};

// The fire a torch lit on a monster or a player: a small flame on it, moving about its upper body, hurting it every
// half second (burns painted where the flame is: VR_Wound_Hit, "vr_torch_burn"); out when its time is up, when it dies,
// or in water.
void() VR_WallTorch_BurnThink =
{
    entity v = self.enemy;
    if(v == world || v.wt_burner != self || v.health <= 0 || time >= self.wt_last || !v.takedamage ||
       VR_Wound_Level(v) >= 2)
    {
        if(v.wt_burner == self)
        {
            v.wt_burner = world;
        }
        dprint(sprintf("walltorch: %s stops burning\n", v.classname));
        remove(self);
        return;
    }
    self.nextthink = time + 0.05;
    // Its place on the body: the upper half, wandering slowly round it.
    float a = time * 1.7 + self.wt_hits;
    vector half = (v.maxs - v.mins) * 0.5;
    vector at = (v.absmin + v.absmax) * 0.5;
    at_x = at_x + cos(a) * half_x * 0.45;
    at_y = at_y + sin(a) * half_y * 0.45;
    at_z = at_z + half_z * (0.1 + 0.3 * (0.5 + 0.5 * sin(a * 0.63)));
    setorigin(self, at);
    if(time < self.wt_out_time)
    {
        return;
    }
    self.wt_out_time = time + 0.5;
    float dmg = self.wt_fire * 0.5;
    particle2(at, '0 0 40', QVR_PARTICLE_PRESET_LAVASPIKE, 4); // embers
    particle2(at, '0 0 20', QVR_PARTICLE_PRESET_SMOKE, 2);
    VR_SetHitKind(QVR_HITKIND_NONE, -1, at, '0 0 -1');
    T_Damage(v, self, self.owner, dmg); // (vr_debug_shots logs it: "by vr_torch_burn")
    VR_ClearHitKind();
};

void(entity xVictim, entity xAttacker, vector xAt, vector xDir) VR_WallTorch_Ignite =
{
    float dps = cvar_hget(cvarh_vr_walltorch_burn);
    float len = cvar_hget(cvarh_vr_walltorch_burn_time);
    if(!(xVictim.flags & (FL_MONSTER | FL_CLIENT)) || xVictim.health <= 0 || dps <= 0 || len <= 0 ||
       VR_WallTorch_Fireproof(xVictim) || xVictim == xAttacker)
    {
        return;
    }
    if(xDir == '0 0 0')
    {
        xDir = '0 0 -1';
    }
    woundevent(xVictim, xAt, normalize(xDir), QVR_WOUND_BURN, 14, 0); // scorched where the head struck
    entity f = xVictim.wt_burner;
    if(f == world || f.classname != "vr_torch_burn" || f.enemy != xVictim)
    {
        f = spawn();
        f.classname = "vr_torch_burn";
        f.enemy = xVictim;
        f.movetype = MOVETYPE_NONE;
        f.solid = SOLID_NOT;
        setmodel(f, "progs/flame2.mdl"); // id's small flame (it flickers its own light: vr_emissive.cpp)
        f.frame = 0;
        f.scale = 0.6;
        f.wt_hits = random() * 6; // (where on the body it starts)
        f.wt_out_time = time + 0.5;
        f.think = VR_WallTorch_BurnThink;
        f.nextthink = time;
        setorigin(f, (xVictim.absmin + xVictim.absmax) * 0.5);
        xVictim.wt_burner = f;
        sound(xVictim, CHAN_AUTO, "vr/torch_light.wav", 0.6, ATTN_NORM);
    }
    f.owner = xAttacker;
    f.wt_fire = dps;
    f.wt_last = time + len;
    dprint(sprintf("walltorch: %s burns for %g s at %g a second\n", xVictim.classname, len, dps));
};

// Thrown lit into a monster, it sets it burning too (and hurts it as a thrown box does: forcegrabbable_touch).
void() VR_WallTorch_Touch =
{
    if(self.wt_state == 1 && self.wt_fire > 0 && self.throwhit == QVR_THROWHIT_NEVER_HIT && other.takedamage &&
       other != self.owner)
    {
        VR_WallTorch_Ignite(other, self.owner, self.origin, self.velocity);
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
        vr_melee_blunt = TRUE; // a wooden knock (VR_Melee_HitSound's)
        sound(self, VRGetGunChannel(xHand), VR_Melee_HitSound("fisthit.wav"), 1.0, ATTN_NORM);
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
        if(T_Damage_VRMelee(trace_ent, self, self, xdmg))
        {
            SpawnBloodSplash(org, xdmg);
        }
        if(lit)
        {
            VR_WallTorch_Ignite(trace_ent, self, org, v_forward);
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
    precache_sound("ambience/fire1.wav");
    precache_sound("misc/null.wav");
    self.wt_state = 0;
    self.wt_home = self.origin;
    self.wt_fire = 1;
    self.solid = SOLID_NOT_BUT_TOUCHABLE;
    self.movetype = MOVETYPE_NONE;
    self.flags = self.flags | FL_FORCEGRABBABLE; // (the hand takes it by its drawn shape: vr_physics.cpp handOn)
    self.handtouch = VR_WallTorch_Handtouch;
    setsize(self, '-3 -3 -14', '3 3 3'); // its stick and cup (not its flame)
    setorigin(self, self.origin);
    VR_WallTorch_Show(self);
};
