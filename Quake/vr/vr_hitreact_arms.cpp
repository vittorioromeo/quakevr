// vr_hitreact_arms.cpp -- the armed monsters' arm poses (QVR_SVC_ARMPOSE) placed on their drawn bodies: the slots,
// the drawn poses, the targets and the weapon's turn, the strike's body turn, the fit of a rigid motion, an armed rig's
// spec for vr_agarms. Moved out of vr_hitreact.cpp (see vr_hitreact.hpp); what they share is vr_hitreact_internal.hpp.

#include "vr_hitreact_internal.hpp"

namespace hitreact_detail
{

ArmPoses armPoses;

} // namespace hitreact_detail

namespace hitreact_detail
{

[[nodiscard]] ArmPose* armPoseOf(int ent, bool make)
{
    ArmPose* free = nullptr;
    for(ArmPose& s : armPoses.slots)
    {
        if(s.ent == ent)
        {
            return &s;
        }
        if(!free && (s.ent == 0 || s.cur.t < cl.mtime[0] - armPoseStale || s.cur.t > cl.mtime[0] + 1.0))
        {
            free = &s;
        }
    }
    if(!make || !free)
    {
        return nullptr;
    }
    *free = ArmPose{};
    free->ent = ent;
    return free;
}
} // namespace hitreact_detail

namespace
{

[[nodiscard]] glm::vec3 nlerp(const glm::vec3& a, const glm::vec3& b, float t)
{
    const glm::vec3 m = a + (b - a) * t;
    return glm::length(m) > 1e-4f ? glm::normalize(m) : b;
}
} // namespace

namespace hitreact_detail
{

// The poses it is drawn between now: R_SetupAliasFrame's lerp read without changing it (a frame begun this frame: the
// pose it leaves, as it will be drawn).
void drawnPoses(const entity_t* e, const aliashdr_t* hdr, int& pose1, int& pose2, float& blend)
{
    const int frame = (e->frame >= 0 && e->frame < hdr->numframes) ? e->frame : 0;
    int posenum = hdr->frames[frame].firstpose;
    if(hdr->frames[frame].numposes > 1)
    {
        posenum += static_cast<int>(cl.time / hdr->frames[frame].interval) % hdr->frames[frame].numposes;
    }
    const bool valid = e->currentpose >= 0 && e->currentpose < hdr->numposes && e->previouspose >= 0 &&
                       e->previouspose < hdr->numposes;
    if(!valid || e->currentpose != posenum || !r_lerpmodels.value)
    {
        pose1 = pose2 = valid ? e->currentpose : posenum;
        blend = 0.f;
        return;
    }
    pose1 = e->previouspose;
    pose2 = e->currentpose;
    blend = za::clamp(static_cast<float>(cl.time - e->lerpstart) / za::max(e->lerptime, 1e-3f), 0.f, 1.f);
}

// The entity angles (a model's pitch) that turn a weapon model's forward to `fw` and its up to `up` (QC VR_AxeStick_Angles).
[[nodiscard]] glm::vec3 weaponAngles(const glm::vec3& fw, const glm::vec3& up)
{
    vec3_t a, f, r, u;
    a[YAW] = za::atan2(fw.y, fw.x) * (180.f / 3.14159265f);
    a[PITCH] = -za::atan2(fw.z, za::sqrt(fw.x * fw.x + fw.y * fw.y)) * (180.f / 3.14159265f);
    a[ROLL] = 0.f;
    AngleVectors(a, f, r, u);
    const glm::vec3 u0{u[0], u[1], u[2]};
    const float roll = za::atan2(glm::dot(glm::cross(u0, up), fw), glm::dot(u0, up)) * (180.f / 3.14159265f);
    a[ROLL] = roll;
    AngleVectors(a, f, r, u);
    if(glm::dot(geom::vec(u), up) < 0.999f)
    {
        a[ROLL] = -roll;
    }
    return {-a[PITCH], a[YAW], a[ROLL]};
}

[[nodiscard]] glm::vec3 turnYaw(float yawDeg, const glm::vec3& d)
{
    const float yaw = yawDeg * (3.14159265f / 180.f);
    const float c = za::cos(yaw), s = za::sin(yaw);
    return {c * d.x - s * d.y, s * d.x + c * d.y, d.z};
}

// The turn `corr` laid on every bone on rig's `chest` (the chest too; its loose pieces not), on the left (rest -> now).
void layOnChest(const ragdoll::Rig& rig, int chest, const rigpose::Rigid& corr, rigpose::Rigid* bones)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        bool onChest = b == chest;
        for(int p = rig.bones[b].parent, n = 0; p >= 0 && n < ragdoll::maxBones && !onChest; p = rig.bones[p].parent, n++)
        {
            onChest = p == chest;
        }
        if(onChest && rig.bones[b].joint != ragdoll::Joint::Loose)
        {
            bones[b] = composeRigid(corr, bones[b]);
        }
    }
}

