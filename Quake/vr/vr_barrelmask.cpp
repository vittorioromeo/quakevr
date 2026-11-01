// vr_barrelmask.cpp -- each gun's barrel mask and its paint mode: see vr_barrelmask.hpp.

#include "vr_barrelmask.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_text3d.hpp"
#include "vr_toolpanel.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <glm/glm.hpp>

#include <string.h>

namespace qvr::barrelmask
{
namespace
{

using weapons::Key;
using Bits = za::Vector<za::U32>;

// A model's triangles in its drawing order (its index buffer's: the shader's gl_PrimitiveID) and its rest pose (pose
// 0) in its units, made once (again for a reloaded model).
struct Geometry
{
    const void* hdr{nullptr};
    za::Vector<glm::vec3> rest;             // per .mdl vertex
    za::Vector<za::Array<int, 3>> tris;     // their .mdl vertices
    za::Vector<glm::vec3> centre;           // each triangle's middle
};
ankerl::unordered_dense::map<const qmodel_t*, Geometry> geometries;

// A part's triangles' counterparts on its gun (the gun's triangle in the same place, -1 none), made once.
struct PartMap
{
    const void* partHdr{nullptr};
    const void* gunHdr{nullptr};
    za::Vector<int> gunTri;
};
struct PairKey
{
    const qmodel_t* drawn;
    const qmodel_t* gun;
    int slot;
    bool operator==(const PairKey& o) const { return drawn == o.drawn && gun == o.gun && slot == o.slot; }
};
struct PairHash
{
    using is_avalanching = void;
    [[nodiscard]] za::U64 operator()(const PairKey& k) const noexcept
    {
        const auto mix = [](za::U64 h, za::U64 v) { return (h ^ v) * 0x100000001B3ull; };
        return mix(mix(mix(0xCBF29CE484222325ull, reinterpret_cast<za::U64>(k.drawn)), reinterpret_cast<za::U64>(k.gun)),
            static_cast<za::U64>(k.slot));
    }
};
ankerl::unordered_dense::map<PairKey, PartMap, PairHash> partMaps;

// A drawn model's bits for a slot's mask, written to the GPU buffer at `first` (words).
struct Uploaded
{
    unsigned generation{0};
    const void* drawnHdr{nullptr};
    int first{-1}; // -1: no mask
};
ankerl::unordered_dense::map<PairKey, Uploaded, PairHash> uploaded;

constexpr int bufferWords = 1 << 15; // (128 KB; written from the start again when full)
GLuint buffer = 0;
int bufferUsed = 0;

constexpr char sextets[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

[[nodiscard]] int sextetOf(char c)
{
    const char* at = strchr(sextets, c);
    return c && at ? static_cast<int>(at - sextets) : -1;
}

[[nodiscard]] const aliashdr_t* aliasOf(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    return hdr && hdr->poseverttype == aliashdr_t::PV_QUAKE1 && !hdr->nextsurface && hdr->numposes > 0 ? hdr : nullptr;
}

[[nodiscard]] const Geometry* geometryOf(const qmodel_t* model)
{
    const aliashdr_t* hdr = aliasOf(model);
    if(!hdr)
    {
        return nullptr;
    }
    Geometry& g = geometries[model];
    if(g.hdr == hdr)
    {
        return &g;
    }
    g = Geometry{};
    g.hdr = hdr;
    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* poses = reinterpret_cast<const trivertx_t*>(base + hdr->vertexes);
    for(int v = 0; v < hdr->numverts; v++)
    {
        g.rest.pushBack(glm::vec3{poses[v].v[0] * hdr->scale[0] + hdr->scale_origin[0],
            poses[v].v[1] * hdr->scale[1] + hdr->scale_origin[1], poses[v].v[2] * hdr->scale[2] + hdr->scale_origin[2]});
    }
    const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        za::Array<int, 3> t;
        glm::vec3 c{0.f};
        for(int k = 0; k < 3; k++)
        {
            t[k] = za::min(static_cast<int>(mesh[indexes[i + k]].vertindex), hdr->numverts - 1);
            c += g.rest[static_cast<za::SizeT>(t[k])] / 3.f;
        }
        g.tris.pushBack(t);
        g.centre.pushBack(c);
    }
    return &g;
}

// Mask text -> bits (false: none, or for another model: its triangle count differs).
[[nodiscard]] bool parse(const char* text, int numtris, Bits& bits)
{
    const char* colon = text ? strchr(text, ':') : nullptr;
    if(!colon || numtris <= 0 || Q_atoi(text) != numtris)
    {
        return false;
    }
    bits.clear();
    bits.resize(static_cast<za::SizeT>((numtris + 31) / 32), 0u);
    int bit = 0;
    for(const char* c = colon + 1; *c && bit < numtris; c++)
    {
        const int v = sextetOf(*c);
        if(v < 0)
        {
            return false;
        }
        for(int k = 0; k < 6 && bit < numtris; k++, bit++)
        {
            bits[static_cast<za::SizeT>(bit >> 5)] |= (v >> k & 1) ? 1u << (bit & 31) : 0u;
        }
    }
    return true;
}

[[nodiscard]] za::String encode(const Bits& bits, int numtris)
{
    za::String out = va("%d:", numtris);
    for(int bit = 0; bit < numtris; bit += 6)
    {
        int v = 0;
        for(int k = 0; k < 6 && bit + k < numtris; k++)
        {
            v |= (bits[static_cast<za::SizeT>((bit + k) >> 5)] >> ((bit + k) & 31) & 1u) ? 1 << k : 0;
        }
        out += sextets[v];
    }
    return out;
}

[[nodiscard]] bool bitOf(const Bits& bits, int t)
{
    return t >= 0 && (bits[static_cast<za::SizeT>(t >> 5)] >> (t & 31) & 1u) != 0u;
}

void setBit(Bits& bits, int t, bool on)
{
    za::U32& w = bits[static_cast<za::SizeT>(t >> 5)];
    w = on ? (w | 1u << (t & 31)) : (w & ~(1u << (t & 31)));
}

// The cvar holding slot `slot`'s mask (its owner's, if it inherits it).
[[nodiscard]] cvar_t* maskVar(int slot)
{
    return slot >= 0 ? weapons::cvar(weapons::ownerSlot(slot, Key::GunfxBarrelMask), Key::GunfxBarrelMask) : nullptr;
}

// Slot `slot`'s mask on its gun model `gun` (false: none).
[[nodiscard]] bool slotMask(int slot, const qmodel_t* gun, Bits& bits)
{
    const Geometry* g = geometryOf(gun);
    const cvar_t* var = maskVar(slot);
    return g && var && parse(var->string, static_cast<int>(g->tris.size()), bits);
}

// Each of `part`'s triangles' counterpart on `gun` (the gun's triangle whose middle is nearest its middle, within a unit).
[[nodiscard]] const za::Vector<int>* partMap(const qmodel_t* part, const qmodel_t* gun)
{
    const Geometry* p = geometryOf(part);
    const Geometry* g = geometryOf(gun);
    if(!p || !g)
    {
        return nullptr;
    }
    PartMap& m = partMaps[PairKey{part, gun, -1}];
    if(m.partHdr == p->hdr && m.gunHdr == g->hdr)
    {
        return &m.gunTri;
    }
    m = PartMap{p->hdr, g->hdr, {}};
    for(const glm::vec3& c : p->centre)
    {
        int best = -1;
        float bestD = 1.f;
        for(za::SizeT t = 0; t < g->centre.size(); t++)
        {
            const glm::vec3 d = g->centre[t] - c;
            const float dd = za::fabs(d.x) + za::fabs(d.y) + za::fabs(d.z);
            if(dd < bestD)
            {
                bestD = dd;
                best = static_cast<int>(t);
                if(dd < 1e-3f)
                {
                    break;
                }
            }
        }
        m.gunTri.pushBack(best);
    }
    return &m.gunTri;
}

// The paint mode's session.
enum PanelButton
{
    BtnUndo,
    BtnClear,
    BtnPremarked,
    BtnCancel,
    BtnDone,
    BtnCount
};

constexpr float PANEL_DISTANCE = 0.55f; // metres ahead of the eyes
constexpr float PANEL_DROP = 0.32f;     // metres below them
constexpr float BUTTON_WIDTH = 0.17f;   // metres
constexpr float BUTTON_HEIGHT = 0.055f;
constexpr float BUTTON_GAP = 0.015f;
constexpr float BUTTON_DEPTH = 0.08f;
constexpr float BRUSH_MIN = 0.3f;     // model units
constexpr float BRUSH_MAX = 8.f;
constexpr float BRUSH_DEFAULT = 1.2f;
constexpr float RAY_REACH = 120.f;    // units: the other hand points at the gun from at most this far
constexpr int UNDO_MAX = 48;

struct Session
{
    bool active{false};
    int world{-1};
    int returnPage{-1};
    int hand{HAND_MAIN};
    int slot{-1};
    const qmodel_t* gun{nullptr};
    za::String entry;          // the mask's text on entering (Cancel)
    Bits bits;                 // the mask as painted
    int numtris{0};
    za::Vector<Bits> undo;
    float brush{BRUSH_DEFAULT};
    bool hit{false};
    glm::vec3 hitModel{0.f};   // where the ray meets the gun (its model units)
    glm::vec3 hitNormal{0.f};
    int stroke{0};             // 1 painting (trigger), -1 erasing (grip), 0 neither
    bool strokeUndone{false};  // (the stroke's undo step taken)
    int hoverButton[HAND_COUNT]{-1, -1};
    bool eaten[HAND_COUNT][6]{};
    glm::vec3 panelAt{0.f}, panelRight{0.f}, panelUp{0.f}, panelNormal{0.f};
    float panelYaw{0.f};
    za::String feedback;
    double feedbackUntil{0.0};
};
Session ses;

[[nodiscard]] int otherHand()
{
    return ses.hand == HAND_MAIN ? HAND_OFF : HAND_MAIN;
}

void say(const za::String& text)
{
    toolpanel::say(ses.feedback, ses.feedbackUntil, "Paint Barrel Mask", text, 3.0);
}

[[nodiscard]] int countBits(const Bits& bits)
{
    int n = 0;
    for(za::U32 w : bits)
    {
        for(; w; w &= w - 1u)
        {
            n++;
        }
    }
    return n;
}

// The painted mask to its key (each change at once: the glow shows it).
void store()
{
    if(cvar_t* var = maskVar(ses.slot))
    {
        Cvar_SetQuick(var, encode(ses.bits, ses.numtris).cStr());
    }
}

void pushUndo()
{
    toolpanel::pushUndo(ses.undo, UNDO_MAX, Bits{ses.bits});
}

// The triangles with a corner within the brush of `at` (model units) painted (`on`) or erased; how many changed.
int brushAt(const glm::vec3& at, float radius, bool on)
{
    const Geometry* g = geometryOf(ses.gun);
    if(!g)
    {
        return 0;
    }
    int changed = 0;
    for(za::SizeT t = 0; t < g->tris.size(); t++)
    {
        bool in = false;
        for(int k = 0; k < 3 && !in; k++)
        {
            in = glm::length(g->rest[static_cast<za::SizeT>(g->tris[t][k])] - at) <= radius;
        }
        if(in && bitOf(ses.bits, static_cast<int>(t)) != on)
        {
            setBit(ses.bits, static_cast<int>(t), on);
            changed++;
        }
    }
    return changed;
}

// Where the other hand's ray meets the gun's rest pose (its model units), the nearest triangle hit.
void aimFrame(const hands::State& s)
{
    ses.hit = false;
    const Geometry* g = geometryOf(ses.gun);
    glm::vec3 o, e;
    const int other = otherHand();
    const glm::vec3 from = s.pos[other], to = from + hands::forward(s.aimRot[other]) * RAY_REACH;
    if(!g || !view::gunFromWorld(ses.hand, from, o) || !view::gunFromWorld(ses.hand, to, e))
    {
        return;
    }
    const glm::vec3 d = e - o;
    float best = 1.f;
    for(za::SizeT t = 0; t < g->tris.size(); t++)
    {
        const glm::vec3 a = g->rest[static_cast<za::SizeT>(g->tris[t][0])], b = g->rest[static_cast<za::SizeT>(g->tris[t][1])],
                        c = g->rest[static_cast<za::SizeT>(g->tris[t][2])];
        const glm::vec3 e1 = b - a, e2 = c - a, p = glm::cross(d, e2);
        const float det = glm::dot(e1, p);
        if(za::fabs(det) < 1e-9f)
        {
            continue;
        }
        const glm::vec3 q = o - a;
        const float u = glm::dot(q, p) / det;
        const glm::vec3 r = glm::cross(q, e1);
        const float v = glm::dot(d, r) / det, k = glm::dot(e2, r) / det;
        if(u < 0.f || v < 0.f || u + v > 1.f || k < 0.f || k >= best)
        {
            continue;
        }
        best = k;
        ses.hit = true;
        ses.hitModel = o + d * k;
        ses.hitNormal = glm::normalize(glm::cross(e1, e2));
    }
}

void strokeFrame()
{
    if(ses.stroke == 0 || !ses.hit)
    {
        return;
    }
    const Bits before{ses.bits};
    if(brushAt(ses.hitModel, ses.brush, ses.stroke > 0) > 0)
    {
        if(!ses.strokeUndone)
        {
            toolpanel::pushUndo(ses.undo, UNDO_MAX, Bits{before});
            ses.strokeUndone = true;
        }
        store();
        haptics::pulse(otherHand(), 0.01f, 0.2f);
    }
}

void undo()
{
    if(ses.undo.empty())
    {
        say("nothing to undo");
        return;
    }
    ses.bits = ses.undo.back();
    ses.undo.popBack();
    store();
    say("undone");
}

void clearMask()
{
    pushUndo();
    for(za::U32& w : ses.bits)
    {
        w = 0u;
    }
    store();
    say("cleared: paint the barrel");
}

// The shipped mask (weapons.txt's: the key's default).
void premarked()
{
    pushUndo();
    const cvar_t* var = maskVar(ses.slot);
    Bits shipped;
    if(var && parse(var->default_string, ses.numtris, shipped))
    {
        ses.bits = shipped;
        say("the pre-marked barrel");
    }
    else
    {
        for(za::U32& w : ses.bits)
        {
            w = 0u;
        }
        say("no pre-marked barrel for this gun: cleared");
    }
    store();
}

void press(int b)
{
    switch(b)
    {
        case BtnUndo: undo(); break;
        case BtnClear: clearMask(); break;
        case BtnPremarked: premarked(); break;
        case BtnCancel: finish(false); break;
        case BtnDone: finish(true); break;
        default: break;
    }
}

[[nodiscard]] const char* buttonLabel(int b)
{
    switch(b)
    {
        case BtnUndo: return "Undo";
        case BtnClear: return "Clear";
        case BtnPremarked: return "Pre-marked";
        case BtnCancel: return "Cancel";
        default: return "Done";
    }
}

constexpr toolpanel::Grid buttonGrid{BUTTON_WIDTH, BUTTON_HEIGHT, BUTTON_GAP, BUTTON_DEPTH};

[[nodiscard]] toolpanel::Frame buttonFrame()
{
    return toolpanel::Frame{ses.panelAt, ses.panelRight, ses.panelUp, ses.panelNormal};
}

[[nodiscard]] glm::vec3 buttonAt(int b)
{
    const float col = static_cast<float>(b % 3 - 1), row = static_cast<float>(b / 3);
    return toolpanel::buttonCentre(buttonFrame(), buttonGrid, col, 0.5f - row);
}

[[nodiscard]] int buttonAtHand(const glm::vec3& p)
{
    return toolpanel::buttonUnder(buttonFrame(), buttonGrid, BtnCount, p, buttonAt, [](int) { return true; });
}

void placePanel(const hands::State& s)
{
    const float m2u = units::metresToUnits();
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, s.headAngles.y, 0.f}, fwd, right, up);
    ses.panelAt = s.head + fwd * (PANEL_DISTANCE * m2u) - up * (PANEL_DROP * m2u);
    ses.panelRight = right;
    ses.panelUp = up;
    ses.panelNormal = -fwd;
    ses.panelYaw = s.headAngles.y;
}

