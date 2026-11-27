// vr_menu_paths.inc -- menu paths (menu::pathTo, expandPaths, vr_menu_path_check), vr_menu_dump and vr_menu_press.
// Split from vr_menu.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

bool qvr::menu::pathTo(za::StringView spec, za::String& out)
{
    return resolvePath(spec, out);
}

za::String qvr::menu::expandPaths(za::StringView text, int width, int* missing)
{
    constexpr za::StringView open = "{menu:";
    za::String out;
    za::SizeT at = 0;
    for(za::SizeT start; (start = text.find(open, at)) != za::StringView::nPos;)
    {
        const za::SizeT end = text.find('}', start);
        if(end == za::StringView::nPos)
        {
            break;
        }
        out.append(text.substrByPosLen(at, start - at));
        const za::StringView spec = text.substrByPosLen(start + open.size(), end - start - open.size());
        za::String path;
        if(!resolvePath(spec, path))
        {
            if(missing)
            {
                (*missing)++;
            }
            Con_Warning("MENU PATH MISSING: {menu:%.*s} (a map's board names a menu page or row that no longer exists)\n",
                static_cast<int>(spec.size()), spec.data());
            out += "[menu? ";
            out.append(spec);
            out += "]";
            at = end + 1;
            continue;
        }
        // Broken into lines at its " > "s, each about `width` characters from the start of its line.
        const za::SizeT lineStart = out.rfind('\n');
        int column = static_cast<int>(out.size() - (lineStart == za::StringView::nPos ? 0 : lineStart + 1));
        za::StringView rest = path;
        for(bool first = true; !rest.empty(); first = false)
        {
            const za::SizeT sep = rest.find(" > ");
            const za::StringView part = rest.substrByPosLen(0, sep);
            rest = sep == za::StringView::nPos ? za::StringView{} : rest.substrByPosLen(sep + 3);
            const int need = static_cast<int>(part.size()) + (first ? 0 : 3);
            if(!first && column + need > width)
            {
                out += "\n> ";
                column = 2;
            }
            else if(!first)
            {
                out += " > ";
                column += 3;
            }
            out.append(part);
            column += static_cast<int>(part.size());
        }
        at = end + 1;
    }
    out.append(text.substrByPosLen(at));
    return out;
}

void qvr::menu::forEachSettingPath(za::FunctionRef<void(const cvar_t& var, const char* page, const char* label)> f)
{
    buildSearchIndex(); // (Search's index: every page under the VR Settings and the Advanced VR Options, breadth first)
    for(const SearchEntry& e : search.index)
    {
        if(e.cvar && !e.isPage)
        {
            f(*e.cvar, e.path.cStr(), e.label.cStr());
        }
    }
}

void qvr::menu::forEachSettingRange(za::FunctionRef<void(const cvar_t& var, float lo, float hi)> f)
{
    buildSearchIndex();
    for(const SearchEntry& e : search.index)
    {
        if(e.cvar && !e.isPage && e.ranged)
        {
            f(*e.cvar, e.lo, e.hi);
        }
    }
}

// vr_menu_path_check [file or text]: every {menu:...} in the loaded map's entities (or in a text file of the game's, as
// maps/vrcalibration.map; or in the text given, starting with "{"), each with its path; "menu paths: N found, M missing"
// last.
static void pathsFileCheck(const char* name);

void qvr::menu::pathCheck_f()
{
    const char* data = nullptr;
    byte* file = nullptr;
    if(Cmd_Argc() > 1 && Cmd_Argv(1)[0] == '{')
    {
        data = Cmd_Args(); // the text itself: vr_menu_path_check "{menu:Locomotion>Lean}"
    }
    else if(Cmd_Argc() > 1 && za::StringView{Cmd_Argv(1)}.endsWith("menu_paths.txt"))
    {
        pathsFileCheck(Cmd_Argv(1));
        return;
    }
    else if(Cmd_Argc() > 1)
    {
        file = COM_LoadMallocFile(Cmd_Argv(1), nullptr);
        if(!file)
        {
            Con_Printf("vr_menu_path_check: can't open %s\n", Cmd_Argv(1));
            return;
        }
        data = reinterpret_cast<const char*>(file);
    }
    else if(sv.active && sv.worldmodel && sv.worldmodel->entities)
    {
        data = sv.worldmodel->entities;
    }
    else
    {
        Con_Printf("vr_menu_path_check [file or {menu:...}]: the loaded map's {menu:...} board paths, or a file's\n");
        return;
    }
    int found = 0, missing = 0;
    const za::StringView text{data};
    for(za::SizeT at = 0; (at = text.find("{menu:", at)) != za::StringView::nPos;)
    {
        const za::SizeT end = text.find('}', at);
        if(end == za::StringView::nPos)
        {
            break;
        }
        const za::StringView spec = text.substrByPosLen(at + 6, end - at - 6);
        za::String path;
        if(resolvePath(spec, path))
        {
            found++;
            Con_Printf("menu path: %.*s -> %s\n", static_cast<int>(spec.size()), spec.data(), path.cStr());
        }
        else
        {
            missing++;
            Con_Printf("MENU PATH MISSING: %.*s\n", static_cast<int>(spec.size()), spec.data());
        }
        at = end + 1;
    }
    Con_Printf("menu paths: %d found, %d missing\n", found, missing);
    VR_HeapFree(file);
}

