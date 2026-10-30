// vr_menu_checklist.inc -- the Checklist page (Debug > Checklist): items, folds, ticks, undo.
// Split from vr_menu.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

// Checklist (vr_checklist.hpp; the corner's "Checklist" button, Debug > Checklist): what to test in the headset or give
// feedback on, from quakevr/checklist.txt, each item ticked by picking it; its long texts on the lines under it.
int checklistGeneration = -1; // the list's generation the page was built for
int checklistHidden = -1;     // and Hide Ticked's

[[nodiscard]] const char* checklistSummary()
{
    char(&text)[48] = readouts.checklistSummary;
    if(!checklist::loaded())
    {
        return "No quakevr/checklist.txt";
    }
    if(checklist::failedCount() > 0)
    {
        q_snprintf(text, sizeof(text), "%d open of %d, %d failed", checklist::openCount(), checklist::itemCount(),
            checklist::failedCount());
    }
    else
    {
        q_snprintf(text, sizeof(text), "%d open of %d", checklist::openCount(), checklist::itemCount());
    }
    return text;
}

// The test running (vr_scenario.hpp): its id, or none.
[[nodiscard]] const char* checklistTesting()
{
    char(&text)[48] = readouts.checklistTesting;
    if(!*scenario::current())
    {
        return "No test running";
    }
    q_snprintf(text, sizeof(text), "Testing: %s", scenario::current());
    return text;
}

[[nodiscard]] const char* checklistBlank()
{
    return " "; // (the gap between two entries)
}

[[nodiscard]] const char* checklistEmpty()
{
    return checklist::itemCount() > 0 ? "All ticked." : "Nothing on the list.";
}

[[nodiscard]] const char* checklistLine(int arg)
{
    return checklist::line(arg >> 8, arg & 255);
}

[[nodiscard]] bool checklistDim(int arg)
{
    return checklist::ticked(arg >> 8);
}

[[nodiscard]] int checklistMark(int arg)
{
    const int m = checklist::mark(arg >> 8);
    return m == checklist::Passed ? 1 : m == checklist::Failed ? 2 : 0;
}

[[nodiscard]] const char* checklistHelp(int arg)
{
    const int item = arg >> 8;
    const int m = checklist::mark(item);
    za::String& text = readouts.checklistItemHelp;
    text = m == checklist::Passed ? "Passed. " : m == checklist::Failed ? "Failed: kept on the list until it is replaced. " : "";
    text += "Pick Pass, Fail or Test under it (left and right, or point at one); Pass or Fail again puts it back to "
            "untested. ";
    const char* id = checklist::scenario(item);
    if(*id)
    {
        text += "Test sets up its scene (";
        text += id;
        text += ") and puts you in it; Reset Test above runs it again.";
    }
    else
    {
        text += "No test scene for this one.";
    }
    return text.cStr();
}

// The buttons under each item: Pass, Fail, Test.
[[nodiscard]] const char* checklistButtonLabel(int, int b)
{
    return b == 0 ? "Pass" : b == 1 ? "Fail" : "Test";
}

[[nodiscard]] bool checklistButtonDim(int arg, int b)
{
    return b == 2 && !*checklist::scenario(arg >> 8);
}

[[nodiscard]] int checklistButtonTint(int arg, int b)
{
    const int m = checklist::mark(arg >> 8);
    return b == 0 && m == checklist::Passed ? 1 : b == 1 && m == checklist::Failed ? 2 : 0;
}

[[nodiscard]] const char* checklistButtonsText(int)
{
    return "Pass  Fail  Test"; // (what a search or a dump of the page shows)
}

void checklistButtonPress(int arg, int b)
{
    const int item = arg >> 8;
    checklist::hovered(item); // (a voice note's context: the item picked)
    Con_DPrintf("checklist: item %d, %s pressed\n", item, checklistButtonLabel(arg, b));
    if(b < 2)
    {
        checklist::press(item, b == 0 ? checklist::Passed : checklist::Failed);
        return;
    }
    if(scenario::start(checklist::scenario(item), checklist::text(item)))
    {
        menuui::backToGame(1); // (the main hand's buzz; the menu reopens here)
    }
}