void drawPanel()
{
    const float m2u = units::metresToUnits();
    const glm::vec3 angles{0.f, ses.panelYaw, 0.f};
    for(int b = 0; b < BtnCount; b++)
    {
        const glm::vec3 at = buttonAt(b);
        const bool hovered = ses.hoverButton[HAND_OFF] == b || ses.hoverButton[HAND_MAIN] == b;
        const glm::vec4 color = hovered ? glm::vec4{1.f, 0.85f, 0.15f, 1.f} : glm::vec4{0.5f, 0.7f, 1.f, 0.8f};
        const glm::vec3 r = ses.panelRight * (BUTTON_WIDTH * 0.5f * m2u), u = ses.panelUp * (BUTTON_HEIGHT * 0.5f * m2u);
        const glm::vec3 back = at - ses.panelNormal * 0.15f;
        const glm::vec4 face = hovered ? glm::vec4{0.35f, 0.3f, 0.05f, 0.85f} : glm::vec4{0.04f, 0.06f, 0.1f, 0.8f};
        lines::triangle(back - r - u, back + r - u, back + r + u, face);
        lines::triangle(back - r - u, back + r + u, back - r + u, face);
        const glm::vec3 c[4] = {at - r - u, at + r - u, at + r + u, at - r + u};
        for(int i = 0; i < 4; i++)
        {
            (hovered ? lines::glow : lines::line)(c[i], c[(i + 1) % 4], hovered ? 0.25f : 0.12f, color, color);
        }
        const float scale = za::min(BUTTON_WIDTH * 0.85f * m2u / (8.f * 11.f), BUTTON_HEIGHT * 0.55f * m2u / 8.f);
        text3d::queue(buttonLabel(b), at, angles, text3d::Align::Centre, scale);
    }
    const cvar_t* id = weapons::cvar(ses.slot, Key::ID);
    za::String text = va("PAINT BARREL MASK (slot %d: %s)", ses.slot + 1, id ? COM_SkipPath(id->string) : "?");
    text += va("\n%d of %d triangles marked; brush %.1f", countBits(ses.bits), ses.numtris, static_cast<double>(ses.brush));
    text += "\npoint the other hand at the gun:";
    text += "\ntrigger paints, grip erases, stick sizes";
    text += "\nbutton: touch it, pull the trigger";
    text += "\nB/Y: undo     menu button: done";
    int lineCount = 6;
    if(realtime < ses.feedbackUntil)
    {
        text += "\n" + ses.feedback;
        lineCount++;
    }
    constexpr float HINT_SCALE = 0.05f;
    const float half = 8.f * HINT_SCALE * (static_cast<float>(lineCount) * 0.5f + 0.4f);
    const float top = (BUTTON_HEIGHT * 1.5f + BUTTON_GAP * 0.5f + 0.02f) * m2u;
    text3d::queueOverlay(text, ses.panelAt + ses.panelUp * (top + half), angles, HINT_SCALE);
}

