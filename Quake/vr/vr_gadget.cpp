// vr_gadget.cpp -- see vr_gadget.hpp.
//
// The screen is drawn with the engine's own 2D functions (the status bar's pictures from gfx.wad,
// the console font; vr_gfx.hpp's draw2D) into an offscreen target, on a virtual 240 x 150 screen
// covering it, at the end of the 2D pass. Each eye's scene then shows the texture over the model's
// screen, a frame later, after the opaque entities (so that the bloom catches it): in one phosphor
// colour, as a small CRT (vr_gadget_crt), with a soft glow round its edge (vr_screen_glow, drawn by
// vr_text3d with the weapons' ammo screens').
//
// The log over it (vr_notify_wrist) is the console's notify lines (console.c keeps the times of
// its last 16 lines for it: Con_NotifyLine), laid out by vr_text3d facing the viewer.

#include "vr_gadget.hpp"
#include "vr_color.hpp"
#include "vr_gfx.hpp"
#include "vr_hue.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_lighting.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_meleehud.hpp"
#include "vr_profile.hpp"
#include "vr_text3d.hpp"
#include "vr_hands.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Erase.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "vr_zancle.hpp"

#include <string.h>

extern "C" qboolean Image_WritePNG(const char* name, byte* data, int width, int height, int bpp, qboolean upsidedown); // image.c

namespace qvr::gadget
{
namespace
{

constexpr int width = 240;  // the virtual screen, and the texture's size (at twice that)
constexpr int height = 150;

Pose current;
gfx::Target target;

// The log's shape: characters a line, lines at most, seconds a line fades out over.
constexpr int logColumns = 40;
constexpr int logRows = 8;
constexpr float logFade = 1.f;

// The screen's lights' keys (no entity's: entities' are positive; vr_emissive.cpp's ammo screens
// take -0x5C00 and below): the beam in front of it, and the faint glow round it.
constexpr int beamLightKey = -0x5C10;
constexpr int glowLightKey = -0x5C11;

// The screen's CRT strength (vr_gadget_crt).
[[nodiscard]] float crtStrength()
{
    return CLAMP(0.f, vr_gadget_crt.value, 2.f);
}

// A number from 0 to 1 for `n`, the same every time.
[[nodiscard]] float hash(uint32_t n)
{
    n ^= n >> 16;
    n *= 0x7feb352dU;
    n ^= n >> 15;
    n *= 0x846ca68bU;
    n ^= n >> 16;
    return static_cast<float>(n & 0xffffff) / static_cast<float>(0x1000000);
}

// Status bar pictures (gfx.wad) by name.
[[nodiscard]] const char* digitPic(int digit, bool red)
{
    static char name[16];
    if(digit < 0)
    {
        return red ? "anum_minus" : "num_minus";
    }
    q_snprintf(name, sizeof(name), red ? "anum_%d" : "num_%d", digit);
    return name;
}

[[nodiscard]] const char* facePic(int level) // 0 hurt .. 4 healthy
{
    static const char* const names[5] = {"face5", "face4", "face3", "face2", "face1"};
    return names[level];
}

constexpr const char* armorPics[3] = {"sb_armor1", "sb_armor2", "sb_armor3"};
constexpr const char* ammoPics[4] = {"sb_shells", "sb_nails", "sb_rocket", "sb_cells"};
constexpr const char* keyPics[2] = {"sb_key1", "sb_key2"};
constexpr const char* powerupPics[4] = {"sb_invis", "sb_invuln", "sb_suit", "sb_quad"};
constexpr const char* sigilPics[4] = {"sb_sigil1", "sb_sigil2", "sb_sigil3", "sb_sigil4"};

constexpr glm::vec4 white{1.f};

// The screen's palette, from vr_gadget_screen_hue (by default the player's, vr_hue.hpp) /
// _brightness / _background (the default is the green of a phosphor screen).
struct Palette
{
    glm::vec3 background;
    glm::vec3 scanline;
    glm::vec3 line; // frame and separators
    glm::vec4 text;
};

[[nodiscard]] Palette palette()
{
    const cvar_t& own = vr_gadget_screen_hue;
    const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f);
    const float back = CLAMP(0.f, vr_gadget_screen_background.value, 4.f);
    Palette p;
    p.background = hue::color(own, 0.57f, 0.07f * back);
    p.scanline = hue::color(own, 0.6f, 0.05f * back);
    p.line = glm::min(hue::color(own, 0.58f, 0.6f * bright), glm::vec3{1.f});
    p.text = glm::vec4{glm::min(hue::color(own, 0.55f, bright), glm::vec3{1.f}), 1.f};
    return p;
}

using gfx::draw2D::fill;

// Big numbers, right-aligned in `digits` places of 24 x 24 (scaled). The screen is of one colour:
// red ones (a warning) blink.
void number(float x, float y, int value, int digits, bool red, float scale)
{
    if(red && za::fmod(realtime, 0.8) >= 0.5)
    {
        return;
    }
    char str[16];
    q_snprintf(str, sizeof(str), "%d", CLAMP(-99, value, 999));
    const int length = static_cast<int>(strlen(str));
    x += static_cast<float>(digits - length) * 24.f * scale;
    for(const char* c = str; *c; c++)
    {
        gfx::draw2D::pic(x, y, digitPic(*c == '-' ? -1 : *c - '0', red), scale);
        x += 24.f * scale;
    }
}

// A warning on the screen blinks as its red numbers do (number()): lit 0.5 s of every 0.8.
[[nodiscard]] bool blinkOn()
{
    return za::fmod(realtime, 0.8) < 0.5;
}

// The top row with vr_gadget_stamina (docs/vr-port/ROUND21.md, "Stamina on the gadget; the glow"), in the screen's one
// colour, so by brightness: "STAMINA" and ten cells, lit for what's left (the one being filled lit in part), empty
// ones outlined. Low (one more one-handed parry knocks the weapon away): the lit cells blink. None left: "EXHAUSTED"
// blinks over the empty cells, and the screen's frame with it (layout). Coming back: a bright sweep runs along the empty cells.
// Hanging from a hold spends it (vr_climb_stamina): "HANGING", and a dark notch runs back through the lit ones. While a counter's window is open,
// the label is "COUNTER" lit in reverse and the rule under the row is a thick bar running out with the window; without
// parry stamina that is all it shows, over the title. False when it shows nothing (the title then).
bool meleeRow(const Palette& pal)
{
    const meleehud::State m = meleehud::state();
    if(!vr_gadget_stamina.value || (!m.stamina && m.counter <= 0.f))
    {
        return false;
    }

    constexpr float left = 8.f, right = width - 8.f;
    constexpr float cellsX = 72.f, cellW = 13.f, cellGap = 3.f, cellY = 5.f, cellH = 9.f;
    constexpr int cells = 10;

    // The label: STAMINA, or COUNTER in reverse while the window is open.
    if(m.counter > 0.f)
    {
        fill(left - 2.f, 4.f, 7.f * 8.f + 4.f, 11.f, glm::vec3{pal.text});
        gfx::draw2D::color(glm::vec4{pal.background, 1.f});
        gfx::draw2D::text(left, 6.f, 8.f, "COUNTER");
    }
    else
    {
        gfx::draw2D::color(pal.text);
        gfx::draw2D::text(left, 6.f, 8.f, m.draining ? "HANGING" : "STAMINA"); // (a hang drains it: vr_climb_stamina)
    }
    gfx::draw2D::color(white);

    if(m.stamina)
    {
        const float lit = m.left * cells; // cells' worth left
        const bool dim = m.low && !blinkOn();
        for(int i = 0; i < cells; i++)
        {
            const float x = cellsX + i * (cellW + cellGap);
            const float share = za::clamp(lit - static_cast<float>(i), 0.f, 1.f);
            // Its outline (an empty cell), then what's lit of it.
            fill(x, cellY, cellW, 1.f, pal.line);
            fill(x, cellY + cellH - 1.f, cellW, 1.f, pal.line);
            fill(x, cellY, 1.f, cellH, pal.line);
            fill(x + cellW - 1.f, cellY, 1.f, cellH, pal.line);
            if(share > 0.f)
            {
                const float w = za::max(1.f, za::round(cellW * share));
                fill(x, cellY, w, cellH, dim ? pal.line : glm::vec3{pal.text});
            }
        }
        const float cellsEnd = cellsX + cells * (cellW + cellGap) - cellGap;
        if(m.left <= 0.f)
        {
            if(blinkOn())
            {
                constexpr const char* word = "EXHAUSTED";
                const float x = za::round((cellsX + cellsEnd) * 0.5f - 9.f * 4.f);
                fill(x - 3.f, cellY - 1.f, 9.f * 8.f + 6.f, cellH + 2.f, pal.background);
                fill(x - 3.f, cellY - 1.f, 9.f * 8.f + 6.f, 1.f, pal.line);
                fill(x - 3.f, cellY + cellH, 9.f * 8.f + 6.f, 1.f, pal.line);
                gfx::draw2D::color(pal.text);
                gfx::draw2D::text(x, 6.f, 8.f, word);
                gfx::draw2D::color(white);
            }
        }
        else if(m.draining)
        {
            // The drain: a dark notch running back through the lit cells, towards the start, every 0.7 s.
            const float to = cellsX + lit * (cellW + cellGap);
            const float t = static_cast<float>(za::fmod(realtime, 0.7) / 0.7);
            const float x = to - (to - cellsX) * t;
            if(to - cellsX > 3.f)
            {
                fill(za::max(x - 2.f, cellsX), cellY + 1.f, 2.f, cellH - 2.f, pal.background);
            }
        }
        else if(m.recovering)
        {
            // The sweep: from what's lit to the end, every 0.7 s.
            const float from = cellsX + lit * (cellW + cellGap);
            const float t = static_cast<float>(za::fmod(realtime, 0.7) / 0.7);
            const float x = from + (cellsEnd - from) * t;
            if(cellsEnd - from > 3.f)
            {
                fill(za::min(x, cellsEnd - 2.f), cellY + 1.f, 2.f, cellH - 2.f, glm::vec3{pal.text});
            }
        }
    }

    // The rule under the row; while the window is open, a thick bar running out with it (from the right).
    fill(left, 16.f, right - left, 1.f, pal.line);
    if(m.counter > 0.f)
    {
        fill(left, 16.f, za::max(2.f, za::round((right - left) * m.counter)), 2.f, glm::vec3{pal.text});
    }
    return true;
}

void layout()
{
    const Palette pal = palette();

    // A tinted screen with faint scanlines (the CRT look has its own) and a frame.
    fill(0.f, 0.f, width, height, pal.background);
    for(int y = 0; y < height && crtStrength() <= 0.f; y += 3)
    {
        fill(0.f, static_cast<float>(y), width, 1.f, pal.scanline);
    }
    // Exhausted (no parry stamina left, vr_gadget_stamina): the frame blinks bright, to be seen out of the corner of the eye.
    const meleehud::State m = meleehud::state();
    const bool alarm = vr_gadget_stamina.value && m.stamina && m.left <= 0.f && blinkOn();
    const glm::vec3 frame = alarm ? glm::vec3{pal.text} : pal.line;
    fill(0.f, 0.f, width, 2.f, frame);
    fill(0.f, height - 2.f, width, 2.f, frame);
    fill(0.f, 0.f, 2.f, height, frame);
    fill(width - 2.f, 0.f, 2.f, height, frame);

    // The top row: the title, or parry stamina and the counter's window (vr_gadget_stamina).
    if(!meleeRow(pal))
    {
        gfx::draw2D::color(pal.text);
        gfx::draw2D::text(8.f, 6.f, 8.f, "RANGER STATUS");
        gfx::draw2D::color(white);
        fill(8.f, 16.f, width - 16.f, 1.f, pal.line);
    }

    // Face and health, armour.
    const int health = cl.stats[STAT_HEALTH];
    const int face = health >= 100 ? 4 : CLAMP(0, health / 20, 4);
    gfx::draw2D::pic(8.f, 22.f, facePic(face));
    number(36.f, 22.f, health, 3, health < 25, 1.f);

    const int armor = cl.stats[STAT_ARMOR];
    const int armorType = (cl.items & IT_ARMOR3) ? 2 : (cl.items & IT_ARMOR2) ? 1 : 0;
    if(armor > 0)
    {
        gfx::draw2D::pic(128.f, 22.f, armorPics[armorType]);
    }
    number(156.f, 22.f, armor, 3, false, 1.f);

    // Ammo: four columns, icon over count.
    const int ammo[4] = {cl.stats[STAT_SHELLS], cl.stats[STAT_NAILS], cl.stats[STAT_ROCKETS], cl.stats[STAT_CELLS]};
    for(int i = 0; i < 4; i++)
    {
        const float x = 14.f + i * 58.f;
        gfx::draw2D::pic(x, 56.f, ammoPics[i]);
        char count[8];
        q_snprintf(count, sizeof(count), "%3d", ammo[i]);
        gfx::draw2D::color(pal.text);
        gfx::draw2D::text(x - 3.f, 84.f, 10.f, count);
        gfx::draw2D::color(white);
    }

    // Keys, powerups and sigils in a row.
    float x = 12.f;
    const int keyBits[2] = {IT_KEY1, IT_KEY2};
    for(int i = 0; i < 2; i++)
    {
        if(cl.items & keyBits[i])
        {
            gfx::draw2D::pic(x, 100.f, keyPics[i]);
            x += 20.f;
        }
    }
    const int powerupBits[4] = {IT_INVISIBILITY, IT_INVULNERABILITY, IT_SUIT, IT_QUAD};
    for(int i = 0; i < 4; i++)
    {
        if(cl.items & powerupBits[i])
        {
            gfx::draw2D::pic(x, 100.f, powerupPics[i]);
            x += 20.f;
        }
    }
    for(int i = 0; i < 4; i++)
    {
        if(cl.items & (1 << (28 + i)))
        {
            gfx::draw2D::pic(x, 100.f, sigilPics[i]);
            x += 12.f;
        }
    }

    // The level, kills and secrets.
    if(!vr_gadget_show_level.value)
    {
        return;
    }
    fill(8.f, 122.f, width - 16.f, 1.f, pal.line);
    char line[64];
    q_snprintf(line, sizeof(line), "%.22s", cl.levelname);
    gfx::draw2D::color(pal.text);
    gfx::draw2D::text(8.f, 127.f, 8.f, line);
    q_snprintf(line, sizeof(line), "K %d/%d  S %d/%d", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS],
        cl.stats[STAT_SECRETS], cl.stats[STAT_TOTALSECRETS]);
    gfx::draw2D::text(8.f, 138.f, 8.f, line);
    gfx::draw2D::color(white);
}