// vr_menu_dump [level 0-2]: every page shown at that Menu Detail (default 0, Standard), its rows' labels, choices and help,
// as "MD|" lines in the console (the copy review: typos, wording, rows in the wrong level).
void qvr::menu::dump_f()
{
    const int level = CLAMP(0, Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 0, LevelCount - 1);
    const bool full = Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(2), "full");
    const int was = levelOverride;
    levelOverride = level;
    for(int p = 0; p < pageCount; p++)
    {
        if(pages[p].level > level)
        {
            continue;
        }
        Con_Printf("MD|PAGE|%d|%s\n", p, pages[p].title);
        for(const Item& item : items(p))
        {
            const char* kind = item.kind == Item::Header ? "HEAD" : item.kind == Item::Slider ? "SLID" :
                item.kind == Item::Cycle ? "CYCL" : item.kind == Item::Action ? "ACT" : item.kind == Item::Key ? "KEY" : "INFO";
            za::String choices;
            for(const Choice& c : item.choices)
            {
                choices += c.label;
                choices += " / ";
            }
            const char* label = item.label && *item.label ? item.label
                                : item.infoArg            ? item.infoArg(item.arg) // (a list's row: its text)
                                : item.info               ? item.info()
                                                          : "";
            if(!full)
            {
                Con_Printf("MD|%s|%s|%s|%s\n", kind, label ? label : "", choices.cStr(), item.helpText ? item.helpText : "");
                continue;
            }
            za::String values;
            for(const Choice& c : item.choices)
            {
                values += va("%g ", c.value);
            }
            Con_Printf("MD|%s|%s|%s|%s|%s|cvar %s|range %g %g step %g fmt %s|ext %d %g %g|neg %s %g|lvl %d|unav %d|page %d|cmd %s"
                       "|act %d|wide %d|part %d\n",
                kind, label ? label : "", choices.cStr(), values.cStr(), item.helpText ? item.helpText : "",
                item.cvar ? item.cvar->name : "-", item.min, item.max, item.step, item.format ? item.format : "-",
                item.extendable ? 1 : 0, item.hardMin, item.hardMax, item.negativeLabel ? item.negativeLabel : "-",
                item.negativeStart, item.level, item.unavailable ? 1 : 0, item.page, item.command ? item.command : "-",
                item.action || item.actionArg ? 1 : 0, item.wide ? 1 : 0, item.partOf);
        }
    }
    levelOverride = was;
}

// The weapon whose rows stand for the Held Weapon pages in the dump (Weapon Offsets, Weights: built for a weapon in the
// hand, so with none held they are only a "hold a weapon" line): the 17th slot, its cvars end _17.
static constexpr int dumpPathsWeaponSlot = 16;

// Search's index as the dump's rows (without "MP|"), every Held Weapon page included whatever the hands hold.
static za::Vector<za::String> pathRows()
{
    pinSlotPages(dumpPathsWeaponSlot); // (so a full regeneration lists the Held Weapon pages whatever the hands hold)
    buildSearchIndex();
    pinSlotPages(-1); // (the pages built for the hands again)
    za::Vector<za::String> rows;
    for(const SearchEntry& e : search.index)
    {
        const za::String full = e.path.empty() ? e.label : e.path + " > " + e.label;
        rows.pushBack(za::String{va("%s%s|%d|%s|%s|%s", e.isPage ? "@" : "", e.isPage ? "page" : e.cvar ? e.cvar->name : "",
            e.level, e.page >= 0 ? pages[e.page].title : "", e.isPage ? "" : e.label.cStr(), full.cStr())});
    }
    return rows;
}

