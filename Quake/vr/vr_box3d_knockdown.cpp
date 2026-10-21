#include "vr_alloccount.h"
// vr_box3d_knockdown.cpp -- knockdowns: recoveries and getting up (the pose fit, the stand spot) and the stagger,
// topple, shove and get-up API.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"

namespace box3d::detail
{
namespace
{
// ----------------------------------------------------------------------------
// Knockdowns (vr_knockdown; QC vr_knockdown.qc; Combat > Knockdowns). A shove can knock a living monster with a rig down:
// the QC makes it touchable and not solid, .vr_knockdown 1, and its ragdoll is made at once (ragdollKnockdown) from the
// frame it is drawn in, its gun welded in its hand (vr_knockdown_weld). It is a ragdoll as a dead one is (hit on its
// limbs, grabbed, pushed, thrown; killed there, it simply stays one: its gun goes, the QC drops it). To get up
// (ragdollGetUp) the QC names the frame its get-up starts from (its own fall-and-rise frames' lowest, or a death's last
// to play backwards; two to choose from): the frame is fitted to how the body lies (its turn about the vertical and
// where, by its joints; the pelvis's tilt picks between two), room to stand is looked for there and around it
// (vr_knockdown_search: never through a wall, a floor or a ceiling), the hands holding it let go, and its parts go. Its
// last pose is then blended into the animation as it plays (vr_knockdown_blend: a Recovery, published as a ragdoll is
// until the blend is done).

// Bone `b` of `rig` animated in frame pose `pose`, for an entity at `origin` turned by `turn`: p_world = rot * (scale *
// p_rest) + pos, as the ragdolls' bodies.
void animatedBone(const ragdoll::Rig& rig, int pose, int b, const glm::quat& turn, const glm::vec3& origin, float scale,
    glm::quat& rot, glm::vec3& pos)
{
    glm::quat r;
    glm::vec3 t;
    ragdoll::bonePose(rig, pose, b, r, t);
    rot = glm::normalize(turn * r);
    pos = origin + turn * (t * scale);
}
} // namespace

// The getting-up monsters' poses, after each step: their ragdolls' last blended into their animation (its frames lerped
// as the client lerps them), published as ragdolls; done, the animated model is drawn again.
void updateRecoveries()
{
    for(za::SizeT i = 0; i < world->recoveries.size();)
    {
        Recovery& rec = world->recoveries[i];
        edict_t* ent = rec.num < qcvm->num_edicts ? EDICT_NUM(rec.num) : nullptr;
        const float t = static_cast<float>((qcvm->time - rec.start) / za::max(static_cast<double>(rec.duration), 1e-3));
        const bool knocked = ent && !ent->free && knockedDown(ent);
        if(!ent || ent->free || modelOf(ent) != rec.rig->model || ent->v.health <= 0.f || knocked || t >= 1.f)
        {
            if(!knocked && !isRagdollSlot(rec.num))
            {
                ragdoll::unpublish(rec.num);
            }
            world->recoveries.eraseAt(i);
            continue;
        }
        // Its frames lerped as the client lerps them once it is drawn animated again, by the server's times: from the
        // think that set the frame (due at the last step: the step's thinks run between its time and the next step's) to
        // the next, and to the time of the message it is drawn at. (The lerp from this step's time over the get-up's
        // interval lagged by a step: each frame's end was skipped, and in bullet time, the thinks slowed, it held still.)
        const int frame = static_cast<int>(ent->v.frame);
        const double now = qcvm->time + host_frametime;
        if(frame != rec.frame)
        {
            rec.prevFrame = rec.frame < 0 ? frame : rec.frame;
            rec.frame = frame;
            rec.changed = rec.due > qcvm->time && rec.due <= now ? rec.due : qcvm->time;
        }
        rec.due = ent->v.nextthink;
        const double interval = rec.due > rec.changed ? rec.due - rec.changed
                                                      : 0.1 / za::clamp(static_cast<double>(vr_knockdown_getup_speed.value), 0.1, 10.0);
        const float lerp = za::clamp(static_cast<float>((now - rec.changed) / interval), 0.f, 1.f);
        const int pose1 = ragdoll::poseOfFrame(rec.rig->model, rec.prevFrame), pose2 = ragdoll::poseOfFrame(rec.rig->model, rec.frame);
        const glm::quat turn = glm::angleAxis(glm::radians(ent->v.angles[1]), glm::vec3{0.f, 0.f, 1.f});
        const glm::vec3 origin = vec(ent->v.origin);
        const float w = smooth01(t);
        za::Array<glm::quat, ragdoll::maxBones> rot{};
        za::Array<glm::vec3, ragdoll::maxBones> pos{};
        for(int b = 0; b < rec.count; b++)
        {
            const za::SizeT k = static_cast<za::SizeT>(b);
            glm::quat r1, r2;
            glm::vec3 p1, p2;
            animatedBone(*rec.rig, pose1, b, turn, origin, rec.scale, r1, p1);
            animatedBone(*rec.rig, pose2, b, turn, origin, rec.scale, r2, p2);
            const glm::quat ar = glm::slerp(r1, r2, lerp);
            const glm::vec3 ap = glm::mix(p1, p2, lerp);
            // (The joint's place blended in a line and the turn about it: no swing about the model's origin.)
            const glm::vec3 pivot = rec.rig->bones[b].pivot * rec.scale;
            const glm::vec3 fromPivot = rec.rot[k] * pivot + rec.pos[k], toPivot = ar * pivot + ap;
            rot[k] = glm::normalize(glm::slerp(rec.rot[k], ar, w));
            pos[k] = glm::mix(fromPivot, toPivot, w) - rot[k] * pivot;
        }
        ragdoll::publish(rec.num, rec.rig, rec.count, rot.data(), pos.data(), rec.scale, qcvm->time + host_frametime, 0, 0,
            limbsBleed(ent));
        i++;
    }
}

namespace
{
// How well frame `frame` of `rig`, turned by `yaw` (out) about the vertical and put at `origin` (out), fits ragdoll `r`
// as it lies: its joints' places (a 2D fit of the turn, their middles matched) and the pelvis's turn. Lower is better.
[[nodiscard]] float fitFrame(const RagdollBodies& r, int frame, float& yaw, glm::vec3& origin)
{
    const ragdoll::Rig& rig = *r.rig;
    const int pose = ragdoll::poseOfFrame(rig.model, frame);
    za::Array<glm::vec3, ragdoll::maxBones> model{}, worldP{};
    int n = 0;
    glm::vec3 cm{0.f}, cw{0.f};
    for(int b = 0; b < r.count; b++)
    {
        if(rig.bones[b].joint == ragdoll::Joint::Loose || partCut(r, b))
        {
            continue;
        }
        glm::quat ar;
        glm::vec3 at;
        ragdoll::bonePose(rig, pose, b, ar, at);
        const b3WorldTransform xf = b3Body_GetTransform(r.body[static_cast<za::SizeT>(b)]);
        const glm::vec3 pivot = rig.bones[b].pivot * r.scale;
        model[static_cast<za::SizeT>(n)] = ar * pivot + at * r.scale;
        worldP[static_cast<za::SizeT>(n)] = fromB3(xf.q) * pivot + world->toU(xf.p);
        cm += model[static_cast<za::SizeT>(n)];
        cw += worldP[static_cast<za::SizeT>(n)];
        n++;
    }
    if(n == 0)
    {
        return 1e30f;
    }
    cm /= static_cast<float>(n);
    cw /= static_cast<float>(n);
    float sc = 0.f, cc = 0.f;
    for(int i = 0; i < n; i++)
    {
        const glm::vec3 m = model[static_cast<za::SizeT>(i)] - cm, w = worldP[static_cast<za::SizeT>(i)] - cw;
        cc += m.x * w.x + m.y * w.y;
        sc += m.x * w.y - m.y * w.x;
    }
    yaw = za::atan2(sc, cc);
    const glm::quat turn = glm::angleAxis(yaw, glm::vec3{0.f, 0.f, 1.f});
    float err = 0.f;
    for(int i = 0; i < n; i++)
    {
        const glm::vec3 d = turn * (model[static_cast<za::SizeT>(i)] - cm) - (worldP[static_cast<za::SizeT>(i)] - cw);
        err += glm::dot(d, d);
    }
    err /= static_cast<float>(n);
    origin = cw - turn * cm;
    // The pelvis's turn as drawn against the frame's (a body on its back or on its front).
    glm::quat pr;
    glm::vec3 pt;
    ragdoll::bonePose(rig, pose, 0, pr, pt);
    const glm::quat drawn = fromB3(b3Body_GetTransform(r.body[0]).q);
    const float c = za::clamp(za::abs(glm::dot(glm::normalize(turn * pr), drawn)), 0.f, 1.f);
    const float angle = 2.f * za::acos(c);
    constexpr float unitsPerRadian = 12.f;
    return err + (angle * unitsPerRadian) * (angle * unitsPerRadian);
}
} // namespace

// Where a monster of box mins..maxs can stand near `at` (its middle there), its ragdoll lying from `from` (its pelvis):
// `at`, else around it, out to `range` units in rings; each place reached from `from` without crossing a wall, on a
// floor within a step of the one under the body (never one above or below it), its box clear (of the world, monsters
// and players). False if none.
// Lying in water, slime or lava (NOTES.md vrstart_2026-10-09_17-55-49: a knockdown floating in deep water never got up;
// one over the bottom stood on it, under water, and drowned): a floor anywhere under the place, with its eyes out of
// the liquid there, reached over the body (from up to 96 units over it: out over a pool's edge or onto a quay), out to
// `range` (a get-up's: where it stands up, near the body; ROUND22.md, "Carried into water: no snap back") or, `range` 0
// (a swimmer's goal: livingSwim), vr_knockdown_water_search: shallow water's bottom or the bank (a get-up searching that
// far stood up on the bank it had been carried from, 190 units off). `anywhere` (QC: no such place for a while): the box
// clear at the body or around it, floor or none (it stands up in the liquid, a walking monster again, and sinks).
[[nodiscard]] bool standSpot(edict_t* ent, const glm::vec3& from, const glm::vec3& at, const glm::vec3& mins,
    const glm::vec3& maxs, float range, glm::vec3& out, bool anywhere)
{
    const auto point = [](const glm::vec3& a, const glm::vec3& b, edict_t* pass) {
        vec3_t va{a.x, a.y, a.z}, vb{b.x, b.y, b.z};
        return SV_Move(va, vec3_origin, vec3_origin, vb, MOVE_NOMONSTERS, pass);
    };
    const auto wetAt = [](const glm::vec3& p) {
        vec3_t v{p.x, p.y, p.z};
        const int c = VR_LiquidContents(sv.worldmodel, v, SV_PointContents(v));
        return c <= CONTENTS_WATER && c >= CONTENTS_LAVA;
    };
    const auto boxClear = [&](const glm::vec3& c) {
        vec3_t o{c.x, c.y, c.z};
        vec3_t lo{mins.x, mins.y, mins.z}, hi{maxs.x, maxs.y, maxs.z};
        const trace_t box = SV_Move(o, lo, hi, o, MOVE_NORMAL, ent);
        return !box.startsolid && !box.allsolid;
    };
    const glm::vec3 eye = from + glm::vec3{0.f, 0.f, 2.f};
    constexpr float step = 8.f;
    if(anywhere)
    {
        const glm::vec3 base{at.x, at.y, from.z - (mins.z + maxs.z) * 0.5f};
        for(float radius = 0.f; radius <= za::max(range, 0.f) + 0.01f; radius += step)
        {
            const int dirs = radius > 0.f ? za::clamp(static_cast<int>(glm::two_pi<float>() * radius / step), 8, 32) : 1;
            for(int d = 0; d < dirs; d++)
            {
                const float a = glm::two_pi<float>() * static_cast<float>(d) / static_cast<float>(dirs);
                const glm::vec3 c = base + glm::vec3{za::cos(a), za::sin(a), 0.f} * radius;
                if(point(eye, glm::vec3{c.x, c.y, eye.z}, ent).fraction >= 1.f)
                {
                    // (Lying on a pool's bottom, the box about its pelvis reaches into the floor: lifted out of it.)
                    for(const float lift : {0.f, 8.f, 16.f, 24.f, 32.f})
                    {
                        const glm::vec3 o = c + glm::vec3{0.f, 0.f, lift};
                        if(boxClear(o))
                        {
                            out = o;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
    if(wetAt(eye) || wetAt(from))
    {
        // Over the body: up to 96 units (vrstart's island stands 56 over its sea), under the ceiling.
        const trace_t up = point(eye, eye + glm::vec3{0.f, 0.f, 96.f}, ent);
        const glm::vec3 top{eye.x, eye.y, up.endpos[2] - 1.f};
        const trace_t down = point(eye, eye - glm::vec3{0.f, 0.f, 1024.f}, ent);
        const float bottom = down.fraction < 1.f ? down.endpos[2] : eye.z - 1024.f;
        const float eyes = (maxs.z - mins.z) * 0.8f; // (over its feet: a monster's eyes, about)
        int walls = 0, noFloor = 0, deep = 0, high = 0, noRoom = 0; // (vr_knockdown_debug 2: why each place wasn't)
        const auto tryWet = [&](const glm::vec3& c) {
            const glm::vec3 over{c.x, c.y, top.z};
            if(point(top, over, ent).fraction < 1.f)
            {
                ++walls;
                return false; // (a wall between, over the body)
            }
            const trace_t tr = point(over, glm::vec3{c.x, c.y, bottom - 32.f}, ent);
            if(tr.fraction >= 1.f || tr.plane.normal[2] < 0.7f)
            {
                ++noFloor;
                return false;
            }
            const glm::vec3 floor{tr.endpos[0], tr.endpos[1], tr.endpos[2]};
            if(wetAt(floor + glm::vec3{0.f, 0.f, eyes}))
            {
                ++deep;
                return false; // (its head under: deep here)
            }
            if(range > 0.f && floor.z > from.z + za::max(vr_knockdown_water_climb.value, 0.f))
            {
                ++high;
                return false; // (a get-up's: a bank far over its body, carried down there; it swims up to it first)
            }
            // (On a slope, the box over the point meets the slope: up to a step higher, it falls onto it.)
            for(const float lift : {0.25f, 6.f, 12.f, 18.f})
            {
                const glm::vec3 o{c.x, c.y, floor.z - mins.z + lift};
                if(boxClear(o))
                {
                    out = o;
                    return true;
                }
            }
            ++noRoom;
            return false;
        };
        const auto report = [&](bool found) {
            if(vr_knockdown_debug.value >= 2.f)
            {
                Con_Printf("knockdown: %d in a liquid at %.0f %.0f %.0f (over it to %.0f, its bottom %.0f): %s; places with a "
                           "wall between %d, no floor %d, too deep %d, too high %d, no room %d\n",
                    NUM_FOR_EDICT(ent), from.x, from.y, from.z, top.z, bottom, found ? "a floor found" : "none", walls,
                    noFloor, deep, high, noRoom);
            }
            return found;
        };
        if(tryWet(at))
        {
            return report(true);
        }
        const float wetRange = range > 0.f ? range : za::max(vr_knockdown_water_search.value, 0.f);
        for(float radius = step; radius <= wetRange + 0.01f; radius += radius < 48.f ? step : 2.f * step)
        {
            const int dirs = za::clamp(static_cast<int>(glm::two_pi<float>() * radius / step), 8, 32);
            for(int d = 0; d < dirs; d++)
            {
                const float a = glm::two_pi<float>() * (static_cast<float>(d) + (static_cast<int>(radius / step) % 2 ? 0.5f : 0.f)) /
                                static_cast<float>(dirs);
                if(tryWet(at + glm::vec3{za::cos(a), za::sin(a), 0.f} * radius))
                {
                    return report(true);
                }
            }
        }
        return report(false);
    }
    // The floor under the body.
    trace_t down = point(eye, eye - glm::vec3{0.f, 0.f, 128.f}, ent);
    const float floorZ = down.fraction < 1.f ? down.endpos[2] : from.z - 24.f;
    const float height = eye.z;
    // A slope up to vr_knockdown_getup_slope is floor to stand on (ROUND22.md, "Knocked down on a slope": enforcers slid
    // down a ramp and never got up: the box over the point met the slope, no room anywhere). Steeper: none (it slides on;
    // the QC's forced get-up, vr_knockdown_getup_force, stands it up anywhere at last).
    const float steepest = glm::radians(za::clamp(vr_knockdown_getup_slope.value, 0.f, 60.f));
    const float flattest = za::cos(steepest + glm::radians(1.f)); // (a degree over: a map's 30 is 30.03)
    const float corner = glm::length(glm::vec2{za::max(-mins.x, maxs.x), za::max(-mins.y, maxs.y)});
    const auto tryAt = [&](const glm::vec3& c) {
        const glm::vec3 level{c.x, c.y, height};
        if(point(eye, level, ent).fraction < 1.f)
        {
            return false; // (a wall between)
        }
        const trace_t tr = point(level, level - glm::vec3{0.f, 0.f, height - floorZ + 32.f}, ent);
        if(tr.fraction >= 1.f || tr.plane.normal[2] < flattest || za::abs(tr.endpos[2] - floorZ) > 24.f)
        {
            return false; // (no floor near the body's: a ledge, a drop, a step too high, too steep)
        }
        // (On a slope its box over the point meets the slope uphill: lifted clear by as much as its corner rises, about;
        // it settles onto the slope.)
        const float nz = za::clamp(tr.plane.normal[2], 0.05f, 1.f);
        const float rise = corner * za::sqrt(za::max(1.f - nz * nz, 0.f)) / nz;
        vec3_t lo{mins.x, mins.y, mins.z}, hi{maxs.x, maxs.y, maxs.z};
        const float most = rise > 0.5f ? rise + 1.f : 0.25f; // (level: as before, at its floor)
        for(float lift = 0.25f;; lift = za::min(lift + 4.f, most))
        {
            vec3_t o{c.x, c.y, tr.endpos[2] - mins.z + lift};
            const trace_t box = SV_Move(o, lo, hi, o, MOVE_NORMAL, ent);
            if(!box.startsolid && !box.allsolid)
            {
                out = glm::vec3{o[0], o[1], o[2]};
                return true;
            }
            if(lift >= most)
            {
                return false;
            }
        }
    };
    if(tryAt(at))
    {
        return true;
    }
    for(float radius = step; radius <= za::max(range, 0.f) + 0.01f; radius += step)
    {
        const int dirs = za::clamp(static_cast<int>(glm::two_pi<float>() * radius / step), 8, 32);
        for(int d = 0; d < dirs; d++)
        {
            const float a = glm::two_pi<float>() * (static_cast<float>(d) + (static_cast<int>(radius / step) % 2 ? 0.5f : 0.f)) /
                            static_cast<float>(dirs);
            if(tryAt(at + glm::vec3{za::cos(a), za::sin(a), 0.f} * radius))
            {
                return true;
            }
        }
    }
    return false;
}
} // namespace box3d::detail

namespace qvr::box3d
{
bool ragdollKnockdown(edict_t* ent)
{
    if(!canRagdoll(ent) || !knockedDown(ent))
    {
        return false;
    }
    const int num = NUM_FOR_EDICT(ent);
    for(za::SizeT i = 0; i < world->recoveries.size(); i++)
    {
        if(world->recoveries[i].num == num)
        {
            world->recoveries.eraseAt(i); // (knocked down again as it got up)
            break;
        }
    }
    Slot& s = slotOf(num);
    if(s.ragdoll >= 0)
    {
        return true;
    }
    destroyBody(s);
    const glm::vec3 vel = vec(ent->v.velocity);
    if(!createRagdoll(ent, num, s))
    {
        return false;
    }
    (void)vel;
    writeRagdoll(ent, s); // (drawn as one at once)
    if(vr_knockdown_debug.value)
    {
        Con_Printf("knockdown: %d %s down as a ragdoll (%d ragdolls)\n", num, PR_GetString(ent->v.classname), ragdollCount());
    }
    return true;
}

namespace
{
// ---- Active staggers, falls and get-ups (vr_ragdoll_active_stagger, vr_ragdoll_active_getup; vr_ragactive.hpp;
// ROUND22.md, "Active ragdolls: stagger, judo, get-up") ----
// A living monster of the active classes (activeClasses): a shove makes it stagger as a pinned ragdoll (ragdollStagger;
// QC VR_Knockdown_Try) that falls (a knockdown) or recovers (ragdollStaggered: handed back to the animated monster where
// it stands, its AI on); going down (a stagger's fall, a shove's knockdown, a throw) its arms brace (startFall); getting
// up (ragdollGetUp), its body rises through its get-up and is handed back standing.

// Whether `ent`'s kind is driven alive (vr_ragdoll_active, its class's).
[[nodiscard]] bool activeLiving(edict_t* ent)
{
    return vr_ragdoll_active.value != 0.f && ent && !ent->free && ent->v.health > 0.f && activeClassOf(ent) != nullptr;
}

// The drive begun on `r` (a living one's ragdoll), its clip's model where `ent` stands now.
void beginLiving(RagdollBodies& r, edict_t* ent)
{
    ragactive::begin(r.active, activeParts(r), modelOf(ent), glmv(world->toM(vec(ent->v.origin))), r.joints.data(),
        r.jointFriction, r.jointStiffness, qcvm->time);
}

// The frame a get-up from `from` ends standing in (its frames named nothing in Quake VR's models: found by the pose):
// going on from it (never into a death, at most `most` frames), the first whose pelvis is up as high as it stands
// (frame 0's: its standing frame) but an eighth of the way from `from`'s; backwards for a death (`back`: to its
// first). `from` if none.
[[nodiscard]] int getupEnd(const ragdoll::Rig& rig, int from, int most, bool back)
{
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(rig.model)));
    if(!hdr || from < 0 || from >= hdr->numframes)
    {
        return from;
    }
    const auto pelvisZ = [&rig](int f) {
        glm::quat q;
        glm::vec3 t;
        ragdoll::bonePose(rig, ragdoll::poseOfFrame(rig.model, f), 0, q, t);
        return (q * rig.bones[0].pivot + t).z;
    };
    const float low = pelvisZ(from), stand = pelvisZ(0);
    if(back)
    {
        for(int d = 0; d < rig.deaths; d++)
        {
            if(from >= rig.deathFirst[d] && from <= rig.deathLast[d])
            {
                return rig.deathFirst[d];
            }
        }
        return from;
    }
    for(int f = from + 1; f < hdr->numframes && f <= from + most; f++)
    {
        if(ragdoll::deathProgress(rig, f) >= 0.f)
        {
            return f - 1;
        }
        if(pelvisZ(f) >= low + 0.875f * (stand - low))
        {
            return f;
        }
    }
    return za::min(from + most, hdr->numframes - 1);
}

// Whether `p` (units) is in water, slime or lava.
[[nodiscard]] bool liquidAt(const glm::vec3& p)
{
    vec3_t v{p.x, p.y, p.z};
    const int c = VR_LiquidContents(sv.worldmodel, v, SV_PointContents(v));
    return c <= CONTENTS_WATER && c >= CONTENTS_LAVA;
}

// `ent` going down (its ragdoll made: a stagger gone over, a shove's knockdown, a throw) towards level `dir`: its arms
// brace (ragactive::beginFall), parts `slack` slack for `slackTime` s (a throw's held ones), `curl` a throw's.
void startFall(edict_t* ent, RagdollBodies& r, const glm::vec3& dir, uint32_t slack, float slackTime, bool curl,
    float legsHold = 0.f)
{
    if(vr_ragdoll_active_stagger.value == 0.f || !activeLiving(ent))
    {
        return;
    }
    if(!r.active.on)
    {
        beginLiving(r, ent);
        const int frame = static_cast<int>(ent->v.frame); // (the pose it went down in: its tone holds it)
        r.active.clip = ragactive::Clip{frame, frame, static_cast<float>(frame), qcvm->time, 10.f, false};
    }
    ragactive::beginFall(r.active, activeParts(r), dir, slack, slackTime, curl, qcvm->time, legsHold);
    if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
    {
        Con_Printf("fall: %d %s goes down towards %.2f %.2f, its arms bracing%s\n", r.num, PR_GetString(ent->v.classname), dir.x,
            dir.y, curl ? ", thrown (curling, slack where held)" : "");
    }
}

// `ent`'s ragdoll `r` handed back to the animated monster (a get-up, a stagger recovered, a rise done): its last pose
// kept for the blend over `duration` s (a Recovery), the hands holding it let go, its parts gone; `ent` standing at `spot`
// turned `yaw` (rad), in frame `frame` (-1: as it is), .vr_knockdown 2. Its pose's distance from the animated one (the
// blend's to cover: the most and the mean over its vertices, units) printed with vr_knockdown_debug, `why` its cause.
// (`frame`: the QC's get-up sets the same frame itself.)
// (`spot` by value: it may be r's own, which its parts' end clears.)
void handBack(edict_t* ent, RagdollBodies& r, int frame, float yaw, const glm::vec3 spot, float duration, const char* why)
{
    const int num = r.num;
    Recovery rec;
    rec.num = num;
    rec.rig = r.rig;
    rec.scale = r.scale;
    rec.count = r.count;
    for(int b = 0; b < r.count; b++)
    {
        const b3WorldTransform xf = b3Body_GetTransform(r.body[static_cast<za::SizeT>(b)]);
        rec.rot[static_cast<za::SizeT>(b)] = fromB3(xf.q);
        rec.pos[static_cast<za::SizeT>(b)] = world->toU(xf.p);
    }
    if(vr_knockdown_debug.value)
    {
        // The gap: each vertex as the ragdoll has it, against the animated frame where it stands.
        const int pose = ragdoll::poseOfFrame(r.rig->model, frame >= 0 ? frame : static_cast<int>(ent->v.frame));
        const glm::quat turn = glm::angleAxis(yaw, glm::vec3{0.f, 0.f, 1.f});
        float most = 0.f, sum = 0.f;
        int n = 0;
        for(int b = 0; b < r.count; b++)
        {
            if(partCut(r, b) || r.rig->bones[b].joint == ragdoll::Joint::Loose)
            {
                continue;
            }
            glm::quat ar;
            glm::vec3 ap;
            animatedBone(*r.rig, pose, b, turn, spot, r.scale, ar, ap);
            for(const glm::vec3& v : r.rig->bones[b].points)
            {
                const glm::vec3 a = rec.rot[static_cast<za::SizeT>(b)] * (v * r.scale) + rec.pos[static_cast<za::SizeT>(b)];
                const float d = glm::distance(a, ar * (v * r.scale) + ap);
                most = za::max(most, d);
                sum += d;
                n++;
            }
        }
        const glm::vec3 lay = rec.pos[0];
        Con_Printf("handback: %d %s %s: its pose from the animated frame %d: %.1f units at most, %.1f mean; blended over "
                   "%.2f s; its pelvis at %.0f %.0f %.0f, it stands at %.0f %.0f %.0f (%.0f units across)\n",
            num, PR_GetString(ent->v.classname), why, frame >= 0 ? frame : static_cast<int>(ent->v.frame), most,
            n > 0 ? sum / static_cast<float>(n) : 0.f, duration, lay.x, lay.y, lay.z, spot.x, spot.y, spot.z,
            glm::length(glm::vec2{spot - lay}));
    }
    rec.start = qcvm->time;
    if(vr_knockdown_debug.value >= 2.f)
    {
        ragdoll::watchGetup(num, qcvm->time, qcvm->time + 2.5);
    }
    rec.duration = za::max(duration, 0.f);
    destroyBody(slotOf(num));
    world->recoveries.pushBack(rec);
    if(fields().vr_knockdown >= 0)
    {
        fieldFloat(ent, fields().vr_knockdown) = 2.f;
    }
    store(spot, ent->v.origin);
    ent->v.angles[0] = ent->v.angles[2] = 0.f;
    ent->v.angles[1] = glm::degrees(yaw);
    ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
    if(frame >= 0)
    {
        ent->v.frame = static_cast<float>(frame);
    }
    // (Drawn as it lay until the step's blend: no frame of its animated model in between.)
    ragdoll::publish(num, rec.rig, rec.count, rec.rot.data(), rec.pos.data(), rec.scale, qcvm->time + host_frametime, 0, 0,
        limbsBleed(ent));
}

} // namespace

bool ragdollCanStagger(edict_t* ent)
{
    // (Not wading: its body would float on as it staggers. A shove there is the knockdown's chance alone.)
    return world && vr_ragdoll_active_stagger.value != 0.f && activeLiving(ent) && canRagdoll(ent) &&
           !liquidAt(vec(ent->v.origin));
}

bool ragdollCanDrown(edict_t* ent)
{
    return world && vr_ragdoll_active_liquids.value != 0.f && activeLiving(ent) && canRagdoll(ent);
}

bool ragdollFlail(edict_t* ent, bool on)
{
    RagdollBodies* rp = ent && world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(!rp || !rp->rig)
    {
        return false;
    }
    RagdollBodies& r = *rp;
    ragactive::State& a = r.active;
    ragactive::Context c; // (what beginFlail and endFlail read)
    c.world = world->id;
    c.now = qcvm->time;
    c.gravity = za::max(-b3World_GetGravity(world->id).z, 0.f);
    c.liquidAt = liquidFor(r.num);
    c.num = r.num;
    c.born = r.born;
    if(!on)
    {
        const bool was = ragactive::flailing(a);
        ragactive::endFlail(a, c);
        return was;
    }
    if(!activeLiving(ent) || (a.on && ragactive::swimming(a)))
    {
        return false;
    }
    const bool fresh = !a.on;
    if(fresh)
    {
        beginLiving(r, ent);
    }
    if(ragactive::beginFlail(a, activeParts(r), c))
    {
        if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
        {
            Con_Printf("flail: %d %s lifted: it flails in the hands\n", r.num, PR_GetString(ent->v.classname));
        }
        return true;
    }
    if(fresh)
    {
        ragactive::end(a);
    }
    return false;
}

bool ragdollDrowning(edict_t* ent)
{
    RagdollBodies* rp = ent && world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(!rp)
    {
        return false;
    }
    rp->drowning = true;
    if(ragactive::swimming(rp->active) && rp->active.swim.living)
    {
        rp->active.swim.panic = 1.f; // (already swimming: frantic from now)
        rp->drowning = false;
    }
    if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
    {
        Con_Printf("drown: %d %s out of breath: it struggles as a ragdoll\n", rp->num, PR_GetString(ent->v.classname));
    }
    return true;
}

// (ROUND22.md, "A snapped neck flops")
bool ragdollNeckSnapped(edict_t* ent)
{
    RagdollBodies* rp = ent && world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(!rp || !rp->rig || vr_foegrab_necksnap_loose.value == 0.f)
    {
        return false;
    }
    RagdollBodies& r = *rp;
    const uint32_t neck = neckBones(r);
    if(neck == 0u)
    {
        return false;
    }
    const float cone = glm::radians(za::clamp(vr_foegrab_necksnap_cone.value, 0.f, 170.f));
    const float twist = glm::radians(za::clamp(vr_foegrab_necksnap_twist.value, 0.f, 175.f));
    const float friction = r.jointFriction * za::clamp(vr_foegrab_necksnap_friction.value, 0.f, 1.f);
    // (Driven now (a death's collapse, a swim): those joints left as they are set here from now, never driven:
    // activeParts' `loose`.)
    for(int b = 0; b < r.count; b++)
    {
        const b3JointId j = r.joints[static_cast<za::SizeT>(b)];
        if((neck & (1u << b)) == 0u || B3_IS_NULL(j) || !b3Joint_IsValid(j))
        {
            continue;
        }
        r.active.strength[static_cast<za::SizeT>(b)] = 0.f;
        float was = 0.f, wasTwist = 0.f;
        if(r.rig->bones[b].joint == ragdoll::Joint::Ball)
        {
            was = b3SphericalJoint_GetConeLimit(j);
            wasTwist = b3SphericalJoint_GetUpperTwistLimit(j);
            b3SphericalJoint_SetConeLimit(j, za::max(cone, was));
            b3SphericalJoint_SetTwistLimits(j, za::min(-twist, b3SphericalJoint_GetLowerTwistLimit(j)), za::max(twist, wasTwist));
            b3SphericalJoint_EnableSpring(j, false);
            b3SphericalJoint_SetTargetRotation(j, b3Quat_identity);
            b3SphericalJoint_EnableMotor(j, friction > 0.f);
            b3SphericalJoint_SetMotorVelocity(j, b3Vec3_zero);
            b3SphericalJoint_SetMaxMotorTorque(j, friction);
        }
        else if(r.rig->bones[b].joint == ragdoll::Joint::Hinge)
        {
            was = b3RevoluteJoint_GetUpperLimit(j);
            wasTwist = 0.f;
            b3RevoluteJoint_SetLimits(j, za::min(-cone, b3RevoluteJoint_GetLowerLimit(j)), za::max(cone, was));
            b3RevoluteJoint_EnableSpring(j, false);
            b3RevoluteJoint_EnableMotor(j, friction > 0.f);
            b3RevoluteJoint_SetMotorSpeed(j, 0.f);
            b3RevoluteJoint_SetMaxMotorTorque(j, friction);
        }
        b3Joint_WakeBodies(j);
        if(vr_debug_ragdoll.value || vr_knockdown_debug.value || vr_foegrab_necksnap_debug.value)
        {
            Con_Printf("necksnap: %d %s's %s joint loose: swing %.0f -> %.0f deg, twist %.0f -> %.0f deg, friction %.2f "
                       "(was %.2f), no spring\n",
                r.num, PR_GetString(ent->v.classname), r.rig->bones[b].name, glm::degrees(was), glm::degrees(za::max(cone, was)),
                glm::degrees(wasTwist), glm::degrees(za::max(twist, wasTwist)), friction, r.jointFriction);
        }
    }
    r.neckSnapped = true;
    return true;
}

bool ragdollNeckAngle(int num, float& swing, float& twist)
{
    const RagdollBodies* r = world ? ragdollOf(num) : nullptr;
    if(!r || !r->rig || r->rig->head <= 0 || r->rig->head >= r->count)
    {
        return false;
    }
    const b3JointId j = r->joints[static_cast<za::SizeT>(r->rig->head)];
    if(B3_IS_NULL(j) || !b3Joint_IsValid(j))
    {
        return false;
    }
    if(r->rig->bones[r->rig->head].joint == ragdoll::Joint::Ball)
    {
        swing = glm::degrees(b3SphericalJoint_GetConeAngle(j));
        twist = glm::degrees(b3SphericalJoint_GetTwistAngle(j));
    }
    else
    {
        swing = glm::degrees(b3RevoluteJoint_GetAngle(j));
        twist = 0.f;
    }
    return true;
}

bool ragdollGuard(edict_t* ent, const glm::vec3& at, float hold)
{
    RagdollBodies* rp = ent && world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(!rp || !rp->rig || !activeLiving(ent))
    {
        return false;
    }
    RagdollBodies& r = *rp;
    ragactive::State& a = r.active;
    if(a.on && (a.death.on || ragactive::climbing(a) || ragactive::swimming(a)))
    {
        return false; // (its hands busy)
    }
    if(!a.on)
    {
        beginLiving(r, ent); // (lying: no clip, no strength; its arms alone, by their reaches)
    }
    ragactive::guard(a, glmv(world->toM(at)), qcvm->time, hold);
    if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
    {
        Con_Printf("guard: %d %s guards its head (%s) for %.2f s\n", r.num, PR_GetString(ent->v.classname),
            a.stagger.on ? "staggering" : a.fall.on ? "falling" : a.rise.on ? "getting up" : "down", hold);
    }
    return true;
}

bool ragdollStagger(edict_t* ent, const glm::vec3& dir, float push, float balance, float reach, float recoverAt, bool sure,
    float farther)
{
    RagdollBodies* rp = ent && world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(!rp || !ragdollCanStagger(ent))
    {
        return false;
    }
    RagdollBodies& r = *rp;
    beginLiving(r, ent);
    ragactive::StaggerInfo si;
    si.dir = dir;
    // (A standing one's slide is long, the shove's game: its stagger a share of it, at most a few steps; a harder shove's
    // (`farther`: two hands over 1, a running shove) as much farther as its slide, at most 2.5 times.)
    const float mostTravel = 4.f * za::clamp(farther, 1.f, 2.5f); // m
    si.travel = za::min(za::max(reach, 0.f) / world->m2u * za::clamp(vr_ragdoll_active_stagger_travel.value, 0.f, 3.f), mostTravel);
    si.push = push;
    si.balance = balance;
    si.recoverAt = recoverAt;
    si.sure = sure;
    // Its stance: its standing frame (the classes' first: stand1), its weight over its feet (its pain's frames may be a
    // fall: the grunt's painb); its body made in the frame it was in (its pain's first: the flinch) goes back to it.
    si.first = si.last = 0;
    ragactive::beginStagger(r.active, activeParts(r), si, qcvm->time);
    r.staggerStuck = -1.0;
    if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
    {
        const ragactive::Stagger& g = r.active.stagger;
        Con_Printf("stagger: %d %s shoved (push %.2f, balance %.2f): frames %d-%d held, its pelvis pinned (%.2f body weights) "
                   "and carried %.0f units over %.2f s, %.0f units tall at the pelvis%s, up again %s %.2f s\n",
            r.num, PR_GetString(ent->v.classname), push, balance, si.first, si.last, r.active.pin.force,
            g.travel * world->m2u, g.time, g.height * world->m2u, sure ? ", sure-footed" : "",
            recoverAt > 0.f ? "at" : "by its balance, from", recoverAt > 0.f ? recoverAt : g.time + 0.2f);
    }
    return true;
}

int ragdollStaggered(edict_t* ent, const glm::vec3& mins, const glm::vec3& maxs)
{
    RagdollBodies* rp = ent && world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(!rp)
    {
        // (A loaded game's, its ragdoll not made yet: asked again.)
        return ent && world && knockedDown(ent) && ragdollCanStagger(ent) ? 1 : 0;
    }
    RagdollBodies& r = *rp;
    ragactive::State& a = r.active;
    // A loaded game's: its ragdoll made again, its stagger never begun on it (the QC begins it again).
    if(!a.on && !a.stagger.on && a.stagger.outcome == ragactive::Outcome::Going && a.stagger.decided < 0.0 &&
        ragdollCanStagger(ent))
    {
        return 3;
    }
    if(!a.on || !a.stagger.on || a.stagger.outcome == ragactive::Outcome::Fell)
    {
        return 0;
    }
    if(a.stagger.outcome == ragactive::Outcome::Going)
    {
        return 1;
    }
    // Recovered: standing where its body stands, in the frame its joints hold (fitted: its turn and place), if it fits.
    const int frame = static_cast<int>(za::max(ragactive::clipFrame(a, qcvm->time), 0.f) + 0.5f);
    float yaw = 0.f;
    glm::vec3 at{0.f};
    const ragdoll::Rig& rig = *r.rig;
    const b3WorldTransform pelvis = b3Body_GetTransform(r.body[0]);
    const glm::vec3 from = fromB3(pelvis.q) * (rig.bones[0].pivot * r.scale) + world->toU(pelvis.p);
    glm::vec3 spot{0.f};
    if(fitFrame(r, frame, yaw, at) < 1e29f && standSpot(ent, from, at, mins, maxs, 24.f, spot))
    {
        handBack(ent, r, frame, yaw, spot, za::max(vr_ragdoll_active_stagger_blend.value, 0.f), "recovered from a stagger");
        return 2;
    }
    // (No room to stand: it holds on a moment, then goes over.)
    r.staggerStuck = r.staggerStuck < 0.0 ? qcvm->time : r.staggerStuck;
    if(qcvm->time - r.staggerStuck > 0.5)
    {
        if(vr_knockdown_debug.value)
        {
            Con_Printf("stagger: %d recovered but no room to stand: it falls\n", r.num);
        }
        ragactive::lapse(a, activeParts(r), qcvm->time);
        return 0;
    }
    return 1;
}

bool ragdollThrown(edict_t* ent, const glm::vec3& dir, const glm::vec3* palms, int count, float legsHold)
{
    RagdollBodies* rp = ent && world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(!rp)
    {
        return false;
    }
    uint32_t slack = 0u;
    for(int i = 0; i < count; i++)
    {
        float distance = 0.f;
        const int part = nearestPart(*rp, palms[i], distance);
        slack |= part >= 0 && distance < 24.f ? (1u << part) : 0u;
    }
    startFall(ent, *rp, dir, slack, 0.6f, true, legsHold);
    return true;
}

bool ragdollMarkThrown(edict_t* ent, const glm::vec3* velocity)
{
    RagdollBodies* rp = ent && world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(!rp)
    {
        return false;
    }
    rp->thrownAt = qcvm->time;
    rp->thrownPeak = 0.f;
    if(!velocity || glm::length(*velocity) < 0.01f)
    {
        return true;
    }
    const b3Vec3 v = b3v(glmv(world->toM(*velocity)));
    for(int b = 0; b < rp->count; b++)
    {
        const b3BodyId body = rp->body[static_cast<za::SizeT>(b)];
        if(!B3_IS_NULL(body) && b3Body_IsValid(body))
        {
            b3Body_SetLinearVelocity(body, v);
            b3Body_SetAwake(body, true);
        }
    }
    return true;
}

bool ragdollGrace(edict_t* ent, int player, float seconds)
{
    const int num = ent && world ? NUM_FOR_EDICT(ent) : 0;
    const RagdollBodies* rp = num > 0 ? ragdollOf(num) : nullptr;
    if(!rp)
    {
        return false;
    }
    for(World::Grace& g : world->graces)
    {
        if(g.num == num && g.player == player && g.ragdoll)
        {
            g.until = g.born == rp->born ? za::max(g.until, qcvm->time + seconds) : qcvm->time + seconds;
            g.born = rp->born;
            return true;
        }
    }
    World::Grace g;
    g.num = num;
    g.player = player;
    g.born = rp->born;
    g.until = qcvm->time + seconds;
    g.ragdoll = true;
    world->graces.pushBack(g);
    return true;
}

bool ragdollTopple(edict_t* ent, const glm::vec3& dir, float topple, float spin, float feetSpeed, float toppleHold,
    float launch, bool whole)
{
    RagdollBodies* rp = ent ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(!rp)
    {
        return false;
    }
    RagdollBodies& r = *rp;
    // Its parts' middles and masses (metres), their shared motion (the throw's push and lift: createRagdoll's entVel), how
    // high it stands.
    float lowest = 1e30f, highest = -1e30f, mass = 0.f;
    glm::vec3 shared{0.f}, middle{0.f};
    za::Array<glm::vec3, ragdoll::maxBones> at{};
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        at[static_cast<za::SizeT>(b)] = glmv(b3Body_GetWorldCenter(body));
        if(r.rig->bones[b].joint == ragdoll::Joint::Loose)
        {
            continue; // (his gun, welded in his hand: moved as the hand is, not counted)
        }
        const float m = b3Body_GetMass(body);
        mass += m;
        shared += glmv(b3Body_GetLinearVelocity(body)) * m;
        middle += at[static_cast<za::SizeT>(b)] * m;
        lowest = za::min(lowest, at[static_cast<za::SizeT>(b)].z);
        highest = za::max(highest, at[static_cast<za::SizeT>(b)].z);
    }
    if(mass <= 0.f)
    {
        return false;
    }
    shared /= mass;
    middle /= mass;
    // Its feet: the parts in the lowest quarter of its height (their middles), standing on the floor (its box's bottom).
    const float floor = za::min(glmv(world->toM(vec(ent->v.absmin))).z, lowest);
    const float height = za::max(highest - floor, 0.05f);
    glm::vec3 pivot{0.f};
    int feet = 0;
    r.pinned = 0u;
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b) || r.rig->bones[b].joint == ragdoll::Joint::Loose ||
            at[static_cast<za::SizeT>(b)].z > lowest + 0.25f * (highest - lowest))
        {
            continue;
        }
        pivot += at[static_cast<za::SizeT>(b)];
        feet++;
        r.pinned |= 1u << b;
    }
    pivot = feet > 0 ? pivot / static_cast<float>(feet) : middle;
    // The sweep: the feet go back at `sweep` m/s (held so until it has turned a quarter, at most toppleHold: then they fly
    // on as they go) while the top keeps topple * height: the whole turns at topple + sweep / height about a pivot sweep /
    // turn above the feet (at most its middle height: in place); no sweep, about the floor under them, held toppleHold.
    const float sweep = whole ? 0.f : za::max(feetSpeed, 0.f) / world->m2u;
    const float turnRate = topple + sweep / height;
    pivot.z = sweep > 0.f && turnRate > 0.f ? za::min(pivot.z + sweep / turnRate, floor + 0.5f * height) : floor;
    r.feet = r.pinned;
    r.pinned = whole ? 0u : r.pinned; // (whole: nothing held, its feet go on over the edge with the rest)
    const float hold = whole                               ? 0.f
                       : sweep > 0.f && turnRate > 0.f ? za::min(toppleHold, 0.5f * 3.14159265f / turnRate)
                                                       : toppleHold;
    r.pinUntil = qcvm->time + static_cast<double>(hold);
    r.pinVel = -glm::vec2{dir.x, dir.y} * sweep;
    const glm::vec3 up{0.f, 0.f, 1.f};
    const glm::vec3 over = glm::cross(up, dir) * turnRate; // (the top goes along dir)
    const glm::vec3 turn = up * spin;
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const glm::vec3 p = at[static_cast<za::SizeT>(b)];
        const float share = za::clamp((p.z - floor) / height, 0.f, 1.f);
        glm::vec3 offset = p - middle;
        offset.z = 0.f;
        const glm::vec3 lin = glmv(b3Body_GetLinearVelocity(body));
        const glm::vec3 kept =
            whole ? lin - glm::vec3{shared.x, shared.y, 0.f} * (1.f - launch) : lin - shared * (1.f - share * launch);
        b3Body_SetLinearVelocity(body, b3v(kept + glm::cross(over, p - pivot) + glm::cross(turn, offset)));
        b3Body_SetAngularVelocity(body, b3v(glmv(b3Body_GetAngularVelocity(body)) + over + turn));
        b3Body_SetAwake(body, true);
    }
    if(vr_knockdown_debug.value || vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d thrown over its feet: %.0f deg/s (topple %.0f, its feet swept back at %.0f u/s, the pivot "
                   "%.1f units up) towards %.2f %.2f, spin %.0f deg/s, %d feet held %.2f s, %.0f units tall, its launch "
                   "%.0f u/s %s\n",
            NUM_FOR_EDICT(ent), glm::degrees(turnRate), glm::degrees(topple), sweep * world->m2u,
            (pivot.z - floor) * world->m2u, dir.x, dir.y, glm::degrees(spin), whole ? 0 : feet, hold, height * world->m2u,
            glm::length(shared) * world->m2u, whole ? "kept whole (over a ledge)" : "shared by height");
    }
    return true;
}