// One of the screen's lights, `out` units in front of it, in its colour times `k`.
void light(int key, const Pose& pose, float out, float radius, float k)
{
    dlight_t* dl = CL_AllocDlight(key);
    const glm::vec3 p = pose.origin + pose.axes[2] * out;
    dl->origin[0] = p.x;
    dl->origin[1] = p.y;
    dl->origin[2] = p.z;
    dl->die = static_cast<float>(cl.time + 0.05);
    dl->radius = radius;
    const bool darkplaces = vr_dlight_falloff.value != 0.f;
    const glm::vec3 c = hue::color(vr_gadget_screen_hue, 0.5f, 1.f) * (k * (darkplaces ? 0.3f : 0.6f));
    dl->color[0] = c.r;
    dl->color[1] = c.g;
    dl->color[2] = c.b;
    if(darkplaces)
    {
        lighting::dlightLook(dl, 0.f, 0.f);
    }
    lighting::dlightNoShadow(dl);
}

// The screen's light, in its colour. There are no spot lights: the beam is a light well in front of
// the screen, reaching out about 1.5 m the way it faces and hardly behind it (so that a wall
// beyond the wrist stays dark while the screen faces the player), and a faint small one just in
// front of it lights the hand and forearm round it. Placed before each eye's scene, they last
// until the next frame's.
void glow(const Pose& pose)
{
    const float k = vr_gadget_light.value;
    const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f);
    if(!pose.valid || k <= 0.f || bright <= 0.f)
    {
        return;
    }
    light(beamLightKey, pose, 14.f, 36.f, bright * k * 1.1f);
    light(glowLightKey, pose, 1.5f * pose.scale, 16.f, bright * k * 0.35f);
}

// Lines of text kept from call to call (the frames' messages and log): clear() keeps the strings and their buffers,
// add() hands out the next one emptied, so that lines laid out again every frame allocate nothing once there.
class Lines
{
public:
    void clear() { count = 0; }

    [[nodiscard]] za::String& add()
    {
        if(count == strings.size())
        {
            strings.emplaceBack();
        }
        za::String& s = strings[count++];
        s.clear();
        return s;
    }

    void popBack() { count--; }
    [[nodiscard]] za::SizeT size() const { return count; }
    [[nodiscard]] bool empty() const { return count == 0; }
    [[nodiscard]] const za::String& operator[](za::SizeT i) const { return strings[i]; }
    [[nodiscard]] const za::String& back() const { return strings[count - 1]; }

    friend za::SizeT heldBytes(const Lines& l) { return mem::heldBytes(l.strings); } // (vr_mem.hpp)

private:
    za::Vector<za::String> strings;
    za::SizeT count{0};
};

// `text` broken into lines of at most `columns` characters, between words where it can be,
// appended to `out` (the console's coloured characters plain: the log is in the screen's colour).
void wrap(za::StringView text, Lines& out, int columns = logColumns)
{
    while(text.endsWith(' '))
    {
        text.removeSuffix(1);
    }
    while(!text.empty())
    {
        size_t cut = za::min(text.size(), static_cast<size_t>(columns));
        if(cut < text.size())
        {
            const size_t space = text.substrByPosLen(0, cut + 1).rfind(' ');
            if(space != za::StringView::nPos && space > 0)
            {
                cut = space;
            }
        }
        za::String& line = out.add();
        line = text.substrByPosLen(0, cut);
        for(char& c : line)
        {
            c = static_cast<char>(static_cast<unsigned char>(c) & 127);
        }
        text.removePrefix(cut);
        while(text.startsWith(' '))
        {
            text.removePrefix(1);
        }
    }
}

// Whether the log is where the notify lines go (and the gadget is there to show it).
[[nodiscard]] bool logShown()
{
    return vr_notify_wrist.value != 0.f && active() && current.valid;
}

// make_gadget.py's units (Quake units at vr_world_scale 1.25) in cm, for the offsets in cm.
constexpr float cmPerModelUnit = 3.048f;

// The screen's centre, on its face, and how much it faces `eye` (0 when it is edge on or turned
// away, 1 from 0.5 on: the log's and the hologram's fade).
[[nodiscard]] glm::vec3 screenCentre()
{
    return current.origin + current.axes[2] * (0.43f * current.scale);
}

// How far the gadget reaches over the screen's centre along the view's `up` (world units): its
// casing's highest corner (make_gadget.py: 3.8 x 2.6 x 0.7 model units round its origin), whichever
// way it is turned. The hologram and the log float over it.
[[nodiscard]] float gadgetTop(const glm::vec3& up)
{
    const glm::vec3 c = screenCentre();
    float top = 0.f;
    for(int i = 0; i < 8; i++)
    {
        const glm::vec3 m{(i & 1) ? 1.9f : -1.9f, (i & 2) ? 1.3f : -1.3f, (i & 4) ? 0.35f : -0.35f};
        top = za::max(top, glm::dot(current.origin + current.axes * (m * current.scale) - c, up));
    }
    return top;
}

[[nodiscard]] float facing(const glm::vec3& eye)
{
    const glm::vec3 toEye = eye - screenCentre();
    const float f = glm::dot(current.axes[2], toEye) / za::max(glm::length(toEye), 0.01f);
    return CLAMP(0.f, (f - 0.2f) / 0.3f, 1.f);
}

// ---------------------------------------------------------------------------------------------
// The hologram (vr_messages_hologram, vr_gadget.hpp).

