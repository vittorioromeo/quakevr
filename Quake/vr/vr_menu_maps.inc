// vr_menu_maps.inc -- Map Library (the corner's Maps button, and Single Player > Map Library): the external map
// index (vr_mapindex.cpp) read as a list. What is typed on the page's own keyboard (the Search page's, reused here)
// and the filters on its bottom bar choose which packages are listed, on the right; one picked shows its
// description, file count and size on the left and offers Install (its zip downloaded and unpacked into the game dir,
// its progress shown on the bar) or, when its files are there already, Play. Installing never starts a map: Play is
// its own action (vr_mapinstall.cpp). Included by vr_menu.cpp, after vr_menu_search.inc, whose text box, keyboard,
// key hit test and text drawing this page uses, and whose layout(), drawHelp and pages' bookkeeping it shares.
//
// The list is built once per change: the results are cached here and mapindex::search is run again only when the
// text, a filter, the sort, vr_maps_allow_progs or the index itself changed (never per frame). maps_page_stats
// prints what it cost (Debug > Tools).

constexpr int mapsMaxQuery = 48;                 // (the longest titles and authors are shorter)
constexpr float mapsRowHeight = 18.f;            // a package's two lines (menu y), as Search's results
constexpr int mapsDetailChars = 640;             // a description's length shown
constexpr float mapsPageWidth = 460.f;           // the page across, as Search caps its own
constexpr float mapsKeySplit = 0.5f;             // the keyboard's share (the list gets the rest)

// The bottom bar's buttons, in order.
enum MapsButton
{
    MapButtonSort,
    MapButtonType,
    MapButtonSize,
    MapButtonRating,
    MapButtonInstalled,
    MapButtonAction,
    MapButtonCount
};

// The sorts the Sort button cycles.
struct MapsSortChoice
{
    const char* label;
    mapindex::Sort sort;
    bool newestFirst;
};
constexpr MapsSortChoice mapsSorts[] = {
    {"by rating", mapindex::Sort::Rating, true},
    {"newest", mapindex::Sort::Date, true},
    {"oldest", mapindex::Sort::Date, false},
    {"largest", mapindex::Sort::Bytes, true},
    {"smallest", mapindex::Sort::Bytes, false},
    {"by title", mapindex::Sort::Title, true},
};
constexpr int mapsSortCount = static_cast<int>(sizeof(mapsSorts) / sizeof(mapsSorts[0]));

// The minimum community ratings the Rating button cycles (Entry::score, x100; 0: any, the unrated included).
struct MapsRatingChoice
{
    const char* label;
    int min;
};
constexpr MapsRatingChoice mapsRatings[] = {
    {"any rating", 0}, {"rated 3+", 300}, {"rated 3.5+", 350}, {"rated 4+", 400}, {"rated 4.5+", 450}};
constexpr int mapsRatingCount = static_cast<int>(sizeof(mapsRatings) / sizeof(mapsRatings[0]));

// What the Installed button cycles: every package, only the ones installed from here (maps_installed.txt), or only
// the ones not installed yet.
enum MapsShown
{
    MapsShowAll,
    MapsShowInstalled,
    MapsShowNotInstalled,
    MapsShowCount
};
constexpr const char* mapsShownLabels[MapsShowCount] = {"all maps", "installed", "to install"};

// A filter choice read from the index (a type or a map_size a package carries, and how many do).
struct MapsFacet
{
    za::String value;
    int count{0};
};

// The page's state. The results are the cache; `sig...` is what they were built for.
struct MapsPage
{
    za::String query;
    int sort{1}; // newest
    int rating{0}; // mapsRatings
    int shown{MapsShowAll}; // MapsShown
    za::Vector<MapsFacet> typeFacets, sizeFacets;
    int typeIdx{0}, sizeIdx{0}; // 0: any; n > 0: facets[n - 1]
    za::I64 facetsAt{-1};       // mapindex::generation() the facets were read from

    za::Vector<const mapindex::Entry*> results;
    za::String sigQuery;
    za::String sigType, sigSize;
    int sigSort{-1}, sigTypeIdx{-1}, sigSizeIdx{-1}, sigRating{-1}, sigShown{-1};
    int sigProgs{-1};
    za::I64 sigFetched{-1}; // mapindex::generation() (not fetchedAt: a cache read again repeats it, its Entries new)
    int sigInstalled{-1};
    int hiddenProgs{0}; // packages left out for carrying their own progs.dat

    int from{PageMain}; // where Back goes
    int resultScroll{0};
    int hoverKey{-1}, hoverResult{-1}, hoverButton{-1};  // under the laser
    int focusKey{-1}, focusResult{-1}, focusButton{-1};  // the sticks' selection
    // The package picked (a click on its row, or the sticks on it): the detail and Install / Play are its until another
    // is picked, wherever the mouse or the selection goes then (to the button). Kept by its sha256 (the list is built
    // again as the text or a filter changes, the index itself when it is fetched again), its row found again then.
    za::String pickedSha;
    int picked{-1};
    int visibleResults{1};
    // The detail's notes on the package it shows (what Play starts; the mod it was made for), built again when the
    // package, the index or what is installed changes (not every frame).
    za::String notesSha;
    za::U32 notesGeneration{0};
    int notesInstalled{-1};
    za::String notes;


    // measured (maps_page_stats)
    int searchRuns{0};
    double searchMs{0.0};
    int drawFrames{0};
    double drawMs{0.0};
};
MapsPage mapPage;

// The page's own text buffers (drawn once per frame; no allocation).
struct MapsText
{
    char title[128];
    char meta[256];
    char author[128];
    char detail[mapsDetailChars];
    char label[64];
};
MapsText mapsText;

// ----------------------------------------------------------------------------
// The index read as the page needs it
// ----------------------------------------------------------------------------

// A field's values joined ("Rickard Edé, Someone Else"), in the menu's letters (UTF8_ToQuake, as the Mods menu
// shows an author): the index holds UTF-8, the menu draws Quake's conchars.
[[nodiscard]] za::String mapsJoined(const mapindex::Text& field, const char* sep)
{
    za::String joined;
    mapindex::forParts(mapindex::index().field(field), [&](const za::String& part) {
        if(!joined.empty())
        {
            joined += sep;
        }
        joined += part;
    });
    return joined;
}

