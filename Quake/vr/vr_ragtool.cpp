// vr_ragtool.cpp -- the ragdoll tool: see vr_ragtool.hpp and docs/vr-port/RAGTOOL.md.
//
// Adding a behaviour: a row in `behaviours` (its steps: the game's own calls, at times from the run's start). Adding a
// setting to the registry: a row in `params` (its group, its cvar, its slider's range). The menu (vr_menu_ragtool.inc),
// the console lists and the presets follow.

#include "vr_ragtool.hpp"

#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_progs.hpp"
#include "vr_ragdoll.hpp"
#include "vr_toolgun.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <stdio.h>
#include <string.h>

using namespace qvr;

namespace
{
namespace physics = qvr::ragtool::physics;

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return glm::vec3{v[0], v[1], v[2]};
}

// ---- The behaviours ----

enum class Op : uint8_t
{
    End,
    Cmd,     // a console command (text: %d the subject's entity)
    Place,   // the player put where the push comes from (vr_ragtool_dir plus arg degrees), placeGap units off
    Hold,    // the virtual hand takes the part of vr_ragtool_hold (arg >= 0: that kind instead)
    Lift,    // the hand's goal raised arg units
    Drag,    // the hand's goal carried arg units away from the player's side
    Flail,   // the held one flails (box3d::ragdollFlail)
    Release, // the hand lets go
    Parry,   // QC's parry stagger (VR_Parry_Ragdoll) from the player's side
};

struct Step
{
    float at; // s from the run's start
    Op op;
    float arg;
    const char* text;
};

constexpr int mostSteps = 6;

struct Behaviour
{
    const char* name;
    const char* label;
    const char* help;
    float length; // s watched (0: vr_ragtool_length)
    Step steps[mostSteps];
};

constexpr float placeGap = 64.f;  // units: the player this far off the subject when a push comes from him
constexpr float handSpeed = 90.f; // units/s: the virtual hand towards its goal
constexpr float stageAhead = 160.f; // units: the subject this far ahead of where the player starts
constexpr double settleFirst = 0.5; // s: a fresh subject stands this long before its behaviour's steps begin

