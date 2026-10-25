// vr_agglitch.hpp -- the armed humanoids' glitch finder (ROUND22.md, "Armed humanoids in motion: the glitch finder and
// a robust solve"; Debug > Tests > Armed Humanoids: Glitch Finder, test 46). QC's sweep (vr_armedgrunt_glitch_test.qc)
// plays each kind with each weapon class through the real gameplay loops (aiming at a target moving all round, firing,
// reloading, running, pain, its strikes, a pickup) and names its subject (vr_agglitch_ent, _subject, _state); the arms'
// solve (vr_agarms.cpp) hands this every frame of that monster as drawn. Per frame: elbow flips (its swing about its
// reach line), an elbow off its outward and downward way, the arms in the torso, pelvis or head (vr_agbody.hpp), hands
// short of their grips, the off hand off its second grip and its grip toggling, joint gaps, the weapon off its aim, jumps
// of the hands, elbows and shoulders. vr_agglitch_report ranks the worst moments (each shot as an eye image: its front in
// the left eye, its side in the right) and prints each metric's worst and 95th percentile per subject; files in
// <gamedir>/agglitch.

#pragma once

#include "vr_rigpose.hpp"

#include <glm/glm.hpp>

namespace qvr::agglitch
{

// Whether entity `ent` is the finder's subject now (vr_agglitch_ent).
[[nodiscard]] bool watching(int ent);

struct Arm
{
    glm::vec3 S{0.f}, E{0.f}, W{0.f}, grip{0.f}; // its shoulder, elbow, wrist and grip as drawn (model space)
    glm::vec3 pole{0.f};                         // the elbow's preferred way (model space, unit)
    float miss{0.f};                             // its wrist brought in, out of reach (units)
    float turned{0.f};                           // its hand turned back, its wrist past its most (degrees)
    float pen{0.f};                              // the most its elbow, forearm, wrist or hand is inside the body (units)
    int penPart{-1};                             // ... which proxy (agbody::Part)
};

struct Sample
{
    int ent{0};
    Arm arm[2];
    glm::vec3 chestUp{0.f, 0.f, 1.f};
    bool want2{false}, held2{false}; // the off hand wanting the second grip, holding it
    float second{0.f};               // ... off it, held (units)
    float aim{0.f};                  // the right weapon off its wanted line (degrees)
    float gap{0.f};                  // the joints' gaps (units)
};

// agarms::solve: the subject's frame as drawn.
void note(const Sample& s);

// The metrics (flip, wrong, pen, reach, grip2, toggle, gap, aim, jump, shoulder, wrist): names, units, the value of each
// that is a glitch (a moment's severity: its value over this).
inline constexpr int numMetrics = 11;
[[nodiscard]] const char* metricName(int k);
[[nodiscard]] const char* metricUnit(int k);
[[nodiscard]] float metricBad(int k);

// One frame's metrics (the Pose Studio's Live tape, vr_poselive.hpp; note's own): `s` with the frame before it `p1` and
// the one before that `p2` (null: none), `dt` seconds after p1. out[k] the worst of either arm, side[k] which (0 right);
// the toggle's left 0 (note counts it over the last second).
void measure(const Sample& s, const Sample* p1, const Sample* p2, float dt, float* out, signed char* side);

void registerCommands();

} // namespace qvr::agglitch