// The ray from the other hand and the brush's ring where it meets the gun (cyan painting, red erasing).
void drawBrush(const hands::State& s)
{
    const int other = otherHand();
    const glm::vec4 colour = ses.stroke < 0 ? glm::vec4{1.f, 0.3f, 0.25f, 0.95f} : glm::vec4{0.3f, 0.95f, 1.f, 0.95f};
    glm::vec3 hitWorld;
    if(!ses.hit || !view::gunToWorld(ses.hand, ses.hitModel, hitWorld))
    {
        lines::line(s.pos[other], s.pos[other] + hands::forward(s.aimRot[other]) * 24.f, 0.1f, colour, glm::vec4{colour.r, colour.g, colour.b, 0.f});
        return;
    }
    lines::line(s.pos[other], hitWorld, 0.08f, glm::vec4{colour.r, colour.g, colour.b, 0.3f}, colour);
    glm::vec3 u = glm::cross(ses.hitNormal, glm::vec3{0.f, 0.f, 1.f});
    u = glm::length(u) < 1e-3f ? glm::vec3{1.f, 0.f, 0.f} : glm::normalize(u);
    const glm::vec3 v = glm::cross(ses.hitNormal, u);
    constexpr int segments = 20;
    glm::vec3 prev{0.f};
    for(int i = 0; i <= segments; i++)
    {
        const float a = static_cast<float>(i) * 6.2831853f / segments;
        glm::vec3 p;
        if(!view::gunToWorld(ses.hand, ses.hitModel + ses.hitNormal * 0.05f + (u * cosf(a) + v * sinf(a)) * ses.brush, p))
        {
            return;
        }
        if(i > 0)
        {
            lines::line(prev, p, 0.06f, colour, colour);
        }
        prev = p;
    }
}

