// vr_dirparryviz.cpp -- Show Directional Parry (vr_dirparry_debug): see vr_dirparryviz.hpp.

#include "vr_dirparryviz.hpp"

#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_grasp.hpp"
#include "vr_lines.hpp"
#include "vr_render.hpp"
#include "vr_text3d.hpp"
#include "vr_view.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>

namespace qvr::dirparryviz
{
namespace
{

// The QC's kinds of line (vr_dirparry.qc VR_DRP_Debug: keep in step).
enum Kind : int
{
    Blade = 1,     // its blade's line then (where it met yours best)
    SpanStart = 2, // ... where the span its way is followed over begins (last frame)
    SpanEnd = 3,   // ... and ends (the window's end at its blow; now during its strike)
    Yours = 4,     // your weapon's parry line
    Way = 5,       // its way there (the meeting to 12 units along it)
    Ideal = 6,     // the ideal line through your line's nearest point: square to its blade and its way
    Gap = 7,       // the two lines' nearest points
};

// The verdicts (vr_dirparry.qc VR_DRP_SHOW_*: keep in step): a strike's 0..2; live + 10; its blow held back.
constexpr int verdictMiss = 0, verdictPartial = 1, verdictParried = 2, verdictLive = 10, verdictHeld = 13;

struct Line
{
    int kind{0};
    glm::vec3 a{0.f}, b{0.f};
};

struct Reading
{
    za::Array<Line, 8> lines{};
    int lineCount{0};
    int verdict{-1};
    glm::vec3 at{0.f};
    glm::vec3 score{0.f}, angle{0.f}, contact{0.f}, marks{0.f}, more{0.f}; // as debugdirparrynote's
    double time{-1.0};
    bool reported{false}; // its first draw printed (a strike's)
};

constexpr double liveShown = 0.25; // s: a live reading older than this is gone
constexpr double heldShown = 3.0;  // s: the last strike's verdict
constexpr int columns = 40;        // the readout's lines, padded to one width (centred: a table)

// Each held melee weapon's line this server frame as the parry reads it (debugdirparryline), and last frame's.
struct Track
{
    int num{0};                    // the weapon's entity
    glm::vec3 a{0.f}, b{0.f};      // its line now: its back, its front (QC VR_AG_WeaponLine)
    glm::vec3 a0{0.f}, b0{0.f};    // ... last frame's
    bool measured{false};          // the drawn weapon's ends found (below)
    glm::vec3 butt{0.f}, tip{0.f}; // ... its butt and its tip (muzzle) as the renderer last drew it
};

// A weapon's model matrix as the renderer last drew it (drawnMatrix: only the weapons tracked).
struct Drawn
{
    int num{0};
    int frame{-1};
    glm::mat4 m{1.f};
};

constexpr int maxTracks = 16;

// The line check (vr_dirparry_linecheck): the line's ends against the drawn weapon's, the worst so far.
struct Check
{
    int frames{0};
    float worstBack{0.f}, worstFront{0.f};
};

struct Viz
{
    Reading building; // the reading being sent (its lines; its note ends it)
    Reading live;     // the last reading during a strike
    Reading held;     // the last strike's verdict
    double trackTime{-1.0};
    za::Array<Track, maxTracks> tracks{};
    int trackCount{0};
    za::Array<Drawn, maxTracks> drawn{};
    Check check;
    bool dump{false}; // vr_dirparry_linecheck dump: the next measured line printed in the weapon's own frame
    bool look{false}; // vr_dirparry_linecheck look: the mock's eyes put close to the next measured weapon
    bool cams{false}; // ... they are
};
Viz viz;

const glm::vec4 green{0.2f, 1.f, 0.3f, 1.f};
const glm::vec4 orange{1.f, 0.55f, 0.1f, 1.f};
const glm::vec4 red{1.f, 0.15f, 0.15f, 1.f};
const glm::vec4 white{1.f, 1.f, 1.f, 0.9f};
const glm::vec4 yellow{1.f, 0.95f, 0.15f, 1.f};
const glm::vec4 cyan{0.2f, 0.9f, 1.f, 0.9f};
const glm::vec4 grey{0.8f, 0.8f, 0.8f, 0.35f};

[[nodiscard]] int base(int verdict)
{
    return verdict == verdictHeld ? verdictPartial : verdict >= verdictLive ? verdict - verdictLive : verdict;
}

[[nodiscard]] glm::vec4 colourOf(int verdict)
{
    const int v = base(verdict);
    glm::vec4 c = v == verdictParried ? green : v == verdictPartial ? orange : red;
    c.a *= verdict >= verdictLive ? 0.6f : 1.f;
    return c;
}

[[nodiscard]] const char* verdictText(const Reading& r)
{
    switch(r.verdict)
    {
    case verdictParried: return "PARRIED";
    case verdictPartial: return r.score.z > 0.f ? "PARTIAL: it lands, cut" : "PARTIAL: it lands (no cut)";
    case verdictMiss: return "MISS: it lands";
    case verdictLive + verdictParried: return "during its strike: would parry";
    case verdictLive + verdictPartial: return "during its strike: under the threshold";
    case verdictLive + verdictMiss: return "during its strike: no marks";
    case verdictHeld: return "its blow held back: the window";
    default: return "?";
    }
}

[[nodiscard]] bool fresh(const Reading& r, double shown)
{
    const double now = sv.qcvm.time;
    return r.time >= 0.0 && now >= r.time && now - r.time <= shown;
}

// One readout line, padded to `columns` (centred lines of one length line up as a table).
void pad(char* out, size_t size, const char* text)
{
    q_snprintf(out, size, "%-*s", columns, text);
}

void readout(const Reading& r, const glm::vec3& eye)
{
    char l[7][64];
    char t[64];
    if(r.verdict == verdictPartial && r.score.z > 0.f)
    {
        q_snprintf(t, sizeof(t), "%s %.0f%%", verdictText(r), static_cast<double>(r.score.z * 100.f));
    }
    else
    {
        q_snprintf(t, sizeof(t), "%s", verdictText(r));
    }
    pad(l[0], sizeof(l[0]), t);
    q_snprintf(t, sizeof(t), "score %.2f  need %.2f", static_cast<double>(r.score.x), static_cast<double>(r.score.y));
    pad(l[1], sizeof(l[1]), t);
    q_snprintf(t, sizeof(t), "angle %.0f deg off the ideal   x%.2f", static_cast<double>(r.angle.x),
        static_cast<double>(r.marks.x));
    pad(l[2], sizeof(l[2]), t);
    q_snprintf(t, sizeof(t), "  full within %.0f, none at %.0f deg", static_cast<double>(r.angle.y),
        static_cast<double>(r.angle.z));
    pad(l[3], sizeof(l[3]), t);
    q_snprintf(t, sizeof(t), "contact %.1f units apart      x%.2f", static_cast<double>(r.contact.x),
        static_cast<double>(r.marks.y));
    pad(l[4], sizeof(l[4]), t);
    q_snprintf(t, sizeof(t), "  full within %.1f, none at %.1f", static_cast<double>(r.contact.y),
        static_cast<double>(r.contact.z));
    pad(l[5], sizeof(l[5]), t);
    q_snprintf(t, sizeof(t), "%s hand, %.3f s %s, speed %.2fx", r.more.x > 0.5f ? "main" : "off",
        static_cast<double>(r.more.y < 0.f ? -r.more.y : r.more.y), r.more.y > 0.f ? "ahead" : "ago",
        static_cast<double>(r.more.z));
    pad(l[6], sizeof(l[6]), t);
    char text[7 * 66];
    q_snprintf(text, sizeof(text), "%s\n%s\n%s\n%s\n%s\n%s\n%s", l[0], l[1], l[2], l[3], l[4], l[5], l[6]);
    // The score against the threshold: a bar after "score .. need .." (filled to the score), the threshold a white tick.
    constexpr float barAt = 24.f, barCells = 15.f;
    const float score = za::clamp(r.score.x, 0.f, 1.f), need = za::clamp(r.score.y, 0.f, 1.f);
    const text3d::OverlayBar bars[2]{
        {1, barAt, barCells * score, barCells, colourOf(r.verdict)},
        {1, barAt + barCells * need - 0.15f, 0.3f, 0.f, white},
    };
    const glm::vec3 pos = r.at + glm::vec3{0.f, 0.f, 16.f};
    const glm::vec3 to = pos - eye;
    text3d::queueOverlay(text, pos, glm::vec3{0.f, glm::degrees(za::atan2(to.y, to.x)), 0.f}, 0.06f,
        za::Span<const text3d::OverlayBar>{bars, 2});
}

void drawReading(const Reading& r)
{
    const glm::vec4 c = colourOf(r.verdict);
    constexpr float thin = 0.12f, thick = 0.35f;
    for(int i = 0; i < r.lineCount; i++)
    {
        const Line& s = r.lines[static_cast<za::SizeT>(i)];
        switch(s.kind)
        {
        case Blade: lines::line(s.a, s.b, thick, white, white); break;
        case SpanStart:
        case SpanEnd: lines::line(s.a, s.b, thin, grey, grey); break;
        case Yours: lines::line(s.a, s.b, thick, c, c); break;
        case Way:
            lines::line(s.a, s.b, thin * 2.f, yellow, yellow);
            lines::point(s.b, 1.5f, yellow); // (its head)
            break;
        case Ideal:
            lines::line(s.a, s.b, thin * 1.5f, cyan, cyan);
            lines::point(s.a, 0.8f, cyan);
            lines::point(s.b, 0.8f, cyan);
            break;
        case Gap:
            lines::line(s.a, s.b, thin, c, c);
            lines::point(s.a, 0.8f, white);
            lines::point(s.b, 0.8f, c);
            break;
        default: break;
        }
    }
    lines::point(r.at, 1.8f, c);
}

// Weapon entity `num`'s drawn matrix from its model's own frame (x forward, y left, z up: weaponLocalGeometry's) to the
// world, as the renderer drew it in the last two frames (the .mdl's packing taken off, as vr_rigstudio_aimline.cpp's).
[[nodiscard]] bool drawnFrame(int num, glm::mat4& out)
{
    for(const Drawn& d : viz.drawn)
    {
        if(d.num == num && d.frame >= 0 && host_framecount - d.frame <= 2 && cl_entities && cl_entities[num].model)
        {
            entity_t at{};
            at.model = cl_entities[num].model;
            at.scale = ENTSCALE_DEFAULT;
            float m0[16];
            render::entityMatrix(at, false, ENTSCALE_DEFAULT, glm::vec3{0.f}, m0);
            glm::mat4 f0;
            for(int c = 0; c < 4; c++)
            {
                f0[c] = glm::vec4{m0[c * 4], m0[c * 4 + 1], m0[c * 4 + 2], m0[c * 4 + 3]};
            }
            out = d.m * glm::inverse(f0);
            return true;
        }
    }
    return false;
}

void drawTrack(const Track& t)
{
    const glm::vec4 steel{0.45f, 0.75f, 1.f, 1.f};
    const glm::vec4 was{0.45f, 0.75f, 1.f, 0.35f};
    lines::line(t.a, t.b, 0.3f, steel, steel); // (its line now)
    lines::point(t.a, 1.2f, steel);
    lines::point(t.b, 1.2f, white);
    lines::line(t.a0, t.b0, 0.15f, was, was); // (last frame's: the sweep between)
    lines::line(t.a0, t.a, 0.08f, was, was);
    lines::line(t.b0, t.b, 0.08f, was, was);
    if(t.measured)
    {
        const glm::vec4 magenta{1.f, 0.25f, 1.f, 1.f};
        lines::point(t.butt, 0.8f, magenta); // (the drawn weapon's own ends: on the line's when it is right)
        lines::point(t.tip, 0.8f, magenta);
    }
}

void print(const Reading& r, const char* what)
{
    Con_Printf("dirparryviz: %s %s: score %.2f (need %.2f), %.0f deg off the ideal (full within %.0f, none at %.0f: "
               "x%.2f), %.1f units apart (full within %.1f, none at %.1f: x%.2f), speed %.2fx\n",
        what, verdictText(r), static_cast<double>(r.score.x), static_cast<double>(r.score.y),
        static_cast<double>(r.angle.x), static_cast<double>(r.angle.y), static_cast<double>(r.angle.z),
        static_cast<double>(r.marks.x), static_cast<double>(r.contact.x), static_cast<double>(r.contact.y),
        static_cast<double>(r.contact.z), static_cast<double>(r.marks.y), static_cast<double>(r.more.z));
}

} // namespace

bool on()
{
    return vr_dirparry_debug.value != 0.f;
}

void PF_debugdirparry()
{
    if(!on() || viz.building.lineCount >= static_cast<int>(viz.building.lines.size()))
    {
        return;
    }
    Line& l = viz.building.lines[static_cast<za::SizeT>(viz.building.lineCount++)];
    l.kind = static_cast<int>(G_FLOAT(OFS_PARM0));
    l.a = geom::vec(G_VECTOR(OFS_PARM1));
    l.b = geom::vec(G_VECTOR(OFS_PARM2));
}

void PF_debugdirparryline()
{
    G_FLOAT(OFS_RETURN) = -1.f;
    if(!on())
    {
        return;
    }
    const double now = sv.qcvm.time;
    if(viz.trackTime != now)
    {
        viz.trackTime = now;
        viz.trackCount = 0;
    }
    if(viz.trackCount >= maxTracks)
    {
        return;
    }
    Track& t = viz.tracks[static_cast<za::SizeT>(viz.trackCount++)];
    t = Track{};
    t.num = NUM_FOR_EDICT(G_EDICT(OFS_PARM0));
    t.a = geom::vec(G_VECTOR(OFS_PARM1));
    t.b = geom::vec(G_VECTOR(OFS_PARM2));
    t.a0 = geom::vec(G_VECTOR(OFS_PARM3));
    t.b0 = geom::vec(G_VECTOR(OFS_PARM4));
    // The drawn weapon's own butt and tip (its model's geometry, the renderer's matrix: none of the line's code) against
    // the line's ends: the line is read from the same drawing (hitreact::drawnWeapon, last drawn), so they meet.
    glm::mat4 w;
    glm::vec3 muzzle, butt, second;
    int secondType = 0;
    if(t.num > 0 && t.num < cl.num_entities && drawnFrame(t.num, w) &&
       view::weaponLocalGeometry(cl_entities[t.num].model, muzzle, butt, second, secondType))
    {
        t.measured = true;
        t.butt = glm::vec3{w * glm::vec4{butt, 1.f}};
        t.tip = glm::vec3{w * glm::vec4{muzzle, 1.f}};
        // Its tip: the middle of its points within a unit of its frontmost along the line (as its butt is of the rearmost);
        // its muzzle is where it strikes, not its far end, for a head on a haft (an axe's, a hammer's face beside it).
        {
            const glm::mat4 inv = glm::inverse(w);
            const glm::vec3 la{inv * glm::vec4{t.a, 1.f}}, lb{inv * glm::vec4{t.b, 1.f}};
            entity_t at{};
            at.model = cl_entities[t.num].model;
            at.scale = ENTSCALE_DEFAULT;
            const grasp::Shape* shape = grasp::shapeOf(at, -1);
            if(shape && glm::length(lb - la) > 1e-3f)
            {
                const glm::vec3 dl = glm::normalize(lb - la);
                const glm::mat4 m = grasp::shapeToWorld(at, false);
                float front = -1e30f;
                for(const grasp::Triangle& tri : shape->tris)
                {
                    for(const glm::vec3& p : tri.p)
                    {
                        front = za::max(front, glm::dot(glm::vec3{m * glm::vec4{p, 1.f}}, dl));
                    }
                }
                glm::vec3 sum{0.f};
                int n = 0;
                for(const grasp::Triangle& tri : shape->tris)
                {
                    for(const glm::vec3& p : tri.p)
                    {
                        const glm::vec3 v{m * glm::vec4{p, 1.f}};
                        if(glm::dot(v, dl) >= front - 1.f)
                        {
                            sum += v;
                            n++;
                        }
                    }
                }
                if(n > 0)
                {
                    t.tip = glm::vec3{w * glm::vec4{sum / static_cast<float>(n), 1.f}};
                }
            }
        }
        const float back = glm::length(t.a - t.butt), front = glm::length(t.b - t.tip);
        viz.check.frames++;
        viz.check.worstBack = za::max(viz.check.worstBack, back);
        viz.check.worstFront = za::max(viz.check.worstFront, front);
        G_FLOAT(OFS_RETURN) = za::max(back, front);
        if(viz.look)
        {
            // The headless mock's eyes close to it (the left from its flat side, 30 units; the right from above that
            // side, 40): its drawn model, its line and the drawn ends in one eye image (vr_eyeshot).
            viz.look = false;
            viz.cams = true;
            const glm::vec3 mid = (t.butt + t.tip) * 0.5f;
            const glm::vec3 side = glm::normalize(glm::vec3{w[1]});
            const glm::vec3 at[2] = {mid + side * 30.f, mid + glm::normalize(side + glm::vec3{0.f, 0.f, 1.f}) * 40.f};
            float pitch[2], yaw[2];
            for(int i = 0; i < 2; i++)
            {
                const glm::vec3 d = glm::normalize(mid - at[i]);
                pitch[i] = -za::asin(d.z) * (180.f / 3.14159265f);
                yaw[i] = za::atan2(d.y, d.x) * (180.f / 3.14159265f);
            }
            setMockEyeCameras(true, at, pitch, yaw);
        }
        if(viz.dump)
        {
            viz.dump = false;
            const glm::mat4 inv = glm::inverse(w);
            const glm::vec3 la{inv * glm::vec4{t.a, 1.f}}, lb{inv * glm::vec4{t.b, 1.f}};
            glm::vec3 lo{1e30f}, hi{-1e30f};
            entity_t at{};
            at.model = cl_entities[t.num].model;
            at.scale = ENTSCALE_DEFAULT;
            if(const grasp::Shape* shape = grasp::shapeOf(at, -1))
            {
                const glm::mat4 m = grasp::shapeToWorld(at, false);
                for(const grasp::Triangle& tri : shape->tris)
                {
                    for(const glm::vec3& p : tri.p)
                    {
                        const glm::vec3 v{m * glm::vec4{p, 1.f}};
                        lo = glm::min(lo, v);
                        hi = glm::max(hi, v);
                    }
                }
            }
            Con_Printf("dirparryviz: dump %s: in its own frame: line %.1f %.1f %.1f .. %.1f %.1f %.1f; butt %.1f %.1f %.1f, "
                       "muzzle %.1f %.1f %.1f; its triangles' box %.1f %.1f %.1f .. %.1f %.1f %.1f\n",
                at.model->name, la.x, la.y, la.z, lb.x, lb.y, lb.z, butt.x, butt.y, butt.z, muzzle.x, muzzle.y, muzzle.z,
                lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);
        }
    }
}

void drawnMatrix(const entity_t* e, const float* matrix)
{
    if(!on() || !cl_entities || e < cl_entities || e >= cl_entities + cl_max_edicts)
    {
        return;
    }
    const int num = static_cast<int>(e - cl_entities);
    int slot = -1;
    for(int i = 0; i < viz.trackCount; i++)
    {
        slot = viz.tracks[static_cast<za::SizeT>(i)].num == num ? i : slot;
    }
    if(slot < 0)
    {
        return; // (not a weapon the parry reads)
    }
    Drawn& d = viz.drawn[static_cast<za::SizeT>(slot)];
    d.num = num;
    d.frame = host_framecount;
    for(int c = 0; c < 4; c++)
    {
        d.m[c] = glm::vec4{matrix[c * 4], matrix[c * 4 + 1], matrix[c * 4 + 2], matrix[c * 4 + 3]};
    }
}

void PF_debugdirparrynote()
{
    G_FLOAT(OFS_RETURN) = 0.f;
    if(!on())
    {
        viz.building = Reading{};
        return;
    }
    Reading r = viz.building;
    viz.building = Reading{};
    r.verdict = static_cast<int>(G_FLOAT(OFS_PARM0));
    r.at = geom::vec(G_VECTOR(OFS_PARM1));
    r.score = geom::vec(G_VECTOR(OFS_PARM2));
    r.angle = geom::vec(G_VECTOR(OFS_PARM3));
    r.contact = geom::vec(G_VECTOR(OFS_PARM4));
    r.marks = geom::vec(G_VECTOR(OFS_PARM5));
    r.more = geom::vec(G_VECTOR(OFS_PARM6));
    r.time = sv.qcvm.time;
    if(r.verdict < verdictLive)
    {
        viz.held = r;
        print(r, "strike");
    }
    else
    {
        viz.live = r;
    }
    G_FLOAT(OFS_RETURN) = 1.f;
}

void draw(const glm::vec3& eye)
{
    if(!on() || !sv.active)
    {
        return;
    }
    if(viz.trackTime >= 0.0 && sv.qcvm.time >= viz.trackTime && sv.qcvm.time - viz.trackTime <= liveShown)
    {
        for(int i = 0; i < viz.trackCount; i++)
        {
            drawTrack(viz.tracks[static_cast<za::SizeT>(i)]);
        }
    }
    const bool held = fresh(viz.held, heldShown);
    if(fresh(viz.live, liveShown) && (!held || viz.live.time > viz.held.time))
    {
        drawReading(viz.live); // (its strike's reading now: superseded by its verdict)
    }
    if(held)
    {
        drawReading(viz.held);
        readout(viz.held, eye);
        if(!viz.held.reported)
        {
            viz.held.reported = true;
            print(viz.held, "drawn");
        }
    }
}

void lineCheck_f()
{
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "reset"))
    {
        viz.check = Check{};
    }
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "dump"))
    {
        viz.dump = true; // (printed as the next line is measured)
        return;
    }
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "look"))
    {
        const bool off = Cmd_Argc() > 2 && !strcmp(Cmd_Argv(2), "off");
        viz.look = !off;
        if(off && viz.cams)
        {
            viz.cams = false;
            setMockEyeCameras(false);
        }
        return;
    }
    Con_Printf("dirparryviz: line check: %d frames measured, worst back %.2f units, worst front %.2f units\n",
        viz.check.frames, static_cast<double>(viz.check.worstBack), static_cast<double>(viz.check.worstFront));
}

void registerCommands()
{
    Cmd_AddCommand("vr_dirparry_linecheck", lineCheck_f);
}

} // namespace qvr::dirparryviz
