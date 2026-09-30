// vr_motion.qc -- the motion recorder's view of the melee (vr_motion.cpp, docs/vr-port/MOTIONS.md).
//
// The engine records takes of the player's motions (head, hands, buttons) and plays them back in the
// mock headset to test the melee. Two kinds of data come from the QC, through three builtins that
// do nothing unless a take is being recorded or played:
//
// - Events, as the melee registers them (VR_Motion_Event): the hits a hand or the head deals
//   (VR_Motion_Hit, from T_DamageImpl), a striking part getting going (its whoosh: a "stroke",
//   VR_Melee_Whoosh), a bash or shove push (VR_Bash), a parry stance held (VR_Bash: "stance"), a parried
//   blow (VR_Parry) and a batted projectile (VR_Deflect_Send). A hand's blow is "melee" with its category
//   as `sub` (slash, stab, pommel, punch, gun: VR_Melee_Category) and its kind, way and striking part as
//   the detail. The kinds and their `sub` are the evaluation's vocabulary (quakevr/motions/expect.cfg):
//   keep them when the melee changes, or change both.
// - A sample each server frame (VR_Motion_Sample, called by the engine): the hands' striking points
//   as the melee computes them, and the parry and bash-guard state. Any motionvalue() added there
//   lands in the take's `qc` column: a free probe of the melee's own state.

void(string kind, string sub, float hand, float value, vector at, entity targ, string detail) motionevent = #0;
void(float hand, vector at, string name) motionpoint = #0;
void(string key, vector value) motionvalue = #0;

// An event (see above): what (`xKind`: melee, bash, parrybash, shove, headbutt, stroke, push, parry,
// deflect) and its kind (`xSub`), the hand (-1: both or none), a value (damage, or a blow's strength),
// where (world; '0 0 0': unknown), against what, and a note (the striking point).
void(string xKind, string xSub, float xHand, float xValue, vector xAt, entity xTarg, string xDetail) VR_Motion_Event =
{
    motionevent(xKind, xSub, xHand, xValue, xAt, xTarg, xDetail);
};

// A hand as an event's `sub` for the pushes: "main", "off", "both" (-1).
string(float xHand) VR_Motion_HandSub =
{
    if(xHand == cVR_MainHand) return "main";
    if(xHand == cVR_OffHand) return "off";
    return "both";
};

// T_DamageImpl, as the damage is dealt (after quad and the damage multipliers, before armour): a hit
// by a player's hand or head (the kinds VR_SetHitKind tags: a blow, a bash or shove, a headbutt).
void(entity xTarg, entity xAttacker, float xDamage, float xIsVRMelee) VR_Motion_Hit =
{
    if(!(xAttacker.flags & FL_CLIENT) || xTarg == xAttacker)
    {
        return;
    }
    if(vr_hitkind == QVR_HITKIND_BASH)
    {
        VR_Motion_Event(vr_bash_counter ? "parrybash" : "bash", VR_Motion_HandSub(vr_hithand), vr_hithand, xDamage,
                        vr_hitpos, xTarg, "");
        return;
    }
    if(vr_hitkind == QVR_HITKIND_SHOVE)
    {
        VR_Motion_Event("shove", VR_Motion_HandSub(vr_hithand), vr_hithand, xDamage, vr_hitpos, xTarg, "");
        return;
    }
    if(vr_hitkind == QVR_HITKIND_HEADBUTT)
    {
        VR_Motion_Event("headbutt", "", -1, xDamage, vr_hitpos, xTarg, "");
        return;
    }
    if(vr_hitkind != QVR_HITKIND_MELEE && !xIsVRMelee)
    {
        return;
    }

    // A hand's blow (vr_melee.qc): its category, and its kind, way and striking part.
    float hand = vr_hitkind == QVR_HITKIND_MELEE ? vr_hithand : -1;
    string sub = "";
    string point = "";
    vector at = vr_hitpos;
    if(hand >= 0 && xAttacker.ishuman)
    {
        entity oldSelf = self;
        self = xAttacker;
        entity b = VR_Blow(hand);
        self = oldSelf;
        sub = VR_Melee_Category(b.mh_kind, b.mh_thing);
        point = b.mh_part;
        if(at == '0 0 0')
        {
            at = b.mh_c_pos;
        }
    }
    VR_Motion_Event("melee", sub, hand, xDamage, at, xTarg, point);
};

