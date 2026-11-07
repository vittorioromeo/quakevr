// vr_footsteps.cpp -- enemies' footsteps; see vr_footsteps.hpp and docs/vr-port/ROUND23.md, "Enemy footsteps".
//
// - When. The contacts are read from the model's own frames, once a model (analyse), so they can't drift from what is
//   drawn; nothing per frame is kept by hand, and a replaced model gets its own. Each foot's vertices are the low part
//   of its shin bone in the rest pose (the ragdoll rig, as vr_aglegs.cpp finds them). Along each of the model's
//   animations (the runs vr_mdldequant.cpp splits them into, loops and all) a foot swings forward, comes down and is
//   then carried back as the body goes on over it: a contact is the first moment after the swing's forward end that the
//   foot is also down on the floor. Its frame and how far into the lerp to that frame (Quake's clients blend from the
//   last frame to the new one over the think's interval) are the table's. A monster entering such a frame steps then,
//   that far into the interval: no timer, and any animation (a charge's, an attack's) steps if the monster moves on it.
// - Who. The bipeds in `kinds`, by their drawn model: an armed humanoid's frames are its plain model's, so it needs nothing
//   of its own. Arcane Dimensions' monsters have their own models and their own footsteps (ad_monster_footstep): not here.
//   Dogs, spawn, scrags, fish, vores: not bipeds.
// - Whether. Alive, on the ground, a walking monster (MOVETYPE_STEP), not in a liquid, not a ragdoll, and going at
//   vr_footstep_min_speed or faster (a pain's or an attack's shift of weight on the spot is silent).
// - How loud. By its speed over its last frames: vr_footstep_walk_vol at vr_footstep_walk_speed up to vr_footstep_run_vol at
//   vr_footstep_run_speed, carrying from vr_footstep_walk_atten to vr_footstep_run_atten; its kind's weight on top.
// - The heavy ones (Sole::Thud) play at vr_footstep_heavy_speed (the playback rate: pitch and length together, as the kind's
//   own pitch and the jitter, which it multiplies). They carry: vr_footstep_heavy_volume louder, and vr_footstep_heavy_carry on their distance for the
//   falloff, the range and who counts as nearest: a heavy step is heard from further, over nearer boots.
// - The budget. Any free channel (never its voice's or its weapon's). Only the nearest vr_footstep_nearest of those moving
//   within vr_footstep_range of a player are heard, at vr_footstep_rate steps a second at most: a crowd is the few by you.
//
// Everything goes through the server's datagram (physsound::emitSound), by precache index.

#include "vr_footsteps.hpp"

#include "vr_cvars.hpp"
#include "vr_geom.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_physsound.hpp"
#include "vr_progs.hpp"
#include "vr_ragdoll.hpp"
#include "vr_mdldequant.hpp"
#include "vr_random.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/glm.hpp>
#include "vr/vr_ea.h"

using namespace qvr;

