// vr_closequarters.cpp -- close-quarters slowdown (see vr_closequarters.hpp). His voice note, 2026-11-01 18:14: "if the
// player walks into an enemy full speed, then it becomes really messy ... it should still be possible for a player to
// get right into an enemy's face, but ... the player speed slowing down if they're very close to an enemy".
//
// SV_AirMove's wish velocity (the stick's, after the most speed's caps), walking: the live enemy (FL_MONSTER, alive,
// damageable, solid) whose box is nearest the player's centre horizontally, at the player's height, within Start Radius
// (vr_closequarters_start, metres): its part towards that enemy times a factor from 1 at Start Radius down to Min Factor
// (vr_closequarters_min) at End Radius (vr_closequarters_end) and nearer, the less the further the move turns from
// straight at it (all of it within vr_closequarters_angle degrees, none past vr_closequarters_angle_falloff); its
// sideways part times 1 .. Strafe Factor (vr_closequarters_strafe) on the same ramp. Moving away: only the sideways part.

#include "vr_closequarters.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_progs.hpp"
#include "vr_testkit.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <string.h>

using namespace qvr;

namespace
{

// The last walking move a live enemy slowed (or the one that found none), and the approach's extremes since
// vr_closequarters_test start: what the test reads.
struct LastMove
{
    int moves{0};         // walking moves seen
    int slowed{0};        // ... with an enemy within Start Radius
    float gap{0.f};       // the last slowed one's: metres to the enemy's box
    float toward{1.f};    // its factors
    float side{1.f};
    float speedIn{0.f};   // its wish speed before and after (units/s)
    float speedOut{0.f};
    float leastToward{1.f}; // the least toward factor since start
    float leastGap{1e9f};   // the least gap since start
    float farSlowed{0.f};   // the most toward slowdown (1 - factor) at a gap past Start Radius (must stay 0)
    int foe{0};             // the last slowed move's enemy (its edict number)
    float firstGap{-1.f};   // the first slowed move's gap since start (metres)
};
LastMove lastMove;

[[nodiscard]] float startRadius()
{
    return za::max(vr_closequarters_start.value, 0.f);
}

[[nodiscard]] float endRadius()
{
    return za::clamp(vr_closequarters_end.value, 0.f, startRadius());
}

// A live enemy one can walk into: a monster alive, damageable and solid; with Only Live Foes (vr_closequarters_live) not
// a ragdoll either (knocked down, staggering, held up by a hand, getting up through its body: active ragdoll form).
[[nodiscard]] bool liveEnemy(edict_t* e)
{
    const int solid = static_cast<int>(e->v.solid);
    if(!((static_cast<int>(e->v.flags) & FL_MONSTER) && e->v.health > 0.f &&
           static_cast<int>(e->v.takedamage) != DAMAGE_NO && solid != SOLID_NOT && solid != SOLID_TRIGGER))
    {
        return false;
    }
    return vr_closequarters_live.value == 0.f ||
           (!box3d::isRagdoll(NUM_FOR_EDICT(e)) && progs::fieldFloatOr(e, progs::fields().vr_knockdown, 0.f) != 1.f);
}

// The horizontal distance in units from `at` to the box (`lo`, `hi`): 0 inside it.
[[nodiscard]] float boxGap(const glm::vec3& at, const float* lo, const float* hi)
{
    const float dx = za::max(za::max(lo[0] - at.x, at.x - hi[0]), 0.f);
    const float dy = za::max(za::max(lo[1] - at.y, at.y - hi[1]), 0.f);
    return glm::sqrt(dx * dx + dy * dy);
}

} // namespace

namespace qvr::closequarters
{

Factors factorsFor(const glm::vec3& wish, const glm::vec3& toEnemy, const float gap)
{
    Factors f;
    const float start = startRadius();
    const float end = endRadius();
    if(gap >= start)
    {
        return f;
    }
    const float ramp = start > end ? za::clamp((start - gap) / (start - end), 0.f, 1.f) : 1.f;
    f.side = 1.f - ramp * (1.f - za::clamp(vr_closequarters_strafe.value, 0.f, 1.f));
    const float speed = glm::length(wish);
    const float along = speed > 0.f ? glm::dot(wish, toEnemy) / speed : 0.f;
    if(along <= 0.f)
    {
        return f; // moving away (or still): its part towards nothing
    }
    const float angle = glm::degrees(glm::acos(za::clamp(along, -1.f, 1.f)));
    const float full = za::clamp(vr_closequarters_angle.value, 0.f, 90.f);
    const float none = za::clamp(vr_closequarters_angle_falloff.value, full, 90.f);
    const float weight = angle <= full ? 1.f : (none > full ? za::clamp((none - angle) / (none - full), 0.f, 1.f) : 0.f);
    f.toward = 1.f - ramp * weight * (1.f - za::clamp(vr_closequarters_min.value, 0.f, 1.f));
    return f;
}

} // namespace qvr::closequarters

