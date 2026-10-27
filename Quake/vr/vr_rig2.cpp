// vr_rig2.cpp -- the authored humanoid rig (vr_rig2.hpp): its file, its joints' math, its skinning, and its drawing
// as a skeletal model ("<model>#rig2") for the Rig Studio's subject. The checks: vr_rig2_check.cpp.

#include "vr_rig2.hpp"
#include "vr_api.h"
#include "vr_api_render.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_mdldequant.hpp"
#include "vr_rigdata.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::rig2
{

namespace
{

constexpr float pi = 3.14159265358979f;
constexpr float deg = pi / 180.f;
constexpr const char* skinnedSuffix = "#rig2";

struct Entry
{
    za::String model; // its name
    za::UniquePtr<Rig> rig; // null: no file
    bool tried{false};
};
za::Vector<Entry> rigs; // the rigs read this session

// A drawn subject (drawPosed), swapped while a frame draws (swapModels / restoreModels).
struct Drawn
{
    int num{0};
    const qmodel_t* model{nullptr};
    int frame{-1}; // host_framecount it was published at
    int bones{0};
    float skin[maxBones * 12]{};
    // swapped:
    qmodel_t* original{nullptr};
    qmodel_t* skinned{nullptr};
    short previousPose{0}, currentPose{0};
    float lerpStart{0.f}, lerpTime{0.f};
    byte lerpFlags{0};
    bool swapped{false};
    int drawsFrame{-1}, draws{0}, lastDraws{0}; // its bone matrices handed to the renderer (vr_rig2_draw_info)
    int studioFrame{-1};       // the Rig Studio's pose published at (drawPosed: the game's drawLive yields to it)
    rigpose::Rigid F[maxBones]; // the bones as published (drawnBone: a ragdoll made of it starts there)
};
za::Vector<Drawn> drawn;

// vr_rig2 in the game, read when a map is spawned (latch): -1 not read yet.
int gameLatched = -1;
int gameGenerations = 0; // the rigs' generations then (a rig's file changed: what is made from it made again)
// The generation each skinned model was made from (rebuilt when the rig's weights change: a reload)
struct Made
{
    qmodel_t* mod{nullptr};
    int generation{-1};
    double aoLookedAt{-1.0}; // its occlusion looked for (made before it was baked: once a second until it is)
};
za::Vector<Made> made;

// The skinned model's vertices shaded as the game draws its .mdl's rest pose (pose 0) through the alias renderer
// (gl_mesh.c, PV_QUAKE1): its dequantised place (vr_mdl_dequant's offset from the stored bytes), its smoothed normal
// (vr_mdl_dequant's, else the table's), its own baked occlusion (vr_ao.cpp; negated in the normal's 4th byte, the
// shaders' PoseAO; 0: none yet). The bones' blend moves the places and turns the normals (the alias shader).
// Returns whether its occlusion was there to fill.
bool shadeLikeMdl(qmodel_t* src, const aliashdr_t* sh, iqmvert_t* verts, int numVerts)
{
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(sh) + sh->meshdesc);
    const unsigned char* ao = VR_AliasVertexAO(src, sh); // [pose * numverts + vertex], 255 open
    mdldequant::prepare(src); // (its offsets now if not uploaded yet)
    for(int v = 0; v < numVerts; v++)
    {
        const int mv = desc[v].vertindex;
        iqmvert_t& o = verts[v];
        signed char n[3];
        mdldequant::restVertex(sh, v, o.xyz, n);
        for(int i = 0; i < 3; i++)
        {
            o.norm[i] = static_cast<int8_t>(n[i]);
        }
        o.norm[3] = ao ? static_cast<int8_t>(-za::clamp(static_cast<int>(static_cast<float>(ao[mv]) * (127.f / 255.f) + 0.5f), 1, 127)) : 0;
    }
    return ao != nullptr;
}

// A skinned model made before its .mdl's occlusion was baked (queued then): filled once it is, looked for once a second.
void catchUpAO(qmodel_t* sk, qmodel_t* src)
{
    auto* hdr = static_cast<aliashdr_t*>(Mod_Extradata(sk));
    auto* verts = hdr ? reinterpret_cast<iqmvert_t*>(reinterpret_cast<byte*>(hdr) + hdr->vertexes) : nullptr;
    const auto* sh = src ? static_cast<const aliashdr_t*>(Mod_Extradata(src)) : nullptr;
    if(!verts || !sh || hdr->numverts_vbo <= 0 || verts[0].norm[3] < 0)
    {
        return;
    }
    for(Made& m : made)
    {
        if(m.mod != sk || (realtime - m.aoLookedAt < 1.0 && realtime >= m.aoLookedAt))
        {
            continue;
        }
        m.aoLookedAt = realtime;
        if(VR_AliasVertexAO(src, sh) && shadeLikeMdl(src, sh, verts, hdr->numverts_vbo))
        {
            GLMesh_DeleteVertexBuffer(sk);
            GLMesh_LoadVertexBuffer(sk, hdr);
        }
    }
}

// ---- Words of a line ----
struct Words
{
    char buf[512]{};
    const char* w[64]{};
    int n{0};
    void split(const char* s, int len)
    {
        n = 0;
        const int l = za::min(len, static_cast<int>(sizeof(buf)) - 1);
        memcpy(buf, s, static_cast<size_t>(l));
        buf[l] = 0;
        char* hash = strchr(buf, '#');
        if(hash)
        {
            *hash = 0;
        }
        char* p = buf;
        while(*p && n < 64)
        {
            while(*p == ' ' || *p == '\t' || *p == '\r')
            {
                *p++ = 0;
            }
            if(!*p)
            {
                break;
            }
            w[n++] = p;
            while(*p && *p != ' ' && *p != '\t' && *p != '\r')
            {
                p++;
            }
        }
    }
    [[nodiscard]] bool is(int i, const char* s) const { return i < n && !strcmp(w[i], s); }
    [[nodiscard]] float f(int i) const { return i < n ? strtof(w[i], nullptr) : 0.f; }
    [[nodiscard]] glm::vec3 v3(int i) const { return glm::vec3{f(i), f(i + 1), f(i + 2)}; }
    [[nodiscard]] glm::vec2 v2(int i) const { return glm::vec2{f(i), f(i + 1)}; }
};
Words words; // (the parser's line: file scope, not a local static)

void num(za::String& out, float v)
{
    char t[32];
    q_snprintf(t, sizeof(t), " %.6g", za::abs(v) < 5e-7f ? 0.f : v);
    out += t;
}

void vec(za::String& out, const glm::vec3& v)
{
    num(out, v.x);
    num(out, v.y);
    num(out, v.z);
}

[[nodiscard]] glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback)
{
    const float l = glm::length(v);
    return l > 1e-6f ? v / l : fallback;
}

