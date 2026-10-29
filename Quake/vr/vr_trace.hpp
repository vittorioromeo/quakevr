// vr_trace.hpp -- client-side traces against the world, through the local server.
//
// Hands, weapons, the crosshair and the teleport aim collide with the level. Ironwail's
// client has no collision world of its own, so traces go through the local server (as the
// old engine did): they only work while hosting, which covers single player.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Vocabulary/Optional.hpp"


namespace qvr::worldtrace
{

// A box swept from `start` to `end`, ignoring the local player (or, `pass` above 0, that edict and what it owns
// instead); nothing when not hosting.
[[nodiscard]] za::Optional<trace_t> move(
    const glm::vec3& start, const glm::vec3& mins, const glm::vec3& maxs, const glm::vec3& end, int type, int pass = 0);

// A line through the world's geometry and, with `brushEntities`, the moving brush models the
// client has (lifts, doors, platforms; not rotated), from the client's own data: works without
// the local server; with `ownFiles` also those whose models are files of their own (a func_wall of
// maps/vr_proptable.bsp: the firing range's prop table), but never the entities `skipA` and
// `skipB` (the props the hands hold: a held explosive box is a model of its own file, and a line
// from the hand to its corners met the box itself). Never the things lying round (rigid bodies: the
// health, ammo and explosive boxes) nor a model drawn scaled or offset: their hulls are their models'
// own, unscaled and unturned, not what is drawn. `line` is how far along the world alone it
// gets, 0..1. `hitEntity`: the client entity the line stopped at (0: the world, or nothing).
[[nodiscard]] trace_t world(const glm::vec3& start, const glm::vec3& end, bool brushEntities = true, bool ownFiles = false,
    int skipA = 0, int skipB = 0, int* hitEntity = nullptr);
[[nodiscard]] float line(const glm::vec3& start, const glm::vec3& end);

// A line through the local server's world (SV_Move with a point box, MOVE_NOMONSTERS by default) passing the current
// VM's world entity, or `pass` exactly (null passes nothing). The caller has the server's VM current and is hosting.
[[nodiscard]] inline trace_t serverLine(const glm::vec3& a, const glm::vec3& b, edict_t* pass, int type = MOVE_NOMONSTERS)
{
    vec3_t s{a.x, a.y, a.z}, e{b.x, b.y, b.z}, zero{0.f, 0.f, 0.f};
    return SV_Move(s, zero, zero, e, type, pass);
}

[[nodiscard]] inline trace_t serverLine(const glm::vec3& a, const glm::vec3& b)
{
    return serverLine(a, b, qcvm->edicts);
}

// Point contents. `contents`: the local server's (SV_PointContents: a current reads as water; the caller is hosting).
// `clientContents`: the client's map's leaf at p (currents and sky as they are; CONTENTS_EMPTY without a map).
// `liquidContents`: either one's (`client`) with the liquid brush entities' too (VR_LiquidContents: a func_water's).
// Each site keeps its own rule of what counts as liquid (ROUND notes D3): isLiquid is water, slime or lava, and with
// `currents` the CONTENTS_CURRENT_* too.
[[nodiscard]] inline int contents(const glm::vec3& p)
{
    vec3_t v{p.x, p.y, p.z};
    return SV_PointContents(v);
}

[[nodiscard]] inline int clientContents(const glm::vec3& p)
{
    if(!cl.worldmodel)
    {
        return CONTENTS_EMPTY;
    }
    vec3_t v{p.x, p.y, p.z};
    return Mod_PointInLeaf(v, cl.worldmodel)->contents;
}

[[nodiscard]] inline int liquidContents(const glm::vec3& p, bool client)
{
    vec3_t v{p.x, p.y, p.z};
    if(client)
    {
        return cl.worldmodel ? VR_LiquidContents(cl.worldmodel, v, Mod_PointInLeaf(v, cl.worldmodel)->contents)
                             : CONTENTS_EMPTY;
    }
    return VR_LiquidContents(sv.worldmodel, v, SV_PointContents(v));
}

[[nodiscard]] constexpr bool isLiquid(int c, bool currents = false)
{
    return (c <= CONTENTS_WATER && c >= CONTENTS_LAVA) || (currents && c <= CONTENTS_CURRENT_0 && c >= CONTENTS_CURRENT_DOWN);
}

// Whether the player's box (Quake's hull 1) moves from `start` to `end` through the world's
// geometry unblocked, from the client's own data.
[[nodiscard]] bool playerBoxFits(const glm::vec3& start, const glm::vec3& end);

[[nodiscard]] inline glm::vec3 endPos(const trace_t& tr)
{
    return {tr.endpos[0], tr.endpos[1], tr.endpos[2]};
}

[[nodiscard]] inline glm::vec3 normal(const trace_t& tr)
{
    return {tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]};
}

} // namespace qvr::worldtrace
