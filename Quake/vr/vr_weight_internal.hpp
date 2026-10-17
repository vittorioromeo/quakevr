#pragma once

// vr_weight_internal.hpp -- what vr_weight.cpp (see vr_weight.hpp) shares with its diagnostics, vr_weight_diag.cpp.
// Internal: included by those two alone. What was the file's anonymous namespace and both use is namespace
// weight::detail: its types and variables here (each variable one object: inline), its functions declared here
// and defined in the file that owns them.

#include "vr_modelmetadata.hpp"
#include "vr_weight.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_fatigue.hpp"
#include "vr_flashlight.hpp"
#include "vr_held.hpp"
#include "vr_main.hpp"
#include "vr_meleehud.hpp"
#include "vr_profile.hpp"
#include "vr_props.hpp"
#include "vr_protocol.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/quaternion.hpp>

#include <stdio.h>
#include <string.h>

namespace qvr::weight::detail
{

inline constexpr float gunGirth = 0.06f; // m

inline constexpr float gunBore = 0.08f;  // m

inline constexpr float gunRollRadius2 = gunGirth * gunGirth + gunBore * gunBore; // m^2: its inertia about the forearm, per kg

inline constexpr float sagShare = 0.3f;        // of gravity's pull at arm's length (vr_weight_spring_sag 1)

struct Body
{
    bool active{false};
    glm::vec3 x{0.f}, v{0.f}; // the grip: metres, m/s (body frame)
    glm::quat q{1.f, 0.f, 0.f, 0.f};
    glm::vec3 w{0.f};         // rad/s (body frame)
    bool targetValid{false};
    glm::vec3 xt{0.f};        // last frame's target
    glm::quat qt{1.f, 0.f, 0.f, 0.f};
    Offset off;
    const char* model{nullptr}; // what it holds (another: taken afresh)
    int entity{0};
    double jumpUntil{-1.0}; // vr_gametime: put back in the hand, the move says so until then (dropBits: the melee's jump)
};

[[nodiscard]] glm::quat quatFromAngles(const glm::vec3& angles);
[[nodiscard]] Tuning weaponTuning(int slot);
[[nodiscard]] Tuning propTuning(int slot);
void rodLoad(Load& l, float mass, float balance, float length, float radius);
void gunLoad(Load& l, float mass, float balance, float length);
[[nodiscard]] bool putBack(const Body& b, const glm::vec3& xt, const glm::quat& qt, float snapCm);
[[nodiscard]] bool step(Body& b, const Load& l, float m, const glm::vec3& xt0, const glm::vec3& xt, const glm::quat& qt0,
    const glm::quat& qt, float dt, float sag);
[[nodiscard]] glm::vec3 rotationVector(glm::quat q);
[[nodiscard]] glm::vec3 anglesFromQuat(const glm::quat& q);

} // namespace qvr::weight::detail
