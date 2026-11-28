// vr_reloadsys.cpp -- see vr_reloadsys.hpp. The procedural reloads' table: the built-ins (his shotgun's values, promoted
// 2026-11-28 as the vr_flat_prl_* tunables had them, which this file replaces; the super shotgun's), the file
// (quakevr/data/reloads.txt: `reload <gun> { <field> <values> ... key "<name>" { <field> <values> ... } ... }`, every field
// optional, its default where left out), and the commands.

#include "vr_reloadsys.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_strings.hpp"
#include "vr_weapon_ids.h"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::reloadsys
{

namespace
{

constexpr const char* itemNames[static_cast<int>(Item::Count)] = {"shell", "pair", "magazine", "rocket", "cylinder"};
constexpr const char* destNames[static_cast<int>(Dest::Count)] = {"port", "chamber", "receiver", "muzzle", "cylinder"};
constexpr const char* targetNames[static_cast<int>(Target::Count)] = {"rest", "pouch", "mouth", "dest", "hold"};
constexpr const char* actionNames[static_cast<int>(Action::Count)] = {"none", "take", "insert", "open", "close", "eject"};
constexpr const char* condNames[static_cast<int>(Cond::Count)] = {"always", "closed", "opened", "loaded"};
constexpr const char* easeNames[static_cast<int>(Ease::Count)] = {"linear", "smooth", "in", "out"};

// (reloadmags: the magazine guns after the shotguns; the lava nailguns and the plasma-look thunderbolt are the same guns.)
constexpr int gunCount = 6;
constexpr const char* gunNames[gunCount] = {"shotgun", "supershotgun", "nailgun", "supernailgun", "thunderbolt", "nailpistol"};
constexpr int gunWeapons[gunCount] = {WID_SHOTGUN, WID_SUPER_SHOTGUN, WID_NAILGUN, WID_SUPER_NAILGUN, WID_LIGHTNING, WID_NAILPISTOL};

struct Table
{
    Reload reloads[gunCount];
    int revision{1};
    int saved{1};
};
Table table;
char pathText[MAX_OSPATH];

Key makeKey(const char* name, float time, Target target)
{
    Key k;
    q_strlcpy(k.name, name, sizeof(k.name));
    k.time = time;
    k.target = target;
    return k;
}

// His shotgun (the vr_flat_prl_* values promoted 2026-11-28): the gun tilted as the hand reaches (0.17 s of its 0.2), the
// pouch, the pair to the port's mouth (0.5 out, bowed 3) and in, the next pair, back. The item's turn at the port baked from
// his hand at the port (pitch -40, yaw 59, roll -9 on the gun's) at a level view: the same look there.
Reload shotgun()
{
    Reload r;
    q_strlcpy(r.gun, "shotgun", sizeof(r.gun));
    r.weapon = WID_SHOTGUN;
    r.item = Item::Pair;
    r.dest = Dest::Port;
    r.itemRot = glm::vec3{-38.17f, -43.74f, 51.87f};
    const glm::vec3 tilt{-20.f, 60.f, 69.f}, move{3.f, -1.f, -2.5f};
    const glm::vec3 pouchOfs{0.f, 0.5f, 1.f}, pouchRot{90.f, 27.f, 5.f};
    Key k[6];
    k[0] = makeKey("reach", 0.2f, Target::Pouch);
    k[0].gunTime = 0.17f;
    k[1] = makeKey("grip", 0.06f, Target::Pouch);
    k[1].action = Action::Take;
    k[2] = makeKey("carry", 0.22f, Target::Mouth);
    k[2].arc = 3.f;
    k[2].approach = 0.5f;
    k[3] = makeKey("insert", 0.08f, Target::Dest);
    k[3].action = Action::Insert;
    k[4] = makeKey("reach again", 0.2f, Target::Pouch);
    k[4].arc = 3.f;
    k[5] = makeKey("return", 0.22f, Target::Rest);
    k[5].gunTime = 0.2f;
    k[5].finish = true;
    for(int i = 0; i < 6; i++)
    {
        if(k[i].target == Target::Pouch)
        {
            k[i].handOfs = pouchOfs;
            k[i].handRot = pouchRot;
        }
        if(i < 5)
        {
            k[i].gunRot = tilt;
            k[i].gunOfs = move;
        }
        r.keys[i] = k[i];
    }
    r.keyCount = 6;
    r.loopFrom = 1;
    r.loopTo = 4;
    return r;
}

// The super shotgun (reloadsys; his words: "it should break open after firing ... If it isn't, you should break it apart.
// And you should take two shells from the pouch and then put them into the barrels"): the gun turned muzzle down, in and
// over (its chambers up to the free hand); broken open by a flick down if it is shut (its spent shells out, a live one kept);
// a taped pair (one shell for one place) into the chambers; flicked up shut at the end if loaded. A start for his tuning.
Reload superShotgun()
{
    Reload r;
    q_strlcpy(r.gun, "supershotgun", sizeof(r.gun));
    r.weapon = WID_SUPER_SHOTGUN;
    r.item = Item::Pair;
    r.dest = Dest::Chamber;
    r.itemRot = glm::vec3{-38.17f, -43.74f, 51.87f};
    const glm::vec3 tilt{25.f, 35.f, -30.f}, move{2.f, -1.f, -1.f};
    const glm::vec3 pouchOfs{0.f, 0.5f, 1.f}, pouchRot{90.f, 27.f, 5.f};
    Key k[9];
    k[0] = makeKey("present", 0.17f, Target::Hold);
    k[1] = makeKey("break open", 0.18f, Target::Hold);
    k[1].gunTime = 0.12f;
    k[1].action = Action::Open;
    k[1].cond = Cond::Closed;
    k[1].ease = Ease::Out;
    k[2] = makeKey("reach", 0.2f, Target::Pouch);
    k[3] = makeKey("grip", 0.06f, Target::Pouch);
    k[3].action = Action::Take;
    k[4] = makeKey("carry", 0.24f, Target::Mouth);
    k[4].arc = 3.f;
    k[4].approach = 0.8f;
    k[5] = makeKey("insert", 0.1f, Target::Dest);
    k[5].action = Action::Insert;
    k[6] = makeKey("reach again", 0.2f, Target::Pouch);
    k[6].arc = 3.f;
    k[7] = makeKey("return", 0.2f, Target::Rest);
    k[7].gunTime = 0.14f;
    k[7].finish = true;
    k[7].cond = Cond::Loaded;
    k[7].gunRot = glm::vec3{-15.f, 15.f, -10.f};
    k[8] = makeKey("close", 0.18f, Target::Rest);
    k[8].action = Action::Close;
    k[8].finish = true;
    for(int i = 0; i < 9; i++)
    {
        if(k[i].target == Target::Pouch)
        {
            k[i].handOfs = pouchOfs;
            k[i].handRot = pouchRot;
        }
        if(i < 7)
        {
            k[i].gunRot = tilt;
            k[i].gunOfs = move;
        }
        r.keys[i] = k[i];
    }
    r.keys[1].gunRot = tilt + glm::vec3{18.f, 0.f, 0.f}; // (the flick down)
    r.keyCount = 9;
    r.loopFrom = 3;
    r.loopTo = 6;
    return r;
}

// A magazine gun (reloadmags; his words 2026-11-28: "we should get into a state where the magazine has been ejected. And
// then the hand will grab a magazine from the pouch ... work backwards from the target position of the magazine ... Each
// weapon also should ... tilt a little bit"): the gun turned to present its well (`tilt`, `move`), the magazine in it let
// out by a flick (`flick` on the tilt; skipped with none in: an empty gun has dropped it already), the pouch, a magazine
// carried to `approach` units out of the well along its line (the seated magazine's: the backwards solve) and pushed home,
// back. A start for his tuning in the Reload Studio.
Reload magazine(int gun, const glm::vec3& tilt, const glm::vec3& move, const glm::vec3& flick, float approach)
{
    Reload r;
    q_strlcpy(r.gun, gunNames[gun], sizeof(r.gun));
    r.weapon = gunWeapons[gun];
    r.item = Item::Magazine;
    r.dest = Dest::Receiver;
    const glm::vec3 pouchOfs{0.f, 0.5f, 1.f}, pouchRot{90.f, 27.f, 5.f};
    Key k[7];
    k[0] = makeKey("present", 0.15f, Target::Hold);
    k[1] = makeKey("eject", 0.14f, Target::Hold);
    k[1].gunTime = 0.1f;
    k[1].ease = Ease::Out;
    k[1].action = Action::Eject;
    k[1].cond = Cond::Closed;
    k[2] = makeKey("reach", 0.22f, Target::Pouch);
    k[3] = makeKey("grip", 0.06f, Target::Pouch);
    k[3].action = Action::Take;
    k[4] = makeKey("carry", 0.26f, Target::Mouth);
    k[4].arc = 3.f;
    k[4].approach = approach;
    k[5] = makeKey("insert", 0.1f, Target::Dest);
    k[5].action = Action::Insert;
    k[6] = makeKey("return", 0.22f, Target::Rest);
    k[6].gunTime = 0.2f;
    k[6].finish = true;
    for(int i = 0; i < 7; i++)
    {
        if(k[i].target == Target::Pouch)
        {
            k[i].handOfs = pouchOfs;
            k[i].handRot = pouchRot;
        }
        if(i < 6)
        {
            k[i].gunRot = tilt;
            k[i].gunOfs = move;
        }
        r.keys[i] = k[i];
    }
    r.keys[1].gunRot = tilt + flick;
    r.keyCount = 7;
    return r;
}

// The built-in reload of gun `i` (gunNames). The magazine guns' tilts: the nailgun's and the thunderbolt's wells under them
// rolled in and up to the free hand as the shotgun's port; the super nailgun's on its inner side, already the free hand's,
// turned in; the nail pistol's up its grip, the grip's foot turned forward and in.
Reload builtInAt(int i)
{
    switch(gunWeapons[za::clamp(i, 0, gunCount - 1)])
    {
        case WID_SUPER_SHOTGUN: return superShotgun();
        case WID_NAILGUN: return magazine(i, {-15.f, 45.f, 60.f}, {3.f, -1.f, -2.f}, {12.f, 0.f, 10.f}, 4.f);
        case WID_SUPER_NAILGUN: return magazine(i, {-10.f, 40.f, -10.f}, {3.f, -1.f, -1.f}, {0.f, 0.f, -25.f}, 4.f);
        case WID_LIGHTNING: return magazine(i, {-15.f, 45.f, 55.f}, {3.f, -1.f, -2.f}, {12.f, 0.f, 10.f}, 4.f);
        case WID_NAILPISTOL: return magazine(i, {-35.f, 30.f, 45.f}, {3.f, -1.f, -1.f}, {15.f, 0.f, 0.f}, 3.f);
        default: return shotgun();
    }
}

[[nodiscard]] int lookup(const char* word, const char* const* names, int n)
{
    for(int i = 0; i < n; i++)
    {
        if(!ZA_STRCMP(word, names[i]))
        {
            return i;
        }
    }
    return -1;
}

template<class E>
bool setWord(E& v, const char* const* names, int count, const char* const* args, int n)
{
    const int i = n >= 1 ? lookup(args[0], names, count) : -1;
    if(i < 0)
    {
        return false;
    }
    v = static_cast<E>(i);
    return true;
}

// The fields (the file's words, the set command's): a reload's (key nullptr) or a key's, `args` their values (`n` of them).
// False: not a field, or too few values.
bool setField(Reload& r, Key* k, const char* field, const char* const* args, int n)
{
    const auto vec = [&](glm::vec3& v) {
        if(n < 3)
        {
            return false;
        }
        v = glm::vec3{static_cast<float>(atof(args[0])), static_cast<float>(atof(args[1])), static_cast<float>(atof(args[2]))};
        return true;
    };
    const auto num = [&](float& v) {
        if(n < 1)
        {
            return false;
        }
        v = static_cast<float>(atof(args[0]));
        return true;
    };
    const auto word = [&](auto& v, const char* const* names, int count) { return setWord(v, names, count, args, n); };
    if(!k)
    {
        if(!ZA_STRCMP(field, "item")) return word(r.item, itemNames, static_cast<int>(Item::Count));
        if(!ZA_STRCMP(field, "dest")) return word(r.dest, destNames, static_cast<int>(Dest::Count));
        if(!ZA_STRCMP(field, "item_rot")) return vec(r.itemRot);
        if(!ZA_STRCMP(field, "loop"))
        {
            if(n < 2)
            {
                return false;
            }
            r.loopFrom = atoi(args[0]) - 1; // (the file's keys counted from 1; 0 none)
            r.loopTo = atoi(args[1]) - 1;
            return true;
        }
        return false;
    }
    if(!ZA_STRCMP(field, "name"))
    {
        if(n < 1)
        {
            return false;
        }
        q_strlcpy(k->name, args[0], sizeof(k->name));
        return true;
    }
    if(!ZA_STRCMP(field, "time")) return num(k->time);
    if(!ZA_STRCMP(field, "gun_time")) return num(k->gunTime);
    if(!ZA_STRCMP(field, "ease")) return word(k->ease, easeNames, static_cast<int>(Ease::Count));
    if(!ZA_STRCMP(field, "hand")) return word(k->target, targetNames, static_cast<int>(Target::Count));
    if(!ZA_STRCMP(field, "hand_ofs")) return vec(k->handOfs);
    if(!ZA_STRCMP(field, "hand_rot")) return vec(k->handRot);
    if(!ZA_STRCMP(field, "arc")) return num(k->arc);
    if(!ZA_STRCMP(field, "approach")) return num(k->approach);
    if(!ZA_STRCMP(field, "gun_rot")) return vec(k->gunRot);
    if(!ZA_STRCMP(field, "gun_ofs")) return vec(k->gunOfs);
    if(!ZA_STRCMP(field, "action")) return word(k->action, actionNames, static_cast<int>(Action::Count));
    if(!ZA_STRCMP(field, "if")) return word(k->cond, condNames, static_cast<int>(Cond::Count));
    if(!ZA_STRCMP(field, "finish"))
    {
        float v = 0.f;
        const bool ok = num(v);
        k->finish = v != 0.f;
        return ok;
    }
    return false;
}

// How many values field `field` takes (0: not a field).
[[nodiscard]] int arity(const char* field)
{
    constexpr const char* threes[] = {"item_rot", "hand_ofs", "hand_rot", "gun_rot", "gun_ofs"};
    for(const char* t : threes)
    {
        if(!ZA_STRCMP(field, t))
        {
            return 3;
        }
    }
    return !ZA_STRCMP(field, "loop") ? 2 : 1;
}

// `text` read into the table (an entry for a gun replaces its built-in). Problems said, the rest read.
void parse(const char* text)
{
    const char* data = text;
    while((data = COM_Parse(data)) != nullptr)
    {
        if(ZA_STRCMP(com_token, "reload") != 0 || !(data = COM_Parse(data)))
        {
            Con_Printf("\x02%s: \"%s\": expected \"reload <gun>\"\n", pathText, com_token);
            return;
        }
        const int gun = lookup(com_token, gunNames, gunCount);
        Reload fresh;
        Reload scratch;
        Reload& r = gun >= 0 ? fresh : scratch;
        if(gun >= 0)
        {
            q_strlcpy(r.gun, gunNames[gun], sizeof(r.gun));
            r.weapon = gunWeapons[gun];
        }
        else
        {
            Con_Printf("\x02%s: no gun \"%s\" (skipped)\n", pathText, com_token);
        }
        if(!(data = COM_Parse(data)) || ZA_STRCMP(com_token, "{") != 0)
        {
            Con_Printf("\x02%s: expected \"{\" after \"reload %s\"\n", pathText, r.gun);
            return;
        }
        while((data = COM_Parse(data)) != nullptr && ZA_STRCMP(com_token, "}") != 0)
        {
            if(!ZA_STRCMP(com_token, "key"))
            {
                if(!(data = COM_Parse(data)))
                {
                    return;
                }
                Key k;
                q_strlcpy(k.name, com_token, sizeof(k.name));
                if(!(data = COM_Parse(data)) || ZA_STRCMP(com_token, "{") != 0)
                {
                    Con_Printf("\x02%s: expected \"{\" after key \"%s\"\n", pathText, k.name);
                    return;
                }
                while((data = COM_Parse(data)) != nullptr && ZA_STRCMP(com_token, "}") != 0)
                {
                    char field[32];
                    q_strlcpy(field, com_token, sizeof(field));
                    const int n = arity(field);
                    char vals[3][64];
                    const char* args[3] = {vals[0], vals[1], vals[2]};
                    for(int i = 0; i < n && data; i++)
                    {
                        data = COM_Parse(data);
                        q_strlcpy(vals[i], data ? com_token : "0", sizeof(vals[i]));
                    }
                    if(!setField(r, &k, field, args, n))
                    {
                        Con_Printf("\x02%s: %s, key \"%s\": bad field \"%s\"\n", pathText, r.gun, k.name, field);
                    }
                }
                if(r.keyCount < maxKeys)
                {
                    r.keys[r.keyCount++] = k;
                }
                continue;
            }
            char field[32];
            q_strlcpy(field, com_token, sizeof(field));
            const int n = arity(field);
            char vals[3][64];
            const char* args[3] = {vals[0], vals[1], vals[2]};
            for(int i = 0; i < n && data; i++)
            {
                data = COM_Parse(data);
                q_strlcpy(vals[i], data ? com_token : "0", sizeof(vals[i]));
            }
            if(!setField(r, nullptr, field, args, n))
            {
                Con_Printf("\x02%s: %s: bad field \"%s\"\n", pathText, r.gun, field);
            }
        }
        if(gun >= 0 && r.keyCount > 0)
        {
            r.loopFrom = r.loopFrom < r.keyCount ? r.loopFrom : -1;
            r.loopTo = r.loopTo < r.keyCount ? r.loopTo : -1;
            table.reloads[gun] = r;
        }
    }
}

using strings::appendf;

[[nodiscard]] za::String fileText()
{
    za::String out{"// quakevr/data/reloads.txt -- flat-screen play's procedural reloads, per gun (vr_reloadsys.hpp; written by the Reload\n"
                   "// Studio, vr_reloadstudio). Keys play in order; `loop a b` repeats keys a..b until the gun is full; `finish 1` keys\n"
                   "// are the reload's end. hand: rest | pouch | mouth | dest | hold; action: none | take | insert | open | close | eject;\n"
                   "// if: always | closed | opened | loaded; ease: linear | smooth | in | out. Offsets in units (forward, right, up),\n"
                   "// turns in degrees (pitch, yaw, roll); the free hand's mirrored by its side, the gun's by its hand's.\n"};
    for(const Reload& r : table.reloads)
    {
        appendf(out, "\nreload %s\n{\n", r.gun);
        appendf(out, "    item %s\n    dest %s\n", itemNames[static_cast<int>(r.item)], destNames[static_cast<int>(r.dest)]);
        appendf(out, "    item_rot %g %g %g\n    loop %d %d\n", r.itemRot.x, r.itemRot.y, r.itemRot.z, r.loopFrom + 1, r.loopTo + 1);
        for(int i = 0; i < r.keyCount; i++)
        {
            const Key& k = r.keys[i];
            appendf(out, "    key \"%s\"\n    {\n", k.name);
            appendf(out, "        time %g\n        gun_time %g\n        ease %s\n", k.time, k.gunTime, easeNames[static_cast<int>(k.ease)]);
            appendf(out, "        hand %s\n        hand_ofs %g %g %g\n        hand_rot %g %g %g\n", targetNames[static_cast<int>(k.target)],
                k.handOfs.x, k.handOfs.y, k.handOfs.z, k.handRot.x, k.handRot.y, k.handRot.z);
            appendf(out, "        arc %g\n        approach %g\n", k.arc, k.approach);
            appendf(out, "        gun_rot %g %g %g\n        gun_ofs %g %g %g\n", k.gunRot.x, k.gunRot.y, k.gunRot.z, k.gunOfs.x, k.gunOfs.y,
                k.gunOfs.z);
            appendf(out, "        action %s\n        if %s\n        finish %d\n    }\n", actionNames[static_cast<int>(k.action)],
                condNames[static_cast<int>(k.cond)], k.finish ? 1 : 0);
        }
        out += "}\n";
    }
    return out;
}

[[nodiscard]] Reload* byName(const char* name)
{
    const int i = lookup(name, gunNames, gunCount);
    return i >= 0 ? &table.reloads[i] : nullptr;
}

void reload_f()
{
    Con_Printf("reloadsys: %s %s\n", filePath(), load() ? "read" : "not found: the built-ins");
}

void save_f()
{
    Con_Printf("reloadsys: %s %s\n", filePath(), save() ? "written" : "COULD NOT BE WRITTEN");
}

void list_f()
{
    for(const Reload& r : table.reloads)
    {
        Con_Printf("reloadsys: %s (weapon %d): %s into the %s, item turn %.1f %.1f %.1f, loop %d-%d, %d keys\n", r.gun, r.weapon,
            itemNames[static_cast<int>(r.item)], destNames[static_cast<int>(r.dest)], r.itemRot.x, r.itemRot.y, r.itemRot.z, r.loopFrom,
            r.loopTo, r.keyCount);
        for(int i = 0; i < r.keyCount; i++)
        {
            const Key& k = r.keys[i];
            Con_Printf("reloadsys:  %d %s: %.2f s (gun %.2f) %s, hand %s, action %s, if %s%s\n", i + 1, k.name, k.time, gunTime(k),
                easeNames[static_cast<int>(k.ease)], targetNames[static_cast<int>(k.target)], actionNames[static_cast<int>(k.action)],
                condNames[static_cast<int>(k.cond)], k.finish ? ", finish" : "");
        }
    }
}

// vr_reloadsys_set <gun> <key: its number from 1, its name, or - for the reload's own> <field> <values...>
void set_f()
{
    if(Cmd_Argc() < 5)
    {
        Con_Printf("vr_reloadsys_set <gun> <key number|name|-> <field> <values...>: a reload's field (the file's words)\n");
        return;
    }
    Reload* r = byName(Cmd_Argv(1));
    if(!r)
    {
        Con_Printf("reloadsys: no gun \"%s\"\n", Cmd_Argv(1));
        return;
    }
    Key* k = nullptr;
    const char* which = Cmd_Argv(2);
    if(ZA_STRCMP(which, "-") != 0)
    {
        const int n = atoi(which);
        for(int i = 0; i < r->keyCount && !k; i++)
        {
            if((n > 0 && i == n - 1) || !ZA_STRCMP(r->keys[i].name, which))
            {
                k = &r->keys[i];
            }
        }
        if(!k)
        {
            Con_Printf("reloadsys: %s has no key \"%s\"\n", r->gun, which);
            return;
        }
    }
    const char* args[3] = {Cmd_Argv(4), Cmd_Argc() > 5 ? Cmd_Argv(5) : "0", Cmd_Argc() > 6 ? Cmd_Argv(6) : "0"};
    if(!setField(*r, k, Cmd_Argv(3), args, Cmd_Argc() - 4))
    {
        Con_Printf("reloadsys: bad field or values \"%s\"\n", Cmd_Argv(3));
        return;
    }
    touched();
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_reloadsys_reload", reload_f);
    Cmd_AddCommand("vr_reloadsys_save", save_f);
    Cmd_AddCommand("vr_reloadsys_list", list_f);
    Cmd_AddCommand("vr_reloadsys_set", set_f);
    for(int i = 0; i < gunCount; i++)
    {
        table.reloads[i] = builtInAt(i);
    }
}

Reload* forWeapon(int weapon)
{
    for(Reload& r : table.reloads)
    {
        if(r.weapon == weapon && r.keyCount > 0)
        {
            return &r;
        }
    }
    return nullptr;
}

int count()
{
    return gunCount;
}

Reload& at(int i)
{
    return table.reloads[za::clamp(i, 0, gunCount - 1)];
}

Reload builtIn(int weapon)
{
    for(int i = 0; i < gunCount; i++)
    {
        if(gunWeapons[i] == weapon)
        {
            return builtInAt(i);
        }
    }
    return shotgun();
}

const char* filePath()
{
    // (quakevr/ in the last base folder that has one, as the armour's: the file the studio writes is the one read)
    const char* name = vr_reload_file.string && vr_reload_file.string[0] ? vr_reload_file.string : "data/reloads.txt";
    for(int i = com_numbasedirs - 1; i >= 0; i--)
    {
        char dir[MAX_OSPATH];
        q_snprintf(dir, sizeof(dir), "%s/quakevr", com_basedirs[i]);
        if(files::isDirectory(dir) || i == 0)
        {
            q_snprintf(pathText, sizeof(pathText), "%s/%s", dir, name);
            return pathText;
        }
    }
    q_snprintf(pathText, sizeof(pathText), "quakevr/%s", name);
    return pathText;
}

bool load()
{
    const char* path = filePath();
    for(int i = 0; i < gunCount; i++)
    {
        table.reloads[i] = builtInAt(i);
    }
    za::String text;
    const bool found = files::readText(path, text);
    if(found)
    {
        parse(text.cStr());
    }
    table.revision++;
    table.saved = table.revision;
    return found;
}

bool save()
{
    const char* path = filePath();
    char dir[MAX_OSPATH];
    q_strlcpy(dir, path, sizeof(dir));
    if(char* slash = strrchr(dir, '/'))
    {
        *slash = 0;
        (void)files::createDirectories(dir);
    }
    const bool ok = files::writeText(path, fileText().cStr());
    if(ok)
    {
        table.saved = table.revision;
    }
    return ok;
}

int revision()
{
    return table.revision;
}

void touched()
{
    table.revision++;
}

int savedRevision()
{
    return table.saved;
}

const char* itemName(Item v)
{
    return itemNames[za::clamp(static_cast<int>(v), 0, static_cast<int>(Item::Count) - 1)];
}
const char* destName(Dest v)
{
    return destNames[za::clamp(static_cast<int>(v), 0, static_cast<int>(Dest::Count) - 1)];
}
const char* targetName(Target v)
{
    return targetNames[za::clamp(static_cast<int>(v), 0, static_cast<int>(Target::Count) - 1)];
}
const char* actionName(Action v)
{
    return actionNames[za::clamp(static_cast<int>(v), 0, static_cast<int>(Action::Count) - 1)];
}
const char* condName(Cond v)
{
    return condNames[za::clamp(static_cast<int>(v), 0, static_cast<int>(Cond::Count) - 1)];
}
const char* easeName(Ease v)
{
    return easeNames[za::clamp(static_cast<int>(v), 0, static_cast<int>(Ease::Count) - 1)];
}

float ease(Ease e, float t)
{
    const float c = za::clamp(t, 0.f, 1.f);
    switch(e)
    {
        case Ease::Linear: return c;
        case Ease::In: return c * c;
        case Ease::Out: return 1.f - (1.f - c) * (1.f - c);
        default: return c * c * (3.f - 2.f * c);
    }
}

float handTime(const Key& k)
{
    return za::max(0.02f, k.time);
}

float gunTime(const Key& k)
{
    return za::max(0.02f, k.gunTime > 0.f ? k.gunTime : k.time);
}

int takeKey(const Reload& r)
{
    for(int i = 0; i < r.keyCount; i++)
    {
        if(r.keys[i].action == Action::Take)
        {
            return i;
        }
    }
    return -1;
}

int insertKey(const Reload& r)
{
    for(int i = 0; i < r.keyCount; i++)
    {
        if(r.keys[i].action == Action::Insert)
        {
            return i;
        }
    }
    return -1;
}

} // namespace qvr::reloadsys