void hoverFrame(const hands::State& s)
{
    for(int h = 0; h < HAND_COUNT; h++)
    {
        const int b = buttonAtHand(s.pos[h]);
        if(b != ses.hoverButton[h] && b >= 0)
        {
            haptics::pulse(h, 0.015f, 0.25f);
        }
        ses.hoverButton[h] = b;
    }
}

// vr_barrelpaint [main|off]: enters it on that hand's gun (default: the main hand's).
void start_f()
{
    const bool off = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "off");
    (void)start(off ? HAND_OFF : HAND_MAIN, -1);
}

void done_f()
{
    finish(true);
}

void cancel_f()
{
    finish(false);
}

// vr_barrelpaint_press <index>: presses panel button `index` (0 Undo, 1 Clear, 2 Pre-marked, 3 Cancel, 4 Done).
void press_f()
{
    if(ses.active && Cmd_Argc() > 1)
    {
        press(Q_atoi(Cmd_Argv(1)));
    }
}

// vr_barrelpaint_brush <x> <y> <z> <radius> [0: erase]: a brush stroke at that point of the gun (its model units), as
// the trigger (the grip) there (a test aid).
void brush_f()
{
    if(!ses.active || Cmd_Argc() < 5)
    {
        Con_Printf("vr_barrelpaint_brush: not painting (vr_barrelpaint), or: x y z radius [0: erase]\n");
        return;
    }
    const glm::vec3 at{Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
    const bool on = Cmd_Argc() < 6 || Q_atoi(Cmd_Argv(5)) != 0;
    pushUndo();
    const int changed = brushAt(at, static_cast<float>(Q_atof(Cmd_Argv(4))), on);
    store();
    Con_Printf("barrelpaint brush %s %d triangles, %d marked\n", on ? "painted" : "erased", changed, countBits(ses.bits));
}

// vr_barrelmask_report <slot (1..)> [x0 y0 z0 x1 y1 z1 [part <model>]]: the slot's mask on its gun model (its marked
// triangles, their box in the model's units) and of those whose middle is in the box given, how many are marked; and
// likewise of a part drawn in the gun's place, its triangles marked through the gun's.
void report_f()
{
    const int slot = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) - 1 : weapons::heldSlot(HAND_MAIN);
    const cvar_t* id = slot >= 0 ? weapons::cvar(slot, Key::ID) : nullptr;
    za::String name = id ? id->string : "";
    if(const char* hash = strchr(name.cStr(), '#'))
    {
        name = za::String{name.cStr(), static_cast<za::SizeT>(hash - name.cStr())};
    }
    const qmodel_t* gun = name.empty() ? nullptr : Mod_ForName(name.cStr(), false);
    const Geometry* g = gun ? geometryOf(gun) : nullptr;
    Bits bits;
    if(!g)
    {
        Con_Printf("barrelmask slot %d: no gun model\n", slot + 1);
        return;
    }
    const bool has = slotMask(slot, gun, bits);
    glm::vec3 lo{1e9f}, hi{-1e9f};
    int marked = 0, inBox = 0, inBoxMarked = 0;
    glm::vec3 b0{-1e9f}, b1{1e9f};
    if(Cmd_Argc() >= 8)
    {
        b0 = glm::vec3{Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4))};
        b1 = glm::vec3{Q_atof(Cmd_Argv(5)), Q_atof(Cmd_Argv(6)), Q_atof(Cmd_Argv(7))};
    }
    for(za::SizeT t = 0; t < g->tris.size(); t++)
    {
        const bool on = has && bitOf(bits, static_cast<int>(t));
        const glm::vec3& c = g->centre[t];
        const bool in = glm::all(glm::greaterThanEqual(c, b0)) && glm::all(glm::lessThanEqual(c, b1));
        inBox += in ? 1 : 0;
        inBoxMarked += in && on ? 1 : 0;
        if(on)
        {
            marked++;
            lo = glm::min(lo, c);
            hi = glm::max(hi, c);
        }
    }
    Con_Printf("barrelmask slot %d %s tris %d marked %d mask %d x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f box %d marked %d\n",
        slot + 1, gun->name, static_cast<int>(g->tris.size()), marked, has ? 1 : 0, static_cast<double>(lo.x),
        static_cast<double>(hi.x), static_cast<double>(lo.y), static_cast<double>(hi.y), static_cast<double>(lo.z),
        static_cast<double>(hi.z), inBox, inBoxMarked);
    // A part drawn in the gun's place: its triangles marked through the gun's.
    const qmodel_t* part = Cmd_Argc() >= 10 && !q_strcasecmp(Cmd_Argv(8), "part") ? Mod_ForName(Cmd_Argv(9), false) : nullptr;
    const za::Vector<int>* map = part && has ? partMap(part, gun) : nullptr;
    if(map)
    {
        const Geometry* pg = geometryOf(part);
        int partMarked = 0, partBox = 0, partBoxMarked = 0, unmatched = 0;
        for(za::SizeT t = 0; t < map->size(); t++)
        {
            const bool on = bitOf(bits, (*map)[t]);
            const glm::vec3& c = pg->centre[t];
            const bool in = glm::all(glm::greaterThanEqual(c, b0)) && glm::all(glm::lessThanEqual(c, b1));
            partMarked += on ? 1 : 0;
            partBox += in ? 1 : 0;
            partBoxMarked += in && on ? 1 : 0;
            unmatched += (*map)[t] < 0 ? 1 : 0;
        }
        Con_Printf("barrelmask part %s tris %d marked %d unmatched %d box %d marked %d\n", part->name,
            static_cast<int>(map->size()), partMarked, unmatched, partBox, partBoxMarked);
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_barrelpaint", start_f);
    Cmd_AddCommand("vr_barrelpaint_done", done_f);
    Cmd_AddCommand("vr_barrelpaint_cancel", cancel_f);
    Cmd_AddCommand("vr_barrelpaint_press", press_f);
    Cmd_AddCommand("vr_barrelpaint_brush", brush_f);
    Cmd_AddCommand("vr_barrelmask_report", report_f);
}