// A ball joint's frame: its swing's zero `n`, its swings' axes (turning n toward fwd, toward side).
struct Frame
{
    glm::vec3 n{0.f, 0.f, -1.f}, f{1.f, 0.f, 0.f}, s{0.f, 1.f, 0.f};
    glm::vec3 axF{0.f}, axS{0.f};
};

[[nodiscard]] Frame frameOf(const Bone& b)
{
    Frame fr;
    fr.n = safeNormalize(b.neutral, glm::vec3{0.f, 0.f, -1.f});
    const glm::vec3 helper = za::abs(fr.n.x) < 0.9f ? glm::vec3{1.f, 0.f, 0.f} : glm::vec3{0.f, 1.f, 0.f};
    fr.f = safeNormalize(b.fwd - fr.n * glm::dot(b.fwd, fr.n), glm::normalize(glm::cross(fr.n, helper)));
    fr.s = safeNormalize(b.side - fr.n * glm::dot(b.side, fr.n) - fr.f * glm::dot(b.side, fr.f), glm::cross(fr.n, fr.f));
    fr.axF = glm::cross(fr.n, fr.f);
    fr.axS = glm::cross(fr.n, fr.s);
    return fr;
}

[[nodiscard]] glm::quat ballTurn(const Frame& fr, const glm::vec3& a)
{
    return rigpose::fromVector((fr.axF * a.x + fr.axS * a.y) * deg) * glm::angleAxis(a.z * deg, fr.n);
}

[[nodiscard]] glm::vec3 parentDir(const Rig& rig, int b)
{
    const Bone& bone = rig.bones[b];
    if(bone.parent < 0)
    {
        return glm::vec3{0.f, 0.f, 1.f};
    }
    const Bone& p = rig.bones[bone.parent];
    return safeNormalize(bone.pivot - p.pivot, safeNormalize(p.end - p.pivot, glm::vec3{0.f, 0.f, 1.f}));
}

[[nodiscard]] glm::vec3 boneDir(const Bone& b)
{
    return safeNormalize(b.end - b.pivot, glm::vec3{0.f, 0.f, 1.f});
}

// A hinge's bend of direction `d` from its parent's `pd` about `axis` (degrees; both projected on its plane).
[[nodiscard]] float bendOf(const glm::vec3& axis, const glm::vec3& pd, const glm::vec3& d)
{
    const glm::vec3 a = safeNormalize(axis, glm::vec3{0.f, 1.f, 0.f});
    const glm::vec3 p = pd - a * glm::dot(pd, a);
    const glm::vec3 q = d - a * glm::dot(d, a);
    return za::atan2(glm::dot(glm::cross(p, q), a), glm::dot(p, q)) / deg;
}

[[nodiscard]] float rnd(uint32_t& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return static_cast<float>(s & 0xffffffu) / 16777216.f;
}

void filePath(const char* model, char* out, int size)
{
    char base[64], root[MAX_OSPATH];
    rigdata::baseName(model, base, sizeof(base));
    rigdata::rootDir(root, sizeof(root));
    q_snprintf(out, static_cast<size_t>(size), "%s/rigs/%s.rig2.txt", root, base);
}

[[nodiscard]] qmodel_t* skinnedOf(const qmodel_t* model)
{
    char name[MAX_QPATH];
    q_snprintf(name, sizeof(name), "%s%s", model->name, skinnedSuffix);
    return Mod_ForName(name, false);
}

} // namespace

// ---- The joints ----

int boneByName(const Rig& rig, const char* name)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        if(!q_strcasecmp(rig.bones[b].name, name))
        {
            return b;
        }
    }
    return -1;
}

glm::vec3 restAngles(const Rig& rig, int b)
{
    const Bone& bone = rig.bones[b];
    if(bone.joint == Joint::Ball)
    {
        const Frame fr = frameOf(bone);
        const glm::vec3 d = boneDir(bone);
        const glm::vec3 v = glm::cross(fr.n, d);
        const float s = glm::length(v);
        if(s < 1e-6f)
        {
            return glm::vec3{0.f};
        }
        const glm::vec3 r = v / s * za::atan2(s, glm::dot(fr.n, d));
        return glm::vec3{glm::dot(r, fr.axF) / deg, glm::dot(r, fr.axS) / deg, 0.f};
    }
    if(bone.joint == Joint::Hinge)
    {
        return glm::vec3{bendOf(bone.axis, parentDir(rig, b), boneDir(bone)), 0.f, 0.f};
    }
    return glm::vec3{0.f};
}

Pose restPose(const Rig& rig)
{
    Pose p;
    for(int b = 0; b < rig.numBones; b++)
    {
        p.a[b] = restAngles(rig, b);
    }
    return p;
}

glm::vec3 clampAngles(const Rig& rig, int b, const glm::vec3& in)
{
    const Bone& bone = rig.bones[b];
    glm::vec3 a = in;
    if(bone.joint == Joint::Ball)
    {
        const float lx = a.x >= 0.f ? bone.swingX.y : -bone.swingX.x;
        const float ly = a.y >= 0.f ? bone.swingY.y : -bone.swingY.x;
        a.x = lx <= 1e-3f ? 0.f : a.x;
        a.y = ly <= 1e-3f ? 0.f : a.y;
        const float ex = lx > 1e-3f ? a.x / lx : 0.f, ey = ly > 1e-3f ? a.y / ly : 0.f;
        const float e = ex * ex + ey * ey;
        if(e > 1.f)
        {
            const float k = 1.f / za::sqrt(e);
            a.x *= k;
            a.y *= k;
        }
        a.z = za::clamp(a.z, bone.twist.x, bone.twist.y);
        return a;
    }
    if(bone.joint == Joint::Hinge)
    {
        return glm::vec3{za::clamp(a.x, bone.range.x, bone.range.y), 0.f, 0.f};
    }
    return restAngles(rig, b);
}