// SV_AirMove (walking, not noclip): the wish velocity near a live enemy, slowed straight at it (vr_closequarters).
extern "C" void VR_CloseQuartersMove(edict_t* ent, float* wishvel)
{
    if(!ent || vr_closequarters.value == 0.f || static_cast<int>(ent->v.movetype) != MOVETYPE_WALK)
    {
        return;
    }
    const glm::vec3 wish{wishvel[0], wishvel[1], 0.f};
    const float speedIn = glm::length(wish);
    if(speedIn <= 0.f)
    {
        return;
    }
    lastMove.moves++;
    const float m2u = units::metresToUnits();
    const float start = startRadius() * m2u;
    const glm::vec3 at{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    const float zLo = ent->v.absmin[2], zHi = ent->v.absmax[2];
    float gap = start;
    edict_t* const foe = progs::nearestBy(1, start, [&](edict_t* e) {
        if(e == ent || !liveEnemy(e) || e->v.absmax[2] <= zLo || e->v.absmin[2] >= zHi)
        {
            return start;
        }
        return boxGap(at, e->v.absmin, e->v.absmax);
    }, &gap);
    if(!foe)
    {
        return;
    }
    glm::vec3 toEnemy{(foe->v.absmin[0] + foe->v.absmax[0]) * 0.5f - at.x, (foe->v.absmin[1] + foe->v.absmax[1]) * 0.5f - at.y, 0.f};
    const float len = glm::length(toEnemy);
    if(len <= 0.f)
    {
        return;
    }
    toEnemy /= len;
    const float gapM = gap / m2u;
    const closequarters::Factors f = closequarters::factorsFor(wish, toEnemy, gapM);
    const float along = glm::dot(wish, toEnemy);
    const glm::vec3 towardPart = toEnemy * za::max(along, 0.f);
    const glm::vec3 out = towardPart * f.toward + (wish - towardPart) * f.side;
    wishvel[0] = out.x;
    wishvel[1] = out.y;
    LastMove& l = lastMove;
    l.slowed++;
    l.gap = gapM;
    l.toward = f.toward;
    l.side = f.side;
    l.speedIn = speedIn;
    l.speedOut = glm::length(out);
    l.leastToward = za::min(l.leastToward, f.toward);
    l.leastGap = za::min(l.leastGap, gapM);
    l.foe = NUM_FOR_EDICT(foe);
    l.firstGap = l.firstGap < 0.f ? gapM : l.firstGap;
    if(gapM >= startRadius())
    {
        l.farSlowed = za::max(l.farSlowed, 1.f - f.toward);
    }
}

namespace
{

// vr_closequarters_test start | approach | end: "start" clears the record; the script walks straight at a grunt (spawned
// 120 units ahead) and runs "approach": it was slowed only within Start Radius, down to Min Factor at its face; then it
// strafes beside it and runs "end": the sideways move kept Strafe Factor of its speed (no part towards it slowed), and
// the factors' rule on the settings (none past Start Radius, Min Factor at End Radius straight at it, between them
// halfway, Strafe Factor sideways, moving away only the sideways part). Prints "vr_closequarters_test: N passed, M
// failed".
testkit::Tally cqTally;

void test_f()
{
    constexpr const char* name = "vr_closequarters_test";
    const char* step = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    LastMove& l = lastMove;
    const float minF = za::clamp(vr_closequarters_min.value, 0.f, 1.f);
    const float strafe = za::clamp(vr_closequarters_strafe.value, 0.f, 1.f);
    if(!strcmp(step, "start"))
    {
        l = LastMove{};
        cqTally = testkit::Tally{};
        return;
    }
    if(!strcmp(step, "approach"))
    {
        if(!sv.active)
        {
            return;
        }
        const progs::ServerVm vm;
        const edict_t* const foe = l.foe > 0 && l.foe < qcvm->num_edicts ? EDICT_NUM(l.foe) : nullptr;
        Con_Printf("%s: approach: %d walking moves, %d slowed (%s #%d, first at %.2f m); nearest %.2f m, least toward "
                   "factor %.3f (Min Factor %.2f)\n",
            name, l.moves, l.slowed, foe ? PR_GetString(foe->v.classname) : "none", l.foe, l.firstGap, l.leastGap,
            l.leastToward, minF);
        cqTally.check(l.moves > l.slowed && l.slowed > 0, name, "the approach walked unslowed far off, slowed near it");
        cqTally.check(l.farSlowed == 0.f, name, "nothing slowed past Start Radius");
        cqTally.check(l.leastGap <= endRadius(), name, va("it reached End Radius (%.2f m, want <= %.2f)", l.leastGap, endRadius()));
        cqTally.check(za::abs(l.leastToward - minF) < 0.01f, name,
            va("straight at its face: slowed to Min Factor (%.3f, want %.2f)", l.leastToward, minF));
        l = LastMove{};
        return;
    }
    if(!strcmp(step, "live"))
    {
        // Only Live Foes (vr_closequarters_live): the grunt standing counts; knocked down (its ragdoll made, alive) it
        // doesn't, and does again with the option off.
        if(!sv.active)
        {
            return;
        }
        const progs::ServerVm vm;
        edict_t* grunt = nullptr;
        for(int i = 1; i < qcvm->num_edicts && !grunt; i++)
        {
            edict_t* const e = EDICT_NUM(i);
            grunt = !e->free && !strcmp(PR_GetString(e->v.classname), "monster_army") && e->v.health > 0.f ? e : nullptr;
        }
        cqTally.check(grunt != nullptr && liveEnemy(grunt), name, "the grunt standing: a live foe");
        // (the knockdown's mark, .vr_knockdown 1: a living ragdoll's, QC vr_knockdown.qc; the test's grunts here are
        // spawned as targets a sweep does not knock down, so the mark is set for the check and restored)
        const int kd = progs::fields().vr_knockdown;
        if(!grunt || kd < 0)
        {
            cqTally.check(false, name, "a grunt with the knockdown field");
            return;
        }
        const float mark = progs::fieldFloat(grunt, kd);
        progs::fieldFloat(grunt, kd) = 1.f;
        const float was = vr_closequarters_live.value;
        Cvar_SetValueQuick(&vr_closequarters_live, 1.f);
        const bool liveOn = liveEnemy(grunt);
        Cvar_SetValueQuick(&vr_closequarters_live, 0.f);
        const bool liveOff = liveEnemy(grunt);
        Cvar_SetValueQuick(&vr_closequarters_live, was);
        progs::fieldFloat(grunt, kd) = mark;
        Con_Printf("%s: live: knocked down (marked): a foe with Only Live Foes %d, without %d\n", name, liveOn ? 1 : 0,
            liveOff ? 1 : 0);
        cqTally.check(!liveOn, name, "knocked down: no foe with Only Live Foes");
        cqTally.check(liveOff, name, "knocked down: a foe again with Only Live Foes off");
        return;
    }
    Con_Printf("%s: strafe: %d slowed moves, last at %.2f m: toward %.3f, side %.3f, speed %.0f -> %.0f\n", name, l.slowed,
        l.gap, l.toward, l.side, l.speedIn, l.speedOut);
    cqTally.check(l.slowed > 0, name, "the strafe was beside it");
    cqTally.check(l.speedIn > 0.f && l.speedOut >= l.speedIn * (strafe - 0.02f), name,
        va("strafing past it kept its speed (%.0f of %.0f, Strafe Factor %.2f)", l.speedOut, l.speedIn, strafe));
    // The rule on the settings, straight, sideways and away.
    const glm::vec3 way{1.f, 0.f, 0.f};
    const float s = startRadius(), e = endRadius();
    using closequarters::factorsFor;
    const closequarters::Factors past = factorsFor(way * 320.f, way, s + 0.05f);
    cqTally.check(past.toward == 1.f && past.side == 1.f, name, "past Start Radius: unchanged");
    const closequarters::Factors atEnd = factorsFor(way * 320.f, way, e);
    cqTally.check(za::abs(atEnd.toward - minF) < 0.001f, name, va("straight at End Radius: Min Factor (%.3f)", atEnd.toward));
    const closequarters::Factors mid = factorsFor(way * 320.f, way, (s + e) * 0.5f);
    cqTally.check(za::abs(mid.toward - (1.f + minF) * 0.5f) < 0.01f, name, va("halfway: halfway (%.3f)", mid.toward));
    const closequarters::Factors sideways = factorsFor(glm::vec3{0.f, 320.f, 0.f}, way, e);
    cqTally.check(za::abs(sideways.side - strafe) < 0.001f && (sideways.toward == 1.f || vr_closequarters_angle_falloff.value >= 90.f),
        name, va("sideways at End Radius: Strafe Factor (%.3f), nothing towards (%.3f)", sideways.side, sideways.toward));
    const closequarters::Factors away = factorsFor(way * -320.f, way, e);
    cqTally.check(away.toward == 1.f, name, "moving away: not slowed");
    cqTally.verdict(name);
}

} // namespace

namespace qvr::closequarters
{

void init()
{
    Cmd_AddCommand("vr_closequarters_test", test_f);
}

} // namespace qvr::closequarters