namespace
{

enum class Sole
{
    Boot,  // leather on the floor (grunts, enforcers)
    Metal, // armoured boots (knights)
    Bare,  // wet bare feet (zombies, gremlins)
    Thud,  // great bare feet (ogres, shamblers, fiends)
    Count
};

constexpr int soleCount = static_cast<int>(Sole::Count);
constexpr int soleTakes = 5;

constexpr const char* soleFiles[soleCount][soleTakes] = {
    {"vr/step_boot1.wav", "vr/step_boot2.wav", "vr/step_boot3.wav", "vr/step_boot4.wav", "vr/step_boot5.wav"},
    {"vr/step_metal1.wav", "vr/step_metal2.wav", "vr/step_metal3.wav", "vr/step_metal4.wav", "vr/step_metal5.wav"},
    {"vr/step_bare1.wav", "vr/step_bare2.wav", "vr/step_bare3.wav", "vr/step_bare4.wav", "vr/step_bare5.wav"},
    {"vr/step_thud1.wav", "vr/step_thud2.wav", "vr/step_thud3.wav", "vr/step_thud4.wav", "vr/step_thud5.wav"},
};

struct Kind
{
    modelmeta::Id id;
    Sole sole;
    float pitch; // a bigger body's lower
    float gain;  // and heavier
};

constexpr Kind kinds[] = {
    {modelmeta::Id::Soldier, Sole::Boot, 1.f, 1.f},
    {modelmeta::Id::VrGrunt, Sole::Boot, 1.f, 1.f},
    {modelmeta::Id::Enforcer, Sole::Boot, 0.9f, 1.1f},
    {modelmeta::Id::VrEnforcer, Sole::Boot, 0.9f, 1.1f},
    {modelmeta::Id::Knight, Sole::Metal, 1.f, 1.f},
    {modelmeta::Id::VrKnight, Sole::Metal, 1.f, 1.f},
    {modelmeta::Id::Hknight, Sole::Metal, 0.82f, 1.2f},
    {modelmeta::Id::VrHknight, Sole::Metal, 0.82f, 1.2f},
    {modelmeta::Id::Mg3Rknight, Sole::Metal, 0.95f, 1.1f},
    {modelmeta::Id::Zombie, Sole::Bare, 1.f, 0.9f},
    {modelmeta::Id::Mummy, Sole::Bare, 0.92f, 1.f},
    {modelmeta::Id::Grem, Sole::Bare, 1.3f, 0.6f},
    {modelmeta::Id::Ogre, Sole::Thud, 1.1f, 1.15f},
    {modelmeta::Id::Mg3OgreRocket, Sole::Thud, 1.1f, 1.15f},
    {modelmeta::Id::Demon, Sole::Thud, 1.25f, 1.f},
    {modelmeta::Id::Shambler, Sole::Thud, 0.85f, 1.4f},
    {modelmeta::Id::Mg3ShamblerBlood, Sole::Thud, 0.85f, 1.4f},
};

// The rules for reading a contact off the frames, as shares of a foot's swing (forward) and lift along one animation.
constexpr float footShare = 0.35f;  // a foot: its shin's vertices this low in the rest pose (of the shin's length)
constexpr float minSwing = 0.2f;    // of the shin's length: a foot that swings less isn't stepping in this animation
constexpr float seamShare = 0.6f;   // a run's last and first poses this near along the swing: a loop
constexpr float topShare = 0.1f;    // the swing's forward end: within this of the foot's foremost
constexpr float rearmShare = 0.35f; // back this far before it swings forward again (a shuffle at the top: one step)
constexpr float stanceShare = 0.6f; // back this far and it never came down in this swing: no step
constexpr float downUnits = 1.5f;   // down: within this of the rest pose's sole, the floor, or of the foot's lowest in the
constexpr float downShare = 0.2f;   // animation (a run's foot may never reach the floor), or this share of its lift if more
constexpr float airShare = 0.75f;   // ... but never higher than this over the sole (of the shin's length): a kick

constexpr int minRun = 4;           // poses: shorter runs (a flinch) don't step
constexpr int nearestMost = 16;     // vr_footstep_nearest's cap
constexpr float soleLift = 6.f;     // units: the sound this far over the floor (so that it isn't in it, for occlusion)
constexpr float minFrameTime = 0.05f; // s: a frame's pace over at least this (an animation cut short mid-think)
constexpr float paceEase = 0.4f;    // a frame's share of the pace (monsters move in a frame's jumps)
constexpr float teleport = 96.f;    // units in a frame: more is no walking
constexpr float defaultFrame = 0.1f;
constexpr double stepsBurst = 3.0;  // vr_footstep_rate's bucket
constexpr float errGoodMs = 40.f;   // vr_footstep_report's limits: sound vs the drawn contact
constexpr float liftGood = 5.f;     // ... the drawn foot's mean height over its sole then

constexpr int kindCount = static_cast<int>(sizeof(kinds) / sizeof(kinds[0]));

struct Contact
{
    float frac{-1.f}; // into the lerp to this frame (0 .. 1); negative: none
    glm::vec2 foot{0.f};
};

struct Stats
{
    int steps[2]{0, 0}; // under and over the middle of walk to run
    double volume[2]{0.0, 0.0};
    double errSum{0.0};
    float errMost{0.f};
    double liftSum{0.0};
    int dropped{0};
    int contacts{0};
    int measured{0}; // the steps with the drawn pose to compare with
    double distSum{0.0};
    double earSum{0.0}; // how loud it gets to the nearest player's head: its volume after the falloff (no walls, no air)
};

struct ModelSteps
{
    const qmodel_t* model{nullptr};
    const Kind* kind{nullptr};
    bool ok{false};
    int numFrames{0};
    za::Vector<Contact> contact; // [frame * 2 + side]
    za::Vector<int> pose;        // each frame's
    za::Vector<float> footZ;     // [pose * 2 + side]: the foot's lowest
    za::Vector<float> footX;     // [pose * 2 + side]: its middle along the body
    Stats stats;
};

struct EntSteps
{
    int modelindex{0};
    ModelSteps* steps{nullptr};
    int frame{-1};
    int lastFrame{-1};       // the one before
    double frameAt{0.0};
    float frameLen{defaultFrame};
    glm::vec2 at{0.f};       // where it was at the last frame change
    float pace{0.f};         // units a second
    double due[2]{0.0, 0.0}; // a foot's contact to come (0: none)
    glm::vec2 foot[2]{};
    int footFrame[2]{0, 0};
};

struct State
{
    bool ready{false};
    int indices[soleCount][soleTakes]{};
    int last[soleCount]{};
    ModelSteps models[kindCount]; // a slot a kind, until the next map (entities point to them)
    int numModels{0};
    ModelSteps* byIndex[MAX_MODELS]{};
    bool looked[MAX_MODELS]{};
    za::Vector<EntSteps> ents;
    double lastTime{0.0};
    double tokens{stepsBurst};
    za::U32 pickState{0x51f15e77u};
    double frameSeconds{0.0};
    int framesTimed{0};
    int droppedBudget{0};
};

State state;

[[nodiscard]] int boneNamed(const ragdoll::Rig& rig, const char* name)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        if(!ZA_STRCMP(rig.bones[b].name, name))
        {
            return b;
        }
    }
    return -1;
}

