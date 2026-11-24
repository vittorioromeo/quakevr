// vr_msgtokens.hpp -- a game message's tokens, resolved as it is shown: its flat-screen wording and the player's own keys.
//
// A message (a centre print, a map tip, a world text board, a view message: the tutorial's, ROUND25 "tutflat") may hold
//   - "{flat}": what comes before it is the headset's wording, what comes after it the flat screen's (no headset:
//     !vrActive()); without it the one text is both;
//   - "{key:<cvar>}": the key a key cvar names (vr_flat_key_grenade: 'G'); "{key:<command>}": the first key bound to a
//     command (+jump: 'SPACE'; in flat play not one of the keys flat play reads itself, VR_FlatKeyOwned); none bound:
//     "[<what>: not bound]" (a flat key's Controls label, else the command).
// Resolved on the client from its own binds (a client in a multiplayer game sees his), again whenever they change
// (signature()), so a rebind shows at once on what is up.
#pragma once

#include "Zancle/String/String.hpp"

namespace qvr::msgtokens
{

// Whether `text` holds a token (nothing to resolve otherwise).
[[nodiscard]] bool has(const char* text);

// `text` as shown now: the mode's wording, every key token resolved.
[[nodiscard]] za::String expand(const char* text);

// Changes whenever what expand() reads changes: the mode, a bind, a key cvar.
[[nodiscard]] unsigned signature();

// vr_msgtokens_test (Debug > Tests): its run, once the map's tips have come (a frame each).
void frame();
void test_f();

} // namespace qvr::msgtokens
