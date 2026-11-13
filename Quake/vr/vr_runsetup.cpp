// vr_runsetup.cpp -- the run setup's registry, its run-scoped values, the saved games' `// qvr_run` line and the
// console (vr_runsetup.hpp; docs/vr-port/MUTATORS.md). The screen is the menu's: vr_menu_runsetup.inc.

#include "vr_runsetup.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gameplaypresets.hpp"
#include "vr_gorepresets.hpp"
#include "vr_presets.hpp"
#include "vr_progs.hpp"
#include "vr_srvrandom.hpp"
#include "vr_testkit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace qvr::runsetup
{

namespace
{

// ----------------------------------------------------------------------------
// The choices' options
// ----------------------------------------------------------------------------

constexpr const char* skillOptions[] = {"Easy", "Normal", "Hard", "Nightmare"};
constexpr const char* offOn[] = {"Off", "On"};
constexpr const char* styleOptions[] = {"My Settings", "Classic", "Light", "Unleashed"};
constexpr const char* goreOptions[] = {"My Settings", "Full", "Minimal", "Off"};
constexpr const char* followOffOn[] = {"As Style", "Off", "On"};
constexpr const char* startGunOptions[] = {"Shotgun", "Nail Pistol", "Heavy Revolver", "Nothing"};
constexpr const char* startMeleeOptions[] = {"Axe", "Dagger", "Nothing"};
constexpr const char* pickupsOptions[] = {"Off", "Balanced", "Anything Goes"};
constexpr const char* enemyCountOptions[] = {"1x", "2x", "3x"};
constexpr const char* enemyRandomOptions[] = {"Off", "Balanced", "Anything Goes"};

constexpr int startGunNothing = 3;

[[nodiscard]] int cvarInt(const cvar_t& c)
{
    return static_cast<int>(c.value + (c.value < 0.f ? -0.5f : 0.5f));
}

// ----------------------------------------------------------------------------
// What the choices do beyond their cvar
// ----------------------------------------------------------------------------

bool bnShown()
{
    return vr_mg3_bn_discovered.value != 0.f;
}

// Bloody Nightmare holds the difficulty at Nightmare.
int skillForced()
{
    return bnShown() && vr_runpick_bn.value != 0.f ? 3 : -1;
}

void applySkill(int value, Phase phase)
{
    if(phase == Phase::Start)
    {
        Cvar_SetValueQuick(&skill, static_cast<float>(value));
    }
}

// Its start: skill 3 and vr_mg3_bn_start (QC MG3_Frame: Dawn of the Machine's start map makes it a Bloody Nightmare
// game; elsewhere it is cleared and nothing else happens). A loaded game's serverflags carry it: never a new start.
void applyBloody(int value, Phase phase)
{
    if(phase == Phase::Start && value != 0)
    {
        Cvar_SetValueQuick(&skill, 3.f);
        Cvar_Set("vr_mg3_bn_start", "1");
    }
    else
    {
        Cvar_Set("vr_mg3_bn_start", "0");
    }
}

// Style: the Gameplay preset (0 My Settings; else its level + 1). Not applied when the settings match it already.
void applyStyle(int value, Phase)
{
    presetcore::Kind& k = gameplaypresets::kind();
    if(value <= 0)
    {
        if(k.hasMine())
        {
            (void)k.restoreMine();
        }
    }
    else if(k.matchingLevel() != value - 1)
    {
        k.apply(value - 1);
    }
    gameplaypresets::syncClassic();
}

void applyGore(int value, Phase)
{
    if(value <= 0)
    {
        if(gorepresets::kind().hasMine())
        {
            (void)gorepresets::restoreMine();
        }
    }
    else if(gorepresets::matching() != value)
    {
        gorepresets::apply(value);
    }
}

// Armed Humanoids, Smarter AI: rows of the Gameplay preset set over the Style (My Settings kept first).
void setGameplayRows(cvar_t* const* vars, int n, int value)
{
    if(value <= 0)
    {
        return; // (as the Style left them)
    }
    gameplaypresets::kind().keepMine();
    for(int i = 0; i < n; i++)
    {
        Cvar_SetQuick(vars[i], value == 2 ? "1" : "0");
    }
}

void applyArmed(int value, Phase)
{
    cvar_t* const vars[] = {&vr_agrunt_repl, &vr_agknight, &vr_agdknight};
    setGameplayRows(vars, 3, value);
}

void applySmartAi(int value, Phase)
{
    cvar_t* const vars[] = {&vr_ai_enhanced};
    setGameplayRows(vars, 1, value);
}

// Melee Only holds the Starting gun at nothing.
int startGunForced()
{
    return vr_runpick_meleeonly.value != 0.f ? startGunNothing : -1;
}

// Armed Humanoids on as the screen's picks stand: On, or As Style with a Style that has them (Unleashed: the settings'
// defaults; My Settings: yours as they are now; Classic and Light: off).
bool armedPicked()
{
    const int armed = cvarInt(vr_runpick_armed);
    if(armed != 0)
    {
        return armed == 2;
    }
    const int style = cvarInt(vr_runpick_style);
    if(style == 0)
    {
        return vr_agrunt_repl.value != 0.f || vr_agknight.value != 0.f || vr_agdknight.value != 0.f;
    }
    return style == 3;
}

// Ranged Knights, Wild Weaponry, Armored Foes: held Off while Armed Humanoids is off (they change armed humanoids only).
int armedOnlyForced()
{
    return armedPicked() ? -1 : 0;
}

// ----------------------------------------------------------------------------
// The registry: one line a choice, in the screen's order (the presets before the rows they set over).
// ----------------------------------------------------------------------------

#define QVR_OPTIONS(a) a, static_cast<int>(sizeof(a) / sizeof(a[0]))

// clang-format off
const Mutator registry[] = {
    {"skill", "Difficulty", Section::Difficulty, QVR_OPTIONS(skillOptions), 1, -1,
        "Quake's skill: Easy, Normal, Hard, Nightmare.", &vr_maps_skill, &skill, applySkill, skillForced, nullptr, "Bloody Nightmare"},
    {"bn", "Bloody Nightmare", Section::Difficulty, QVR_OPTIONS(offOn), 0, 0,
        "Dawn of the Machine's hidden difficulty: Nightmare, harder monsters, each level begun with the axe and shotgun only.",
        &vr_runpick_bn, &vr_run_bn, applyBloody, nullptr, bnShown, nullptr},
    {"style", "Style", Section::Presets, QVR_OPTIONS(styleOptions), 3, -1,
        "The Gameplay preset: Classic (Quake as it was), Light (Quake's rules, VR's comforts), Unleashed (the mod). "
        "My Settings: yours, as kept at the first preset.", &vr_runpick_style, &vr_run_style, applyStyle, nullptr, nullptr, nullptr},
    {"gore", "Gore", Section::Presets, QVR_OPTIONS(goreOptions), 0, -1,
        "The Gore preset: Full, Minimal, Off; My Settings: yours.", &vr_runpick_gore, &vr_run_gore, applyGore, nullptr, nullptr, nullptr},
    {"armed", "Armed Humanoids", Section::Mutators, QVR_OPTIONS(followOffOn), 0, -1,
        "Grunts, enforcers, knights and death knights as armed humanoids holding real weapons. As Style: the Style's.",
        &vr_runpick_armed, &vr_run_armed, applyArmed, nullptr, nullptr, nullptr},
    {"smartai", "Smarter AI", Section::Mutators, QVR_OPTIONS(followOffOn), 0, -1,
        "The stealth AI: monsters see and hear as people do, search, follow your trail. As Style: the Style's.",
        &vr_runpick_smartai, &vr_run_smartai, applySmartAi, nullptr, nullptr, nullptr},
    {"startgun", "Starting gun", Section::Mutators, QVR_OPTIONS(startGunOptions), 0, 0,
        "The gun a new game starts with, in its hip holster: the shotgun, the nail pistol, the heavy revolver, or none.",
        &vr_runpick_startgun, &vr_run_startgun, nullptr, startGunForced, nullptr, "Melee Only"},
    {"startmelee", "Starting melee weapon", Section::Mutators, QVR_OPTIONS(startMeleeOptions), 0, 0,
        "The melee weapon a new game starts with: the axe, the dagger, or none (bare fists: punch and grab).",
        &vr_runpick_startmelee, &vr_run_startmelee, nullptr, nullptr, nullptr, nullptr},
    {"meleeonly", "Melee Only", Section::Mutators, QVR_OPTIONS(offOn), 0, 0,
        "Guns are not picked up and do not fire; no starting gun (the melee weapon as chosen). Blades, axes, the chainsaw, fists.",
        &vr_runpick_meleeonly, &vr_run_meleeonly, nullptr, nullptr, nullptr, nullptr},
    {"glasscannon", "Glass Cannon", Section::Mutators, QVR_OPTIONS(offOn), 0, 0,
        "You and the monsters take double damage.", &vr_runpick_glasscannon, &vr_run_glasscannon, nullptr, nullptr, nullptr, nullptr},
    {"pickups", "Randomized Pickups", Section::Mutators, QVR_OPTIONS(pickupsOptions), 0, 0,
        "The level's pickups swapped as it loads. Balanced: ammo for ammo of a gun you hold or one nearby, health for health, "
        "armour and weapons for ones close in value. Anything Goes: anything. Keys, runes and triggered items stay.",
        &vr_runpick_pickups, &vr_run_pickups, nullptr, nullptr, nullptr, nullptr},
    {"ironman", "Ironman", Section::Mutators, QVR_OPTIONS(offOn), 0, 0,
        "No saving: save, quicksave and autosave are off. A death sends you back to the start of the level, with what you "
        "entered it with.", &vr_runpick_ironman, &vr_run_ironman, nullptr, nullptr, nullptr, nullptr},
    {"enemycount", "Enemy Count", Section::Mutators, QVR_OPTIONS(enemyCountOptions), 0, 0,
        "Each monster of a map comes with one or two more of its kind beside it. The map's own still open its doors.",
        &vr_runpick_enemycount, &vr_run_enemycount, nullptr, nullptr, nullptr, nullptr},
    {"enemyrandom", "Randomized Enemies", Section::Mutators, QVR_OPTIONS(enemyRandomOptions), 0, 0,
        "Monsters swapped as a map loads. Balanced: one as tough that moves as it does and fits its spot (Arcane "
        "Dimensions' too when mounted). Anything Goes: any that fits.",
        &vr_runpick_enemyrandom, &vr_run_enemyrandom, nullptr, nullptr, nullptr, nullptr},
    {"rangedknights", "Ranged Knights", Section::Mutators, QVR_OPTIONS(offOn), 0, 0,
        "Knights and death knights may hold guns and take them up: mostly their swords (two now and then), crossbows "
        "with Arcane Dimensions, rarely a chainsaw, more rarely another gun. Needs Armed Humanoids.",
        &vr_runpick_rangedknights, &vr_run_rangedknights, nullptr, armedOnlyForced, nullptr, "Armed Humanoids off"},
    {"wildweapons", "Wild Weaponry", Section::Mutators, QVR_OPTIONS(offOn), 0, 0,
        "Armed humanoids may hold any weapon from the first map on, whether you have found it or not (the first grunt "
        "may hold a rocket launcher). Needs Armed Humanoids.",
        &vr_runpick_wildweapons, &vr_run_wildweapons, nullptr, armedOnlyForced, nullptr, "Armed Humanoids off"},
    {"armoredfoes", "Armored Foes", Section::Mutators, QVR_OPTIONS(offOn), 0, 0,
        "Armed humanoids spawn in armour far more often (the difficulty's armour types and pieces). Needs Armed "
        "Humanoids.",
        &vr_runpick_armoredfoes, &vr_run_armoredfoes, nullptr, armedOnlyForced, nullptr, "Armed Humanoids off"},
};
// clang-format on

#undef QVR_OPTIONS

constexpr int registryCount = static_cast<int>(sizeof(registry) / sizeof(registry[0]));

// The loading save's line (readSaveLines -> onLoadGame), the line written (saveLine), the summary's text.
struct State
{
    int pending[registryCount]{};
    bool pendingHave[registryCount]{};
    int pendingSeed{0};      // the run's seed (vr_run_seed: `seed=`)
    bool pendingHaveSeed{false}; // ... the line has one (a save from before it was: a new one drawn)
    bool haveLine{false};
    bool reading{false}; // readSaveLines ran for the save being loaded
    char line[256]{};
    char summary[256]{};
};
State state;

// A new run seed (1..0xFFFF, exact in QC's floats): sv_random_seed's when a test fixes it, else the clock's (not
// rand()'s: the C library's sequence starts the same each launch).
[[nodiscard]] int newSeed()
{
    const int fixed = static_cast<int>(Cvar_VariableValue("sv_random_seed"));
    return fixed != 0 ? (fixed & 0xFFFF) + 1 : static_cast<int>(srvrandom::freshSeed(0xFFFFu));
}

void setActive(int i, int value)
{
    const Mutator& m = registry[i];
    value = value < 0 ? 0 : value >= m.optionCount ? m.optionCount - 1 : value;
    Cvar_SetValueQuick(m.active, static_cast<float>(value));
}

void applyActive(int i, Phase phase)
{
    if(registry[i].apply)
    {
        registry[i].apply(activeValue(i), phase);
    }
}

void appendPart(const char* part)
{
    if(state.summary[0])
    {
        q_strlcat(state.summary, " / ", sizeof(state.summary));
    }
    q_strlcat(state.summary, part, sizeof(state.summary));
}

// vr_run: the run's choices; vr_run <id> <value>: one set (and applied, as a run's start would); vr_run start: the
// picks made the run's (the screen's Start without a map).
void runCmd()
{
    if(Cmd_Argc() >= 2 && !strcmp(Cmd_Argv(1), "start"))
    {
        start();
        Con_Printf("vr_run: started: %s\n", summary(false));
        return;
    }
    if(Cmd_Argc() >= 3)
    {
        const int i = find(Cmd_Argv(1));
        if(i < 0)
        {
            Con_Printf("vr_run: no choice \"%s\"\n", Cmd_Argv(1));
            return;
        }
        setActive(i, atoi(Cmd_Argv(2)));
        applyActive(i, Phase::Start);
    }
    for(int i = 0; i < registryCount; i++)
    {
        const Mutator& m = registry[i];
        const int a = activeValue(i);
        const int f = forcedValue(i);
        Con_Printf("  %-12s %d %-14s (screen: %s%s)\n", m.id, a, m.options[a], m.options[pickValue(i)],
            f >= 0 ? ", held" : "");
    }
    Con_Printf("vr_run: %s\n", summary(false));
}

// vr_runsetup_test: the registry (its cvars named after its ids, defaults as the screen's), a run's start (the picks,
// a held row), the save line written and read back (a run's choices, the presets' values), an old save's defaults and
// the hub's end of a run. Presets' My Settings in the test files; every setting it changed set back.
// vr_run_check <startgun> <startmelee> <glasscannon> <meleeonly> <gore>: the running game is that run (its vr_run_* cvars, the
// Gore preset its value sets, QC VR_Run_TestCheck: the player's loadout, a hit's damage, guns refused); vr_run_check
// verdict: "runmutators: N passed, M failed" (the mutators' end-to-end test: a run's start, changelevel, save and load).
testkit::Tally checkTally;

void checkCmd()
{
    constexpr const char* name = "runmutators";
    if(Cmd_Argc() == 2 && !strcmp(Cmd_Argv(1), "verdict"))
    {
        checkTally.verdict(name);
        checkTally = testkit::Tally{};
        return;
    }
    if(Cmd_Argc() < 6)
    {
        Con_Printf("vr_run_check <startgun> <startmelee> <glasscannon> <meleeonly> <gore> | verdict\n");
        return;
    }
    const int sg = atoi(Cmd_Argv(1));
    const int sm = atoi(Cmd_Argv(2));
    const int g = atoi(Cmd_Argv(3));
    const int m = atoi(Cmd_Argv(4));
    const int gore = atoi(Cmd_Argv(5));
    const int w = sg * 10 + sm; // (the QC's one number: gun, melee)
    const char* what = va("%s: run %d %d %d %d %d", sv.name, sg, sm, g, m, gore);
    const bool cvars = activeValue(find("startgun")) == sg && activeValue(find("startmelee")) == sm &&
                       activeValue(find("glasscannon")) == g &&
                       activeValue(find("meleeonly")) == m && activeValue(find("gore")) == gore;
    checkTally.check(cvars, name, va("%s: the run's cvars (%s)", what, summary(false)));
    checkTally.check(gore == 0 || gorepresets::matching() == gore, name, va("%s: the Gore preset's values", what));
    bool qc = false;
    if(sv.active)
    {
        const progs::ServerVm vm;
        dfunction_t* fn = VR_ED_FindFunction("VR_Run_TestCheck");
        qc = fn && progs::qcFloat(static_cast<func_t>(fn - qcvm->functions), static_cast<float>(w), static_cast<float>(g),
                                 static_cast<float>(m)) != 0.f;
    }
    checkTally.check(qc, name, va("%s: the game (loadout, damage, guns)", what));
    if(cvars && qc)
    {
        Con_Printf("runmutators: PASS %s\n", what);
    }
}

// QC function `name` (the server's VM, current) run with self `ent`, other `oth` (the world when null) and one float
// parameter: its float return (0 when the progs have no such function). The caller's self and other are kept.
float qcOnEdict(const char* name, edict_t* ent, edict_t* oth, float a)
{
    dfunction_t* fn = VR_ED_FindFunction(name);
    if(!fn)
    {
        return 0.f;
    }
    const int oldSelf = pr_global_struct->self;
    const int oldOther = pr_global_struct->other;
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(ent);
    pr_global_struct->other = oth ? EDICT_TO_PROG(oth) : 0; // (0: the world)
    G_FLOAT(OFS_PARM0) = a;
    PR_ExecuteProgram(static_cast<func_t>(fn - qcvm->functions));
    const float r = G_FLOAT(OFS_RETURN);
    pr_global_struct->self = oldSelf;
    pr_global_struct->other = oldOther;
    return r;
}

// vr_run_enemies_check <enemycount> <enemyrandom>: the running map as that run spawned it (the run's cvars; QC
// VR_Run_EnemiesCheck: copies beside their originals and of their kind, swaps of the kinds the choice allows, the
// map's death-counting triggers still able to complete, the kill count); vr_run_enemies_check verdict: "runenemies: N
// passed, M failed" (the enemy mutators' test: a run's start, changelevel, save and load, a new run).
// A map's trigger chains (QC VR_Run_EnemiesChains) as its first check with both off found them: later checks of the
// same map with the mutators on must find the same.
struct EnemiesCheck
{
    testkit::Tally tally;
    struct Base
    {
        char map[64];
        float chains;
    };
    Base bases[16]{};
    int baseCount{0};
};
EnemiesCheck enemiesCheck;

void enemiesChains(int c, int r, const char* what)
{
    float chains = 0.f;
    {
        const progs::ServerVm vm;
        dfunction_t* fn = VR_ED_FindFunction("VR_Run_EnemiesChains");
        if(!fn)
        {
            return;
        }
        chains = progs::qcFloat(static_cast<func_t>(fn - qcvm->functions));
    }
    EnemiesCheck& s = enemiesCheck;
    for(int i = 0; i < s.baseCount; i++)
    {
        if(!strcmp(s.bases[i].map, sv.name))
        {
            s.tally.check(chains == s.bases[i].chains, "runenemies",
                va("%s: the map's trigger chains as without the mutators (%.0f, %.0f)", what, chains, s.bases[i].chains));
            return;
        }
    }
    if(c == 0 && r == 0 && s.baseCount < 16)
    {
        q_strlcpy(s.bases[s.baseCount].map, sv.name, sizeof(s.bases[0].map));
        s.bases[s.baseCount++].chains = chains;
    }
}

void enemiesCheckCmd()
{
    constexpr const char* name = "runenemies";
    testkit::Tally& enemiesTally = enemiesCheck.tally;
    if(Cmd_Argc() == 2 && !strcmp(Cmd_Argv(1), "verdict"))
    {
        enemiesTally.verdict(name);
        enemiesCheck = EnemiesCheck{};
        return;
    }
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_run_enemies_check <enemycount> <enemyrandom> | verdict\n");
        return;
    }
    const int c = atoi(Cmd_Argv(1));
    const int r = atoi(Cmd_Argv(2));
    char what[96];
    q_snprintf(what, sizeof(what), "%s: enemies %d %d", sv.name, c, r); // (not va's: the QC check's prints reuse those)
    const bool cvars = activeValue(find("enemycount")) == c && activeValue(find("enemyrandom")) == r;
    enemiesTally.check(cvars, name, va("%s: the run's cvars (%s)", what, summary(false)));
    bool qc = false;
    if(sv.active)
    {
        const progs::ServerVm vm;
        dfunction_t* fn = VR_ED_FindFunction("VR_Run_EnemiesCheck");
        qc = fn && progs::qcFloat(static_cast<func_t>(fn - qcvm->functions), static_cast<float>(c), static_cast<float>(r)) !=
                       0.f;
    }
    enemiesTally.check(qc, name, va("%s: the map's monsters", what));
    if(sv.active)
    {
        enemiesChains(c, r, what);
    }
    if(cvars && qc)
    {
        Con_Printf("runenemies: PASS %s\n", what);
    }
}

// vr_run_armed_check <rangedknights> <wildweapons> <armoredfoes>: the running game as that run arms its humanoids (the
// run's cvars; QC VR_Run_ArmedCheck: the rolls' shares over many spawns as the choices say, the map's own knights and
// armed humanoids as rolled); vr_run_armed_check verdict: "runarmed: N passed, M failed".
testkit::Tally armedTally;

void armedCheckCmd()
{
    constexpr const char* name = "runarmed";
    if(Cmd_Argc() == 2 && !strcmp(Cmd_Argv(1), "verdict"))
    {
        armedTally.verdict(name);
        armedTally = testkit::Tally{};
        return;
    }
    if(Cmd_Argc() < 4)
    {
        Con_Printf("vr_run_armed_check <rangedknights> <wildweapons> <armoredfoes> | verdict\n");
        return;
    }
    const int rk = atoi(Cmd_Argv(1));
    const int ww = atoi(Cmd_Argv(2));
    const int af = atoi(Cmd_Argv(3));
    char what[96];
    q_snprintf(what, sizeof(what), "%s: armed %d %d %d", sv.name, rk, ww, af); // (not va's: the QC check's prints reuse those)
    const bool cvars = activeValue(find("rangedknights")) == rk && activeValue(find("wildweapons")) == ww &&
                       activeValue(find("armoredfoes")) == af;
    armedTally.check(cvars, name, va("%s: the run's cvars (%s)", what, summary(false)));
    bool qc = sv.active;
    for(int part = 1; part <= 4 && sv.active; part++) // (one call a part: thousands of rolls in one are a runaway loop)
    {
        const progs::ServerVm vm;
        dfunction_t* fn = VR_ED_FindFunction(va("VR_Run_ArmedCheck%d", part));
        qc = fn && progs::qcFloat(static_cast<func_t>(fn - qcvm->functions), static_cast<float>(rk), static_cast<float>(ww),
                                 static_cast<float>(af)) != 0.f && qc;
    }
    armedTally.check(qc, name, va("%s: the rolls and the map's armed humanoids", what));
    if(cvars && qc)
    {
        Con_Printf("runarmed: PASS %s\n", what);
    }
}

void testCmd()
{
    testkit::Tally t;
    constexpr const char* name = "runsetup";
    gameplaypresets::kind().useTestFile(true);
    gorepresets::kind().useTestFile(true);
    gameplaypresets::kind().keepMine(); // (the test files: the settings as they are now, set back at the end)
    gorepresets::kind().keepMine();
    float picks[registryCount];
    for(int i = 0; i < registryCount; i++)
    {
        const Mutator& m = registry[i];
        picks[i] = m.pick->value;
        if(m.active != &skill)
        {
            t.check(!strcmp(m.active->name, va("vr_run_%s", m.id)) && !strcmp(m.pick->name, va("vr_runpick_%s", m.id)),
                name, va("%s: its cvars named after it", m.id));
            t.check(atoi(m.pick->default_string) == m.def, name, va("%s: the pick's default is the registry's", m.id));
        }
        t.check(m.def >= 0 && m.def < m.optionCount && m.help && m.help[0], name, va("%s: a default, a help", m.id));
    }

    // A run: Classic, Minimal gore, armed on over it, the revolver, glass cannon.
    Cvar_Set("vr_runpick_style", "1");
    Cvar_Set("vr_runpick_gore", "2");
    Cvar_Set("vr_runpick_armed", "2");
    Cvar_Set("vr_runpick_smartai", "0");
    Cvar_Set("vr_runpick_startgun", "2");
    Cvar_Set("vr_runpick_startmelee", "1");
    Cvar_Set("vr_runpick_meleeonly", "0");
    Cvar_Set("vr_runpick_glasscannon", "1");
    Cvar_Set("vr_runpick_pickups", "2");
    Cvar_Set("vr_runpick_ironman", "1");
    Cvar_Set("vr_runpick_rangedknights", "1");
    Cvar_Set("vr_runpick_wildweapons", "1");
    Cvar_Set("vr_runpick_armoredfoes", "1");
    start();
    t.check(vr_run_rangedknights.value == 1.f && vr_run_wildweapons.value == 1.f && vr_run_armoredfoes.value == 1.f, name,
        "start: Ranged Knights, Wild Weaponry, Armored Foes (Armed Humanoids on)");
    t.check(vr_run_pickups.value == 2.f && vr_run_ironman.value == 1.f && ironman() && vr_run_seed.value >= 1.f, name,
        "start: Randomized Pickups (anything goes), Ironman, the run's seed");
    t.check(gameplaypresets::kind().matchingLevel() == -1 && vr_agrunt_repl.value == 1.f && vr_ai_enhanced.value == 0.f &&
                vr_nav.value == 0.f,
        name, "start: Classic with armed humanoids on over it");
    t.check(gorepresets::matching() == gorepresets::choiceMinimal, name, "start: Gore Minimal");
    t.check(vr_run_startgun.value == 2.f && vr_run_startmelee.value == 1.f && vr_run_glasscannon.value == 1.f && vr_run_meleeonly.value == 0.f, name,
        "start: the mutators' cvars from the picks");
    char saved[256];
    q_strlcpy(saved, saveLine(), sizeof(saved));
    t.check(strstr(saved, "style=1") && strstr(saved, "gore=2") && strstr(saved, "startgun=2") && strstr(saved, "startmelee=1") &&
                strstr(saved, "glasscannon=1") && strstr(saved, "pickups=2") && strstr(saved, "ironman=1") &&
                strstr(saved, "rangedknights=1") && strstr(saved, "wildweapons=1") && strstr(saved, "armoredfoes=1"),
        name, "the save line holds the run");

    // Armed Humanoids off holds the three armed-only rows Off (Off, or As Style with Classic or Light); on frees them.
    {
        const int ids[] = {find("rangedknights"), find("wildweapons"), find("armoredfoes")};
        const auto held = [&](bool want) {
            bool ok = true;
            for(const int i : ids)
            {
                ok = ok && (want ? forcedValue(i) == 0 && pickValue(i) == 0 : forcedValue(i) == -1 && pickValue(i) == 1);
            }
            return ok;
        };
        t.check(held(false), name, "armed-only rows free while Armed Humanoids is On");
        Cvar_Set("vr_runpick_armed", "1");
        t.check(held(true), name, "Armed Humanoids Off: Ranged Knights, Wild Weaponry, Armored Foes held Off");
        Cvar_Set("vr_runpick_armed", "0");
        t.check(held(true), name, "As Style with Classic: held Off");
        Cvar_Set("vr_runpick_style", "3");
        t.check(held(false), name, "As Style with Unleashed: free");
        Cvar_Set("vr_runpick_style", "2");
        t.check(held(true), name, "As Style with Light: held Off");
        Cvar_Set("vr_runpick_armed", "1");
        start();
        t.check(vr_run_rangedknights.value == 0.f && vr_run_wildweapons.value == 0.f && vr_run_armoredfoes.value == 0.f,
            name, "a run with Armed Humanoids Off starts them Off (the picks kept)");
        t.check(vr_runpick_rangedknights.value == 1.f, name, "the held pick itself is kept for next time");
        Cvar_Set("vr_runpick_style", "1");
        Cvar_Set("vr_runpick_armed", "2");
    }
    Con_Printf("runsetup: %s", saved);

    // Melee Only holds the Starting gun at nothing (the melee weapon is still the pick).
    Cvar_Set("vr_runpick_meleeonly", "1");
    t.check(forcedValue(find("startgun")) == startGunNothing && pickValue(find("startgun")) == startGunNothing &&
                forcedValue(find("startmelee")) == -1 && pickValue(find("startmelee")) == 1,
        name, "Melee Only holds the Starting gun at nothing, the melee weapon free");
    const int key = pageKey();
    Cvar_Set("vr_runpick_meleeonly", "0");
    t.check(pageKey() != key && forcedValue(find("startgun")) == -1, name, "let go: the screen's key changes");

    // Another run: Unleashed, gore as mine, no mutators.
    Cvar_Set("vr_runpick_style", "3");
    Cvar_Set("vr_runpick_gore", "1");
    Cvar_Set("vr_runpick_armed", "0");
    Cvar_Set("vr_runpick_startgun", "0");
    Cvar_Set("vr_runpick_startmelee", "0");
    Cvar_Set("vr_runpick_glasscannon", "0");
    Cvar_Set("vr_runpick_pickups", "0");
    Cvar_Set("vr_runpick_ironman", "0");
    Cvar_Set("vr_runpick_rangedknights", "0");
    Cvar_Set("vr_runpick_wildweapons", "0");
    Cvar_Set("vr_runpick_armoredfoes", "0");
    start();
    t.check(gameplaypresets::kind().matchingLevel() == gameplaypresets::levelUnleashed && vr_run_glasscannon.value == 0.f &&
                vr_run_pickups.value == 0.f && !ironman() && vr_run_rangedknights.value == 0.f && vr_run_armoredfoes.value == 0.f,
        name, "run B: Unleashed, no glass cannon, pickups as the map's, saving allowed");

    // The first run's save loaded: its choices and the presets' values back.
    char text[320];
    q_snprintf(text, sizeof(text), "header\n%s// qvr_sound x\n", saved);
    readSaveLines(text);
    onLoadGame();
    t.check(vr_run_style.value == 1.f && vr_run_startgun.value == 2.f && vr_run_startmelee.value == 1.f &&
                vr_run_glasscannon.value == 1.f && vr_run_pickups.value == 2.f && vr_run_ironman.value == 1.f &&
                vr_run_rangedknights.value == 1.f && vr_run_wildweapons.value == 1.f && vr_run_armoredfoes.value == 1.f,
        name, "load A: the run's choices back");
    t.check(vr_agrunt_repl.value == 1.f && vr_nav.value == 0.f && gorepresets::matching() == gorepresets::choiceMinimal,
        name, "load A: the presets' values back (Classic + armed, Gore Minimal)");

    // An old save (no line): the mutators' defaults, the presets as they are.
    readSaveLines("header\n// qvr_save 3 progs 0 build x\n");
    onLoadGame();
    t.check(vr_run_startgun.value == 0.f && vr_run_startmelee.value == 0.f && vr_run_glasscannon.value == 0.f &&
                vr_run_style.value == 0.f && vr_run_pickups.value == 0.f && vr_run_ironman.value == 0.f &&
                gorepresets::matching() == gorepresets::choiceMinimal,
        name, "an old save: mutators at their defaults, presets left as they are");

    // A save from before the gun and the melee weapon were two choices: startweapon 1 (revolver), 2 (axe alone), 3
    // (unarmed) become gun, melee.
    constexpr int oldWant[][2] = {{2, 0}, {3, 0}, {3, 2}};
    for(int v = 1; v <= 3; v++)
    {
        readSaveLines(va("header\n// qvr_run style=0 startweapon=%d\n", v));
        onLoadGame();
        t.check(vr_run_startgun.value == static_cast<float>(oldWant[v - 1][0]) &&
                    vr_run_startmelee.value == static_cast<float>(oldWant[v - 1][1]),
            name, va("an old save's startweapon=%d becomes gun %d, melee %d", v, oldWant[v - 1][0], oldWant[v - 1][1]));
    }

    // The hub ends a run: the mutators back to their defaults.
    Cvar_Set("vr_run_meleeonly", "1");
    Cvar_Set("vr_run_ironman", "1");
    endRun();
    t.check(vr_run_meleeonly.value == 0.f && vr_run_ironman.value == 0.f, name, "the hub ends the run: mutators off");

    // Everything set back.
    for(int i = 0; i < registryCount; i++)
    {
        Cvar_SetValueQuick(registry[i].pick, picks[i]);
    }
    (void)gameplaypresets::kind().restoreMine();
    (void)gorepresets::restoreMine();
    gameplaypresets::kind().useTestFile(false);
    gorepresets::kind().useTestFile(false);
    gameplaypresets::syncClassic();
    t.verdict(name);
}

} // namespace

