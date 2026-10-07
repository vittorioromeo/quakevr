// vr_menu_search.inc -- Search (the corner's Search button, vr_menuui.cpp): a text box and a keyboard on the left, the
// settings, actions and pages that match on the right, the best first, updated as you type; one picked opens its page
// on it (Back returns here). Every page and row is looked through, whatever Menu Detail shows: a result above it says
// the level, and picking it raises Menu Detail to that. Included by vr_menu.cpp (its pages and items()).
//
// The match: each word typed is looked for in the label (a whole word, a word's start, inside a word, its letters in
// order, or a word with one letter wrong), then in the help (a little less), the pages above and the cvar's name
// (less again); every word must be found somewhere. The label holding the whole text typed, at its start best, comes
// first; then shorter labels. A word with underscores in it is a cvar's name (or a part of one, "gib_health"): matched
// against the cvar's whole name, below the labels unless typed in full. vr_menu_search <text> prints the results (the
// same ranking).
//
// Under the keyboard, the last results opened (searchRecentMax, the latest first): one picked opens it as a result
// does. Kept across starts in quakevr/search_recent.txt (one a line: "row|" or "page|", its path, " > ", its label); one
// no longer found (renamed, removed) is left out of the list shown. menu_vr recent [clear] prints them (SRECENT, with
// where each is drawn, for the tests) or clears them.

struct SearchEntry
{
    za::String label;     // the row's label, or the page's title
    za::String path;      // the pages above it, from its root (menuRoots): "Advanced VR Options > Combat"
    za::String lowLabel;  // lower case, for matching
    za::String lowHelp;
    za::String lowPath;
    za::String lowCvar;
    int page{-1};         // where it is (a page: itself)
    int level{0};         // the Menu Detail it shows from
    bool isPage{false};
};

struct SearchResult
{
    int entry;
    float score;
    bool byCvar{false}; // found by its cvar's name rather than its words (the cvar shown with it)
};

constexpr int searchMaxResults = 60;
constexpr int searchRecentMax = 6;           // results opened kept under the keyboard
constexpr float searchRecentHeight = 12.f;   // a line of them (menu y)
constexpr int searchMaxQuery = 48; // (the longest cvars' names)

struct SearchState
{
    za::String query;
    za::Vector<SearchEntry> index;
    za::Vector<SearchResult> results;
    int resultScroll{0};     // the first result shown
    int hoverKey{-1};        // under the laser or the mouse
    int hoverResult{-1};
    int focusKey{-1};        // the sticks' selection: a key...
    int focusResult{-1};     // ...or a result (-1: neither: the laser's)
    int visibleResults{1};   // as last drawn
    za::Vector<za::String> recent;  // the results opened last, the latest first (their keys: searchEntryKey)
    za::Vector<int> recentEntries;  // each one's entry in `index` now (-1: not found)
    bool recentLoaded{false};
    int hoverRecent{-1};            // under the laser or the mouse
    int focusRecent{-1};            // the sticks' selection
};
SearchState search;

// What an entry is kept by in the recent list: its kind, the pages above it and its label.
[[nodiscard]] za::String searchEntryKey(const SearchEntry& e)
{
    return za::String{e.isPage ? "page|" : "row|"} + e.path + (e.path.empty() ? "" : " > ") + e.label;
}

[[nodiscard]] za::String searchRecentPath()
{
    return za::String{com_gamedir} + "/search_recent.txt";
}

// The recent list's entries found in the index (built again whenever the index is).
void searchResolveRecent()
{
    search.recentEntries.clear();
    for(const za::String& key : search.recent)
    {
        int found = -1;
        for(int i = 0; i < static_cast<int>(search.index.size()) && found < 0; i++)
        {
            found = searchEntryKey(search.index[i]) == key ? i : -1;
        }
        search.recentEntries.pushBack(found);
    }
}

void searchLoadRecent()
{
    search.recentLoaded = true;
    search.recent.clear();
    za::String text;
    if(!files::readText(searchRecentPath().cStr(), text))
    {
        return;
    }
    files::forLines(text, [](za::StringView line) {
        while(line.endsWith('\r') || line.endsWith(' '))
        {
            line.removeSuffix(1);
        }
        if(!line.empty() && static_cast<int>(search.recent.size()) < searchRecentMax)
        {
            // Saved when the Advanced VR Options were under the VR Settings (a link there before): their own tree now.
            const za::StringView under{"VR Settings > Advanced VR Options"};
            const size_t bar = line.find('|');
            if(bar != za::StringView::nPos && line.substrByPosLen(bar + 1).startsWith(under))
            {
                search.recent.emplaceBack(za::String{line.substrByPosLen(0, bar + 1)} +
                                          za::String{line.substrByPosLen(bar + 1 + 14)}); // (less "VR Settings > ")
                return;
            }
            search.recent.emplaceBack(line);
        }
    });
}

void searchSaveRecent()
{
    za::String text;
    for(const za::String& key : search.recent)
    {
        text += key;
        text += '\n';
    }
    if(!files::writeText(searchRecentPath().cStr(), text))
    {
        Con_Printf("menu search: couldn't write %s\n", searchRecentPath().cStr());
    }
}

// An entry opened: first in the recent list (moved up when it is there already), the oldest past searchRecentMax
// dropped; saved at once.
void searchNoteRecent(int entry)
{
    const za::String key = searchEntryKey(search.index[entry]);
    za::Vector<za::String> list{key};
    for(const za::String& k : search.recent)
    {
        if(k != key && static_cast<int>(list.size()) < searchRecentMax)
        {
            list.pushBack(k);
        }
    }
    search.recent = ZA_MOVE(list);
    searchResolveRecent();
    searchSaveRecent();
}

