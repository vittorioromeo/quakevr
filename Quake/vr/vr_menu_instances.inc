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
        (b == pageArmourStudio && static_cast<int>(vr_armourstudio_kind.value) == qvr::armourdata::KindPlayer) ||
        ui().bodyCloneLatch == ui().page)
    {
        return true;
    }
    const cvar_t* chosen = selectedSetting();
    ui().bodyCloneLatch = chosen && tweaksBody(chosen->name) ? ui().page : -1;
    return ui().bodyCloneLatch == ui().page;
}

bool qvr::menu::armourStudioPage()
{
    return key_dest == key_menu && m_state == m_vr && ui().page >= 0 && ui().page < pageCount && PageRef::at(ui().page) == pageArmourStudio;
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

// maps_page_pick <row> [source]: that row of the list picked (its detail shown; a Slipseer map's page read), the
// Source filter set first when given (0 both, 1 Quaddicted, 2 Slipseer). A test aid (screenshots, the debug menu).
void qvr::menu::mapsPagePick_f()
{
    if(Cmd_Argc() >= 3)
    {
        mapPage.source = CLAMP(0, Q_atoi(Cmd_Argv(2)), mapsSourceCount - 1);
    }
    mapsShown();
    int r = Cmd_Argc() >= 2 ? Q_atoi(Cmd_Argv(1)) : 0;
    if(Cmd_Argc() >= 2 && !isdigit(static_cast<unsigned char>(Cmd_Argv(1)[0])))
    {
        // A title instead of a row: the first whose title holds it (any case).
        r = -1;
        for(int i = 0; i < static_cast<int>(mapPage.results.size()) && r < 0; i++)
        {
            if(q_strcasestr(mapindex::index().field(mapPage.results[i]->title), Cmd_Argv(1)))
            {
                r = i;
            }
        }
    }
    mapsPick(r);
    mapsScrollTo(r);
    Con_Printf("maps_page_pick: row %d of %d%s\n", r, static_cast<int>(mapPage.results.size()),
        mapPage.picked >= 0 ? "" : " (none: out of the list)");
}

// vr_runsetup [hub|newgame|library <map>]: the screen for that start (the hub's teleporter's; tests).
void qvr::menu::runSetup_f()
{
    const char* what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!q_strcasecmp(what, "hub"))
    {
        openRunSetup(runsetup::Launch::Hub, za::String{}, nullptr, -1, true);
    }
    else if(!q_strcasecmp(what, "newgame"))
    {
        openRunSetup(runsetup::Launch::NewGame, za::String{}, nullptr, -1, true);
    }
    else
    {
        openRunSetup(runsetup::Launch::None, za::String{}, nullptr, -1, false);
    }
}

extern "C" void VR_RunSetupNewGame()
{
    openRunSetup(runsetup::Launch::NewGame, za::String{}, nullptr, -1, true);
}

extern "C" void VR_RunSetupLevel(const char* map)
{
    openRunSetup(runsetup::Launch::Level, za::String{}, map, -1, mapinstall::mapHasSkillSelect(map));
}

