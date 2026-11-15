// vr_clientprefs.hpp -- per-player preferences the server judges a player by (MP_PLAN.md, B3: "settings in three kinds";
// the second kind): how he holds, reloads, holsters and aims (vr_weapon_grip_mode, vr_reload_mode..., vr_gunangle...).
// Each client sends his own up when they change (the "vr_prefs" client command, reliable); the server keeps them per
// client slot, and the QC's reads of those settings answer with the player's own (cvar_hget: the player in `self`, else
// in `other`; clientpref(player, handle): that player's). Single player, a demo, a slot whose prefs have not come: the
// server's own settings, as before.

#pragma once

#include "vr_engine.hpp"

namespace qvr::clientprefs
{

// The preference's place in the list for this setting, or -1 when it is not a per-player preference.
[[nodiscard]] int indexOf(const cvar_t* var);

// The value of preference `pref` (indexOf's, of `var`) for `player` (a player edict on the server): his own when he sent
// them (multiplayer), else the server's `var`.
[[nodiscard]] float value(int pref, const cvar_t* var, const edict_t* player);

// cvar_hget's: `var` (preference `pref`) for the player the QC is running for (`self` when a player, else `other` when a
// player), else the server's.
[[nodiscard]] float forQc(int pref, const cvar_t* var);

// Client: this player's preferences up, when they changed (at most every vr_net_avatarcal_interval); from the move. And
// his rig: each hand's fist as he poses it (held::fist; the server's held::fistOf for him), when it changed.
void send();

// Client: a new connection: they go up again.
void clientClear();

// Server: the slot's player left or a new one came: his preferences forgotten (this game's own slot: its hands again).
void serverReset(int slot);

// One store for every player's hands (B9): the server reads each player's fists, palm frames and grip fit rigs from his
// slot (held::fistOf, grip::handFrameOf, gripfit::fit), the same code for all of them. A remote player's arrive in his
// messages (send); this game's own, while it hosts (single player, a listen server), go into its slot directly as they
// are measured (held::setFist, grip::setHandFrame, gripfit::setHandRig): ownSlot() is that slot, -1 when not hosting.
[[nodiscard]] int ownSlot();
void publishOwn(int hand); // (all three of the hand's into ownSlot(): at a reset of it)

// vr_mp_prefs, the "vr_prefs" client command.
void init();

} // namespace qvr::clientprefs
