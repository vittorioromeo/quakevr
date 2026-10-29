// vr_carry2h.cpp -- see vr_carry2h.hpp.

#include "vr_carry2h.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_grip.hpp"
#include "vr_gripfit.hpp"
#include "vr_held.hpp"
#include "vr_physics.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_units.hpp"
#include "vr_portals.hpp"
#include "vr_server.hpp"
#include "vr_move.hpp"
#include "vr_view.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"


namespace qvr::carry2h
{
namespace
{

// A hand pulled off a prop held in both hands still holds it while its fist is within this (metres) of its surface.
constexpr float detachTouch = 0.02f;

// A prop held in both hands stopped by a wall (vr_carry_two_hands_solid): for this long (s) after, the hands may be this
// much further (metres) off it before they let go (they go on into the wall as they push it).
constexpr double wallTime = 0.3;
constexpr float wallSlack = 0.3f;


// Both hands pulled off a prop held in both: the one that moved less than half as far as the other, less this (metres),
// keeps it (one hand pulled away from the other held still).
constexpr float keepMoved = 0.03f;

// Below this far apart (metres) the line between the hands (or the grips) says nothing of the object's turn; above
// twice it, it says everything.
constexpr float noLine = 0.03f;

[[nodiscard]] float lineWeight(float distance)
{
    const float lo = noLine * units::metresToUnits();
    const float t = glm::clamp((distance - lo) / lo, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// The least turn taking the unit vector `from` to `to`; opposite, half a turn about `across` (made perpendicular).
[[nodiscard]] glm::quat leastTurn(const glm::vec3& from, const glm::vec3& to, const glm::vec3& across)
{
    const float c = glm::dot(from, to);
    if(c < -0.9999f)
    {
        glm::vec3 axis = across - from * glm::dot(across, from);
        if(glm::length(axis) < 1e-4f)
        {
            axis = za::fabs(from.x) < 0.9f ? glm::cross(from, glm::vec3{1.f, 0.f, 0.f}) : glm::cross(from, glm::vec3{0.f, 1.f, 0.f});
        }
        return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
    }
    const glm::vec3 axis = glm::cross(from, to);
    return glm::normalize(glm::quat{1.f + c, axis.x, axis.y, axis.z});
}

} // namespace

Hold record(const Frame& object, const Frame hands[2])
{
    Hold h;
    const glm::quat toObject = glm::inverse(object.rot);
    for(int i = 0; i < 2; i++)
    {
        h.inHand[i] = glm::normalize(glm::inverse(hands[i].rot) * object.rot);
        h.grip[i] = toObject * (hands[i].pos - object.pos);
    }
    const glm::vec3 d = hands[1].pos - hands[0].pos;
    h.span = glm::length(d);
    h.axis = h.span > 1e-4f ? toObject * (d / h.span) : glm::vec3{1.f, 0.f, 0.f};
    return h;
}

Frame reached(const Hold& hold, const Frame& at, int hand)
{
    if(hold.reach[hand] == glm::vec3{0.f})
    {
        return at;
    }
    return {at.pos + at.rot * hold.reach[hand], at.rot};
}

Frame solve(const Hold& hold, const Frame controllers[2])
{
    // (Each hand where it holds it from: its reach.)
    const Frame hands[2]{reached(hold, controllers[0], 0), reached(hold, controllers[1], 1)};
    // The turn each hand alone would give it, averaged.
    const glm::quat a = hands[0].rot * hold.inHand[0];
    glm::quat b = hands[1].rot * hold.inHand[1];
    if(glm::dot(a, b) < 0.f)
    {
        b = -b;
    }
    glm::quat rot = glm::normalize(a + b); // |a + b| >= sqrt(2)

    // Swung onto the line between the hands.
    const glm::vec3 d = hands[1].pos - hands[0].pos;
    const float distance = glm::length(d);
    const float k = lineWeight(distance) * lineWeight(hold.span);
    if(k > 0.f)
    {
        const glm::vec3 have = rot * hold.axis;
        // Opposite (the hands swapped over): half a turn about the object's up (or forward) across the line.
        const glm::vec3 across = rot * (za::fabs(hold.axis.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f});
        const glm::quat swing = leastTurn(have, d / distance, across);
        rot = glm::normalize(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, swing, k) * rot);
    }

    // The middle of the grips on the middle of the hands.
    const glm::vec3 grips = (hold.grip[0] + hold.grip[1]) * 0.5f;
    const glm::vec3 middle = (hands[0].pos + hands[1].pos) * 0.5f;
    return {middle - rot * grips, rot};
}

Frame onGrip(const Hold& hold, const Frame& object, int hand)
{
    return {object.pos + object.rot * hold.grip[hand], glm::normalize(object.rot * glm::inverse(hold.inHand[hand]))};
}

glm::quat fromAngles(const float* angles, bool brush)
{
    return glm::normalize(glm::quat_cast(held::axesFromAngles(angles, brush)));
}

void toAngles(const glm::quat& q, float* out, bool brush)
{
    held::anglesFromAxes(glm::mat3_cast(q), out, brush);
}

// ----------------------------------------------------------------------------
// Server side

namespace
{

ankerl::unordered_dense::map<int, Hold> holds; // entity -> its hold

// Server side: where each hand was, from the player's origin, when it was last on its grip (detached).
struct Watch
{
    glm::vec3 onGrip[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    bool valid[2]{false, false};
    // Where the body was when the thing was last placed from the hands (serverPlace): the hands (moved with the body
    // before the carry's frame) are compared with it moved as far, so the body's own motion (walking, running, sliding
    // out of a teleporter) never pulls a hand off it (round 21, "Hands: both work").
    glm::vec3 placedBody{0.f};
    bool placed{false};
    double blocked{-1e9}; // when it was last stopped by the level (clear): the hands pushing it in may go wallSlack further
};
ankerl::unordered_dense::map<int, Watch> watches;

// Where each hand held a thing (in the thing's own frame, its grip) when that hand let go of it held in both (keep): the
// hand gripping it again there takes it (reaches), however far its fist test is off the thing's drawn surface. A prop
// sits where it was fitted to the drawn fist, not to its test: a small one (the heads at Size 0.7, props v57) about
// 2.6 cm off it, past Two-Handed Grab Reach, so the hand that let go of it could not grip it again where it was.
struct LetGo
{
    glm::vec3 grip[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    bool valid[2]{false, false};
};
ankerl::unordered_dense::map<int, LetGo> letGo;

[[nodiscard]] bool brushModel(edict_t* ent)
{
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    return model && model->type == mod_brush;
}

// Whether point `at` is inside `ent`'s own box (a brush model's, else its entity's) where `object` has it, grown by
// `grow` units (with vr_carry_two_hands_solid: a hand pushing it against a wall passes into it, or slides along its side).
[[nodiscard]] bool inside(edict_t* ent, const Frame& object, const glm::vec3& at, float grow)
{
    if(!vr_carry_two_hands_solid.value)
    {
        return false;
    }
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const bool brush = model && model->type == mod_brush;
    const glm::vec3 lo = brush ? glm::vec3{model->mins[0], model->mins[1], model->mins[2]} : glm::vec3{ent->v.mins[0], ent->v.mins[1], ent->v.mins[2]};
    const glm::vec3 hi = brush ? glm::vec3{model->maxs[0], model->maxs[1], model->maxs[2]} : glm::vec3{ent->v.maxs[0], ent->v.maxs[1], ent->v.maxs[2]};
    const glm::vec3 local = glm::inverse(object.rot) * (at - object.pos);
    return glm::all(glm::greaterThanEqual(local, lo - glm::vec3{grow})) && glm::all(glm::lessThanEqual(local, hi + glm::vec3{grow}));
}

} // namespace

// Solve both grips in the object's room even when the hands straddle the aperture.
static Frame foldedHand(edict_t* player, int hand)
{
    using namespace progs;
    if(const auto* move = server::clientMove(player))
    {
        const glm::vec3 root{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
        const auto gate = portals::reach(root, move->hands[1].pos);
        glm::vec3 angles = move->hands[hand].rot;
        angles.y = anglemod(angles.y + gate.yaw);
        return {gate.turn * (move->hands[hand].pos - gate.from) + gate.to, fromAngles(&angles[0], true)};
    }
    const auto angles = fieldVec(player, progs::handFields(hand).rot);
    return {fieldVec(player, progs::handFields(hand).pos), fromAngles(&angles[0], true)};
}

// The grip fit (vr_gripfit.hpp) of the hand that has just taken hold of what the other holds (the other's was fitted as
// it took it, or is its grip's own place): its grip on it moved out of it if the hand sinks in too deep (the drawn hand is
// held on its grip), the line between the grips with it.
static void fitGrip(edict_t* ent, edict_t* player, Hold& hold, const Frame& object, const Frame hands[2])
{
    const int num = NUM_FOR_EDICT(ent);
    const int first = grip::serverHand(num);
    const bool local = cls.state != ca_dedicated && NUM_FOR_EDICT(player) == 1;
    const glm::vec3 lo{ent->v.mins[0], ent->v.mins[1], ent->v.mins[2]};
    const glm::vec3 hi{ent->v.maxs[0], ent->v.maxs[1], ent->v.maxs[2]};
    const glm::quat toObject = glm::inverse(object.rot);
    bool moved = false;
    for(int i = 0; i < 2; i++)
    {
        if(i == first)
        {
            continue;
        }
        glm::vec3 move{0.f};
        if(gripfit::fit(num, i, local, true, glm::mat3_cast(toObject * hands[i].rot), toObject * (hands[i].pos - object.pos), lo, hi, nullptr,
               move))
        {
            hold.grip[i] += move;
            moved = true;
        }
    }
    if(moved)
    {
        const glm::vec3 d = hold.grip[1] - hold.grip[0];
        hold.span = glm::length(d);
        hold.axis = hold.span > 1e-4f ? d / hold.span : glm::vec3{1.f, 0.f, 0.f};
    }
}

glm::vec3 serverPlace(edict_t* ent, edict_t* player, bool grab)
{
    QVR_PROFILE("carry2h");
    using namespace progs;
    const FieldOffsets& f = fields();
    const bool brush = brushModel(ent);
    const glm::vec3 origin{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    if(f.handpos < 0 || f.handrot < 0 || f.offhandpos < 0 || f.offhandrot < 0)
    {
        return origin;
    }
    Frame hands[2];
    for(int h = 0; h < 2; h++)
    {
        hands[h] = foldedHand(player, h);
    }

    const int num = NUM_FOR_EDICT(ent);
    const auto it = holds.find(num);
    if(grab || it == holds.end())
    {
        // The hand taking hold of what the other holds alone, drawn held out of it at its surface
        // (vr_hand_collide_props_keep_out; a listen server's own player): it takes it there, not where the real hand is
        // in it, and holds it from there (Hold::reach).
        glm::vec3 reach[2]{glm::vec3{0.f}, glm::vec3{0.f}};
        if(grab && cls.state != ca_dedicated && NUM_FOR_EDICT(player) == 1)
        {
            const int first = grip::serverHand(num);
            for(int h = 0; h < 2; h++)
            {
                const glm::vec3 out = h != first ? view::heldOutOf(h, num) : glm::vec3{0.f};
                if(out == glm::vec3{0.f})
                {
                    continue;
                }
                if(vr_debug_carry.value || developer.value)
                {
                    Con_Printf("carry2h: %s hand takes it held out of it at its surface, %.1f cm off the controller\n",
                        h ? "main" : "off", static_cast<double>(glm::length(out) / units::metresToUnits() * 100.f));
                }
                hands[h].pos += out;
                reach[h] = glm::inverse(hands[h].rot) * out;
            }
        }
        Hold& hold = holds[num];
        hold = record({origin, fromAngles(ent->v.angles, brush)}, hands);
        if(grab)
        {
            fitGrip(ent, player, hold, {origin, fromAngles(ent->v.angles, brush)}, hands);
        }
        hold.reach[0] = reach[0];
        hold.reach[1] = reach[1];
        watches.erase(num);
        letGo.erase(num); // (in both hands again)
        return origin;
    }
    Frame object = solve(it->second, hands);
    const bool blocked = clear(num, {origin, fromAngles(ent->v.angles, brush)}, object);
    toAngles(object.rot, ent->v.angles, brush);
    Watch& w = watches[num];
    if(blocked)
    {
        w.blocked = qcvm->time;
    }
    w.placedBody = glm::vec3{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    w.placed = true;
    return object.pos;
}

bool clear(int num, const Frame& from, Frame& to)
{
    // Pushed as hard as the hands like, it stops at the wall (and slides along it), turned as it was if its new turn
    // would put it in (the author's note, vrfiringrange 2026-09-30 02:49: held in both hands, it went through walls).
    if(vr_carry_two_hands_solid.value)
    {
        return box3d::holdClear(num, from.pos, from.rot, to.pos, to.rot);
    }
    return false;
}


namespace
{
// Separation of the turned prop's drawn box from the torso box (the physics body's width, in world units).
// All 15 separating axes are tested: a sideways or tilted crate cannot hide a corner in the body.
[[nodiscard]] glm::vec3 separation(const glm::vec3& lo, const glm::vec3& hi, const Frame& object,
    const glm::vec3& bodyLo, const glm::vec3& bodyHi)
{
    const glm::mat3 axes = glm::mat3_cast(object.rot);
    const glm::vec3 half = (hi - lo) * 0.5f, bodyHalf = (bodyHi - bodyLo) * 0.5f;
    const glm::vec3 d = object.pos + axes * ((lo + hi) * 0.5f) - (bodyLo + bodyHi) * 0.5f;
    float least = 1e30f;
    glm::vec3 push{0.f};
    const auto test = [&](glm::vec3 axis) {
        const float length = glm::length(axis);
        if(length < 1e-4f) { return true; }
        axis /= length;
        const float reach = glm::dot(glm::abs(axis), bodyHalf) +
            za::abs(glm::dot(axis, axes[0])) * half.x + za::abs(glm::dot(axis, axes[1])) * half.y +
            za::abs(glm::dot(axis, axes[2])) * half.z;
        const float along = glm::dot(d, axis), depth = reach - za::abs(along);
        if(depth <= 0.f) { return false; }
        if(depth < least) { least = depth; push = axis * (along < 0.f ? -depth : depth); }
        return true;
    };
    for(int i = 0; i < 3; i++)
    {
        const glm::vec3 axis = glm::vec3{glm::mat3{1.f}[i]};
        if(!test(axis) || !test(axes[i])) { return glm::vec3{0.f}; }
        for(int j = 0; j < 3; j++)
        {
            if(!test(glm::cross(axis, axes[j]))) { return glm::vec3{0.f}; }
        }
    }
    return push;
}

bool constrain(const glm::vec3& lo, const glm::vec3& hi, const Frame& from, Frame& to,
    const glm::vec3& bodyLo, const glm::vec3& bodyHi)
{
    Frame safe{from.pos, to.rot};
    glm::vec3 push = separation(lo, hi, safe, bodyLo, bodyHi);
    bool blocked = glm::length(push) > 1e-4f;
    if(blocked)
    {
        // The player may have walked into the previous pose, or the hand rotated a corner into him.
        // Free it on its present side first; the subsequent sweep cannot pull it across his torso.
        safe.pos += push + glm::normalize(push) * 0.05f;
    }
    const glm::mat3 axes = glm::mat3_cast(to.rot);
    const glm::vec3 half = (hi - lo) * 0.5f, bodyHalf = (bodyHi - bodyLo) * 0.5f;
    const glm::vec3 delta = to.pos - safe.pos;
    const glm::vec3 d = safe.pos + axes * ((lo + hi) * 0.5f) - (bodyLo + bodyHi) * 0.5f;
    float enter = 0.f, leave = 1.f;
    // Continuous SAT translation sweep: intersect the time intervals of all separating axes. This catches an
    // arbitrarily fast hand move right across the torso even when both endpoints are clear.
    const auto interval = [&](glm::vec3 axis) {
        if(glm::length(axis) < 1e-4f) { return true; }
        axis = glm::normalize(axis);
        const float reach = glm::dot(glm::abs(axis), bodyHalf) +
            za::abs(glm::dot(axis, axes[0])) * half.x + za::abs(glm::dot(axis, axes[1])) * half.y +
            za::abs(glm::dot(axis, axes[2])) * half.z;
        const float along = glm::dot(d, axis), motion = glm::dot(delta, axis);
        if(za::abs(motion) < 1e-6f) { return za::abs(along) < reach; }
        const float a = (-reach - along) / motion, b = (reach - along) / motion;
        enter = za::max(enter, za::min(a, b));
        leave = za::min(leave, za::max(a, b));
        return enter < leave;
    };
    bool hit = true;
    for(int i = 0; i < 3 && hit; i++)
    {
        const glm::vec3 axis = glm::mat3{1.f}[i];
        hit = interval(axis) && interval(axes[i]);
        for(int j = 0; j < 3 && hit; j++) { hit = interval(glm::cross(axis, axes[j])); }
    }
    if(hit)
    {
        safe.pos += delta * za::max(0.f, enter - 1e-4f);
        blocked = true;
    }
    else { safe.pos = to.pos; }
    to.pos = safe.pos;
    return blocked;
}

[[nodiscard]] float loadScale(float mass, float size)
{
    const float byMass = vr_carry_move_mass.value > 0.f ? mass / vr_carry_move_mass.value : 0.f;
    const float bySize = vr_carry_move_size.value > 0.f ? za::max(0.f, size / vr_carry_move_size.value - 1.f) : 0.f;
    return za::max(za::clamp(vr_carry_move_min.value, 0.05f, 1.f), 1.f / (1.f + za::max(byMass, bySize)));
}

[[nodiscard]] bool solidCarry(edict_t* ent)
{
    return ent && !ent->free && progs::fieldFloatOr(ent, progs::fields().carry_rigid, 0.f) == 2.f;
}
}

bool bodyClear(edict_t* ent, edict_t* player, const Frame& from, Frame& to)
{
    if(!vr_carry_body_collision.value || !solidCarry(ent) || !player ||
        progs::fieldFloatOr(ent, progs::fields().carry_portal, 0.f) > 0.f ||
        qcvm->time < progs::fieldFloatOr(player, progs::fields().carry_teleported, 0.f)) { return false; }
    glm::vec3 lo, hi;
    const int modelIndex = static_cast<int>(ent->v.modelindex);
    if(modelIndex <= 0 || modelIndex >= MAX_MODELS || !sv.models[modelIndex]) { return false; }
    const auto& f = progs::fields();
    held::modelBox(sv.models[modelIndex], progs::fieldVec(ent, f.model_scale), progs::fieldVec(ent, f.model_scale_origin),
        progs::fieldVec(ent, f.model_offset), lo, hi);
    const float radius = za::max(vr_box3d_player_radius.value, 1.f) * 0.01f * units::metresToUnits();
    const glm::vec3 origin{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    const bool blocked = constrain(lo, hi, from, to, origin + glm::vec3{-radius, -radius, player->v.mins[2]},
        origin + glm::vec3{radius, radius, player->v.maxs[2]});
    if(blocked)
    {
        // Separating from a torso after the player walked into the old pose must not push the crate into a wall.
        box3d::holdClear(NUM_FOR_EDICT(ent), from.pos, from.rot, to.pos, to.rot);
    }
    return blocked;
}

bool bodyFits(edict_t* ent, edict_t* player, const Frame& at)
{
    if(!vr_carry_body_collision.value || !solidCarry(ent) || !player ||
        progs::fieldFloatOr(ent, progs::fields().carry_portal, 0.f) > 0.f ||
        qcvm->time < progs::fieldFloatOr(player, progs::fields().carry_teleported, 0.f)) { return true; }
    const int index = static_cast<int>(ent->v.modelindex);
    if(index <= 0 || index >= MAX_MODELS || !sv.models[index]) { return true; }
    const auto& f = progs::fields();
    glm::vec3 lo, hi;
    held::modelBox(sv.models[index], progs::fieldVec(ent, f.model_scale), progs::fieldVec(ent, f.model_scale_origin),
        progs::fieldVec(ent, f.model_offset), lo, hi);
    const float radius = za::max(vr_box3d_player_radius.value, 1.f) * 0.01f * units::metresToUnits();
    const glm::vec3 origin{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    return glm::length(separation(lo, hi, at, origin + glm::vec3{-radius, -radius, player->v.mins[2]},
        origin + glm::vec3{radius, radius, player->v.maxs[2]})) < 1e-3f;
}

glm::vec3 bodyPlace(edict_t* ent, edict_t* player, const glm::vec3& target)
{
    const glm::quat rot = fromAngles(ent->v.angles, brushModel(ent));
    Frame to{target, rot};
    const Frame from{glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]}, rot};
    bodyClear(ent, player, from, to);
    return to.pos;
}

float moveScale(edict_t* player)
{
    if(!player || static_cast<int>(player->v.movetype) != MOVETYPE_WALK || player->v.health <= 0.f) { return 1.f; }
    const auto& f = progs::fields();
    const int nums[2]{f.offhand_held >= 0 ? progs::fieldInt(player, f.offhand_held) : 0, f.mainhand_held >= 0 ? progs::fieldInt(player, f.mainhand_held) : 0};
    float mass = 0.f, size = 0.f;
    for(int h = 0; h < 2; h++)
    {
        if(nums[h] <= 0 || (h && nums[h] == nums[0])) { continue; }
        edict_t* ent = PROG_TO_EDICT(nums[h]);
        if(!solidCarry(ent)) { continue; }
        mass += box3d::propMass(ent);
        glm::vec3 lo, hi;
        const int index = static_cast<int>(ent->v.modelindex);
        if(index > 0 && index < MAX_MODELS && sv.models[index])
        {
            held::modelBox(sv.models[index], progs::fieldVec(ent, f.model_scale), progs::fieldVec(ent, f.model_scale_origin),
                progs::fieldVec(ent, f.model_offset), lo, hi);
            const glm::vec3 extent = hi - lo;
            size += za::max(extent.x, za::max(extent.y, extent.z)) / units::metresToUnits();
        }
    }
    return loadScale(mass, size);
}

void loadTest_f()
{
    const glm::vec3 lo{-20.f}, hi{20.f}, bodyLo{-5.f, -5.f, -24.f}, bodyHi{5.f, 5.f, 32.f};
    Frame front{glm::vec3{0.f, 50.f, 24.f}}, across{glm::vec3{0.f, -50.f, 24.f}};
    const bool stopped = constrain(lo, hi, front, across, bodyLo, bodyHi);
    Frame overlapped{glm::vec3{0.f, 15.f, 24.f}}, freed = overlapped;
    constrain(lo, hi, overlapped, freed, bodyLo, bodyHi);
    Frame overhead{glm::vec3{0.f, -50.f, 70.f}}, above{glm::vec3{0.f, 50.f, 70.f}};
    const bool blockedAbove = constrain(lo, hi, overhead, above, bodyLo, bodyHi);
    Frame rotated{glm::vec3{50.f, 50.f, 24.f}, glm::angleAxis(glm::radians(45.f), glm::vec3{0.f, 0.f, 1.f})};
    Frame diagonal{glm::vec3{-5000.f, -5000.f, 24.f}, rotated.rot};
    const bool stoppedDiagonal = constrain(lo, hi, rotated, diagonal, bodyLo, bodyHi);
    const bool ok = stopped && across.pos.y >= 24.99f && freed.pos.y >= 24.99f && !blockedAbove &&
        stoppedDiagonal && diagonal.pos.x > 0.f && diagonal.pos.y > 0.f &&
        glm::length(separation(lo, hi, diagonal, bodyLo, bodyHi)) < 1e-4f;
    Con_Printf("carryloadtest: torso %s cross %.3f overlap %.3f overhead %s; scales empty %.3f 5kg %.3f 65kg/1.5m %.3f 160kg %.3f bulk2m %.3f\n",
        ok ? "PASS" : "FAIL", across.pos.y, freed.pos.y, blockedAbove ? "blocked" : "clear",
        loadScale(0.f, 0.f), loadScale(5.f, 0.5f), loadScale(65.f, 1.5f), loadScale(160.f, 1.f), loadScale(5.f, 2.f));
    if(sv.active && svs.clients[0].active)
    {
        qcvm_t* oldvm = nullptr;
        PR_PushQCVM(&sv.qcvm, &oldvm);
        edict_t* player = svs.clients[0].edict;
        Con_Printf("carryloadtest: current speed scale %.3f, horizontal speed %.3f\n", moveScale(player),
            glm::length(glm::vec2{player->v.velocity[0], player->v.velocity[1]}));
        const auto& f = progs::fields();
        int count = 0;
        float physical = 0.f, drawn = 0.f;
        const int refs[2]{f.offhand_held >= 0 ? progs::fieldInt(player, f.offhand_held) : 0,
            f.mainhand_held >= 0 ? progs::fieldInt(player, f.mainhand_held) : 0};
        const float radius = za::max(vr_box3d_player_radius.value, 1.f) * 0.01f * units::metresToUnits();
        const glm::vec3 origin{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
        const glm::vec3 bodyLo = origin + glm::vec3{-radius, -radius, player->v.mins[2]};
        const glm::vec3 bodyHi = origin + glm::vec3{radius, radius, player->v.maxs[2]};
        for(int h = 0; h < 2; h++)
        {
            if(refs[h] <= 0 || (h && refs[h] == refs[0])) { continue; }
            edict_t* ent = PROG_TO_EDICT(refs[h]);
            if(!solidCarry(ent)) { continue; }
            count++;
            glm::vec3 boxLo, boxHi;
            const int index = static_cast<int>(ent->v.modelindex), num = NUM_FOR_EDICT(ent);
            if(index <= 0 || index >= MAX_MODELS || !sv.models[index]) { continue; }
            held::modelBox(sv.models[index], progs::fieldVec(ent, f.model_scale), progs::fieldVec(ent, f.model_scale_origin),
                progs::fieldVec(ent, f.model_offset), boxLo, boxHi);
            const Frame object{glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]},
                fromAngles(ent->v.angles, brushModel(ent))};
            physical = za::max(physical, glm::length(separation(boxLo, boxHi, object, bodyLo, bodyHi)));
            if(num < cl.num_entities && cl_entities[num].model == sv.models[index])
            {
                const entity_t& e = cl_entities[num];
                const Frame local{glm::vec3{e.origin[0], e.origin[1], e.origin[2]}, fromAngles(e.angles, brushModel(ent))};
                drawn = za::max(drawn, glm::length(separation(boxLo, boxHi, local, bodyLo, bodyHi)));
            }
        }
        float bothScale = moveScale(player);
        if(f.offhand_held >= 0 && refs[1])
        {
            progs::fieldInt(player, f.offhand_held) = refs[1];
            bothScale = moveScale(player);
            progs::fieldInt(player, f.offhand_held) = refs[0];
        }
        Con_Printf("carryloadtest: %d solid props, torso overlap physical %.4f drawn %.4f units, same prop in both hands scale %.3f\n",
            count, physical, drawn, bothScale);
        PR_PopQCVM(oldvm);
    }
}

bool reaches(edict_t* ent, edict_t* player, int hand)
{
    // What the other hand holds: a little farther than a thing lying about (vr_carry_two_hands_reach), so that the hand that
    // let go of it grips it again where it is (it was fitted to the fist's drawn shape, about 1 cm off the fist's test).
    const float reach = za::fmax(vr_carry_two_hands_reach.value, 0.f) * 0.01f * units::metresToUnits();
    if(held::grabTouch(ent, player, hand, reach))
    {
        return true;
    }
    // Or it is within that reach of where it held the thing when it let go of it (LetGo).
    const auto it = letGo.find(NUM_FOR_EDICT(ent));
    if(it == letGo.end() || hand < 0 || hand > 1 || !it->second.valid[hand])
    {
        return false;
    }
    const Frame object{glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]}, fromAngles(ent->v.angles, brushModel(ent))};
    const float off = glm::distance(foldedHand(player, hand).pos, object.pos + object.rot * it->second.grip[hand]);
    if(vr_debug_carry.value >= 2.f)
    {
        Con_Printf("carry2h: %s hand %.2f cm from where it let go of it (reach %.2f cm)\n", hand == 0 ? "off" : "main",
            off / units::metresToUnits() * 100.f, reach / units::metresToUnits() * 100.f);
    }
    return off <= reach;
}

int detached(edict_t* ent, edict_t* player)
{
    using namespace progs;
    const FieldOffsets& f = fields();
    const int num = NUM_FOR_EDICT(ent);
    const auto it = holds.find(num);
    if(it == holds.end() || f.handpos < 0 || f.handrot < 0 || f.offhandpos < 0 || f.offhandrot < 0)
    {
        return 0;
    }
    const float m2u = units::metresToUnits();
    const float drift = za::fmax(vr_carry_two_hands_drift.value, 0.f) * 0.01f * m2u;
    const float most = drift + za::fmax(vr_carry_two_hands_detach.value, 0.f) * 0.01f * m2u;
    const bool brush = brushModel(ent);
    const glm::vec3 body{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    Watch& w = watches[num];
    // Stopped by the level a moment ago (clear): the hands pushing it into a wall pass into it and the wall, as far again
    // as wallSlack, before they let go.
    const float limit = qcvm->time - w.blocked < wallTime ? most + wallSlack * m2u : most;
    // (Moved with the body since it was placed: see Watch.)
    const glm::vec3 carried = w.placed ? body - w.placedBody : glm::vec3{0.f};
    const Frame object{glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]} + carried, fromAngles(ent->v.angles, brush)};
    glm::vec3 hand[2];
    bool left[2];
    float moved[2];
    for(int h = 0; h < 2; h++)
    {
        hand[h] = reached(it->second, foldedHand(player, h), h).pos; // (where it holds it from)
        const float distance = glm::distance(hand[h], onGrip(it->second, object, h).pos);
        // Where the hand was (from the body: walking doesn't count) when it was last on its grip.
        if(distance <= drift || !w.valid[h])
        {
            w.onGrip[h] = hand[h] - body;
            w.valid[h] = true;
        }
        moved[h] = glm::distance(hand[h] - body, w.onGrip[h]);
        // (A hand in it, or no further off it than it may be off its grip, is pushing it into a wall that stops it (clear),
        // or along it: not pulled off.)
        left[h] = distance > limit && !held::grabTouch(ent, player, h, detachTouch * m2u) && !inside(ent, object, hand[h], limit);
        if(vr_debug_carry.value >= 2.f)
        {
            Con_Printf("carry2h: %s hand %.1f cm from its grip (most %.1f), moved %.1f cm since on it%s\n", h == 0 ? "off" : "main",
                distance / m2u * 100.f, most / m2u * 100.f, moved[h] / m2u * 100.f,
                left[h] ? ", pulled off" : distance > most ? ", still touching it" : "");
            if(left[h])
            {
                const glm::vec3 local = glm::inverse(object.rot) * (hand[h] - object.pos);
                Con_Printf("carry2h: that hand at %.1f %.1f %.1f in it\n", local.x, local.y, local.z);
            }
        }
    }
    // Both off it: pulled apart together, or one pulled away from the other held still (the prop, centred between them,
    // leaves both grips alike)? The one that moved much less keeps it.
    if(left[0] && left[1])
    {
        const float still = keepMoved * m2u;
        for(int h = 0; h < 2; h++)
        {
            if(moved[h] > 2.f * moved[1 - h] + still)
            {
                left[1 - h] = false;
            }
        }
    }
    const int off = (left[0] ? 1 : 0) | (left[1] ? 2 : 0);
    if(off == 1 || off == 2)
    {
        keep(ent, player, off == 1 ? 1 : 0);
    }
    else if(off)
    {
        holds.erase(num);
        watches.erase(num);
    }
    return off;
}

void keep(edict_t* ent, edict_t* player, int hand)
{
    using namespace progs;
    const FieldOffsets& f = fields();
    const int num = NUM_FOR_EDICT(ent);
    const auto it = holds.find(num);
    const int pos = hand == 0 ? f.offhandpos : f.handpos;
    if(it != holds.end() && pos >= 0 && (hand == 0 || hand == 1))
    {
        // The other hand let go of it: where it held it, for gripping it again there (LetGo, reaches).
        LetGo& g = letGo[num];
        g.grip[1 - hand] = it->second.grip[1 - hand];
        g.valid[1 - hand] = true;
        g.valid[hand] = false;
        // Moved onto its grip in that hand (else it would stay as far off it as the hand was off its grip: up to the
        // drift, and the drift plus the detach pulled off), turned as it is. The client takes it there (vr_held.cpp).
        const glm::vec3 to = fieldVec(player, pos) - fromAngles(ent->v.angles, brushModel(ent)) * it->second.grip[hand];
        for(int i = 0; i < 3; i++)
        {
            ent->v.origin[i] = to[i];
        }
        SV_LinkEdict(ent, false);
    }
    holds.erase(num);
    watches.erase(num);
}

bool serverHold(int num, Hold& out)
{
    const auto it = holds.find(num);
    if(it == holds.end())
    {
        return false;
    }
    out = it->second;
    return true;
}

void resetServer()
{
    holds.clear();
    watches.clear();
    letGo.clear();
}

void forgetEntity(int num)
{
    holds.erase(num);
    watches.erase(num);
    letGo.erase(num);
}

} // namespace qvr::carry2h

extern "C" float VR_CarryMoveScale(edict_t* player)
{
    return qvr::carry2h::moveScale(player);
}