// Each server frame while a take is recorded or played (the engine calls it; self: the player,
// `xTarget` the take's monster or world): the striking points of each hand (vr_melee.qc's layout, as last
// swept; six of a sword's nine: the pommel, the hilt, the blade's root, mid-blade, the blade, the tip), and
//  parry  x, y: whether the main, the off hand's weapon would parry a monster's blow from the target
//         (or from straight ahead without one; VR_Parry_Blocks), z: crossed arms would (unarmed)
//  guard  x: the hand in the parry stance (a weapon across; 0 off, 1 main, -1 none), y: the hand pushing it
//         as a bash does (-1 none), z: how far that push has gone (m)
//  m_blow, o_blow  the hand's motion (VR_Melee_Track): x the far end's speed (m/s: a sword's tip, the
//         knuckles), y the grip's, z the far end's run-up towards where it goes (m: VR_Melee_Run)
void(entity xTarget) VR_Motion_Sample =
{
    float hand;
    for(hand = 0; hand < 2; ++hand)
    {
        entity pb = VR_Blow(hand);
        VR_Melee_Layout(pb.mh_thing);
        // Constant indices (a variable one reads a vector array's element wrong: its x alone).
#define VR_MOTION_POINT(k) \
        if(k < pb.mh_n) \
        { \
            motionpoint(hand, pb.mh_cp[k], vr_mpt_name[k]); \
        }
        if(pb.mh_thing == VR_MTHING_SWORD)
        {
            VR_MOTION_POINT(0)
            VR_MOTION_POINT(1)
            VR_MOTION_POINT(3)
            VR_MOTION_POINT(5)
            VR_MOTION_POINT(6)
            VR_MOTION_POINT(8)
        }
        else
        {
            VR_MOTION_POINT(0)
            VR_MOTION_POINT(1)
            VR_MOTION_POINT(2)
            VR_MOTION_POINT(3)
            VR_MOTION_POINT(4)
            VR_MOTION_POINT(5)
        }
#undef VR_MOTION_POINT
        // The weapon's line (VR_Parry_WeaponLine): its butt (a sword's pommel, the axe's or the hammer's
        // handle end, a gun's grip) and its far end (a sword's tip, a head, a muzzle).
        if(VR_Parry_WeaponLine(self, hand))
        {
            motionvalue(hand == cVR_MainHand ? "m_butt" : "o_butt", vr_parry_butt);
            motionvalue(hand == cVR_MainHand ? "m_end" : "o_end", vr_parry_end);
        }
    }

    vector to = VRForwardOf('0 1 0' * self.v_viewangle_y);
    if(xTarget != world)
    {
        vector d = xTarget.origin + (xTarget.mins + xTarget.maxs) * 0.5 - self.origin;
        d_z = 0;
        if(vlen(d) > 0.1)
        {
            to = normalize(d);
        }
    }
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;
    vector parry;
    if(xTarget != world)
    {
        vr_parry_from = xTarget.origin + (xTarget.mins + xTarget.maxs) * 0.5;
    }
    parry_x = VR_Parry_Blocks(self, cVR_MainHand, to);
    parry_y = VR_Parry_Blocks(self, cVR_OffHand, to);
    vr_parry_from = '0 0 0';
    parry_z = VR_Parry_ArmsCrossed(self, to);
    motionvalue("parry", parry);

    entity bm = VR_Blow(cVR_MainHand);
    entity bo = VR_Blow(cVR_OffHand);
    vector guard = '-1 -1 0';
    if(bm.mh_stance == 1)
    {
        guard_x = cVR_MainHand;
    }
    else if(bo.mh_stance == 1)
    {
        guard_x = cVR_OffHand;
    }
    if(bm.mh_pushing)
    {
        guard_y = cVR_MainHand;
        guard_z = bm.mh_push_dist;
    }
    else if(bo.mh_pushing)
    {
        guard_y = cVR_OffHand;
        guard_z = bo.mh_push_dist;
    }
    motionvalue("guard", guard);
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;

    motionvalue("m_blow", [vlen(bm.mh_vfar), vlen(bm.mh_vgrip), VR_Melee_Run(bm, FALSE)]);
    motionvalue("o_blow", [vlen(bo.mh_vfar), vlen(bo.mh_vgrip), VR_Melee_Run(bo, FALSE)]);
};

// Playback (vr_motion_play): `self`'s hand `xHand` holds weapon `xWid` (WID_*, WID_FIST: empty) with
// `xFlags` (QVR_WPNFLAG_USE_SECONDARY_AMMO: the hell knight's sword, a gun's other ammo), loaded, as
// impulse 150 + id does.
void(float xHand, float xWid, float xFlags) VR_Motion_Equip =
{
    if(xWid < WID_FIST || xWid > WID_LAST)
    {
        return;
    }
    if(xWid != WID_FIST)
    {
        VR_WeaponUtil_EntAddWeapon(self, xWid);
    }
    VRSetEntWeapon(self, xHand, xWid);
    VRSetEntWeaponFlags(self, xHand, xFlags & QVR_WPNFLAG_USE_SECONDARY_AMMO);
    VRSetEntWeaponClip(self, xHand, WeaponIdToBaseClipSize(xWid));
    W_SetCurrentAmmoFor(xHand);
};