void qvr::menu::dumpPaths_f()
{
    const za::Vector<za::String> rows = pathRows();
    for(const za::String& r : rows)
    {
        Con_Printf("MP|%s\n", r.cStr());
    }
    Con_Printf("menu paths dumped: %d\n", static_cast<int>(rows.size()));
}

// Whether `v` has `r`: searched from `hint` on (two lists in the same order find each row where the last was).
static bool hasRow(const za::Vector<za::String>& v, const za::String& r, za::SizeT& hint)
{
    for(za::SizeT n = 0; n < v.size(); n++)
    {
        const za::SizeT i = (hint + n) % v.size();
        if(v[i] == r)
        {
            hint = i + 1;
            return true;
        }
    }
    return false;
}

// vr_menu_path_check <...>/menu_paths.txt: the committed file against a regeneration (the rows it lacks, the rows it
// has that the menus no longer do): "menu_paths.txt: current" or "MENU PATHS FILE STALE" (regenerate it: menu_paths.py).
static void pathsFileCheck(const char* name)
{
    FILE* f = fopen(name, "rb");
    if(!f)
    {
        Con_Printf("MENU PATHS FILE STALE: can't open %s\n", name);
        return;
    }
    za::String text;
    char buf[4096];
    for(size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;)
    {
        text.append(za::StringView{buf, static_cast<za::SizeT>(n)});
    }
    fclose(f);
    za::Vector<za::String> have;
    for(za::SizeT at = 0; at < text.size();)
    {
        za::SizeT end = text.find('\n', at);
        end = end == za::StringView::nPos ? text.size() : end;
        za::SizeT len = end - at;
        if(len > 0 && text[at + len - 1] == '\r')
        {
            len--;
        }
        if(len > 0 && text[at] != '#')
        {
            have.pushBack(za::String{za::StringView{text}.substrByPosLen(at, len)});
        }
        at = end + 1;
    }
    const za::Vector<za::String> now = pathRows();
    int lacks = 0, extra = 0;
    za::SizeT hint = 0;
    for(const za::String& r : now)
    {
        if(!hasRow(have, r, hint) && lacks++ < 5)
        {
            Con_Printf("MENU PATHS FILE LACKS: %s\n", r.cStr());
        }
    }
    hint = 0;
    for(const za::String& r : have)
    {
        if(!hasRow(now, r, hint) && extra++ < 5)
        {
            Con_Printf("MENU PATHS FILE HAS (gone): %s\n", r.cStr());
        }
    }
    if(lacks || extra)
    {
        Con_Printf("MENU PATHS FILE STALE: %d rows missing, %d gone (regenerate: Misc/quakevr/menu_paths.py)\n", lacks, extra);
    }
    else
    {
        Con_Printf("menu_paths.txt: current (%d rows)\n", static_cast<int>(now.size()));
    }
}