int count()
{
    return registryCount;
}

const Mutator& at(int i)
{
    return registry[i < 0 ? 0 : i >= registryCount ? registryCount - 1 : i];
}

int find(const char* id)
{
    for(int i = 0; i < registryCount; i++)
    {
        if(!q_strcasecmp(registry[i].id, id))
        {
            return i;
        }
    }
    return -1;
}

int forcedValue(int i)
{
    const Mutator& m = at(i);
    if(!m.forced || (m.shown && !m.shown()))
    {
        return -1;
    }
    return m.forced();
}

int pickValue(int i)
{
    const Mutator& m = at(i);
    const int f = forcedValue(i);
    if(f >= 0)
    {
        return f;
    }
    if(m.shown && !m.shown())
    {
        return m.def;
    }
    const int v = cvarInt(*m.pick);
    return v < 0 ? 0 : v >= m.optionCount ? m.optionCount - 1 : v;
}

int activeValue(int i)
{
    const Mutator& m = at(i);
    const int v = cvarInt(*m.active);
    return v < 0 ? 0 : v >= m.optionCount ? m.optionCount - 1 : v;
}

void start()
{
    int values[registryCount];
    for(int i = 0; i < registryCount; i++)
    {
        values[i] = pickValue(i); // (all read before any is set: a rule reads the picks)
    }
    for(int i = 0; i < registryCount; i++)
    {
        setActive(i, values[i]);
    }
    for(int i = 0; i < registryCount; i++)
    {
        applyActive(i, Phase::Start);
    }
    // The run's seed (Randomized Pickups, and the maps' content: a map's layout the same all through the run), a new
    // one every run even with the same choices. In the saved games (`seed=`): a loaded run's next maps roll as its did.
    Cvar_SetValueQuick(&vr_run_seed, static_cast<float>(newSeed()));
    Cvar_Set("vr_run_hubgo", "0");
    Con_Printf("Run setup: %s\n", summary(false));
}