// The recent list's rows shown (those found), in order: their places in `recent`.
[[nodiscard]] int searchRecentShown(int* rows)
{
    int n = 0;
    for(int i = 0; i < static_cast<int>(search.recentEntries.size()); i++)
    {
        if(search.recentEntries[i] >= 0)
        {
            rows[n++] = i;
        }
    }
    return n;
}

[[nodiscard]] za::String lowered(const char* text)
{
    za::String s = text ? text : "";
    for(char& c : s)
    {
        c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
    return s;
}

[[nodiscard]] bool alnum(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

// Letters swapped, missing, extra or wrong: at most `limit` of them (the optimal string alignment distance), else
// limit + 1.
[[nodiscard]] int editDistance(za::StringView a, za::StringView b, int limit)
{
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());
    if(za::abs(n - m) > limit || n > 31 || m > 31)
    {
        return limit + 1;
    }
    int d[33][33];
    for(int i = 0; i <= n; i++)
    {
        d[i][0] = i;
    }
    for(int j = 0; j <= m; j++)
    {
        d[0][j] = j;
    }
    for(int i = 1; i <= n; i++)
    {
        int rowBest = limit + 1;
        for(int j = 1; j <= m; j++)
        {
            const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            d[i][j] = q_min(q_min(d[i - 1][j] + 1, d[i][j - 1] + 1), d[i - 1][j - 1] + cost);
            if(i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
            {
                d[i][j] = q_min(d[i][j], d[i - 2][j - 2] + 1);
            }
            rowBest = q_min(rowBest, d[i][j]);
        }
        if(rowBest > limit)
        {
            return limit + 1;
        }
    }
    return d[n][m];
}

// How well one word typed matches a text (lower case): 1 the whole text, 0.9 a word of it, 0.8 a word's start (less as
// the word goes on), 0.6 inside a word, 0.45 a word with one letter wrong (0.4 a word's start so), and, `fuzzy`, its
// letters in order (up to 0.3, by how close they are); 0 none.
[[nodiscard]] float wordMatch(const za::String& word, const za::String& text, bool fuzzy)
{
    if(word.empty() || text.empty())
    {
        return 0.f;
    }
    if(text == word)
    {
        return 1.f;
    }
    float best = 0.f;
    const za::StringView t{text};
    const za::StringView w{word};
    for(size_t i = 0; i < t.size();)
    {
        if(!alnum(t[i]))
        {
            i++;
            continue;
        }
        size_t j = i;
        while(j < t.size() && alnum(t[j]))
        {
            j++;
        }
        const za::StringView part = t.substrByPosLen(i, j - i);
        if(part == w)
        {
            best = za::fmax(best, 0.9f);
        }
        else if(part.startsWith(w))
        {
            best = za::fmax(best, 0.8f - 0.1f * za::fmin(1.f, static_cast<float>(part.size() - w.size()) / 8.f));
        }
        else if(w.size() >= 4 && editDistance(part, w, 1) <= 1)
        {
            best = za::fmax(best, 0.45f);
        }
        else if(w.size() >= 5 && part.size() > w.size() && editDistance(part.substrByPosLen(0, w.size()), w, 1) <= 1)
        {
            best = za::fmax(best, 0.4f);
        }
        i = j;
    }
    if(best < 0.6f && t.find(w) != za::StringView::nPos)
    {
        best = 0.6f;
    }
    if(best < 0.3f && fuzzy && w.size() >= 3)
    {
        size_t first = za::StringView::nPos, at = 0, k = 0;
        for(; at < t.size() && k < w.size(); at++)
        {
            if(t[at] == w[k])
            {
                first = k == 0 ? at : first;
                k++;
            }
        }
        if(k == w.size())
        {
            const float span = static_cast<float>(at - first);
            best = za::fmax(best, 0.3f * static_cast<float>(w.size()) / span);
        }
    }
    return best;
}

// How well a name typed (its underscores kept) matches a cvar's whole name: 1 the name, 0.85 its end from an underscore
// ("gib_health"), 0.8 its start, 0.7 inside it, 0.45 the name with one letter wrong; 0 none.
[[nodiscard]] float cvarMatch(const za::String& token, const za::String& name)
{
    if(token.empty() || name.empty())
    {
        return 0.f;
    }
    if(token == name)
    {
        return 1.f;
    }
    const za::StringView n{name};
    const za::StringView t{token};
    if(n.size() > t.size() && n.endsWith(t) && n[n.size() - t.size() - 1] == '_')
    {
        return 0.85f;
    }
    if(n.startsWith(t))
    {
        return 0.8f;
    }
    if(n.find(t) != za::StringView::nPos)
    {
        return 0.7f;
    }
    return t.size() >= 6 && editDistance(n, t, 1) <= 1 ? 0.45f : 0.f;
}

// The entry's score for the words typed (0: one of them found nowhere); `byCvar`: found by its cvar's name (`tokens`:
// the text split at spaces only, underscores kept), below the labels' matches unless the name is typed in full.
[[nodiscard]] float searchScore(const SearchEntry& e, const za::Vector<za::String>& words,
    const za::Vector<za::String>& tokens, const za::String& phrase, bool& byCvar)
{
    float total = 0.f;
    for(const za::String& w : words)
    {
        const float s = za::fmax(za::fmax(wordMatch(w, e.lowLabel, true), 0.55f * wordMatch(w, e.lowHelp, false)),
            za::fmax(0.4f * wordMatch(w, e.lowPath, true), 0.5f * wordMatch(w, e.lowCvar, true)));
        if(s <= 0.f)
        {
            total = 0.f;
            break;
        }
        total += s;
    }
    total /= static_cast<float>(words.size());
    if(total > 0.f && e.lowLabel == phrase)
    {
        total += 0.6f;
    }
    else if(total > 0.f && e.lowLabel.startsWith(phrase))
    {
        total += 0.35f;
    }
    else if(total > 0.f && za::StringView{e.lowLabel}.find(za::StringView{phrase}) != za::StringView::nPos)
    {
        total += 0.2f;
    }
    // The cvar's name: each name typed found in it (a word without an underscore, in the words above too). An underscore
    // typed (no label has one): a cvar's name meant, the words' matches less.
    byCvar = false;
    if(za::anyOf(tokens.begin(), tokens.end(), [](const za::String& t) { return za::StringView{t}.find('_') != za::StringView::nPos; }))
    {
        total *= 0.8f;
    }
    if(!e.lowCvar.empty() && za::anyOf(tokens.begin(), tokens.end(), [](const za::String& t) {
           return za::StringView{t}.find('_') != za::StringView::nPos;
       }))
    {
        float named = 0.f;
        for(const za::String& t : tokens)
        {
            const float s = cvarMatch(t, e.lowCvar);
            if(s <= 0.f)
            {
                named = 0.f;
                break;
            }
            named += s;
        }
        named = 0.9f * named / static_cast<float>(tokens.size());
        named += tokens.size() == 1 && tokens[0] == e.lowCvar ? 0.9f : 0.f; // the whole name: first
        if(named > total)
        {
            total = named;
            byCvar = true;
        }
    }
    if(total <= 0.f)
    {
        return 0.f;
    }
    total += e.isPage ? 0.05f : 0.f;                     // a page first among equals
    total += e.level <= menuLevel() ? 0.08f : 0.f;        // one shown now before one above Menu Detail
    total -= 0.002f * static_cast<float>(e.lowLabel.size()); // shorter labels first among equals
    total -= 0.01f * static_cast<float>(za::count(e.path.begin(), e.path.end(), '>')); // and nearer VR Settings
    return total;
}

// Every page reached from the VR Settings or the Advanced VR Options (menuRoots) and every row on it, at Developer (the
// pages built for it again as they are next shown), with the way there: the links' labels, breadth first (as menu paths,
// resolvePath).
void buildSearchIndex()
{
    search.index.clear();
    const int was = levelOverride;
    levelOverride = LevelDeveloper;
    int from[pageCount];
    const char* link[pageCount]{};
    int needs[pageCount]{};
    for(int& f : from)
    {
        f = -1;
    }
    int queue[pageCount];
    int queued = 0;
    for(const int root : menuRoots)
    {
        from[root] = root;
        link[root] = pages[root].title;
        needs[root] = pages[root].level;
        queue[queued++] = root;
    }
    for(int q = 0; q < queued; q++)
    {
        const int p = queue[q];
        if(pages[p].build == pageSearch || pages[p].build == pageChanged)
        {
            continue;
        }
        for(const Item& item : items(p))
        {
            const bool isLink = item.kind == Item::Action && item.page >= 0 && item.page < pageCount && !item.actionArg;
            if(isLink && item.label && from[item.page] < 0)
            {
                from[item.page] = p;
                link[item.page] = item.label;
                needs[item.page] = q_max(q_max(needs[p], item.level), pages[item.page].level);
                queue[queued++] = item.page;
            }
        }
    }
    za::String paths[pageCount]; // to each page, itself included: "Advanced VR Options > Combat"
    for(int q = 0; q < queued; q++)
    {
        const int p = queue[q];
        paths[p] = isMenuRoot(p) ? za::String{pages[p].title} : paths[from[p]] + " > " + link[p];
    }
    for(int q = 0; q < queued; q++)
    {
        const int p = queue[q];
        if(pages[p].build == pageSearch || pages[p].build == pageChanged)
        {
            continue;
        }
        if(p != PageMain)
        {
            SearchEntry e;
            e.label = link[p];
            e.path = isMenuRoot(p) ? za::String{} : paths[from[p]]; // (Advanced VR Options: its own root)
            e.page = p;
            e.level = needs[p];
            e.isPage = true;
            e.lowLabel = lowered(link[p]);
            e.lowHelp = lowered(pages[p].title);
            e.lowPath = lowered(e.path.cStr());
            search.index.pushBack(ZA_MOVE(e));
        }
        for(const Item& item : items(p))
        {
            const bool isLink = item.kind == Item::Action && item.page >= 0 && !item.actionArg;
            if(isLink || !item.label || !item.label[0] || !selectable(item) || item.wide || item.cvar == &vr_menu_level)
            {
                continue; // (links: their pages; the lists' rows: their own)
            }
            SearchEntry e;
            e.label = item.label;
            e.path = paths[p];
            e.page = p;
            e.level = q_max(needs[p], item.level);
            e.lowLabel = lowered(item.label);
            e.lowHelp = lowered(item.helpText);
            e.lowPath = lowered(e.path.cStr());
            e.lowCvar = item.cvar ? lowered(item.cvar->name) : za::String{};
            search.index.pushBack(ZA_MOVE(e));
        }
    }
    levelOverride = was;
    if(!search.recentLoaded)
    {
        searchLoadRecent();
    }
    searchResolveRecent();
}

void runSearch()
{
    search.results.clear();
    search.resultScroll = 0;
    search.focusResult = -1;
    za::Vector<za::String> words;
    za::Vector<za::String> tokens; // (split at spaces and punctuation, but not at underscores: cvars' names)
    const za::String phrase = lowered(search.query.cStr());
    for(size_t i = 0; i < phrase.size();)
    {
        const auto inName = [&](char c) { return alnum(c) || c == '_'; };
        while(i < phrase.size() && !inName(phrase[i]))
        {
            i++;
        }
        size_t j = i;
        while(j < phrase.size() && inName(phrase[j]))
        {
            j++;
        }
        if(j > i)
        {
            tokens.pushBack(za::String{za::StringView{phrase}.substrByPosLen(i, j - i)});
        }
        i = j;
    }
    for(size_t i = 0; i < phrase.size();)
    {
        while(i < phrase.size() && !alnum(phrase[i]))
        {
            i++;
        }
        size_t j = i;
        while(j < phrase.size() && alnum(phrase[j]))
        {
            j++;
        }
        if(j > i)
        {
            words.pushBack(za::String{za::StringView{phrase}.substrByPosLen(i, j - i)});
        }
        i = j;
    }
    if(words.empty())
    {
        return;
    }
    for(int i = 0; i < static_cast<int>(search.index.size()); i++)
    {
        bool byCvar = false;
        const float s = searchScore(search.index[i], words, tokens, phrase, byCvar);
        if(s > 0.25f)
        {
            search.results.pushBack({i, s, byCvar});
        }
    }
    za::stableSort(search.results.begin(), search.results.end(),
        [](const SearchResult& a, const SearchResult& b) { return a.score > b.score; });
    // The same row reached twice (a label on two pages is two rows; the same page listed twice is one).
    za::Vector<SearchResult> kept;
    for(const SearchResult& r : search.results)
    {
        const SearchEntry& e = search.index[r.entry];
        const bool twice = za::anyOf(kept.begin(), kept.end(), [&](const SearchResult& k) {
            const SearchEntry& o = search.index[k.entry];
            return o.page == e.page && o.isPage == e.isPage && o.label == e.label;
        });
        if(!twice && static_cast<int>(kept.size()) < searchMaxResults)
        {
            kept.pushBack(r);
        }
    }
    search.results = ZA_MOVE(kept);
}

// Opens the result's page on its row (raising Menu Detail to show it); Back from it: Search.
// Opens an entry's page on it (a result picked, or one of the recent list's), and notes it in the recent list.
void openEntry(int entry)
{
    if(entry < 0 || entry >= static_cast<int>(search.index.size()))
    {
        return;
    }
    searchNoteRecent(entry);
    const SearchEntry& e = search.index[entry];
    if(e.level > menuLevel())
    {
        Cvar_SetValueQuick(&vr_menu_level, static_cast<float>(e.level));
    }
    const int target = e.page;
    const za::String label = e.label;
    const bool isPage = e.isPage;
    // Back from the result's page goes up its place in the tree (its natural parent), not to Search: Search left off
    // the stack, the pages above the result put on it (the VR Settings too for one of theirs; not above the Advanced
    // VR Options), then whatever Search was opened from.
    navSyncTop();
    if(navTop() == page)
    {
        nav.count--;
    }
    za::Vector<int> chain;
    for(int p = target; static_cast<int>(chain.size()) < pageCount; p = homeOf(p))
    {
        chain.pushBack(p);
        if(p == PageMain || p == PageAdvanced)
        {
            break;
        }
    }
    for(const int p : za::reversed(chain))
    {
        navPush(p);
    }
    showPage(target);
    if(!isPage)
    {
        const auto& list = items(target);
        for(int i = 0; i < static_cast<int>(list.size()); i++)
        {
            if(list[i].label && selectable(list[i]) && label == list[i].label)
            {
                cursors[target] = i;
                scrolls[target] = q_max(i - 3, 0);
                break;
            }
        }
    }
    Con_DPrintf("menu search: \"%s\" opens %s%s%s (Back: up the tree)\n", search.query.cStr(), pages[target].title,
        isPage ? "" : " on ", isPage ? "" : label.cStr());
    S_LocalSound("misc/menu2.wav");
}

void openResult(int r)
{
    if(r >= 0 && r < static_cast<int>(search.results.size()))
    {
        openEntry(search.results[r].entry);
    }
}

// The recent list's row `i` (of those shown) opened.
void openRecent(int i)
{
    int rows[searchRecentMax];
    const int n = searchRecentShown(rows);
    if(i >= 0 && i < n)
    {
        openEntry(search.recentEntries[rows[i]]);
    }
}

// ----------------------------------------------------------------------------
// The page: layout and drawing
// ----------------------------------------------------------------------------

constexpr char keyBackspace = 8;
constexpr char keyClear = 1;

struct SearchKey
{
    char ch;           // typed (keyBackspace, keyClear)
    const char* label;
    float x0, x1, yc;  // menu x; the middle's menu y
};

struct SearchLayout
{
    float left, right, split; // the keyboard from left to split, the results from split to right
    float boxY;               // the text box's middle
    float half;               // half a key's (and the box's) height, true pixels
    float k;                  // the canvas's row spacing (Painter's)
    float resultsTop, resultsBottom;
    float recentTop;          // the recent list under the keyboard: its title's line, its rows under it
    za::Vector<SearchKey> keys;
};

constexpr float searchResultHeight = 18.f; // a result's two lines (menu y)

char searchKeyLetters[4][10][2]; // the keyboard's key labels, two chars each, filled with the layout (SearchKey points at them)

// `keyHalf`: half a key's height (true pixels); `keyGap`: the room between keys, across (menu units) and down (true
// pixels). The Map Library's keys are larger, easier to hit with the laser.
[[nodiscard]] SearchLayout searchLayout(float maxWidth = 440.f, float splitFraction = 0.7f, float keyHalf = 7.f,
    float keyGap = 1.f)
{
    const Layout l = layout();
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    float cl, ct, cr, cb;
    Draw_GetTransformBounds(&t, &cl, &ct, &cr, &cb);
    SearchLayout s;
    s.k = za::fmax(1.f, -t.scale[1] * vid.guiheight / (t.scale[0] * vid.guiwidth));
    s.left = za::fmax(cl + 4.f, 12.f); // (right of the corner's buttons)
    s.right = za::fmin(cr - 4.f, s.left + maxWidth);
    s.split = s.left + (s.right - s.left) * splitFraction;
    s.half = keyHalf;
    const float rowStep = (2.f * s.half + 1.f + 2.f * keyGap) / s.k;
    s.boxY = static_cast<float>(l.listTop) + s.half / s.k;
    s.resultsTop = static_cast<float>(l.listTop);
    s.resultsBottom = static_cast<float>(l.helpTop) - 4.f; // (the help under them)

    static constexpr const char* rows[] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL_", "ZXCVBNM.'/"};
    const float width = s.split - s.left - 6.f;
    const float keyW = width / 10.5f; // (the lower rows set in by half a key at most)
    float y = s.boxY + rowStep * 1.4f;
    for(int r = 0; r < 4; r++, y += rowStep)
    {
        const int n = static_cast<int>(strlen(rows[r]));
        const float x = s.left + (r == 2 ? keyW * 0.25f : r == 3 ? keyW * 0.5f : 0.f);
        for(int i = 0; i < n; i++)
        {
            searchKeyLetters[r][i][0] = rows[r][i];
            searchKeyLetters[r][i][1] = '\0';
            s.keys.pushBack({static_cast<char>(rows[r][i] >= 'A' && rows[r][i] <= 'Z' ? rows[r][i] - 'A' + 'a' : rows[r][i]),
                searchKeyLetters[r][i], x + keyW * i + keyGap, x + keyW * (i + 1) - keyGap, y});
        }
    }
    s.keys.pushBack({' ', "Space", s.left + keyW * 1.f + keyGap, s.left + keyW * 6.f - keyGap, y});
    s.keys.pushBack({keyBackspace, "Delete", s.left + keyW * 6.f + keyGap, s.left + keyW * 8.f - keyGap, y});
    s.keys.pushBack({keyClear, "Clear", s.left + keyW * 8.f + keyGap, s.left + keyW * 10.f - keyGap, y});
    s.recentTop = y + (s.half + 6.f) / s.k;
    return s;
}

// The recent list's rows that fit above the help (the Map Library's page has none: it reuses the layout).
[[nodiscard]] int searchRecentRows(const SearchLayout& s)
{
    return CLAMP(0, static_cast<int>((s.resultsBottom - s.recentTop - 8.f) / searchRecentHeight), searchRecentMax);
}

[[nodiscard]] float searchRecentY(const SearchLayout& s, int i)
{
    return s.recentTop + 8.f + searchRecentHeight * static_cast<float>(i);
}

// The recent list's row (of those shown) under a spot (-1: none).
[[nodiscard]] int searchRecentAt(const SearchLayout& s, float x, float y)
{
    int rows[searchRecentMax];
    const int n = q_min(searchRecentShown(rows), searchRecentRows(s));
    if(x < s.left || x > s.split - 6.f || y < searchRecentY(s, 0) - 2.f)
    {
        return -1;
    }
    const int i = static_cast<int>((y - searchRecentY(s, 0) + 2.f) / searchRecentHeight);
    return i >= 0 && i < n ? i : -1;
}

[[nodiscard]] bool keyHit(const SearchLayout& s, const SearchKey& key, float x, float y)
{
    const float h = (s.half + 2.5f) / s.k; // (the gaps between keys counted to the nearer key: no dead spots)
    return x >= key.x0 - 1.5f && x <= key.x1 + 1.5f && y >= key.yc - h && y <= key.yc + h;
}

[[nodiscard]] int searchResultAt(const SearchLayout& s, float x, float y)
{
    if(x < s.split || x > s.right || y < s.resultsTop)
    {
        return -1;
    }
    const int r = search.resultScroll + static_cast<int>((y - s.resultsTop) / searchResultHeight);
    return r >= 0 && r < static_cast<int>(search.results.size()) && r < search.resultScroll + search.visibleResults ? r : -1;
}

// Text in `size`-pixel letters from x, no wider than `width` (cut with "..").
void searchText(float x, float y, float size, float width, const char* text, bool white)
{
    const int fit = q_max(static_cast<int>(width / size), 0);
    const int n = static_cast<int>(strlen(text));
    for(int i = 0; i < n && i < fit; i++)
    {
        const char c = n > fit && i >= fit - 2 ? '.' : text[i];
        Draw_CharacterEx(x + size * static_cast<float>(i), y, size, size, white ? c : (c | 128));
    }
}

void typeInSearch(char ch)
{
    if(ch == keyBackspace)
    {
        if(!search.query.empty())
        {
            search.query = za::String{za::StringView{search.query}.substrByPosLen(0, search.query.size() - 1)};
        }
    }
    else if(ch == keyClear)
    {
        search.query = za::String{};
    }
    else if(ch >= 32 && ch < 127 && static_cast<int>(search.query.size()) < searchMaxQuery)
    {
        search.query += ch;
    }
    runSearch();
}

void drawSearch()
{
    if(search.index.empty())
    {
        buildSearchIndex(); // (opened another way than its button: the menu reopened on it, menu_vr)
        runSearch();
    }
    const SearchLayout s = searchLayout();
    const menupaint::Painter p;
    namespace colors = menupaint::colors;
    const Layout l = layout();
    qpic_t* title = Draw_CachePic("gfx/p_option.lmp");
    M_DrawPic((320 - title->width) / 2, l.top + 4, title);
    M_PrintWhite((320 - 8 * 6) / 2, l.top + 28, "Search");

    // The text box: what is typed, a cursor after it.
    p.rounded(s.left, s.split - 6.f, s.boxY, s.half, 3.f, colors::boxBorder);
    p.rounded(s.left + 1.f, s.split - 7.f, s.boxY, s.half - 1.f, 2.f, colors::boxFill);
    const float textWidth = s.split - 6.f - s.left - 12.f;
    if(search.query.empty())
    {
        searchText(s.left + 5.f, s.boxY - 4.f, 8.f, textWidth, "Type a setting's name...", false);
    }
    else
    {
        const int fit = static_cast<int>(textWidth / 8.f) - 1;
        const int n = static_cast<int>(search.query.size());
        const char* shown = search.query.cStr() + q_max(n - fit, 0); // its end, if it is long
        searchText(s.left + 5.f, s.boxY - 4.f, 8.f, textWidth, shown, true);
        if(static_cast<int>(realtime * 2.0) % 2 == 0)
        {
            Draw_CharacterEx(s.left + 5.f + 8.f * static_cast<float>(strlen(shown)), s.boxY - 4.f, 8.f, 8.f, 11); // the cursor
        }
    }

    // The keys.
    for(int i = 0; i < static_cast<int>(s.keys.size()); i++)
    {
        const SearchKey& key = s.keys[i];
        const bool hot = search.hoverKey == i || search.focusKey == i;
        p.rounded(key.x0, key.x1, key.yc, s.half, 3.f, hot ? colors::highlightEdge : colors::boxBorder);
        p.rounded(key.x0 + 1.f, key.x1 - 1.f, key.yc, s.half - 1.f, 2.f, hot ? colors::buttonHover : colors::boxFill);
        const float w = 8.f * static_cast<float>(strlen(key.label));
        searchText((key.x0 + key.x1 - w) * 0.5f, key.yc - 4.f, 8.f, key.x1 - key.x0, key.label, hot);
    }

    // The recent list: what was opened last, the latest first; each label, and the page it is on in small letters.
    {
        int rows[searchRecentMax];
        const int n = q_min(searchRecentShown(rows), searchRecentRows(s));
        const float width = s.split - 6.f - s.left;
        if(searchRecentRows(s) > 0)
        {
            searchText(s.left, s.recentTop, 6.f, width, n > 0 ? "Opened recently:" : "Opened recently: none yet", false);
        }
        for(int i = 0; i < n; i++)
        {
            const SearchEntry& e = search.index[search.recentEntries[rows[i]]];
            const float y = searchRecentY(s, i);
            const bool hot = search.hoverRecent == i || search.focusRecent == i;
            p.rounded(s.left, s.split - 6.f, y + 3.5f, 5.5f * s.k, 2.f, hot ? colors::buttonHover : colors::boxFill);
            if(hot)
            {
                p.rounded(s.left - 1.f, s.split - 5.f, y + 3.5f, 6.f * s.k, 2.f, colors::listHover);
            }
            za::StringView where{e.path};
            const size_t cut = where.rfind('>');
            if(cut != za::StringView::nPos && cut + 2 <= where.size())
            {
                where.removePrefix(cut + 2); // the page it is on (a page: the one linking it)
            }
            char name[64];
            q_snprintf(name, sizeof(name), "%s%s", e.label.cStr(), e.isPage ? " >" : "");
            const float whereWidth = q_min(5.f * static_cast<float>(where.size()), width * 0.4f);
            const float nameWidth = width - 8.f - whereWidth - 6.f;
            const float nameSize = za::fmax(5.f, za::fmin(7.f, nameWidth / q_max(static_cast<float>(strlen(name)), 1.f)));
            searchText(s.left + 4.f, y + (7.f - nameSize), nameSize, nameWidth, name, true);
            char page[64];
            q_snprintf(page, sizeof(page), "%.*s", static_cast<int>(q_min(where.size(), sizeof(page) - 1)), where.data());
            searchText(s.split - 10.f - whereWidth, y + 2.f, 5.f, whereWidth, page, false);
        }
    }

    // The results: the label, and under it in small letters the pages it is on (the last of them if long), and the
    // Menu Detail it needs if above the one shown.
    const float x0 = s.split + 2.f;
    const float width = s.right - x0;
    search.visibleResults = q_max(static_cast<int>((s.resultsBottom - s.resultsTop) / searchResultHeight), 1);
    const int n = static_cast<int>(search.results.size());
    if(n == 0)
    {
        searchText(x0, s.resultsTop + 2.f, 6.f, width, search.query.empty() ? "Results show here" : "Nothing found", false);
    }
    for(int r = search.resultScroll; r < n && r < search.resultScroll + search.visibleResults; r++)
    {
        const SearchEntry& e = search.index[search.results[r].entry];
        const float y = s.resultsTop + searchResultHeight * static_cast<float>(r - search.resultScroll);
        const bool hot = search.hoverResult == r || search.focusResult == r;
        if(hot)
        {
            p.rounded(x0 - 2.f, s.right, y + 7.5f, 8.5f * s.k, 2.f, colors::listHover);
        }
        char name[64];
        q_snprintf(name, sizeof(name), "%s%s", e.label.cStr(), e.isPage ? " >" : "");
        const float nameSize = za::fmax(5.f, za::fmin(8.f, width / q_max(static_cast<float>(strlen(name)), 1.f)));
        searchText(x0, y + (8.f - nameSize), nameSize, width, name, true);
        za::StringView path{e.path};
        const za::StringView root{"VR Settings > "};
        if(path.startsWith(root) && path.size() > root.size())
        {
            path.removePrefix(root.size());
        }
        za::String where{path};
        if(search.results[r].byCvar)
        {
            where += va(": %s", e.lowCvar.cStr()); // (the end kept when cut: the name found)
        }
        if(e.level > menuLevel())
        {
            where += va(" (%s)", levelName(e.level));
        }
        const int fit = static_cast<int>(width / 5.f);
        const char* shown = where.cStr();
        if(static_cast<int>(where.size()) > fit)
        {
            shown += where.size() - static_cast<size_t>(fit) + 2; // its end: the pages nearest the result
            searchText(x0, y + 9.f, 5.f, 10.f, "..", false);
            searchText(x0 + 10.f, y + 9.f, 5.f, width - 10.f, shown, false);
        }
        else
        {
            searchText(x0, y + 9.f, 5.f, width, shown, false);
        }
    }
    if(n > search.visibleResults)
    {
        char more[32];
        q_snprintf(more, sizeof(more), "%d-%d of %d", search.resultScroll + 1,
            q_min(search.resultScroll + search.visibleResults, n), n);
        searchText(s.right - 5.f * static_cast<float>(strlen(more)), s.resultsTop - 7.f, 5.f, width, more, false);
    }

    drawHelp(search.focusRecent >= 0 || search.hoverRecent >= 0
                 ? "Opened recently, the latest first: pick one to open its page on it again, as a result. Kept across "
                   "starts (quakevr/search_recent.txt)."
             : search.focusKey >= 0 || search.hoverKey >= 0 || n == 0
                 ? "Type with the keys (or a keyboard); the results update as you go. Pick one to open its page on it; "
                   "Back returns here."
                 : "A result above Menu Detail (in brackets) raises it to show its page. Back: where Search was opened.");
}

// ----------------------------------------------------------------------------
// The page: input
// ----------------------------------------------------------------------------

void searchScrollTo(int r)
{
    search.resultScroll = CLAMP(0, search.resultScroll, q_max(static_cast<int>(search.results.size()) - search.visibleResults, 0));
    if(r >= 0)
    {
        search.resultScroll = r < search.resultScroll ? r
                              : r >= search.resultScroll + search.visibleResults ? r - search.visibleResults + 1
                                                                                  : search.resultScroll;
    }
}

// The sticks' selection moved across the keys (rows of them, nearest across) and onto the results (right of the keys).
void searchMove(int dx, int dy)
{
    const SearchLayout s = searchLayout();
    const int keys = static_cast<int>(s.keys.size());
    const int results = static_cast<int>(search.results.size());
    int recentRows[searchRecentMax];
    const int recents = q_min(searchRecentShown(recentRows), searchRecentRows(s));
    if(search.focusRecent >= 0)
    {
        if(dx > 0 && results > 0)
        {
            search.focusRecent = -1;
            search.focusResult = search.resultScroll;
        }
        else if(dy < 0 && search.focusRecent == 0)
        {
            search.focusRecent = -1;
            search.focusKey = keys - 3; // the bottom row's Space
        }
        else if(dy != 0)
        {
            search.focusRecent = CLAMP(0, search.focusRecent + dy, q_max(recents - 1, 0));
        }
        return;
    }
    if(search.focusResult >= 0)
    {
        if(dx < 0)
        {
            search.focusResult = -1;
            search.focusKey = 9; // the end of the top row
            return;
        }
        search.focusResult = CLAMP(0, search.focusResult + dy, q_max(results - 1, 0));
        searchScrollTo(search.focusResult);
        return;
    }
    if(search.focusKey < 0)
    {
        search.focusKey = 0;
        return;
    }
    const SearchKey& at = s.keys[search.focusKey];
    const float xc = (at.x0 + at.x1) * 0.5f;
    if(dx != 0)
    {
        const int next = search.focusKey + dx;
        if(next < 0 || next >= keys || s.keys[next].yc != at.yc)
        {
            if(dx > 0 && results > 0)
            {
                search.focusKey = -1;
                search.focusResult = search.resultScroll;
            }
            return;
        }
        search.focusKey = next;
        return;
    }
    // Up or down: the nearest key across in the row above or below.
    int best = -1;
    float bestRow = 1e9f, bestDx = 1e9f;
    for(int i = 0; i < keys; i++)
    {
        const float row = (s.keys[i].yc - at.yc) * static_cast<float>(dy);
        if(row <= 0.f)
        {
            continue;
        }
        const float d = za::fabs((s.keys[i].x0 + s.keys[i].x1) * 0.5f - xc);
        if(row < bestRow - 0.5f || (row < bestRow + 0.5f && d < bestDx))
        {
            best = i;
            bestRow = row;
            bestDx = d;
        }
    }
    if(best >= 0)
    {
        search.focusKey = best;
    }
    else if(dy < 0)
    {
        search.focusKey = -1;
        menuui::focusToolbar(-1); // up from the top row: the corner's buttons
    }
    else if(recents > 0)
    {
        search.focusKey = -1;
        search.focusRecent = 0; // down from the bottom row: the recent list
    }
}

void searchKey(int key)
{
    const SearchLayout s = searchLayout();
    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON:
        case K_MOUSE2:
        case K_MOUSE4: goBack(); return;
        case K_BACKSPACE: typeInSearch(keyBackspace); return;
        case K_DEL: typeInSearch(keyClear); return;
        case K_UPARROW: searchMove(0, -1); S_LocalSound("misc/menu1.wav"); return;
        case K_DOWNARROW: searchMove(0, 1); S_LocalSound("misc/menu1.wav"); return;
        case K_LEFTARROW: searchMove(-1, 0); S_LocalSound("misc/menu1.wav"); return;
        case K_RIGHTARROW: searchMove(1, 0); S_LocalSound("misc/menu1.wav"); return;
        case K_MWHEELUP:
        case K_MWHEELDOWN:
            search.resultScroll += key == K_MWHEELUP ? -1 : 1;
            searchScrollTo(-1);
            return;
        case K_ENTER:
        case K_KP_ENTER:
            // A keyboard's Enter: the recent row or result picked, else the first result.
            if(search.focusRecent >= 0)
            {
                openRecent(search.focusRecent);
                return;
            }
            openResult(search.focusResult >= 0 ? search.focusResult : 0);
            return;
        case K_ABUTTON:
            if(search.focusRecent >= 0)
            {
                openRecent(search.focusRecent);
            }
            else if(search.focusResult >= 0)
            {
                openResult(search.focusResult);
            }
            else if(search.focusKey >= 0)
            {
                typeInSearch(s.keys[search.focusKey].ch);
                S_LocalSound("misc/menu1.wav");
            }
            return;
        case K_MOUSE1:
            for(int i = 0; i < static_cast<int>(s.keys.size()); i++)
            {
                if(keyHit(s, s.keys[i], m_mousex, m_mousey))
                {
                    typeInSearch(s.keys[i].ch);
                    S_LocalSound("misc/menu1.wav");
                    return;
                }
            }
            if(const int recent = searchRecentAt(s, m_mousex, m_mousey); recent >= 0)
            {
                openRecent(recent);
                return;
            }
            openResult(searchResultAt(s, m_mousex, m_mousey));
            return;
        default: return; // (typing: VR_Menu_Char)
    }
}

void searchMouse(float x, float y)
{
    const SearchLayout s = searchLayout();
    int key = -1;
    for(int i = 0; i < static_cast<int>(s.keys.size()) && key < 0; i++)
    {
        key = keyHit(s, s.keys[i], x, y) ? i : -1;
    }
    const int result = key >= 0 ? -1 : searchResultAt(s, x, y);
    const int recent = key >= 0 || result >= 0 ? -1 : searchRecentAt(s, x, y);
    if((key >= 0 && key != search.hoverKey) || (result >= 0 && result != search.hoverResult) ||
        (recent >= 0 && recent != search.hoverRecent))
    {
        search.focusKey = search.focusResult = search.focusRecent = -1; // the laser's now
        if(ui_mouse_sound.value)
        {
            S_LocalSound("misc/menu1.wav");
        }
    }
    search.hoverKey = key;
    search.hoverResult = result;
    search.hoverRecent = recent;
}

// Search, from any menu (the corner's button): the text as it was left, the results again (the pages may have
// changed).
void openSearchPage()
{
    const int target = pageIndex(pageSearch);
    if(m_state == m_vr && page == target)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    buildSearchIndex();
    runSearch();
    search.hoverKey = search.hoverResult = search.hoverRecent = -1;
    search.focusRecent = -1;
    search.focusKey = 0;
    openFromAnywhere(target); // (Back: where it was opened from)
}

// menu_vr recent [clear]: the recent list (SRECENT: its row shown, or "-" when no longer found; the label, the path;
// where it is drawn while Search is shown, its middle in menu coordinates, for vr_mock_laser / vr_mock_mouse), or the
// list cleared.
void searchRecentCommand(bool clear)
{
    if(search.index.empty())
    {
        buildSearchIndex();
    }
    if(clear)
    {
        search.recent.clear();
        searchResolveRecent();
        searchSaveRecent();
    }
    const SearchLayout s = searchLayout();
    int shown = 0;
    for(int i = 0; i < static_cast<int>(search.recent.size()); i++)
    {
        const int entry = search.recentEntries[i];
        if(entry < 0)
        {
            Con_Printf("SRECENT|-|%s\n", search.recent[i].cStr());
            continue;
        }
        const SearchEntry& e = search.index[entry];
        Con_Printf("SRECENT|%d|%s|%s|%.0f %.0f\n", shown, e.label.cStr(), e.path.cStr(), (s.left + s.split - 6.f) * 0.5f,
            searchRecentY(s, shown) + 3.5f);
        shown++;
    }
    Con_Printf("SRECENT %d kept, %d shown (room for %d); first result at %.0f %.0f\n", static_cast<int>(search.recent.size()),
        shown, searchRecentRows(s), (s.split + s.right) * 0.5f, s.resultsTop + 4.f);
}
