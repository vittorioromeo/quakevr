// ----------------------------------------------------------------------------
// Flat-screen play's quick grenade (-flatscreenmode; docs/vr-port/FLATPLAY.md, "The quick grenade"; his words 2026-11-10)
// ----------------------------------------------------------------------------
//
// "The 'G' key should serve as a quick grenade key. If the player is not dual wielding, any free hand will be used to
// throw a grenade like you can already do in VR from the back pouch. Priority: any completely empty hand (dominant first)
// -> off-hand holding a weapon's grip hotspot. Pressing 'G' will simply equip a grenade in that hand, by first animating
// the chosen hand to go back to a relaxed position (simulating grabbing the grenade from the pouch). Then the hand will
// come back into view with a held deactivated grenade. Pressing the fire button for the hand holding the grenade will
// first trigger its countdown (activate) it. After that, holding the fire button will prime a throw by putting the hand
// in an overhead throw pose, and releasing it will throw the grenade."
//
// G (vr_flat_key_grenade: impulse 57, vr_flatgrenade.cpp) picks the hand (VR_FlatGrenade_Choose) and starts its trip to
// the pouch (vr_flat_grenade_fetch_time); as it gets there the hand takes the back pouch's grenade, the headset's own
// (VR_HandGrenade_Take, vr_grenade.qc: its ammo, a rocket, leaves with it; the proximity launcher's or the multi-grenade
// as the pouch gives them), carried in the hand unarmed. The hand's fire button (LMB the main hand, RMB the off hand while
// a grenade is held: flatplay::routeFire) arms it (VR_HandGrenade_Arm: the pin, the fuse, vr_grenade_fuse; the
// headset's own countdown and cues); then, held, winds up the throw (vr_flat_grenade_arm_mode 1: the next press; 0: the
// arming press itself), and let go, throws it along the crosshair vr_flat_grenade_throw_up (plus the arc's
// vr_flat_grenade_throw_pitch) above it, as fast as the wind-up was long (vr_flat_grenade_speed_min .. _max over
// vr_flat_grenade_wind_full, times vr_flat_grenade_speed_scale), with his own motion
// (VR_Throw_Inherit): the carry's own throw (VR_Carry_Throw) and the grenade's release (VR_Grenade_Released: the player's).
// G again while it is unarmed puts it back in the pouch (the rocket back). Gone from the hand (it went off, the drop key let
// go of it), it is forgotten. The hand then goes back to its pose: an off hand to the gun's grip (vr_flatsight.cpp).
//
// The client draws it from STAT_QVR_FLATGREN (.vr_flatgren: the phase + 8 for the main hand; vr_flatgrenade.cpp).
// developer 1 prints each step ("flat grenade: ...").

.float vr_flatgren;        // STAT_QVR_FLATGREN: the phase (VR_FLATGREN_*) + 8 when it is the main hand's
.float vr_flatgren_phase;  // VR_FLATGREN_*
.float vr_flatgren_hand;   // the hand
.float vr_flatgren_at;     // when the phase began
.entity vr_flatgren_ent;   // the grenade in the hand (from VR_FLATGREN_HELD)
.float vr_flatgren_btn;    // the hand's fire button last frame

float VR_FLATGREN_NONE = 0;
float VR_FLATGREN_FETCH = 1;   // the hand on its way to the pouch
float VR_FLATGREN_HELD = 2;    // the grenade in the hand, unarmed
float VR_FLATGREN_ARMED = 3;   // armed (its fuse runs)
float VR_FLATGREN_WIND = 4;    // the fire button held: the overhead wind-up
float VR_FLATGREN_THROWN = 5;  // thrown: the follow-through (vr_flat_throw_follow_time: the throw's motion, shared)

.float vr_flatgren_bykey;  // the wind-up is the throw key's (J held: impulse 39), not the fire button's: J's release throws

// The release (flatthrow3, his words 2026-11-26: "the arm/hand should travel forward for a split second with the
// object/weapon still attached, and then release it"): let go of the key (or the grenade's fire button), the follow-through
// begins with the thing still in the hand, carried on with it vr_flat_throw_release_time (0: at once), and it leaves there,
// from the hand where it then is (VR_FlatThrow_From), as hard as it was wound up. The quick grenade's and the throw key's.
.float vr_flatgren_launch;  // (VR_FLATGREN_THROWN) when its grenade leaves the hand; 0 thrown already
.float vr_flatgren_held;    // how long it was wound up (its throw's strength)
.float vr_flatthrow_launch; // (VR_FLATTHROW_THROWN) when what it holds leaves the hand; 0 thrown already
.float vr_flatthrow_held;   // how long it was wound up

// The release's delay (s): vr_flat_throw_release_time.
float() VR_FlatThrow_ReleaseTime =
{
    return min(0.5, max(0, cvar_hget(cvarh_vr_flat_throw_release_time)));
};

// The follow-through's length (s): vr_flat_throw_follow_time, and never over before the thing has left the hand.
float() VR_FlatThrow_FollowTime =
{
    return max(max(0.02, cvar_hget(cvarh_vr_flat_throw_follow_time)), VR_FlatThrow_ReleaseTime() + 0.02);
};

// The throw key's state (below, "The throw key"; in the same stat: VR_FlatThrow_Bits).
.float vr_flatthrow_phase;    // VR_FLATTHROW_*
.float vr_flatthrow_hand;     // the hand that throws
.float vr_flatthrow_at;       // when the phase began
.float vr_flatthrow_press_at; // when B went down (tap < vr_flat_throw_tap_time drops; hold winds up and throws)
.float vr_flatthrow_both;     // held in both hands as the wind-up began (a weapon with the other hand on its grip, a crate, a ragdoll)
.entity vr_flatthrow_ent;     // what it drops/throws (a carried thing or ragdoll; world: weapon or embedded blade)
.float vr_flatthrow_wid;      // the hand's weapon as B went down (0: not a weapon)
.float vr_flatthrow_kind;     // 1 weapon, 2 carried prop/crate/gib, 3 grabbed ragdoll, 4 embedded blade, 5 quick grenade
float VR_FLATTHROW_WIND = 1;    // held >= tap time: winding up behind the shoulder, charging
float VR_FLATTHROW_THROWN = 2;  // let go after wind-up: the follow-through (vr_flat_throw_follow_time)
float VR_FLATTHROW_PENDING = 3; // held < tap time: waiting for quick-tap drop or hold wind-up (no wind-up shown)

float() VR_FlatGrab_Bits;                        // (vr_flatgrab.qc)
void(entity e) VR_FlatGrab_NoteThrown;           // (vr_flatgrab.qc)
void(entity xRag, vector xVel) VR_FlatGrab_ReleaseHeld;    // (vr_flatgrab.qc)
entity(entity p, float xHand) VR_FlatGrab_Held;            // (vr_flatgrab.qc)

// STAT_QVR_FLATGREN's throw part: the phase * 16, + 64 the main hand's, + 128 both hands (vr_flatgrenade.cpp).
float() VR_FlatThrow_Bits =
{
    if(!self.vr_flatthrow_phase)
    {
        return 0;
    }
    return self.vr_flatthrow_phase * 16 + (self.vr_flatthrow_hand == cVR_MainHand ? 64 : 0) + (self.vr_flatthrow_both ? 128 : 0);
};

float vr_flatgren_thrown;      // (the tests) throws, and the last one's speed and wind-up
float vr_flatgren_lastspeed;
float vr_flatgren_lastheld;
entity vr_flatgren_last;       // (the tests) the last grenade thrown
entity vr_flatthrow_last;      // (the tests) the last thing the throw key or the quick grenade threw, where it left, and
vector vr_flatthrow_from;      // until when it is watched for where it first meets something and "landed" (developer 1)
float vr_flatthrow_watch;
entity vr_flatthrow_hitwho;    // (VR_FlatThrow_WatchHit) the one it watches; world at each throw (a freed grenade's slot is reused)

string(float xHand) VR_FlatGrenade_HandName =
{
    return xHand == cVR_MainHand ? "main" : "off";
};

void(float xPhase) VR_FlatGrenade_Set =
{
    self.vr_flatgren_phase = xPhase;
    self.vr_flatgren_at = time;
    self.vr_flatgren_bykey = FALSE;
    if(xPhase != VR_FLATGREN_THROWN)
    {
        self.vr_flatgren_launch = 0;
    }
    self.vr_flatgren = (xPhase ? xPhase + (self.vr_flatgren_hand == cVR_MainHand ? 8 : 0) : 0) + VR_FlatThrow_Bits() +
                       VR_FlatGrab_Bits();
};

