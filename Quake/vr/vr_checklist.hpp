// vr_checklist.hpp -- the playtest checklist (the VR menu's "Checklist" corner button, Debug > Checklist): what to
// test in the headset or give feedback on, read from quakevr/checklist.txt at runtime (the coordinator edits it, no
// rebuild), each item ticked in the menu. The ticks are kept in quakevr/checklist_ticks.txt (not committed), by the
// item's text: items added, moved or removed leave the others' ticks alone.

#pragma once

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
[[nodiscard]] int openCount(); // the items not ticked
[[nodiscard]] bool loaded();   // the file was found

// An item's section (-1: none, before the first "[Section]"), and a section's name.
[[nodiscard]] int sectionOf(int item);
[[nodiscard]] const char* sectionName(int section);

[[nodiscard]] bool ticked(int item);
void toggle(int item); // saved at once

// Undo, this session: each tick and untick made (toggle) is kept, the last `undoDepth`; undo() puts the last one back as
// it was (saved at once) and returns that item (-1: nothing to undo; -2: the item is no longer on the list).
inline constexpr int undoDepth = 64;
[[nodiscard]] int undoCount();
[[nodiscard]] const char* undoText(); // the item's text the next undo() changes ("" : none)
[[nodiscard]] bool undoTicks();       // whether that undo ticks it again (it was ticked before) rather than unticks it
int undo();

// An item's text wrapped to `columns`: how many lines, and one of them (valid until the next call).
[[nodiscard]] int lineCount(int item);
[[nodiscard]] const char* line(int item, int l);

// vr_checklist [reload | tick <n> | undo]: the list (CLSUM/CLITEM lines, for tests), read again, item n ticked or
// unticked, or the last tick undone (CLUNDO: the item, the undos left).
void command_f();

} // namespace qvr::checklist
