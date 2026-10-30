//
//
//
// ----------------------------------------------------------------------------
// VR Utilities Implementation Details
// ----------------------------------------------------------------------------

// Set by the engine round a spawn function it runs after the map has spawned (box3d::spawnClass: the toolgun, physics
// spawns): 1 a live spawn (what a map makes static stays an entity: vr_walltorch.qc; an item placed at once:
// VR_SpawnLive_Finish), 2 the toolgun's ghost's probe (made to be looked at and removed: nothing else made with it).
nosave float vr_spawn_live;

// The per-hand getters run many times a frame (the QC VM interprets every statement, and a call
// costs several), so they are branchless selects that fteqcc inlines (`inline`: its inliner takes
// only branch-free bodies): the off hand's (cVR_OffHand, 0) value for 0, the main hand's for any
// other (as the old if/else did). Both fields are read; they are finite, so `a * 1 + b * 0` is `a`.
#define VR_IMPL_SELECT(xHand, xOff, xMain) ((xMain) * (xHand != cVR_OffHand) + (xOff) * (xHand == cVR_OffHand))

#define VR_IMPL_MAKE_VECTOR_GETTER(xFnName, xOff, xMain) \
    inline vector(float xHand) xFnName = \
    { \
        return VR_IMPL_SELECT(xHand, xOff, xMain); \
    }

#define VR_IMPL_MAKE_FLOAT_GETTER(xFnName, xOff, xMain) \
    inline float(float xHand) xFnName = \
    { \
        return VR_IMPL_SELECT(xHand, xOff, xMain); \
    }

#define VR_IMPL_MAKE_FLOAT_SETTER(xFnName, xOff, xMain) \
    void(float xHand, float xValue) xFnName = \
    { \
        if(xHand == cVR_OffHand) *xOff = xValue; \
        else                     *xMain = xValue; \
    }

// Whether a hotspot is a holster (QVR_HS_LEFT_SHOULDER_HOLSTER 3 .. QVR_HS_RIGHT_UPPER_HOLSTER 9,
// but for QVR_HS_HAND_SWITCH 7).
inline float(float xHotspot) isHolsterHotspot =
{
    return (xHotspot >= QVR_HS_LEFT_SHOULDER_HOLSTER) * (xHotspot <= QVR_HS_RIGHT_UPPER_HOLSTER) *
           (xHotspot != QVR_HS_HAND_SWITCH);
}

//
//
//
// ----------------------------------------------------------------------------
// VR Getters
// ----------------------------------------------------------------------------

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandPos,
                           self.offhandpos,
                           self.handpos);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandRot,
                           self.offhandrot,
                           self.handrot);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandVel,
                           self.offhandvel,
                           self.handvel);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandThrowVel,
                           self.offhandthrowvel,
                           self.handthrowvel);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandAVel,
                           self.offhandavel,
                           self.handavel);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandThrowPos,
                           self.offhandthrowpos,
                           self.handthrowpos);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetHandThrowFlick,
                           self.offhandthrowflick,
                           self.handthrowflick);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetHandThrowAge,
                          self.offhandthrowage,
                          self.handthrowage);

VR_IMPL_MAKE_VECTOR_GETTER(VRGetMuzzlePos,
                           self.offmuzzlepos,
                           self.muzzlepos);

// Where a hand's shots, projectiles and beams go: its aim turned by the weapon's Shot Pitch and Yaw (the client's
// weapons::shotAngles). The hand's aim for a player the engine gives none (a bot: its shotrot stays 0 0 0).
vector(float xHand) VRGetWeaponFireRot =
{
    vector r = VR_IMPL_SELECT(xHand, self.offshotrot, self.shotrot);
    if(r == '0 0 0')
    {
        return VRGetHandRot(xHand);
    }
    return r;
}

vector(float xHand) VRGetWeaponFireRotFwd =
{
    makeforward(VRGetWeaponFireRot(xHand));
    return v_forward;
}