// Its arm pose's targets now (`s` fresh), on the poses drawn: s.right, s.left, s.fw, s.up. False: none now.
bool placeArmPose(const entity_t* e, const aliashdr_t* hdr, int pose1, int pose2, float blend, ArmPose& s)
{
    const ArmedRig* a = armedRigOf(e->model);
    if(!a || hdr->numverts != a->numVerts || s.cur.t < cl.mtime[0] - armPoseStale)
    {
        return false;
    }
    // Between the last two messages, as the entities' origins are (CL_RelinkEntities' lerp).
    ArmPoseMsg m = s.cur;
    if(s.prev.t > 0.0 && s.cur.t > s.prev.t && s.prev.prop == s.cur.prop && (s.prev.flags == s.cur.flags))
    {
        const float f = za::clamp(static_cast<float>((cl.time - s.prev.t) / (s.cur.t - s.prev.t)), 0.f, 1.f);
        const ArmPoseMsg& p = s.prev;
        m.wr = p.wr + (m.wr - p.wr) * f;
        m.v = p.v + (m.v - p.v) * f;
        m.fwB = nlerp(p.fwB, m.fwB, f);
        m.upB = nlerp(p.upB, m.upB, f);
        m.fwG = nlerp(p.fwG, m.fwG, f);
        m.upG = nlerp(p.upG, m.upG, f);
        m.wo = p.wo + (m.wo - p.wo) * f;
        m.wl = p.wl + (m.wl - p.wl) * f;
        m.vl = p.vl + (m.vl - p.vl) * f;
        if(p.propL == m.propL)
        {
            m.fwL = nlerp(p.fwL, m.fwL, f);
            m.upL = nlerp(p.upL, m.upL, f);
        }
        m.torso = p.torso + (m.torso - p.torso) * f;
        m.elbowR = p.elbowR + (m.elbowR - p.elbowR) * f;
        m.elbowL = p.elbowL + (m.elbowL - p.elbowL) * f;
        m.take = p.take + (m.take - p.take) * f;
        m.gripPos = p.gripPos + (m.gripPos - p.gripPos) * f; // (zero in a message with none)
        m.gripTurn = p.gripTurn + (m.gripTurn - p.gripTurn) * f;
        m.freePos = p.freePos + (m.freePos - p.freePos) * f;
        m.freeTurn = p.freeTurn + (m.freeTurn - p.freeTurn) * f;
        m.offKeyed = m.offKeyed || p.offKeyed;
        m.stretch = p.stretch + (m.stretch - p.stretch) * f;
    }
    const float scale = za::max(ENTSCALE_DECODE(e->scale), 0.01f);
    glm::vec3 hr{0.f};
    for(const int v : a->rightHand)
    {
        hr += mdldequant::modelVertex(hdr, pose1, pose2, blend, v) / static_cast<float>(za::getArraySize(a->rightHand));
    }
    // Its drawn rifle's line: the carry's turn.
    const glm::vec3 rear = (mdldequant::modelVertex(hdr, pose1, pose2, blend, a->gunRear[0]) + mdldequant::modelVertex(hdr, pose1, pose2, blend, a->gunRear[1])) * 0.5f;
    const glm::vec3 muzzle =
        (mdldequant::modelVertex(hdr, pose1, pose2, blend, a->gunMuzzle[0]) + mdldequant::modelVertex(hdr, pose1, pose2, blend, a->gunMuzzle[1])) * 0.5f;
    glm::vec3 gx = muzzle - rear;
    gx = glm::length(gx) > 1e-4f ? glm::normalize(gx) : glm::vec3{1.f, 0.f, 0.f};
    glm::vec3 gz = mdldequant::modelVertex(hdr, pose1, pose2, blend, a->gunTop) - mdldequant::modelVertex(hdr, pose1, pose2, blend, a->gunBottom);
    gz -= gx * glm::dot(gz, gx);
    gz = glm::length(gz) > 1e-4f ? glm::normalize(gz) : glm::vec3{0.f, 0.f, 1.f};
    const glm::vec3 gy = glm::cross(gz, gx);
    const glm::vec3 fwG = gx * m.fwG.x + gy * m.fwG.y + gz * m.fwG.z;
    const glm::vec3 upG = gx * m.upG.x + gy * m.upG.y + gz * m.upG.z;
    glm::vec3 fw = nlerp(m.fwB, fwG, m.wo);
    glm::vec3 up = nlerp(m.upB, upG, m.wo);
    up -= fw * glm::dot(up, fw);
    up = glm::length(up) > 1e-4f ? glm::normalize(up) : glm::vec3{0.f, 0.f, 1.f};
    // The animation's hand eased (a frame drawn unlerped, the shot's, moved it 7 units at once): the weapon and both
    // hands follow it without the jolt.
    const float dt = static_cast<float>(cl.time - s.handTime);
    s.hand = (s.handTime <= 0.0 || dt < 0.f || dt > 0.2f) ? hr : s.hand + (hr - s.hand) * (1.f - za::exp(-dt / handEase));
    s.handTime = cl.time;
    hr = s.hand;
    // vr_agarms: placed from its chest's anchor as drawn (the server placed them from it as it had it).
    const bool anchored = (m.flags & apAnchor) && a->anchor >= 0;
    const glm::vec3 anchorAt = anchored ? mdldequant::modelVertex(hdr, pose1, pose2, blend, a->anchor) : glm::vec3{0.f};
    if(anchored)
    {
        hr = anchorAt;
    }
    s.right = hr * m.wr + m.v / scale;
    s.fw = fw;
    s.up = up;
    s.prop = m.prop;
    s.flags = m.flags;
    s.propL = (m.flags & apLeftWeapon) ? m.propL : 0;
    s.fwL = m.fwL;
    s.upL = m.upL;
    s.left = m.vl / scale;
    s.torso = m.torso;
    s.elbowR = m.elbowR;
    s.elbowL = m.elbowL;
    s.waist = m.waist / scale;
    s.anchorAt = anchorAt;
    s.wr = anchored ? m.wr : 0.f;
    s.wl = anchored ? m.wl : 0.f;
    s.take = (m.flags & apFixed) ? m.take : 0.f;
    s.takeSide = m.takeSide;
    s.offKeyed = m.offKeyed;
    s.gripPos = m.gripPos;
    s.gripTurn = m.gripTurn;
    s.freePos = m.freePos / scale;
    s.freeTurn = m.freeTurn;
    s.stretch = m.stretch;
    s.keyGun = m.keyGun;
    if(anchored)
    {
        s.left += anchorAt * m.wl;
    }
    s.frame = host_framecount;
    return true;
}