void(string xWhy) VR_FlatGrenade_Cancel =
{
    if(self.vr_flatgren_phase)
    {
        dprint(sprintf("flat grenade: %s hand: over (%s)\n", VR_FlatGrenade_HandName(self.vr_flatgren_hand), xWhy));
    }
    self.vr_flatgren_ent = world;
    VR_FlatGrenade_Set(VR_FLATGREN_NONE);
};

// Whether `xHand` of self holds a weapon's grip: two-handed aiming (the client's bit), the hand without a weapon and the
// other with one, carrying nothing.
float(float xHand) VR_FlatGrenade_OnGrip =
{
    return VRGet2HAiming(self) && VRIsHandWeaponless(self, xHand) && !VRIsHandWeaponless(self, VRGetOtherHand(xHand)) &&
           VR_Carry_Held(self, xHand) == world;
};

// Whether `xHand` of self may go for a grenade: empty (VR_HandGrenade_PouchHandEmpty: nothing in it, not busy, not
// climbing), or on a weapon's grip; and the other hand's gun not on its reload trip.
float(float xHand, float xGrip) VR_FlatGrenade_HandFree =
{
    if(VR_FlatReload_Busy(self, VRGetOtherHand(xHand)) || VR_FlatReload_Busy(self, xHand))
    {
        return FALSE;
    }
    if(xGrip)
    {
        return VR_FlatGrenade_OnGrip(xHand) && VR_Hand_Free(self, xHand, VR_HANDFREE_GUNGRIP);
    }
    return !VR_FlatGrenade_OnGrip(xHand) && VR_HandGrenade_PouchHandEmpty(self, xHand);
};

// The hand that takes the grenade (-1: none): not dual wielding; a completely empty hand, the dominant (the main hand)
// first; else a hand on a weapon's grip (the off hand, as a rifle is held).
float() VR_FlatGrenade_Choose =
{
    if(!VRIsHandWeaponless(self, cVR_MainHand) && !VRIsHandWeaponless(self, cVR_OffHand))
    {
        return -1;
    }
    if(VR_FlatGrenade_HandFree(cVR_MainHand, FALSE)) { return cVR_MainHand; }
    if(VR_FlatGrenade_HandFree(cVR_OffHand, FALSE)) { return cVR_OffHand; }
    if(VR_FlatGrenade_HandFree(cVR_OffHand, TRUE)) { return cVR_OffHand; }
    if(VR_FlatGrenade_HandFree(cVR_MainHand, TRUE)) { return cVR_MainHand; }
    return -1;
};

// The unarmed grenade in the hand back in the pouch (G again): its rocket back, as VR_HandGrenade_LetGo puts it back.
void() VR_FlatGrenade_PutBack =
{
    entity g = self.vr_flatgren_ent;
    float h = self.vr_flatgren_hand;
    if(g == world || VR_Carry_Held(self, h) != g || !VR_HandGrenade_Unarmed(g) ||
       VR_HandGrenade_Ammo(self, g) + 1 > VR_HandGrenade_MaxAmmo(self, g))
    {
        return;
    }
    VR_Hand_Release(self, h, "stow");
    g.carry_player = world;
    VR_HandGrenade_AmmoBack(self, g);
    VR_HandGrenade_AmmoChanged(self);
    sound(self, VRGetHandChannel(h), "vr/grenade_pouch.wav", 0.6, ATTN_NORM);
    remove(g);
    VR_FlatGrenade_Cancel("put back in the pouch");
};

// G (impulse 57): a grenade into a free hand, or the one held unarmed back.
void() VR_FlatGrenade_Key =
{
    if(!VR_FlatPlay(self) || !cvar_hget(cvarh_vr_flat_grenade) || self.health <= 0 || !self.ishuman)
    {
        return;
    }
    float ph = self.vr_flatgren_phase;
    if(ph == VR_FLATGREN_HELD)
    {
        VR_FlatGrenade_PutBack();
        return;
    }
    if(ph == VR_FLATGREN_FETCH || ph == VR_FLATGREN_ARMED || ph == VR_FLATGREN_WIND || self.vr_flatgren_launch)
    {
        dprint(sprintf("flat grenade: G: the %s hand is busy with one (phase %g)\n", VR_FlatGrenade_HandName(self.vr_flatgren_hand), ph));
        return;
    }
    if(self.ammo_rockets < 1)
    {
        sprint(self, "No rockets for a grenade.\n");
        dprint("flat grenade: G: the pouch is empty (no rockets)\n");
        return;
    }
    float h = VR_FlatGrenade_Choose();
    if(h < 0)
    {
        dprint(sprintf("flat grenade: G: no free hand (main weapon %g, off weapon %g, two-handed %g)\n",
            VRGetEntWeapon(self, cVR_MainHand), VRGetEntWeapon(self, cVR_OffHand), VRGet2HAiming(self)));
        return;
    }
    self.vr_flatgren_hand = h;
    self.vr_flatgren_ent = world;
    VR_FlatGrenade_Set(VR_FLATGREN_FETCH);
    dprint(sprintf("flat grenade: G: the %s hand goes for the pouch%s\n", VR_FlatGrenade_HandName(h),
        VR_FlatGrenade_OnGrip(h) ? " (off the weapon's grip)" : ""));
};

// Where a flat throw (the throw key's, the quick grenade's) leaves and its way (his words 2026-11-23: thrown things
// "seem to come from the centre of the eyes rather than from the hand holding them"): from the throwing hand where it is
// drawn (the client's pose is the server's: the wound-up hand, in view at the screen's corner), or where the line to it
// from the eyes meets a wall; aimed at what the crosshair points at (up to VR_FLATTHROW_AIM_FAR away), so that it goes
// there from the hand, `xUp` degrees above that line. What the crosshair points at lying at the hand or behind it (a
// wall at the face): along the aim. Returns where it leaves; vr_flatthrow_dir its way, vr_flatthrow_ang its angles
// (makevectors' convention) and v_forward / v_right / v_up made from them.
float VR_FLATTHROW_AIM_FAR = 2048;
float VR_FLATTHROW_AIM_NEAR = 24;
vector vr_flatthrow_dir;
vector vr_flatthrow_ang;
vector(float xHand, float xUp) VR_FlatThrow_From =
{
    vector head = VRGetHeadPos(self);
    makevectors(self.v_angle);
    vector aim = v_forward;
    traceline(head, head + aim * VR_FLATTHROW_AIM_FAR, FALSE, self);
    vector at = trace_endpos;
    vector hand = VRGetEntHandPos(self, xHand);
    traceline(head, hand, TRUE, self);
    vector from = trace_fraction < 1 ? trace_endpos - normalize(hand - head) * 2 : hand;
    vector way = at - from;
    if(way * aim < VR_FLATTHROW_AIM_NEAR)
    {
        way = aim;
    }
    vector ang = vectoangles(way);
    ang_x = -ang_x - xUp; // (vectoangles' pitch is up positive, makevectors' down positive)
    vr_flatthrow_ang = ang;
    makevectors(ang);
    vr_flatthrow_dir = v_forward;
    return from;
};

// The quick grenade's throw `xHeld` seconds wound up: its speed (units/s) and how far above the crosshair it goes (degrees:
// Throw Above Aim, and the arc's pitch on top of it, the two added). VR_FlatGrenade_Throw's and VR_FlatThrow_Aim's.
float(float xHeld) VR_FlatGrenade_Speed =
{
    float full = min(5, max(0.1, cvar_hget(cvarh_vr_flat_grenade_wind_full)));
    float k = min(1, max(0, xHeld / full));
    float lo = max(0, cvar_hget(cvarh_vr_flat_grenade_speed_min));
    float hi = max(lo, cvar_hget(cvarh_vr_flat_grenade_speed_max));
    return (lo + (hi - lo) * k) * min(3, max(0.1, cvar_hget(cvarh_vr_flat_grenade_speed_scale)));
};

float() VR_FlatGrenade_Up =
{
    float up = min(60, max(-30, cvar_hget(cvarh_vr_flat_grenade_throw_up)));
    return min(80, up + min(60, max(-30, cvar_hget(cvarh_vr_flat_grenade_throw_pitch))));
};

