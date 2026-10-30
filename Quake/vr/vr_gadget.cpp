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

#include "vr_menuui.hpp"
#include "vr_gadget.hpp"
#include "vr_doom.hpp"
#include "vr_gearlights.hpp"
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
    PicAdArtifact = 33,  // AD support: Arcane Dimensions' artifact icons, its own files (STAT_QVR_ADHUD's bits 0-5)
    PicAdKey = 39,       // AD support: its custom keys' icons, by HUD skin (adib_ckey1 .. 14)
    PicCount = 53
};

constexpr const char* picNames[PicCount] = {"num_0", "num_1", "num_2", "num_3", "num_4", "num_5", "num_6", "num_7",
    "num_8", "num_9", "num_minus", "face5", "face4", "face3", "face2", "face1", "sb_armor1", "sb_armor2", "sb_armor3",
    "sb_shells", "sb_nails", "sb_rocket", "sb_cells", "sb_key1", "sb_key2", "sb_invis", "sb_invuln", "sb_suit", "sb_quad",
    "sb_sigil1", "sb_sigil2", "sb_sigil3", "sb_sigil4",
    "gfx/adib_lshield.lmp", "gfx/adib_atank.lmp", "gfx/adib_bbelt.lmp", "gfx/adib_jboots.lmp", "gfx/adib_sharp.lmp",
    "gfx/adib_nailp.lmp", "gfx/adib_ckey1.lmp", "gfx/adib_ckey2.lmp", "gfx/adib_ckey3.lmp", "gfx/adib_ckey4.lmp",
    "gfx/adib_ckey5.lmp", "gfx/adib_ckey6.lmp", "gfx/adib_ckey7.lmp", "gfx/adib_ckey8.lmp", "gfx/adib_ckey9.lmp",
    "gfx/adib_ckey10.lmp", "gfx/adib_ckey11.lmp", "gfx/adib_ckey12.lmp", "gfx/adib_ckey13.lmp", "gfx/adib_ckey14.lmp"};

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
        const qpic_t* p = nullptr;
        size_t bytes = 0;
        za::Vector<byte> file; // (a picture's own file: AD's icons)
        if(!strncmp(picNames[i], "gfx/", 4)) // AD support: Arcane Dimensions' icons, while its data is mounted
        {
            if(byte* buf = COM_LoadMallocFile(picNames[i], nullptr))
            {
                file.resize(static_cast<za::SizeT>(com_filesize));
                memcpy(file.data(), buf, file.size());
                free(buf);
                p = reinterpret_cast<const qpic_t*>(file.data());
                bytes = file.size();
            }
        }
        else
        {
            p = static_cast<const qpic_t*>(W_GetLumpName(picNames[i], &info));
            bytes = p && info && info->type == TYP_QPIC ? static_cast<size_t>(info->size) : 0;
        }
        if(!p || bytes < sizeof(int) * 2 || p->width <= 0 || p->height <= 0 || p->width > 64 || p->height > 64 ||
            bytes < sizeof(int) * 2 + static_cast<size_t>(p->width * p->height))
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

// Whether picture `id` can be drawn (AD's icons only while its data is there).
[[nodiscard]] bool picOk(int id)
{
    return id < PicAdArtifact || greyPic(id) != nullptr;
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
    const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f) * gearlights::screen(); // (dimmed: the gear lights)
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
    int stealth{-1}; // the stealth gem's (STAT_QVR_STEALTH): -1 none
    int adHud{0};    // AD support: Arcane Dimensions' artifacts, debuffs, custom keys (STAT_QVR_ADHUD), their skins, the
    int adKeys{0};   // soonest artifact's seconds left
    int adTime{0};
    int adArt[2]{0, 0}; // AD support: the AD page's: each artifact's seconds left (STAT_QVR_ADART1, ADART2: 8 bits each)
    int adDebuff{0};    // the poison and bleeding still to come, the burning's seconds (STAT_QVR_ADDEBUFF)
    int adUp{0};        // the carried weapons' upgrades (STAT_QVR_ADUPGRADES: 3 bits a weapon)
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
    r.stealth = cl.stats[protocol::STAT_QVR_STEALTH];
    r.adHud = cl.stats[protocol::STAT_QVR_ADHUD];
    r.adKeys = cl.stats[protocol::STAT_QVR_ADKEYS];
    r.adTime = cl.stats[protocol::STAT_QVR_ADTIME];
    r.adArt[0] = cl.stats[protocol::STAT_QVR_ADART1];
    r.adArt[1] = cl.stats[protocol::STAT_QVR_ADART2];
    r.adDebuff = cl.stats[protocol::STAT_QVR_ADDEBUFF];
    r.adUp = cl.stats[protocol::STAT_QVR_ADUPGRADES];

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
        case 6: // AD support: Arcane Dimensions' row: every artifact, the three debuffs, two custom keys, a timer
            r.adHud = 0x3f | 0x1c0 | 0x600;
            r.adKeys = 2 | (5 << 4);
            r.adTime = 27;
            // (its page: the artifacts' times, one for good; poison, bleeding, burning; a Shadow Axe in the main hand,
            // a Widowmaker in the right hip holster and another in the left shoulder's, a Plasma Gun in the off hand)
            r.adArt[0] = 27 | (255 << 8) | (61 << 16);
            r.adArt[1] = 12 | (45 << 8) | (90 << 16);
            r.adDebuff = 18 | (6 << 8) | (3 << 16);
            r.adUp = 1 | (4 << 3) | (2 << 15) | (2 << 6);
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