// The forward vector of `xAngles`, leaving v_forward (and v_right, v_up) as they were: for code
// that runs while a caller still needs them.
vector(vector xAngles) VRForwardOf =
{
    vector oldForward = v_forward;
    makeforward(xAngles);
    vector forward = v_forward;
    v_forward = oldForward;
    return forward;
}

typedef void() NullaryFn;
typedef void(entity, float) PainFn;

PainFn(NullaryFn fn) asPainFn =
{
    return fn;
}

NullaryFn(PainFn fn) fromPainToTouchFn =
{
    return fn;
}

//
//
//

VR_IMPL_MAKE_FLOAT_GETTER(VRGetMeleeHitSoundPlayed,
                          self.offhand_melee_hit_sound_played,
                          self.melee_hit_sound_played);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetMeleeHitSoundPlayed,
                          &self.offhand_melee_hit_sound_played,
                          &self.melee_hit_sound_played);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetGunChannel,
                          CHAN_WEAPON2,
                          CHAN_WEAPON);

// sound(), a little higher or lower each time (up to vr_snd_pitch_jitter percent, at random): the most frequent
// sounds (blows, swings, squishes, clicks, footsteps), one recording each, less alike.
void(entity e, float chan, string samp, float vol, float atten) VR_SoundVaried =
{
    float j = min(25, cvar_hget(cvarh_vr_snd_pitch_jitter));
    if(j <= 0)
    {
        sound(e, chan, samp, vol, atten);
        return;
    }
    sound_pitch(e, chan, samp, vol, atten, 100 + crandom() * j);
};

// A sound from the hand xHand (any free channel: CHAN_HAND, CHAN_HAND2), not the middle of the head.
VR_IMPL_MAKE_FLOAT_GETTER(VRGetHandChannel,
                          CHAN_HAND2,
                          CHAN_HAND);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetFireButtonPressed,
                          self.button3,
                          self.button0);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetCurrentAmmo,
                          self.currentammo2,
                          self.currentammo);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetAttackFinished,
                          self.offhand_attack_finished,
                          self.attack_finished);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetAttackFinished,
                          &self.offhand_attack_finished,
                          &self.attack_finished);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetWeaponFrame,
                          self.weaponframe2,
                          self.weaponframe);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetWeaponFrame,
                          &self.weaponframe2,
                          &self.weaponframe);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetAmmoCounter,
                          &self.ammocounter2,
                          &self.ammocounter);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetWeaponFlags,
                          self.weaponflags2,
                          self.weaponflags);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetWeaponFlags,
                          &self.weaponflags2,
                          &self.weaponflags);

// (The hands' magazines: VRGetWeaponClip, VRSetWeaponClip, vr_weaponinst.qc.)

VR_IMPL_MAKE_FLOAT_SETTER(VRSetWeaponClipSize,
                          &self.weaponclipsize2,
                          &self.weaponclipsize);

VR_IMPL_MAKE_FLOAT_GETTER(VRGetLavaGunFired,
                          self.offHandLavaGunFired,
                          self.mainHandLavaGunFired);

VR_IMPL_MAKE_FLOAT_SETTER(VRSetLavaGunFired,
                          &self.offHandLavaGunFired,
                          &self.mainHandLavaGunFired);

// Whether `e` plays with tracked VR hands (not a flat-screen player, not a bot): the engine tells,
// per client.
inline float(entity e) VRHandsTracked =
{
    return !!(e.vrbits0 & QVR_VRBITS0_HANDSTRACKED);
}

inline float(entity e, float xHand) VRGetEntFireButtonPressed =
{
    return VR_IMPL_SELECT(xHand, e.button3, e.button0);
}

// The hand's upper face button held (B on the main hand, Y on the off hand; the engine's key, in the game): the
// grappling hook's reel (vr_grapple.qc).
inline float(entity e, float xHand) VRGetEntSecondaryPressed =
{
    return !!(e.vrbits0 & VR_IMPL_SELECT(xHand, QVR_VRBITS0_OFFHAND_SECONDARY, QVR_VRBITS0_MAINHAND_SECONDARY));
}