// Thrown: let go of towards what the crosshair points at, `xHeld` seconds wound up.
void(float xHand, entity g, float xHeld) VR_FlatGrenade_Throw =
{
    float full = min(5, max(0.1, cvar_hget(cvarh_vr_flat_grenade_wind_full)));
    float k = min(1, max(0, xHeld / full));
    float hi = max(max(0, cvar_hget(cvarh_vr_flat_grenade_speed_min)), cvar_hget(cvarh_vr_flat_grenade_speed_max));
    float speed = VR_FlatGrenade_Speed(xHeld);
    float up = VR_FlatGrenade_Up();
    // It leaves from the wound-up hand, towards what the crosshair points at (VR_FlatThrow_From).
    vector start = VR_FlatThrow_From(xHand, up);
    vector dir = vr_flatthrow_dir;
    VR_Hand_Release(self, xHand, "throw");
    setorigin(g, start);
    VR_Carry_Loose(g);
    g.vr_spin = '0 0 0';
    VR_Carry_Throw(g, dir * speed); // (the carry's throw: its true-scale gravity; fast, a missile)
    g.velocity = g.velocity + self.velocity * VR_Throw_Inherit(self);
    VR_Grenade_Released(g, xHand); // (the player's now; armed already)
    vr_flatgren_thrown++;
    vr_flatgren_lastspeed = speed;
    vr_flatgren_lastheld = xHeld;
    vr_flatgren_last = g;
    vr_flatthrow_last = g; // (developer 1: where it first meets something, VR_FlatThrow_WatchHit, as the throw key's)
    vr_flatthrow_from = start;
    vr_flatthrow_watch = time + 4;
    vr_flatthrow_hitwho = world;
    vector launchAng = vectoangles(g.velocity);
    float launch = launchAng_x; // (up positive)
    if(launch > 180)
    {
        launch = launch - 360;
    }
    dprint(sprintf("flat grenade: thrown by the %s hand after %.2f s wound up: %.0f u/s (%.0f%% of the way to %.0f), %.2f s of fuse left\n",
        VR_FlatGrenade_HandName(xHand), xHeld, vlen(g.velocity), k * 100, hi, g.vr_gren_fuse - time));
    dprint(sprintf("flat grenade: launched %.1f deg above level, %.1f above the crosshair, %.1f u/s\n", launch, up, vlen(g.velocity)));
    makevectors(self.v_angle);
    vector gOff = start - VRGetHeadPos(self);
    dprint(sprintf("flat grenade: left the hand %.3f s after the release, %.1f forward of the eye\n",
        self.vr_flatgren_phase == VR_FLATGREN_THROWN ? time - self.vr_flatgren_at : 0, gOff * v_forward));
    self.vr_flatgren_ent = world;
    VR_FlatGrenade_Set(VR_FLATGREN_THROWN);
};

// Let go of (the fire button, or J's release), `xHeld` seconds wound up: the follow-through begins with the grenade still in
// the hand, and it leaves vr_flat_throw_release_time later (VR_FlatGrenade_Frame), from the hand there (0: at once).
void(float xHand, entity g, float xHeld) VR_FlatGrenade_LetGo =
{
    float rt = VR_FlatThrow_ReleaseTime();
    if(rt <= 0)
    {
        VR_FlatGrenade_Throw(xHand, g, xHeld);
        return;
    }
    VR_FlatGrenade_Set(VR_FLATGREN_THROWN);
    self.vr_flatgren_launch = time + rt;
    self.vr_flatgren_held = xHeld;
    dprint(sprintf("flat grenade: let go of after %.2f s wound up: carried on %.2f s into the follow-through\n", xHeld, rt));
};

// A grenade that came into a hand some other way (his words 2026-11-11: "Grabbing a grenade (live or non-live) via
// force-grabbing should put it in the same state as when 'G' is pressed, so that the player can throw it if live, or
// trigger + throw if not live"): a force grab's catch (E, impulses 50, 51), a hand's grab of one lying by, a catch in flight.
// The hand takes the quick grenade's state as after G's trip to the pouch: a live one (an ogre's, a hand grenade armed)
// ready to throw (VR_FLATGREN_ARMED: the fire button held winds up, let go throws), a dud (a hand grenade with its pin in)
// held (VR_FLATGREN_HELD: the fire button arms it first; G puts it in the pouch). The main hand first if both hold one.
void() VR_FlatGrenade_Adopt =
{
    if(!VR_FlatPlay(self) || !cvar_hget(cvarh_vr_flat_grenade) || self.health <= 0 || !self.ishuman)
    {
        return;
    }
    float h = cVR_MainHand;
    entity g = VR_Carry_Held(self, h);
    if(g == world || !g.vr_grenade)
    {
        h = cVR_OffHand;
        g = VR_Carry_Held(self, h);
    }
    if(g == world || !g.vr_grenade || VR_Carry_Held(self, VRGetOtherHand(h)) == g)
    {
        return; // (none; or held in both hands: the carry's two-handed throw)
    }
    self.vr_flatgren_hand = h;
    self.vr_flatgren_ent = g;
    self.vr_flatgren_btn = VRGetFireButtonPressed(h); // (a button already down arms nothing)
    VR_FlatGrenade_Set(VR_HandGrenade_Armed(g) ? VR_FLATGREN_ARMED : VR_FLATGREN_HELD);
    dprint(sprintf("flat grenade: the %s hand took a %s grenade (%s): as after G, %s\n", VR_FlatGrenade_HandName(h),
        VR_HandGrenade_Armed(g) ? "live" : "dud", g.classname, VR_HandGrenade_Armed(g) ? "the fire button throws it" :
        "the fire button arms it"));
};

// Whether the fire button of the hand holding `g` is the quick grenade's (VR_HandGrenade_Trigger: not the carry's take).
float(entity g) VR_FlatGrenade_Owns =
{
    return g != world && self.vr_flatgren_ent == g && self.vr_flatgren_phase >= VR_FLATGREN_HELD &&
           self.vr_flatgren_phase <= VR_FLATGREN_WIND;
};

// Each server frame (W_Frame, after the carry's hands): the trip's end, the fire button's arming, wind-up and throw; a
// grenade taken some other way adopted.
void() VR_FlatGrenade_Frame =
{
    float ph = self.vr_flatgren_phase;
    if(!ph)
    {
        VR_FlatGrenade_Adopt();
        return;
    }
    float h = self.vr_flatgren_hand;
    if(!VR_FlatPlay(self) || self.health <= 0 || self.vr_flatgren_at > time)
    {
        VR_FlatGrenade_Cancel(self.health <= 0 ? "dead" : "not flat-screen play");
        return;
    }
    float since = time - self.vr_flatgren_at;
    if(ph == VR_FLATGREN_FETCH)
    {
        if(since < max(0.05, cvar_hget(cvarh_vr_flat_grenade_fetch_time)) - 0.001)
        {
            return;
        }
        if(!VR_Hand_Free(self, h, VR_HANDFREE_GUNGRIP))
        {
            VR_FlatGrenade_Cancel("the hand took something else");
            return;
        }
        VR_HandGrenade_Take(h); // (the back pouch's: vr_grenade.qc; its rocket leaves the reserve)
        entity taken = VR_Carry_Held(self, h);
        if(taken == world || !taken.vr_hgren)
        {
            VR_FlatGrenade_Cancel("no grenade came out of the pouch");
            return;
        }
        self.vr_flatgren_ent = taken;
        self.vr_flatgren_btn = VRGetFireButtonPressed(h); // (a button already down arms nothing)
        VR_FlatGrenade_Set(VR_FLATGREN_HELD);
        dprint(sprintf("flat grenade: the %s hand has it (%.2f s), %g rockets left\n", VR_FlatGrenade_HandName(h), since,
            self.ammo_rockets));
        return;
    }
    if(ph == VR_FLATGREN_THROWN)
    {
        if(self.vr_flatgren_launch && time >= self.vr_flatgren_launch - 0.001) // (the release: carried on, it leaves now)
        {
            self.vr_flatgren_launch = 0;
            entity lg = self.vr_flatgren_ent;
            if(lg != world && VR_Carry_Held(self, h) == lg)
            {
                float began = self.vr_flatgren_at;
                VR_FlatGrenade_Throw(h, lg, self.vr_flatgren_held);
                self.vr_flatgren_at = began; // (the follow-through goes on from the key's release)
            }
            else
            {
                dprint(sprintf("flat grenade: the %s hand no longer holds its grenade at the release: nothing thrown\n", VR_FlatGrenade_HandName(h)));
                self.vr_flatgren_ent = world;
            }
            return;
        }
        if(self.vr_flatgren_launch)
        {
            return;
        }
        if(since >= VR_FlatThrow_FollowTime() - 0.001)
        {
            dprint(sprintf("flat grenade: done; the %s hand holds %s (kind %g)\n", VR_FlatGrenade_HandName(h),
                VR_Carry_Held(self, h) == world ? "nothing" : VR_Carry_Held(self, h).classname, VR_Hand_Kind(self, h)));
            VR_FlatGrenade_Set(VR_FLATGREN_NONE);
        }
        return;
    }

    entity g = self.vr_flatgren_ent;
    if(g == world || VR_Carry_Held(self, h) != g)
    {
        VR_FlatGrenade_Cancel("the grenade left the hand");
        return;
    }
    float btn = VRGetFireButtonPressed(h);
    float pressed = btn && !self.vr_flatgren_btn;
    self.vr_flatgren_btn = btn;
    if(ph == VR_FLATGREN_HELD)
    {
        if(VR_HandGrenade_Armed(g))
        {
            VR_FlatGrenade_Set(VR_FLATGREN_ARMED); // (armed some other way)
        }
        else if(pressed)
        {
            VR_HandGrenade_Arm(g, h, "flat: the fire button");
            VR_FlatGrenade_Set(cvar_hget(cvarh_vr_flat_grenade_arm_mode) == 0 ? VR_FLATGREN_WIND : VR_FLATGREN_ARMED);
        }
        return;
    }
    if(ph == VR_FLATGREN_ARMED)
    {
        if(pressed)
        {
            VR_FlatGrenade_Set(VR_FLATGREN_WIND);
            dprint(sprintf("flat grenade: winding up, %.2f s of fuse left\n", g.vr_gren_fuse - time));
        }
        return;
    }
    if(ph == VR_FLATGREN_WIND && !btn && !self.vr_flatgren_bykey) // (the throw key's wind-up: its release throws)
    {
        VR_FlatGrenade_LetGo(h, g, since);
    }
};

