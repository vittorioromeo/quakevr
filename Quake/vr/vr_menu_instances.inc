// vr_menu_instances.inc -- menu instances (the toolgun's and the corner's menus), the toolgun's cheats, jumps to pages, scrolling.
// Split from vr_menu.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

bool qvr::menu::bodyClonePage()
{
    if(key_dest != key_menu || m_state != m_vr || ui().page < 0 || ui().page >= pageCount)
    {
        ui().bodyCloneLatch = -1;
        return false;
    }
    const PageRef b = PageRef::at(ui().page);
    if(vr_menu_body_clone.value >= 2.f || b == pageFlashlightMounts || b == pageWofsHolstered || b == pageVrsRangerHead ||
        b == pageHolsterZones || b == pageSeatedHolsters || b == pageVrsBodyEquipment || b == pageVrsHolsters ||
        ui().bodyCloneLatch == ui().page)
    {
        return true;
    }
    const cvar_t* chosen = selectedSetting();
    ui().bodyCloneLatch = chosen && tweaksBody(chosen->name) ? ui().page : -1;
    return ui().bodyCloneLatch == ui().page;
}

int qvr::menu::currentPage()
{
    return ui().page;
}

int qvr::menu::currentInstance()
{
    return currentMenu;
}

void qvr::menu::selectInstance(int instance)
{
    instance = CLAMP(0, instance, instanceCount - 1);
    if(instance == currentMenu)
    {
        return;
    }
    // Quake's menu state and the menus' mouse go with the instance (MenuInstance::quakeState).
    ui().quakeState = static_cast<int>(m_state);
    ui().mouseX = m_mousex;
    ui().mouseY = m_mousey;
    currentMenu = instance;
    m_state = static_cast<m_state_e>(ui().quakeState);
    m_mousex = ui().mouseX;
    m_mousey = ui().mouseY;
    M_RefreshBounds(); // (its menu's layout: the lists' mouse)
}

bool qvr::menu::instanceOpen(int instance)
{
    if(instance <= 0)
    {
        return key_dest == key_menu && mainState() != m_none;
    }
    // (Hidden while the menu is closed: the panels stay open in their places, shown again when it opens, MULTI_MENU.md.)
    return instance < instanceCount && menuInstances[instance].open && key_dest == key_menu && mainState() != m_none;
}

int qvr::menu::openInstances()
{
    int n = 0;
    for(int i = 0; i < instanceCount; i++)
    {
        n += instanceOpen(i) ? 1 : 0;
    }
    return n;
}

int qvr::menu::mainState()
{
    return currentMenu == 0 ? static_cast<int>(m_state) : menuInstances[0].quakeState;
}

bool qvr::menu::openInstance()
{
    if(key_dest != key_menu || m_state == m_none)
    {
        return false;
    }
    glm::vec3 corner, xAxis, yAxis;
    if(!panel::menuQuad(hands::current(), 0, corner, xAxis, yAxis))
    {
        Con_Printf("New Panel: only with the menu in the headset\n");
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return false;
    }
    int slot = -1;
    for(int i = 1; i < instanceCount && slot < 0; i++)
    {
        slot = menuInstances[i].open ? -1 : i;
    }
    if(slot < 0)
    {
        Con_Printf("New Panel: %d panels open at most\n", instanceCount);
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return false;
    }

    // The page shown here (not one for one panel at a time), else the VR Settings; each page's selection and scroll as
    // here, nothing under way.
    const int target = (m_state == m_vr || m_state == m_confirm) && !oneViewerGroup(ui().page) ? ui().page : PageMain;
    MenuInstance& n = menuInstances[slot];
    const MenuInstance& from = ui();
    n = MenuInstance{};
    for(int p = 0; p < pageCount; p++)
    {
        n.cursors[p] = from.cursors[p];
        n.scrolls[p] = from.scrolls[p];
        n.leftAnchors[p] = from.leftAnchors[p];
        n.visits[p] = from.visits[p];
    }
    n.visitCount = from.visitCount;
    n.open = true;
    n.quakeState = m_vr;
    n.page = PageMain;

    selectInstance(slot);
    navReset();
    if(target == PageMain)
    {
        navPush(PageMain);
        showPage(PageMain);
    }
    else
    {
        openInTree(target);
    }
    panel::openMenuPanel(slot);
    qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
    Con_DPrintf("menu panel %d opened: %s\n", slot + 1, pages[ui().page].title);
    return true;
}