// vr_runsetup_screen_test: each way in opens the screen for its start (a start hall
// with skill selection: no Difficulty row), its rows the registry's, a held row shown as a line, Back cancels (nothing
// started, the picks kept), Start with nothing to start makes the picks the run's.
void qvr::menu::runSetupScreenTest_f()
{
    testkit::Tally t;
    constexpr const char* name = "runsetup_screen";
    auto check = [&](bool ok, const char* what) { t.check(ok, name, what); };
    const int page = pageIndex(pageRunSetup);
    auto hasLabel = [](const za::Vector<Item>& items, const char* label) {
        for(const Item& it : items)
        {
            if(it.kind != Item::Header && it.label && !strcmp(it.label, label)) // (a row, not a section's header)
            {
                return true;
            }
        }
        return false;
    };
    auto shownFor = [&](runsetup::Launch launch) { return ui().page == page && runSetup.launch == launch; };
    check(mapinstall::mapHasSkillSelect("start"), "start.bsp has skill halls");
    check(!mapinstall::mapHasSkillSelect("e1m1"), "e1m1 has none");
    const float ask = vr_runsetup_ask.value;
    const float meleeOnly = vr_runpick_meleeonly.value;
    Cvar_Set("vr_runsetup_ask", "1");

    // The Map Library's Play: a map without skill halls asks the difficulty.
    openRunSetup(runsetup::Launch::Library, za::String{"0000"}, "e1m1", -1, mapinstall::mapHasSkillSelect("e1m1"));
    za::Vector<Item> items = pageRunSetup();
    check(shownFor(runsetup::Launch::Library), "Map Library's Play opens it");
    const char* const rows[] = {"Difficulty", "Style", "Gore", "Armed Humanoids", "Smarter AI", "Starting gun",
        "Starting melee weapon", "Melee Only", "Glass Cannon", "Start"};
    bool allRows = true;
    for(const char* r : rows)
    {
        if(!hasLabel(items, r))
        {
            Con_Printf("runsetup_screen: no \"%s\" row\n", r);
            allRows = false;
        }
    }
    check(items.size() > 0 && items[0].kind != Item::Header && !hasLabel(items, "Run Setup"),
        "no title, no recap: the page starts with the difficulty picker");
    check(allRows, "its rows: Difficulty, the presets, the mutators, Start");
    check(ui().cursors[page] == static_cast<int>(items.size()) - 1 || ui().cursors[page] == 0,
        "the cursor on Start (or on Difficulty, the page longer than its view)");
    goBack();
    check(ui().page != page && runSetup.launch == runsetup::Launch::Library, "Back leaves it, nothing started");

    // A start map with skill halls: no Difficulty row, the mutators still asked.
    openRunSetup(runsetup::Launch::Library, za::String{"0000"}, "start", -1, mapinstall::mapHasSkillSelect("start"));
    items = pageRunSetup();
    check(!hasLabel(items, "Difficulty") && hasLabel(items, "Starting gun"), "skill halls: no Difficulty, mutators asked");
    goBack();

    // New Game (menu.c's), Official Campaigns' row, the hub's teleporter.
    VR_RunSetupNewGame();
    check(shownFor(runsetup::Launch::NewGame) && runSetup.skillHalls, "New Game opens it (start's halls)");
    goBack();
    if(VR_CampaignUnavailable(0) == 0)
    {
        openRunSetupCampaign(0);
        check(shownFor(runsetup::Launch::Campaign) && runSetup.campaign == 0, "Official Campaigns' row opens it");
        goBack();
    }
    // The Levels list after a start hall's launch (New Game's, the campaign's): the page as shown (the menu's built
    // rows, not a fresh build) has its Difficulty row at once (his flat report 2026-11-13: only after a fold).
    auto shown = [&]() -> const za::Vector<Item>& { return menu_detail::items(page); };
    const float bnFound = vr_mg3_bn_discovered.value;
    Cvar_Set("vr_mg3_bn_discovered", "0");
    VR_RunSetupLevel("e1m1"); // (the flat Levels list's Enter: not the old difficulty menu)
    check(shownFor(runsetup::Launch::Level) && !runSetup.skillHalls && !strcmp(runSetup.map.cStr(), "e1m1") &&
              m_state != m_skill,
        "the Levels list opens it for that map (Difficulty asked)");
    check(hasLabel(shown(), "Difficulty"), "after a start hall's launch: the shown page has Difficulty");
    check(hasLabel(shown(), "Bloody Nightmare"), "Bloody Nightmare a row, never found");
    // Opened at its top: the Difficulty row in view (the draw scrolls only to keep the cursor in view), the cursor on
    // Start when that is in view too, else on Difficulty.
    const int at = ui().cursors[page];
    const bool onStart = at >= 0 && at < static_cast<int>(shown().size()) && shown()[at].label &&
                         !strcmp(shown()[at].label, "Start");
    check(ui().scrolls[page] == 0 && at < rowsFrom(shown(), 0) && (onStart || !strcmp(shown()[at].label, "Difficulty")),
        "opened at its top, Difficulty and the cursor in view (on Start when it fits)");
    Con_Printf("runsetup_screen: %d rows, %d in view from the top, the cursor on %s\n", static_cast<int>(shown().size()),
        rowsFrom(shown(), 0), shown()[at].label ? shown()[at].label : "?");
    goBack();
    VR_RunSetupLevel("start");
    check(shownFor(runsetup::Launch::Level) && runSetup.skillHalls, "the Levels list's start hall: no Difficulty row");
    check(!hasLabel(shown(), "Difficulty"), "... and the shown page has none");
    goBack();
    VR_RunSetupLevel("e1m1");
    check(hasLabel(shown(), "Difficulty"), "a map after the start hall: the shown page has Difficulty again");
    goBack();
    Cvar_SetValueQuick(&vr_mg3_bn_discovered, bnFound);
    Cbuf_InsertText("vr_runsetup hub\n");
    Cbuf_Execute();
    check(shownFor(runsetup::Launch::Hub), "the hub's teleporter (vr_runsetup hub) opens it");

    // A held row: Melee Only holds the Starting gun (a line, the page rebuilt); the melee weapon stays a row.
    Cvar_Set("vr_runpick_meleeonly", "1");
    check(runSetupStale(), "Melee Only: the page is rebuilt");
    items = pageRunSetup();
    check(!hasLabel(items, "Starting gun") && hasLabel(items, "Starting melee weapon") && !runSetupStale(),
        "Starting gun held: shown as a line, the melee weapon a row");
    Cvar_SetValueQuick(&vr_runpick_meleeonly, meleeOnly);
    goBack();

    // Start with nothing to start: the picks are the run's (the presets as My Settings, kept in the test files: the
    // settings are not changed).
    gameplaypresets::kind().useTestFile(true);
    gorepresets::kind().useTestFile(true);
    gameplaypresets::kind().keepMine();
    gorepresets::kind().keepMine();
    const char* const presetPicks[] = {"vr_runpick_style", "vr_runpick_gore", "vr_runpick_armed", "vr_runpick_smartai"};
    float presetValues[4];
    for(int i = 0; i < 4; i++)
    {
        presetValues[i] = Cvar_VariableValue(presetPicks[i]);
        Cvar_Set(presetPicks[i], "0");
    }
    runSetup.launch = runsetup::Launch::None;
    runSetupStart();
    bool same = true;
    for(int i = 0; i < runsetup::count(); i++)
    {
        same = same && runsetup::activeValue(i) == runsetup::pickValue(i);
    }
    check(same && key_dest == key_game, "Start: the picks made the run's, the menu closed");
    for(int i = 0; i < 4; i++)
    {
        Cvar_SetValue(presetPicks[i], presetValues[i]);
    }
    gameplaypresets::kind().useTestFile(false);
    gorepresets::kind().useTestFile(false);
    Cvar_SetValueQuick(&vr_runsetup_ask, ask);
    t.verdict(name);
}