void ensureSeed()
{
    if(vr_run_seed.value <= 0.f)
    {
        Cvar_SetValueQuick(&vr_run_seed, static_cast<float>(newSeed()));
    }
}

void endRun()
{
    for(int i = 0; i < registryCount; i++)
    {
        const Mutator& m = registry[i];
        if(m.section == Section::Mutators && !m.apply)
        {
            setActive(i, m.def);
        }
    }
}

const char* summary(bool picks)
{
    state.summary[0] = 0;
    auto value = [picks](int i) { return picks ? pickValue(i) : activeValue(i); };
    const int bn = find("bn");
    if(registry[bn].shown() && value(bn))
    {
        appendPart("Bloody Nightmare");
    }
    else
    {
        appendPart(skillOptions[value(find("skill"))]);
    }
    appendPart(styleOptions[value(find("style"))]);
    const int gore = value(find("gore"));
    if(gore)
    {
        appendPart(va("Gore %s", goreOptions[gore]));
    }
    const int armed = value(find("armed"));
    if(armed)
    {
        appendPart(armed == 2 ? "Armed humanoids" : "No armed humanoids");
    }
    const int ai = value(find("smartai"));
    if(ai)
    {
        appendPart(ai == 2 ? "Smarter AI" : "Quake's AI");
    }
    {
        constexpr const char* guns[] = {"Shotgun", "Nail pistol", "Revolver", "No gun"};
        constexpr const char* melees[] = {"axe", "dagger", "no melee"};
        const int sg = value(find("startgun"));
        const int sm = value(find("startmelee"));
        if(sg != 0 || sm != 0)
        {
            appendPart(va("%s and %s start", guns[sg], melees[sm]));
        }
    }
    if(value(find("meleeonly")))
    {
        appendPart("Melee only");
    }
    if(value(find("glasscannon")))
    {
        appendPart("Glass cannon");
    }
    const int pickups = value(find("pickups"));
    if(pickups)
    {
        appendPart(pickups == 2 ? "Wild pickups" : "Random pickups");
    }
    if(value(find("ironman")))
    {
        appendPart("Ironman");
    }
    if(const int count = value(find("enemycount")))
    {
        appendPart(count == 2 ? "3x enemies" : "2x enemies");
    }
    if(const int random = value(find("enemyrandom")))
    {
        appendPart(random == 2 ? "Any enemies" : "Random enemies");
    }
    if(value(find("rangedknights")))
    {
        appendPart("Ranged knights");
    }
    if(value(find("wildweapons")))
    {
        appendPart("Wild weaponry");
    }
    if(value(find("armoredfoes")))
    {
        appendPart("Armored foes");
    }
    return state.summary;
}

