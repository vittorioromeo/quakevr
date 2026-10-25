// vr_propose_defaults.cpp -- Save My Settings as Defaults: the settings changed from their shipped defaults, written
// for review (vr_propose_defaults.hpp).

#include "vr_propose_defaults.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_menu.hpp"

#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Container/Vector.hpp"
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace qvr::proposedefaults
{
namespace
{

// The areas, in the file's order: the menus' pages, the weapon slots, the held objects' slots, the rest.
enum Rank : int
{
    RankMenu,
    RankWeaponSlot,
    RankPropSlot,
    RankOther
};

struct Row
{
    const cvar_t* var{nullptr};
    za::String area;  // its section's title: the pages above its row, "Weapon slot 05 (progs/v_rock.mdl)", ...
    za::String where; // its row's menu path ("" : not in the menus)
    int rank{RankOther};
};

// What a name holding one of these looks like (lower case): one's own (the body, the room, the comfort settings, the
// menus' own state) or a debug aid.
constexpr const char* personalParts[] = {"calib", "bodycal", "handcal", "comfort", "height", "handed", "seated",
    "floor_offset", "gunangle", "gunyaw", "offhandpitch", "offhandyaw", "dominant", "body_tweak", "body_arm",
    "body_elbow", "body_shoulder", "body_forearm", "body_wrist", "body_eye", "body_torso", "xr_runtime", "_version",
    "tips_seen", "tutorial", "note_device", "vr_motion_", "vr_menu_", "vignette", "snapturn"};
constexpr const char* debugParts[] = {"debug", "test", "show", "dump", "profile", "trace", "mock", "dryrun",
    "developer", "verbose", "_log", "log_"};

[[nodiscard]] bool hasAny(const char* name, const char* const* parts, int count)
{
    for(int i = 0; i < count; i++)
    {
        if(strstr(name, parts[i]))
        {
            return true;
        }
    }
    return false;
}

// Two values the same: as numbers when both are (0.50 and 0.5), else as text.
[[nodiscard]] bool sameValue(const char* a, const char* b)
{
    char* endA = nullptr;
    char* endB = nullptr;
    const double x = strtod(a, &endA);
    const double y = strtod(b, &endB);
    if(endA != a && !*endA && endB != b && !*endB)
    {
        return static_cast<float>(x) == static_cast<float>(y);
    }
    return strcmp(a, b) == 0;
}

// A slot's number from a name's last "_NN" (0: none).
[[nodiscard]] int slotOf(const char* name)
{
    const char* last = strrchr(name, '_');
    if(!last || !last[1])
    {
        return 0;
    }
    for(const char* c = last + 1; *c; c++)
    {
        if(*c < '0' || *c > '9')
        {
            return 0;
        }
    }
    return atoi(last + 1);
}

// The weapon or held object slot's area, `prefix` "vr_wofs" or "vr_prop": its model (its ID key) after its number.
[[nodiscard]] za::String slotArea(const char* title, const char* prefix, int slot)
{
    const cvar_t* id = Cvar_FindVar(va("%s_id_%02d", prefix, slot));
    const char* model = id && id->string[0] ? id->string : "free";
    return za::String{va("%s %02d (%s)", title, slot, model)};
}

// The rest's area: the name up to its second underscore ("vr_lootbeam_*"), or its first ("r_*", "gl_*"); a name without
// one, itself.
[[nodiscard]] za::String prefixArea(const char* name)
{
    const char* first = strchr(name, '_');
    const char* second = first && !q_strncasecmp(name, "vr_", 3) ? strchr(first + 1, '_') : nullptr;
    const char* end = second ? second : first;
    if(!end)
    {
        return za::String{"Not in the menus: "} + name;
    }
    return za::String{"Not in the menus: "} + za::String{za::StringView{name, static_cast<za::SizeT>(end - name)}} + "_*";
}

[[nodiscard]] const char* tagOf(const cvar_t& var)
{
    char lower[128];
    q_strlcpy(lower, var.name, sizeof(lower));
    for(char* c = lower; *c; c++)
    {
        *c = *c >= 'A' && *c <= 'Z' ? static_cast<char>(*c - 'A' + 'a') : *c;
    }
    if(hasAny(lower, debugParts, static_cast<int>(sizeof(debugParts) / sizeof(debugParts[0]))))
    {
        return ", debug?";
    }
    if(hasAny(lower, personalParts, static_cast<int>(sizeof(personalParts) / sizeof(personalParts[0]))))
    {
        return ", personal?";
    }
    return "";
}

// A value as a quoted cfg value (its own quotes made single).
[[nodiscard]] za::String quoted(const char* value)
{
    za::String out{"\""};
    for(const char* c = value; *c; c++)
    {
        out += *c == '"' ? '\'' : *c;
    }
    out += '"';
    return out;
}

void propose_f()
{
    za::String path;
    (void)write(path);
}

} // namespace

int write(za::String& path)
{
    za::Vector<Row> rows;
    for(const cvar_t* var = Cvar_FindVarAfter("", 0); var; var = Cvar_FindVarAfter(var->name, 0))
    {
        if(!var->default_string || (var->flags & CVAR_ROM) || sameValue(var->string, var->default_string))
        {
            continue;
        }
        Row r;
        r.var = var;
        const int slot = slotOf(var->name);
        if(slot > 0 && !q_strncasecmp(var->name, "vr_wofs_", 8))
        {
            r.rank = RankWeaponSlot;
            r.area = slotArea("Weapon slot", "vr_wofs", slot);
            r.where = "its weapon's tuning pages (that weapon held)";
        }
        else if(slot > 0 && !q_strncasecmp(var->name, "vr_prop_", 8))
        {
            r.rank = RankPropSlot;
            r.area = slotArea("Held object slot", "vr_prop", slot);
            r.where = "its held object's tuning pages (that object held)";
        }
        rows.pushBack(ZA_MOVE(r));
    }

    // The menus' rows: each setting's first (nearest the roots).
    menu::forEachSettingPath(
        [&](const cvar_t& var, const char* page, const char* label)
        {
            for(Row& r : rows)
            {
                if(r.var == &var && r.rank == RankOther && r.where.empty())
                {
                    r.rank = RankMenu;
                    r.area = page;
                    r.where = za::String{page} + " > " + label;
                }
            }
        });
    for(Row& r : rows)
    {
        if(r.rank == RankOther)
        {
            r.area = prefixArea(r.var->name);
        }
    }
    za::stableSort(rows.begin(), rows.end(),
        [](const Row& a, const Row& b)
        {
            if(a.rank != b.rank)
            {
                return a.rank < b.rank;
            }
            if(const int order = q_strcasecmp(a.area.cStr(), b.area.cStr()); order != 0)
            {
                return order < 0;
            }
            return q_strcasecmp(a.var->name, b.var->name) < 0;
        });

    const time_t now = time(nullptr);
    const za::String dir = za::String{com_gamedir} + "/proposed_defaults";
    files::createDirectories(dir.cStr());
    const za::String stamp = files::timestamp(now);
    path = dir + "/" + stamp + ".cfg";
    for(int n = 2; files::exists(path.cStr()); n++)
    {
        path = va("%s/%s_%d.cfg", dir.cStr(), stamp.cStr(), n); // (two in one second)
    }

    za::String text;
    text += "// Quake VR: settings changed from their shipped defaults, proposed as new defaults (vr_propose_defaults, ";
    text += files::timestamp(now, "%Y-%m-%d %H:%M:%S");
    text += ").\n// Nothing was changed in the game. Each line: name \"value\" // default \"old\", its menu row, archived or "
            "not;\n// \"personal?\" and \"debug?\" mark the rows that look like one's own or a debug aid, for filtering.\n";
    const za::String* lastArea = nullptr;
    for(const Row& r : rows)
    {
        if(!lastArea || *lastArea != r.area)
        {
            text += "\n// == ";
            text += r.area;
            text += " ==\n";
            lastArea = &r.area;
        }
        text += r.var->name;
        text += " ";
        text += quoted(r.var->string);
        text += " // default ";
        text += quoted(r.var->default_string);
        text += ", ";
        text += r.where.empty() ? "not in the menus" : r.where.cStr();
        text += (r.var->flags & CVAR_ARCHIVE) ? ", archived" : ", not archived";
        text += tagOf(*r.var);
        text += "\n";
    }
    if(rows.empty())
    {
        text += "\n// Nothing changed from the defaults.\n";
    }
    if(!files::writeText(path.cStr(), text.toStringView()))
    {
        Con_Printf("vr_propose_defaults: can't write %s\n", path.cStr());
        return -1;
    }
    const int count = static_cast<int>(rows.size());
    Con_Printf("Saved %d setting%s for review: %s\n", count, count == 1 ? "" : "s", path.cStr());
    return count;
}

void init()
{
    Cmd_AddCommand("vr_propose_defaults", propose_f);
}

} // namespace qvr::proposedefaults
