// vr_agpose.cpp -- armed humanoids' pose keys (vr_agpose.hpp).

#include "vr_agpose.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/quaternion.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace qvr;

namespace
{

// The shipped keys (vr_agrunt_pose_dump prints rows to paste here): the key's code (agpose::code: kind * 10000 +
// class * 1000 + pose * 10 + phase), then its parts as agpose::Part orders them. None yet: QC's poses as they are.
struct ShippedRow
{
    int code;
    float v[agpose::NumParts * 3];
};
constexpr ShippedRow shipped[] = {
    {-1, {}}, // (the end)
};

// QC's grunt-made fit of each kind (vr_armedmon.qc VR_AM_Row: vr_am_fit_scale): a place on the grunt times this is the
// enforcer's.
constexpr glm::vec3 fitScale[agpose::numKinds] = {glm::vec3{1.f, 1.f, 1.f}, glm::vec3{1.f, 1.35f, 1.1f}};

struct Entry
{
    int code{-1};
    agpose::Key k;
};

constexpr int maxEntries = 512;

struct State
{
    za::Vector<Entry> over; // vr_agrunt_poses parsed
    za::String parsed;
    bool everParsed{false};
    int previewCode{-1};
    agpose::Key previewKey;
};
State state;

[[nodiscard]] bool validCode(int c)
{
    const int kind = c / 10000, cls = (c / 1000) % 10, pose = (c / 10) % 100, phase = c % 10;
    return c >= 0 && kind < agpose::numKinds && cls < agpose::NumClasses && pose < agpose::NumPoses && phase < agpose::numPhases;
}

void parse()
{
    const char* text = vr_agrunt_poses.string ? vr_agrunt_poses.string : "";
    if(state.everParsed && strcmp(state.parsed.cStr(), text) == 0)
    {
        return;
    }
    state.everParsed = true;
    state.parsed = za::String{text};
    state.over.clear();
    const char* p = text;
    while(*p && static_cast<int>(state.over.size()) < maxEntries)
    {
        Entry e;
        char* end = nullptr;
        e.code = static_cast<int>(strtol(p, &end, 10));
        bool ok = end != p;
        p = end;
        for(int i = 0; i < agpose::NumParts * 3 && ok; i++)
        {
            const float f = strtof(p, &end);
            ok = end != p;
            e.k.v[i / 3][i % 3] = f;
            p = end;
        }
        if(ok && validCode(e.code))
        {
            state.over.pushBack(e);
        }
        const char* semi = strchr(p, ';');
        if(!semi)
        {
            break;
        }
        p = semi + 1;
    }
}

void write()
{
    za::String text;
    for(const Entry& e : state.over)
    {
        text += va("%d", e.code);
        for(int i = 0; i < agpose::NumParts * 3; i++)
        {
            text += va(" %g", static_cast<double>(e.k.v[i / 3][i % 3]));
        }
        text += ";";
    }
    Cvar_SetQuick(&vr_agrunt_poses, text.cStr());
    parse();
}

[[nodiscard]] bool shippedOf(int code, agpose::Key& out)
{
    for(const ShippedRow& r : shipped)
    {
        if(r.code == code && r.code >= 0)
        {
            for(int i = 0; i < agpose::NumParts * 3; i++)
            {
                out.v[i / 3][i % 3] = r.v[i];
            }
            return true;
        }
    }
    return false;
}

[[nodiscard]] float rounded(float f, float per)
{
    return static_cast<float>(static_cast<int>(f * per + (f < 0.f ? -0.5f : 0.5f))) / per;
}

[[nodiscard]] bool isTurn(int part)
{
    return part == agpose::RTurn || part == agpose::LTurn || part == agpose::Torso;
}

[[nodiscard]] bool same(const agpose::Key& a, const agpose::Key& b)
{
    for(int i = 0; i < agpose::NumParts; i++)
    {
        const float eps = isTurn(i) ? 0.05f : 0.005f;
        for(int c = 0; c < 3; c++)
        {
            if(za::abs(a.v[i][c] - b.v[i][c]) > eps)
            {
                return false;
            }
        }
    }
    return true;
}

void dropCode(int code)
{
    za::Vector<Entry> kept;
    for(const Entry& e : state.over)
    {
        if(e.code != code)
        {
            kept.pushBack(e);
        }
    }
    state.over = static_cast<za::Vector<Entry>&&>(kept);
}

// QC constants that are a stance's place (the grunt's: vr_agarms' pose places them from its chest, fitted): the key's
// part moves them by its change, exactly (its direction turned by the part's turn).
struct QcConstant
{
    int cls, pose, part;
    const char* name;
    glm::vec3 value;
    const char* fwName; // ("" none)
    glm::vec3 fw;
};
constexpr QcConstant qcConstants[] = {
    {agpose::Melee, agpose::Carry, agpose::RPos, "VR_AGP_CARRY_R", {6.f, -9.f, 3.f}, "VR_AGP_CARRY_R_FW", {0.95f, -0.1f, 0.15f}},
    {agpose::Melee2, agpose::Carry, agpose::RPos, "VR_AGP_PORT", {9.f, -5.f, 3.f}, "VR_AGP_PORT_FW", {0.35f, 0.55f, 0.75f}},
    {agpose::Melee, agpose::Aim, agpose::RPos, "VR_AGP_READY", {10.f, -7.f, 4.f}, "VR_AGP_READY_FW", {0.3f, 0.1f, 0.95f}},
    {agpose::Melee, agpose::Aim, agpose::LPos, "VR_AGP_FREE_L", {7.f, 9.f, 4.f}, "", {}},
    {agpose::Fists, agpose::Aim, agpose::RPos, "VR_AGP_GUARD_R", {9.f, -5.f, 13.f}, "", {}},
    {agpose::Fists, agpose::Aim, agpose::LPos, "VR_AGP_GUARD_L", {11.f, 5.f, 15.f}, "", {}},
};

[[nodiscard]] glm::vec3 turned(const glm::vec3& v, const glm::vec3& rotDeg)
{
    const float a = glm::length(rotDeg);
    if(a < 1e-4f)
    {
        return v;
    }
    return glm::angleAxis(a * (3.14159265f / 180.f), rotDeg / a) * v;
}

void dump_f()
{
    za::String text;
    agpose::dumpText(text);
    Con_Printf("%s", text.cStr());
}

} // namespace

