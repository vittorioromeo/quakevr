// vr_menu_mgroups.inc -- Combat > Monster Groups (vr_mgroups.hpp; ROUND22.md, "Monster groups"): a page for each group
// (a row for every ragdoll trigger and action) and a page for each trigger and action (a row for every group): the same
// settings, two ways in.

enum MgroupKind
{
    MgroupToggle,
    MgroupTimes,
    MgroupChance,
    MgroupResist,
    MgroupThrow,
    MgroupHold,
    MgroupPoise,
    MgroupHealth
};

struct MgroupParamRow
{
    const char* label;
    MgroupKind kind;
    const char* help;
};

// In mgroups::Param's order.
constexpr MgroupParamRow mgroupParamRows[mgroups::paramCount] = {
    {"Ragdoll Triggers", MgroupToggle,
        "The ragdoll triggers (Combat > Ragdoll Triggers) can knock it down: on for the humanoids. Needs Ragdoll Triggers "
        "and Knockdowns on, and a get-up of its own (none for flyers, swimmers, spawns and the bosses)."},
    {"Trigger Resistance", MgroupResist,
        "Every ragdoll trigger's chance divided by this and its least damage times it. 0: by its size (Ragdoll Triggers > "
        "Monster Resistance, By Size)."},
    {"Ledges", MgroupTimes, "Ledges' chance (Ragdoll Triggers) times this for it. 0: never."},
    {"Stairs", MgroupTimes, "Stairs' chance times this for it. 0: never."},
    {"Slopes", MgroupTimes, "Slopes' chance times this for it. 0: never."},
    {"Shotgun Range", MgroupTimes, "Shotgun Range's chance times this for it. 0: never."},
    {"Wounded", MgroupTimes, "Wounded's chance times this for it. 0: never."},
    {"Strong Blows", MgroupTimes, "Strong Blows' chance times this for it. 0: never."},
    {"Explosions", MgroupTimes, "Explosions' chance times this for it. 0: never."},
    {"Thrown Bodies", MgroupTimes, "Thrown Bodies' chance times this for it. 0: never."},
    {"Shoulder Charges", MgroupTimes, "Shoulder Charges' chance times this for it. 0: never."},
    {"Shove Knockdown", MgroupChance,
        "A shove's base chance to knock it down (Combat > Knockdowns: times Chance, the hands, its wounds, your stamina). "
        "Only a monster with a get-up of its own; others are only pushed."},
    {"Shove Push", MgroupTimes,
        "How far your shoves, bashes and unbalancing hits move it, times this (bigger monsters already move less; a "
        "flyer is pushed aside, never knocked down). 0: it stands its ground."},
    {"Hits Unbalance", MgroupTimes,
        "Strong hits' chance to unbalance it (Knockdowns > Hits Unbalance), times this. 0: never. Bosses (1000 health "
        "or more) never."},
    {"Leg Sweep", MgroupToggle, "A staff-like thing swept through its lower legs knocks it down (Knockdowns > Leg Sweep)."},
    {"Leg Pull and Lift", MgroupToggle, "Both hands on its legs pull them out or lift it by them (Holding Enemies)."},
    {"Lift", MgroupToggle, "Both hands pulling up lift it off its feet (Holding Enemies > Lift)."},
    {"Neck Snap", MgroupToggle, "Both hands twisting its head snap its neck (Holding Enemies > Neck Snap)."},
    {"Manhandle", MgroupToggle,
        "Grabbed staggered or weak, it goes down in your hand (Holding Enemies: Grab While Staggered, Grab When Weak)."},
    {"Two-Hand Throw", MgroupThrow,
        "Turning it over hard with both hands throws it down (Holding Enemies > Two-Hand Throw): always, only when hurt "
        "(below When Hurt there), or never. Who Can Be Thrown: By Mass instead."},
    {"Hold Strength", MgroupHold,
        "One hand's hold on it: how much your hold slows it and pulls it along (two hands' together more). By Size: "
        "Holding Enemies' small, big and huge holds (or By Mass)."},
    {"Parry Poise", MgroupPoise,
        "How hard it is to stagger by parrying its blows (Parry and Bash > Parry Balance): your parry's strength (bare "
        "hands 0.4, a weapon 1, a heavy one up to 1.3; two hands x1.5; well timed x1.25) over this: under 0.5 the blow is "
        "only blocked, under 1 it flinches, up to 2 it staggers back as a ragdoll, more longer. Over 1 it also cuts your "
        "melee's chances to knock it down or unbalance it. Off: a parry never staggers it."},
    {"Health", MgroupHealth,
        "Its health times this as it appears, on top of Monster Health's cut for big monsters (Combat > Monster Health; "
        "only with it on). Monsters already about keep theirs. Not the scripted bosses (Chthon, Shub, Arcane "
        "Dimensions' wave bosses)."},
    {"Knockdown Chance", MgroupTimes,
        "Every chance to knock it down times this: shoves, ragdoll triggers, hits, stomps, leg sweeps; a sure one (over "
        "a ledge, unaware) by this chance too. Mini Bosses 0.5: hard to put down."},
    {"Time Down", MgroupTimes,
        "Its time down times this: its own timer (Knockdowns > Time Down) and the most hits and your hands keep it down."},
    {"Get-Up Speed", MgroupTimes, "Its get-up's speed times this (Knockdowns > Get-Up Speed)."},
    {"Neck Snap Kills", MgroupToggle,
        "The neck snap kills it. Off: the twist only hurts it (Holding Enemies > Neck Snap, Tough Neck Damage) and it "
        "fights on."},
    {"Struggle", MgroupToggle,
        "Held while knocked down, it struggles against your hands (Holding Enemies > Struggle): its members with a "
        "struggle of their own (each class's strength, speed, full health and arm drive: Struggle Profiles below). On "
        "for the humanoids; never ogres, fiends or beasts."},
    {"Struggle Strength", MgroupTimes, "Its struggle's strength (each class's own) times this."},
    {"Struggle Speed", MgroupTimes, "Its struggle's speed (each class's own) times this."},
    {"Struggle Arm Drive", MgroupTimes,
        "Its arms' drive while it struggles (each class's own: an enforcer's heavy gun hands 2.5) times this; never under "
        "1 nor over 2.5."},
    {"Struggle Weaker When Hurt", MgroupToggle,
        "It struggles weaker as it is hurt (its health over its class's full health). Off: always at full effort."},
};

