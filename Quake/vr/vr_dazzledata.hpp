#pragma once

// vr_dazzledata.hpp -- the flashlight dazzle's shield pose tweaked per armed humanoid kind (his words 2026-11-28, item 14;
// ROUND26.md "dazzlestudio"): offsets laid on the base pose (QC vr_armedgrunt_dazzle.qc VR_AGP_Shield) for the shielding
// hand, its elbow and forearm, and the gun hand moved away, one set for a single weapon and one for two. All zero (the
// default) is the base pose exactly. Kept in quakevr/data/dazzle.txt (vr_dazzle_file), edited by the Dazzle Studio
// (vr_dazzlestudio.hpp), read by QC through dazzletweak (the server's pose: every client draws it from the pose message).

#include <glm/glm.hpp>

namespace qvr::dazzledata
{

inline constexpr int kindCount = 6; // QC's VR_AM_* rows: grunt, enforcer, knight, death knight, AD's death guard, crossbow knight
inline constexpr int setCount = 2;  // 0 one weapon (or none), 1 two weapons (vr_agrunt_dazzle_dual's shield)

struct Tweak
{
    glm::vec3 hand{0.f};     // the shielding hand's grip moved (its body's space: forward, left, up; units)
    glm::vec3 handTurn{0.f}; // the left weapon turned in that hand (two weapons; pitch, yaw, roll; degrees)
    glm::vec3 elbow{0.f};    // added to the elbow's way (VR_AG_SHIELD_POLE '0.3 1 -0.45', normalized after)
    float twist{0.f};        // the elbow turned about the shoulder-to-hand line (degrees: its forearm rolled)
    glm::vec3 gun{0.f};      // the gun hand moved away while it shields (its body's space; units, times the shield's share)

    [[nodiscard]] bool zero() const;
};

[[nodiscard]] const char* kindName(int kind); // "grunt" .. (the file's words)
[[nodiscard]] const char* kindLabel(int kind); // "Grunt" .. (the studio's)

// The tweak of `kind`'s set (clamped indices); loaded from the file at first use.
[[nodiscard]] Tweak& at(int kind, int set);
void touched(); // an edit made (the unsaved mark)
[[nodiscard]] int revision();
[[nodiscard]] int savedRevision();

[[nodiscard]] const char* filePath();
bool load(); // false: no file (all zero)
bool save();
void clear(); // every tweak zero (not saved)

void init();          // the commands: vr_dazzle_load, vr_dazzle_save
void PF_dazzletweak(); // QC: vector(float kind, float dual, float field) dazzletweak

} // namespace qvr::dazzledata