bool ragdollShove(edict_t* ent, const glm::vec3& dir, const RagdollShove& p)
{
    RagdollBodies* rp = ent ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(!rp)
    {
        return false;
    }
    RagdollBodies& r = *rp;
    RagdollBodies::ShoveDrive& d = r.shove;
    if(p.ledge)
    {
        // (Pushed whole and turned as it goes over, its launch cut to `keep`: ragdollTopple's whole; then kept going.)
        float speed = 0.f;
        glm::vec3 com{0.f};
        float mass = 0.f;
        for(int b = 0; b < r.count; b++)
        {
            if(partCut(r, b) || r.rig->bones[b].joint == ragdoll::Joint::Loose)
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            const float m = b3Body_GetMass(body);
            mass += m;
            com += glmv(b3Body_GetWorldCenter(body)) * m;
            speed += glm::dot(glmv(b3Body_GetLinearVelocity(body)), dir) * m;
        }
        if(mass <= 0.f || !ragdollTopple(ent, dir, p.topple, 0.f, 0.f, 0.f, p.keep, true))
        {
            return false;
        }
        d = RagdollBodies::ShoveDrive{};
        d.on = true;
        d.ledge = true;
        d.maxSpin = za::max(p.maxSpin, 0.1f);
        d.rate = za::max(p.topple, 0.f);
        d.angle = za::max(p.angle, 0.f);
        d.start = qcvm->time;
        d.dir = dir;
        d.speed = za::max(speed / mass * p.keep, p.minSpeed / world->m2u);
        d.reach = p.reach / world->m2u;
        d.over = p.over;
        d.from = com / mass;
        d.floor = glmv(world->toM(vec(ent->v.absmin))).z;
        if(vr_knockdown_debug.value || vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d shoved over a ledge: kept at %.0f u/s along the shove until %.0f units on (then at most "
                       "%.0f u/s), turning %.0f deg/s\n",
                NUM_FOR_EDICT(ent), d.speed * world->m2u, p.reach, d.speed * world->m2u * za::min(d.over, 1.f),
                glm::degrees(p.topple));
        }
        startFall(ent, r, dir, 0u, 0.f, false);
        return true;
    }
    // Its parts' middles, masses and launch (metres), how high it stands, its feet (the lowest quarter).
    float lowest = 1e30f, highest = -1e30f, mass = 0.f;
    glm::vec3 com{0.f}, shared{0.f};
    za::Array<glm::vec3, ragdoll::maxBones> at{};
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        at[static_cast<za::SizeT>(b)] = glmv(b3Body_GetWorldCenter(body));
        if(r.rig->bones[b].joint == ragdoll::Joint::Loose)
        {
            continue;
        }
        const float m = b3Body_GetMass(body);
        mass += m;
        com += at[static_cast<za::SizeT>(b)] * m;
        shared += glmv(b3Body_GetLinearVelocity(body)) * m;
        lowest = za::min(lowest, at[static_cast<za::SizeT>(b)].z);
        highest = za::max(highest, at[static_cast<za::SizeT>(b)].z);
    }
    if(mass <= 0.f)
    {
        return false;
    }
    com /= mass;
    shared /= mass;
    const float floor = za::min(glmv(world->toM(vec(ent->v.absmin))).z, lowest);
    const float height = za::max(highest - floor, 0.05f);
    d = RagdollBodies::ShoveDrive{};
    r.pinned = 0u;
    r.feet = 0u;
    float meanShare = 0.f;
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        const float share = za::clamp((at[static_cast<za::SizeT>(b)].z - floor) / height, 0.f, 1.f);
        d.share[static_cast<za::SizeT>(b)] = share;
        if(r.rig->bones[b].joint == ragdoll::Joint::Loose)
        {
            continue;
        }
        meanShare += share * b3Body_GetMass(r.body[static_cast<za::SizeT>(b)]);
        r.feet |= at[static_cast<za::SizeT>(b)].z <= lowest + 0.25f * (highest - lowest) ? 1u << b : 0u;
    }
    // Its travel: its pelvis `travel` units on when it lies (what is seen of where it went; the turn about its feet carries
    // it its own height on, the drive the rest), at most the shove's own speed, over at least the turn's time.
    const float launch = za::max(glm::dot(shared, dir), 0.f);
    const float pelvisUp = (partCut(r, 0) ? com.z : at[0].z) - floor;
    const float turned = pelvisUp * za::sin(za::clamp(p.angle, 0.f, 3.14159265f * 0.5f));
    const float drive = launch > 0.f ? za::max(p.travel / world->m2u - turned, 0.f) : 0.f;
    const float duration = za::max(za::max(p.time, 0.05f), launch > 0.f ? 2.f * drive / launch : 0.f);
    d.on = true;
    d.start = qcvm->time;
    d.dir = dir;
    d.speed = 2.f * drive / duration;
    d.decel = d.speed / duration;
    d.duration = duration;
    d.pelvis0 = partCut(r, 0) ? com : at[0];
    d.pelvisUp = pelvisUp;
    d.from = com;
    d.angle = za::max(p.angle, 0.f);
    d.time = d.angle > 0.f ? za::max(p.time, 0.05f) : 0.f;
    d.lag = za::clamp(p.lag, 0.f, 1.f);
    d.maxSpin = za::max(p.maxSpin, 0.1f);
    d.meanShare = meanShare / mass;
    // Its start: the shove's launch along it replaced by the travel's (its hop kept), no turn yet.
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const glm::vec3 lin = glmv(b3Body_GetLinearVelocity(body));
        b3Body_SetLinearVelocity(body, b3v(glm::vec3{d.dir.x * d.speed, d.dir.y * d.speed, lin.z}));
        b3Body_SetAngularVelocity(body, b3Vec3{0.f, 0.f, 0.f});
        b3Body_SetAwake(body, true);
    }
    if(vr_knockdown_debug.value || vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d shoved down: %.0f units on (%.0f of it its turn about its feet), %.0f u/s slowing to none "
                   "over %.2f s (its launch %.0f u/s); turned %.0f deg over %.2f s, its feet %.2f behind, at most %.0f "
                   "deg/s; %.0f units tall\n",
            NUM_FOR_EDICT(ent), p.travel, turned * world->m2u, d.speed * world->m2u, duration, launch * world->m2u,
            glm::degrees(d.angle), d.time, d.lag, glm::degrees(d.maxSpin), height * world->m2u);
    }
    startFall(ent, r, dir, 0u, 0.f, false);
    return true;
}