bool withinLimits(const Rig& rig, int b, const glm::vec3& a, float slackDeg)
{
    const glm::vec3 c = clampAngles(rig, b, a);
    return glm::length(c - a) <= slackDeg;
}

Pose randomPose(const Rig& rig, uint32_t& seed)
{
    Pose p = restPose(rig);
    for(int b = 0; b < rig.numBones; b++)
    {
        const Bone& bone = rig.bones[b];
        if(bone.joint == Joint::Ball)
        {
            const float phi = rnd(seed) * 2.f * pi, r = za::sqrt(rnd(seed));
            const float cx = za::cos(phi), cy = za::sin(phi);
            p.a[b].x = r * cx * (cx >= 0.f ? bone.swingX.y : -bone.swingX.x);
            p.a[b].y = r * cy * (cy >= 0.f ? bone.swingY.y : -bone.swingY.x);
            p.a[b].z = bone.twist.x + (bone.twist.y - bone.twist.x) * rnd(seed);
        }
        else if(bone.joint == Joint::Hinge)
        {
            p.a[b].x = bone.range.x + (bone.range.y - bone.range.x) * rnd(seed);
        }
    }
    return p;
}

void forward(const Rig& rig, const Pose& pose, rigpose::Rigid* out)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        const Bone& bone = rig.bones[b];
        glm::quat R{1.f, 0.f, 0.f, 0.f};
        if(bone.joint == Joint::Ball)
        {
            const Frame fr = frameOf(bone);
            R = ballTurn(fr, pose.a[b]) * glm::inverse(ballTurn(fr, restAngles(rig, b)));
        }
        else if(bone.joint == Joint::Hinge)
        {
            R = glm::angleAxis((pose.a[b].x - restAngles(rig, b).x) * deg, safeNormalize(bone.axis, glm::vec3{0.f, 1.f, 0.f}));
        }
        rigpose::Rigid L;
        L.rot = R;
        L.pos = bone.pivot - R * bone.pivot;
        if(bone.parent >= 0 && bone.parent < b)
        {
            const rigpose::Rigid& P = out[bone.parent];
            out[b].rot = glm::normalize(P.rot * L.rot);
            out[b].pos = P.rot * L.pos + P.pos;
        }
        else
        {
            out[b] = L;
        }
    }
}

glm::vec3 aimAngles(const Rig& rig, int b, const rigpose::Rigid& Fparent, const glm::vec3& dir, const glm::vec3& current)
{
    const Bone& bone = rig.bones[b];
    const glm::vec3 d = safeNormalize(glm::inverse(Fparent.rot) * dir, boneDir(bone)); // (in the rest pose's space)
    if(bone.joint == Joint::Ball)
    {
        const Frame fr = frameOf(bone);
        const glm::vec3 v = glm::cross(fr.n, d);
        const float s = glm::length(v);
        glm::vec3 a{0.f, 0.f, current.z};
        if(s > 1e-6f)
        {
            const glm::vec3 r = v / s * za::atan2(s, glm::dot(fr.n, d));
            a.x = glm::dot(r, fr.axF) / deg;
            a.y = glm::dot(r, fr.axS) / deg;
        }
        return clampAngles(rig, b, a);
    }
    if(bone.joint == Joint::Hinge)
    {
        return clampAngles(rig, b, glm::vec3{bendOf(bone.axis, parentDir(rig, b), d), 0.f, 0.f});
    }
    return current;
}

glm::quat localOf(const Rig& rig, int b, const glm::vec3& angles)
{
    const Bone& bone = rig.bones[b];
    if(bone.joint == Joint::Ball)
    {
        const Frame fr = frameOf(bone);
        return glm::normalize(ballTurn(fr, angles) * glm::inverse(ballTurn(fr, restAngles(rig, b))));
    }
    if(bone.joint == Joint::Hinge)
    {
        return glm::angleAxis((angles.x - restAngles(rig, b).x) * deg, safeNormalize(bone.axis, glm::vec3{0.f, 1.f, 0.f}));
    }
    return glm::quat{1.f, 0.f, 0.f, 0.f};
}

glm::vec3 anglesOf(const Rig& rig, int b, const glm::quat& local)
{
    const Bone& bone = rig.bones[b];
    if(bone.joint == Joint::Ball)
    {
        // ballTurn(a) = local ballTurn(rest): its twist about n (the swing-twist split), its swing's rotation vector.
        const Frame fr = frameOf(bone);
        glm::quat q = glm::normalize(local * ballTurn(fr, restAngles(rig, b)));
        q = q.w < 0.f ? -q : q;
        const float tw = 2.f * za::atan2(glm::dot(glm::vec3{q.x, q.y, q.z}, fr.n), q.w);
        glm::quat swing = glm::normalize(q * glm::inverse(glm::angleAxis(tw, fr.n)));
        swing = swing.w < 0.f ? -swing : swing;
        const glm::vec3 im{swing.x, swing.y, swing.z};
        const float s = glm::length(im);
        const glm::vec3 r = s > 1e-7f ? im * (2.f * za::atan2(s, swing.w) / s) : im * 2.f;
        float twDeg = tw / deg;
        twDeg = twDeg > 180.f ? twDeg - 360.f : twDeg < -180.f ? twDeg + 360.f : twDeg;
        return glm::vec3{glm::dot(r, fr.axF) / deg, glm::dot(r, fr.axS) / deg, twDeg};
    }
    if(bone.joint == Joint::Hinge)
    {
        const glm::vec3 axis = safeNormalize(bone.axis, glm::vec3{0.f, 1.f, 0.f});
        glm::quat q = glm::normalize(local);
        q = q.w < 0.f ? -q : q;
        const float tw = 2.f * za::atan2(glm::dot(glm::vec3{q.x, q.y, q.z}, axis), q.w);
        return glm::vec3{restAngles(rig, b).x + tw / deg, 0.f, 0.f};
    }
    return glm::vec3{0.f};
}