// The contacts of `side`'s foot along one animation `run` (poses; `loop`: its last goes on to its first).
void readRun(ModelSteps& m, const za::Vector<int>& poseFrame, const za::Vector<glm::vec3>& feet,
    const za::Vector<int>& run, bool knownLoop, int side, float shin, float sole)
{
    const int n = static_cast<int>(run.size());
    const auto X = [&](int i) { return feet[static_cast<za::SizeT>(run[i]) * 2 + side].x; };
    const auto Z = [&](int i) { return feet[static_cast<za::SizeT>(run[i]) * 2 + side].z; };
    float xLo = X(0), xHi = X(0), zLo = Z(0), zHi = Z(0);
    int back = 0;
    for(int i = 1; i < n; i++)
    {
        back = X(i) < xLo ? i : back;
        xLo = za::min(xLo, X(i));
        xHi = za::max(xHi, X(i));
        zLo = za::min(zLo, Z(i));
        zHi = za::max(zHi, Z(i));
    }
    const float swing = xHi - xLo;
    if(swing < minSwing * shin)
    {
        return;
    }
    // (a loop the progs don't spell out, such as the armed humanoids' computed frames: its ends meet)
    const float seam = X(n - 1) - X(0);
    const bool loop = knownLoop || za::max(seam, -seam) <= seamShare * swing;
    const float xTop = xHi - topShare * swing;
    const float xRearm = xHi - rearmShare * swing;
    const float xStance = xHi - stanceShare * swing;
    const float zDown = za::min(za::max(zLo + za::max(downUnits, downShare * (zHi - zLo)), sole + downUnits), sole + airShare * shin);
    const int start = loop ? back : 0;
    bool armed = X(start) <= xRearm;
    for(int k = 0; k < n; k++)
    {
        const int i = (start + k) % n;
        if(!armed)
        {
            armed = X(i) <= xRearm;
            continue;
        }
        if(X(i) < xTop)
        {
            continue;
        }
        armed = false;
        for(int q = 0; q < n; q++)
        {
            if(!loop && i + q >= n)
            {
                break;
            }
            const int j = (i + q) % n;
            if(X(j) < xStance)
            {
                break;
            }
            if(Z(j) > zDown)
            {
                continue;
            }
            const int frame = poseFrame[static_cast<za::SizeT>(run[j])];
            if(frame < 0)
            {
                break;
            }
            float frac = 0.f;
            if(loop || j > 0)
            {
                const int p = (j + n - 1) % n;
                const float fz = Z(p) > zDown ? (Z(p) - zDown) / za::max(Z(p) - Z(j), 0.01f) : 0.f;
                const float fx = X(p) < xTop ? (xTop - X(p)) / za::max(X(j) - X(p), 0.01f) : 0.f;
                frac = za::clamp(za::max(fz, fx), 0.f, 1.f);
            }
            Contact& c = m.contact[static_cast<za::SizeT>(frame) * 2 + side];
            c.frac = frac;
            const glm::vec3& f = feet[static_cast<za::SizeT>(run[j]) * 2 + side];
            c.foot = glm::vec2{f.x, f.y};
            m.stats.contacts++;
            break;
        }
    }
}