const char* saveLine()
{
    q_strlcpy(state.line, "// qvr_run", sizeof(state.line));
    for(int i = 0; i < registryCount; i++)
    {
        q_strlcat(state.line, va(" %s=%d", registry[i].id, activeValue(i)), sizeof(state.line));
    }
    q_strlcat(state.line, va(" seed=%d", static_cast<int>(vr_run_seed.value)), sizeof(state.line));
    q_strlcat(state.line, "\n", sizeof(state.line));
    return state.line;
}

void readSaveLines(const char* text)
{
    state.reading = true;
    state.haveLine = false;
    for(int i = 0; i < registryCount; i++)
    {
        state.pendingHave[i] = false;
    }
    state.pendingSeed = 0;
    state.pendingHaveSeed = false;
    const char* p = text ? strstr(text, "\n// qvr_run ") : nullptr;
    if(!p)
    {
        return;
    }
    state.haveLine = true;
    p += 12;
    while(*p && *p != '\n' && *p != '\r')
    {
        char key[32];
        int value = 0;
        int used = 0;
        if(sscanf(p, " %31[^= \r\n]=%d%n", key, &value, &used) != 2 || used <= 0)
        {
            break;
        }
        p += used;
        if(!strcmp(key, "seed"))
        {
            state.pendingSeed = value;
            state.pendingHaveSeed = true;
            continue;
        }
        if(!strcmp(key, "startweapon")) // (a save from before the gun and the melee weapon were two choices)
        {
            // 0 shotgun, 1 revolver, 2 axe alone, 3 unarmed -> gun, melee
            constexpr int oldGun[] = {0, 2, 3, 3};
            constexpr int oldMelee[] = {0, 0, 0, 2};
            const int v = value < 0 || value > 3 ? 0 : value;
            const int gi = find("startgun");
            const int mi = find("startmelee");
            state.pending[gi] = oldGun[v];
            state.pendingHave[gi] = true;
            state.pending[mi] = oldMelee[v];
            state.pendingHave[mi] = true;
            continue;
        }
        const int i = find(key);
        if(i >= 0)
        {
            state.pending[i] = value;
            state.pendingHave[i] = true;
        }
    }
}