// vr_runlaunch_test: every way a run starts (the Levels list, the Map Library's Play, New Game, Official Campaigns, the
// hub's teleporter), each with other choices than the run before it, the screen's Start pressed: the map spawns with
// the new run's choices (never the last run's: his flat report 2026-11-13) and plays as they say (QC
// VR_Run_LaunchCheck: the loadout, Glass Cannon's blows on a monster and the training dummy by a gun's path and a
// hand's, Melee Only on the map's own guns and ammunition, the enemies' copies and swaps and the pickups' swaps on
// e1m1); a saved run loaded brings its choices, and the next start the screen's again. Driven a frame at a time
// (runLaunchTestFrame: each step waits for its map's player and the monsters' start thinks). Verdict "runlaunch: N
// passed, M failed". The presets as My Settings (test files) and every pick set back.
struct RunLaunchPicks
{
    int gun, melee, meleeOnly, glass, count, random, pickups;
};
// A: Melee Only (the gun held at Nothing), a dagger, 3x enemies. B: the revolver, Glass Cannon, 1x, Anything Goes
// enemies and pickups. G: Melee Only and Glass Cannon (the firing range's dummy).
constexpr RunLaunchPicks runLaunchPicks[] = {{0, 1, 1, 0, 2, 0, 0}, {2, 0, 0, 1, 0, 2, 2}, {0, 0, 1, 1, 0, 0, 0}};

enum class RunLaunchWay
{
    Level,
    Library,
    NewGame,
    Campaign,
    Map, // (a map by the console: the hub's, before its teleporter)
    Hub,
    Load,
    FiringRange, // the VR menu's Firing Range button (no Run Setup screen: the screen's mutators all the same)
    ConsoleRun,  // `vr_run glasscannon 0` then `map`: a run's choice set by hand is kept
};

struct RunLaunchStep
{
    RunLaunchWay way;
    const char* map;  // the map it starts
    int picks;        // runLaunchPicks' row, -1: none (a load: the saved run's)
    bool check;       // checked (not the hub's own map)
    bool loadout;     // a new game: the run's starting weapons
    bool save;        // saved after its check (the run the Load step brings back)
    const char* what;
};
constexpr RunLaunchStep runLaunchSteps[] = {
    {RunLaunchWay::Level, "e1m1", 0, true, true, false, "the Levels list"},
    {RunLaunchWay::Level, "e1m1", 1, true, true, true, "the Levels list again, other choices"},
    {RunLaunchWay::Library, "e1m1", 0, true, true, false, "the Map Library's Play"},
    {RunLaunchWay::NewGame, "start", 1, true, true, false, "New Game"},
    {RunLaunchWay::Campaign, "start", 0, true, true, false, "Official Campaigns' row"},
    {RunLaunchWay::Map, "vrstart", -1, false, false, false, "the hub"},
    {RunLaunchWay::Hub, "start", 1, true, false, false, "the hub's teleporter"},
    {RunLaunchWay::Level, "vrfiringrange", 2, true, false, false, "the Levels list: the firing range"},
    {RunLaunchWay::Load, "e1m1", -1, true, false, false, "the saved run loaded"},
    {RunLaunchWay::Level, "e1m1", 0, true, true, false, "the Levels list after a load"},
    {RunLaunchWay::FiringRange, "vrfiringrange", 2, true, false, false, "the Firing Range button (no screen)"},
    {RunLaunchWay::ConsoleRun, "vrfiringrange", -1, true, false, false, "`vr_run glasscannon 0` then `map`"},
};
constexpr int runLaunchStepCount = static_cast<int>(sizeof(runLaunchSteps) / sizeof(runLaunchSteps[0]));
constexpr int runLaunchGiveUp = 72 * 60; // frames a step may wait for its map (60 s of game time)
constexpr int runLaunchSettle = 48;      // frames after the player is in: the monsters' start thinks (0.1-0.6 s)
constexpr const char* runLaunchPresets[] = {"vr_runpick_style", "vr_runpick_gore", "vr_runpick_armed", "vr_runpick_smartai"};

