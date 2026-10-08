// vr_gadget.cpp -- see vr_gadget.hpp.
//
// The screen is drawn with the engine's own 2D functions (the status bar's pictures from gfx.wad,
// made grey to take any colour; the console font; vr_gfx.hpp's draw2D) into an offscreen target, on a virtual
// 240 x 150 screen covering it, at the end of the 2D pass, in its own colours (palette(): a dark background in the
// screen's colour, near-white values with a dark outline, red warnings; layout() has the grid). Each eye's scene then
// shows the texture over the model's screen, a frame later, after the opaque entities (so that the bloom catches it),
// as a small CRT (vr_gadget_crt), with a soft glow round its edge (vr_screen_glow, drawn by vr_text3d with the
// weapons' ammo screens').
//
// The log over it (vr_notify_wrist) is the console's notify lines (console.c keeps the times of
// its last 16 lines for it: Con_NotifyLine), laid out by vr_text3d facing the viewer.

#include "vr_gadget.hpp"
#include "vr_body.hpp"
#include "vr_portals.hpp"
#include "vr_bullettime.hpp"
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
#include "vr_protocol.hpp"
#include "vr_relight.hpp"
#include "vr_text3d.hpp"
#include "vr_hands.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Erase.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/ReverseIterator.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
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
#include "Zancle/Vocabulary/Pair.hpp"
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

// The status bar's pictures (gfx.wad) the screen draws, made grey (each one's brightness, its brightest texel white,
// in the palette's grey ramp) so that they take any colour: the numbers white (or red), the icons the screen's colour.
enum PicId : int
{
    PicNum0 = 0,         // num_0 .. num_9
    PicNumMinus = 10,    // num_minus
    PicFace = 11,        // face5 (hurt) .. face1 (healthy)
    PicArmor = 16,       // sb_armor1 .. 3
    PicAmmo = 19,        // sb_shells, sb_nails, sb_rocket, sb_cells
    PicKey = 23,         // sb_key1, 2
    PicPowerup = 25,     // sb_invis, sb_invuln, sb_suit, sb_quad
    PicSigil = 29,       // sb_sigil1 .. 4
    PicCount = 33
};

constexpr const char* picNames[PicCount] = {"num_0", "num_1", "num_2", "num_3", "num_4", "num_5", "num_6", "num_7",
    "num_8", "num_9", "num_minus", "face5", "face4", "face3", "face2", "face1", "sb_armor1", "sb_armor2", "sb_armor3",
    "sb_shells", "sb_nails", "sb_rocket", "sb_cells", "sb_key1", "sb_key2", "sb_invis", "sb_invuln", "sb_suit", "sb_quad",
    "sb_sigil1", "sb_sigil2", "sb_sigil3", "sb_sigil4"};

// The grey pictures (made from the game's gfx.wad when first drawn; again after a game change: onGameDirChanged).
struct GreyPics
{
    bool built{false};
    za::Array<za::Vector<byte>, PicCount> pic{};  // Draw_PicBytes() each (Draw_ReplacePic's)
    za::Array<za::Vector<byte>, PicCount> data{}; // its texels (palette indices, 255 transparent): kept while drawn
    za::Array<bool, PicCount> ok{};
};
GreyPics greys;

void buildGreys()
{
    greys.built = true;
    for(int i = 0; i < PicCount; i++)
    {
        greys.ok[i] = false;
        lumpinfo_t* info = nullptr;
        const qpic_t* p = static_cast<const qpic_t*>(W_GetLumpName(picNames[i], &info));
        if(!p || !info || info->type != TYP_QPIC || p->width <= 0 || p->height <= 0 || p->width > 64 || p->height > 64 ||
            static_cast<size_t>(info->size) < sizeof(int) * 2 + static_cast<size_t>(p->width * p->height))
        {
            continue;
        }
        const int n = p->width * p->height;
        float lum[64 * 64];
        float top = 0.f;
        for(int t = 0; t < n; t++)
        {
            const byte index = p->data[t];
            lum[t] = 0.f;
            if(index != 255)
            {
                const unsigned c = d_8to24table[index];
                lum[t] = 0.2126f * static_cast<float>(c & 255) + 0.7152f * static_cast<float>((c >> 8) & 255) +
                         0.0722f * static_cast<float>((c >> 16) & 255);
                top = za::max(top, lum[t]);
            }
        }
        za::Vector<byte>& d = greys.data[i];
        d.resize(static_cast<za::SizeT>(n));
        for(int t = 0; t < n; t++)
        {
            // The grey ramp (palette 0 black .. 15 the lightest), a little brighter in the middle (the numbers' shading).
            const float v = top > 0.f ? glm::pow(lum[t] / top, 0.75f) : 0.f;
            d[static_cast<za::SizeT>(t)] = p->data[t] == 255 ? byte{255} : static_cast<byte>(za::lround(v * 15.f));
        }
        if(greys.pic[i].empty())
        {
            greys.pic[i].resize(Draw_PicBytes(), 0);
        }
        char name[32];
        q_snprintf(name, sizeof(name), "vr_gadget_%s", picNames[i]);
        Draw_ReplacePic(reinterpret_cast<qpic_t*>(greys.pic[i].data()), name, p->width, p->height, d.data());
        greys.ok[i] = true;
    }
}