namespace
{

// A text as a JSON string (quotes, backslashes and control characters escaped; other bytes as they are).
[[nodiscard]] za::String jsonText(const char* text)
{
    za::String out{"\""};
    for(const char* c = text ? text : ""; *c; c++)
    {
        const unsigned char u = static_cast<unsigned char>(*c);
        if(u == '"' || u == '\\')
        {
            out += '\\';
            out += *c;
        }
        else if(u == '\n')
        {
            out += "\\n";
        }
        else if(u < 0x20)
        {
            out += va("\\u%04x", u);
        }
        else
        {
            out += *c;
        }
    }
    out += "\"";
    return out;
}

// One setting for Setup's settings page (Misc/installer/settings_page.py): its cvar, then its row's (the first in
// Search's order: the shallowest), each field an "MS|<cvar>|<key>|<JSON value>" line (a long help stays under the
// console's line length).
void dumpSetting(const char* name)
{
    const cvar_t* var = Cvar_FindVar(name);
    if(!var)
    {
        Con_Printf("MS|%s|exists|false\n", name);
        return;
    }
    Con_Printf("MS|%s|exists|true\n", name);
    Con_Printf("MS|%s|archive|%s\n", name, (var->flags & CVAR_ARCHIVE) ? "true" : "false");
    if(var == &vr_graphics_preset)
    {
        // Not a saved setting: a button (applies a group of settings once). Its choices are the Graphics Presets
        // page's buttons, its help the Graphics page's link to them; its default the shipped level.
        Con_Printf("MS|%s|kind|\"apply\"\n", name);
        Con_Printf("MS|%s|default|\"%d\"\n", name, gfxpresets::levelHigh);
        for(const SearchEntry& e : search.index)
        {
            if(e.isPage && e.page == pageIndex(pageGraphicsPresets))
            {
                Con_Printf("MS|%s|path|%s\n", name, jsonText((e.path.empty() ? e.label : e.path + " > " + e.label).cStr()).cStr());
                Con_Printf("MS|%s|label|%s\n", name, jsonText(e.label.cStr()).cStr());
                break;
            }
        }
        for(const Item& item : items(pageIndex(pageGraphics)))
        {
            if(item.kind == Item::Action && item.page == pageIndex(pageGraphicsPresets) && item.helpText)
            {
                Con_Printf("MS|%s|help|%s\n", name, jsonText(item.helpText).cStr());
            }
        }
        for(int level = 0; level < gfxpresets::levelCount; level++)
        {
            const char* levelLabel = gfxpresets::levelName(level);
            for(const Item& item : items(pageIndex(pageGraphicsPresets)))
            {
                if(item.kind == Item::Action && item.label && za::StringView{item.label}.startsWith(za::StringView{levelLabel}))
                {
                    Con_Printf("MS|%s|choice|%d|%s\n", name, level, jsonText(item.label).cStr());
                    Con_Printf("MS|%s|choicehelp|%d|%s\n", name, level, jsonText(item.helpText).cStr());
                    break;
                }
            }
        }
        for(const Item& item : items(pageIndex(pageGraphicsPresets)))
        {
            if(item.kind == Item::Action && item.label && !strcmp(item.label, "Standalone"))
            {
                Con_Printf("MS|%s|choice|%d|%s\n", name, gfxpresets::levelStandalone, jsonText(item.label).cStr());
                Con_Printf("MS|%s|choicehelp|%d|%s\n", name, gfxpresets::levelStandalone, jsonText(item.helpText).cStr());
                break;
            }
        }
        return;
    }
    // A preset (Handedness): a button too; its choices are the ones that set something, its default the one the
    // settings' defaults match.
    const Preset* preset = nullptr;
    for(const Preset& p : presets)
    {
        preset = p.choice == var ? &p : preset;
    }
    const PresetChoice* presetDefault = nullptr;
    for(int i = 0; preset && i < preset->count && !presetDefault; i++)
    {
        bool all = true;
        for(const PresetSetting& s : preset->choices[i].settings)
        {
            const cvar_t* setting = Cvar_FindVar(s.cvar);
            all = all && setting && setting->default_string && Q_atof(setting->default_string) == s.value;
        }
        presetDefault = all ? &preset->choices[i] : nullptr;
    }
    if(preset)
    {
        Con_Printf("MS|%s|default|\"%g\"\n", name, presetDefault ? presetDefault->value : var->value);
    }
    else
    {
        Con_Printf("MS|%s|default|%s\n", name, jsonText(var->default_string).cStr());
    }
    for(const SearchEntry& e : search.index)
    {
        if(e.isPage || e.cvar != var)
        {
            continue;
        }
        for(const Item& item : items(e.page))
        {
            if(item.cvar != var || (item.kind != Item::Slider && item.kind != Item::Cycle))
            {
                continue;
            }
            const bool toggle = item.kind == Item::Cycle && item.choices.size() == 2 && item.choices[0].value == 0.f &&
                                item.choices[1].value == 1.f && !q_strcasecmp(item.choices[0].label, "Off") &&
                                !q_strcasecmp(item.choices[1].label, "On");
            Con_Printf("MS|%s|kind|\"%s\"\n", name,
                preset ? "apply" : item.kind == Item::Slider ? "slider" : toggle ? "toggle" : "choice");
            Con_Printf("MS|%s|label|%s\n", name, jsonText(e.label.cStr()).cStr());
            Con_Printf("MS|%s|path|%s\n", name, jsonText((e.path.empty() ? e.label : e.path + " > " + e.label).cStr()).cStr());
            Con_Printf("MS|%s|level|%d\n", name, e.level);
            if(item.helpText)
            {
                Con_Printf("MS|%s|help|%s\n", name, jsonText(item.helpText).cStr());
            }
            if(item.kind == Item::Slider)
            {
                Con_Printf("MS|%s|min|%g\nMS|%s|max|%g\nMS|%s|step|%g\n", name, item.min, name, item.max, name, item.step);
                Con_Printf("MS|%s|format|%s\n", name, jsonText(item.format).cStr());
                if(item.extendable)
                {
                    Con_Printf("MS|%s|hardmin|%g\nMS|%s|hardmax|%g\n", name, item.hardMin, name, item.hardMax);
                }
            }
            else if(!toggle)
            {
                for(const Choice& c : item.choices)
                {
                    bool sets = !preset;
                    for(int i = 0; preset && i < preset->count; i++)
                    {
                        sets = sets || preset->choices[i].value == c.value;
                    }
                    if(sets)
                    {
                        Con_Printf("MS|%s|choice|%g|%s\n", name, c.value, jsonText(c.label).cStr());
                    }
                }
            }
            return;
        }
    }
    Con_Printf("MS|%s|kind|\"none\"\n", name); // no menu row shows it (any more)
}

} // namespace