float gpuMask(const qmodel_t* drawn, const qmodel_t* gun, int slot)
{
    if(!drawn || !gun || slot < 0)
    {
        return 0.f;
    }
    const aliashdr_t* hdr = aliasOf(drawn);
    if(!hdr)
    {
        return 0.f;
    }
    Uploaded& u = uploaded[PairKey{drawn, gun, slot}];
    if(u.generation == weapons::settingsGeneration() && u.drawnHdr == hdr)
    {
        return u.first >= 0 ? static_cast<float>(u.first + 1) : 0.f;
    }
    u.generation = weapons::settingsGeneration();
    u.drawnHdr = hdr;
    u.first = -1;
    Bits gunBits;
    if(!slotMask(slot, gun, gunBits))
    {
        return 0.f;
    }
    Bits bits;
    if(drawn == gun)
    {
        bits = gunBits;
    }
    else
    {
        const za::Vector<int>* map = partMap(drawn, gun);
        if(!map)
        {
            return 0.f;
        }
        bits.resize(static_cast<za::SizeT>((map->size() + 31) / 32), 0u);
        for(za::SizeT t = 0; t < map->size(); t++)
        {
            if(bitOf(gunBits, (*map)[t]))
            {
                setBit(bits, static_cast<int>(t), true);
            }
        }
    }
    const int words = static_cast<int>(bits.size());
    if(words <= 0 || words > bufferWords)
    {
        return 0.f;
    }
    if(!buffer)
    {
        buffer = GL_CreateBuffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW, "vr barrel masks",
            sizeof(za::U32) * bufferWords, nullptr);
    }
    if(bufferUsed + words > bufferWords) // (full: from the start again, the others written again when next drawn)
    {
        bufferUsed = 0;
        for(auto& [k, other] : uploaded)
        {
            other.generation = 0;
        }
        u.generation = weapons::settingsGeneration();
    }
    GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    GL_BufferSubDataFunc(GL_SHADER_STORAGE_BUFFER, static_cast<GLintptr>(bufferUsed) * static_cast<GLintptr>(sizeof(za::U32)),
        static_cast<GLsizeiptr>(words) * static_cast<GLsizeiptr>(sizeof(za::U32)), bits.data());
    u.first = bufferUsed;
    bufferUsed += words;
    return static_cast<float>(u.first + 1);
}

