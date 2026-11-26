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

// vr_runsetup [hub|newgame|arena]: the screen for that start (the hub's teleporter's; Arena Mode's past its notice;
// tests and screenshots).
void qvr::menu::runSetup_f()
{
    const char* what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!q_strcasecmp(what, "hub"))
    {
        openRunSetup(runsetup::Launch::Hub, za::String{}, nullptr, -1, true);
    }
    else if(!q_strcasecmp(what, "arena"))
    {
        openRunSetupArena();
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

namespace
{

// The Run Setup page shown's row `label`, or -1.
[[nodiscard]] int runSetupRowIndex(const char* label)
{
    const za::Vector<Item>& list = items(pageIndex(pageRunSetup));
    for(int i = 0; i < static_cast<int>(list.size()); i++)
    {
        if(list[i].kind == Item::Cycle && list[i].label && !strcmp(list[i].label, label))
        {
            return i;
        }
    }
    return -1;
}

// Picture `c` of the shown page's row `label` clicked as the mouse (or the laser) would: the row scrolled to the top, the
// mouse moved onto the picture's middle (iconCentre), the button pressed. True when the mouse's hit test found that
// picture and the row's cvar is that choice's after the click.
[[nodiscard]] bool runSetupClickIcon(const char* label, int c)
{
    const int page = pageIndex(pageRunSetup);
    const int r = runSetupRowIndex(label);
    float x = 0.f, y = 0.f;
    if(r < 0)
    {
        return false;
    }
    ui().scrolls[page] = r;
    if(!iconCentre(items(page), r, c, x, y))
    {
        return false;
    }
    cvar_t* const var = items(page)[r].cvar;
    const float want = items(page)[r].choices[static_cast<za::SizeT>(c)].value;
    VR_Menu_Mousemove(x, y);
    const bool hit = iconHover.page == page && iconHover.row == r && iconHover.icon == c && ui().cursors[page] == r;
    m_mousex = x;
    m_mousey = y;
    VR_Menu_Key(K_MOUSE1, 0); // (the page may be built anew after this: `var` and `want` read before)
    return hit && var->value == want;
}

// vr_runsetup_screen_test's icon pickers (vr_menu_icons.inc): Difficulty, the Starting gun and melee weapon and Enemy
// Count are icon rows, each choice picked by a click on its picture (the mouse's hit test) and stepped through by the
// keys; a click on the label picks nothing; Difficulty's four cards spaced as the weapon pickers'; a start hall has no
// Difficulty; vr_menu_icons 0 draws them as plain rows. Every pick set back.
void runSetupIconTest(testkit::Tally& t)
{
    constexpr const char* name = "runsetup_screen";
    auto check = [&](bool ok, const char* what) { t.check(ok, name, what); };
    const int page = pageIndex(pageRunSetup);
    cvar_t* const kept[] = {&vr_maps_skill, &vr_runpick_startgun, &vr_runpick_startmelee, &vr_runpick_enemycount,
        &vr_menu_icons, &vr_runpick_meleeonly};
    float was[6];
    for(int k = 0; k < 6; k++)
    {
        was[k] = kept[k]->value;
    }
    const za::String folded{vr_menu_folded.string};
    Cvar_Set("vr_menu_folded", ""); // (every section open: a folded row has no picture to click)
    Cvar_Set("vr_menu_icons", "1");
    Cvar_Set("vr_runpick_meleeonly", "0");
    VR_RunSetupLevel("e1m1");
    const char* const rows[] = {"Difficulty", "Starting gun", "Starting melee weapon", "Enemy Count"};
    for(const char* label : rows)
    {
        const int r = runSetupRowIndex(label);
        const bool picker = r >= 0 && iconRowOn(items(page)[r]);
        check(picker && fullHeight(items(page)[r]) > 32, va("%s: an icon picker (%d high)", label, picker ? fullHeight(items(page)[r]) : 0));
        if(!picker)
        {
            continue;
        }
        const int n = static_cast<int>(items(page)[r].choices.size());
        cvar_t* const var = items(page)[r].cvar;
        int clicked = 0;
        for(int c = n - 1; c >= 0; c--) // (from the last: each click a change)
        {
            clicked += runSetupClickIcon(label, c) ? 1 : 0;
        }
        check(clicked == n, va("%s: each of its %d pictures picked by a click on it (%d)", label, n, clicked));
        // A click on its label (above the pictures): nothing picked.
        {
            const int rr = runSetupRowIndex(label);
            ui().scrolls[page] = rr;
            const float before = var->value;
            m_mousex = 160.f;
            m_mousey = static_cast<float>(layout().listTop + rowTop(items(page), rr, rr)) + 2.f;
            VR_Menu_Mousemove(m_mousex, m_mousey);
            VR_Menu_Key(K_MOUSE1, 0);
            check(ui().cursors[page] == rr && var->value == before && iconHover.icon < 0,
                va("%s: a click on its label selects the row, picks nothing", label));
        }
        // The keys: right steps through each choice from the first, at the last it wraps to the first; left back.
        int stepped = 0;
        Cvar_SetValueQuick(var, items(page)[runSetupRowIndex(label)].choices[0].value);
        for(int c = 1; c <= n; c++)
        {
            ui().cursors[page] = runSetupRowIndex(label);
            VR_Menu_Key(K_RIGHTARROW, 0);
            stepped += var->value == items(page)[runSetupRowIndex(label)].choices[static_cast<za::SizeT>(c % n)].value ? 1 : 0;
        }
        ui().cursors[page] = runSetupRowIndex(label);
        VR_Menu_Key(K_LEFTARROW, 0);
        stepped += var->value == items(page)[runSetupRowIndex(label)].choices[static_cast<za::SizeT>(n - 1)].value ? 1 : 0;
        check(stepped == n + 1, va("%s: right and left step through its choices (%d of %d)", label, stepped, n + 1));
    }
    // Difficulty's four cards spaced as the weapon pickers' (runsetup3: the same card width and vr_menu_icon_gap), at the
    // default gap and at the knob's top.
    const float gapWas = vr_menu_icon_gap.value;
    for(const char* g : {"16", "24", "4"})
    {
        Cvar_Set("vr_menu_icon_gap", g);
        const int d = runSetupRowIndex("Difficulty");
        const int sg = runSetupRowIndex("Starting gun");
        const int sm = runSetupRowIndex("Starting melee weapon");
        const bool rows = d >= 0 && sg >= 0 && sm >= 0;
        const IconRowLayout ld = rows ? iconRowLayout(items(page)[d]) : IconRowLayout{};
        const IconRowLayout lg = rows ? iconRowLayout(items(page)[sg]) : IconRowLayout{};
        const IconRowLayout lm = rows ? iconRowLayout(items(page)[sm]) : IconRowLayout{};
        check(rows && ld.n == 4 && ld.cell == lg.cell && ld.cell == lm.cell && ld.gap == lg.gap && ld.gap == lm.gap,
            va("gap %s: Difficulty's 4 cards %.1f apart (gap %.1f), the guns' %.1f (%.1f), the melee's %.1f (%.1f)", g,
                ld.cell, ld.gap, lg.cell, lg.gap, lm.cell, lm.gap));
    }
    Cvar_SetValueQuick(&vr_menu_icon_gap, gapWas);
    goBack();
    // A start hall: no Difficulty at all.
    VR_RunSetupLevel("start");
    check(runSetupRowIndex("Difficulty") < 0 && runSetupRowIndex("Starting gun") >= 0,
        "a start hall: no Difficulty row, the mutators' pickers there");
    goBack();
    // vr_menu_icons 0: plain rows.
    Cvar_Set("vr_menu_icons", "0");
    VR_RunSetupLevel("e1m1");
    const int gun = runSetupRowIndex("Starting gun");
    check(gun >= 0 && !iconRowOn(items(page)[gun]) && fullHeight(items(page)[gun]) == 8,
        "vr_menu_icons 0: the Starting gun a plain row (8 high)");
    goBack();
    for(int k = 0; k < 6; k++)
    {
        Cvar_SetValueQuick(kept[k], was[k]);
    }
    Cvar_Set("vr_menu_folded", folded.cStr());
}

} // namespace

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
    // The label of the page's `nth` selectable row (0 the first), or "".
    auto nthSelectable = [](const za::Vector<Item>& list, int nth) -> const char* {
        for(const Item& it : list)
        {
            if(selectable(it) && nth-- == 0)
            {
                return it.label ? it.label : "";
            }
        }
        return "";
    };
    // Start first on every way in (runsetup3: his call 2026-11-15), the cursor on it; with skill halls no Difficulty,
    // without them Difficulty second.
    auto startFirst = [&](const za::Vector<Item>& list, bool halls, const char* way) {
        const bool first = !strcmp(nthSelectable(list, 0), "Start") && ui().cursors[page] == 0 && ui().scrolls[page] == 0;
        bool difficulty = false;
        for(const Item& it : list)
        {
            difficulty = difficulty || (it.kind == Item::Cycle && it.label && !strcmp(it.label, "Difficulty"));
        }
        const bool second = halls ? !difficulty : !strcmp(nthSelectable(list, 1), "Difficulty");
        check(first && second, va("%s: Start the first row, the cursor on it; %s (second: \"%s\")", way,
                                   halls ? "no Difficulty (skill halls)" : "Difficulty second", nthSelectable(list, 1)));
    };
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
        "no title, no recap: the page starts with its first row");
    check(allRows, "its rows: Start, Difficulty, the presets, the mutators");
    startFirst(items, false, "the Map Library's Play (e1m1)");
    {
        // Then the presets: Style and Gore the next choices after Difficulty.
        check(!strcmp(nthSelectable(items, 2), "Style") && !strcmp(nthSelectable(items, 3), "Gore"),
            va("then the presets (\"%s\", \"%s\")", nthSelectable(items, 2), nthSelectable(items, 3)));
    }
    goBack();
    check(ui().page != page && runSetup.launch == runsetup::Launch::Library, "Back leaves it, nothing started");

    // A start map with skill halls: no Difficulty, the presets and mutators still asked.
    // (difficulty: the Difficulty row's choices, 0 none.)
    auto difficulty = [](const za::Vector<Item>& list) {
        for(const Item& it : list)
        {
            if(it.kind == Item::Cycle && it.label && !strcmp(it.label, "Difficulty"))
            {
                return static_cast<int>(it.choices.size());
            }
        }
        return 0;
    };
    openRunSetup(runsetup::Launch::Library, za::String{"0000"}, "start", -1, mapinstall::mapHasSkillSelect("start"));
    items = pageRunSetup();
    check(difficulty(items) == 0 && hasLabel(items, "Starting gun") && hasLabel(items, "Style"),
        "skill halls: no Difficulty, the presets and mutators asked");
    startFirst(items, true, "the Map Library's Play (start)");
    goBack();

    // New Game (menu.c's), Official Campaigns' row, the hub's teleporter.
    auto shown = [&]() -> const za::Vector<Item>& { return menu_detail::items(page); };
    VR_RunSetupNewGame();
    check(shownFor(runsetup::Launch::NewGame) && runSetup.skillHalls, "New Game opens it (start's halls)");
    startFirst(shown(), true, "New Game");
    goBack();
    if(VR_CampaignUnavailable(0) == 0)
    {
        openRunSetupCampaign(0);
        check(shownFor(runsetup::Launch::Campaign) && runSetup.campaign == 0, "Official Campaigns' row opens it");
        startFirst(shown(), true, "Official Campaigns' row");
        goBack();
    }
    // Arcane Dimensions: its experimental notice's Continue opens the screen too (his flat notes 2026-11-15: it started
    // at once), its Start the developer launch (runSetupStart). Not installed: nothing opened.
    {
        const int ad = qvr::ad::campaignIndex;
        openRunSetupExperimental(ad);
        check(VR_CampaignInstalled(ad) ? shownFor(runsetup::Launch::Campaign) && runSetup.campaign == ad
                                       : runSetup.campaign != ad || ui().page != page,
            "Arcane Dimensions' Continue opens it (installed) or nothing (not)");
        if(ui().page == page)
        {
            startFirst(shown(), true, "Arcane Dimensions' Continue");
            goBack();
        }
    }
    // Arena Mode (arenacamp): its notice's Continue opens its own screen: exactly Start, Difficulty, Gore,
    // Glass Cannon, Melee Only (no other row: the arena's balance), Start first; Back leaves it; the next way in has
    // the whole page again.
    if(VR_CampaignUnavailable(0) == 0)
    {
        openRunSetupArena();
        check(shownFor(runsetup::Launch::Arena) && runSetup.arena, "Arena Mode's Continue opens its screen");
        const char* const want[] = {"Start", "Difficulty", "Gore", "Glass Cannon", "Melee Only"};
        int n = 0;
        bool same = true;
        for(const Item& it : pageRunSetup()) // (the page's own rows: not the menu's Reset This Page under them)
        {
            if(selectable(it))
            {
                same = same && n < 6 && it.label && !strcmp(it.label, want[n]);
                if(!same && n < 7)
                {
                    Con_Printf("runsetup_screen: Arena Mode's row %d: \"%s\"\n", n, it.label ? it.label : "");
                }
                n++;
            }
        }
        check(same && n == 5, va("Arena Mode: exactly Start, Difficulty, Gore, Glass Cannon, Melee Only (%d rows)", n));
        startFirst(shown(), false, "Arena Mode");
        goBack();
        check(ui().page != page && runSetup.launch == runsetup::Launch::Arena, "Arena Mode: Back leaves it, nothing started");
        VR_RunSetupLevel("e1m1");
        check(!runSetup.arena && hasLabel(shown(), "Style") && hasLabel(shown(), "Armed Humanoids"),
            "the next way in (the Levels list): the whole page again");
        goBack();
    }
    // The Levels list after a start hall's launch (New Game's, the campaign's): the page as shown (the menu's built
    // rows, not a fresh build) has its Difficulty row at once (his flat report 2026-11-13: only after a fold).
    VR_RunSetupLevel("e1m1"); // (the flat Levels list's Enter: not the old difficulty menu)
    check(shownFor(runsetup::Launch::Level) && !runSetup.skillHalls && !strcmp(runSetup.map.cStr(), "e1m1") &&
              m_state != m_skill,
        "the Levels list opens it for that map (Difficulty asked)");
    check(difficulty(shown()) == 4, "after a start hall's launch: the shown page has Difficulty's four choices");
    check(!hasLabel(shown(), "Bloody Nightmare"), "no Bloody Nightmare anywhere on the page");
    startFirst(shown(), false, "the Levels list (e1m1)");
    Con_Printf("runsetup_screen: %d rows, %d in view from the top\n", static_cast<int>(shown().size()), rowsFrom(shown(), 0));
    goBack();
    VR_RunSetupLevel("start");
    check(shownFor(runsetup::Launch::Level) && runSetup.skillHalls, "the Levels list's start hall: its halls choose");
    check(difficulty(shown()) == 0, "... and the shown page has no Difficulty");
    startFirst(shown(), true, "the Levels list (start)");
    goBack();
    VR_RunSetupLevel("e1m1");
    check(difficulty(shown()) == 4, "a map after the start hall: the shown page has the four again");
    goBack();
    runSetupIconTest(t);
    Cbuf_InsertText("vr_runsetup hub\n");
    Cbuf_Execute();
    check(shownFor(runsetup::Launch::Hub), "the hub's teleporter (vr_runsetup hub) opens it");
    startFirst(shown(), true, "the hub's teleporter");

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
    Arena,       // Select Campaign's Arena Mode (its own screen's Start): the arena's fixed choices, no autosave
    Scene,       // a checklist test scene (runLaunchScene, its map `map`): its settings set, waited for until it runs
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
    // The presets picked before its Start (-1: the pick left as it is): Style (1 Classic, 2 Light, 3 Unleashed), Gore
    // (1 Full, 2 Minimal, 3 Off), Armed Humanoids (0 As Style, 1 Off, 2 On). Each other than the run before it: the
    // map spawns with them on its first start (presetlate: his flat report 2026-11-15, the second start only).
    int style;
    int gore;
    int armed;
};
constexpr RunLaunchStep runLaunchSteps[] = {
    // (Classic with picks row 0 on e1m1 is left out: Classic's full-size hulls leave two of 3x Enemy Count's copies in
    // solid there, VR_Run_LaunchCheck's miss; ROUND25.md, "presetlate".)
    {RunLaunchWay::Level, "e1m1", 0, true, true, false, "the Levels list", 2, 2, 0},
    {RunLaunchWay::Level, "e1m1", 1, true, true, true, "the Levels list again, other choices", 1, 1, 0},
    {RunLaunchWay::Library, "e1m1", 0, true, true, false, "the Map Library's Play", 3, 3, 0},
    {RunLaunchWay::NewGame, "start", 1, true, true, false, "New Game", 1, 2, 0},
    {RunLaunchWay::Campaign, "start", 0, true, true, false, "Official Campaigns' row", 3, 1, 0},
    {RunLaunchWay::Map, "vrstart", -1, false, false, false, "the hub", -1, -1, -1},
    {RunLaunchWay::Hub, "start", 1, true, false, false, "the hub's teleporter", 2, 3, 2},
    {RunLaunchWay::Level, "vrfiringrange", 2, true, false, false, "the Levels list: the firing range", 3, 3, 0},
    {RunLaunchWay::Load, "e1m1", -1, true, false, false, "the saved run loaded", -1, -1, -1},
    {RunLaunchWay::Level, "e1m1", 0, true, true, false, "the Levels list after a load", 3, 2, 0},
    {RunLaunchWay::FiringRange, "vrfiringrange", 2, true, false, false, "the Firing Range button (no screen)", -1, -1, -1},
    {RunLaunchWay::ConsoleRun, "vrfiringrange", -1, true, false, false, "`vr_run glasscannon 0` then `map`", -1, -1, -1},
    // Arena Mode (arenacamp): picks row 1 (Glass Cannon on, random enemies and pickups the arena turns off), then the
    // Levels list: the player's own picks the run's again (Classic after the arena's Unleashed).
    {RunLaunchWay::Arena, "vrarena", 1, true, false, false, "Arena Mode", 1, 2, 0},
    {RunLaunchWay::Level, "e1m1", 1, true, true, false, "the Levels list after Arena Mode", 1, 3, 0},
    // His log (2026-11-14): a test scene running (classic_drops: Classic's armed humanoids and bursts off, kept to put
    // back), then the Levels list with Unleashed: the scene's settings put back before the run's are set, never over
    // them at the map's spawn (the first run Unleashed; nothing for beforeMapLoad to set back).
    {RunLaunchWay::Scene, "e1m1", -1, false, false, false, "the classic_drops test scene", -1, -1, -1},
    {RunLaunchWay::Level, "e1m1", 0, true, true, false, "the Levels list during a test scene", 3, 1, 0},
};
constexpr const char* runLaunchScene = "classic_drops";
constexpr int runLaunchStepCount = static_cast<int>(sizeof(runLaunchSteps) / sizeof(runLaunchSteps[0]));
// The step whose Start is followed, before its `map`, by commands changing two of the Style's settings (the armed
// humanoids, the guns' reloading: as whatever set the last run's back in his game would): its map spawns with the
// Start's all the same, those two set back (runsetup::spawnPresetSets). Every other step: none to set back.
constexpr int runLaunchDriftStep = 9;
constexpr const char* runLaunchDrift = "vr_agrunt_repl 0\nvr_reload_mode 0\n";
constexpr int runLaunchGiveUp = 72 * 60; // frames a step may wait for its map (60 s of game time)
constexpr int runLaunchSettle = 48;      // frames after the player is in: the monsters' start thinks (0.1-0.6 s)
// Arena Mode's step: past the level-start autosave's moment (2.5 s) and into a wave (vr_arena_goto 2 at runLaunchSettle).
constexpr int runLaunchArenaSettle = 72 * 5;
constexpr const char* runLaunchArenaSave = "autosave/vrarena"; // (the name an autosave of it would have)
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
    float arenaPicks[64]{}; // every pick before Arena Mode's Start (unchanged by it)
    float autosave[2]{};    // sv_autosave, vr_autosave_level_start before Arena Mode's step (on during it)
    bool arenaSaveBefore{false}; // an autosave/vrarena.sav there before it (not this test's to remove)
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
    const int presets[] = {s.style, s.gore, s.armed};
    for(int p = 0; p < 3; p++)
    {
        if(presets[p] >= 0)
        {
            Cvar_SetValue(runLaunchPresets[p], static_cast<float>(presets[p])); // (as the screen's row sets it)
        }
    }
    switch(s.way)
    {
        case RunLaunchWay::Level: VR_RunSetupLevel(s.map); break;
        case RunLaunchWay::Library:
            openRunSetup(runsetup::Launch::Library, za::String{"0000"}, s.map, -1, mapinstall::mapHasSkillSelect(s.map));
            break;
        case RunLaunchWay::NewGame: VR_RunSetupNewGame(); break;
        case RunLaunchWay::Campaign: openRunSetupCampaign(0); break;
        case RunLaunchWay::Arena:
            Cvar_Set("vr_runpick_ironman", "1"); // (his pick: left alone by the arena's Start, which forces Ironman off)
            for(int i = 0; i < runsetup::count() && i < 64; i++)
            {
                runLaunch.arenaPicks[i] = runsetup::at(i).pick->value;
            }
            runLaunch.autosave[0] = Cvar_VariableValue("sv_autosave");
            runLaunch.autosave[1] = vr_autosave_level_start.value;
            Cvar_Set("sv_autosave", "1"); // (on: none made all the same)
            Cvar_SetValueQuick(&vr_autosave_level_start, 1.f);
            runLaunch.arenaSaveBefore = qvr::files::exists(va("%s/%s.sav", com_gamedir, runLaunchArenaSave));
            openRunSetupArena();
            break;
        case RunLaunchWay::Hub: openRunSetup(runsetup::Launch::Hub, za::String{}, nullptr, -1, true); break;
        case RunLaunchWay::Map: Cbuf_AddText(va("map %s\n", s.map)); return;
        case RunLaunchWay::Load: Cbuf_AddText("load rf_launch_b\n"); return;
        case RunLaunchWay::Scene: Cbuf_AddText(va("vr_scenario %s\n", runLaunchScene)); return;
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
    if(&s == &runLaunchSteps[runLaunchDriftStep])
    {
        Cbuf_InsertText(runLaunchDrift); // (ahead of the Start's `map`)
    }
    if(s.way == RunLaunchWay::Hub)
    {
        Cbuf_AddText("changelevel start\n"); // (the teleporter, vr_run_hubgo set: it lets him through)
    }
}

