// vr_rigdata.cpp -- the rigs' and animations' override data: see vr_rigdata.hpp.

#include "vr_rigdata.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_mem.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"

#include "Zancle/Algorithm/Erase.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace qvr;

namespace
{

constexpr float pi = 3.14159265358979f;
constexpr float deg = pi / 180.f;

// One model's entry (read when first asked for; `none`: it has no files).
struct Entry
{
    const qmodel_t* model{nullptr};
    bool none{false};
    za::UniquePtr<rigdata::ModelData> data;
};
struct Registry
{
    za::Vector<Entry> entries;
    auto members() { return mem::list(entries); }
};
mem::Cache<Registry> registry{"rig data", mem::GameDirChange};
// Whether any entry has tracks or sequences (the game's layer: rigseq.cpp).
struct RegistryState
{
    bool animation{false};
};
RegistryState regState;

void recount()
{
    regState.animation = false;
    for(const Entry& e : registry.entries)
    {
        if(e.data && (!e.data->overrides.empty() || !e.data->seqs.empty() || !e.data->poses.empty()))
        {
            regState.animation = regState.animation || !e.data->overrides.empty() || !e.data->seqs.empty();
        }
    }
}

// ---- Text ----

// A number as written (%.6g; -0 as 0).
void num(za::String& out, float v)
{
    char b[32];
    q_snprintf(b, sizeof(b), " %.6g", v == 0.f ? 0.f : v);
    out += b;
}
void vec3(za::String& out, const glm::vec3& v)
{
    num(out, v.x);
    num(out, v.y);
    num(out, v.z);
}

// A name for the files: letters, digits, '_', '-', '.' (anything else '_'); "" -> "unnamed".
void cleanName(const char* in, char* out, int size)
{
    int n = 0;
    for(const char* p = in; *p && n < size - 1; p++)
    {
        const char c = *p;
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        out[n++] = ok ? c : '_';
    }
    out[n] = 0;
    if(n == 0)
    {
        q_strlcpy(out, "unnamed", static_cast<size_t>(size));
    }
}

// A line's words (whitespace separated), up to 24.
struct Words
{
    char buf[512]{};
    const char* w[24]{};
    int n{0};
    void split(const char* line, int len)
    {
        len = za::min(len, static_cast<int>(sizeof(buf)) - 1);
        memcpy(buf, line, static_cast<size_t>(len));
        buf[len] = 0;
        n = 0;
        char* p = buf;
        while(*p && n < 24)
        {
            while(*p == ' ' || *p == '\t' || *p == '\r')
            {
                *p++ = 0;
            }
            if(!*p || *p == '#')
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
    [[nodiscard]] const char* s(int i) const { return i < n ? w[i] : ""; }
};

[[nodiscard]] int boneByName(const ragdoll::Rig& rig, const char* name)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        if(!strcmp(rig.bones[b].name, name))
        {
            return b;
        }
    }
    return -1;
}

[[nodiscard]] rigdata::Interp interpOf(const char* s)
{
    if(!strcmp(s, "linear"))
    {
        return rigdata::Interp::Linear;
    }
    if(!strcmp(s, "step"))
    {
        return rigdata::Interp::Step;
    }
    return rigdata::Interp::Smooth;
}

void writeTrack(za::String& out, const rigdata::Track& t, const ragdoll::Rig& rig, const char* indent)
{
    out += indent;
    out += "track ";
    out += t.bone >= 0 && t.bone < rig.numBones ? rig.bones[t.bone].name : "?";
    out += "\n";
    for(const rigdata::Key& k : t.keys)
    {
        out += indent;
        out += "  key";
        num(out, k.frame);
        out += " ";
        out += rigdata::interpName(k.interp);
        vec3(out, k.value.rot);
        vec3(out, k.value.off);
        out += "\n";
    }
    out += indent;
    out += "end\n";
}

// Lines of `text` in turn: f(words) -> false stops.
template <typename F>
void forLines(const char* text, F&& f)
{
    const char* p = text;
    Words w;
    while(*p)
    {
        const char* e = strchr(p, '\n');
        const int len = e ? static_cast<int>(e - p) : static_cast<int>(strlen(p));
        w.split(p, len);
        if(w.n > 0 && !f(w))
        {
            return;
        }
        p += len + (e ? 1 : 0);
    }
}

// A track's keys read until its `end` (the parser's state).
struct TrackReader
{
    rigdata::Track* track{nullptr};
    bool key(const Words& w)
    {
        if(!track || !w.is(0, "key") || w.n < 9)
        {
            return false;
        }
        rigdata::Key k;
        k.frame = w.f(1);
        k.interp = interpOf(w.s(2));
        k.value.rot = w.v3(3);
        k.value.off = w.v3(6);
        track->keys.pushBack(k);
        return true;
    }
};

void sortKeys(rigdata::Track& t)
{
    // (insertion: few keys, stable)
    for(za::SizeT i = 1; i < t.keys.size(); i++)
    {
        for(za::SizeT j = i; j > 0 && t.keys[j].frame < t.keys[j - 1].frame; j--)
        {
            const rigdata::Key tmp = t.keys[j];
            t.keys[j] = t.keys[j - 1];
            t.keys[j - 1] = tmp;
        }
    }
}

// ---- The rig's refit (the bones whose vertices changed: Horn's rigid fit, as vr_ragdoll.cpp's) ----

glm::quat largestEigenQuat(float n[4][4])
{
    float v[4][4] = {{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f, 0.f, 1.f}};
    for(int sweep = 0; sweep < 24; sweep++)
    {
        float off = 0.f;
        for(int p = 0; p < 4; p++)
        {
            for(int q = p + 1; q < 4; q++)
            {
                off += n[p][q] * n[p][q];
            }
        }
        if(off < 1e-18f)
        {
            break;
        }
        for(int p = 0; p < 3; p++)
        {
            for(int q = p + 1; q < 4; q++)
            {
                if(za::abs(n[p][q]) < 1e-20f)
                {
                    continue;
                }
                const float theta = (n[q][q] - n[p][p]) / (2.f * n[p][q]);
                const float t = (theta >= 0.f ? 1.f : -1.f) / (za::abs(theta) + za::sqrt(theta * theta + 1.f));
                const float c = 1.f / za::sqrt(t * t + 1.f), s = t * c;
                for(int k = 0; k < 4; k++)
                {
                    const float a = n[k][p], b = n[k][q];
                    n[k][p] = c * a - s * b;
                    n[k][q] = s * a + c * b;
                }
                for(int k = 0; k < 4; k++)
                {
                    const float a = n[p][k], b = n[q][k];
                    n[p][k] = c * a - s * b;
                    n[q][k] = s * a + c * b;
                }
                for(int k = 0; k < 4; k++)
                {
                    const float a = v[k][p], b = v[k][q];
                    v[k][p] = c * a - s * b;
                    v[k][q] = s * a + c * b;
                }
            }
        }
    }
    int best = 0;
    for(int i = 1; i < 4; i++)
    {
        if(n[i][i] > n[best][best])
        {
            best = i;
        }
    }
    const glm::quat q{v[0][best], v[1][best], v[2][best], v[3][best]};
    const float len = glm::length(q);
    return len > 1e-12f ? q / len : glm::quat{1.f, 0.f, 0.f, 0.f};
}

struct Fit
{
    glm::vec3 sa{0.f}, sb{0.f};
    float s[3][3]{};
    int n{0};
    void add(const glm::vec3& a, const glm::vec3& b)
    {
        sa += a;
        sb += b;
        for(int i = 0; i < 3; i++)
        {
            for(int j = 0; j < 3; j++)
            {
                s[i][j] += a[i] * b[j];
            }
        }
        n++;
    }
    void solve(glm::quat& rot, glm::vec3& pos) const
    {
        if(n == 0)
        {
            rot = glm::quat{1.f, 0.f, 0.f, 0.f};
            pos = glm::vec3{0.f};
            return;
        }
        const float inv = 1.f / static_cast<float>(n);
        const glm::vec3 ca = sa * inv, cb = sb * inv;
        float m[3][3];
        for(int i = 0; i < 3; i++)
        {
            for(int j = 0; j < 3; j++)
            {
                m[i][j] = s[i][j] - static_cast<float>(n) * ca[i] * cb[j];
            }
        }
        const float sxx = m[0][0], sxy = m[0][1], sxz = m[0][2], syx = m[1][0], syy = m[1][1], syz = m[1][2], szx = m[2][0],
                    szy = m[2][1], szz = m[2][2];
        float N[4][4] = {{sxx + syy + szz, syz - szy, szx - sxz, sxy - syx},
            {syz - szy, sxx - syy - szz, sxy + syx, szx + sxz},
            {szx - sxz, sxy + syx, -sxx + syy - szz, syz + szy},
            {sxy - syx, szx + sxz, syz + szy, -sxx - syy + szz}};
        rot = largestEigenQuat(N);
        pos = cb - rot * ca;
    }
};

[[nodiscard]] glm::vec3 poseVert(const aliashdr_t* hdr, int pose, int v)
{
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const trivertx_t& t = base[static_cast<za::SizeT>(pose) * static_cast<za::SizeT>(hdr->numverts) + static_cast<za::SizeT>(v)];
    return glm::vec3{t.v[0], t.v[1], t.v[2]} * glm::vec3{hdr->scale[0], hdr->scale[1], hdr->scale[2]} +
           glm::vec3{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
}

// The bones `changed` (bits) of `rig` fitted again to their vertices (each pose), their points (hull) and the
// triangles' bones made again.
void refit(ragdoll::Rig& rig, const aliashdr_t* hdr, uint32_t changed)
{
    const int nv = rig.numVerts;
    for(int b = 0; b < rig.numBones; b++)
    {
        if(!(changed & (1u << b)))
        {
            continue;
        }
        rig.bones[b].points.clear();
        for(int v = 0; v < nv; v++)
        {
            if(rig.vertBone[static_cast<za::SizeT>(v)] != b)
            {
                continue;
            }
            const glm::vec3 p = poseVert(hdr, 0, v);
            bool seen = false;
            for(const glm::vec3& q : rig.bones[b].points)
            {
                seen = seen || q == p;
            }
            if(!seen)
            {
                rig.bones[b].points.pushBack(p);
            }
        }
        for(int p = 0; p < rig.numPoses; p++)
        {
            Fit fit;
            glm::vec3 first{0.f};
            bool same = true;
            for(int v = 0; v < nv; v++)
            {
                if(rig.vertBone[static_cast<za::SizeT>(v)] != b)
                {
                    continue;
                }
                const glm::vec3 q = poseVert(hdr, p, v);
                if(fit.n == 0)
                {
                    first = q;
                }
                same = same && q == first;
                fit.add(poseVert(hdr, 0, v), q);
            }
            if(fit.n < 3)
            {
                continue; // (too few to fit: its old motion kept)
            }
            const za::SizeT at = static_cast<za::SizeT>(p * rig.numBones + b);
            fit.solve(rig.poseRot[at], rig.posePos[at]);
            if(at < rig.poseHidden.size())
            {
                rig.poseHidden[at] = same ? 1 : 0;
            }
        }
    }
    if(!hdr->numindexes)
    {
        return;
    }
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
    rig.triBones.clear();
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        uint32_t bits = 0;
        for(int k = 0; k < 3; k++)
        {
            const int v = za::clamp(static_cast<int>(desc[idx[i + k]].vertindex), 0, nv - 1);
            bits |= 1u << rig.vertBone[static_cast<za::SizeT>(v)];
        }
        bool seen = false;
        for(const uint32_t t : rig.triBones)
        {
            seen = seen || t == bits;
        }
        if(!seen)
        {
            rig.triBones.pushBack(bits);
        }
    }
}

void loadEntry(Entry& e)
{
    e.none = true;
    e.data.reset();
    if(!e.model)
    {
        return;
    }
    ragdoll::Rig* rig = const_cast<ragdoll::Rig*>(ragdoll::rigFor(const_cast<qmodel_t*>(e.model)));
    if(!rig)
    {
        return;
    }
    char base[64], path[MAX_OSPATH], dir[MAX_OSPATH];
    rigdata::baseName(e.model->name, base, sizeof(base));
    za::UniquePtr<rigdata::ModelData> m = za::makeUnique<rigdata::ModelData>();
    m->model = e.model->name;
    m->base = base;
    for(int b = 0; b < rig->numBones; b++)
    {
        m->boneName[b] = rig->bones[b].name;
    }
    bool any = false;
    rigdata::rigPath(base, path, sizeof(path));
    za::String text, err;
    if(files::readText(path, text))
    {
        if(rigdata::readRig(text.cStr(), *rig, *m, err))
        {
            m->haveRigFile = true;
            any = true;
        }
        else
        {
            Con_Printf("rigdata: %s: %s\n", path, err.cStr());
        }
    }
    rigdata::animDir(base, dir, sizeof(dir));
    if(files::isDirectory(dir))
    {
        za::Vector<za::String> names;
        files::forEachEntry(dir, [&](const char* name, bool isDir) {
            const size_t n = strlen(name);
            if(!isDir && n > 9 && !strcmp(name + n - 9, ".anim.txt"))
            {
                names.pushBack(za::String{name});
            }
        });
        // (in name order: the same on every system)
        for(za::SizeT i = 1; i < names.size(); i++)
        {
            for(za::SizeT j = i; j > 0 && strcmp(names[j].cStr(), names[j - 1].cStr()) < 0; j--)
            {
                za::String t = names[j];
                names[j] = names[j - 1];
                names[j - 1] = t;
            }
        }
        for(const za::String& n : names)
        {
            q_snprintf(path, sizeof(path), "%s/%s", dir, n.cStr());
            za::String t2, e2;
            rigdata::Sequence s;
            if(files::readText(path, t2) && rigdata::readSequence(t2.cStr(), *rig, s, e2))
            {
                m->seqs.pushBack(ZA_MOVE(s));
                any = true;
            }
            else
            {
                Con_Printf("rigdata: %s: %s\n", path, e2.empty() ? "could not be read" : e2.cStr());
            }
        }
    }
    if(any)
    {
        e.none = false;
        e.data = ZA_MOVE(m);
        if(developer.value)
        {
            Con_Printf("rigdata: %s: rig file %s, %d sequences\n", e.model->name, e.data->haveRigFile ? "read" : "none",
                static_cast<int>(e.data->seqs.size()));
        }
    }
}

[[nodiscard]] Entry* entryOf(const qmodel_t* model, bool load)
{
    for(Entry& e : registry.entries)
    {
        if(e.model == model)
        {
            return &e;
        }
    }
    if(!load)
    {
        return nullptr;
    }
    registry.entries.pushBack(Entry{});
    Entry& e = registry.entries.back();
    e.model = model;
    loadEntry(e);
    recount();
    return &e;
}

} // namespace

