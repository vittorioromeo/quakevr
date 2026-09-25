// vr_worldtext.hpp -- QC-driven 3D text labels (func_worldtext_banner, tutorial signs, ...).
//
// The server owns the list (created by the worldtext_* builtins while spawning or playing),
// broadcasts every change and replays the whole list to each client when it spawns.
// The client keeps a mirror for rendering. Floating texts (below) are fire-and-forget.

#pragma once

#include "vr_engine.hpp"

#include <string>
#include <vector>

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
    std::string text;
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
    std::string text;
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
void clientWriteAll(sizebuf_t* msg); // the client's texts, for a demo recorded mid-game
[[nodiscard]] const std::vector<WorldText>& clientTexts();

// The floating texts still showing at client time `now` (those done are dropped).
[[nodiscard]] const std::vector<FloatText>& clientFloatTexts(double now);

} // namespace qvr::worldtext
