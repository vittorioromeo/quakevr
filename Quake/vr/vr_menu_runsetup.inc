// vr_menu_runsetup.inc -- the Run Setup screen (included by vr_menu_maps.inc): a run's difficulty, presets and
// mutators before it starts, flat and VR alike (a VR page; the flat menus open it as their own). Opened by the Map
// Library's Play, Single Player > New Game (menu.c), Official Campaigns' rows and the hub's teleporter (QC
// vr_run.qc: `vr_runsetup hub`). Its rows are the registry's (vr_runsetup.cpp): each a cycle on vr_runpick_<id>, a
// line instead while another's rule holds it; Start makes them the run's (runsetup::start) and starts what opened it.
// A start map with skill halls (id's start, a pack's hall: the campaigns, New Game, the hub, such a package): its halls
// choose the difficulty, so its Difficulty row is only Skill Halls or Bloody Nightmare. The mutators and presets are
// asked there too. Difficulty, the Starting gun and melee weapon and Enemy Count are icon pickers (vr_menu_icons.inc).

// What opened it, and what the page was built for (rebuilt when that or a held row changes: runSetupStale).
struct RunSetupState
{
    runsetup::Launch launch{runsetup::Launch::None};
    za::String sha;
    za::String map; // "": the package's start map
    int campaign{-1};
    bool skillHalls{false};
    int builtKey{-1};
    int builtHalls{-1};
    int lastSkill{1}; // Difficulty's last choice before Bloody Nightmare (a start hall's "Skill Halls" choice keeps it)
};
RunSetupState runSetup;

// The icon pickers' pictures (vr_menu_icons.inc; vr_menu_icons 0: plain rows), one a choice in the registry's order:
// the guns and melee weapons the toolgun's thumbnails of their models, Nothing a bare hand, Difficulty's emblems,
// Enemy Count's grunts.
constexpr MenuIcon runSetupSkillIcons[] = {{.emblem = MenuIcon::Easy}, {.emblem = MenuIcon::Normal},
    {.emblem = MenuIcon::Hard}, {.emblem = MenuIcon::Nightmare}, {.emblem = MenuIcon::Bloody}};
constexpr MenuIcon runSetupHallsIcons[] = {{.emblem = MenuIcon::Halls}, {.emblem = MenuIcon::Bloody}};
constexpr MenuIcon runSetupGunIcons[] = {{.model = "progs/v_shot.mdl", .side = true},
    {.model = "progs/v_nailpistol.mdl", .side = true}, {.model = "progs/v_revolver.mdl", .side = true},
    {.emblem = MenuIcon::Hand}};
constexpr MenuIcon runSetupMeleeIcons[] = {{.model = "progs/v_axe.mdl", .side = true},
    {.model = "progs/v_dagger.mdl", .side = true}, {.emblem = MenuIcon::Hand}};
constexpr MenuIcon runSetupCountIcons[] = {{.model = "progs/soldier.mdl", .figures = 1},
    {.model = "progs/soldier.mdl", .figures = 2}, {.model = "progs/soldier.mdl", .figures = 3}};
struct RunSetupIconRow
{
    const char* id;
    const MenuIcon* icons;
    int count;
};
constexpr RunSetupIconRow runSetupIconRows[] = {
    {"skill", runSetupSkillIcons, 5},
    {"startgun", runSetupGunIcons, 4},
    {"startmelee", runSetupMeleeIcons, 3},
    {"enemycount", runSetupCountIcons, 3},
};

// Registry row i's pictures (as many as its choices), or nullptr.
const MenuIcon* runSetupIcons(int i)
{
    const runsetup::Mutator& m = runsetup::at(i);
    for(const RunSetupIconRow& r : runSetupIconRows)
    {
        if(!strcmp(r.id, m.id) && r.count == m.optionCount())
        {
            return r.icons;
        }
    }
    return nullptr;
}

// (the Difficulty section has no header: the page starts with it, the next sections' headers give the gap)
constexpr const char* runSetupSections[] = {nullptr, "Presets", "Mutators"};

const char* runSetupHallsLine()
{
    return "Difficulty: the start map's skill halls";
}

// A row held by another's rule: "Starting gun: Nothing (Melee Only)".
const char* runSetupHeldLine(int i)
{
    const runsetup::Mutator& m = runsetup::at(i);
    return va("%s: %s (%s)", m.label, m.options[runsetup::pickValue(i)], m.heldBy ? m.heldBy : "held");
}

bool runSetupStale()
{
    return runSetup.builtKey != runsetup::pageKey() || runSetup.builtHalls != (runSetup.skillHalls ? 1 : 0);
}

