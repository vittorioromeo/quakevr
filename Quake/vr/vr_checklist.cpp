// vr_checklist.cpp -- the playtest checklist (vr_checklist.hpp): quakevr/checklist.txt read at runtime ("[Section]"
// lines, one item a line, '#' or "//" comments), its ticks in quakevr/checklist_ticks.txt (one ticked item's text a
// line).

#include "vr_checklist.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_mem.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
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
    za::Vector<za::String> texts;                // each item's text as in the file (its key in the ticks)
    za::Vector<za::String> shown;                // as drawn: characters the menu's font lacks replaced
    za::Vector<int> section;                      // each item's (-1: none)
    za::Vector<int> firstLine;                    // each item's first wrapped line in `lines`
    za::Vector<qza::Pair<int, int>> lines;        // start and length in its `shown` text
    ankerl::unordered_dense::set<za::String> tickedTexts;   // every ticked item's text, those no longer listed too
    auto members() { return qvr::mem::list(sections, texts, shown, section, firstLine, lines, tickedTexts); }
};
mem::Cache<State> state{"checklist", mem::Never};

bool ticksRead = false;
bool fileFound = false;
int gen = 0;
int open = 0;
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
void wrap(const za::String& text, za::Vector<qza::Pair<int, int>>& out)
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
    for(const za::String& t : state.texts)
    {
        open += state.tickedTexts.count(t) ? 0 : 1;
    }
}

void readTicks()
{
    ticksRead = true;
    state.tickedTexts.clear();
    za::Vector<za::String> lines;
    if(readLines(ticksPath(), lines))
    {
        for(za::String& l : lines)
        {
            if(!l.empty())
            {
                state.tickedTexts.insert(ZA_MOVE(l));
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
    fclose(f);
}

void readList()
{
    state.sections.clear();
    state.texts.clear();
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
        state.texts.pushBack(l);
        state.shown.pushBack(drawable(l));
        state.section.pushBack(current);
        state.firstLine.pushBack(static_cast<int>(state.lines.size()));
        wrap(state.shown.back(), state.lines);
    }
    state.firstLine.pushBack(static_cast<int>(state.lines.size())); // (the end of the last item's)
    countOpen();
    gen++;
}

} // namespace

void refresh(bool force)
{
    if(!ticksRead)
    {
        readTicks();
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

bool ticked(int item)
{
    return item >= 0 && item < itemCount() && state.tickedTexts.count(state.texts[item]) != 0;
}

void toggle(int item)
{
    if(item < 0 || item >= itemCount())
    {
        return;
    }
    const za::String& t = state.texts[item];
    if(!state.tickedTexts.erase(t))
    {
        state.tickedTexts.insert(t);
    }
    writeTicks();
    countOpen();
    gen++;
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
    const char* prefix = l > 0 ? "    " : ticked(item) ? "[x] " : "[ ] ";
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
    else if(Cmd_Argc() >= 2)
    {
        Con_Printf("vr_checklist [reload | tick <n>]: the checklist (quakevr/checklist.txt), read again, or item n ticked "
                   "or unticked\n");
        return;
    }
    else
    {
        refresh();
    }
    Con_Printf("CLSUM|%s|%d open|%d items|%d sections\n", fileFound ? "found" : "missing", open, itemCount(),
        static_cast<int>(state.sections.size()));
    for(int i = 0; i < itemCount(); i++)
    {
        Con_Printf("CLITEM|%d|%s|%s|%s\n", i, ticked(i) ? "x" : " ", sectionName(sectionOf(i)), state.shown[i].cStr());
    }
}

} // namespace qvr::checklist