namespace
{
void closeQuietly(int instance)
{
    if(instance <= 0 || instance >= menu::instanceCount || !menuInstances[instance].open)
    {
        return;
    }
    panel::closeMenuPanel(instance);
    if(currentMenu == instance)
    {
        menu::selectInstance(0);
    }
    menuInstances[instance].open = false;
    menuInstances[instance].quakeState = m_none;
}
} // namespace

void qvr::menu::closeInstance(int instance)
{
    if(instance > 0 && instance < instanceCount && menuInstances[instance].open)
    {
        closeQuietly(instance);
        qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
    }
}

void qvr::menu::closeExtraInstances()
{
    for(int i = 1; i < instanceCount; i++)
    {
        closeQuietly(i);
    }
}

void qvr::menu::tidyInstances()
{
    bool extras = false;
    for(int i = 1; i < instanceCount; i++)
    {
        extras = extras || menuInstances[i].open;
    }
    if(!extras)
    {
        return;
    }
    if(key_dest != key_menu || mainState() == m_none)
    {
        // The menu closed (from whichever panel): the panels are hidden, not closed (instanceOpen), each keeping its
        // page and place; the first is the current one again.
        if(currentMenu != 0)
        {
            const bool none = m_state == m_none;
            m_state = m_vr;
            selectInstance(0);
            if(none)
            {
                m_state = m_none;
            }
        }
        return;
    }
    if(currentMenu != 0 && m_state != m_vr && m_state != m_confirm && m_state != m_maps)
    {
        // Out of the VR pages to one of Quake's menus (single: on the first panel; the Levels, from the corner's button,
        // stay on the panel pressed on); this panel stays on its page.
        const m_state_e state = m_state;
        m_state = m_vr;
        selectInstance(0);
        m_state = state;
    }
}

void qvr::menu::drawInstance(int instance)
{
    const int was = currentMenu;
    drawnInstance = instance;
    selectInstance(instance);
    M_DrawPanel();
    selectInstance(was);
    drawnInstance = -1;
}

void qvr::menu::panels_f()
{
    const char* arg = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    const int n = Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) - 1 : -1;
    if(!q_strcasecmp(arg, "open"))
    {
        openInstance();
        return;
    }
    if(!q_strcasecmp(arg, "close") && n >= 1 && n < instanceCount)
    {
        closeInstance(n);
        return;
    }
    if(!q_strcasecmp(arg, "focus") && n >= 0 && n < instanceCount && instanceOpen(n))
    {
        selectInstance(n);
        return;
    }
    if(arg[0])
    {
        Con_Printf("vr_menu_panels [open | close <2..%d> | focus <1..%d>]: the menu panels open (MENUPANELS), or another opened, "
                   "one closed or focused (the first closes with Back to game)\n",
            instanceCount, instanceCount);
        return;
    }
    Con_Printf("MENUPANELS open %d focused %d\n", openInstances(), currentMenu + 1);
    const int was = currentMenu;
    for(int i = 0; i < instanceCount; i++)
    {
        if(!instanceOpen(i))
        {
            continue;
        }
        selectInstance(i);
        const bool vrPage = m_state == m_vr;
        const auto& list = items(ui().page);
        const int cursor = ui().cursors[ui().page];
        const char* row = vrPage && cursor >= 0 && cursor < static_cast<int>(list.size()) ? rowLabel(list[cursor]) : "";
        Con_Printf("MENUPANEL %d menu %d page %d \"%s\" row \"%s\" value %g drawn %d\n", i + 1, static_cast<int>(m_state),
            vrPage ? ui().page : -1, vrPage ? pages[ui().page].title : "", row, ui().drawnValue,
            ui().drawnFrame >= 0 ? host_framecount - ui().drawnFrame : -1);
    }
    selectInstance(was);
}