void runSetupCloseMenu()
{
    IN_Activate();
    key_dest = key_game;
    m_state = m_none;
}

// Start: the picks made the run's, then what opened the screen starts it.
void runSetupStart()
{
    runsetup::start();
    const runsetup::Launch launch = runSetup.launch;
    runSetup.launch = runsetup::Launch::None;
    switch(launch)
    {
        case runsetup::Launch::Library:
            if(mapinstall::play(runSetup.sha, runSetup.map.size() ? runSetup.map.cStr() : nullptr,
                   runSetup.skillHalls ? -1 : runsetup::activeValue(runsetup::find("skill"))))
            {
                S_LocalSound("misc/menu3.wav");
                return;
            }
            S_LocalSound("misc/menu1.wav");
            goBack(); // (the library shows why: the package's line)
            return;
        case runsetup::Launch::NewGame: M_NewGameStart(); return;
        case runsetup::Launch::Campaign: VR_SelectCampaign(runSetup.campaign); return;
        case runsetup::Launch::Level:
            M_LevelStart(runSetup.map.cStr(), runSetup.skillHalls ? -1 : runsetup::activeValue(runsetup::find("skill")));
            return;
        case runsetup::Launch::Hub:
            Cvar_Set("vr_run_hubgo", "1"); // (the teleporter lets the player through: QC vr_run.qc)
            runSetupCloseMenu();
            return;
        case runsetup::Launch::None: runSetupCloseMenu(); return;
    }
}

// The mutators in groups (vr_runsetup_groups; ROUND25.md, "Menu look"), each its own section in this order; a mutator
// in none goes under the last.
struct RunSetupGroup
{
    const char* title;
    const char* ids[8];
};
constexpr RunSetupGroup runSetupGroups[] = {
    {"Rules", {"startgun", "startmelee", "meleeonly", "glasscannon", "ironman"}},
    {"Enemies", {"armed", "rangedknights", "wildweapons", "armoredfoes", "smartai", "enemycount", "enemyrandom"}},
    {"World", {"pickups"}},
};
constexpr int runSetupGroupCount = static_cast<int>(sizeof(runSetupGroups) / sizeof(runSetupGroups[0]));

// Mutator `id`'s group (the last for one in none).
int runSetupGroupOf(const char* id)
{
    for(int g = 0; g < runSetupGroupCount; g++)
    {
        for(const char* listed : runSetupGroups[g].ids)
        {
            if(listed && !strcmp(listed, id))
            {
                return g;
            }
        }
    }
    return runSetupGroupCount - 1;
}

// Registry row i: a cycle (Difficulty big: Item::Big), or a line while another's rule holds it.
void runSetupRow(za::Vector<Item>& items, int i)
{
    const runsetup::Mutator& m = runsetup::at(i);
    if(runsetup::forcedValue(i) >= 0)
    {
        items.pushBack(infoLine(runSetupHeldLine, i));
        return;
    }
    za::Vector<Choice> choices;
    for(int o = 0; o < m.optionCount(); o++)
    {
        choices.pushBack(Choice{static_cast<float>(o), m.options[o]});
    }
    Item row = cycle(m.label, *m.pick, ZA_MOVE(choices)).help(m.help);
    if(const MenuIcon* icons = runSetupIcons(i))
    {
        row = row.iconPicker(icons);
    }
    items.pushBack(i == runsetup::find("skill") ? row.big() : row);
}

// A start map's skill halls choose the difficulty: its row only "Skill Halls" (Difficulty's last choice kept) or Bloody
// Nightmare (always a choice: his call, 2026-11-14).
void runSetupHallsRow(za::Vector<Item>& items)
{
    za::Vector<Choice> choices;
    choices.pushBack(Choice{static_cast<float>(runSetup.lastSkill), "Skill Halls"});
    choices.pushBack(Choice{static_cast<float>(runsetup::skillBloody), "Bloody Nightmare"});
    items.pushBack(cycle("Difficulty", vr_maps_skill, ZA_MOVE(choices))
                       .help("The start map's skill halls choose the difficulty. Bloody Nightmare: Nightmare and Dawn of "
                             "the Machine's hidden difficulty (harder monsters, each level begun with the axe and shotgun only).")
                       .big()
                       .iconPicker(runSetupHallsIcons));
}