[[nodiscard]] Item mgroupItem(int g, mgroups::Param p, const char* label)
{
    const MgroupParamRow& r = mgroupParamRows[static_cast<int>(p)];
    cvar_t& c = mgroups::setting(g, p);
    switch(r.kind)
    {
        case MgroupToggle: return toggle(label, c).help(r.help);
        case MgroupTimes: return slider(label, c, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f).help(r.help);
        case MgroupChance: return slider(label, c, 0.f, 1.f, 0.01f, "%.2f").help(r.help);
        case MgroupResist: return slider(label, c, 0.f, 5.f, 0.05f, "%.2f").extend(0.f, 50.f).help(r.help);
        case MgroupThrow:
            return cycle(label, c, {{0.f, "Never"}, {1.f, "When Hurt"}, {2.f, "Always"}}).help(r.help);
        case MgroupHold:
        {
            Item i = slider(label, c, -0.05f, 1.f, 0.05f, "%.2f");
            i.negativeLabel = "By Size";
            i.negativeStart = 0.f;
            return i.help(r.help);
        }
        case MgroupPoise:
        {
            Item i = slider(label, c, -0.05f, 3.f, 0.05f, "%.2f").extend(-0.05f, 10.f);
            i.negativeLabel = "Off";
            i.negativeStart = 0.f;
            return i.help(r.help);
        }
        case MgroupHealth: return slider(label, c, 0.25f, 2.f, 0.05f, "%.2fx").extend(0.05f, 10.f).help(r.help);
    }
    return header(label);
}