// The arm pose of `e` placed this frame (relinkArms'), or none.
[[nodiscard]] const ArmPose* armPoseNow(const entity_t* e)
{
    const ArmPose* s = armPoseOf(client::entityNumber(e), false);
    return (s && s->frame == host_framecount) ? s : nullptr;
}

// A strike's body (QC vr_armedgrunt_pose.qc VR_AGP_TorsoNow; ROUND22.md, "Armed monsters: the body in their strikes"):
// its chest turned about its waist (its joint with the pelvis) by s.torso (yaw about z, pitch about y, roll about x: as
// QC turned the hands' targets and the weapons), its pelvis by vr_agrunt_strike_hips of the yaw (the legs left planted),
// its head turned back by vr_agrunt_strike_head (its eyes kept on its target), every other bone on the chest (the arms)
// carried. Out: every bone's turn on `posed` (none: unturned). False: no turn now.
bool bodyTurn(const ragdoll::Rig& rig, const rigpose::Rigid* posed, const ArmPose& s, rigpose::Rigid* out)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        out[b] = rigpose::Rigid{};
    }
    const glm::vec3 t = s.torso * (3.14159265f / 180.f);
    if(glm::dot(t, t) < 1e-8f)
    {
        return false;
    }
    const int chest = chestOf(rig);
    // (the derived rigs' chest on the pelvis; the authored rig's on its spine on the pelvis: the turn at the
    // waist, the spine's joint, carrying the chest)
    int waist = chest;
    while(rig.authored && waist >= 0 && rig.bones[waist].parent >= 0 && rig.bones[rig.bones[waist].parent].parent >= 0)
    {
        waist = rig.bones[waist].parent;
    }
    const int pelvis = waist >= 0 ? rig.bones[waist].parent : -1;
    if(pelvis < 0 || rig.bones[pelvis].parent >= 0)
    {
        return false;
    }
    const glm::vec3 X{1.f, 0.f, 0.f}, Y{0.f, 1.f, 0.f}, Z{0.f, 0.f, 1.f};
    const auto about = [](const glm::quat& r, const glm::vec3& j) {
        rigpose::Rigid k;
        k.rot = r;
        k.pos = j - r * j;
        return k;
    };
    const glm::quat q = glm::angleAxis(t.x, Z) * glm::angleAxis(t.y, Y) * glm::angleAxis(t.z, X);
    const glm::quat qp = glm::angleAxis(za::clamp(vr_agrunt_strike_hips.value, 0.f, 1.f) * t.x, Z);
    out[pelvis] = about(qp, rigpose::apply(posed[pelvis], rig.bones[pelvis].pivot));
    out[waist] = composeRigid(out[pelvis], about(glm::inverse(qp) * q, rigpose::apply(posed[waist], rig.bones[waist].pivot)));
    out[chest] = out[waist];
    const int head = rig.head >= 0 ? rig.head : boneNamed(rig, "head");
    const float hs = za::clamp(vr_agrunt_strike_head.value, 0.f, 1.f);
    for(int b = 0; b < rig.numBones; b++)
    {
        if(b == chest || b == pelvis || b == waist || rig.bones[b].joint == ragdoll::Joint::Loose)
        {
            continue;
        }
        bool onChest = false;
        for(int p = rig.bones[b].parent, n = 0; p >= 0 && n < ragdoll::maxBones; p = rig.bones[p].parent, n++)
        {
            onChest = onChest || p == chest;
        }
        if(!onChest)
        {
            continue; // (the legs: planted)
        }
        out[b] = out[chest];
        if(b == head && hs > 0.f)
        {
            const glm::quat back = glm::angleAxis(-hs * t.x, Z) * glm::angleAxis(-hs * t.y, Y);
            out[b] = composeRigid(out[chest], about(back, rigpose::apply(posed[b], rig.bones[b].pivot)));
        }
    }
    return true;
}