namespace qvr::agpose
{

bool get(int c, Key& out)
{
    out = Key{};
    if(c == state.previewCode && c >= 0)
    {
        out = state.previewKey;
        return true;
    }
    parse();
    for(const Entry& e : state.over)
    {
        if(e.code == c)
        {
            out = e.k;
            return true;
        }
    }
    return shippedOf(c, out);
}

Key value(int c)
{
    Key k;
    (void)get(c, k);
    return k;
}

void setPreview(int c, const Key& k)
{
    state.previewCode = c;
    state.previewKey = k;
}

void clearPreview()
{
    state.previewCode = -1;
}

void set(int c, const Key& k)
{
    if(!validCode(c))
    {
        return;
    }
    parse();
    Key r;
    for(int i = 0; i < NumParts; i++)
    {
        for(int a = 0; a < 3; a++)
        {
            r.v[i][a] = rounded(k.v[i][a], isTurn(i) ? 10.f : 100.f);
        }
    }
    Key ship;
    const bool hasShip = shippedOf(c, ship);
    dropCode(c);
    if(!same(r, hasShip ? ship : Key{}) && static_cast<int>(state.over.size()) < maxEntries)
    {
        state.over.pushBack(Entry{c, r});
    }
    write();
}

void resetPose(int kind, int cls, int pose)
{
    parse();
    for(int ph = 0; ph < numPhases; ph++)
    {
        dropCode(code(kind, cls, pose, ph));
    }
    write();
}

int copyKind(int from, int to)
{
    if(from < 0 || from >= numKinds || to < 0 || to >= numKinds || from == to)
    {
        return 0;
    }
    parse();
    za::Vector<Entry> kept;
    for(const Entry& e : state.over)
    {
        if(e.code / 10000 != to)
        {
            kept.pushBack(e);
        }
    }
    int n = 0;
    const glm::vec3 s = fitScale[to] / fitScale[from];
    for(const Entry& e : state.over)
    {
        if(e.code / 10000 != from)
        {
            continue;
        }
        Entry c = e;
        c.code = e.code - from * 10000 + to * 10000;
        c.k.v[RPos] *= s;
        c.k.v[LPos] *= s;
        kept.pushBack(c);
        n++;
    }
    state.over = static_cast<za::Vector<Entry>&&>(kept);
    write();
    return n;
}

void dumpText(za::String& out)
{
    parse();
    out += "// vr_agrunt_pose_dump: rows for Quake/vr/vr_agpose.cpp's shipped[] (the changes laid on QC's poses):\n";
    out += "// {code (kind*10000 + class*1000 + pose*10 + phase), {right place, right turn, left place, left turn,\n";
    out += "//  right elbow's pole, left elbow's pole, torso yaw pitch roll}}\n";
    for(const Entry& e : state.over)
    {
        const int kind = e.code / 10000, cls = (e.code / 1000) % 10, pose = (e.code / 10) % 100, phase = e.code % 10;
        out += va("    {%d, {", e.code);
        for(int i = 0; i < NumParts * 3; i++)
        {
            out += va("%s%.*ff", i ? ", " : "", isTurn(i / 3) ? 1 : 2, static_cast<double>(e.k.v[i / 3][i % 3]));
        }
        out += va("}}, // %s, %s, %s%s%s\n", kind == 0 ? "grunt" : "enforcer", className(cls), poseName(cls, pose),
            isStrike(pose) ? ", " : "", isStrike(pose) ? phaseName(phase) : "");
        // (a stance whose place is one QC constant: that constant moved, instead of the row; the grunt's units)
        for(const QcConstant& q : qcConstants)
        {
            if(q.cls != cls || q.pose != pose || phase != 0)
            {
                continue;
            }
            const glm::vec3 d = e.k.v[q.part] / fitScale[kind];
            const glm::vec3 v = q.value + d;
            out += va("    //   QC instead (%s): vector %s = '%.2f %.2f %.2f';", kind == 0 ? "grunt" : "enforcer", q.name,
                static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z));
            if(q.fwName[0])
            {
                const glm::vec3 fw = turned(glm::normalize(q.fw), e.k.v[q.part + 1]);
                out += va(" vector %s = '%.2f %.2f %.2f';", q.fwName, static_cast<double>(fw.x), static_cast<double>(fw.y),
                    static_cast<double>(fw.z));
            }
            out += "\n";
        }
    }
    if(state.over.empty())
    {
        out += "// (no pose keys changed)\n";
    }
}

