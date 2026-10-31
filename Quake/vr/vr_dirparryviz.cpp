// vr_dirparryviz.cpp -- Show Directional Parry (vr_dirparry_debug): see vr_dirparryviz.hpp.

#include "vr_dirparryviz.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_lines.hpp"
#include "vr_text3d.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

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

struct Viz
{
    Reading building; // the reading being sent (its lines; its note ends it)
    Reading live;     // the last reading during a strike
    Reading held;     // the last strike's verdict
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

} // namespace qvr::dirparryviz