// A model's table (see the top). False: no feet found on it.
bool analyse(ModelSteps& m, qmodel_t* model)
{
    const ragdoll::Rig* rig = ragdoll::rigFor(model);
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    if(!rig || !hdr || hdr->numverts != rig->numVerts || hdr->numposes < minRun)
    {
        return false;
    }
    mdldequant::prepare(model);
    const za::SizeT nv = static_cast<za::SizeT>(hdr->numverts);
    const za::SizeT np = static_cast<za::SizeT>(hdr->numposes);
    za::Vector<int> foot[2];
    float shinLen[2]{0.f, 0.f};
    float soleZ = 1e9f;
    const char* shins[2] = {"shin_r", "shin_l"};
    const char* boots[2] = {"foot_r", "foot_l"};
    for(int s = 0; s < 2; s++)
    {
        const int shin = boneNamed(*rig, shins[s]);
        const int boot = boneNamed(*rig, boots[s]);
        if(shin < 0)
        {
            return false;
        }
        float lo = 1e9f;
        for(za::SizeT v = 0; v < nv; v++)
        {
            if(rig->vertBone[v] == shin || (boot >= 0 && rig->vertBone[v] == boot))
            {
                lo = za::min(lo, mdldequant::modelVertex(hdr, 0, 0, 0.f, static_cast<int>(v)).z);
            }
        }
        const float len = rig->bones[shin].pivot.z - lo;
        if(lo > 1e8f || len < 2.f)
        {
            return false;
        }
        shinLen[s] = len;
        soleZ = za::min(soleZ, lo);
        for(za::SizeT v = 0; v < nv; v++)
        {
            if((rig->vertBone[v] == shin || (boot >= 0 && rig->vertBone[v] == boot)) &&
                mdldequant::modelVertex(hdr, 0, 0, 0.f, static_cast<int>(v)).z < lo + footShare * len)
            {
                foot[s].pushBack(static_cast<int>(v));
            }
        }
        if(foot[s].empty())
        {
            return false;
        }
    }

    // Each pose's feet: the foot's lowest, and its middle along the body.
    za::Vector<glm::vec3> feet;
    feet.resize(np * 2);
    m.footZ.resize(np * 2);
    m.footX.resize(np * 2);
    for(za::SizeT p = 0; p < np; p++)
    {
        for(int s = 0; s < 2; s++)
        {
            float zLo = 1e9f;
            glm::vec2 sum{0.f};
            for(const int v : foot[s])
            {
                const glm::vec3 r = mdldequant::modelVertex(hdr, static_cast<int>(p), static_cast<int>(p), 0.f, v);
                zLo = za::min(zLo, r.z);
                sum += glm::vec2{r.x, r.y};
            }
            sum /= static_cast<float>(foot[s].size());
            feet[p * 2 + s] = glm::vec3{sum.x, sum.y, zLo};
            m.footZ[p * 2 + s] = zLo;
            m.footX[p * 2 + s] = sum.x;
        }
    }

    m.numFrames = hdr->numframes;
    m.contact.resize(static_cast<za::SizeT>(m.numFrames) * 2);
    m.pose.resize(static_cast<za::SizeT>(m.numFrames));
    za::Vector<int> poseFrame;
    poseFrame.resize(np);
    for(za::SizeT p = 0; p < np; p++)
    {
        poseFrame[p] = -1;
    }
    for(int f = 0; f < m.numFrames; f++)
    {
        const int first = hdr->frames[f].firstpose;
        m.pose[static_cast<za::SizeT>(f)] = first;
        if(hdr->frames[f].numposes == 1 && first >= 0 && first < hdr->numposes)
        {
            poseFrame[static_cast<za::SizeT>(first)] = f;
        }
    }

    // Its animations: from each pose nothing leads to, then the loops left.
    za::Vector<short> next;
    za::Vector<unsigned char> progs;
    za::Vector<unsigned char> led, seen;
    next.resize(np);
    progs.resize(np);
    led.resize(np);
    seen.resize(np);
    for(za::SizeT p = 0; p < np; p++)
    {
        led[p] = 0;
        seen[p] = 0;
    }
    mdldequant::poseSuccessors(hdr, model->name, next.data(), progs.data());
    for(za::SizeT p = 0; p < np; p++)
    {
        if(next[p] >= 0 && next[p] < hdr->numposes && static_cast<za::SizeT>(next[p]) != p)
        {
            led[static_cast<za::SizeT>(next[p])] = 1;
        }
    }
    za::Vector<int> run;
    for(int pass = 0; pass < 2; pass++)
    {
        for(za::SizeT first = 0; first < np; first++)
        {
            if(seen[first] || (pass == 0 && led[first]))
            {
                continue;
            }
            run.clear();
            int p = static_cast<int>(first);
            bool loop = false;
            while(p >= 0 && p < hdr->numposes && !seen[static_cast<za::SizeT>(p)])
            {
                seen[static_cast<za::SizeT>(p)] = 1;
                run.pushBack(p);
                const int after = next[static_cast<za::SizeT>(p)];
                loop = after == static_cast<int>(first);
                p = after;
            }
            if(static_cast<int>(run.size()) >= minRun)
            {
                for(int s = 0; s < 2; s++)
                {
                    readRun(m, poseFrame, feet, run, loop, s, shinLen[s], soleZ);
                }
            }
        }
    }
    return true;
}

