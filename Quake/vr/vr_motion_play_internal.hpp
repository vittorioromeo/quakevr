#pragma once
// vr_motion_play_internal.hpp -- what vr_motion_play.cpp (the playback) shares with vr_motion_eval.cpp (vr_motion_eval,
// vr_motion_header_check). Internal: included by those two alone. Namespace motion::detail: the types and the state
// (once vr_motion_play.cpp's anonymous namespace) and the functions the other file calls.
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_motion.hpp"
#include "vr_motion_take.hpp"
#include "vr_zancle.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/Pair.hpp"

#include <glm/glm.hpp>

namespace qvr::motion::detail
{

// The evaluation's map loads run on fixed frames too (a server frame with each, at 72 Hz), from the "map"
// command to the take: every take starts at the same server time, in the same state, whatever the machine
// (the QC's 32-bit floats and its entities' think times depend on the absolute time).
extern bool fixedLoading;

struct Frame
{
    double t{0.0};
    double dt{1.0 / 90.0};
    Phase phase{PhaseRec};
    bool tick{false};
    double svDt{0.0};
    TrackingState tracking; // the runtime's poses and the controllers
    float playYaw{0.f};
    bool helping[HAND_COUNT]{};

    // What the take says the game did, to compare with: the hands and head in the dummy's frame
    // (units), and the melee's events (their `at` in the player's frame).
    bool hasD{false};
    glm::vec3 handD[HAND_COUNT]{};
    bool hasW{false};                 // world, with the monster's origin
    glm::vec3 handW[HAND_COUNT]{};
    glm::vec3 headD{0.f};
    za::Vector<Event> events;

    // For placing the player (the first frame's).
    bool hasOrg{false};
    glm::vec3 org{0.f};     // world (the client's)
    bool hasSvOrg{false};
    glm::vec3 svOrg{0.f};   // world (the server's)
    bool hasVel{false};
    glm::vec3 velPF{0.f};   // the player's velocity (player frame, units/s)
    glm::vec3 leanPF{0.f};  // player frame
    bool hasMon{false};
    bool hasMonW{false};
    za::String monClass;
    glm::vec3 monW{0.f};
    float monYawW{0.f};
    glm::vec3 monPF{0.f};
    int wid[HAND_COUNT]{};
    int wflags[HAND_COUNT]{};
};

struct Take
{
    za::String path;
    za::String name; // the file's name
    ankerl::unordered_dense::map<za::String, za::String> header;
    za::String label, category, detail, map;
    bool hasYaw0{false};
    float yaw0{0.f};
    bool hasTicks{false};
    bool hasPlayYaw{false};
    bool hasXrTime{false}; // the frames' poses' own times (xr_time, in tracking.time): the runtime's, steadier than dt
    bool hasWeapons{false};
    bool dummyAttacks{false}; // recorded with the training dummy striking back (the header's "dummy attacks: on")
    za::Vector<Frame> frames;
    size_t firstRec{0};
};

enum class State
{
    Idle,
    Setup, // placing the player, the weapons, the grips, holding the first pose
    Play,  // the take's frames
    Post,  // a moment holding its last pose (late events), then the report
};

struct Options
{
    za::String target;   // a classname, or #<entity number>; "" the take's monster's class, else the dummy
    bool yawSet{false};
    float yaw{0.f};       // the player's heading (the take's yaw0) in the world
    bool place{true};
    bool watch{false};    // at the recorded pace (else as fast as the frames go)
    bool save{false};     // the replay as a take, into motions/replays/
    bool recorded{false}; // the melee settings as recorded too
    bool quiet{false};
    float rate{0.f};      // resampled to this many frames a second (0: the take's own frames)
    float jitter{0.f};    // with rate: each frame's time off by up to this fraction of a frame (a headset's timing)
    int stutter{0};       // with rate: every this many frames one is dropped (the frame before lasts two; 0 none)
    // Tests (Misc/quakevr/melee_wall_takes.sh): the take's monster put at a point, not at a target (`at <x y z yaw>`:
    // the player in front of it as a thing there facing `yaw` would have him, as for another target), and the main
    // hand's weapon another (`weapon <id>`, QC WID_*; -1 the take's).
    bool atSet{false};
    glm::vec3 at{0.f};
    float atYaw{0.f};
    int weapon{-1};
};

// A playback's outcome.
struct Report
{
    za::String file;
    int frames{0};
    za::Vector<Event> recorded; // the take's events (from its start)
    za::Vector<Event> replayed; // the replay's
    za::Vector<double> recordedT, replayedT;
    double errMax[HAND_COUNT]{};
    double errSum[HAND_COUNT]{};
    int errCount{0};
    int parryFrames[HAND_COUNT]{};
    int parryAnyFrames{0}; // either hand's weapon or crossed arms
    int guardFrames{0};
    int recFrames{0};
    int wid[HAND_COUNT]{};
    za::String targetClass;
    za::String targetName; // its classname (the training dummy's: its enemy's, as the melee's events name it)
    za::String warnings;
    bool ok{false};
};

// The take's hits against the replay's: each recorded hit matched by a replayed one of the same kind,
// sub, hand, target and striking point; the damage and time differences of those matched.
struct Match
{
    int recordedHits{0};
    int replayedHits{0};
    int matched{0};
    double maxDamageDiff{0.0}; // relative
    double maxTimeDiff{0.0};
};

// vr_motion_play.cpp: the playback's state (State::Idle: none), its take, and the evaluation's hook on a report.
extern State state;
extern Take take;
extern void (*onDone)(const Report&);

// vr_motion_play.cpp: the settings in a take's header, the test options, the events, a playback started or stopped.
[[nodiscard]] bool anchorSetting(const za::String& name);
[[nodiscard]] bool decodeAnchor(const cvar_t& var, za::String& value);
void collectSettings(const za::String& value, bool pairs, za::Vector<za::Pair<za::String, za::String>>& out);
[[nodiscard]] bool parseTestOption(int& i, Options& o);
void applyLineOptions(const char* s, Options& o);
bool isHit(const Event& e);
[[nodiscard]] za::String eventText(const Event& e);
[[nodiscard]] Match matchHits(const Report& r);
[[nodiscard]] za::String eventsText(const za::Vector<Event>& events, const za::Vector<double>& times, bool hitsAndPushes);
void stopPlayback(const char* why);
[[nodiscard]] bool startPlayback(const za::String& path, const Options& o);

// vr_motion_eval.cpp: the evaluation's step each frame, and its commands.
void evalFrame();
void eval_f();
void headerCheck_f();

} // namespace qvr::motion::detail