// `field` as one line, in the menu's letters, cut to `out`'s size.
void mapsQuake(const mapindex::Text& field, char* out, size_t size, const char* sep = ", ")
{
    const za::String joined = mapsJoined(field, sep);
    UTF8_ToQuake(out, size, joined.cStr());
}

// The types and map sizes the index actually carries, most common first (read when the index arrives, not per frame).
void buildMapsFacets()
{
    const mapindex::Index& idx = mapindex::index();
    mapPage.typeFacets.clear();
    mapPage.sizeFacets.clear();
    const auto add = [](za::Vector<MapsFacet>& facets, const za::String& value) {
        if(value.empty())
        {
            return;
        }
        for(MapsFacet& f : facets)
        {
            if(f.value == value)
            {
                f.count++;
                return;
            }
        }
        facets.pushBack({ZA_MOVE(value), 1});
    };
    for(const mapindex::Entry& e : idx.entries)
    {
        mapindex::forParts(idx.field(e.types), [&](const za::String& part) { add(mapPage.typeFacets, part); });
        mapindex::forParts(idx.field(e.sizes), [&](const za::String& part) { add(mapPage.sizeFacets, part); });
    }
    const auto byCount = [](const MapsFacet& a, const MapsFacet& b) {
        return a.count > b.count || (a.count == b.count && a.value < b.value);
    };
    za::stableSort(mapPage.typeFacets.begin(), mapPage.typeFacets.end(), byCount);
    za::stableSort(mapPage.sizeFacets.begin(), mapPage.sizeFacets.end(), byCount);
}

// The results, again. Only called when something the list depends on changed (mapsShown decides that).
void mapsRunSearch()
{
    const double t0 = Sys_DoubleTime();
    const mapindex::Index& idx = mapindex::index();
    mapindex::Query q;
    q.text = mapPage.query;
    if(mapPage.typeIdx > 0 && mapPage.typeIdx <= static_cast<int>(mapPage.typeFacets.size()))
    {
        q.type = mapPage.typeFacets[mapPage.typeIdx - 1].value;
    }
    if(mapPage.sizeIdx > 0 && mapPage.sizeIdx <= static_cast<int>(mapPage.sizeFacets.size()))
    {
        q.mapSize = mapPage.sizeFacets[mapPage.sizeIdx - 1].value;
    }
    // (the installed ones listed whatever vr_maps_allow_progs says: they are there already)
    q.allowProgs = vr_maps_allow_progs.value != 0.f || mapPage.shown == MapsShowInstalled;
    q.sort = mapsSorts[mapPage.sort].sort;
    q.newestFirst = mapsSorts[mapPage.sort].newestFirst;
    q.minRating = mapsRatings[mapPage.rating].min;
    q.limit = 0; // every match: the page scrolls, and its header says how many there are
    mapindex::search(q, mapPage.results);
    if(mapPage.shown != MapsShowAll)
    {
        // Installed or not: the installed list (main thread, in memory), kept in the search's order.
        const bool want = mapPage.shown == MapsShowInstalled;
        za::Vector<const mapindex::Entry*> kept;
        for(const mapindex::Entry* e : mapPage.results)
        {
            if(mapinstall::installed(za::String{idx.field(e->sha256)}) == want)
            {
                kept.pushBack(e);
            }
        }
        mapPage.results = ZA_MOVE(kept);
    }

    mapPage.hiddenProgs = 0;
    if(!q.allowProgs)
    {
        for(const mapindex::Entry& e : idx.entries)
        {
            if(e.hasProgs)
            {
                mapPage.hiddenProgs++;
            }
        }
    }

    mapPage.sigQuery = mapPage.query;
    mapPage.sigType = q.type;
    mapPage.sigSize = q.mapSize;
    mapPage.sigSort = mapPage.sort;
    mapPage.sigTypeIdx = mapPage.typeIdx;
    mapPage.sigSizeIdx = mapPage.sizeIdx;
    mapPage.sigRating = mapPage.rating;
    mapPage.sigShown = mapPage.shown;
    mapPage.sigProgs = q.allowProgs ? 1 : 0;
    mapPage.sigFetched = mapindex::generation();
    mapPage.sigInstalled = static_cast<int>(mapinstall::installedList().size());
    mapPage.resultScroll = 0;
    mapPage.focusResult = mapPage.hoverResult = -1;
    mapPage.picked = -1;
    for(int r = 0; r < static_cast<int>(mapPage.results.size()) && !mapPage.pickedSha.empty(); r++)
    {
        if(za::StringView{idx.field(mapPage.results[r]->sha256)} == za::StringView{mapPage.pickedSha})
        {
            mapPage.picked = r;
            break;
        }
    }
    mapPage.searchRuns++;
    mapPage.searchMs += (Sys_DoubleTime() - t0) * 1000.0;
}

// The list, and the filters, built again only when something changed: the text, a filter, the sort, the progs
// setting, the index arriving, or a package installed.
void mapsShown()
{
    const za::I64 generation = mapindex::generation();
    if(mapPage.facetsAt != generation)
    {
        buildMapsFacets();
        mapPage.facetsAt = generation;
    }
    const int installed = static_cast<int>(mapinstall::installedList().size());
    const int progs = vr_maps_allow_progs.value != 0.f ? 1 : 0;
    if(mapPage.sigQuery != mapPage.query || mapPage.sigSort != mapPage.sort || mapPage.sigTypeIdx != mapPage.typeIdx ||
        mapPage.sigSizeIdx != mapPage.sizeIdx || mapPage.sigRating != mapPage.rating || mapPage.sigShown != mapPage.shown || mapPage.sigProgs != progs || mapPage.sigFetched != generation ||
        mapPage.sigInstalled != installed)
    {
        mapsRunSearch();
    }
}

// ----------------------------------------------------------------------------
// Layout
// ----------------------------------------------------------------------------

struct MapsBar
{
    float x0[MapButtonCount], x1[MapButtonCount];
    float yc;
    float half;
};