// A console line, plain (the coloured characters' high bit off, trailing spaces trimmed): how old it
// is, and whether it is the game's (a server print that is not the engine's own reply, engineLine).
struct NotifyLine
{
    za::String text;
    double seconds{0.0};
    bool game{false};
};

[[nodiscard]] za::SizeT heldBytes(const NotifyLine& l) // (vr_mem.hpp)
{
    return mem::heldBytes(l.text);
}

// The hologram's and the wrist log's buffers, laid out again each frame (the main thread).
struct GadgetScratch
{
    Lines messageLines;                               // a message's lines (makeMessage)
    za::Vector<bool> keep;                           // the queued messages shown (the hologram's queue)
    za::String hologramKey;                          // the shown messages' text: its image's key
    za::String hologramLine;                         // a line drawn into the image
    za::Vector<bool> used;                           // the image's blocks drawn this frame
    Lines wrapped;                                    // the wrist log's lines (notifyLines: out.lines views them)
    za::Vector<qza::Pair<za::SizeT, float>> picked; // those shown: wrapped's line, its alpha
    NotifyLine notifyLine;                            // a console line read
    za::String plain;                                // a line to check (VR_GameLineOnWrist)
    auto members() { return qvr::mem::list(messageLines, keep, hologramKey, hologramLine, used, wrapped, picked, notifyLine, plain); }
};
mem::Scratch<GadgetScratch> scratch{"gadget"};

// Whether a server print's line is the engine's own reply rather than the game's (the progs'): the
// server's banner, a cheat's "godmode ON", setpos's and ping's figures, a server cvar changed, a pause.
[[nodiscard]] bool engineLine(za::StringView s)
{
    while(s.startsWith(' '))
    {
        s.removePrefix(1);
    }
    constexpr za::StringView prefixes[] = {"VERSION ", "usage:", "current values:", "Client ping times",
        "Can't suicide", "Pause not allowed", "Kicked by"};
    for(const za::StringView p : prefixes)
    {
        if(s.startsWith(p))
        {
            return true;
        }
    }
    if(s.find("\" changed to \"") != za::StringView::nPos || s.endsWith(" paused the game") ||
        s.endsWith(" unpaused the game"))
    {
        return true;
    }
    // "godmode ON", "noclip OFF": a word, then ON or OFF.
    const size_t space = s.find(' ');
    if(space != za::StringView::nPos && s.find(' ', space + 1) == za::StringView::nPos)
    {
        const za::StringView word = s.substrByPosLen(space + 1);
        if(word == "ON" || word == "OFF")
        {
            return true;
        }
    }
    // Figures only.
    return !za::anyOf(s.begin(), s.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); });
}

// The console's line `age` back from the newest, if it is a notify line (Con_NotifyLine).
[[nodiscard]] bool notifyLine(int age, NotifyLine& out)
{
    const char* text = nullptr;
    int length = 0;
    int server = 0;
    if(!Con_NotifyLine(age, &text, &length, &out.seconds, &server))
    {
        return false;
    }
    while(length > 0 && (text[length - 1] & 127) == ' ')
    {
        length--;
    }
    out.text.assign(text, static_cast<za::SizeT>(length));
    for(char& c : out.text)
    {
        c = static_cast<char>(static_cast<unsigned char>(c) & 127);
    }
    out.game = server && !engineLine(out.text);
    return true;
}

// Its image: the messages' lines in the console font, white on black, on a virtual screen of font
// pixels (holoTexels texels each, with mipmaps for the glow), each message a block of its own (its
// lines centred), oldest at the top; drawn again only when the messages change.
constexpr int holoWidth = 336; // 40 characters and the room round them
constexpr int holoHeight = 128;
constexpr int holoTexels = 4;
constexpr int holoPad = 4;      // font pixels round a block's text (its glow)
constexpr int holoColumns = 40; // a line's characters at most
constexpr int holoRows = 10;    // lines at most (the oldest messages go)
constexpr int holoMessagesMax = 6;
constexpr float holoFadeIn = 0.15f, holoFadeOut = 0.6f;
constexpr float holoOpen = 0.35f; // seconds it takes to project (grow out of the screen)
constexpr float holoGrow = 0.2f;  // and a new message to

struct HoloMessage
{
    za::String text; // its lines, joined by '\n'
    int columns{0}, rows{0};
    double start{0.0};    // realtime its life counts from: printed (or last repeated); held, when first seen
    double appeared{0.0}; // realtime it first was (held: first seen): its fade-in and growth
    double printed{0.0};  // a console line's print time, which it is taken by (0: a centre print)
    int generation{-1};   // the map's (worldGeneration): a new map's messages start afresh
    unsigned id{0};
    bool centre{false};
    bool notify{false}; // a notification (anything but a pickup): see vr_messages_hologram_only
    bool seen{false};   // shown to the player once (the hologram facing them, at least half faded in)
};

// A block of the image.
struct HoloBlock
{
    za::String text;
    int columns{0}, rows{0};
    float v0{0.f}, v1{0.f}; // its texture rows (v up: v0 its bottom)
};

za::Vector<HoloMessage> queue;        // the game's messages, oldest first
double newestLine = 0.0;               // the newest console line taken into it (its print time)
unsigned nextId = 1;
za::String latestCentre;              // the latest centre print's text (SCR_CenterPrint's)
za::Vector<HoloMessage> holoMessages; // this frame's, oldest first
int holoCollected = -1;                // the host frame they were collected in
gfx::Target holoTarget;
za::Vector<HoloBlock> holoDrawn; // what the image holds
za::String holoDrawnKey;

// Laid out once a frame (layoutHologram), for both eyes.
struct HoloFrame
{
    int frame{-1};
    za::Vector<gfx::Vertex> text, beam;
    glm::vec4 params{0.f};
    float top{0.f};            // world units its top is over the screen's centre (the view's up); 0: not shown
    bool centreShown{false};   // it shows the centre print
    double lastShown{-100.0};  // realtime it was last shown
    double opened{-100.0};     // realtime it last appeared: its projection's start
    float shown{0.f};          // how much the gadget is in view and faces the viewer (0..1), this frame
    int shownFrame{-100};      // the frame of that
};
HoloFrame holo;
float logLift = -1.f; // the log's lift, following the hologram's top smoothly (world units; -1 not yet)

[[nodiscard]] bool hologramOn()
{
    return vr_messages_hologram.value != 0.f && active();
}

[[nodiscard]] float hologramLife()
{
    return za::max(1.f, vr_messages_hologram_time.value);
}

[[nodiscard]] float hologramEffect()
{
    return CLAMP(0.f, vr_messages_hologram_effect.value, 2.f);
}

// vr_messages_hologram_only: the game's messages are the hologram's alone (never in front of the head), and a
// notification waits in it until seen, announced by a chime from the gadget and a buzz of its hand.
[[nodiscard]] bool hologramOnly()
{
    return vr_messages_hologram_only.value != 0.f && hologramOn();
}

// How long a notification waits to be seen at most (a stale one is no use).
constexpr double heldMax = 300.0;

[[nodiscard]] bool held(const HoloMessage& m)
{
    return m.notify && !m.seen && hologramOnly();
}

[[nodiscard]] bool alive(const HoloMessage& m)
{
    return m.generation == worldGeneration() &&
           (held(m) ? realtime - m.appeared < heldMax : realtime - m.start < static_cast<double>(hologramLife()));
}

// Whether the gadget was in view and facing the viewer last frame (the hologram's `shown`, at least half).
[[nodiscard]] bool gadgetInView()
{
    return holo.shownFrame >= host_framecount - 1 && holo.shown >= 0.5f;
}

// A pickup's print ("You got the Grenade Launcher", "You get 20 shells", "You receive 25 health", "You got armor"): the
// thing is in the hand already, so not a notification.
[[nodiscard]] bool pickupLine(za::StringView s)
{
    constexpr za::StringView prefixes[] = {"You got ", "You get ", "You receive "};
    return za::anyOf(prefixes, prefixes + za::getArraySize(prefixes), [&](za::StringView p) { return s.startsWith(p); });
}

// `text` as a message: plain, its lines wrapped to holoColumns, the empty ones at its ends left out.
void makeMessage(za::StringView text, HoloMessage& m)
{
    Lines& lines = scratch.messageLines;
    lines.clear();
    while(!text.empty())
    {
        const size_t end = za::min(text.find('\n'), text.size());
        const size_t before = lines.size();
        wrap(text.substrByPosLen(0, end), lines, holoColumns);
        if(lines.size() == before)
        {
            (void)lines.add(); // an empty line
        }
        text.removePrefix(za::min(end + 1, text.size()));
    }
    while(!lines.empty() && lines.back().empty())
    {
        lines.popBack();
    }
    size_t first = 0;
    while(first < lines.size() && lines[first].empty())
    {
        first++;
    }
    m.text.clear();
    m.columns = 0;
    m.rows = 0;
    for(size_t i = first; i < lines.size(); i++)
    {
        if(i > first)
        {
            m.text += '\n';
        }
        m.text += lines[i];
        m.columns = za::max(m.columns, static_cast<int>(lines[i].size()));
        m.rows++;
    }
}

// ---- The notification (vr_messages_hologram_only): a chime from the gadget and a buzz of its hand ----------------

// The chime is Quake's message sound (misc/talk.wav, the one the maps' messages come with), played at the gadget's
// screen on an entity number of its own and kept there as the arm moves (setPose): heard from the wrist.
constexpr int chimeEntity = -0x5C12;
double lastChime = -100.0;
double buzzAgain = -1.0; // realtime the buzz's second pulse is due (<0: none)

[[nodiscard]] int gadgetHand()
{
    return hands::gadgetHand();
}

void chime(sfx_t* sfx = nullptr)
{
    if(realtime - lastChime < 0.3 || !current.valid)
    {
        return; // one just played (the game's own, moved here, or ours)
    }
    sfx = sfx ? sfx : S_PrecacheSound("misc/talk.wav");
    if(!sfx)
    {
        return;
    }
    lastChime = realtime;
    Con_DPrintf("gadget: chime (%s) from the gadget\n", sfx->name);
    const glm::vec3 c = screenCentre();
    vec3_t org{c.x, c.y, c.z};
    S_StartSound(chimeEntity, 1, sfx, org, 1.f, 1.f);
}