// The hand's lower face button held (A on the main hand, X on the off hand; its key, jump or reload, pressed as ever):
// the grappling hook's unreel (vr_grapple.qc).
inline float(entity e, float xHand) VRGetEntPrimaryPressed =
{
    return !!(e.vrbits0 & VR_IMPL_SELECT(xHand, QVR_VRBITS0_OFFHAND_PRIMARY, QVR_VRBITS0_MAINHAND_PRIMARY));
}

// The head (between the eyes): the client's, which leans away from the origin; the old eye point if
// the client sends none.
vector(entity e) VRGetHeadPos =
{
    return e.headpos != '0 0 0' ? e.headpos : e.origin + e.view_ofs;
};

inline vector(entity e, float xHand) VRGetEntHandPos =
{
    return VR_IMPL_SELECT(xHand, e.offhandpos, e.handpos);
}

inline vector(entity e, float xHand) VRGetEntHandRot =
{
    return VR_IMPL_SELECT(xHand, e.offhandrot, e.handrot);
}

// The hand's velocity, in metres per second.
inline vector(entity e, float xHand) VRGetEntHandVel =
{
    return VR_IMPL_SELECT(xHand, e.offhandvel, e.handvel);
}

float(float xHand) VRIsAmmolessWeapon =
{
    return VRGetCurrentAmmo(xHand) == AID_NONE;
}

inline float(float xHand) VRGetOtherHand =
{
    return VR_IMPL_SELECT(xHand, cVR_MainHand, cVR_OffHand);
}

void(entity e, float xHand, float v) VRSetEntWeapon =
{
    if(!VR_Pack_WeaponAvailable(v)) { v = WID_FIST; }
    if(xHand == cVR_OffHand) e.weapon2 = v;
    else                     e.weapon = v;
}

void(entity e, float xHand, float v) VRSetEntWeaponFlags =
{
    v = VR_Pack_WeaponFlags(xHand == cVR_OffHand ? e.weapon2 : e.weapon, v);
    if(xHand == cVR_OffHand) e.weaponflags2 = v;
    else                     e.weaponflags = v;
}

inline float(entity e, float xHand) VRGetEntWeapon =
{
    return VR_IMPL_SELECT(xHand, e.weapon2, e.weapon);
}

inline float(entity e, float xHand) VRGetEntWeaponFlags =
{
    return VR_IMPL_SELECT(xHand, e.weaponflags2, e.weaponflags);
}

float*(entity e, float xHand) VRGetEntWeaponPtr =
{
    if(xHand == cVR_OffHand) return &e.weapon2;
    else                     return &e.weapon;
}

float*(entity e, float xHand) VRGetEntWeaponFlagsPtr =
{
    if(xHand == cVR_OffHand) return &e.weaponflags2;
    else                     return &e.weaponflags;
}

// Whether `xHand` holds no weapon (its weapon is the fist). It may still carry a box, a gib, a torch (vr_carry.qc):
// for what is about weapons alone (a weapon to drop, its sound, its line); anything else asks VRIsHandEmpty.
inline float(entity e, float xHand) VRIsHandWeaponless =
{
    return VR_IMPL_SELECT(xHand, e.weapon2, e.weapon) == WID_FIST;
}

// Whether `xHand` is empty: no weapon, and carrying nothing (a box, a gib, a torch, a grenade: .mainhand_held,
// .offhand_held). What may take something, grip a hotspot or steady the other hand's weapon.
float(entity e, float xHand) VRIsHandEmpty =
{
    if(VR_IMPL_SELECT(xHand, e.weapon2, e.weapon) != WID_FIST)
    {
        return FALSE;
    }
    return (xHand == cVR_OffHand ? e.offhand_held : e.mainhand_held) == world;
}

