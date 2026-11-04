// vr_weaponstudio_internal.hpp -- the Weapon Studio's shared internals: the key table (every weapons::Key, its page,
// how it is best shown and its comfortable range), the studio's state, and what its two halves share
// (vr_weaponstudio.cpp and vr_weaponstudio_ui.cpp). See vr_weaponstudio.hpp.

#pragma once

#include "vr_weaponstudio.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/InPlaceVector.hpp"

#include <glm/glm.hpp>

namespace qvr::weaponstudio
{

// Degrees and radians (the store holds the angles in degrees; the maths is done in radians).
inline constexpr float degToRad = 0.0174532925f;
inline constexpr float radToDeg = 57.29578f;

namespace detail
{

// A key's page (the Weapon Offsets menu's pages, the Weapon Weights page's, and the keys the menus keep apart).
enum class Group : int
{
    Placement,  // where the weapon sits: its offsets, whole-weapon and hand-only posing, scale, length, grip overlap
    Fingers,    // the fingers on it: opens, curls, places, biases
    Muzzle,     // the muzzle and sights, the shot's angles, the flatscreen aim
    TwoHanded,  // the two-handed grips and their helping hand
    Hotspots,   // the four grip spots, their own posing
    Stock,      // the virtual stock
    Ammo,       // the ammo screen: its button and its text
    Holstered,  // how it hangs in a hip, upper or shoulder holster
    Effects,    // recoil, flash, tracers, muzzle effects
    Flashlight, // the torch and its clip
    Weights,    // mass, springs, damage, throwing (the Weapon Weights page's keys)
    Blade,      // a blade's shape
    Reload,     // the loading port, magazine and well
    Embedded,   // a melee weapon embedded in what it hit
    Identity,   // the model's name, and which slot it inherits from
    Other,      // (the rules missed it: still shown and editable, and vr_weaponstudio_test says so)
    Count
};

// How a key is best shown.
enum class Kind : int
{
    Length,  // Quake units
    Angle,   // degrees
    Share,   // 0..1
    Scale,   // a multiplier
    Spring,  // the weights' multipliers (0..20 like the menu's spring sliders)
    Speed,   // units/s
    Seconds, // s
    Choice,  // a mode, a type, a switch: a few values
    Anchor,  // vr_anchor.hpp's text (the console and the config reach it by the same name)
    Count
};

// A key as the window shows it. One per weapons::Key, in the .inc's order, so an entry's place is the key's number.
struct Entry
{
    weapons::Key key{weapons::Key::Count};
    Group group{Group::Other};
    Kind kind{Kind::Length};
    int hotspot{0}; // a hotspot key's number (1..4), 0: not one
    int holster{0}; // a holster key's kind (weapons::HolsterKind), -1: not one
    char name[40]{""};  // the data name: vr_wofs_<name>_NN without the wrapper
    char label[52]{""}; // the same spaced out and shorted of what the page's header already says
    float lo{0.f};
    float hi{0.f};
    float step{0.05f};
};

extern Entry tableEntries[static_cast<int>(weapons::Key::Count)];
extern int tableSize; // keys the table holds (its own count: the test checks it against weapons::Key::Count)

void buildTable();
[[nodiscard]] const Entry* entryOf(weapons::Key key);
[[nodiscard]] int countInGroup(Group g);
[[nodiscard]] int uncovered(); // keys left in Other
const char* groupLabel(Group g);
const char* kindLabel(Kind k);
// A mode/switch key's values named as the game's own menus name them (nullptr: show the number).
[[nodiscard]] const char* const* choiceNamesFor(const Entry& e);
[[nodiscard]] bool matches(const Entry& e, const char* filter);

// A key's value written the way the menus write it: through its stand-in cvar for that slot.
bool writeValue(const Entry& e, int slot, float value);
bool writeText(const Entry& e, int slot, const char* text);

// Which keys this window changed since it opened (its "changed" readout: the store marks no edit of its own).
void markTouched(int slot, weapons::Key key, float before);
[[nodiscard]] bool touched(int slot, weapons::Key key);
[[nodiscard]] float savedValue(int slot, weapons::Key key); // what it held before this window first changed it
[[nodiscard]] int touchedCount(int slot);
void clearTouched();
void clearTouched(int slot);
} // namespace detail

inline constexpr int maxSpots = 1200;
inline constexpr int maxMarkers = 24;

// A widget of the window the tests' mouse can find (what the Arena Wave Editor's tests do).
struct Spot
{
    char name[40]{""};
    float x{0.f};
    float y{0.f};
};

// A world line or point to draw over the game's own view (its ends are world points, Quake units; r, g, bl its colour).
struct Marker
{
    glm::vec3 a{0.f};
    glm::vec3 b{0.f};
    float r{1.f};
    float g{1.f};
    float bl{1.f};
    bool point{false}; // a: this one dot (b unused)
    char what[20]{""};
};

// One hand's held weapon, measured (0: the off hand, 1: the main hand).
struct Readout
{
    bool held{false};
    int slot{-1};
    char model[48]{""};
    bool sights{false};    // the model has sights to line up (sightalign::sightLine)
    bool geometry{false};  // its own muzzle and butt found (view::weaponLocalGeometry)
    float sightToShot{0.f};  // degrees: the sight line to the direction the gun shoots (weapons::shotAngles)
    float barrelToShot{0.f}; // degrees: the muzzle-to-butt line to that direction
    float muzzleGapCm{0.f};  // the muzzle it shoots from, to the sight line
    float realMuzzleCm{0.f}; // that muzzle, to the model's own muzzle
    bool fist{false};       // your hand's fist sits on it through the shipped rig (view::weaponFrame)
    bool alignRan{false};
};

// The preview humanoid: an armed monster spawned frozen beside the player, like the Rig Studio's subject.
struct Preview
{
    bool made{false};
    int ent{0}; // its edict number
    int kind{0};
    int code{0};  // its main hand's weapon code (toolgun::armed)
    int code2{0}; // its off hand's (0: none)
    int slot{-1};  // the weapons' slots its guns draw with, from the arms solve's pose note
    int slotL{-1};
    float yaw{0.f};   // the yaw it is held at (it stands facing the player)
    char pose[64]{""};
    int noteFrame{-1};
};

// The smallest the window may be made, and the largest it is offered (its own frame, in vr_weaponstudio_ui.cpp; the
// self-test checks it). At this size its top row -- the slot, "my main hand", "my off hand" and `close` -- is still
// one line, and a key row's label, its drag and its box are still together on one line, with `close` reachable.
inline constexpr float winMinW = 560.f;
inline constexpr float winMinH = 360.f;

struct State
{
    bool open{false};
    bool hold{true};      // the game's physics held while the window is open
    bool askClose{false}; // closed only after ImGui::End (ROUND23.md: the wave editor's Play and Close crashed)
    bool showMarkers{true};
    bool showCrosshair{true};
    int slot{0};          // the slot being edited
    detail::Group group{detail::Group::Placement};
    int hotspot{1};       // which hotspot the Hotspots page shows (1..4)
    int holster{0};       // which holster the Holstered page shows (weapons::HolsterKind)
    char filter[40]{""};  // a key's name or part of one: only the rows matching it are shown
    int copyFromSlot{-1};   // "Copy into this slot from slot ...": the other slot's number
    int previewKind{0};
    int previewCode{4};   // the armed list's codes (0: random from its pool, -1: unarmed)
    int previewCode2{0};  // 0: none
    bool wantMake{false}; // asked for the window's own buttons, done after its End (a QC spawn is not done mid-window)
    bool wantRemove{false};
    bool wantReset{false};  // and the store's own work: reset, print, copy, stop copying
    bool wantPrint{false};
    bool wantCopy{false};
    bool wantStopCopy{false};
    // The self-test: what it puts back at its end (the hold, the window's place and size), and its copy step's slot
    // (the source; what the target held; its model's name; the hand offset a copy has to leave).
    bool testHold{true};
    bool testWinPlaced{false};
    float testWinX{0.f};
    float testWinY{0.f};
    float testWinW{0.f};
    float testWinH{0.f};
    int testCopyFrom{-1};
    float testCopyWas[static_cast<int>(weapons::Key::Count)]{};
    float testCopyInherit{0.f};
    float testCopyHand{0.f};
    char testCopyId[64]{""};
    bool wantAlign{false};
    bool alignApply{false};
    int alignHand{2}; // 0 off, 1 main, 2 both
    za::InPlaceVector<Spot, maxSpots> spots;
    za::InPlaceVector<Marker, maxMarkers> markers;
    Readout hand[2];
    Preview preview;
    int rowsShown{0}; // the key rows the last frame drew (the filter's test)
    // The tests' mouse and counts (vr_weaponstudio_test).
    bool testing{false};
    bool testClosing{false}; // the last step asked for Close: the verdict comes once the window is closed
    int testStep{0};
    int testWait{0};
    int closeTries{0}; // (the Close click and its retries: a stuck test still ends with a verdict line)
    int poseFrames{0};  // (the hold preview's wait for the arms solve's note: counted frames, ended by the note)
    int clickOffscreen{0}; // clicks whose spot fell outside the screen: the window does not fit it
    int passed{0};
    int failed{0};
    int clickPhase{0};
    char clickName[40]{""};
    float clickX{0.f};
    float clickY{0.f};
    bool dragging{false};    // the click above is held and walked by (clickDX, clickDY): a drag, not a tap
    float clickDX{0.f};
    float clickDY{0.f};
    int keyPhase{0};         // a key press fed to ImGui: 1 down, 2 up (the tests' Escape)
    // Where the window sits and how big it is. Kept here for the session -- no cvar, no ImGui ini file: read back
    // from ImGui at the window's End, applied again the frame the window appears. (Set-always every frame would
    // snap back every drag and resize he makes.)
    bool winPlaced{false};
    float winX{0.f};
    float winY{0.f};
    float winW{0.f};
    float winH{0.f};
    // The drawn camera, while the window is open (vr_weaponstudio.cpp's camera block). `camOn` false is `off`, the
    // default: the view stays where the game puts his own eyes and nothing of this runs, so every number the game
    // measures -- his hands, the markers, the align read-outs, the tests -- stays exactly as it was. Flown out,
    // `camPos` is where its eye stands and `camTurn` how it looks (pitch, yaw, roll: pitch down is positive), both in
    // the world. `reset` and the window's Close put it back at his eyes.
    bool camOn{false};
    glm::vec3 camPos{0.f};
    glm::vec3 camTurn{0.f};
    bool wantCamReset{false}; // the window's "camera back to my eyes", done after its End (as its other buttons)
    // The window-frame checks (drag, resize, its title bar's X, Escape) count here and end with their own verdict.
    int schedPassed{0};
    int schedFailed{0};
    float schedX{0.f}; // where it was before the drag, and how big it was made
    float schedY{0.f};
    float schedW{0.f};
    float schedH{0.f};
};

extern State st;

// What vr_weaponstudio.cpp and vr_weaponstudio_ui.cpp share.
void noteSpot(const char* name);
void noteSpotAt(const char* name, float x, float y); // a spot that is not a widget: the title bar, a corner
[[nodiscard]] const Spot* spotNamed(const char* name);
void feedMock();        // the tests' mouse, fed before the ImGui frame
void testFrame();
void check(bool ok, const char* what);
void testVerdict();
void drawOverlays();    // the markers and the crosshair, over the game's view
bool drawWindow();      // the window (vr_weaponstudio_ui.cpp); false: it asked to close
void afterWindow(bool keep);
void closeStudio();
void openStudio();
void alignRun(int hand, bool apply);
void makePreview();
void removePreview();
void resetToDefaults(int slot);
void printChanges(int slot);
void copyInto(int from, int to);
} // namespace qvr::weaponstudio
