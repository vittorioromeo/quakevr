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
#include "vr_profile.hpp"
#include "vr_text3d.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>

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
    if(red && std::fmod(realtime, 0.8) >= 0.5)
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

void layout()
{
    const Palette pal = palette();

    // A tinted screen with faint scanlines (the CRT look has its own) and a frame.
    fill(0.f, 0.f, width, height, pal.background);
    for(int y = 0; y < height && crtStrength() <= 0.f; y += 3)
    {
        fill(0.f, static_cast<float>(y), width, 1.f, pal.scanline);
    }
    fill(0.f, 0.f, width, 2.f, pal.line);
    fill(0.f, height - 2.f, width, 2.f, pal.line);
    fill(0.f, 0.f, 2.f, height, pal.line);
    fill(width - 2.f, 0.f, 2.f, height, pal.line);

    gfx::draw2D::color(pal.text);
    gfx::draw2D::text(8.f, 6.f, 8.f, "RANGER STATUS");
    gfx::draw2D::color(white);
    fill(8.f, 16.f, width - 16.f, 1.f, pal.line);

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

// `text` broken into lines of at most `columns` characters, between words where it can be,
// appended to `out` (the console's coloured characters plain: the log is in the screen's colour).
void wrap(std::string_view text, std::vector<std::string>& out, int columns = logColumns)
{
    while(!text.empty() && text.back() == ' ')
    {
        text.remove_suffix(1);
    }
    while(!text.empty())
    {
        size_t cut = std::min(text.size(), static_cast<size_t>(columns));
        if(cut < text.size())
        {
            const size_t space = text.substr(0, cut + 1).rfind(' ');
            if(space != std::string_view::npos && space > 0)
            {
                cut = space;
            }
        }
        std::string line{text.substr(0, cut)};
        for(char& c : line)
        {
            c = static_cast<char>(static_cast<unsigned char>(c) & 127);
        }
        out.push_back(std::move(line));
        text.remove_prefix(cut);
        while(!text.empty() && text.front() == ' ')
        {
            text.remove_prefix(1);
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
        top = std::max(top, glm::dot(current.origin + current.axes * (m * current.scale) - c, up));
    }
    return top;
}

[[nodiscard]] float facing(const glm::vec3& eye)
{
    const glm::vec3 toEye = eye - screenCentre();
    const float f = glm::dot(current.axes[2], toEye) / std::max(glm::length(toEye), 0.01f);
    return CLAMP(0.f, (f - 0.2f) / 0.3f, 1.f);
}

// ---------------------------------------------------------------------------------------------
// The hologram (vr_messages_hologram, vr_gadget.hpp).

// A console line, plain (the coloured characters' high bit off, trailing spaces trimmed): how old it
// is, and whether it is the game's (a server print that is not the engine's own reply, engineLine).
struct NotifyLine
{
    std::string text;
    double seconds{0.0};
    bool game{false};
};

// Whether a server print's line is the engine's own reply rather than the game's (the progs'): the
// server's banner, a cheat's "godmode ON", setpos's and ping's figures, a server cvar changed, a pause.
[[nodiscard]] bool engineLine(std::string_view s)
{
    while(!s.empty() && s.front() == ' ')
    {
        s.remove_prefix(1);
    }
    constexpr std::string_view prefixes[] = {"VERSION ", "usage:", "current values:", "Client ping times",
        "Can't suicide", "Pause not allowed", "Kicked by"};
    for(const std::string_view p : prefixes)
    {
        if(s.starts_with(p))
        {
            return true;
        }
    }
    if(s.find("\" changed to \"") != std::string_view::npos || s.ends_with(" paused the game") ||
        s.ends_with(" unpaused the game"))
    {
        return true;
    }
    // "godmode ON", "noclip OFF": a word, then ON or OFF.
    const size_t space = s.find(' ');
    if(space != std::string_view::npos && s.find(' ', space + 1) == std::string_view::npos)
    {
        const std::string_view word = s.substr(space + 1);
        if(word == "ON" || word == "OFF")
        {
            return true;
        }
    }
    // Figures only.
    return std::none_of(s.begin(), s.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); });
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
    out.text.assign(text, static_cast<size_t>(length));
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
    std::string text; // its lines, joined by '\n'
    int columns{0}, rows{0};
    double start{0.0};    // realtime it was printed (or last repeated)
    double appeared{0.0}; // realtime it first was
};

// A message's block in the image.
struct HoloBlock
{
    std::string text;
    int columns{0}, rows{0};
    float v0{0.f}, v1{0.f}; // its texture rows (v up: v0 its bottom)
};

