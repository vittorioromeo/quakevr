// vr_box3d_striketest.cpp -- vr_prop_strike_test: the main hand (a fist or what it holds) struck into the solid prop
// nearest the first player again and again, at a set speed, down onto its top or ahead into its near side, until it
// breaks (ROUND22.md, "Crates break from fewer blows"). Hits to break a crate or a barrel per weapon, the same strike
// every time: the mock headset's hand moved by its own tracking (not to a point the view pushes).
// Split from vr_box3d_diag.cpp's tests; the shared declarations are in vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"
#include "vr_progs.hpp"
#include "vr_handpose.hpp"
#include "vr_hands.hpp"

namespace box3d::detail
{
namespace
{
// The test under way: the prop, the strike's way and speed, the phase of the strike now.
struct StrikeTest
{
    bool on{false};
    int prop{0};
    bool down{true};          // down onto its top (else ahead into its near side)
    float speed{0.f};         // units/s of the strike
    int most{0};              // strikes at most
    int count{0};             // strikes so far
    int phase{0};             // 0 draw back, 6 glide, 7 step up, 5 aim, 8, 1 settle, 2 strike, 3 hold, 4 back
    double phaseEnd{0.0};     // server time the settle or the hold ends
    glm::vec3 dir{0.f};       // the strike's way
    float travel{0.f};        // units the strike goes
    float done{0.f};          // and has gone (the strike, then back)
    float health{0.f};        // the prop's health as the strike began
    float first{0.f};         // and at the start
    double start{0.0};
    glm::vec3 glideTo{0.f};   // the hand's slow glide (phase 6): where to, until when, the phase after
    double glideUntil{0.0};
    int after{0};
};
StrikeTest strike;

constexpr float strikeRise = 16.f;  // units before the prop the strike starts
constexpr float strikeDepth = 10.f; // units into it the strike goes on
constexpr float strikeBack = 10.f;  // units/s back out (slow: a fast return reads as a blow)
constexpr float strikeStand = 34.f; // units: you step up to this from its middle before each strike
constexpr float strikeDrawBack = 14.f; // units the hand is drawn back from it before you step up

void strikeFinish(const char* why)
{
    const edict_t* e = EDICT_NUM(strike.prop);
    Con_Printf("striketest: done (%s): %d strikes, health %.0f -> %.0f%s\n", why, strike.count, strike.first,
        e->free ? 0.f : e->v.health, e->free || e->v.health <= 0.f ? " (broken)" : "");
    strike.on = false;
}

// The point that strikes: the far end of what the hand holds (an axe's head, a sword's tip), else the hand.
[[nodiscard]] glm::vec3 strikePoint(const hands::State& st)
{
    return st.muzzleValid[HAND_MAIN] ? st.muzzle[HAND_MAIN] : st.pos[HAND_MAIN];
}
} // namespace

// vr_prop_strike_test [down|ahead [<m/s> [<strikes>]]]: the main hand struck into the solid prop nearest the first player
// (down onto its top, or ahead into its near side) at that speed (6), that many times at most (12) or until it breaks.
void strikeTest_f()
{
    if(!sv.active || !world || svs.maxclients < 1)
    {
        Con_Printf("striketest: no game\n");
        return;
    }
    const progs::ServerVm vm;
    const glm::vec3 eye = vec(progs::playerEdict()->v.origin);
    int best = 0;
    float bestDist = 1e30f;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts && num < static_cast<int>(world->slots.size()); num++)
    {
        const Slot& s = world->slots[static_cast<size_t>(num)];
        edict_t* e = EDICT_NUM(num);
        if(e->free || s.kind != Kind::Prop || !isSolidProp(e) || B3_IS_NULL(s.body) || !b3Body_IsValid(s.body))
        {
            continue;
        }
        const float d = glm::distance(vec(e->v.origin), eye);
        if(d < bestDist)
        {
            best = num;
            bestDist = d;
        }
    }
    if(!best)
    {
        Con_Printf("striketest: no solid prop\n");
        return;
    }
    strike = StrikeTest{};
    strike.on = true;
    strike.prop = best;
    strike.down = !(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "ahead"));
    strike.speed = (Cmd_Argc() > 2 ? Q_atof(Cmd_Argv(2)) : 6.f) * world->m2u;
    strike.most = Cmd_Argc() > 3 ? Q_atoi(Cmd_Argv(3)) : 12;
    strike.first = EDICT_NUM(best)->v.health;
    strike.start = qcvm->time;
    Con_Printf("striketest: %d %s, health %.0f, %s at %.2f m/s\n", best, PR_GetString(EDICT_NUM(best)->v.classname),
        strike.first, strike.down ? "down" : "ahead", strike.speed / world->m2u);
}

