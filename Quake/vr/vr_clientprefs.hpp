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

// Client: this player's preferences up, when they changed (at most every vr_net_avatarcal_interval); from the move.
void send();

// Client: a new connection: they go up again.
void clientClear();

// Server: the slot's player left or a new one came: his preferences forgotten.
void serverReset(int slot);

// vr_mp_prefs, the "vr_prefs" client command.
void init();

} // namespace qvr::clientprefs
