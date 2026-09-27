// vr_handrig.cpp -- see vr_handrig.hpp.

#include "vr_handrig.hpp"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <cstring>
#include <string>

namespace qvr::handrig
{
namespace
{

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

[[nodiscard]] glm::quat turnAt(int finger, int joint, int frame)
{
    const float* q = data::turns[finger][frame][joint];
    return glm::quat{q[3], q[0], q[1], q[2]};
}

[[nodiscard]] glm::quat turn(int finger, int joint, float curl)
{
    const float c = std::fmin(std::fmax(curl, 0.f), static_cast<float>(data::numFrames - 1));
    const int a = std::min(static_cast<int>(c), data::numFrames - 2);
    return glm::slerp(turnAt(finger, joint, a), turnAt(finger, joint, a + 1), c - static_cast<float>(a));
}

// A turn about a joint's pivot.
[[nodiscard]] Rigid about(int finger, int joint, const glm::quat& q)
{
    const glm::mat3 r = glm::mat3_cast(q);
    const glm::vec3 pivot = vec(data::pivots[finger][joint]);
    return {r, pivot - r * pivot};
}

void segments(const Pose& p, int finger, const float curls[jointsPerFinger], Rigid out[jointsPerFinger + 1],
    glm::quat turns[jointsPerFinger])
{
    out[0] = {glm::mat3{1.f}, p.shift[finger]};
    for(int k = 0; k < jointsPerFinger; k++)
    {
        glm::quat q = turn(finger, k, curls[k]);
        if(finger == Thumb && k == 0)
        {
            q = p.metacarpal * q;
        }
        q = glm::normalize(q);
        turns[k] = q;
        out[k + 1] = out[k] * about(finger, k, q);
    }
}

// A vertex where its joints put it (the GPU's blend).
[[nodiscard]] glm::vec3 blend(const Posed& posed, int count, const short* joints, const float* weights, const float* pos)
{
    const glm::vec3 p = vec(pos);
    if(count == 1)
    {
        return posed.joint[joints[0]](p);
    }
    glm::vec3 out{0.f};
    for(int i = 0; i < count; i++)
    {
        out += weights[i] * posed.joint[joints[i]](p);
    }
    return out;
}

struct ModelCheck
{
    qmodel_t* model{nullptr};
    std::string name; // the model's (its slot is reused by another after a game change)
    bool usable{false};
};
ModelCheck checked;

} // namespace

void pose(const Pose& p, Posed& out)
{
    for(int f = 0; f < FingerCount; f++)
    {
        segments(p, f, p.curl[f], out.segment[f], out.turn[f]);
    }
    const glm::quat none{1.f, 0.f, 0.f, 0.f};
    for(int j = 0; j < data::numJoints; j++)
    {
        const data::Joint& d = data::joints[j];
        switch(d.kind)
        {
        case data::PalmJoint:
            out.joint[j] = Rigid{};
            break;
        case data::SegmentJoint:
            out.joint[j] = out.segment[d.finger][d.index];
            break;
        case data::PartJoint:
            // After the segment before the joint, the joint's turn by its share (slerped: the ring there keeps its
            // size however far the joint turns).
            out.joint[j] = out.segment[d.finger][d.index] *
                           about(d.finger, d.index, glm::normalize(glm::slerp(none, out.turn[d.finger][d.index], d.share)));
            break;
        }
    }
    for(int v = 0; v < data::numVertices; v++)
    {
        const data::Vertex& dv = data::vertices[v];
        out.vertex[v] = blend(out, dv.count, dv.joint, dv.weight, dv.pos);
    }
}

void fingerSegments(const Pose& p, int finger, const float curls[jointsPerFinger], Rigid out[jointsPerFinger + 1])
{
    glm::quat turns[jointsPerFinger];
    segments(p, finger, curls, out, turns);
}

float jointRate(int finger, int joint)
{
    float rate = 0.f;
    for(int f = 0; f + 1 < data::numFrames - 1; f++) // the closing path, frames 0..4
    {
        glm::quat d = glm::normalize(glm::inverse(turnAt(finger, joint, f)) * turnAt(finger, joint, f + 1));
        if(d.w < 0.f)
        {
            d = -d;
        }
        rate = std::fmax(rate, glm::angle(d));
    }
    return rate;
}

glm::vec3 palmVertex(const Posed& posed, int i)
{
    const data::PalmVertex& pv = data::palmVertices[i];
    return blend(posed, pv.count, pv.joint, pv.weight, pv.pos);
}

void skin(const Posed& posed, float out[data::numJoints * 12])
{
    for(int j = 0; j < data::numJoints; j++)
    {
        const Rigid& m = posed.joint[j];
        float* o = out + j * 12;
        for(int row = 0; row < 3; row++)
        {
            o[row * 4 + 0] = m.r[0][row];
            o[row * 4 + 1] = m.r[1][row];
            o[row * 4 + 2] = m.r[2][row];
            o[row * 4 + 3] = m.t[row];
        }
    }
}

void reset()
{
    checked = ModelCheck{};
}

bool usable(qmodel_t* model)
{
    if(model == checked.model && (!model || checked.name == model->name))
    {
        return checked.usable;
    }
    checked = ModelCheck{model, model ? model->name : "", false};
    if(!model || model->type != mod_alias || model->needload)
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    if(hdr->poseverttype != aliashdr_t::PV_IQM || hdr->numbones != data::numJoints)
    {
        Con_DPrintf("%s: not the jointed hand this engine expects (%d joints), the six hand models are drawn\n", model->name,
            data::numJoints);
        return false;
    }
    const auto* bones = reinterpret_cast<const boneinfo_t*>(reinterpret_cast<const byte*>(hdr) + hdr->boneinfo);
    for(int j = 0; j < data::numJoints; j++)
    {
        if(strcmp(bones[j].name, data::joints[j].name) != 0)
        {
            Con_DPrintf("%s: joint %d is \"%s\", not \"%s\"\n", model->name, j, bones[j].name, data::joints[j].name);
            return false;
        }
    }
    checked.usable = true;
    return true;
}

} // namespace qvr::handrig