struct RunLaunchTest
{
    int step{-1}; // -1: not running
    bool launched{false};
    int spawns{0};  // runsetup::mapSpawns() at the launch
    int frames{0};  // waited for the map
    int settled{0}; // frames since its player came in
    int savedRun[64]{}; // the run saved (the Load step's)
    int expect[64]{};   // the run set by hand (the ConsoleRun step's)
    float picks[64]{}; // every pick as before the test
    float ask{1.f};
    testkit::Tally tally;
};
RunLaunchTest runLaunch;

void runLaunchSetPicks(const RunLaunchPicks& p)
{
    Cvar_SetValueQuick(&vr_runpick_startgun, static_cast<float>(p.gun));
    Cvar_SetValueQuick(&vr_runpick_startmelee, static_cast<float>(p.melee));
    Cvar_SetValueQuick(&vr_runpick_meleeonly, static_cast<float>(p.meleeOnly));
    Cvar_SetValueQuick(&vr_runpick_glasscannon, static_cast<float>(p.glass));
    Cvar_SetValueQuick(&vr_runpick_enemycount, static_cast<float>(p.count));
    Cvar_SetValueQuick(&vr_runpick_enemyrandom, static_cast<float>(p.random));
    Cvar_SetValueQuick(&vr_runpick_pickups, static_cast<float>(p.pickups));
}

void runLaunchEnd()
{
    RunLaunchTest& t = runLaunch;
    for(int i = 0; i < runsetup::count() && i < 64; i++)
    {
        Cvar_SetValueQuick(runsetup::at(i).pick, t.picks[i]);
    }
    gameplaypresets::kind().useTestFile(false);
    gorepresets::kind().useTestFile(false);
    Cvar_SetValueQuick(&vr_runsetup_ask, t.ask);
    t.step = -1;
    t.tally.verdict("runlaunch");
}

// The step's way in, as a press of its Start (the screen opened, then its Start).
void runLaunchGo(const RunLaunchStep& s)
{
    if(s.picks >= 0)
    {
        runLaunchSetPicks(runLaunchPicks[s.picks]);
    }
    switch(s.way)
    {
        case RunLaunchWay::Level: VR_RunSetupLevel(s.map); break;
        case RunLaunchWay::Library:
            openRunSetup(runsetup::Launch::Library, za::String{"0000"}, s.map, -1, mapinstall::mapHasSkillSelect(s.map));
            break;
        case RunLaunchWay::NewGame: VR_RunSetupNewGame(); break;
        case RunLaunchWay::Campaign: openRunSetupCampaign(0); break;
        case RunLaunchWay::Hub: openRunSetup(runsetup::Launch::Hub, za::String{}, nullptr, -1, true); break;
        case RunLaunchWay::Map: Cbuf_AddText(va("map %s\n", s.map)); return;
        case RunLaunchWay::Load: Cbuf_AddText("load rf_launch_b\n"); return;
        case RunLaunchWay::FiringRange: playFiringRange(); return;
        case RunLaunchWay::ConsoleRun:
            Cmd_ExecuteString("vr_run glasscannon 0", src_command);
            for(int i = 0; i < runsetup::count() && i < 64; i++)
            {
                runLaunch.expect[i] = runsetup::activeValue(i); // (the run as set by hand, the picks not)
            }
            Cbuf_AddText(va("map %s\n", s.map));
            return;
    }
    runSetupStart();
    if(s.way == RunLaunchWay::Hub)
    {
        Cbuf_AddText("changelevel start\n"); // (the teleporter, vr_run_hubgo set: it lets him through)
    }
}

