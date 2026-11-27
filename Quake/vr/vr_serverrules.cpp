// vr_serverrules.cpp -- see vr_serverrules.hpp.

#include "vr_serverrules.hpp"
#include "vr_clientprefs.hpp"
#include "vr_cvars.hpp"
#include "vr_cvars_util.hpp"
#include "vr_config.hpp"
#include "vr_engine.hpp"
#include "vr_gameplaypresets.hpp"
#include "vr_menu.hpp"
#include "vr_presets.hpp"
#include "vr_protocol.hpp"
#include "vr_testkit.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Base/Strstr.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <float.h>
#include <stdlib.h>

namespace qvr::serverrules
{
namespace
{

// The balance pages: a setting under them (Search's paths: menu::forEachSettingPath) and a Gameplay Presets row are
// each a rule, a per-player preference or the player's own, as its vr_cvars.inc row says (QVR_RULE, QVR_PREF, QVR_OWN:
// vr_cvars.hpp). One with none is warned about when the rules are gathered (checkKinds). (Not Locomotion: its comfort,
// turning and controls are each player's own; its few balance ones, vr_crouch_speed, vr_crouch_speed_scale and
// vr_slope_walk, are rules by their rows, as the melee timing settings are.)
constexpr const char* rulePages[] = {
    "Advanced VR Options > Combat",
    "Advanced VR Options > Carrying and Throwing",
    "Advanced VR Options > Arcane Dimensions",
    "Advanced VR Options > World",
    "Advanced VR Options > Movement > Climbing",
    "Advanced VR Options > Movement > Grappling Hook",
    "Advanced VR Options > Movement > Swimming",
    "Advanced VR Options > Movement > Player Hitbox",
    "Advanced VR Options > Movement > Monster Hitbox",
    "VR Settings > Gameplay",
};

// What the names said before the kinds were written down (the rule list was the pages less these): a player's own, on
// those pages, has one of them in its name -- what only his senses get (sounds, haptics, colours, what is shown or
// printed) and how his own hands and weapons are posed and held. Only checkKinds' guess for a setting without a kind.
constexpr const char* ownParts[] = {"sound", "haptic", "_show", "debug", "_viz", "vignette", "_hud", "_hue", "colour",
    "color", "_saturation", "_lightness", "_alpha", "_glow", "_text", "_print", "_draw", "vr_wofs_", "vr_climb_hand_",
    "vr_grip_fit", "_stick_", "anygrip"};

struct State
{
    bool built{false};
    int unset{0};             // settings on a rule page without a kind (checkKinds)
    za::Vector<cvar_t*> list; // the rules, by address (isRule's search)
    // Server: the values the clients were last sent; serverFrame's changed ones (kept: no allocation a frame).
    za::Vector<float> sent;
    za::Vector<int> changed;
    // Client: playing by a remote server's rules (shadowed), each rule's own value kept meanwhile; the config's write
    // swaps them in (swapped: the cvars' strings while it writes).
    bool shadowed{false};
    za::Vector<za::String> own;
    za::Vector<const char*> live;
    bool swapped{false};
    // Client: what a remote server's value of each rule may be, lo..hi (gatherRanges; review A7): its menu rows' range
    // with its default and the gameplay presets' values; no row: any finite number.
    za::Vector<float> lo;
    za::Vector<float> hi;
};
State state;

void gatherRanges();

[[nodiscard]] bool pageIsRules(const char* path)
{
    for(const char* p : rulePages)
    {
        const za::SizeT n = ZA_STRLEN(p);
        if(!ZA_STRNCMP(path, p, n) && (path[n] == '\0' || path[n] == ' '))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool ownByName(const char* name)
{
    for(const char* part : ownParts)
    {
        if(ZA_STRSTR(name, part))
        {
            return true;
        }
    }
    return false;
}

// A setting on a rule page or a Gameplay Presets row (archived: the others are no one's choice to keep) whose vr_cvars.inc
// row has no kind: warned about, with what its name would have made it. It plays as the player's own (not sent) until its
// row says. Each named once.
void checkOne(const cvar_t* var, const char* where, za::Vector<const cvar_t*>& seen)
{
    if(!var || !(var->flags & CVAR_ARCHIVE) || cvars::kindOf(var) != cvars::Kind::Unset)
    {
        return;
    }
    for(const cvar_t* v : seen)
    {
        if(v == var)
        {
            return;
        }
    }
    seen.pushBack(var);
    state.unset++;
    const char* guess = clientprefs::indexOf(var) >= 0 ? "QVR_PREF" : ownByName(var->name) ? "QVR_OWN" : "QVR_RULE";
    Con_Warning("vr_serverrules: %s (%s) has no kind in vr_cvars.inc (its name says %s): played as your own\n", var->name,
        where, guess);
}

void checkKinds()
{
    za::Vector<const cvar_t*> seen;
    const presetcore::Table& t = gameplaypresets::kind().table();
    for(int i = 0; i < t.rowCount; i++)
    {
        checkOne(t.rows[i].var, "a Gameplay Presets row", seen);
    }
    menu::forEachSettingPath([&seen](const cvar_t& var, const char* page, const char*) {
        if(pageIsRules(page))
        {
            checkOne(Cvar_FindVar(var.name), page, seen);
        }
    });
}

// The rules (the settings whose rows say QVR_RULE), gathered once and sorted by address; the rule pages checked then
// (the menus' pages built for it: Search's index).
void build()
{
    if(state.built)
    {
        return;
    }
    state.built = true;
    cvars::forEachWithKind([](cvar_t* var, cvars::Kind kind) {
        if(kind == cvars::Kind::Rule)
        {
            state.list.pushBack(var);
        }
    });
    for(const za::Span<cvar_t> table : standInTables()) // (the data tables' values: their tables' kinds)
    {
        for(cvar_t& var : table)
        {
            if(cvars::kindOf(&var) == cvars::Kind::Rule)
            {
                state.list.pushBack(&var);
            }
        }
    }
    checkKinds();
    za::quickSort(state.list.begin(), state.list.end());
    state.sent.resize(state.list.size());
    state.own.resize(state.list.size());
    state.live.resize(state.list.size());
    gatherRanges();
}

[[nodiscard]] int indexOf(const cvar_t* var)
{
    if(!state.built || !var)
    {
        return -1;
    }
    za::SizeT lo = 0, hi = state.list.size();
    while(lo < hi)
    {
        const za::SizeT mid = (lo + hi) / 2;
        if(state.list[mid] == var)
        {
            return static_cast<int>(mid);
        }
        if(state.list[mid] < var)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    return -1;
}

[[nodiscard]] float defaultOf(const cvar_t* var)
{
    return var->default_string ? static_cast<float>(atof(var->default_string)) : 0.f;
}

// QVR_SVC_RULES: [byte flags (1: all -- every rule not in these messages is at its default)][byte count]
// ([string cvar][string value]) x count, a message at most chunkBytes long (the first of several carries the flag).
constexpr int flagAll = 1;
constexpr int chunkBytes = 1000;
constexpr int chunkMost = 255;

void writeRules(sizebuf_t* msg, const int* which, int count, bool all)
{
    int k = 0;
    bool first = true;
    while(k < count || first)
    {
        // As many as fit in a chunk.
        int n = 0;
        int bytes = 4;
        while(k + n < count && n < chunkMost)
        {
            const cvar_t* var = state.list[which[k + n]];
            const int size = static_cast<int>(ZA_STRLEN(var->name) + ZA_STRLEN(var->string)) + 2;
            if(n > 0 && bytes + size > chunkBytes)
            {
                break;
            }
            bytes += size;
            n++;
        }
        MSG_WriteByte(msg, protocol::svc_quakevr);
        MSG_WriteByte(msg, protocol::QVR_SVC_RULES);
        MSG_WriteByte(msg, all && first ? flagAll : 0);
        MSG_WriteByte(msg, n);
        for(int j = 0; j < n; j++)
        {
            const cvar_t* var = state.list[which[k + j]];
            MSG_WriteString(msg, var->name);
            MSG_WriteString(msg, var->string);
        }
        k += n;
        first = false;
    }
}

[[nodiscard]] cvar_t* ruleNamed(const char* name)
{
    cvar_t* var = Cvar_FindVar(name);
    return indexOf(var) >= 0 ? var : nullptr;
}

// Whether `s` is all a number (strtod's, spaces around it allowed).
[[nodiscard]] bool isNumber(const char* s, double* out = nullptr)
{
    if(!s)
    {
        return false;
    }
    char* end = nullptr;
    const double d = strtod(s, &end);
    if(end == s)
    {
        return false;
    }
    while(*end == ' ' || *end == '\t')
    {
        end++;
    }
    if(out)
    {
        *out = d;
    }
    return *end == '\0';
}

void widen(za::SizeT i, float v)
{
    state.lo[i] = q_min(state.lo[i], v);
    state.hi[i] = q_max(state.hi[i], v);
}

// Each rule's lo..hi (State::lo): its menu rows' ranges, then (one on a row) its default and the gameplay presets' values
// too, so that nothing a host can pick from the menus or a preset is ever cut.
void gatherRanges()
{
    const za::SizeT n = state.list.size();
    state.lo.resize(n);
    state.hi.resize(n);
    for(za::SizeT i = 0; i < n; i++)
    {
        state.lo[i] = FLT_MAX;
        state.hi[i] = -FLT_MAX;
    }
    menu::forEachSettingRange([](const cvar_t& var, float lo, float hi) {
        const int i = indexOf(&var);
        if(i >= 0)
        {
            widen(static_cast<za::SizeT>(i), lo);
            widen(static_cast<za::SizeT>(i), hi);
        }
    });
    const presetcore::Table& t = gameplaypresets::kind().table();
    for(int r = 0; r < t.rowCount; r++)
    {
        const int i = indexOf(t.rows[r].var);
        double d = 0.0;
        for(int l = 0; i >= 0 && l < t.levelCount; l++)
        {
            if(state.lo[i] <= state.hi[i] && isNumber(t.rows[r].values[l], &d))
            {
                widen(static_cast<za::SizeT>(i), static_cast<float>(d));
            }
        }
    }
    for(za::SizeT i = 0; i < n; i++)
    {
        double d = 0.0;
        if(state.lo[i] > state.hi[i]) // (no menu row)
        {
            state.lo[i] = -FLT_MAX;
            state.hi[i] = FLT_MAX;
        }
        else if(isNumber(state.list[i]->default_string, &d))
        {
            widen(i, static_cast<float>(d));
        }
    }
}

// A remote server's value of rule `i` as this client takes it (review A7): nullptr refuses it (not finite, or not a
// number where the rule's default is one), else the value, clamped to lo..hi (written into `buf`).
[[nodiscard]] const char* checkedValue(za::SizeT i, const char* value, char* buf, za::SizeT size)
{
    double d = 0.0;
    if(!isNumber(value, &d))
    {
        return isNumber(state.list[i]->default_string) ? nullptr : value; // (a text rule takes text)
    }
    if(!(d >= -FLT_MAX && d <= FLT_MAX)) // (NaN too)
    {
        return nullptr;
    }
    const float v = static_cast<float>(d);
    const float c = q_min(q_max(v, state.lo[i]), state.hi[i]);
    if(c == v)
    {
        return value;
    }
    q_snprintf(buf, size, "%.9g", static_cast<double>(c));
    return buf;
}

// vr_serverrules_selftest: a remote server's rule values checked (review A7): not finite or not a number refused, past
// a rule's menu range clamped to it, in it kept as sent; every rule's default and gameplay preset value in its range.
void selftest_f()
{
    build();
    testkit::Tally tally;
    za::SizeT pick = state.list.size();
    int ranged = 0;
    for(za::SizeT i = 0; i < state.list.size(); i++)
    {
        if(state.hi[i] < FLT_MAX && isNumber(state.list[i]->default_string))
        {
            ranged++;
            pick = pick < state.list.size() ? pick : i;
        }
    }
    tally.check(ranged > 0 && pick < state.list.size(), "rules with a menu range found", va("%d of %d", ranged,
        static_cast<int>(state.list.size())));
    if(pick < state.list.size())
    {
        char buf[64];
        const cvar_t* var = state.list[pick];
        const float lo = state.lo[pick], hi = state.hi[pick];
        Con_Printf("  %s: %g..%g\n", var->name, static_cast<double>(lo), static_cast<double>(hi));
        tally.check(!checkedValue(pick, "nan", buf, sizeof(buf)), "\"nan\" refused");
        tally.check(!checkedValue(pick, "inf", buf, sizeof(buf)), "\"inf\" refused");
        tally.check(!checkedValue(pick, "1e40", buf, sizeof(buf)), "\"1e40\" (past a float) refused");
        tally.check(!checkedValue(pick, "text", buf, sizeof(buf)), "\"text\" refused (a number's rule)");
        tally.check(!checkedValue(pick, "", buf, sizeof(buf)), "\"\" refused");
        const char* up = checkedValue(pick, "1e30", buf, sizeof(buf));
        tally.check(up && static_cast<float>(atof(up)) == hi, "\"1e30\" clamped to the range's top", up ? up : "refused");
        const char* down = checkedValue(pick, "-1e30", buf, sizeof(buf));
        tally.check(down && static_cast<float>(atof(down)) == lo, "\"-1e30\" clamped to the range's bottom",
            down ? down : "refused");
        const char* same = var->default_string;
        tally.check(checkedValue(pick, same, buf, sizeof(buf)) == same, "the default kept as sent", same);
    }
    int outside = 0;
    for(za::SizeT i = 0; i < state.list.size(); i++)
    {
        double d = 0.0;
        if(isNumber(state.list[i]->default_string, &d) &&
            (static_cast<float>(d) < state.lo[i] || static_cast<float>(d) > state.hi[i]))
        {
            Con_Printf("  outside: %s default %s\n", state.list[i]->name, state.list[i]->default_string);
            outside++;
        }
    }
    const presetcore::Table& t = gameplaypresets::kind().table();
    for(int r = 0; r < t.rowCount; r++)
    {
        const int i = indexOf(t.rows[r].var);
        double d = 0.0;
        for(int l = 0; i >= 0 && l < t.levelCount; l++)
        {
            if(isNumber(t.rows[r].values[l], &d) && (static_cast<float>(d) < state.lo[i] || static_cast<float>(d) > state.hi[i]))
            {
                Con_Printf("  outside: %s preset %s\n", t.rows[r].var->name, t.rows[r].values[l]);
                outside++;
            }
        }
    }
    tally.check(outside == 0, "every rule's default and gameplay preset value in its range", va("%d outside", outside));
    tally.verdict("vr_serverrules_selftest");
}

// Client: the server's rules govern from now on: each rule's own value kept.
void shadow()
{
    if(state.shadowed)
    {
        return;
    }
    for(za::SizeT i = 0; i < state.list.size(); i++)
    {
        state.own[i] = state.list[i]->string;
    }
    state.shadowed = true;
}

// vr_serverrules [cvar...]: how many rules and whose govern; each cvar named: whether a rule, its value here, the
// player's own (while a remote server's govern) and whether locked ("mp_rule:" lines, the harness's mp_rules). With
// none named, the rules off their defaults.
void list_f()
{
    build();
    const bool isRemote = remote();
    Con_Printf("mp_rules: count=%d unset=%d remote=%d shadowed=%d (%s)\n", static_cast<int>(state.list.size()), state.unset,
        isRemote ? 1 : 0, state.shadowed ? 1 : 0, state.shadowed ? "the remote server's govern" : sv.active ? "this server's govern" : "your own");
    if(Cmd_Argc() > 1)
    {
        for(int a = 1; a < Cmd_Argc(); a++)
        {
            const cvar_t* var = Cvar_FindVar(Cmd_Argv(a));
            const int i = indexOf(var);
            if(!var)
            {
                Con_Printf("mp_rule: name=%s none\n", Cmd_Argv(a));
                continue;
            }
            Con_Printf("mp_rule: name=%s rule=%d kind=%s value=%s own=%s locked=%d\n", var->name, i >= 0 ? 1 : 0,
                cvars::kindName(cvars::kindOf(var)), var->string, i >= 0 && state.shadowed ? state.own[i].cStr() : var->string,
                locked(var) ? 1 : 0);
        }
        return;
    }
    for(za::SizeT i = 0; i < state.list.size(); i++)
    {
        const cvar_t* var = state.list[i];
        const bool mine = state.shadowed && ZA_STRCMP(state.own[i].cStr(), var->string);
        if(var->value != defaultOf(var) || mine)
        {
            Con_Printf("  %-32s %s%s%s%s\n", var->name, var->string, mine ? "  (yours " : "", mine ? state.own[i].cStr() : "",
                mine ? ")" : "");
        }
    }
}

} // namespace

bool isRule(const cvar_t* var)
{
    return indexOf(var) >= 0;
}

bool remote()
{
    return cls.state == ca_connected && !sv.active && !cls.demoplayback;
}

bool locked(const cvar_t* var)
{
    return state.shadowed && remote() && isRule(var);
}

float own(const cvar_t* var)
{
    const int i = indexOf(var);
    return i >= 0 && state.shadowed ? static_cast<float>(atof(state.own[i].cStr())) : var->value;
}

const char* lockedBecause()
{
    return state.shadowed && remote() ? "The server's rules govern while you play on it (a preset sets them): its host picks "
                                        "them. Yours come back when you leave."
                                      : nullptr;
}

void serverWriteAll(sizebuf_t* msg)
{
    if(svs.maxclients <= 1)
    {
        return; // (single player: its own rules)
    }
    build();
    za::Vector<int> which;
    for(za::SizeT i = 0; i < state.list.size(); i++)
    {
        if(state.list[i]->value != defaultOf(state.list[i]))
        {
            which.pushBack(static_cast<int>(i));
        }
    }
    writeRules(msg, which.data(), static_cast<int>(which.size()), true);
}

void serverFrame()
{
    if(!state.built || svs.maxclients <= 1)
    {
        return; // (serverReset builds them with the map)
    }
    za::Vector<int>& which = state.changed;
    which.clear();
    for(za::SizeT i = 0; i < state.list.size(); i++)
    {
        if(state.list[i]->value != state.sent[i])
        {
            state.sent[i] = state.list[i]->value;
            which.pushBack(static_cast<int>(i));
        }
    }
    if(which.size() > 0)
    {
        writeRules(&sv.reliable_datagram, which.data(), static_cast<int>(which.size()), false);
    }
}

void serverReset()
{
    if(svs.maxclients <= 1)
    {
        return; // (single player: nothing to send, nothing gathered)
    }
    build();
    for(za::SizeT i = 0; i < state.list.size(); i++)
    {
        state.sent[i] = state.list[i]->value;
    }
}

void clientParse()
{
    const int flags = MSG_ReadByte();
    const int count = MSG_ReadByte();
    const bool apply = remote(); // (a demo's, a listen host's own: read past)
    if(apply)
    {
        build();
        shadow();
        if(flags & flagAll)
        {
            for(cvar_t* var : state.list)
            {
                if(var->default_string && ZA_STRCMP(var->string, var->default_string))
                {
                    Cvar_SetQuick(var, var->default_string);
                }
            }
        }
    }
    for(int k = 0; k < count; k++)
    {
        char name[64];
        q_strlcpy(name, MSG_ReadString(), sizeof(name));
        const char* value = MSG_ReadString();
        if(cvar_t* var = apply ? ruleNamed(name) : nullptr) // (a rule this build does not know: skipped)
        {
            char buf[64];
            const char* checked = checkedValue(static_cast<za::SizeT>(indexOf(var)), value, buf, sizeof(buf));
            if(checked != value)
            {
                Con_Warning("vr_serverrules: the server's %s \"%.32s\" %s%s\n", var->name, value, checked ? "clamped to " : "refused",
                    checked ? checked : "");
            }
            if(checked && ZA_STRCMP(var->string, checked))
            {
                Cvar_SetQuick(var, checked);
            }
        }
    }
}

void clientLeft()
{
    if(!state.shadowed)
    {
        return;
    }
    state.shadowed = false;
    for(za::SizeT i = 0; i < state.list.size(); i++)
    {
        if(ZA_STRCMP(state.list[i]->string, state.own[i].cStr()))
        {
            Cvar_SetQuick(state.list[i], state.own[i].cStr());
        }
    }
}

void registerCommands()
{
    Cmd_AddCommand("vr_serverrules", list_f);
    Cmd_AddCommand("vr_serverrules_selftest", selftest_f);
}

} // namespace qvr::serverrules

// Host_WriteConfigurationToFile, around Cvar_WriteVariables: while a remote server's rules govern, the player's own
// values written (each rule's string swapped for his own while the file is written, no callback run).
extern "C" void VR_ServerRulesConfigWrite(int begin)
{
    using namespace qvr::serverrules;
    if((begin != 0) == state.swapped || (begin && !state.shadowed))
    {
        return;
    }
    for(za::SizeT i = 0; i < state.list.size(); i++)
    {
        if(begin)
        {
            state.live[i] = state.list[i]->string;
            state.list[i]->string = state.own[i].cStr();
        }
        else
        {
            state.list[i]->string = state.live[i];
        }
    }
    state.swapped = begin != 0;
}

// CL_Disconnect: left the server: the player's own values back.
extern "C" void VR_ServerRulesLeft(void)
{
    qvr::serverrules::clientLeft();
}