[[nodiscard]] ModelSteps* stepsOf(int modelindex)
{
    if(modelindex <= 0 || modelindex >= MAX_MODELS)
    {
        return nullptr;
    }
    if(state.looked[modelindex])
    {
        return state.byIndex[modelindex];
    }
    state.looked[modelindex] = true;
    qmodel_t* model = sv.models[modelindex];
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    const Kind* kind = nullptr;
    for(const Kind& k : kinds)
    {
        kind = modelmeta::is(model, k.id) ? &k : kind;
    }
    if(!kind)
    {
        return nullptr;
    }
    if(state.numModels >= kindCount)
    {
        return nullptr;
    }
    ModelSteps* m = &state.models[state.numModels++];
    m->model = model;
    m->kind = kind;
    m->ok = analyse(*m, model);
    Con_DPrintf("footsteps: %s: %d contacts%s\n", model->name, m->stats.contacts, m->ok ? "" : " (no feet found)");
    state.byIndex[modelindex] = m->ok ? m : nullptr;
    return state.byIndex[modelindex];
}

// How far a kind's step counts its distance (the falloff, the range, the nearest): the heavy ones as if nearer.
[[nodiscard]] float carryOf(const Kind& kind)
{
    return kind.sole == Sole::Thud ? za::max(vr_footstep_heavy_carry.value, 0.05f) : 1.f;
}

[[nodiscard]] bool grounded(const edict_t* e, int num)
{
    return e->v.health > 0.f && (static_cast<int>(e->v.flags) & FL_ONGROUND) && static_cast<int>(e->v.movetype) == MOVETYPE_STEP &&
           e->v.watertype >= CONTENTS_EMPTY && static_cast<int>(e->v.solid) != SOLID_NOT && !ragdoll::published(num);
}

// The sound's way from the server: a take of its kind's sole, never the last one, at the foot.
void play(const edict_t* e, int num, EntSteps& st, int side, float pace, float distSq, double now)
{
    ModelSteps& m = *st.steps;
    const Kind& kind = *m.kind;
    const int sole = static_cast<int>(kind.sole);
    const int pick = static_cast<int>(vrand::xorshift32(state.pickState) % (soleTakes - 1));
    const int take = pick >= state.last[sole] ? pick + 1 : pick;
    state.last[sole] = take;

    const float ramp = za::clamp((pace - vr_footstep_walk_speed.value) /
                                     za::max(vr_footstep_run_speed.value - vr_footstep_walk_speed.value, 1.f),
        0.f, 1.f);
    const float volume = za::min((vr_footstep_walk_vol.value + (vr_footstep_run_vol.value - vr_footstep_walk_vol.value) * ramp) *
                                     kind.gain * (kind.sole == Sole::Thud ? vr_footstep_heavy_volume.value : 1.f) * vr_footstep_volume.value,
        1.f);
    const float carry = carryOf(kind);
    const float atten = (vr_footstep_walk_atten.value + (vr_footstep_run_atten.value - vr_footstep_walk_atten.value) * ramp) * carry;

    const float yaw = e->v.angles[YAW] * (3.14159265f / 180.f);
    const float cs = za::cos(yaw), sn = za::sin(yaw);
    const glm::vec2 foot = st.foot[side];
    const glm::vec3 at{e->v.origin[0] + foot.x * cs - foot.y * sn, e->v.origin[1] + foot.x * sn + foot.y * cs,
        e->v.origin[2] + e->v.mins[2] + soleLift};
    const float rate = kind.pitch * (kind.sole == Sole::Thud ? vr_footstep_heavy_speed.value : 1.f) * physsound::jitterPitch();
    if(!physsound::emitSound(num, 0, state.indices[sole][take], volume, atten, at, rate))
    {
        return;
    }

    // The report's: this sound against the foot as the client draws it now.
    Stats& s = m.stats;
    const int fast = ramp >= 0.5f ? 1 : 0;
    s.steps[fast]++;
    s.volume[fast] += volume;
    const float dist = za::sqrt(distSq) / carry; // (distSq: as counted, carried)
    const float ear = volume * za::max(1.f - dist * atten * vr_snd_falloff.value / 1000.f, 0.f);
    s.distSum += dist;
    s.earSum += ear;
    float err = 0.f, lift = 0.f;
    // (only what is being drawn: the renderer keeps an entity's lerp, and an unseen one's is old)
    if(cl_entities && num < cl.num_entities && cl.time - cl_entities[num].lerpstart <= 2.0 * st.frameLen)
    {
        const entity_t& d = cl_entities[num];
        s.measured++;
        const int pose2 = m.pose[static_cast<za::SizeT>(st.footFrame[side])];
        const float frac = m.contact[static_cast<za::SizeT>(st.footFrame[side]) * 2 + side].frac;
        const bool drawn = d.currentpose == pose2 && d.lerptime > 0.f;
        // (its lerp is on, or the next frame's has just begun: the contact was that far before its start; or not drawn yet)
        const bool past = !drawn && d.previouspose == pose2 && d.lerptime > 0.f;
        const double start = drawn ? d.lerpstart : past ? d.lerpstart - st.frameLen : cl.time;
        const float len = drawn ? d.lerptime : st.frameLen;
        err = static_cast<float>((cl.time - (start + frac * len)) * 1000.0);
        const float blend = drawn ? za::clamp(static_cast<float>((cl.time - d.lerpstart) / za::max(d.lerptime, 1e-3f)), 0.f, 1.f) : 0.f;
        const int pose1 = drawn && d.previouspose >= 0 && static_cast<za::SizeT>(d.previouspose) < m.footZ.size() / 2 ? d.previouspose : pose2;
        const float z = m.footZ[static_cast<za::SizeT>(pose1) * 2 + side] +
                        (m.footZ[static_cast<za::SizeT>(pose2) * 2 + side] - m.footZ[static_cast<za::SizeT>(pose1) * 2 + side]) * blend;
        lift = z - m.footZ[static_cast<za::SizeT>(m.pose[0]) * 2 + side];
    }
    const float absErr = za::max(err, -err);
    s.errSum += absErr;
    s.errMost = za::max(s.errMost, absErr);
    s.liftSum += za::max(lift, -lift);
    if(vr_footstep_log.value > 0.f)
    {
        Con_Printf("footstep: %s ent %d %s frame %d pace %.0f vol %.2f rate %.3f atten %.2f dist %.0f ear %.3f err %.0f ms lift %.1f t %.3f\n", m.model->name, num,
            side == 0 ? "right" : "left", st.footFrame[side], pace, volume, rate, atten, dist, ear, err, lift, now);
    }
}

