// vr_gadget.hpp -- the wrist gadget (vr_hud_mode 1): a device strapped over the back of the off
// hand's forearm (progs/vrgadget.mdl, from Misc/quakevr/make_gadget.py) whose screen shows the
// HUD: the player's face, health and armour, ammo, keys, powerups and sigils, the level, kills
// and secrets. Raise the wrist and turn it to read it, like a watch. Its screen is a small
// CRT (vr_gadget_crt) in the player's colour with near-white values that glows (vr_screen_glow) and casts a light in its colour the
// way it faces (vr_gadget_light), and the console's messages float over it in a small log facing
// the player (vr_notify_wrist), rather than at the edge of the view; the game's messages (a key
// needed, a pickup) it projects as a hologram just over its screen (vr_messages_hologram).

#pragma once

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/StringView.hpp"

#include <glm/glm.hpp>


namespace qvr::gadget
{

struct Pose
{
    bool valid{false};
    glm::vec3 origin{0.f};
    glm::mat3 axes{1.f}; // the screen's right, up and out
    float scale{1.f};    // world units per model unit
};

// Whether the HUD is the gadget's (vr_hud_mode 1, in the headset, in game).
[[nodiscard]] bool active();

// AD support: the screen's pages (the gadget's outer button, vr_gearlights.cpp; vr_gadget_page): the HUD, and Arcane
// Dimensions' page (its artifacts with their times, keys, debuffs and the carried weapons' upgrades) while its data is
// loaded. togglePage: to the other one; false (the HUD kept) without AD's data. page: the one shown (0 the HUD, 1 AD's).
bool togglePage();
[[nodiscard]] int page();

// Set by the view every frame (before each eye's scene). It also places the screen's lights.
void setPose(const Pose& pose);
[[nodiscard]] const Pose& pose();

// The screen's corners (in model space: the lower-left one, and its extent along x and y), where
// its texture is drawn over the model's screen.
void screenRect(glm::vec3& corner, glm::vec2& size);

// The game directory changed (VR_OnGameDirChanged): the screen's grey pictures are made again from its gfx.wad.
void onGameDirChanged();

// Draws the screen's contents into its texture, at the end of the 2D pass (and the weapons' ammo
// screens' into theirs: text3d::renderScreens).
void renderScreen();

// The hologram's and the frame rate's images drawn so far (each drawn again only when its text or its font's style
// changes: vr_fontstyle_live_test).
[[nodiscard]] int imageDrawCount();

// Panel boards (worldtext::isPanel: the Survival Arena's banners): a map's board laid out by this screen's code, in its
// palette, font and pictures. One image for all of them, as they all say the same: drawn at the end of the 2D pass when
// its text or look changes (text3d::renderScreens), in the world by vr_text3d's boards through Shade::Screen.
void renderPanelBoard(const char* text);
void releasePanelBoard(); // (no board wants it: its image freed)
[[nodiscard]] unsigned panelBoardTexture(); // 0: none yet
[[nodiscard]] glm::vec3 panelBoardPhosphor(); // the screen's colour at full strength, for the board's glow and the static
// The tests' look at the image (QC panelboard_read), a part of it in the panel's pixels. `what` 0: how much of it is inked
// (0..1); 1: its checksum (24 bits); 2: 1 while there is an image at all (any text, no part); 3: its ink's red over its
// green (0..1). -1 when the image doesn't hold `text` (yet: it is drawn in a frame or two).
// The arena's messages on the wrist (arenaMessage), `what` 4..8 (x, y, w, h unused): 4 how much of the hologram image's
// block for the arena message `text` (as sent; "": the last one) is inked (0..1; -1 not the last one, or not in the image yet); 5 the frames
// it has been shown so far, facing the viewer (-1 not the last one); 6 the arena messages taken by the wrist; 7 those shown
// in view instead (worldtext::arenaInViewCount); 8 the buzzes they gave.
[[nodiscard]] float panelBoardRead(const char* text, float x, float y, float w, float h, int what);

// How much a CRT screen glitches at `time` (0..1, the CRT look's): now and then a short burst.
// The ammo screens' offset their time, so that they glitch at other moments.
[[nodiscard]] float glitch(double time);

// A screen tap registered (vr_bullettime.cpp tapScreen): the click from the screen (vr_bullettime_tap_sound; a double
// tap's first, or the tap that starts or stops bullet time: `activation`) and the screen glitching for a moment
// (vr_bullettime_tap_glitch, _glitch_time).
void tapFeedback(bool activation);

// How much the text, numbers and icons on the CRT screens (the gadget's and the ammo screens') glow
// (vr_screen_text_glow, 0..3; Shade::Screen's glow).
[[nodiscard]] float textGlow();

// The screens' text towards white (the gadget's values: vr_gadget_screen_text_white; the ammo screens' and the pouch
// counter's: vr_ammo_screen_text_white): `colour` mixed with near-white by `whiteness` (clamped 0..1).
[[nodiscard]] glm::vec3 whitened(const glm::vec3& colour, float whiteness);

// While on, gfx::draw2D::text and gfx::fontTexture() draw in the screens' font: the console's brightened, so that
// near-white text comes out near-white, not mid-grey. Off: the console's again.
void useBrightFont(bool on);

// The flat-screen visor's panels (vr_visor.cpp; vr_gadget_visor.inc): the HUD in the screen's own palette, font, numbers
// and icons (and its made-up readings, vr_gadget_test_state) in seven small images, drawn at the end of the 2D pass
// (renderVisor) on a black face (Shade::Screen's inkAlpha leaves it clear: the mount behind is the face) and shown by
// vr_visor.cpp inside mounts on the screen's edges. Kept here so that a headset's visor could take the same images later.
enum VisorPanel : int
{
    VisorVitals,     // health and armour (top left)
    VisorStatus,     // bullet time's and stamina's bars, the ammo carried in a grid (top right)
    VisorLeftAmmo,   // the left hand's weapon's ammo (bottom left; HAND_OFF is the left hand)
    VisorRightAmmo,  // the right hand's (bottom right)
    VisorLog,        // the console's notify lines (low in the middle)
    VisorSideLeft,   // keys, sigils, AD's custom keys (the left edge's middle)
    VisorSideRight,  // powerups with their seconds, AD's artifacts and debuffs (the right edge's middle)
    VisorPanelCount
};
struct VisorInput
{
    float boot[VisorPanelCount]{1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f}; // each panel's power-on sweep (0 .. 1: done)
    float alarm{0.f};                                                // the health panel's low-health pulse now (0 .. 1)
    bool log{false};                                                 // the notify lines in the low panel (vr_flat_visor_log)
};
struct VisorImage
{
    unsigned texture{0};
    glm::vec2 size{0.f}; // its virtual screen, pixels
    bool shown{false};   // drawn (its contents: an empty hand's panel, an empty side mount's, are drawn empty)
};
// The mounts' colours, the screens' own: the face, its frame line, the warning red, the values.
struct VisorColors
{
    glm::vec3 face{0.f};
    glm::vec3 line{0.f};
    glm::vec3 warn{0.f};
    glm::vec3 value{0.f};
};
void renderVisor(const VisorInput& in);
[[nodiscard]] VisorImage visorImage(int panel);
[[nodiscard]] glm::vec4 visorPhosphor(); // the screens' colour at full strength (Shade::Screen's static)
[[nodiscard]] VisorColors visorColors();
[[nodiscard]] int visorLogLines();       // the notify lines the low panel shows

// Draws the texture over the model's screen, in one phosphor colour and as a small CRT
// (vr_gadget_crt: scanlines, a flicker, faint static and now and then a glitch), in each eye's
// scene after the opaque entities (VR_DrawSceneOpaque), where the bloom catches its light.
void drawScreen();

// A soft glow round a screen's edge (vr_screen_glow): the screen's centre, right and up (unit
// vectors), its half width and height, how far out the glow reaches, its colour and strength.
struct Glow
{
    glm::vec3 centre{0.f};
    glm::vec3 right{1.f, 0.f, 0.f};
    glm::vec3 up{0.f, 1.f, 0.f};
    glm::vec2 halfSize{0.f};
    float spread{0.f};
    glm::vec4 color{0.f};
};

// The gadget screen's glow, drawn by vr_text3d with the weapons' ammo screens'; false when there
// is none.
[[nodiscard]] bool screenGlow(Glow& out);

// The log floating over the gadget (vr_notify_wrist): the console's newest notify lines, wrapped
// to fit, oldest first, each with how faded in it is (1 .. 0); drawn by vr_text3d, only while the
// screen is turned towards the viewer (like a watch, checked by raising the wrist).
struct Log
{
    za::Vector<za::StringView> lines; // the gadget's own text, valid until the next log()
    za::Vector<float> alpha;
    glm::vec3 base{0.f}; // the screen's centre: the log's bottom edge is `lift` above it (the view's up)
    float lift{0.f};
    glm::vec3 normal{0.f}; // out of the screen
    float charSize{0.f}; // units
    glm::vec3 color{1.f}; // the screen's text colour
    glm::vec3 backColor{0.f};
};

// False when there is nothing to show.
[[nodiscard]] bool log(Log& out);

// The game's messages as a hologram the gadget projects over its screen (vr_messages_hologram), apart
// from the log, which keeps the engine's lines (diagnostics: dimmer, and higher, over the hologram).
// Game messages: the centre prints (a key needed, a secret found, the map's text: SCR_CenterPrint, but
// not the intermission's) and the server's prints (svc_print: pickups, deaths, chat), less the engine's
// own replies among those ("godmode ON", setpos's figures, the server's banner). A few seconds each
// (vr_messages_hologram_time), the newest nearest the screen, facing the viewer, in the screen's colour,
// on a faint beam of light from the screen, with a hologram's scanlines, flicker and glitches; it is
// projected (grows out of the screen) as it appears. Only while the screen faces the viewer and is in
// view: else the centre prints show in front of the head as before (VR_CenterPrintOnWrist). Their text
// is drawn into a small image when the messages change (renderScreen), and the hologram in each eye's
// translucent pass (text3d::drawTranslucent), laid out once a frame.
void drawHologram();

// A tip (vr_tips.cpp, vr_tips 2): `text` as one of the hologram's messages, waiting until the gadget is seen (a chime from
// it and a buzz of its hand meanwhile, as vr_messages_hologram_only's notifications), then shown `seconds`. False when
// there is no hologram (the HUD not the gadget, vr_messages_hologram off): shown another way then.
bool tip(za::StringView text, float seconds);

// A notice about something you just did (bullet time pressed with its meter empty, vr_bullettime.cpp): `text` as one of
// the hologram's messages now, for `seconds`, seen if you look at the gadget then; not a notification (no waiting to be
// seen, no chime nor buzz). False when there is no hologram.
bool notice(za::StringView text, float seconds);

// A Survival Arena message (vr_arena_wrist: a wave's start and clear, the countdown, a perk taken, the run's end; QC
// VR_ARENA_Show): `text` as one of the hologram's messages now, for `seconds` (its first line, up to a blank line, its
// heading), in place of the arena's last one (the countdown's seconds replace each other, not stacking). Not held until
// seen (it is of the moment: the banners and the cards keep what matters), but one arriving while the gadget is out of
// view, the arena's last one gone, buzzes its hand once (a watch's buzz) so that you look. False when there is no
// hologram (the HUD not the gadget, vr_messages_hologram off, recording mode): it shows in view then.
bool arenaMessage(za::StringView text, float seconds);

// QVR round 21 (the Screens page's Show a Test Message): the next of a few of the game's own messages (a key needed,
// maps' texts, a secret, a powerup running out), sent as the game sends them, so that pressing again stacks them; with
// a menu open too. False when there is no hologram (no game, or it is off).
bool testMessage();

// With vr_messages_hologram_only (round 21) the game's messages never show in front of the head: they wait in the
// hologram until it is seen (a notification's life starts then; five minutes at most), and a new notification (a
// centre print, or a server's print other than a pickup) arriving while the gadget is out of view chimes from the
// gadget (misc/talk.wav, the game's own message sound moved there: VR_GameSound) and buzzes its hand twice.

} // namespace qvr::gadget
