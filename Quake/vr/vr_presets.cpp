// vr_presets.cpp -- the preset machinery the Graphics, Gore and Gameplay presets share (vr_presets.hpp).

#include "vr_presets.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/String/StringView.hpp"

#include <stdlib.h>
#include <string.h>

namespace qvr::presetcore
{

bool same(const char* a, const char* b)
{
    char* endA = nullptr;
    char* endB = nullptr;
    const double x = strtod(a, &endA);
    const double y = strtod(b, &endB);
    if(endA != a && *endA == '\0' && endB != b && *endB == '\0')
    {
        return static_cast<float>(x) == static_cast<float>(y);
    }
    return strcmp(a, b) == 0;
}

const char* Kind::levelName(int level) const
{
    return t.levelNames[za::clamp(level, 0, t.levelCount - 1)];
}

const char* Kind::target(const Row& row, int level) const
{
    const char* v = row.values[za::clamp(level, 0, t.levelCount - 1)];
    return v ? v : row.var->default_string ? row.var->default_string : "0";
}

const Row* Kind::findRow(const char* name) const
{
    for(int i = 0; i < t.rowCount; i++)
    {
        if(!strcmp(name, t.rows[i].var->name))
        {
            return &t.rows[i];
        }
    }
    return nullptr;
}

za::String Kind::minePath() const
{
    return za::String{com_gamedir} + "/" + t.what + (testFile ? "_my_settings_test.cfg" : "_my_settings.cfg");
}

void Kind::useTestFile(bool on)
{
    testFile = on;
    mineLoaded = false;
    if(on)
    {
        files::remove(minePath().cStr());
    }
}

// My Settings: a row's `name "value"` a line. Read the first time it is asked for in a game folder; written whole
// through a temporary file.
void Kind::loadMine()
{
    const za::String path = minePath();
    if(mineLoaded && mineFrom == za::StringView{path.cStr()})
    {
        return;
    }
    mineLoaded = true;
    minePresent = false;
    mineFrom = path;
    mine.clear();
    za::String text;
    if(!files::readText(path.cStr(), text))
    {
        return;
    }
    files::forLines(za::StringView{text.cStr()}, [this](za::StringView ln) {
        za::SizeT i = 0;
        while(i < ln.size() && static_cast<unsigned char>(ln[i]) <= ' ')
        {
            i++;
        }
        za::SizeT j = i;
        while(j < ln.size() && static_cast<unsigned char>(ln[j]) > ' ')
        {
            j++;
        }
        const za::String name{ln.substrByPosLen(i, j - i)};
        if(name.empty() || ln.substrByPosLen(i, ln.size() - i).startsWith(za::StringView{"//"}) || !findRow(name.cStr()))
        {
            return;
        }
        const za::SizeT open = ln.find('"', j);
        const za::SizeT close = open == za::StringView::nPos ? za::StringView::nPos : ln.find('"', open + 1);
        if(close == za::StringView::nPos)
        {
            return;
        }
        mine.pushBack(MineEntry{name, za::String{ln.substrByPosLen(open + 1, close - open - 1)}});
    });
    minePresent = !mine.empty();
}

void Kind::writeMine()
{
    za::String text{"// Quake VR: your own "};
    text += t.what;
    text += " settings (";
    text += t.menuPath;
    text += " > My Settings), kept the first time a preset was applied or by Save as My Settings.\n";
    for(const MineEntry& e : mine)
    {
        text += e.name;
        text += " \"";
        text += e.value;
        text += "\"\n";
    }
    const za::String path = minePath();
    const za::String tmp = path + ".tmp";
    if(!files::writeText(tmp.cStr(), za::StringView{text.cStr()}) || !files::rename(tmp.cStr(), path.cStr()))
    {
        files::remove(tmp.cStr());
        Con_Printf("VR: could not write %s\n", path.cStr());
    }
}

// The current values of every row kept as My Settings, and written.
void Kind::captureMine()
{
    loadMine();
    mine.clear();
    for(int i = 0; i < t.rowCount; i++)
    {
        mine.pushBack(MineEntry{za::String{t.rows[i].var->name}, za::String{t.rows[i].var->string}});
    }
    minePresent = true;
    writeMine();
}

void Kind::apply(int level)
{
    level = za::clamp(level, 0, t.levelCount - 1);
    loadMine();
    if(!minePresent)
    {
        captureMine(); // your own settings, before the first preset changes them
        Con_DPrintf("%s preset: your settings kept as My Settings (%s)\n", t.what, minePath().cStr());
    }
    for(int i = 0; i < t.rowCount; i++)
    {
        Cvar_SetQuick(t.rows[i].var, target(t.rows[i], level));
    }
}

bool Kind::restoreMine()
{
    loadMine();
    if(!minePresent)
    {
        return false;
    }
    for(const MineEntry& e : mine)
    {
        if(const Row* row = findRow(e.name.cStr()))
        {
            Cvar_SetQuick(row->var, e.value.cStr());
        }
    }
    return true;
}

void Kind::saveMine()
{
    captureMine();
}

bool Kind::hasMine()
{
    loadMine();
    return minePresent;
}

int Kind::differing(int level, bool print) const
{
    int n = 0;
    for(int i = 0; i < t.rowCount; i++)
    {
        const Row& row = t.rows[i];
        const char* want = target(row, level);
        if(!same(row.var->string, want))
        {
            n++;
            if(print)
            {
                Con_Printf("    %s %s (%s: %s)\n", row.var->name, row.var->string, levelName(level), want);
            }
        }
    }
    return n;
}

int Kind::matchingLevel() const
{
    // (every menu frame, for each kind: a level is left at its first row that differs, not counted through)
    for(int level = 0; level < t.levelCount; level++)
    {
        bool all = true;
        for(int i = 0; i < t.rowCount && all; i++)
        {
            all = same(t.rows[i].var->string, target(t.rows[i], level));
        }
        if(all)
        {
            return level;
        }
    }
    return -1;
}

bool Kind::matchesMine()
{
    loadMine();
    if(!minePresent)
    {
        return false;
    }
    for(const MineEntry& e : mine)
    {
        const Row* row = findRow(e.name.cStr());
        if(row && !same(row->var->string, e.value.cStr()))
        {
            return false;
        }
    }
    return true;
}

const char* Kind::stateName()
{
    const int level = matchingLevel();
    const bool isMine = matchesMine();
    if(level >= 0)
    {
        return isMine ? va("%s (My Settings)", levelName(level)) : levelName(level);
    }
    return isMine ? "My Settings" : "Custom";
}

void Kind::printStatus()
{
    Con_Printf("%s preset: %s (rows %d, My Settings %s)\n", t.what, stateName(), t.rowCount, hasMine() ? "kept" : "none");
    int nearest = 0;
    int nearestDiff = 1 << 30;
    for(int level = 0; level < t.levelCount; level++)
    {
        if(const int diff = differing(level, false); diff < nearestDiff)
        {
            nearestDiff = diff;
            nearest = level;
        }
    }
    if(nearestDiff == 0)
    {
        return;
    }
    Con_Printf("  nearest %s, %d differ:\n", levelName(nearest), nearestDiff);
    (void)differing(nearest, true);
}

void Kind::setNote(const char* text)
{
    q_strlcpy(note, text, sizeof(note));
    noteTime = realtime;
}

const char* Kind::menuStateLine()
{
    q_snprintf(line, sizeof(line), "Now: %s", stateName());
    return line;
}

const char* Kind::menuNoteLine() const
{
    return realtime - noteTime > 5.0 ? "" : note;
}

void Kind::menuApply(int level)
{
    const bool first = !hasMine();
    apply(level);
    setNote(first ? va("%s set. Your own settings were kept as My Settings.", levelName(level))
                  : va("%s set.", levelName(level)));
}

void Kind::menuRestoreMine()
{
    setNote(restoreMine() ? "My Settings set back."
                          : "None kept yet: they are kept the first time you pick a preset, or with Save as My Settings.");
}

void Kind::menuSaveMine()
{
    if(hasMine() && realtime - saveArmedTime > 3.0)
    {
        saveArmedTime = realtime;
        setNote("Press Save as My Settings again to replace the ones kept.");
        return;
    }
    saveArmedTime = -100.0;
    saveMine();
    setNote("The settings as they are now kept as My Settings.");
}

} // namespace qvr::presetcore
