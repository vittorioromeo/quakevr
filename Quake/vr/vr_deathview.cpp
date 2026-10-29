// vr_deathview.cpp -- see vr_deathview.hpp.
//
// The Immersive view: the body's head bone as drawn this frame (ragdoll::drawnPart) carries a point between its pivot
// (the neck) and its end (the brow): the eyes. The view's anchor follows it, smoothed; your own head's motion since the
// view went there is added (turned by the view's turn), so your steps and turns are still yours; the view's yaw follows
// the head's (its forward and up flattened: a face up has no forward), smoothed and rate-limited, never its pitch or
// roll. A body gone (gibbed, retired) leaves the view where it was until you respawn.
//
// Vomit Mode (vr_death_view_vomit; the author's note vrfiringrange_2026-10-23_01-18-00, a player's request): over the
// rest, the view is the head bone's own: the eyes where they are this frame and its whole turn (pitch, yaw and roll),
// no smoothing, no turn limit; your own head's steps are left out (only the eyes' spacing kept), its turn is put on top of
// the bone's (its yaw from where you faced as the view went there), as if turning the body's head.
// It needs a body (vr_death_view 1 or 2); a menu still takes the view out to Third Person's place.

#include "vr_deathview.hpp"

#include "vr_comfortfade.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_progs.hpp"
#include "vr_ragdoll.hpp"
#include "vr_geom.hpp"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

