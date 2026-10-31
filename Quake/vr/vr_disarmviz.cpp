// vr_disarmviz.cpp -- Debug > Show Disarm Volumes (vr_debug_disarm): see vr_disarmviz.hpp.

#include "vr_disarmviz.hpp"

#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_geom.hpp"
#include "vr_held.hpp"
#include "vr_lines.hpp"
#include "vr_physics.hpp"
#include "vr_progs.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/Limits.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>

namespace qvr::disarmviz
{
namespace
{

// The QC's kinds (vr_disarmviz.qc VR_DV_*: keep in step).
enum Kind : int
{
    WeaponHit = 1,    // the held weapon's line, the reach of a blow (VR_AG_HandStrikeSpeed) and a parry's sweep round it
    WeaponShot = 2,   // ... its shot volume (VR_AGS_W_RADIUS x vr_agrunt_shotdisarm_size)
    WeaponNear = 3,   // ... a near miss's (half counts: vr_agrunt_shotdisarm_near more)
    HandShot = 4,     // its hand and forearm's shot volume (VR_AGS_HandLine)
    HandNear = 5,     // ... a near miss's
    GripBlow = 6,     // a body blow this near its grip knocks it out (VR_AG_Hit: VR_AG_DISARM_HAND)
    LimbBlow = 7,     // ... a blow on a limb (positional part 2) this near
    RagPart = 8,      // the ragdoll limb the weapon rides (num: the humanoid; radius: the part)
    PlayerStrike = 9, // a player's hand: the line a strike on the weapon is measured from (its hand to its weapon's muzzle)
    PlayerSweep = 10, // ... its blow's line, the parry's sweep (grip to far end)
    Tug = 11,         // a tug over the weapon: a the hand, b the grip, radius its progress (0..1)
    DirYours = 12,    // the directional parry (QC vr_dirparry.qc): your weapon's parry line (radius 1: it parried)
    DirBlade = 13,    // ... its blade's line where they met
    DirWay = 14,      // ... its blade's way there
};

// The notes' kinds (debugdisarmnote): 0 a state; the hits.
constexpr za::Array<const char*, 6> noteNames{"state", "parry", "disarm", "body blow", "shot", "blocked"};

struct Shape
{
    int num{0};
    int kind{0};
    glm::vec3 a{0.f}, b{0.f};
    float radius{0.f};
};

struct Label
{
    int num{0};
    glm::vec3 at{0.f};
    char text[128]{};
};

struct Hover
{
    int weapon{0};                       // the nearest weapon measured (0: none within a quarter metre)
    float gap{0.f}, allowed{0.f};        // units
    bool takes{false}, swept{false};     // a grip takes it now (only by the sweep over its last places)
    glm::vec3 from{0.f}, to{0.f};        // the fist's nearest sphere's middle, its surface's nearest point
};

constexpr int maxShapes = 384;
constexpr int maxLabels = 32;
constexpr double shownFresh = 0.25; // s: a frame's shapes older than this are gone (turned off, a map change)
constexpr double hitShown = 2.0;    // s: the last hit's mark

struct Viz
{
    double time{-1.0}; // the server frame of the QC's shapes and labels
    za::Array<Shape, maxShapes> shapes{};
    int shapeCount{0};
    za::Array<Label, maxLabels> labels{};
    int labelCount{0};
    za::Vector<glm::vec4> fist[2]; // each hand's fist spheres (world), as the grab tests them
    Hover hover[2]{};
    int printed[2]{};              // vr_debug_disarm 2: the hover last printed (its weapon, negative: near only)
    double fistTime{-1.0};
    int hitNum{0}, hitWhat{0};
    glm::vec3 hitAt{0.f};
    char hitText[128]{};
    double hitTime{-1.0};
};
Viz viz;

void beginFrame()
{
    const double now = sv.qcvm.time;
    if(viz.time != now)
    {
        viz.time = now;
        viz.shapeCount = 0;
        viz.labelCount = 0;
    }
}

[[nodiscard]] bool fresh(double t)
{
    const double now = sv.qcvm.time;
    return t >= 0.0 && now >= t && now - t <= shownFresh;
}

// ---- drawing ----

void ring(const glm::vec3& c, const glm::vec3& u, const glm::vec3& v, float r, const glm::vec4& colour, float width,
    float from = 0.f, float to = 2.f * geom::pi)
{
    constexpr int sides = 16;
    for(int i = 0; i < sides; i++)
    {
        const float t0 = from + (to - from) * static_cast<float>(i) / sides;
        const float t1 = from + (to - from) * static_cast<float>(i + 1) / sides;
        lines::line(c + r * (u * za::cos(t0) + v * za::sin(t0)), c + r * (u * za::cos(t1) + v * za::sin(t1)), width, colour,
            colour);
    }
}

void sphere(const glm::vec3& c, float r, const glm::vec4& colour, float width)
{
    const glm::vec3 x{1.f, 0.f, 0.f}, y{0.f, 1.f, 0.f}, z{0.f, 0.f, 1.f};
    ring(c, x, y, r, colour, width);
    ring(c, x, z, r, colour, width);
    ring(c, y, z, r, colour, width);
}

// A capsule of `r` round a..b: its end rings, their caps' half rings, four sides.
void capsule(const glm::vec3& a, const glm::vec3& b, float r, const glm::vec4& colour, float width)
{
    const glm::vec3 d = b - a;
    const float len = glm::length(d);
    if(len < 0.01f)
    {
        sphere(a, r, colour, width);
        return;
    }
    const glm::vec3 w = d / len;
    const glm::vec3 u = glm::normalize(glm::cross(w, za::fabs(w.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f}));
    const glm::vec3 v = glm::cross(w, u);
    ring(a, u, v, r, colour, width);
    ring(b, u, v, r, colour, width);
    ring(a, u, -w, r, colour, width, 0.f, geom::pi);
    ring(a, v, -w, r, colour, width, 0.f, geom::pi);
    ring(b, u, w, r, colour, width, 0.f, geom::pi);
    ring(b, v, w, r, colour, width, 0.f, geom::pi);
    for(const glm::vec3& o : {u, -u, v, -v})
    {
        lines::line(a + o * r, b + o * r, width, colour, colour);
    }
}

void label(const char* text, const glm::vec3& at, const glm::vec3& eye)
{
    const glm::vec3 to = at - eye;
    text3d::queueOverlay(text, at, glm::vec3{0.f, glm::degrees(za::atan2(to.y, to.x)), 0.f}, 0.05f);
}

const glm::vec4 orange{1.f, 0.55f, 0.1f, 0.9f};
const glm::vec4 yellow{1.f, 0.95f, 0.15f, 1.f};
const glm::vec4 green{0.2f, 1.f, 0.3f, 1.f};
const glm::vec4 magenta{1.f, 0.25f, 1.f, 0.8f};
const glm::vec4 cyan{0.2f, 0.9f, 1.f, 0.8f};
const glm::vec4 red{1.f, 0.15f, 0.15f, 0.85f};
const glm::vec4 white{1.f, 1.f, 1.f, 0.85f};
const glm::vec4 blue{0.35f, 0.55f, 1.f, 0.9f};
const glm::vec4 grey{0.8f, 0.8f, 0.8f, 0.35f};

[[nodiscard]] glm::vec4 faint(glm::vec4 c)
{
    c.a *= 0.35f;
    return c;
}

[[nodiscard]] glm::vec4 hitColour(int what)
{
    switch(what)
    {
    case 1: return cyan;
    case 2: return red;
    case 4: return magenta;
    case 5: return yellow;
    default: return white;
    }
}

[[nodiscard]] bool tugged(int num)
{
    for(int i = 0; i < viz.shapeCount; i++)
    {
        if(viz.shapes[static_cast<za::SizeT>(i)].kind == Tug && viz.shapes[static_cast<za::SizeT>(i)].num == num)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool hovered(int num)
{
    if(!fresh(viz.fistTime))
    {
        return false;
    }
    return (viz.hover[0].weapon == num && viz.hover[0].takes) || (viz.hover[1].weapon == num && viz.hover[1].takes);
}

void drawShape(const Shape& s)
{
    constexpr float thin = 0.12f, thick = 0.3f;
    switch(s.kind)
    {
    case WeaponHit:
    {
        const bool tug = tugged(s.num), hover = hovered(s.num);
        capsule(s.a, s.b, s.radius, tug ? green : hover ? yellow : orange, tug || hover ? thick : thin * 1.5f);
        lines::line(s.a, s.b, thick, tug ? green : hover ? yellow : orange, tug ? green : hover ? yellow : orange);
        break;
    }
    case WeaponShot: capsule(s.a, s.b, s.radius, magenta, thin); break;
    case WeaponNear: capsule(s.a, s.b, s.radius, faint(magenta), thin); break;
    case HandShot: capsule(s.a, s.b, s.radius, cyan, thin); break;
    case HandNear: capsule(s.a, s.b, s.radius, faint(cyan), thin); break;
    case GripBlow: sphere(s.a, s.radius, red, thin); break;
    case LimbBlow: sphere(s.a, s.radius, faint(red), thin); break;
    case RagPart: (void)box3d::ragdollPartDraw(s.num, static_cast<int>(s.radius), white, thin * 1.5f); break;
    case PlayerStrike:
        lines::line(s.a, s.b, thin * 2.f, white, white);
        lines::point(s.a, 1.2f, white);
        break;
    case PlayerSweep: lines::line(s.a, s.b, thin * 2.f, blue, blue); break;
    case DirYours: lines::line(s.a, s.b, thick, s.radius > 0.5f ? green : red, s.radius > 0.5f ? green : red); break;
    case DirBlade: lines::line(s.a, s.b, thick, white, white); break;
    case DirWay:
        lines::line(s.a, s.b, thin * 2.f, yellow, yellow);
        lines::point(s.a, 1.5f, yellow);
        break;
    case Tug:
    {
        // The hand to the grip, and its progress: a bar over the grip, filled green to the share won.
        lines::line(s.a, s.b, thick, green, green);
        const glm::vec3 bar = s.b + glm::vec3{0.f, 0.f, 6.f};
        const glm::vec3 span{0.f, 0.f, 8.f};
        lines::line(bar, bar + span, thick, grey, grey);
        lines::line(bar, bar + span * za::min(za::max(s.radius, 0.f), 1.f), thick * 2.f, green, green);
        break;
    }
    default: break;
    }
}

void drawFists()
{
    if(!fresh(viz.fistTime))
    {
        return;
    }
    for(int h = 0; h < 2; h++)
    {
        const Hover& hv = viz.hover[h];
        const glm::vec4 c = hv.takes ? yellow : grey;
        for(const glm::vec4& s : viz.fist[h])
        {
            lines::point(glm::vec3{s}, 2.f * s.w, glm::vec4{c.r, c.g, c.b, hv.takes ? 0.45f : 0.2f});
        }
        if(hv.weapon > 0)
        {
            // The fist's nearest sphere to the weapon's nearest surface point (green within the allowance).
            const glm::vec4 lc = hv.takes ? green : faint(white);
            lines::line(hv.from, hv.to, 0.15f, lc, lc);
            lines::point(hv.to, 0.8f, lc);
        }
    }
}

} // namespace

bool on()
{
    return vr_debug_disarm.value != 0.f;
}

void PF_debugdisarm()
{
    if(!on())
    {
        return;
    }
    beginFrame();
    if(viz.shapeCount >= maxShapes)
    {
        return;
    }
    Shape& s = viz.shapes[static_cast<za::SizeT>(viz.shapeCount++)];
    s.num = NUM_FOR_EDICT(G_EDICT(OFS_PARM0));
    s.kind = static_cast<int>(G_FLOAT(OFS_PARM1));
    s.a = geom::vec(G_VECTOR(OFS_PARM2));
    s.b = geom::vec(G_VECTOR(OFS_PARM3));
    s.radius = G_FLOAT(OFS_PARM4);
}

void PF_debugdisarmnote()
{
    if(!on())
    {
        return;
    }
    const int num = NUM_FOR_EDICT(G_EDICT(OFS_PARM0));
    const int what = static_cast<int>(G_FLOAT(OFS_PARM1));
    const glm::vec3 at = geom::vec(G_VECTOR(OFS_PARM2));
    const char* text = G_STRING(OFS_PARM3);
    if(what == 0)
    {
        beginFrame();
        if(viz.labelCount < maxLabels)
        {
            Label& l = viz.labels[static_cast<za::SizeT>(viz.labelCount++)];
            l.num = num;
            l.at = at;
            q_strlcpy(l.text, text, sizeof(l.text));
        }
        return;
    }
    viz.hitNum = num;
    viz.hitWhat = what;
    viz.hitAt = at;
    viz.hitTime = sv.qcvm.time;
    q_snprintf(viz.hitText, sizeof(viz.hitText), "%s\n%s", what > 0 && what < static_cast<int>(noteNames.size())
        ? noteNames[static_cast<za::SizeT>(what)] : "hit", text);
    if(vr_debug_disarm.value >= 2.f)
    {
        Con_Printf("disarmviz: %s on %d at %.1f %.1f %.1f: %s\n", what > 0 && what < static_cast<int>(noteNames.size())
            ? noteNames[static_cast<za::SizeT>(what)] : "hit", num, static_cast<double>(at.x), static_cast<double>(at.y),
            static_cast<double>(at.z), text);
    }
}

void serverFrame()
{
    edict_t* player = on() && qcvm == &sv.qcvm && fresh(viz.time) ? progs::localPlayer() : nullptr;
    if(!player)
    {
        viz.fistTime = -1.0;
        return;
    }
    const progs::FieldOffsets& fo = progs::fields();
    if(player->free || fo.handpos < 0 || fo.offhandpos < 0)
    {
        return;
    }
    for(int h = 0; h < 2; h++)
    {
        Hover& hv = viz.hover[h];
        hv = Hover{};
        viz.fist[h].clear();
        if(held::fist(h).empty())
        {
            continue;
        }
        held::fistInWorld(h, progs::fieldVec(player, h == 0 ? fo.offhandpos : fo.handpos),
            progs::fieldVec(player, h == 0 ? fo.offhandrot : fo.handrot), viz.fist[h]);
        // Each weapon the QC drew this frame, measured as the grab measures it: the nearest kept.
        float best = ZA_FLOAT_MAX;
        for(int i = 0; i < viz.shapeCount; i++)
        {
            const Shape& s = viz.shapes[static_cast<za::SizeT>(i)];
            if(s.kind != WeaponHit || s.num <= 0 || s.num >= qcvm->num_edicts)
            {
                continue;
            }
            float allowed = 0.f;
            bool swept = false;
            const float gap = physics::armedGrabGap(EDICT_NUM(s.num), player, h, allowed, swept);
            if(gap < best)
            {
                best = gap;
                hv.weapon = s.num;
                hv.gap = gap;
                hv.allowed = allowed;
                hv.takes = gap <= allowed;
                hv.swept = swept;
            }
        }
        if(hv.weapon > 0)
        {
            // (Where, for the line: its sphere nearest the surface, and that surface's point.)
            edict_t* w = EDICT_NUM(hv.weapon);
            float least = ZA_FLOAT_MAX;
            for(const glm::vec4& s : viz.fist[h])
            {
                glm::vec3 near{0.f};
                const float d = held::surfaceDistance(w, glm::vec3{s}, &near);
                if(d >= 0.f && d - s.w < least)
                {
                    least = d - s.w;
                    hv.from = glm::vec3{s};
                    hv.to = near;
                }
            }
        }
        const int now = hv.takes ? hv.weapon : -hv.weapon;
        if(vr_debug_disarm.value >= 2.f && now != viz.printed[h])
        {
            const float cm = 100.f / units::metresToUnits();
            Con_Printf("disarmviz: %s hand %s %d (gap %.2f cm, allowed %.2f cm%s)\n", h == 0 ? "off" : "main",
                hv.weapon == 0 ? "near no weapon" : hv.takes ? "takes weapon" : "near weapon", hv.weapon,
                static_cast<double>(hv.weapon ? hv.gap * cm : 0.f), static_cast<double>(hv.allowed * cm),
                hv.swept ? ", by its sweep" : "");
        }
        viz.printed[h] = now;
    }
    viz.fistTime = sv.qcvm.time;
}

void draw(const glm::vec3& eye)
{
    if(!on() || !sv.active)
    {
        return;
    }
    if(fresh(viz.time))
    {
        for(int i = 0; i < viz.shapeCount; i++)
        {
            drawShape(viz.shapes[static_cast<za::SizeT>(i)]);
        }
        const float cm = 100.f / units::metresToUnits();
        for(int i = 0; i < viz.labelCount; i++)
        {
            // Its state, and each hand near it: how far its fist is, and whether a grip takes it.
            const Label& l = viz.labels[static_cast<za::SizeT>(i)];
            char text[256];
            int n = q_snprintf(text, sizeof(text), "%s", l.text);
            for(int h = 0; h < 2 && fresh(viz.fistTime); h++)
            {
                const Hover& hv = viz.hover[h];
                if(hv.weapon == l.num && n > 0 && n < static_cast<int>(sizeof(text)))
                {
                    n += q_snprintf(text + n, sizeof(text) - static_cast<size_t>(n), "\n%s: %s %.1f cm (%.1f)%s",
                        h == 0 ? "off" : "main", hv.takes ? "GRIP TAKES" : "fist", static_cast<double>(hv.gap * cm),
                        static_cast<double>(hv.allowed * cm), hv.swept ? " swept" : "");
                }
            }
            label(text, l.at, eye);
        }
    }
    drawFists();
    const double now = sv.qcvm.time;
    if(viz.hitTime >= 0.0 && now >= viz.hitTime && now - viz.hitTime <= hitShown)
    {
        const glm::vec4 c = hitColour(viz.hitWhat);
        const glm::vec3 x{3.f, 0.f, 0.f}, y{0.f, 3.f, 0.f}, z{0.f, 0.f, 3.f};
        lines::point(viz.hitAt, 2.5f, c);
        lines::line(viz.hitAt - x, viz.hitAt + x, 0.25f, c, c);
        lines::line(viz.hitAt - y, viz.hitAt + y, 0.25f, c, c);
        lines::line(viz.hitAt - z, viz.hitAt + z, 0.25f, c, c);
        label(viz.hitText, viz.hitAt + glm::vec3{0.f, 0.f, 5.f}, eye);
    }
}

} // namespace qvr::disarmviz
