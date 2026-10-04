// vr_portals.hpp -- slipgates that show where they lead (vr_portals), as Portal's portals do; a view only.
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
// Once a frame the side worth a look is picked (in front of the head, in its PVS, near, big on the eyes; held a while).
// For each eye, before the eye's own view, the scene is drawn again into scene targets of the eye's size from that eye
// moved through the gate (vr_stereo.cpp): the same projection with an oblique near plane on the destination's side of
// the gate's plane (what is between the view and that plane is not drawn), and outside the gate's box on screen the
// depth set to the near plane first, so that nothing is shaded there. Drawing the eye, the side's faces (the liquid
// shaders, LiquidPortal) show that image by their pixels, whatever their shape, under a little of the slipgate's own
// shimmer. One gate at a time; a gate seen through a gate shows its texture. The server sends the client what is round
// the destinations of the gates it can see (their PVS added to its own), so the monsters and items there are seen too.
// Both eyes are right (each its own view); the desktop's spectator camera shows the slipgates' texture.
//
// Walking and shooting through (vr_portals_walk; the server, single player and listen servers): a player is carried
// through as his head comes within 18 units of a side's plane over the gate, his trigger touched and active, kept
// where he is, how he moves and where he looks relative to the gate (turned and shifted by the side's own mapping, the
// view's: it does not jump); what flies (missiles, grenades, gibs) as its frame's path crosses the plane. QuakeC's
// teleport_touch leaves players to it (portal_handles) unless one stays in the trigger 1.5 s without reaching the
// plane (Quake's teleport then); VR_Portal_Crossed does the rest of what the teleport did (its targets, what the hands
// carry). Monsters teleport as in Quake.

#pragma once

namespace qvr::portals
{

// Once a frame, before the eyes are drawn: the gates (found anew for a new map) and the side looked through.
void update();

// For an eye about to be drawn (vr_stereo.cpp's eye loop; its view set: stereo::eye()): whether a view through the
// gate is wanted for it (the gate is on its screen).
[[nodiscard]] bool wantedForEye(int eye);

// Brackets the drawing of the view through the gate for an eye; `texture` is its scene's colour (composite), read by
// that eye's teleport faces.
void beginView();
void endView(unsigned texture);

// Whether the view being drawn is one through a gate.
[[nodiscard]] bool viewing();

} // namespace qvr::portals