void infoLine(const ModelSteps& m)
{
    Con_Printf("%s: %d frames, %d contacts:", m.model->name, m.numFrames, m.stats.contacts);
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(m.model)));
    for(int f = 0; f < m.numFrames; f++)
    {
        for(int s = 0; s < 2; s++)
        {
            const Contact& c = m.contact[static_cast<za::SizeT>(f) * 2 + s];
            if(c.frac >= 0.f)
            {
                Con_Printf(" %s/%c.%02d", hdr ? hdr->frames[f].name : "?", s == 0 ? 'r' : 'l', static_cast<int>(c.frac * 100.f));
            }
        }
    }
    Con_Printf("\n");
}

// vr_footstep_info: each model's contacts so far (frame name / foot . how far into the lerp, %).
// vr_footstep_info <model substring> <frame> <count>: that model's feet over those frames, for reading the rule against.
void info_f()
{
    if(Cmd_Argc() >= 4)
    {
        const int first = Q_atoi(Cmd_Argv(2));
        const int count = Q_atoi(Cmd_Argv(3));
        for(int i = 0; i < state.numModels; i++)
        {
            const ModelSteps& m = state.models[i];
            if(!m.ok || !strstr(m.model->name, Cmd_Argv(1)))
            {
                continue;
            }
            const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(m.model)));
            for(int f = first; f < first + count && f < m.numFrames; f++)
            {
                const za::SizeT p = static_cast<za::SizeT>(m.pose[static_cast<za::SizeT>(f)]);
                const Contact& r = m.contact[static_cast<za::SizeT>(f) * 2];
                const Contact& l = m.contact[static_cast<za::SizeT>(f) * 2 + 1];
                Con_Printf("%s %3d %-10s r x %6.1f z %6.1f %s | l x %6.1f z %6.1f %s\n", m.model->name, f, hdr ? hdr->frames[f].name : "?", m.footX[p * 2],
                    m.footZ[p * 2], r.frac >= 0.f ? "STEP" : "    ", m.footX[p * 2 + 1], m.footZ[p * 2 + 1], l.frac >= 0.f ? "STEP" : "");
            }
        }
        return;
    }
    for(int i = 0; i < state.numModels; i++)
    {
        const ModelSteps& m = state.models[i];
        if(m.ok)
        {
            infoLine(m);
        }
        else
        {
            Con_Printf("%s: no feet found\n", m.model->name);
        }
    }
    Con_Printf("footsteps: %s; %d dropped by the budget; %.1f us a frame over %d frames\n", state.ready ? "ready" : "not ready (not Quake VR's progs)",
        state.droppedBudget, state.framesTimed > 0 ? state.frameSeconds * 1e6 / state.framesTimed : 0.0, state.framesTimed);
}