struct MapsLayout
{
    SearchLayout s; // the text box, the keys, the page's left and right edges (vr_menu_search.inc)
    MapsBar bar;
    float detailTop; // the picked package, under the keys
    float listBottom; // both columns stop above the bar
};

[[nodiscard]] MapsLayout mapsLayout()
{
    const Layout l = layout();
    MapsLayout m;
    // The keys as large as fit (easier to hit with the laser), with more room between them, above the picked package's
    // detail (a few lines kept for it) and the bar.
    constexpr float detailRoom = 30.f;
    for(float half = 9.f;; half -= 0.5f)
    {
        m.s = searchLayout(mapsPageWidth, mapsKeySplit, half, half > 7.f ? 1.5f : 1.f);
        const float keysBottom = m.s.keys.empty() ? m.s.boxY : m.s.keys.back().yc + m.s.half / m.s.k;
        if(half <= 7.f || keysBottom + detailRoom <= static_cast<float>(l.helpTop))
        {
            break;
        }
    }
    m.bar.half = 7.f; // (the bar's buttons as before)
    // At the page's foot (its status line just under it), well clear of the columns' last lines above.
    m.bar.yc = static_cast<float>(l.bottom) - m.bar.half - 10.f;
    m.listBottom = static_cast<float>(l.helpTop) - 6.f;
    const float rowStep = (2.f * m.s.half + 3.f) / m.s.k;
    m.detailTop = m.s.keys.empty() ? m.s.boxY + rowStep : m.s.keys.back().yc + rowStep;

    // Five filters across the left, the action (Install / Play) at the right end of the bar.
    const float total = m.s.right - m.s.left;
    const float action = za::fmin(110.f, total * 0.24f);
    const float each = (total - action - 6.f) / static_cast<float>(MapButtonAction);
    for(int i = 0; i < MapButtonAction; i++)
    {
        m.bar.x0[i] = m.s.left + each * static_cast<float>(i) + 2.f;
        m.bar.x1[i] = m.s.left + each * static_cast<float>(i + 1) - 2.f;
    }
    m.bar.x0[MapButtonAction] = m.s.right - action;
    m.bar.x1[MapButtonAction] = m.s.right;
    return m;
}

[[nodiscard]] bool mapsButtonHit(const MapsLayout& m, int button, float x, float y)
{
    const float h = (m.bar.half + 2.f) / m.s.k;
    return x >= m.bar.x0[button] - 1.f && x <= m.bar.x1[button] + 1.f && y >= m.bar.yc - h && y <= m.bar.yc + h;
}

// The row under a spot of the list (-1: none).
[[nodiscard]] int mapsResultAt(const MapsLayout& m, float x, float y)
{
    if(x < m.s.split || x > m.s.right || y < m.s.resultsTop || y > m.listBottom)
    {
        return -1;
    }
    const int r = mapPage.resultScroll + static_cast<int>((y - m.s.resultsTop) / mapsRowHeight);
    return r >= 0 && r < static_cast<int>(mapPage.results.size()) && r < mapPage.resultScroll + mapPage.visibleResults
               ? r
               : -1;
}

// The package the detail shows and Install / Play acts on: what the sticks are on, else the one picked, else what the
// laser is over.
[[nodiscard]] int mapsSelected()
{
    const int r = mapPage.focusResult >= 0 ? mapPage.focusResult : mapPage.picked >= 0 ? mapPage.picked : mapPage.hoverResult;
    return r >= 0 && r < static_cast<int>(mapPage.results.size()) ? r : -1;
}

// Picks row `r` (-1: none).
void mapsPick(int r)
{
    const bool valid = r >= 0 && r < static_cast<int>(mapPage.results.size());
    mapPage.picked = valid ? r : -1;
    mapPage.pickedSha = valid ? za::String{mapindex::index().field(mapPage.results[r]->sha256)} : za::String{};
}

void mapsScrollTo(int r)
{
    mapPage.resultScroll =
        CLAMP(0, mapPage.resultScroll, q_max(static_cast<int>(mapPage.results.size()) - mapPage.visibleResults, 0));
    if(r >= 0)
    {
        mapPage.resultScroll = r < mapPage.resultScroll ? r
                              : r >= mapPage.resultScroll + mapPage.visibleResults ? r - mapPage.visibleResults + 1
                                                                                   : mapPage.resultScroll;
    }
}

// ----------------------------------------------------------------------------
// What the page does
// ----------------------------------------------------------------------------

// A job is running (its thread has it: the bar's action is Cancel meanwhile).
[[nodiscard]] bool mapsBusy()
{
    return mapinstall::running();
}

void mapsInstallAt(int r)
{
    if(r < 0 || r >= static_cast<int>(mapPage.results.size()))
    {
        return;
    }
    const mapindex::Entry* e = mapPage.results[r];
    za::String why;
    if(!mapinstall::begin(e, true, &why))
    {
        Con_Printf("maps: %s not started: %s\n", mapindex::index().field(e->title), why.cStr());
        S_LocalSound("misc/menu1.wav");
        return;
    }
    S_LocalSound("misc/menu3.wav");
}

// Play: its own action, for a package whose files are in the game dir already.
void mapsPlayAt(int r)
{
    if(r < 0 || r >= static_cast<int>(mapPage.results.size()))
    {
        return;
    }
    const mapindex::Entry* e = mapPage.results[r];
    if(mapinstall::play(za::String{mapindex::index().field(e->sha256)}))
    {
        S_LocalSound("misc/menu3.wav");
    }
    else
    {
        S_LocalSound("misc/menu1.wav");
    }
}

// Enter on a row: install what isn't installed, play what is.
void mapsPrimaryAt(int r)
{
    if(r < 0 || r >= static_cast<int>(mapPage.results.size()))
    {
        return;
    }
    const za::String sha{mapindex::index().field(mapPage.results[r]->sha256)};
    if(mapinstall::installed(sha))
    {
        mapsPlayAt(r);
    }
    else
    {
        mapsInstallAt(r);
    }
}

