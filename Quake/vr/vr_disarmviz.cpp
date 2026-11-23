// vr_disarmviz.cpp -- Debug > Show Disarm Volumes (vr_debug_disarm): see vr_disarmviz.hpp.

#include "vr_disarmviz.hpp"

#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_disarmhull.hpp"
#include "vr_geom.hpp"
#include "vr_held.hpp"
#include "vr_lines.hpp"
#include "vr_mpavatar.hpp"
#include "vr_physics.hpp"
#include "vr_progs.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/Limits.hpp"
#include "Zancle/Base/SizeT.hpp"
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
    // (The volumes: hulls of what is drawn, vr_disarmhull.hpp; radius: the units each is grown by. A hand's: num the
    // humanoid, a.x its side.)
    WeaponHit = 1,    // the held weapon's strike volume (vr_agrunt_vol_strike), and its line a..b (the parry's)
    WeaponShot = 2,   // ... its shot volume (vr_agrunt_vol_shot_weapon)
    WeaponNear = 3,   // ... a near miss's (half counts: vr_agrunt_shotdisarm_near more)
    HandShot = 4,     // the hand holding it and its forearm: their shot volume (vr_agrunt_vol_shot_hand)
    HandNear = 5,     // ... a near miss's
    GripBlow = 6,     // the weapon's body blow volume (VR_AG_Hit: vr_agrunt_vol_blow)
    LimbBlow = 7,     // ... the hand's
    RagPart = 8,      // the ragdoll limb the weapon rides (num: the humanoid; radius: the part)
    PlayerStrike = 9, // a player's hand: the line a strike on the weapon is measured from (its hand to its weapon's muzzle)
    PlayerSweep = 10, // ... its blow's line, the parry's sweep (grip to far end)
    Tug = 11,         // a tug over the weapon: a the hand, b the grip, radius its progress (0..1)
    DirYours = 12,    // the directional parry (QC vr_dirparry.qc): your weapon's parry line (radius 1: it parried)
    DirBlade = 13,    // ... its blade's line where they met
    DirWay = 14,      // ... its blade's way there
    HandHit = 20,     // the hand holding it: its strike volume (vr_agrunt_vol_strike)
    // (bots4) A player's held weapon (a bot's, a human's; num the player): capsules a..b grown by radius.
    PlayerGrab = 21,  // another player's grab (VR_PDG_Gap: its line, vr_player_disarm_reach)
    PlayerSpot = 22,  // ... a hotspot of it (a = b)
    PlayerShotW = 23, // a shot on it (VR_PD_Meet: its line)
    PlayerShotH = 24, // ... on the hand holding it (a = b)
};

// (bots4) The QC's radii round a weapon's line and a hand (vr_armedgrunt_shot.qc VR_AGS_W_RADIUS, VR_AGS_H_RADIUS: keep
// in step), for a client's rebuild of another player's shot volumes.
constexpr float shotWeaponRadius = 3.f;
constexpr float shotHandRadius = 3.5f;

// The notes' kinds (debugdisarmnote): 0 a state; the hits.
constexpr za::Array<const char*, 7> noteNames{"state", "parry", "disarm", "body blow", "shot", "blocked", "tug"};
constexpr int tugNote = 6;

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
    char text[512]{}; // (a tug's readout: several lines)
};