const Behaviour behaviours[] = {
    {"stand", "Stand (Idle)", "Spawned fresh and left standing: the start of every other run.", 2.f, {{0.f, Op::End, 0.f, nullptr}}},
    {"stagger", "Shove Stagger and Balance Walk",
        "vr_ragdoll_act stagger: a living stagger from a shove, his balance walk, then recovered.", 0.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act stagger %d"}, {0.f, Op::End, 0.f, nullptr}}},
    {"stagger_fall", "Stagger and Fall", "vr_ragdoll_act stagger_fall: a stagger he loses: down, then up again.", 7.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act stagger_fall %d"}, {0.f, Op::End, 0.f, nullptr}}},
    {"shove1", "One-Hand Shove (Direction)",
        "vr_knockdown_test 21 from Push From: the game's own open-palm shove (its push, stagger and chance).", 0.f,
        {{0.1f, Op::Place, 0.f, nullptr}, {0.2f, Op::Cmd, 0.f, "vr_knockdown_test 21"}, {0.f, Op::End, 0.f, nullptr}}},
    {"shove2", "Two-Hand Shove (Direction)", "vr_knockdown_test 22 from Push From: the two-handed shove.", 0.f,
        {{0.1f, Op::Place, 0.f, nullptr}, {0.2f, Op::Cmd, 0.f, "vr_knockdown_test 22"}, {0.f, Op::End, 0.f, nullptr}}},
    {"knockdown", "Knockdown Fall (Direction)", "vr_knockdown_test 0 from Push From: knocked down whatever the chance.", 6.f,
        {{0.1f, Op::Place, 0.f, nullptr}, {0.2f, Op::Cmd, 0.f, "vr_knockdown_test 0"}, {0.f, Op::End, 0.f, nullptr}}},
    {"getup", "Knocked Down, Then Get Up", "vr_ragdoll_act getup: down, and up again by the get-up's drive.", 8.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act getup %d"}, {0.f, Op::End, 0.f, nullptr}}},
    {"rollover", "Face Down: Roll Over and Get Up",
        "Knocked down from behind Push From (on his face), then up at once (vr_knockdown_test 1): the get-up's roll over.",
        9.f,
        {{0.1f, Op::Place, 180.f, nullptr}, {0.2f, Op::Cmd, 0.f, "vr_knockdown_test 0"},
            {2.5f, Op::Cmd, 0.f, "vr_knockdown_test 1"}, {0.f, Op::End, 0.f, nullptr}}},
    {"crawl", "Dying Crawl", "vr_ragdoll_act crawl: shot down, he crawls away.", 8.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act crawl %d"}, {0.f, Op::End, 0.f, nullptr}}},
    {"crawl_legless", "Both Legs Cut Off: Crawl", "vr_ragdoll_act cut_legs: no legs, he crawls on his arms.", 8.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act cut_legs %d"}, {0.f, Op::End, 0.f, nullptr}}},
    {"death", "Death: Shot in the Body", "vr_ragdoll_act death_body: the death drive as he falls.", 0.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act death_body %d"}, {0.f, Op::End, 0.f, nullptr}}},
    {"stumble", "Death: Stumbling On", "vr_ragdoll_act stumble: dying on the run.", 0.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act stumble %d"}, {0.f, Op::End, 0.f, nullptr}}},
    {"flinch", "Flinch (Body)", "vr_ragdoll_act hit_body: a living hit's reaction.", 3.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act hit_body %d"}, {0.f, Op::End, 0.f, nullptr}}},
    {"knee", "Knee Buckle and Limp", "vr_ragdoll_act hit_leg: a shot in his leg that doesn't kill.", 0.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act hit_leg %d"}, {0.f, Op::End, 0.f, nullptr}}},
    {"hit_stagger", "Hit Stagger: Strong Blow (Direction)",
        "vr_knockdown_test 42 from Push From: a strong blow's stagger (Hits Unbalance), no damage.", 0.f,
        {{0.1f, Op::Place, 0.f, nullptr}, {0.2f, Op::Cmd, 0.f, "vr_knockdown_test 42"}, {0.f, Op::End, 0.f, nullptr}}},
    {"parry", "Parry Stagger (Direction)",
        "QC's VR_Parry_Ragdoll from Push From: a parried enemy's ragdoll stagger back (vr_parry_stagger s).", 0.f,
        {{0.1f, Op::Place, 0.f, nullptr}, {0.2f, Op::Parry, 0.f, nullptr}, {0.f, Op::End, 0.f, nullptr}}},
    {"struggle", "Struggle in the Hand (Hold)",
        "Knocked down, then held by the virtual hand at Hold: his struggle (swats, squirm, the head hold's arms over his "
        "head, upright on an arm drag). Move the hand: the arrows, Page Up and Down.",
        10.f,
        {{0.1f, Op::Place, 0.f, nullptr}, {0.2f, Op::Cmd, 0.f, "vr_knockdown_test 0"}, {1.6f, Op::Hold, -1.f, nullptr},
            {0.f, Op::End, 0.f, nullptr}}},
    {"lift", "Lifted by the Hand (Hold)", "As Struggle, the hand then lifting him 40 units.", 10.f,
        {{0.1f, Op::Place, 0.f, nullptr}, {0.2f, Op::Cmd, 0.f, "vr_knockdown_test 0"}, {1.6f, Op::Hold, -1.f, nullptr},
            {2.2f, Op::Lift, 40.f, nullptr}, {0.f, Op::End, 0.f, nullptr}}},
    {"drag", "Dragged by the Hand (Hold)",
        "As Struggle, the hand then dragging him 64 units towards you (Hold: Arm for the arm drag's upright).", 10.f,
        {{0.1f, Op::Place, 0.f, nullptr}, {0.2f, Op::Cmd, 0.f, "vr_knockdown_test 0"}, {1.6f, Op::Hold, -1.f, nullptr},
            {2.2f, Op::Drag, 64.f, nullptr}, {0.f, Op::End, 0.f, nullptr}}},
    {"flail", "Lifted: Flail", "Knocked down, taken by the chest and lifted: his flail in the hands (box3d::ragdollFlail).",
        8.f,
        {{0.1f, Op::Place, 0.f, nullptr}, {0.2f, Op::Cmd, 0.f, "vr_knockdown_test 0"}, {1.6f, Op::Hold, 0.f, nullptr},
            {1.7f, Op::Flail, 0.f, nullptr}, {1.7f, Op::Lift, 48.f, nullptr}, {0.f, Op::End, 0.f, nullptr}}},
    {"guard", "Guard the Head", "vr_ragdoll_act guard: his arms up over his head.", 4.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act guard %d"}, {0.f, Op::End, 0.f, nullptr}}},
    {"lava", "Lava: Dying Flail", "vr_ragdoll_act swim_lava: his flail in lava (forced on dry land).", 6.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act swim_lava %d"}, {0.f, Op::End, 0.f, nullptr}}},
    {"drown", "Drowning: Paddle", "vr_ragdoll_act swim_water: his paddle (forced on dry land).", 6.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act swim_water %d"}, {0.f, Op::End, 0.f, nullptr}}},
    {"throw", "Two-Hand Throw (Judo)", "vr_ragdoll_act throw: thrown over, then on his feet again.", 8.f,
        {{0.2f, Op::Cmd, 0.f, "vr_ragdoll_act throw %d"}, {0.f, Op::End, 0.f, nullptr}}},
};
constexpr int behaviourTotal = static_cast<int>(sizeof(behaviours) / sizeof(behaviours[0]));

// ---- The parameter registry ----

enum Group : int
{
    GroupMotors,
    GroupJoints,
    GroupStagger,
    GroupShove,
    GroupGetUp,
    GroupDeath,
    GroupStruggle,
    GroupParry,
    GroupFlail,
    GroupTotal
};
constexpr const char* groupNames[GroupTotal] = {"Motors", "Joint Groups", "Stagger and Balance", "Shove",
    "Knockdown and Get-Up", "Death and Crawl", "Struggle", "Parry and Hit Stagger", "Flail, Liquids and Hand"};

const ragtool::Param params[] = {
    {GroupMotors, "vr_ragdoll_active_strength", "Strength", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupMotors, "vr_ragdoll_active_hertz", "Spring Rate (Hz)", 0.f, 20.f, 0.5f, "%.1f", false},
    {GroupMotors, "vr_ragdoll_active_damping", "Damping", 0.f, 4.f, 0.05f, "%.2f", false},
    {GroupMotors, "vr_ragdoll_active_torque", "Max Force", 0.f, 10.f, 0.1f, "%.1f", false},
    {GroupMotors, "vr_ragdoll_active_fade", "Strength Lasts", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupMotors, "vr_ragdoll_joint_friction", "Joint Friction", 0.f, 10.f, 0.1f, "%.1f", false},
    {GroupMotors, "vr_ragdoll_damping", "Body Damping", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupMotors, "vr_ragdoll_limits", "Joint Limits (All)", 0.25f, 2.f, 0.05f, "%.2f", true},
    {GroupJoints, "vr_ragdoll_active_hertz_legs", "Legs: Spring", 0.f, 4.f, 0.05f, "%.2fx", false},
    {GroupJoints, "vr_ragdoll_active_damping_legs", "Legs: Damping", 0.f, 4.f, 0.05f, "%.2fx", false},
    {GroupJoints, "vr_ragdoll_active_torque_legs", "Legs: Max Force", 0.f, 4.f, 0.05f, "%.2fx", false},
    {GroupJoints, "vr_ragdoll_limits_legs", "Legs: Limits", 0.25f, 2.f, 0.05f, "%.2fx", true},
    {GroupJoints, "vr_ragdoll_active_hertz_torso", "Torso: Spring", 0.f, 4.f, 0.05f, "%.2fx", false},
    {GroupJoints, "vr_ragdoll_active_damping_torso", "Torso: Damping", 0.f, 4.f, 0.05f, "%.2fx", false},
    {GroupJoints, "vr_ragdoll_active_torque_torso", "Torso: Max Force", 0.f, 4.f, 0.05f, "%.2fx", false},
    {GroupJoints, "vr_ragdoll_limits_torso", "Torso: Limits", 0.25f, 2.f, 0.05f, "%.2fx", true},
    {GroupJoints, "vr_ragdoll_active_hertz_arms", "Arms: Spring", 0.f, 4.f, 0.05f, "%.2fx", false},
    {GroupJoints, "vr_ragdoll_active_damping_arms", "Arms: Damping", 0.f, 4.f, 0.05f, "%.2fx", false},
    {GroupJoints, "vr_ragdoll_active_torque_arms", "Arms: Max Force", 0.f, 4.f, 0.05f, "%.2fx", false},
    {GroupJoints, "vr_ragdoll_limits_arms", "Arms: Limits", 0.25f, 2.f, 0.05f, "%.2fx", true},
    {GroupJoints, "vr_ragdoll_active_hertz_head", "Head: Spring", 0.f, 4.f, 0.05f, "%.2fx", false},
    {GroupJoints, "vr_ragdoll_active_damping_head", "Head: Damping", 0.f, 4.f, 0.05f, "%.2fx", false},
    {GroupJoints, "vr_ragdoll_active_torque_head", "Head: Max Force", 0.f, 4.f, 0.05f, "%.2fx", false},
    {GroupJoints, "vr_ragdoll_limits_head", "Head: Limits", 0.25f, 2.f, 0.05f, "%.2fx", true},
    {GroupStagger, "vr_ragdoll_active_stagger", "Stagger (Switch)", 0.f, 1.f, 1.f, "%.0f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_lean", "Lean (deg)", 0.f, 60.f, 1.f, "%.0f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_push", "Push", 0.f, 5.f, 0.1f, "%.1f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_travel", "Travel", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_pin", "Pelvis Pin", 0.f, 5.f, 0.1f, "%.1f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_blend", "Blend (s)", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_settle", "Settle", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_settle_time", "Settle Time (s)", 0.f, 2.f, 0.05f, "%.2f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_drop", "Drop (%)", 0.f, 60.f, 1.f, "%.0f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_steps", "Most Steps", 0.f, 12.f, 1.f, "%.0f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_walk", "Walked Backpedal", 0.f, 1.f, 1.f, "%.0f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_lift", "Step Lift", 0.f, 0.6f, 0.02f, "%.2f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_crouch", "Crouch", 0.f, 0.25f, 0.01f, "%.2f", false},
    {GroupStagger, "vr_ragdoll_active_stagger_arms", "Arm Flail", 0.f, 2.f, 0.05f, "%.2f", false},
    {GroupStagger, "vr_ragdoll_active_balance_time", "Balance Time (s)", 0.f, 4.f, 0.05f, "%.2f", false},
    {GroupStagger, "vr_ragdoll_active_balance_lean", "Balance Lean (deg)", 0.f, 45.f, 1.f, "%.0f", false},
    {GroupShove, "vr_knockdown_chance", "Knockdown Chance", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupShove, "vr_knockdown_onehand", "One-Hand Strength", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupShove, "vr_knockdown_twohand", "Two-Hand Strength", 0.f, 4.f, 0.05f, "%.2f", false},
    {GroupShove, "vr_knockdown_push", "Push", 0.f, 4.f, 0.05f, "%.2f", false},
    {GroupShove, "vr_shove_push_onehand", "One-Hand Push", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupShove, "vr_shove_push_twohand", "Two-Hand Push", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupShove, "vr_knockdown_shove_travel", "Shove Travel", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupShove, "vr_knockdown_shove_topple_angle", "Topple Angle (deg)", 0.f, 180.f, 5.f, "%.0f", false},
    {GroupShove, "vr_knockdown_shove_topple_time", "Topple Time (s)", 0.f, 2.f, 0.05f, "%.2f", false},
    {GroupShove, "vr_knockdown_shove_feet_lag", "Feet Lag (s)", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupShove, "vr_knockdown_shove_max_spin", "Most Spin (deg/s)", 0.f, 720.f, 10.f, "%.0f", false},
    {GroupShove, "vr_knockdown_shove_ledge_topple", "Ledge Topple (deg/s)", 0.f, 600.f, 10.f, "%.0f", false},
    {GroupShove, "vr_knockdown_shove_ledge_push", "Ledge Push", 0.f, 1.5f, 0.05f, "%.2f", false},
    {GroupShove, "vr_knockdown_shove_ledge_over", "Ledge Carry Past Edge", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupGetUp, "vr_knockdown_time_min", "Down at Least (s)", 0.f, 10.f, 0.25f, "%.2f", false},
    {GroupGetUp, "vr_knockdown_time_max", "Down at Most (s)", 0.f, 10.f, 0.25f, "%.2f", false},
    {GroupGetUp, "vr_knockdown_blend", "Blend (s)", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupGetUp, "vr_knockdown_getup_speed", "Get-Up Speed", 0.1f, 3.f, 0.05f, "%.2f", false},
    {GroupGetUp, "vr_knockdown_getup_force", "Get-Up Force", 0.f, 10.f, 0.1f, "%.1f", false},
    {GroupGetUp, "vr_knockdown_getup_slope", "Get-Up Slope (deg)", 0.f, 60.f, 1.f, "%.0f", false},
    {GroupGetUp, "vr_ragdoll_active_getup", "Active Get-Up (Switch)", 0.f, 1.f, 1.f, "%.0f", false},
    {GroupGetUp, "vr_ragdoll_active_getup_pin", "Get-Up Pin", 0.f, 5.f, 0.1f, "%.1f", false},
    {GroupGetUp, "vr_ragdoll_active_getup_blend", "Get-Up Blend (s)", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupGetUp, "vr_ragdoll_active_getup_pace", "Get-Up Pace", 0.1f, 3.f, 0.05f, "%.2f", false},
    {GroupGetUp, "vr_ragdoll_active_getup_hold", "Roll Over and Pull In", 0.f, 2.f, 0.05f, "%.2f", false},
    {GroupGetUp, "vr_knockdown_wiggle", "Struggling Strength", 0.f, 3.f, 0.1f, "%.1f", false},
    {GroupGetUp, "vr_knockdown_wiggle_frequency", "Struggling Rate (Hz)", 0.f, 4.f, 0.1f, "%.1f", false},
    {GroupDeath, "vr_ragdoll_active_pin", "Death Pin", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupDeath, "vr_ragdoll_active_pin_time", "Pin Time (s)", 0.f, 2.f, 0.05f, "%.2f", false},
    {GroupDeath, "vr_ragdoll_active_stumble", "Stumble From (u/s)", 0.f, 400.f, 10.f, "%.0f", false},
    {GroupDeath, "vr_ragdoll_active_wound", "Hold the Wound (Chance)", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupDeath, "vr_ragdoll_active_crawl", "Crawl (Chance)", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupDeath, "vr_ragdoll_active_crawl_time", "Crawl Time (s)", 0.f, 10.f, 0.25f, "%.2f", false},
    {GroupDeath, "vr_ragdoll_active_crawl_back", "Crawl on the Back", 0.f, 1.f, 1.f, "%.0f", false},
    {GroupDeath, "vr_ragdoll_active_crawl_force", "Crawl Force", 0.f, 4.f, 0.05f, "%.2f", false},
    {GroupDeath, "vr_ragdoll_active_writhe", "Writhe", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupDeath, "vr_ragdoll_active_dismember_time", "Dismembered Time (s)", 0.f, 10.f, 0.25f, "%.2f", false},
    {GroupStruggle, "vr_struggle_strength", "Strength", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_speed", "Speed", 0.2f, 3.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_variation", "Variation", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_fatigue", "Fatigue (s)", 0.f, 60.f, 1.f, "%.0f", false},
    {GroupStruggle, "vr_struggle_tired", "Tired Strength", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_bursts", "Bursts", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_blend", "Blend (s)", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_held_share", "Held Part's Share", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_swat", "Swats", 0.f, 2.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_squirm", "Squirm", 0.f, 2.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_look", "Look at You", 0.f, 1.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_look_strength", "Look Strength", 0.f, 0.9f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_upright", "Upright on Arm Drag", 0.f, 2.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_frantic", "Frantic", 0.f, 2.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_flail", "Flail", 0.f, 2.f, 0.05f, "%.2f", false},
    {GroupStruggle, "vr_struggle_arm_reach", "Arm Reach (deg)", 0.f, 180.f, 5.f, "%.0f", false},
    {GroupParry, "vr_parry_stagger", "Parry Stagger (s)", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupParry, "vr_parry_stagger_min", "Stagger at Least", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupParry, "vr_parry_stagger_max", "Stagger at Most", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupParry, "vr_parry_stagger_heavy", "Heavy Stagger", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupParry, "vr_parry_wobble", "Wobble", 0.f, 2.f, 0.05f, "%.2f", false},
    {GroupParry, "vr_hit_react_strength", "Hit Reaction Strength", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupParry, "vr_hit_react_time", "Hit Reaction Time", 0.f, 3.f, 0.05f, "%.2f", false},
    {GroupFlail, "vr_ragdoll_active_flail", "Flail", 0.f, 4.f, 0.05f, "%.2f", false},
    {GroupFlail, "vr_ragdoll_active_paddle", "Paddle", 0.f, 10.f, 0.1f, "%.1f", false},
    {GroupFlail, "vr_toolgun_physgun_ragdoll_force", "Virtual Hand's Pull", 0.f, 10.f, 0.25f, "%.2f", false},
};
constexpr int paramTotal = static_cast<int>(sizeof(params) / sizeof(params[0]));

// ---- The tool's state ----

constexpr int mostModels = 128;

struct Run
{
    int behaviour{-1};
    int next{0};          // its next step
    double begun{0.0};    // server time
    bool on{false};
    int count{0};         // runs so far (the log's)
    // The log's measures over the run.
    double nextSample{0.0};
    float errorSum{0.f}, errorMost{0.f};
    int errorN{0};
    float strengthSum{0.f};
    float travel{0.f};
    glm::vec3 last{0.f};
    bool haveLast{false};
    float lowest{1e30f};
    int ragdollFrames{0}, frames{0};
};

struct Tool
{
    bool on{false};
    bool flag{false};
    bool flagStarted{false};
    bool wantMap{false};      // waiting for vr_ragtool_map to load
    bool staged{false};       // the stage found (in this map)
    glm::vec3 home{0.f};      // the player's start
    float homeYaw{0.f};
    glm::vec3 stage{0.f};     // the subject's feet
    float stageYaw{0.f};
    int subject{0};           // its entity (0: none)
    int subjectModel{-1};     // the vr_ragtool_models row it is
    double spawnAt{-1.0};     // server time to make it (after the clean-up); < 0: none pending
    int playAfterSpawn{-1};   // the behaviour to run once made
    bool paused{false};
    int steps{0};             // frames to run while paused
    bool holding{false};
    glm::vec3 hand{0.f}, handGoal{0.f};
    bool timescaleSet{false};
    bool flagsSet{false};
    Run run;
    // The models: the toolgun's spawn list's monsters with a rig, available now.
    za::Array<int, mostModels> models{};
    int modelCount{0};
    bool listed{false};
    char status[256]{};
};
Tool tool;

struct SvVm
{
    qcvm_t* old{nullptr};
    SvVm() { PR_PushQCVM(&sv.qcvm, &old); }
    ~SvVm() { PR_PopQCVM(old); }
};

[[nodiscard]] bool inGame()
{
    return sv.active && svs.maxclients >= 1 && cls.state == ca_connected && cls.signon == SIGNONS;
}

[[nodiscard]] edict_t* player()
{
    return EDICT_NUM(1);
}

[[nodiscard]] edict_t* subjectEnt()
{
    if(tool.subject <= 0 || tool.subject >= sv.qcvm.num_edicts)
    {
        return nullptr;
    }
    edict_t* e = EDICT_NUM(tool.subject);
    return e->free ? nullptr : e;
}

void listModels()
{
    if(tool.listed)
    {
        return;
    }
    tool.listed = true;
    tool.modelCount = 0;
    for(int i = 0; i < toolgun::spawnableCount() && tool.modelCount < mostModels; i++)
    {
        if(strcmp(toolgun::spawnableSection(i), "Monsters") != 0 || !toolgun::spawnableAvailable(i))
        {
            continue;
        }
        qmodel_t* m = Mod_ForName(toolgun::spawnableModel(i), false);
        if(!m || !ragdoll::eligible(m) || !ragdoll::rigFor(m))
        {
            continue;
        }
        tool.models[static_cast<za::SizeT>(tool.modelCount++)] = i;
    }
}

[[nodiscard]] int modelRow()
{
    return za::clamp(static_cast<int>(vr_ragtool_model.value), 0, za::max(tool.modelCount - 1, 0));
}

[[nodiscard]] const char* modelName(int row)
{
    return row >= 0 && row < tool.modelCount ? toolgun::spawnableLabel(tool.models[static_cast<za::SizeT>(row)]) : "?";
}

[[nodiscard]] int behaviourRow()
{
    return za::clamp(static_cast<int>(vr_ragtool_behaviour.value), 0, behaviourTotal - 1);
}

// Where a push comes from: the subject's facing turned by vr_ragtool_dir (and `extra`) degrees, flat.
[[nodiscard]] glm::vec3 pushSide(const edict_t* e, float extra)
{
    const float a = glm::radians(e->v.angles[1] + vr_ragtool_dir.value + extra);
    return glm::vec3{za::cos(a), za::sin(a), 0.f};
}

void movePlayer(const glm::vec3& at)
{
    edict_t* p = player();
    p->v.origin[0] = at.x;
    p->v.origin[1] = at.y;
    p->v.origin[2] = at.z;
    p->v.velocity[0] = p->v.velocity[1] = p->v.velocity[2] = 0.f;
    SV_LinkEdict(p, false);
}

void releaseHand()
{
    if(tool.holding)
    {
        box3d::physgunRagdollRelease(glm::vec3{0.f});
    }
    tool.holding = false;
}

bool callParry(edict_t* m)
{
    const func_t fn = progs::findFunction("VR_Parry_Ragdoll");
    if(!fn)
    {
        Con_Printf("ragtool: no VR_Parry_Ragdoll in the progs\n");
        return false;
    }
    const int oldSelf = pr_global_struct->self, oldOther = pr_global_struct->other;
    pr_global_struct->time = sv.qcvm.time;
    pr_global_struct->self = EDICT_TO_PROG(player());
    G_INT(OFS_PARM0) = EDICT_TO_PROG(m);
    G_INT(OFS_PARM1) = EDICT_TO_PROG(player());
    G_FLOAT(OFS_PARM2) = vr_parry_stagger.value;
    PR_ExecuteProgram(fn);
    const float ok = G_FLOAT(OFS_RETURN);
    pr_global_struct->self = oldSelf;
    pr_global_struct->other = oldOther;
    return ok != 0.f;
}

// The run's summary on the console (vr_ragtool_log).
void logRun(const char* why)
{
    Run& r = tool.run;
    if(r.behaviour < 0 || vr_ragtool_log.value <= 0.f)
    {
        return;
    }
    const physics::Sample s = physics::sample(tool.subject);
    Con_Printf("ragtool: run %d %s on %s (%s): %.2f s, ragdoll %d%% of frames, tracking error mean %.2f most %.2f deg, "
               "strength %.2f, pelvis travel %.1f u, lowest %.1f u over the floor, end %.1f u over it\n",
        r.count, behaviours[r.behaviour].name, modelName(tool.subjectModel), why, sv.qcvm.time - r.begun,
        r.frames ? r.ragdollFrames * 100 / r.frames : 0, r.errorN ? r.errorSum / static_cast<float>(r.errorN) : 0.f,
        r.errorMost, r.errorN ? r.strengthSum / static_cast<float>(r.errorN) : 0.f, r.travel,
        r.lowest < 1e29f ? r.lowest : 0.f, s.ragdoll ? s.pelvis.z - s.floor : 0.f);
}

void endRun(const char* why)
{
    if(!tool.run.on)
    {
        return;
    }
    logRun(why);
    tool.run.on = false;
}

void respawn(int playAfter)
{
    const SvVm vm; // (from a command too)
    endRun("respawned");
    releaseHand();
    if(!inGame() || !tool.staged)
    {
        return;
    }
    // (Every monster removed, and what lay about: QC's own clean slate; the new one made a moment later.)
    Cbuf_InsertText("vr_knockdown_test 20\nvr_scene_clean all\n"); // (next, ahead of a script's waits)
    tool.subject = 0;
    tool.spawnAt = sv.qcvm.time + 0.25;
    tool.playAfterSpawn = playAfter;
}

void makeSubject()
{
    listModels();
    tool.spawnAt = -1.0;
    if(tool.modelCount == 0)
    {
        Con_Printf("ragtool: no monster with a rig to spawn\n");
        return;
    }
    const int row = modelRow();
    movePlayer(tool.home);
    edict_t* e = toolgun::spawnEntryAt(tool.models[static_cast<za::SizeT>(row)], tool.stage, tool.stageYaw);
    if(!e)
    {
        Con_Printf("ragtool: %s could not be spawned\n", modelName(row));
        return;
    }
    tool.subject = NUM_FOR_EDICT(e);
    tool.subjectModel = row;
    Con_Printf("ragtool: %s spawned (entity %d, %s)\n", modelName(row), tool.subject, PR_GetString(e->v.classname));
    if(tool.playAfterSpawn >= 0)
    {
        Run& r = tool.run;
        const int count = r.count + 1;
        r = Run{};
        r.count = count;
        r.behaviour = tool.playAfterSpawn;
        r.begun = sv.qcvm.time + settleFirst; // (settled on its feet first: a fresh AD monster may refuse a knockdown)
        r.on = true;
        tool.playAfterSpawn = -1;
    }
}

// The stage: ahead of the player's start, on the floor.
void findStage()
{
    edict_t* p = player();
    tool.home = vec(p->v.origin);
    tool.homeYaw = p->v.angles[1];
    const float a = glm::radians(tool.homeYaw);
    const glm::vec3 fwd{za::cos(a), za::sin(a), 0.f};
    vec3_t start{tool.home.x, tool.home.y, tool.home.z}, zero{0.f, 0.f, 0.f};
    const glm::vec3 ahead = tool.home + fwd * stageAhead;
    vec3_t end{ahead.x, ahead.y, ahead.z};
    trace_t tr = SV_Move(start, zero, zero, end, MOVE_NOMONSTERS, p);
    const glm::vec3 at = tr.fraction < 1.f ? tool.home + fwd * za::max(stageAhead * tr.fraction - 40.f, 48.f) : ahead;
    vec3_t from{at.x, at.y, at.z}, down{at.x, at.y, at.z - 256.f};
    tr = SV_Move(from, zero, zero, down, MOVE_NOMONSTERS, p);
    tool.stage = glm::vec3{at.x, at.y, tr.fraction < 1.f ? tr.endpos[2] : at.z + p->v.mins[2]};
    tool.stageYaw = tool.homeYaw + 180.f;
    tool.staged = true;
    tool.listed = false; // (AD's monsters are there once its folder is mounted, as a map loads: vr_ad_mounted)
    listModels();
    // (The camera's start: facing the stage, a little down on it.)
    p->v.angles[0] = 15.f;
    p->v.v_angle[0] = 15.f;
    p->v.fixangle = 1.f;
}

void stepRun()
{
    Run& r = tool.run;
    edict_t* e = subjectEnt();
    if(!r.on || r.behaviour < 0)
    {
        return;
    }
    if(!e)
    {
        endRun("its entity gone");
        return;
    }
    const Behaviour& b = behaviours[r.behaviour];
    const float t = static_cast<float>(sv.qcvm.time - r.begun);
    while(r.next < mostSteps && b.steps[r.next].op != Op::End && t >= b.steps[r.next].at)
    {
        const Step& s = b.steps[r.next++];
        switch(s.op)
        {
        case Op::Cmd:
        {
            char text[160];
            q_snprintf(text, sizeof(text), s.text, tool.subject);
            q_strlcat(text, "\n", sizeof(text));
            Cbuf_InsertText(text);
            break;
        }
        case Op::Place: movePlayer(vec(e->v.origin) + pushSide(e, s.arg) * placeGap); break;
        case Op::Hold:
        {
            const int kind = s.arg >= 0.f ? static_cast<int>(s.arg) : static_cast<int>(vr_ragtool_hold.value);
            const auto h = static_cast<physics::Hold>(za::clamp(kind, 0, static_cast<int>(physics::Hold::Count) - 1));
            glm::vec3 grip{0.f};
            releaseHand();
            if(physics::takeHold(tool.subject, h, grip))
            {
                tool.holding = true;
                tool.hand = tool.handGoal = grip;
                Con_Printf("ragtool: the hand holds his %s\n", physics::holdName(h));
            }
            else
            {
                Con_Printf("ragtool: no %s to hold (not a ragdoll yet?)\n", physics::holdName(h));
            }
            break;
        }
        case Op::Lift: tool.handGoal.z += s.arg; break;
        case Op::Drag: tool.handGoal += pushSide(e, 0.f) * s.arg; break;
        case Op::Flail:
            Con_Printf("ragtool: flail %s\n", box3d::ragdollFlail(e, true) ? "begun" : "refused");
            break;
        case Op::Release: releaseHand(); break;
        case Op::Parry: Con_Printf("ragtool: parry stagger %s\n", callParry(e) ? "begun" : "refused"); break;
        default: break;
        }
    }
    // The log's measures.
    const physics::Sample s = physics::sample(tool.subject);
    r.frames++;
    if(s.ragdoll)
    {
        r.ragdollFrames++;
        if(r.haveLast)
        {
            r.travel += glm::distance(s.pelvis, r.last);
        }
        r.last = s.pelvis;
        r.haveLast = true;
        r.lowest = za::min(r.lowest, s.pelvis.z - s.floor);
    }
    if(s.driven)
    {
        r.errorSum += s.error;
        r.errorMost = za::max(r.errorMost, s.error);
        r.strengthSum += s.strength;
        r.errorN++;
    }
    if(vr_ragtool_log.value >= 2.f && sv.qcvm.time >= r.nextSample)
    {
        r.nextSample = sv.qcvm.time + 0.5;
        Con_Printf("ragtool: %.2f s pelvis %.0f %.0f %.0f (%.1f over the floor) error %.2f deg strength %.2f%s\n", t,
            s.pelvis.x, s.pelvis.y, s.pelvis.z, s.pelvis.z - s.floor, s.error, s.strength, s.driven ? "" : " (not driven)");
    }
    const float length = b.length > 0.f ? b.length : za::max(vr_ragtool_length.value, 0.5f);
    if(t >= length)
    {
        endRun("done");
        releaseHand();
        if(vr_ragtool_loop.value != 0.f)
        {
            respawn(r.behaviour);
        }
    }
}

void setStatus()
{
    const Run& r = tool.run;
    q_snprintf(tool.status, sizeof(tool.status), "%s%s | %s %s | %s x%.2f%s", modelName(tool.subjectModel),
        tool.subject ? "" : " (none)", r.on ? behaviours[r.behaviour].label : behaviours[behaviourRow()].label,
        r.on ? va("%.1f s", sv.qcvm.time - r.begun) : "(ready)", tool.paused ? "PAUSED" : "running",
        za::max(vr_ragtool_speed.value, 0.01f), tool.holding ? " | holding" : "");
}

// ---- Presets ----

[[nodiscard]] const char* presetPath(const char* name)
{
    return va("%s/ragtool/%s.cfg", com_gamedir, name && *name ? name : "preset");
}

void save_f()
{
    const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "preset";
    char dir[MAX_OSPATH];
    q_snprintf(dir, sizeof(dir), "%s/ragtool", com_gamedir);
    Sys_mkdir(dir);
    char path[MAX_OSPATH];
    q_strlcpy(path, presetPath(name), sizeof(path));
    FILE* f = fopen(path, "w");
    if(!f)
    {
        Con_Printf("ragtool: could not write %s\n", path);
        return;
    }
    fprintf(f, "// Ragdoll tool preset (vr_ragtool_save %s): every registry setting; `// default` marks the changed ones.\n",
        name);
    int changed = 0;
    for(const ragtool::Param& p : params)
    {
        const cvar_t* v = Cvar_FindVar(p.cvar);
        if(!v)
        {
            continue;
        }
        const bool differs = v->default_string && strcmp(v->string, v->default_string) != 0;
        changed += differs ? 1 : 0;
        fprintf(f, "%s \"%s\"%s%s%s\n", p.cvar, v->string, differs ? " // default \"" : "",
            differs ? v->default_string : "", differs ? "\"" : "");
    }
    fclose(f);
    Con_Printf("ragtool: saved %d settings (%d changed) to %s\n", paramTotal, changed, path);
}

void load_f()
{
    const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "preset";
    char path[MAX_OSPATH];
    q_strlcpy(path, presetPath(name), sizeof(path));
    FILE* f = fopen(path, "r");
    if(!f)
    {
        Con_Printf("ragtool: no preset %s\n", path);
        return;
    }
    char line[512];
    int set = 0;
    while(fgets(line, sizeof(line), f))
    {
        char cvar[128], value[256];
        if(sscanf(line, "%127s \"%255[^\"]\"", cvar, value) == 2 && cvar[0] != '/')
        {
            if(Cvar_FindVar(cvar))
            {
                Cvar_Set(cvar, value);
                set++;
            }
        }
    }
    fclose(f);
    Con_Printf("ragtool: loaded %d settings from %s\n", set, path);
}

void params_f()
{
    const int only = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : -1;
    int missing = 0, changed = 0;
    for(const ragtool::Param& p : params)
    {
        const cvar_t* v = Cvar_FindVar(p.cvar);
        missing += v ? 0 : 1;
        const bool differs = v && v->default_string && strcmp(v->string, v->default_string) != 0;
        changed += differs ? 1 : 0;
        if(only < 0 || only == p.group)
        {
            Con_Printf("  [%d %s] %s = %s%s%s%s\n", p.group, groupNames[p.group], p.cvar, v ? v->string : "(missing)",
                differs ? " (default " : "", differs ? v->default_string : "", differs ? ")" : "");
        }
    }
    Con_Printf("ragtool: %d settings in %d groups, %d changed, %d missing\n", paramTotal, static_cast<int>(GroupTotal),
        changed, missing);
}

void reset_f()
{
    const int only = Cmd_Argc() > 1 && strcmp(Cmd_Argv(1), "all") != 0 ? Q_atoi(Cmd_Argv(1)) : -1;
    int n = 0;
    for(const ragtool::Param& p : params)
    {
        cvar_t* v = Cvar_FindVar(p.cvar);
        if(v && v->default_string && (only < 0 || only == p.group))
        {
            Cvar_SetQuick(v, v->default_string);
            n++;
        }
    }
    Con_Printf("ragtool: %d settings reset to their defaults\n", n);
}

// ---- Commands ----

void start_f()
{
    tool.on = true;
    tool.flagStarted = true;
    listModels();
    const bool here = Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "here");
    if(here && inGame())
    {
        tool.staged = false; // (found on the next server frame, where the player stands)
        tool.wantMap = false;
    }
    else
    {
        tool.staged = false;
        tool.wantMap = true;
        tool.subject = 0;
        Cbuf_InsertText(va("map %s\n", vr_ragtool_map.string)); // (next: ahead of what a script still has waiting)
    }
    Con_Printf("ragtool: on (%d monsters with a rig; Tab: its menu page)\n", tool.modelCount);
}

void exit_f()
{
    if(!tool.on)
    {
        return;
    }
    const SvVm vm;
    endRun("the tool closed");
    releaseHand();
    if(tool.timescaleSet)
    {
        Cvar_Set("host_timescale", "0");
        tool.timescaleSet = false;
    }
    if(tool.flagsSet && inGame())
    {
        edict_t* p = player();
        p->v.flags = static_cast<float>(static_cast<int>(p->v.flags) & ~(FL_GODMODE | FL_NOTARGET));
    }
    tool.flagsSet = false;
    tool.on = false;
    tool.paused = false;
    Con_Printf("ragtool: off\n");
}

void models_f()
{
    listModels();
    for(int i = 0; i < tool.modelCount; i++)
    {
        const int e = tool.models[static_cast<za::SizeT>(i)];
        Con_Printf("  %d: %s (%s, %s%s)\n", i, toolgun::spawnableLabel(e), toolgun::spawnableClass(e),
            toolgun::spawnableModel(e), *toolgun::spawnablePack(e) ? va(", %s", toolgun::spawnablePack(e)) : "");
    }
    Con_Printf("ragtool: %d models\n", tool.modelCount);
}

void behaviours_f()
{
    for(int i = 0; i < behaviourTotal; i++)
    {
        Con_Printf("  %d: %s - %s\n", i, behaviours[i].name, behaviours[i].label);
    }
}

[[nodiscard]] int findBehaviour(const char* name)
{
    for(int i = 0; i < behaviourTotal; i++)
    {
        if(!strcmp(behaviours[i].name, name))
        {
            return i;
        }
    }
    return name[0] >= '0' && name[0] <= '9' ? za::clamp(Q_atoi(name), 0, behaviourTotal - 1) : -1;
}

void play_f()
{
    if(!tool.on)
    {
        Con_Printf("ragtool: not on (vr_ragtool)\n");
        return;
    }
    int b = behaviourRow();
    if(Cmd_Argc() > 1)
    {
        b = findBehaviour(Cmd_Argv(1));
        if(b < 0)
        {
            Con_Printf("ragtool: no behaviour %s (vr_ragtool_behaviours)\n", Cmd_Argv(1));
            return;
        }
        Cvar_SetValueQuick(&vr_ragtool_behaviour, static_cast<float>(b));
    }
    respawn(b);
}

void spawn_f()
{
    if(Cmd_Argc() > 1)
    {
        listModels();
        int row = -1;
        for(int i = 0; i < tool.modelCount && row < 0; i++)
        {
            const int e = tool.models[static_cast<za::SizeT>(i)];
            row = !q_strcasecmp(toolgun::spawnableClass(e), Cmd_Argv(1)) || !q_strcasecmp(toolgun::spawnableName(e), Cmd_Argv(1))
                      ? i
                      : -1;
        }
        Cvar_SetValueQuick(&vr_ragtool_model, static_cast<float>(row >= 0 ? row : Q_atoi(Cmd_Argv(1))));
    }
    respawn(-1);
}

void pause_f()
{
    tool.paused = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) != 0 : !tool.paused;
    tool.steps = 0;
    Con_Printf("ragtool: %s\n", tool.paused ? "paused" : "running");
}

void step_f()
{
    tool.paused = true;
    tool.steps += za::max(Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 1, 1);
}

void hand_f()
{
    if(Cmd_Argc() < 4)
    {
        Con_Printf("usage: vr_ragtool_hand <dx> <dy> <dz> (units, world; the hand's goal moved)\n");
        return;
    }
    tool.handGoal += glm::vec3{Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
}

void grab_f()
{
    if(tool.holding)
    {
        releaseHand();
        Con_Printf("ragtool: the hand lets go\n");
        return;
    }
    if(!inGame())
    {
        return;
    }
    const SvVm vm;
    const auto h = static_cast<physics::Hold>(
        za::clamp(static_cast<int>(vr_ragtool_hold.value), 0, static_cast<int>(physics::Hold::Count) - 1));
    glm::vec3 grip{0.f};
    if(physics::takeHold(tool.subject, h, grip))
    {
        tool.holding = true;
        tool.hand = tool.handGoal = grip;
        Con_Printf("ragtool: the hand holds his %s\n", physics::holdName(h));
    }
    else
    {
        Con_Printf("ragtool: no %s to hold (a ragdoll only: knock him down first)\n", physics::holdName(h));
    }
}

void status_f()
{
    setStatus();
    Con_Printf("ragtool: %s, %s\n", tool.on ? "on" : "off", tool.status);
    if(inGame() && tool.subject)
    {
        const SvVm vm;
        const physics::Sample s = physics::sample(tool.subject);
        Con_Printf("ragtool: subject %d ragdoll %d driven %d error %.2f deg strength %.2f pelvis %.1f over the floor\n",
            tool.subject, s.ragdoll ? 1 : 0, s.driven ? 1 : 0, s.error, s.strength, s.pelvis.z - s.floor);
    }
}

// The camera: the player's view angles (the mouse) turned about the subject, vr_ragtool_cam_dist off.
void cameraView(float* origin, float* angles)
{
    glm::vec3 focus{0.f};
    bool got = false;
    {
        const SvVm vm;
        got = physics::focus(tool.subject, focus);
    }
    if(!got)
    {
        focus = tool.stage + glm::vec3{0.f, 0.f, 24.f};
    }
    const glm::vec3 a{cl.viewangles[0], cl.viewangles[1], 0.f};
    vec3_t fwd, right, up, av{a.x, a.y, a.z};
    AngleVectors(av, fwd, right, up);
    const float dist = za::clamp(vr_ragtool_cam_dist.value, 16.f, 2000.f);
    const float shift = za::clamp(vr_ragtool_cam_shift.value, -1.f, 1.f) * dist;
    for(int i = 0; i < 3; i++)
    {
        origin[i] = focus[i] - fwd[i] * dist - right[i] * shift;
        angles[i] = a[i];
    }
}
} // namespace

namespace qvr::ragtool
{
bool active()
{
    return tool.on;
}

bool flagGiven()
{
    return tool.flag;
}

void init()
{
    tool.flag = COM_CheckParm("-ragtool") != 0;
    Cmd_AddCommand("vr_ragtool", start_f);
    Cmd_AddCommand("vr_ragtool_exit", exit_f);
    Cmd_AddCommand("vr_ragtool_models", models_f);
    Cmd_AddCommand("vr_ragtool_behaviours", behaviours_f);
    Cmd_AddCommand("vr_ragtool_play", play_f);
    Cmd_AddCommand("vr_ragtool_spawn", spawn_f);
    Cmd_AddCommand("vr_ragtool_pause", pause_f);
    Cmd_AddCommand("vr_ragtool_step", step_f);
    Cmd_AddCommand("vr_ragtool_hand", hand_f);
    Cmd_AddCommand("vr_ragtool_grab", grab_f);
    Cmd_AddCommand("vr_ragtool_params", params_f);
    Cmd_AddCommand("vr_ragtool_reset", reset_f);
    Cmd_AddCommand("vr_ragtool_save", save_f);
    Cmd_AddCommand("vr_ragtool_load", load_f);
    Cmd_AddCommand("vr_ragtool_status", status_f);
}

void serverFrame()
{
    if(!tool.on || !inGame())
    {
        return;
    }
    if(tool.wantMap && strcmp(sv.name, vr_ragtool_map.string) != 0)
    {
        return; // (the room not loaded yet)
    }
    tool.wantMap = false;
    edict_t* p = player();
    if(!tool.flagsSet || !(static_cast<int>(p->v.flags) & FL_NOTARGET))
    {
        p->v.flags = static_cast<float>(static_cast<int>(p->v.flags) | FL_GODMODE | FL_NOTARGET);
        tool.flagsSet = true;
    }
    if(!tool.staged)
    {
        findStage();
        respawn(-1);
        return;
    }
    if(tool.spawnAt >= 0.0 && sv.qcvm.time >= tool.spawnAt)
    {
        makeSubject();
    }
    else if(tool.spawnAt < 0.0 && tool.subject && tool.modelCount > 0 && modelRow() != tool.subjectModel)
    {
        respawn(-1); // (another Model picked on the page: made at once)
    }
    if(tool.holding)
    {
        const glm::vec3 d = tool.handGoal - tool.hand;
        const float most = handSpeed * static_cast<float>(host_frametime);
        const float len = glm::length(d);
        tool.hand += len > most ? d * (most / len) : d;
        if(!box3d::physgunRagdollMove(tool.hand))
        {
            tool.holding = false;
        }
        // (Kept down while the hand holds him, as a player's hand keeps him: QC's VR_Knockdown_Keep, which sees only
        // players' hands, not this one: its timer pushed on.)
        edict_t* e = subjectEnt();
        const int kd = progs::fields().vr_knockdown, until = ED_FindFieldOffset("vr_kd_until");
        if(e && tool.holding && kd >= 0 && until >= 0 && progs::fieldFloat(e, kd) == 1.f && e->v.health > 0.f)
        {
            float& t = progs::fieldFloat(e, until);
            t = za::max(t, static_cast<float>(sv.qcvm.time) + 0.5f);
        }
    }
    stepRun();
}

void draw()
{
    if(!tool.on || !inGame())
    {
        return;
    }
    physics::Show show;
    show.bones = vr_ragtool_show_bones.value != 0.f;
    show.limits = vr_ragtool_show_limits.value != 0.f;
    show.targets = vr_ragtool_show_targets.value != 0.f;
    show.contacts = vr_ragtool_show_contacts.value != 0.f;
    show.balance = vr_ragtool_show_balance.value != 0.f;
    show.scale = vrActive() ? 1.f : za::clamp(vr_ragtool_cam_dist.value / 40.f, 1.f, 12.f); // (flat: about 2 pixels)
    {
        const SvVm vm;
        physics::draw(tool.subject, show);
    }
    if(tool.holding)
    {
        const glm::vec4 yellow{1.f, 0.9f, 0.1f, 1.f};
        lines::point(tool.hand, 3.f, yellow);
        lines::line(tool.hand, tool.handGoal, 0.15f, yellow, glm::vec4{1.f, 0.9f, 0.1f, 0.3f});
    }
}

int modelCount()
{
    listModels();
    return tool.modelCount;
}

const char* modelLabel(int i)
{
    return modelName(i);
}

int behaviourCount()
{
    return behaviourTotal;
}

const char* behaviourLabel(int i)
{
    return i >= 0 && i < behaviourTotal ? behaviours[i].label : "?";
}

const char* behaviourHelp(int i)
{
    return i >= 0 && i < behaviourTotal ? behaviours[i].help : "";
}

int groupCount()
{
    return GroupTotal;
}

const char* groupName(int g)
{
    return g >= 0 && g < GroupTotal ? groupNames[g] : "?";
}

int paramCount()
{
    return paramTotal;
}

const Param& param(int i)
{
    return params[za::clamp(i, 0, paramTotal - 1)];
}

const char* statusLine()
{
    setStatus();
    return tool.status;
}
} // namespace qvr::ragtool

// ---- The engine's hooks (vr_api.h) ----

// V_CalcRefdef: the tool's camera over the player's view (nonzero: it was set).
extern "C" int VR_RagtoolView(float* origin, float* angles)
{
    if(!tool.on || !inGame())
    {
        return 0;
    }
    // (The speed, each frame: host_timescale while the tool runs.)
    const float speed = za::clamp(vr_ragtool_speed.value, 0.02f, 4.f);
    if(speed != 1.f || tool.timescaleSet)
    {
        if(Cvar_VariableValue("host_timescale") != (speed == 1.f ? 0.f : speed))
        {
            Cvar_SetValue("host_timescale", speed == 1.f ? 0.f : speed);
        }
        tool.timescaleSet = speed != 1.f;
    }
    cameraView(origin, angles);
    return 1;
}

// V_RenderView: the queued debug lines (the subject's overlay, and any other debug draw's) over the flat view, x-ray, as
// a headset's eye draws them in its UI pass (vr_stereo.cpp drawUi).
extern "C" void VR_RagtoolOverlay(void)
{
    if(!tool.on || vrActive())
    {
        return;
    }
    lines::drawInEye(vec(r_refdef.vieworg));
}

// Host_ServerFrame: nonzero while the tool holds the game still (paused, no step left to run).
extern "C" int VR_RagtoolHolds(void)
{
    if(!tool.on || !tool.paused)
    {
        return 0;
    }
    if(tool.steps > 0)
    {
        tool.steps--;
        return 0;
    }
    return 1;
}

// Key_Event in the game: the tool's keys (nonzero: taken).
extern "C" int VR_RagtoolKey(int key, int down)
{
    if(!tool.on || key_dest != key_game)
    {
        return 0;
    }
    const auto press = [&](void (*fn)()) {
        if(down)
        {
            fn();
        }
        return 1;
    };
    const auto handBy = [&](float dx, float dy, float dz) {
        if(down)
        {
            vec3_t fwd, right, up, av{0.f, cl.viewangles[1], 0.f};
            AngleVectors(av, fwd, right, up);
            tool.handGoal += glm::vec3{fwd[0], fwd[1], 0.f} * dy + glm::vec3{right[0], right[1], 0.f} * dx +
                             glm::vec3{0.f, 0.f, dz};
        }
        return 1;
    };
    constexpr float handStep = 6.f;
    switch(key)
    {
    case K_TAB: return press([] { menu::jumpToRagtool(); });
    case K_SPACE: return press([] { Cbuf_AddText("vr_ragtool_play\n"); });
    case 'p': return press([] { Cbuf_AddText("vr_ragtool_pause\n"); });
    case '.': return press([] { Cbuf_AddText("vr_ragtool_step\n"); });
    case '[':
        return press([] { Cvar_SetValueQuick(&vr_ragtool_speed, za::max(vr_ragtool_speed.value / 1.5f, 0.05f)); });
    case ']':
        return press([] { Cvar_SetValueQuick(&vr_ragtool_speed, za::min(vr_ragtool_speed.value * 1.5f, 2.f)); });
    case 'r': return press([] { Cbuf_AddText("vr_ragtool_spawn\n"); });
    case 'b':
        return press([] {
            Cvar_SetValueQuick(&vr_ragtool_behaviour, static_cast<float>((behaviourRow() + 1) % behaviourTotal));
            Con_Printf("ragtool: behaviour %s\n", behaviours[behaviourRow()].label);
        });
    case 'm':
        return press([] {
            listModels();
            Cvar_SetValueQuick(&vr_ragtool_model, static_cast<float>((modelRow() + 1) % za::max(tool.modelCount, 1)));
            Cbuf_AddText("vr_ragtool_spawn\n");
        });
    case 'h':
        return press([] {
            Cvar_SetValueQuick(&vr_ragtool_hold,
                static_cast<float>((static_cast<int>(vr_ragtool_hold.value) + 1) % static_cast<int>(physics::Hold::Count)));
            Con_Printf("ragtool: hold %s\n", physics::holdName(static_cast<physics::Hold>(static_cast<int>(vr_ragtool_hold.value))));
        });
    case 'g': return press([] { Cbuf_AddText("vr_ragtool_grab\n"); });
    case 'l': return press([] { Cvar_SetValueQuick(&vr_ragtool_loop, vr_ragtool_loop.value != 0.f ? 0.f : 1.f); });
    case K_MWHEELUP: return press([] { Cvar_SetValueQuick(&vr_ragtool_cam_dist, za::max(vr_ragtool_cam_dist.value * 0.9f, 24.f)); });
    case K_MWHEELDOWN:
        return press([] { Cvar_SetValueQuick(&vr_ragtool_cam_dist, za::min(vr_ragtool_cam_dist.value * 1.1f, 1500.f)); });
    case K_UPARROW: return handBy(0.f, handStep, 0.f);
    case K_DOWNARROW: return handBy(0.f, -handStep, 0.f);
    case K_LEFTARROW: return handBy(-handStep, 0.f, 0.f);
    case K_RIGHTARROW: return handBy(handStep, 0.f, 0.f);
    case K_PGUP: return handBy(0.f, 0.f, handStep);
    case K_PGDN: return handBy(0.f, 0.f, -handStep);
    default: return 0;
    }
}

void qvr::ragtool::clientFrame()
{
    if(tool.flag && !tool.flagStarted)
    {
        tool.flagStarted = true;
        Cbuf_InsertText("vr_ragtool\n"); // (next: ahead of what a script still has waiting)
    }
}