PhysJoint physJoint(const Rig& rig, int b)
{
    PhysJoint j;
    const Bone& bone = rig.bones[b];
    if(bone.joint == Joint::Ball)
    {
        // Its zero's frame F0 (z the swing's zero n, x toward fwd); A: F0 turned to the swing's middle (the cone round
        // it, its axes toward fwd and side); B: F0 as the rest pose's bone carries it, less the twist's middle. Box3D's
        // B relative to A is then inverse(F0) inverse(T_mid) T(a) F0 rotZ(-twist mid): no swing nor twist at the middle.
        const Frame fr = frameOf(bone);
        const glm::mat3 m0{fr.f, glm::cross(fr.n, fr.f), fr.n};
        const glm::quat F0 = glm::normalize(glm::quat_cast(m0));
        const glm::vec3 mid{(bone.swingX.x + bone.swingX.y) * 0.5f, (bone.swingY.x + bone.swingY.y) * 0.5f, 0.f};
        const float twistMid = (bone.twist.x + bone.twist.y) * 0.5f;
        j.frameA = glm::normalize(ballTurn(fr, mid) * F0);
        const glm::vec3 rest = restAngles(rig, b);
        j.frameB = glm::normalize(ballTurn(fr, glm::vec3{rest.x, rest.y, 0.f}) * F0 * glm::angleAxis(-twistMid * deg, glm::vec3{0.f, 0.f, 1.f}));
        j.coneX = za::max(bone.swingX.y - bone.swingX.x, 0.f) * 0.5f * deg;
        j.coneY = za::max(bone.swingY.y - bone.swingY.x, 0.f) * 0.5f * deg;
        j.twist = za::max(bone.twist.y - bone.twist.x, 0.f) * 0.5f * deg;
    }
    else if(bone.joint == Joint::Hinge)
    {
        const glm::vec3 axis = safeNormalize(bone.axis, glm::vec3{0.f, 1.f, 0.f});
        const glm::vec3 z{0.f, 0.f, 1.f};
        const float c = glm::dot(z, axis);
        const glm::vec3 cr = glm::cross(z, axis);
        j.frameA = c < -0.9999f ? glm::quat{0.f, 1.f, 0.f, 0.f} : glm::normalize(glm::quat{1.f + c, cr.x, cr.y, cr.z});
        j.frameB = j.frameA;
        const float rest = restAngles(rig, b).x;
        j.lower = (bone.range.x - rest) * deg;
        j.upper = (bone.range.y - rest) * deg;
    }
    return j;
}

bool gameOn()
{
    if(gameLatched < 0)
    {
        latch();
    }
    return gameLatched == 1;
}

void latch()
{
    int generations = 0;
    for(const Entry& e : rigs)
    {
        generations += e.rig ? e.rig->generation + 1 : 0;
    }
    const int now = vr_rig2.value != 0.f ? 1 : 0;
    const bool changed = gameLatched >= 0 && (now != gameLatched || (now == 1 && generations != gameGenerations));
    gameLatched = now;
    gameGenerations = generations;
    if(vr_rig2_game.value != static_cast<float>(now))
    {
        Cvar_SetValueQuick(&vr_rig2_game, static_cast<float>(now)); // (QC's armed grunts: their shoulders, vr_armedmon.qc)
    }
    if(changed)
    {
        resetGame();
        Con_Printf("vr_rig2 %d: the grunt on the %s rig\n", now, now ? "authored" : "old");
    }
}

bool unswapped(int num, entity_t& out)
{
    if(num <= 0 || num >= cl_max_edicts || !cl_entities)
    {
        return false;
    }
    out = cl_entities[num];
    for(const Drawn& d : drawn)
    {
        if(d.num == num && d.swapped && out.model == d.skinned)
        {
            out.model = d.original;
            out.previouspose = d.previousPose;
            out.currentpose = d.currentPose;
            out.lerpstart = d.lerpStart;
            out.lerptime = d.lerpTime;
            out.lerpflags = d.lerpFlags;
        }
    }
    return true;
}

bool drawnNow(int num)
{
    for(const Drawn& d : drawn)
    {
        if(d.num == num && d.frame == host_framecount)
        {
            return true;
        }
    }
    return false;
}

void skin(const Rig& rig, const rigpose::Rigid* F, const glm::vec3* rest, int numVerts, glm::vec3* out, bool dq)
{
    const int nw = static_cast<int>(rig.weights.size());
    for(int v = 0; v < numVerts; v++)
    {
        const glm::vec3 p = rest[v];
        if(v >= nw || rig.weights[static_cast<za::SizeT>(v)].n == 0)
        {
            out[v] = rigpose::apply(F[0], p);
            continue;
        }
        const Weights& w = rig.weights[static_cast<za::SizeT>(v)];
        if(!dq)
        {
            glm::vec3 q{0.f};
            for(int i = 0; i < w.n; i++)
            {
                q += rigpose::apply(F[w.bone[i]], p) * w.w[i];
            }
            out[v] = q;
            continue;
        }
        // Dual quaternions (Kavan et al. 2007): the rotations blended on the same hemisphere as the heaviest.
        const glm::quat r0 = F[w.bone[0]].rot;
        glm::quat br{0.f, 0.f, 0.f, 0.f}, bd{0.f, 0.f, 0.f, 0.f};
        for(int i = 0; i < w.n; i++)
        {
            const rigpose::Rigid& T = F[w.bone[i]];
            const float sgn = glm::dot(T.rot, r0) < 0.f ? -1.f : 1.f;
            const glm::quat d = glm::quat{0.f, T.pos.x, T.pos.y, T.pos.z} * T.rot * 0.5f;
            br = br + T.rot * (w.w[i] * sgn);
            bd = bd + d * (w.w[i] * sgn);
        }
        const float len = za::max(glm::length(br), 1e-6f);
        br = br * (1.f / len);
        bd = bd * (1.f / len);
        const glm::vec3 rv{br.x, br.y, br.z}, dv{bd.x, bd.y, bd.z};
        const glm::vec3 t = 2.f * (br.w * dv - bd.w * rv + glm::cross(rv, dv));
        out[v] = br * p + t;
    }
}

float weightOn(const Rig& rig, int v, int bone)
{
    if(v < 0 || v >= static_cast<int>(rig.weights.size()))
    {
        return 0.f;
    }
    const Weights& w = rig.weights[static_cast<za::SizeT>(v)];
    for(int i = 0; i < w.n; i++)
    {
        if(w.bone[i] == bone)
        {
            return w.w[i];
        }
    }
    return 0.f;
}

// ---- The file ----