// ----------------------------------------------------------------------------
// The throw key (his ask 2026-11-23: "a flat-screen throw, reusing and improving the grenade throw motions";
// docs/vr-port/FLATPLAY.md, "The throw key")
// ----------------------------------------------------------------------------
//
// J (vr_flat_key_throw; vr_flatgrenade.cpp sends impulse 39 as it goes down, 40 as it comes up, and 40 again while the
// server still winds up with the key up) throws what a hand holds along the crosshair: a weapon (an axe, a sword, a
// dagger, a gun), a carried thing (a rock, a prop, a crate, a gib, a head) or the quick grenade. Held, the hand winds up
// behind the shoulder (the drawn motion is the grenade's own, shared: vr_flat_throw_*); let go, it throws, as hard as it
// was held (vr_flat_throw_speed_min .. _max m/s of the hand over vr_flat_throw_charge_time): a tap is a quick toss.
// The throw is the headset's (one path, note_reuse.txt): the hand's throw estimate is set to the throw's (its velocity
// from the hand to what the crosshair points at, vr_flat_throw_up above it; its spin end over end, vr_flat_throw_spin;
// leaving from the wound-up hand, VR_FlatThrow_From; the hand turned vr_flat_throw_release_pitch against the throw) and the hand lets go as a headset's does
// (DropWeaponInHand, VR_Carry_Release): the weight rules (throwvelocity), the throw assist, the spin by mass, the damage, an
// axe's or a sword's sticking, a gib's burst, a grenade's owner are all theirs.
// Held in both hands (a long gun with the other hand on its grip, a crate; his call 2026-11-23, replacing flatthrow's
// two-handed throw): the throwing hand alone throws, one-handed. The other hand lets go as the wind-up begins (a crate's
// at once here, VR_Carry_EndTwoHands, and not taken again while it winds up; a gun's grip by the client, the throw's hand
// busy: vr_flatgrenade.cpp letsGo) and takes a gun's grip again after the throw, as after the other let-go moves; the
// headset's two-handed window (vr_throw_2h_window) is not counted for it.
// The quick grenade's hand: J winds up its own throw (VR_FlatGrenade_Throw), let go throws it as the fire button's release
// does (an unarmed one leaves unarmed: a dud, as a headset's).
// developer 1 prints each step ("flat throw: ...") and where the thing ended up ("flat throw: landed: ...").

float vr_flatthrow_ammo0;      // (the tests) his ammo and the hand's clip as the wind-up began (none is shot meanwhile)
float vr_flatthrow_ammo1;      // and as it is thrown
float(float xHand) VR_FlatThrow_Rounds =
{
    return self.ammo_shells + self.ammo_nails + self.ammo_rockets + self.ammo_cells + VR_FlatReload_Unloadable(xHand);
};
float vr_flatthrow_count;      // (the tests) throws, the last one's hand speed (m/s), wind-up and what it threw
float vr_flatthrow_lastspeed;
float vr_flatthrow_lastheld;  // (vr_flatthrow_last, _from, _watch: above, the quick grenade's throw sets them too)
float(entity e) VR_AxeStick_IsStuck; // (vr_axestick.qc)

void(float xPhase) VR_FlatThrow_Set =
{
    self.vr_flatthrow_phase = xPhase;
    self.vr_flatthrow_at = time;
    if(xPhase != VR_FLATTHROW_THROWN)
    {
        self.vr_flatthrow_launch = 0;
    }
    self.vr_flatgren = (self.vr_flatgren_phase ? self.vr_flatgren_phase + (self.vr_flatgren_hand == cVR_MainHand ? 8 : 0) : 0) +
                       VR_FlatThrow_Bits() + VR_FlatGrab_Bits();
};

// Whether self winds up `xBox` with the throw key (one-handed: vr_carry.qc's second hand doesn't take it meanwhile).
float(entity xBox) VR_FlatThrow_WindsUp =
{
    return self.vr_flatthrow_phase == VR_FLATTHROW_WIND && self.vr_flatthrow_ent == xBox;
};

// Whether `p`'s `xHand` winds up its weapon to throw it (his words 2026-11-23: no shooting the gun about to be thrown):
// weapons.qc doesn't fire it meanwhile (the client neither fires nor sights it: vr_flatgrenade.cpp throwsWith).
float(entity p, float xHand) VR_FlatThrow_WindsUpWeapon =
{
    return (p.vr_flatthrow_phase == VR_FLATTHROW_WIND || p.vr_flatthrow_launch) && p.vr_flatthrow_hand == xHand &&
           p.vr_flatthrow_kind == 1;
};

// What `xHand` of self can drop or throw: 1 weapon, 2 carried prop/crate/gib, 3 grabbed ragdoll, 4 embedded blade,
// 0 nothing (empty; a grenade is handled via vr_flatgren_phase; a gun on its reload trip).
float(float xHand) VR_FlatThrow_What =
{
    if(VR_FlatReload_Busy(self, xHand))
    {
        return 0;
    }
    if(!VRIsHandWeaponless(self, xHand))
    {
        return 1;
    }
    if(VR_Hand_Kind(self, xHand) == VR_HOLD_EMBED)
    {
        return 4;
    }
    if(VR_Hand_Kind(self, xHand) == VR_HOLD_DRAG)
    {
        return 3; // (a big prop dragged by its surface: dropped and thrown as a ragdoll held, vr_flatgrab.qc)
    }
    entity e = VR_Carry_Held(self, xHand);
    if(e != world && !e.vr_grenade)
    {
        return isragdoll(e) ? 3 : 2;
    }
    return 0;
};

// The hand that drops/throws (-1 none): a carried thing or ragdoll before an embedded blade before a weapon, off hand
// before main hand (the non-dominant hand drops first).
float() VR_FlatThrow_Choose =
{
    float wOff = VR_FlatThrow_What(cVR_OffHand);
    float wMain = VR_FlatThrow_What(cVR_MainHand);
    if(wOff == 2 || wOff == 3) { return cVR_OffHand; }
    if(wMain == 2 || wMain == 3) { return cVR_MainHand; }
    if(wOff == 4) { return cVR_OffHand; }
    if(wMain == 4) { return cVR_MainHand; }
    if(wOff == 1) { return cVR_OffHand; }
    if(wMain == 1) { return cVR_MainHand; }
    return -1;
};