void runLaunchCheck(const RunLaunchStep& s)
{
    RunLaunchTest& t = runLaunch;
    char what[128];
    q_snprintf(what, sizeof(what), "%s (%s)", s.what, sv.name);
    bool spawned = true;
    bool now = true;
    for(int i = 0; i < runsetup::count() && i < 64; i++)
    {
        const int want = s.way == RunLaunchWay::Load         ? t.savedRun[i]
                         : s.way == RunLaunchWay::ConsoleRun ? t.expect[i]
                                                             : runsetup::pickValue(i);
        if(runsetup::spawnedWith(i) != want || runsetup::activeValue(i) != want)
        {
            Con_Printf("runlaunch: %s: %s spawned with %d, %d now; the run's: %d\n", what, runsetup::at(i).id,
                runsetup::spawnedWith(i), runsetup::activeValue(i), want);
        }
        spawned = spawned && runsetup::spawnedWith(i) == want;
        now = now && runsetup::activeValue(i) == want;
    }
    t.tally.check(spawned, "runlaunch", va("%s: the map spawned with the run's choices", what));
    t.tally.check(now, "runlaunch", va("%s: the run's choices", what));
    auto active = [](const char* id) { return static_cast<float>(runsetup::activeValue(runsetup::find(id))); };
    const float w = active("startgun") * 10.f + active("startmelee");
    const float gm = active("glasscannon") + 2.f * active("meleeonly") + (s.loadout ? 4.f : 0.f);
    const float en = active("enemycount") + 3.f * active("enemyrandom") + 9.f * active("pickups");
    t.tally.check(runsetup::qcTest("VR_Run_LaunchCheck", w, gm, en), "runlaunch",
        va("%s: the game as they say (VR_Run_LaunchCheck)", what));
    if(s.save)
    {
        for(int i = 0; i < runsetup::count() && i < 64; i++)
        {
            t.savedRun[i] = runsetup::activeValue(i);
        }
        Cbuf_AddText("save rf_launch_b\n");
    }
    Con_Printf("runlaunch: checked %s: %s\n", what, runsetup::summary(false));
}

void qvr::menu::runLaunchTestFrame()
{
    RunLaunchTest& t = runLaunch;
    if(t.step < 0)
    {
        return;
    }
    if(t.step >= runLaunchStepCount)
    {
        runLaunchEnd();
        return;
    }
    const RunLaunchStep& s = runLaunchSteps[t.step];
    if(!t.launched)
    {
        if(s.way == RunLaunchWay::Campaign && VR_CampaignUnavailable(0) != 0)
        {
            Con_Printf("runlaunch: %s skipped (id's campaign unavailable)\n", s.what);
            t.step++;
            return;
        }
        if(s.way == RunLaunchWay::Library && !mapindex::find(za::String{"0000"}))
        {
            // (No map index in a test run: the package's play is refused after the Start. The Start's own part, the
            // run's choices, is the screen's for every way in: checked here.)
            runLaunchGo(s);
            bool now = true;
            for(int i = 0; i < runsetup::count(); i++)
            {
                now = now && runsetup::activeValue(i) == runsetup::pickValue(i);
            }
            t.tally.check(now, "runlaunch", va("%s: its Start made the choices the run's (no package to play)", s.what));
            Con_Printf("runlaunch: checked %s: the run's choices only (no map index here)\n", s.what);
            t.step++;
            return;
        }
        t.launched = true;
        t.spawns = runsetup::mapSpawns();
        t.frames = 0;
        t.settled = 0;
        runLaunchGo(s);
        return;
    }
    if(++t.frames > runLaunchGiveUp)
    {
        t.tally.check(false, "runlaunch", va("%s: timed out (sv %s, spawns %d)", s.what, sv.name, runsetup::mapSpawns() - t.spawns));
        runLaunchEnd();
        return;
    }
    if(runsetup::mapSpawns() == t.spawns || !sv.active || strcmp(sv.name, s.map) != 0 || cls.signon != SIGNONS ||
        cl.stats[STAT_HEALTH] <= 0 || ++t.settled < runLaunchSettle)
    {
        return;
    }
    if(s.check)
    {
        runLaunchCheck(s);
    }
    t.step++;
    t.launched = false;
}

void qvr::menu::runLaunchTest_f()
{
    RunLaunchTest& t = runLaunch;
    t = RunLaunchTest{};
    for(int i = 0; i < runsetup::count() && i < 64; i++)
    {
        t.picks[i] = runsetup::at(i).pick->value;
    }
    t.ask = vr_runsetup_ask.value;
    Cvar_Set("vr_runsetup_ask", "1");
    // The presets as My Settings, kept in the test files: his settings are not changed.
    gameplaypresets::kind().useTestFile(true);
    gorepresets::kind().useTestFile(true);
    gameplaypresets::kind().keepMine();
    gorepresets::kind().keepMine();
    for(const char* p : runLaunchPresets)
    {
        Cvar_Set(p, "0");
    }
    Cvar_Set("vr_runpick_ironman", "0");
    Cvar_Set("vr_runpick_bn", "0");
    t.step = 0;
}