namespace qvr::deathview
{
namespace
{

constexpr float eyeAlong = 0.7f; // the eyes: this share of the way from the head bone's pivot (the neck) to its end

struct State
{
    int frame{-1};          // host_framecount of the last update
    bool active{false};     // the Immersive view holds
    bool reached{false};    // it reached the head this death (a body gone later: the view stays)
    int doll{0};            // the body (edict), 0 none
    bool headValid{false};  // the head bone drawn this frame
    glm::vec3 headEyes{0.f}; // where its eyes are now (world)
    float headYaw{0.f};     // its yaw now (degrees)
    glm::quat headRot{1.f, 0.f, 0.f, 0.f}; // its whole turn now (forward +x, left +y, up +z: Vomit Mode)
    glm::vec3 anchor{0.f};  // the view's (smoothed)
    glm::vec3 trackedRef{0.f}; // the tracked head when the view went there
    float yawRef{0.f};      // the head's yaw then
    float yaw{0.f};         // the view's turn now (degrees)
    double last{0.0};       // realtime of the last update
    glm::vec3 eye0{0.f};    // the first eye's view as given (tests)
    glm::vec3 eye0Angles{0.f}; // and its angles
    // How far the view is in the head (0 Third Person's place .. 1 in it): out while a menu or the console is open (the
    // author's note vrfiringrange_2026-10-08_22-42-47: a menu was nearly unusable from the head), in again after,
    // eased over vr_death_view_menu_time each way.
    float inHead{1.f};
    bool vomitRefSet{false}; // Vomit Mode: your head's yaw taken as the bone's straight ahead
    float vomitYawRef{0.f};
};
State state;

[[nodiscard]] edict_t* serverEdict(int num)
{
    if(!sv.active || num <= 0 || num >= sv.qcvm.num_edicts)
    {
        return nullptr;
    }
    edict_t* ed = reinterpret_cast<edict_t*>(reinterpret_cast<byte*>(sv.qcvm.edicts) + num * sv.qcvm.edict_size);
    return ed->free ? nullptr : ed;
}

// The ragdoll body of player `player` (a listen server's edicts; QC vr_deathdoll.qc): its edict, 0 none.
[[nodiscard]] int dollOf(int player)
{
    const progs::FieldOffsets& f = progs::fields();
    if(!sv.active || player <= 0 || player > svs.maxclients || f.vr_deathdoll_body < 0 || f.vr_deathdoll < 0)
    {
        return 0;
    }
    edict_t* p = serverEdict(player);
    const int ref = p ? progs::fieldInt(p, f.vr_deathdoll_body) : 0;
    if(ref <= 0 || sv.qcvm.edict_size <= 0)
    {
        return 0;
    }
    const int num = ref / sv.qcvm.edict_size;
    edict_t* b = serverEdict(num);
    return b && progs::fieldInt(b, f.vr_deathdoll) == player * sv.qcvm.edict_size ? num : 0; // (its player: an entity)
}

[[nodiscard]] int mode()
{
    return static_cast<int>(vr_death_view.value);
}

// Vomit Mode: on, and a body to be in (vr_death_view 0 leaves none).
[[nodiscard]] bool vomit()
{
    return vr_death_view_vomit.value != 0.f && mode() >= 1;
}

[[nodiscard]] bool menuOpen()
{
    return key_dest == key_menu || key_dest == key_console;
}

// The share of the Immersive view in effect: inHead eased (smoothstep: no jolt at either end).
[[nodiscard]] float weight()
{
    return glm::smoothstep(0.f, 1.f, state.inHead);
}

[[nodiscard]] bool dead()
{
    return cls.state == ca_connected && cls.signon == SIGNONS && !cl.intermission && cl.stats[STAT_HEALTH] <= 0;
}

// The head bone of body `num` as drawn now: its eyes and yaw (and its whole turn, `turn`).
bool readHead(int num, glm::vec3& eyes, float& yaw, glm::quat* turn = nullptr)
{
    const ragdoll::Rig* rig = nullptr;
    glm::quat rot;
    glm::vec3 pos;
    float scale = 1.f;
    if(num <= 0 || !ragdoll::drawnPart(num, 0, rot, pos, scale, &rig) || !rig || rig->head < 0 ||
        !ragdoll::drawnPart(num, rig->head, rot, pos, scale))
    {
        return false;
    }
    const ragdoll::Bone& head = rig->bones[rig->head];
    eyes = rot * (scale * (head.pivot + (head.end - head.pivot) * eyeAlong)) + pos;
    if(turn)
    {
        *turn = glm::normalize(rot);
    }
    const glm::vec3 fwd = rot * glm::vec3{1.f, 0.f, 0.f}, up = rot * glm::vec3{0.f, 0.f, 1.f};
    const glm::vec2 flat{fwd.x + up.x, fwd.y + up.y}; // (a face up: its crown's way; upright: its face's)
    if(glm::length(flat) > 0.25f)
    {
        yaw = glm::degrees(za::atan2(flat.y, flat.x));
    }
    return true;
}

void deactivate()
{
    if(state.active || state.reached)
    {
        if(state.reached && !dead() && cls.state == ca_connected)
        {
            comfortfade::start(vr_death_view_fade.value); // (back on your feet: the jump unseen)
        }
    }
    state.active = false;
    state.reached = false;
    state.doll = 0;
    state.headValid = false;
    state.yaw = 0.f;
    state.vomitRefSet = false;
    ragdoll::hideHeadOf(0);
}

void update(const hands::State& s)
{
    if(state.frame == host_framecount)
    {
        return;
    }
    state.frame = host_framecount;
    const double now = realtime;
    const float dt = static_cast<float>(za::clamp(now - state.last, 0.0, 0.1));
    state.last = now;

    if(!dead() || (mode() < 2 && !vomit()) || !s.valid)
    {
        if(state.active || state.reached)
        {
            deactivate();
        }
        state.doll = !dead() ? 0 : dollOf(cl.viewentity);
        return;
    }
    state.doll = dollOf(cl.viewentity);
    float yaw = state.headYaw;
    state.headValid = readHead(state.doll, state.headEyes, yaw, &state.headRot);
    if(state.headValid)
    {
        state.headYaw = yaw;
    }
    if(!state.reached)
    {
        if(!state.headValid)
        {
            return; // (not a ragdoll yet: the view where it was)
        }
        state.reached = true;
        state.active = true;
        state.anchor = state.headEyes;
        state.trackedRef = s.head;
        state.yawRef = state.headYaw;
        state.yaw = 0.f;
        state.inHead = menuOpen() ? 0.f : 1.f;
        comfortfade::start(vr_death_view_fade.value);
        Con_DPrintf("deathview: in body %d's head at %.1f %.1f %.1f\n", state.doll, state.anchor.x, state.anchor.y, state.anchor.z);
    }
    state.active = true;
    {
        const float time = za::max(vr_death_view_menu_time.value, 0.f);
        const float goal = menuOpen() ? 0.f : 1.f;
        const float most = time > 0.f ? dt / time : 1.f;
        state.inHead = state.inHead < goal ? za::min(goal, state.inHead + most) : za::max(goal, state.inHead - most);
    }
    if(state.headValid && vomit())
    {
        state.anchor = state.headEyes; // (the head's own: no smoothing; its turn is taken whole in eyeView)
        state.yaw = 0.f;
    }
    else if(state.headValid)
    {
        const float tau = za::max(vr_death_view_smooth.value, 0.f);
        const float k = tau > 0.f ? 1.f - za::exp(-dt / tau) : 1.f;
        state.anchor += (state.headEyes - state.anchor) * k;
        if(vr_death_view_turn.value != 0.f)
        {
            const float want = geom::wrap180(state.headYaw - state.yawRef);
            float step = geom::wrap180(want - state.yaw) * k;
            const float most = za::max(vr_death_view_turn_speed.value, 0.f) * dt;
            step = za::clamp(step, -most, most);
            state.yaw = geom::wrap180(state.yaw + step);
        }
        else
        {
            state.yaw = 0.f;
        }
    }
    ragdoll::hideHeadOf(state.headValid && weight() > 0.5f ? state.doll : 0);
}

void status_f()
{
    const progs::FieldOffsets& f = progs::fields();
    edict_t* p = serverEdict(cl.viewentity);
    const bool hiddenFromOthers = p && f.vr_deathdoll_body >= 0 && progs::fieldInt(p, f.vr_deathdoll_body) != 0;
    const int doll = dollOf(cl.viewentity);
    glm::vec3 eyes{0.f};
    float yaw = 0.f;
    const bool rag = readHead(doll, eyes, yaw);
    const ragdoll::Rig* rig = nullptr;
    glm::quat rot;
    glm::vec3 pos;
    float scale = 1.f;
    const bool parts = doll > 0 && ragdoll::drawnPart(doll, 0, rot, pos, scale, &rig);
    Con_Printf("deathview: mode %d health %d doll %d ragdoll %d head %d (eyes %.1f %.1f %.1f yaw %.0f) immersive %d "
               "headhidden %d camera %.1f %.1f %.1f (%.1f from the eyes) turn %.1f hiddenfromothers %d inhead %.2f menu %d "
               "vomit %d camangles %.1f %.1f %.1f\n",
        mode(), cl.stats[STAT_HEALTH], doll, parts ? 1 : 0, rag ? 1 : 0, eyes.x, eyes.y, eyes.z, yaw, immersive() ? 1 : 0,
        immersive() && state.headValid ? 1 : 0, state.eye0.x, state.eye0.y, state.eye0.z,
        rag ? glm::length(state.eye0 - eyes) : -1.f, state.yaw, hiddenFromOthers ? 1 : 0, state.inHead, menuOpen() ? 1 : 0,
        vomit() ? 1 : 0, state.eye0Angles.x, state.eye0Angles.y, state.eye0Angles.z);
    if(rag && vomit())
    {
        const glm::mat3 b = glm::mat3_cast(state.headRot);
        const glm::vec3 a = hands::anglesFromVectors(b[0], b[2]);
        Con_Printf("deathview: head bone angles %.1f %.1f %.1f\n", a.x, a.y, a.z);
    }
}

} // namespace

void eyeView(const hands::State& s, int eye, glm::vec3& origin, glm::vec3& angles)
{
    update(s);
    if(state.active && vomit())
    {
        // Vomit Mode: the eye where the head bone's eyes are, its spacing from your head's centre turned from your head's
        // axes into the bone's, and the bone's whole turn as the view's (as far in as the menu lets it: weight).
        // Your own head's turn on top of the bone's (the author's note vrfiringrange_2026-10-23_17-45-04): its yaw from
        // where it faced as the view went there, its pitch and roll as they are; looking straight ahead is the bone's.
        const float w = weight();
        if(!state.vomitRefSet)
        {
            state.vomitRefSet = true;
            state.vomitYawRef = angles.y;
        }
        const glm::mat3 own = hands::basisOf(angles); // (forward, left, up)
        const glm::mat3 rel = hands::basisOf(glm::vec3{angles.x, geom::wrap180(angles.y - state.vomitYawRef), angles.z});
        const glm::quat full = glm::normalize(state.headRot * glm::quat_cast(rel));
        const glm::mat3 view = glm::mat3_cast(full);
        const glm::vec3 spacing = glm::transpose(own) * (origin - s.head);
        origin = glm::mix(origin, state.anchor + view * spacing, w);
        const glm::mat3 turn = glm::mat3_cast(glm::slerp(glm::quat_cast(own), full, w));
        angles = hands::anglesFromVectors(turn[0], turn[2]);
    }
    else if(state.active)
    {
        // In the head as far as the menu lets it (weight): from Third Person's place (the view as given) to the head's.
        const float w = weight();
        const float r = glm::radians(state.yaw), c = za::cos(r), n = za::sin(r);
        const glm::vec3 d = origin - state.trackedRef; // (your own head since: its steps and the eyes' spacing)
        origin = glm::mix(origin, state.anchor + glm::vec3{c * d.x - n * d.y, n * d.x + c * d.y, d.z}, w);
        angles.y += state.yaw * w;
    }
    if(eye == 0)
    {
        state.eye0 = origin;
        state.eye0Angles = angles;
    }
}

bool immersive()
{
    return state.active && weight() > 0.5f; // (out for a menu: the hands and gear drawn again, for its pointer)
}

void clear()
{
    state = State{};
    ragdoll::hideHeadOf(0);
}

void registerCommands()
{
    Cmd_AddCommand("vr_death_view_status", status_f);
}

} // namespace qvr::deathview

// sv_main.c: whether a client's entity is kept from the other clients (a dead player whose ragdoll body lies there: QC
// vr_deathdoll.qc; the body is what they see).
extern "C" int VR_SV_HiddenFromOthers(edict_t* ent)
{
    const int ofs = qvr::progs::fields().vr_deathdoll_body;
    if(ofs < 0 || !ent)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(ent);
    return num >= 1 && num <= svs.maxclients && qvr::progs::fieldInt(ent, ofs) != 0 ? 1 : 0;
}