void mapsButtonAt(int button)
{
    switch(button)
    {
        case MapButtonSort:
            mapPage.sort = (mapPage.sort + 1) % mapsSortCount;
            break;
        case MapButtonType:
            mapPage.typeIdx = (mapPage.typeIdx + 1) % (static_cast<int>(mapPage.typeFacets.size()) + 1);
            break;
        case MapButtonSize:
            mapPage.sizeIdx = (mapPage.sizeIdx + 1) % (static_cast<int>(mapPage.sizeFacets.size()) + 1);
            break;
        case MapButtonRating:
            mapPage.rating = (mapPage.rating + 1) % mapsRatingCount;
            break;
        case MapButtonInstalled:
            mapPage.shown = (mapPage.shown + 1) % MapsShowCount;
            break;
        case MapButtonAction:
            if(mapsBusy())
            {
                // While a job runs the action is its Cancel.
                S_LocalSound(mapinstall::cancel() ? "misc/menu3.wav" : "misc/menu1.wav");
                return;
            }
            mapsPrimaryAt(mapsSelected());
            return; // (its own sound)
        default: return;
    }
    S_LocalSound("misc/menu1.wav");
}

void mapsTypeIn(char ch)
{
    if(ch == keyBackspace)
    {
        if(!mapPage.query.empty())
        {
            mapPage.query = za::String{za::StringView{mapPage.query}.substrByPosLen(0, mapPage.query.size() - 1)};
        }
    }
    else if(ch == keyClear)
    {
        mapPage.query = za::String{};
    }
    else if(ch >= 32 && ch < 127 && static_cast<int>(mapPage.query.size()) < mapsMaxQuery)
    {
        mapPage.query += ch;
    }
    else
    {
        return;
    }
    mapsShown();
}

// ----------------------------------------------------------------------------
// The page: input
// ----------------------------------------------------------------------------

// The sticks' selection: the keys (left), the packages (right), the bar (bottom).
void mapsMoveImpl(int dx, int dy);

// The sticks' selection moved; a package it lands on is picked (it stays the detail's as the selection moves on to the
// bar's Install / Play).
void mapsMove(int dx, int dy)
{
    mapsMoveImpl(dx, dy);
    if(mapPage.focusResult >= 0)
    {
        mapsPick(mapPage.focusResult);
    }
}

void mapsMoveImpl(int dx, int dy)
{
    const MapsLayout m = mapsLayout();
    const int keys = static_cast<int>(m.s.keys.size());
    const int results = static_cast<int>(mapPage.results.size());

    if(mapPage.focusButton >= 0)
    {
        if(dy < 0)
        {
            mapPage.focusButton = -1;
            mapPage.focusResult = mapPage.resultScroll;
            return;
        }
        if(dx != 0)
        {
            const int next = mapPage.focusButton + dx;
            if(next >= 0 && next < MapButtonCount)
            {
                mapPage.focusButton = next;
            }
        }
        return;
    }
    if(mapPage.focusResult >= 0)
    {
        if(dx < 0)
        {
            mapPage.focusResult = -1;
            mapPage.focusKey = 9; // the end of the keyboard's top row
            return;
        }
        if(dx > 0)
        {
            mapPage.focusResult = -1;
            mapPage.focusButton = 0;
            return;
        }
        mapPage.focusResult = CLAMP(0, mapPage.focusResult + dy, q_max(results - 1, 0));
        mapsScrollTo(mapPage.focusResult);
        return;
    }
    if(mapPage.focusKey < 0)
    {
        mapPage.focusKey = 0;
        return;
    }
    const SearchKey& at = m.s.keys[mapPage.focusKey];
    const float xc = (at.x0 + at.x1) * 0.5f;
    if(dx != 0)
    {
        const int next = mapPage.focusKey + dx;
        if(next < 0 || next >= keys || m.s.keys[next].yc != at.yc)
        {
            if(dx > 0 && results > 0)
            {
                mapPage.focusKey = -1;
                mapPage.focusResult = mapPage.resultScroll;
            }
            return;
        }
        mapPage.focusKey = next;
        return;
    }
    if(dy > 0)
    {
        // Past the keyboard's last row: the bar.
        bool below = false;
        for(const SearchKey& key : m.s.keys)
        {
            below = below || key.yc > at.yc;
        }
        if(below)
        {
            int best = -1;
            float bestRow = 1e9f, bestDx = 1e9f;
            for(int i = 0; i < keys; i++)
            {
                const float row = (m.s.keys[i].yc - at.yc) * static_cast<float>(dy);
                if(row <= 0.f)
                {
                    continue;
                }
                const float d = za::fabs((m.s.keys[i].x0 + m.s.keys[i].x1) * 0.5f - xc);
                if(row < bestRow - 0.5f || (row < bestRow + 0.5f && d < bestDx))
                {
                    best = i;
                    bestRow = row;
                    bestDx = d;
                }
            }
            if(best >= 0)
            {
                mapPage.focusKey = best;
                return;
            }
        }
        mapPage.focusKey = -1;
        mapPage.focusButton = 0;
        return;
    }
    // Up: the row above, or the corner's buttons from the top row.
    int best = -1;
    float bestRow = 1e9f, bestDx = 1e9f;
    for(int i = 0; i < keys; i++)
    {
        const float row = (m.s.keys[i].yc - at.yc) * static_cast<float>(dy);
        if(row <= 0.f)
        {
            continue;
        }
        const float d = za::fabs((m.s.keys[i].x0 + m.s.keys[i].x1) * 0.5f - xc);
        if(row < bestRow - 0.5f || (row < bestRow + 0.5f && d < bestDx))
        {
            best = i;
            bestRow = row;
            bestDx = d;
        }
    }
    if(best >= 0)
    {
        mapPage.focusKey = best;
    }
    else
    {
        mapPage.focusKey = -1;
        menuui::focusToolbar(-1);
    }
}