void onLoadGame()
{
    if(!state.reading)
    {
        return;
    }
    state.reading = false;
    if(!state.haveLine)
    {
        Con_Printf("Run setup: this saved game has no run choices (saved before they were): the mutators at their "
                   "defaults, the presets as they are\n");
    }
    // The saved run's seed: its next maps' pickups, crates and barrels, enemy swaps as that run's, not the run played
    // before the load. A save from before the seed was saved: a new one (sv_random_seed's when a test fixes it).
    Cvar_SetValueQuick(&vr_run_seed, static_cast<float>(state.pendingHaveSeed ? state.pendingSeed : newSeed()));
    for(int i = 0; i < registryCount; i++)
    {
        const Mutator& m = registry[i];
        if(state.pendingHave[i])
        {
            setActive(i, state.pending[i]);
            applyActive(i, Phase::Load);
        }
        else if(m.oldSave >= 0)
        {
            setActive(i, m.oldSave);
            applyActive(i, Phase::Load);
        }
        else if(m.active != &skill)
        {
            setActive(i, 0); // (My Settings, As Style: left as they are)
        }
    }
    Con_Printf("Run setup: the saved game's run: %s\n", summary(false));
}

int ironman()
{
    return vr_run_ironman.value != 0.f;
}

int pageKey()
{
    int key = 0;
    for(int i = 0; i < registryCount; i++)
    {
        const Mutator& m = registry[i];
        key = key * 3 + (forcedValue(i) >= 0 ? 1 : 0) + (m.shown && !m.shown() ? 2 : 0);
    }
    return key;
}

