// vr_scenario.hpp -- the checklist's test scenes (ROUND22.md, "Checklist: Pass, Fail and Test"; docs/vr-port/TESTING.md,
// "Checklist scenarios: how to write one"): quakevr/scenarios/<id>.txt, a few plain lines saying the map, where you stand
// and face, what is in your hands, what is placed around you (the toolgun's spawn list: monsters, armed grunts, props,
// weapons), cheats, settings changed for the test (put back after it) and console commands. A checklist line points at
// one with a trailing "{test:<id>}"; its Test button (or vr_scenario <id>) loads the map fresh, sets everything up and
// shows the hint (the checklist line's text, or the file's own) floating ahead for vr_scenario_hint_time seconds.
//
// The format, one directive a line ('#' lines are comments; a name with spaces in "quotes"):
//   title <text>                         what it is (vr_scenario list)
//   map <name>                           maps/<name>.bsp, loaded fresh at every run (required)
//   start <x> <y> <z> [yaw]              where you stand (the player's origin, as `vr_scenario where` prints it) and face;
//                                        default the map's start
//   god | notarget                       cheats on
//   health <n>
//   give <weapon> [main|off]             a toolgun Weapons entry ("Crowbar", "AD Death Sergeant's Flail") in that hand
//   spawn <entry> <fwd> [left] [up] [yaw]
//                                        a toolgun spawn entry by its label ("Armed Grunt: Axe", "Wooden Crate",
//                                        "Explosive Box"; or a classname) `fwd` units ahead of you, `left` to your left,
//                                        on the floor under that point (`up` above it); facing you, or `yaw` degrees
//                                        turned from your facing (0: the way you face)
//   spawnarmed <classname> <weapon> <weapon2> <fwd> [left] [up] [yaw]
//                                        an armed humanoid (monster_vr_grunt, monster_vr_enforcer) with its right hand's weapon
//                                        code (vr_toolgun_armed_list; 0 the pool, -1 fists) and its left's (0 none), placed as spawn
//   cvar <name> <value>                set for the test; put back by End Test, the next test, a map change or load, and
//                                        never written to the config
//   cmd <console command>                run once everything is placed (`cmd vr_agrunt_test 7`, `cmd impulse 9`)
//   hint <text>                          the hint shown (default: the checklist line's text)

#pragma once

namespace qvr::scenario
{

// Runs scenario `id`: its settings set, the map loaded, the rest placed once it has spawned. `hint`: the hint when the
// file has none (the checklist line's text). False (printed, and lastError()): no such file, or errors in it.
bool start(const char* id, const char* hint = "");
// The last one started, again from the start (Reset Test). False: none yet.
bool reset();
// The test ended: its settings put back, its hint hidden (you stay where you are).
void stop();

// The test loading or running ("" none), and the last start's error ("" none).
[[nodiscard]] const char* current();
[[nodiscard]] const char* lastError();

// Once a frame (vr_main.cpp): the placing once the map is in, the hint, vr_scenario runall's steps.
void frame();
// The server's world reset (vr_progs.cpp resetServerWorld): another map or a load ends the test (not its own load).
void worldReset();

// vr_scenario [<id> | reset | stop | hint | list | validate | runall | where]: see vr_scenario.cpp's command_f.
void command_f();

} // namespace qvr::scenario
