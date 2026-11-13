// vr_menu_runsetup.inc -- the Run Setup screen (included by vr_menu_maps.inc): a run's difficulty, presets and
// mutators before it starts, flat and VR alike (a VR page; the flat menus open it as their own). Opened by the Map
// Library's Play, Single Player > New Game (menu.c), Official Campaigns' rows and the hub's teleporter (QC
// vr_run.qc: `vr_runsetup hub`). Its rows are the registry's (vr_runsetup.cpp): each a cycle on vr_runpick_<id>, a
// line instead while another's rule holds it; Start makes them the run's (runsetup::start) and starts what opened it.
// A start map with skill halls (id's start, a pack's hall: the campaigns, New Game, the hub, such a package) has no
// Difficulty row: its halls choose it. The mutators and presets are asked there too.

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
};
RunSetupState runSetup;

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

za::Vector<Item> pageRunSetup()
{
    runSetup.builtKey = runsetup::pageKey();
    runSetup.builtHalls = runSetup.skillHalls ? 1 : 0;
    za::Vector<Item> items;
    for(int s = 0; s < 3; s++)
    {
        if(runSetupSections[s])
        {
            items.pushBack(header(runSetupSections[s]));
        }
        if(s == static_cast<int>(runsetup::Section::Difficulty) && runSetup.skillHalls)
        {
            items.pushBack(info(runSetupHallsLine));
        }
        for(int i = 0; i < runsetup::count(); i++)
        {
            const runsetup::Mutator& m = runsetup::at(i);
            if(static_cast<int>(m.section) != s || !m.isShown() ||
                (runSetup.skillHalls && i == runsetup::find("skill")))
            {
                continue;
            }
            if(runsetup::forcedValue(i) >= 0)
            {
                items.pushBack(infoLine(runSetupHeldLine, i));
                continue;
            }
            za::Vector<Choice> choices;
            for(int o = 0; o < m.optionCount(); o++)
            {
                choices.pushBack(Choice{static_cast<float>(o), m.options[o]});
            }
            items.pushBack(cycle(m.label, *m.pick, ZA_MOVE(choices)).help(m.help));
        }
    }
    items.pushBack(action("Start", runSetupStart)
                       .help("Starts the run with these choices (kept for next time; each saved game keeps its run's "
                             "and sets them back when loaded). Back: not started."));
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
    ui().cursors[page] = static_cast<int>(pageRunSetup().size()) - 1;
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
