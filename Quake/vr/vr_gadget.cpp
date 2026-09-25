// vr_gadget.cpp -- see vr_gadget.hpp.
//
// The screen is drawn with the engine's own 2D functions (the status bar's pictures from gfx.wad,
// the console font; vr_gfx.hpp's draw2D) into an offscreen target, on a virtual 240 x 150 screen
// covering it. The panel (vr_panel.cpp) draws the texture over the model's
// screen in each eye, a frame later, as it does the rest of the HUD.

#include "vr_gadget.hpp"
#include "vr_color.hpp"
#include "vr_gfx.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_main.hpp"

#include <cstring>

namespace qvr::gadget
{
namespace
{

constexpr int width = 240;  // the virtual screen, and the texture's size (at twice that)
constexpr int height = 150;

Pose current;
gfx::Target target;

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

// Big numbers, right-aligned in `digits` places of 24 x 24 (scaled).
void number(float x, float y, int value, int digits, bool red, float scale)
{
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

    // A tinted screen with faint scanlines and a frame.
    fill(0.f, 0.f, width, height, pal.background);
    for(int y = 0; y < height; y += 3)
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

} // namespace

bool active()
{
    return vr_hud_mode.value == 1.f && vrActive() && cls.state == ca_connected && cls.signon == SIGNONS &&
           !cl.intermission;
}

void setPose(const Pose& pose)
{
    current = pose;
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
    if(!active())
    {
        return;
    }

    gfx::ensureTarget(target, width * 2, height * 2);
    gfx::begin2D(target, width, height);
    layout();
    gfx::end2D();
}

unsigned screenTexture()
{
    return target.texture;
}

} // namespace qvr::gadget