HoloMessage centre;                    // the latest centre print (empty: none)
std::vector<HoloMessage> holoMessages; // this frame's, oldest first
int holoCollected = -1;                // the host frame they were collected in
gfx::Target holoTarget;
std::vector<HoloBlock> holoDrawn; // what the image holds
std::string holoDrawnKey;

// Laid out once a frame (layoutHologram), for both eyes.
struct HoloFrame
{
    int frame{-1};
    std::vector<gfx::Vertex> text, beam;
    glm::vec4 params{0.f};
    float top{0.f};            // world units its top is over the screen's centre (the view's up); 0: not shown
    bool centreShown{false};   // it shows the centre print
    double lastShown{-100.0};  // realtime it was last shown
    double opened{-100.0};     // realtime it last appeared: its projection's start
};
HoloFrame holo;
float logLift = -1.f; // the log's lift, following the hologram's top smoothly (world units; -1 not yet)

[[nodiscard]] bool hologramOn()
{
    return vr_messages_hologram.value != 0.f && active();
}

[[nodiscard]] float hologramLife()
{
    return std::max(1.f, vr_messages_hologram_time.value);
}

[[nodiscard]] float hologramEffect()
{
    return CLAMP(0.f, vr_messages_hologram_effect.value, 2.f);
}

// `text` as a message: plain, its lines wrapped to holoColumns, the empty ones at its ends left out.
void makeMessage(std::string_view text, HoloMessage& m)
{
    static std::vector<std::string> lines;
    lines.clear();
    while(!text.empty())
    {
        const size_t end = std::min(text.find('\n'), text.size());
        const size_t before = lines.size();
        wrap(text.substr(0, end), lines, holoColumns);
        if(lines.size() == before)
        {
            lines.emplace_back(); // an empty line
        }
        text.remove_prefix(std::min(end + 1, text.size()));
    }
    while(!lines.empty() && lines.back().empty())
    {
        lines.pop_back();
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
        m.text += (i > first ? "\n" : "") + lines[i];
        m.columns = std::max(m.columns, static_cast<int>(lines[i].size()));
        m.rows++;
    }
}

// This frame's messages (once a frame): the game's lines from the console's newest, and the centre
// print, while they last; oldest first, the oldest left out beyond holoRows lines.
void collectMessages()
{
    if(holoCollected == host_framecount)
    {
        return;
    }
    holoCollected = host_framecount;
    holoMessages.clear();
    if(!hologramOn())
    {
        return;
    }

    const double life = hologramLife();
    NotifyLine line;
    for(int age = 0; age < 16 && static_cast<int>(holoMessages.size()) < holoMessagesMax; age++)
    {
        if(!notifyLine(age, line))
        {
            continue;
        }
        if(line.seconds >= life)
        {
            break; // older lines are older still
        }
        if(!line.game || line.text.empty())
        {
            continue;
        }
        HoloMessage& m = holoMessages.emplace_back();
        makeMessage(line.text, m);
        m.start = m.appeared = realtime - line.seconds;
        if(m.rows == 0)
        {
            holoMessages.pop_back();
        }
    }
    if(!centre.text.empty() && realtime - centre.start < life)
    {
        holoMessages.push_back(centre);
    }
    std::stable_sort(holoMessages.begin(), holoMessages.end(),
        [](const HoloMessage& a, const HoloMessage& b) { return a.start < b.start; });

    int rows = 0;
    size_t keep = holoMessages.size();
    while(keep > 0 && rows + holoMessages[keep - 1].rows <= holoRows &&
          static_cast<int>(holoMessages.size() - keep) < holoMessagesMax)
    {
        rows += holoMessages[--keep].rows;
    }
    if(keep == holoMessages.size() && !holoMessages.empty())
    {
        // The newest alone is too long: its first lines.
        HoloMessage& m = holoMessages.back();
        int lines = 1;
        for(size_t i = 0; i < m.text.size(); i++)
        {
            if(m.text[i] == '\n' && ++lines > holoRows)
            {
                m.text.resize(i);
                break;
            }
        }
        m.rows = std::min(m.rows, holoRows);
        keep--;
    }
    holoMessages.erase(holoMessages.begin(), holoMessages.begin() + static_cast<std::ptrdiff_t>(keep));
}

