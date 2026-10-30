// vr_menu_help.inc -- rows' help text and the help box, the Search/Console/Map Library rows, page dumps and help checks, path resolving.
// Split from vr_menu.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

// Render Scale's help: the size the eyes are rendered at, and the headset's images'.
[[nodiscard]] const char* renderScaleHelp()
{
    char(&text)[192] = readouts.renderScaleHelp;
    const Backend* be = backend();
    const EyeSizes s = be ? be->eyeSizes() : EyeSizes{};
    if(s.width <= 0 || s.height <= 0)
    {
        return "Eye rendering resolution, times the headset's (SteamVR's resolution included). Above 1: smoother "
               "edges, slower. Below 1: faster, blurrier.";
    }
    const int w = scaledEyeSize(s.width, s.maxWidth);
    const int h = scaledEyeSize(s.height, s.maxHeight);
    q_snprintf(text, sizeof(text), "Renders %dx%d per eye, resampled to the headset's %dx%d. Above 1: smoother "
                                   "edges, slower. Below 1: faster, blurrier.",
        w, h, s.width, s.height);
    return text;
}

// A server rule's help on a remote server's client: the server sets it (its value shown), yours applies when you host.
[[nodiscard]] const char* serverRuleHelp(const Item& item, const char* help)
{
    za::String& text = readouts.serverRuleHelp;
    char own[48], line[160];
    q_snprintf(own, sizeof(own), item.kind == Item::Slider ? item.format : "%g", item.cvar->value);
    q_snprintf(line, sizeof(line), "Set by the server you are connected to (its value shown). Yours, %s, applies when you host or play alone.", own);
    text = line;
    if(help && help[0])
    {
        text += ' ';
        text += help;
    }
    return text.cStr();
}

// An extendable slider's help: its own (`help`, may be null), and that left and right go past the
// bar's ends and how far; that first while the value is on or past an end, where it matters.
[[nodiscard]] const char* extendableHelp(const Item& item, const char* help)
{
    za::String& text = readouts.extendableHelp;
    char lo[32], hi[32], hint[128];
    q_snprintf(lo, sizeof(lo), item.format, item.hardMin);
    q_snprintf(hi, sizeof(hi), item.format, item.hardMax);
    const bool left = item.hardMin < item.min;
    const bool right = item.hardMax > item.max;
    if(left && right)
    {
        q_snprintf(hint, sizeof(hint), "Hold left or right past the ends to go further (%s to %s).", lo, hi);
    }
    else
    {
        q_snprintf(hint, sizeof(hint), "Hold %s past the end to go further (%s %s).", right ? "right" : "left",
            right ? "up to" : "down to", right ? hi : lo);
    }
    const float v = item.cvar->value;
    const float eps = item.step * 0.01f;
    const bool onEnd = v >= item.max - eps || v <= item.min + eps;
    if(!help || !help[0])
    {
        text = hint;
    }
    else
    {
        // Built in place: the static string keeps its capacity, so no allocation per frame.
        text = onEnd ? hint : help;
        text += ' ';
        text += onEnd ? help : hint;
    }
    return text.cStr();
}

// A row's help as drawn under the list: its own (or its function's), a server rule's note, an extendable slider's
// hint (null: none).
const char* itemHelp(const Item& item)
{
    const char* help = item.cvar == &vr_render_scale ? renderScaleHelp() : item.helpArg ? item.helpArg(item.arg) : item.helpText;
    if(lockedItem(item) && !item.unavailable)
    {
        help = serverRuleHelp(item, help);
    }
    else if(item.kind == Item::Slider && item.extendable)
    {
        help = extendableHelp(item, help);
    }
    if(item.kind == Item::Slider && !lockedItem(item))
    {
        // The fine steps' modifier, last.
        za::String& text = readouts.fineHelp;
        text = help && help[0] ? help : "";
        text += text.empty() ? "" : " ";
        text += vrActive() ? "Hold a grip for fine steps." : "Hold Shift for fine steps.";
        help = text.cStr();
    }
    return help;
}


constexpr double helpPageBase = 2.0; // seconds a part of the help stays, and more for each word on it (vr_menu_help_wpm)