void mapsKey(int key)
{
    mapsShown(); // (the index replaced at the last frame's end, before this frame's draw: its results again first)
    const MapsLayout m = mapsLayout();
    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON:
        case K_MOUSE2:
        case K_MOUSE4: showPage(mapPage.from); S_LocalSound("misc/menu2.wav"); return;
        case K_BACKSPACE: mapsTypeIn(keyBackspace); return;
        case K_DEL: mapsTypeIn(keyClear); return;
        case K_UPARROW: mapsMove(0, -1); S_LocalSound("misc/menu1.wav"); return;
        case K_DOWNARROW: mapsMove(0, 1); S_LocalSound("misc/menu1.wav"); return;
        case K_LEFTARROW: mapsMove(-1, 0); S_LocalSound("misc/menu1.wav"); return;
        case K_RIGHTARROW: mapsMove(1, 0); S_LocalSound("misc/menu1.wav"); return;
        case K_MWHEELUP:
        case K_MWHEELDOWN:
            mapPage.resultScroll += key == K_MWHEELUP ? -1 : 1;
            mapsScrollTo(-1);
            return;
        case K_ENTER:
        case K_KP_ENTER:
            if(mapPage.focusButton >= 0)
            {
                mapsButtonAt(mapPage.focusButton);
            }
            else if(mapPage.focusResult >= 0)
            {
                mapsPrimaryAt(mapPage.focusResult);
            }
            else if(mapPage.focusKey >= 0)
            {
                mapsTypeIn(m.s.keys[mapPage.focusKey].ch);
                S_LocalSound("misc/menu1.wav");
            }
            else
            {
                mapsPrimaryAt(mapsSelected());
            }
            return;
        case K_ABUTTON:
            if(mapPage.focusButton >= 0)
            {
                mapsButtonAt(mapPage.focusButton);
            }
            else if(mapPage.focusResult >= 0)
            {
                mapsPrimaryAt(mapPage.focusResult);
            }
            else if(mapPage.focusKey >= 0)
            {
                mapsTypeIn(m.s.keys[mapPage.focusKey].ch);
                S_LocalSound("misc/menu1.wav");
            }
            return;
        case K_MOUSE1:
            for(int i = 0; i < static_cast<int>(m.s.keys.size()); i++)
            {
                if(keyHit(m.s, m.s.keys[i], m_mousex, m_mousey))
                {
                    mapsTypeIn(m.s.keys[i].ch);
                    S_LocalSound("misc/menu1.wav");
                    return;
                }
            }
            for(int b = 0; b < MapButtonCount; b++)
            {
                if(mapsButtonHit(m, b, m_mousex, m_mousey))
                {
                    mapsButtonAt(b);
                    return;
                }
            }
            if(const int r = mapsResultAt(m, m_mousex, m_mousey); r >= 0)
            {
                if(r == mapPage.picked)
                {
                    mapsPrimaryAt(r); // (a second click on the picked one: Install or Play, as its button)
                }
                else
                {
                    mapsPick(r);
                    S_LocalSound("misc/menu1.wav");
                }
            }
            return;
        default: return; // (typing: VR_Menu_Char)
    }
}

void mapsMouse(float x, float y)
{
    mapsShown(); // (as mapsKey)
    const MapsLayout m = mapsLayout();
    int key = -1;
    for(int i = 0; i < static_cast<int>(m.s.keys.size()) && key < 0; i++)
    {
        key = keyHit(m.s, m.s.keys[i], x, y) ? i : -1;
    }
    int button = -1;
    for(int b = 0; b < MapButtonCount && key < 0 && button < 0; b++)
    {
        button = mapsButtonHit(m, b, x, y) ? b : -1;
    }
    const int result = key >= 0 || button >= 0 ? -1 : mapsResultAt(m, x, y);
    if((key >= 0 && key != mapPage.hoverKey) || (result >= 0 && result != mapPage.hoverResult) ||
        (button >= 0 && button != mapPage.hoverButton))
    {
        mapPage.focusKey = mapPage.focusResult = mapPage.focusButton = -1; // the laser's now
        if(ui_mouse_sound.value)
        {
            S_LocalSound("misc/menu1.wav");
        }
    }
    mapPage.hoverKey = key;
    mapPage.hoverResult = result;
    mapPage.hoverButton = button;
}

// The page, from the corner's Maps button or Single Player > Map Library.
void openMapsPage()
{
    const int target = pageIndex(pageMaps);
    if(m_state != m_vr)
    {
        VR_Menu_Open();
    }
    else if(page == target)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    mapPage.from = page == target ? PageMain : page;
    mapsShown();
    mapPage.hoverKey = mapPage.hoverResult = mapPage.hoverButton = -1;
    mapPage.focusKey = 0;
    parentPage[target] = mapPage.from;
    showPage(target);
    S_LocalSound("misc/menu2.wav");
}

// maps_page_stats (Debug > Tools): the list is built once per change, not per frame.
void mapsPageStatsPrint()
{
    const MapsLayout m = mapsLayout();
    Con_Printf("map browser: %d search runs, %.1f ms total (%.2f ms each); %d frames drawn, %.1f ms total (%.3f ms "
               "each); %d results cached, %d packages in the index\n",
        mapPage.searchRuns, mapPage.searchMs,
        mapPage.searchRuns ? mapPage.searchMs / static_cast<double>(mapPage.searchRuns) : 0.0, mapPage.drawFrames,
        mapPage.drawMs, mapPage.drawFrames ? mapPage.drawMs / static_cast<double>(mapPage.drawFrames) : 0.0,
        static_cast<int>(mapPage.results.size()), static_cast<int>(mapindex::index().entries.size()));
    Con_Printf("map browser layout: %.0f..%.0f across, keyboard %.0f, list %.0f, %d rows, rows %.0f..%.0f, bar y %.0f "
               "action %.0f..%.0f\n",
        m.s.left, m.s.right, m.s.split - m.s.left, m.s.right - m.s.split, mapPage.visibleResults, m.s.resultsTop,
        m.listBottom, m.bar.yc, m.bar.x0[MapButtonAction], m.bar.x1[MapButtonAction]);
}

// ----------------------------------------------------------------------------
// The page: drawing
// ----------------------------------------------------------------------------

