// ----------------------------------------------------------------------------
// Weapon clash: your own two weapons meeting, and the material table every weapon-on-weapon contact uses (ROUND23.md,
// "Weapon clash"). His words (2026-11-01): "if I'm dual wielding swords or dual wielding any weapon and they collide with
// each other with enough speed, I would like that same spark effect that also happens when I'm colliding with an enemy
// weapon ... a sword blade versus sword blade should definitely spark and have a tink sound, while maybe a shotgun versus
// a shotgun should probably just have a more dull sound and no spark particles."
//
// Each frame (PlayerVRMelee, after the hands' new poses) the two held weapons' lines (each hand's blow line, its near end
// to its far end, VR_Melee_Track) are swept from their last pose to now; the first moment they come within their
// thicknesses of each other (apart at the last pose: a new meeting, not two blades resting crossed) is a clash when the
// two lines close on each other at vr_clash_speed m/s or faster (the relative speed of their nearest points across the
// gap; the real speed: the hands' own clock, so slow motion clashes as fast as you swing). At the meeting point: the
// pair's row of the table below (sparks, a sound), the sparks and the volume by the speed (vr_clash_sparks,
// vr_clash_volume), light haptics in both hands; then nothing from that pair for vr_clash_cooldown s. vr_clash_melee and
// vr_clash_ranged turn it on for the melee weapons and the guns (a pair clashes when both of its weapons are on).
//
// The table (VR_Clash_Row): blade x blade sparks, the blade's clang and a tink over it; blade x crowbar fewer sparks and
// the clang; crowbar x crowbar fewer, a metal clank; blade or crowbar x gun fewer still and the clank; gun x gun no
// sparks, a dull light metal knock; wood (a carried club, a torch) x anything no sparks, a wooden thud; a hand (a fist,
// an open hand) on a weapon no sparks, the clank at half. An enemy's weapon met by your blow (VR_AG_BladeContact) takes
// its row from the same table.
// ----------------------------------------------------------------------------

float VR_CLASH_HAND = 0;  // a fist or an open hand
float VR_CLASH_BLADE = 1; // a sword, an axe, Mjolnir, the chainsaw: edged steel
float VR_CLASH_STEEL = 2; // the crowbar: a steel bar
float VR_CLASH_GUN = 3;   // a gun: its body of blued and cast metal
float VR_CLASH_WOOD = 4;  // a carried club (a wall torch)

float VR_CLASH_STEPS = 12; // the sweep's most samples (one a unit of the points' way, at least 2)

// The row VR_Clash_Row picked: its sparks (times the amount asked), its sound (an impact table key), its volume
// (times the volume asked), a tink over it.
float vr_clash_rsparks;
float vr_clash_rmat;
float vr_clash_rkind;
float vr_clash_rslot;
float vr_clash_rvol;
float vr_clash_rtink;

// What the clashes did (the tests; developer 1 prints "clash: ..." lines).
float vr_clash_n;        // own weapons' clashes
float vr_clash_nsparks;  // ... that sent sparks
float vr_clash_ntinks;   // ... with the tink
float vr_clash_slow;     // meetings too slow to clash
vector vr_clash_x;       // the last clash's meeting point
float vr_clash_spd;      // ... its closing speed (m/s)
float vr_clash_sparked;  // ... its spark particles asked (the impact calls' amounts summed; 0 none)
float vr_clash_vol;      // ... its volume
string vr_clash_snd;     // ... its sound ("" none)

.float vr_clash_next; // the player: no clash of its own weapons before this time (vr_clash_cooldown)

// What a striking thing (VR_MTHING_*) is made of, as a clash's.
float(float xThing) VR_Clash_MatOf =
{
    if(xThing == VR_MTHING_SWORD || xThing == VR_MTHING_AXE || xThing == VR_MTHING_SAW)
    {
        return VR_CLASH_BLADE;
    }
    if(xThing == VR_MTHING_CROWBAR)
    {
        return VR_CLASH_STEEL;
    }
    if(xThing == VR_MTHING_GUN)
    {
        return VR_CLASH_GUN;
    }
    if(xThing == VR_MTHING_CLUB)
    {
        return VR_CLASH_WOOD;
    }
    return VR_CLASH_HAND;
};