// vr_continue_rows_test: Continue on the launch rows (vr_continue.cpp). Without an autosave Quake's row has no Continue;
// a run (Hard, Classic, Glass Cannon) played and saved into autosave/: Official Campaigns gets "Continue (just now):
// <the run's summary>" under Quake's row, and a map package's autosave (the fixture: that save as a package's) gives
// the Map Library's Continue the same; the row pressed with other run choices loads that save, the menu closed, the
// run's choices back; the fixtures removed, no Continue again. Phases (it queues each next one itself): start, save
// (waits for the player to be in), check, loaded. Ends with its verdict line.
namespace
{
constexpr const char* continueTestSave = "autosave/co_continue_test";
constexpr const char* continueTestCopy = "autosave/co_continue_pkg";
constexpr const char* continueTestSha = "c0471e57c0471e57c0471e57c0471e57c0471e57c0471e57c0471e57c0471e57";
constexpr int continueTestWaitFrames = 600;
struct ContinueTest
{
    testkit::Tally tally;
    int waited{0};
    float autosave{0.f}; // sv_autosave before it
    char summary[160]{};
    char label[192]{};
};
ContinueTest continueTest;

// Quake's Continue row on Official Campaigns as built now (nullptr: none).
const Item* continueTestRow(const za::Vector<Item>& items)
{
    for(const Item& it : items)
    {
        if(it.kind == Item::Action && it.infoArg == campaignContinueLabel && it.arg == 0)
        {
            return &it;
        }
    }
    return nullptr;
}
} // namespace