// Transition from VR_FLATTHROW_PENDING to VR_FLATTHROW_WIND (or VR_FLATGREN_WIND) once B is held >= vr_flat_throw_tap_time.
void() VR_FlatThrow_BeginWind =
{
    float kind = self.vr_flatthrow_kind;
    if(kind == 5)
    {
        float gph = self.vr_flatgren_phase;
        if(gph != VR_FLATGREN_HELD && gph != VR_FLATGREN_ARMED)
        {
            VR_FlatThrow_Set(0);
            return;
        }
        self.vr_flatthrow_phase = 0;
        VR_FlatGrenade_Set(VR_FLATGREN_WIND);
        self.vr_flatgren_bykey = TRUE;
        dprint(sprintf("flat throw: B: the %s hand winds up its grenade (%s)\n", VR_FlatGrenade_HandName(self.vr_flatgren_hand),
            gph == VR_FLATGREN_ARMED ? "armed" : "unarmed: a dud"));
        return;
    }
    float h = self.vr_flatthrow_hand;
    float other = VRGetOtherHand(h);
    entity e = self.vr_flatthrow_ent;
    float both = FALSE;
    if(kind == 2)
    {
        if(e != world && e.carry_2h && VR_Carry_Held(self, other) == e)
        {
            both = TRUE;
            h = cVR_MainHand;
            VR_Carry_EndTwoHands(e); // (the off hand lets go now, the main hand winds up and throws it alone)
        }
    }
    else if(kind == 3)
    {
        both = (e != world && VR_FlatGrab_Held(self, other) == e);
        if(both)
        {
            h = cVR_MainHand;
        }
    }
    else if(kind == 1)
    {
        both = VR_FlatGrenade_OnGrip(other); // (the other hand on this weapon's grip)
    }
    self.vr_flatthrow_hand = h;
    self.vr_flatthrow_both = both;
    VR_FlatThrow_Set(VR_FLATTHROW_WIND);
    vr_flatthrow_refused = 0;
    vr_flatthrow_ammo0 = VR_FlatThrow_Rounds(h);
    dprint(sprintf("flat throw: B: the %s hand winds up %s%s (ammo %g)\n", VR_FlatGrenade_HandName(h),
        kind == 1 ? sprintf("weapon %g", self.vr_flatthrow_wid) : e.classname,
        both ? (kind == 3 ? " (held in both hands)" : " (held in both hands: the other hand lets go)") : "", vr_flatthrow_ammo0));
};

// B down (impulse 39): enters pending tap/hold window (quick tap < vr_flat_throw_tap_time drops; hold winds up).
void() VR_FlatThrow_Press =
{
    if(!VR_FlatPlay(self) || self.health <= 0 || !self.ishuman ||
       self.vr_flatthrow_phase == VR_FLATTHROW_WIND || self.vr_flatthrow_phase == VR_FLATTHROW_PENDING ||
       self.vr_flatthrow_launch || self.vr_flatgren_launch) // (the release: still in the hand, about to leave it)
    {
        return;
    }
    float gph = self.vr_flatgren_phase;
    if(gph == VR_FLATGREN_HELD || gph == VR_FLATGREN_ARMED)
    {
        self.vr_flatthrow_hand = self.vr_flatgren_hand;
        self.vr_flatthrow_ent = self.vr_flatgren_ent;
        self.vr_flatthrow_wid = 0;
        self.vr_flatthrow_kind = 5;
        self.vr_flatthrow_both = FALSE;
        self.vr_flatthrow_press_at = time;
        VR_FlatThrow_Set(VR_FLATTHROW_PENDING);
        if(cvar_hget(cvarh_vr_flat_throw_tap_time) <= 0)
        {
            VR_FlatThrow_BeginWind();
        }
        return;
    }
    if(gph)
    {
        dprint(sprintf("flat throw: B: the %s hand is busy with a grenade (phase %g)\n", VR_FlatGrenade_HandName(self.vr_flatgren_hand), gph));
        return;
    }
    float h = VR_FlatThrow_Choose();
    if(h < 0)
    {
        dprint("flat throw: B: nothing in either hand to drop or throw\n");
        return;
    }
    float kind = VR_FlatThrow_What(h);
    entity e = (kind == 2 || kind == 3) ? VR_FlatGrab_Held(self, h) : world;
    self.vr_flatthrow_hand = h;
    self.vr_flatthrow_ent = e;
    self.vr_flatthrow_wid = kind == 1 ? VRGetEntWeapon(self, h) : 0;
    self.vr_flatthrow_kind = kind;
    self.vr_flatthrow_both = FALSE;
    self.vr_flatthrow_press_at = time;
    VR_FlatThrow_Set(VR_FLATTHROW_PENDING);
    if(kind != 4 && cvar_hget(cvarh_vr_flat_throw_tap_time) <= 0)
    {
        VR_FlatThrow_BeginWind();
    }
};

// The hand's throw estimate (the headset's: VRGetHandThrowVel etc.) set to `xVel` (m/s), no flick, spin `xAVel` (rad/s),
// left from `xFrom` just now, the hand turned `xRot`.
void(float xHand, vector xVel, vector xAVel, vector xFrom, vector xRot) VR_FlatThrow_SetHand =
{
    if(xHand == cVR_OffHand)
    {
        self.offhandthrowvel = xVel; self.offhandthrowflick = '0 0 0'; self.offhandavel = xAVel;
        self.offhandthrowpos = xFrom; self.offhandthrowage = 0; self.offhandrot = xRot;
        return;
    }
    self.handthrowvel = xVel; self.handthrowflick = '0 0 0'; self.handavel = xAVel;
    self.handthrowpos = xFrom; self.handthrowage = 0; self.handrot = xRot;
};

float VR_FLATTHROW_SPIN_LEAST = 0.4; // a timed blade's spin: at least this times its own (vr_flat_throw_spin_time)
float VR_FLATTHROW_SPIN_MOST = 2.5;  // and at most this times it (and vr_throw_spin_max)

// Quick tap on B (< vr_flat_throw_tap_time, or holding an embedded blade): drops held weapon/object with a gentle short
// toss (vr_flat_throw_drop_speed) or lets go of a stuck/embedded blade (leaving it in the body).
void() VR_FlatThrow_Drop =
{
    float h = self.vr_flatthrow_hand;
    float kind = self.vr_flatthrow_kind;
    float held = time - self.vr_flatthrow_press_at;
    float dropSpeed = max(0, cvar_hget(cvarh_vr_flat_throw_drop_speed));

    if(kind == 4)
    {
        VR_Forcegrab_FlatDropHand(h);
        VR_FlatThrow_Set(0);
        return;
    }

    if(kind == 3)
    {
        entity rag = self.vr_flatthrow_ent;
        if(rag != world && (VR_FlatGrab_Held(self, cVR_OffHand) == rag || VR_FlatGrab_Held(self, cVR_MainHand) == rag))
        {
            makevectors(self.v_angle);
            vector dvel = v_forward * (dropSpeed * 0.5 * VRMetersToUnits());
            VR_FlatGrab_ReleaseHeld(rag, dvel);
            VR_FlatGrab_NoteThrown(rag);
            dprint(sprintf("flat drop: ragdoll %s dropped by the %s hand after %.2f s\n", rag.classname, VR_FlatGrenade_HandName(h), held));
        }
        VR_FlatThrow_Set(0);
        return;
    }

    vector from = VR_FlatThrow_From(h, 0);
    vector dir = vr_flatthrow_dir;
    vector rot = VRGetHandRot(h);

    vector mV = self.handthrowvel, mF = self.handthrowflick, mA = self.handavel, mP = self.handthrowpos, mR = self.handrot;
    float mG = self.handthrowage;
    vector oV = self.offhandthrowvel, oF = self.offhandthrowflick, oA = self.offhandavel, oP = self.offhandthrowpos, oR = self.offhandrot;
    float oG = self.offhandthrowage;
    // A melee weapon held in inverted grip keeps its current held hand orientation on drop too.
    float revHeld = (kind == 1 && !VRIsHandWeaponless(self, h) && VRGetEntWeapon(self, h) == self.vr_flatthrow_wid &&
                     (VRGetEntWeaponFlags(self, h) & QVR_WPNFLAG_REVGRIP) != 0);
    VR_FlatThrow_SetHand(h, dir * dropSpeed, '0 0 0', from, revHeld ? (h == cVR_MainHand ? mR : oR) : rot);

    entity dropped = world;
    if(kind == 5)
    {
        entity g = self.vr_flatgren_ent;
        if(g != world && VR_Carry_Held(self, h) == g)
        {
            VR_Carry_Release(h, g);
            dropped = g;
        }
        VR_FlatGrenade_Cancel("dropped");
    }
    else if(kind == 2)
    {
        entity e = self.vr_flatthrow_ent;
        if(e != world && VR_Carry_Held(self, h) == e)
        {
            if(e.carry_2h)
            {
                VR_Carry_EndTwoHands(e);
            }
            float was2h = e.carry_2h_end;
            e.carry_2h_end = 0;
            VR_Carry_Release(h, e);
            e.carry_2h_end = was2h;
            dropped = e;
        }
    }
    else if(kind == 1 && !VRIsHandWeaponless(self, h) && VRGetEntWeapon(self, h) == self.vr_flatthrow_wid)
    {
        float last2h = self.last_2h_time;
        self.last_2h_time = 0;
        vr_wpn_dropped = world;
        DropWeaponInHand(self, h, VRGetEntWeapon(self, h), VRGetEntWeaponFlags(self, h));
        VRHandStopFiring(self, h);
        dropped = vr_wpn_dropped;
        self.last_2h_time = last2h;
    }

    self.handthrowvel = mV; self.handthrowflick = mF; self.handavel = mA; self.handthrowpos = mP; self.handrot = mR;
    self.handthrowage = mG;
    self.offhandthrowvel = oV; self.offhandthrowflick = oF; self.offhandavel = oA; self.offhandthrowpos = oP; self.offhandrot = oR;
    self.offhandthrowage = oG;

    VR_FlatThrow_Set(0);
    if(dropped != world)
    {
        VR_FlatGrab_NoteThrown(dropped);
        vr_flatthrow_last = dropped;
        vr_flatthrow_from = dropped.origin;
        vr_flatthrow_watch = time + 4;
        vr_flatthrow_hitwho = world;
        dprint(sprintf("flat drop: %s dropped by the %s hand after %.2f s: the hand at %.1f m/s, it leaves at %.0f u/s\n",
            dropped.classname, VR_FlatGrenade_HandName(h), held, dropSpeed, vlen(dropped.velocity)));
    }
};