// The stealth AI's gem (vr_stealth_gem; QC vr_stealth.qc): a small diamond in the armour tile's top right corner, its
// glow how visible you are (dark: unseen in the shadows; bright: in full light, standing), a frame round it lit by how
// loud your last noise was (red when loud: running, a shot).
void stealthGem(const Readout& r, const Palette& pal)
{
    if(r.stealth < 0 || !vr_stealth_gem.value)
    {
        return;
    }
    const float vis = za::clamp(static_cast<float>(r.stealth % 1000) / 100.f, 0.f, 1.f);
    const float loud = za::clamp(static_cast<float>(r.stealth / 1000) / 100.f, 0.f, 1.f);
    constexpr float size = 10.f;
    const float x = tileX[1] + tileWidth - size - 2.f, y = tileY[0];
    if(loud > 0.02f)
    {
        meterFrame(x - 2.f, y - 2.f, size + 4.f, size + 4.f, loud > 0.5f ? pal.warn * (0.4f + 0.6f * loud) : pal.value * loud);
    }
    const glm::vec3 glow = pal.dim * 0.35f + (pal.value - pal.dim * 0.35f) * vis;
    for(int i = 0; i < static_cast<int>(size); i++)
    {
        const float w = 2.f * za::round(size * 0.5f - za::abs(static_cast<float>(i) + 0.5f - size * 0.5f));
        if(w > 0.f)
        {
            fill(x + (size - w) * 0.5f, y + static_cast<float>(i), w, 1.f, glow);
        }
    }
}