bool ragdollStance(int num, glm::vec3& pelvis, glm::vec3& head, glm::vec3& feet)
{
    RagdollBodies* r = ragdollOf(num);
    if(!r || r->count < 1)
    {
        return false;
    }
    const auto centre = [&](int b) { return world->toU(b3Body_GetWorldCenter(r->body[static_cast<za::SizeT>(b)])); };
    const auto counted = [&](int b) { return !partCut(*r, b) && r->rig->bones[b].joint != ragdoll::Joint::Loose; };
    if(r->feet == 0u)
    {
        // (Not toppled: its feet the parts in its lowest quarter now, as ragdollTopple's, kept for the next calls.)
        float lowest = 1e30f, highest = -1e30f;
        for(int b = 0; b < r->count; b++)
        {
            lowest = counted(b) ? za::min(lowest, centre(b).z) : lowest;
            highest = counted(b) ? za::max(highest, centre(b).z) : highest;
        }
        for(int b = 0; b < r->count; b++)
        {
            r->feet |= counted(b) && centre(b).z <= lowest + 0.25f * (highest - lowest) ? 1u << b : 0u;
        }
    }
    pelvis = centre(0);
    int top = r->rig->head >= 0 && r->rig->head < r->count && !partCut(*r, r->rig->head) ? r->rig->head : -1;
    glm::vec3 sum{0.f};
    int held = 0;
    for(int b = 0; b < r->count; b++)
    {
        if(!counted(b))
        {
            continue;
        }
        const glm::vec3 c = centre(b);
        if((r->feet & (1u << b)) != 0u)
        {
            sum += c;
            held++;
        }
        top = r->rig->head < 0 && (top < 0 || c.z > centre(top).z) ? b : top;
    }
    head = top >= 0 ? centre(top) : pelvis;
    feet = held > 0 ? sum / static_cast<float>(held) : pelvis;
    return true;
}

