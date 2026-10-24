// vr_aggrip.cpp -- armed monsters' grip offsets (vr_aggrip.hpp).

#include "vr_aggrip.hpp"
#include "vr_cvars.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace qvr;

namespace
{

// The shipped grip offsets (vr_agrunt_grip_dump prints rows to paste here): kind (-1 both), the weapon's Weapon Offsets
// ID, the hand, x y z (units), pitch yaw roll (degrees). A weapon with no row: none (its origin in the hand's middle,
// held as the rest pose's gun).
struct Row
{
    int kind;
    const char* id;
    int hand;
    float x, y, z, pitch, yaw, roll;
};
constexpr Row defaults[] = {
    // (Seeded by vr_agrunt_grip_fit on a grunt's idle: its wrist unturned as the server readies each melee weapon; the
    // guns are held as the grunt held its own.)
    {-1, "progs/v_axe.mdl", aggrip::Main, 0.f, 0.f, 0.f, -34.f, 121.f, -27.5f},
    {-1, "progs/v_ksword.mdl", aggrip::Main, -1.6f, 4.6f, 0.95f, -11.f, 109.5f, -23.f}, // (its guard above the fist)
    {-1, "progs/v_crowbar.mdl", aggrip::Main, 0.f, 0.f, 0.f, -7.f, 108.f, -22.5f},
    {-1, "progs/v_chainsaw.mdl", aggrip::Main, 0.f, 0.f, 0.f, -32.f, -94.f, -172.5f},
    {-1, "progs/v_hammer.mdl", aggrip::Main, 0.f, 0.f, 0.f, -28.5f, 118.f, -25.5f},
};

// vr_agarms: each kind's hands' grip frames for every weapon (the weapon's entity frame is the hand's, as your
// controller's is: its Weapon Offsets draw it off that), on top of each hand's rest grip frame (its middle, turned as
// the rest pose's gun). The Grip Editor's "All Weapons" entries; a weapon's own (above) go on top of these.
struct GlobalRow
{
    int kind;
    int hand;
    float x, y, z, pitch, yaw, roll;
};
constexpr GlobalRow globalDefaults[] = {
    {0, aggrip::Main, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0, aggrip::Off, 0.f, 0.f, 0.f, 15.f, -35.f, -90.f}, // (the left hand's rest hold is under a barrel: a pistol grip turned into it; ag_offtry.sh)
    {0, aggrip::Second, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {1, aggrip::Main, 0.f, 0.f, 0.f, 30.f, -10.f, 0.f}, // (the enforcer's wrists straighter so)
    {1, aggrip::Off, 0.f, 0.f, 0.f, 15.f, -35.f, -90.f},
    {1, aggrip::Second, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
};

struct Override
{
    int kind{0};
    int slot{-1};
    int hand{0};
    aggrip::Offset o;
};

constexpr int maxOverrides = 128;
constexpr int textSize = 8192;

struct Key
{
    int kind{-1};
    int slot{-1};
    int hand{-1};
    [[nodiscard]] bool operator==(const Key& k) const { return kind == k.kind && slot == k.slot && hand == k.hand; }
};

struct State
{
    Override over[maxOverrides];
    int numOver{0};
    char parsed[textSize]{}; // the vr_agrunt_grips text parsed into `over`
    bool everParsed{false};
    // the editor
    Key loaded;
    float sliders[6]{};
    int subjectFrame{-1};
    float subjectDist{1e30f};
    int subjectKind{-1}, subjectSlot{-1}, subjectSlotL{-1};
    Key subject; // the last frame's
    char line[256]{}; // subjectLine's
};
State state;

cvar_t* const sliderCvars[6] = {&vr_agrunt_grip_x, &vr_agrunt_grip_y, &vr_agrunt_grip_z, &vr_agrunt_grip_pitch,
    &vr_agrunt_grip_yaw, &vr_agrunt_grip_roll};

[[nodiscard]] const char* slotId(int slot)
{
    if(slot == aggrip::allWeapons)
    {
        return "*";
    }
    const cvar_t* c = slot >= 0 && slot < weapons::numSlots ? weapons::cvar(slot, weapons::Key::ID) : nullptr;
    return c && c->string ? c->string : "-1";
}

void parse()
{
    if(state.everParsed && strcmp(state.parsed, vr_agrunt_grips.string) == 0)
    {
        return;
    }
    state.everParsed = true;
    q_strlcpy(state.parsed, vr_agrunt_grips.string, sizeof(state.parsed));
    state.numOver = 0;
    const char* p = state.parsed;
    while(*p && state.numOver < maxOverrides)
    {
        char id[256];
        Override o;
        int used = 0;
        if(sscanf(p, " %d %255s %d %f %f %f %f %f %f%n", &o.kind, id, &o.hand, &o.o.pos.x, &o.o.pos.y, &o.o.pos.z,
               &o.o.angles.x, &o.o.angles.y, &o.o.angles.z, &used) == 9)
        {
            o.slot = strcmp(id, "*") == 0 ? aggrip::allWeapons : weapons::slotForName(id);
            if((o.slot >= 0 || o.slot == aggrip::allWeapons) && o.kind >= 0 && o.kind < aggrip::numKinds && o.hand >= 0 && o.hand < aggrip::NumHands)
            {
                state.over[state.numOver++] = o;
            }
            p += used;
        }
        const char* semi = strchr(p, ';');
        if(!semi)
        {
            break;
        }
        p = semi + 1;
    }
}

void write()
{
    char text[textSize];
    text[0] = 0;
    for(int i = 0; i < state.numOver; i++)
    {
        const Override& o = state.over[i];
        char row[512];
        q_snprintf(row, sizeof(row), "%d %s %d %g %g %g %g %g %g;", o.kind, slotId(o.slot), o.hand, o.o.pos.x, o.o.pos.y,
            o.o.pos.z, o.o.angles.x, o.o.angles.y, o.o.angles.z);
        q_strlcat(text, row, sizeof(text));
    }
    Cvar_SetQuick(&vr_agrunt_grips, text);
    parse();
}

[[nodiscard]] bool defaultOf(int kind, int slot, int hand, aggrip::Offset& out)
{
    if(slot == aggrip::allWeapons)
    {
        for(const GlobalRow& r : globalDefaults)
        {
            if(r.kind == kind && r.hand == hand)
            {
                out.pos = {r.x, r.y, r.z};
                out.angles = {r.pitch, r.yaw, r.roll};
                return true;
            }
        }
        return false;
    }
    if(vr_agarms.value != 0.f)
    {
        return false; // (the seeds above fitted the old poses' weapon frames: vr_agarms' hands hold them as you do)
    }
    const Row* found = nullptr;
    for(const Row& r : defaults)
    {
        if(r.hand == hand && (r.kind == kind || (r.kind < 0 && !found)) && weapons::slotForName(r.id) == slot)
        {
            found = &r;
        }
    }
    if(!found)
    {
        return false;
    }
    out.pos = {found->x, found->y, found->z};
    out.angles = {found->pitch, found->yaw, found->roll};
    return true;
}

void dump_f()
{
    parse();
    const bool all = Cmd_Argc() > 1 && strcmp(Cmd_Argv(1), "all") == 0;
    Con_Printf("// vr_agrunt_grip_dump%s: rows for vr_aggrip.cpp's defaults (kind -1 both kinds)\n", all ? " all" : "");
    for(int i = 0; i < state.numOver; i++)
    {
        const Override& o = state.over[i];
        if(o.slot == aggrip::allWeapons)
        {
            Con_Printf("    {%d, aggrip::%s, %.2ff, %.2ff, %.2ff, %.1ff, %.1ff, %.1ff}, // (globalDefaults: every weapon)\n", o.kind,
                o.hand == aggrip::Main ? "Main" : o.hand == aggrip::Off ? "Off" : "Second", o.o.pos.x, o.o.pos.y, o.o.pos.z,
                o.o.angles.x, o.o.angles.y, o.o.angles.z);
            continue;
        }
        Con_Printf("    {%d, \"%s\", aggrip::%s, %.2ff, %.2ff, %.2ff, %.1ff, %.1ff, %.1ff},\n", o.kind, slotId(o.slot),
            o.hand == aggrip::Main ? "Main" : o.hand == aggrip::Off ? "Off" : "Second", o.o.pos.x, o.o.pos.y, o.o.pos.z,
            o.o.angles.x, o.o.angles.y, o.o.angles.z);
    }
    if(all)
    {
        for(const Row& r : defaults)
        {
            Con_Printf("    {%d, \"%s\", aggrip::%s, %.2ff, %.2ff, %.2ff, %.1ff, %.1ff, %.1ff}, // (default)\n", r.kind, r.id,
                r.hand == aggrip::Main ? "Main" : r.hand == aggrip::Off ? "Off" : "Second", r.x, r.y, r.z, r.pitch, r.yaw,
                r.roll);
        }
    }
    if(state.numOver == 0 && !all)
    {
        Con_Printf("// (no changes from the defaults; vr_agrunt_grip_dump all prints those)\n");
    }
}

// vr_agrunt_grip_reset: the edited entry back to its default (all: every change dropped).
void reset_f()
{
    parse();
    const bool all = Cmd_Argc() > 1 && strcmp(Cmd_Argv(1), "all") == 0;
    int kept = 0;
    for(int i = 0; i < state.numOver; i++)
    {
        const Override& o = state.over[i];
        if(!all && !(Key{o.kind, o.slot, o.hand} == state.loaded))
        {
            state.over[kept++] = o;
        }
    }
    state.numOver = kept;
    write();
    state.loaded = Key{}; // (reloaded next frame)
}

} // namespace

namespace qvr::aggrip
{

Offset global(int kind, int hand)
{
    return offset(kind, allWeapons, hand);
}

Offset offset(int kind, int slot, int hand)
{
    parse();
    for(int i = 0; i < state.numOver; i++)
    {
        const Override& o = state.over[i];
        if(o.kind == kind && o.slot == slot && o.hand == hand)
        {
            return o.o;
        }
    }
    Offset d;
    (void)defaultOf(kind, slot, hand, d);
    return d;
}

void noteSubject(int kind, int slot, int slotL, float dist)
{
    if(state.subjectFrame != host_framecount)
    {
        state.subjectFrame = host_framecount;
        state.subjectDist = 1e30f;
    }
    if(dist < state.subjectDist)
    {
        state.subjectDist = dist;
        state.subjectKind = kind;
        state.subjectSlot = slot;
        state.subjectSlotL = slotL;
    }
}

void frame()
{
    parse();
    const int hand = za::clamp(static_cast<int>(vr_agrunt_grip_hand.value), 0, static_cast<int>(NumHands) - 1);
    if(state.subjectFrame == host_framecount && state.subjectKind >= 0)
    {
        const int slot = hand == Off ? state.subjectSlotL : state.subjectSlot;
        state.subject = Key{state.subjectKind, vr_agrunt_grip_scope.value == 0.f ? allWeapons : slot, hand};
    }
    const Key key = state.subject;
    if(key.kind < 0 || (key.slot < 0 && key.slot != allWeapons))
    {
        return;
    }
    if(!(key == state.loaded))
    {
        const Offset o = offset(key.kind, key.slot, key.hand);
        const float v[6] = {o.pos.x, o.pos.y, o.pos.z, o.angles.x, o.angles.y, o.angles.z};
        for(int i = 0; i < 6; i++)
        {
            state.sliders[i] = v[i];
            Cvar_SetValueQuick(sliderCvars[i], v[i]);
        }
        state.loaded = key;
        return;
    }
    bool changed = false;
    for(int i = 0; i < 6; i++)
    {
        changed = changed || sliderCvars[i]->value != state.sliders[i];
    }
    if(!changed)
    {
        return;
    }
    for(int i = 0; i < 6; i++)
    {
        state.sliders[i] = sliderCvars[i]->value;
    }
    int at = -1;
    for(int i = 0; i < state.numOver; i++)
    {
        if(Key{state.over[i].kind, state.over[i].slot, state.over[i].hand} == key)
        {
            at = i;
        }
    }
    if(at < 0 && state.numOver < maxOverrides)
    {
        at = state.numOver++;
    }
    if(at < 0)
    {
        return;
    }
    Override& o = state.over[at];
    o.kind = key.kind;
    o.slot = key.slot;
    o.hand = key.hand;
    o.o.pos = {state.sliders[0], state.sliders[1], state.sliders[2]};
    o.o.angles = {state.sliders[3], state.sliders[4], state.sliders[5]};
    write();
}

const char* subjectLine()
{
    const Key k = state.subject;
    if(k.kind < 0 || (k.slot < 0 && k.slot != allWeapons))
    {
        q_snprintf(state.line, sizeof(state.line), "No armed grunt near (spawn one)");
        return state.line;
    }
    q_snprintf(state.line, sizeof(state.line), "%s: %s, %s", k.kind == 0 ? "Grunts" : "Enforcers",
        k.slot == allWeapons ? "every weapon" : slotId(k.slot),
        k.hand == Main ? "main hand" : k.hand == Off ? "off hand (dual)" : "off hand on its second grip");
    return state.line;
}

void registerCommands()
{
    Cmd_AddCommand("vr_agrunt_grip_dump", dump_f);
    Cmd_AddCommand("vr_agrunt_grip_reset", reset_f);
}

} // namespace qvr::aggrip