// Bullet time's meter (vr_bullettime.cpp): "TIME" and a bar, lit for what's left. Running: the label white, the bar
// drains and its frame blinks; cooling down: only its frame, dim; not enough to start: the bar dim. Earned
// (vr_bullettime_meter_mode 1): the bar shows while cooling too (it fills by damage dealt), and its frame lights a moment
// as a hit fills it.
void timeRow(const Readout& r, const Palette& pal)
{
    const bullettime::Meter& m = r.time;
    if(!m.enabled)
    {
        return;
    }
    const float y = rowY[1] - 1.f, h = 9.f, w = meterRight - meterX;
    drawText(labelX, rowY[1], 8.f, "TIME", m.active ? pal.value : pal.label, pal);
    const glm::vec3 frame = m.active ? (blinkOn() ? pal.value : pal.line)
                          : m.gained ? pal.value
                          : m.cooling ? pal.dim * 0.6f : pal.line;
    meterFrame(meterX, y, w, h, frame);
    if(!m.cooling || m.earned)
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
    const auto columnsLeftOf = [](float x) { return za::max(0, static_cast<int>((x - 4.f - margin) / 8.f)); };

    // What is shown at the right, left to right: the keys, powerups and sigils; then (AD support) Arcane Dimensions'
    // artifacts (AD's icons), the soonest one's seconds left, its custom keys (by HUD skin) and its debuffs (blinking).
    struct Entry
    {
        int pic;          // a picture, or -1: the text
        const char* text;
        glm::vec3 rgb;
    };
    za::Array<Entry, 32> entries{};
    int count = 0;
    const auto add = [&](int pic, const char* text, const glm::vec3& rgb) {
        if(count < 32) { entries[count++] = Entry{pic, text, rgb}; }
    };
    for(int i = 0; i < 10; i++)
    {
        if(r.items & itemBits[i]) { add(itemPic(i), nullptr, i >= 2 && i < 6 ? pal.value : pal.label); } // (powerups in use: white)
    }
    char adTime[8] = "";
    for(int i = 0; i < 6; i++)
    {
        if((r.adHud & (1 << i)) && picOk(PicAdArtifact + i)) { add(PicAdArtifact + i, nullptr, pal.value); }
    }
    if(r.adTime > 0 && (r.adHud & 0x3f))
    {
        q_snprintf(adTime, sizeof(adTime), "%d", za::min(r.adTime, 999));
        add(-1, adTime, pal.value);
    }
    for(int k = 0; k < 4; k++)
    {
        const int pic = PicAdKey + CLAMP(0, (r.adKeys >> (4 * k)) & 15, 13);
        if((r.adHud & (512 << k)) && picOk(pic)) { add(pic, nullptr, pal.label); }
    }
    constexpr const char* debuffs[3] = {"BURN", "POISON", "BLEED"};
    for(int d = 0; d < 3; d++)
    {
        if(r.adHud & (64 << d)) { add(-1, debuffs[d], blinking(pal.warn)); }
    }
    const auto entryWidth = [&](const Entry& e) {
        return e.pic >= 0 ? (greyPic(e.pic) ? static_cast<float>(greyPic(e.pic)->width) : 16.f)
                          : static_cast<float>(strlen(e.text)) * 8.f;
    };

    float itemsW = 0.f; // at full size, with their gaps
    for(int i = 0; i < count; i++)
    {
        itemsW += entryWidth(entries[i]) + 2.f;
    }

    char stats[64], shortStats[64];
    q_snprintf(stats, sizeof(stats), "KILLS %d/%d  SECRETS %d/%d", r.kills, r.totalKills, r.secrets, r.totalSecrets);
    q_snprintf(shortStats, sizeof(shortStats), "K %d/%d  S %d/%d", r.kills, r.totalKills, r.secrets, r.totalSecrets);
    const int fullColumns = columnsLeftOf(meterRight - itemsW);
    const bool small = itemsW > 0.f && static_cast<int>(strlen(shortStats)) > fullColumns &&
                       (r.relight || vr_gadget_show_level.value);
    const float scale = small ? 0.5f : 1.f;

    float x = meterRight;
    for(int i = count - 1; i >= 0; i--)
    {
        const Entry& e = entries[i];
        x -= entryWidth(e) * scale;
        if(e.pic >= 0)
        {
            drawPic(x, small ? infoY[0] : infoY[0] + 1.f, e.pic, scale, e.rgb, pal);
        }
        else
        {
            drawText(x, small ? infoY[0] + 1.f : infoY[0] + 5.f, 8.f * scale, e.text, e.rgb, pal);
        }
        x -= small ? 1.f : 2.f;
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

#include "vr_ad_gadget.inc" // AD support: the Arcane Dimensions page (adPage, adPageShown, adTogglePage)

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
    if(adPageShown()) { adPage(r, pal); return; } // AD support: the Arcane Dimensions page in its place (togglePage)

    // Health (the face its icon; red under 25) and armour.
    const int face = r.health >= 100 ? 4 : CLAMP(0, r.health / 20, 4);
    tile(0, 0, "HEALTH", PicFace + face, r.health, false, r.health < 25 ? blinking(pal.warn) : pal.value, pal);
    tile(1, 0, "ARMOUR", r.armorType >= 0 ? PicArmor + r.armorType : -1, r.armor, false, r.armor > 0 ? pal.value : pal.dim, pal);
    stealthGem(r, pal);

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
    const float k = vr_gadget_light.value * gearlights::light(); // (dimmed: the gear lights)
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
    return vr_notify_wrist.value != 0.f && active() && current.valid && !recordingClean(); // (recording mode: none)
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

#include "vr_gadget_holo.inc"
#include "vr_gadget_fps.inc"

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

void tapFeedback(bool activation)
{
    const float volume = za::clamp(vr_bullettime_tap_sound.value, 0.f, 1.f);
    const char* played = "none";
    if(volume > 0.f && current.valid)
    {
        if(sfx_t* sfx = S_PrecacheSound(activation ? "vr/gadget_tap_on.wav" : "vr/gadget_tap.wav"))
        {
            const glm::vec3 c = screenCentre();
            vec3_t org{c.x, c.y, c.z};
            S_StartSound(tapEntity, 1, sfx, org, volume, 1.f); // (channel 1: a click cuts the one before)
            tapFx.soundAt = realtime;
            played = sfx->name;
        }
    }
    const float strength = za::clamp(vr_bullettime_tap_glitch.value, 0.f, 2.f) * (activation ? 1.f : 0.7f);
    const float length = za::max(0.f, vr_bullettime_tap_glitch_time.value) * (activation ? 1.25f : 1.f);
    if(strength > 0.f && length > 0.f)
    {
        tapFx.glitchAt = realtime;
        tapFx.glitchStrength = strength;
        tapFx.glitchLength = length;
        tapFx.glitchFrames = 0;
    }
    if(vr_debug_bullettime.value)
    {
        Con_Printf("gadget: tap feedback (%s): click %s at %.2f, glitch %.2f for %.2f s\n",
            activation ? "activation" : "first tap", played, volume, strength, strength > 0.f ? length : 0.f);
    }
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

    // The chime and a tap's click stay at the gadget as the arm moves; the buzz's second pulse.
    if((realtime - lastChime < 3.0 || realtime - tapFx.soundAt < 1.0) && pose.valid)
    {
        const glm::vec3 c = screenCentre();
        S_MixJoin(); // (the pool's mix done first: S_MixKick)
        for(int i = NUM_AMBIENTS; i < total_channels && i < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; i++)
        {
            channel_t& ch = snd_channels[i];
            if(ch.sfx && (ch.entnum == chimeEntity || ch.entnum == tapEntity))
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
        haptics::pulse(gadgetHand(), 0.05f, 0.4f, 160.f);
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

bool notice(za::StringView text, float seconds)
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
    m.life = za::max(seconds, 1.f); // (not a notification: no waiting, no chime; the same text again keeps it on)
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
        qvr::menuui::uiSound(qvr::menuui::UiSound::Talk);
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
    doom::renderScreen(); // Doom's picture and its touch controls (vr_doom), while it is open and can be seen
    if(!active())
    {
        return;
    }

    if(!doom::onGadget()) // (Doom shows instead: the HUD isn't drawn)
    {
        gfx::ensureTarget(target, width * 2, height * 2, true, "gadget screen"); // mipmaps: for its text's glow
        gfx::begin2D(target, width, height);
        layout();
        gfx::end2D();
    }
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

bool togglePage()
{
    return adTogglePage(); // AD support (vr_ad_gadget.inc)
}

int page()
{
    return adPageShown() ? 1 : 0; // AD support (vr_ad_gadget.inc)
}

void drawScreen()
{
    doom::drawPanel(); // (vr_doom_panel: Doom's floating panel)
    // Doom open on the gadget (vr_doom): its picture instead of the HUD's.
    const gfx::Texture doomTexture = doom::onGadget() ? doom::gadgetTexture() : 0;
    if(!active() || !current.valid || (!target.texture && !doomTexture))
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
    // A screen tap's glitch over the CRT's own (whether the CRT look is on or not).
    const float tap = tapGlitch();
    if(tap > 0.f)
    {
        tapFx.glitchFrames++;
    }
    else if(tapFx.glitchFrames > 0)
    {
        if(vr_debug_bullettime.value)
        {
            Con_Printf("gadget: tap glitch drawn in %d frames (both eyes)\n", tapFx.glitchFrames);
        }
        tapFx.glitchFrames = 0;
    }
    const float g = za::max(k > 0.f ? glitch(realtime) * za::min(k, 1.f) : 0.f, tap);
    if(doomTexture)
    {
        // Doom's: its own CRT strength (vr_doom_crt, half the HUD's: a picture, not text), a scanline a pixel, no glow.
        const float dk = za::clamp(k * vr_doom_crt.value * 0.5f, 0.f, 2.f);
        gfx::draw(quad, gfx::sceneViewProjection(),
            {.shade = gfx::Shade::Screen, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = true,
                .params = {time, dk, za::min(g, tap), 0.f},
                .screen = {200.f * size.x / size.y, 200.f, 1.f}, .trueColor = true},
            doomTexture);
        return;
    }
    gfx::draw(quad, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Screen, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = true,
            .params = {time, k, g, textGlow()},
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
    const float k = CLAMP(0.f, vr_screen_glow.value, 3.f) * gearlights::light(); // (dimmed: the gear lights)
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
    for(const HoloMessage& m : queue) // (the hologram's messages up now: tests)
    {
        if(alive(m))
        {
            Con_Printf("gadget: hologram message (%.1f s left%s) \"%s\"\n",
                lifeOf(m) - static_cast<float>(realtime - m.start), m.notify ? ", notification" : "", m.text.cStr());
        }
    }
}

// vr_gadget_screen_dump [name]: the screen's image as drawn this frame (before the CRT and the glow), flat, to screenshots/<name>.png (default gadget_screen), for tests.
void screenDump_f()
{
    // (Doom's picture and its touch controls while Doom shows: vr_doom.)
    const gfx::Target& target = doom::onGadget() && doom::gadgetTexture() ? doom::screenTarget() : gadget::target;
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
