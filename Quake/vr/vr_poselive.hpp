// vr_poselive.hpp -- the Pose Studio's Live mode (vr_posestudio.hpp; ROUND22.md, "Pose Studio: Live"; docs/SETTINGS.md).
// The studio's subject runs its real gameplay loops where it stands (QC VR_AGK_Live: its own AI against a target moving
// round it: aim and fire, reload, run, its strikes, pain, a pickup; the panel's Loop picks one or all) at 0.1x..1x (the
// panel's Speed: host_timescale), the other armed humanoids still frozen. Every frame of it drawn goes on a tape (the
// last ~10 s of game time): the arms' solve's inputs as relinkSkinned has them (vr_hitreact.cpp's arm pose: its targets
// on the body as drawn; its frames and place drawn), the solve's state before it (vr_agarms, vr_aglegs, vr_agbody: the
// eases, the elbows' continuity), the pose keys QC laid on and their shares, and the glitch finder's measures of it
// (vr_agglitch.hpp). Pause holds the game (its clock all but stopped) and draws the subject from the tape: the
// playhead's frame solved again from what was recorded, exactly; scrubbed with either stick (left/right) or the panel.
// Dragging a key's part at a paused frame edits the key that weighs most there (moved by the drag over its share:
// vr_posestudio_live_follow), laid on the recorded frames by their shares and solved again from
// vr_posestudio_live_warm seconds before (so the eases see the edit as play would). Next Glitch: the tape's worst
// moments by the finder's measures, worst first, paused there. Play goes on from where the game was paused.

#pragma once

#include "vr_agarms.hpp"
#include "vr_agbody.hpp"
#include "vr_agglitch.hpp"
#include "vr_aglegs.hpp"
#include "vr_agpose.hpp"

#include "Zancle/String/String.hpp"

#include <glm/glm.hpp>

namespace qvr::poselive
{

enum Loop : int
{
    LoopAll = 0, // the glitch finder's 19 s: aim and fire round it, a run, strikes up close, pain, a pickup
    LoopAim,     // aim and fire at a target circling and rising and falling
    LoopReload,  // ... its magazine emptied every 3 s: reloads
    LoopRun,     // a target far off: a run (in place)
    LoopStrikes, // a target circling close: its strikes
    LoopPickup,  // its weapon knocked out, taken up again
    NumLoops
};

inline constexpr int maxUses = 8;        // keys QC lays on in one frame
inline constexpr int poseBytes = 768;    // vr_hitreact.cpp's arm pose
inline constexpr int entBytes = 512;     // ... its entity

// A key QC laid on the subject in a frame: its code, share, whether mirrored (a left hook), the key as it was.
struct KeyUse
{
    int code{-1};
    float w{0.f};
    bool mirror{false};
    agpose::Key key;
};

struct Frame
{
    double time{0.0};                  // cl.time it was drawn at
    int pose1{0}, pose2{0};            // its frames' poses as drawn and their blend
    float blend{0.f};
    int frame{0};
    unsigned char ent[entBytes];       // its entity as drawn (its place, its frames and their lerps: vr_hitreact.cpp)
    unsigned char pose[poseBytes];     // the arm pose (vr_hitreact.cpp)
    int armsLen{0}, legsLen{0}, bodyLen{0};
    unsigned char arms[agarms::stateBytes];
    unsigned char legs[aglegs::stateBytes];
    unsigned char body[agbody::stateBytes];
    int uses{0};
    KeyUse use[maxUses];
    // measured after its solve (the glitch finder's)
    bool measured{false};
    agglitch::Sample sample;
    float m[agglitch::numMetrics]{};
    signed char side[agglitch::numMetrics]{};
    float sev{0.f};                    // the worst metric over its glitch value (1: a glitch)
    int worst{0};
};

void init(); // the commands (vr_posestudio_live*)

[[nodiscard]] bool on();     // Live (playing or paused)
[[nodiscard]] bool paused();
[[nodiscard]] int subject(); // its entity (0 none)
[[nodiscard]] int loop();
[[nodiscard]] float speed();
[[nodiscard]] int restarts(); // the loop started again this many times (QC: from its start)

bool start(int ent); // false: said why
void stop();         // Frozen again (the time scale put back)
void togglePause();
void cycleLoop();
void cycleSpeed();
void scrub(float seconds); // pauses; the playhead moved by seconds of game time (negative: back)
void step(int frames);
void nextGlitch();

// The panel's lines (posestudio): mode, playhead, the key weighing most, the measures; and the bars of the measures
// (each from character 0 of lines `firstBar`..).
void describe(za::String& out, int& measureLine);
[[nodiscard]] float measureShare(int i, bool& bad); // the i-th measure shown (pen, flip, reach, grip2, jump): value over its glitch value

// Paused: the key weighing most at the playhead (-1: none), its share, whether mirrored there.
[[nodiscard]] int dominant(float& w, bool& mirror);

// Once a host frame while the studio runs (posestudio::frame): the time scale; a stick's scrub.
void frame();
[[nodiscard]] bool sticksTaken(); // paused: the sticks scrub (no walking, no turning)
void sticks(const glm::vec2& off, const glm::vec2& main);

// What the studio's hold tells QC for `ent` (posestudio::qcHold): the loop (-1: not live).
[[nodiscard]] int qcLoop(int ent);
// QC (weapondrawnpose(e, +-(code + share), 23)): a key laid on `ent` this server frame (negative: mirrored).
void qcKey(int ent, float arg);

// relinkArms (vr_hitreact.cpp). Playing: the subject's frame recorded (record: the next to fill, null none; recorded:
// filled, after its solve).
[[nodiscard]] bool recording(int ent);
Frame* record();
void recorded();
// Paused: the subject drawn from the tape: `count` frames from `first` to the playhead solved in turn (it alone with no
// edits laid on there); at(first, k) the k-th of them.
[[nodiscard]] bool replaying(int ent);
bool window(int& first, int& count);
[[nodiscard]] const Frame& at(int first, int k);
// The keys' edits since frame `f` was recorded, by its shares (false: none).
bool delta(const Frame& f, agpose::Key& out);
void setWarming(bool on); // a window's frame before the playhead solving now: nothing noted
[[nodiscard]] bool warming();

// agarms::solve's measures of the subject's frame (the glitch finder's sample).
[[nodiscard]] bool measuring(int ent);
void sample(const agglitch::Sample& s);

} // namespace qvr::poselive