bool read(const char* text, Rig& out, za::String& error)
{
    out.numBones = 0;
    out.weights.clear();
    const char* p = text;
    int line = 0;
    bool header = false;
    char msg[160];
    while(*p)
    {
        const char* e = strchr(p, '\n');
        const int len = e ? static_cast<int>(e - p) : static_cast<int>(strlen(p));
        words.split(p, len);
        p += len + (e ? 1 : 0);
        line++;
        const Words& w = words;
        if(w.n == 0)
        {
            continue;
        }
        if(w.is(0, "qvrrig2"))
        {
            header = true;
        }
        else if(w.is(0, "model") && w.n >= 2)
        {
            out.model = w.w[1];
        }
        else if(w.is(0, "bone") && w.n >= 4)
        {
            if(out.numBones >= maxBones)
            {
                q_snprintf(msg, sizeof(msg), "line %d: more than %d bones", line, maxBones);
                error = msg;
                return false;
            }
            Bone& b = out.bones[out.numBones];
            b = Bone{};
            q_strlcpy(b.name, w.w[1], sizeof(b.name));
            b.parent = w.is(2, "-") ? -1 : boneByName(out, w.w[2]);
            if(!w.is(2, "-") && b.parent < 0)
            {
                q_snprintf(msg, sizeof(msg), "line %d: bone %s: no parent %s before it", line, w.w[1], w.w[2]);
                error = msg;
                return false;
            }
            b.joint = w.is(3, "ball") ? Joint::Ball : w.is(3, "hinge") ? Joint::Hinge : w.is(3, "fixed") ? Joint::Fixed : Joint::Root;
            for(int i = 4; i < w.n; i++)
            {
                if(w.is(i, "pivot")) { b.pivot = w.v3(i + 1); i += 3; }
                else if(w.is(i, "end")) { b.end = w.v3(i + 1); i += 3; }
                else if(w.is(i, "neutral")) { b.neutral = w.v3(i + 1); i += 3; }
                else if(w.is(i, "fwd")) { b.fwd = w.v3(i + 1); i += 3; }
                else if(w.is(i, "side")) { b.side = w.v3(i + 1); i += 3; }
                else if(w.is(i, "axis")) { b.axis = w.v3(i + 1); i += 3; }
                else if(w.is(i, "swingx")) { b.swingX = w.v2(i + 1); i += 2; }
                else if(w.is(i, "swingy")) { b.swingY = w.v2(i + 1); i += 2; }
                else if(w.is(i, "twist")) { b.twist = w.v2(i + 1); i += 2; }
                else if(w.is(i, "range")) { b.range = w.v2(i + 1); i += 2; }
            }
            out.numBones++;
        }
        else if(w.is(0, "w") && w.n >= 4)
        {
            const int v = atoi(w.w[1]);
            if(v < 0 || v > 65535)
            {
                continue;
            }
            if(static_cast<int>(out.weights.size()) <= v)
            {
                out.weights.resize(static_cast<za::SizeT>(v + 1));
            }
            Weights& ws = out.weights[static_cast<za::SizeT>(v)];
            ws = Weights{};
            for(int i = 2; i + 1 < w.n && ws.n < maxWeights; i += 2)
            {
                const int bo = boneByName(out, w.w[i]);
                if(bo < 0)
                {
                    q_snprintf(msg, sizeof(msg), "line %d: vertex %d: no bone %s", line, v, w.w[i]);
                    error = msg;
                    return false;
                }
                ws.bone[ws.n] = bo;
                ws.w[ws.n] = w.f(i + 1);
                ws.n++;
            }
        }
    }
    if(!header || out.numBones == 0)
    {
        error = header ? "no bones" : "not a qvrrig2 file";
        return false;
    }
    out.generation++;
    return true;
}

void write(const Rig& rig, za::String& out)
{
    out.clear();
    out += "qvrrig2 1\nmodel ";
    out += rig.model;
    out += "\n# Made by Misc/quakevr/make_rig2.py (its header: the method), edited in the Rig Studio (Range of Motion); hand-editable.\n";
    out += "# Model units, x forward, y left, z up; degrees. bone <name> <parent|-> <root|ball|hinge|fixed> pivot <xyz> end <xyz>\n";
    out += "#   ball: neutral <xyz> fwd <xyz> side <xyz> swingx <min max> swingy <min max> twist <min max>\n";
    out += "#   hinge: axis <xyz> range <min max> (its bend from straight; the rest pose's own bend measured)\n";
    out += "# w <vertex> <bone> <weight> ... (up to 4, summing to 1)\n";
    for(int b = 0; b < rig.numBones; b++)
    {
        const Bone& bone = rig.bones[b];
        out += "bone ";
        out += bone.name;
        out += " ";
        out += bone.parent >= 0 ? rig.bones[bone.parent].name : "-";
        out += bone.joint == Joint::Ball ? " ball" : bone.joint == Joint::Hinge ? " hinge" : bone.joint == Joint::Fixed ? " fixed" : " root";
        out += " pivot";
        vec(out, bone.pivot);
        out += " end";
        vec(out, bone.end);
        if(bone.joint == Joint::Ball)
        {
            out += " neutral";
            vec(out, bone.neutral);
            out += " fwd";
            vec(out, bone.fwd);
            out += " side";
            vec(out, bone.side);
            out += " swingx";
            num(out, bone.swingX.x);
            num(out, bone.swingX.y);
            out += " swingy";
            num(out, bone.swingY.x);
            num(out, bone.swingY.y);
            out += " twist";
            num(out, bone.twist.x);
            num(out, bone.twist.y);
        }
        else if(bone.joint == Joint::Hinge)
        {
            out += " axis";
            vec(out, bone.axis);
            out += " range";
            num(out, bone.range.x);
            num(out, bone.range.y);
        }
        out += "\n";
    }
    for(za::SizeT v = 0; v < rig.weights.size(); v++)
    {
        const Weights& w = rig.weights[v];
        out += "w ";
        out += za::toString(static_cast<int>(v));
        for(int i = 0; i < w.n; i++)
        {
            out += " ";
            out += rig.bones[w.bone[i]].name;
            num(out, w.w[i]);
        }
        out += "\n";
    }
}

bool save(Rig& rig, za::String& pathOut)
{
    char path[MAX_OSPATH], dir[MAX_OSPATH], root[MAX_OSPATH];
    filePath(rig.model.cStr(), path, sizeof(path));
    rigdata::rootDir(root, sizeof(root));
    q_snprintf(dir, sizeof(dir), "%s/rigs", root);
    files::createDirectories(dir);
    za::String text;
    write(rig, text);
    pathOut = path;
    if(!files::writeText(path, text.toStringView()))
    {
        return false;
    }
    rig.path = path;
    return true;
}

