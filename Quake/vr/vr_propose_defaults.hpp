// vr_propose_defaults.hpp -- Save My Settings as Defaults (ROUND22.md, "Proposing his settings as defaults"): every
// setting changed from its shipped default (its cvar's default, vr_defaults.cfg's vr_default included) written to a
// proposal file for the coordinator, quakevr/proposed_defaults/<date>_<time>.cfg: one line a setting,
//   name "value" // default "old", <menu path, or not in the menus>, archived|not archived[, personal?|debug?]
// sorted by area (the menus' pages; the weapon slots, vr_wofs_*; the held objects' slots, vr_prop_*; the rest by their
// names' prefix). Everything is written (debug settings too), the rows that look personal (calibration, comfort, height,
// handedness, the hands' angles, the body's measures, the menus' own) or debug (debug, test, show, dump, profile, trace,
// mock) tagged for the coordinator to filter. Nothing in the game changes: no default is set here.
//
// From the console (vr_propose_defaults), Debug > Save My Settings as Defaults, and the menu's corner button (Save
// Default: at Menu Detail: Developer only; a second press within 3 seconds writes, the button then says "Saved N").

#pragma once

#include "Zancle/String/String.hpp"

namespace qvr::proposedefaults
{

void init(); // vr_propose_defaults

// Writes the proposal file (its path in `path`) and prints "Saved N settings for review: <path>". The settings written,
// -1 when the file could not be written.
[[nodiscard]] int write(za::String& path);

} // namespace qvr::proposedefaults