// Arena Mode's step, past its level start and into a wave (arenacamp): the player's picks unchanged by its Start, its
// fixed choices the run's, no autosave made (the level start's, a quiet moment's, vr_autosave_now refused); an "old"
// autosave of the arena (the test's own save under that name) offered nowhere: no Continue under Arena Mode, not
// Quake's Continue either. That save removed again (unless one was there before).
void runLaunchArenaCheck()
{
    RunLaunchTest& t = runLaunch;
    constexpr const char* name = "runlaunch";
    bool kept = true;
    for(int i = 0; i < runsetup::count() && i < 64; i++)
    {
        if(runsetup::at(i).pick->value != t.arenaPicks[i])
        {
            Con_Printf("runlaunch: Arena Mode: %s's pick %g, %g before\n", runsetup::at(i).id, runsetup::at(i).pick->value,
                t.arenaPicks[i]);
            kept = false;
        }
    }
    t.tally.check(kept, name, "Arena Mode: the player's Run Setup picks unchanged by its Start");
    auto active = [](const char* id) { return runsetup::activeValue(runsetup::find(id)); };
    t.tally.check(active("style") == 3 && active("armed") == 2 && active("smartai") == 2 && active("rangedknights") == 0 &&
                      active("pickups") == 0 && active("enemyrandom") == 0 && active("glasscannon") == 1 &&
                      active("ironman") == 0 && vr_run_ironman.value == 0.f,
        name, va("Arena Mode: Ironman forced off (his pick on, unchanged), Unleashed, armed humanoids and smarter AI on, ranged knights left to the arena, the rest off; "
                 "Glass Cannon as picked (%s)", runsetup::summary(false)));
    t.tally.check(vr_agrunt_repl.value != 0.f && vr_ai_enhanced.value != 0.f, name,
        "Arena Mode: the armed humanoids' and smarter AI's settings on");
    char path[MAX_OSPATH];
    q_snprintf(path, sizeof(path), "%s/%s.sav", com_gamedir, runLaunchArenaSave);
    t.tally.check(!VR_AutosaveAllowed(sv.name) && (t.arenaSaveBefore || !qvr::files::exists(path)), name,
        "Arena Mode: no autosave made (level start, 5 s in, a wave)");
    Cmd_ExecuteString("vr_autosave_now", src_command); // (refused, with its line)
    Cvar_SetValueQuick(&vr_arena_goto, 0.f);
    Cvar_SetValue("sv_autosave", t.autosave[0]);
    Cvar_SetValueQuick(&vr_autosave_level_start, t.autosave[1]);
    // No saving of any kind in the arena: save and quicksave write nothing (the line said), the Save menu refused.
    const char* nosaveProbe = "an_arena_nosave";
    char probePath[MAX_OSPATH];
    q_snprintf(probePath, sizeof(probePath), "%s/%s.sav", com_gamedir, nosaveProbe);
    Cmd_ExecuteString(va("save %s", nosaveProbe), src_command);
    Cmd_ExecuteString("save quick", src_command);
    Host_WaitForSaveThread();
    Con_Printf("runlaunch: nosave: arena %d, probe %d, quick %d\n", VR_ArenaNoSave(), qvr::files::exists(probePath) ? 1 : 0,
        qvr::files::exists(va("%s/quick.sav", com_gamedir)) ? 1 : 0);
    t.tally.check(VR_ArenaNoSave() && !qvr::files::exists(probePath),
        name, "Arena Mode: save and quicksave refused (\"No saving in Arena Mode\"), no file written");
    // An old autosave of the arena (made some other way: never a Continue).
    (void)qvr::files::writeText(probePath, "5\nold arena autosave\n// qvr_package\n// qvr_package_title\n");
    (void)VR_LaunchContinueTestFixture(nosaveProbe, runLaunchArenaSave, "");
    (void)qvr::files::remove(probePath);
    VR_LaunchContinueRefresh();
    const za::Vector<Item> camps = pageCampaigns();
    const Item* arena = nullptr;
    for(const Item& it : camps)
    {
        arena = !arena && it.infoArg == arenaModeTitle ? &it : arena;
    }
    const bool underAd = arena && arena != camps.data() && (arena - 1)->infoArg == VR_CampaignTitle &&
                         (arena - 1)->arg == qvr::ad::campaignIndex;
    const bool noContinue = arena && (arena + 1 == camps.data() + camps.size() || (arena + 1)->look != Item::Sub);
    const char* quakeSave = VR_LaunchContinueSave(0, "");
    Con_Printf("runlaunch: arena's save there %d, Arena Mode's row %d, no Continue %d, Quake's Continue %s\n",
        qvr::files::exists(path) ? 1 : 0, arena ? 1 : 0, noContinue ? 1 : 0, quakeSave ? quakeSave : "none");
    t.tally.check(qvr::files::exists(path) && arena && noContinue && (!quakeSave || strcmp(quakeSave, runLaunchArenaSave)),
        name, "an autosave of the arena there: no Continue under Arena Mode, nor Quake's");
    t.tally.check(underAd && arena->look == Item::Lead && arena->tagArg && !strcmp(arena->tagArg(arena->arg), "experimental"),
        name, "Select Campaign: Arena Mode right under Arcane Dimensions, its look, \"experimental\" at its right");
    if(!t.arenaSaveBefore)
    {
        (void)VR_LaunchContinueTestFixture(nullptr, runLaunchArenaSave, "");
    }
}