// Word-wrapped to the canvas's width (helpColumns) in the box under the list (helpBoxLines). A longer text turns pages
// by itself, each kept long enough to read its words, with a bar at the box's right showing which part is shown.
void drawHelp(const char* text)
{
    const int columns = helpColumns();
    const int box = helpBoxLines();
    int starts[helpMaxWrapped], lengths[helpMaxWrapped];
    const int lines = wrapHelp(text, columns, starts, lengths);
    int first = 0;
    if(lines > box)
    {
        za::U32 hash = 2166136261u;
        for(const char* c = text; *c; c++)
        {
            hash = (hash ^ static_cast<unsigned char>(*c)) * 16777619u;
        }
        if(hash != ui().helpPaging.hash)
        {
            ui().helpPaging.hash = hash;
            ui().helpPaging.since = realtime;
            ui().helpPaging.shown = -1;
        }
        // Each page's time from its words; the pages one after another, then the first again.
        const int pageCount = (lines + box - 1) / box;
        double durations[helpMaxWrapped];
        double cycle = 0.0;
        for(int pg = 0; pg < pageCount; pg++)
        {
            int words = 0;
            for(int l = pg * box; l < lines && l < (pg + 1) * box; l++)
            {
                for(int i = 0; i < lengths[l]; i++)
                {
                    words += text[starts[l] + i] != ' ' && (i == 0 || text[starts[l] + i - 1] == ' ') ? 1 : 0;
                }
            }
            durations[pg] = helpPageBase + 60.0 * words / CLAMP(60.0, static_cast<double>(vr_menu_help_wpm.value), 600.0);
            cycle += durations[pg];
        }
        double t = fmod(realtime - ui().helpPaging.since, cycle);
        int shown = 0;
        while(shown < pageCount - 1 && t >= durations[shown])
        {
            t -= durations[shown];
            shown++;
        }
        first = shown * box;
        if(shown != ui().helpPaging.shown)
        {
            ui().helpPaging.shown = shown;
            Con_DPrintf("menu help: part %d of %d (%d lines, %.1f s)\n", shown + 1, pageCount, lines, realtime - ui().helpPaging.since);
        }

        // Which part is shown: the bar's thumb, and its share of the text.
        const Layout l = layout();
        const menupaint::Painter p;
        namespace colors = menupaint::colors;
        const float x = static_cast<float>((320 + 8 * columns) / 2 + 3);
        const float top = static_cast<float>(l.helpTop);
        const float height = static_cast<float>(box * 8);
        p.rect(x, x + 2.f, top + height * 0.5f, height * 0.5f * p.k, colors::track);
        const float t0 = top + height * static_cast<float>(first) / static_cast<float>(lines);
        const float t1 = top + height * static_cast<float>(q_min(first + box, lines)) / static_cast<float>(lines);
        p.rect(x, x + 2.f, (t0 + t1) * 0.5f, (t1 - t0) * 0.5f * p.k, colors::scrollThumb);
    }
    for(int line = first; line < lines && line < first + box; line++)
    {
        char buf[64];
        const int n = q_min(lengths[line], static_cast<int>(sizeof(buf)) - 1);
        memcpy(buf, text + starts[line], n);
        buf[n] = '\0';
        M_PrintWhite((320 - 8 * n) / 2, layout().helpTop + (line - first) * 8, buf);
    }
}

// menu_vr dump: every page reached from the VR Settings or the Advanced VR Options (menuRoots) through the pages' links (breadth first: its
// depth, the page linking it first, its rows), each row (kind, header above, label, setting, page
// opened), the links into each page, and the pages no link reaches. For the menus' coverage check
// (docs/vr-port/menu_coverage.sh): each page is shown to be built for what the hands hold now.
const char* searchRowText()
{
    return "";
}

// The Search page's rows: a line of text only (showPage and the menu's bookkeeping expect one; the page is drawn and
// driven by vr_menu_search.inc).
za::Vector<Item> pageSearch()
{
    return {info(searchRowText)};
}

// The Console page's rows: as Search's (vr_menu_console.inc draws and drives it).
za::Vector<Item> pageConsole()
{
    return {info(searchRowText)};
}

// The Map Library page's rows: as Search's (vr_menu_maps.inc draws and drives it).
za::Vector<Item> pageMaps()
{
    return {info(searchRowText)};
}

