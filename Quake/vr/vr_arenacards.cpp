// vr_arenacards.cpp -- the Survival Arena's perk cards (vr_arenacards.hpp; QC/vr_arena_perks.qc; docs/vr-port/ROUND23.md,
// "Perk cards").
//
// The panel: 320 x 200 virtual pixels (four texels each, mipmapped), drawn with Quake's font and flat fills into an
// image of its own, shown in each eye on a quad vr_arena_cards_width units wide, vr_arena_cards_distance in front of
// the head and a little below the eyes, leaning back to face them (not depth tested, as the menus: nothing in the room
// hides it). A title, the cards in one row (two rows past four: Nightmare offers five), each its perk's colour band and
// name, its item's thumbnail (vr_toolgun_icons.cpp) and what it does; a footer saying how to pick and the perks held.
//
// The QC's offer is read through the server's VM once a frame (a single player game), as the toolgun's panel reads its
// catalogue: VR_ARK_CardSerial (0: none open; a new number: a new offer, placed in front again), _CardCount,
// _CardBit, _Name, _What, _Model, _Colour, _CardTitle, _PoolSize; VR_ARK_CardsSeen is said every frame the panel can
// be shown (a game with no headset shows nothing: the QC withdraws such an offer); VR_ARK_CardPick(i) is the choice.

#include "vr_arenacards.hpp"

#include "vr_api.h"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_main.hpp"
#include "vr_menuui.hpp"
#include "vr_progs.hpp"
#include "vr_renderdoc.hpp"
#include "vr_toolgun.hpp"
#include "vr_toolgun_icons.hpp"
#include "vr_toolpanel.hpp"

#include "Zancle/Base/Exchange.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <stdio.h>
#include <string.h>

using namespace qvr;

namespace
{

constexpr float panelWidth = 320.f; // its virtual pixels
constexpr float panelHeight = 200.f;
constexpr int texelsPerPixel = 4;
constexpr int maxCards = 8;          // the QC's pool (ARK_POOL_N)
constexpr double armSeconds = 0.5;   // a trigger held as the cards come up is not a pick: presses count after this
constexpr double awaySeconds = 1.0;  // looked away from the panel this long: it comes back in front of the head
constexpr float awayCos = 0.5f;      // looked away: the panel more than 60 degrees off the view's middle
constexpr float awayFar = 3.f;       // or further than this many times its distance (walked off)
constexpr float dropBelowEyes = 0.1f;  // its middle below the eyes, as a share of its distance

// The panel image's drawing (vr_toolpanel.hpp).
using toolpanel::fill;
using toolpanel::Rect;
using toolpanel::text;
using toolpanel::textFit;

struct Card
{
    int bit{0};
    char name[32]{};
    char what[96]{};
    char model[64]{};
    glm::vec3 colour{0.5f};
};

// The panel's state (the main thread: the input, the host frame, the 2D pass, the eyes).
struct CardsState
{
    int serial{0}; // the QC's offer shown (0: none)
    int count{0};
    Card cards[maxCards];
    char title[48]{};
    int held{0}; // the perks the run holds, and the pool's size (the footer)
    int pool{maxCards};

    // Where it is in the world: its middle, unit right and up (facing the head it was placed for).
    bool placed{false};
    glm::vec3 centre{0.f}, right{0.f, -1.f, 0.f}, up{0.f, 0.f, 1.f};
    bool inGame{false};     // shown last frame (back from a menu: placed in front again)
    double shownAt{0.0};    // the client's time the offer came up (the arming)
    double awaySince{-1.0}; // the client's time the head turned away from it (-1: it is in view)