// A group's page: every trigger and action.
template <int G>
[[nodiscard]] za::Vector<Item> pageMonsterGroup()
{
    za::Vector<Item> items;
    items.pushBack(header("Ragdoll Triggers"));
    for(int p = 0; p < mgroups::paramCount; ++p)
    {
        if(p == static_cast<int>(mgroups::Param::Shove))
        {
            items.pushBack(header("Actions"));
        }
        if(p == static_cast<int>(mgroups::Param::Poise))
        {
            items.pushBack(header("Parry"));
        }
        if(p == static_cast<int>(mgroups::Param::Health))
        {
            items.pushBack(header("Health"));
        }
        if(p == static_cast<int>(mgroups::Param::KdScale))
        {
            items.pushBack(header("Toughness"));
        }
        if(p == static_cast<int>(mgroups::Param::Struggle))
        {
            items.pushBack(header("Struggle"));
        }
        items.pushBack(mgroupItem(G, static_cast<mgroups::Param>(p), mgroupParamRows[p].label));
    }
    if(mgroups::struggleProfilesText(G)[0])
    {
        items.pushBack(command("Struggle Profiles", "vr_mgroup_list")
                           .help(mgroups::struggleProfilesText(G))); // (strength/speed/full health/arm drive per class)
    }
    items.pushBack(header("Members"));
    items.pushBack(command("List Every Monster's Group", "vr_mgroup_list")
                       .help(mgroups::members(G)));
    return items;
}

// A trigger's or an action's page: every group.
template <int P>
[[nodiscard]] za::Vector<Item> pageMonsterParam()
{
    za::Vector<Item> items;
    if constexpr(P == static_cast<int>(mgroups::Param::Resist))
    {
        items.pushBack(row(opt::vr_kdtrig_resist_size));
        items.pushBack(header("Each Group"));
    }
    for(int g = 0; g < mgroups::groupCount; ++g)
    {
        items.pushBack(mgroupItem(g, static_cast<mgroups::Param>(P), mgroups::label(g)));
    }
    return items;
}

// Each group's row from G on (each its own page: a template per group).
template <int G>
void mgroupGroupRows(za::Vector<Item>& items)
{
    if constexpr(G < mgroups::groupCount)
    {
        items.pushBack(open(mgroups::label(G), pageIndex(pageMonsterGroup<G>)).help(mgroups::members(G)));
        mgroupGroupRows<G + 1>(items);
    }
}

// Each trigger's and action's row from P on.
template <int P>
void mgroupParamRowsOpen(za::Vector<Item>& items)
{
    if constexpr(P < mgroups::paramCount)
    {
        items.pushBack(open(mgroupParamRows[P].label, pageIndex(pageMonsterParam<P>)).help(mgroupParamRows[P].help));
        mgroupParamRowsOpen<P + 1>(items);
    }
}

[[nodiscard]] za::Vector<Item> pageMonsterGroups()
{
    za::Vector<Item> items{
        command("List Every Monster's Group", "vr_mgroup_list")
            .help("vr_mgroup_list: every monster's classname (id1, the mission packs, Dawn of the Machine, Arcane "
                  "Dimensions and Quoth's) and its group, then this map's monsters' to the console."),
    };
    mgroupGroupRows<0>(items);
    items.pushBack(header("By Trigger or Action"));
    mgroupParamRowsOpen<0>(items);
    return items;
}

// Combat > Monster Health (vr_mgroups.cpp healthMultiplier; vr_cvars.inc vr_monster_health*): big monsters' health cut
// by a curve on their full health, then each group's own multiplier; examples below, as set now.
[[nodiscard]] const char* monsterHealthExample(int i)
{
    const char* line = mgroups::healthExampleLine(i);
    return line ? line : "";
}

[[nodiscard]] za::Vector<Item> pageMonsterHealth()
{
    za::Vector<Item> items{
        row(opt::vr_monster_health),
        header("Curve"),
        open("Each Group's Health", pageIndex(pageMonsterParam<static_cast<int>(mgroups::Param::Health)>))
            .help("Each monster group's health times its own multiplier, on top of the curve (Monster Groups > Health)."),
    };
    for(int i = 0; mgroups::healthExampleLine(i); ++i)
    {
        items.pushBack(infoLine(monsterHealthExample, i));
    }
    return items;
}
