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
// shimmer. Views do not recurse; teleport surfaces inside a destination view are omitted. The server sends what is round
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
// the teleport did (its targets, what the hands carry). Monsters teleport as in Quake.

#pragma once

#include <glm/vec3.hpp>
#include <glm/mat3x3.hpp>

namespace qvr::portals
{

// Before each scene view: the gates (found anew for a new map) and the side looked through by this camera.
void update(const float* origin = nullptr, const float* angles = nullptr);

// Candidate views ranked for this camera; limit is 1..8, default 4. No recursive portal views.
int viewLimit();
int viewCount();
int layer(); // next array layer to receive the current rendered view
void selectView(int view);

// For an eye about to be drawn (vr_stereo.cpp's eye loop; its view set: stereo::eye()): whether a view through the
// gate is wanted for it (the gate is on its screen).
[[nodiscard]] bool wantedForView();

// Brackets the drawing of the view through the gate for an eye; `texture` is its scene's colour (composite), read by
// that eye's teleport faces.
void beginView();
void endView(unsigned texture);

// Whether the view being drawn is one through a gate.
[[nodiscard]] bool viewing();

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

} // namespace qvr::portals