// A watch's buzz: two short gentle pulses.
void buzz()
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value)
    {
        Con_DPrintf("gadget: buzz (%s hand)\n", gadgetHand() == HAND_MAIN ? "main" : "off");
        be->haptic(gadgetHand(), 0.05f, 160.f, 0.4f);
        buzzAgain = realtime + 0.13;
    }
}

// A message came: with vr_messages_hologram_only, a notification not in view chimes and buzzes.
void announce(const HoloMessage& m)
{
    if(!m.notify || !hologramOnly() || gadgetInView())
    {
        return;
    }
    Con_DPrintf("gadget: notification \"%s\"\n", m.text.cStr());
    chime();
    buzz();
}

// Adds a message to the queue: the same text again while it is up (a locked door touched again) keeps that one on
// instead (announced again if it had been seen and is out of view).
void addMessage(HoloMessage m)
{
    m.generation = worldGeneration();
    for(HoloMessage& e : queue)
    {
        if(e.text == m.text && alive(e))
        {
            if(held(e))
            {
                return; // still waiting to be seen
            }
            e.start = m.start;
            e.printed = za::max(e.printed, m.printed);
            if(hologramOnly() && e.notify && !gadgetInView())
            {
                e.seen = false; // not in view: waits again
                e.appeared = realtime;
                announce(e);
            }
            return;
        }
    }
    m.id = nextId++;
    announce(m);
    queue.pushBack(ZA_MOVE(m));
    if(queue.size() > 16)
    {
        queue.erase(queue.begin());
    }
}

// This frame's messages (once a frame): the game's new console lines taken into the queue as they are printed (a
// line pieced together from several prints, as a pickup's, taken whole within its first second), the queue's dead
// ones dropped; then the newest that fit, oldest first, beyond holoRows lines or holoMessagesMax messages left out
// (the ones waiting to be seen kept first).
void collectMessages()
{
    if(holoCollected == host_framecount)
    {
        return;
    }
    holoCollected = host_framecount;
    if(!hologramOn())
    {
        holoMessages.clear();
        queue.clear();
        return;
    }

    static NotifyLine line;          // (scratch kept between frames: its text's buffer)
    static HoloMessage continuation; // (likewise)
    for(int age = 0; age < 16; age++)
    {
        if(!notifyLine(age, line))
        {
            continue;
        }
        if(line.seconds >= 1.0)
        {
            break; // older lines are older still (taken already)
        }
        if(!line.game || line.text.empty())
        {
            continue;
        }
        const double printed = realtime - line.seconds;
        auto it = za::findIf(queue.begin(), queue.end(),
            [&](const HoloMessage& m) { return m.printed > 0.0 && qza::abs(m.printed - printed) < 1e-4; });
        if(it != queue.end())
        {
            makeMessage(line.text, continuation);
            if(continuation.rows > 0 && continuation.text != it->text)
            {
                it->text = continuation.text; // continued since
                it->columns = continuation.columns;
                it->rows = continuation.rows;
            }
            continue;
        }
        if(printed <= newestLine + 1e-4)
        {
            continue; // taken before, gone since
        }
        newestLine = printed;
        HoloMessage m;
        makeMessage(line.text, m);
        if(m.rows == 0)
        {
            continue;
        }
        m.start = m.appeared = m.printed = printed;
        m.notify = !pickupLine(m.text);
        addMessage(ZA_MOVE(m));
    }
    za::vectorEraseIf(queue, [](const HoloMessage& m) { return !alive(m); });

    // The newest that fit: the waiting ones first, then the rest; one too long alone, its first lines.
    za::Vector<bool>& keep = scratch.keep;
    keep.clear();
    keep.resize(queue.size(), false);
    int rows = 0, count = 0;
    for(const bool waiting : {true, false})
    {
        for(size_t i = queue.size(); i-- > 0;)
        {
            const HoloMessage& m = queue[i];
            const int r = za::min(m.rows, holoRows);
            if(keep[i] || held(m) != waiting || count >= holoMessagesMax || rows + r > holoRows)
            {
                continue;
            }
            keep[i] = true;
            rows += r;
            count++;
        }
    }
    // (Copied into the elements holoMessages already has: their strings' buffers are reused.)
    za::SizeT shown = 0;
    for(size_t i = 0; i < queue.size(); i++)
    {
        if(!keep[i])
        {
            continue;
        }
        if(shown == holoMessages.size())
        {
            holoMessages.emplaceBack();
        }
        HoloMessage& m = holoMessages[shown++];
        m = queue[i];
        if(m.rows > holoRows)
        {
            int lines = 1;
            for(size_t c = 0; c < m.text.size(); c++)
            {
                if(m.text[c] == '\n' && ++lines > holoRows)
                {
                    m.text.resize(c);
                    break;
                }
            }
            m.rows = holoRows;
        }
    }
    holoMessages.resize(shown);
    za::insertionSort(holoMessages.begin(), holoMessages.end(),
        [](const HoloMessage& a, const HoloMessage& b) { return a.start < b.start; });
}

// Marks this frame's messages seen, the hologram facing the viewer (layoutHologram): a waiting one's life starts now,
// and it grows in as if it had just come.
void markSeen()
{
    for(HoloMessage& shown : holoMessages)
    {
        auto it = za::findIf(queue.begin(), queue.end(), [&](const HoloMessage& m) { return m.id == shown.id; });
        if(it == queue.end() || it->seen)
        {
            continue;
        }
        if(held(*it))
        {
            it->start = it->appeared = realtime;
            shown.start = shown.appeared = realtime;
        }
        it->seen = shown.seen = true;
    }
}

// The image, if the messages changed (end of the 2D pass).
void renderHologram()
{
    collectMessages();
    if(holoMessages.empty())
    {
        return; // the old image holds (its blocks are matched by their text)
    }
    za::String& key = scratch.hologramKey;
    key.clear();
    for(const HoloMessage& m : holoMessages)
    {
        key += m.text;
        key += '\x1f';
    }
    if(key == holoDrawnKey && holoTarget.texture)
    {
        return;
    }
    holoDrawnKey = key;

    QVR_GPU_PROFILE("gadget hologram");
    gfx::ensureTarget(holoTarget, holoWidth * holoTexels, holoHeight * holoTexels, true, "gadget hologram");
    gfx::begin2D(holoTarget, holoWidth, holoHeight);
    fill(0.f, 0.f, holoWidth, holoHeight, glm::vec3{0.f});
    gfx::draw2D::color(white);
    holoDrawn.clear();
    int y = 0;
    for(const HoloMessage& m : holoMessages)
    {
        const int h = m.rows * 8 + holoPad * 2;
        if(y + h > holoHeight)
        {
            break;
        }
        za::StringView text = m.text;
        for(int row = 0; row < m.rows; row++)
        {
            const size_t end = za::min(text.find('\n'), text.size());
            za::String& line = scratch.hologramLine; // (a c_str for draw2D::text)
            line = text.substrByPosLen(0, end);
            const float x = static_cast<float>(holoWidth - static_cast<int>(line.size()) * 8) * 0.5f;
            gfx::draw2D::text(x, static_cast<float>(y + holoPad + row * 8), 8.f, line.cStr());
            text.removePrefix(za::min(end + 1, text.size()));
        }
        holoDrawn.pushBack({m.text, m.columns, m.rows, 1.f - static_cast<float>(y + h) / holoHeight,
            1.f - static_cast<float>(y) / holoHeight});
        y += h;
    }
    gfx::end2D();
}

// 0..1 eased out (cubic).
[[nodiscard]] float easeOut(float t)
{
    t = CLAMP(0.f, t, 1.f);
    return 1.f - (1.f - t) * (1.f - t) * (1.f - t);
}