namespace qvr::rigdata
{

glm::quat toQuat(const glm::vec3& rotDeg)
{
    return rigpose::fromVector(rotDeg * deg);
}

glm::vec3 toRotDeg(const glm::quat& q)
{
    return rigpose::toVector(q) / deg;
}

rigpose::Rigid toRigid(const Local& l, const glm::vec3& c)
{
    rigpose::Rigid r;
    r.rot = toQuat(l.rot);
    r.pos = c + l.off - r.rot * c;
    return r;
}

Local fromRigid(const rigpose::Rigid& r, const glm::vec3& c)
{
    Local l;
    l.rot = toRotDeg(r.rot);
    l.off = rigpose::apply(r, c) - c;
    return l;
}

rigpose::Rigid compose(const rigpose::Rigid& a, const rigpose::Rigid& b)
{
    rigpose::Rigid r;
    r.rot = glm::normalize(a.rot * b.rot);
    r.pos = a.rot * b.pos + a.pos;
    return r;
}

rigpose::Rigid inverse(const rigpose::Rigid& a)
{
    rigpose::Rigid r;
    r.rot = glm::inverse(a.rot);
    r.pos = -(r.rot * a.pos);
    return r;
}

rigpose::Rigid lerp(const rigpose::Rigid& a, const rigpose::Rigid& b, float t)
{
    rigpose::Rigid r;
    r.rot = glm::normalize(glm::slerp(a.rot, b.rot, t));
    r.pos = glm::mix(a.pos, b.pos, t);
    return r;
}

const char* interpName(Interp i)
{
    switch(i)
    {
    case Interp::Linear: return "linear";
    case Interp::Step: return "step";
    default: return "smooth";
    }
}

float seconds(const Sequence& s)
{
    return static_cast<float>(za::max(s.length, 1)) / za::max(s.fps, 0.01f);
}

void baseName(const char* model, char* out, int size)
{
    const char* slash = strrchr(model, '/');
    const char* b = slash ? slash + 1 : model;
    char tmp[64];
    q_strlcpy(tmp, b, sizeof(tmp));
    char* dot = strrchr(tmp, '.');
    if(dot)
    {
        *dot = 0;
    }
    cleanName(tmp, out, size);
}

void rootDir(char* out, int size)
{
    if(vr_rigdata_dir.string[0])
    {
        char clean[64];
        cleanName(vr_rigdata_dir.string, clean, sizeof(clean));
        q_snprintf(out, static_cast<size_t>(size), "%s/%s", com_gamedir, clean);
    }
    else
    {
        q_snprintf(out, static_cast<size_t>(size), "%s", com_gamedir);
    }
}

void rigPath(const char* base, char* out, int size)
{
    char root[MAX_OSPATH];
    rootDir(root, sizeof(root));
    q_snprintf(out, static_cast<size_t>(size), "%s/rigs/%s.rig.txt", root, base);
}

void animDir(const char* base, char* out, int size)
{
    char root[MAX_OSPATH];
    rootDir(root, sizeof(root));
    q_snprintf(out, static_cast<size_t>(size), "%s/anims/%s", root, base);
}

void animPath(const char* base, const char* name, char* out, int size)
{
    char dir[MAX_OSPATH], clean[64];
    animDir(base, dir, sizeof(dir));
    cleanName(name, clean, sizeof(clean));
    q_snprintf(out, static_cast<size_t>(size), "%s/%s.anim.txt", dir, clean);
}

void writeRig(const ModelData& m, const ragdoll::Rig& rig, za::String& out)
{
    out.clear();
    out += "# Quake VR rig overrides (the Rig Studio: docs/vr-port/RIGSTUDIO.md), laid on the rig computed from the model\n";
    out += "qvrrig 1\nmodel ";
    out += m.model;
    out += "\n";
    for(int b = 0; b < rig.numBones; b++)
    {
        const BoneEdit& e = m.bones[b];
        if(!e.on)
        {
            continue;
        }
        out += "bone ";
        out += rig.bones[b].name;
        out += " pivot";
        vec3(out, e.pivot);
        out += " end";
        vec3(out, e.end);
        out += " cone";
        num(out, e.cone);
        out += " twist";
        num(out, e.twist);
        out += " lower";
        num(out, e.lower);
        out += " upper";
        num(out, e.upper);
        out += " hinge";
        vec3(out, e.hinge);
        out += "\n";
    }
    for(const VertWeights& w : m.weights)
    {
        out += "weight ";
        out += za::toString(w.v);
        for(int i = 0; i < w.n; i++)
        {
            out += " ";
            out += w.bone[i] >= 0 && w.bone[i] < rig.numBones ? rig.bones[w.bone[i]].name : "?";
            num(out, w.w[i]);
        }
        out += "\n";
    }
    for(const NamedPose& p : m.poses)
    {
        char name[64];
        cleanName(p.name.cStr(), name, sizeof(name));
        out += "pose ";
        out += name;
        out += "\n";
        for(int b = 0; b < rig.numBones; b++)
        {
            if(!(p.pose.mask & (1u << b)))
            {
                continue;
            }
            out += "  key ";
            out += rig.bones[b].name;
            vec3(out, p.pose.bone[b].rot);
            vec3(out, p.pose.bone[b].off);
            out += "\n";
        }
        out += "end\n";
    }
    for(const AnimOverride& o : m.overrides)
    {
        if(o.tracks.empty())
        {
            continue;
        }
        out += "override ";
        out += o.anim;
        out += "\n";
        for(const Track& t : o.tracks)
        {
            if(!t.keys.empty())
            {
                writeTrack(out, t, rig, "  ");
            }
        }
        out += "end\n";
    }
}

void writeSequence(const ModelData& m, const Sequence& s, const ragdoll::Rig& rig, za::String& out)
{
    char name[64];
    cleanName(s.name.cStr(), name, sizeof(name));
    out.clear();
    out += "# Quake VR animation (the Rig Studio: docs/vr-port/RIGSTUDIO.md): its bones' keys from the rest pose\n";
    out += "qvranim 1\nmodel ";
    out += m.model;
    out += "\nname ";
    out += name;
    out += "\nfps";
    num(out, s.fps);
    out += "\nlength ";
    out += za::toString(s.length);
    out += "\nloop ";
    out += s.loop ? "1" : "0";
    out += "\nblend";
    num(out, s.blendIn);
    num(out, s.blendOut);
    out += "\narms ";
    out += s.arms ? "1" : "0";
    out += "\nhold ";
    out += s.hold ? "1" : "0";
    out += "\n";
    if(!s.base.empty())
    {
        out += "base ";
        out += s.base;
        out += "\n";
    }
    for(const Event& e : s.events)
    {
        char ev[64];
        cleanName(e.name.cStr(), ev, sizeof(ev));
        out += "event ";
        out += za::toString(e.frame);
        out += " ";
        out += ev;
        out += "\n";
    }
    for(const Track& t : s.tracks)
    {
        if(!t.keys.empty())
        {
            writeTrack(out, t, rig, "");
        }
    }
}

bool readRig(const char* text, const ragdoll::Rig& rig, ModelData& m, za::String& error)
{
    bool header = false, ok = true;
    NamedPose* pose = nullptr;
    AnimOverride* over = nullptr;
    TrackReader tr;
    int lineNo = 0;
    forLines(text, [&](const Words& w) {
        lineNo++;
        if(!header)
        {
            if(!w.is(0, "qvrrig"))
            {
                error = "not a rig file (no qvrrig line first)";
                ok = false;
                return false;
            }
            header = true;
            return true;
        }
        if(tr.track)
        {
            if(w.is(0, "end"))
            {
                sortKeys(*tr.track);
                tr.track = nullptr;
            }
            else if(!tr.key(w))
            {
                Con_Printf("rigdata: rig file: a track's line not read: %s\n", w.s(0));
            }
            return true;
        }
        if(pose)
        {
            if(w.is(0, "end"))
            {
                pose = nullptr;
            }
            else if(w.is(0, "key") && w.n >= 8)
            {
                const int b = boneByName(rig, w.s(1));
                if(b >= 0)
                {
                    pose->pose.bone[b].rot = w.v3(2);
                    pose->pose.bone[b].off = w.v3(5);
                    pose->pose.mask |= 1u << b;
                }
            }
            return true;
        }
        if(over)
        {
            if(w.is(0, "end"))
            {
                over = nullptr;
            }
            else if(w.is(0, "track") && w.n >= 2)
            {
                const int b = boneByName(rig, w.s(1));
                over->tracks.pushBack(Track{});
                over->tracks.back().bone = b >= 0 ? b : 0;
                tr.track = &over->tracks.back();
                if(b < 0)
                {
                    Con_Printf("rigdata: rig file: no bone %s (its track read on the first bone)\n", w.s(1));
                }
            }
            return true;
        }
        if(w.is(0, "model"))
        {
            m.model = w.s(1);
        }
        else if(w.is(0, "bone") && w.n >= 2)
        {
            const int b = boneByName(rig, w.s(1));
            if(b < 0)
            {
                Con_Printf("rigdata: rig file line %d: no bone %s in the rig\n", lineNo, w.s(1));
                return true;
            }
            BoneEdit& e = m.bones[b];
            e.on = true;
            e.pivot = rig.bones[b].pivot;
            e.end = rig.bones[b].end;
            e.cone = rig.bones[b].cone / deg;
            e.twist = rig.bones[b].twist / deg;
            e.lower = rig.bones[b].lower / deg;
            e.upper = rig.bones[b].upper / deg;
            e.hinge = rig.bones[b].hinge;
            for(int i = 2; i < w.n; i++)
            {
                if(w.is(i, "pivot")) { e.pivot = w.v3(i + 1); i += 3; }
                else if(w.is(i, "end")) { e.end = w.v3(i + 1); i += 3; }
                else if(w.is(i, "hinge")) { e.hinge = w.v3(i + 1); i += 3; }
                else if(w.is(i, "cone")) { e.cone = w.f(++i); }
                else if(w.is(i, "twist")) { e.twist = w.f(++i); }
                else if(w.is(i, "lower")) { e.lower = w.f(++i); }
                else if(w.is(i, "upper")) { e.upper = w.f(++i); }
            }
        }
        else if(w.is(0, "weight") && w.n >= 4)
        {
            VertWeights vw;
            vw.v = static_cast<int>(strtol(w.s(1), nullptr, 10));
            for(int i = 2; i + 1 < w.n && vw.n < maxWeights; i += 2)
            {
                const int b = boneByName(rig, w.s(i));
                if(b >= 0)
                {
                    vw.bone[vw.n] = b;
                    vw.w[vw.n] = w.f(i + 1);
                    vw.n++;
                }
            }
            if(vw.v >= 0 && vw.v < rig.numVerts && vw.n > 0)
            {
                // (in vertex order; a vertex twice: the later)
                za::SizeT at = 0;
                while(at < m.weights.size() && m.weights[at].v < vw.v)
                {
                    at++;
                }
                if(at < m.weights.size() && m.weights[at].v == vw.v)
                {
                    m.weights[at] = vw;
                }
                else
                {
                    m.weights.insert(m.weights.begin() + static_cast<za::PtrDiffT>(at), vw);
                }
            }
        }
        else if(w.is(0, "pose") && w.n >= 2)
        {
            m.poses.pushBack(NamedPose{});
            pose = &m.poses.back();
            pose->name = w.s(1);
        }
        else if(w.is(0, "override") && w.n >= 2)
        {
            AnimOverride* found = nullptr;
            for(AnimOverride& o : m.overrides)
            {
                found = !strcmp(o.anim.cStr(), w.s(1)) ? &o : found;
            }
            if(!found)
            {
                m.overrides.pushBack(AnimOverride{});
                found = &m.overrides.back();
                found->anim = w.s(1);
            }
            over = found;
        }
        else
        {
            Con_Printf("rigdata: rig file line %d: %s not known\n", lineNo, w.s(0));
        }
        return true;
    });
    if(!header && ok)
    {
        error = "empty";
        return false;
    }
    return ok;
}

bool readSequence(const char* text, const ragdoll::Rig& rig, Sequence& s, za::String& error)
{
    bool header = false, ok = true;
    TrackReader tr;
    forLines(text, [&](const Words& w) {
        if(!header)
        {
            if(!w.is(0, "qvranim"))
            {
                error = "not an animation file (no qvranim line first)";
                ok = false;
                return false;
            }
            header = true;
            return true;
        }
        if(tr.track)
        {
            if(w.is(0, "end"))
            {
                sortKeys(*tr.track);
                tr.track = nullptr;
            }
            else if(!tr.key(w))
            {
                Con_Printf("rigdata: animation %s: a track's line not read: %s\n", s.name.cStr(), w.s(0));
            }
            return true;
        }
        if(w.is(0, "name")) { s.name = w.s(1); }
        else if(w.is(0, "fps")) { s.fps = za::clamp(w.f(1), 0.1f, 240.f); }
        else if(w.is(0, "length")) { s.length = za::clamp(static_cast<int>(strtol(w.s(1), nullptr, 10)), 1, 100000); }
        else if(w.is(0, "loop")) { s.loop = w.f(1) != 0.f; }
        else if(w.is(0, "blend")) { s.blendIn = za::max(w.f(1), 0.f); s.blendOut = za::max(w.f(2), 0.f); }
        else if(w.is(0, "arms")) { s.arms = w.f(1) != 0.f; }
        else if(w.is(0, "hold")) { s.hold = w.f(1) != 0.f; }
        else if(w.is(0, "base")) { s.base = w.s(1); }
        else if(w.is(0, "model")) { }
        else if(w.is(0, "event") && w.n >= 3)
        {
            Event e;
            e.frame = static_cast<int>(strtol(w.s(1), nullptr, 10));
            e.name = w.s(2);
            za::SizeT at = 0;
            while(at < s.events.size() && s.events[at].frame <= e.frame)
            {
                at++;
            }
            s.events.insert(s.events.begin() + static_cast<za::PtrDiffT>(at), e);
        }
        else if(w.is(0, "track") && w.n >= 2)
        {
            const int b = boneByName(rig, w.s(1));
            if(b < 0)
            {
                Con_Printf("rigdata: animation %s: no bone %s (its track skipped)\n", s.name.cStr(), w.s(1));
                s.tracks.pushBack(Track{});
                tr.track = &s.tracks.back(); // (read, then dropped below)
                tr.track->bone = -1;
                return true;
            }
            s.tracks.pushBack(Track{});
            tr.track = &s.tracks.back();
            tr.track->bone = b;
        }
        return true;
    });
    za::vectorEraseIf(s.tracks, [](const Track& t) { return t.bone < 0; });
    if(!header && ok)
    {
        error = "empty";
        return false;
    }
    if(ok && s.name.empty())
    {
        error = "no name";
        return false;
    }
    return ok;
}

const ModelData* of(const qmodel_t* model)
{
    if(!model || model->type != mod_alias || !ragdoll::eligible(model))
    {
        return nullptr;
    }
    Entry* e = entryOf(model, true);
    return e && !e->none ? e->data.get() : nullptr;
}

void publish(const qmodel_t* model, const ModelData& m)
{
    Entry* e = entryOf(model, true);
    if(!e)
    {
        return;
    }
    if(!e->data)
    {
        e->data = za::makeUnique<ModelData>();
    }
    *e->data = m;
    e->none = false;
    recount();
}

void reload(const qmodel_t* model)
{
    Entry* e = entryOf(model, false);
    if(e)
    {
        loadEntry(*e);
        recount();
    }
}

bool anyAnimation()
{
    return regState.animation;
}

void newMap()
{
    registry.entries.clear();
    regState.animation = false;
}

bool applyToRig(qmodel_t* model, const aliashdr_t* hdr, ragdoll::Rig& rig)
{
    if(!model || !hdr)
    {
        return false;
    }
    char base[64], path[MAX_OSPATH];
    baseName(model->name, base, sizeof(base));
    rigPath(base, path, sizeof(path));
    za::String text, err;
    if(!files::exists(path) || !files::readText(path, text))
    {
        return false; // (no file: the computed rig as it is)
    }
    const double t0 = Sys_DoubleTime();
    ModelData m;
    if(!readRig(text.cStr(), rig, m, err))
    {
        Con_Printf("rigdata: %s: %s (not applied)\n", path, err.cStr());
        return false;
    }
    int bones = 0;
    for(int b = 0; b < rig.numBones; b++)
    {
        const BoneEdit& e = m.bones[b];
        if(!e.on)
        {
            continue;
        }
        ragdoll::Bone& bone = rig.bones[b];
        bone.pivot = e.pivot;
        bone.end = e.end;
        bone.cone = e.cone * deg;
        bone.twist = e.twist * deg;
        bone.lower = e.lower * deg;
        bone.upper = e.upper * deg;
        bone.hinge = glm::length(e.hinge) > 1e-4f ? glm::normalize(e.hinge) : bone.hinge;
        bones++;
    }
    uint32_t changed = 0;
    int verts = 0;
    for(const VertWeights& w : m.weights)
    {
        if(w.v < 0 || w.v >= rig.numVerts || w.n <= 0)
        {
            continue;
        }
        int best = 0;
        for(int i = 1; i < w.n; i++)
        {
            best = w.w[i] > w.w[best] ? i : best;
        }
        const uint8_t nb = static_cast<uint8_t>(w.bone[best]);
        uint8_t& vb = rig.vertBone[static_cast<za::SizeT>(w.v)];
        if(vb != nb)
        {
            changed |= (1u << vb) | (1u << nb);
            vb = nb;
            verts++;
        }
    }
    if(changed)
    {
        refit(rig, hdr, changed);
    }
    Con_Printf("rigdata: %s: %d bone overrides, %d vertices on other bones (%.2f ms)\n", model->name, bones, verts,
        (Sys_DoubleTime() - t0) * 1000.0);
    return true;
}

bool sample(const Track& t, float frame, float loopLength, Local& out)
{
    const int n = static_cast<int>(t.keys.size());
    if(n == 0)
    {
        return false;
    }
    const auto& k = t.keys;
    if(n == 1)
    {
        out = k[0].value;
        return true;
    }
    if(loopLength > 0.f)
    {
        frame = frame - za::floor(frame / loopLength) * loopLength;
    }
    // The segment [i, i + 1] holding `frame` (a loop: from the last key round to the first, `loopLength` on).
    int i = -1;
    for(int j = 0; j < n; j++)
    {
        if(k[static_cast<za::SizeT>(j)].frame <= frame)
        {
            i = j;
        }
    }
    float f0, f1;
    int a, b;
    if(i < 0)
    {
        if(loopLength <= 0.f)
        {
            out = k[0].value;
            return true;
        }
        a = n - 1;
        b = 0;
        f0 = k[static_cast<za::SizeT>(a)].frame - loopLength;
        f1 = k[0].frame;
    }
    else if(i == n - 1)
    {
        if(loopLength <= 0.f)
        {
            out = k[static_cast<za::SizeT>(n - 1)].value;
            return true;
        }
        a = n - 1;
        b = 0;
        f0 = k[static_cast<za::SizeT>(a)].frame;
        f1 = k[0].frame + loopLength;
    }
    else
    {
        a = i;
        b = i + 1;
        f0 = k[static_cast<za::SizeT>(a)].frame;
        f1 = k[static_cast<za::SizeT>(b)].frame;
    }
    const Key& ka = k[static_cast<za::SizeT>(a)];
    const Key& kb = k[static_cast<za::SizeT>(b)];
    const float span = za::max(f1 - f0, 1e-4f);
    const float u = za::clamp((frame - f0) / span, 0.f, 1.f);
    if(ka.interp == Interp::Step)
    {
        out = ka.value;
        return true;
    }
    if(ka.interp == Interp::Linear || n < 2)
    {
        out.rot = toRotDeg(glm::slerp(toQuat(ka.value.rot), toQuat(kb.value.rot), u));
        out.off = glm::mix(ka.value.off, kb.value.off, u);
        return true;
    }
    // Smooth: Catmull-Rom through the keys before and after (held at the ends of an open track).
    const auto at = [&](int j) -> const Key& {
        if(loopLength > 0.f)
        {
            return k[static_cast<za::SizeT>(((j % n) + n) % n)];
        }
        return k[static_cast<za::SizeT>(za::clamp(j, 0, n - 1))];
    };
    const int ia = a, ib = (b == 0 && a == n - 1) ? n : a + 1;
    const Key& kp = at(ia - 1);
    const Key& kn = at(ib + 1);
    const float spans[3] = {span, span, span};
    const rigpose::Spline s = rigpose::spline(u, spans, 0.f);
    const glm::quat q[4] = {toQuat(kp.value.rot), toQuat(ka.value.rot), toQuat(kb.value.rot), toQuat(kn.value.rot)};
    out.rot = toRotDeg(rigpose::rotation(q, s));
    out.off = s.w[0] * kp.value.off + s.w[1] * ka.value.off + s.w[2] * kb.value.off + s.w[3] * kn.value.off;
    return true;
}

int animGroups(const aliashdr_t* hdr, AnimGroup* out, int max)
{
    int n = 0;
    for(int f = 0; f < hdr->numframes; f++)
    {
        char name[16];
        q_strlcpy(name, hdr->frames[f].name, sizeof(name));
        int len = static_cast<int>(strlen(name));
        while(len > 0 && name[len - 1] >= '0' && name[len - 1] <= '9')
        {
            name[--len] = 0;
        }
        if(len == 0)
        {
            q_strlcpy(name, "frames", sizeof(name));
        }
        if(n > 0 && !strcmp(out[n - 1].name, name))
        {
            out[n - 1].count++;
            continue;
        }
        if(n >= max)
        {
            break;
        }
        AnimGroup& g = out[n++];
        q_strlcpy(g.name, name, sizeof(g.name));
        g.first = f;
        g.count = 1;
    }
    return n;
}

bool groupOfFrame(const aliashdr_t* hdr, int frame, AnimGroup& out, int& index)
{
    AnimGroup groups[256];
    const int n = animGroups(hdr, groups, 256);
    for(int i = 0; i < n; i++)
    {
        if(frame >= groups[i].first && frame < groups[i].first + groups[i].count)
        {
            out = groups[i];
            index = frame - groups[i].first;
            return true;
        }
    }
    return false;
}

int frameOfName(const aliashdr_t* hdr, const char* name)
{
    for(int f = 0; f < hdr->numframes; f++)
    {
        if(!q_strcasecmp(hdr->frames[f].name, name))
        {
            return f;
        }
    }
    return -1;
}

int poseOfFrame(const aliashdr_t* hdr, int frame)
{
    if(frame < 0 || frame >= hdr->numframes)
    {
        return 0;
    }
    return hdr->frames[frame].firstpose;
}

int frameOfPose(const aliashdr_t* hdr, int pose)
{
    for(int f = 0; f < hdr->numframes; f++)
    {
        if(hdr->frames[f].firstpose == pose && hdr->frames[f].numposes == 1)
        {
            return f;
        }
    }
    return -1;
}

void forward(const ragdoll::Rig& rig, const glm::vec3* pivots, const Local* locals, rigpose::Rigid* F)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        const rigpose::Rigid L = toRigid(locals[b], pivots[b]);
        const int p = rig.bones[b].parent;
        F[b] = (p >= 0 && p < b) ? compose(F[p], L) : L;
    }
}