void checklistResetTest()
{
    if(scenario::reset())
    {
        menuui::backToGame(1);
    }
}

void checklistEndTest()
{
    scenario::stop();
}

void checklistReload()
{
    checklist::refresh(true);
}

// A section's fold row, under its header (ROUND23.md, "Checklist: folding sections"): its count, picked to fold or unfold
// it (the folds kept in quakevr/checklist_sections.txt).
[[nodiscard]] const char* checklistSectionRow(int s)
{
    char(&text)[48] = readouts.checklistSection;
    const int open = checklist::sectionOpen(s);
    const int total = checklist::sectionTotal(s);
    const bool folded = checklist::collapsed(s);
    if(open == 0)
    {
        q_snprintf(text, sizeof(text), "%s All %d passed (%s)", folded ? "[+]" : "[-]", total, folded ? "show" : "hide");
    }
    else
    {
        q_snprintf(text, sizeof(text), "%s %d open of %d (%s)", folded ? "[+]" : "[-]", open, total, folded ? "show" : "hide");
    }
    return text;
}

[[nodiscard]] const char* checklistSectionHelp(int s)
{
    za::String& text = readouts.checklistSectionHelp;
    text = checklist::collapsed(s) ? "Folded: pick it to show this section's items again. " : "Pick it to fold this section: only its header and count shown. ";
    text += "Kept across Reload List and restarts; a section with nothing open starts folded.";
    return text.cStr();
}

[[nodiscard]] bool checklistSectionDim(int)
{
    return false; // (set only for the selected row's highlight, as an item's)
}

[[nodiscard]] int checklistSectionMark(int s)
{
    return checklist::sectionTotal(s) > 0 && checklist::sectionOpen(s) == 0 ? 1 : 0; // (green: all passed)
}

void checklistSectionPick(int s)
{
    checklist::setCollapsed(s, !checklist::collapsed(s));
}

void checklistCollapseAll()
{
    checklist::setCollapsed(-1, true);
}

void checklistExpandAll()
{
    checklist::setCollapsed(-1, false);
}

// Undo Last Tick: the session's ticks and unticks taken back one at a time, the last first (checklist::undo); its help
// names the item the next one changes, and the one it changed last (found again with Hide Ticked off).
[[nodiscard]] bool checklistUndoDim(int)
{
    return checklist::undoCount() == 0;
}

[[nodiscard]] const char* checklistUndoHelp(int)
{
    za::String& text = readouts.checklistUndoHelp;
    char count[48];
    q_snprintf(count, sizeof(count), "%d to undo. ", checklist::undoCount());
    text = checklist::undoCount() == 0 ? "Nothing to undo: each tick and untick made since the game started can be "
                                         "taken back here, the last first."
                                       : count;
    const int restores = checklist::undoRestores();
    text += checklist::undoCount() == 0               ? ""
            : restores == checklist::Passed ? "Next makes it passed again: "
            : restores == checklist::Failed ? "Next makes it failed again: "
                                            : "Next makes it untested again: ";
    if(checklist::undoCount() > 0)
    {
        text += checklist::undoText();
    }
    if(!pageTexts.checklistUndone.empty())
    {
        text += checklist::undoCount() > 0 ? " (Last undone: " : " Last undone: ";
        text += pageTexts.checklistUndone;
        text += checklist::undoCount() > 0 ? ")" : "";
    }
    return text.cStr();
}

void checklistUndo()
{
    const za::String what = checklist::undoText();
    if(checklist::undo() == -1)
    {
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return;
    }
    pageTexts.checklistUndone = what;
    qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
}