// The throw key's throw `xHeld` seconds wound up: the hand's speed (m/s, as a headset's estimate; the weight rules make
// the rest), and how far above the crosshair it goes (degrees, vr_flat_throw_up). VR_FlatThrow_Throw's and
// VR_FlatThrow_Aim's.
float(float xHeld) VR_FlatThrow_Speed =
{
    float full = min(5, max(0.05, cvar_hget(cvarh_vr_flat_throw_charge_time)));
    float k = min(1, max(0, xHeld / full));
    float lo = max(0, cvar_hget(cvarh_vr_flat_throw_speed_min));
    float hi = max(lo, cvar_hget(cvarh_vr_flat_throw_speed_max));
    return lo + (hi - lo) * k;
};

float() VR_FlatThrow_Up =
{
    return min(60, max(-30, cvar_hget(cvarh_vr_flat_throw_up)));
};

// A body (`e`) thrown at `xSpeed` m/s of the hand along `xDir`: its launch (units/s), two-handed strength held in both
// hands (vr_flat_grab_ragdoll_throw), with his own motion. VR_FlatThrow_Throw's and VR_FlatThrow_Aim's.
vector(entity e, vector xDir, float xSpeed) VR_FlatThrow_RagVel =
{
    float bothRag = VR_FlatGrab_Held(self, cVR_OffHand) == e && VR_FlatGrab_Held(self, cVR_MainHand) == e;
    float ragScale = max(0, cvar_hget(cvarh_vr_flat_grab_ragdoll_throw));
    float handMult = bothRag ? 1.0 : 0.65;
    return xDir * (xSpeed * VRMetersToUnits() * ragScale * handMult) + self.velocity * VR_Throw_Inherit(self);
};

// Let go after wind-up (impulse 40, `xHeld` seconds wound up): thrown along the crosshair by the headset's own throw.
void(float xHeld) VR_FlatThrow_Throw =
{
    float h = self.vr_flatthrow_hand;
    float kind = self.vr_flatthrow_kind;
    float afterKey = self.vr_flatthrow_phase == VR_FLATTHROW_THROWN ? time - self.vr_flatthrow_at : 0; // (the release's delay)
    float full = min(5, max(0.05, cvar_hget(cvarh_vr_flat_throw_charge_time)));
    float k = min(1, max(0, xHeld / full));
    float speed = VR_FlatThrow_Speed(xHeld);
    // It leaves from the wound-up hand, towards what the crosshair points at (VR_FlatThrow_From).
    vector from = VR_FlatThrow_From(h, VR_FlatThrow_Up());
    vector ang = vr_flatthrow_ang;
    vector dir = vr_flatthrow_dir;
    vector spin = -v_right * max(0, cvar_hget(cvarh_vr_flat_throw_spin)); // end over end, the top going forward and down
    vector rot = ang;
    rot_x = rot_x + min(90, max(-90, cvar_hget(cvarh_vr_flat_throw_release_pitch)));
    vector head = VRGetHeadPos(self);

    // (The estimate as it was, back after: the engine's own until the next move.)
    vector mV = self.handthrowvel, mF = self.handthrowflick, mA = self.handavel, mP = self.handthrowpos, mR = self.handrot;
    float mG = self.handthrowage;
    vector oV = self.offhandthrowvel, oF = self.offhandthrowflick, oA = self.offhandavel, oP = self.offhandthrowpos, oR = self.offhandrot;
    float oG = self.offhandthrowage;
    // A melee weapon held in inverted grip keeps its current held hand orientation rather than snapping to upright release_pitch.
    float revHeld = (kind == 1 && !VRIsHandWeaponless(self, h) && VRGetEntWeapon(self, h) == self.vr_flatthrow_wid &&
                     (VRGetEntWeaponFlags(self, h) & QVR_WPNFLAG_REVGRIP) != 0);
    VR_FlatThrow_SetHand(h, dir * speed, spin, from, revHeld ? (h == cVR_MainHand ? mR : oR) : rot);
    vr_flatthrow_ammo1 = VR_FlatThrow_Rounds(h);

    // One-handed for weapons/props (his call 2026-11-23), two-handed strength for ragdolls held in both hands.
    entity e = self.vr_flatthrow_ent;
    entity thrown = world;
    vector ragLaunchVel = '0 0 0';
    if(kind == 3 && e != world)
    {
        float hasOff = (VR_FlatGrab_Held(self, cVR_OffHand) == e);
        float hasMain = (VR_FlatGrab_Held(self, cVR_MainHand) == e);
        if(hasOff || hasMain)
        {
            ragLaunchVel = VR_FlatThrow_RagVel(e, dir, speed);
            VR_FlatGrab_ReleaseHeld(e, ragLaunchVel);
            thrown = e;
        }
    }
    else if(kind == 2 && e != world)
    {
        if(VR_Carry_Held(self, h) == e)
        {
            if(e.carry_2h)
            {
                VR_Carry_EndTwoHands(e); // (taken in both again since the wind-up began: let go of now)
            }
            float was2h = e.carry_2h_end;
            e.carry_2h_end = 0;
            VR_Carry_Release(h, e);
            e.carry_2h_end = was2h;
            thrown = e;
        }
    }
    else if(kind == 1 && !VRIsHandWeaponless(self, h) && VRGetEntWeapon(self, h) == self.vr_flatthrow_wid)
    {
        float last2h = self.last_2h_time;
        self.last_2h_time = 0;
        vr_wpn_dropped = world;
        DropWeaponInHand(self, h, VRGetEntWeapon(self, h), VRGetEntWeaponFlags(self, h));
        VRHandStopFiring(self, h);
        thrown = vr_wpn_dropped;
        self.last_2h_time = last2h;
    }

    self.handthrowvel = mV; self.handthrowflick = mF; self.handavel = mA; self.handthrowpos = mP; self.handrot = mR;
    self.handthrowage = mG;
    self.offhandthrowvel = oV; self.offhandthrowflick = oF; self.offhandavel = oA; self.offhandthrowpos = oP; self.offhandrot = oR;
    self.offhandthrowage = oG;

    VR_FlatThrow_Set(VR_FLATTHROW_THROWN);
    if(thrown == world)
    {
        dprint(sprintf("flat throw: the %s hand no longer holds what it wound up with: nothing thrown\n", VR_FlatGrenade_HandName(h)));
        return;
    }
    VR_FlatGrab_NoteThrown(thrown);
    // No wrist times a flat throw's spin: a blade's is timed so that its edge or point leads where its arc meets something
    // (bladespinto: the spin nearest its own, VR_FLATTHROW_SPIN_LEAST times it to vr_throw_spin_max), and it sticks there.
    float spun = vlen(thrown.vr_spin);
    if(thrown.classname == "thrown_weapon" && cvar_hget(cvarh_vr_flat_throw_spin_time) && spun > 0.1)
    {
        float most = min(VR_FLATTHROW_SPIN_MOST, max(1, cvar_hget(cvarh_vr_throw_spin_max) / spun));
        thrown.vr_spin = bladespinto(thrown, thrown.vr_spin, VR_FLATTHROW_SPIN_LEAST, most);
    }
    vr_flatthrow_count++;
    vr_flatthrow_lastspeed = speed;
    vr_flatthrow_lastheld = xHeld;
    vr_flatthrow_last = thrown;
    vr_flatthrow_from = thrown.origin;
    vr_flatthrow_watch = time + 4;
    vr_flatthrow_hitwho = world;
    float outSpeed = kind == 3 ? vlen(ragLaunchVel) : vlen(thrown.velocity);
    dprint(sprintf("flat throw: %s thrown by the %s hand%s after %.2f s: the hand at %.1f m/s (%.0f%% charged), ",
        thrown.classname, VR_FlatGrenade_HandName(h), self.vr_flatthrow_both ? (kind == 3 ? " (two-handed)" : " (one-handed: the other hand let go)") : "", xHeld, speed, k * 100));
    dprint(sprintf("it leaves at %.0f u/s, spin %.1f rad/s\n", outSpeed, vlen(thrown.vr_spin)));
    dprint(sprintf("flat throw:   from %v at %v (the aim %v, the eye %v)\n", thrown.origin, kind == 3 ? ragLaunchVel : thrown.velocity, self.v_angle, head));
    makevectors(self.v_angle);
    vector off = thrown.origin - head;
    dprint(sprintf("flat throw:   left %.1f forward, %.1f right, %.1f up of the eye\n", off * v_forward, off * v_right, off * v_up));
    vector handOff = thrown.origin - VRGetEntHandPos(self, h);
    dprint(sprintf("flat throw:   left the hand %.3f s after the key's release, %.1f units from it\n", afterKey, vlen(handOff)));
    dprint(sprintf("flat throw:   ammo %g at the wind-up, %g now; fire refused %g frames\n", vr_flatthrow_ammo0, vr_flatthrow_ammo1,
        vr_flatthrow_refused));
};