Rig* rigFor(const qmodel_t* model)
{
    if(!model)
    {
        return nullptr;
    }
    for(Entry& e : rigs)
    {
        if(!strcmp(e.model.cStr(), model->name))
        {
            return e.rig.get();
        }
    }
    Entry e;
    e.model = model->name;
    e.tried = true;
    char path[MAX_OSPATH];
    filePath(model->name, path, sizeof(path));
    za::String text, err;
    if(!files::readText(path, text))
    {
        // (the tests' folder has none: the game folder's)
        char base[64];
        rigdata::baseName(model->name, base, sizeof(base));
        q_snprintf(path, sizeof(path), "%s/rigs/%s.rig2.txt", com_gamedir, base);
        if(!files::readText(path, text))
        {
            rigs.pushBack(ZA_MOVE(e));
            return nullptr;
        }
    }
    za::UniquePtr<Rig> r = za::makeUnique<Rig>();
    if(!read(text.cStr(), *r, err))
    {
        Con_Printf("rig2: %s: %s\n", path, err.cStr());
        rigs.pushBack(ZA_MOVE(e));
        return nullptr;
    }
    r->path = path;
    r->model = model->name;
    e.rig = ZA_MOVE(r);
    Rig* out = e.rig.get();
    rigs.pushBack(ZA_MOVE(e));
    return out;
}

void reload()
{
    // (the rigs are owned here: pointers handed out are stale after this; the studio asks again each frame)
    for(Entry& e : rigs)
    {
        if(!e.rig)
        {
            continue;
        }
        char path[MAX_OSPATH];
        q_strlcpy(path, e.rig->path.cStr(), sizeof(path));
        za::String text, err;
        if(files::readText(path, text))
        {
            Rig fresh;
            fresh.generation = e.rig->generation;
            if(read(text.cStr(), fresh, err))
            {
                fresh.path = e.rig->path;
                fresh.model = e.model;
                *e.rig = ZA_MOVE(fresh);
            }
            else
            {
                Con_Printf("rig2: %s: %s\n", path, err.cStr());
            }
        }
    }
    za::Vector<Entry> kept;
    for(Entry& e : rigs)
    {
        if(e.rig)
        {
            kept.pushBack(ZA_MOVE(e));
        }
    }
    rigs = ZA_MOVE(kept); // (the models without one are looked for again)
}

// ---- Drawing ----

namespace
{
void publishDrawn(int entnum, const qmodel_t* model, const rigpose::Rigid* F, int numBones, bool studio, uint32_t hidden = 0)
{
    Drawn* d = nullptr;
    for(Drawn& x : drawn)
    {
        d = x.num == entnum ? &x : d;
    }
    if(d && !studio && d->studioFrame >= host_framecount - 1)
    {
        return; // (the Rig Studio's pose of its subject this frame or the last: its own)
    }
    if(!d)
    {
        // (a slot drawn by nobody lately reused: the drawn entities' list stays as long as the most drawn at once)
        for(Drawn& x : drawn)
        {
            d = !d && x.frame < host_framecount - 2 && !x.swapped ? &x : d;
        }
    }
    if(!d)
    {
        drawn.pushBack(Drawn{});
        d = &drawn.back();
    }
    if(d->num != entnum)
    {
        *d = Drawn{};
    }
    d->num = entnum;
    d->model = model;
    d->frame = host_framecount;
    d->studioFrame = studio ? host_framecount : d->studioFrame;
    d->bones = za::min(numBones, maxBones);
    const Rig* rig = hidden ? rigFor(model) : nullptr;
    for(int b = 0; b < d->bones; b++)
    {
        d->F[b] = F[b];
        const bool gone = (hidden >> b) & 1u; // (a hidden bone: its vertices at its pivot, its triangles gone)
        const glm::mat3 r = gone ? glm::mat3{0.f} : glm::mat3_cast(F[b].rot);
        const glm::vec3 t = gone ? rigpose::apply(F[b], rig && b < rig->numBones ? rig->bones[b].pivot : glm::vec3{0.f}) : F[b].pos;
        float* o = d->skin + b * 12;
        for(int row = 0; row < 3; row++)
        {
            o[row * 4 + 0] = r[0][row];
            o[row * 4 + 1] = r[1][row];
            o[row * 4 + 2] = r[2][row];
            o[row * 4 + 3] = t[row];
        }
    }
}
} // namespace

void drawPosed(int entnum, const qmodel_t* model, const rigpose::Rigid* F, int numBones)
{
    publishDrawn(entnum, model, F, numBones, true);
}

void drawLive(int entnum, const qmodel_t* model, const rigpose::Rigid* F, int numBones, uint32_t hidden)
{
    publishDrawn(entnum, model, F, numBones, false, hidden);
}

bool drawnBone(int num, int b, glm::quat& rot, glm::vec3& pos)
{
    for(const Drawn& d : drawn)
    {
        if(d.num == num && d.frame >= host_framecount - 2 && b >= 0 && b < d.bones)
        {
            rot = d.F[b].rot;
            pos = d.F[b].pos;
            return true;
        }
    }
    return false;
}

qmodel_t* skinnedFor(const qmodel_t* model)
{
    Rig* rig = model ? rigFor(model) : nullptr;
    if(!rig)
    {
        return nullptr;
    }
    qmodel_t* sk = skinnedOf(model);
    if(!sk || sk->type != mod_alias)
    {
        return nullptr;
    }
    // (made again when the rig was read again)
    bool fresh = false;
    for(Made& m : made)
    {
        if(m.mod == sk)
        {
            fresh = true;
            if(m.generation != rig->generation)
            {
                Cache_Free(&sk->cache, false);
                m.generation = rig->generation;
                sk = skinnedOf(model);
            }
        }
    }
    if(!fresh)
    {
        made.pushBack(Made{sk, rig->generation});
    }
    if(!sk || sk->type != mod_alias)
    {
        return nullptr;
    }
    catchUpAO(sk, const_cast<qmodel_t*>(model));
    return sk;
}

