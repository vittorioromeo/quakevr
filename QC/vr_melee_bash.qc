// vr_melee_bash.qc -- melee for tracked hands, last part: swing sounds by speed, parry stamina and counter-attacks,
// the stance and the bash, and each frame's melee. Split from vr_melee.qc (the model is described at its top);
// compiled right after it (progs.src).

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
