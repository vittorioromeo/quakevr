// vr_bindsetup.hpp -- the Controller Bindings screen (ROUND26.md "ctrlbind"): the controller in use (the headset's
// controllers by their OpenXR interaction profile and the headset's name, a flat screen's gamepad by SDL's controller
// type) drawn with each button labelled by what it is bound to; a label dragged onto another swaps the two bindings, a
// click picks the button's action from a list of every action. One screen for VR (the laser) and flat (the mouse, the
// pad), drawn by the menus' 2D pass (m_bindsetup, menu.c). It edits Quake's binds (the gamepad keys the controllers send:
// vr_input.cpp, in_sdl.c), so configs keep working. Pictures and anchors: Misc/quakevr/make_controller_art.py.
// TODO(splitscreen): per-seat bindings (BACKLOG.md section 3): with true seats each seat's controller gets its own page.

#pragma once

namespace qvr::bindsetup
{

// The controllers the screen knows (each has a picture: several share one).
enum class Model : unsigned char
{
    Quest3,
    Quest3S,
    Quest2,
    Quest1, // Quest 1 and Rift S: Touch's second generation (Quest 2's picture)
    QuestPro,
    RiftCV1,
    Touch, // Touch controllers on an unknown headset (Quest 3's picture)
    Index,
    Vive,
    Wmr,
    ReverbG2,
    Pico,
    ViveCosmos, // Cosmos and Focus 3 (Touch-shaped)
    Frame,
    SimpleVr,
    Xbox360,
    XboxOne,
    PlayStation,
    SwitchPro,
    GenericPad,
    Keyboard, // no gamepad: the keyboard's bindings are Options > Controls
    Count
};

[[nodiscard]] const char* modelId(Model m);   // "quest3", the menu_bindsetup argument
[[nodiscard]] const char* modelName(Model m); // "Meta Quest 3"
[[nodiscard]] bool modelFromId(const char* id, Model& out);

// Detection: the headset's controllers from an interaction profile ("/interaction_profiles/oculus/touch_controller" or
// "" if none yet) and the headset's name (OpenXR's systemName); a gamepad from SDL's controller type (SDL_GameControllerType;
// -1: no gamepad).
[[nodiscard]] Model modelForProfile(const char* profile, const char* systemName);
[[nodiscard]] Model modelForPad(int sdlType);
// The controller in use now: the headset's while VR runs, else the gamepad's, else Keyboard.
[[nodiscard]] Model detect();

// Opens the screen (from Options > Controller Bindings, the VR menu, the main menu's bindings panel's Setup Bindings:
// vr_bindpanel.cpp): `model` shown instead of the detected one when not Count. False when it did not open (keyboard and
// mouse: no controller in use): the caller opens the keyboard's page instead (VR_BindSetup_Open: Options > Controls).
[[nodiscard]] bool open(Model model = Model::Count);

// The reminder panels' data (vr_bindpanel.cpp; ROUND26 "bindpanel2"): a controller's small pictures (a VR pair, the left
// hand first, or a gamepad) and each of its buttons as this screen labels it, from the same tables and binds, so the
// panels show what the screen edits. Keyboard: none (0).
struct PanelButton
{
    int picture{0};             // 0 the left hand or the pad, 1 the right hand
    float u{0.f}, v{0.f};       // its anchor on that picture (0..1)
    const char* name{""};       // "Trigger", "A"
    const char* glyphName{""};  // its glyph's file (gfx/vr/glyphs/<glyphName>)
    const void* glyph{nullptr}; // that glyph loaded (a qpic_t, for Draw_SubPic; nullptr: none)
    const char* action{""};     // its binding's name ("Attack"; "" unbound)
    bool fixed{false};          // the menu button (never rebound)
};
struct PanelPictures
{
    const void* pic[2]{nullptr, nullptr}; // qpic_t (nullptr: not found)
    float aspect[2]{1.f, 1.f};            // height / width
    int count{0};
};
// Its buttons into `out` (up to max): how many.
[[nodiscard]] int panelButtons(Model m, PanelPictures& pictures, PanelButton* out, int max);

void registerCommands();

} // namespace qvr::bindsetup