void bind()
{
    if(buffer)
    {
        GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 16, buffer, 0, sizeof(za::U32) * bufferWords);
    }
}

bool active()
{
    return ses.active && ses.world == worldGeneration();
}

bool painting(int hand)
{
    return active() && hand == ses.hand;
}

bool start(int hand, int returnPage)
{
    const auto refuse = [](const char* why) {
        Con_Printf("Paint Barrel Mask: %s\n", why);
        return false;
    };
    if(ses.active)
    {
        return false;
    }
    if(!vrActive())
    {
        return refuse("it needs the headset (or vr_backend mock)");
    }
    if(cls.state != ca_connected || !cl.worldmodel || !hands::current().valid)
    {
        return refuse("start a map first");
    }
    const qmodel_t* gun = weapons::heldModel(hand);
    const int slot = weapons::heldSlot(hand);
    const Geometry* g = slot >= 0 && slot != weapons::fistSlot() ? geometryOf(gun) : nullptr;
    if(!g || g->tris.empty())
    {
        return refuse(hand == HAND_MAIN ? "hold a gun in the main hand" : "hold a gun in the off hand");
    }
    if(posing::active())
    {
        posing::stop(false);
    }
    if(key_dest == key_menu)
    {
        menuui::backToGame(HAND_MAIN);
    }
    Cbuf_AddText("-attack\n-offhandattack\n");
    ses = Session{};
    ses.active = true;
    ses.world = worldGeneration();
    ses.returnPage = returnPage;
    ses.hand = hand;
    ses.slot = slot;
    ses.gun = gun;
    ses.numtris = static_cast<int>(g->tris.size());
    const cvar_t* var = maskVar(slot);
    ses.entry = var ? var->string : "0";
    if(!slotMask(slot, gun, ses.bits))
    {
        ses.bits.clear();
        ses.bits.resize(static_cast<za::SizeT>((ses.numtris + 31) / 32), 0u);
    }
    placePanel(hands::current());
    const cvar_t* id = weapons::cvar(slot, Key::ID);
    say(va("started on %s (slot %d): %d triangles marked", id ? id->string : "?", slot + 1, countBits(ses.bits)));
    Con_Printf("Paint Barrel Mask: started on slot %d, %d of %d triangles marked\n", slot + 1, countBits(ses.bits), ses.numtris);
    menuui::uiSound(menuui::UiSound::Adjust);
    return true;
}

