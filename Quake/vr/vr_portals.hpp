// vr_portals.hpp -- slipgate views, movement, shots and one-hop dynamic lighting.
//
// A Quake teleporter is two things the map does not tie together: the slipgate's faces (their texture *teleport, any
// shape) and an invisible trigger_teleport brush round them, whose target is an info_teleport_destination (a point and
// a yaw). As a map's world is ready, each trigger's teleport faces (the world's, within 24 units of its brush) are
// grouped by plane: a side of the gate, seen from in front of its plane. Each side maps onto its destination by a turn
// about the vertical and a shift: the gate's middle at a standing player's height (its foot plus 24 units, where the
// player's origin is) onto the destination's point, walking into the gate onto the destination's yaw; a level gate (in
// a floor or a ceiling) keeps its looks straight down (or up) and is seen from over the destination (or from its floor).
// Nothing in the map changes: the triggers and destinations are the game's (the server's entities), the faces the
// world's.
//
// Before each camera the visible sides are ranked (in front of the camera, in its PVS, near and big on screen).
// Up to vr_portals_maxviews sides are rendered, default four, maximum eight. Each uses a separate texture-array layer.
// The view through each is seen from the destination's leaf (its PVS: the view is behind the
// destination, often in a wall or another room). For each eye, before the eye's own view, the scene is drawn again into
// scene targets of the eye's size from that eye moved through the gate (vr_stereo.cpp): the same projection with an
// oblique near plane on the destination's side of the gate's plane (what is between the view and that plane is not
// drawn), and outside the gate's box on screen the depth set to the near plane first, so that nothing is shaded there.
// Each eye uses the same rigid turn and shift as movement and traces, at any distance: screen-space portal sampling
// therefore shows the target a shot through that pixel reaches. Drawing the eye, the side's faces (the liquid
// shaders, LiquidPortal) show that image by their pixels, whatever their shape, under a little of the slipgate's own
// shimmer. Within a view, gates seen in it show their own views (vr_portals_recursion, up to three more gates deep, each
// gate deeper drawn at fewer pixels); with it 0, teleport surfaces inside a destination view are omitted. The server sends what is round
// the destinations of the gates it can see (their PVS added to its own), so the monsters and items there are seen too.
// Each eye, the flat camera and the spectator camera prepare their own view with the same rigid transform.
// The single entrance-plane star layer has Size and Opacity controls; these never change the physical aperture.
// Dynamic-light rays are constrained to that aperture, with their folded
// travel distance and shadow checks on both sides. Destination torches use their own PVS and folded viewing distance.
//
// The whole feature is one switch: vr_slipgates (Graphics > Slipgates, default on). With it 0 nothing here runs - no gate
// is built, none is looked through, no view is drawn, nothing is carried or traced through one, and Quake's
// trigger_teleport moves the player exactly as it did before this file existed. vr_portals (the view) and
// vr_portals_walk (going through) are its two parts. Flipping it takes effect at once, no map reload.
//
// Walking and shooting through (vr_portals_walk; the server, single player and listen servers): a player is carried
// through when his torso's middle plane - the plane that halves his collision box, his feet to the top of his head -
// goes through a side's plane over its aperture. The box, not the head: the box is what the world stops, so what he
// collides with and what goes through are the same thing. Leaning in, reaching with the hands, or clipping the aperture
// does not cross; the whole collision box must fit the aperture. A frame or sill stops him as any wall does; a
// raised opening requires a jump into it. His trigger touched
// and active, kept where he is, how he moves and where he looks relative to the gate (turned and shifted by the side's
// own mapping, the view's: it does not jump). This machine's player's client is told
// (hands::portalCrossing): the play space turns by the gate's yaw itself, the head's lean and the stairs' easing are kept
// through the jump. What flies (missiles, grenades, gibs, thrown props: Box3D takes the move up) as its frame's path
// crosses the plane; traces with MOVE_PORTALS (the guns' pellets, the lightning) go on from the far side (portal_entry,
// portal_exit, portal_turn: the beams in two pieces). QuakeC's teleport_touch leaves players to it (portal_handles):
// for every recognised slipgate, including while the frame or sill blocks him. Triggers without recognised faces
// retain Quake's teleport. VR_Portal_Crossed does the rest of what
// the teleport did (its targets, what the hands carry). Monsters walk through paired gates so too (vr_portals_monsters:
// VR_PortalMonsterCross), else teleport as in Quake; ragdolls are carried whole as the pelvis crosses (box3d).

#pragma once

#include <glm/vec3.hpp>
#include "Zancle/Container/Vector.hpp"
#include <glm/mat3x3.hpp>

struct edict_s;