// Whether `xHand` carries a gun by its foregrip (the hand-off).
inline float(entity e, float xHand) VRIsHandCarryingByForegrip =
{
    return (VR_IMPL_SELECT(xHand, e.weapon2, e.weapon) != WID_FIST) *
           !!(VR_IMPL_SELECT(xHand, e.weaponflags2, e.weaponflags) & QVR_WPNFLAG_FOREGRIP_CARRIED);
}

void(entity e, float xHand) VRSetHandEmpty =
{
    VRSetEntWeapon(e, xHand, WID_FIST);
    VRSetEntWeaponFlags(e, xHand, 0);
    VRSetEntWeaponInst(e, xHand, world); // (its record: moved on first, else gone with it)
}

inline float(float xHand) VRGetWeapon =
{
    return VR_IMPL_SELECT(xHand, self.weapon2, self.weapon);
}

inline float(entity e, float xHand) VRIsEntHandGrabbingBit =
{
    return VR_IMPL_SELECT(xHand, !!(e.vrbits0 & QVR_VRBITS0_OFFHAND_GRABBING), !!(e.vrbits0 & QVR_VRBITS0_MAINHAND_GRABBING));
}

inline float(entity e, float xHand) VRIsEntHandPrevGrabbingBit =
{
    return VR_IMPL_SELECT(xHand, !!(e.vrbits0 & QVR_VRBITS0_OFFHAND_PREVGRABBING), !!(e.vrbits0 & QVR_VRBITS0_MAINHAND_PREVGRABBING));
}

inline float(entity e, float xHand) VRIsEntHandReloadingBit =
{
    return VR_IMPL_SELECT(xHand, !!(e.vrbits0 & QVR_VRBITS0_OFFHAND_RELOADING), !!(e.vrbits0 & QVR_VRBITS0_MAINHAND_RELOADING));
}

inline float(entity e, float xHand) VRIsEntHandReloadFlickingBit =
{
    return VR_IMPL_SELECT(xHand, !!(e.vrbits0 & QVR_VRBITS0_OFFHAND_RELOADFLICKING), !!(e.vrbits0 & QVR_VRBITS0_MAINHAND_RELOADFLICKING));
}

float(entity entPlayer) VRIsTouchingHandFake =
{
    return entPlayer.touchinghand == cVR_FakeHand;
}

float(entity entPlayer) VRGetTouchingHand =
{
    if(VRIsTouchingHandFake(entPlayer))
    {
        return cVR_MainHand;
    }

    return entPlayer.touchinghand;
}

float(entity entTarget) VRIsHandtouchHandFake =
{
    return entTarget.handtouch_hand == cVR_FakeHand;
}

float(entity entTarget) VRGetHandtouchHand =
{
    if(VRIsHandtouchHandFake(entTarget))
    {
        return cVR_MainHand;
    }

    return entTarget.handtouch_hand;
}

inline float(float flags, float bit) VRHasBit =
{
    return !!(flags & bit);
}

float(float flags, float bit, float value) VRSetBit =
{
    if(value)
    {
        return flags | bit;
    }
    else
    {
        return flags & ~bit;
    }
}

inline float(entity ent) VRGetOffHandGrabbingBit =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_OFFHAND_GRABBING);
}

inline float(entity ent) VRGetOffHandPrevGrabbingBit =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_OFFHAND_PREVGRABBING);
}

inline float(entity ent) VRGetMainHandGrabbingBit =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_MAINHAND_GRABBING);
}

inline float(entity ent) VRGetMainHandPrevGrabbingBit =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_MAINHAND_PREVGRABBING);
}

inline float(entity ent) VRGet2HAiming =
{
    return !!(ent.vrbits0 & QVR_VRBITS0_2H_AIMING);
}


void(entity ent, float value) VRSetTeleporting =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_TELEPORTING, value);
}

void(entity ent, float value) VRSetOffHandGrabbingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_GRABBING, value);
}

void(entity ent, float value) VRSetOffHandPrevGrabbingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_PREVGRABBING, value);
}

void(entity ent, float value) VRSetMainHandGrabbingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_GRABBING, value);
}