extern "C" int VR_MenuDrawSelect()
{
    if(instanceDrawDepth++ > 0 || drawnInstance >= 0)
    {
        return -1;
    }
    menu::tidyInstances();
    if(currentMenu == 0 || !panel::menusInEyes())
    {
        return -1; // (on a flat screen, or the runtime's own panel: the focused panel's menu only)
    }
    const int was = currentMenu;
    menu::selectInstance(0);
    return was;
}

extern "C" int VR_MenuDrawRestore(int was)
{
    instanceDrawDepth--;
    if(was < 0)
    {
        return 0;
    }
    menu::selectInstance(was);
    return 1;
}

int qvr::menu::toolgunCheatCount()
{
    findToolgunCheats();
    return static_cast<int>(toolgunCheats.list.size());
}

const char* qvr::menu::toolgunCheatName(int i)
{
    return i >= 0 && i < toolgunCheatCount() ? toolgunCheats.list[i].name : "";
}

bool qvr::menu::toolgunCheatOn(int i)
{
    return i >= 0 && i < toolgunCheatCount() && toolgunCheatIsOn(toolgunCheats.list[i]);
}

void qvr::menu::toolgunCheatUse(int i)
{
    if(i < 0 || i >= toolgunCheatCount())
    {
        return;
    }
    const ToolgunCheat& c = toolgunCheats.list[i];
    if(c.cvar)
    {
        Cvar_SetValueQuick(c.cvar, c.cvar->value != 0.f ? 0.f : 1.f);
    }
    if(c.command)
    {
        runCommand(c.command);
    }
    if(c.action)
    {
        c.action();
    }
    Con_Printf("toolgun: %s\n", c.name);
}

const cvar_t* qvr::menu::selectedSetting()
{
    if(m_state != m_vr)
    {
        return nullptr;
    }
    const auto& list = items(ui().page);
    const int cursor = ui().cursors[ui().page];
    return cursor >= 0 && cursor < static_cast<int>(list.size()) ? list[cursor].cvar : nullptr;
}

void qvr::menu::jumpToAdvanced()
{
    if(menuLevel() < LevelAdvanced)
    {
        Cvar_SetValueQuick(&vr_menu_level, static_cast<float>(LevelAdvanced)); // (the corner's Advanced VR: the pages it lists)
    }
    if(m_state == m_vr && ui().page == PageAdvanced)
    {
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return;
    }
    openFromAnywhere(PageAdvanced); // (Back: where it was pressed)
}

void qvr::menu::openSearch()
{
    openSearchPage();
}

void qvr::menu::openConsole()
{
    openConsolePage();
}

// The Map Library, from the corner's Maps button or Single Player > Map Library.
void qvr::menu::openMaps()
{
    openMapsPage();
}

// maps_page [text]: the Map Library page, with the text typed in (as vr_menu_search is for the Search page).
void qvr::menu::mapsPage_f()
{
    mapPage.query = Cmd_Argc() >= 2 ? za::String{Cmd_Args()} : za::String{};
    openMapsPage();
}

// maps_page_stats (Debug > Tools).
void qvr::menu::mapsPageStats_f()
{
    mapsPageStatsPrint();
}

// vr_appearance_random: Player Appearance > Randomize, from the console (tests).
void qvr::menu::randomAppearance_f()
{
    if(Cmd_Argc() > 1)
    {
        menu_detail::randomState = static_cast<uint32_t>(Q_atoi(Cmd_Argv(1))) | 1u;
    }
    menu_detail::randomAppearance();
}