// B up (impulse 40): quick tap (< vr_flat_throw_tap_time) drops / lets go of embedded blade; held throws.
void() VR_FlatThrow_Release =
{
    if(self.vr_flatgren_bykey && self.vr_flatgren_phase == VR_FLATGREN_WIND)
    {
        entity g = self.vr_flatgren_ent;
        if(g != world && VR_Carry_Held(self, self.vr_flatgren_hand) == g)
        {
            VR_FlatGrenade_LetGo(self.vr_flatgren_hand, g, time - self.vr_flatgren_at);
        }
        return;
    }
    if(self.vr_flatthrow_phase == VR_FLATTHROW_PENDING)
    {
        VR_FlatThrow_Drop();
        return;
    }
    if(self.vr_flatthrow_phase == VR_FLATTHROW_WIND)
    {
        // The release (flatthrow3): carried on into the follow-through, thrown at its end (VR_FlatThrow_Frame).
        float held = time - self.vr_flatthrow_at;
        float rt = VR_FlatThrow_ReleaseTime();
        if(rt <= 0)
        {
            VR_FlatThrow_Throw(held);
            return;
        }
        VR_FlatThrow_Set(VR_FLATTHROW_THROWN);
        self.vr_flatthrow_launch = time + rt;
        self.vr_flatthrow_held = held;
        dprint(sprintf("flat throw: the %s hand let go of the key after %.2f s wound up: carried on %.2f s into the follow-through\n",
            VR_FlatGrenade_HandName(self.vr_flatthrow_hand), held, rt));
    }
};

// The throwing hand's attack button pressed while B is held (impulse 41; flatthrow2, his words 2026-11-26: "cancel a throw
// by pressing the throwing hand's corresponding attack button while the throw key is being pressed"): nothing thrown, the
// hand back to its hold, whatever it held (a weapon, a prop, a body, the quick grenade: held again as before B, armed or
// not). The client holds the button back till it is let go (vr_flatgrenade.cpp swallowedButtons): it fires nothing.
float vr_flatthrow_cancels; // (the tests) throws cancelled
void() VR_FlatThrow_Cancel =
{
    if(self.vr_flatgren_bykey && self.vr_flatgren_phase == VR_FLATGREN_WIND)
    {
        entity g = self.vr_flatgren_ent;
        float armed = g != world && VR_HandGrenade_Armed(g);
        VR_FlatGrenade_Set(armed ? VR_FLATGREN_ARMED : VR_FLATGREN_HELD);
        self.vr_flatgren_btn = TRUE; // (the button that cancelled is no fresh press: it neither arms nor winds up)
        vr_flatthrow_cancels = vr_flatthrow_cancels + 1;
        dprint(sprintf("flat throw: cancelled: the %s hand keeps its grenade (%s)\n", VR_FlatGrenade_HandName(self.vr_flatgren_hand),
            armed ? "armed" : "unarmed"));
        return;
    }
    float ph = self.vr_flatthrow_phase;
    if(ph != VR_FLATTHROW_WIND && ph != VR_FLATTHROW_PENDING)
    {
        return;
    }
    float h = self.vr_flatthrow_hand;
    float kind = self.vr_flatthrow_kind;
    vr_flatthrow_cancels = vr_flatthrow_cancels + 1;
    dprint(sprintf("flat throw: cancelled: the %s hand keeps %s (ammo %g)\n", VR_FlatGrenade_HandName(h),
        kind == 1 ? sprintf("weapon %g", self.vr_flatthrow_wid) : (kind == 5 ? "its grenade" : (self.vr_flatthrow_ent != world ?
        self.vr_flatthrow_ent.classname : "what it holds")), VR_FlatThrow_Rounds(h)));
    VR_FlatThrow_Set(0);
};

// Whether `xHand` of `p` is the free hand while the other winds up a throw, the throw key's or the quick grenade's (its
// guiding pose: vr_flatgrenade.cpp, vr_flat_throw_guide; flatthrow2, his words 2026-11-26: it caught an ogre's grenade
// thrown at him): it catches nothing meanwhile (VR_Grenade_HandCatches, VR_Carry_Handtouch).
float(entity p, float xHand) VR_FlatThrow_Guides =
{
    if(!VR_FlatPlay(p))
    {
        return FALSE;
    }
    if(p.vr_flatthrow_phase == VR_FLATTHROW_WIND)
    {
        return xHand != p.vr_flatthrow_hand;
    }
    return p.vr_flatgren_phase == VR_FLATGREN_WIND && xHand != p.vr_flatgren_hand;
};

// Whether the thing being dropped/thrown is still held in the expected hand(s).
float() VR_FlatThrow_StillHeld =
{
    float h = self.vr_flatthrow_hand;
    float kind = self.vr_flatthrow_kind;
    if(kind == 5)
    {
        return (self.vr_flatgren_phase == VR_FLATGREN_HELD || self.vr_flatgren_phase == VR_FLATGREN_ARMED) &&
               self.vr_flatgren_ent != world && VR_Carry_Held(self, self.vr_flatgren_hand) == self.vr_flatgren_ent;
    }
    if(kind == 4)
    {
        return VR_Hand_Kind(self, h) == VR_HOLD_EMBED;
    }
    if(kind == 3)
    {
        return self.vr_flatthrow_ent != world &&
               (VR_FlatGrab_Held(self, cVR_OffHand) == self.vr_flatthrow_ent || VR_FlatGrab_Held(self, cVR_MainHand) == self.vr_flatthrow_ent);
    }
    if(kind == 2)
    {
        return self.vr_flatthrow_ent != world && VR_Carry_Held(self, h) == self.vr_flatthrow_ent;
    }
    return !VRIsHandWeaponless(self, h) && VRGetEntWeapon(self, h) == self.vr_flatthrow_wid;
};

// Where a throw from `xFrom` at `xVel` (units/s), falling at `xGravity` (units/s^2), first meets something: its arc
// stepped VR_THROWARC_STEP seconds at a time (up to VR_THROWARC_MOST), a traceline each step (monsters too), past
// `xSelf` (the thrower) and `xHeld` (the thing itself, still in the hand). Nothing within: where it is then.
float VR_THROWARC_STEP = 0.02;
float VR_THROWARC_MOST = 4;
vector(vector xFrom, vector xVel, float xGravity, entity xSelf, entity xHeld) VR_Throw_ArcEnd =
{
    vector p = xFrom;
    vector v = xVel;
    float t = 0;
    float dt = VR_THROWARC_STEP;
    while(t < VR_THROWARC_MOST)
    {
        vector q = p + v * dt - '0 0 1' * (0.5 * xGravity * dt * dt);
        traceline(p, q, FALSE, xSelf);
        if(trace_ent != world && trace_ent == xHeld)
        {
            traceline(trace_endpos, q, FALSE, xHeld); // (the thing in the hand itself: on past it)
        }
        if(trace_fraction < 1)
        {
            return trace_endpos;
        }
        p = q;
        v_z = v_z - xGravity * dt;
        t = t + dt;
    }
    return p;
};

