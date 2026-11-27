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

// Opens the screen (from Options > Controller Bindings, the VR menu, the main menu's Setup Bindings): `model` shown
// instead of the detected one when not Count. Keyboard opens Options > Controls (the keyboard's tab).
void open(Model model = Model::Count);

void registerCommands();

} // namespace qvr::bindsetup