// vr_footstep_report: each model's steps heard: walking and running, how loud, and how far the sound is from the drawn
// contact; `footstep_test: N passed, M failed` on what a test expects (walking softer than running, the sounds on the
// contacts). `vr_footstep_report quiet` without that line.
void report_f()
{
    int passed = 0, failed = 0;
    for(int i = 0; i < state.numModels; i++)
    {
        const ModelSteps& m = state.models[i];
        const Stats& s = m.stats;
        const int steps = s.steps[0] + s.steps[1];
        if(steps == 0)
        {
            continue;
        }
        const double walk = s.steps[0] ? s.volume[0] / s.steps[0] : 0.0;
        const double run = s.steps[1] ? s.volume[1] / s.steps[1] : 0.0;
        const double err = s.measured ? s.errSum / s.measured : 0.0;
        const double lift = s.measured ? s.liftSum / s.measured : 0.0;
        const bool softer = !s.steps[0] || !s.steps[1] || walk < run;
        const bool onIt = err <= errGoodMs && lift <= liftGood;
        Con_Printf("footstep_test: %s %s: walk %d steps vol %.2f, run %d steps vol %.2f; %d drawn: err mean %.1f most %.1f ms; foot over its sole %.1f units; at %.0f units, %.3f at the head\n",
            softer && onIt ? "PASS" : "FAIL", m.model->name, s.steps[0], walk, s.steps[1], run, s.measured, err, s.errMost, lift, s.distSum / steps, s.earSum / steps);
        (softer && onIt ? passed : failed)++;
    }
    if(Cmd_Argc() < 2)
    {
        Con_Printf("footsteps: %d dropped by the budget; %.1f us a frame\n", state.droppedBudget,
            state.framesTimed > 0 ? state.frameSeconds * 1e6 / state.framesTimed : 0.0);
        testkit::verdict("footstep_test", passed, failed);
    }
}

} // namespace

void qvr::footsteps::precache()
{
    state.ready = false;
    state.ents.clear();
    for(int i = 0; i < state.numModels; i++)
    {
        state.models[i] = ModelSteps{};
    }
    state.numModels = 0;
    for(int i = 0; i < MAX_MODELS; i++)
    {
        state.byIndex[i] = nullptr;
        state.looked[i] = false;
    }
    state.lastTime = 0.0;
    state.droppedBudget = 0;
    state.frameSeconds = 0.0;
    state.framesTimed = 0;
    if(!progs::bindings().isVrProgs || sv.state != ss_loading)
    {
        return;
    }
    for(int s = 0; s < soleCount; s++)
    {
        for(int k = 0; k < soleTakes; k++)
        {
            state.indices[s][k] = physsound::precacheOne(soleFiles[s][k]);
        }
    }
    state.ready = true;
}

void qvr::footsteps::registerCommands()
{
    Cmd_AddCommand("vr_footstep_info", info_f);
    Cmd_AddCommand("vr_footstep_report", report_f);
}