// The rigid motion taking points `a` nearest to `b` (Horn's quaternion: the largest eigenvector, by Jacobi).
[[nodiscard]] rigpose::Rigid fitRigid(const glm::vec3* a, const glm::vec3* b, int n)
{
    rigpose::Rigid r;
    if(n <= 0)
    {
        return r;
    }
    glm::dvec3 ca{0.0}, cb{0.0};
    for(int i = 0; i < n; i++)
    {
        ca += glm::dvec3{a[i]};
        cb += glm::dvec3{b[i]};
    }
    ca /= static_cast<double>(n);
    cb /= static_cast<double>(n);
    double S[3][3] = {};
    for(int i = 0; i < n; i++)
    {
        const glm::dvec3 da = glm::dvec3{a[i]} - ca, db = glm::dvec3{b[i]} - cb;
        for(int x = 0; x < 3; x++)
        {
            for(int y = 0; y < 3; y++)
            {
                S[x][y] += da[x] * db[y];
            }
        }
    }
    double N[4][4] = {
        {S[0][0] + S[1][1] + S[2][2], S[1][2] - S[2][1], S[2][0] - S[0][2], S[0][1] - S[1][0]},
        {S[1][2] - S[2][1], S[0][0] - S[1][1] - S[2][2], S[0][1] + S[1][0], S[2][0] + S[0][2]},
        {S[2][0] - S[0][2], S[0][1] + S[1][0], -S[0][0] + S[1][1] - S[2][2], S[1][2] + S[2][1]},
        {S[0][1] - S[1][0], S[2][0] + S[0][2], S[1][2] + S[2][1], -S[0][0] - S[1][1] + S[2][2]}};
    double V[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    for(int sweep = 0; sweep < 30; sweep++)
    {
        double off = 0.0;
        for(int p = 0; p < 4; p++)
        {
            for(int q = p + 1; q < 4; q++)
            {
                off += N[p][q] * N[p][q];
            }
        }
        if(off < 1e-20)
        {
            break;
        }
        for(int p = 0; p < 4; p++)
        {
            for(int q = p + 1; q < 4; q++)
            {
                if(za::abs(N[p][q]) < 1e-18)
                {
                    continue;
                }
                const double th = (N[q][q] - N[p][p]) / (2.0 * N[p][q]);
                const double t = (th >= 0.0 ? 1.0 : -1.0) / (za::abs(th) + za::sqrt(th * th + 1.0));
                const double c = 1.0 / za::sqrt(t * t + 1.0), s = t * c;
                for(int k = 0; k < 4; k++)
                {
                    const double kp = N[k][p], kq = N[k][q];
                    N[k][p] = c * kp - s * kq;
                    N[k][q] = s * kp + c * kq;
                }
                for(int k = 0; k < 4; k++)
                {
                    const double pk = N[p][k], qk = N[q][k];
                    N[p][k] = c * pk - s * qk;
                    N[q][k] = s * pk + c * qk;
                }
                for(int k = 0; k < 4; k++)
                {
                    const double kp = V[k][p], kq = V[k][q];
                    V[k][p] = c * kp - s * kq;
                    V[k][q] = s * kp + c * kq;
                }
            }
        }
    }
    int best = 0;
    for(int i = 1; i < 4; i++)
    {
        best = N[i][i] > N[best][best] ? i : best;
    }
    const glm::quat q{static_cast<float>(V[0][best]), static_cast<float>(V[1][best]), static_cast<float>(V[2][best]),
        static_cast<float>(V[3][best])};
    r.rot = glm::normalize(q);
    r.pos = glm::vec3{cb} - r.rot * glm::vec3{ca};
    return r;
}

[[nodiscard]] rigpose::Rigid inverseRigid(const rigpose::Rigid& a)
{
    rigpose::Rigid r;
    r.rot = glm::inverse(a.rot);
    r.pos = -(r.rot * a.pos);
    return r;
}

// A frame (x forward, z up) as a rotation.
[[nodiscard]] glm::quat frameRot(const glm::vec3& fw, const glm::vec3& upHint)
{
    const glm::vec3 x = glm::length(fw) > 1e-5f ? glm::normalize(fw) : glm::vec3{1.f, 0.f, 0.f};
    glm::vec3 z = upHint - x * glm::dot(upHint, x);
    z = glm::length(z) > 1e-4f ? glm::normalize(z) : glm::normalize(glm::cross(x, glm::vec3{0.f, 1.f, 0.f}) + glm::vec3{0.f, 0.f, 1e-3f});
    const glm::vec3 y = glm::cross(z, x);
    return glm::normalize(glm::quat_cast(glm::mat3{x, y, z}));
}
} // namespace hitreact_detail

