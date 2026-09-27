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

constexpr const char* fingerNames[FingerCount] = {"thumb", "index", "middle", "ring", "pinky"};

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

[[nodiscard]] glm::quat turnAt(int finger, int joint, int frame)
{
    const float* q = data::turns[finger][frame][joint];
    return glm::quat{q[3], q[0], q[1], q[2]};
}

// Where a curl falls between the frames.
struct Between
{
    int a, b;
    float t;
};

[[nodiscard]] Between between(float curl)
{
    const float c = std::fmin(std::fmax(curl, 0.f), static_cast<float>(data::numFrames - 1));
    const int a = std::min(static_cast<int>(c), data::numFrames - 2);
    return {a, a + 1, c - static_cast<float>(a)};
}

[[nodiscard]] glm::quat turn(int finger, int joint, float curl)
{
    const Between w = between(curl);
    return glm::slerp(turnAt(finger, joint, w.a), turnAt(finger, joint, w.b), w.t);
}

[[nodiscard]] glm::vec3 localAt(int v, float curl)
{
    const Between w = between(curl);
    const float* a = data::vertices[v].local[w.a];
    const float* b = data::vertices[v].local[w.b];
    return glm::mix(vec(a), vec(b), w.t);
}

// The least turn taking unit vector `from` to `to`.
[[nodiscard]] glm::mat3 arc(const glm::vec3& from, const glm::vec3& to)
{
    const float d = glm::dot(from, to);
    if(d > 0.99999f)
    {
        return glm::mat3{1.f};
    }
    if(d < -0.99999f)
    {
        const glm::vec3 side = std::fabs(from.x) < 0.9f ? glm::vec3{1.f, 0.f, 0.f} : glm::vec3{0.f, 1.f, 0.f};
        return glm::mat3_cast(glm::angleAxis(glm::pi<float>(), glm::normalize(glm::cross(from, side))));
    }
    const glm::vec3 c = glm::cross(from, to);
    return glm::mat3_cast(glm::normalize(glm::quat{1.f + d, c.x, c.y, c.z}));
}

[[nodiscard]] glm::vec3 normalAt(int v, float curl)
{
    const Between w = between(curl);
    return glm::mix(vec(data::vertices[v].normal[w.a]), vec(data::vertices[v].normal[w.b]), w.t);
}

void segments(const Pose& p, int finger, const float curls[jointsPerFinger], Rigid out[jointsPerFinger + 1])
{
    out[0] = {glm::mat3{1.f}, p.shift[finger]};
    for(int k = 0; k < jointsPerFinger; k++)
    {
        glm::quat q = turn(finger, k, curls[k]);
        if(finger == Thumb && k == 0)
        {
            q = p.metacarpal * q;
        }
        const glm::mat3 r = glm::mat3_cast(glm::normalize(q));
        const glm::vec3 pivot = vec(data::pivots[finger][k]);
        out[k + 1] = out[k] * Rigid{r, pivot - r * pivot};
    }
}

void vertices(const Pose& p, int finger, const float curls[jointsPerFinger], Posed& out)
{
    (void)p;
    for(int v = data::firstVertex[finger]; v < data::firstVertex[finger + 1]; v++)
    {
        const data::Vertex& dv = data::vertices[v];
        const int joint = dv.bone > 0 ? dv.bone - 1 : 0;
        out.vertex[v] = out.segment[finger][dv.bone](localAt(v, curls[joint]));
        out.normal[v] = normalAt(v, curls[joint]);
    }
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
        segments(p, f, p.curl[f], out.segment[f]);
        vertices(p, f, p.curl[f], out);
    }
}

void poseFinger(const Pose& p, int finger, const float curls[jointsPerFinger], Posed& out)
{
    segments(p, finger, curls, out.segment[finger]);
    vertices(p, finger, curls, out);
}

void fingerSegments(const Pose& p, int finger, const float curls[jointsPerFinger], Rigid out[jointsPerFinger + 1])
{
    segments(p, finger, curls, out);
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
    const glm::vec3 p = vec(pv.pos);
    return pv.thumb > 0.f ? glm::mix(p, posed.segment[Thumb][1](p), pv.thumb) : p;
}

void skin(const Posed& posed, float out[data::numJoints * 12])
{
    const auto write = [&](int joint, const glm::mat3& r, const glm::vec3& t) {
        float* m = out + joint * 12;
        for(int row = 0; row < 3; row++)
        {
            m[row * 4 + 0] = r[0][row];
            m[row * 4 + 1] = r[1][row];
            m[row * 4 + 2] = r[2][row];
            m[row * 4 + 3] = t[row];
        }
    };

    write(0, glm::mat3{1.f}, glm::vec3{0.f});
    write(1, posed.segment[Thumb][1].r, posed.segment[Thumb][1].t);

    // Each vertex at its place, its normal turned from the one the engine computed for the mesh to the models'
    // own (as the six models were lit), turned with its segment.
    const glm::quat thumbTurn = glm::quat_cast(posed.segment[Thumb][1].r);
    for(int i = 0; i < data::numPalmVertices; i++)
    {
        const data::PalmVertex& pv = data::palmVertices[i];
        glm::vec3 n = vec(pv.normal);
        if(pv.thumb > 0.f)
        {
            n = glm::mat3_cast(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, thumbTurn, pv.thumb)) * n;
        }
        const glm::mat3 r = arc(vec(pv.bindNormal), glm::normalize(n));
        write(pv.joint, r, palmVertex(posed, i) - r * vec(pv.pos));
    }
    for(int v = 0; v < data::numVertices; v++)
    {
        const data::Vertex& dv = data::vertices[v];
        const glm::mat3 r = arc(vec(dv.bindNormal), glm::normalize(posed.segment[dv.finger][dv.bone].r * posed.normal[v]));
        write(dv.joint, r, posed.vertex[v] - r * vec(dv.local[0]));
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
    if(strcmp(bones[0].name, "palm") != 0 || strcmp(bones[1].name, "thumb_1") != 0)
    {
        return false;
    }
    for(int i = 0; i < data::numPalmVertices; i++)
    {
        char name[32];
        q_snprintf(name, sizeof(name), "palm_v%02d", i);
        if(strcmp(bones[data::palmVertices[i].joint].name, name) != 0)
        {
            return false;
        }
    }
    for(int v = 0; v < data::numVertices; v++)
    {
        const data::Vertex& dv = data::vertices[v];
        char name[32];
        q_snprintf(name, sizeof(name), "%s_v%02d", fingerNames[dv.finger], v - data::firstVertex[dv.finger]);
        if(strcmp(bones[dv.joint].name, name) != 0)
        {
            Con_DPrintf("%s: joint %d is \"%s\", not \"%s\"\n", model->name, dv.joint, bones[dv.joint].name, name);
            return false;
        }
    }
    checked.usable = true;
    return true;
}

} // namespace qvr::handrig