void(entity ent, float value) VRSetMainHandPrevGrabbingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_PREVGRABBING, value);
}

void(entity ent, float value) VRSet2HAiming =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_2H_AIMING, value);
}

void(entity ent, float value) VRSetOffHandReloadFlickingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_RELOADFLICKING, value);
}

void(entity ent, float value) VRSetOffHandPrevReloadFlickingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_OFFHAND_PREVRELOADFLICKING, value);
}

void(entity ent, float value) VRSetMainHandReloadFlickingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_RELOADFLICKING, value);
}

void(entity ent, float value) VRSetMainHandPrevReloadFlickingBit =
{
    ent.vrbits0 = VRSetBit(ent.vrbits0, QVR_VRBITS0_MAINHAND_PREVRELOADFLICKING, value);
}

// Flatscreen play (-flatscreenmode; vr_flatplay.cpp, docs/vr-port/FLATPLAY.md): the VR gameplay played on the desktop
// screen, with the hands the game poses itself. The gate every flatscreen control hangs on, per player (co-op: his own
// client's mode, .vr_flatscreen from his move); 0 in a headset and 0 without the flag.
float(entity xPlayer) VR_FlatPlay =
{
    return xPlayer.vr_flatscreen != 0;
};

float(entity ent) VRIsWeaponReloadingEnabled =
{
    // Flatscreen play (-flatscreenmode): no reloading for that player. A magazine is refilled from the reserve as it
    // empties, with no handling step (vr_flatplay.cpp, docs/vr-port/FLATPLAY.md).
    // (Whether any gun reloads: the global mode, or a gun's own override.)
    return (VR_Reload_BestMode() != QVR_RELOADMODE_NONE) &&
           (getWeaponMode() == QVR_WEAPONMODE_IMMERSIVE) &&
           !VR_FlatPlay(ent);
}

// The same of the weapon `wid` alone (its own reloading mode: the Per-Weapon Reload Overrides).
// (Its record `inst`: AD support: the Widowmaker's and the Plasma Gun's own modes; world: not known.)
float(entity ent, float wid, entity inst) VRIsWeaponReloadingEnabledForInst =
{
    return VRIsWeaponReloadingEnabled(ent) &&
           WeaponIdToSupportsReload(wid) &&
           VR_Reload_ModeOfInst(wid, inst) != QVR_RELOADMODE_NONE;
}

float(entity ent, float wid) VRIsWeaponReloadingEnabledForWeapon =
{
    return VRIsWeaponReloadingEnabledForInst(ent, wid, world);
}

float(entity ent, float xHand) VRIsWeaponReloadingEnabledFor =
{
    return ent.ishuman &&
           VRIsWeaponReloadingEnabledForInst(ent, VRGetWeapon(xHand), VRGetEntWeaponInst(ent, xHand));
}

// (Not the Widowmaker: a super shotgun that never breaks open.)
float(entity ent, float xHand) VRIsWeaponReloadFlickingEnabledFor =
{
    return VRIsWeaponReloadingEnabledFor(ent, xHand) &&
           WeaponIdToSupportsReloadFlicking(VRGetWeapon(xHand)) && !AD_WidowmakerIn(ent, xHand);
}

// A missile's touch (self the missile): whether it leaves `xOther` alone as its owner (Quake: never on the one who fired
// it). One carried through a teleporter (.vr_gate_crossed, set by the engine) meets its owner: your nail or rocket at
// your own image, seen through the gate, hits you.
float vr_owner_touches; // the touches a missile had of its owner (the tests: vr_ad_test 90)
float(entity xOther) VR_OwnerSpared =
{
    if(xOther == self.owner)
    {
        vr_owner_touches++;
    }
    return xOther == self.owner && !self.vr_gate_crossed;
}

// ----------------------------------------------------------------------------
// Canonical helpers (CODE_STYLE.md, "Canonical helpers"): use these, don't write local copies.
// ----------------------------------------------------------------------------