// Each server frame, before the step: the hand's next move.
void strikeTestFrame()
{
    if(!strike.on || !world)
    {
        return;
    }
    const edict_t* e = EDICT_NUM(strike.prop);
    const Slot* s = strike.prop < static_cast<int>(world->slots.size()) ? &world->slots[static_cast<size_t>(strike.prop)] : nullptr;
    if(e->free || e->v.health <= 0.f || !s || s->kind != Kind::Prop || B3_IS_NULL(s->body) || !b3Body_IsValid(s->body))
    {
        if(strike.phase >= 2)
        {
            Con_Printf("striketest: strike %d: health %.0f -> broken\n", strike.count, strike.health);
        }
        strikeFinish("the prop is gone");
        return;
    }
    if(qcvm->time - strike.start > 120.0)
    {
        strikeFinish("timed out");
        return;
    }
    const float dt = static_cast<float>(host_frametime);
    const hands::State& st = hands::current();
    const auto by = [](const glm::vec3& d) {
        Cmd_ExecuteString(va("vr_mock_hand_to main by %.3f %.3f %.3f", d.x, d.y, d.z), src_command);
    };
    if(strike.phase == 0)
    {
        if(strike.count >= strike.most)
        {
            strikeFinish("strikes done");
            return;
        }
        // The hand drawn back slowly from it first (stepping up with the hand ahead would sweep it into the prop).
        const b3AABB box0 = b3Body_ComputeAABB(s->body);
        const glm::vec3 mid0 = (world->toU(box0.lowerBound) + world->toU(box0.upperBound)) * 0.5f;
        glm::vec3 toward = mid0 - vec(EDICT_NUM(1)->v.origin);
        toward.z = 0.f;
        toward = glm::length(toward) > 1.f ? glm::normalize(toward) : glm::vec3{1.f, 0.f, 0.f};
        strike.glideTo = st.pos[HAND_MAIN] - toward * strikeDrawBack + glm::vec3{0.f, 0.f, strike.down ? 8.f : 0.f};
        strike.glideUntil = qcvm->time + 4.0;
        strike.after = 7;
        strike.phase = 6;
        return;
    }
    if(strike.phase == 6)
    {
        // A slow glide of the hand to glideTo (no blow: under any strike's speed).
        const glm::vec3 d = strike.glideTo - st.pos[HAND_MAIN];
        if(glm::length(d) < 0.3f || qcvm->time >= strike.glideUntil)
        {
            strike.phase = strike.after;
            return;
        }
        by(glm::normalize(d) * za::min(glm::length(d), strikeBack * dt));
        return;
    }
    if(strike.phase == 7)
    {
        // You step up to it as it lies now (knocked away by the last strike: out of the arm's reach), then aim.
        const b3AABB box0 = b3Body_ComputeAABB(s->body);
        const glm::vec3 mid0 = (world->toU(box0.lowerBound) + world->toU(box0.upperBound)) * 0.5f;
        const glm::vec3 me = vec(EDICT_NUM(1)->v.origin);
        glm::vec3 toward = mid0 - me;
        toward.z = 0.f;
        if(glm::length(toward) > strikeStand + 2.f)
        {
            const glm::vec3 to = mid0 - glm::normalize(toward) * strikeStand;
            Cmd_ExecuteString(va("setpos %.1f %.1f %.1f", to.x, to.y, me.z), src_command);
        }
        strike.phase = 5;
        strike.phaseEnd = qcvm->time + 0.15;
        return;
    }
    if(strike.phase == 5)
    {
        if(qcvm->time < strike.phaseEnd)
        {
            return;
        }
        // Where the strike starts, the prop as it lies now: over its top's middle, or before its side towards you.
        const b3AABB box = b3Body_ComputeAABB(s->body);
        const glm::vec3 lo = world->toU(box.lowerBound), hi = world->toU(box.upperBound), mid = (lo + hi) * 0.5f;
        glm::vec3 from;
        if(strike.down)
        {
            strike.dir = glm::vec3{0.f, 0.f, -1.f};
            from = glm::vec3{mid.x, mid.y, hi.z + strikeRise};
            strike.travel = strikeRise + strikeDepth;
        }
        else
        {
            glm::vec3 toward = mid - vec(EDICT_NUM(1)->v.origin);
            toward.z = 0.f;
            strike.dir = glm::length(toward) > 1.f ? glm::normalize(toward) : glm::vec3{1.f, 0.f, 0.f};
            const float half = za::min(hi.x - lo.x, hi.y - lo.y) * 0.5f;
            from = mid - strike.dir * (half + strikeRise);
            strike.travel = strikeRise + strikeDepth;
        }
        strike.glideTo = from - (strikePoint(st) - st.pos[HAND_MAIN]);
        strike.glideUntil = qcvm->time + 6.0;
        strike.after = 8;
        strike.phase = 6;
        return;
    }
    if(strike.phase == 8)
    {
        strike.phase = 1;
        strike.phaseEnd = qcvm->time + 0.6;
        return;
    }
    if(strike.phase == 1)
    {
        if(qcvm->time >= strike.phaseEnd)
        {
            strike.phase = 2;
            strike.done = 0.f;
            strike.count++;
            strike.health = e->v.health;
            const b3AABB box = b3Body_ComputeAABB(s->body);
            const glm::vec3 lo = world->toU(box.lowerBound), hi = world->toU(box.upperBound);
            const glm::vec3 p = strikePoint(st), press = qvr::box3d::pressOffset(1, HAND_MAIN);
            Con_Printf("striketest: strike %d begins: the point %.1f %.1f %.1f, the prop %.1f %.1f %.1f .. %.1f %.1f %.1f, "
                       "held back %.1f\n", strike.count, p.x, p.y, p.z, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z, glm::length(press));
            // The strike in one move the mock glides through itself, frame by frame of its own (vr_mock_hand_glide, real
            // time: run the game with -RealTime). Moved a step each server frame, the hand jumped a step at a time on the
            // client's faster frames: the weight's spring took a held weapon for jumping (put back in the hand each time),
            // and its blows read up to 3-4 times the strike's speed (ROUND22.md, "Crates: one hit").
            const float secs = strike.travel / za::max(strike.speed, 1.f);
            const float glideWas = vr_mock_hand_glide.value;
            Cvar_SetValueQuick(&vr_mock_hand_glide, secs);
            by(strike.dir * strike.travel);
            Cvar_SetValueQuick(&vr_mock_hand_glide, glideWas);
            strike.phaseEnd = qcvm->time + static_cast<double>(secs);
        }
        return;
    }
    if((strike.phase == 2 || strike.phase == 3) && developer.value >= 3.f)
    {
        // Developer 3: the hand each frame of the strike: as tracked (before the weight's spring and the walls), as the
        // client has it and as the game has it (units along the strike, from the prop's middle).
        const FieldOffsets& f = fields();
        const glm::vec3 g = f.handpos >= 0 ? fieldVec(EDICT_NUM(1), f.handpos) : glm::vec3{0.f};
        const b3AABB box = b3Body_ComputeAABB(s->body);
        const glm::vec3 mid = (world->toU(box.lowerBound) + world->toU(box.upperBound)) * 0.5f;
        const auto at = [&](const glm::vec3& p) { return glm::dot(p - mid, strike.dir); };
        const glm::vec3 wall = qvr::handpose::wallPush(HAND_MAIN);
        Con_Printf("striketest: t %.4f tracked %.1f (up %.1f) hand %.1f (up %.1f) game %.1f; held off the level %.1f %.1f %.1f\n",
            qcvm->time, at(st.unresolvedPos[HAND_MAIN]), st.unresolvedPos[HAND_MAIN].z, at(st.pos[HAND_MAIN]),
            st.pos[HAND_MAIN].z, at(g), wall.x, wall.y, wall.z);
    }
    if(strike.phase == 2)
    {
        if(qcvm->time >= strike.phaseEnd)
        {
            strike.phase = 3;
            strike.phaseEnd = qcvm->time + 0.15;
        }
        return;
    }
    if(strike.phase == 3)
    {
        if(qcvm->time >= strike.phaseEnd)
        {
            strike.phase = 4;
            strike.done = 0.f;
        }
        return;
    }
    const float step = za::min(strikeBack * dt, strike.travel - strike.done);
    by(-strike.dir * step);
    strike.done += step;
    if(strike.done >= strike.travel - 1e-3f)
    {
        Con_Printf("striketest: strike %d: health %.0f -> %.0f\n", strike.count, strike.health, e->v.health);
        strike.phase = 0;
    }
}
} // namespace box3d::detail
