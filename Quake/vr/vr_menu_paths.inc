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
    size_t at = 0;
    for(size_t start; (start = text.find(open, at)) != za::StringView::nPos;)
    {
        const size_t end = text.find('}', start);
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
        const size_t lineStart = out.rfind('\n');
        int column = static_cast<int>(out.size() - (lineStart == za::StringView::nPos ? 0 : lineStart + 1));
        za::StringView rest = path;
        for(bool first = true; !rest.empty(); first = false)
        {
            const size_t sep = rest.find(" > ");
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

// vr_menu_path_check [file or text]: every {menu:...} in the loaded map's entities (or in a text file of the game's, as
// maps/vrcalibration.map; or in the text given, starting with "{"), each with its path; "menu paths: N found, M missing"
// last.
void qvr::menu::pathCheck_f()
{
    const char* data = nullptr;
    byte* file = nullptr;
    if(Cmd_Argc() > 1 && Cmd_Argv(1)[0] == '{')
    {
        data = Cmd_Args(); // the text itself: vr_menu_path_check "{menu:Locomotion>Lean}"
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
    for(size_t at = 0; (at = text.find("{menu:", at)) != za::StringView::nPos;)
    {
        const size_t end = text.find('}', at);
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
                item.kind == Item::Cycle ? "CYCL" : item.kind == Item::Action ? "ACT" : "INFO";
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
            Con_Printf("MD|%s|%s|%s|%s\n", kind, label ? label : "", choices.cStr(), item.helpText ? item.helpText : "");
        }
    }
    levelOverride = was;
}

void qvr::menu::press_f()
{
    // (Unquoted, the page and the row split at " > ": a test script's quotes need not survive its shell.)
    char args[512];
    q_strlcpy(args, Cmd_Args() ? Cmd_Args() : "", sizeof(args));
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
        for(const Item& item : items(p))
        {
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
                locked = item.unavailable;
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