// The image, if the messages changed (end of the 2D pass).
void renderHologram()
{
    collectMessages();
    if(holoMessages.empty())
    {
        return; // the old image holds (its blocks are matched by their text)
    }
    std::string key;
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
        std::string_view text = m.text;
        for(int row = 0; row < m.rows; row++)
        {
            const size_t end = std::min(text.find('\n'), text.size());
            const std::string line{text.substr(0, end)};
            const float x = static_cast<float>(holoWidth - static_cast<int>(line.size()) * 8) * 0.5f;
            gfx::draw2D::text(x, static_cast<float>(y + holoPad + row * 8), 8.f, line.c_str());
            text.remove_prefix(std::min(end + 1, text.size()));
        }
        holoDrawn.push_back({m.text, m.columns, m.rows, 1.f - static_cast<float>(y + h) / holoHeight,
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
    collectMessages();
    if(holoMessages.empty() || !current.valid || !holoTarget.texture || holoDrawn.empty())
    {
        return;
    }

    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const float scale = current.scale;
    const glm::vec3 c = screenCentre();
    const float lift = gadgetTop(up) + std::max(0.f, vr_messages_hologram_height.value) / cmPerModelUnit * scale;
    const glm::vec3 base = c + up * lift;

    // While the screen faces the viewer, and it is in view.
    float shown = facing(eye);
    const glm::vec4 clip = gfx::sceneViewProjection() * glm::vec4{base, 1.f};
    if(clip.w <= 0.f || std::abs(clip.x) > clip.w * 1.1f || std::abs(clip.y) > clip.w * 1.1f)
    {
        shown = 0.f;
    }
    if(shown <= 0.f)
    {
        return;
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
    const glm::vec3 rgb = hue::color(vr_gadget_screen_hue, 0.5f, 0.95f * std::max(bright, 0.3f));
    const double life = hologramLife();

    // The blocks, newest first from the bottom up, each growing as it appears.
    static std::vector<bool> used;
    used.assign(holoDrawn.size(), false);
    float y = 0.f, widest = 0.f, strongest = 0.f;
    double newest = -100.0;
    for(auto it = holoMessages.rbegin(); it != holoMessages.rend(); ++it)
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
                           CLAMP(0.f, static_cast<float>(m.start + life - realtime) / holoFadeOut, 1.f);
        if(fade <= 0.f)
        {
            continue;
        }
        const float grow = k > 0.f ? easeOut(age / holoGrow) : 1.f;
        const float alpha = fade * shown * (0.4f + 0.6f * open);
        newest = std::max(newest, m.appeared);
        strongest = std::max(strongest, alpha);
        if(!centre.text.empty() && m.text == centre.text && m.start == centre.start)
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
            holo.text.push_back(*p);
        }
        y += h;
        widest = std::max(widest, w);
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
        g = std::max(g, 0.8f * std::sin(since / 0.35f * 3.14159265f));
    }
    g = std::max(g, 0.8f * (1.f - open));
    const float time = static_cast<float>(std::fmod(realtime, 1000.0));
    holo.params = {time, k, g * std::min(k, 1.f), 0.f};

    // The beam: from the screen (a little inside its edge) to the blocks' outline, its corners
    // matched by where they are in the view (whichever way the gadget is turned).
    const float beam = 0.3f * std::min(k, 1.f) * strongest * (1.f + 1.5f * (1.f - open));
    if(beam <= 0.f)
    {
        return;
    }
    glm::vec3 corner;
    glm::vec2 size;
    screenRect(corner, size);
    std::array<glm::vec3, 4> s;
    std::array<float, 4> angle;
    for(int i = 0; i < 4; i++)
    {
        const glm::vec3 m{((i == 1 || i == 2) ? 0.5f : -0.5f) * size.x * 0.85f, (i >= 2 ? 0.5f : -0.5f) * size.y * 0.85f,
            corner.z + 0.02f};
        s[static_cast<size_t>(i)] = current.origin + current.axes * (m * scale);
        const glm::vec3 d = s[static_cast<size_t>(i)] - c;
        angle[static_cast<size_t>(i)] = std::atan2(glm::dot(d, up), glm::dot(d, right));
    }
    // Round the screen from the corner nearest the view's lower left (-135 degrees), as the blocks' outline.
    std::array<int, 4> order{0, 1, 2, 3};
    std::sort(order.begin(), order.end(), [&](int a, int b) { return angle[static_cast<size_t>(a)] < angle[static_cast<size_t>(b)]; });
    int first = 0;
    float nearest = 10.f;
    for(int i = 0; i < 4; i++)
    {
        float d = std::abs(angle[static_cast<size_t>(order[static_cast<size_t>(i)])] + 2.3561945f);
        d = std::min(d, 6.2831853f - d);
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
            holo.beam.push_back(*q);
        }
    }
}

void messageTest_f();

} // namespace