extern "C" void VR_Footsteps_Frame(void)
{
    if(!state.ready || qcvm != &sv.qcvm || vr_footstep_volume.value <= 0.f)
    {
        return;
    }
    const double now = qcvm->time;
    const double dt = now - state.lastTime;
    state.lastTime = now;
    if(dt <= 0.0)
    {
        return;
    }
    const double wall = Sys_DoubleTime();
    state.tokens = za::min(state.tokens + dt * za::max(vr_footstep_rate.value, 0.f), stepsBurst);

    glm::vec3 ears[MAX_SCOREBOARD];
    int numEars = 0;
    for(int c = 0; c < svs.maxclients && c < MAX_SCOREBOARD; c++)
    {
        if(svs.clients[c].active && svs.clients[c].edict)
        {
            const float* o = svs.clients[c].edict->v.origin;
            ears[numEars++] = glm::vec3{o[0], o[1], o[2]};
        }
    }

    const float rangeSq = vr_footstep_range.value * vr_footstep_range.value;
    const int keep = za::clamp(static_cast<int>(vr_footstep_nearest.value), 0, nearestMost);
    float nearest[nearestMost];
    int numNearest = 0;
    if(state.ents.size() < static_cast<za::SizeT>(qcvm->num_edicts))
    {
        state.ents.resize(static_cast<za::SizeT>(qcvm->num_edicts));
    }

    // Two passes over the moving: the pace and the contacts coming due first, the nearest counted, then those due heard.
    struct Due
    {
        int num;
        int side;
        float distSq;
    };
    Due dues[32];
    int numDue = 0;

    EA_OPEN(64) for(int i = svs.maxclients + 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || !(static_cast<int>(e->v.flags) & FL_MONSTER))
        {
            continue;
        }
        EntSteps& st = state.ents[static_cast<za::SizeT>(i)];
        const int modelindex = static_cast<int>(e->v.modelindex);
        if(st.modelindex != modelindex)
        {
            st = EntSteps{};
            st.modelindex = modelindex;
            st.steps = stepsOf(modelindex);
            st.at = glm::vec2{e->v.origin[0], e->v.origin[1]};
        }
        if(!st.steps)
        {
            continue;
        }
        const int frame = static_cast<int>(e->v.frame);
        if(frame != st.frame)
        {
            const glm::vec2 here{e->v.origin[0], e->v.origin[1]};
            const float moved = glm::length(here - st.at);
            const float since = static_cast<float>(now - st.frameAt);
            if(st.frame >= 0 && since > 0.f && moved <= teleport)
            {
                st.pace += (moved / za::max(since, minFrameTime) - st.pace) * paceEase;
            }
            st.at = here;
            st.frameAt = now;
            // (a jump to another animation cuts off a contact still to come: that foot's frame isn't drawn any more)
            if(frame != st.frame + 1)
            {
                st.due[0] = st.due[1] = 0.0;
            }
            st.lastFrame = st.frame;
            st.frame = frame;
            const float interval = static_cast<float>(e->v.nextthink - now);
            st.frameLen = interval > 0.03f && interval <= 0.3f ? interval : defaultFrame;
            if(frame >= 0 && frame < st.steps->numFrames)
            {
                for(int s = 0; s < 2; s++)
                {
                    const Contact& c = st.steps->contact[static_cast<za::SizeT>(frame) * 2 + s];
                    if(c.frac >= 0.f)
                    {
                        st.due[s] = now + c.frac * st.frameLen;
                        st.foot[s] = c.foot;
                        st.footFrame[s] = frame;
                    }
                }
            }
        }
        if(!grounded(e, i) || st.pace < vr_footstep_min_speed.value)
        {
            st.due[0] = st.due[1] = 0.0;
            continue;
        }
        float distSq = 1e18f;
        for(int k = 0; k < numEars; k++)
        {
            const glm::vec3 d = ears[k] - glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
            distSq = za::min(distSq, glm::dot(d, d));
        }
        const float carried = carryOf(*st.steps->kind);
        distSq *= carried * carried;
        if(distSq > rangeSq)
        {
            st.due[0] = st.due[1] = 0.0;
            continue;
        }
        // The nearest `keep` moving so far (sorted, the farthest last).
        if(numNearest < keep || (keep > 0 && distSq < nearest[numNearest - 1]))
        {
            int at = numNearest < keep ? numNearest++ : numNearest - 1;
            while(at > 0 && nearest[at - 1] > distSq)
            {
                nearest[at] = nearest[at - 1];
                at--;
            }
            nearest[at] = distSq;
        }
        for(int s = 0; s < 2; s++)
        {
            if(st.due[s] > 0.0 && now >= st.due[s] && numDue < 32)
            {
                dues[numDue++] = Due{i, s, distSq};
                st.due[s] = 0.0;
            }
        }
    } EA_CLOSE(64)

    const float limit = numNearest > 0 ? nearest[numNearest - 1] : -1.f;
    for(int k = 0; k < numDue; k++)
    {
        // The nearest first among those due now.
        int best = k;
        for(int j = k + 1; j < numDue; j++)
        {
            best = dues[j].distSq < dues[best].distSq ? j : best;
        }
        const Due d = dues[best];
        dues[best] = dues[k];
        EntSteps& st = state.ents[static_cast<za::SizeT>(d.num)];
        if(d.distSq > limit || state.tokens < 1.0)
        {
            state.droppedBudget++;
            st.steps->stats.dropped++;
            continue;
        }
        state.tokens -= 1.0;
        play(EDICT_NUM(d.num), d.num, st, d.side, st.pace, d.distSq, now);
    }
    state.frameSeconds += Sys_DoubleTime() - wall;
    state.framesTimed++;
}