za::Vector<Item> pageRunSetup()
{
    runSetup.builtKey = runsetup::pageKey();
    runSetup.builtHalls = runSetup.skillHalls ? 1 : 0;
    if(const int s = static_cast<int>(vr_maps_skill.value + 0.5f); s >= 0 && s < runsetup::skillBloody)
    {
        runSetup.lastSkill = s;
    }
    const bool grouped = vr_runsetup_groups.value != 0.f;
    za::Vector<Item> items;
    for(int s = 0; s < 3; s++)
    {
        const bool mutators = s == static_cast<int>(runsetup::Section::Mutators);
        if(runSetupSections[s] && !(mutators && grouped))
        {
            items.pushBack(header(runSetupSections[s]));
        }
        if(s == static_cast<int>(runsetup::Section::Difficulty) && runSetup.skillHalls && vr_runsetup_halls_line.value != 0.f)
        {
            items.pushBack(info(runSetupHallsLine));
        }
        if(s == static_cast<int>(runsetup::Section::Difficulty) && runSetup.skillHalls)
        {
            runSetupHallsRow(items);
        }
        const auto shown = [s](int i) {
            const runsetup::Mutator& m = runsetup::at(i);
            return static_cast<int>(m.section) == s && m.isShown() && m.ownRow &&
                   !(runSetup.skillHalls && i == runsetup::find("skill"));
        };
        if(mutators && grouped)
        {
            for(int g = 0; g < runSetupGroupCount; g++)
            {
                const size_t before = items.size();
                items.pushBack(header(runSetupGroups[g].title));
                for(const char* id : runSetupGroups[g].ids)
                {
                    if(const int i = id ? runsetup::find(id) : -1; i >= 0 && shown(i))
                    {
                        runSetupRow(items, i);
                    }
                }
                for(int i = 0; g == runSetupGroupCount - 1 && i < runsetup::count(); i++)
                {
                    if(shown(i) && runSetupGroupOf(runsetup::at(i).id) == g)
                    {
                        bool listed = false;
                        for(const char* id : runSetupGroups[g].ids)
                        {
                            listed = listed || (id && !strcmp(id, runsetup::at(i).id));
                        }
                        if(!listed)
                        {
                            runSetupRow(items, i); // (a mutator in no group: the last's)
                        }
                    }
                }
                if(items.size() == before + 1)
                {
                    items.popBack(); // (a group with nothing shown: no header)
                }
            }
            continue;
        }
        for(int i = 0; i < runsetup::count(); i++)
        {
            if(shown(i))
            {
                runSetupRow(items, i);
            }
        }
    }
    // Start: a big button of its own below the mutators (Item::Button: a gap above it, not in their section).
    items.pushBack(action("Start", runSetupStart)
                       .help("Starts the run with these choices (kept for next time; each saved game keeps its run's "
                             "and sets them back when loaded). Back: not started.")
                       .button());
    return items;
}

// The screen opened for `launch` (its cursor on Start: the last choices are one press away), or, with
// vr_runsetup_ask 0, the run started at once with them.
void openRunSetup(runsetup::Launch launch, const za::String& sha, const char* map, int campaign, bool skillHalls)
{
    runSetup.launch = launch;
    runSetup.sha = sha;
    runSetup.map = map ? za::String{map} : za::String{};
    runSetup.campaign = campaign;
    runSetup.skillHalls = skillHalls;
    if(vr_runsetup_ask.value == 0.f)
    {
        runSetupStart();
        return;
    }
    const int page = pageIndex(pageRunSetup);
    openFromAnywhere(page);
    // Built anew for this launch (its Difficulty row or the halls' line, its held rows), never the last launch's rows:
    // runSetupStale alone missed it (a page built for the size above it counted as this launch's).
    menuPages.done[page] = false;
    const za::Vector<Item>& built = items(page);
    // At its top (his flat report 2026-11-13: the cursor on Start scrolled the Difficulty row out of view on a map
    // without skill halls, the page a row longer than the view): the cursor on Start while it is in view from there,
    // else on the first choice.
    int start = 0;
    int first = -1;
    for(int i = 0; i < static_cast<int>(built.size()); i++)
    {
        start = built[i].look == Item::Button && built[i].label && !strcmp(built[i].label, "Start") ? i : start;
        first = first < 0 && selectable(built[i]) ? i : first;
    }
    ui().scrolls[page] = 0;
    ui().cursors[page] = start < rowsFrom(built, 0) || first < 0 ? start : first;
}

void openRunSetupCampaign(int index)
{
    if(VR_CampaignUnavailable(index) != 0)
    {
        VR_SelectCampaign(index); // (only its reason printed)
        return;
    }
    openRunSetup(runsetup::Launch::Campaign, za::String{}, nullptr, index, true);
}
