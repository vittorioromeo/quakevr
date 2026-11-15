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
    float up{0.f};                   // clc_move's upmove (swimming: up or down)
    vec3_t steer{0.f, 0.f, 0.f};     // the angles steering it (the head's: VR_MoveAngles); walking: the yaw alone
    vec3_t view{0.f, 0.f, 0.f};      // the aim (Quake's wall friction)
    vec3_t roomscale{0.f, 0.f, 0.f}; // the room-scale walk, units a second (its x, y)
    bool jump{false};                // the jump key held (clc_move's bit 2)
};

// The server's rules a move runs by (the server's values, sent with each state: the client replays with them).
struct Rules
{
    float maxspeed{320.f};       // sv_maxspeed with stamina, carry, heavy weapon and crouch scales (once a server frame)
    float stickScale{1.f};       // the stick's scale carrying (VR_CarryMoveScale; the water's below, by each move's level)
    float accelerate{10.f}, friction{4.f}, stopspeed{100.f}, edgefriction{2.f};
    float gravity{800.f};        // sv_gravity times the player's .gravity
    float maxvelocity{2000.f};
    float stepsize{18.f};
    float roomscaleScale{1.f};   // 1 / VR_PlayerMoveSpeedup
    bool slopeWalk{true};        // vr_slope_walk
    bool nostep{false};          // sv_nostep
    bool roomscaleOn{true};      // the room-scale walk moves him (not held under a low ceiling: vr_lowceil)
    float jumpSpeed{0.f};        // QC PlayerJump's, run in each move (270); 0: QuakeC jumps (once a server frame)
    float boxHeight{0.f};        // his box's height over his feet crouched (hull::crouchedHeight; 0: standing)

    // The water (B8e), each move's level found from where he is (SV_CheckWater's three points, over his origin).
    float feetZ{-23.f}, waistZ{4.f}, eyeZ{22.f};
    float shallowStick{1.f};     // VR_WaterStickScale walking with feet in it (vr_swim_shallow_speed; 1: not a swimmer)
    float wadeStick{1.f};        // ... swimming waist deep on the bottom (vr_swim_wade_speed)
    float swimStick{1.f};        // ... swimming under or off the bottom (vr_swim_stick_speed)
    float swimMax{320.f};        // SV_WaterMove's top wanted speed (sv_maxspeed carrying)
    float glide{0.f};            // VR_AfterWaterMove's glide off the bottom (vr_swim_glide; 0: not a swimmer)
    float swimMost{-1.f};        // ... its speed at most (vr_swim_max_speed; -1: no cap, not a swimmer)
    bool waterDrag{false};       // QC WaterMove's drag runs in each move (its .vr_moveswim), not once a server frame
};

// The player as a move sees him. The hooks default to a client's world (BSP models only, nothing touched).
struct Body
{
    vec3_t origin{0.f, 0.f, 0.f}, velocity{0.f, 0.f, 0.f};
    int flags{0};                          // FL_ONGROUND, FL_WATERJUMP, FL_JUMPRELEASED
    vec3_t groundNormal{0.f, 0.f, 1.f};    // the walkable floor last met (vr_slope_walk) ...
    float groundAge{1e6f};                 // ... this long ago, seconds
    bool removed{false};                   // a touch removed him (the server's): the move ends
    float holdLeft{0.f};                   // .teleport_time less the time: moving back held, a water jump's end
    float jumpDir[2]{0.f, 0.f};            // a water jump's push over the edge (.movedir's x, y)
    int waterLevel{0}, waterType{CONTENTS_EMPTY}; // (each move's, found at its start)

    virtual ~Body() = default;
    // The map's contents at a point (SV_PointContents: the world's, a current as water).
    [[nodiscard]] virtual int contents(const float* p) = 0;
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
    // He jumped (PlayerJump's sound, its .button2 cleared: the server's).
    virtual void jumped() {}
};

// Walls and floors met during the last playerMove (a prediction's "flat ground": none but the floor, no step).
struct MoveNotes
{
    bool wall{false};    // a plane steeper than the floor's met
    bool stepped{false}; // a step up taken
    bool air{false};     // off the ground at some point
    bool slope{false};   // a floor not level met (normal under 0.999 up)
    bool swim{false};    // swimming (waterlevel 2 or more) or a water jump at some point
};

// One move of a walking or swimming player (SV_ClientThink's walk, swim or water jump, QC PlayerPreThink's water drag
// and jump, gravity, SV_WalkMove, the room-scale walk), for in.dt.
void playerMove(Body& b, const Input& in, const Rules& r, MoveNotes* notes = nullptr);

// SV_CheckWater's level (0-3) and type at the body's origin, as each move finds them (b.waterLevel, b.waterType).
void checkWater(Body& b, const Rules& r);

// The pieces (each as sv_user.c / sv_phys.c, the move's dt for host_frametime).
int clipVelocity(const float* in, const float* normal, float* out, float overbounce);
int flyMove(Body& b, float time, trace_t* steptrace, MoveNotes* notes);
void walkMove(Body& b, const Input& in, const Rules& r, float dt, MoveNotes* notes);

} // namespace qvr::pmove