// A tug's verdict and its last readout (debugdisarmnote 6), kept tugShown s over where it ended.
struct Verdict
{
    glm::vec3 at{0.f};
    char text[640]{};
    double time{-1.0};
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
constexpr double tugShown = 4.0;    // s: a tug's verdict
constexpr int maxVerdicts = 4;

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
    za::Array<Verdict, maxVerdicts> verdicts{};
    int nextVerdict{0};
    // (bots4) Each player's volumes as last drawn (the harness's mp_debugviz line): the host frame, how many, the first
    // grab capsule's ends.
    struct Shown
    {
        int frame{-1};
        int count{0};
        glm::vec3 a{0.f}, b{0.f};
    };
    za::Array<Shown, MAX_SCOREBOARD + 1> players{};
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

// Weapon `num`'s volume (its drawn model's hull where it is drawn now) grown by `grow`.
void weaponHull(int num, float grow, const glm::vec4& colour, float width)
{
    disarmhull::Placed p;
    if(num > 0 && num < sv.qcvm.num_edicts && disarmhull::weapon(EDICT_NUM(num), p))
    {
        disarmhull::draw(p, grow, colour, width);
    }
}

// Humanoid s.num's hand on side s.a.x and its forearm as drawn now, their two hulls grown by s.radius.
void handHull(const Shape& s, const glm::vec4& colour, float width)
{
    disarmhull::HandPlaced p;
    if(s.num > 0 && s.num < sv.qcvm.num_edicts && disarmhull::handShown(EDICT_NUM(s.num), s.a.x > 0.5f ? 1 : 0, p))
    {
        disarmhull::draw(p, s.radius, colour, width);
    }
}

// (bots4) A wire capsule a..b of radius r (a sphere when they meet): a ring round each end across its axis, the four
// sides, and each end's two half rings away from the other end.
void capsule(const glm::vec3& a, const glm::vec3& b, float r, const glm::vec4& colour, float width)
{
    constexpr int steps = 16;
    const glm::vec3 d = b - a;
    const float len = glm::length(d);
    const glm::vec3 axis = len > 0.01f ? d / len : glm::vec3{0.f, 0.f, 1.f};
    const glm::vec3 helper = za::abs(axis.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f};
    const glm::vec3 u = glm::normalize(glm::cross(axis, helper)), v = glm::cross(axis, u);
    const auto arc = [&](const glm::vec3& c, const glm::vec3& x, const glm::vec3& y, float from, float to) {
        for(int i = 0; i < steps; i++)
        {
            const float t0 = from + (to - from) * static_cast<float>(i) / steps;
            const float t1 = from + (to - from) * static_cast<float>(i + 1) / steps;
            lines::line(c + (x * za::cos(t0) + y * za::sin(t0)) * r, c + (x * za::cos(t1) + y * za::sin(t1)) * r, width,
                colour, colour);
        }
    };
    constexpr float pi = 3.14159265f;
    arc(a, u, v, 0.f, 2.f * pi);
    arc(b, u, v, 0.f, 2.f * pi);
    arc(a, u, -axis, 0.f, pi);
    arc(a, v, -axis, 0.f, pi);
    arc(b, u, axis, 0.f, pi);
    arc(b, v, axis, 0.f, pi);
    if(len > 0.01f)
    {
        for(const glm::vec3& side : {u, -u, v, -v})
        {
            lines::line(a + side * r, b + side * r, width, colour, colour);
        }
    }
}

// (bots4) Player `num`'s volume of `kind` (PlayerGrab...), drawn and counted (the harness's line). Not the viewer's own.
void playerVolume(int num, int kind, const glm::vec3& a, const glm::vec3& b, float r)
{
    if(num <= 0 || num > MAX_SCOREBOARD || num == cl.viewentity)
    {
        return;
    }
    constexpr float thin = 0.12f;
    switch(kind)
    {
    case PlayerGrab: capsule(a, b, r, orange, thin * 1.5f); break;
    case PlayerSpot: capsule(a, b, r, faint(orange), thin); break;
    case PlayerShotW: capsule(a, b, r, magenta, thin); break;
    case PlayerShotH: capsule(a, b, r, cyan, thin); break;
    default: return;
    }
    Viz::Shown& s = viz.players[static_cast<za::SizeT>(num)];
    if(s.frame != host_framecount)
    {
        s = Viz::Shown{host_framecount, 0, a, b};
    }
    else if(kind == PlayerGrab && s.count == 0)
    {
        s.a = a;
        s.b = b;
    }
    s.count++;
}

// (bots4) A client (no server here): the other players' volumes rebuilt from their weapons as drawn here (mpavatar:
// a bot's placed by the server, a player's where his client drew it), by the rules the host sent (QVR_RULE: grab
// and pvp, the reach, the shot volumes). No hotspots (not sent): the line and the hand.
void clientPlayers()
{
    const float reach = vr_player_disarm_reach.value * 0.01f * units::metresToUnits();
    const bool grab = vr_player_disarm_pvp.value != 0.f && vr_player_disarm_grab.value != 0.f;
    const bool shot = (vr_player_disarm.value != 0.f || vr_player_disarm_pvp.value != 0.f) &&
                      vr_player_disarm_chance.value > 0.f;
    const int parts = static_cast<int>(vr_agrunt_shotdisarm_parts.value);
    for(int e = 1; e <= za::min(cl.maxclients, MAX_SCOREBOARD); e++)
    {
        for(int side = 0; side < 2; side++)
        {
            glm::vec3 butt{0.f}, muzzle{0.f}, hand{0.f};
            if(e == cl.viewentity || !mpavatar::drawnWeapon(e, side, butt, muzzle, hand))
            {
                continue;
            }
            if(grab)
            {
                playerVolume(e, PlayerGrab, butt, muzzle, reach);
            }
            if(shot && parts != 1)
            {
                playerVolume(e, PlayerShotW, butt, muzzle, shotWeaponRadius + vr_agrunt_vol_shot_weapon.value);
            }
            if(shot && parts != 0)
            {
                playerVolume(e, PlayerShotH, hand, hand, shotHandRadius + vr_agrunt_vol_shot_hand.value);
            }
        }
    }
}

void drawShape(const Shape& s)
{
    constexpr float thin = 0.12f, thick = 0.3f;
    switch(s.kind)
    {
    case PlayerGrab:
    case PlayerSpot:
    case PlayerShotW:
    case PlayerShotH: playerVolume(s.num, s.kind, s.a, s.b, s.radius); break;
    case WeaponHit:
    {
        // (its hull grown by the strike's units, and its line: the parry's)
        const bool tug = tugged(s.num), hover = hovered(s.num);
        const glm::vec4 c = tug ? green : hover ? yellow : orange;
        weaponHull(s.num, s.radius, c, tug || hover ? thick : thin * 1.5f);
        lines::line(s.a, s.b, thin, c, c);
        break;
    }
    case HandHit: handHull(s, orange, thin * 1.5f); break;
    case WeaponShot: weaponHull(s.num, s.radius, magenta, thin); break;
    case WeaponNear: weaponHull(s.num, s.radius, faint(magenta), thin); break;
    case HandShot: handHull(s, cyan, thin); break;
    case HandNear: handHull(s, faint(cyan), thin); break;
    case GripBlow: weaponHull(s.num, s.radius, red, thin); break;
    case LimbBlow: handHull(s, red, thin); break;
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
    if(what == tugNote)
    {
        Verdict& v = viz.verdicts[static_cast<za::SizeT>(viz.nextVerdict)];
        viz.nextVerdict = (viz.nextVerdict + 1) % maxVerdicts;
        v.at = at;
        v.time = sv.qcvm.time;
        q_strlcpy(v.text, text, sizeof(v.text));
        Con_Printf("disarmviz: tug on %d: %s\n", num, text);
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

bool playerShown(int ent, int& count, glm::vec3& a, glm::vec3& b)
{
    if(ent <= 0 || ent > MAX_SCOREBOARD || viz.players[static_cast<za::SizeT>(ent)].frame < host_framecount - 1)
    {
        return false;
    }
    const Viz::Shown& s = viz.players[static_cast<za::SizeT>(ent)];
    count = s.count;
    a = s.a;
    b = s.b;
    return true;
}

void draw(const glm::vec3& eye)
{
    if(!on())
    {
        return;
    }
    if(!sv.active)
    {
        if(cls.state == ca_connected && !cls.demoplayback)
        {
            clientPlayers(); // (bots4: a client's own rebuild of the other players' volumes)
        }
        return;
    }
    if(fresh(viz.time))
    {
        // (the hulls read the server's edicts: its progs switched in, the view's set up with the client's or none)
        {
            const progs::ServerVm vm;
            for(int i = 0; i < viz.shapeCount; i++)
            {
                drawShape(viz.shapes[static_cast<za::SizeT>(i)]);
            }
        }
        const float cm = 100.f / units::metresToUnits();
        for(int i = 0; i < viz.labelCount; i++)
        {
            // Its state, and each hand near it: how far its fist is, and whether a grip takes it.
            const Label& l = viz.labels[static_cast<za::SizeT>(i)];
            char text[640];
            int n = q_snprintf(text, sizeof(text), "%s", l.text);
            for(int h = 0; h < 2 && fresh(viz.fistTime); h++)
            {
                const Hover& hv = viz.hover[h];
                if(hv.weapon == l.num && n > 0 && n < static_cast<int>(sizeof(text)))
                {
                    n += q_snprintf(text + n, sizeof(text) - static_cast<za::SizeT>(n), "\n%s: %s %.1f cm (%.1f)%s",
                        h == 0 ? "off" : "main", hv.takes ? "GRIP TAKES" : "fist", static_cast<double>(hv.gap * cm),
                        static_cast<double>(hv.allowed * cm), hv.swept ? " swept" : "");
                }
            }
            label(text, l.at, eye);
        }
    }
    drawFists();
    const double now = sv.qcvm.time;
    for(const Verdict& v : viz.verdicts)
    {
        if(v.time >= 0.0 && now >= v.time && now - v.time <= tugShown)
        {
            label(v.text, v.at, eye);
        }
    }
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
