// vr_pmove.hpp -- a player's walk, one move at a time: the code a remote client's moves run with on the server and his
// own prediction replays them with (MP_PLAN.md, B8). One module both call, so the two agree to the bit where they see
// the same world.
//
// It is sv_user.c's SV_AirMove (the head steering, friction, acceleration) and sv_phys.c's gravity, SV_FlyMove and
// SV_WalkMove (steps, wall friction, the unstick) with the move's own duration in place of host_frametime, plus the
// room-scale walk (vr_physics.cpp VR_ClientRoomscaleMove) and the slope's gravity (vr_slope_walk). What only the server
// knows (touches, props stood on and shoved, the edicts) are a Body's hooks. Single player and a listen host's own
// client walk through it too (vr_pmove_own, default 1 since B11c; frame-identical to sv_phys.c's path in the traced
// walk, swim and lift); vr_pmove_own 0 is the fallback to sv_phys.c's path.

#pragma once

#include "vr_engine.hpp"
#include "vr_slide.hpp"

namespace qvr::pmove
{

// One move: what the client asked for over one of its frames.
struct Input
{
    double dt{0.0};                  // its duration, seconds (clamped by the caller); a double as host_frametime, so the
                                     // pieces of sv_user.c / sv_phys.c that multiply by it round as theirs (B11b part 2)
    float forward{0.f}, side{0.f};   // the stick (clc_move's units)
    float up{0.f};                   // clc_move's upmove (swimming: up or down)
    vec3_t steer{0.f, 0.f, 0.f};     // the angles steering it (the head's: VR_MoveAngles); walking: the yaw alone
    vec3_t view{0.f, 0.f, 0.f};      // the aim (Quake's wall friction)
    vec3_t roomscale{0.f, 0.f, 0.f}; // the room-scale walk, units a second (its x, y)
    bool jump{false};                // the jump key held (clc_move's bit 2)
    float hands[2][3]{};             // the hands relative to the body (hanging: the hang's pull, B8i)
    int grabs{3};                    // the grips held (bit 0 the off hand, 1 the main: a let-go ends its hold)
    float headZ{0.f};                // the head's height over the body (hanging: the mantle's test, B8j)
    // A hold's take (vr_climb.cpp, B8j part 2: climb::moveTake, climb::replayTake), relative to the body as the hands:
    float tracked[2][3]{};           // the controllers' own points
    float throwVel[2][3]{};          // the hands' throw velocities (the search's reach towards)
    float head[3]{};                 // the head
    int hotspots[2]{0, 0};           // the hands' hotspots (a holster's grip is not a hold)
    int buttons{0};                  // protocol::QVR_BUTTON_* (the hands tracked, the flashlight in a hand)
    float handVel[2][3]{};           // the hands' velocities, metres a second (VRGetEntHandVel: the water climb's pull)
    int reels{0};                    // the hands whose B/Y is held (bit 0 the off hand, 1 the main: the grapple's reel)
    bool slide{false};               // the slide input held (vr_slide: the move's protocol::moveSlideBit; B14)
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
    float maxsZ{32.f};           // his box's top over his origin (.maxs_z: WaterJumpTry's eye line)