// Text in `size`-pixel letters from x, no wider than `width`, wrapped over at most `maxLines` lines. On one line its end
// is kept (the tail of a long line is what matters for a title or an author); over more, its start.
int drawMapsWrapped(const char* text, float x, float y, float size, float width, int maxLines)
{
    const int fit = q_max(static_cast<int>(width / size), 8);
    const int n = static_cast<int>(strlen(text));
    const char* start = n > fit && maxLines == 1 ? text + n - fit : text; // one line: its end
    int lines = 0;
    for(const char* p = start; *p && lines < maxLines;)
    {
        int take = 0;
        while(p[take] && take < fit)
        {
            take++;
        }
        if(p[take])
        {
            int cut = take;
            while(cut > 0 && p[cut - 1] != ' ')
            {
                cut--;
            }
            if(cut > 0)
            {
                take = cut;
            }
        }
        for(int i = 0; i < take; i++)
        {
            Draw_CharacterEx(x + size * static_cast<float>(i), y + static_cast<float>(lines) * (size + 2.f), size, size,
                static_cast<unsigned char>(p[i]) | 128);
        }
        p += take;
        while(*p == ' ')
        {
            p++;
        }
        lines++;
    }
    return lines;
}

// A button of the bottom bar.
void drawMapsButton(const menupaint::Painter& p, const MapsLayout& m, int button, const char* label, bool hot,
    bool enabled)
{
    namespace colors = menupaint::colors;
    p.rounded(m.bar.x0[button], m.bar.x1[button], m.bar.yc, m.bar.half, 3.f,
        hot ? colors::highlightEdge : colors::boxBorder);
    p.rounded(m.bar.x0[button] + 1.f, m.bar.x1[button] - 1.f, m.bar.yc, m.bar.half - 1.f, 2.f,
        hot ? colors::buttonHover : colors::boxFill);
    // Six-pixel letters, five where that fits the label whole (the bar holds five filters and the action).
    const float room = m.bar.x1[button] - m.bar.x0[button] - 10.f;
    const float size = 6.f * static_cast<float>(strlen(label)) <= room ? 6.f : 5.f;
    searchText(m.bar.x0[button] + 5.f, m.bar.yc - size * 0.5f - 1.f, size, room, label, hot && enabled);
    if(!enabled)
    {
        const float y = m.bar.yc;
        p.rect(m.bar.x0[button] + 3.f, m.bar.x1[button] - 3.f, y, 0.7f, colors::switchOff);
    }
}

// The picked package: its description, file count and size (the left column, under the keys).
void drawMapsDetail(const MapsLayout& m, const menupaint::Painter& p)
{
    namespace colors = menupaint::colors;
    const float x = m.s.left;
    const float width = m.s.split - 6.f - m.s.left;
    const float bottom = m.listBottom;
    float y = m.detailTop;
    const int r = mapsSelected();
    if(r < 0)
    {
        const char* help = mapPage.results.empty()
                               ? "Type with the keys (or a keyboard). The filters along the bottom choose what kind of "
                                 "package is listed and the order. Packages that bring their own progs.dat are left out "
                                 "(vr_maps_allow_progs, Debug > Tools)."
                               : "Point at a package, or select one with the sticks, to read it here. Enter installs it, "
                                 "or plays it when it is installed already. Back leaves the page.";
        drawMapsWrapped(help, x, y, 6.f, width, 8);
        return;
    }
    const mapindex::Entry& e = *mapPage.results[r];
    const mapindex::Index& idx = mapindex::index();
    const za::String sha{idx.field(e.sha256)};
    const bool have = mapinstall::installed(sha);

    mapsQuake(e.title, mapsText.title, sizeof(mapsText.title));
    const float titleSize = za::fmax(5.f, za::fmin(8.f, width / q_max(static_cast<float>(strlen(mapsText.title)), 1.f)));
    searchText(x, y, titleSize, width, mapsText.title, true);
    y += titleSize + 4.f;

    mapsQuake(e.author, mapsText.author, sizeof(mapsText.author));
    q_snprintf(mapsText.meta, sizeof(mapsText.meta), "%s", mapsText.author[0] ? mapsText.author : "unknown author");
    drawMapsWrapped(mapsText.meta, x, y, 6.f, width, 1);
    y += 10.f;

    const za::String sizeLabel = mapinstall::formatBytes(e.bytes);
    char rated[48] = "";
    if(e.userRating)
    {
        q_snprintf(rated, sizeof(rated), "  rated %.1f/5", static_cast<double>(e.userRating) / 100.0);
    }
    else if(e.rating)
    {
        q_snprintf(rated, sizeof(rated), "  rated %d/5", static_cast<int>(e.rating));
    }
    q_snprintf(mapsText.meta, sizeof(mapsText.meta), "%s  %s  %s  %d files  %s%s%s",
        idx.field(e.date) ? idx.field(e.date) : "undated",
        idx.field(e.types) ? idx.field(e.types) : "type?",
        idx.field(e.sizes) ? idx.field(e.sizes) : "size?", e.files, sizeLabel.cStr(), rated, have ? "  INSTALLED" : "");
    drawMapsWrapped(mapsText.meta, x, y, 6.f, width, 2);
    y += 14.f;

    // What Play starts (or why it cannot), and the mod it was made for: said here, not only in the console.
    const int installedNow = static_cast<int>(mapinstall::installedList().size());
    if(mapPage.notesSha != sha || mapPage.notesGeneration != mapindex::generation() ||
        mapPage.notesInstalled != installedNow)
    {
        mapPage.notesSha = sha;
        mapPage.notesGeneration = mapindex::generation();
        mapPage.notesInstalled = installedNow;
        mapPage.notes = za::String{};
        if(have)
        {
            za::String why;
            int count = 0;
            const za::String start = mapinstall::startMap(sha, &why, &count);
            mapPage.notes = start.empty() ? za::String{"Cannot be played: "} + why + "."
                            : count > 1  ? za::String{"Play starts "} + start + " (one of its " + za::toString(count) +
                                              " maps; maps_play in the console starts another)."
                                         : za::String{"Play starts "} + start + ".";
        }
        const za::String mod = mapinstall::madeFor(e);
        if(mod.size())
        {
            mapPage.notes += za::String{mapPage.notes.empty() ? "" : " "} + "Made for " + mod +
                             ", whose own progs Quake VR does not run: its monsters, items and effects are missing.";
        }
    }
    const za::String& problem = mapinstall::playProblem(sha);
    const char* notes = problem.size() ? problem.cStr() : mapPage.notes.cStr();
    if(*notes)
    {
        const int lines = drawMapsWrapped(notes, x, y, 6.f, width, 4);
        y += static_cast<float>(lines) * 8.f + 4.f;
    }

    mapsQuake(e.description, mapsText.detail, sizeof(mapsText.detail));
    if(mapsText.detail[0])
    {
        const int maxLines = q_max(static_cast<int>((bottom - y) / 8.f), 0);
        drawMapsWrapped(mapsText.detail, x, y, 6.f, width, maxLines);
    }
    else
    {
        q_snprintf(mapsText.meta, sizeof(mapsText.meta), "%s",
            have ? "Installed: press Enter, or Play below, to start it." : "Press Enter, or Install below, to get it.");
        drawMapsWrapped(mapsText.meta, x, y, 6.f, width, 2);
    }
    (void)p;
}

