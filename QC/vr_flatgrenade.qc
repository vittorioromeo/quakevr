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
// a grenade is held: flatplay::routeFire) arms it (VR_HandGrenade_Arm: the pin, the fuse, vr_handgrenade_fuse; the
// headset's own countdown and cues); then, held, winds up the throw (vr_flat_grenade_arm_mode 1: the next press; 0: the
// arming press itself), and let go, throws it along the crosshair vr_flat_grenade_throw_up above it, as fast as the
// wind-up was long (vr_flat_grenade_speed_min .. _max over vr_flat_grenade_wind_full), with his own motion
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
float VR_FLATGREN_THROWN = 5;  // thrown: the follow-through (vr_flat_grenade_follow_time)

float vr_flatgren_thrown;      // (the tests) throws, and the last one's speed and wind-up
float vr_flatgren_lastspeed;
float vr_flatgren_lastheld;
entity vr_flatgren_last;       // (the tests) the last grenade thrown

string(float xHand) VR_FlatGrenade_HandName =
{
    return xHand == cVR_MainHand ? "main" : "off";
};

void(float xPhase) VR_FlatGrenade_Set =
{
    self.vr_flatgren_phase = xPhase;
    self.vr_flatgren_at = time;
    self.vr_flatgren = xPhase ? xPhase + (self.vr_flatgren_hand == cVR_MainHand ? 8 : 0) : 0;
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
    if(ph == VR_FLATGREN_FETCH || ph == VR_FLATGREN_ARMED || ph == VR_FLATGREN_WIND)
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

// Thrown: let go of along the crosshair, `xHeld` seconds wound up.
void(float xHand, entity g, float xHeld) VR_FlatGrenade_Throw =
{
    float full = min(5, max(0.1, cvar_hget(cvarh_vr_flat_grenade_wind_full)));
    float k = min(1, max(0, xHeld / full));
    float lo = max(0, cvar_hget(cvarh_vr_flat_grenade_speed_min));
    float hi = max(lo, cvar_hget(cvarh_vr_flat_grenade_speed_max));
    float speed = lo + (hi - lo) * k;
    vector ang = self.v_angle;
    ang_x = ang_x - min(60, max(-30, cvar_hget(cvarh_vr_flat_grenade_throw_up))); // (Quake's pitch: up is negative)
    makevectors(ang);
    vector dir = v_forward;
    // It leaves just ahead of his eyes (the wound-up hand is behind his head): never inside a wall.
    vector head = VRGetHeadPos(self);
    traceline(head, head + dir * 12, TRUE, self);
    vector start = trace_fraction < 1 ? trace_endpos - dir * 2 : trace_endpos;
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
    dprint(sprintf("flat grenade: thrown by the %s hand after %.2f s wound up: %.0f u/s (%.0f%% of the way to %.0f), %.2f s of fuse left\n",
        VR_FlatGrenade_HandName(xHand), xHeld, vlen(g.velocity), k * 100, hi, g.vr_gren_fuse - time));
    self.vr_flatgren_ent = world;
    VR_FlatGrenade_Set(VR_FLATGREN_THROWN);
};

// Each server frame (W_Frame, after the carry's hands): the trip's end, the fire button's arming, wind-up and throw.
void() VR_FlatGrenade_Frame =
{
    float ph = self.vr_flatgren_phase;
    if(!ph)
    {
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
        if(since >= max(0.02, cvar_hget(cvarh_vr_flat_grenade_follow_time)) - 0.001)
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
    if(ph == VR_FLATGREN_WIND && !btn)
    {
        VR_FlatGrenade_Throw(h, g, since);
    }
};
