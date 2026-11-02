// vr_playersteps.cpp -- the player's own footsteps on the drawn feet; see vr_playersteps.hpp and ROUND23.md, "The player's
// walk".
//
// - The old trigger. QC's player_run (one think in 0.1 s while moving): every 95 units or so on the ground, one of
//   misc/foot1..7.wav at 0.5, ATTN_NORM, from the player, whatever the legs are doing.
// - On the feet (vr_body_step_sounds). A touchdown of the drawn legs (avatar::Skeleton::steps) plays the same recordings at
//   the same volume from that foot's place, on the client, in the frame the foot is drawn down. fromLegs() is the one rule
//   for both sides, so that a step is either the foot's or the old trigger's, never both: the old one asks oldStepSilent.
// - The old trigger keeps: the setting off; no walking legs drawn (a shorter body, vr_body_walk 0, no VR); in the air (it
//   only sounds on the ground: the landing's step, as it always was); swimming and climbing (the legs aren't walking,
//   whatever the ground says); multiplayer (bots too): the server's sound goes to every client and can't leave one out, and
//   the others can't see your feet's phase, so everyone there hears the old trigger.
// - Standing steps (turning on the spot, moving in the room) are a shuffle: vr_footstep_player_turn_vol of a step's volume.
//   The old trigger had none: it only runs while the stick moves you.
// - One step never twice: a landing's old step right before the first touchdown, or the other way round, within mergeGap,
//   is the one sound (the touchdown is still counted, and the footprints have it).

#include "vr_playersteps.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_physsound.hpp"
#include "vr_random.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/glm.hpp>

using namespace qvr;

namespace
{

constexpr const char* stepSounds[] = {"misc/foot1.wav", "misc/foot2.wav", "misc/foot3.wav", "misc/foot4.wav",
                                      "misc/foot5.wav", "misc/foot6.wav", "misc/foot7.wav"};
constexpr int stepSoundCount = static_cast<int>(sizeof(stepSounds) / sizeof(stepSounds[0]));
constexpr float stepVolume = 0.5f;  // QC player_run's
constexpr float stepAtten = 1.f;    // ATTN_NORM
constexpr float soleLift = 6.f;     // units: the sound this far over the floor (as the enemies': not in it, for occlusion)
constexpr double mergeGap = 0.12;   // s: two steps closer than this, from the two sources, are one

// vr_body_step_report's numbers since its last call.
struct Report
{
    double time{0.0};
    double distance{0.0};
    double strideSum{0.0};
    double apartSum{0.0}; // the feet's distance across the walk, world units
    int frames{0};
    int touchdowns[2]{0, 0};
    int standing{0};  // of those, a standing step's
    int sounds{0};    // played from a foot
    int merged{0};    // a touchdown heard as the old step just before it
    int oldPlayed{0};
    int oldSilent{0};
    double errSum{0.0}; // s, a sound from the frame its foot is drawn down
    double errMost{0.0};
    int errCount{0};
    float lowest[2]{1e9f, 1e9f}; // the ankle's height, each foot
    float plantedMost{0.f};      // ... the most a planted foot stood over its lowest
};

struct State
{
    unsigned heard[2]{0u, 0u};
    za::U32 pick{0x51ed2701u};
    double lastFoot{-1.0}; // vr_gametime: the last sound from a foot
    double lastOld{-1.0};  // ... of the old trigger's
    bool armed{false};     // vr_body_step_report has been called: the frames are measured
    double lastFrame{-1.0};
    double soundAt[2]{-1.0, -1.0}; // each foot's last sound, until its contact is seen
    float lastAlong[2]{0.f, 0.f};  // the drawn foot along the walk, from its hip
    bool coming[2]{false, false};  // ... and it is swinging forward
    Report report;
    testkit::Tally tally;
};

State state;

[[nodiscard]] bool fromLegs()
{
    if(vr_body_step_sounds.value == 0.f || vr_body_walk.value == 0.f || cl.maxclients > 1)
    {
        return false;
    }
    avatar::Skeleton k;
    return avatar::skeleton(k) && k.legs && avatar::walkState().onFeet;
}

void playStep(const avatar::Skeleton& k, int side, float volume)
{
    const float t = vrand::xorshift01(state.pick);
    sfx_t* sfx = S_PrecacheSound(stepSounds[za::min(stepSoundCount - 1, static_cast<int>(t * stepSoundCount))]);
    const glm::vec3 at = k.ankle[side] + glm::vec3{0.f, 0.f, soleLift - k.sole[side]};
    vec3_t org{at.x, at.y, at.z};
    // (World, not the player: the view entity's sounds aren't placed. Any free channel, as the old one's.)
    S_StartSoundPitch(0, 0, sfx, org, volume, stepAtten, physsound::jitterPitch());
}

// What the report wants of each pose: the time, the ground covered, and when each drawn foot actually comes down (the
// forward end of its swing, seen from where it is drawn against its hip: as vr_footsteps.cpp reads an enemy's), against its
// sound; and how far a foot that should be planted stands off the floor (the stride's ends past the leg's reach).
void measure(const avatar::Skeleton& k, const avatar::WalkState& walk, double now)
{
    Report& r = state.report;
    const double dt = state.lastFrame >= 0.0 ? now - state.lastFrame : 0.0;
    if(dt <= 0.0 && state.lastFrame >= 0.0)
    {
        return;
    }
    state.lastFrame = now;
    r.time += dt;
    r.distance += dt * glm::length(glm::vec2{cl.velocity[0], cl.velocity[1]});
    r.strideSum += walk.stride;
    const glm::vec2 across = glm::vec2{-walk.dir.y, walk.dir.x};
    r.apartSum += za::abs(glm::dot(glm::vec2{k.ankle[0] - k.ankle[1]}, across));
    r.frames++;
    for(int side = 0; side < 2; side++)
    {
        const float h = k.ankle[side].z;
        const float along = glm::dot(k.ankle[side] - k.hip[side], walk.dir);
        const bool forward = along > state.lastAlong[side];
        if(state.coming[side] && !forward)
        {
            // The last frame was the foot's last going forward: its contact.
            if(state.soundAt[side] >= 0.0)
            {
                const double err = za::abs(state.soundAt[side] - (now - dt));
                r.errSum += err;
                r.errMost = za::max(r.errMost, err);
                r.errCount++;
                state.soundAt[side] = -1.0;
            }
        }
        state.coming[side] = forward;
        state.lastAlong[side] = along;
        r.lowest[side] = za::min(r.lowest[side], h);
        if(walk.walking && walk.planted[side])
        {
            r.plantedMost = za::max(r.plantedMost, (h - r.lowest[side]) / k.m2w);
        }
    }
}

// vr_body_step_report [<label> [legs|old|standing]]: the walk since the last call: its rate (counted touchdowns, not the
// formula), stride, how the planted foot slides, each source's sounds and how far they were from the drawn contact. legs:
// every touchdown was heard once from its foot, none from the old trigger. old: the reverse. standing: standing steps, each
// heard. `reset`: forgets them, unprinted. `done`: the verdict line, `playersteps_test: N passed, M failed`.
void report_f()
{
    state.armed = true;
    const char* label = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!strcmp(label, "done"))
    {
        state.tally.verdict("playersteps_test");
        return;
    }
    const Report r = state.report;
    state.report = Report{};
    if(!strcmp(label, "reset"))
    {
        return;
    }
    const int steps = r.touchdowns[0] + r.touchdowns[1];
    const int walked = steps - r.standing;
    const double uPerS = r.time > 0.0 ? r.distance / r.time : 0.0;
    const double m2w = avatar::legsMetreUnits();
    const double rate = r.time > 0.0 ? static_cast<double>(walked) / r.time : 0.0;
    const double stride = r.frames > 0 ? r.strideSum / r.frames : 0.0;
    const double slide = uPerS > 1.0 ? 100.0 * (1.0 - stride * rate / (uPerS / m2w)) : 0.0;
    Con_Printf("playersteps %s: %.2f s at %.0f u/s (%.2f m/s): L %d R %d, %.2f steps/s, stride %.2f m, planted foot slides "
               "%.0f%%, off the floor %.1f cm, feet apart %.2f m; sounds %d merged %d standing %d, old played %d silent %d; sound error mean "
               "%.1f ms most %.1f ms (%d)\n",
        label, r.time, uPerS, uPerS / m2w, r.touchdowns[0], r.touchdowns[1], rate, stride, slide, r.plantedMost * 100.f,
        r.frames > 0 ? r.apartSum / r.frames / m2w : 0.0, r.sounds,
        r.merged, r.standing, r.oldPlayed, r.oldSilent, r.errCount > 0 ? 1000.0 * r.errSum / r.errCount : 0.0, 1000.0 * r.errMost,
        r.errCount);
    if(Cmd_Argc() > 2)
    {
        const char* expect = Cmd_Argv(2);
        const bool ok = !strcmp(expect, "legs")
                            ? walked >= 4 && r.sounds + r.merged == steps && r.oldPlayed == 0 && r.errCount > 0 && r.errMost < 0.03
                        : !strcmp(expect, "standing") ? r.standing >= 2 && r.sounds == steps && r.oldPlayed == 0
                                                      : r.sounds == 0 && r.oldPlayed > 0;
        state.tally.check(ok, "playersteps_test", label);
    }
}

} // namespace