// The hologram's triangles this frame (once, facing the first view drawn): each message's block a
// quad facing the viewer, the newest at the bottom, `vr_messages_hologram_height` over the gadget as
// seen (the view's up); the beam from the screen up to them.
void layoutHologram()
{
    if(holo.frame == host_framecount)
    {
        return;
    }
    holo.frame = host_framecount;
    holo.text.clear();
    holo.beam.clear();
    holo.top = 0.f;
    holo.centreShown = false;
    holo.shown = 0.f;
    holo.shownFrame = host_framecount;
    collectMessages();
    if(!current.valid)
    {
        return;
    }

    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const float scale = current.scale;
    const glm::vec3 c = screenCentre();
    const float lift = gadgetTop(up) + za::max(0.f, vr_messages_hologram_height.value) / cmPerModelUnit * scale;
    const glm::vec3 base = c + up * lift;

    // While the screen faces the viewer, and it is in view (with or without messages: gadgetInView).
    float shown = facing(eye);
    const glm::vec4 clip = gfx::sceneViewProjection() * glm::vec4{base, 1.f};
    if(clip.w <= 0.f || qza::abs(clip.x) > clip.w * 1.1f || qza::abs(clip.y) > clip.w * 1.1f)
    {
        shown = 0.f;
    }
    holo.shown = shown;
    if(shown <= 0.f || holoMessages.empty() || !holoTarget.texture || holoDrawn.empty())
    {
        return;
    }
    if(shown >= 0.5f)
    {
        markSeen();
    }
    if(realtime - holo.lastShown > 0.4)
    {
        holo.opened = realtime; // projected again
    }
    holo.lastShown = realtime;

    const float k = hologramEffect();
    const float open = k > 0.f ? easeOut(static_cast<float>(realtime - holo.opened) / holoOpen) : 1.f;
    const float px = 0.26f * CLAMP(0.25f, vr_messages_hologram_size.value, 4.f) * scale / 8.f; // a font pixel
    const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f);
    const glm::vec3 rgb = hue::color(vr_gadget_screen_hue, 0.5f, 0.95f * za::max(bright, 0.3f));
    const double life = hologramLife();

    // The blocks, newest first from the bottom up, each growing as it appears.
    za::Vector<bool>& used = scratch.used;
    used.clear();
    used.resize(holoDrawn.size(), false);
    float y = 0.f, widest = 0.f, strongest = 0.f;
    double newest = -100.0;
    for(auto it = qza::rbegin(holoMessages); it != qza::rend(holoMessages); ++it)
    {
        const HoloMessage& m = *it;
        size_t b = 0;
        while(b < holoDrawn.size() && (used[b] || holoDrawn[b].text != m.text))
        {
            b++;
        }
        if(b == holoDrawn.size())
        {
            continue; // not in the image yet (next frame)
        }
        used[b] = true;
        const HoloBlock& block = holoDrawn[b];

        const float age = static_cast<float>(realtime - m.appeared);
        const float fade = CLAMP(0.f, age / holoFadeIn, 1.f) *
                           (held(m) ? 1.f : CLAMP(0.f, static_cast<float>(m.start + life - realtime) / holoFadeOut, 1.f));
        if(fade <= 0.f)
        {
            continue;
        }
        const float grow = k > 0.f ? easeOut(age / holoGrow) : 1.f;
        const float alpha = fade * shown * (0.4f + 0.6f * open);
        newest = za::max(newest, m.appeared);
        strongest = za::max(strongest, alpha);
        if(m.centre && m.text == latestCentre)
        {
            holo.centreShown = shown >= 0.5f;
        }

        const float w = static_cast<float>(block.columns * 8 + holoPad * 2) * px * (0.3f + 0.7f * open) * (0.6f + 0.4f * grow);
        const float h = static_cast<float>(block.rows * 8 + holoPad * 2) * px * open * grow;
        const float du = static_cast<float>(block.columns * 8 + holoPad * 2) * 0.5f / holoWidth;
        const glm::vec3 bl = base - right * (w * 0.5f) + up * y;
        const glm::vec3 br = bl + right * w;
        const glm::vec3 tr = br + up * h;
        const glm::vec3 tl = bl + up * h;
        const glm::vec4 color{rgb, alpha};
        const gfx::Vertex v[4] = {{bl, {0.5f - du, block.v0}, color}, {br, {0.5f + du, block.v0}, color},
            {tr, {0.5f + du, block.v1}, color}, {tl, {0.5f - du, block.v1}, color}};
        for(const gfx::Vertex* p : {&v[0], &v[1], &v[2], &v[0], &v[2], &v[3]})
        {
            holo.text.pushBack(*p);
        }
        y += h;
        widest = za::max(widest, w);
    }
    if(holo.text.empty())
    {
        return;
    }
    holo.top = lift + y;

    // Glitches: now and then, as a message comes, and as it is projected.
    const float since = static_cast<float>(realtime - newest);
    float g = glitch(realtime + 4.1) * 0.7f;
    if(since < 0.35f)
    {
        g = za::max(g, 0.8f * za::sin(since / 0.35f * 3.14159265f));
    }
    g = za::max(g, 0.8f * (1.f - open));
    const float time = static_cast<float>(za::fmod(realtime, 1000.0));
    holo.params = {time, k, g * za::min(k, 1.f), 0.f};

    // The beam: from the screen (a little inside its edge) to the blocks' outline, its corners
    // matched by where they are in the view (whichever way the gadget is turned).
    const float beam = 0.3f * za::min(k, 1.f) * strongest * (1.f + 1.5f * (1.f - open));
    if(beam <= 0.f)
    {
        return;
    }
    glm::vec3 corner;
    glm::vec2 size;
    screenRect(corner, size);
    za::Array<glm::vec3, 4> s;
    za::Array<float, 4> angle;
    for(int i = 0; i < 4; i++)
    {
        const glm::vec3 m{((i == 1 || i == 2) ? 0.5f : -0.5f) * size.x * 0.85f, (i >= 2 ? 0.5f : -0.5f) * size.y * 0.85f,
            corner.z + 0.02f};
        s[static_cast<size_t>(i)] = current.origin + current.axes * (m * scale);
        const glm::vec3 d = s[static_cast<size_t>(i)] - c;
        angle[static_cast<size_t>(i)] = za::atan2(glm::dot(d, up), glm::dot(d, right));
    }
    // Round the screen from the corner nearest the view's lower left (-135 degrees), as the blocks' outline.
    za::Array<int, 4> order{0, 1, 2, 3};
    za::quickSort(order.begin(), order.end(), [&](int a, int b) { return angle[static_cast<size_t>(a)] < angle[static_cast<size_t>(b)]; });
    int first = 0;
    float nearest = 10.f;
    for(int i = 0; i < 4; i++)
    {
        float d = qza::abs(angle[static_cast<size_t>(order[static_cast<size_t>(i)])] + 2.3561945f);
        d = za::min(d, 6.2831853f - d);
        if(d < nearest)
        {
            nearest = d;
            first = i;
        }
    }
    const glm::vec3 p[4] = {base - right * (widest * 0.5f), base + right * (widest * 0.5f),
        base + right * (widest * 0.5f) + up * y, base - right * (widest * 0.5f) + up * y};
    const glm::vec4 color{rgb, beam};
    for(int i = 0; i < 4; i++)
    {
        const int j = (i + 1) % 4;
        const glm::vec3& sa = s[static_cast<size_t>(order[static_cast<size_t>((first + i) % 4)])];
        const glm::vec3& sb = s[static_cast<size_t>(order[static_cast<size_t>((first + j) % 4)])];
        const gfx::Vertex a{sa, {-1.f, 0.f}, color}, b{sb, {1.f, 0.f}, color}, pb{p[j], {1.f, 1.f}, color},
            pa{p[i], {-1.f, 1.f}, color};
        for(const gfx::Vertex* q : {&a, &b, &pb, &a, &pb, &pa})
        {
            holo.beam.pushBack(*q);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// The FPS counter (vr_gadget_fps), floating just under the gadget as seen, facing the viewer as the hologram over it
// does, in the screen's colour (the hologram's shade with a little of its look: faint scanlines, no glitches, so that
// the figures read steadily), characters about 5 mm; the words dimmer than the figures. Drawn into a small image when
// it changes.
//
// 1, Basic: " 90 FPS CPU  5.2 GPU  8.1", the frames' over the last half second (qvr::frameRate: the memory log's
// busy_ms and gpu_eyes_ms), drawn again when the figures change (twice a second).
//
// 2, Detailed (as fpsVR's), from the frames one by one (profile::frameSample, the same phases):
//
//      90 FPS   90 HZ   0 LATE       the rate over the last quarter second; the headset's refresh; the frames in the
//   MS   NOW  AVG  MIN  MAX          last 5 s that missed a refresh (their period over 1.25 refreshes: the runtime
//   CPU  5.2  5.1  4.8  7.9          showed the previous frame again, reprojected)
//   GPU  8.1  8.0  7.6 12.3          the last frame; the last quarter second's average; the last 5 s's least and most
//   CPU __.___|___                    the last 3 s, each column the worst frame over it, up to twice the budget (a frame
//   GPU ___.______                    period); the dotted line is the budget, and what is over it is brighter
//
// Figures over the budget are on a lit block. The figures change four times a second, the graphs twenty.

constexpr int fpsTexels = 4;
constexpr float fpsCharSize = 0.17f; // model units a character is tall (about 5 mm at the default size)
constexpr float fpsGap = 0.25f;      // model units under the gadget as seen

// Basic.
constexpr int fpsColumns = 25;
constexpr int fpsBasicWidth = fpsColumns * 8 + holoPad * 2; // font pixels
constexpr int fpsBasicHeight = 8 + holoPad * 2;

// Detailed.
constexpr int fpsDetailColumns = 26;
constexpr int fpsRowPitch = 10;       // font pixels a text row takes
constexpr int fpsTextRows = 4;
constexpr int fpsStrip = 16;          // a graph's height
constexpr int fpsStripGap = 3;
constexpr int fpsLabel = 3 * 8 + 4;   // a graph's label and the space after it
constexpr int fpsDetailWidth = fpsDetailColumns * 8 + holoPad * 2;
constexpr int fpsGraphWidth = fpsDetailColumns * 8 - fpsLabel;
constexpr int fpsDetailHeight = holoPad * 2 + fpsTextRows * fpsRowPitch + 2 + fpsStrip * 2 + fpsStripGap;
constexpr double fpsAverageTime = 0.25; // seconds: the rate, AVG
constexpr double fpsRangeTime = 5.0;    // MIN, MAX, LATE
constexpr double fpsGraphTime = 3.0;
constexpr double fpsFiguresEvery = 0.25;
constexpr double fpsGraphEvery = 0.05;
constexpr float fpsLateOver = 1.25f;    // periods over this many refreshes missed one (as the memory log's slow frames)
constexpr float fpsAssumedHz = 90.f;    // the budget when the runtime doesn't tell its refresh (the mock)

gfx::Target fpsTarget;
za::String fpsDrawn; // Basic: the text the image holds
int fpsDrawnMode = 0; // the mode the image is of (0: none)
int fpsWidth = fpsBasicWidth, fpsHeight = fpsBasicHeight; // the image's, in font pixels

struct FpsFrame
{
    int frame{-1};
    za::Vector<gfx::Vertex> quad;
    float time{0.f};
};
FpsFrame fps;

[[nodiscard]] int fpsMode()
{
    return static_cast<int>(CLAMP(0.f, vr_gadget_fps.value, 2.f));
}

[[nodiscard]] bool fpsOn()
{
    return fpsMode() != 0;
}

// Detailed: one series' figures (milliseconds; <0 unknown).
struct FpsSeries
{
    float now{-1.f}, avg{-1.f}, min{-1.f}, max{-1.f};
};

struct FpsFigures
{
    float fps{-1.f};
    float hz{0.f};     // the runtime's refresh (0: not told)
    float budget{0.f}; // ms: a refresh period
    int late{0};
    FpsSeries cpu, gpu;
};

FpsFigures fpsFigures;
double fpsFiguresAt = -1.0, fpsGraphAt = -1.0; // realtime each was last made

// The frames kept (profile::frameSample), for the figures.
void measureFps(FpsFigures& f)
{
    f = FpsFigures{};
    const double period = profile::displayPeriodMs();
    f.hz = period > 0.0 ? static_cast<float>(1000.0 / period) : 0.f;
    f.budget = period > 0.0 ? static_cast<float>(period) : 1000.f / fpsAssumedHz;

    profile::FrameSample newest;
    if(!profile::frameSample(0, newest))
    {
        return;
    }
    struct Acc
    {
        double sum{0.0};
        int n{0};
        float min{1e9f}, max{-1.f}, now{-1.f};
        void add(float v, bool recent)
        {
            if(v < 0.f)
            {
                return;
            }
            now = now < 0.f ? v : now;
            min = za::min(min, v);
            max = za::max(max, v);
            if(recent)
            {
                sum += v;
                n++;
            }
        }
        [[nodiscard]] FpsSeries series() const
        {
            return n > 0 ? FpsSeries{now, static_cast<float>(sum / n), min, max} : FpsSeries{};
        }
    };
    Acc cpu, gpu;
    double periods = 0.0;
    int frames = 0;
    profile::FrameSample fs;
    for(int back = 0; profile::frameSample(back, fs) && newest.time - fs.time <= fpsRangeTime; back++)
    {
        const bool recent = newest.time - fs.time < fpsAverageTime;
        cpu.add(fs.cpuMs, recent);
        gpu.add(fs.gpuMs, recent || gpu.n == 0); // (the newest few aren't read back yet)
        if(recent)
        {
            periods += fs.periodMs;
            frames++;
        }
        f.late += fs.periodMs > fpsLateOver * f.budget ? 1 : 0;
    }
    f.fps = periods > 0.0 ? static_cast<float>(1000.0 * frames / periods) : -1.f;
    f.cpu = cpu.series();
    f.gpu = gpu.series();
}

// A figure in five characters (" 12.3", " 123", "    -").
void formatMs(char (&out)[8], float ms)
{
    if(ms < 0.f)
    {
        q_strlcpy(out, "    -", sizeof(out));
    }
    else if(ms < 99.95f)
    {
        q_snprintf(out, sizeof(out), "%5.1f", ms);
    }
    else
    {
        q_snprintf(out, sizeof(out), "%5d", za::min(static_cast<int>(za::lround(ms)), 9999));
    }
}

// Detailed: the image (figures and graphs).
void renderFpsDetailed()
{
    const double now = realtime;
    const bool figures = fpsFiguresAt < 0.0 || now < fpsFiguresAt || now - fpsFiguresAt >= fpsFiguresEvery;
    const bool graph = figures || now < fpsGraphAt || now - fpsGraphAt >= fpsGraphEvery;
    if(fpsDrawnMode == 2 && fpsTarget.texture && !graph)
    {
        return;
    }
    if(figures)
    {
        measureFps(fpsFigures);
        fpsFiguresAt = now;
    }
    fpsGraphAt = now;
    fpsDrawnMode = 2;
    fpsWidth = fpsDetailWidth;
    fpsHeight = fpsDetailHeight;
    const FpsFigures& f = fpsFigures;

    QVR_GPU_PROFILE("gadget fps");
    gfx::ensureTarget(fpsTarget, fpsWidth * fpsTexels, fpsHeight * fpsTexels, true, "gadget fps");
    gfx::begin2D(fpsTarget, fpsWidth, fpsHeight);
    fill(0.f, 0.f, static_cast<float>(fpsWidth), static_cast<float>(fpsHeight), glm::vec3{0.f});
    const glm::vec4 word{0.6f, 0.6f, 0.6f, 1.f};
    const glm::vec3 dim{0.45f};
    const float x0 = static_cast<float>(holoPad);
    float x = x0, y = static_cast<float>(holoPad);
    const auto part = [&](const char* s, const glm::vec4& c, bool over = false) {
        const float w = 8.f * static_cast<float>(ZA_STRLEN(s));
        if(over)
        {
            // The figure (not its leading spaces) on a lit block (dim: the glow round it stays soft).
            const char* t = s;
            while(*t == ' ')
            {
                t++;
            }
            const float lead = 8.f * static_cast<float>(t - s);
            fill(x + lead - 1.f, y - 1.f, w - lead + 2.f, 10.f, glm::vec3{0.3f});
        }
        gfx::draw2D::color(c);
        gfx::draw2D::text(x, y, 8.f, s);
        x += w;
    };
    const auto newRow = [&] {
        x = x0;
        y += static_cast<float>(fpsRowPitch);
    };

    char a[8], b[8], c[8];
    q_snprintf(a, sizeof(a), f.fps >= 0.f ? "%3d" : "  -", CLAMP(0, static_cast<int>(za::lround(f.fps)), 999));
    q_snprintf(b, sizeof(b), f.hz > 0.f ? "%5d" : "    -", CLAMP(0, static_cast<int>(za::lround(f.hz)), 999));
    q_snprintf(c, sizeof(c), "%4d", za::min(f.late, 9999));
    part(a, white);
    part(" FPS", word);
    part(b, white);
    part(" HZ", word);
    part(c, white, f.late > 0);
    part(" LATE", word);
    newRow();
    part(" MS  NOW  AVG  MIN  MAX", word);
    for(const auto& [label, series] : {qza::Pair{"CPU", &f.cpu}, qza::Pair{"GPU", &f.gpu}})
    {
        newRow();
        part(label, word);
        for(const float v : {series->now, series->avg, series->min, series->max})
        {
            char t[8];
            formatMs(t, v);
            part(t, white, v > f.budget);
        }
    }

    // The graphs: the last fpsGraphTime seconds, newest at the right; a column the worst frame over it.
    const float top = static_cast<float>(holoPad + fpsTextRows * fpsRowPitch + 2);
    float cpuCol[fpsGraphWidth], gpuCol[fpsGraphWidth];
    za::fill(cpuCol, cpuCol + za::getArraySize(cpuCol), -1.f);
    za::fill(gpuCol, gpuCol + za::getArraySize(gpuCol), -1.f);
    profile::FrameSample newest, fs;
    if(profile::frameSample(0, newest))
    {
        const double column = fpsGraphTime / fpsGraphWidth;
        for(int back = 0; profile::frameSample(back, fs); back++)
        {
            // Over the columns its period spans (a hitch is as wide as it lasted).
            const double end = newest.time + newest.periodMs * 1e-3;
            const int first = fpsGraphWidth - 1 - static_cast<int>((end - fs.time) / column);
            const int last = fpsGraphWidth - 1 - static_cast<int>((end - fs.time - fs.periodMs * 1e-3) / column);
            if(last < 0)
            {
                break;
            }
            for(int col = za::max(first, 0); col <= za::min(last, fpsGraphWidth - 1); col++)
            {
                cpuCol[col] = za::max(cpuCol[col], fs.cpuMs);
                gpuCol[col] = za::max(gpuCol[col], fs.gpuMs);
            }
        }
    }
    const float gx = x0 + static_cast<float>(fpsLabel);
    for(int strip = 0; strip < 2; strip++)
    {
        const float sy = top + static_cast<float>(strip * (fpsStrip + fpsStripGap));
        x = x0;
        y = sy + static_cast<float>(fpsStrip - 8) * 0.5f;
        part(strip == 0 ? "CPU" : "GPU", word);
        const float* v = strip == 0 ? cpuCol : gpuCol;
        const float half = static_cast<float>(fpsStrip) * 0.5f; // the budget's height
        const float bottom = sy + static_cast<float>(fpsStrip);
        for(int i = 0; i < fpsGraphWidth; i++)
        {
            if(v[i] < 0.f)
            {
                continue;
            }
            const float h = za::max(1.f, za::min(v[i] / f.budget, 2.f) * half);
            const float low = za::min(h, half);
            fill(gx + static_cast<float>(i), bottom - low, 1.f, low, dim);
            if(h > half)
            {
                // Over the budget: brighter, its top lit (a solid lit block would glow too much in the bloom).
                fill(gx + static_cast<float>(i), bottom - h, 1.f, h - half, glm::vec3{0.65f});
                fill(gx + static_cast<float>(i), bottom - h, 1.f, 1.f, glm::vec3{1.f});
            }
        }
        for(int i = 0; i < fpsGraphWidth; i += 4) // the budget, dotted
        {
            fill(gx + static_cast<float>(i), bottom - half - 0.5f, 2.f, 1.f, glm::vec3{0.8f});
        }
        fill(gx, bottom, static_cast<float>(fpsGraphWidth), 0.5f, dim); // the baseline
    }
    gfx::draw2D::color(white);
    gfx::end2D();
}

// The image, when it changes (end of the 2D pass).
void renderFps()
{
    const int mode = fpsMode();
    if(mode == 0)
    {
        fpsDrawn.clear();
        fpsDrawnMode = 0;
        return;
    }
    if(mode == 2)
    {
        fpsDrawn.clear();
        renderFpsDetailed();
        return;
    }
    FrameRate r;
    const bool known = frameRate(r);
    char figures[3][8];
    q_snprintf(figures[0], sizeof(figures[0]), known ? "%3d" : "  -", CLAMP(0, static_cast<int>(za::lround(r.fps)), 999));
    q_snprintf(figures[1], sizeof(figures[1]), known ? "%5.1f" : "    -", za::min(r.cpuMs, 999.9f));
    q_snprintf(figures[2], sizeof(figures[2]), known && r.gpuMs >= 0.f ? "%5.1f" : "    -", za::min(r.gpuMs, 999.9f));
    const za::String text = za::String{figures[0]} + figures[1] + figures[2];
    if(text == fpsDrawn && fpsDrawnMode == 1 && fpsTarget.texture)
    {
        return;
    }
    fpsDrawn = text;
    fpsDrawnMode = 1;
    fpsWidth = fpsBasicWidth;
    fpsHeight = fpsBasicHeight;

    QVR_GPU_PROFILE("gadget fps");
    gfx::ensureTarget(fpsTarget, fpsWidth * fpsTexels, fpsHeight * fpsTexels, true, "gadget fps");
    gfx::begin2D(fpsTarget, fpsWidth, fpsHeight);
    fill(0.f, 0.f, static_cast<float>(fpsWidth), static_cast<float>(fpsHeight), glm::vec3{0.f});
    const glm::vec4 word{0.6f, 0.6f, 0.6f, 1.f};
    float x = static_cast<float>(holoPad);
    const auto part = [&](const char* s, const glm::vec4& c) {
        gfx::draw2D::color(c);
        gfx::draw2D::text(x, static_cast<float>(holoPad), 8.f, s);
        x += 8.f * static_cast<float>(ZA_STRLEN(s));
    };
    part(figures[0], white);
    part(" FPS CPU", word);
    part(figures[1], white);
    part(" GPU", word);
    part(figures[2], white);
    gfx::draw2D::color(white);
    gfx::end2D();
}

// Its quad this frame (once, facing the first view drawn): centred under the gadget as seen, while the screen faces
// the viewer (as the hologram and the log).
void layoutFps()
{
    if(fps.frame == host_framecount)
    {
        return;
    }
    fps.frame = host_framecount;
    fps.quad.clear();
    if(!fpsOn() || !current.valid || !fpsTarget.texture || fpsDrawnMode != fpsMode())
    {
        return;
    }
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const float shown = facing(eye);
    if(shown <= 0.f)
    {
        return;
    }
    const float scale = current.scale;
    const glm::vec3 top = screenCentre() - up * (gadgetTop(-up) + fpsGap * scale);
    const float px = fpsCharSize * scale / 8.f; // a font pixel
    const float w = static_cast<float>(fpsWidth) * px;
    const float h = static_cast<float>(fpsHeight) * px;
    const glm::vec3 tl = top - right * (w * 0.5f);
    const glm::vec3 tr = tl + right * w;
    const glm::vec3 br = tr - up * h;
    const glm::vec3 bl = tl - up * h;
    const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f);
    const glm::vec4 color{hue::color(vr_gadget_screen_hue, 0.5f, 0.95f * za::max(bright, 0.3f)), shown};
    const gfx::Vertex v[4] = {{bl, {0.f, 0.f}, color}, {br, {1.f, 0.f}, color}, {tr, {1.f, 1.f}, color}, {tl, {0.f, 1.f}, color}};
    for(const gfx::Vertex* p : {&v[0], &v[1], &v[2], &v[0], &v[2], &v[3]})
    {
        fps.quad.pushBack(*p);
    }
    fps.time = static_cast<float>(za::fmod(realtime, 1000.0));
}

void drawFps()
{
    layoutFps();
    if(fps.quad.empty())
    {
        return;
    }
    QVR_GPU_PROFILE("gadget fps");
    // Not depth tested: over the other hand, as the hologram.
    gfx::draw(fps.quad, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Hologram, .blend = gfx::Blend::Premultiplied, .depthTest = false, .depthWrite = false,
            .params = {fps.time, 0.35f * za::min(hologramEffect(), 1.f), 0.f, 0.f}, .screen = {static_cast<float>(fpsWidth), static_cast<float>(fpsHeight), 0.5f}},
        fpsTarget.texture);
}