void finish(bool keep)
{
    if(!ses.active)
    {
        return;
    }
    if(!keep)
    {
        if(cvar_t* var = maskVar(ses.slot))
        {
            Cvar_SetQuick(var, ses.entry.cStr());
        }
    }
    const int page = ses.returnPage;
    ses.active = false;
    ses.undo.clear();
    Con_Printf("Paint Barrel Mask: %s (slot %d: %d of %d triangles marked)\n", keep ? "done" : "cancelled, the mask put back",
        ses.slot + 1, keep ? countBits(ses.bits) : -1, ses.numtris);
    S_LocalSound(keep ? "misc/menu2.wav" : "misc/menu1.wav");
    if(page >= 0)
    {
        menu::reopen(page);
    }
}

bool button(int hand, posing::Button b, bool down)
{
    const int i = static_cast<int>(b);
    if(hand < 0 || hand >= HAND_COUNT || i < 0 || i >= 6)
    {
        return false;
    }
    if(!down)
    {
        const bool mine = ses.eaten[hand][i];
        ses.eaten[hand][i] = false;
        if(mine && hand == otherHand() && (b == posing::Button::Trigger || b == posing::Button::Grip))
        {
            ses.stroke = 0;
        }
        return mine || (active() && b == posing::Button::Grip);
    }
    if(!active() || key_dest != key_game)
    {
        return false;
    }
    ses.eaten[hand][i] = true;
    switch(b)
    {
        case posing::Button::Trigger:
            if(ses.hoverButton[hand] >= 0)
            {
                haptics::pulse(hand, 0.03f, 0.5f);
                press(ses.hoverButton[hand]);
            }
            else if(hand == otherHand())
            {
                ses.stroke = 1;
                ses.strokeUndone = false;
            }
            break;
        case posing::Button::Grip:
            if(hand == otherHand())
            {
                ses.stroke = -1;
                ses.strokeUndone = false;
            }
            break;
        case posing::Button::Secondary: undo(); break;
        case posing::Button::Menu: finish(true); break;
        default: break;
    }
    return true;
}