// vr_menu_search <text>: the Search page's results for the text, best first, with their scores and pages.
// vr_menu_slider_step <cvar> <steps> [fine]: the slider of `cvar` (the first on the VR menus' pages) stepped that many
// times (negative: down) as the menu's keys step it, with the fine modifier held if "fine"; each value printed as the
// cvar holds it and as the slider shows it (the plain steps' round values: the author's note
// vrfiringrange_2026-10-07_22-23-40).
void qvr::menu::sliderStep_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_menu_slider_step <cvar> <steps> [fine]\n");
        return;
    }
    const char* name = Cmd_Argv(1);
    const int steps = Q_atoi(Cmd_Argv(2));
    const bool fine = Cmd_Argc() > 3 && !q_strcasecmp(Cmd_Argv(3), "fine");
    const int was = levelOverride;
    levelOverride = LevelDeveloper;
    for(int p = 0; p < pageCount; p++)
    {
        if(p == pageIndex(pageSearch) || p == pageIndex(pageChanged))
        {
            continue;
        }
        for(const Item& found : items(p))
        {
            if(found.kind != Item::Slider || !found.cvar || q_strcasecmp(found.cvar->name, name))
            {
                continue;
            }
            const Item item = found; // (change may build the page again)
            fineForced = fine;
            for(int i = 0; i < za::abs(steps); i++)
            {
                change(item, steps > 0 ? 1 : -1);
                char text[64];
                sliderText(item, item.cvar->value, text, sizeof(text));
                Con_Printf("vr_menu_slider_step: %s \"%s\" shown %s\n", item.cvar->name, item.cvar->string, text);
            }
            fineForced = false;
            levelOverride = was;
            return;
        }
    }
    levelOverride = was;
    Con_Printf("vr_menu_slider_step: no slider of %s\n", name);
}

void qvr::menu::search_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_menu_search <text>: the VR menus' settings, actions and pages matching it, best first\n");
        return;
    }
    buildSearchIndex();
    search.query = Cmd_Args();
    runSearch();
    Con_Printf("vr_menu_search \"%s\": %d results (of %d rows and pages)\n", search.query.cStr(),
        static_cast<int>(search.results.size()), static_cast<int>(search.index.size()));
    for(int r = 0; r < static_cast<int>(search.results.size()) && r < 15; r++)
    {
        const SearchEntry& e = search.index[search.results[r].entry];
        Con_Printf("%2d %.2f %s%s | %s%s%s\n", r + 1, search.results[r].score, e.label.cStr(), e.isPage ? " (page)" : "",
            e.path.cStr(), search.results[r].byCvar ? va(": %s", e.lowCvar.cStr()) : "",
            e.level > menuLevel() ? va(" (%s)", levelName(e.level)) : "");
    }
}

float qvr::menu::contentLeft()
{
    if(m_state != m_vr)
    {
        return static_cast<float>(M_ContentLeft());
    }
    const auto build = PageRef::at(ui().page);
    if(build == pageSearch || build == pageMaps || build == pageConsole)
    {
        return 0.f; // (Search and the Map Library from x 12 at least; the console's page right of the buttons)
    }
    return static_cast<float>(q_min(pageRowsLeft(), (320 - 8 * helpColumns()) / 2));
}

float qvr::menu::contentRightBelow(float y)
{
    if(m_state != m_vr)
    {
        return M_ContentRightBelow(y);
    }
    const auto build = PageRef::at(ui().page);
    const Layout l = layout();
    if(build == pageConsole || build == pageSearch || build == pageMaps)
    {
        return y < static_cast<float>(l.bottom) ? pageRight(build) : -1e9f; // (their keyboards and buttons down to the bottom)
    }
    // The rows down to the help's box (or the bottom), as far right as their scrollbar's box (scrollbarX + 12); the help
    // centred at its widest (helpColumns), its paging bar right of it (drawHelp).
    const za::Vector<Item>& list = menuPages.built[ui().page];
    const bool help = hasHelp(list);
    float right = -1e9f;
    if(y < static_cast<float>(help ? l.helpTop - 4 : l.bottom - 8))
    {
        right = static_cast<float>(midPos + 200);
    }
    if(help && y < static_cast<float>(l.helpTop + 8 * helpBoxLines()))
    {
        right = za::fmax(right, static_cast<float>((320 + 8 * helpColumns()) / 2 + 5));
    }
    return right;
}

float qvr::menu::contentLeftBelow(float y)
{
    if(m_state != m_vr)
    {
        return M_ContentLeftBelow(y);
    }
    const auto build = PageRef::at(ui().page);
    const Layout l = layout();
    if(build == pageConsole || build == pageSearch || build == pageMaps)
    {
        return y < static_cast<float>(l.bottom) ? contentLeft() : 1e9f; // (down to the bottom)
    }
    // As contentRightBelow: the rows down to the help's box (or the bottom), the help under them.
    const za::Vector<Item>& list = menuPages.built[ui().page];
    const bool help = hasHelp(list);
    float left = 1e9f;
    if(y < static_cast<float>(help ? l.helpTop - 4 : l.bottom - 8))
    {
        left = static_cast<float>(pageRowsLeft());
    }
    if(help && y < static_cast<float>(l.helpTop + 8 * helpBoxLines()))
    {
        left = za::fmin(left, static_cast<float>((320 - 8 * helpColumns()) / 2 - 5));
    }
    return left;
}