void messageTest_f();
void gadgetInfo_f();
void screenDump_f();

// A centre print as a message (VR_GameCenterPrint, the test).
void centrePrint(za::StringView text)
{
    HoloMessage m;
    makeMessage(text, m);
    if(m.rows == 0)
    {
        for(auto it = qza::rbegin(queue); it != qza::rend(queue); ++it)
        {
            if(it->centre)
            {
                if(!held(*it))
                {
                    queue.erase(&*it);
                }
                break;
            }
        }
        latestCentre.clear();
        return;
    }
    m.start = m.appeared = realtime;
    m.centre = true;
    m.notify = true;
    latestCentre = m.text;
    addMessage(ZA_MOVE(m));
}

} // namespace

// Once in each 7 seconds, at a random moment of them, a burst of 0.12 to 0.35 seconds rising and
// falling; one in four slots has none.
float glitch(double time)
{
    constexpr double slot = 7.0;
    const double index = za::floor(time / slot);
    const uint32_t n = static_cast<uint32_t>(static_cast<int64_t>(index)) * 3u;
    if(hash(n + 2u) < 0.25f)
    {
        return 0.f;
    }
    const double start = index * slot + hash(n) * (slot - 0.5);
    const double length = 0.12 + 0.23 * hash(n + 1u);
    const double x = (time - start) / length;
    if(x <= 0.0 || x >= 1.0)
    {
        return 0.f;
    }
    return static_cast<float>(za::sin(x * 3.14159265)) * (0.6f + 0.4f * hash(n + 2u));
}