void swapModels()
{
    if(cls.state != ca_connected || !cl_entities)
    {
        return;
    }
    for(Drawn& d : drawn)
    {
        d.swapped = false;
        if(d.frame < host_framecount - 2 || d.num <= 0 || d.num >= cl_max_edicts)
        {
            continue;
        }
        entity_t* e = &cl_entities[d.num];
        if(e->model != d.model)
        {
            continue;
        }
        qmodel_t* sk = skinnedFor(d.model);
        if(!sk)
        {
            continue;
        }
        d.original = e->model;
        d.skinned = sk;
        d.previousPose = e->previouspose;
        d.currentPose = e->currentpose;
        d.lerpStart = e->lerpstart;
        d.lerpTime = e->lerptime;
        d.lerpFlags = e->lerpflags;
        e->model = sk;
        e->previouspose = e->currentpose = 0;
        e->lerpflags &= static_cast<byte>(~(LERP_RESETANIM | LERP_RESETANIM2));
        d.swapped = true;
    }
}

void restoreModels()
{
    for(Drawn& d : drawn)
    {
        if(!d.swapped || d.num <= 0 || d.num >= cl_max_edicts)
        {
            continue;
        }
        entity_t* e = &cl_entities[d.num];
        if(e->model == d.skinned)
        {
            e->model = d.original;
            e->previouspose = d.previousPose;
            e->currentpose = d.currentPose;
            e->lerpstart = d.lerpStart;
            e->lerptime = d.lerpTime;
            e->lerpflags = d.lerpFlags;
        }
        d.swapped = false;
    }
}

int bonePoses(const entity_t* e, const float** matrices)
{
    if(!e || drawn.empty())
    {
        return 0;
    }
    for(Drawn& d : drawn)
    {
        if(d.swapped && e->model == d.skinned && e == &cl_entities[d.num])
        {
            if(!matrices)
            {
                return d.bones;
            }
            *matrices = d.skin;
            if(d.drawsFrame != host_framecount)
            {
                d.lastDraws = d.drawsFrame == host_framecount - 1 ? d.draws : 0;
                d.drawsFrame = host_framecount;
                d.draws = 0;
            }
            d.draws++;
            return d.bones;
        }
    }
    return 0;
}

const qmodel_t* sourceModel(const entity_t* e)
{
    if(!e || drawn.empty() || !cl_entities)
    {
        return nullptr;
    }
    for(const Drawn& d : drawn)
    {
        if(d.swapped && e->model == d.skinned && e == &cl_entities[d.num])
        {
            return d.original;
        }
    }
    return nullptr;
}

void drawInfo()
{
    const auto model = [](const char* what, const qmodel_t* m) {
        const auto* h = m && m->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(m))) : nullptr;
        if(!h)
        {
            Con_Printf("  %s: none\n", what);
            return;
        }
        int surfaces = 0;
        for(const aliashdr_t* s = h; s; s = Mod_NextSurface(const_cast<aliashdr_t*>(s)))
        {
            surfaces++;
        }
        const gltexture_t* t = h->gltextures[0][0];
        const gltexture_t* n = t ? TexMgr_NormalMap(const_cast<gltexture_t*>(t)) : nullptr;
        Con_Printf("  %s: %s pv %d surfaces %d verts %d tris %d poses %d bones %d dequant %d flags %d\n", what, m->name,
            static_cast<int>(h->poseverttype), surfaces, h->numverts_vbo, h->numtris, h->numposes, h->numbones, h->dequant ? 1 : 0, m->flags);
        Con_Printf("  %s skin: %s %dx%d (source %ux%u) flags 0x%x; fullbright %s; normal map %s %dx%d\n", what, t ? t->name : "-",
            t ? t->width : 0, t ? t->height : 0, t ? t->source_width : 0u, t ? t->source_height : 0u, t ? static_cast<unsigned>(t->flags) : 0u,
            h->fbtextures[0][0] ? h->fbtextures[0][0]->name : "-", n ? n->name : "-", n ? n->width : 0, n ? n->height : 0);
    };
    if(drawn.empty())
    {
        Con_Printf("rig2 draw: nothing drawn on the authored rig\n");
    }
    for(const Drawn& d : drawn)
    {
        Con_Printf("rig2 draw: entity %d, published frame %d (now %d), swapped %d, drawn %d times last frame\n", d.num, d.frame,
            host_framecount, d.skinned ? 1 : 0, d.drawsFrame >= host_framecount - 1 ? (d.drawsFrame == host_framecount ? d.draws : d.lastDraws) : 0);
        model("old", d.model);
        model("authored", d.skinned);
        if(d.num > 0 && d.num < cl_max_edicts && cl_entities)
        {
            // the per-model shading the renderer looks up (retro textures and lighting: by the model, its name and skin)
            entity_t e = cl_entities[d.num];
            for(int k = 0; k < 2; k++)
            {
                qmodel_t* m = const_cast<qmodel_t*>(k == 0 ? d.model : d.skinned);
                const auto* h = m && m->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(m)) : nullptr;
                if(!h)
                {
                    continue;
                }
                e.model = m;
                float retro[4], part[4];
                VR_RetroAlias(&e, h, 1, retro, part);
                Con_Printf("  %s retro: set %g, Quake skin %gx%g, light texel share %g\n", k == 0 ? "old" : "authored", retro[0],
                    retro[1], retro[2], VR_RetroLightSkinScale(m, h, e.skinnum));
            }
        }
    }
}

