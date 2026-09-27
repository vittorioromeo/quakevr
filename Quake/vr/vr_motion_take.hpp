// vr_motion_take.hpp -- the motion recorder's takes (vr_motion.cpp, vr_motion_play.cpp): what a frame of
// a take holds, and its CSV file (docs/vr-port/MOTIONS.md documents every column).
//
// A row holds a host frame (the headset's rate): the runtime's tracking as it came (tracking space,
// what playback feeds back in), the hands and head as the game placed them (world), the weapons, and
// the last server frame's sample (the player, the nearest monster, the QC's striking points and parry
// state) with the melee events of this frame. The file gives positions in the player's frame: from
// the player's origin (that frame's), turned by the take's yaw (the head's at its start), x forward,
// y left, z up, in Quake units (_u) and metres (_m).

#pragma once

#include "vr_backend.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace qvr::motion
{

// Bumped when a column changes meaning (added columns don't: readers go by name).
constexpr int formatVersion = 1;

// A melee event (QC VR_Motion_Event): kind, its sub-kind, the hand (-1: both, or not a hand's),
// a value (damage, or a blow's strength), where (world), against what, a note.
struct Event
{
    std::string kind;
    std::string sub;
    int hand{-1};
    float value{0.f};
    bool hasAt{false};
    glm::vec3 at{0.f};
    std::string target;
    std::string detail;
};

struct Point
{
    glm::vec3 at{0.f};
    std::string name;
};

// A server frame's view (VR_ServerFrameEnd), shared by the host frames until the next one.
struct ServerSample
{
    double time{0.0}; // sv.time
    bool player{false};
    glm::vec3 origin{0.f};
    glm::vec3 velocity{0.f};
    bool onGround{false};

    bool monster{false}; // the nearest monster (or the training dummy)
    int monEnt{0};
    std::string monClass;
    glm::vec3 monOrigin{0.f};
    glm::vec3 monMins{0.f};
    glm::vec3 monMaxs{0.f};
    std::string monTargetname;
    glm::vec3 monAngles{0.f};
    float monHealth{0.f};

    // VR_Motion_Sample (QC): the hands' striking points, and named values (parry, guard, ...).
    bool qc{false};
    std::vector<Point> points[HAND_COUNT];
    std::vector<std::pair<std::string, glm::vec3>> values;

    [[nodiscard]] const glm::vec3* value(const char* key) const;
};

struct HandRow
{
    // As the game placed it (world; angles in Quake's convention, the gun angle offsets included).
    glm::vec3 pos{0.f};
    glm::vec3 rot{0.f};
    glm::vec3 vel{0.f};    // m/s, world axes (the runtime's velocity turned with the play space)
    glm::vec3 angVel{0.f}; // rad/s, likewise

    HandInput input;       // the runtime's (the recorder's button taken out)
    float curl[5]{};       // thumb, index, middle, ring, pinky: 0 open .. 1 curled (as drawn)

    int wid{0};            // QC weapon id (0: empty), its flags, and its model
    int wflags{0};
    std::string model;
    bool helping{false};   // steadies the other hand's weapon
    int grip2h{0};         // this hand's weapon held two-handed: 0 no, 1 by the foregrip, 2 by the blade
    float twoHand{0.f};    // how far into the two-handed grip (0..1)
    bool carried{false};   // a gun hanging from this hand's foregrip (the hand-off)
    int hotspot{0};
    bool muzzleOk{false};
    glm::vec3 muzzle{0.f};

    Pose raw;              // the runtime's pose (tracking space)
};

enum Phase : int
{
    PhasePre = 0,  // before the button (vr_motion_preroll)
    PhaseRec = 1,  // the button held: the labelled motion
    PhaseTail = 2, // after it (vr_motion_tail)
};

struct Row
{
    double realtime{0.0};
    double dt{0.0};      // the engine's frame time (since the last frame)
    double xrTime{-1.0}; // the runtime's time of the poses
    bool tick{false};    // a server frame ran in this host frame,
    double svDt{0.0};    // with this frame time
    Phase phase{PhaseRec};

    glm::vec3 origin{0.f}; // the player's (the client's: the frame the hands are placed in)
    glm::vec3 lean{0.f};
    glm::vec3 head{0.f};
    glm::vec3 headAngles{0.f};
    glm::vec3 headVel{0.f}; // m/s
    glm::vec3 aim{0.f};     // the view angles sent to the server (the main hand's aim)
    float bodyYaw{0.f};
    float crouch{0.f};
    float playYaw{0.f};     // the play space's turn (degrees)

    Pose rawHead;
    HandRow hands[HAND_COUNT];

    std::shared_ptr<const ServerSample> sv;
    std::vector<Event> events;
};

// What the header says about a take.
struct TakeInfo
{
    std::string label;
    std::string category; // the label's category ("": none of them), and the rest of it
    std::string detail;
    std::string note;
    std::string date;     // YYYY-MM-DD HH:MM:SS
    std::string map;
    std::string source;   // "headset (<runtime>)", "mock", "replay of <file>"
    float yaw0{0.f};      // the player frame's heading: the head's world yaw at t = 0
    glm::vec3 origin0{0.f};
    double t0{0.0};       // realtime at t = 0
    int take{0};          // its number among the label's takes
};

// Writes a take (header and rows); false (with a console message) if it can't.
[[nodiscard]] bool writeTake(const std::string& path, const TakeInfo& info, const std::vector<Row>& rows);

// The categories, named by the result expected (vr_motion_category indexes them), each with its
// optional details (vr_motion_detail; the first, "", is none). A take's label is the category, or
// <category>_<detail>.
struct Choice
{
    const char* name;    // in file names and expect.cfg
    const char* display; // in the menu
};
struct Category
{
    Choice choice;
    std::vector<Choice> details;
};
[[nodiscard]] const std::vector<Category>& categories();
[[nodiscard]] const Category& chosenCategory();
[[nodiscard]] const Choice& chosenDetail();
[[nodiscard]] std::string chosenLabel();

// The category of a label ("" if none: a label of vr_motion_record's own).
[[nodiscard]] std::string categoryOf(const std::string& label);

// quakevr/motions (made if missing), and a label made safe for a file name.
[[nodiscard]] std::string motionsDir();
[[nodiscard]] std::string safeLabel(const std::string& label);

// The latest server sample (null before the first), and the events of the frame so far.
[[nodiscard]] std::shared_ptr<const ServerSample> latestSample();

} // namespace qvr::motion
