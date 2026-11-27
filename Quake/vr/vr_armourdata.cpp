// vr_armourdata.cpp -- see vr_armourdata.hpp (the armour's hand tweaks, quakevr/data/armour.txt).

#include "vr_armourdata.hpp"
#include "vr_agarmor.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_rig2.hpp"

#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <stdio.h>
#include <string.h>

namespace qvr::armourdata
{

namespace
{

struct Entry
{
    za::String name;
    Override parts[NumParts][NumSides];
};

constexpr int undoMost = 64;
constexpr uint32_t offsetBits = (1u << OffsetX) | (1u << OffsetY) | (1u << OffsetZ);
constexpr uint32_t turnBits = (1u << Pitch) | (1u << Yaw) | (1u << Roll);

za::Vector<Entry> table;
za::Vector<za::Vector<Entry>> undoSteps;
int gen = 1;
char pathText[MAX_OSPATH]{};
za::I64 fileTime = 0;   // (files::lastWriteTime's stamp of the file last read or written)
double lastLook = -1e9; // (realtime)

// The file's head: what each value does (written by save, the shipped file's whole text).
constexpr const char* fileHead =
    "// quakevr/data/armour.txt -- the armour's hand tweaks: per wearer kind and piece, laid over what the generator makes\n"
    "// (Quake/vr/vr_armourdata.hpp, vr_agarmor.cpp; docs/vr-port/ARMOUR_STUDIO.md). Nothing here: the generator's armour exactly.\n"
    "// Written by the Armour Studio (Debug > Tools > Armour Studio: Save) or by hand; read again when it changes on disk\n"
    "// (vr_armour_watch) or on vr_armour_reload.\n"
    "//\n"
    "// `kind <name> { <part> { key \"value\" ... } ... }`. Kinds: grunt, enforcer, knight, death_knight, player (every vrbody\n"
    "// build), or a model's path (\"progs/vrbody_bare.mdl\", over its kind's). Parts: cuirass, helmet, pauldron, bracer, thigh,\n"
    "// shin, each limb part's sleeve over its joint (pauldron_sleeve, bracer_sleeve, thigh_sleeve, shin_sleeve). A limb part\n"
    "// is both sides, mirrored (the left's values; the right's the mirror image); <part>_l or <part>_r one side's own, over it.\n"
    "//   offset   \"x y z\"   units along the model's forward, left and up (its rest pose)\n"
    "//   rotate   \"p y r\"   degrees about the model's left, up and forward axes, round the piece's middle\n"
    "//   scale    \"s\"       its size round its middle (1 as made)\n"
    "//   scale_x / scale_y / scale_z \"s\"  its size along the model's forward, left and up alone, times scale (1 as made)\n"
    "//   standoff \"u\"       units further off the skin (below 0 closer)\n"
    "//   start    \"u\"       units its first row of plates starts further along it (towards the waist, hand or foot)\n"
    "//   end      \"u\"       units its last row ends further along it\n"
    "//   rows     \"n\"       its rows of plates (0 the generator's; the cuirass counts its collar row)\n"
    "//   taper_start \"s\"  the plate rows' size round its line at its first row (1 as made), lerped row by row to ...\n"
    "//   taper_end   \"s\"  ... this at its last row (a thigh wider at the hip, narrower at the knee: 1.15 and 0.9)\n"
    "//   leather  \"0\"       a sleeve's leather not drawn at all (1 or unset: drawn)\n"
    "//   hero     \"0\"       its hero plates off, every plate its own (1 or unset: the breastplate, the pauldrons' top row,\n"
    "//                      the elbows' and knees' cops each one plate over several columns; vr_agarmor_hero 0: off everywhere)\n"
    "// (start, end, rows and the tapers: the cuirass and the limbs' plates; not the helmet or the sleeves; leather: the sleeves)\n"
    "// ad_knight: AD's death guard and crossbow knight, one set for both (their rigs' bones the same; else each by its path).\n"
    "// A beast by its model's path (\"progs/dog.mdl\"; Quake/vr/vr_beastarmour.cpp): its pieces cuirass, helmet, pauldron,\n"
    "// bracer, thigh (_l / _r as above), plate8 and plate9 (slots 8 and 9: the rottweiler's croup and collar, the ogre's back\n"
    "// plate and faulds...): offset, rotate, scale, standoff (pushed out from its middle), taper_start / taper_end (along its\n"
    "// bone). Each piece's <part>_sleeve is its leather junction to the piece it joins: leather \"0\" off, taper_start /\n"
    "// taper_end its width at this piece's end and the other's, standoff its thickness, folds, fold_depth, and\n"
    "//   sag      \"u\"       the junction's middle dropped this far (below 0 raised)\n";

[[nodiscard]] Entry* find(const char* name)
{
    for(Entry& e : table)
    {
        if(!ZA_STRCMP(e.name.cStr(), name))
        {
            return &e;
        }
    }
    return nullptr;
}

// A part's name in the file (and its side): "bracer", "bracer_l", "thigh_sleeve_r". False: no such part.
bool partOf(const char* name, int& part, int& side)
{
    const za::SizeT len = ZA_STRLEN(name);
    for(int p = 0; p < NumParts; p++)
    {
        const za::SizeT n = ZA_STRLEN(partNames[p]);
        if(!ZA_STRNCMP(name, partNames[p], n) && (len == n || (limbPart(p) && len == n + 2 && name[n] == '_' && (name[n + 1] == 'l' || name[n + 1] == 'r'))))
        {
            part = p;
            side = len == n ? SideBoth : name[n + 1] == 'l' ? SideLeft : SideRight;
            return true;
        }
    }
    return false;
}

void bump()
{
    gen++;
    agarmor::overridesChanged();
}

[[nodiscard]] bool anySet(const Entry& e)
{
    for(const auto& part : e.parts)
    {
        for(const Override& o : part)
        {
            if(o.has)
            {
                return true;
            }
        }
    }
    return false;
}

// The table as the file has it.
[[nodiscard]] za::String fileText()
{
    za::String out{fileHead};
    char line[256];
    // (the kinds in their order, then the models)
    za::Vector<const Entry*> order;
    for(const char* k : kindNames)
    {
        const Entry* e = find(k);
        if(e && anySet(*e))
        {
            order.pushBack(e);
        }
    }
    for(const Entry& e : table)
    {
        bool kind = false;
        for(const char* k : kindNames)
        {
            kind = kind || !ZA_STRCMP(k, e.name.cStr());
        }
        if(!kind && anySet(e))
        {
            order.pushBack(&e);
        }
    }
    for(const Entry* e : order)
    {
        q_snprintf(line, sizeof(line), "\nkind \"%s\"\n{\n", e->name.cStr());
        out += line;
        for(int part = 0; part < NumParts; part++)
        {
            for(int side = 0; side < NumSides; side++)
            {
                const Override& o = e->parts[part][side];
                if(!o.has)
                {
                    continue;
                }
                q_snprintf(line, sizeof(line), "    %s%s\n    {\n", partNames[part], side == SideLeft ? "_l" : side == SideRight ? "_r" : "");
                out += line;
                if(o.has & offsetBits)
                {
                    q_snprintf(line, sizeof(line), "        offset \"%g %g %g\"\n", static_cast<double>(o.v[OffsetX]), static_cast<double>(o.v[OffsetY]),
                        static_cast<double>(o.v[OffsetZ]));
                    out += line;
                }
                if(o.has & turnBits)
                {
                    q_snprintf(line, sizeof(line), "        rotate \"%g %g %g\"\n", static_cast<double>(o.v[Pitch]), static_cast<double>(o.v[Yaw]),
                        static_cast<double>(o.v[Roll]));
                    out += line;
                }
                for(int f = Scale; f < NumFields; f++)
                {
                    if(o.has & (1u << static_cast<unsigned>(f)))
                    {
                        q_snprintf(line, sizeof(line), "        %s \"%g\"\n", fieldNames[f], static_cast<double>(o.v[f]));
                        out += line;
                    }
                }
                out += "    }\n";
            }
        }
        out += "}\n";
    }
    return out;
}

// `text` read into `into`. Problems said, the rest read.
void parse(const char* text, za::Vector<Entry>& into)
{
    const char* data = text;
    int problems = 0;
    while((data = COM_Parse(data)) != nullptr)
    {
        if(ZA_STRCMP(com_token, "kind") != 0)
        {
            Con_Printf("\x02%s: \"%s\": expected \"kind <name>\"\n", pathText, com_token);
            problems++;
            break;
        }
        data = COM_Parse(data);
        if(!data)
        {
            break;
        }
        za::String name{com_token};
        if(!strchr(name.cStr(), '/'))
        {
            bool known = false;
            for(const char* k : kindNames)
            {
                known = known || !ZA_STRCMP(k, name.cStr());
            }
            if(!known)
            {
                Con_Printf("\x02%s: no kind \"%s\" (grunt, enforcer, knight, death_knight, player, ad_knight, or a model's path)\n", pathText, name.cStr());
                problems++;
            }
        }
        Entry* e = nullptr;
        for(Entry& x : into)
        {
            e = !ZA_STRCMP(x.name.cStr(), name.cStr()) ? &x : e;
        }
        if(!e)
        {
            into.pushBack(Entry{});
            e = &into.back();
            e->name = name;
        }
        data = COM_Parse(data);
        if(!data || ZA_STRCMP(com_token, "{") != 0)
        {
            Con_Printf("\x02%s: expected \"{\" after kind %s\n", pathText, name.cStr());
            problems++;
            break;
        }
        while((data = COM_Parse(data)) != nullptr && ZA_STRCMP(com_token, "}") != 0)
        {
            int part = 0, side = 0;
            const bool known = partOf(com_token, part, side);
            if(!known)
            {
                Con_Printf("\x02%s: kind %s: no part \"%s\": ignored\n", pathText, name.cStr(), com_token);
                problems++;
            }
            data = COM_Parse(data);
            if(!data || ZA_STRCMP(com_token, "{") != 0)
            {
                Con_Printf("\x02%s: expected \"{\" after a part's name\n", pathText);
                problems++;
                return;
            }
            Override scratch;
            Override& o = known ? e->parts[part][side] : scratch;
            while((data = COM_Parse(data)) != nullptr && ZA_STRCMP(com_token, "}") != 0)
            {
                za::String key{com_token};
                data = COM_Parse(data);
                if(!data)
                {
                    break;
                }
                float x[3]{0.f, 0.f, 0.f};
                const int got = sscanf(com_token, "%f %f %f", &x[0], &x[1], &x[2]);
                const bool triple = key == "offset" || key == "rotate";
                if(got != (triple ? 3 : 1))
                {
                    Con_Printf("\x02%s: %s: \"%s\" not %s: ignored\n", pathText, key.cStr(), com_token, triple ? "three numbers" : "a number");
                    problems++;
                    continue;
                }
                if(triple)
                {
                    const int first = key == "offset" ? OffsetX : Pitch;
                    for(int i = 0; i < 3; i++)
                    {
                        o.v[first + i] = x[i];
                    }
                    o.has |= key == "offset" ? offsetBits : turnBits;
                    continue;
                }
                int field = -1;
                for(int f = Scale; f < NumFields; f++)
                {
                    field = key == fieldNames[f] ? f : field;
                }
                if(field < 0)
                {
                    Con_Printf("\x02%s: no key \"%s\" (offset, rotate, scale, standoff, start, end, rows, taper_start, taper_end, "
                               "leather, folds, fold_depth, hero, sag, scale_x, scale_y, scale_z): ignored\n", pathText, key.cStr());
                    problems++;
                    continue;
                }
                o.v[field] = x[0];
                o.has |= 1u << static_cast<unsigned>(field);
            }
        }
    }
    if(problems > 0)
    {
        Con_Printf("\x02%s: %d problem%s\n", pathText, problems, problems == 1 ? "" : "s");
    }
}

void reload_f()
{
    const bool found = load();
    Con_Printf("armour: %s %s, %d part%s set\n", pathText, found ? "read" : "not there (the generator's armour)", countSet(), countSet() == 1 ? "" : "s");
}

void print_f()
{
    Con_Printf("%s", fileText().cStr());
    Con_Printf("armour: %s, %d part%s set, generation %d\n", pathText, countSet(), countSet() == 1 ? "" : "s", gen);
}

// AD's two knights (KindAdKnight).
constexpr const char* adModels[2] = {"progs/vrdguard.mdl", "progs/vrdcrossbow.mdl"};
int adSharedKnown = -1; // (adShared: -1 not looked yet, read again with the file)

// Part `part`, side `side` laid over `out` from `first`, then `second` (either may be null): the most specific wins value by
// value (each entry's side, then its both-sides value, mirrored on the right). Whether anything is set.
bool layerOver(Override& out, int part, int side, const Entry* first, const Entry* second)
{
    const Entry* layers[2] = {first, second};
    for(const Entry* e : layers)
    {
        for(int pass = 0; e && pass < (side == SideBoth ? 1 : 2); pass++)
        {
            const Override& o = e->parts[part][pass == 0 ? side : SideBoth];
            const bool mirror = pass == 1 && side == SideRight;
            for(int f = 0; f < NumFields; f++)
            {
                const uint32_t bit = 1u << static_cast<unsigned>(f);
                if((o.has & bit) && !(out.has & bit))
                {
                    out.v[f] = mirror && (f == OffsetY || f == Yaw || f == Roll) ? -o.v[f] : o.v[f];
                    out.has |= bit;
                }
            }
        }
    }
    return out.has != 0;
}

// (FNV-1a over a piece's values)
[[nodiscard]] uint32_t hashOver(uint32_t h, int key, const Override& o)
{
    h = h == 0 ? 2166136261u : h;
    h = (h ^ static_cast<uint32_t>(key)) * 16777619u;
    h = (h ^ o.has) * 16777619u;
    for(const float x : o.v)
    {
        uint32_t u = 0;
        ZA_MEMCPY(&u, &x, 4);
        h = (h ^ u) * 16777619u;
    }
    return h;
}

} // namespace

int kindOf(const char* model)
{
    if(!model)
    {
        return -1;
    }
    if(!ZA_STRCMP(model, "progs/vrgrunt.mdl") || !ZA_STRCMP(model, "progs/soldier.mdl"))
    {
        return KindGrunt;
    }
    if(!ZA_STRCMP(model, "progs/vrenforcer.mdl") || !ZA_STRCMP(model, "progs/enforcer.mdl"))
    {
        return KindEnforcer;
    }
    if(!ZA_STRCMP(model, "progs/vrknight.mdl"))
    {
        return KindKnight;
    }
    if(!ZA_STRCMP(model, "progs/vrhknight.mdl"))
    {
        return KindDeathKnight;
    }
    if((!ZA_STRCMP(model, adModels[0]) || !ZA_STRCMP(model, adModels[1])) && adShared())
    {
        return KindAdKnight;
    }
    return !ZA_STRNCMP(model, "progs/vrbody", 12) ? KindPlayer : -1;
}

bool adShared()
{
    // (peeked, not kept: reading them through rig2::rigFor would count them in its edits' generations; once a read of the
    // file, the authored rigs being files)
    if(adSharedKnown < 0)
    {
        rig2::Rig a, b;
        adSharedKnown = rig2::peek(adModels[0], a) && rig2::peek(adModels[1], b) && a.numBones > 0 && rig2::sameBones(a, b) ? 1 : 0;
    }
    return adSharedKnown == 1;
}

void beastPart(int slot, bool sleeve, int& part, int& side)
{
    side = slot >= agarmor::PauldronL && slot <= agarmor::ThighR ? ((slot - agarmor::PauldronL) % 2 == 0 ? SideLeft : SideRight) : SideBoth;
    if(slot >= agarmor::PauldronL && slot <= agarmor::ThighR)
    {
        part = PartPauldron + (slot - agarmor::PauldronL) / 2 + (sleeve ? PartPauldronSleeve - PartPauldron : 0);
        return;
    }
    part = slot == agarmor::Cuirass ? (sleeve ? PartCuirassSleeve : PartCuirass) : slot == agarmor::Helmet ? (sleeve ? PartHelmetSleeve : PartHelmet) :
        slot == 8 ? (sleeve ? PartPlate8Sleeve : PartPlate8) : (sleeve ? PartPlate9Sleeve : PartPlate9);
}

bool resolveBeast(const char* model, int slot, bool sleeve, Override& out)
{
    out = Override{};
    for(int f = 0; f < NumFields; f++)
    {
        out.v[f] = fieldDefault(f);
    }
    if(table.empty() || !model || slot < 0 || slot >= agarmor::NumPieces)
    {
        return false;
    }
    int part = 0, side = 0;
    beastPart(slot, sleeve, part, side);
    return layerOver(out, part, side, find(model), nullptr);
}

int beastSleeve(int part)
{
    return part == PartTail ? PartTailSleeve : part == PartPlate8Back ? PartPlate8BackSleeve : part >= PartPauldron && part <= PartShin ? part + (PartPauldronSleeve - PartPauldron) : part;
}

bool resolveBeastPart(const char* model, int part, int side, bool sleeve, Override& out)
{
    out = Override{};
    for(int f = 0; f < NumFields; f++)
    {
        out.v[f] = fieldDefault(f);
    }
    if(table.empty() || !model || part < 0 || part >= NumParts)
    {
        return false;
    }
    return layerOver(out, sleeve ? beastSleeve(part) : part, limbPart(part) ? side : SideBoth, find(model), nullptr);
}

uint32_t beastSignature(const char* model)
{
    uint32_t h = 0;
    for(int piece = 0; piece < agarmor::NumPieces; piece++)
    {
        for(const bool sleeve : {false, true})
        {
            Override o;
            if(resolveBeast(model, piece, sleeve, o))
            {
                h = hashOver(h, piece * 2 + (sleeve ? 1 : 0), o);
            }
        }
    }
    // (the parts with their own tweaks: the ogre's shins by side, the scrag's tail and his lower body's back half)
    for(const int part : {PartShin, PartTail, PartPlate8Back})
    {
        for(int side = SideLeft; side <= SideRight; side++)
        {
            for(const bool sleeve : {false, true})
            {
                Override o;
                if(resolveBeastPart(model, part, side, sleeve, o))
                {
                    h = hashOver(h, 64 + part * 8 + side * 2 + (sleeve ? 1 : 0), o);
                }
            }
        }
    }
    return h;
}

bool resolve(const char* model, int piece, bool sleeve, Override& out)
{
    out = Override{};
    for(int f = 0; f < NumFields; f++)
    {
        out.v[f] = fieldDefault(f);
    }
    if(table.empty() || piece < 0 || piece >= agarmor::NumPieces || (sleeve && piece < agarmor::PauldronL))
    {
        return false;
    }
    const int part = piece == agarmor::Cuirass ? PartCuirass : piece == agarmor::Helmet ? PartHelmet :
        PartPauldron + (piece - agarmor::PauldronL) / 2 + (sleeve ? PartPauldronSleeve - PartPauldron : 0);
    const int side = piece < agarmor::PauldronL ? SideBoth : (piece - agarmor::PauldronL) % 2 == 0 ? SideLeft : SideRight;
    const int kind = kindOf(model);
    return layerOver(out, part, side, model ? find(model) : nullptr, kind >= 0 ? find(kindNames[kind]) : nullptr);
}

uint32_t signature(const char* model)
{
    uint32_t h = 0;
    for(int piece = 0; piece < agarmor::NumPieces; piece++)
    {
        for(const bool sleeve : {false, true})
        {
            Override o;
            if(resolve(model, piece, sleeve, o))
            {
                h = hashOver(h, piece * 2 + (sleeve ? 1 : 0), o);
            }
        }
    }
    return h;
}

int generation()
{
    return gen;
}

Override* entry(const char* name, int part, int side, bool make)
{
    if(part < 0 || part >= NumParts || side < 0 || side >= NumSides)
    {
        return nullptr;
    }
    Entry* e = find(name);
    if(!e && make)
    {
        table.pushBack(Entry{});
        e = &table.back();
        e->name = za::String{name};
    }
    return e ? &e->parts[part][side] : nullptr;
}

void clear(const char* name, int part, int side)
{
    Entry* e = find(name);
    for(int p = 0; e && p < NumParts; p++)
    {
        for(int s = 0; s < NumSides; s++)
        {
            e->parts[p][s] = (part < 0 || part == p) && (side < 0 || side == s) ? Override{} : e->parts[p][s];
        }
    }
}

void changed()
{
    bump();
}

void pushUndo()
{
    if(static_cast<int>(undoSteps.size()) >= undoMost)
    {
        undoSteps.erase(&undoSteps[0]);
    }
    undoSteps.pushBack(table);
}

bool popUndo()
{
    if(undoSteps.empty())
    {
        return false;
    }
    table = undoSteps.back();
    undoSteps.popBack();
    bump();
    return true;
}

int undoCount()
{
    return static_cast<int>(undoSteps.size());
}

const char* filePath()
{
    // (quakevr/ in the last base folder that has one, as the game's data files: the file the studio writes is the one read)
    const char* name = vr_armour_file.string && vr_armour_file.string[0] ? vr_armour_file.string : "data/armour.txt";
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
    za::String text;
    const bool found = files::readText(path, text);
    za::Vector<Entry> fresh;
    if(found)
    {
        parse(text.cStr(), fresh);
    }
    table = fresh;
    adSharedKnown = -1;
    fileTime = files::lastWriteTime(path);
    bump();
    return found;
}

bool save()
{
    const char* path = filePath();
    char dir[MAX_OSPATH];
    q_strlcpy(dir, path, sizeof(dir));
    char* slash = strrchr(dir, '/');
    if(slash)
    {
        *slash = 0;
        (void)files::createDirectories(dir);
    }
    const bool ok = files::writeText(path, fileText().cStr());
    fileTime = files::lastWriteTime(path); // (not read again by the watcher: it is what we have)
    return ok;
}

int countSet()
{
    int n = 0;
    for(const Entry& e : table)
    {
        for(const auto& part : e.parts)
        {
            for(const Override& o : part)
            {
                n += o.has ? 1 : 0;
            }
        }
    }
    return n;
}

void init()
{
    Cmd_AddCommand("vr_armour_reload", reload_f);
    Cmd_AddCommand("vr_armour_print", print_f);
    (void)load();
}

void frame()
{
    if(!vr_armour_watch.value || (realtime - lastLook < 1.0 && realtime >= lastLook))
    {
        return;
    }
    lastLook = realtime;
    const za::I64 t = files::lastWriteTime(filePath());
    if(t != fileTime)
    {
        const bool found = load();
        Con_Printf("armour: %s %s, %d part%s set\n", pathText, found ? "changed: read again" : "gone: the generator's armour", countSet(),
            countSet() == 1 ? "" : "s");
    }
}

} // namespace qvr::armourdata
