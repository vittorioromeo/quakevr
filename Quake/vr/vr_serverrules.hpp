// vr_serverrules.hpp -- server rules: settings the server's QC judges by for every player (docs/vr-port/MULTIPLAYER.md,
// "Server rules"). The melee timing settings (how fast a blow must be, how long a pommel strike waits, a bash's and a
// headbutt's speed, a deflection's speed and window) decide whether a blow lands, so the server's value governs all
// clients: it is sent to each client as it joins (QVR_SVC_RULES, with the spawn state) and again whenever it changes.
//
// Single player and a listen server's host: nothing changes (the server is this process; its cvar is the rule). A client
// of a remote server: the menu shows the server's value, dimmed, and does not change it (the local cvar stays the
// player's own value, for when they host). vr_serverrules prints them.

#pragma once

struct cvar_s;

namespace qvr::serverrules
{

// Whether `var` is a server rule.
[[nodiscard]] bool isRule(const cvar_s* var);

// Connected to a server in another process (not single player, not a listen server's host, not a demo).
[[nodiscard]] bool remote();

// A server rule while connected to a remote server: shown as the server's, not changed from here.
[[nodiscard]] bool locked(const cvar_s* var);

// The value that governs `var` here: the server's while locked (once received), its own otherwise.
[[nodiscard]] float shown(const cvar_s* var);

// Server: every rule into a joining client's spawn state.
void serverWriteAll(struct sizebuf_s* msg);

// Server: the rules changed since last sent, to every client (reliable). Once a server frame.
void serverFrame();

// Server: a new map is up: what the clients get with their spawn state counts as sent.
void serverReset();

// Client: QVR_SVC_RULES.
void clientParse();

// Client: a new connection or map: nothing received yet.
void clientReset();

void registerCommands();

} // namespace qvr::serverrules
