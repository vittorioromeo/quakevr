// vr_gorepresets.cpp -- the Gore preset (vr_gorepresets.hpp).

#include "vr_gorepresets.hpp"
#include "vr_cvars.hpp"
#include "vr_bloodstream.hpp"
#include "vr_decals.hpp"
#include "vr_engine.hpp"
#include "vr_gore.hpp"
#include "vr_particles.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Math/Clamp.hpp"

#include <stdlib.h>
#include <string.h>

namespace qvr::gorepresets
{
namespace
{

// A row: a setting and its value at Minimal and Off (Full: the setting's default).
struct Row
{
    const char* name;
    const char* minimal;
    const char* off;
};

// What Minimal keeps: the blood of a hit (Hit Blood 0.6x), its small splats on the model (wounds, dents) and the
// bullet and blast marks. What it drops: every blood stain on the world (vr_gore 0: no sprays, pools, runs, drips, gib
// splats), blood streams and spurts, drips, mist, bloody feet and hands, blood on your gear, the small gibs and brain
// chunks (a gibbing is Quake's few chunks: vr_smallgibs 0, vr_gib_limbs 0), beheading, limbs cut off or popped, teeth.
// Off is that with no hit blood either and no blood painted on models (ROUND25.md, "Gore preset").
// clang-format off
constexpr Row rows[] = {
    //                              Minimal  Off
    // Stains and sprays on the world
    {"vr_gore",                     "0",     "0"},
    {"vr_gore_spray",               "0",     "0"},
    {"vr_gore_pools",               "0",     "0"},
    {"vr_gore_drips",               "0",     "0"},
    {"vr_gore_wound_drips",         "0",     "0"},
    {"vr_gore_stick_thrown",        "0",     "0"},
    {"vr_gore_backsplatter",        "0",     "0"},
    {"vr_gore_mist",                "0",     "0"},
    {"vr_gib_blood",                "0",     "0"},
    {"vr_water_blood",              "0",     "0"},
    // Streams and spurts
    {"vr_gore_streams",             "0",     "0"},
    // Feet, hands and gear
    {"vr_footprints",               "0",     "0"},
    {"vr_handprints",               "0",     "0"},
    {"vr_gore_hands",               "0",     "0"},
    {"vr_gore_spatter",             "0",     "0"},
    {"vr_gore_spatter_gibs",        "0",     "0"},
    {"vr_gore_gear",                "0",     "0"},
    {"vr_gore_gear_nearby",         "0",     "0"},
    {"vr_gore_gear_drops",          "0",     "0"},
    {"vr_gore_spread",              "0",     "0"},
    // Your own wounds
    {"vr_player_gore",              "0",     "0"},
    {"vr_body_blood",               "0",     "0"},
    {"vr_body_blood_marks",         "0",     "0"},
    {"vr_body_blood_floor",         "0",     "0"},
    // Chunks, heads and limbs
    {"vr_smallgibs",                "0",     "0"},
    {"vr_smallgibs_player",         "0",     "0"},
    {"vr_smallgibs_brains",         "0",     "0"},
    {"vr_gib_limbs",                "0",     "0"},
    {"vr_decap",                    "0",     "0"},
    {"vr_limbs",                    "0",     "0"},
    {"vr_teeth",                    "0",     "0"},
    // The blood of a hit, and painted on the models
    {"vr_blood_amount",                "0.6",   "0"},
    {"vr_wounds",                   nullptr, "0"},
};
// clang-format on

constexpr int rowCount = static_cast<int>(sizeof(rows) / sizeof(rows[0]));

// A row's value at a choice (1 Full, 2 Minimal, 3 Off); nullptr in the table: the default.
[[nodiscard]] const char* target(const cvar_t& var, const Row& row, int choice)
{
    const char* v = choice == choiceMinimal ? row.minimal : choice == choiceOff ? row.off : nullptr;
    return v ? v : var.default_string ? var.default_string : "0";
}

[[nodiscard]] bool same(const char* a, const char* b)
{
    char* endA = nullptr;
    char* endB = nullptr;
    const double x = strtod(a, &endA);
    const double y = strtod(b, &endB);
    if(endA != a && *endA == '\0' && endB != b && *endB == '\0')
    {
        return static_cast<float>(x) == static_cast<float>(y);
    }
    return strcmp(a, b) == 0;
}

bool busy = false; // the preset's own changes: not a choice made

[[nodiscard]] int differing(int choice, bool print)
{
    int n = 0;
    for(const Row& row : rows)
    {
        const cvar_t* var = Cvar_FindVar(row.name);
        if(!var)
        {
            continue;
        }
        const char* want = target(*var, row, choice);
        if(!same(var->string, want))
        {
            n++;
            if(print)
            {
                Con_Printf("    %s %s (wants %s)\n", row.name, var->string, want);
            }
        }
    }
    return n;
}

void onChoice(cvar_t* var)
{
    if(busy)
    {
        return;
    }
    const int choice = static_cast<int>(var->value);
    if(choice >= choiceFull && choice <= choiceOff)
    {
        apply(choice);
    }
}

void statusCmd()
{
    const int now = matching();
    Con_Printf("gore preset: %s (rows %d)\n", now == choiceFull ? "Full" : now == choiceMinimal ? "Minimal" : now == choiceOff ? "Off" : "Custom", rowCount);
    if(now != choiceCustom)
    {
        return;
    }
    int nearest = choiceFull;
    int best = 1 << 30;
    for(int c = choiceFull; c <= choiceOff; c++)
    {
        if(const int d = differing(c, false); d < best)
        {
            best = d;
            nearest = c;
        }
    }
    Con_Printf("  nearest %s, %d differ:\n", nearest == choiceFull ? "Full" : nearest == choiceMinimal ? "Minimal" : "Off", best);
    (void)differing(nearest, true);
}

// vr_gore_preset_test: the preset sets its rows and shows Custom after a change by hand; Minimal's hit still bleeds a little
// and leaves no stain; Off bleeds not at all. Needs a map (a world to mark).
void testCmd()
{
    testkit::Tally t;
    constexpr const char* name = "gorepreset";
    if(!cl.worldmodel || cl.viewentity <= 0 || cl.viewentity >= cl.num_entities)
    {
        Con_Printf("gorepreset: needs a map\n");
        testkit::verdict(name, 0, 1);
        return;
    }
    const entity_t& pl = cl_entities[cl.viewentity];
    const glm::vec3 dir{1.f, 0.f, 0.f};
    const glm::vec3 org{pl.origin[0] + 48.f, pl.origin[1], pl.origin[2]};

    // Minimal: every row at its value, and the choice shown.
    Cvar_SetValue("vr_gore_preset", static_cast<float>(choiceMinimal));
    t.check(differing(choiceMinimal, false) == 0, name, "Minimal sets every row");
    t.check(matching() == choiceMinimal, name, "Minimal matches");
    // Changed by hand: Custom.
    Cvar_SetValue("vr_gore_spray", 1.f);
    t.check(matching() == choiceCustom, name, "a change by hand shows Custom");
    sync();
    t.check(vr_gore_preset.value == 0.f, name, "the choice shown reads Custom");
    Cvar_SetValue("vr_gore_preset", static_cast<float>(choiceMinimal));

    // A shot: Minimal bleeds a little, and leaves no stain or stream.
    decals::clear();
    const int stainsBefore = decals::liveCount();
    const float tallyBefore = vr_blood_tally.value;
    (void)particles::spawn(org, dir, particles::Preset::Blood, 20);
    const float minimalBlood = vr_blood_tally.value - tallyBefore;
    (void)gore::event(org, dir, gore::EventHit, 40);
    (void)gore::event(org, dir, gore::EventBurst, 20);
    (void)gore::event(org, dir, gore::EventCorpse, 0);
    const int afterEvents = decals::liveCount(); // (the gore events: sprays, pools, runs; none at vr_gore 0)
    decals::fromEffect(org, dir, particles::Preset::Blood, 20); // (the hit's own small splats: kept)
    Con_Printf("gorepreset: Minimal: %.0f blood particles, stains %d -> %d -> %d, vr_gore_streams %.0f\n", minimalBlood, stainsBefore,
        afterEvents, decals::liveCount(), vr_gore_streams.value);
    t.check(minimalBlood > 0.f, name, "Minimal: a hit still bleeds");
    t.check(afterEvents == stainsBefore && decals::liveCount() - stainsBefore <= 2, name, "Minimal: no blood stain, only the hit's small splats");
    t.check(vr_gore_streams.value == 0.f && bloodstream::ownStreams() == 0, name, "Minimal: no blood stream");
    t.check(vr_smallgibs.value == 0.f && vr_smallgibs_brains.value == 0.f && vr_decap.value == 0.f && vr_limbs.value == 0.f, name,
        "Minimal: no small gibs, brain chunks, beheading or limbs");

    // Full: back to the defaults, and a hit bleeds in full.
    Cvar_SetValue("vr_gore_preset", static_cast<float>(choiceFull));
    t.check(matching() == choiceFull, name, "Full matches the defaults");
    const float fullBefore = vr_blood_tally.value;
    (void)particles::spawn(org, dir, particles::Preset::Blood, 20);
    const float fullBlood = vr_blood_tally.value - fullBefore;
    t.check(fullBlood > minimalBlood, name, "Full bleeds more than Minimal");

    // Off: no blood at all.
    Cvar_SetValue("vr_gore_preset", static_cast<float>(choiceOff));
    t.check(matching() == choiceOff, name, "Off matches");
    const float offBefore = vr_blood_tally.value;
    (void)particles::spawn(org, dir, particles::Preset::Blood, 20);
    t.check(vr_blood_tally.value == offBefore, name, "Off: no blood at all");
    Con_Printf("gorepreset: blood particles: Full %.0f, Minimal %.0f, Off %.0f\n", fullBlood, minimalBlood, vr_blood_tally.value - offBefore);

    Cvar_SetValue("vr_gore_preset", static_cast<float>(choiceFull)); // (as shipped again)
    t.verdict(name);
}

} // namespace

void apply(int choice)
{
    choice = za::clamp(choice, choiceFull, choiceOff);
    busy = true;
    for(const Row& row : rows)
    {
        if(cvar_t* var = Cvar_FindVar(row.name))
        {
            Cvar_SetQuick(var, target(*var, row, choice));
        }
    }
    Cvar_SetValueQuick(&vr_gore_preset, static_cast<float>(choice));
    busy = false;
}

int matching()
{
    for(int c = choiceFull; c <= choiceOff; c++)
    {
        if(differing(c, false) == 0)
        {
            return c;
        }
    }
    return choiceCustom;
}

void sync()
{
    const int shown = matching();
    if(static_cast<int>(vr_gore_preset.value) != shown)
    {
        busy = true;
        Cvar_SetValueQuick(&vr_gore_preset, static_cast<float>(shown));
        busy = false;
    }
}

void init()
{
    Cvar_SetCallback(&vr_gore_preset, onChoice);
    Cmd_AddCommand("vr_gore_preset_status", statusCmd);
    Cmd_AddCommand("vr_gore_preset_test", testCmd);
}

} // namespace qvr::gorepresets