#include "vr_menu_search.inc"
#include "vr_menu_console.inc"
#include "vr_menu_maps.inc"

void dumpPages()
{
    const int was = ui().page;
    int depth[pageCount];
    int from[pageCount];
    int linksIn[pageCount]{};
    for(int p = 0; p < pageCount; p++)
    {
        depth[p] = -1;
        from[p] = -1;
    }
    za::Vector<int> queue;
    for(const int root : menuRoots)
    {
        queue.pushBack(root);
        depth[root] = 0;
    }
    for(size_t q = 0; q < queue.size(); q++)
    {
        const int p = queue[q];
        showPage(p);
        const auto& list = items(p);
        int settings = 0;
        for(const Item& item : list)
        {
            settings += selectable(item) && item.page < 0 ? 1 : 0;
        }
        Con_Printf("MDPAGE|%d|%d|%d|%s|%d|%d\n", p, depth[p], from[p], pages[p].title, static_cast<int>(list.size()), settings);
        for(int i = 0; i < static_cast<int>(list.size()); i++)
        {
            const Item& item = list[i];
            const char kind = item.kind == Item::Header ? 'H'
                              : item.kind == Item::Slider ? 'S'
                              : item.kind == Item::Cycle  ? 'C'
                              : item.kind == Item::Info   ? 'I'
                              : item.page >= 0 && !item.actionArg ? 'O'
                                                                  : 'A';
            za::String label = item.label ? item.label : item.infoArg ? item.infoArg(item.arg) : item.info ? item.info() : "";
            for(char& c : label)
            {
                c = c == '|' || c == '\n' ? ' ' : c;
            }
            Con_Printf("MDROW|%d|%s|%c|%s|%s|%s|%d\n", p, pages[p].title, kind, rowSection(list, i), label.cStr(),
                item.cvar ? item.cvar->name : "", kind == 'O' ? item.page : -1);
            if(kind == 'O' && item.page >= 0 && item.page < pageCount)
            {
                linksIn[item.page]++;
                if(depth[item.page] < 0)
                {
                    depth[item.page] = depth[p] + 1;
                    from[item.page] = p;
                    queue.pushBack(item.page);
                }
            }
        }
    }
    for(int p = 0; p < pageCount; p++)
    {
        Con_Printf("MDLINKS|%d|%s|%d|%d\n", p, pages[p].title, linksIn[p], depth[p]);
    }
    showPage(was);
}

// menu_vr helpcheck [columns]: every page's rows' help wrapped as drawHelp wraps it (at `columns`, else the canvas's
// width now): HELPLONG for each help longer than its page's box (its pages turn), HELPPAGE for each page whose box
// grew, HELPSUM the count, the longest and how many need turning pages. For the help's fit (the menu's tests).
void helpCheck(int columnsAsked)
{
    const int was = ui().page;
    const int columns = columnsAsked > 0 ? columnsAsked : helpColumns();
    int rows = 0, longest = 0, longestChars = 0, turning = 0, grown = 0;
    int histogram[helpMaxLines + 2]{};
    for(int p = 0; p < pageCount; p++)
    {
        if(p == pageIndex(pageSearch) || p == pageIndex(pageConsole) || p == pageIndex(pageMaps))
        {
            continue;
        }
        showPage(p);
        const auto& list = items(p);
        int box = helpMinLines;
        for(const Item& item : list)
        {
            const char* help = itemHelp(item);
            box = help ? q_max(box, wrapHelp(help, columns)) : box;
        }
        box = q_min(box, helpMaxLines);
        if(box > helpMinLines)
        {
            grown++;
            Con_Printf("HELPPAGE|%d|%s|%d lines|%d rows shown\n", p, pages[p].title, box, visibleRows(list));
        }
        for(const Item& item : list)
        {
            const char* help = itemHelp(item);
            if(!help || !help[0])
            {
                continue;
            }
            const int lines = wrapHelp(help, columns);
            rows++;
            histogram[q_min(lines, helpMaxLines + 1)]++;
            longest = q_max(longest, lines);
            longestChars = q_max(longestChars, static_cast<int>(strlen(help)));
            if(lines > box)
            {
                turning++;
                Con_Printf("HELPLONG|%d|%s|%s|%d chars|%d lines|box %d\n", p, pages[p].title,
                    item.label ? item.label : "", static_cast<int>(strlen(help)), lines, box);
            }
        }
    }
    showPage(was);
    Con_Printf("HELPSUM|%d columns|%d rows with help|longest %d lines (%d chars)|%d pages grown|%d turn pages|lines:",
        columns, rows, longest, longestChars, grown, turning);
    for(int l = 1; l <= helpMaxLines + 1; l++)
    {
        Con_Printf(" %d%s=%d", l, l > helpMaxLines ? "+" : "", histogram[l]);
    }
    Con_Printf("\n");
}