bool active()
{
    return vr_hud_mode.value == 1.f && vrActive() && cls.state == ca_connected && cls.signon == SIGNONS &&
           !cl.intermission;
}

void setPose(const Pose& pose)
{
    current = pose;
    glow(pose);

    // The chime stays at the gadget as the arm moves; the buzz's second pulse.
    if(realtime - lastChime < 3.0 && pose.valid)
    {
        const glm::vec3 c = screenCentre();
        for(int i = NUM_AMBIENTS; i < total_channels && i < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; i++)
        {
            channel_t& ch = snd_channels[i];
            if(ch.sfx && ch.entnum == chimeEntity)
            {
                ch.origin[0] = c.x;
                ch.origin[1] = c.y;
                ch.origin[2] = c.z;
            }
        }
    }
    if(buzzAgain >= 0.0 && realtime >= buzzAgain)
    {
        buzzAgain = -1.0;
        if(Backend* be = backend(); be && !vr_disablehaptics.value)
        {
            be->haptic(gadgetHand(), 0.05f, 160.f, 0.4f);
        }
    }
}

bool testMessage()
{
    if(!hologramOn())
    {
        return false;
    }
    // The game's own: a key needed, maps' texts (e1m1, e4m4: two lines), a secret, a powerup running out (a print).
    struct Test
    {
        const char* text;
        bool centre;
    };
    static constexpr Test tests[] = {{"You need the gold key", true}, {"You must press the three buttons...", true},
        {"You found a secret area!", true}, {"Quad Damage is wearing off", false},
        {"Are you sure you want to exit now?\nYou left something important behind.", true},
        {"A secret cave has opened...", true}};
    static int next = 0;
    const Test& t = tests[next];
    next = (next + 1) % static_cast<int>(za::getArraySize(tests));

    // As the game's: its sound at the head (a trigger's misc/talk.wav), unless it is the notification's (from the
    // gadget, as the message is taken: announce).
    if(!hologramOnly() || gadgetInView())
    {
        S_LocalSound("misc/talk.wav");
    }
    if(!t.centre)
    {
        Con_ServerPrint((za::String{t.text} + "\n").cStr());
    }
    else if(key_dest == key_menu)
    {
        centrePrint(t.text); // (the game's centre prints are not taken while a menu is open)
    }
    else
    {
        SCR_CenterPrint(t.text);
        Con_LogCenterPrint(t.text);
    }
    return true;
}

const Pose& pose()
{
    return current;
}

void screenRect(glm::vec3& corner, glm::vec2& size)
{
    // make_gadget.py: the screen spans x -1.5..1.5, y -0.93..0.93, over the top face at z 0.41.
    corner = {-1.5f, -0.93f, 0.43f};
    size = {3.f, 1.86f};
}

void renderScreen()
{
    QVR_GPU_PROFILE("gadget screen");
    text3d::renderScreens(); // the weapons' ammo screens' images too
    if(!active())
    {
        return;
    }

    gfx::ensureTarget(target, width * 2, height * 2, true, "gadget screen"); // mipmaps: for its text's glow
    gfx::begin2D(target, width, height);
    layout();
    gfx::end2D();
    renderHologram();
    renderFps();

    if(static bool registered = false; !registered) // a test command (the module has no init hook here)
    {
        registered = true;
        Cmd_AddCommand("vr_message_test", messageTest_f);
        Cmd_AddCommand("vr_gadget_info", gadgetInfo_f);
        Cmd_AddCommand("vr_gadget_screen_dump", screenDump_f);
    }
}

void drawScreen()
{
    if(!active() || !current.valid || !target.texture)
    {
        return;
    }

    glm::vec3 corner;
    glm::vec2 size;
    screenRect(corner, size);
    const glm::vec3 origin = current.origin + current.axes * (corner * current.scale);
    const glm::vec3 xAxis = current.axes[0] * (size.x * current.scale);
    const glm::vec3 yAxis = current.axes[1] * (size.y * current.scale);

    // The phosphor's colour is the text's; the texture's brightness says how lit each texel is.
    const glm::vec4 phosphor = palette().text;
    const gfx::Vertex c[4] = {{origin, {0.f, 0.f}, phosphor}, {origin + xAxis, {1.f, 0.f}, phosphor},
        {origin + xAxis + yAxis, {1.f, 1.f}, phosphor}, {origin + yAxis, {0.f, 1.f}, phosphor}};
    const gfx::Vertex quad[6] = {c[0], c[1], c[2], c[0], c[2], c[3]};

    const float time = static_cast<float>(za::fmod(realtime, 1000.0));
    const float k = crtStrength();
    gfx::draw(quad, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Screen, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = true,
            .params = {time, k, k > 0.f ? glitch(realtime) * za::min(k, 1.f) : 0.f, textGlow()},
            .screen = {width, height, 0.5f}},
        target.texture);
}

float textGlow()
{
    return CLAMP(0.f, vr_screen_text_glow.value, 3.f);
}

bool screenGlow(Glow& out)
{
    const float k = CLAMP(0.f, vr_screen_glow.value, 3.f);
    const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f);
    if(k <= 0.f || bright <= 0.f || !active() || !current.valid || !target.texture)
    {
        return false;
    }

    // Just over the screen (its face 0.43 out), round it over the bezel and the casing.
    glm::vec3 corner;
    glm::vec2 size;
    screenRect(corner, size);
    out.centre = current.origin + current.axes[2] * ((corner.z + 0.02f) * current.scale);
    out.right = current.axes[0];
    out.up = current.axes[1];
    out.halfSize = size * (0.5f * current.scale);
    out.spread = 0.32f * current.scale;
    out.color = glm::vec4{glm::vec3{palette().text}, 0.25f * k};
    return true;
}

