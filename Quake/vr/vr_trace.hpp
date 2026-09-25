// vr_trace.hpp -- client-side traces against the world, through the local server.
//
// Hands, weapons, the crosshair and the teleport aim collide with the level. Ironwail's
// client has no collision world of its own, so traces go through the local server (as the
// old engine did): they only work while hosting, which covers single player.

#pragma once

#include "vr_engine.hpp"

#include <optional>

namespace qvr::worldtrace
{

// A box swept from `start` to `end`, ignoring the local player; nothing when not hosting.
[[nodiscard]] std::optional<trace_t> move(
    const glm::vec3& start, const glm::vec3& mins, const glm::vec3& maxs, const glm::vec3& end, int type);

// A line through the world's geometry and, with `brushEntities`, the moving brush models the
// client has (lifts, doors, platforms; not rotated), from the client's own data: works without
// the local server. `line` is how far along the world alone it gets, 0..1.
[[nodiscard]] trace_t world(const glm::vec3& start, const glm::vec3& end, bool brushEntities = true);
[[nodiscard]] float line(const glm::vec3& start, const glm::vec3& end);

[[nodiscard]] inline glm::vec3 endPos(const trace_t& tr)
{
    return {tr.endpos[0], tr.endpos[1], tr.endpos[2]};
}

[[nodiscard]] inline glm::vec3 normal(const trace_t& tr)
{
    return {tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]};
}

} // namespace qvr::worldtrace