void qvr::menu::dumpSettings_f()
{
    buildSearchIndex();
    const int was = levelOverride;
    levelOverride = LevelDeveloper;
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        dumpSetting(Cmd_Argv(i));
    }
    levelOverride = was;
    Con_Printf("menu settings dumped: %d\n", Cmd_Argc() - 1);
}

void qvr::menu::press_f()
{
    // (Unquoted, the page and the row split at " > ": a test script's quotes need not survive its shell.)
    char args[512];
    q_strlcpy(args, Cmd_Args() ? Cmd_Args() : "", sizeof(args));
    for(za::SizeT n = strlen(args); n > 0 && static_cast<unsigned char>(args[n - 1]) <= ' '; n--)
    {
        args[n - 1] = '\0'; // (a line's end: a carriage return, a space)
    }
    char* split = strstr(args, " > ");
    if(!split)
    {
        Con_Printf("usage: vr_menu_press <page title> > <row label>\n");
        return;
    }
    *split = '\0';
    const char* title = args;
    const char* label = split + 3;
    const int was = levelOverride;
    levelOverride = LevelDeveloper;
    const char* command = nullptr;
    void (*action)() = nullptr;
    bool found = false;
    bool locked = false;
    for(int p = 0; p < pageCount && !found; p++)
    {
        if(q_strcasecmp(pages[p].title, title))
        {
            continue;
        }
        const za::Vector<Item>& list = items(p);
        for(za::SizeT n = 0; n < list.size(); n++)
        {
            const Item& item = list[n];
            // A row of buttons (the Voice Chat page's players, the Checklist's items): "<its text's start> / <button>",
            // its labels on the row under it.
            const char* slash = strstr(label, " / ");
            if(slash && item.buttonPress && item.infoArg && n + 1 < list.size() && list[n + 1].buttonLabel)
            {
                const za::SizeT len = static_cast<za::SizeT>(slash - label);
                const char* text = item.infoArg(item.arg);
                if(!q_strncasecmp(text, label, static_cast<int>(len)) && (text[len] == 0 || text[len] == ' '))
                {
                    for(int b = 0; b < list[n + 1].buttons; b++)
                    {
                        if(!q_strcasecmp(list[n + 1].buttonLabel(list[n + 1].arg, b), slash + 3))
                        {
                            locked = item.buttonDim && item.buttonDim(item.arg, b);
                            levelOverride = was;
                            if(locked)
                            {
                                Con_Printf("vr_menu_press: %s > %s is dimmed, not pressed\n", title, label);
                                return;
                            }
                            item.buttonPress(item.arg, b);
                            Con_Printf("vr_menu_press: %s > %s\n", title, label);
                            return;
                        }
                    }
                }
            }
            if(item.kind == Item::Action && item.actionArg && item.infoArg && !q_strcasecmp(item.infoArg(item.arg), label))
            {   // a list row (the campaign selector): its text, then its pick(arg)
                locked = item.dimArg && item.dimArg(item.arg);
                found = true;
                if(!locked) { item.actionArg(item.arg); Con_Printf("vr_menu_press: %s > %s\n", title, label); }
                levelOverride = was;
                if(locked) { Con_Printf("vr_menu_press: %s > %s is locked (unavailable), not pressed\n", title, label); }
                return;
            }
            if(item.kind == Item::Action && item.label && !q_strcasecmp(item.label, label))
            {
                command = item.command;
                action = item.page < 0 && !item.actionArg ? item.action : nullptr;
                locked = item.unavailable || (item.setsRules && serverrules::lockedBecause());
                found = true;
                break;
            }
        }
    }
    levelOverride = was;
    if(!found || (!command && !action))
    {
        Con_Printf("vr_menu_press: no button \"%s\" on page \"%s\"\n", label, title);
        return;
    }
    if(locked)
    {
        Con_Printf("vr_menu_press: %s > %s is locked (unavailable), not pressed\n", title, label);
        return;
    }
    Con_Printf("vr_menu_press: %s > %s%s%s\n", title, label, command ? ": " : "", command ? command : "");
    if(command)
    {
        runCommand(command);
    }
    else
    {
        action();
    }
}