void init()
{
    Cmd_AddCommand("vr_run", runCmd);
    Cmd_AddCommand("vr_runsetup_test", testCmd);
    Cmd_AddCommand("vr_run_check", checkCmd);
    Cmd_AddCommand("vr_run_enemies_check", enemiesCheckCmd);
    Cmd_AddCommand("vr_run_armed_check", armedCheckCmd);
}

} // namespace qvr::runsetup

// Ironman (vr_run_ironman): saving refused (host_cmd.c's save: every save, quick and auto too), the autosaves not tried
// (host.c), the menus' Save greyed (menu.c, vr_menuui.cpp); a death restarts the level (QC vr_run_ironman.qc).
extern "C" int VR_RunIronman()
{
    return qvr::runsetup::ironman();
}

extern "C" int VR_RunIronmanRefusesSave()
{
    if(!qvr::runsetup::ironman())
    {
        return 0;
    }
    Con_Printf("Ironman: no saving (a death sends you back to the start of the level)\n");
    SCR_CenterPrint("Ironman: no saving");
    Cvar_SetValueQuick(&qvr::vr_ironman_refused, qvr::vr_ironman_refused.value + 1.f);
    return 1;
}

// ED_LoadFromFile (pr_edict.c), a map's entity about to spawn: QC VR_Run_MonsterSpawning (self: it, its keys parsed)
// swaps a monster's classname (Randomized Enemies) and answers how many copies of it to spawn (Enemy Count, 0..2).
// Nothing asked while both are off, in deathmatch, or for anything but a monster_.
extern "C" int VR_Run_MonsterSpawning(edict_t* ent)
{
    if((qvr::vr_run_enemycount.value == 0.f && qvr::vr_run_enemyrandom.value == 0.f) || deathmatch.value != 0.f ||
        strncmp(PR_GetString(ent->v.classname), "monster_", 8) != 0)
    {
        return 0;
    }
    const int n = static_cast<int>(qvr::runsetup::qcOnEdict("VR_Run_MonsterSpawning", ent, nullptr, 0.f));
    return n < 0 ? 0 : n > 2 ? 2 : n;
}

// ... once the whole map has spawned, its copy k (1..), its keys parsed again: QC VR_Run_MonsterCopy (self the copy,
// other its monster) places it beside its monster; 0: no room (not spawned).
extern "C" int VR_Run_MonsterCopy(edict_t* copy, edict_t* orig, int k)
{
    return qvr::runsetup::qcOnEdict("VR_Run_MonsterCopy", copy, orig, static_cast<float>(k)) != 0.f ? 1 : 0;
}