    // The water (B8e), each move's level found from where he is (SV_CheckWater's three points, over his origin).
    float feetZ{-23.f}, waistZ{4.f}, eyeZ{22.f};
    float shallowStick{1.f};     // VR_WaterStickScale walking with feet in it (vr_swim_shallow_speed; 1: not a swimmer)
    float wadeStick{1.f};        // ... swimming waist deep on the bottom (vr_swim_wade_speed)
    float swimStick{1.f};        // ... swimming under or off the bottom (vr_swim_stick_speed)
    float swimMax{320.f};        // SV_WaterMove's top wanted speed (sv_maxspeed carrying)
    float glide{0.f};            // VR_AfterWaterMove's glide off the bottom (vr_swim_glide; 0: not a swimmer)
    float swimMost{-1.f};        // ... its speed at most (vr_swim_max_speed; -1: no cap, not a swimmer)
    bool waterDrag{false};       // QC WaterMove's drag runs in each move (its .vr_moveswim), not once a server frame
    bool waterJumpOut{false};    // QC CheckWaterJump's jump out (WaterJumpTry along his facing) runs in each move
                                 // (its .vr_movewaterjump: Quake's jump out, not the hands' climb out), not once a frame
    bool waterClimb{false};      // the hands' climb out of the water (waterClimbTry: vr_water_jump 0) runs in each move
                                 // (its .vr_movewaterclimb), not once a frame
    int climbHands{0};           // ... the hands that may climb (empty: bit 0 the off hand, 1 the main), the frame's
    float minsZ{-24.f};          // his box's bottom under his origin (.mins_z: the climb's rise over the top)
    bool crawl{false};           // lying down, crawling with his arms (VR_AfterCrawlMove: vr_prone_crawl, tracked hands)
    float crawlTop{0.f};         // ... the stick's crawl speed at most (sv_maxspeed, vr_prone_speed_scale, _stick_scale)
    float crawlFriction{3.f};    // ... its glide out between pulls (vr_prone_crawl_friction, per second)
    float crawlMost{120.f};      // ... its speed at most (vr_prone_crawl_max_speed)
    bool slide{false};           // a headset's slide runs in the moves (vr_slide.cpp: not flatscreen, not lying down; B14)
    slide::Tuning slideTuning;   // ... on the server's numbers
};

// VR_AfterCrawlMove's crawl (vr_physics_water.cpp; ROUND24.md, "Crawling with the arms"): how quickly, per second, the
// crawl's own speed reaches what a pull asks (a hand gripping the floor: a few frames).
inline constexpr float crawlGrip = 15.f;

// The crawl's own speed along the floor (`crawl`, x and y) after one step of `dt`: what the pulls ask (`want`, the
// stick's and the hands') reached quickly, never braking it along the pull; gliding out between pulls (`pulling` false)
// by `friction`; at most `most`. Then, on the ground, the player's `velocity` along it kept up to it (the floor's
// friction took some). False: not finite (the crawl stopped, the velocity left). One function for single player's
// frame and a remote client's moves.
bool crawlStep(float* crawl, const float* want, bool pulling, float friction, float most, float dt, bool onGround,
    float* velocity);
// The stick's share of the crawl's pull: where it points along the floor (`yaw`, degrees: the move's steering), at
// most `top`, added to `want` -- true if it pulls (on the ground, more than 1 unit asked).
bool crawlStickWant(float yaw, float forwardmove, float sidemove, float top, bool onGround, float* want);

// A mantle (vr_climb.cpp, B8h): the body carried from `from` straight up to `mid`, then over to `to`, at an even speed
// along the two, in `time` seconds; `at` the time it has run. One way for single player's frame (vr_climb's mantleStep)
// and a remote client's moves (the server's and his replay's).
struct Mantle
{
    bool on{false};
    float from[3]{0.f, 0.f, 0.f}, mid[3]{0.f, 0.f, 0.f}, to[3]{0.f, 0.f, 0.f};
    float at{0.f}, time{0.f};
    float headAbove{0.f}; // (the hang's test: the head's height over the top as it began, the climb's print)
};
// One step of `dt` along it: its place at its new time (`out`) -- true when it reached its end.
bool mantleStep(Mantle& m, float dt, float* out);

struct Body; // (below)

// The grappling hook's rope holding him (vr_grapple.qc, a hook in the world: GH_ANCHOR; B8j part 4), as a step sees it.
// The corner and the rope's path beyond it are the frame's (QC VR_Grapple_RopeShape); the length, the reel's ramp and
// the taut are each step's.
struct Grapple
{
    bool on{false};
    int hand{-1};          // the gun's hand (0 the off hand, 1 the main; -1 a holster): the taut's feel
    int drivers{0};        // the hands whose B/Y reels it (bit 0 the off hand, 1 the main: QC VR_Grapple_ReelHeld)
    float at[3]{0.f, 0.f, 0.f}; // the rope's first corner from his body (the hook, nothing in the way: gr_pa)
    float beyond{0.f};     // the rope's path beyond that corner, to the hook (gr_ba)
    float len{0.f};        // the rope's length (.gh_len)
    float speed{0.f};      // the reel's full speed (.gh_speed times .gh_stamina_mult)
    float reelAge{-1.f};   // reeling this long (seconds of steps: QC's time less .gh_reel; -1 not reeling)
    float shortest{0.f};   // the rope's shortest (vr_grapple_min_length)
    float payout{0.f};     // the unreel's speed (.gh_payout: the server's, once a frame)
    bool taut{false};      // the rope was taut at the last step (.gh_taut)
};
// QC's GH_RAMP (seconds the reel takes to its speed, from a fifth), GH_STALL (units the rope may be shorter than he is
// far), GH_BRAKE (units/s/s he slows at, arriving).
inline constexpr float grappleRamp = 0.15f, grappleStall = 8.f, grappleBrake = 2500.f;
// The reel's one rule (BACKLOG 9 item 4: the player's grappleStep and QC's builtin grapplereel for props and monsters
// call it): `len` (the rope) shortened over `dt` at the reel's speed, `full` ramped from a fifth over grappleRamp by
// `age` (seconds reeling), taking any slack in at once, never shorter than `shortest` nor grappleStall shorter than the
// load is far (`dist`, the rope's whole way) -> that speed braked to stop at `shortest` (grappleBrake): how fast the
// load should come.
float grappleReel(float& len, float full, float age, float dist, float shortest, float dt);
// One step of `dt` (QC VR_Grapple_ServiceAnchor's Wind and HoldPlayer): reeling, the rope shortened at the reel's
// ramped speed (never shorter than g.shortest, nor grappleStall shorter than he is far), braking as he arrives; then he
// is held: never farther from the corner than the rope less its path beyond (put back on it, swept: a wall stops that,
// the rest slid along it), no speed away from it, reeling no slower towards it than the reel (nor much faster); lifted
// off the floor by a pull up. The rope's going taut tells Body::grappleTaut. Single player's frame (QC's builtin
// grapplehold, once) and a remote client's moves (each, on the server and in his replay): one function.
void grappleStep(Body& b, Grapple& g, bool reeling, float dt);

// Hanging from holds (vr_climb.cpp, B8i): a hold, as a step sees it.
struct HangGrip
{
    bool active{false};
    bool light{false};               // on a light prop: pulled towards it, it does not carry the body
    float hold[3]{0.f, 0.f, 0.f};    // where the hand holds now (world)
    float lastRel[3]{0.f, 0.f, 0.f}; // the hand relative to the body at the last step
    float top{0.f};                  // the ledge's top at the hold now
    // The mantle's test (B8j): the ledge, the pull down, the last search that found no room.
    float out[2]{0.f, 0.f};          // the ledge's way out from the face (level)
    float relAtGrabZ{0.f};           // the hand's height relative to the body when it took hold
    float depth[5]{-1.f, -1.f, -1.f, -1.f, -1.f}; // the top's depth from the lip in at mantleSides along it (-1: not known)
    bool missed{false};              // the last search found no room ...
    float missAge{0.f};              // ... this long ago (seconds of steps) ...
    float missOrigin[3]{0.f, 0.f, 0.f}, missHold[3]{0.f, 0.f, 0.f}; // ... the body and the hold there ...
    float missTop{0.f};              // ... and the top
};
// The hang's state a step runs on: the holds, what the body owes (earlier pulls into the face it couldn't make: made
// up first by a pull the other way), slipping (no stamina: it sinks).
struct Hang
{
    bool on{false};
    bool slipping{false};
    HangGrip grips[2];
    float owed[3]{0.f, 0.f, 0.f};
    float minsZ{-24.f}; // the box's bottom below the origin (the feet's limit: no higher than the highest hold)
    // The mantle's test after each step (B8j; the server's rules): 0 mantleTime, none.
    float maxsXY[2]{16.f, 16.f}; // the box's sides (the footing on the top)
    float mantleTime{0.f};       // the mantle's duration
    float mantleHeight{0.f};     // the head this far over the top at least (vr_climb_mantle_height, 0-40)
    float mantleSlope{0.f};      // a sloping top up to this steep (vr_climb_mantle_slope, 0-60 degrees; 0: level only)
    bool verbose{false};         // the search's prints (vr_climb_debug)
    bool noRoom{false};          // (out) the last test pulled for a mantle and found no room
    int mantleHand{-1};          // (out) the hand whose hold the mantle began from
};
// The mantle's test (vr_climb.cpp, B8j): a hand that pulled down this far from where it took hold ...
inline constexpr float mantlePull = 8.f;
// ... a search that found no room holds this long (the body and the hold still) ...
inline constexpr float mantleRetry = 0.15f;
// ... unless the body moved this far; the spots along the edge it tries (the depths a HangGrip carries), and the top
// the box must stand on (from the lip in), units.
inline constexpr float mantleRetryMove = 1.f;
inline constexpr float mantleSides[5] = {0.f, 8.f, -8.f, 16.f, -16.f};
inline constexpr float minFooting = 3.f;
// A step's (or a frame's steps') sums, for the server's once-a-frame part (vr_climb.cpp: the reach, the owed settled,
// the trace): the origin before the first step, the moves meant and wanted, the hands' pull weights, the highest top.
struct HangSums
{
    int steps{0};
    float origin[3]{0.f, 0.f, 0.f};
    float meant[3]{0.f, 0.f, 0.f};  // what the body would have done unhindered (the carry and the pull, owed made up)
    float wanted[3]{0.f, 0.f, 0.f}; // the carry and the pull as asked
    float weights{0.f};
    float weight[2]{0.f, 0.f};
    float top{-1e9f};
    int count{0};
};
// The hang's constants (vr_climb.cpp's, the top of that file).
inline constexpr float hangMaxReach = 48.f;  // the body stays this close (horizontally) to each hold
inline constexpr float hangMaxSpeed = 500.f; // units / second the body follows the hands at
inline constexpr float hangSlipSink = 8.f;   // units a second an exhausted body sinks, slipping
// One step of `dt`: the pull (what the holding hands moved relative to the body since the last step, `rel` now, the
// other way, each weighted by how far it moved), `carry` (the holds' own motion since: a brush model's), the owed made
// up, the slip's sink; the limits (the feet no higher than the highest hold, the body within hangMaxReach of each hold,
// hangMaxSpeed); the body moved there (moveBody). Each grip's lastRel becomes `rel`. Single player's hanging frame (one
// step of the frame's time) and a remote client's moves (the server's and his replay's).
void hangStep(Body& b, Hang& h, const float (*rel)[3], const float* carry, float dt, HangSums* sums);
// The body's box moved towards `target`: straight, else round the corner (vertical first or horizontal first) or slid
// along what it meets, whichever ends nearest (vr_climb.cpp's moveBody).
void moveBody(Body& b, const float* target);
// The mantle's way from hold `k` (vr_climb.cpp's findMantle: a level top, else a sloping one): straight up from the
// body to `mid` over the top, over to `to` standing on it, with room -- false: none.
[[nodiscard]] bool findMantle(Body& b, const Hang& h, int k, float* mid, float* to);
// The mantle's test after a step (`rel` the hands relative to the body, `headZ` the head's height over it): a holding
// hand pulled down mantlePull from where it took hold, the head mantleHeight over its top, not slipping, and a way
// (findMantle; a search that found no room kept mantleRetry) -> the mantle begun from the body's place (`m`, its time 0;
// h.mantleHand its hand). h.noRoom: pulled for one, no room. Single player's hanging frame and a remote client's moves
// (the server's and his replay's).
bool hangMantle(Body& b, Hang& h, const float (*rel)[3], float headZ, float dt, Mantle& m);

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
    // The water's level and type: SV_CheckWater's, found by each move after its walk or swim chose (as a server frame's
    // runs after SV_ClientThink and PlayerPreThink, before gravity and SV_WalkMove): at a move's start the last move's,
    // which its walk, swim, QC WaterMove's drag and CheckWaterJump go by, as single player's (B11b part 2).
    int waterLevel{0}, waterType{CONTENTS_EMPTY};
    int thinkLevel{0};                     // the level the last move's walk or swim went by (the server's strokes)
    float crawl[2]{0.f, 0.f};              // the crawl's own speed along the floor (lying down: Rules::crawl)
    Mantle mantle;                         // mantling: the move carries him along it (B8h)
    bool mantledNow{false};                // the last move ended the mantle (he stands)
    Hang hang;                             // hanging: the move's hands run the hang's step (B8i)
    HangSums* hangSums{nullptr};           // ... adding to these (the server's frame part)
    Grapple grapple;                       // a grappling hook's rope holds him: each walking move's step (B8j part 4)
    slide::Slide slide;                    // the slide (Rules::slide: slide::move in each walking move; B14) ...
    double slideClock{0.0};                // ... its clock at the move (the server's time; a replay's own from 0)
    float staminaLeft{1e9f};               // ... the stamina he has (a slide too tired for its first part does not start)

