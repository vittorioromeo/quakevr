// vr_update.hpp -- the in-game update notice's check (vr_update.cpp): the installer's release feed (latest.json) read
// on a thread of its own at start-up, at most once an hour (a cached answer otherwise), its version compared with the
// game's (VR_Version, the VERSION file) as semantic versions. The menus' bottom right corner says when a newer one is
// out (vr_menubrand.cpp).

#pragma once

#include "Zancle/String/String.hpp"

namespace qvr::update
{

// Start-up (the first frame's end, the config read): the check's thread, unless vr_update_check is 0 or this is a
// test run (QVR_TEST_BACKGROUND). Nothing waits for it.
void start();
// Quit: the check cancelled and its thread joined (or let go after 3 s, as the map index's).
void finish();
// The main thread, every frame: takes what the check found.
void poll();
// vr_update_status, vr_update_check_now.
void registerCommands();

// A newer release than this game, to tell the player about: its version as the feed gives it ("0.9.1") and the page
// to open (the feed's "page", else the releases' latest). Null: none (nothing newer, not checked, vr_update_check 0).
// vr_update_test_version stands in for the feed's version. Main thread.
struct Notice
{
    za::String version;
    za::String page;
};
[[nodiscard]] const Notice* notice();

// Semantic versions (semver.org, 2.0.0): <0 when a is older than b, 0 the same, >0 newer. A leading "v" and anything
// after a space (the build's " (date hash)") are ignored, and so is "+build". `ok` false: one of them is not
// MAJOR.MINOR.PATCH[-prerelease] (the comparison is then 0).
[[nodiscard]] int compareVersions(const char* a, const char* b, bool& ok);

} // namespace qvr::update
