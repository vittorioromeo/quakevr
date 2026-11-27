// vr_serverrules.hpp -- server rules: settings the server's QC judges by for every player (docs/vr-port/MP_DESIGN.md,
// "Server rules"; MP_PLAN.md, B6: "settings in three kinds", the first). Every balance setting is one: the settings on
// the balance pages of the menus (Advanced VR Options > Combat, Carrying and Throwing, Arcane Dimensions, World; Movement's
// Climbing, Grappling Hook, Swimming and hitboxes; VR Settings > Gameplay), the gameplay preset's switches and the melee
// timing (how fast a blow must be, a pommel's wait, a bash's, headbutt's and deflection's speed and window). Not rules
// though on those pages: a player's preferences (vr_clientprefs.cpp), settings not kept in the config (tests, debug) and
// what only his own senses get (sounds, haptics, colours, what is shown: their names, below).
//
// The server's values go to each client as it joins (QVR_SVC_RULES, with the spawn state: those off their defaults, the
// rest are the defaults) and again whenever one changes. A client of a remote server plays by them: his cvars hold the
// server's values while he is joined (so whatever his own game reads of a rule, it reads the server's), his own values
// kept and put back when he leaves, and written to his config in the meantime (VR_ServerRulesConfigWrite). His menu shows
// the rules dimmed and does not change them; the Gameplay Presets' buttons are locked.
//
// Single player, a listen server's host and a demo: nothing changes (the server is this process; its cvar is the rule).
// vr_serverrules prints them.

#pragma once

struct cvar_s;

namespace qvr::serverrules
{

// Whether `var` is a server rule.
[[nodiscard]] bool isRule(const cvar_s* var);

// Connected to a server in another process (not single player, not a listen server's host, not a demo).
[[nodiscard]] bool remote();

// A server rule while playing by a remote server's rules: shown as the server's, not changed from here.
[[nodiscard]] bool locked(const cvar_s* var);

// The player's own value of `var` (a rule's kept one while a remote server's govern; else its cvar's).
[[nodiscard]] float own(const cvar_s* var);

// A reason a rule-setting button is locked here (a remote server's rules govern), or nullptr: it works.
[[nodiscard]] const char* lockedBecause();

// Server: every rule off its default into a joining client's spawn state.
void serverWriteAll(struct sizebuf_s* msg);

// Server: the rules changed since last sent, to every client (reliable). Once a server frame.
void serverFrame();

// Server: a new map is up: what the clients get with their spawn state counts as sent.
void serverReset();

// Client: QVR_SVC_RULES.
void clientParse();

// Client: left the server (CL_Disconnect): his own values back.
void clientLeft();

void registerCommands();

} // namespace qvr::serverrules