void qvr::menu::continueRowsTest_f()
{
    constexpr const char* name = "continue_rows";
    ContinueTest& t = continueTest;
    auto check = [&](bool ok, const char* what) { t.tally.check(ok, name, what); };
    const char* phase = Cmd_Argc() > 1 ? Cmd_Argv(1) : "start";
    if(!strcmp(phase, "start"))
    {
        t = ContinueTest{};
        t.autosave = Cvar_VariableValue("sv_autosave");
        Cvar_SetValue("sv_autosave", 0.f); // (no autosave of its own: the test's save is the only one it makes)
        Host_WaitForSaveThread();
        VR_LaunchContinueTestFixture(nullptr, continueTestSave, "");
        VR_LaunchContinueTestFixture(nullptr, continueTestCopy, "");
        const za::Vector<Item> before = pageCampaigns();
        const char* other = VR_LaunchContinueSave(0, "");
        if(other)
        {
            Con_Printf("continue_rows: the absence check skipped: another Quake autosave is there (%s)\n", other);
        }
        else
        {
            check(!continueTestRow(before), "no autosave: Quake's row has no Continue");
        }
        check(!VR_LaunchContinueSave(0, continueTestSha), "no autosave: the package has no Continue");
        Cbuf_AddText("vr_run skill 2\nvr_run style 1\nvr_run glasscannon 1\nmap e1m1\nvr_continue_rows_test save\n");
        return;
    }
    if(!strcmp(phase, "save"))
    {
        const edict_t* player = progs::localPlayer();
        const bool in = cls.signon == SIGNONS && player && player->v.health > 0;
        if(!in)
        {
            if(++t.waited > continueTestWaitFrames)
            {
                check(false, "timed out: the player never came in");
                Cvar_SetValue("sv_autosave", t.autosave);
                t.tally.verdict(name);
                return;
            }
            Cbuf_AddText("wait\nvr_continue_rows_test save\n");
            return;
        }
        q_strlcpy(t.summary, runsetup::summary(false), sizeof(t.summary));
        q_snprintf(t.label, sizeof(t.label), "Continue (just now): %s", t.summary); // (the Map Library's button)
        Cbuf_AddText(va("save %s\nvr_continue_rows_test check\n", continueTestSave));
        return;
    }
    if(!strcmp(phase, "check"))
    {
        Host_WaitForSaveThread();
        check(VR_LaunchContinueTestFixture(continueTestSave, continueTestCopy, continueTestSha) == 1,
            "the package's fixture written");
        const za::Vector<Item> items = pageCampaigns();
        const Item* row = continueTestRow(items);
        char label[192];
        const Item* note = row && row + 1 < items.data() + items.size() && (row + 1)->partOf == 1 ? row + 1 : nullptr;
        q_snprintf(label, sizeof(label), "%s (%s) / %s", row ? row->infoArg(row->arg) : "",
            row && row->tagArg ? row->tagArg(row->arg) : "", note ? note->infoArg(note->arg) : "");
        char expected[192];
        q_snprintf(expected, sizeof(expected), "Continue (just now) / %s", t.summary);
        if(row && strcmp(label, expected))
        {
            Con_Printf("continue_rows: the row reads \"%s\", expected \"%s\"\n", label, expected);
        }
        check(row && !strcmp(label, expected), "Quake's row has its Continue (its age), the run's summary under it");
        check(row && row != &items[0] && (row - 1)->infoArg == VR_CampaignTitle && (row - 1)->arg == 0,
            "its Continue right under Quake's row");
        // Select Campaign's look (ROUND25.md, "campvis"): the campaign a group's title, its Continue under it, the run
        // a note line; Quake ready: no tag; a long run cut at a part, never mid-word.
        check(row && note && (row - 1)->look == Item::Lead && row->look == Item::Sub && note->look == Item::Note,
            "Quake's title, its Continue and its run line drawn as a group");
        check(VR_CampaignUnavailable(0) != 0 || !campaignTag(0)[0], "Quake ready: no status tag");
        char fit[64];
        fitAtWord("Normal / Unleashed / Armed humanoids / Glass cannon", 34, fit, sizeof(fit));
        check(!strcmp(fit, "Normal / Unleashed ..."), va("a long run cut at a part (\"%s\")", fit));
        fitAtWord("Hard / Classic", 34, fit, sizeof(fit));
        check(!strcmp(fit, "Hard / Classic"), "a short run whole");
        const char* save = VR_LaunchContinueSave(0, "");
        check(save && !strcmp(save, continueTestSave), "it is the save just made");
        const char* pkg = VR_LaunchContinueLabel(0, continueTestSha, 0);
        check(pkg && !strcmp(pkg, t.label), "the Map Library's package has its Continue, the same summary");
        const char* pkgSave = VR_LaunchContinueSave(0, continueTestSha);
        check(pkgSave && !strcmp(pkgSave, continueTestCopy), "the package's Continue is its own autosave");
        Con_Printf("continue_rows: %s | package: %s\n", label, pkg ? pkg : "none");
        if(!row)
        {
            Cvar_SetValue("sv_autosave", t.autosave);
            t.tally.verdict(name);
            return;
        }
        // Other choices now: the load must bring the run's back.
        Cbuf_AddText("vr_run skill 0\nvr_run style 3\nvr_run glasscannon 0\nvr_continue_rows_test press\n");
        return;
    }
    if(!strcmp(phase, "press"))
    {
        check(strcmp(runsetup::summary(false), t.summary) != 0, "other run choices before the press");
        // The row pressed as the menu does (Official Campaigns open).
        VR_OpenCampaignSelector();
        const za::Vector<Item> items = pageCampaigns();
        const Item* row = continueTestRow(items);
        check(row != nullptr && m_state == m_vr, "the page open, its Continue there");
        if(row)
        {
            row->actionArg(row->arg);
        }
        Cbuf_AddText("vr_continue_rows_test loaded\n");
        return;
    }
    if(!strcmp(phase, "loaded"))
    {
        check(key_dest == key_game && m_state == m_none, "the menu closed");
        check(sv.active && !strcmp(sv.name, "e1m1"), "the save loaded (e1m1)");
        check(!strcmp(runsetup::summary(false), t.summary), "the run's choices back from the save");
        check(static_cast<int>(skill.value) == 2, "skill 2 (Hard) back");
        Con_Printf("continue_rows: the run after the load: %s\n", runsetup::summary(false));
        VR_LaunchContinueTestFixture(nullptr, continueTestSave, "");
        VR_LaunchContinueTestFixture(nullptr, continueTestCopy, "");
        const za::Vector<Item> after = pageCampaigns();
        check(!continueTestRow(after) || VR_LaunchContinueSave(0, ""), "its autosave removed: no Continue from it");
        check(!VR_LaunchContinueSave(0, continueTestSha), "the package's removed: no Continue");
        Cvar_SetValue("sv_autosave", t.autosave);
        t.tally.verdict(name);
        return;
    }
    Con_Printf("usage: vr_continue_rows_test [start]\n");
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

// vr_menu_flash_test (Debug > Tools; ROUND25.md, "campvis"): a Search result opened ("Arm Drip Rate", on Gore) lands on
// its row, which flashes (vr_menu_flash_time) and is scrolled to the list's middle (as far as the page lets it); the
// flash is over once its time has passed; with vr_menu_flash_time 0 nothing flashes. Phases (it queues each next one
// itself, a frame apart): start, land (until the centring settles), fade (until the flash's time is over), off. Ends
// with its verdict line.
namespace
{
struct FlashTest
{
    testkit::Tally tally;
    int frames{0};
    float time{1.5f}; // vr_menu_flash_time before it
};
FlashTest flashTest;
constexpr int flashTestMaxFrames = 72 * 8; // (the give-up: the flash's longest is 5 s)
constexpr const char* flashTestRow = "Arm Drip Rate"; // (the middle of Gore, a long page: room to centre it)
} // namespace

void qvr::menu::flashTest_f()
{
    constexpr const char* name = "menu_flash";
    FlashTest& t = flashTest;
    auto check = [&](bool ok, const char* what) { t.tally.check(ok, name, what); };
    const char* phase = Cmd_Argc() > 1 ? Cmd_Argv(1) : "start";
    const auto giveUp = [&](const char* what) {
        check(false, what);
        Cvar_SetValue("vr_menu_flash_time", t.time);
        t.tally.verdict(name);
    };
    if(!strcmp(phase, "start"))
    {
        t = FlashTest{};
        t.time = vr_menu_flash_time.value;
        Cvar_SetValue("vr_menu_flash_time", 1.5f);
        Cbuf_AddText(va("menu_vr\nvr_menu_search -open %s\nwait\nvr_menu_flash_test land\n", flashTestRow));
        return;
    }
    if(!strcmp(phase, "land"))
    {
        if(ui().flashCentre && ++t.frames < flashTestMaxFrames)
        {
            Cbuf_AddText("wait\nvr_menu_flash_test land\n");
            return;
        }
        const auto& list = items(ui().page);
        const int cursor = ui().cursors[ui().page];
        const bool onRow = cursor >= 0 && cursor < static_cast<int>(list.size());
        check(!ui().flashCentre, "the page drawn, its centring settled");
        check(onRow && list[cursor].label && !strcmp(list[cursor].label, flashTestRow), va("the page open on %s's row", flashTestRow));
        check(ui().flashPage == ui().page && ui().flashRow == cursor, "the flash on that row");
        check(flashAlpha() > 0.f, va("it flashes (strength now %.2f)", flashAlpha()));
        if(onRow)
        {
            const int scroll = ui().scrolls[ui().page];
            const int middle = rowTop(list, scroll, cursor) + rowHeight(list, cursor) / 2;
            const int space = listSpace(list);
            Con_Printf("menu_flash: row %d at %d of the list's %d (scroll %d of %d)\n", cursor, middle, space, scroll,
                maxScroll(list));
            check(abs(middle - space / 2) <= 10 || scroll == maxScroll(list) || scroll == 0, "the row in the list's middle");
        }
        t.frames = 0;
        Cbuf_AddText("wait\nvr_menu_flash_test fade\n");
        return;
    }
    if(!strcmp(phase, "fade"))
    {
        if(realtime - ui().flashStart < 1.5 + 0.05)
        {
            if(++t.frames >= flashTestMaxFrames)
            {
                giveUp("timed out: the flash's time never passed");
                return;
            }
            Cbuf_AddText("wait\nvr_menu_flash_test fade\n");
            return;
        }
        check(flashAlpha() == 0.f, "the flash over after its time");
        Cvar_SetValue("vr_menu_flash_time", 0.f);
        Cbuf_AddText(va("vr_menu_search -open %s\nwait\nvr_menu_flash_test off\n", flashTestRow));
        return;
    }
    if(!strcmp(phase, "off"))
    {
        check(flashAlpha() == 0.f, "vr_menu_flash_time 0: no flash");
        Cvar_SetValue("vr_menu_flash_time", t.time);
        t.tally.verdict(name);
    }
}

void qvr::menu::search_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_menu_search [-open] <text>: the VR menus' settings, actions and pages matching it, best first "
                   "(-open: the best opened, as Search's first result picked)\n");
        return;
    }
    const bool open = !strcmp(Cmd_Argv(1), "-open");
    buildSearchIndex();
    const char* text = Cmd_Args();
    if(open)
    {
        text = strstr(text, "-open") + 5;
        while(*text == ' ')
        {
            text++;
        }
    }
    search.query = text;
    runSearch();
    if(open)
    {
        if(!search.results.empty())
        {
            openResult(0);
        }
        Con_Printf("vr_menu_search -open \"%s\": %s (on %s)\n", search.query.cStr(),
            search.results.empty() ? "no result" : search.index[search.results[0].entry].label.cStr(), menuShownName());
        return;
    }
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

void qvr::menu::jumpToArmourStudio()
{
    if(!armourStudioPage())
    {
        openFromAnywhere(pageIndex(pageArmourStudio));
    }
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

bool qvr::menu::escapeLocal()
{
    return m_state == m_vr && (ui().sliderGrab || ui().scrollGrab || dropDownItem() != nullptr);
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