// What an enemy's held weapon (its weapon id) is made of.
float(float xWid) VR_Clash_MatOfWid =
{
    if(xWid == WID_SWORD || xWid == WID_AXE || xWid == WID_MJOLNIR || xWid == WID_SUPERAXE || xWid == WID_CHAINSAW)
    {
        return VR_CLASH_BLADE;
    }
    if(xWid == WID_CROWBAR)
    {
        return VR_CLASH_STEEL;
    }
    if(xWid < 0 || xWid == WID_FIST)
    {
        return VR_CLASH_HAND;
    }
    return VR_CLASH_GUN;
};

string(float xMat) VR_Clash_MatName =
{
    if(xMat == VR_CLASH_BLADE) { return "blade"; }
    if(xMat == VR_CLASH_STEEL) { return "crowbar"; }
    if(xMat == VR_CLASH_GUN) { return "gun"; }
    if(xMat == VR_CLASH_WOOD) { return "wood"; }
    return "hand";
};

// The table's row for `xA` meeting `xB` (either way round): sets vr_clash_r*.
void(float xA, float xB) VR_Clash_Row =
{
    float lo = min(xA, xB);
    float hi = max(xA, xB);
    vr_clash_rtink = FALSE;
    vr_clash_rmat = VR_MAT_METAL;
    vr_clash_rslot = VR_IMPACT_SOUND;
    if(hi == VR_CLASH_WOOD)
    {
        vr_clash_rsparks = 0; // a wooden thud
        vr_clash_rmat = VR_MAT_WOOD;
        vr_clash_rkind = VR_HIT_BLOW;
        vr_clash_rslot = VR_IMPACT_LAYER;
        vr_clash_rvol = 0.8;
    }
    else if(lo == VR_CLASH_HAND)
    {
        vr_clash_rsparks = 0; // a hand on a weapon: the clank at half (VR_AG_BladeContact's, as it was)
        vr_clash_rkind = VR_HIT_BASH;
        vr_clash_rvol = 0.5;
    }
    else if(lo == VR_CLASH_BLADE && hi == VR_CLASH_BLADE)
    {
        vr_clash_rsparks = 1; // blade on blade: sparks, the clang, a tink
        vr_clash_rkind = VR_HIT_BLADE;
        vr_clash_rvol = 1;
        vr_clash_rtink = TRUE;
    }
    else if(lo == VR_CLASH_BLADE && hi == VR_CLASH_STEEL)
    {
        vr_clash_rsparks = 0.7;
        vr_clash_rkind = VR_HIT_BLADE;
        vr_clash_rvol = 0.9;
    }
    else if(lo == VR_CLASH_STEEL && hi == VR_CLASH_STEEL)
    {
        vr_clash_rsparks = 0.5;
        vr_clash_rkind = VR_HIT_BASH;
        vr_clash_rvol = 0.9;
    }
    else if(hi == VR_CLASH_GUN && lo != VR_CLASH_GUN)
    {
        vr_clash_rsparks = 0.35; // a blade or the crowbar on a gun: fewer sparks, the clank
        vr_clash_rkind = VR_HIT_BASH;
        vr_clash_rvol = 0.8;
    }
    else
    {
        vr_clash_rsparks = 0; // gun on gun: a dull light metal knock (armour's), no sparks
        vr_clash_rmat = VR_MAT_ARMOUR;
        vr_clash_rkind = VR_HIT_BLOW;
        vr_clash_rslot = VR_IMPACT_OVER;
        vr_clash_rvol = 0.8;
    }
};

// `xA` met `xB` at `xAt`: the row's sparks (`xAmount` times its own, in at most `xMost` bursts of 20, every other one
// the other way across: off both) along `xDir`, and its sound from `xSrc` at `xVol` times its own. The spark amount
// sent (0: none); vr_clash_snd the sound.
float(entity xSrc, float xA, float xB, vector xAt, vector xDir, float xAmount, float xVol, float xMost) VR_Clash_Fx =
{
    VR_Clash_Row(xA, xB);
    float left = xAmount * vr_clash_rsparks;
    float sent = 0;
    float i;
    for(i = 0; i < xMost && left > 0.5; i++)
    {
        float a = min(20, left);
        vector d = xDir;
        if(i & 1)
        {
            d_x = -d_x;
            d_y = -d_y;
        }
        impact(VR_MAT_METAL, VR_HIT_BLOW, xAt, d, a);
        sent = sent + a;
        left = left - a;
    }
    vr_clash_snd = impactsound(vr_clash_rmat, vr_clash_rkind, vr_clash_rslot);
    float vol = min(1, xVol * vr_clash_rvol);
    if(vr_clash_snd != "" && vol > 0)
    {
        sound(xSrc, CHAN_AUTO, vr_clash_snd, vol, ATTN_NORM);
    }
    if(vr_clash_rtink && vol > 0)
    {
        sound(xSrc, CHAN_AUTO, "weapons/tink1.wav", min(1, vol * 0.7), ATTN_NORM);
    }
    return sent;
};

