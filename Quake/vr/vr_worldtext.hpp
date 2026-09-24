// vr_worldtext.hpp -- QC-driven 3D text labels (func_worldtext_banner, tutorial signs, ...).
//
// The server owns the list (created by the worldtext_* builtins while spawning or playing),
// broadcasts every change and replays the whole list to each client when it spawns.
// The client keeps a mirror for rendering.

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

// Server side (called by the builtins with the QC VM current).
void serverReset();
[[nodiscard]] int serverMake();
void serverSetText(int handle, const char* text);
void serverSetPos(int handle, const glm::vec3& pos);
void serverSetAngles(int handle, const glm::vec3& angles);
void serverSetHAlign(int handle, HAlign hAlign);
void serverSetScale(int handle, float scale);
void serverWriteAll(sizebuf_t* msg); // replay for a spawning client

// Client side.
void clientReset();
void clientParse(int subcmd); // QVR_SVC_WORLDTEXT_*
[[nodiscard]] const std::vector<WorldText>& clientTexts();

} // namespace qvr::worldtext