namespace qvr::playersteps
{

void frame(const avatar::Skeleton& k)
{
    const bool feet = fromLegs();
    const avatar::WalkState walk = avatar::walkState();
    const double now = vr_gametime;
    for(int side = 0; side < 2; side++)
    {
        if(state.heard[side] == k.steps[side])
        {
            continue;
        }
        state.heard[side] = k.steps[side];
        Report& r = state.report;
        r.touchdowns[side]++;
        r.standing += walk.walking ? 0 : 1;
        if(!feet)
        {
            continue;
        }
        if(state.lastOld >= 0.0 && now - state.lastOld < mergeGap)
        {
            r.merged++;
            continue;
        }
        const float share = walk.walking ? 1.f : za::clamp(vr_footstep_player_turn_vol.value, 0.f, 1.f);
        if(share > 0.f)
        {
            playStep(k, side, stepVolume * share);
        }
        state.lastFoot = now;
        state.soundAt[side] = now;
        r.sounds++;
    }
    if(state.armed)
    {
        measure(k, walk, now);
    }
}

bool oldStepSilent(int ent)
{
    if(ent != cl.viewentity || cls.state != ca_connected)
    {
        return false; // (another player's, a bot's)
    }
    const double now = vr_gametime;
    if(fromLegs() || (state.lastFoot >= 0.0 && now - state.lastFoot < mergeGap))
    {
        state.report.oldSilent++;
        return true;
    }
    state.lastOld = now;
    state.report.oldPlayed++;
    return false;
}

void registerCommands()
{
    Cmd_AddCommand("vr_body_step_report", report_f);
}

} // namespace qvr::playersteps