void drawMapsPage()
{
    const double t0 = Sys_DoubleTime();
    mapsShown();
    const MapsLayout m = mapsLayout();
    const menupaint::Painter p;
    namespace colors = menupaint::colors;
    const Layout l = layout();

    qpic_t* title = Draw_CachePic("gfx/p_option.lmp");
    M_DrawPic((320 - title->width) / 2, l.top + 4, title);
    M_PrintWhite((320 - 8 * 11) / 2, l.top + 28, "Map Library");

    // The text box: what is typed, a cursor after it.
    p.rounded(m.s.left, m.s.split - 6.f, m.s.boxY, m.s.half, 3.f, colors::boxBorder);
    p.rounded(m.s.left + 1.f, m.s.split - 7.f, m.s.boxY, m.s.half - 1.f, 2.f, colors::boxFill);
    const float textWidth = m.s.split - 6.f - m.s.left - 12.f;
    if(mapPage.query.empty())
    {
        searchText(m.s.left + 5.f, m.s.boxY - 4.f, 8.f, textWidth, "Title or author...", false);
    }
    else
    {
        const int fit = static_cast<int>(textWidth / 8.f) - 1;
        const int n = static_cast<int>(mapPage.query.size());
        const char* shown = mapPage.query.cStr() + q_max(n - fit, 0);
        searchText(m.s.left + 5.f, m.s.boxY - 4.f, 8.f, textWidth, shown, true);
        if(static_cast<int>(realtime * 2.0) % 2 == 0)
        {
            Draw_CharacterEx(m.s.left + 5.f + 8.f * static_cast<float>(strlen(shown)), m.s.boxY - 4.f, 8.f, 8.f, 11);
        }
    }

    // The keys (the Search page's).
    for(int i = 0; i < static_cast<int>(m.s.keys.size()); i++)
    {
        const SearchKey& key = m.s.keys[i];
        const bool hot = mapPage.hoverKey == i || mapPage.focusKey == i;
        p.rounded(key.x0, key.x1, key.yc, m.s.half, 3.f, hot ? colors::highlightEdge : colors::boxBorder);
        p.rounded(key.x0 + 1.f, key.x1 - 1.f, key.yc, m.s.half - 1.f, 2.f, hot ? colors::buttonHover : colors::boxFill);
        // A key's letters, shrunk to its width (the page is narrower than Search's, so the wide keys are named short).
        const char* label = key.label;
        if(key.ch == ' ')
        {
            label = "SPC";
        }
        else if(key.ch == keyBackspace)
        {
            label = "DEL";
        }
        else if(key.ch == keyClear)
        {
            label = "CLR";
        }
        const float keyWidth = key.x1 - key.x0 - 2.f;
        const float labelSize = za::fmax(5.f, za::fmin(8.f, keyWidth / q_max(static_cast<float>(strlen(label)), 1.f)));
        const float w = labelSize * static_cast<float>(strlen(label));
        searchText((key.x0 + key.x1 - w) * 0.5f, key.yc - labelSize * 0.5f, labelSize, keyWidth, label, hot); // (centred)
    }

    drawMapsDetail(m, p);

    // The packages: title, and under it its date, type, size, download and author.
    const float x0 = m.s.split + 2.f;
    const float width = m.s.right - x0;
    mapPage.visibleResults = q_max(static_cast<int>((m.listBottom - m.s.resultsTop) / mapsRowHeight), 1);
    const int n = static_cast<int>(mapPage.results.size());
    mapsScrollTo(-1);
    if(n > mapPage.visibleResults)
    {
        char more[96];
        if(mapPage.hiddenProgs)
        {
            q_snprintf(more, sizeof(more), "%d-%d of %d  %d hidden: their own progs.dat", mapPage.resultScroll + 1,
                q_min(mapPage.resultScroll + mapPage.visibleResults, n), n, mapPage.hiddenProgs);
        }
        else
        {
            q_snprintf(more, sizeof(more), "%d-%d of %d packages", mapPage.resultScroll + 1,
                q_min(mapPage.resultScroll + mapPage.visibleResults, n), n);
        }
        searchText(x0, m.s.resultsTop - 7.f, 5.f, width, more, false);
    }
    if(n == 0)
    {
        if(mapindex::index().entries.empty())
        {
            // Why there is nothing yet: the fetch's latest step, or how it ended (all of it in the console too).
            searchText(x0, m.s.resultsTop + 2.f, 6.f, width, "The index has not arrived yet:", false);
            const za::String state = mapindex::state();
            drawMapsWrapped(state.cStr(), x0, m.s.resultsTop + 10.f, 5.f, width, 6);
        }
        else
        {
            searchText(x0, m.s.resultsTop + 2.f, 6.f, width,
                mapPage.shown == MapsShowInstalled && mapinstall::installedList().empty()
                    ? "Nothing installed from the Map Library yet"
                    : "Nothing matches",
                false);
        }
    }
    for(int r = mapPage.resultScroll; r < n && r < mapPage.resultScroll + mapPage.visibleResults; r++)
    {
        const mapindex::Entry& e = *mapPage.results[r];
        const mapindex::Index& idx = mapindex::index();
        const float y = m.s.resultsTop + mapsRowHeight * static_cast<float>(r - mapPage.resultScroll);
        const bool hot = mapPage.hoverResult == r || mapPage.focusResult == r || mapPage.picked == r;
        const bool have = mapinstall::installed(za::String{idx.field(e.sha256)});
        const bool working = mapinstall::busy(za::String{idx.field(e.sha256)});
        if(mapPage.picked == r)
        {
            p.rounded(x0 - 3.f, m.s.right + 1.f, y + 7.5f, 9.5f * m.s.k, 2.f, colors::highlightEdge); // (picked: an edge)
        }
        if(hot)
        {
            p.rounded(x0 - 2.f, m.s.right, y + 7.5f, 8.5f * m.s.k, 2.f, colors::listHover);
        }
        if(have)
        {
            p.rect(x0 - 2.f, x0 + 1.5f, y + 7.5f, 6.f * m.s.k, colors::marker); // installed
        }
        mapsQuake(e.title, mapsText.title, sizeof(mapsText.title));
        const float nameSize = za::fmax(5.f, za::fmin(8.f, (width - 8.f) / q_max(static_cast<float>(strlen(mapsText.title)), 1.f)));
        searchText(x0 + 4.f, y + (8.f - nameSize), nameSize, width - 8.f, mapsText.title, true);

        const za::String sizeLabel = mapinstall::formatBytes(e.bytes);
        mapsQuake(e.author, mapsText.author, sizeof(mapsText.author));
        q_snprintf(mapsText.meta, sizeof(mapsText.meta), "%s %s %s %s %d files  %s%s",
            idx.field(e.date), idx.field(e.types) ? idx.field(e.types) : "?",
            idx.field(e.sizes) ? idx.field(e.sizes) : "?", sizeLabel.cStr(), e.files, mapsText.author,
            working ? "  ..." : (have ? "  installed" : ""));
        const int fit = static_cast<int>(width / 5.f);
        const int metaLen = static_cast<int>(strlen(mapsText.meta));
        searchText(x0 + 4.f, y + 9.f, 5.f, width - 4.f, metaLen > fit ? mapsText.meta + metaLen - fit : mapsText.meta,
            false);
    }

    // The bottom bar: the filters, and Install or Play for the picked package (its progress while a job runs).
    const int r = mapsSelected();
    const bool have = r >= 0 && mapinstall::installed(za::String{mapindex::index().field(mapPage.results[r]->sha256)});
    for(int b = 0; b < MapButtonAction; b++)
    {
        const bool hot = mapPage.hoverButton == b || mapPage.focusButton == b;
        // Short labels (five filters share the bar): the choice alone where it names itself.
        if(b == MapButtonSort)
        {
            q_snprintf(mapsText.label, sizeof(mapsText.label), "%s", mapsSorts[mapPage.sort].label);
        }
        else if(b == MapButtonType)
        {
            q_snprintf(mapsText.label, sizeof(mapsText.label), "%s",
                mapPage.typeIdx > 0 ? mapPage.typeFacets[mapPage.typeIdx - 1].value.cStr() : "any type");
        }
        else if(b == MapButtonRating)
        {
            q_snprintf(mapsText.label, sizeof(mapsText.label), "%s", mapsRatings[mapPage.rating].label);
        }
        else if(b == MapButtonInstalled)
        {
            q_snprintf(mapsText.label, sizeof(mapsText.label), "%s", mapsShownLabels[mapPage.shown]);
        }
        else
        {
            q_snprintf(mapsText.label, sizeof(mapsText.label), "%s",
                mapPage.sizeIdx > 0 ? mapPage.sizeFacets[mapPage.sizeIdx - 1].value.cStr() : "any size");
        }
        drawMapsButton(p, m, b, mapsText.label, hot, true);
    }

    if(mapsBusy())
    {
        // The job's progress under Cancel (the status line below says what it is doing).
        const int pct = mapinstall::percent();
        const bool hot = mapPage.hoverButton == MapButtonAction || mapPage.focusButton == MapButtonAction;
        const float x = m.bar.x0[MapButtonAction], x1 = m.bar.x1[MapButtonAction];
        p.rounded(x, x1, m.bar.yc, m.bar.half, 3.f, colors::highlightEdge);
        p.rounded(x + 1.f, x1 - 1.f, m.bar.yc, m.bar.half - 1.f, 2.f, hot ? colors::buttonHover : colors::boxFill);
        const float span = x1 - x - 10.f;
        p.rect(x + 5.f, x + 5.f + span * 0.01f * static_cast<float>(pct), m.bar.yc + 4.f / m.s.k, 2.5f,
            colors::scrollThumb);
        if(mapinstall::cancelling())
        {
            q_snprintf(mapsText.label, sizeof(mapsText.label), "stopping...");
        }
        else
        {
            q_snprintf(mapsText.label, sizeof(mapsText.label), "Cancel (%s %d%%)",
                mapinstall::job().phase == mapinstall::Phase::Extract ? "unpacking" : "downloading", pct);
        }
        searchText(x + 5.f, m.bar.yc - 4.f, 6.f, span, mapsText.label, true);
    }
    else
    {
        const bool hot = mapPage.hoverButton == MapButtonAction || mapPage.focusButton == MapButtonAction;
        if(r >= 0)
        {
            const za::String sizeLabel = mapinstall::formatBytes(mapPage.results[r]->bytes);
            q_snprintf(mapsText.label, sizeof(mapsText.label), have ? "Play" : "Install %s", sizeLabel.cStr());
            drawMapsButton(p, m, MapButtonAction, mapsText.label, hot, true);
        }
        else
        {
            drawMapsButton(p, m, MapButtonAction, "Install / Play", hot, false);
        }
    }

    // What the downloads are doing, always: the running job (what, how far, the mirror, the seconds), or idle with how
    // the last one ended (mapinstall::statusLine; maps_status prints the same).
    const za::String jobLine = mapinstall::statusLine();
    searchText(m.s.left, m.bar.yc + (m.bar.half + 6.f) / m.s.k, 6.f, m.s.right - m.s.left, jobLine.cStr(), false);

    mapPage.drawFrames++;
    mapPage.drawMs += (Sys_DoubleTime() - t0) * 1000.0;
}