[[nodiscard]] za::Vector<Item> pageChecklist()
{
    checklist::refresh();
    checklistGeneration = checklist::generation();
    checklistHidden = vr_checklist_hide_ticked.value != 0.f ? 1 : 0;

    // The headers' names, one for each run of items under a section, where the items point (all copied before any is
    // pointed at): the name, cut to fit, and its open and total counts ("Survival Arena  12/42").
    const int n = checklist::itemCount();
    za::Vector<za::String>& names = pageTexts.checklistSections;
    names.clear();
    for(int i = 0; i < n; i++)
    {
        if(i == 0 || checklist::sectionOf(i) != checklist::sectionOf(i - 1))
        {
            const int s = checklist::sectionOf(i);
            const char* name = checklist::sectionName(s);
            char label[41] = "";
            if(*name)
            {
                char count[24];
                q_snprintf(count, sizeof(count), "  %d/%d", checklist::sectionOpen(s), checklist::sectionTotal(s));
                const int room = 40 - static_cast<int>(strlen(count));
                const int len = static_cast<int>(strlen(name));
                q_snprintf(label, sizeof(label), len > room ? "%.*s..%s" : "%.*s%s", len > room ? room - 2 : len, name, count);
            }
            names.emplaceBack(label);
        }
    }

    za::Vector<Item> items = {
        info(checklistSummary),
        toggle("Hide Ticked", vr_checklist_hide_ticked).help("Leaves the passed items out of the list. Off: every item, the passed ones dimmed."),
        action("Reload List", checklistReload)
            .help("Reads quakevr/checklist.txt again (done by itself too when the file changes). Passes and fails are "
                  "kept by each item's text, in quakevr/checklist_ticks.txt."),
        info(checklistTesting),
        action("Reset Test", checklistResetTest)
            .help("Runs the last test scene again from the start (the map loaded fresh, everything placed again), and "
                  "closes the menu. Bind a key to vr_scenario reset for the same."),
        action("End Test", checklistEndTest)
            .help("Ends the test scene: the settings it changed put back (also done by the next test, another map, a "
                  "load, or quitting). You stay where you are."),
    };
    Item undo = action("Undo Last Tick", checklistUndo);
    undo.helpArg = checklistUndoHelp;
    undo.dimArg = checklistUndoDim;
    items.insert(items.begin() + 2, undo); // (under Hide Ticked)
    if(checklist::sectionCount() > 1)
    {
        items.pushBack(action("Collapse All", checklistCollapseAll)
                .help("Folds every section: only the headers and their counts shown, each unfolded by picking the row "
                      "under it."));
        items.pushBack(action("Expand All", checklistExpandAll).help("Unfolds every section."));
    }
    // Each section: its header (its name and counts) and its fold row, always; its items unless folded.
    int run = -1;
    bool folded = false;
    int shown = 0;  // the items shown under this header
    int listed = 0; // and the rows, in all
    for(int i = 0; i < n && i < (1 << 20); i++)
    {
        if(i == 0 || checklist::sectionOf(i) != checklist::sectionOf(i - 1))
        {
            run++;
            const int s = checklist::sectionOf(i);
            folded = !names[run].empty() && checklist::collapsed(s);
            if(!names[run].empty())
            {
                items.pushBack(header(names[run].cStr()));
                Item f = row(checklistSectionRow, checklistSectionHelp, checklistSectionPick, s, -1);
                f.dimArg = checklistSectionDim;
                f.markArg = checklistSectionMark;
                items.pushBack(f);
                shown = 0; // (no gap between the fold row and the first item)
                listed++;
            }
        }
        if(folded || (checklistHidden && checklist::ticked(i)))
        {
            continue;
        }
        if(shown > 0)
        {
            items.pushBack(info(checklistBlank)); // vertical padding: long entries no longer run into a wall of text
        }
        shown++;
        listed++;
        const int lines = q_min(checklist::lineCount(i), 255);
        for(int l = 0; l < lines; l++)
        {
            Item r = row(checklistLine, checklistHelp, nullptr, (i << 8) | l, -1);
            r.dimArg = checklistDim;
            r.markArg = checklistMark;
            r.partOf = l;
            if(l == 0)
            {
                r.buttons = 3;
                r.buttonPress = checklistButtonPress;
                r.buttonDim = checklistButtonDim;
            }
            items.pushBack(r);
        }
        // Its buttons, under it (part of it: selected with it).
        Item b = row(checklistButtonsText, checklistHelp, nullptr, (i << 8) | lines, -1);
        b.partOf = lines;
        b.buttons = 3;
        b.buttonLabel = checklistButtonLabel;
        b.buttonDim = checklistButtonDim;
        b.buttonTint = checklistButtonTint;
        items.pushBack(b);
    }
    if(listed == 0)
    {
        items.pushBack(info(checklistEmpty));
    }
    return items;
}
