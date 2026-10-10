// vr_worldtext.hpp -- QC-driven 3D text labels (func_worldtext_banner, tutorial signs, ...).
//
// The server owns the list (created by the worldtext_* builtins while spawning or playing),
// broadcasts every change and replays the whole list to each client when it spawns.
// The client keeps a mirror for rendering. Floating texts (below) are fire-and-forget.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"


namespace qvr::worldtext
{

enum class HAlign : int
{
    Left = 0,
    Centre = 1,
    Right = 2
};

struct WorldText
{
    za::String text;
    glm::vec3 pos{0.f};
    glm::vec3 angles{0.f};
    HAlign hAlign{HAlign::Left};
    float scale{1.f};
};

// A floating text: a short-lived label that rises from a point and fades out, facing the viewer
// (the firing range dummy's damage numbers). The server only passes them on (unreliably, to every
// client); the client keeps them for floatTextLife seconds.
struct FloatText
{
    za::String text;
    glm::vec3 pos{0.f};
    glm::vec3 color{1.f};
    float scale{1.f};
    double start{0.0}; // the client time it appeared
};

inline constexpr double floatTextLife = 1.5;

// Server side (called by the builtins with the QC VM current).
void serverReset();
[[nodiscard]] int serverMake();
void serverSetText(int handle, const char* text);
void serverSetPos(int handle, const glm::vec3& pos);
void serverSetAngles(int handle, const glm::vec3& angles);
void serverSetHAlign(int handle, HAlign hAlign);
void serverSetScale(int handle, float scale);
void serverWriteAll(sizebuf_t* msg); // replay for a spawning client
void serverFloatText(const glm::vec3& pos, const char* text, const glm::vec3& color, float scale);

// Client side.
void clientReset();
void clientParse(int subcmd); // QVR_SVC_WORLDTEXT_*
void clientParseFloatText();  // QVR_SVC_FLOATTEXT
void clientParseViewMessage(); // QVR_SVC_VIEWMESSAGE
void clientWriteAll(sizebuf_t* msg); // the client's texts, for a demo recorded mid-game
[[nodiscard]] const za::Vector<WorldText>& clientTexts();
// Which list clientTexts() is: a new number at each clientReset (a new map or connection; never 0), so that what is
// kept for a handle (vr_text3d.cpp's boards) is not taken for the next list's.
[[nodiscard]] unsigned clientGeneration();

// The floating texts still showing at client time `now` (those done are dropped).
[[nodiscard]] const za::Vector<FloatText>& clientFloatTexts(double now);

// A view message (QC viewmessage: target_vr_message, the tutorial's welcome): floating in the middle of the view, at eye
// level, as VR Calibration's text (setup::drawViewText), for the seconds the server says; not in the wrist gadget's
// hologram. Without a headset, a centre print held that long instead. Showing (VR): new tips wait (vr_tips.cpp), as for a
// centre print.
[[nodiscard]] bool viewMessageShowing();
// Once a frame (vr_main.cpp, after the texts are cleared): the view message drawn while it lasts (in game: not in a menu,
// paused or at the intermission).
void viewMessageFrame();

} // namespace qvr::worldtext
