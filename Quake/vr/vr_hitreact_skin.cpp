// vr_hitreact_skin.cpp -- relinkSkinned: an armed monster's arms posed by its weapons (vr_agarms) and those weapons
// drawn on its hands. Moved out of vr_hitreact.cpp (see vr_hitreact.hpp); what they share is vr_hitreact_internal.hpp.

#include "vr_hitreact_internal.hpp"

namespace hitreact_detail
{

// vr_agarms: `e`'s arms posed by its weapons (vr_agarms.cpp) and those drawn on its hands; `s` placed this frame.
void relinkSkinned(const ArmPose& s, const entity_t* e, const aliashdr_t* hdr, int pose1, int pose2, float blend)
{
    const ArmedRig* a = armedRigOf(e->model);
    const ragdoll::Rig* rig = ragdoll::rigFor(e->model);
    agarms::Spec spec;
    if(!a || !rig || rig->numPoses != hdr->numposes || !specOf(*a, *rig, hdr, spec))
    {
        return;
    }
    // Its bones as drawn (kickInstance's), their hit reactions (the chest's carries the shoulders, and the weapons).
    rigpose::Rigid posed[ragdoll::maxBones];
    const float b = pose1 == pose2 ? 0.f : za::clamp(blend, 0.f, 1.f);
    for(int i = 0; i < rig->numBones; i++)
    {
        const rigpose::Rigid k1 = rigpose::key(*rig, pose1, i), k2 = rigpose::key(*rig, pose2, i);
        posed[i].rot = glm::slerp(k1.rot, k2.rot, b);
        posed[i].pos = glm::mix(k1.pos, k2.pos, b);
    }
    if(rig->authored)
    {
        rig2::posedSmooth(e, hdr, *rig, pose1, pose2, blend, posed); // (drawn from these: r_lerpmodels 3's spline)
    }
    // The Rig Studio's animations (vr_rigseq.cpp; kickInstance draws them so): its bones from them, the layers below on top.
    bool layerArms = false;
    // (the keys' torso: the chest on the hips as keyed against as the frame has it; the upper body's hold keeps it)
    rigpose::Rigid layerTorso;
    bool layered = false;
    if(rigseq::wants(e))
    {
        rigpose::Rigid F[ragdoll::maxBones];
        if(rigseq::bones(e, *rig, hdr, pose1, pose2, blend, posed, F, &layerArms))
        {
            int hips = spec.chest;
            for(int n = 0; hips >= 0 && rig->bones[hips].parent >= 0 && n < ragdoll::maxBones; n++)
            {
                hips = rig->bones[hips].parent;
            }
            if(spec.chest >= 0 && hips >= 0 && hips != spec.chest)
            {
                const rigpose::Rigid was = composeRigid(inverseRigid(posed[hips]), posed[spec.chest]);
                const rigpose::Rigid now = composeRigid(inverseRigid(F[hips]), F[spec.chest]);
                layerTorso = composeRigid(inverseRigid(was), now);
                layered = true;
            }
            for(int i = 0; i < rig->numBones; i++)
            {
                posed[i] = F[i];
            }
        }
    }
    // (the strafe lean's measure, vr_agrunt_lean_info: its pelvis's roll by it, its chest's as solved)
    const auto rollOf = [](const rigpose::Rigid& r) {
        const glm::vec3 u = r.rot * glm::vec3{0.f, 0.f, 1.f};
        return za::atan2(-u.y, u.z) * (180.f / 3.14159265f);
    };
    const auto yawOf = [](const rigpose::Rigid& r) {
        const glm::vec3 f = r.rot * glm::vec3{1.f, 0.f, 0.f};
        return za::atan2(f.y, f.x) * (180.f / 3.14159265f);
    };
    float leanPelvisYaw = 0.f;
    int leanHips = spec.chest;
    for(int n = 0; leanHips >= 0 && rig->bones[leanHips].parent >= 0 && n < ragdoll::maxBones; n++)
    {
        leanHips = rig->bones[leanHips].parent;
    }
    float leanPelvis = 0.f;
    {
        // A take's kneel (vr_aglegs: its legs posed, the body above carried by its pelvis, the arms solved on it)
        float yawDeg = 0.f;
        const glm::vec3 origin = drawnOrigin(e, &yawDeg);
        rigpose::Rigid legs[ragdoll::maxBones];
        aglegs::Take take;
        take.w = s.take;
        take.side = s.takeSide;
        const int hs = s.takeSide == 1 ? 1 : 0;
        if(s.take > 0.002f && spec.upper[hs] >= 0 && spec.fore[hs] >= 0 && spec.chest >= 0)
        {
            // (its hand reaching for the weapon lying, then taking it up: the pelvis bends as far as the hand needs)
            const ragdoll::Bone& u = rig->bones[spec.upper[hs]];
            const ragdoll::Bone& f = rig->bones[spec.fore[hs]];
            take.reach = true;
            take.target = hs ? s.left : s.right;
            take.shoulder = u.pivot;
            take.chest = rig->bones[spec.chest].pivot;
            take.armLen = glm::length(f.pivot - u.pivot) + glm::length(f.end - f.pivot);
            take.torso = s.torso;
        }
        if(aglegs::solve(e, hdr, *rig, spec.kind, posed, take, origin, yawDeg, za::max(ENTSCALE_DECODE(e->scale), 0.01f),
               legs))
        {
            for(int i = 0; i < rig->numBones; i++)
            {
                posed[i] = composeRigid(legs[i], posed[i]);
            }
        }
        // The strafe lean (vr_agrunt_strafe_lean): moving sideways, its pelvis and legs tipped toward the way it goes,
        // about the floor under it, on every bone; the upper body's hold below keeps the chest aimed on the hips.
        rigpose::Rigid leanTurn;
        if(aglegs::lean(e, *rig, origin, yawDeg, s.take > 0.002f, leanTurn))
        {
            const float before = leanHips >= 0 ? rollOf(posed[leanHips]) : 0.f;
            const float beforeYaw = leanHips >= 0 ? yawOf(posed[leanHips]) : 0.f;
            for(int i = 0; i < rig->numBones; i++)
            {
                posed[i] = composeRigid(leanTurn, posed[i]);
            }
            leanPelvis = leanHips >= 0 ? rollOf(posed[leanHips]) - before : 0.f;
            leanPelvisYaw = leanHips >= 0 ? std::remainder(yawOf(posed[leanHips]) - beforeYaw, 360.f) : 0.f;
        }
    }
    // The procedural upper body (vr_agrunt_upper: the chest held on the hips, not the frames'; a pickup's crouch keeps
    // the frames'): laid under everything else, drawn too (boneTurns); the targets placed from the chest's anchor move
    // with it.
    rigpose::Rigid upperCorr;
    const bool pickup = (s.flags & apReach) || ((s.flags & apBody) && (s.flags & apFixed));
    const bool uppered = agbody::upper(s.ent, *rig, spec.chest, posed, pickup, upperCorr, aglegs::twistNow(e));
    if(uppered)
    {
        layOnChest(*rig, spec.chest, upperCorr, posed);
        if(layered)
        {
            // (the Rig Studio's torso keys on the held chest: its turn on the hips as keyed, not taken out with the
            // frames')
            const rigpose::Rigid keyed = composeRigid(posed[spec.chest], layerTorso);
            layOnChest(*rig, spec.chest, composeRigid(keyed, inverseRigid(posed[spec.chest])), posed);
        }
    }
    aglegs::leanMeasured(e, leanPelvis, spec.chest >= 0 ? rollOf(posed[spec.chest]) : 0.f, leanPelvisYaw,
        spec.chest >= 0 ? std::remainder(yawOf(posed[spec.chest]) - yawOf(rigpose::key(*rig, 0, spec.chest)), 360.f) : 0.f);
    const glm::vec3 anchorShift = uppered ? rigpose::apply(upperCorr, s.anchorAt) - s.anchorAt : glm::vec3{0.f};
    // A strike's body (bodyTurn: the torso, hips and head turned as QC turned the hands' targets) under the arms; the
    // same turns drawn on the bones (boneTurns).
    rigpose::Rigid body[ragdoll::maxBones];
    const bool bodied = bodyTurn(*rig, posed, s, body);
    for(int i = 0; i < rig->numBones && bodied; i++)
    {
        posed[i] = composeRigid(body[i], posed[i]);
    }
    rigpose::Rigid kick[ragdoll::maxBones];
    const bool kicked = vr_hit_react.value == 1.f && hitreact::reacting(e) && hitreact::boneKicks(e, *rig, posed, kick);
    const rigpose::Rigid chestKick = kicked ? kick[spec.chest] : rigpose::Rigid{};
    const int kind = spec.kind;
    agarms::Targets t;
    const bool hasR = heldProp(s.prop);
    const int slot = hasR ? weapons::slotForModel(cl_entities[s.prop].model) : -1;
    if(hasR)
    {
        t.goal[0] = agarms::Goal::Frame;
        t.weapon[0].rot = frameRot(s.fw, s.up);
        t.weapon[0].pos = s.right;
        t.offset[0] = gripRigid(kind, slot, aggrip::Main);
    }
    else if(s.flags & apReach)
    {
        // (reaching for a weapon lying: its grip frame onto the weapon's handle, nothing drawn on it yet)
        t.goal[0] = agarms::Goal::Frame;
        t.weapon[0].rot = frameRot(s.fw, s.up);
        t.weapon[0].pos = s.right;
        t.offset[0] = gripRigid(kind, -1, aggrip::Main);
        t.reaching = true;
    }
    else
    {
        t.goal[0] = agarms::Goal::Point; // (its fists: a guard, a punch)
        t.weapon[0].pos = s.right;
    }
    int slotL = -1;
    glm::vec3 secondPoint{0.f}; // (the second grip's point, the weapon's frame: vr_agholds_report)
    if((s.flags & apLeftWeapon) && heldProp(s.propL))
    {
        slotL = weapons::slotForModel(cl_entities[s.propL].model);
        t.goal[1] = agarms::Goal::Frame;
        t.weapon[1].rot = frameRot(s.fwL, s.upL);
        t.weapon[1].pos = s.left;
        t.offset[1] = gripRigid(kind, slotL, aggrip::Off);
    }
    else if(s.flags & apLeft)
    {
        t.goal[1] = agarms::Goal::Point;
        t.weapon[1].pos = s.left;
    }
    else if(!(s.flags & apLeftFree) && hasR)
    {
        // A two-handed weapon's second grip (view::secondGrip: its first Grip hotspot, or a front handle, the Widowmaker's;
        // in the weapon's frame), or a Cup under the right hand. QC's one-handed hold (the Weapon Holds) sends apLeftFree;
        // its two-handed one apTwo: a weapon with neither cupped.
        int type = 0;
        if(view::secondGrip(cl_entities[s.prop].model, secondPoint, type) && type == 1)
        {
            t.secondAt = composeRigid(rigpose::Rigid{glm::quat{1.f, 0.f, 0.f, 0.f}, secondPoint}, gripRigid(kind, slot, aggrip::Second));
            t.second = true;
        }
        else if(type == 2 && (s.keyGun || !lowCarry(t)))
        {
            // (vr_agarms_arm_clear: a pistol carried low, pointing down, in one hand: the other cupping it there reached
            // across the body, its upper arm through the torso; unless its pose key chose On the Gun)
            t.cup = true;
        }
        if(!t.second && !t.cup && (s.flags & apTwo) && (s.keyGun || !lowCarry(t)))
        {
            t.cup = true; // (two-handed and no second grip: the off hand under and round the right on its handle)
        }
        t.keyGun = s.keyGun; // (its second grip out of reach: cupping the right hand instead, vr_agarms)
    }
    if(s.offKeyed && hasR && !(s.flags & (apLeft | apLeftWeapon)))
    {
        // The pose keys' off hand: its grip on the weapon moved and turned (the weapon's frame), the free hand's
        const auto rotOf = [](const glm::vec3& r) {
            const float a = glm::length(r);
            return a < 1e-4f ? glm::quat{1.f, 0.f, 0.f, 0.f} : glm::angleAxis(glm::radians(a), r / a);
        };
        t.offKeyed = true;
        t.gripOff = rigpose::Rigid{rotOf(s.gripTurn), s.gripPos};
        t.freeOff = rigpose::Rigid{rotOf(s.freeTurn), s.freePos};
    }
    t.stretch = s.stretch;
    if(uppered)
    {
        t.weapon[0].pos += anchorShift * s.wr;
        t.weapon[1].pos += anchorShift * s.wl;
    }
    if(bodied && !(s.flags & apFixed))
    {
        // (QC turned the targets about its guess of the waist, `s.waist`; the chest turns about its own joint: the
        // targets moved by the difference, a shift, so the arms ride the chest exactly. apFixed: a take's bend, the
        // targets where the lying weapon is: not moved)
        const rigpose::Rigid& c = body[spec.chest];
        const glm::vec3 shift = c.pos + c.rot * s.waist - s.waist;
        for(int side = 0; side < 2; side++)
        {
            t.weapon[side].pos += shift;
        }
    }
    for(int side = 0; side < 2 && kicked; side++)
    {
        t.weapon[side] = composeRigid(chestKick, t.weapon[side]); // (the torso's flinch carries the weapons)
    }
    if(bodied || (s.flags & apBody))
    {
        // (its elbows' poles in a strike: raised and drawn back in the wind-up, on its turned chest; the Pose Studio's
        // keys may send them with no turn: vr_agpose.hpp)
        const glm::quat cr = bodied ? body[spec.chest].rot : glm::quat{1.f, 0.f, 0.f, 0.f};
        t.pole[0] = cr * s.elbowR;
        t.pole[1] = cr * s.elbowL;
    }
    // (a target that is no place on its body: none solved, its weapons left where the server has them, never drawn far
    // off, at NaN or inside it: the note of 2026-10-24 03:11:45's invisible sword)
    for(int side = 0; side < 2; side++)
    {
        if(t.goal[side] != agarms::Goal::None && !placeable(t.weapon[side]))
        {
            return;
        }
    }
    posestudio::adjust(s.ent, t); // (the Pose Studio's drag: a weapon held at the controller)
    if(layerArms)
    {
        rigseq::adjustTargets(*rig, spec, posed, kicked ? kick : nullptr, t); // (the Rig Studio's arms: the hands on their bones)
    }
    agarms::Placed placed;
    if(!agarms::solve(e, hdr, *rig, spec, posed, kicked ? kick : nullptr, t, placed))
    {
        return;
    }
    if(rig->authored)
    {
        // drawn on the authored rig, its arms laid on its arm bones within their limits, its weapons carried
        // by its hands as drawn (vr_rig2_game.cpp)
        rig2::liveArmed(e, *rig, posed, kicked ? kick : nullptr, placed, pose1, pose2, blend, layerArms);
    }
    if(ArmPose* seen = armPoseOf(s.ent, false))
    {
        seen->seenFrame = host_framecount;
        seen->seenKind = kind;
        seen->seenSlot = slot;
        seen->seenLeft = t.second ? 1 : t.cup ? 2 : t.goal[1] == agarms::Goal::Frame ? 4 : t.goal[1] == agarms::Goal::Point ? 3 : 0;
        seen->seenHeld2 = placed.held2;
        seen->seenSecond = secondPoint;
    }
    if(posestudio::active() || rigstudio::wantsNotes())
    {
        posestudio::Note n;
        n.ent = s.ent;
        n.kind = kind;
        n.slot = slot;
        n.slotL = slotL;
        n.prop = hasR ? s.prop : 0;
        n.propL = placed.on[1] ? s.propL : 0;
        n.pose = (s.flags & apReach)                          ? "pickup (reaching)"
                 : (s.flags & apBody) && (s.flags & apFixed) ? "pickup (the bend)"
                 : (s.flags & apBody)                        ? "a strike"
                 : !hasR                                     ? "fists"
                 : vr_agrunt_freeze.value != 0.f             ? "idle, weapon ready (frozen)"
                                                             : "live";
        n.space.origin = drawnOrigin(e, &n.space.yawDeg);
        n.space.scale = za::max(ENTSCALE_DECODE(e->scale), 0.01f);
        n.t = t;
        n.placed = placed;
        n.bodyFlag = (s.flags & apBody) != 0;
        n.torso = n.bodyFlag ? s.torso : glm::vec3{0.f};
        n.elbowPole[0] = n.bodyFlag ? s.elbowR : glm::vec3{0.f};
        n.elbowPole[1] = n.bodyFlag ? s.elbowL : glm::vec3{0.f};
        n.waist = s.waist;
        if(posestudio::active())
        {
            posestudio::note(n);
        }
        rigstudio::note(n); // (the Rig Studio's display: its subject's IK targets and grips)
    }
    if(placed.on[0] && hasR && placeable(placed.weapon[0]))
    {
        placeProp(cl_entities[s.prop], e, placed.weapon[0], nullptr);
    }
    if(placed.on[1] && placeable(placed.weapon[1]))
    {
        placeProp(cl_entities[s.propL], e, placed.weapon[1], nullptr);
    }
    if(vr_agrunt_ik_debug.value >= 2.f && hasR)
    {
        const entity_t& p = cl_entities[s.prop];
        Con_Printf("agarms ent %d prop %d: want %.1f %.1f %.1f, placed %d %.1f %.1f %.1f, drawn at %.0f %.0f %.0f (it %.0f %.0f %.0f), "
                   "model %s\n",
            s.ent, s.prop, t.weapon[0].pos.x, t.weapon[0].pos.y, t.weapon[0].pos.z, placed.on[0] ? 1 : 0, placed.weapon[0].pos.x,
            placed.weapon[0].pos.y, placed.weapon[0].pos.z, p.origin[0], p.origin[1], p.origin[2], e->origin[0], e->origin[1],
            e->origin[2], p.model ? p.model->name : "none");
    }
    // (vr_agrunt_ik_err: how far its held weapons are drawn from it, most)
    for(const int num : {hasR ? s.prop : 0, placed.on[1] ? s.propL : 0})
    {
        if(num > 0)
        {
            const entity_t& p = cl_entities[num];
            ragErr.propFar = za::max(ragErr.propFar, glm::length(geom::vec(p.origin) - drawnOrigin(e, nullptr)));
        }
    }
    aggrip::noteSubject(kind, slot, slotL, viewDistance(e));
}

} // namespace hitreact_detail