bool makeSkinnedModel(qmodel_t* mod)
{
    const size_t len = strlen(mod->name), suffix = strlen(skinnedSuffix);
    if(len <= suffix || strcmp(mod->name + len - suffix, skinnedSuffix))
    {
        return false;
    }
    char base[MAX_QPATH];
    q_strlcpy(base, mod->name, za::min(sizeof(base), len - suffix + 1));
    qmodel_t* src = Mod_ForName(base, false);
    const Rig* rig = src ? rigFor(src) : nullptr;
    if(!rig || src->type != mod_alias)
    {
        return false;
    }
    const auto* sh = static_cast<const aliashdr_t*>(Mod_Extradata(src));
    if(!sh || sh->poseverttype != aliashdr_t::PV_QUAKE1)
    {
        return false;
    }
    const int numVerts = sh->numverts_vbo, numIndexes = sh->numindexes, numBones = rig->numBones, numFrames = za::max(sh->numframes, 1);

    const int start = Hunk_LowMark();
    const size_t hdrSize = sizeof(aliashdr_t) + sizeof(maliasframedesc_t) * static_cast<size_t>(numFrames - 1);
    auto* hdr = static_cast<aliashdr_t*>(Hunk_Alloc(static_cast<int>(hdrSize)));
    auto* bones = static_cast<boneinfo_t*>(Hunk_Alloc(static_cast<int>(sizeof(boneinfo_t)) * numBones));
    auto* bind = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numBones));
    auto* poses = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numBones));
    auto* verts = static_cast<iqmvert_t*>(Hunk_Alloc(static_cast<int>(sizeof(iqmvert_t)) * numVerts));
    auto* indexes = static_cast<unsigned short*>(Hunk_Alloc(static_cast<int>(sizeof(unsigned short)) * numIndexes));

    hdr->ident = sh->ident;
    hdr->version = sh->version;
    hdr->scale[0] = hdr->scale[1] = hdr->scale[2] = 1.f;
    hdr->boundingradius = sh->boundingradius;
    hdr->numskins = sh->numskins;
    hdr->skinwidth = sh->skinwidth;
    hdr->skinheight = sh->skinheight;
    hdr->numverts = numVerts;
    hdr->numverts_vbo = numVerts;
    hdr->numtris = sh->numtris;
    hdr->numframes = numFrames;
    hdr->synctype = sh->synctype;
    hdr->flags = sh->flags;
    hdr->size = sh->size;
    hdr->numindexes = numIndexes;
    hdr->numposes = 1;
    hdr->numbones = numBones;
    hdr->poseverttype = aliashdr_t::PV_IQM;
    hdr->nextsurface = 0;
    for(int f = 0; f < numFrames; f++)
    {
        hdr->frames[f] = sh->frames[za::min(f, sh->numframes - 1)];
        hdr->frames[f].firstpose = 0;
        hdr->frames[f].numposes = 1;
    }
    memcpy(hdr->gltextures, sh->gltextures, sizeof(hdr->gltextures));
    memcpy(hdr->fbtextures, sh->fbtextures, sizeof(hdr->fbtextures));
    memcpy(hdr->texels, sh->texels, sizeof(hdr->texels));
    for(int b = 0; b < numBones; b++)
    {
        q_strlcpy(bones[b].name, rig->bones[b].name, sizeof(bones[b].name));
        bones[b].parent = -1;
        for(int i = 0; i < 12; i++)
        {
            const float id = i % 5 == 0 ? 1.f : 0.f;
            bones[b].inverse.mat[i] = id;
            bind[b].mat[i] = id;
            poses[b].mat[i] = id;
        }
    }
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(sh) + sh->meshdesc);
    (void)shadeLikeMdl(src, sh, verts, numVerts); // (its occlusion later if not baked yet: swapModels)
    for(int v = 0; v < numVerts; v++)
    {
        const int mv = desc[v].vertindex;
        iqmvert_t& o = verts[v];
        o.st[0] = (static_cast<float>(desc[v].st[0]) + 0.5f) / static_cast<float>(TexMgr_PadConditional(sh->skinwidth));
        o.st[1] = (static_cast<float>(desc[v].st[1]) + 0.5f) / static_cast<float>(TexMgr_PadConditional(sh->skinheight));
        for(int i = 0; i < 4; i++)
        {
            o.weight[i] = 0;
            o.idx[i] = 0;
        }
        const Weights* w = mv < static_cast<int>(rig->weights.size()) ? &rig->weights[static_cast<za::SizeT>(mv)] : nullptr;
        if(!w || w->n == 0)
        {
            o.weight[0] = 255;
            continue;
        }
        // (bytes summing to 255, the heaviest first)
        int order[maxWeights] = {0, 1, 2, 3};
        for(int i = 0; i < w->n; i++)
        {
            for(int j = i + 1; j < w->n; j++)
            {
                if(w->w[order[j]] > w->w[order[i]])
                {
                    const int x = order[i];
                    order[i] = order[j];
                    order[j] = x;
                }
            }
        }
        int left = 255;
        for(int i = 0; i < w->n; i++)
        {
            const int k = order[i];
            const int b = i + 1 < w->n ? static_cast<int>(w->w[k] * 255.f + 0.5f) : left;
            o.weight[i] = static_cast<uint8_t>(za::clamp(za::min(b, left), 0, 255));
            o.idx[i] = static_cast<uint8_t>(za::clamp(w->bone[k], 0, numBones - 1));
            left -= o.weight[i];
        }
    }
    memcpy(indexes, reinterpret_cast<const byte*>(sh) + sh->indexes, sizeof(unsigned short) * static_cast<size_t>(numIndexes));
    hdr->boneinfo = reinterpret_cast<byte*>(bones) - reinterpret_cast<byte*>(hdr);
    hdr->bindpose = reinterpret_cast<byte*>(bind) - reinterpret_cast<byte*>(hdr);
    hdr->boneposedata = reinterpret_cast<byte*>(poses) - reinterpret_cast<byte*>(hdr);
    hdr->vertexes = reinterpret_cast<byte*>(verts) - reinterpret_cast<byte*>(hdr);
    hdr->indexes = reinterpret_cast<byte*>(indexes) - reinterpret_cast<byte*>(hdr);

    if(mod->meshvbo)
    {
        GLMesh_DeleteVertexBuffer(mod);
    }
    GLMesh_LoadVertexBuffer(mod, hdr);
    mod->type = mod_alias;
    mod->needload = false;
    mod->numframes = numFrames;
    mod->synctype = src->synctype;
    mod->flags = src->flags;
    mod->path_id = src->path_id;
    for(int i = 0; i < 3; i++)
    {
        mod->mins[i] = mod->ymins[i] = mod->rmins[i] = -64.f;
        mod->maxs[i] = mod->ymaxs[i] = mod->rmaxs[i] = 64.f;
    }
    const int total = Hunk_LowMark() - start;
    Cache_Alloc(&mod->cache, total, mod->name);
    if(!mod->cache.data)
    {
        Hunk_FreeToLowMark(start);
        return false;
    }
    memcpy(mod->cache.data, hdr, static_cast<size_t>(total));
    Hunk_FreeToLowMark(start);
    Con_DPrintf("rig2: %s made (%d vertices, %d bones)\n", mod->name, numVerts, numBones);
    return true;
}

} // namespace qvr::rig2
