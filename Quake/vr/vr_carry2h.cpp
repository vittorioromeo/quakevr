// vr_carry2h.cpp -- see vr_carry2h.hpp.

#include "vr_carry2h.hpp"
#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_physics.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_units.hpp"

#include <cmath>
#include <unordered_map>

namespace qvr::carry2h
{
namespace
{

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
            axis = std::fabs(from.x) < 0.9f ? glm::cross(from, glm::vec3{1.f, 0.f, 0.f}) : glm::cross(from, glm::vec3{0.f, 1.f, 0.f});
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

Frame solve(const Hold& hold, const Frame hands[2])
{
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
        const glm::vec3 across = rot * (std::fabs(hold.axis.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f});
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

std::unordered_map<int, Hold> holds; // entity -> its hold

[[nodiscard]] bool brushModel(edict_t* ent)
{
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    return model && model->type == mod_brush;
}

} // namespace

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
    const int pos[2] = {f.offhandpos, f.handpos};
    const int rot[2] = {f.offhandrot, f.handrot};
    for(int h = 0; h < 2; h++)
    {
        const glm::vec3 angles = fieldVec(player, rot[h]);
        hands[h] = {fieldVec(player, pos[h]), fromAngles(&angles[0], true)};
    }

    const int num = NUM_FOR_EDICT(ent);
    const auto it = holds.find(num);
    if(grab || it == holds.end())
    {
        holds[num] = record({origin, fromAngles(ent->v.angles, brush)}, hands);
        return origin;
    }
    const Frame object = solve(it->second, hands);
    toAngles(object.rot, ent->v.angles, brush);
    return object.pos;
}

bool reaches(edict_t* ent, const glm::vec3& point, int hand)
{
    const bool inBox = physics::pointInModelBox(ent, point, 2.f);
    const float reach = vr_carry_reach.value > 0.f ? vr_carry_reach.value * 0.01f * units::metresToUnits() : 0.f;
    glm::vec3 nearest{0.f};
    const float distance = inBox && (reach > 0.f || vr_debug_carry.value || vr_debug_physics_shapes.value) ? held::surfaceDistance(ent, point, &nearest) : -1.f;
    if(inBox)
    {
        held::noteCarryProbe(hand, ent, point, distance, nearest, reach);
    }
    if(vr_debug_carry.value >= 2.f)
    {
        Con_Printf("carry2h: hand %d at %.1f %.1f %.1f, %s its box, %.1f units from its surface (reach %.1f)\n", hand, point.x,
            point.y, point.z, inBox ? "in" : "not in", distance, reach);
    }
    return inBox && (reach <= 0.f || distance < 0.f || distance <= reach);
}

void resetServer()
{
    holds.clear();
}

} // namespace qvr::carry2h
