// vr_checklist.cpp -- the playtest checklist (vr_checklist.hpp): quakevr/checklist.txt read at runtime ("[Section]"
// lines, one item a line, '#' or "//" comments, an item's trailing "{test:<id>}" its scenario), its marks in
// quakevr/checklist_ticks.txt (a passed item's text a line, a failed one's after "[fail] "), the sections folded or
// unfolded by hand in quakevr/checklist_sections.txt ("collapsed <name>" / "expanded <name>").

#include "vr_checklist.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_mem.hpp"
#include "vr_scenario.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/Vocabulary/Pair.hpp"
#include "vr_zancle.hpp"

#include <stdio.h>

namespace qvr::checklist
{

namespace
{

// The list as read, and the ticks (the main thread's; the menu's pages point at none of it: they ask by index).
struct State
{
    za::Vector<za::String> sections;
    za::Vector<za::String> texts;                // each item's text as in the file, its tag left out (its key in the ticks)
    za::Vector<za::String> scenarios;            // each item's "{test:<id>}" tag's id ("" none)
    za::Vector<za::String> shown;                // as drawn: characters the menu's font lacks replaced
    za::Vector<int> section;                      // each item's (-1: none)
    za::Vector<int> firstLine;                    // each item's first wrapped line in `lines`
    za::Vector<za::Pair<int, int>> lines;        // start and length in its `shown` text
    ankerl::unordered_dense::set<za::String> tickedTexts;   // every passed item's text, those no longer listed too
    ankerl::unordered_dense::set<za::String> failedTexts;   // every failed item's, likewise
    za::Vector<za::String> undoTexts;            // the presses made this session, the last at the end
    za::Vector<int> undoWasTicked;               // and each item's Mark before (what undo() restores)
    za::Vector<int> sectionOpens;                // each section's open items, and its items
    za::Vector<int> sectionTotals;
    za::Vector<int> sectionDefaults;             // each section's fold by default (1: nothing open when read)
    ankerl::unordered_dense::set<za::String> collapsedNames; // the sections folded by hand (by name), those gone too
    ankerl::unordered_dense::set<za::String> expandedNames;  // and unfolded by hand
    auto members()
    {
        return qvr::mem::list(sections, texts, scenarios, shown, section, firstLine, lines, tickedTexts, failedTexts,
            undoTexts, undoWasTicked, sectionOpens, sectionTotals, sectionDefaults, collapsedNames, expandedNames);
    }
};
mem::Cache<State> state{"checklist", mem::Never};

constexpr const char* failPrefix = "[fail] "; // a failed item's line in the ticks file
constexpr size_t failPrefixLength = 7;

bool ticksRead = false;
bool fileFound = false;
int gen = 0;
int open = 0;
int failed = 0;
double lastLook = -1e9;
za::I64 fileTime{0}; // (files::lastWriteTime's stamp)
char lineText[64];

[[nodiscard]] za::String listPath()
{
    return za::String{com_gamedir} + "/checklist.txt";
}

[[nodiscard]] za::String ticksPath()
{
    return za::String{com_gamedir} + "/checklist_ticks.txt";
}

[[nodiscard]] za::String foldsPath()
{
    return za::String{com_gamedir} + "/checklist_sections.txt";
}

constexpr const char* collapsedPrefix = "collapsed "; // a folds file's lines: one of these, then the section's name
constexpr size_t collapsedPrefixLength = 10;
constexpr const char* expandedPrefix = "expanded ";
constexpr size_t expandedPrefixLength = 9;

[[nodiscard]] za::String trimmed(za::StringView s)
{
    size_t a = 0;
    size_t b = s.size();
    while(a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n'))
    {
        a++;
    }
    while(b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n'))
    {
        b--;
    }
    return za::String{s.substrByPosLen(a, b - a)};
}

// The file's lines, trimmed (false: no file).
[[nodiscard]] bool readLines(const za::String& path, za::Vector<za::String>& out)
{
    FILE* f = fopen(path.cStr(), "rb");
    if(!f)
    {
        return false;
    }
    za::String text;
    char buf[4096];
    for(size_t n; (n = fread(buf, 1, sizeof(buf), f)) > 0;)
    {
        text.append(buf, n);
    }
    fclose(f);
    if(text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF)
    {
        text.erase(0, 3); // a UTF-8 BOM
    }
    for(size_t start = 0; start <= text.size();)
    {
        size_t end = text.find('\n', start);
        end = end == za::StringView::nPos ? text.size() : end;
        out.pushBack(trimmed(text.substrByPosLen(start, end - start)));
        start = end + 1;
    }
    return true;
}

// As the menu's font draws it: tabs as spaces, each other character outside ASCII (a UTF-8 sequence) as one '?'.
[[nodiscard]] za::String drawable(const za::String& s)
{
    za::String out;
    for(size_t i = 0; i < s.size(); i++)
    {
        const auto c = static_cast<unsigned char>(s[i]);
        if(c == '\t')
        {
            out += ' ';
        }
        else if(c >= 0x80)
        {
            while(i + 1 < s.size() && (static_cast<unsigned char>(s[i + 1]) & 0xC0) == 0x80)
            {
                i++;
            }
            out += '?';
        }
        else if(c >= 32)
        {
            out += static_cast<char>(c);
        }
    }
    return out;
}

// `text` in lines of `columns` at most, broken at spaces (a longer word cut).
void wrap(const za::String& text, za::Vector<za::Pair<int, int>>& out)
{
    const int n = static_cast<int>(text.size());
    int p = 0;
    do
    {
        while(p < n && text[p] == ' ')
        {
            p++;
        }
        int len = n - p;
        if(len > columns)
        {
            len = columns;
            while(len > 0 && text[p + len] != ' ')
            {
                len--;
            }
            if(len == 0)
            {
                len = columns;
            }
        }
        while(len > 0 && text[p + len - 1] == ' ')
        {
            len--;
        }
        out.emplaceBack(p, len);
        p += len;
        while(p < n && text[p] == ' ')
        {
            p++;
        }
    } while(p < n);
}

void countOpen()
{
    open = 0;
    failed = 0;
    state.sectionOpens.clear();
    state.sectionTotals.clear();
    state.sectionOpens.resize(state.sections.size(), 0);
    state.sectionTotals.resize(state.sections.size(), 0);
    for(size_t i = 0; i < state.texts.size(); i++)
    {
        const za::String& t = state.texts[i];
        const int isOpen = state.tickedTexts.count(t) ? 0 : 1;
        open += isOpen;
        failed += state.failedTexts.count(t) ? 1 : 0;
        const int s = state.section[i];
        if(s >= 0)
        {
            state.sectionOpens[s] += isOpen;
            state.sectionTotals[s]++;
        }
    }
}

void readFolds()
{
    state.collapsedNames.clear();
    state.expandedNames.clear();
    za::Vector<za::String> lines;
    if(!readLines(foldsPath(), lines))
    {
        return;
    }
    for(const za::String& l : lines)
    {
        if(l.size() > collapsedPrefixLength && !strncmp(l.cStr(), collapsedPrefix, collapsedPrefixLength))
        {
            state.collapsedNames.insert(za::String{l.cStr() + collapsedPrefixLength});
        }
        else if(l.size() > expandedPrefixLength && !strncmp(l.cStr(), expandedPrefix, expandedPrefixLength))
        {
            state.expandedNames.insert(za::String{l.cStr() + expandedPrefixLength});
        }
    }
}

void writeFolds()
{
    const za::String path = foldsPath();
    FILE* f = fopen(path.cStr(), "wb");
    if(!f)
    {
        Con_Printf("vr_checklist: couldn't write %s\n", path.cStr());
        return;
    }
    // In the list's order, then those no longer listed (as the ticks).
    ankerl::unordered_dense::set<za::String> written;
    for(const za::String& s : state.sections)
    {
        if(written.insert(s).second)
        {
            if(state.collapsedNames.count(s))
            {
                fprintf(f, "%s%s\n", collapsedPrefix, s.cStr());
            }
            else if(state.expandedNames.count(s))
            {
                fprintf(f, "%s%s\n", expandedPrefix, s.cStr());
            }
        }
    }
    for(const za::String& s : state.collapsedNames)
    {
        if(!written.count(s))
        {
            fprintf(f, "%s%s\n", collapsedPrefix, s.cStr());
        }
    }
    for(const za::String& s : state.expandedNames)
    {
        if(!written.count(s))
        {
            fprintf(f, "%s%s\n", expandedPrefix, s.cStr());
        }
    }
    fclose(f);
}

[[nodiscard]] int markOf(const za::String& t)
{
    return state.tickedTexts.count(t) ? Passed : state.failedTexts.count(t) ? Failed : Untested;
}

void setMarkOf(const za::String& t, int m)
{
    state.tickedTexts.erase(t);
    state.failedTexts.erase(t);
    if(m == Passed)
    {
        state.tickedTexts.insert(t);
    }
    else if(m == Failed)
    {
        state.failedTexts.insert(t);
    }
}

void readTicks()
{
    ticksRead = true;
    state.tickedTexts.clear();
    state.failedTexts.clear();
    za::Vector<za::String> lines;
    if(readLines(ticksPath(), lines))
    {
        for(za::String& l : lines)
        {
            if(l.size() > failPrefixLength && !strncmp(l.cStr(), failPrefix, failPrefixLength))
            {
                state.failedTexts.insert(za::String{l.cStr() + failPrefixLength});
            }
            else if(!l.empty())
            {
                state.tickedTexts.insert(ZA_MOVE(l)); // (an older file's: every line a tick)
            }
        }
    }
}

void writeTicks()
{
    const za::String path = ticksPath();
    FILE* f = fopen(path.cStr(), "wb");
    if(!f)
    {
        Con_Printf("vr_checklist: couldn't write %s\n", path.cStr());
        return;
    }
    // In the list's order, then those no longer listed: a stable file to look at.
    ankerl::unordered_dense::set<za::String> written;
    for(const za::String& t : state.texts)
    {
        if(state.tickedTexts.count(t) && written.insert(t).second)
        {
            fprintf(f, "%s\n", t.cStr());
        }
    }
    for(const za::String& t : state.tickedTexts)
    {
        if(!written.count(t))
        {
            fprintf(f, "%s\n", t.cStr());
        }
    }
    written.clear();
    for(const za::String& t : state.texts)
    {
        if(state.failedTexts.count(t) && written.insert(t).second)
        {
            fprintf(f, "%s%s\n", failPrefix, t.cStr());
        }
    }
    for(const za::String& t : state.failedTexts)
    {
        if(!written.count(t))
        {
            fprintf(f, "%s%s\n", failPrefix, t.cStr());
        }
    }
    fclose(f);
}

// A line's trailing "{test:<id>}" tag: its id (out), the line without it (returned).
[[nodiscard]] za::String withoutTag(const za::String& l, za::String& id)
{
    id.clear();
    if(l.empty() || l.back() != '}')
    {
        return l;
    }
    const size_t open = l.rfind('{');
    if(open == za::StringView::nPos || l.size() - open < 7 || strncmp(l.cStr() + open, "{test:", 6) != 0)
    {
        return l;
    }
    id = trimmed(l.substrByPosLen(open + 6, l.size() - open - 7));
    return trimmed(l.substrByPosLen(0, open));
}

void readList()
{
    state.sections.clear();
    state.texts.clear();
    state.scenarios.clear();
    state.shown.clear();
    state.section.clear();
    state.firstLine.clear();
    state.lines.clear();

    za::Vector<za::String> lines;
    fileFound = readLines(listPath(), lines);
    int current = -1;
    for(const za::String& l : lines)
    {
        if(l.empty() || l[0] == '#' || (l.size() >= 2 && l[0] == '/' && l[1] == '/'))
        {
            continue;
        }
        if(l.size() >= 2 && l.front() == '[' && l.back() == ']')
        {
            state.sections.pushBack(drawable(trimmed(l.substrByPosLen(1, l.size() - 2))));
            current = static_cast<int>(state.sections.size()) - 1;
            continue;
        }
        za::String id;
        za::String text = withoutTag(l, id);
        state.shown.pushBack(drawable(text));
        state.texts.pushBack(ZA_MOVE(text));
        state.scenarios.pushBack(ZA_MOVE(id));
        state.section.pushBack(current);
        state.firstLine.pushBack(static_cast<int>(state.lines.size()));
        wrap(state.shown.back(), state.lines);
    }
    state.firstLine.pushBack(static_cast<int>(state.lines.size())); // (the end of the last item's)
    countOpen();
    state.sectionDefaults.clear();
    for(size_t s = 0; s < state.sections.size(); s++)
    {
        state.sectionDefaults.pushBack(state.sectionTotals[s] > 0 && state.sectionOpens[s] == 0 ? 1 : 0);
    }
    gen++;
}

} // namespace

void refresh(bool force)
{
    if(!ticksRead)
    {
        readTicks();
        readFolds();
        force = true;
    }
    if(!force && realtime - lastLook < 1.0 && realtime >= lastLook)
    {
        return;
    }
    lastLook = realtime;

    const za::I64 t = files::lastWriteTime(listPath().cStr());
    const bool found = t != 0;
    if(force || found != fileFound || (found && t != fileTime))
    {
        fileTime = t;
        readList();
    }
}

int generation()
{
    return gen;
}

int itemCount()
{
    return static_cast<int>(state.texts.size());
}

int openCount()
{
    return open;
}

bool loaded()
{
    return fileFound;
}

int sectionOf(int item)
{
    return item >= 0 && item < itemCount() ? state.section[item] : -1;
}

const char* sectionName(int section)
{
    return section >= 0 && section < static_cast<int>(state.sections.size()) ? state.sections[section].cStr() : "";
}

int sectionCount()
{
    return static_cast<int>(state.sections.size());
}

int sectionOpen(int section)
{
    return section >= 0 && section < sectionCount() ? state.sectionOpens[section] : 0;
}

int sectionTotal(int section)
{
    return section >= 0 && section < sectionCount() ? state.sectionTotals[section] : 0;
}

bool collapsed(int section)
{
    if(section < 0 || section >= sectionCount())
    {
        return false;
    }
    const za::String& name = state.sections[section];
    return state.collapsedNames.count(name) ? true : state.expandedNames.count(name) ? false : state.sectionDefaults[section] != 0;
}

void setCollapsed(int section, bool on)
{
    if(section >= sectionCount())
    {
        return;
    }
    for(int s = section < 0 ? 0 : section; s < (section < 0 ? sectionCount() : section + 1); s++)
    {
        const za::String& name = state.sections[s];
        state.collapsedNames.erase(name);
        state.expandedNames.erase(name);
        (on ? state.collapsedNames : state.expandedNames).insert(name);
    }
    writeFolds();
    gen++;
}

void clearCollapseChoices()
{
    state.collapsedNames.clear();
    state.expandedNames.clear();
    writeFolds();
    gen++;
}

int mark(int item)
{
    return item >= 0 && item < itemCount() ? markOf(state.texts[item]) : Untested;
}

bool ticked(int item)
{
    return mark(item) == Passed;
}

int failedCount()
{
    return failed;
}

const char* scenario(int item)
{
    return item >= 0 && item < itemCount() ? state.scenarios[item].cStr() : "";
}

const char* text(int item)
{
    return item >= 0 && item < itemCount() ? state.texts[item].cStr() : "";
}

void press(int item, int m)
{
    if(item < 0 || item >= itemCount() || (m != Passed && m != Failed))
    {
        return;
    }
    const za::String& t = state.texts[item];
    const int was = markOf(t);
    setMarkOf(t, was == m ? Untested : m);
    if(static_cast<int>(state.undoTexts.size()) >= undoDepth)
    {
        state.undoTexts.erase(state.undoTexts.begin());
        state.undoWasTicked.erase(state.undoWasTicked.begin());
    }
    state.undoTexts.pushBack(t);
    state.undoWasTicked.pushBack(was);
    writeTicks();
    countOpen();
    gen++;
}

void toggle(int item)
{
    press(item, Passed);
}

int undoCount()
{
    return static_cast<int>(state.undoTexts.size());
}

const char* undoText()
{
    return state.undoTexts.empty() ? "" : state.undoTexts.back().cStr();
}

int undoRestores()
{
    return state.undoWasTicked.empty() ? Untested : state.undoWasTicked.back();
}

int undo()
{
    if(state.undoTexts.empty())
    {
        return -1;
    }
    const za::String t = state.undoTexts.back();
    const int was = state.undoWasTicked.back();
    state.undoTexts.popBack();
    state.undoWasTicked.popBack();
    setMarkOf(t, was);
    writeTicks();
    countOpen();
    gen++;
    for(int i = 0; i < itemCount(); i++)
    {
        if(state.texts[i] == t)
        {
            return i;
        }
    }
    return -2; // (no longer on the list: its tick restored all the same)
}

int lineCount(int item)
{
    return item >= 0 && item < itemCount() ? state.firstLine[item + 1] - state.firstLine[item] : 0;
}

const char* line(int item, int l)
{
    if(l < 0 || l >= lineCount(item))
    {
        return "";
    }
    const auto [start, len] = state.lines[state.firstLine[item] + l];
    const int m = mark(item);
    const char* prefix = l > 0 ? "    " : m == Passed ? "[x] " : m == Failed ? "[!] " : "[ ] ";
    q_snprintf(lineText, sizeof(lineText), "%s%.*s", prefix, len, state.shown[item].cStr() + start);
    return lineText;
}

void command_f()
{
    if(Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "reload"))
    {
        refresh(true);
    }
    else if(Cmd_Argc() >= 3 && !q_strcasecmp(Cmd_Argv(1), "tick"))
    {
        refresh();
        toggle(Q_atoi(Cmd_Argv(2)));
    }
    else if(Cmd_Argc() >= 3 && (!q_strcasecmp(Cmd_Argv(1), "pass") || !q_strcasecmp(Cmd_Argv(1), "fail")))
    {
        refresh();
        press(Q_atoi(Cmd_Argv(2)), !q_strcasecmp(Cmd_Argv(1), "pass") ? Passed : Failed);
    }
    else if(Cmd_Argc() >= 3 && !q_strcasecmp(Cmd_Argv(1), "test"))
    {
        refresh();
        const int item = Q_atoi(Cmd_Argv(2));
        if(!*scenario(item))
        {
            Con_Printf("vr_checklist: item %d has no {test:...} scenario\n", item);
            return;
        }
        scenario::start(scenario(item), text(item));
        return;
    }
    else if(Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "undo"))
    {
        refresh();
        const int item = undo();
        Con_Printf("CLUNDO|%d|%d left\n", item, undoCount());
    }
    else if(Cmd_Argc() >= 3 && (!q_strcasecmp(Cmd_Argv(1), "collapse") || !q_strcasecmp(Cmd_Argv(1), "expand")))
    {
        refresh();
        setCollapsed(!q_strcasecmp(Cmd_Argv(2), "all") ? -1 : Q_atoi(Cmd_Argv(2)), !q_strcasecmp(Cmd_Argv(1), "collapse"));
    }
    else if(Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "folds_default"))
    {
        refresh();
        clearCollapseChoices();
    }
    else if(Cmd_Argc() >= 2)
    {
        Con_Printf("vr_checklist [reload | tick <n> | pass <n> | fail <n> | test <n> | undo | collapse <s|all> | expand "
                   "<s|all> | folds_default]: the checklist (quakevr/checklist.txt), read again, item n ticked or "
                   "unticked, passed or failed (again: back to untested), its scenario run, the last press undone, "
                   "section s folded or unfolded in the menu, or every fold back to its default\n");
        return;
    }
    else
    {
        refresh();
    }
    Con_Printf("CLSUM|%s|%d open|%d items|%d sections|%d failed\n", fileFound ? "found" : "missing", open, itemCount(),
        static_cast<int>(state.sections.size()), failed);
    for(int s = 0; s < sectionCount(); s++)
    {
        Con_Printf("CLSEC|%d|%d|%d|%s|%s\n", s, sectionOpen(s), sectionTotal(s), collapsed(s) ? "collapsed" : "expanded",
            sectionName(s));
    }
    for(int i = 0; i < itemCount(); i++)
    {
        const int m = mark(i);
        Con_Printf("CLITEM|%d|%s|%s|%s|%s\n", i, m == Passed ? "x" : m == Failed ? "!" : " ", sectionName(sectionOf(i)),
            state.shown[i].cStr(), state.scenarios[i].cStr());
    }
}

// The item last hovered or picked in the menu, for a voice note's context (vr_notectx.cpp): by its text (the list may
// change before the note).
za::String hoveredItemText;
za::String hoveredItemSection;
double hoveredItemAt = -1.0;

void hovered(int item)
{
    if(item < 0 || item >= itemCount())
    {
        return;
    }
    hoveredItemAt = realtime;
    if(hoveredItemText != state.texts[item])
    {
        hoveredItemText = state.texts[item];
        hoveredItemSection = sectionName(sectionOf(item));
    }
}

bool lastHovered(za::String& text, za::String& section, int& item, double& when)
{
    if(hoveredItemAt < 0.0)
    {
        return false;
    }
    text = hoveredItemText;
    section = hoveredItemSection;
    when = hoveredItemAt;
    item = -1;
    for(int i = 0; i < itemCount(); i++)
    {
        if(state.texts[i] == hoveredItemText)
        {
            item = i;
            break;
        }
    }
    return true;
}

} // namespace qvr::checklist