// The first player entity (single player: the player; world if none). Coop code that wants the nearest or a visible
// client keeps checkclient.
inline entity() VR_Player = { return find(world, classname, "player"); };
// The player entity after `p` (the loop: `for(p = VR_Player(); p; p = VR_NextPlayer(p))`).
inline entity(entity p) VR_NextPlayer = { return find(p, classname, "player"); };

// The first living player (health above 0, not dead; world if none).
entity() VR_PlayerAlive =
{
    entity p = VR_Player();
    while(p)
    {
        if(p.health > 0 && !p.deadflag) { return p; }
        p = VR_NextPlayer(p);
    }
    return world;
};

// `v` held to xLow .. xHigh.
float(float v, float xLow, float xHigh) clamp =
{
    if(v < xLow) return xLow;
    if(v > xHigh) return xHigh;
    return v;
};

// The angle `a` (degrees) wrapped to -180 .. 180 (180 itself becomes -180).
inline float(float a) VR_Wrap180 = { return a - 360 * floor((a + 180) / 360); };

// a x b.
vector(vector a, vector b) VR_Cross =
{
    vector c;
    c_x = a_y * b_z - a_z * b_y;
    c_y = a_z * b_x - a_x * b_z;
    c_z = a_x * b_y - a_y * b_x;
    return c;
};

// a .. b at t (0 a, 1 b; not clamped).
inline vector(vector a, vector b, float t) VR_Lerp = { return a + (b - a) * t; };
// Smoothstep of `x` clamped to 0 .. 1.
float(float x) VR_Smooth = { x = min(1, max(0, x)); return x * x * (3 - 2 * x); };

// The least distance from `p` to the segment a .. b.
float(vector p, vector a, vector b) VR_PointSegmentDistance =
{
    vector ab = b - a;
    float l2 = ab * ab;
    float k = l2 > 0 ? min(1, max(0, ((p - a) * ab) / l2)) : 0;
    return vlen(p - (a + ab * k));
};

// VR_SegmentDistance's closest points: how far along the first segment (0 its start, 1 its end), and the second.
float vr_seg_s;
float vr_seg_t;

// The least distance between the segments `xA0` .. `xA1` and `xB0` .. `xB1` (their closest points: vr_seg_s,
// vr_seg_t). A segment whose squared length is at most `xEps` is a point; for near-parallel segments (the determinant
// at most `xDenomMin`) the search starts from the first's start.
float(vector xA0, vector xA1, vector xB0, vector xB1, float xEps, float xDenomMin) VR_SegmentDistanceEps =
{
    vector d1 = xA1 - xA0;
    vector d2 = xB1 - xB0;
    vector r = xA0 - xB0;
    float a = d1 * d1;
    float e = d2 * d2;
    float f = d2 * r;
    float s = 0;
    float t = 0;
    vr_seg_s = 0;
    vr_seg_t = 0;
    if(a <= xEps && e <= xEps)
    {
        return vlen(r);
    }
    if(a <= xEps)
    {
        t = min(1, max(0, f / e));
    }
    else
    {
        float c = d1 * r;
        if(e <= xEps)
        {
            s = min(1, max(0, -c / a));
        }
        else
        {
            float bb = d1 * d2;
            float denom = a * e - bb * bb;
            if(denom > xDenomMin)
            {
                s = min(1, max(0, (bb * f - c * e) / denom));
            }
            t = (bb * s + f) / e;
            if(t < 0)
            {
                t = 0;
                s = min(1, max(0, -c / a));
            }
            else if(t > 1)
            {
                t = 1;
                s = min(1, max(0, (bb - c) / a));
            }
        }
    }
    vr_seg_s = s;
    vr_seg_t = t;
    return vlen((xA0 + d1 * s) - (xB0 + d2 * t));
};

// VR_SegmentDistanceEps with the usual thresholds.
inline float(vector xA0, vector xA1, vector xB0, vector xB1) VR_SegmentDistance =
{
    return VR_SegmentDistanceEps(xA0, xA1, xB0, xB1, 0.0001, 0.0001);
};