namespace qvr::portals
{
struct Reach
{
    glm::vec3 position{0.f}, from{0.f}, to{0.f};
    glm::mat3 turn{1.f};
    float yaw = 0.f;
    int gate = 0;
};
// A physical hand or muzzle beyond the aperture, while the torso stays here.
Reach reach(const glm::vec3& root, const glm::vec3& point);
// A muzzle (or anything along what the hand holds) beyond the aperture: with the hand if the hand is through (one gate
// for both), else where the weapon itself goes through (the line from the grip to it), else as reach from the body.
// The body's line alone missed a gun reached in aslant or from beside the gate (it crossed the plane beside the
// aperture): the muzzle stayed behind the gate's surface and the shot hit the wall there.
Reach reachAlong(const glm::vec3& root, const glm::vec3& hand, const glm::vec3& point);
// The closest visible one-gate image of a point, for gravity-glove aiming.
glm::vec3 pullImage(const glm::vec3& from, const glm::vec3& point, int* gate = nullptr);
// The same from the client (its effects: the force grab's beam to what it aims at through a gate): where `point` is
// seen from `from`, through a gate (its image behind the gate's surface) or not, under the server's VM (a listen
// server's; a remote one's client: `point`).
glm::vec3 pullImageSeen(const glm::vec3& from, const glm::vec3& point);
// Search centres for a conservative force-grab broad phase: the hand itself and its
// active source-facing one-hop images in destination rooms. pullImage still decides visibility.
void pullSearchOrigins(const glm::vec3& from, za::Vector<glm::vec3>& out, float range = -1.f);

// One-hop AI sight from a monster/muzzle to a player point. A positive gate pins the route.
int aiImage(edict_s* observer, edict_s* target, const glm::vec3& from, const glm::vec3& point,
    int gate, glm::vec3& image);
glm::vec3 aiMap(int gate, const glm::vec3& value, bool direction);

// Views within views (vr_portals_recursion): a camera's depth is how many gates it looks through (0: the eye's own,
// the flat or the spectator camera; 1: a view through a gate; 2: a gate seen in that view; ...). maxDepth(): the
// deepest view drawn, 1 (no views within views) .. 4.
[[nodiscard]] int maxDepth();

// Before each scene view (depth 0): the gates (found anew for a new map) and the sides looked through by this camera,
// ranked. For a view's own views (depth over 0, r_refdef the view's camera: carryCamera), those seen through the gate
// the view is in, beyond its exit, within its box on the screen.
void update(const float* origin = nullptr, const float* angles = nullptr, int depth = 0);

// Candidate views ranked for the camera of a depth; limit is 1..8: vr_portals_maxviews (default 4) for the eye's,
// vr_portals_recursion_views within a view through a gate, 1 deeper.
int viewLimit(int depth = 0);
int viewCount(int depth = 0);
int layer(int depth); // the array layer the view of this depth (1..) about to be drawn goes to
void selectView(int view, int depth = 0);
// r_refdef moved through the gate the view of `depth` looks through (as VR_PortalView does as it is drawn): its
// camera, whose views are ranked and drawn before it.
void carryCamera(int depth);
// The view of `depth` is drawn at 1/scaleAt of its pixels (r_refdef.scale): 1 for the first gate's,
// vr_portals_recursion_scale times smaller each gate deeper.
[[nodiscard]] int scaleAt(int depth);

// For an eye about to be drawn (vr_stereo.cpp's eye loop; its view set: stereo::eye()): whether a view through the
// gate is wanted for it (the gate is on its screen).
[[nodiscard]] bool wantedForView();

// Brackets the drawing of the view of `depth` (1..) through the gate selected for it; `texture` is the array its
// scene's colour (composite) went to, read by the teleport faces of the camera seeing it.
void beginView(int depth);
void endView(unsigned texture, int depth);
// Whether another view may be drawn for a camera of `depth`: views within views, at most vr_portals_recursion_max for
// an eye's camera, drawn depth first (the gate it looks at most first: its views within views before the next gate's);
// the eye's own views always.
[[nodiscard]] bool moreViews(int depth);

// Whether the view being drawn is one through a gate, and how many gates deep (0: none).
[[nodiscard]] bool viewing();
[[nodiscard]] int viewDepth();

// vr_portals_shot (Debug > Slipgates): read the next view through a gate back from its own targets and report what it
// shows - its whole image and the gate's box on the eye's screen, in colour and in depth (depth over 0 in the box:
// something was drawn there). For measuring the view itself rather than which gate is looked through.
void requestShot();
[[nodiscard]] bool shotWanted();
void takeShot(unsigned sceneFbo, int width, int height);

// vr_portals_info: the gates built for this map and where the local player's body is against them (Debug > Slipgates).
void registerCommands();

// Torch candidate views: source and visible gate destinations, using their own PVS and folded distance.
void prepareLightViews(const glm::vec3& eye);
[[nodiscard]] float lightDistance(const glm::vec3& pos, int leaf);

struct LightGate
{
    glm::vec3 from, to, normal, mins, maxs;
    glm::mat3 turn;
    float dist;
};
// One traversal only, through active apertures reached from the light's front side.
int lightGates(const glm::vec3& light, float radius, LightGate* out, int capacity);
// A rigid body's bounds crossing a fitting aperture. Either room can be its owner. An exit with no gate of its own (a
// destination in the open: its aperture is only where things come out) splits only what moves out of it (`velocity`,
// units a second; none: nothing): else a prop resting where a gate leads was cut there.
bool splitBounds(const glm::vec3& lo, const glm::vec3& hi, LightGate& gate, float margin = 0.f, const glm::vec3* velocity = nullptr);
// A point gone from `from` to `to` (world units, a step) in through a gate: its plane crossed from the front over the
// aperture, the gate active (as VR_PortalToss takes what flies). Its mapping in `gate`. (A ragdoll's pelvis: the whole
// ragdoll is carried, box3d.)
bool crossedGate(const glm::vec3& from, const glm::vec3& to, LightGate& gate);

// An eye ahead of the body through a gate (the head leaning or walking in before the torso, which is what the gate
// carries; or still behind once the body is through): the line from `body` (the body's axis at the eye's height) to
// `eye` crosses an open gate's aperture. Then `eye` is moved through that gate and `angles` turned with it, so the view
// is the room the eye is in, not what lies behind the gate's surface. False: no gate between them.
bool eyeThrough(const glm::vec3& body, glm::vec3& eye, glm::vec3& angles);

} // namespace qvr::portals