// Once in each 7 seconds, at a random moment of them, a burst of 0.12 to 0.35 seconds rising and
// falling; one in four slots has none.
float glitch(double time)
{
    constexpr double slot = 7.0;
    const double index = std::floor(time / slot);
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
    return static_cast<float>(std::sin(x * 3.14159265)) * (0.6f + 0.4f * hash(n + 2u));
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

    if(static bool registered = false; !registered) // a test command (the module has no init hook here)
    {
        registered = true;
        Cmd_AddCommand("vr_message_test", messageTest_f);
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

    const float time = static_cast<float>(std::fmod(realtime, 1000.0));
    const float k = crtStrength();
    gfx::draw(quad, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Screen, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = true,
            .params = {time, k, k > 0.f ? glitch(realtime) * std::min(k, 1.f) : 0.f, textGlow()},
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
    static std::vector<std::string> wrapped;
    static NotifyLine line;
    const bool hologram = hologramOn();
    const float dim = CLAMP(0.1f, vr_notify_wrist_alpha.value, 1.f);
    for(int age = 0; age < 16 && static_cast<int>(out.lines.size()) < logRows; age++)
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
        wrapped.clear();
        wrap(line.text, wrapped);
        for(auto it = wrapped.rbegin(); it != wrapped.rend() && static_cast<int>(out.lines.size()) < logRows; ++it)
        {
            out.lines.push_back(std::move(*it));
            out.alpha.push_back(alpha);
        }
    }
    if(out.lines.empty())
    {
        return false;
    }
    std::reverse(out.lines.begin(), out.lines.end());
    std::reverse(out.alpha.begin(), out.alpha.end());

    // Over the gadget (as seen) and the hologram, its characters about 7 mm, in the screen's colour
    // somewhat greyed; it follows the hologram's top smoothly.
    const Palette pal = palette();
    out.base = screenCentre();
    layoutHologram();
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const float cm = current.scale / cmPerModelUnit;
    const float lift = std::max(gadgetTop(up) + std::max(0.f, vr_notify_wrist_height.value) * cm,
        holo.top > 0.f ? holo.top + 1.5f * cm : 0.f);
    logLift = logLift < 0.f ? lift : logLift + (lift - logLift) * std::min(1.f, static_cast<float>(host_frametime) * 10.f);
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

// vr_message_test <center|print|console> <text>: a message as the game's centre print, a server's
// print, or the engine's console line ("\n" in it: a new line), to see where it shows.
void messageTest_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("usage: vr_message_test <center|print|console> <text>\n");
        return;
    }
    std::string text;
    for(int i = 2; i < Cmd_Argc(); i++)
    {
        text += (i > 2 ? " " : "") + std::string{Cmd_Argv(i)};
    }
    for(size_t at; (at = text.find("\\n")) != std::string::npos;)
    {
        text.replace(at, 2, "\n");
    }
    const std::string_view kind = Cmd_Argv(1);
    if(kind == "center" || kind == "centre")
    {
        SCR_CenterPrint(text.c_str());
        Con_LogCenterPrint(text.c_str());
    }
    else if(kind == "print")
    {
        Con_ServerPrint((text + "\n").c_str());
    }
    else
    {
        Con_Printf("%s\n", text.c_str());
    }
}

} // namespace

} // namespace qvr::gadget

// A centre print (SCR_CenterPrint): the game's, for the hologram; not the options menu's preview, nor
// the intermission's text (the gadget is put away). An empty one clears it; the same one again while
// it shows keeps it on (a locked door touched again).
extern "C" void VR_GameCenterPrint(const char* str)
{
    using namespace qvr::gadget;
    if(key_dest == key_menu || cl.intermission)
    {
        return;
    }
    HoloMessage m;
    makeMessage(str ? str : "", m);
    if(m.rows == 0)
    {
        centre = {};
        return;
    }
    if(m.text == centre.text && realtime - centre.start < hologramLife())
    {
        centre.start = realtime;
        return;
    }
    m.start = m.appeared = realtime;
    centre = std::move(m);
}

// While the hologram shows the centre print, it is not shown in front of the head too.
extern "C" int VR_CenterPrintOnWrist()
{
    using namespace qvr::gadget;
    return hologramOn() && holo.centreShown && holo.frame >= host_framecount - 1;
}

// The notify lines go to the log over the wrist gadget (vr_notify_wrist 1), not the view's edge.
extern "C" int VR_NotifyOnWrist()
{
    using namespace qvr;
    using namespace qvr::gadget;
    return vr_notify_wrist.value == 1.f && logShown();
}