// Whether a weapon of this make clashes (vr_clash_melee, vr_clash_ranged).
float(float xMat) VR_Clash_On =
{
    if(xMat == VR_CLASH_GUN)
    {
        return cvar_hget(cvarh_vr_clash_ranged) != 0;
    }
    if(xMat == VR_CLASH_HAND)
    {
        return FALSE;
    }
    return cvar_hget(cvarh_vr_clash_melee) != 0;
};

// The thickness of a weapon of this make (units: the two lines this near meet).
float(float xMat) VR_Clash_Radius =
{
    return (xMat == VR_CLASH_GUN || xMat == VR_CLASH_WOOD) ? 2 : 1;
};

// The two lines a0..a1 (last pose) / c0..c1 (now), moved over `xDtA` s (0: it stood still), and b0..b1 / d0..d1 over
// `xDtB` s, of makes `xMa` and `xMb`: the clash between them this frame, if any (see above). Each hand's own time since
// its last pose: a hand held still for a few frames has its next pose that many frames after its last. `self` the
// player. TRUE if they clashed.
float(vector a0, vector a1, vector c0, vector c1, vector b0, vector b1, vector d0, vector d1, float xDtA, float xDtB,
      float xMa, float xMb) VR_Clash_Lines =
{
    float r = VR_Clash_Radius(xMa) + VR_Clash_Radius(xMb);
    if(VR_AG_SegDist(a0, a1, b0, b1) <= r * 0.5)
    {
        return FALSE; // (well within each other at the last pose already: resting crossed, or a scrape; no new meeting)
    }
    vector pa = a0 + (a1 - a0) * vr_ag_seg_s; // (the nearest points at the last pose)
    vector pb = b0 + (b1 - b0) * vr_ag_seg_t;
    float way = max(max(vlen(c0 - a0), vlen(c1 - a1)), max(vlen(d0 - b0), vlen(d1 - b1)));
    float steps = min(VR_CLASH_STEPS, max(2, ceil(way)));
    float i;
    for(i = 1; i <= steps; i++)
    {
        float t = i / steps;
        vector e0 = a0 + (c0 - a0) * t;
        vector e1 = a1 + (c1 - a1) * t;
        vector f0 = b0 + (d0 - b0) * t;
        vector f1 = b1 + (d1 - b1) * t;
        float gap = VR_AG_SegDist(e0, e1, f0, f1);
        float s = vr_ag_seg_s;
        float u = vr_ag_seg_t;
        vector qa = e0 + (e1 - e0) * s;
        vector qb = f0 + (f1 - f0) * u;
        if(gap <= r)
        {
            // Their nearest points' speeds (the whole frame's), across the gap they closed (its way just before).
            vector n = pb - pa;
            if(vlen(n) < 0.01)
            {
                n = qb - qa;
            }
            n = vlen(n) > 0.01 ? normalize(n) : '0 0 1';
            vector va = xDtA > 0 ? ((c0 + (c1 - c0) * s) - (a0 + (a1 - a0) * s)) * (1 / xDtA) : '0 0 0';
            vector vb = xDtB > 0 ? ((d0 + (d1 - d0) * u) - (b0 + (b1 - b0) * u)) * (1 / xDtB) : '0 0 0';
            float speed = max(0, (va - vb) * n) / VRMetersToUnits(); // (closing: moving apart is none)
            vector x = (qa + qb) * 0.5;
            float need = max(0, cvar_hget(cvarh_vr_clash_speed));
            if(speed < need)
            {
                vr_clash_slow = vr_clash_slow + 1;
                if(cvar_hget(cvarh_developer))
                {
                    dprint(sprintf("clash: %s met %s at %v, %.2f m/s: under %.2f, none\n", VR_Clash_MatName(xMa),
                        VR_Clash_MatName(xMb), x, speed, need));
                }
                return FALSE;
            }
            float k = min(2, speed / 4); // (1 at 4 m/s)
            vector dir = n + '0 0 0.5';
            float vol = (0.3 + 0.35 * k) * max(0, cvar_hget(cvarh_vr_clash_volume));
            float amount = 20 * min(2, speed / 3) * max(0, cvar_hget(cvarh_vr_clash_sparks));
            float sent = VR_Clash_Fx(self, xMa, xMb, x, dir, amount, vol, 4);
            vr_clash_n = vr_clash_n + 1;
            if(sent > 0)
            {
                vr_clash_nsparks = vr_clash_nsparks + 1;
            }
            if(vr_clash_rtink && vol > 0)
            {
                vr_clash_ntinks = vr_clash_ntinks + 1;
            }
            vr_clash_x = x;
            vr_clash_spd = speed;
            vr_clash_sparked = sent;
            vr_clash_vol = min(1, vol * vr_clash_rvol);
            // Light haptics in both hands: a ring for metal on metal that sparks, a dull knock else.
            float hk = max(0, cvar_hget(cvarh_vr_clash_haptics));
            if(hk > 0)
            {
                float amp = min(1, (0.2 + 0.2 * k) * hk);
                float freq = sent > 0 ? 160 : 80;
                haptic(cVR_MainHand, 0, 0.08, freq, amp);
                haptic(cVR_OffHand, 0, 0.08, freq, amp);
            }
            if(cvar_hget(cvarh_developer))
            {
                dprint(sprintf("clash: %s on %s at %v, %.2f m/s: sparks %.0f, ", VR_Clash_MatName(xMa),
                    VR_Clash_MatName(xMb), x, speed, sent));
                dprint(sprintf("sound %s (%.2f)%s\n", vr_clash_snd == "" ? "none" : vr_clash_snd, vr_clash_vol,
                    vr_clash_rtink ? " and a tink" : ""));
            }
            return TRUE;
        }
        pa = qa;
        pb = qb;
    }
    return FALSE;
};