// The path to a page from Quake's main menu, by what the player reads on the way (menu::pathTo): the fewest links from
// the VR Settings ("Options > VR Settings > ...") or the Advanced VR Options ("Advanced VR > ...": the main menu's row
// and the corner's button), each page as its link names it. False when no page has the title, no link reaches it, or (a row asked
// for) the page has no row of that label.
bool resolvePath(za::StringView spec, za::String& out)
{
    const auto trim = [](za::StringView s) {
        while(s.startsWith(' '))
        {
            s.removePrefix(1);
        }
        while(s.endsWith(' '))
        {
            s.removeSuffix(1);
        }
        return za::String{s};
    };
    const size_t split = spec.find('>');
    const za::String title = trim(spec.substrByPosLen(0, split));
    const za::String row = split == za::StringView::nPos ? za::String{} : trim(spec.substrByPosLen(split + 1));
    int target = -1;
    for(int p = 0; p < pageCount && target < 0; p++)
    {
        target = q_strcasecmp(pages[p].title, title.cStr()) ? -1 : p;
    }
    if(target < 0)
    {
        return false;
    }
    // Every page and row, whatever Menu Detail shows (the pages built for it again as they are next shown); the level
    // the way needs said after it.
    const int shownLevel = menuLevel();
    const int wasOverride = levelOverride;
    levelOverride = LevelDeveloper;
    struct Restore
    {
        int was;
        ~Restore() { levelOverride = was; }
    } restore{wasOverride};
    int needs = pages[target].level;
    // The shortest way there through the pages' links (breadth first from the tree's roots, as Search): each page
    // named by the link that opens it.
    int from[pageCount];
    const char* link[pageCount]{};
    for(int& f : from)
    {
        f = -1;
    }
    int queue[pageCount];
    int queued = 0;
    for(const int root : menuRoots)
    {
        from[root] = root;
        queue[queued++] = root;
    }
    for(int q = 0; q < queued && from[target] < 0; q++)
    {
        for(const Item& item : items(queue[q]))
        {
            if(item.kind == Item::Action && item.page >= 0 && item.page < pageCount && !item.actionArg && item.label &&
                from[item.page] < 0)
            {
                from[item.page] = queue[q];
                link[item.page] = item.label;
                queue[queued++] = item.page;
            }
        }
    }
    if(from[target] < 0)
    {
        return false; // no link reaches it
    }
    za::Vector<const char*> names; // from the page up
    int root = target;
    for(; !isMenuRoot(root); root = from[root])
    {
        names.pushBack(link[root]);
        needs = q_max(needs, pages[root].level);
    }
    needs = q_max(needs, pages[root].level);
    out = root == PageMain ? za::String{"Options > "} + pages[PageMain].title : za::String{"Advanced VR"};
    for(const char* name : za::reversed(names))
    {
        out += " > ";
        out += name;
    }
    if(!row.empty())
    {
        const char* found = nullptr;
        for(const Item& item : items(target))
        {
            if(item.label && item.kind != Item::Info && !q_strcasecmp(item.label, row.cStr()))
            {
                found = item.label;
                needs = q_max(needs, item.level);
                break;
            }
        }
        if(!found)
        {
            return false;
        }
        out += " > ";
        out += found;
    }
    // (Advanced VR raises Menu Detail to Advanced itself: only a page or row above it says so on that way.)
    if(needs > (root == PageAdvanced ? q_max(shownLevel, static_cast<int>(LevelAdvanced)) : shownLevel))
    {
        out += va(" (Menu Detail: %s)", levelName(needs));
    }
    return true;
}