// A body's active get-ups lapsed (RagdollBodies::riseLapses) before it gets up the old way (blended from the floor).
constexpr int riseLapsesMost = 2;

int ragdollGetUp(edict_t* ent, int frameA, int frameB, const glm::vec3& mins, const glm::vec3& maxs, float range, bool anywhere)
{
    if(!world || !ent || ent->free)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(ent);
    RagdollBodies* r = ragdollOf(num);
    if(!r || !r->rig)
    {
        return 0;
    }
    const ragdoll::Rig& rig = *r->rig;
    // Hanging from a ledge or pulling itself up (vr_ragdoll_active_pullup): it stays down until it is over (asked again
    // after vr_knockdown_retry).
    if(ragactive::climbing(r->active))
    {
        return 0;
    }
    // Carried in a liquid by a player's hand (ROUND22.md, "Carried into water: no snap back"): it swims in his grip and
    // stays down until let go of (then it paddles to where it can stand, as one shoved in, the QC's give-up in a liquid
    // timed from then: -1, asked again).
    if(!r->active.rise.on && liquidAt(world->toU(b3Body_GetPosition(r->body[0]))))
    {
        for(const World::RagdollGrab& g : world->ragdollGrabs)
        {
            if(g.num == num)
            {
                if(vr_knockdown_debug.value && !r->heldWet)
                {
                    Con_Printf("knockdown: %d held in a liquid: stays down until let go of\n", num);
                }
                r->heldWet = true;
                return -1;
            }
        }
    }
    r->heldWet = false;
    // Its body rising (vr_ragdoll_active_getup): on until it stands (-1), then handed back there (3 or 4: risen), if
    // its spot is still free (it was chosen 1-2.5 s ago: the player or a monster may stand there now).
    if(r->active.on && r->active.rise.on)
    {
        r->active.rise.polled = qcvm->time;
        if(!r->active.rise.done)
        {
            return -1;
        }
        const int frame = static_cast<int>(ragactive::clipFrame(r->active, qcvm->time) + 0.5f);
        const int which = r->riseWhich;
        const b3WorldTransform pelvis = b3Body_GetTransform(r->body[0]);
        const glm::vec3 from = fromB3(pelvis.q) * (rig.bones[0].pivot * r->scale) + world->toU(pelvis.p);
        glm::vec3 spot{0.f};
        if(standSpot(ent, from, r->riseSpot, mins, maxs, 24.f, spot))
        {
            if(vr_knockdown_debug.value && glm::distance(spot, r->riseSpot) > 0.5f)
            {
                Con_Printf("getup: %d risen, its spot taken: stands %.0f units off it\n", num, glm::distance(spot, r->riseSpot));
            }
            r->riseAsked = false;
            handBack(ent, *r, frame, r->riseYaw, spot, za::max(vr_ragdoll_active_getup_blend.value, 0.f), "risen");
            return which + 2;
        }
        // (No room left: it holds on a moment, standing, then slumps and tries again later.)
        r->riseStuck = r->riseStuck < 0.0 ? qcvm->time : r->riseStuck;
        if(qcvm->time - r->riseStuck <= 0.5)
        {
            return -1;
        }
        if(vr_knockdown_debug.value)
        {
            Con_Printf("getup: %d risen but its spot is taken: it slumps\n", num);
        }
        ragactive::lapse(r->active, activeParts(*r), qcvm->time);
    }
    // Its last rise lapsed (lost its pose, or its spot taken) while still asked for: it waits vr_knockdown_retry before
    // trying again (asked again at once, it was pulled in again mid-slump, for ever).
    if(r->riseAsked)
    {
        r->riseAsked = false;
        r->riseStuck = -1.0;
        r->riseLapses++;
        if(vr_knockdown_debug.value)
        {
            Con_Printf("getup: %d its get-up lapsed (%d so far): tries again later%s\n", num, r->riseLapses,
                r->riseLapses >= riseLapsesMost ? " (blended from the floor)" : "");
        }
        return 0;
    }
    float yawA = 0.f, yawB = 0.f;
    glm::vec3 atA{0.f}, atB{0.f};
    const float errA = frameA >= 0 ? fitFrame(*r, frameA, yawA, atA) : 1e30f;
    const float errB = frameB >= 0 ? fitFrame(*r, frameB, yawB, atB) : 1e30f;
    if(errA >= 1e29f && errB >= 1e29f)
    {
        return 0;
    }
    const int which = errB < errA ? 2 : 1;
    const float yaw = which == 2 ? yawB : yawA;
    const glm::vec3 at = which == 2 ? atB : atA;
    const b3WorldTransform pelvis = b3Body_GetTransform(r->body[0]);
    const glm::vec3 from = fromB3(pelvis.q) * (rig.bones[0].pivot * r->scale) + world->toU(pelvis.p);
    glm::vec3 spot;
    if(!standSpot(ent, from, at, mins, maxs, range, spot, anywhere))
    {
        if(vr_knockdown_debug.value)
        {
            Con_Printf("knockdown: %d no room to get up within %.0f units%s: stays down\n", num, range,
                anywhere ? " (anywhere)" : "");
        }
        return 0;
    }

    // Its body rises (vr_ragdoll_active_getup, its kind's; not in a liquid, nor anywhere: those stand up as before): its
    // get-up's frames from the chosen one (its own fall-and-rise on to its end, or a death backwards to its first),
    // after a moment pulled into it (longer the more its pelvis must turn and the farther the place).
    const int chosen = which == 2 ? frameB : frameA;
    if(vr_ragdoll_active_getup.value != 0.f && activeLiving(ent) && !anywhere && !liquidAt(from) && r->riseLapses < riseLapsesMost)
    {
        const int last = getupEnd(rig, chosen, 12, ragdoll::deathProgress(rig, chosen) >= 0.f);
        if(last != chosen)
        {
            glm::quat pr;
            glm::vec3 pt;
            ragdoll::bonePose(rig, ragdoll::poseOfFrame(rig.model, chosen), 0, pr, pt);
            const glm::quat want = glm::angleAxis(yaw, glm::vec3{0.f, 0.f, 1.f}) * pr;
            const float turn = 2.f * za::acos(za::clamp(za::abs(glm::dot(glm::normalize(want), fromB3(pelvis.q))), 0.f, 1.f));
            const float away = glm::length(glm::vec2{spot - at});
            beginLiving(*r, ent);
            ragactive::RiseInfo ri;
            ri.first = chosen;
            ri.last = last;
            ri.fps = 10.f * za::clamp(vr_knockdown_getup_speed.value, 0.1f, 10.f) * za::clamp(vr_ragdoll_active_getup_pace.value, 0.1f, 3.f);
            ri.prep = 0.35f + 0.4f * turn / glm::pi<float>() + 0.3f * za::min(away / 48.f, 1.f);
            ri.origin = glmv(world->toM(spot));
            ri.yaw = yaw;
            ragactive::beginRise(r->active, activeParts(*r), ri, qcvm->time);
            r->riseWhich = which;
            r->riseSpot = spot;
            r->riseYaw = yaw;
            r->riseAsked = true;
            r->riseStuck = -1.0;
            if(vr_knockdown_debug.value)
            {
                Con_Printf("getup: %d rises through frames %d to %d (fit %.0f), %.0f units from where it lay (at %.0f %.0f %.0f), "
                           "its pelvis to turn %.0f deg: pulled in over %.2f s, then %.2f s\n",
                    num, chosen, last, which == 2 ? errB : errA, glm::length(glm::vec2{spot - at}), spot.x, spot.y, spot.z,
                    glm::degrees(turn), ri.prep, static_cast<float>(za::abs(last - chosen)) / ri.fps);
            }
            return -1;
        }
    }

    // Its last pose kept for the blend, the hands holding it let go, its parts gone.
    handBack(ent, *r, chosen, yaw, spot, za::max(vr_knockdown_blend.value, 0.f), "gets up (blended from the floor)");
    if(vr_knockdown_debug.value)
    {
        const float other = which == 2 ? errA : errB;
        Con_Printf("knockdown: %d gets up from frame %d (fit %.0f, the other's %.0f), %.0f units from where it lay, yaw %.0f\n",
            num, which == 2 ? frameB : frameA, which == 2 ? errB : errA, other < 1e29f ? other : -1.f,
            glm::length(glm::vec2{spot - at}), glm::degrees(yaw));
    }
    return which;
}
} // namespace qvr::box3d
