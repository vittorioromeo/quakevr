// vr_pmove.hpp -- a player's walk, one move at a time: the code a remote client's moves run with on the server and his
// own prediction replays them with (MP_PLAN.md, B8). One module both call, so the two agree to the bit where they see
// the same world.
//
// It is sv_user.c's SV_AirMove (the head steering, friction, acceleration) and sv_phys.c's gravity, SV_FlyMove and
// SV_WalkMove (steps, wall friction, the unstick) with the move's own duration in place of host_frametime, plus the
// room-scale walk (vr_physics.cpp VR_ClientRoomscaleMove) and the slope's gravity (vr_slope_walk). What only the server
// knows (touches, props stood on and shoved, the edicts) are a Body's hooks. Single player and a listen host's own
// client keep sv_phys.c's path: nothing of theirs runs here.

#pragma once

#include "vr_engine.hpp"

namespace qvr::pmove
{

// One move: what the client asked for over one of its frames.
struct Input
{
    float dt{0.f};                   // its duration, seconds (clamped by the caller)
    float forward{0.f}, side{0.f};   // the stick (clc_move's units)
    vec3_t steer{0.f, 0.f, 0.f};     // the angles steering it (the head's: VR_MoveAngles); walking: the yaw alone
    vec3_t view{0.f, 0.f, 0.f};      // the aim (Quake's wall friction)
    vec3_t roomscale{0.f, 0.f, 0.f}; // the room-scale walk, units a second (its x, y)
};

// The server's rules a move runs by (the server's values, sent with each state: the client replays with them).
struct Rules
{
    float maxspeed{320.f};       // sv_maxspeed with stamina, carry, heavy weapon and crouch scales (once a server frame)
    float stickScale{1.f};       // the stick's scale wading or carrying (VR_WaterStickScale, VR_CarryMoveScale)
    float accelerate{10.f}, friction{4.f}, stopspeed{100.f}, edgefriction{2.f};
    float gravity{800.f};        // sv_gravity times the player's .gravity
    float maxvelocity{2000.f};
    float stepsize{18.f};
    float roomscaleScale{1.f};   // 1 / VR_PlayerMoveSpeedup
    bool slopeWalk{true};        // vr_slope_walk
    bool nostep{false};          // sv_nostep
    bool gravityOn{true};        // out of water and no water jump (the server frame's SV_CheckWater)
    bool wading{false};          // waterlevel 1 (stairs up off the ground too)
    bool roomscaleOn{true};      // the room-scale walk moves him (not held under a low ceiling: vr_lowceil)
};

// The player as a move sees him. The hooks default to a client's world (BSP models only, nothing touched).
struct Body
{
    vec3_t origin{0.f, 0.f, 0.f}, velocity{0.f, 0.f, 0.f};
    int flags{0};                          // FL_ONGROUND, FL_WATERJUMP
    vec3_t groundNormal{0.f, 0.f, 1.f};    // the walkable floor last met (vr_slope_walk) ...
    float groundAge{1e6f};                 // ... this long ago, seconds
    bool removed{false};                   // a touch removed him (the server's): the move ends

    virtual ~Body() = default;
    // His box from start to end against the solid world (SV_Move's MOVE_NORMAL).
    [[nodiscard]] virtual trace_t trace(const float* start, const float* end) = 0;
    // Whether what the trace met is a BSP model (the world, a door) -- floors of others stand only through standsOn.
    [[nodiscard]] virtual bool metBsp(const trace_t& tr) = 0;
    // SV_UserFriction's ledge test: the floor drops away ahead.
    [[nodiscard]] virtual bool overDropoff(const float* vel, float speed) = 0;
    // A walkable floor that is not a BSP model (a solid prop: VR_StandsOn).
    [[nodiscard]] virtual bool standsOn(const trace_t&) { return false; }
    // He met walkable floor (groundentity, the slope's record).
    virtual void floorMet(const trace_t&) {}
    // A move stopped by something's side (a prop shoved).
    virtual void bumped(const trace_t&) {}
    // He moved by a slide (not linked yet): touches of what the trace met.
    virtual void impact(const trace_t&) {}
    // He was pushed and linked (a step up or down, the unstick's nudge): triggers' and the met thing's touches.
    virtual void pushed(const trace_t&) {}
    // A step down landed on walkable floor (SV_WalkMove's, which never sets FL_ONGROUND for a player).
    virtual void steppedOnto(const trace_t&) {}
    // Walking straight at a live enemy near him, slower (vr_closequarters: the server's).
    virtual void closeQuarters(float*) {}
    // Moving back is held off for a moment after a teleporter (Quake's hack).
    [[nodiscard]] virtual bool backHeld() { return false; }
};

// Walls and floors met during the last playerMove (a prediction's "flat ground": none but the floor, no step).
struct MoveNotes
{
    bool wall{false};    // a plane steeper than the floor's met
    bool stepped{false}; // a step up taken
    bool air{false};     // off the ground at some point
    bool slope{false};   // a floor not level met (normal under 0.999 up)
};

// One move of a walking player (SV_ClientThink's walk, gravity, SV_WalkMove, the room-scale walk), for in.dt.
void playerMove(Body& b, const Input& in, const Rules& r, MoveNotes* notes = nullptr);

// The pieces (each as sv_user.c / sv_phys.c, the move's dt for host_frametime).
int clipVelocity(const float* in, const float* normal, float* out, float overbounce);
int flyMove(Body& b, float time, trace_t* steptrace, MoveNotes* notes);
void walkMove(Body& b, const Input& in, const Rules& r, float dt, MoveNotes* notes);

} // namespace qvr::pmove