void localsOf(const ragdoll::Rig& rig, const glm::vec3* pivots, const rigpose::Rigid* P, Local* out)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        const int p = rig.bones[b].parent;
        const rigpose::Rigid L = (p >= 0 && p < b) ? compose(inverse(P[p]), P[b]) : P[b];
        out[b] = fromRigid(L, pivots[b]);
    }
}

void restOf(const ragdoll::Rig& rig, const ModelData* m, glm::vec3* pivots, glm::vec3* ends)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        const bool on = m && m->bones[b].on;
        pivots[b] = on ? m->bones[b].pivot : rig.bones[b].pivot;
        if(ends)
        {
            ends[b] = on ? m->bones[b].end : rig.bones[b].end;
        }
    }
}

namespace
{
void info_f()
{
    // vr_rigdata_info [model]: the registry's models, or one model's rig as applied (its bones' rest joints).
    const char* want = Cmd_Argc() > 1 ? Cmd_Argv(1) : "progs/vrgrunt.mdl";
    qmodel_t* model = Mod_ForName(want, false);
    const ragdoll::Rig* rig = model ? ragdoll::rigFor(model) : nullptr;
    if(!rig)
    {
        Con_Printf("rigdata: no rig for %s\n", want);
        return;
    }
    const ModelData* m = of(model);
    Con_Printf("rigdata: %s: %d bones, %d vertices; files: %s (rig file %s, %d weights, %d poses, %d overrides, %d sequences)\n",
        model->name, rig->numBones, rig->numVerts, m ? "yes" : "none", m && m->haveRigFile ? "yes" : "no",
        m ? static_cast<int>(m->weights.size()) : 0, m ? static_cast<int>(m->poses.size()) : 0,
        m ? static_cast<int>(m->overrides.size()) : 0, m ? static_cast<int>(m->seqs.size()) : 0);
    for(int b = 0; b < rig->numBones; b++)
    {
        int count = 0;
        for(int v = 0; v < rig->numVerts; v++)
        {
            count += rig->vertBone[static_cast<za::SizeT>(v)] == b ? 1 : 0;
        }
        Con_Printf("rigdata:   bone %d %s parent %d pivot %.3f %.3f %.3f end %.3f %.3f %.3f, %d vertices\n", b,
            rig->bones[b].name, rig->bones[b].parent, rig->bones[b].pivot.x, rig->bones[b].pivot.y, rig->bones[b].pivot.z,
            rig->bones[b].end.x, rig->bones[b].end.y, rig->bones[b].end.z, count);
    }
    for(za::SizeT i = 0; m && i < m->seqs.size(); i++)
    {
        const Sequence& s = m->seqs[i];
        Con_Printf("rigdata:   sequence %s: %d frames at %g fps (%.2f s)%s, %d events, %d tracks\n", s.name.cStr(), s.length,
            s.fps, seconds(s), s.loop ? ", looped" : "", static_cast<int>(s.events.size()), static_cast<int>(s.tracks.size()));
    }
}

void reload_f()
{
    newMap();
    Con_Printf("rigdata: the files are read again as each model is next drawn\n");
}
} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_rigdata_info", info_f);
    Cmd_AddCommand("vr_rigdata_reload", reload_f);
}

} // namespace qvr::rigdata