    virtual ~Body() = default;
    // The map's contents at a point (SV_PointContents: the world's, a current as water).
    [[nodiscard]] virtual int contents(const float* p) = 0;
    // His box from start to end against the solid world (SV_Move's MOVE_NORMAL).
    [[nodiscard]] virtual trace_t trace(const float* start, const float* end) = 0;
    // Whether what the trace met is a BSP model (the world, a door) -- floors of others stand only through standsOn.
    [[nodiscard]] virtual bool metBsp(const trace_t& tr) = 0;
    // A line from start to end against the world and the brush models (traceline's MOVE_NOMONSTERS).
    [[nodiscard]] virtual trace_t line(const float* start, const float* end) = 0;
    // His box against the world and the brush models only (SV_Move's MOVE_NOMONSTERS: the mantle's floor test).
    [[nodiscard]] virtual trace_t traceWorld(const float* start, const float* end) { return trace(start, end); }
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
    // He jumped out of the water (WaterJumpTry: `movedir` its push over the edge; .movedir and .teleport_time, the
    // server's).
    virtual void waterJumped(const float* /*movedir*/) {}
    // ... by hand `hand` (waterClimbTry; 1 the main hand): the top under it `top` over his feet, rising at `rise` (the
    // feel and the print: the server's QuakeC).
    virtual void waterClimbed(int /*hand*/, float /*top*/, float /*rise*/) {}
    // The grapple's rope went taut (grappleStep), him going away from it at `speed` (QC VR_Grapple_Taut: the twang, the
    // jolt in hand `hand`: the server's QuakeC).
    virtual void grappleTaut(int /*hand*/, float /*speed*/) {}
    // A part of the slide's stamina due (slide::payParts): the share of it paid (the server's QuakeC pays it).
    virtual float slidePaid(float part)
    {
        staminaLeft -= part;
        return 1.f;
    }
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

// SV_CheckWater's level (0-3) and type at the body's origin, as each move finds them (b.waterLevel, b.waterType: before
// its gravity and walk).
void checkWater(Body& b, const Rules& r);

// Climbing out of the water by hand (vr_water_jump 0, climbing on; ROUND21.md): a hand of r.climbHands gripping
// (in.grabs) with ground right under it (a top that could be stood on), pulled down at waterClimbPull or faster
// (in.handVel), lifts him as Quake's jump out would, over the edge towards that hand, high enough to clear that top; the
// main hand tried first. True: lifted (FL_WATERJUMP, .movedir; the hooks waterJumped and waterClimbed told). Single
// player's frame (QC CheckWaterJump's builtin vr_waterclimb, once) and a remote client's moves (each, on the server and
// in his replay): one function.
inline constexpr float waterClimbPull = 0.3f; // metres a second (QC's VR_WATERCLIMB_PULL)
bool waterClimbTry(Body& b, const Input& in, const Rules& r);

// The pieces (each as sv_user.c / sv_phys.c, the move's dt for host_frametime).
int clipVelocity(const float* in, const float* normal, float* out, float overbounce);
int flyMove(Body& b, float time, trace_t* steptrace, MoveNotes* notes);
void walkMove(Body& b, const Input& in, const Rules& r, double dt, MoveNotes* notes);

} // namespace qvr::pmove