// The player's (`self`) two held weapons this frame (PlayerVRMelee, after VR_Melee_Frame).
void() VR_Clash_Frame =
{
    if(!cvar_hget(cvarh_vr_clash_melee) && !cvar_hget(cvarh_vr_clash_ranged))
    {
        return;
    }
    if(self.health <= 0 || time < self.vr_clash_next || VRGet2HAiming(self))
    {
        return;
    }
    entity b0 = VR_Blow(cVR_MainHand);
    entity b1 = VR_Blow(cVR_OffHand);
    if(b0.mh_n < 2 || b1.mh_n < 2)
    {
        return;
    }
    float ma = VR_Clash_MatOf(b0.mh_thing);
    float mb = VR_Clash_MatOf(b1.mh_thing);
    if(!VR_Clash_On(ma) || !VR_Clash_On(mb))
    {
        return;
    }
    // A hand whose weapon jumped this frame (put back in it, taken): nothing to sweep. A hand with no new pose stood
    // still: its line now as it was.
    if((b0.mh_fresh && b0.mh_dt <= 0) || (b1.mh_fresh && b1.mh_dt <= 0))
    {
        return;
    }
    float dt0 = b0.mh_fresh ? b0.mh_dt : 0;
    float dt1 = b1.mh_fresh ? b1.mh_dt : 0;
    if(dt0 <= 0 && dt1 <= 0)
    {
        return; // (neither moved)
    }
    float n0 = b0.mh_n - 1;
    float n1 = b1.mh_n - 1;
    vector c0 = b0.mh_cp[0];
    vector c1 = b0.mh_cp[n0];
    vector d0 = b1.mh_cp[0];
    vector d1 = b1.mh_cp[n1];
    vector a0 = b0.mh_fresh ? b0.mh_pp[0] : c0;
    vector a1 = b0.mh_fresh ? b0.mh_pp[n0] : c1;
    vector e0 = b1.mh_fresh ? b1.mh_pp[0] : d0;
    vector e1 = b1.mh_fresh ? b1.mh_pp[n1] : d1;
    if(VR_Clash_Lines(a0, a1, c0, c1, e0, e1, d0, d1, dt0, dt1, ma, mb))
    {
        self.vr_clash_next = time + max(0, cvar_hget(cvarh_vr_clash_cooldown));
    }
};
