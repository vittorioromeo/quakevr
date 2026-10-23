// vr_checklist.hpp -- the playtest checklist (the VR menu's "Checklist" corner button, Debug > Checklist): what to
// test in the headset or give feedback on, read from quakevr/checklist.txt at runtime (the coordinator edits it, no
// rebuild), each item ticked in the menu. The ticks are kept in quakevr/checklist_ticks.txt (not committed), by the
// item's text: items added, moved or removed leave the others' ticks alone.

#pragma once

#include "Zancle/String/String.hpp"

namespace qvr::checklist
{

// Wrapped lines' width in characters (the menu's row: 40, less the "[x] " before the text).
inline constexpr int columns = 36;

// Reads the list again if its file changed (looked at once a second at most; `force`: now), and the ticks at the
// first call. Cheap: called by the menu while it is shown.
void refresh(bool force = false);

// Changes with the list and with each tick (the menu's page built again).
[[nodiscard]] int generation();

[[nodiscard]] int itemCount();
[[nodiscard]] int openCount(); // the items not ticked (passed)
[[nodiscard]] bool loaded();   // the file was found

// An item's section (-1: none, before the first "[Section]"), and a section's name.
[[nodiscard]] int sectionOf(int item);
[[nodiscard]] const char* sectionName(int section);

// Each item's state (ROUND22.md, "Checklist: Pass, Fail and Test"): untested, passed (ticked: retired, left out with Hide
// Ticked) or failed (kept on the list, marked, until a new line replaces it). Kept in the ticks file: a passed item's
// text a line (as the ticks always were), a failed one's after "[fail] ".
enum Mark : int
{
    Untested = 0,
    Passed = 1,
    Failed = 2
};
[[nodiscard]] int mark(int item);
[[nodiscard]] bool ticked(int item); // passed
// Pass or Fail pressed: the item marked so, or back to untested if it already was (saved at once).
void press(int item, int m);
void toggle(int item); // press(item, Passed): a tick, or an untick
[[nodiscard]] int failedCount();

// An item's test scenario: its line's trailing "{test:<id>}" tag (not part of its text, its key or what is drawn), the
// id of quakevr/scenarios/<id>.txt (vr_scenario.hpp); "" none.
[[nodiscard]] const char* scenario(int item);
// The item's text as in the file, its tag left out ("" : no such item).
[[nodiscard]] const char* text(int item);

// Undo, this session: each press made (press, toggle) is kept, the last `undoDepth`; undo() puts the last one back as
// it was (saved at once) and returns that item (-1: nothing to undo; -2: the item is no longer on the list).
inline constexpr int undoDepth = 64;
[[nodiscard]] int undoCount();
[[nodiscard]] const char* undoText(); // the item's text the next undo() changes ("" : none)
[[nodiscard]] int undoRestores();     // the Mark that undo puts back
int undo();

// An item's text wrapped to `columns`: how many lines, and one of them (valid until the next call).
[[nodiscard]] int lineCount(int item);
[[nodiscard]] const char* line(int item, int l);

// vr_checklist [reload | tick <n> | pass <n> | fail <n> | test <n> | undo]: the list (CLSUM/CLITEM lines, for tests),
// read again, item n ticked or unticked, passed or failed (again: untested), its scenario run, or the last press undone
// (CLUNDO: the item, the undos left).
void command_f();

// The item the menu's cursor is on (or picked): kept for a voice note's context (vr_notectx.cpp). lastHovered: its text
// and section, its index now (-1: no longer listed) and when (realtime); false: none since the start.
void hovered(int item);
[[nodiscard]] bool lastHovered(za::String& text, za::String& section, int& item, double& when);

} // namespace qvr::checklist