[[nodiscard]] qpic_t* greyPic(int id)
{
    if(!greys.built)
    {
        buildGreys();
    }
    return greys.ok[id] ? reinterpret_cast<qpic_t*>(greys.pic[id].data()) : nullptr;
}

// The screen's font: the console's (gl_draw.c's char_texture, padded 10 x 10 cells), its white half's greys stretched
// so their lightest is the palette's white (index 15, 235) instead of the console's grey (index 11, 171): the text
// comes out near-white, not mid-grey, at full colour. Made when first drawn, again after a game change.
struct BrightFont
{
    bool built{false};
    za::Vector<byte> data;            // its texels (palette indices; 0 transparent): kept while drawn (a reload's)
    gltexture_t* texture{nullptr};
    gltexture_t* consoleFont{nullptr}; // char_texture while useBrightFont() is on
};
BrightFont brightFont;
constexpr const char* brightFontName = "vr_gadget_conchars";

} // namespace
} // namespace qvr::gadget

extern "C" gltexture_t* char_texture;
extern "C" byte char_texture_data[256 * 10 * 10];

namespace qvr::gadget
{
namespace
{

[[nodiscard]] gltexture_t* brightFontTexture()
{
    if(brightFont.built)
    {
        return brightFont.texture ? brightFont.texture : char_texture;
    }
    brightFont.built = true;
    if(gltexture_t* old = TexMgr_FindTexture(nullptr, brightFontName)) // (freed already by a game change, or not)
    {
        TexMgr_FreeTexture(old);
    }
    constexpr int size = 16 * 10, whiteRows = 8 * 10; // characters 0..127: the white half
    brightFont.data.resize(static_cast<za::SizeT>(size) * size);
    memcpy(brightFont.data.data(), char_texture_data, static_cast<size_t>(size) * size);
    byte top = 0;
    for(int t = 0; t < size * whiteRows; t++)
    {
        const byte b = brightFont.data[static_cast<za::SizeT>(t)];
        top = b < 16 && b > top ? b : top;
    }
    if(top > 0 && top < 15)
    {
        for(int t = 0; t < size * whiteRows; t++)
        {
            byte& b = brightFont.data[static_cast<za::SizeT>(t)];
            b = b > 0 && b < 16 ? static_cast<byte>(za::min(15l, za::lround(b * 15.f / top))) : b;
        }
    }
    brightFont.texture = TexMgr_LoadImage(nullptr, brightFontName, size, size, SRC_INDEXED, brightFont.data.data(), "",
        reinterpret_cast<src_offset_t>(brightFont.data.data()),
        TEXPREF_ALPHA | TEXPREF_NEAREST | TEXPREF_NOPICMIP | TEXPREF_CONCHARS | TEXPREF_UNCOMPRESSED);
    return brightFont.texture ? brightFont.texture : char_texture;
}

constexpr glm::vec4 white{1.f};

// The screen's palette. In its own colours (Shade::Screen's trueColor): a dark background and lines in the screen's
// colour (vr_gadget_screen_hue, by default the player's: vr_hue.hpp; _background, _brightness), the labels and icons
// in it, the values near-white (vr_gadget_screen_text_white) with a dark outline, warnings red. Text and values are
// over 10:1 against the background (white 17:1), the red over 7:1.
struct Palette
{
    glm::vec3 background;
    glm::vec3 scanline;
    glm::vec3 line;   // the frame, the rules, the bars' outlines
    glm::vec4 text;   // the screen's colour at full strength: its light, its glow, the static (and the wrist log's)
    glm::vec3 label;  // labels and icons
    glm::vec3 value;  // values: the numbers, the level's name
    glm::vec3 dim;    // a value that is nothing (no armour, no ammo), a meter not ready
    glm::vec3 warn;   // warnings: low health, an empty weapon, no stamina
    glm::vec3 shadow; // the outline round labels and values
};

[[nodiscard]] Palette palette()
{
    const cvar_t& own = vr_gadget_screen_hue;
    const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f);
    const float back = CLAMP(0.f, vr_gadget_screen_background.value, 4.f);
    const float whiteness = CLAMP(0.f, vr_gadget_screen_text_white.value, 1.f);
    const auto cap = [](const glm::vec3& c) { return glm::min(c, glm::vec3{1.f}); };
    Palette p;
    p.background = hue::color(own, 0.6f, 0.045f * back);
    p.scanline = hue::color(own, 0.6f, 0.032f * back);
    p.line = cap(hue::color(own, 0.58f, 0.55f * bright));
    p.text = glm::vec4{cap(hue::color(own, 0.55f, bright)), 1.f};
    p.label = cap(glm::mix(hue::color(own, 0.5f, 0.9f), glm::vec3{0.82f}, whiteness) * bright); // (a step under the values)
    p.value = cap(whitened(hue::color(own, 0.5f, 1.f), whiteness) * bright);
    p.dim = cap(hue::color(own, 0.45f, 0.55f * bright));
    p.warn = cap(glm::vec3{1.f, 0.42f, 0.36f} * bright);
    p.shadow = p.background * 0.2f;
    return p;
}

using gfx::draw2D::fill;

// A warning blinks: lit 0.5 s of every 0.8 (dimmed, never gone, the rest).
[[nodiscard]] bool blinkOn()
{
    return za::fmod(realtime, 0.8) < 0.5;
}

[[nodiscard]] glm::vec3 blinking(const glm::vec3& c)
{
    return blinkOn() ? c : c * 0.55f;
}

// Text in the console font, `size` pixels a character, in `rgb` over a thin dark outline (half a pixel: a texel).
void drawText(float x, float y, float size, const char* str, const glm::vec3& rgb, const Palette& pal)
{
    constexpr float o = 0.5f;
    static constexpr float offsets[4][2] = {{-o, 0.f}, {o, 0.f}, {0.f, -o}, {0.f, o}};
    useBrightFont(true);
    gfx::draw2D::color(glm::vec4{pal.shadow, 1.f});
    for(const auto& d : offsets)
    {
        gfx::draw2D::text(x + d[0], y + d[1], size, str);
    }
    gfx::draw2D::color(glm::vec4{rgb, 1.f});
    gfx::draw2D::text(x, y, size, str);
    gfx::draw2D::color(white);
    useBrightFont(false);
}

// A grey picture in `rgb`, `scale`d, over the same outline (the game's own when it has no grey one).
void drawPic(float x, float y, int id, float scale, const glm::vec3& rgb, const Palette& pal)
{
    qpic_t* p = greyPic(id);
    if(!p)
    {
        gfx::draw2D::pic(x, y, picNames[id], scale);
        return;
    }
    const float w = static_cast<float>(p->width) * scale, h = static_cast<float>(p->height) * scale;
    constexpr float o = 0.5f;
    static constexpr float offsets[4][2] = {{-o, 0.f}, {o, 0.f}, {0.f, -o}, {0.f, o}};
    const float shadow[3] = {pal.shadow.r, pal.shadow.g, pal.shadow.b};
    for(const auto& d : offsets)
    {
        Draw_SubPic(x + d[0], y + d[1], w, h, p, 0.f, 0.f, 1.f, 1.f, shadow, 1.f);
    }
    const float c[3] = {rgb.r, rgb.g, rgb.b};
    Draw_SubPic(x, y, w, h, p, 0.f, 0.f, 1.f, 1.f, c, 1.f);
}

// Big numbers (the status bar's), right-aligned in `digits` places of 24 x 24 (scaled).
void number(float x, float y, int value, int digits, const glm::vec3& rgb, float scale, const Palette& pal)
{
    char str[16];
    q_snprintf(str, sizeof(str), "%d", CLAMP(-99, value, 999));
    const int length = static_cast<int>(strlen(str));
    x += static_cast<float>(digits - length) * 24.f * scale;
    for(const char* c = str; *c; c++)
    {
        drawPic(x, y, *c == '-' ? PicNumMinus : PicNum0 + (*c - '0'), scale, rgb, pal);
        x += 24.f * scale;
    }
}

// What the screen shows: the client's state, or made-up readings (vr_gadget_test_state).
struct Readout
{
    int health{0};
    int armor{0};
    int armorType{-1};       // 0 .. 2 (green, yellow, red), -1 none
    int handAmmo[2]{0, 0};   // each hand's count (main, off), as its weapon's own screen shows it
    int handType[2]{-1, -1}; // its ammo's picture (0 shells .. 3 cells), -1 none (a fist, a melee weapon)
    int pools[4]{0, 0, 0, 0};
    int items{0};
    meleehud::State melee;
    bullettime::Meter time;
    const char* relight{nullptr}; // the maps being relit (relight::indicator), null when none are
    float relightProgress{0.f};
    const char* level{""};
    int kills{0}, totalKills{0}, secrets{0}, totalSecrets{0};
};

// QC's ammo type (AID_*: vr_defs.qc) as the picture of its kind (-1 none); Rogue's lava nails, multi-rockets and plasma
// as the nails, rockets and cells.
[[nodiscard]] int ammoPicOf(int aid)
{
    static constexpr int pics[8] = {-1, 0, 1, 2, 3, 1, 2, 3};
    return aid >= 0 && aid < 8 ? pics[aid] : -1;
}

[[nodiscard]] Readout readout()
{
    Readout r;
    r.health = cl.stats[STAT_HEALTH];
    r.armor = cl.stats[STAT_ARMOR];
    r.armorType = r.armor <= 0 ? -1 : (cl.items & IT_ARMOR3) ? 2 : (cl.items & IT_ARMOR2) ? 1 : 0;
    r.handAmmo[0] = cl.stats[protocol::STAT_QVR_AMMOCOUNTER];
    r.handAmmo[1] = cl.stats[protocol::STAT_QVR_AMMOCOUNTER2];
    r.handType[0] = ammoPicOf(cl.stats[protocol::STAT_QVR_AMMOTYPE]);
    r.handType[1] = ammoPicOf(cl.stats[protocol::STAT_QVR_AMMO2]);
    r.pools[0] = cl.stats[STAT_SHELLS];
    r.pools[1] = cl.stats[STAT_NAILS];
    r.pools[2] = cl.stats[STAT_ROCKETS];
    r.pools[3] = cl.stats[STAT_CELLS];
    r.items = cl.items;
    r.melee = meleehud::state();
    r.time = bullettime::meter();
    r.relight = relight::indicator();
    r.relightProgress = relight::progress();
    r.level = cl.levelname;
    r.kills = cl.stats[STAT_MONSTERS];
    r.totalKills = cl.stats[STAT_TOTALMONSTERS];
    r.secrets = cl.stats[STAT_SECRETS];
    r.totalSecrets = cl.stats[STAT_TOTALSECRETS];

    switch(static_cast<int>(vr_gadget_test_state.value))
    {
        case 1: // low: health, the main hand's ammo, stamina
            r.health = 18;
            r.armor = 0;
            r.armorType = -1;
            r.handAmmo[0] = 0;
            r.handType[0] = 0;
            r.pools[0] = 0;
            r.melee.stamina = true;
            r.melee.left = 0.2f;
            r.melee.low = true;
            r.melee.counter = 0.f;
            r.melee.draining = r.melee.recovering = false;
            break;
        case 2: // exhausted, a counter's window open
            r.melee.stamina = true;
            r.melee.left = 0.f;
            r.melee.low = true;
            r.melee.counter = 0.6f;
            r.melee.draining = r.melee.recovering = false;
            break;
        case 3: // hanging (stamina draining), bullet time running
            r.melee.stamina = true;
            r.melee.left = 0.55f;
            r.melee.low = false;
            r.melee.counter = 0.f;
            r.melee.draining = true;
            r.melee.recovering = false;
            r.time = {.enabled = true, .active = true, .cooling = false, .ready = false, .level = 0.6f};
            break;
        case 4: // maps being relit; stamina coming back, bullet time cooling down
            r.relight = "RELIGHT 3/12 45% 2:10";
            r.relightProgress = 0.45f;
            r.melee.stamina = true;
            r.melee.left = 0.35f;
            r.melee.low = false;
            r.melee.counter = 0.f;
            r.melee.draining = false;
            r.melee.recovering = true;
            r.time = {.enabled = true, .active = false, .cooling = true, .ready = false, .level = 0.f};
            break;
        case 5: // every key, powerup and sigil; big numbers, a long name
            r.items |= IT_KEY1 | IT_KEY2 | IT_INVISIBILITY | IT_INVULNERABILITY | IT_SUIT | IT_QUAD | static_cast<int>(0xF0000000u);
            r.health = 250;
            r.armor = 200;
            r.armorType = 2;
            r.handAmmo[0] = 200;
            r.handType[0] = 3;
            r.handAmmo[1] = 100;
            r.handType[1] = 2;
            r.pools[0] = 100;
            r.pools[1] = 200;
            r.pools[2] = 100;
            r.pools[3] = 200;
            r.level = "The Gloomy Underground Lair of the Old Ones";
            r.kills = 123;
            r.totalKills = 456;
            r.secrets = 12;
            r.totalSecrets = 34;
            break;
        default: break;
    }
    return r;
}

// The screen (240 x 150), on one grid, the most important at the top and biggest:
//
//   HEALTH          ARMOR             two rows of tiles, 108 wide: a label over an icon and a big number
//   [face] 100      [armour] 150      (the status bar's, white; red when low or empty)
//   MAIN HAND       OFF HAND
//   [ammo]  25      [ammo]  --
//   ----------------------------------
//   STAMINA  [] [] [] [] [] [] [] ...  three rows, their labels in one column, their meters and values in the next,
//   TIME     [=====================]   each always in its own place (empty when it shows nothing)
//   AMMO     [s] 25 [n] 0 [r] 0 [c] 0
//   ----------------------------------
//   THE SLIPGATE COMPLEX     [keys,    the level, kills and secrets (vr_gadget_show_level); the maps being relit
//   KILLS 0/23  SECRETS 0/6   items]   in place of the kills and secrets, a bar under them; the keys, powerups and
//   [relight progress bar]             sigils at the right
constexpr float margin = 8.f;
constexpr float tileWidth = 108.f;
constexpr float tileX[2] = {margin, margin + tileWidth + 8.f};
constexpr float tileY[2] = {4.f, 40.f};
constexpr float labelX = margin;  // the secondary rows' labels
constexpr float meterX = 72.f;    // and their meters and values
constexpr float meterRight = width - margin;
constexpr float rowY[3] = {80.f, 92.f, 104.f}; // stamina, time, ammo (the text's top)
constexpr float ruleY[2] = {76.f, 118.f};
constexpr float infoY[2] = {122.f, 132.f};     // the level's lines
constexpr float barY = 142.f;                  // the relight progress bar

// A tile: its label, an icon (a grey picture; -1 none) and a big number (or "--" for none).
void tile(int column, int row, const char* label, int icon, int value, bool none, const glm::vec3& rgb, const Palette& pal)
{
    const float x = tileX[column], y = tileY[row];
    drawText(x, y, 8.f, label, pal.label, pal);
    if(icon >= 0)
    {
        drawPic(x, y + 9.f, icon, 1.f, pal.label, pal);
    }
    if(none)
    {
        drawPic(x + tileWidth - 48.f, y + 9.f, PicNumMinus, 1.f, pal.dim, pal);
        drawPic(x + tileWidth - 24.f, y + 9.f, PicNumMinus, 1.f, pal.dim, pal);
        return;
    }
    number(x + tileWidth - 72.f, y + 9.f, value, 3, rgb, 1.f, pal);
}

// A meter's outline, from meterX to meterRight, at a row (9 high).
void meterFrame(float x, float y, float w, float h, const glm::vec3& rgb)
{
    fill(x, y, w, 1.f, rgb);
    fill(x, y + h - 1.f, w, 1.f, rgb);
    fill(x, y, 1.f, h, rgb);
    fill(x + w - 1.f, y, 1.f, h, rgb);
}

// The stamina row (vr_gadget_stamina; docs/vr-port/ROUND21.md, "Stamina on the gadget; the glow"): "STAMINA" and ten
// cells, lit for what's left (the one being filled lit in part), empty ones outlined. Low (one more one-handed parry
// knocks the weapon away): the lit cells blink red. None left: "EXHAUSTED" blinks red over the empty cells, and the
// screen's frame with it (layout). Coming back: a bright sweep runs along the empty cells. Hanging from a hold spends it
// (vr_climb_stamina): "HANGING", and a dark notch runs back through the lit ones. While a counter's window is open, the
// label is "COUNTER" lit in reverse, a thick bar under the cells running out with the window; without parry stamina
// that is all it shows.
void staminaRow(const Readout& r, const Palette& pal)
{
    const meleehud::State& m = r.melee;
    if(!vr_gadget_stamina.value || (!m.stamina && m.counter <= 0.f))
    {
        return;
    }
    const float y = rowY[0];
    constexpr float cellY = rowY[0] - 1.f, cellH = 9.f;
    constexpr int cells = 10;
    constexpr float cellGap = 2.f;
    const float cellW = za::floor((meterRight - meterX - cellGap * (cells - 1)) / cells);

    if(m.counter > 0.f)
    {
        fill(labelX - 2.f, y - 2.f, 7.f * 8.f + 4.f, 11.f, pal.value);
        gfx::draw2D::color(glm::vec4{pal.background, 1.f});
        gfx::draw2D::text(labelX, y, 8.f, "COUNTER");
        gfx::draw2D::color(white);
        fill(meterX, cellY + cellH + 1.f, za::max(2.f, za::round((meterRight - meterX) * m.counter)), 2.f, pal.value);
    }
    else
    {
        drawText(labelX, y, 8.f, m.draining ? "HANGING" : "STAMINA", pal.label, pal); // (a hang drains it: vr_climb_stamina)
    }
    if(!m.stamina)
    {
        return;
    }

    const float lit = m.left * cells; // cells' worth left
    const glm::vec3 on = m.low ? blinking(pal.warn) : pal.value;
    for(int i = 0; i < cells; i++)
    {
        const float x = meterX + i * (cellW + cellGap);
        const float share = za::clamp(lit - static_cast<float>(i), 0.f, 1.f);
        meterFrame(x, cellY, cellW, cellH, m.low ? pal.warn * 0.6f : pal.line);
        if(share > 0.f)
        {
            fill(x, cellY, za::max(1.f, za::round(cellW * share)), cellH, on);
        }
    }
    if(m.left <= 0.f)
    {
        if(blinkOn())
        {
            constexpr const char* word = "EXHAUSTED";
            const float x = za::round((meterX + meterRight) * 0.5f - 9.f * 4.f);
            fill(x - 3.f, cellY - 1.f, 9.f * 8.f + 6.f, cellH + 2.f, pal.background);
            drawText(x, y, 8.f, word, pal.warn, pal);
        }
    }
    else if(m.draining)
    {
        // The drain: a dark notch running back through the lit cells, towards the start, every 0.7 s.
        const float to = meterX + lit * (cellW + cellGap);
        const float t = static_cast<float>(za::fmod(realtime, 0.7) / 0.7);
        const float x = to - (to - meterX) * t;
        if(to - meterX > 3.f)
        {
            fill(za::max(x - 2.f, meterX), cellY + 1.f, 2.f, cellH - 2.f, pal.background);
        }
    }
    else if(m.recovering)
    {
        // The sweep: from what's lit to the end, every 0.7 s.
        const float from = meterX + lit * (cellW + cellGap);
        const float t = static_cast<float>(za::fmod(realtime, 0.7) / 0.7);
        const float x = from + (meterRight - from) * t;
        if(meterRight - from > 3.f)
        {
            fill(za::min(x, meterRight - 2.f), cellY + 1.f, 2.f, cellH - 2.f, pal.value);
        }
    }
}

// Bullet time's meter (vr_bullettime.cpp): "TIME" and a bar, lit for what's left. Running: the label white, the bar
// drains and its frame blinks; cooling down: only its frame, dim; not enough to start: the bar dim.
void timeRow(const Readout& r, const Palette& pal)
{
    const bullettime::Meter& m = r.time;
    if(!m.enabled)
    {
        return;
    }
    const float y = rowY[1] - 1.f, h = 9.f, w = meterRight - meterX;
    drawText(labelX, rowY[1], 8.f, "TIME", m.active ? pal.value : pal.label, pal);
    const glm::vec3 frame = m.active ? (blinkOn() ? pal.value : pal.line) : m.cooling ? pal.dim * 0.6f : pal.line;
    meterFrame(meterX, y, w, h, frame);
    if(!m.cooling)
    {
        fill(meterX + 2.f, y + 2.f, (w - 4.f) * CLAMP(0.f, m.level, 1.f), h - 4.f, m.active || m.ready ? pal.value : pal.dim);
    }
}

// Every ammo's count: its icon (small) and the count, white (dim when none). The slots are spaced so a 3-digit count
// still leaves a gap before the next icon (the last slot needs no gap after it).
void ammoRow(const Readout& r, const Palette& pal)
{
    drawText(labelX, rowY[2], 8.f, "AMMO", pal.label, pal);
    constexpr float iconW = 12.f, textGap = 1.f, maxCountW = 3.f * 8.f;
    const float slot = (meterRight - meterX - (iconW + textGap + maxCountW)) / 3.f;
    for(int i = 0; i < 4; i++)
    {
        const float x = za::round(meterX + slot * i);
        drawPic(x, rowY[2] - 2.f, PicAmmo + i, 0.5f, pal.label, pal);
        char count[8];
        q_snprintf(count, sizeof(count), "%d", CLAMP(0, r.pools[i], 999));
        drawText(x + iconW + textGap, rowY[2], 8.f, count, r.pools[i] > 0 ? pal.value : pal.dim, pal);
    }
}

// The level, kills and secrets (vr_gadget_show_level), the maps being relit in place of the kills and secrets with a
// bar under them, and the keys, powerups and sigils at the right: full size over both lines when the kills and secrets
// still fit beside them, else at half size on the level's line only, the second line then the whole width.
void infoRows(const Readout& r, const Palette& pal)
{
    constexpr int itemBits[10] = {IT_KEY1, IT_KEY2, IT_INVISIBILITY, IT_INVULNERABILITY, IT_SUIT, IT_QUAD,
        1 << 28, 1 << 29, 1 << 30, static_cast<int>(1u << 31)};
    const auto itemPic = [](int i) { return i < 2 ? PicKey + i : i < 6 ? PicPowerup + (i - 2) : PicSigil + (i - 6); };
    const auto itemWidth = [&](int i) {
        const qpic_t* p = greyPic(itemPic(i));
        return p ? static_cast<float>(p->width) : 16.f;
    };
    const auto columnsLeftOf = [](float x) { return za::max(0, static_cast<int>((x - 4.f - margin) / 8.f)); };

    float itemsW = 0.f; // at full size, with their gaps
    for(int i = 0; i < 10; i++)
    {
        itemsW += r.items & itemBits[i] ? itemWidth(i) + 2.f : 0.f;
    }

    char stats[64], shortStats[64];
    q_snprintf(stats, sizeof(stats), "KILLS %d/%d  SECRETS %d/%d", r.kills, r.totalKills, r.secrets, r.totalSecrets);
    q_snprintf(shortStats, sizeof(shortStats), "K %d/%d  S %d/%d", r.kills, r.totalKills, r.secrets, r.totalSecrets);
    const int fullColumns = columnsLeftOf(meterRight - itemsW);
    const bool small = itemsW > 0.f && static_cast<int>(strlen(shortStats)) > fullColumns &&
                       (r.relight || vr_gadget_show_level.value);
    const float scale = small ? 0.5f : 1.f;

    float x = meterRight;
    for(int i = 9; i >= 0; i--)
    {
        if(r.items & itemBits[i])
        {
            x -= itemWidth(i) * scale;
            const glm::vec3& rgb = i >= 2 && i < 6 ? pal.value : pal.label; // (powerups in use: white)
            drawPic(x, small ? infoY[0] : infoY[0] + 1.f, itemPic(i), scale, rgb, pal);
            x -= small ? 1.f : 2.f;
        }
    }
    const int columns = columnsLeftOf(x);                          // the level's line
    const int columns2 = small ? columnsLeftOf(meterRight) : columns; // the second line

    char line[64];
    if(vr_gadget_show_level.value)
    {
        q_snprintf(line, sizeof(line), "%.*s", za::min(columns, 40), r.level);
        for(char* c = line; *c; c++) // (the level's names are in mixed case: the font's capitals read better)
        {
            *c = *c >= 'a' && *c <= 'z' ? static_cast<char>(*c - 'a' + 'A') : *c;
        }
        drawText(margin, infoY[0], 8.f, line, pal.value, pal);
    }
    if(r.relight)
    {
        q_snprintf(line, sizeof(line), "%.*s", za::min(columns2, 40), r.relight);
        drawText(margin, infoY[1], 8.f, line, pal.value, pal);
        const float w = meterRight - margin;
        fill(margin, barY, w, 3.f, pal.line * 0.6f);
        fill(margin, barY, za::round(w * CLAMP(0.f, r.relightProgress, 1.f)), 3.f, pal.value);
        return;
    }
    if(!vr_gadget_show_level.value)
    {
        return;
    }
    const char* chosen = static_cast<int>(strlen(stats)) <= columns2 ? stats : shortStats;
    q_snprintf(line, sizeof(line), "%.*s", za::min(columns2, 40), chosen); // (never past the items)
    drawText(margin, infoY[1], 8.f, line, pal.label, pal);
}

void layout()
{
    const Palette pal = palette();
    const Readout r = readout();

    // A dark tinted screen with faint scanlines (the CRT look has its own) and a frame.
    fill(0.f, 0.f, width, height, pal.background);
    for(int y = 0; y < height && crtStrength() <= 0.f; y += 3)
    {
        fill(0.f, static_cast<float>(y), width, 1.f, pal.scanline);
    }
    // Exhausted (no stamina left, vr_gadget_stamina): the frame blinks red, to be seen out of the corner of the eye.
    const bool alarm = vr_gadget_stamina.value && r.melee.stamina && r.melee.left <= 0.f && blinkOn();
    const glm::vec3 frame = alarm ? pal.warn : pal.line;
    fill(0.f, 0.f, width, 2.f, frame);
    fill(0.f, height - 2.f, width, 2.f, frame);
    fill(0.f, 0.f, 2.f, height, frame);
    fill(width - 2.f, 0.f, 2.f, height, frame);

    // Health (the face its icon; red under 25) and armour.
    const int face = r.health >= 100 ? 4 : CLAMP(0, r.health / 20, 4);
    tile(0, 0, "HEALTH", PicFace + face, r.health, false, r.health < 25 ? blinking(pal.warn) : pal.value, pal);
    tile(1, 0, "ARMOR", r.armorType >= 0 ? PicArmor + r.armorType : -1, r.armor, false, r.armor > 0 ? pal.value : pal.dim, pal);

    // Each hand's weapon's ammo (red when empty; "--" for a weapon without).
    for(int h = 0; h < 2; h++)
    {
        const int type = r.handType[h], count = r.handAmmo[h];
        const bool none = type < 0 && count <= 0; // (a chainsaw's fuel, an enemy gun's clip: a count without a kind)
        tile(h, 1, h == 0 ? "MAIN HAND" : "OFF HAND", type >= 0 ? PicAmmo + type : -1, count, none,
            count <= 0 ? blinking(pal.warn) : pal.value, pal);
    }

    fill(margin, ruleY[0], width - 2.f * margin, 1.f, pal.line);
    staminaRow(r, pal);
    timeRow(r, pal);
    ammoRow(r, pal);
    fill(margin, ruleY[1], width - 2.f * margin, 1.f, pal.line);
    infoRows(r, pal);
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
    za::Vector<za::Pair<za::SizeT, float>> picked; // those shown: wrapped's line, its alpha
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
    bool tip{false};    // a tip (gadget::tip): waits to be seen, as with vr_messages_hologram_only
    float life{0.f};    // seconds it shows once seen (0: vr_messages_hologram_time)
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
    return m.notify && !m.seen && (hologramOnly() || m.tip);
}

[[nodiscard]] double lifeOf(const HoloMessage& m)
{
    return m.life > 0.f ? static_cast<double>(m.life) : static_cast<double>(hologramLife());
}

[[nodiscard]] bool alive(const HoloMessage& m)
{
    return m.generation == worldGeneration() && (held(m) ? realtime - m.appeared < heldMax : realtime - m.start < lifeOf(m));
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
    if(!m.notify || !(hologramOnly() || m.tip) || gadgetInView())
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
// collectMessages's buffers (the main thread's frame).
struct CollectScratch
{
    NotifyLine line;          // a console line read
    HoloMessage continuation; // a queued message's text, laid out again (continued since)
    auto members() { return qvr::mem::list(line, continuation); }
};
[[nodiscard]] za::SizeT heldBytes(const HoloMessage& m) // (vr_mem.hpp)
{
    return mem::heldBytes(m.text);
}
mem::Scratch<CollectScratch> collectScratch{"gadget messages"};

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

    NotifyLine& line = collectScratch.line;
    HoloMessage& continuation = collectScratch.continuation;
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
            [&](const HoloMessage& m) { return m.printed > 0.0 && za::abs(m.printed - printed) < 1e-4; });
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
    za::stableSort(holoMessages.begin(), holoMessages.end(),
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
    if(portals::viewing() || holo.frame == host_framecount)
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
    if(clip.w <= 0.f || za::abs(clip.x) > clip.w * 1.1f || za::abs(clip.y) > clip.w * 1.1f)
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
    // The blocks, newest first from the bottom up, each growing as it appears.
    za::Vector<bool>& used = scratch.used;
    used.clear();
    used.resize(holoDrawn.size(), false);
    float y = 0.f, widest = 0.f, strongest = 0.f;
    double newest = -100.0;
    for(const HoloMessage& m : za::reversed(holoMessages))
    {
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
                           (held(m) ? 1.f : CLAMP(0.f, static_cast<float>(m.start + lifeOf(m) - realtime) / holoFadeOut, 1.f));
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
        float d = za::abs(angle[static_cast<size_t>(order[static_cast<size_t>(i)])] + 2.3561945f);
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
    for(const auto& [label, series] : {za::Pair{"CPU", &f.cpu}, za::Pair{"GPU", &f.gpu}})
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
    za::fill(cpuCol, -1.f);
    za::fill(gpuCol, -1.f);
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
        for(auto it = za::rbegin(queue); it != za::rend(queue); ++it)
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
           !cl.intermission && !body::gearHiddenForDeath();
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

namespace
{
int nextTestMessage = 0; // testMessage's next (vr_test_message: the console)
} // namespace

bool tip(za::StringView text, float seconds)
{
    if(!hologramOn())
    {
        return false;
    }
    HoloMessage m;
    makeMessage(text, m);
    if(m.rows == 0)
    {
        return false;
    }
    m.start = m.appeared = realtime;
    m.notify = true;
    m.tip = true;
    m.life = za::max(seconds, 1.f);
    addMessage(ZA_MOVE(m));
    return true;
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
    int& next = nextTestMessage;
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

namespace
{
bool testCommandsRegistered = false; // (vr_message_test, vr_gadget_info: registered on the first call)
} // namespace

void onGameDirChanged()
{
    greys.built = false;      // (the grey pictures are made again from the new game's gfx.wad when next drawn,
    brightFont.built = false; // and the font from its conchars)
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

    if(!testCommandsRegistered) // a test command (the module has no init hook here)
    {
        testCommandsRegistered = true;
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

    // The texture in its own colours (palette()); the phosphor's colour (the static's) is the screen's.
    const glm::vec4 phosphor = palette().text;
    const gfx::Vertex c[4] = {{origin, {0.f, 0.f}, phosphor}, {origin + xAxis, {1.f, 0.f}, phosphor},
        {origin + xAxis + yAxis, {1.f, 1.f}, phosphor}, {origin + yAxis, {0.f, 1.f}, phosphor}};
    const gfx::Vertex quad[6] = {c[0], c[1], c[2], c[0], c[2], c[3]};

    const float time = static_cast<float>(za::fmod(realtime, 1000.0));
    const float k = crtStrength();
    gfx::draw(quad, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Screen, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = true,
            .params = {time, k, k > 0.f ? glitch(realtime) * za::min(k, 1.f) : 0.f, textGlow()},
            .screen = {width, height, 0.5f}, .trueColor = true},
        target.texture);
}

float textGlow()
{
    return CLAMP(0.f, vr_screen_text_glow.value, 3.f);
}

glm::vec3 whitened(const glm::vec3& colour, float whiteness)
{
    return glm::mix(colour, glm::vec3{0.97f}, CLAMP(0.f, whiteness, 1.f));
}

void useBrightFont(bool on)
{
    if(on && !brightFont.consoleFont)
    {
        brightFont.consoleFont = char_texture;
        char_texture = brightFontTexture(); // (Draw_StringEx binds char_texture)
    }
    else if(!on && brightFont.consoleFont)
    {
        char_texture = brightFont.consoleFont;
        brightFont.consoleFont = nullptr;
    }
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
    za::Vector<za::Pair<za::SizeT, float>>& picked = scratch.picked; // wrapped's line, its alpha
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
        if((hologram && line.game) || (!line.game && vr_hud_console_log.value == 0.f))
        {
            continue; // the hologram's; or the console's alone (vr_hud_console_log 0)
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
    for(const auto& [line, alpha] : za::reversed(picked)) // (views taken once `wrapped` is complete)
    {
        out.lines.pushBack(wrapped[line]);
        out.alpha.pushBack(alpha);
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
    if(portals::viewing() || !active() || (static_cast<int>(vr_shot_hide.value) & 1)) // (hidden for a clean shot)
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

// vr_gadget_screen_dump [name]: the screen's image as drawn this frame (before the CRT and the glow), flat, to screenshots/<name>.png (default gadget_screen), for tests.
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
namespace qvr::gadget
{
namespace
{
// Con_DrawNotify's line (`length` characters), plain (the coloured characters' high bit off, trailing spaces trimmed),
// in the scratch (one line at a time).
[[nodiscard]] const za::String& plainLine(const char* text, int length)
{
    za::String& plain = scratch.plain;
    plain.assign(text, static_cast<za::SizeT>(za::max(length, 0)));
    for(char& c : plain)
    {
        c = static_cast<char>(static_cast<unsigned char>(c) & 127);
    }
    while(!plain.empty() && plain.back() == ' ')
    {
        plain.popBack();
    }
    return plain;
}
} // namespace
} // namespace qvr::gadget

extern "C" int VR_GameLineOnWrist(const char* text, int length)
{
    using namespace qvr::gadget;
    if(!hologramOnly() || !text)
    {
        return 0;
    }
    const za::String& plain = plainLine(text, length);
    return !plain.empty() && !engineLine(plain);
}

// Con_DrawNotify, a line about to be drawn in view (or on the flat screen): nonzero to leave it to the console, with
// vr_hud_console_log 0 when it is not a game message (not a server's print, or the engine's own reply in one: the
// same split as the gadget's log, NotifyLine::game).
extern "C" int VR_ConsoleLogLine(const char* text, int length, int server)
{
    using namespace qvr;
    using namespace qvr::gadget;
    if(vr_hud_console_log.value != 0.f || !text)
    {
        return 0;
    }
    if(!server)
    {
        return 1;
    }
    const za::String& plain = plainLine(text, length);
    return !plain.empty() && engineLine(plain);
}

// vr_notify_info (console.c): the wrist gadget's log lines now, printed to the console alone.
extern "C" void VR_NotifyLogInfo()
{
    using namespace qvr::gadget;
    Log out;
    const bool shown = log(out);
    Con_Printf("[skipnotify]vr_notify_info: wrist log %d line(s)%s\n", shown ? static_cast<int>(out.lines.size()) : 0,
        logShown() ? "" : " (no log: the gadget not drawn, or vr_notify_wrist 0)");
    for(const za::StringView l : out.lines)
    {
        Con_Printf("[skipnotify]  wrist: %.*s\n", static_cast<int>(l.size()), l.data());
    }
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