const char* className(int cls)
{
    switch(cls)
    {
        case Fists: return "fists";
        case Gun: return "a gun";
        case Melee: return "a one-handed melee weapon";
        case Melee2: return "a two-handed melee weapon";
        case Saw: return "a chainsaw";
        case TwoMelee: return "two melee weapons";
        case TwoGuns: return "two guns";
        default: return "?";
    }
}

const char* poseName(int cls, int pose)
{
    switch(pose)
    {
        case Carry: return cls == Fists ? "Idle" : cls == Melee2 ? "Port Arms" : "Idle / Carry";
        case Aim: return cls == Fists ? "Guard" : cls == Gun || cls == TwoGuns ? "Aim" : "Ready";
        case Run: return "Run";
        case Pickup: return "Pickup";
        case Jab: return "Jab";
        case Cross: return "Cross";
        case Hook: return "Hook";
        case Uppercut: return "Uppercut";
        case Overhead: return "Overhead";
        case Diagonal: return "Diagonal";
        case Horizontal: return "Horizontal";
        case Backhand: return "Backhand";
        case SawThrust: return "Saw Thrust";
        case Dual: return "Dual Strike";
        default: return "?";
    }
}

const char* phaseName(int phase)
{
    return phase == 0 ? "wind-up" : phase == 1 ? "blow" : "follow-through";
}

int posesOf(int cls, int* out, int max)
{
    int n = 0;
    const auto add = [&](int p) {
        if(n < max)
        {
            out[n++] = p;
        }
    };
    add(Carry);
    add(Aim);
    add(Run);
    if(cls != Fists)
    {
        add(Pickup);
    }
    if(cls == Fists)
    {
        add(Jab);
        add(Cross);
        add(Hook);
        add(Uppercut);
    }
    if(cls == Melee || cls == Melee2 || cls == TwoMelee)
    {
        add(Overhead);
        add(Diagonal);
        add(Horizontal);
        add(Backhand);
    }
    if(cls == Saw)
    {
        add(SawThrust);
    }
    if(cls == TwoMelee)
    {
        add(Dual);
    }
    return n;
}

void registerCommands()
{
    Cmd_AddCommand("vr_agrunt_pose_dump", dump_f);
}

} // namespace qvr::agpose