void sticks(const glm::vec2& off, const glm::vec2& main)
{
    if(!active())
    {
        return;
    }
    const float y = otherHand() == HAND_MAIN ? main.y : off.y;
    if(za::fabs(y) > 0.2f)
    {
        ses.brush = za::clamp(ses.brush * (1.f + y * 1.5f * static_cast<float>(host_frametime)), BRUSH_MIN, BRUSH_MAX);
    }
}

void frame()
{
    if(!ses.active)
    {
        return;
    }
    // Leaving with the map, the game, the headset, the menu opened, the gun gone from the hand (the changes kept).
    if(ses.world != worldGeneration() || cls.state != ca_connected || !vrActive() || key_dest == key_menu ||
        weapons::heldModel(ses.hand) != ses.gun)
    {
        ses.returnPage = -1;
        finish(true);
        return;
    }
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    hoverFrame(s);
    aimFrame(s);
    strokeFrame();
    drawBrush(s);
    drawPanel();
}

bool buttonPoint(int index, glm::vec3& out)
{
    if(!active() || index < 0 || index >= BtnCount)
    {
        return false;
    }
    out = buttonAt(index);
    return true;
}

} // namespace qvr::barrelmask

extern "C" void VR_BarrelMaskBind(void)
{
    qvr::barrelmask::bind();
}