// The run's presets in effect in the map it spawned, on its first start (presetlate): the settings Style, Gore and
// Armed Humanoids set (the Gameplay preset's level unless a row is set over it, the Gore preset's choice, the armed
// humanoids' and reloading's cvars), and the map as they say (QC VR_Run_PresetLaunchCheck: its grunts armed or not,
// the guns' reloading modes).
void runLaunchPresetCheck(const char* what)
{
    RunLaunchTest& t = runLaunch;
    const int style = runsetup::activeValue(runsetup::find("style"));
    const int gore = runsetup::activeValue(runsetup::find("gore"));
    const int armed = runsetup::activeValue(runsetup::find("armed"));
    const int smartai = runsetup::activeValue(runsetup::find("smartai"));
    if(style <= 0)
    {
        return; // (My Settings: the test's own; nothing to expect)
    }
    const bool armedOn = armed == 2 || (armed == 0 && style == 3);
    const bool level = armed != 0 || smartai != 0 || gameplaypresets::kind().matchingLevel() == style - 1;
    const bool cvars = (vr_agrunt_repl.value != 0.f) == armedOn && (vr_reload_mode.value == 0.f) == (style == 1);
    const bool goreOk = gore <= 0 || gorepresets::matching() == gore;
    if(!level || !cvars || !goreOk)
    {
        Con_Printf("runlaunch: %s: Style %d (preset level %d), Gore %d (preset %d), Armed %d: vr_agrunt_repl %g, "
                   "vr_reload_mode %g\n",
            what, style, gameplaypresets::kind().matchingLevel(), gore, gorepresets::matching(), armed,
            vr_agrunt_repl.value, vr_reload_mode.value);
    }
    t.tally.check(level && cvars && goreOk, "runlaunch", va("%s: the presets' settings as the run's Style, Gore and Armed Humanoids", what));
    const bool grunts = !strcmp(sv.name, "e1m1") && runsetup::activeValue(runsetup::find("enemyrandom")) == 0;
    t.tally.check(runsetup::qcTest("VR_Run_PresetLaunchCheck", armedOn ? 1.f : 0.f, style == 1 ? 0.f : 1.f, grunts ? 1.f : 0.f),
        "runlaunch", va("%s: the map spawned with them: grunts %s, reloading %s (VR_Run_PresetLaunchCheck)", what,
                         armedOn ? "armed" : "plain", style == 1 ? "off" : "on"));
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
                         : s.way == RunLaunchWay::Arena      ? runsetup::arenaValue(i)
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
    runLaunchPresetCheck(what);
    const bool drift = &s == &runLaunchSteps[runLaunchDriftStep];
    t.tally.check(runsetup::spawnPresetSets() == (drift ? 2 : 0), "runlaunch",
        va("%s: %s (%d set back)", what,
            drift ? "the two settings changed after its Start set back to the Start's before the map spawned"
                  : "no preset setting changed between its Start and the map's spawn",
            runsetup::spawnPresetSets()));
    if(s.save)
    {
        for(int i = 0; i < runsetup::count() && i < 64; i++)
        {
            t.savedRun[i] = runsetup::activeValue(i);
        }
        Cbuf_AddText("save rf_launch_b\n");
    }
    if(s.way == RunLaunchWay::Arena)
    {
        runLaunchArenaCheck();
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
        cl.stats[STAT_HEALTH] <= 0)
    {
        return;
    }
    const bool arena = s.way == RunLaunchWay::Arena;
    if(arena && t.settled == runLaunchSettle)
    {
        Cvar_Set("vr_arena_goto", "2"); // (into a wave: none autosaved there either)
    }
    if(++t.settled < (arena ? runLaunchArenaSettle : runLaunchSettle) ||
        (s.way == RunLaunchWay::Scene && !scenario::active())) // (the scene running: its things placed)
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

// vr_menu_preset_cycle_test (Debug > Tools; ROUND26.md, "vrmisc"): each preset row (Comfort, Handedness, Gore Preset)
// stepped forward through twice its choices and back again, the menu's syncs after each step (as the next frame's draw
// does): every step shows another choice, never Custom (shownOnlyChoice): Left-handed stepped on is Right-handed again.
// The settings are put back after (Gore's through its My Settings test file). Ends with its verdict line.
void qvr::menu::presetCycleTest_f()
{
    constexpr const char* name = "vr_menu_preset_cycle_test";
    testkit::Tally tally;
    // Comfort's and Handedness's settings as they are (every one their choices set: the first choice's list).
    struct Kept
    {
        const char* cvar;
        float value;
    };
    Kept kept[16]{};
    int keptCount = 0;
    for(const Preset& p : presets)
    {
        for(const PresetSetting& s : p.choices[0].settings)
        {
            if(s.cvar && keptCount < 16)
            {
                kept[keptCount++] = Kept{s.cvar, Cvar_VariableValue(s.cvar)};
            }
        }
    }
    gorepresets::kind().useTestFile(true);
    gorepresets::kind().keepMine();
    const auto syncAll = [] {
        syncPresets();
        gorepresets::sync();
    };
    cvar_t* const rows[] = {&vr_comfort_preset, &vr_handedness, &vr_gore_preset};
    const int was = levelOverride;
    levelOverride = LevelDeveloper;
    for(cvar_t* const var : rows)
    {
        const Item* found = nullptr;
        for(int p = 0; p < pageCount && !found; p++)
        {
            if(p == pageIndex(pageSearch) || p == pageIndex(pageChanged))
            {
                continue;
            }
            for(const Item& i : items(p))
            {
                if(i.kind == Item::Cycle && i.cvar == var)
                {
                    found = &i;
                    break;
                }
            }
        }
        if(!tally.check(found != nullptr, name, va("%s: its row found", var->name)))
        {
            continue;
        }
        const Item item = *found; // (change may build the page again)
        const int n = static_cast<int>(item.choices.size());
        Cvar_SetValue(var->name, item.choices[n - 1].value); // (the last choice: Left-handed, Full Freedom, Off)
        syncAll();
        bool moved = true, neverCustom = true;
        float stuckAt = -1.f;
        for(int dir = 1; dir >= -1; dir -= 2)
        {
            for(int step = 0; step < 2 * n; step++)
            {
                const float before = var->value;
                change(item, dir);
                syncAll();
                if(var->value == before && moved)
                {
                    moved = false;
                    stuckAt = before;
                }
                neverCustom = neverCustom && !shownOnlyChoice(var, var->value);
            }
        }
        tally.check(moved, name, va("%s: every step shows another choice (stuck at %g)", var->name, stuckAt));
        tally.check(neverCustom, name, va("%s: a step never lands on Custom", var->name));
    }
    levelOverride = was;
    for(int i = 0; i < keptCount; i++)
    {
        Cvar_SetValue(kept[i].cvar, kept[i].value);
    }
    (void)gorepresets::restoreMine();
    gorepresets::kind().useTestFile(false);
    syncAll();
    tally.verdict(name);
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

// vr_menu_search_filter_test (Debug > Tools; ROUND25.md, "searchfilter"): Search's filter row (Show: Both, Cvars,
// Tunables). The results for a text under each choice (Cvars: no tunable; Tunables: tunables only; Both: the two
// together), then each way of choosing: the mouse's click on Tunables, the laser's trigger on Cvars, the sticks (up
// from the keys onto the row, across, A; down: the keys again). A release build: no row (its layout has none, the
// filter refused), every result shown. Phases (each queues the next itself): start, open, mouse, laser. The filter and
// the text are put back. Ends with its verdict line.
namespace
{
struct SearchFilterTest
{
    testkit::Tally tally;
    int filter{SearchBoth}; // before it
    za::String query;
    int counts[SearchFilterCount]{};
};
SearchFilterTest searchFilterTest;
constexpr const char* searchFilterTestText = "recoil"; // (both kinds, under searchMaxResults: 12 settings, 6 tunables)
} // namespace

void qvr::menu::searchFilterTest_f()
{
    constexpr const char* name = "menu_search_filter";
    SearchFilterTest& t = searchFilterTest;
    auto check = [&](bool ok, const char* what) { t.tally.check(ok, name, what); };
    const char* phase = Cmd_Argc() > 1 ? Cmd_Argv(1) : "start";
    // Every result shown under the filter is of its kind.
    const auto allOf = [&](bool tunable) {
        return !za::anyOf(search.results.begin(), search.results.end(),
            [&](const SearchResult& r) { return search.index[r.entry].tunable != tunable; });
    };
    const auto finish = [&] {
        search.filter = t.filter;
        search.query = t.query;
        runSearch();
        t.tally.verdict(name);
    };
    const auto spot = [&](int f) {
        const SearchLayout s = searchPageLayout();
        return va("%.0f %.0f", (s.filterX0[f] + s.filterX1[f]) * 0.5f, s.filterYc);
    };
    if(!strcmp(phase, "start"))
    {
        t = SearchFilterTest{};
        t.filter = search.filter;
        t.query = search.query;
        Cbuf_AddText("menu_vr\nwait\nvr_menu_search_filter_test open\n");
        return;
    }
    if(!strcmp(phase, "open"))
    {
        openSearchPage();
        check(key_dest == key_menu && m_state == m_vr && ui().page == pageIndex(pageSearch), "the Search page shown");
        search.query = searchFilterTestText;
        if constexpr(!searchFilterShown)
        {
            search.filter = SearchTunables; // (ignored: no tunables)
            runSearch();
            const bool anyTunable = za::anyOf(search.index.begin(), search.index.end(),
                [](const SearchEntry& e) { return e.tunable; });
            Con_Printf("menu_search_filter: a release build, %d results for \"%s\"\n",
                static_cast<int>(search.results.size()), searchFilterTestText);
            check(!searchPageLayout().filter, "no filter row (a release build)");
            check(!anyTunable && !search.results.empty(), "no tunables; every result shown whatever the filter");
            finish();
            return;
        }
        for(int f = 0; f < SearchFilterCount; f++)
        {
            searchSetFilter(f);
            t.counts[f] = static_cast<int>(search.results.size());
            Con_Printf("menu_search_filter: \"%s\", show %s: %d results\n", searchFilterTestText, searchFilterLabels[f],
                t.counts[f]);
            if(f == SearchCvars || f == SearchTunables)
            {
                check(t.counts[f] > 0 && allOf(f == SearchTunables), va("show %s: only its kind, some found",
                                                                          searchFilterLabels[f]));
            }
        }
        const int both = t.counts[SearchBoth], sum = t.counts[SearchCvars] + t.counts[SearchTunables];
        check(both < searchMaxResults && both == sum, va("show Both: the two together (%d of %d)", both, sum));
        check(searchPageLayout().filter, "the filter row laid out");
        searchSetFilter(SearchBoth);
        Cbuf_AddText(va("wait\nvr_mock_mouse %s click\nwait\nvr_menu_search_filter_test mouse\n", spot(SearchTunables)));
        return;
    }
    if(!strcmp(phase, "mouse"))
    {
        check(search.filter == SearchTunables && allOf(true) &&
                  static_cast<int>(search.results.size()) == t.counts[SearchTunables],
            "the mouse's click on Tunables: the tunables' results at once");
        Cbuf_AddText(va("vr_mock_laser %s\nwait\nwait\nwait\nvr_mock_button main trigger 1\nwait\nwait\nwait\n"
                        "vr_mock_button main trigger 0\nwait\nwait\nwait\nvr_menu_search_filter_test laser\n",
            spot(SearchCvars)));
        return;
    }
    if(!strcmp(phase, "laser"))
    {
        check(search.filter == SearchCvars && allOf(false) &&
                  static_cast<int>(search.results.size()) == t.counts[SearchCvars],
            "the laser's trigger on Cvars: the settings' results");
        check(search.hoverFilter == SearchCvars, "the laser over Cvars lights it");
        Cbuf_AddText("vr_mock_laser off\n");
        // The sticks: up from the keys' top row onto the row (its first button), across twice, A; down again.
        search.hoverKey = search.hoverResult = search.hoverRecent = search.hoverFilter = -1;
        search.focusRecent = search.focusResult = search.focusFilter = -1;
        search.focusKey = 0;
        searchKey(K_UPARROW);
        check(search.focusFilter == SearchBoth && search.focusKey < 0, "up from the keys: the filter row's Both");
        searchKey(K_RIGHTARROW);
        searchKey(K_RIGHTARROW);
        searchKey(K_ABUTTON);
        check(search.filter == SearchTunables && allOf(true), "across to Tunables and A: chosen");
        searchKey(K_DOWNARROW);
        check(search.focusFilter < 0 && search.focusKey >= 0 && search.focusKey < 10, "down from the row: the keys' top row");
        finish();
    }
}

// vr_menu_stop_test (Debug > Tools; ROUND25.md, "campvis"): a menu opened while walking stops the player. On e1m1,
// +forward walks him; the VR Settings opened over the running game (live preview): within 3 frames no movement button
// is held and the server's last move is 0, then he comes to rest; the menu closed, he stays at rest (nothing resumes
// until pressed again). Phases (each queues the next itself, a frame apart): start, in (waits for the player), walk,
// menu, rest, closed. Ends with its verdict line.
namespace
{
struct StopTest
{
    testkit::Tally tally;
    int frames{0};
};
StopTest stopTest;
constexpr int stopTestWalkFrames = 36;   // (half a second of walking: well past the run-up)
constexpr int stopTestInputFrames = 3;   // (the release: at the menu's first move; the server has it a frame later)
constexpr int stopTestMaxFrames = 600;   // (the give-up)
[[nodiscard]] float stopTestSpeed()
{
    const edict_t* player = progs::localPlayer();
    return player ? za::sqrt(player->v.velocity[0] * player->v.velocity[0] + player->v.velocity[1] * player->v.velocity[1]) : 0.f;
}
} // namespace

void qvr::menu::stopTest_f()
{
    constexpr const char* name = "menu_stop";
    StopTest& t = stopTest;
    auto check = [&](bool ok, const char* what) { t.tally.check(ok, name, what); };
    const char* phase = Cmd_Argc() > 1 ? Cmd_Argv(1) : "start";
    const auto next = [&](const char* p) { Cbuf_AddText(va("wait\nvr_menu_stop_test %s\n", p)); };
    const auto giveUp = [&](const char* what) {
        check(false, what);
        Cbuf_AddText("-forward\n");
        t.tally.verdict(name);
    };
    if(!strcmp(phase, "start"))
    {
        t = StopTest{};
        Cbuf_AddText("map e1m1\nvr_menu_stop_test in\n");
        return;
    }
    if(++t.frames > stopTestMaxFrames)
    {
        giveUp(va("timed out in phase %s", phase));
        return;
    }
    if(!strcmp(phase, "in"))
    {
        const edict_t* player = progs::localPlayer();
        if(!(cls.signon == SIGNONS && player && player->v.health > 0))
        {
            next("in");
            return;
        }
        t.frames = 0;
        Cbuf_AddText("+forward\n");
        next("walk");
        return;
    }
    if(!strcmp(phase, "walk"))
    {
        if(t.frames < stopTestWalkFrames)
        {
            next("walk");
            return;
        }
        Con_Printf("menu_stop: walking at %.0f\n", stopTestSpeed());
        check(stopTestSpeed() > 100.f, "+forward walks him");
        check(CL_MoveButtonsHeld() > 0, "forward held");
        t.frames = 0;
        Cbuf_AddText("menu_vr\n");
        next("menu");
        return;
    }
    if(!strcmp(phase, "menu"))
    {
        if(t.frames < stopTestInputFrames)
        {
            next("menu");
            return;
        }
        check(key_dest == key_menu, "the menu open");
        PR_SwitchQCVM(&sv.qcvm); // (its monster check reads the server's edicts: a console command has no VM active)
        const bool runs = VR_MenuRunsGame() != 0;
        PR_SwitchQCVM(nullptr);
        check(runs, "the game running on under it (live preview)");
        check(CL_MoveButtonsHeld() == 0, "no movement button held once it opened");
        check(svs.clients[0].cmd.forwardmove == 0.f && svs.clients[0].cmd.sidemove == 0.f, "the server's move 0");
        t.frames = 0;
        next("rest");
        return;
    }
    if(!strcmp(phase, "rest"))
    {
        if(stopTestSpeed() > 1.f)
        {
            if(t.frames % 24 == 0)
            {
                Con_Printf("menu_stop: speed %.0f at %d frames (server time %.2f)\n", stopTestSpeed(), t.frames, sv.qcvm.time);
            }
            next("rest");
            return;
        }
        Con_Printf("menu_stop: at rest after %d frames\n", t.frames + stopTestInputFrames);
        // (friction stops him: under a second; before, the world slid him on at his walking speed into a wall)
        check(t.frames + stopTestInputFrames <= 72, "he comes to rest under the menu within a second");
        t.frames = 0;
        runSetupCloseMenu(); // (closed as a page's own close does)
        next("closed");
        return;
    }
    if(!strcmp(phase, "closed"))
    {
        if(t.frames < stopTestWalkFrames)
        {
            next("closed");
            return;
        }
        check(key_dest == key_game, "the menu closed");
        Con_Printf("menu_stop: %.0f after the close\n", stopTestSpeed());
        check(stopTestSpeed() < 1.f && CL_MoveButtonsHeld() == 0, "nothing resumes on the close");
        t.tally.verdict(name);
    }
}

void qvr::menu::search_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_menu_search [-filter both|cvars|tunables] [-open] <text>: the VR menus' settings, actions and "
                   "pages matching it, best first (-filter: the Search page's filter row set so, kept; a development "
                   "build's; -open: the best opened, as Search's first result picked)\n");
        return;
    }
    buildSearchIndex();
    int arg = 1;
    if(!strcmp(Cmd_Argv(arg), "-filter") && Cmd_Argc() > arg + 1)
    {
        int f = -1;
        for(int i = 0; i < SearchFilterCount; i++)
        {
            f = !q_strcasecmp(Cmd_Argv(arg + 1), searchFilterLabels[i]) ? i : f;
        }
        if(!searchFilterShown || f < 0)
        {
            Con_Printf("vr_menu_search: %s\n", searchFilterShown ? "-filter both, cvars or tunables"
                                                                  : "no filter (a release build: no tunables)");
            return;
        }
        searchSetFilter(f);
        const SearchLayout s = searchPageLayout();
        Con_Printf("vr_menu_search: filter %s; its buttons at", searchFilterLabels[search.filter]);
        for(int i = 0; i < SearchFilterCount; i++)
        {
            Con_Printf(" %s %.0f %.0f", searchFilterLabels[i], (s.filterX0[i] + s.filterX1[i]) * 0.5f, s.filterYc);
        }
        Con_Printf("\n");
        arg += 2;
        if(Cmd_Argc() <= arg)
        {
            return;
        }
    }
    const bool open = !strcmp(Cmd_Argv(arg), "-open");
    arg += open ? 1 : 0;
    za::String text;
    for(; arg < Cmd_Argc(); arg++)
    {
        if(!text.empty())
        {
            text += ' ';
        }
        text += Cmd_Argv(arg);
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
    Con_Printf("vr_menu_search \"%s\"%s: %d results (of %d rows and pages)\n", search.query.cStr(),
        searchFilterShown ? va(" (show %s)", searchFilterLabels[search.filter]) : "",
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
        // From the corner's buttons: the filter row's choice (a development build's), else the keys.
        search.focusKey = searchFilterShown ? -1 : 0;
        search.focusFilter = searchFilterShown ? search.filter : -1;
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