bool qvr::menu::developerLevel()
{
    return menuLevel() >= LevelDeveloper;
}

void qvr::menu::jumpToChecklist()
{
    const int target = pageIndex(pageChecklist);
    checklist::refresh(true);
    if(m_state == m_vr && ui().page == target)
    {
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return;
    }
    openFromAnywhere(target);
}

void qvr::menu::jumpToSettings()
{
    if(m_state == m_vr && ui().page == PageMain)
    {
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return;
    }
    openFromAnywhere(PageMain);
}

// (The VR pages' Back stack started again: the main menu is the top, its Back the game.)
void qvr::menu::jumpToMainMenu()
{
    if(m_state == m_main && key_dest == key_menu)
    {
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return;
    }
    navReset();
    M_Menu_Main_f();
}

void qvr::menu::jumpToRagtool()
{
    const int target = pageIndex(pageRagtool);
    if(m_state == m_vr && key_dest == key_menu && ui().page == target)
    {
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return;
    }
    openFromAnywhere(target);
}

void qvr::menu::jumpToRelighting()
{
    const int target = pageIndex(pageGraphicsRelighting);
    if(menuLevel() < pages[target].level)
    {
        Cvar_SetValueQuick(&vr_menu_level, static_cast<float>(pages[target].level)); // (as the corner's Advanced VR)
    }
    if(m_state == m_vr && ui().page == target)
    {
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return;
    }
    openFromAnywhere(target); // (Back: where it was pressed)
}

void qvr::menu::jumpToRecording()
{
    const int target = pageIndex(pageRecording);
    if(m_state == m_vr && ui().page == target)
    {
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return;
    }
    openFromAnywhere(target); // (Back: where it was pressed)
}

void qvr::menu::selectEnd(int dir)
{
    if(m_state == m_vr && ui().page == pageIndex(pageSearch))
    {
        search.focusKey = 0; // from the corner's buttons: the keys
        return;
    }
    if(m_state == m_vr && ui().page == pageIndex(pageConsole))
    {
        consolePage.focusKey = 0;
        return;
    }
    if(m_state == m_vr && ui().page == pageIndex(pageMaps))
    {
        mapPage.focusKey = 0; // from the corner's buttons: the keys
        return;
    }
    if(m_state == m_vr)
    {
        const auto& list = items(ui().page);
        ui().cursors[ui().page] = dir > 0 ? firstSelectable(list) : lastSelectable(list);
    }
}

void qvr::menu::reopen(int target)
{
    if(target < PageMain || target >= pageCount)
    {
        target = PageMain;
    }
    enterVrMenu();
    if(navTop() != target)
    {
        navReset(); // (its way back gone: up the tree)
        navPush(target);
    }
    showPage(target); // its cursor, scroll and way back as they were
}

bool qvr::menu::scroll(int rows)
{
    if(m_state != m_vr || ui().sliderGrab || ui().scrollGrab)
    {
        return false;
    }
    if(ui().page == pageIndex(pageConsole))
    {
        consoleScroll(-rows); // (the stick down: newer)
        return true;
    }
    if(ui().page == pageIndex(pageMaps))
    {
        mapPage.resultScroll -= rows; // (the stick down: the next packages)
        mapsScrollTo(-1);
        return true;
    }
    if(dropDownItem())
    {
        // The stick moves the open list's highlight, the list scrolling with it.
        ui().dropDown.hover = CLAMP(0, ui().dropDown.hover + rows, ui().dropDown.count - 1);
        dropDownShowHover();
        return true;
    }
    const auto& list = items(ui().page);
    const int most = maxScroll(list);
    if(most <= 0)
    {
        return false;
    }
    ui().scrolls[ui().page] = CLAMP(0, ui().scrolls[ui().page] + rows, most);
    keepCursorVisible();
    return true;
}