    // Each hand's laser (HAND_OFF, HAND_MAIN), from the host frame.
    bool hit[2]{};
    glm::vec2 cursor[2]{glm::vec2{0.f}, glm::vec2{0.f}};
    glm::vec3 point[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    int hovered[2]{-1, -1};
    bool takenTrigger[2]{}; // a trigger press the panel took (its release is the panel's too)
    bool mock{false};       // vr_arena_cards_mock_laser: the main hand's laser on this spot whatever its pose
    glm::vec2 mockSpot{0.f};

    int pickPending{-1}; // a card picked by a trigger or a key, passed to the QC in the host frame

    gfx::Target target;
};
CardsState cs;

// ---------------------------------------------------------------------------------------------------------------------
// The QC's offer.

void clearOffer()
{
    if(cs.serial != 0)
    {
        Con_DPrintf("arena cards: closed\n");
    }
    cs.serial = 0;
    cs.count = 0;
    cs.placed = false;
    cs.inGame = false;
    cs.awaySince = -1.0;
    cs.pickPending = -1;
    for(int h = 0; h < 2; h++)
    {
        cs.hit[h] = false;
        cs.hovered[h] = -1;
    }
}

[[nodiscard]] bool gameUp()
{
    return sv.active && svs.maxclients == 1 && sv.qcvm.progs && cls.state == ca_connected && cls.signon == SIGNONS;
}

// The offer asked of the QC (a new one read in full), a pick passed on, and the panel's word that it shows them.
void poll()
{
    if(!gameUp())
    {
        clearOffer();
        return;
    }
    const progs::ServerVm vm;
    if(cs.pickPending >= 0 && cs.serial != 0)
    {
        const int pick = cs.pickPending;
        cs.pickPending = -1;
        if(progs::qcFloat(progs::bindings().arenaCards.CardPick, static_cast<float>(pick)) != 0.f)
        {
            Con_DPrintf("arena cards: card %d picked (%s)\n", pick + 1, cs.cards[pick].name);
            qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
        }
    }
    cs.pickPending = -1;

    const int serial = static_cast<int>(progs::qcFloat(progs::bindings().arenaCards.CardSerial));
    if(serial <= 0)
    {
        clearOffer();
        return;
    }
    if(serial != cs.serial)
    {
        clearOffer();
        cs.serial = serial;
        cs.count = za::clamp(static_cast<int>(progs::qcFloat(progs::bindings().arenaCards.CardCount)), 0, maxCards);
        for(int i = 0; i < cs.count; i++)
        {
            Card& c = cs.cards[i];
            c.bit = static_cast<int>(progs::qcFloat(progs::bindings().arenaCards.CardBit, static_cast<float>(i)));
            progs::qcText(c.name, sizeof(c.name), progs::bindings().arenaCards.Name, static_cast<float>(c.bit));
            progs::qcText(c.what, sizeof(c.what), progs::bindings().arenaCards.What, static_cast<float>(c.bit));
            progs::qcText(c.model, sizeof(c.model), progs::bindings().arenaCards.Model, static_cast<float>(c.bit));
            c.colour = progs::qcVec(progs::bindings().arenaCards.Colour, static_cast<float>(c.bit), glm::vec3{0.5f});
        }
        progs::qcText(cs.title, sizeof(cs.title), progs::bindings().arenaCards.CardTitle);
        cs.pool = za::max(1, static_cast<int>(progs::qcFloat(progs::bindings().arenaCards.PoolSize)));
        cs.shownAt = cl.time;
        Con_DPrintf("arena cards: offer %d, %d cards\n", serial, cs.count);
        qvr::menuui::uiSound(qvr::menuui::UiSound::Talk);
    }
    if(vrActive())
    {
        cs.held = static_cast<int>(progs::qcFloat(progs::bindings().arenaCards.CardsSeen));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Where it is.

[[nodiscard]] float distance()
{
    return za::max(vr_arena_cards_distance.value, 8.f);
}

[[nodiscard]] float width()
{
    return za::clamp(vr_arena_cards_width.value, 8.f, 200.f);
}

[[nodiscard]] glm::vec3 levelForward(const hands::State& s)
{
    const float yaw = glm::radians(s.headAngles.y);
    return glm::vec3{za::cos(yaw), za::sin(yaw), 0.f};
}

[[nodiscard]] glm::vec3 viewForward(const hands::State& s)
{
    const float pitch = glm::radians(s.headAngles.x), yaw = glm::radians(s.headAngles.y);
    return glm::vec3{za::cos(pitch) * za::cos(yaw), za::cos(pitch) * za::sin(yaw), -za::sin(pitch)};
}

// In front of the head (its yaw: the panel level whatever the head's pitch), a little below the eyes, leaning back to
// face them.
void place(const hands::State& s)
{
    const glm::vec3 fwd = levelForward(s);
    const float d = distance();
    const float drop = d * dropBelowEyes;
    cs.centre = s.head + fwd * d - glm::vec3{0.f, 0.f, drop};
    cs.right = glm::vec3{fwd.y, -fwd.x, 0.f};
    const float tilt = za::atan2(drop, d);
    cs.up = glm::normalize(glm::vec3{0.f, 0.f, 1.f} * za::cos(tilt) + fwd * za::sin(tilt));
    cs.placed = true;
    cs.awaySince = -1.0;
}

void quad(glm::vec3& corner, glm::vec3& xAxis, glm::vec3& yAxis)
{
    const float w = width();
    const float h = w * panelHeight / panelWidth;
    corner = cs.centre - cs.right * (w * 0.5f) - cs.up * (h * 0.5f);
    xAxis = cs.right * w;
    yAxis = cs.up * h;
}

[[nodiscard]] glm::vec3 pointAt(const glm::vec2& cursor)
{
    glm::vec3 corner, xAxis, yAxis;
    quad(corner, xAxis, yAxis);
    return corner + xAxis * (cursor.x / panelWidth) + yAxis * (1.f - cursor.y / panelHeight);
}

// Where hand `hand`'s laser meets the panel: the point, and the panel's pixel there.
[[nodiscard]] bool laserHit(const hands::State& s, int hand, glm::vec3& point, glm::vec2& cursor)
{
    if(cs.mock && hand == HAND_MAIN)
    {
        cursor = cs.mockSpot;
        point = pointAt(cursor);
        return true;
    }
    glm::vec3 origin, dir, r, u, corner, xAxis, yAxis;
    if(!toolgun::controllerFrame(s, hand, origin, dir, r, u))
    {
        return false;
    }
    quad(corner, xAxis, yAxis);
    const glm::vec3 normal = glm::cross(xAxis, yAxis);
    const float along = glm::dot(dir, normal);
    if(za::fabs(along) < 1e-6f)
    {
        return false;
    }
    const float t = glm::dot(corner - origin, normal) / along;
    if(t <= 0.f)
    {
        return false;
    }
    point = origin + dir * t;
    const float uu = glm::dot(point - corner, xAxis) / glm::dot(xAxis, xAxis);
    const float vv = glm::dot(point - corner, yAxis) / glm::dot(yAxis, yAxis);
    if(uu < 0.f || uu > 1.f || vv < 0.f || vv > 1.f)
    {
        return false;
    }
    cursor = glm::vec2{uu * panelWidth, (1.f - vv) * panelHeight};
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// The layout (virtual pixels, y down).

constexpr float titleBottom = 34.f;
constexpr float footerTop = 186.f;
constexpr float sidePad = 10.f;
constexpr float cardGap = 8.f;

[[nodiscard]] Rect cardRect(int i, int n)
{
    const int rows = n > 4 ? 2 : 1;
    const int perRow = (n + rows - 1) / rows;
    const int row = i / perRow;
    const int inRow = row == rows - 1 ? n - perRow * (rows - 1) : perRow;
    const int col = i - row * perRow;
    const float cardW = (panelWidth - 2.f * sidePad - (perRow - 1) * cardGap) / perRow;
    const float areaH = footerTop - titleBottom - 4.f;
    const float cardH = (areaH - (rows - 1) * 6.f) / rows;
    const float rowW = inRow * cardW + (inRow - 1) * cardGap;
    const float x0 = (panelWidth - rowW) * 0.5f + col * (cardW + cardGap);
    const float y0 = titleBottom + 2.f + row * (cardH + 6.f);
    return Rect{x0, y0, x0 + cardW, y0 + cardH};
}

[[nodiscard]] int cardAt(const glm::vec2& p)
{
    for(int i = 0; i < cs.count; i++)
    {
        if(cardRect(i, cs.count).contains(p))
        {
            return i;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing (into the panel's image, its virtual pixels).

void frameRect(const Rect& r, float t, const glm::vec4& c)
{
    fill(Rect{r.x0, r.y0, r.x1, r.y0 + t}, c);
    fill(Rect{r.x0, r.y1 - t, r.x1, r.y1}, c);
    fill(Rect{r.x0, r.y0 + t, r.x0 + t, r.y1 - t}, c);
    fill(Rect{r.x1 - t, r.y0 + t, r.x1, r.y1 - t}, c);
}

// Words wrapped to `width`, each line centred on cx, from y down, `dim` a character, at most `maxLines`.
void paragraphCentred(float cx, float y, float width, float dim, const char* s, int maxLines)
{
    const int perLine = za::max(1, static_cast<int>(width / dim));
    int lines = 0;
    while(*s && lines < maxLines)
    {
        int n = static_cast<int>(strlen(s));
        if(n > perLine)
        {
            n = perLine;
            while(n > 0 && s[n] != ' ')
            {
                n--;
            }
            if(n == 0)
            {
                n = perLine;
            }
        }
        char line[96];
        q_strlcpy(line, s, static_cast<size_t>(za::min(static_cast<int>(sizeof(line)), n + 1)));
        const float len = static_cast<float>(strlen(line));
        text(cx - len * dim * 0.5f, y + lines * (dim + 2.f), dim, line, false);
        lines++;
        s += n;
        while(*s == ' ')
        {
            s++;
        }
    }
}

[[nodiscard]] bool cardLit(int i)
{
    return (cs.hit[0] && cs.hovered[0] == i) || (cs.hit[1] && cs.hovered[1] == i);
}

void drawCard(int i)
{
    const Card& c = cs.cards[i];
    const Rect r = cardRect(i, cs.count);
    const bool lit = cardLit(i);
    const glm::vec4 edge{c.colour, 1.f};
    const glm::vec4 band{c.colour * (lit ? 0.75f : 0.5f), 1.f};

    fill(r, lit ? glm::vec4{0.16f, 0.13f, 0.10f, 1.f} : glm::vec4{0.10f, 0.08f, 0.06f, 1.f});
    const float bandH = za::min(18.f, r.h() * 0.2f);
    fill(Rect{r.x0, r.y0, r.x1, r.y0 + bandH}, band);
    frameRect(r, lit ? 2.f : 1.f, lit ? glm::vec4{1.f, 0.9f, 0.6f, 1.f} : edge);

    textFit(r.cx(), r.y0 + (bandH - 8.f) * 0.5f, r.w() - 8.f, 8.f, c.name, false);

    // Its number, the key that picks it.
    char num[4];
    snprintf(num, sizeof(num), "%d", i + 1);
    text(r.x0 + 3.f, r.y1 - 9.f, 6.f, num, true);

    // The item's picture.
    const float textLines = 5.f;
    const float textH = textLines * 8.f;
    const float picSize = za::max(8.f, za::min(r.w() - 16.f, r.h() - bandH - textH - 10.f));
    const bool side = !strncmp(c.model, "progs/v_", 8); // (a gun's model: from its side, its length across)
    if(const gfx::Picture* pic = c.model[0] ? toolgun::icons::get(c.model, 0, side) : nullptr)
    {
        gfx::draw2D::picture(*pic, r.cx() - picSize * 0.5f, r.y0 + bandH + 3.f, picSize, picSize);
    }

    paragraphCentred(r.cx(), r.y0 + bandH + picSize + 6.f, r.w() - 6.f, 6.f, c.what, static_cast<int>(textLines));

    if(lit)
    {
        fill(Rect{r.x0 + 2.f, r.y0 + 2.f, r.x1 - 2.f, r.y1 - 2.f}, glm::vec4{1.f, 0.85f, 0.5f, 0.08f});
    }
}

void drawPanel()
{
    fill(Rect{0.f, 0.f, panelWidth, panelHeight}, glm::vec4{0.60f, 0.40f, 0.18f, 1.f});
    fill(Rect{1.f, 1.f, panelWidth - 1.f, panelHeight - 1.f}, glm::vec4{0.05f, 0.04f, 0.03f, 1.f});

    textFit(panelWidth * 0.5f, 6.f, panelWidth - 20.f, 10.f, cs.title, true);
    textFit(panelWidth * 0.5f, 21.f, panelWidth - 20.f, 7.f, "Choose a perk - the arena waits for you", false);

    for(int i = 0; i < cs.count; i++)
    {
        drawCard(i);
    }

    char foot[96];
    snprintf(foot, sizeof(foot), "Point at a card and pull the trigger (or press 1-%d)   Perks held: %d of %d", cs.count,
        cs.held, cs.pool);
    textFit(panelWidth * 0.5f, footerTop + 4.f, panelWidth - 12.f, 6.f, foot, true);
}

// ---------------------------------------------------------------------------------------------------------------------
// Commands.

// vr_arena_cards_mock_laser <card 1..n> | <x> <y> | off (tests): the main hand's laser on the middle of that card (or
// that spot of the panel: its virtual pixels, 320 x 200 from its top left), whatever the hand's pose.
void mockLaser_f()
{
    if(Cmd_Argc() == 2 && !q_strcasecmp(Cmd_Argv(1), "off"))
    {
        cs.mock = false;
        return;
    }
    if(Cmd_Argc() == 3)
    {
        cs.mock = true;
        cs.mockSpot = glm::vec2{Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2))};
        return;
    }
    if(Cmd_Argc() == 2)
    {
        const int card = Q_atoi(Cmd_Argv(1));
        if(card < 1 || card > cs.count)
        {
            Con_Printf("vr_arena_cards_mock_laser: no card %d (%d shown)\n", card, cs.count);
            return;
        }
        const Rect r = cardRect(card - 1, cs.count);
        cs.mock = true;
        cs.mockSpot = glm::vec2{r.cx(), r.cy()};
        return;
    }
    Con_Printf("vr_arena_cards_mock_laser <card 1..n> | <x> <y> | off\n");
}

// vr_arena_cards_pick <card 1..n>: picks it (as its number key does), the cards shown.
void pick_f()
{
    const int card = Cmd_Argc() == 2 ? Q_atoi(Cmd_Argv(1)) : 0;
    if(card < 1 || card > cs.count)
    {
        Con_Printf("vr_arena_cards_pick <card 1..%d>\n", cs.count);
        return;
    }
    cs.pickPending = card - 1;
}

// vr_arena_cards_offer (Debug > Automation & Self-Tests): the arena offers its cards now (between two waves).
void offer_f()
{
    if(!gameUp())
    {
        Con_Printf("vr_arena_cards_offer: no game running\n");
        return;
    }
    const progs::ServerVm vm;
    if(!progs::callQc(progs::bindings().arenaCards.DebugOffer))
    {
        Con_Printf("vr_arena_cards_offer: these progs have no Survival Arena perks\n");
    }
}

void dump_f()
{
    Con_Printf("arena cards: offer %d, %d cards, %s, placed %d at %.0f %.0f %.0f, held %d of %d\n", cs.serial, cs.count,
        arenacards::shown() ? "shown" : "hidden", cs.placed ? 1 : 0, cs.centre.x, cs.centre.y, cs.centre.z, cs.held, cs.pool);
    for(int h = 0; h < 2; h++)
    {
        Con_Printf("  hand %d: laser %s at %.0f %.0f, card %d\n", h, cs.hit[h] ? "on it" : "off it", cs.cursor[h].x,
            cs.cursor[h].y, cs.hovered[h] + 1);
    }
    for(int i = 0; i < cs.count; i++)
    {
        const Rect r = cardRect(i, cs.count);
        Con_Printf("  card %d: %s (%d) %.0f %.0f %.0f %.0f\n", i + 1, cs.cards[i].name, cs.cards[i].bit, r.x0, r.y0, r.x1, r.y1);
    }
}

} // namespace

namespace qvr::arenacards
{

bool shown()
{
    return cs.count > 0 && key_dest == key_game && !cl.intermission && !con_forcedup;
}

void frame(const hands::State& s)
{
    poll();
    if(!shown() || !s.valid)
    {
        cs.inGame = false;
        cs.hit[0] = cs.hit[1] = false;
        return;
    }
    if(!cs.inGame || !cs.placed)
    {
        place(s); // (a new offer, or back from a menu: in front of the head again)
    }
    cs.inGame = true;

    // Looked away from it (or walked off) for a second: it comes back in front of the head.
    const glm::vec3 toPanel = cs.centre - s.head;
    const float dist = glm::length(toPanel);
    const bool away = dist > distance() * awayFar || (dist > 1e-3f && glm::dot(toPanel / dist, viewForward(s)) < awayCos);
    if(!away)
    {
        cs.awaySince = -1.0;
    }
    else if(cs.awaySince < 0.0)
    {
        cs.awaySince = cl.time;
    }
    else if(cl.time - cs.awaySince > awaySeconds)
    {
        place(s);
        Con_DPrintf("arena cards: placed in front of you again\n");
    }

    for(int h = 0; h < 2; h++)
    {
        const int was = cs.hit[h] ? cs.hovered[h] : -1;
        cs.hit[h] = laserHit(s, h, cs.point[h], cs.cursor[h]);
        cs.hovered[h] = cs.hit[h] ? cardAt(cs.cursor[h]) : -1;
        if(cs.hovered[h] >= 0 && cs.hovered[h] != was)
        {
            haptics::pulse(h, 0.01f, 0.15f, 200.f); // (a tick on each card the laser comes onto)
        }
    }
}

bool button(int hand, posing::Button b, bool down)
{
    if(b != posing::Button::Trigger || hand < 0 || hand > 1)
    {
        return false;
    }
    if(!down)
    {
        return za::exchange(cs.takenTrigger[hand], false);
    }
    if(!shown() || !cs.hit[hand])
    {
        return false; // (off the panel: the trigger is the gun's)
    }
    cs.takenTrigger[hand] = true;
    if(cs.hovered[hand] >= 0 && cl.time - cs.shownAt >= armSeconds)
    {
        cs.pickPending = cs.hovered[hand];
        haptics::pulse(hand, 0.03f, 0.5f, 200.f);
    }
    return true;
}

bool key(int k, bool down)
{
    if(!shown() || k < '1' || k > '0' + cs.count)
    {
        return false;
    }
    if(down && cl.time - cs.shownAt >= armSeconds)
    {
        cs.pickPending = k - '1';
    }
    return true;
}

void render()
{
    if(!shown() || !cs.placed)
    {
        return;
    }
    toolgun::icons::makePending(2);
    gfx::ensureTarget(cs.target, static_cast<int>(panelWidth) * texelsPerPixel, static_cast<int>(panelHeight) * texelsPerPixel,
        true, "arena cards");
    gfx::begin2D(cs.target, static_cast<int>(panelWidth), static_cast<int>(panelHeight));
    drawPanel();
    gfx::end2D();
}

void drawInEye(const hands::State& s)
{
    if(!shown() || !cs.placed || qvr::renderdoc::hidingPanel() || !cs.target.texture || !s.valid)
    {
        return;
    }
    glm::vec3 corner, xAxis, yAxis;
    quad(corner, xAxis, yAxis);
    const glm::vec4 white{1.f};
    const gfx::Vertex c[4] = {{corner, {0.f, 0.f}, white}, {corner + xAxis, {1.f, 0.f}, white},
        {corner + xAxis + yAxis, {1.f, 1.f}, white}, {corner + yAxis, {0.f, 1.f}, white}};
    const gfx::Vertex quadTris[6] = {c[0], c[1], c[2], c[0], c[2], c[3]};
    gfx::draw(quadTris, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Opaque, .depthTest = false, .depthWrite = false}, cs.target.texture);

    // Both hands' lasers: to where they meet the panel, a short stub when they don't.
    for(int h = 0; h < 2; h++)
    {
        glm::vec3 origin, fwd, r, u;
        if(!toolgun::controllerFrame(s, h, origin, fwd, r, u))
        {
            continue;
        }
        menuui::drawLaser(origin, cs.hit[h] ? cs.point[h] : origin + fwd * 8.f, cs.hit[h], cs.takenTrigger[h], 0.5f);
    }
}

void registerCommands()
{
    Cmd_AddCommand("vr_arena_cards_mock_laser", mockLaser_f);
    Cmd_AddCommand("vr_arena_cards_pick", pick_f);
    Cmd_AddCommand("vr_arena_cards_offer", offer_f);
    Cmd_AddCommand("vr_arena_cards_dump", dump_f);
}

} // namespace qvr::arenacards

extern "C" int VR_ArenaCardsKey(int key, int down)
{
    return qvr::arenacards::key(key, down != 0) ? 1 : 0;
}
