// vr_menu_toolgun.inc -- the toolgun panel's cheats (vr_toolgun_panel.cpp, its More page; docs/vr-port/TOOLGUN.md):
// Debug > Cheats and Recording's own rows (the same commands, actions and switches), the commonest, by their labels, each
// with a short name for the panel's buttons. Included by vr_menu.cpp (inside its namespace); qvr::menu::toolgunCheat*
// (vr_menu.cpp) hand them out.

// The rows wanted (their labels on the cheats page) and their names on the panel.
struct ToolgunCheatName
{
    const char* label;
    const char* name;
};
constexpr ToolgunCheatName toolgunCheatNames[] = {{"God Mode", "God Mode"}, {"Noclip", "Noclip"}, {"Notarget", "Notarget"},
    {"Fly", "Fly"}, {"All Weapons, Ammo and Keys", "All Weapons"}, {"Full Health and Armour", "Full Health"},
    {"Infinite Ammo", "Infinite Ammo"}, {"Kill All Monsters", "Kill Monsters"}, {"Freeze Monsters", "Freeze Monsters"},
    {"Die Now", "Die Now"}, {"Quick Save", "Quick Save"}, {"Quick Load", "Quick Load"}, {"Restart the Map", "Restart Map"},
    {"Props Made Since the Map Loaded", "Clear Props"}, {"Corpses and Ragdolls", "Clear Corpses"}, {"Everything", "Clear All"},
    {"Normal Speed", "Speed 1x"}, {"Half Speed", "Speed 1/2"}, {"Quarter Speed", "Speed 1/4"}, {"Pause the World", "Pause"}};
constexpr int toolgunCheatMax = static_cast<int>(sizeof(toolgunCheatNames) / sizeof(toolgunCheatNames[0]));

// A row found on the cheats page: what it does (its command, its action or its switch's cvar).
struct ToolgunCheat
{
    const char* name{nullptr};
    const char* command{nullptr};
    void (*action)(){nullptr};
    cvar_t* cvar{nullptr};
};

// The rows, found once (the cheats page built and walked: its labels, commands and cvars are the program's own); the
// main thread (the panel's layout).
struct ToolgunCheats
{
    za::InPlaceVector<ToolgunCheat, toolgunCheatMax> list;
    bool found{false};
};
ToolgunCheats toolgunCheats;

void findToolgunCheats()
{
    if(toolgunCheats.found)
    {
        return;
    }
    toolgunCheats.found = true;
    const za::Vector<Item> all = pageDebugCheats();
    for(const ToolgunCheatName& n : toolgunCheatNames)
    {
        for(const Item& item : all)
        {
            if(item.label && !strcmp(item.label, n.label) &&
                ((item.kind == Item::Action && (item.command || item.action)) || (item.kind == Item::Cycle && item.cvar)))
            {
                toolgunCheats.list.pushBack({n.name, item.command, item.action, item.kind == Item::Cycle ? item.cvar : nullptr});
                break;
            }
        }
    }
}

// Whether a cheat is on now: a switch's cvar, or god mode, notarget, noclip and fly as the player has them.
[[nodiscard]] bool toolgunCheatIsOn(const ToolgunCheat& c)
{
    if(c.cvar)
    {
        return c.cvar->value != 0.f;
    }
    const edict_t* player = progs::playerEdict();
    if(!c.command || !player)
    {
        return false;
    }
    const int flags = static_cast<int>(player->v.flags);
    if(!strcmp(c.command, "god"))
    {
        return (flags & FL_GODMODE) != 0;
    }
    if(!strcmp(c.command, "notarget"))
    {
        return (flags & FL_NOTARGET) != 0;
    }
    if(!strcmp(c.command, "noclip"))
    {
        return static_cast<int>(player->v.movetype) == MOVETYPE_NOCLIP;
    }
    if(!strcmp(c.command, "fly"))
    {
        return static_cast<int>(player->v.movetype) == MOVETYPE_FLY;
    }
    return false;
}