// Item 1 of his flat notes 2026-11-26 ("Dynamic crosshair while in the 'about to throw' pose should aim towards the
// throwing location, not the weapon's muzzle target or melee's target"), flatxhair2: while a hand winds up a throw (the
// throw key's: a weapon, a carried thing, a body; the quick grenade's, by the key or its fire button), where letting go now would land it,
// sent to the view each frame (flatthrowaim: the visor crosshair, flatxhair). Not worked out apart: the release's own
// pieces (VR_FlatThrow_Speed / _Up / _From, VR_Throw_WeaponVel, VR_Carry_ThrowVel, VR_FlatThrow_RagVel,
// VR_FlatGrenade_Speed / _Up, the throw assist, his own motion, the throw's gravity), its arc traced (VR_Throw_ArcEnd).
// The spin does not move where it goes (it turns the thing about its middle: a blade's sticking, not its way).
void() VR_FlatThrow_Aim =
{
    float h = -1;
    vector from = '0 0 0';
    vector vel = '0 0 0';
    float g = VR_Throw_Gravity();
    entity e = world;
    vector inherit = self.velocity * VR_Throw_Inherit(self);
    if(VR_FlatPlay(self) && self.health > 0 && self.ishuman)
    {
        // (The quick grenade's wind-up, whatever winds it up: B, or the fire button held on it, a live grenade taken some
        // other way as well. grenaim, his flat note 2026-11-27 item 3: the attack button's throw shows its landing as B's.)
        if(self.vr_flatgren_phase == VR_FLATGREN_WIND && self.vr_flatgren_ent != world)
        {
            h = self.vr_flatgren_hand;
            e = self.vr_flatgren_ent;
            from = VR_FlatThrow_From(h, VR_FlatGrenade_Up());
            vel = vr_flatthrow_dir * VR_FlatGrenade_Speed(time - self.vr_flatgren_at) + inherit;
        }
        else if(self.vr_flatthrow_phase == VR_FLATTHROW_WIND && VR_FlatThrow_StillHeld())
        {
            h = self.vr_flatthrow_hand;
            float kind = self.vr_flatthrow_kind;
            float speed = VR_FlatThrow_Speed(time - self.vr_flatthrow_at);
            from = VR_FlatThrow_From(h, VR_FlatThrow_Up());
            vector dir = vr_flatthrow_dir;
            e = self.vr_flatthrow_ent;
            if(kind == 1)
            {
                vel = VR_Throw_WeaponVel(self.vr_flatthrow_wid, VRGetEntWeaponFlags(self, h), dir * speed, '0 0 0', 1);
                vel = VRThrowAssist(self, from, vel, g) + inherit;
            }
            else if(kind == 2 && e != world)
            {
                vel = VRThrowAssist(self, from, VR_Carry_ThrowVel(e, dir * speed, '0 0 0', 1), g) + inherit;
            }
            else if(kind == 3 && e != world)
            {
                vel = VR_FlatThrow_RagVel(e, dir, speed);
                g = cvar_hget(cvarh_sv_gravity); // (a body falls as the world's things do)
            }
            else
            {
                h = -1;
            }
        }
    }
    if(h < 0)
    {
        flatthrowaim(self, -1, '0 0 0');
        return;
    }
    vector at = VR_Throw_ArcEnd(from, vel, g, self, e);
    flatthrowaim(self, h, at);
    if(cvar_hget(cvarh_vr_flat_grenade_debug) >= 2)
    {
        dprint(sprintf("flat throw: aim %v from %v at %v\n", at, from, vel));
    }
};

// (developer 1) Where the last throw first met something: the first frame its velocity left its fall (stopped, bounced,
// stuck), printed once ("flat throw: first hit at"), with where it was the frame before and its velocity then (its
// middle's arc on to the wall from there: flatxhair_melee_test's throw against the crosshair's aim).
vector vr_flatthrow_hitvel;
vector vr_flatthrow_hitorg;
void() VR_FlatThrow_WatchHit =
{
    entity t = vr_flatthrow_last;
    if(!vr_flatthrow_watch || t == world || !cvar_hget(cvarh_developer))
    {
        return;
    }
    if(t != vr_flatthrow_hitwho)
    {
        vr_flatthrow_hitwho = t;
        vr_flatthrow_hitvel = t.velocity;
        vr_flatthrow_hitorg = t.origin;
        dprint(sprintf("flat throw: flying from %v at %v (%s)\n", t.origin, t.velocity, t.classname));
        return;
    }
    if(vr_flatthrow_hitvel == '0 0 0')
    {
        return; // (printed)
    }
    vector fall = vr_flatthrow_hitvel - '0 0 1' * (VR_Throw_Gravity() * frametime);
    if(vlen(t.velocity - fall) > 100 || VR_AxeStick_IsStuck(t))
    {
        dprint(sprintf("flat throw: first hit at %v (%s)\n", t.origin, t.classname));
        dprint(sprintf("flat throw: hit from %v at %v\n", vr_flatthrow_hitorg, vr_flatthrow_hitvel));
        vr_flatthrow_hitvel = '0 0 0';
        return;
    }
    vr_flatthrow_hitvel = t.velocity;
    vr_flatthrow_hitorg = t.origin;
};

// Each server frame (after the quick grenade's): pending tap->wind transition, wind-up cancellation when what it holds
// is gone, the follow-through's end, and (developer 1) where the last throw landed.
void() VR_FlatThrow_Frame =
{
    VR_FlatThrow_Aim();
    VR_FlatThrow_WatchHit();
    if(vr_flatthrow_watch && vr_flatthrow_last != world && cvar_hget(cvarh_developer))
    {
        entity t = vr_flatthrow_last;
        float stuck = VR_AxeStick_IsStuck(t);
        if(cvar_hget(cvarh_vr_flat_grenade_debug) >= 2) // (its path, a line a frame: what it met on the way)
        {
            dprint(sprintf("flat throw: path %v at %v, box %v %v\n", t.origin, t.velocity, t.mins, t.maxs));
        }
        if(stuck || time > vr_flatthrow_watch || (t.velocity == '0 0 0' && time > vr_flatthrow_watch - 3.8))
        {
            vector flew = t.origin - vr_flatthrow_from;
            float down = -flew_z;
            flew_z = 0;
            dprint(sprintf("flat throw: landed: %s %.0f units level from where it left, %.0f down, %s at %v\n", t.classname,
                vlen(flew), down, stuck ? "stuck" : (t.velocity == '0 0 0' ? "at rest" : "still moving"), t.origin));
            vr_flatthrow_watch = 0;
        }
    }
    float ph = self.vr_flatthrow_phase;
    if(!ph)
    {
        return;
    }
    float h = self.vr_flatthrow_hand;
    if(!VR_FlatPlay(self) || self.health <= 0)
    {
        VR_FlatThrow_Set(0);
        return;
    }
    if(ph == VR_FLATTHROW_PENDING)
    {
        if(!VR_FlatThrow_StillHeld())
        {
            VR_FlatThrow_Set(0);
            return;
        }
        if(self.vr_flatthrow_kind != 4 && time - self.vr_flatthrow_press_at >= max(0, cvar_hget(cvarh_vr_flat_throw_tap_time)))
        {
            VR_FlatThrow_BeginWind();
        }
        return;
    }
    if(ph == VR_FLATTHROW_WIND)
    {
        if(!VR_FlatThrow_StillHeld())
        {
            dprint(sprintf("flat throw: the %s hand's wind-up is over: what it held is gone\n", VR_FlatGrenade_HandName(h)));
            VR_FlatThrow_Set(0);
        }
        else if(self.vr_flatthrow_both == 1 && self.vr_flatthrow_kind == 1 && !VRGet2HAiming(self))
        {
            self.vr_flatthrow_both = 2; // (printed once)
            dprint(sprintf("flat throw: the other hand let go of the grip, %.2f s into the wind-up\n", time - self.vr_flatthrow_at));
        }
        return;
    }
    if(self.vr_flatthrow_launch) // (the release: carried on into the follow-through, it leaves the hand now)
    {
        if(time >= self.vr_flatthrow_launch - 0.001)
        {
            self.vr_flatthrow_launch = 0;
            float began = self.vr_flatthrow_at;
            VR_FlatThrow_Throw(self.vr_flatthrow_held);
            self.vr_flatthrow_at = began; // (the follow-through goes on from the key's release)
        }
        return;
    }
    if(time - self.vr_flatthrow_at >= VR_FlatThrow_FollowTime() - 0.001)
    {
        VR_FlatThrow_Set(0);
    }
};