bool log(Log& out)
{
    out.lines.clear();
    out.alpha.clear();
    if(!logShown())
    {
        return false;
    }

    const float life = vr_notify_wrist_time.value > 0.f ? vr_notify_wrist_time.value
                                                        : static_cast<float>(Cvar_VariableValue("con_notifytime"));
    if(life <= 0.f)
    {
        return false;
    }

    // Newest first, each console line's wrapped lines in reverse; turned round below. The game's
    // lines are the hologram's (vr_messages_hologram); the rest dimmer (vr_notify_wrist_alpha).
    // The lines are kept in the scratch (out.lines views them) until the next call: nothing allocated from frame to frame.
    Lines& wrapped = scratch.wrapped;
    za::Vector<qza::Pair<za::SizeT, float>>& picked = scratch.picked; // wrapped's line, its alpha
    NotifyLine& line = scratch.notifyLine;
    wrapped.clear();
    picked.clear();
    const bool hologram = hologramOn();
    const float dim = CLAMP(0.1f, vr_notify_wrist_alpha.value, 1.f);
    for(int age = 0; age < 16 && static_cast<int>(picked.size()) < logRows; age++)
    {
        if(!notifyLine(age, line))
        {
            continue; // not a notify line
        }
        const float alpha = CLAMP(0.f, (life - static_cast<float>(line.seconds)) / logFade, 1.f) * dim;
        if(alpha <= 0.f)
        {
            break; // older lines are older still
        }
        if(hologram && line.game)
        {
            continue;
        }
        const za::SizeT first = wrapped.size();
        wrap(line.text, wrapped);
        for(za::SizeT i = wrapped.size(); i-- > first && static_cast<int>(picked.size()) < logRows;)
        {
            picked.emplaceBack(i, alpha);
        }
    }
    if(picked.empty())
    {
        return false;
    }
    for(auto it = qza::rbegin(picked); it != qza::rend(picked); ++it) // (views taken once `wrapped` is complete)
    {
        out.lines.pushBack(wrapped[it->first]);
        out.alpha.pushBack(it->second);
    }

    // Over the gadget (as seen) and the hologram, its characters about 7 mm, in the screen's colour
    // somewhat greyed; it follows the hologram's top smoothly.
    const Palette pal = palette();
    out.base = screenCentre();
    layoutHologram();
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const float cm = current.scale / cmPerModelUnit;
    const float lift = za::max(gadgetTop(up) + za::max(0.f, vr_notify_wrist_height.value) * cm,
        holo.top > 0.f ? holo.top + 1.5f * cm : 0.f);
    logLift = logLift < 0.f ? lift : logLift + (lift - logLift) * za::min(1.f, static_cast<float>(host_frametime) * 10.f);
    out.lift = logLift;
    out.normal = current.axes[2];
    out.charSize = 0.23f * current.scale;
    out.color = glm::mix(glm::vec3{pal.text}, glm::vec3{glm::dot(glm::vec3{pal.text}, glm::vec3{0.3f, 0.5f, 0.2f})}, 0.4f);
    out.backColor = hue::color(vr_gadget_screen_hue, 0.57f, 0.05f);
    return true;
}

void drawHologram()
{
    if(!active())
    {
        return;
    }
    drawFps();
    layoutHologram();
    if(holo.text.empty())
    {
        return;
    }
    QVR_GPU_PROFILE("gadget hologram");
    const glm::mat4 viewProjection = gfx::sceneViewProjection();
    if(!holo.beam.empty())
    {
        gfx::draw(holo.beam, viewProjection,
            {.shade = gfx::Shade::Hologram, .blend = gfx::Blend::Premultiplied, .depthTest = true, .depthWrite = false,
                .params = {holo.params.x, holo.params.y, 0.f, 1.f}});
    }
    // Not depth tested: over the other hand, as the log.
    gfx::draw(holo.text, viewProjection,
        {.shade = gfx::Shade::Hologram, .blend = gfx::Blend::Premultiplied, .depthTest = false, .depthWrite = false,
            .params = holo.params, .screen = {holoWidth, holoHeight, 0.5f}},
        holoTarget.texture);
}

namespace
{

// vr_message_test: the next test message (testMessage, the menu's). vr_message_test <center|print|console> <text>: a
// message as the game's centre print, a server's print, or the engine's console line ("\n" in it: a new line), to see
// where it shows.
void messageTest_f()
{
    if(Cmd_Argc() == 1)
    {
        if(!testMessage())
        {
            Con_Printf("vr_message_test: no hologram (in a game, vr_hud_mode 1, vr_messages_hologram 1)\n");
        }
        return; // the Screens page's Show a Test Message
    }
    if(Cmd_Argc() < 3)
    {
        Con_Printf("usage: vr_message_test [<center|print|console> <text>]\n");
        return;
    }
    za::String text;
    for(int i = 2; i < Cmd_Argc(); i++)
    {
        text += (i > 2 ? " " : "") + za::String{Cmd_Argv(i)};
    }
    for(size_t at; (at = text.find("\\n")) != za::StringView::nPos;)
    {
        text.replace(at, 2, "\n");
    }
    const za::StringView kind = Cmd_Argv(1);
    if(kind == "center" || kind == "centre")
    {
        SCR_CenterPrint(text.cStr());
        Con_LogCenterPrint(text.cStr());
    }
    else if(kind == "print")
    {
        Con_ServerPrint((text + "\n").cStr());
    }
    else
    {
        Con_Printf("%s\n", text.cStr());
    }
}

// vr_gadget_info: the gadget's pose, for tests: its screen's centre and axes (right, up, out), world units.
void gadgetInfo_f()
{
    if(!current.valid)
    {
        Con_Printf("vr_gadget_info: no gadget\n");
        return;
    }
    const glm::vec3& o = current.origin;
    const glm::mat3& a = current.axes;
    Con_Printf("gadget: origin %.3f %.3f %.3f right %.4f %.4f %.4f up %.4f %.4f %.4f out %.4f %.4f %.4f\n", o.x, o.y, o.z,
        a[0].x, a[0].y, a[0].z, a[1].x, a[1].y, a[1].z, a[2].x, a[2].y, a[2].z);
}

// vr_gadget_screen_dump [name]: the screen's image as drawn this frame (before the phosphor's tint, the CRT and the
// glow), flat, to screenshots/<name>.png (default gadget_screen), for tests.
void screenDump_f()
{
    if(!target.texture || !target.framebuffer)
    {
        Con_Printf("vr_gadget_screen_dump: no screen yet\n");
        return;
    }
    const int w = target.width, h = target.height;
    za::Vector<byte> rgba(static_cast<za::SizeT>(w) * h * 4);
    za::Vector<byte> rgb(static_cast<za::SizeT>(w) * h * 3);
    GLint previous = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, target.framebuffer);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous));
    for(za::SizeT p = 0; p < static_cast<za::SizeT>(w) * h; p++)
    {
        for(za::SizeT c = 0; c < 3; c++)
        {
            rgb[p * 3 + c] = rgba[p * 4 + c];
        }
    }
    char name[MAX_OSPATH];
    q_snprintf(name, sizeof(name), "screenshots/%s.png", Cmd_Argc() > 1 ? Cmd_Argv(1) : "gadget_screen");
    Sys_mkdir(va("%s/screenshots", com_gamedir));
    if(Image_WritePNG(name, rgb.data(), w, h, 24, false))
    {
        Con_Printf("vr_gadget_screen_dump: %s (%dx%d)\n", name, w, h);
    }
}

} // namespace

} // namespace qvr::gadget

// A centre print (SCR_CenterPrint): the game's, for the hologram; not the options menu's preview, nor
// the intermission's text (the gadget is put away). Each a message of its own (they stack); the same
// one again while it shows keeps it on (a locked door touched again); an empty one clears the latest
// (unless it waits to be seen).
extern "C" void VR_GameCenterPrint(const char* str)
{
    using namespace qvr::gadget;
    if(key_dest == key_menu || cl.intermission)
    {
        return;
    }
    centrePrint(str ? str : "");
}

// While the hologram shows the centre print, it is not shown in front of the head too; with
// vr_messages_hologram_only, never (it waits in the hologram).
extern "C" int VR_CenterPrintOnWrist()
{
    using namespace qvr::gadget;
    return hologramOn() && (hologramOnly() || (holo.centreShown && holo.frame >= host_framecount - 1));
}

// Con_DrawNotify, a server's line (`text`, `length` characters) about to be drawn in view (vr_notify_wrist 0 or 2):
// nonzero to leave it out, a game message with vr_messages_hologram_only (it waits in the hologram).
extern "C" int VR_GameLineOnWrist(const char* text, int length)
{
    using namespace qvr::gadget;
    if(!hologramOnly() || !text)
    {
        return 0;
    }
    za::String& plain = scratch.plain; // (Con_DrawNotify's, one line at a time)
    plain.assign(text, static_cast<za::SizeT>(za::max(length, 0)));
    for(char& c : plain)
    {
        c = static_cast<char>(static_cast<unsigned char>(c) & 127);
    }
    while(!plain.empty() && plain.back() == ' ')
    {
        plain.popBack();
    }
    return !plain.empty() && !engineLine(plain);
}

// CL_ParseStartSoundPacket: a sound the server started. Quake's message sounds, misc/talk.wav (a trigger's text, on
// the trigger or the player) and misc/secret.wav (a secret found, on its trigger), are the notification's with
// vr_messages_hologram_only while the gadget is out of view: played at the gadget instead, as its chime (they come just
// before their message: announce then only buzzes). Nonzero when taken.
extern "C" int VR_GameSound(int entnum, sfx_t* sfx)
{
    using namespace qvr::gadget;
    (void)entnum;
    if(!sfx || (q_strcasecmp(sfx->name, "misc/talk.wav") && q_strcasecmp(sfx->name, "misc/secret.wav")) ||
        !hologramOnly() || gadgetInView() || !current.valid)
    {
        return 0;
    }
    chime(sfx); // (nothing if one just played)
    return 1;
}

// The notify lines go to the log over the wrist gadget (vr_notify_wrist 1), not the view's edge.
extern "C" int VR_NotifyOnWrist()
{
    using namespace qvr;
    using namespace qvr::gadget;
    return vr_notify_wrist.value == 1.f && logShown();
}