namespace
{

[[nodiscard]] bool isGunOf(const void* row, int v)
{
    return isGunVertex(*static_cast<const ArmedRig*>(row), v);
}
} // namespace

namespace hitreact_detail
{

// The skin's spec of an armed rig's model (vr_agarms): its arms' bones, its chest, its rest gun's turn (the hands' grips).
bool specOf(const ArmedRig& a, const ragdoll::Rig& rig, const aliashdr_t* hdr, agarms::Spec& s)
{
    if(hdr->numverts != a.numVerts || rig.numVerts != hdr->numverts)
    {
        return false;
    }
    s.kind = static_cast<int>(&a - armedRigs);
    s.upper[0] = boneNamed(rig, a.upperR);
    s.fore[0] = boneNamed(rig, a.foreR);
    s.upper[1] = boneNamed(rig, a.upperL);
    s.fore[1] = boneNamed(rig, a.foreL);
    s.chest = boneNamed(rig, "chest");
    s.chest = s.chest >= 0 ? s.chest : chestOf(rig);
    const glm::vec3 rear = (mdldequant::modelVertex(hdr, 0, 0, 0.f, a.gunRear[0]) + mdldequant::modelVertex(hdr, 0, 0, 0.f, a.gunRear[1])) * 0.5f;
    const glm::vec3 muzzle = (mdldequant::modelVertex(hdr, 0, 0, 0.f, a.gunMuzzle[0]) + mdldequant::modelVertex(hdr, 0, 0, 0.f, a.gunMuzzle[1])) * 0.5f;
    s.gunRot = frameRot(muzzle - rear, mdldequant::modelVertex(hdr, 0, 0, 0.f, a.gunTop) - mdldequant::modelVertex(hdr, 0, 0, 0.f, a.gunBottom));
    s.isGun = isGunOf;
    s.row = &a;
    s.torsoSides = a.torsoSides;
    s.twistShare = a.twistShare;
    s.wristMore = a.wristMore;
    return true;
}

} // namespace hitreact_detail
