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
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_lighting.hpp"
#include "vr_main.hpp"
#include "vr_profile.hpp"
#include "vr_text3d.hpp"

#include <algorithm>
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

// The screen's palette, from vr_gadget_screen_hue / _brightness / _background (the default is
// the green of a phosphor screen).
struct Palette
{
    glm::vec3 background;
    glm::vec3 scanline;
    glm::vec3 line; // frame and separators
    glm::vec4 text;
};

[[nodiscard]] Palette palette()
{
    const float hue = vr_gadget_screen_hue.value;
    const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f);
    const float back = CLAMP(0.f, vr_gadget_screen_background.value, 4.f);
    Palette p;
    p.background = hsv(hue, 0.57f, 0.07f * back);
    p.scanline = hsv(hue, 0.6f, 0.05f * back);
    p.line = glm::min(hsv(hue, 0.58f, 0.6f * bright), glm::vec3{1.f});
    p.text = glm::vec4{glm::min(hsv(hue, 0.55f, bright), glm::vec3{1.f}), 1.f};
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
    const glm::vec3 c = hsv(vr_gadget_screen_hue.value, 0.5f, 1.f) * (k * (darkplaces ? 0.3f : 0.6f));
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

// `text` broken into lines of at most logColumns characters, between words where it can be,
// appended to `out` (the console's coloured characters plain: the log is in the screen's colour).
void wrap(std::string_view text, std::vector<std::string>& out)
{
    while(!text.empty() && text.back() == ' ')
    {
        text.remove_suffix(1);
    }
    while(!text.empty())
    {
        size_t cut = std::min(text.size(), static_cast<size_t>(logColumns));
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

    // Newest first, each console line's wrapped lines in reverse; turned round below.
    static std::vector<std::string> wrapped;
    const char* text = nullptr;
    int length = 0;
    double seconds = 0.0;
    for(int age = 0; age < 16 && static_cast<int>(out.lines.size()) < logRows; age++)
    {
        if(!Con_NotifyLine(age, &text, &length, &seconds))
        {
            continue; // not a notify line
        }
        const float alpha = CLAMP(0.f, (life - static_cast<float>(seconds)) / logFade, 1.f);
        if(alpha <= 0.f)
        {
            break; // older lines are older still
        }
        wrapped.clear();
        wrap(std::string_view{text, static_cast<size_t>(length)}, wrapped);
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

    // Just over the screen (3 x 1.86 model units, its face 0.43 out), its characters about 7 mm.
    const Palette pal = palette();
    out.base = current.origin + current.axes[2] * (0.43f * current.scale);
    out.lift = 1.3f * current.scale;
    out.normal = current.axes[2];
    out.charSize = 0.23f * current.scale;
    out.color = glm::vec3{pal.text};
    out.backColor = hsv(vr_gadget_screen_hue.value, 0.57f, 0.05f);
    return true;
}

} // namespace qvr::gadget

// The notify lines go to the log over the wrist gadget (vr_notify_wrist 1), not the view's edge.
extern "C" int VR_NotifyOnWrist()
{
    using namespace qvr;
    using namespace qvr::gadget;
    return vr_notify_wrist.value == 1.f && logShown();
}
