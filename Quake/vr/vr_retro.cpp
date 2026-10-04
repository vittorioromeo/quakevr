// vr_retro.cpp -- see vr_retro.hpp (the settings, the overrides) and vr_retro.h (the shaders' side).

#include "vr_retro.hpp"
#include "vr_retro.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_menu.hpp"
#include "vr_text3d.hpp"
#include "vr_view.hpp"
#include "vr_zancle.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <ctype.h>
#include <stdlib.h>

extern "C" GLuint gl_palette_lut; // gl_texmgr.c: the palette's nearest-colour table (128^3 indices)

namespace qvr::retro
{
namespace
{

struct ParamInfo
{
    const char* suffix; // the cvar's (vr_retro_<key><suffix>); the files' name is it without the '_' ("on" for "")
    const char* def;
    bool toggle;        // 0 or 1 (a multiplier in an override is then an AND)
};

// The settings of every category, in Param's order.
constexpr ParamInfo paramInfo[paramCount] = {
    {"", "1", true},               // On
    {"_snap", "1", true},          // Snap
    {"_block", "1", false},        // Block
    {"_units", "0", true},         // Units
    {"_average", "1", true},       // Average
    {"_soft", "1", false},         // Soft
    {"_fade", "1", false},         // Fade
    {"_palette", "0", false},      // Palette
    {"_dither", "0", false},       // Dither
    {"_dither_scale", "1", false}, // DitherScale
    {"_bump", "1", false},         // Bump
    {"_detail", "0", false},       // Detail: Quake's look has no grain inside a block
};

struct CategoryInfo
{
    const char* key; // vr_retro_<key>
    const char* label;
    const char* help;
};

constexpr CategoryInfo categoryInfo[categoryCount] = {
    {"world", "World", "The map's walls, floors, ceilings and liquids."},
    {"brush", "Brush Entities", "Doors, lifts, buttons, moving walls: the map's moving parts."},
    {"items", "Item Pickups", "The ammo and health boxes, armour, keys, powerups and backpacks."},
    {"props", "Props and Debris", "Crates, explosive boxes, rocks, bricks, planks, shell casings, lanterns."},
    {"gibs", "Gibs", "Gibs and heads."},
    {"smallgibs", "Small Gibs", "The small gibs hits tear out (Gore > Small Gibs)."},
    {"weapons", "Weapons in the World", "Weapons lying about: dropped, thrown, to pick up."},
    {"held", "Held Weapons", "The weapons in your hands and holsters."},
    {"monsters", "Monsters", "Monsters, their corpses, other players."},
    {"hands", "Your Hands", "Your hands (the most detailed textures of you)."},
    {"arms", "Your Arms", "Your body's arms, from the shoulders to the wrists."},
    {"torso", "Your Torso", "Your body's torso, shoulders and neck."},
    {"legs", "Your Legs", "Your body's legs and feet."},
    {"gear", "Your Gear", "The wrist gadget, the flashlight, pauldrons, pouches, leg holsters."},
    {"decals", "Decals", "Blood, scorch marks and bullet chips on the world. Blocks in world units."},
    {"particles", "Particles", "Blood, smoke, sparks, fire and splashes (blocks in world units); Quake's own dots become squares."},
    {"sprites", "Sprites", "Explosions, bubbles and other .spr sprites."},
    {"other", "Other Models", "Everything else drawn with a texture: projectiles, torches and flames, ..."},
};

za::String names[categoryCount * paramCount];
cvar_t cvars[categoryCount * paramCount];

constexpr Category bodySplit[] = {Category::Hands, Category::Arms, Category::Torso, Category::Legs, Category::Gear};

// The block the shaders read (vr_retro.h's RetroUBO, std140).
struct Block
{
    float info[4];                          // x on, y the palette's table is there
    za::U32 palette[256];                   // RGB8 packed (d_8to24table)
    float sets[QVR_RETRO_MAX_SETS * 3][4]; // set s at s * 3: P0, P1, P2
};
static_assert(sizeof(Block) == 16 + 1024 + QVR_RETRO_MAX_SETS * 48, "RetroUBO's layout");
static_assert(QVR_RETRO_MAX_SETS * 3 == 192, "RetroSets[192] in vr_retro.h");
Block block;

[[nodiscard]] float value(Category c, Param p)
{
    return cvarOf(c, p).value;
}

[[nodiscard]] bool on()
{
    return vr_retro.value != 0.f && vr_retro_ab.value == 0.f;
}

[[nodiscard]] int setOf(Category c)
{
    return static_cast<int>(c) + 1;
}

// ---- Overrides (vr_retro.hpp): per model and per world texture, on top of the category's settings

// An override: each setting inherited, replaced or multiplied.
struct Override
{
    OverrideMode mode[paramCount]{};
    float value[paramCount]{};
};

// By key ("model:<name>", "texture:<name>", lower case). The user's entry for a key replaces the shipped one whole.
// Every change bumps `generation`: the pointers handed out below (into these maps) and the sets made from them are
// good until then.
using OverrideMap = ankerl::unordered_dense::map<za::String, Override>;
OverrideMap shipped, user;
unsigned generation = 1;
za::String loadedGamedir; // the game folder the files were read from ("": not yet)

[[nodiscard]] za::String lower(za::StringView s)
{
    za::String out{s};
    for(za::SizeT i = 0; i < out.size(); i++)
    {
        out[i] = static_cast<char>(tolower(static_cast<unsigned char>(out[i])));
    }
    return out;
}

// A world texture's name as the overrides know it: an animation's frames ("+0button", "+1button") and alternate frames
// ("+abutton") all the texture's ("button").
[[nodiscard]] za::String textureKeyName(const char* name)
{
    if(name[0] == '+' && name[1] != '\0')
    {
        name += 2;
    }
    return lower(name);
}

[[nodiscard]] za::String keyOf(bool texture, za::StringView name)
{
    return za::String{texture ? "texture:" : "model:"} + lower(name);
}

[[nodiscard]] const Override* effective(const za::String& key)
{
    if(auto it = user.find(key); it != user.end())
    {
        return &it->second;
    }
    if(auto it = shipped.find(key); it != shipped.end())
    {
        return &it->second;
    }
    return nullptr;
}

[[nodiscard]] za::String filePath(bool shippedFile)
{
    return za::String{com_gamedir} + (shippedFile ? "/retro_overrides_default.txt" : "/retro_overrides.txt");
}

[[nodiscard]] int paramByName(za::StringView name)
{
    for(int p = 0; p < paramCount; p++)
    {
        const char* s = paramInfo[p].suffix;
        const za::StringView n{s[0] == '_' ? s + 1 : "on"};
        if(n == name)
        {
            return p;
        }
    }
    return -1;
}

[[nodiscard]] const char* paramName(int p)
{
    const char* s = paramInfo[p].suffix;
    return s[0] == '_' ? s + 1 : "on";
}

// The file's lines: "model <name> <setting> <op><value> ..." or "texture <name> ...", op '=' replaces, '*'
// multiplies (a bare number replaces); '#' starts a comment.
void readFile(const char* path, OverrideMap& into)
{
    za::String text;
    if(!files::readText(path, text))
    {
        return;
    }
    int lineNo = 0;
    files::forLines(text, [&](za::StringView line) {
        lineNo++;
        if(const za::SizeT hash = line.find('#'); hash != za::StringView::nPos)
        {
            line = line.substrByPosLen(0, hash);
        }
        if(const za::SizeT slashes = line.find("//"); slashes != za::StringView::nPos)
        {
            line = line.substrByPosLen(0, slashes);
        }
        files::Words words{line};
        za::String kind, name;
        if(!words.next(kind))
        {
            return;
        }
        if((kind != "model" && kind != "texture") || !words.next(name))
        {
            Con_Printf("%s:%d: expected \"model <name>\" or \"texture <name>\"\n", path, lineNo);
            return;
        }
        Override o;
        za::String setting, op;
        while(words.next(setting) && words.next(op))
        {
            const int p = paramByName(setting);
            if(p < 0 || op.empty())
            {
                Con_Printf("%s:%d: no setting \"%s\"\n", path, lineNo, setting.cStr());
                continue;
            }
            const char first = op[0];
            const char* number = first == '=' || first == '*' ? op.cStr() + 1 : op.cStr();
            o.mode[p] = first == '*' ? OverrideMode::Multiply : OverrideMode::Replace;
            o.value[p] = static_cast<float>(atof(number));
        }
        into[keyOf(kind == "texture", kind == "texture" ? za::StringView{textureKeyName(name.cStr())} : za::StringView{name})] = o;
    });
}

void loadFiles()
{
    shipped.clear();
    user.clear();
    readFile(filePath(true).cStr(), shipped);
    readFile(filePath(false).cStr(), user);
    loadedGamedir = com_gamedir;
    generation++;
}

void ensureLoaded()
{
    if(loadedGamedir != com_gamedir)
    {
        loadFiles();
    }
}

void saveUser()
{
    za::String text = "# retro_overrides.txt -- your retro textures overrides (Graphics > Retro Textures > Pick), written by\n"
                      "# the game. Each line: model <name> | texture <name>, then <setting> =<value> (replaces the kind's\n"
                      "# setting) or *<value> (multiplies it). Your line for a model or texture replaces the shipped one\n"
                      "# (retro_overrides_default.txt) whole. Settings: on snap block units average soft fade palette dither\n"
                      "# dither_scale bump detail.\n";
    for(const auto* entry : qza::sortedByKey(user))
    {
        const za::String& key = entry->first;
        const za::SizeT colon = key.find(':');
        text += za::String{za::StringView{key}.substrByPosLen(0, colon)} + " " +
                za::String{za::StringView{key}.substrByPosLen(colon + 1, key.size() - colon - 1)};
        for(int p = 0; p < paramCount; p++)
        {
            if(entry->second.mode[p] != OverrideMode::Inherit)
            {
                text += va(" %s %c%g", paramName(p), entry->second.mode[p] == OverrideMode::Multiply ? '*' : '=',
                    entry->second.value[p]);
            }
        }
        text += "\n";
    }
    if(!files::writeText(filePath(false).cStr(), text))
    {
        Con_Printf("retro textures: couldn't write %s\n", filePath(false).cStr());
    }
}

// Overrides looked up by a model's or texture's pointer (again when its name or the overrides changed).
struct CachedOverride
{
    za::String name;
    const Override* o{nullptr};
    unsigned generation{0};
};
ankerl::unordered_dense::map<const void*, CachedOverride> overrideCache;

[[nodiscard]] const Override* overrideFor(const void* ptr, bool texture, const char* name)
{
    ensureLoaded();
    CachedOverride& c = overrideCache[ptr];
    if(c.generation != generation || c.name != name)
    {
        c.name = name;
        c.generation = generation;
        c.o = effective(keyOf(texture, texture ? za::StringView{textureKeyName(name)} : za::StringView{name}));
    }
    return c.o;
}

// A setting for category c with a model's and a texture's overrides on top, in that order.
[[nodiscard]] float resolve(Category c, const Override* m, const Override* t, Param p)
{
    const int i = static_cast<int>(p);
    float v = value(c, p);
    for(const Override* o : {m, t})
    {
        if(!o)
        {
            continue;
        }
        if(o->mode[i] == OverrideMode::Replace)
        {
            v = o->value[i];
        }
        else if(o->mode[i] == OverrideMode::Multiply)
        {
            v *= o->value[i];
        }
    }
    return v;
}

void fillSet(int set, Category c, const Override* m, const Override* t)
{
    const auto r = [&](Param p) { return resolve(c, m, t, p); };
    float* p0 = block.sets[set * 3];
    float* p1 = block.sets[set * 3 + 1];
    float* p2 = block.sets[set * 3 + 2];
    p0[0] = za::clamp(r(Param::Block), 0.0625f, 64.f);
    p0[1] = za::clamp(r(Param::Soft), 0.f, 8.f);
    p0[2] = r(Param::Fade) < 0.f ? -1.f : za::clamp(r(Param::Fade), 0.05f, 16.f); // below 0: never smooth (Never)
    p0[3] = r(Param::Snap) != 0.f ? 1.f : 0.f;
    p1[0] = za::clamp(r(Param::Palette), 0.f, 1.f);
    p1[1] = za::clamp(r(Param::Dither), 0.f, 4.f);
    p1[2] = za::clamp(r(Param::DitherScale), 1.f, 64.f);
    p1[3] = za::clamp(r(Param::Bump), 0.f, 1.f);
    p2[0] = r(Param::Average) != 0.f ? 1.f : 0.f;
    p2[1] = !supports(c, Param::Units) || r(Param::Units) != 0.f ? 1.f : 0.f; // decals, particles: world units
    p2[2] = za::clamp(r(Param::Detail), 0.f, 1.f);
    p2[3] = r(Param::On) != 0.f ? 0.f : 1.f; // off: a body part's set in a run (setRun) drawn as it was
}

// The sets made for overrides (after the categories'), each a category with a model's and a texture's overrides;
// made as draws need them, kept until the overrides change.
struct Combo
{
    Category c;
    const Override* m;
    const Override* t;
};
za::Vector<Combo> combos;
unsigned combosGeneration = 0;
bool combosFull = false; // said once a map

constexpr int firstComboSet = categoryCount + 1;

// The block's last upload (bindForDraw binds it again).
GLuint uploadedBuffer = 0;
GLintptr uploadedOffset = 0;

void upload()
{
    GLuint buf;
    GLbyte* ofs;
    GL_Upload(GL_UNIFORM_BUFFER, &block, sizeof(block), &buf, &ofs);
    uploadedBuffer = buf;
    uploadedOffset = reinterpret_cast<GLintptr>(ofs);
    GL_BindBufferRange(GL_UNIFORM_BUFFER, QVR_RETRO_UBO_BINDING, buf, uploadedOffset, static_cast<GLsizeiptr>(sizeof(block)));
}

void sayFull()
{
    if(!combosFull)
    {
        combosFull = true;
        Con_Printf("retro textures: more than %d overrides in sight; the rest drawn with their kind's settings\n",
            QVR_RETRO_MAX_SETS - firstComboSet);
    }
}

// The set for category c with those overrides (0: none, off).
[[nodiscard]] int setFor(Category c, const Override* m, const Override* t)
{
    if(!on() || resolve(c, m, t, Param::On) == 0.f)
    {
        return 0;
    }
    if(!m && !t)
    {
        return setOf(c);
    }
    if(combosGeneration != generation)
    {
        combos.clear();
        combosGeneration = generation;
    }
    for(za::SizeT i = 0; i < combos.size(); i++)
    {
        if(combos[i].c == c && combos[i].m == m && combos[i].t == t)
        {
            return firstComboSet + static_cast<int>(i);
        }
    }
    if(firstComboSet + static_cast<int>(combos.size()) >= QVR_RETRO_MAX_SETS)
    {
        sayFull();
        return setOf(c);
    }
    combos.pushBack({c, m, t});
    const int set = firstComboSet + static_cast<int>(combos.size()) - 1;
    fillSet(set, c, m, t);
    upload(); // the draws still to come this view read it (bound again)
    return set;
}

// The sets of categories first .. first + n - 1 with a model's override m, one after another (your body's parts: one
// draw, each vertex adding its part to the first's set); without an override, or past the sets' end, the categories'
// own (in the same order). A set whose On is off is marked so (fillSet): that part is drawn as it was.
[[nodiscard]] int setRun(Category first, int n, const Override* m)
{
    if(!m)
    {
        return setOf(first);
    }
    if(combosGeneration != generation)
    {
        combos.clear();
        combosGeneration = generation;
    }
    const int f = static_cast<int>(first);
    for(za::SizeT i = 0; i + static_cast<za::SizeT>(n) <= combos.size(); i++)
    {
        bool match = true;
        for(int k = 0; k < n && match; k++)
        {
            const Combo& co = combos[i + static_cast<za::SizeT>(k)];
            match = co.c == static_cast<Category>(f + k) && co.m == m && co.t == nullptr;
        }
        if(match)
        {
            return firstComboSet + static_cast<int>(i);
        }
    }
    if(firstComboSet + static_cast<int>(combos.size()) + n > QVR_RETRO_MAX_SETS)
    {
        sayFull();
        return setOf(first);
    }
    const int set = firstComboSet + static_cast<int>(combos.size());
    for(int k = 0; k < n; k++)
    {
        combos.pushBack({static_cast<Category>(f + k), m, nullptr});
        fillSet(set + k, static_cast<Category>(f + k), m, nullptr);
    }
    upload();
    return set;
}

// ---- What a model is (its name; Category's comments)

[[nodiscard]] bool startsWith(const char* s, const char* prefix)
{
    return ZA_STRNCMP(s, prefix, ZA_STRLEN(prefix)) == 0;
}

[[nodiscard]] const char* fileOf(const char* name)
{
    const char* f = name;
    for(const char* p = name; *p; p++)
    {
        if(*p == '/')
        {
            f = p + 1;
        }
    }
    return f;
}

template <za::SizeT N>
[[nodiscard]] bool anyPrefix(const char* f, const char* const (&prefixes)[N])
{
    for(const char* p : prefixes)
    {
        if(startsWith(f, p))
        {
            return true;
        }
    }
    return false;
}

constexpr const char* gibPrefixes[] = {"gib1.", "gib2.", "gib3.", "gib_brain", "zom_gib.", "statgib", "h_"};
constexpr const char* handPrefixes[] = {"hand", "finger_"};
constexpr const char* gearPrefixes[] = {"vrgadget", "vrpauldron", "vrpouch", "legholster", "vrflashlight"};
constexpr const char* propPrefixes[] = {"vr_crate", "vr_rock", "vr_brick", "vr_plank", "vr_shell", "vrtorch.", "lantern",
    "candle", "barrel"};
// Quake's monsters, the mission packs' (hipnotic: scorpions, gremlins, the armagon; rogue: mummies, eels, lava men,
// dragons) and the player's model (other players, statues)
constexpr const char* monsterFiles[] = {"soldier.", "dog.", "ogre.", "knight.", "hknight.", "wizard.", "demon.",
    "shambler.", "zombie.", "shalrath.", "enforcer.", "fish.", "tarbaby.", "boss.", "oldone.", "player.", "scor.",
    "grem.", "armabody.", "armalegs.", "mummy.", "eel.", "lavaman.", "dragon.", "ogre_", "sword.", "frogman"};
constexpr const char* itemFiles[] = {"armor.", "backpack.", "w_s_key.", "w_g_key.", "m_s_key.", "m_g_key.", "b_s_key.",
    "b_g_key.", "invulner.", "suit.", "invisibl.", "quaddama.", "end1.", "end2.", "end3.", "end4.", "empathy.",
    "wetsuit.", "shield.", "antigrav."};

// A model's category by its name alone (entities that move it elsewhere: categoryOf).
[[nodiscard]] Category modelCategory(const qmodel_t* m)
{
    const char* f = fileOf(m->name);
    if(m->type == mod_brush)
    {
        if(m->name[0] == '*')
        {
            return Category::Brush;
        }
        if(startsWith(f, "b_explob") || startsWith(f, "b_exbox"))
        {
            return Category::Props;
        }
        return startsWith(f, "b_") ? Category::Items : Category::Props;
    }
    if(m->type == mod_sprite)
    {
        return Category::Sprites;
    }
    if(m->type != mod_alias)
    {
        return Category::Other;
    }
    if(anyPrefix(f, gibPrefixes))
    {
        return Category::Gibs;
    }
    if(startsWith(f, "v_") || startsWith(f, "g_"))
    {
        return Category::Weapons;
    }
    if(startsWith(f, "vrbody"))
    {
        return Category::Torso; // (by part: bodyParts)
    }
    if(anyPrefix(f, handPrefixes))
    {
        return Category::Hands;
    }
    if(anyPrefix(f, gearPrefixes))
    {
        return Category::Gear;
    }
    if(anyPrefix(f, propPrefixes))
    {
        return Category::Props;
    }
    if(anyPrefix(f, monsterFiles))
    {
        return Category::Monsters;
    }
    if((m->flags & EF_ROTATE) || anyPrefix(f, itemFiles))
    {
        return Category::Items;
    }
    return Category::Other;
}

// An entity's category: its model's, but a weapon in your hands or holsters is Held, anything else of yours that is
// not your body or hands your Gear; a gib scaled down (the small gibs: vr_smallgibs.qc) a small gib.
[[nodiscard]] Category categoryOf(const entity_t* e)
{
    if(e == &cl_entities[0])
    {
        return Category::World;
    }
    const Category c = modelCategory(e->model);
    if(e->model->type == mod_alias && view::find(e) != nullptr)
    {
        if(c == Category::Weapons || c == Category::Held)
        {
            return Category::Held;
        }
        return c == Category::Torso || c == Category::Hands ? c : Category::Gear;
    }
    if(c == Category::Gibs && VR_EntityScale(e) < 0.8f)
    {
        return Category::SmallGibs;
    }
    return c;
}

// The world's surfaces and brush submodels' ("*N", the map's own) take overrides by texture only; other models by name.
[[nodiscard]] const Override* modelOverride(const entity_t* e)
{
    if(e == &cl_entities[0] || !e->model || e->model->name[0] == '*')
    {
        return nullptr;
    }
    return overrideFor(e->model, false, e->model->name);
}

// Your body's bones by their names (make_vrbody.py's, vr_avatar.cpp's jointNames): the hands' and wrists', the arms',
// the legs'; the rest (pelvis, spine, chest, neck, head, clavicles) the torso's.
constexpr const char* handBones[] = {"hand_", "wrist_", "finger", "thumb"};
constexpr const char* armBones[] = {"upperarm_", "forearm_", "foretwist"};
constexpr const char* legBones[] = {"thigh_", "calf_", "foot_", "toe"};

[[nodiscard]] Category bonePart(const char* name)
{
    if(anyPrefix(name, handBones))
    {
        return Category::Hands;
    }
    if(anyPrefix(name, armBones))
    {
        return Category::Arms;
    }
    return anyPrefix(name, legBones) ? Category::Legs : Category::Torso;
}

// Your body's parts (progs/vrbody*, skinned): each bone's part (Hands .. Legs: 0 .. 3) as two bits, as InstanceData's
// RetroPart has them: xy the low bits of bones 0..23 and 24..47, zw the high bits, as whole numbers (a vertex's part is
// its heaviest bone's). False (all 0) for any other model, or a body without bones: drawn whole by its category.
bool bodyParts(const qmodel_t* model, float out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0.f;
    if(!model || model->type != mod_alias || !startsWith(fileOf(model->name), "vrbody"))
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_IQM || hdr->numbones <= 0 || !hdr->boneinfo)
    {
        return false;
    }
    const auto* bones = reinterpret_cast<const boneinfo_t*>(reinterpret_cast<const byte*>(hdr) + hdr->boneinfo);
    za::U32 bits[4]{0u, 0u, 0u, 0u};
    for(int i = 0; i < za::min(hdr->numbones, 48); i++)
    {
        const int part = static_cast<int>(bonePart(bones[i].name)) - static_cast<int>(Category::Hands);
        if(part & 1)
        {
            bits[i / 24] |= 1u << (i % 24);
        }
        if(part & 2)
        {
            bits[2 + i / 24] |= 1u << (i % 24);
        }
    }
    for(int k = 0; k < 4; k++)
    {
        out[k] = static_cast<float>(bits[k]);
    }
    return true;
}

[[nodiscard]] bool isBody(const entity_t* e)
{
    float unused[4];
    return categoryOf(e) == Category::Torso && bodyParts(e->model, unused);
}

// Your body's sets: its parts' run (setRun), 0 when every part is off.
[[nodiscard]] int bodySet(const Override* m)
{
    bool any = false;
    for(int k = 0; k < bodyPartCount; k++)
    {
        any = any || resolve(static_cast<Category>(static_cast<int>(Category::Hands) + k), m, nullptr, Param::On) != 0.f;
    }
    return any ? setRun(Category::Hands, bodyPartCount, m) : 0;
}

// The set an entity's textures are drawn with (0: none; your body: the first of its parts' sets).
[[nodiscard]] int entitySet(const entity_t* e)
{
    if(!e || !e->model || !on())
    {
        return 0;
    }
    if(isBody(e))
    {
        return bodySet(modelOverride(e));
    }
    return setFor(categoryOf(e), modelOverride(e), nullptr);
}

// The skin's own size in Quake texels: Quake's .mdl's; our own MD3 and IQM models' (their textures painted at about four
// times a Quake skin's density) a quarter of their texture's, so that a block is about a Quake texel there too.
void skinSize(const aliashdr_t* hdr, int skinnum, float& w, float& h)
{
    w = h = 0.f;
    if(!hdr)
    {
        return;
    }
    if(hdr->poseverttype == aliashdr_t::PV_QUAKE1 && hdr->skinwidth > 0 && hdr->skinheight > 0)
    {
        w = static_cast<float>(hdr->skinwidth);
        h = static_cast<float>(hdr->skinheight);
        return;
    }
    const int skin = skinnum >= 0 && skinnum < hdr->numskins ? skinnum : 0;
    if(const gltexture_t* g = hdr->gltextures[skin][0])
    {
        w = za::max(1.f, static_cast<float>(g->width) * 0.25f);
        h = za::max(1.f, static_cast<float>(g->height) * 0.25f);
    }
}

// ---- Picking what to override: what a ray from the main hand or the head meets first

// The first surface of brush model `m` the segment from `start` to `end` (the model's space) crosses where its texture's
// extents hold the crossing (vr_physsound.cpp's surfaceAlong: RecursiveLightPoint's walk, without the lightmaps).
const msurface_t* surfaceAlong(const qmodel_t* m, const mnode_t* node, const glm::vec3& start, const glm::vec3& end, int depth)
{
    while(node && node->contents >= 0 && depth < 256)
    {
        const mplane_t* p = node->plane;
        const glm::vec3 n{p->normal[0], p->normal[1], p->normal[2]};
        const float front = glm::dot(start, n) - p->dist;
        const float back = glm::dot(end, n) - p->dist;
        if((back < 0.f) == (front < 0.f))
        {
            node = node->children[front < 0.f];
            continue;
        }
        const glm::vec3 mid = start + (end - start) * (front / (front - back));
        if(const msurface_t* s = surfaceAlong(m, node->children[front < 0.f], start, mid, depth + 1))
        {
            return s;
        }
        const msurface_t* surf = m->surfaces + node->firstsurface;
        for(unsigned int i = 0; i < node->numsurfaces; i++, surf++)
        {
            if(!surf->texinfo || surf->texinfo->texnum < 0 || surf->texinfo->texnum >= m->numtextures || !m->textures[surf->texinfo->texnum])
            {
                continue;
            }
            const float* v0 = surf->texinfo->vecs[0];
            const float* v1 = surf->texinfo->vecs[1];
            const int ds = static_cast<int>(mid.x * v0[0] + mid.y * v0[1] + mid.z * v0[2] + v0[3]) - surf->texturemins[0];
            const int dt = static_cast<int>(mid.x * v1[0] + mid.y * v1[1] + mid.z * v1[2] + v1[3]) - surf->texturemins[1];
            if(ds >= 0 && dt >= 0 && ds <= surf->extents[0] && dt <= surf->extents[1])
            {
                return surf;
            }
        }
        node = node->children[front >= 0.f];
        return surfaceAlong(m, node, mid, end, depth + 1);
    }
    return nullptr;
}

// Where the ray from `start` along `dir` crosses surface s's plane (in the model's space), its distance.
[[nodiscard]] float planeDistance(const msurface_t* s, const glm::vec3& start, const glm::vec3& dir)
{
    const glm::vec3 n{s->plane->normal[0], s->plane->normal[1], s->plane->normal[2]};
    const float along = glm::dot(n, dir);
    return za::fabs(along) > 1e-6f ? (s->plane->dist - glm::dot(n, start)) / along : 0.f;
}

struct Hit
{
    bool any{false};
    float distance{1e30f};
    glm::vec3 point{0.f};
    za::String model;   // "" for the world, or the model's name ("*N": a brush submodel's)
    za::String texture; // a brush surface's texture (key name), "" for an alias model
    Category modelCategory{Category::World};
    Category textureCategory{Category::World}; // the world's, or the brush entity's
    glm::vec3 boxMin{0.f}, boxMax{0.f};
    bool box{false};
};

// own: your view entities (hands, body, held weapons) too; a head's look leaves them out (the holsters, the body round it).
[[nodiscard]] Hit traceScene(const glm::vec3& start, const glm::vec3& dir, bool own)
{
    constexpr float reach = 8192.f;
    Hit hit;
    if(cl.worldmodel && cl.worldmodel->nodes)
    {
        const qmodel_t* w = cl.worldmodel;
        if(const msurface_t* s = surfaceAlong(w, w->nodes + w->hulls[0].firstclipnode, start, start + dir * reach, 0))
        {
            hit.any = true;
            hit.distance = planeDistance(s, start, dir);
            hit.texture = textureKeyName(w->textures[s->texinfo->texnum]->name);
            hit.modelCategory = hit.textureCategory = Category::World;
        }
    }
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const entity_t* e = cl_visedicts[i];
        if(!e || !e->model || e == &cl_entities[0])
        {
            continue;
        }
        const glm::vec3 origin{e->origin[0], e->origin[1], e->origin[2]};
        if(e->model->type == mod_brush)
        {
            const qmodel_t* m = e->model;
            const glm::vec3 local = start - origin; // (turned brush entities: as if not turned)
            const msurface_t* s = m->nodes ? surfaceAlong(m, m->nodes + m->hulls[0].firstclipnode, local, local + dir * reach, 0) : nullptr;
            const float d = s ? planeDistance(s, local, dir) : 0.f;
            if(s && d > 0.f && d < hit.distance)
            {
                hit = {};
                hit.any = true;
                hit.distance = d;
                hit.model = m->name;
                hit.texture = textureKeyName(m->textures[s->texinfo->texnum]->name);
                hit.modelCategory = categoryOf(e);
                hit.textureCategory = m->name[0] == '*' ? Category::Brush : hit.modelCategory;
            }
            continue;
        }
        if(e->model->type != mod_alias || (!own && view::find(e) != nullptr))
        {
            continue;
        }
        // its bounds over its frames: turned about the vertical only, the tighter ones for that (R_CullModelForEntity's)
        const float scale = VR_EntityScale(e);
        const bool yawOnly = e->angles[0] == 0.f && e->angles[2] == 0.f;
        const float* mins = yawOnly ? e->model->ymins : e->model->rmins;
        const float* maxs = yawOnly ? e->model->ymaxs : e->model->rmaxs;
        const glm::vec3 lo = origin + glm::vec3{mins[0], mins[1], mins[2]} * scale;
        const glm::vec3 hi = origin + glm::vec3{maxs[0], maxs[1], maxs[2]} * scale;
        float enter = -1e30f, leave = 1e30f;
        for(int a = 0; a < 3; a++)
        {
            if(za::fabs(dir[a]) < 1e-6f)
            {
                if(start[a] < lo[a] || start[a] > hi[a])
                {
                    enter = 1e30f;
                }
                continue;
            }
            float t0 = (lo[a] - start[a]) / dir[a], t1 = (hi[a] - start[a]) / dir[a];
            if(t0 > t1)
            {
                const float tmp = t0;
                t0 = t1;
                t1 = tmp;
            }
            enter = za::max(enter, t0);
            leave = za::min(leave, t1);
        }
        // Not what the ray starts in (the hand's own weapon, your body round the head).
        if(enter > 0.f && enter <= leave && enter < hit.distance)
        {
            hit = {};
            hit.any = true;
            hit.distance = enter;
            hit.model = e->model->name;
            hit.modelCategory = hit.textureCategory = categoryOf(e);
            hit.box = true;
            hit.boxMin = lo;
            hit.boxMax = hi;
        }
    }
    hit.point = start + dir * (hit.any ? hit.distance : 0.f);
    return hit;
}

// The ray: the main hand's aim (as a gun in it points), or the head's.
[[nodiscard]] bool pickRay(bool fromHand, glm::vec3& start, glm::vec3& dir)
{
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return false;
    }
    const glm::vec3 angles = fromHand ? s.rot[1] : s.headAngles;
    vec3_t a{angles.x, angles.y, angles.z}, fwd, right, up;
    AngleVectors(a, fwd, right, up);
    start = fromHand ? s.pos[1] : s.head;
    dir = glm::normalize(glm::vec3{fwd[0], fwd[1], fwd[2]});
    return true;
}

// ---- The editor (Graphics > Retro Textures > Override): the picked target's override in cvars the page shows

struct Target
{
    bool valid{false};
    za::String model, texture; // what it can be keyed by ("" none)
    Category modelCategory{Category::World}, textureCategory{Category::World};
};
Target target;

za::String editNames[paramCount * 2];
cvar_t editValues[paramCount];
cvar_t editModes[paramCount];
cvar_t editKind{"vr_retro_edit_kind", "0", CVAR_NONE}; // 0 the model, 1 the texture
bool editorLoading = false;
bool userDirty = false;
double userDirtyAt = 0.0;

[[nodiscard]] bool editingTexture()
{
    return target.texture.size() > 0 && (target.model.empty() || editKind.value != 0.f);
}

[[nodiscard]] za::String targetKey()
{
    return editingTexture() ? keyOf(true, target.texture) : keyOf(false, target.model);
}

[[nodiscard]] Category targetCategory()
{
    return editingTexture() ? target.textureCategory : target.modelCategory;
}

// The editor's cvars set from the target's override (its own values; inherited ones show the kind's).
void loadEditor()
{
    if(!target.valid)
    {
        return;
    }
    ensureLoaded();
    const Override* o = effective(targetKey());
    editorLoading = true;
    for(int p = 0; p < paramCount; p++)
    {
        const OverrideMode mode = o ? o->mode[p] : OverrideMode::Inherit;
        Cvar_SetValueQuick(&editModes[p], static_cast<float>(static_cast<int>(mode)));
        Cvar_SetValueQuick(&editValues[p], mode == OverrideMode::Inherit ? value(targetCategory(), static_cast<Param>(p)) : o->value[p]);
    }
    editorLoading = false;
}

void changed()
{
    generation++;
    userDirty = true;
    userDirtyAt = realtime;
}

// An editor cvar changed: the target's override in the user's file is the editor's now.
void onEditChanged(cvar_t*)
{
    if(editorLoading || !target.valid)
    {
        return;
    }
    Override o;
    for(int p = 0; p < paramCount; p++)
    {
        o.mode[p] = static_cast<OverrideMode>(za::clamp(static_cast<int>(editModes[p].value), 0, 2));
        o.value[p] = editValues[p].value;
    }
    ensureLoaded();
    user[targetKey()] = o;
    changed();
}

void onEditKindChanged(cvar_t*)
{
    loadEditor();
}

void lockTarget(const Hit& h)
{
    target = {};
    target.valid = h.any;
    if(!h.any)
    {
        return;
    }
    if(h.model.size() > 0 && h.model[0] != '*')
    {
        target.model = h.model;
    }
    target.texture = h.texture;
    target.modelCategory = h.modelCategory;
    target.textureCategory = h.textureCategory;
    target.valid = target.model.size() > 0 || target.texture.size() > 0;
    editorLoading = true;
    Cvar_SetValueQuick(&editKind, target.model.empty() ? 1.f : 0.f);
    editorLoading = false;
    loadEditor();
}

// A pick under way (vr_retro_pick with a delay): the ray traced each frame, what it meets outlined; the target taken
// when the time is up.
struct Pick
{
    bool armed{false};
    bool fromHand{true};
    double at{0.0};
};
Pick pick;

void closeMenu()
{
    if(key_dest == key_menu)
    {
        IN_Activate();
        key_dest = key_game;
        m_state = m_none;
    }
}

void outline(const Hit& h, const glm::vec3& start)
{
    const glm::vec4 yellow{1.f, 0.85f, 0.2f, 0.9f};
    lines::line(start, h.point, 0.15f, glm::vec4{1.f, 0.85f, 0.2f, 0.3f}, yellow);
    if(h.box)
    {
        const glm::vec3 a = h.boxMin, b = h.boxMax;
        const glm::vec3 c[8] = {{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, b.y, a.z}, {a.x, b.y, a.z},
                                {a.x, a.y, b.z}, {b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}};
        constexpr int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for(const auto& e : edges)
        {
            lines::line(c[e[0]], c[e[1]], 0.3f, yellow, yellow);
        }
    }
    else if(h.any)
    {
        lines::point(h.point, 2.f, yellow);
    }
}

[[nodiscard]] za::String hitLabel(const Hit& h)
{
    if(!h.any)
    {
        return "nothing";
    }
    if(h.box || (h.model.size() > 0 && h.model[0] != '*'))
    {
        return h.model + " (" + categoryInfo[static_cast<int>(h.modelCategory)].label + ")";
    }
    return "texture " + h.texture + " (" + categoryInfo[static_cast<int>(h.textureCategory)].label + ")";
}

void finishPick(const Hit& h)
{
    pick.armed = false;
    lockTarget(h);
    Con_Printf("retro textures: picked %s\n", hitLabel(h).cStr());
    menu::reopen(menu::retroOverridePage());
}

// vr_retro_pick [hand|head] [seconds]: what the main hand points at (or the head looks at) picked for the override
// page, at once or after that many seconds (the menu closed meanwhile, the target outlined), and the page opened.
void pick_f()
{
    const bool fromHand = Cmd_Argc() < 2 || q_strcasecmp(Cmd_Argv(1), "head") != 0;
    const float seconds = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : 0.f;
    pick.fromHand = fromHand;
    if(seconds > 0.f)
    {
        pick.armed = true;
        pick.at = realtime + seconds;
        closeMenu();
        return;
    }
    glm::vec3 start, dir;
    if(!pickRay(fromHand, start, dir))
    {
        Con_Printf("vr_retro_pick: no %s pose\n", fromHand ? "hand" : "head");
        return;
    }
    finishPick(traceScene(start, dir, fromHand));
}

// vr_retro_override <model|texture> <name> [<setting> <=value|*value> ...]: sets that override in your file (no
// settings: removes your entry, the shipped one applies again); vr_retro_override list: every override.
void override_f()
{
    ensureLoaded();
    if(Cmd_Argc() < 2 || q_strcasecmp(Cmd_Argv(1), "list") == 0)
    {
        for(const OverrideMap* map : {&shipped, &user})
        {
            for(const auto* entry : qza::sortedByKey(*map))
            {
                Con_Printf("%s %s", map == &user ? "yours  " : "shipped", entry->first.cStr());
                for(int p = 0; p < paramCount; p++)
                {
                    if(entry->second.mode[p] != OverrideMode::Inherit)
                    {
                        Con_Printf(" %s %c%g", paramName(p), entry->second.mode[p] == OverrideMode::Multiply ? '*' : '=',
                            entry->second.value[p]);
                    }
                }
                Con_Printf("\n");
            }
        }
        return;
    }
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_retro_override <model|texture> <name> [<setting> <=value|*value> ...] | list\n");
        return;
    }
    const bool texture = q_strcasecmp(Cmd_Argv(1), "texture") == 0;
    const za::String key = keyOf(texture, texture ? za::StringView{textureKeyName(Cmd_Argv(2))} : za::StringView{Cmd_Argv(2)});
    if(Cmd_Argc() < 4)
    {
        user.erase(key);
    }
    else
    {
        Override o;
        for(int a = 3; a + 1 < Cmd_Argc(); a += 2)
        {
            const int p = paramByName(Cmd_Argv(a));
            const char* v = Cmd_Argv(a + 1);
            if(p < 0)
            {
                Con_Printf("vr_retro_override: no setting \"%s\"\n", Cmd_Argv(a));
                continue;
            }
            o.mode[p] = v[0] == '*' ? OverrideMode::Multiply : OverrideMode::Replace;
            o.value[p] = static_cast<float>(atof(v[0] == '*' || v[0] == '=' ? v + 1 : v));
        }
        user[key] = o;
    }
    changed();
    loadEditor();
}

[[nodiscard]] bool inBodySplit(int c)
{
    for(Category b : bodySplit)
    {
        if(static_cast<int>(b) == c)
        {
            return true;
        }
    }
    return false;
}

// vr_retro_reset [category|body|all]: a category's settings (body: your hands', arms', torso's, legs' and gear's; all:
// every one's) back to their defaults.
void reset_f()
{
    const char* which = Cmd_Argc() > 1 ? Cmd_Argv(1) : "all";
    bool any = false;
    for(int c = 0; c < categoryCount; c++)
    {
        if(ZA_STRCMP(which, "all") != 0 && q_strcasecmp(which, categoryInfo[c].key) != 0 &&
            !(q_strcasecmp(which, "body") == 0 && inBodySplit(c)))
        {
            continue;
        }
        any = true;
        for(int p = 0; p < paramCount; p++)
        {
            cvar_t& var = cvars[c * paramCount + p];
            Cvar_SetQuick(&var, var.default_string);
        }
    }
    if(!any)
    {
        Con_Printf("vr_retro_reset: no category \"%s\" (all, body, or one of:", which);
        for(const CategoryInfo& ci : categoryInfo)
        {
            Con_Printf(" %s", ci.key);
        }
        Con_Printf(")\n");
    }
}

// vr_retro_list: the entities drawn now, each with its model, category, set and skin size (what the retro textures
// see), and the world's.
void list_f()
{
    Con_Printf("retro textures %s; world: %s, set %d\n", on() ? "on" : "off", cl.worldmodel ? cl.worldmodel->name : "-",
        entitySet(&cl_entities[0]));
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const entity_t* e = cl_visedicts[i];
        if(!e || !e->model)
        {
            continue;
        }
        float w = 0.f, h = 0.f;
        int tw = 0, th = 0;
        if(e->model->type == mod_alias)
        {
            const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e->model));
            skinSize(hdr, e->skinnum, w, h);
            const int skin = e->skinnum >= 0 && e->skinnum < hdr->numskins ? e->skinnum : 0;
            if(const gltexture_t* g = hdr->gltextures[skin][0])
            {
                tw = g->width;
                th = g->height;
            }
        }
        const Category c = categoryOf(e);
        Con_Printf("%3d %-28s %-20s set %d%s  skin %gx%g  texture %dx%d  scale %.2f\n", i, e->model->name,
            isBody(e) ? "Your Body (by part)" : categoryInfo[static_cast<int>(c)].label, entitySet(e), modelOverride(e) ? " (override)" : "", w, h, tw, th,
            VR_EntityScale(e));
    }
}

void reload_f()
{
    loadFiles();
    loadEditor();
    Con_Printf("retro textures: %d shipped and %d of your overrides\n", static_cast<int>(shipped.size()),
        static_cast<int>(user.size()));
}

// ---- All Categories (Graphics > Retro Textures > All Categories): one set of values (vr_retro_all_<setting>, On's
// vr_retro_all_on), which of them to apply (vr_retro_all_apply_<setting>) and to which categories
// (vr_retro_all_to_<key>). Applying copies the checked settings to the checked categories' cvars; overrides untouched.
za::String allNames[paramCount * 2 + categoryCount];
cvar_t allValues[paramCount];
cvar_t allApplies[paramCount];
cvar_t allTargets[categoryCount];
cvar_t allLive{"vr_retro_all_live", "0", CVAR_NONE}; // not archived: a config read at start-up never writes through
cvar_t allFrom{"vr_retro_all_from", "0", CVAR_ARCHIVE};
bool allCopying = false;       // copyFrom writing the values: not live
za::String allSummaryText;

// The checked settings (only setting `only`, if not -1) to the checked categories; how many cvars were written.
int applyAll(int only)
{
    int written = 0;
    for(int c = 0; c < categoryCount; c++)
    {
        if(allTargets[c].value == 0.f)
        {
            continue;
        }
        for(int p = 0; p < paramCount; p++)
        {
            if((only >= 0 && p != only) || allApplies[p].value == 0.f)
            {
                continue;
            }
            Cvar_SetQuick(&cvars[c * paramCount + p], allValues[p].string);
            written++;
        }
    }
    return written;
}

void copyFrom(int c)
{
    allCopying = true;
    for(int p = 0; p < paramCount; p++)
    {
        Cvar_SetQuick(&allValues[p], cvars[c * paramCount + p].string);
    }
    allCopying = false;
}

void onAllValueChanged(cvar_t* var)
{
    if(allCopying || allLive.value == 0.f)
    {
        return;
    }
    applyAll(static_cast<int>(var - allValues));
}

[[nodiscard]] int checkedCount(const cvar_t* vars, int n)
{
    int count = 0;
    for(int i = 0; i < n; i++)
    {
        count += vars[i].value != 0.f ? 1 : 0;
    }
    return count;
}

// vr_retro_all_apply: the checked settings of All Categories to the checked categories.
void allApply_f()
{
    const int written = applyAll(-1);
    Con_Printf("vr_retro_all_apply: %d settings to %d categories (%d values)\n", checkedCount(allApplies, paramCount),
        checkedCount(allTargets, categoryCount), written);
}

// vr_retro_all_copy [category]: that category's values (none: vr_retro_all_from's) into All Categories' values.
void allCopy_f()
{
    int c = za::clamp(static_cast<int>(allFrom.value), 0, categoryCount - 1);
    if(Cmd_Argc() > 1)
    {
        c = -1;
        for(int i = 0; i < categoryCount; i++)
        {
            if(q_strcasecmp(Cmd_Argv(1), categoryInfo[i].key) == 0)
            {
                c = i;
            }
        }
        if(c < 0)
        {
            Con_Printf("vr_retro_all_copy: no category \"%s\"\n", Cmd_Argv(1));
            return;
        }
    }
    copyFrom(c);
    Con_Printf("vr_retro_all_copy: the values of %s\n", categoryInfo[c].label);
}

void setAll(cvar_t* vars, int n, bool v)
{
    for(int i = 0; i < n; i++)
    {
        Cvar_SetValueQuick(&vars[i], v ? 1.f : 0.f);
    }
}

void registerAll()
{
    for(int p = 0; p < paramCount; p++)
    {
        allNames[p * 2] = za::String("vr_retro_all_") + paramName(p);
        allNames[p * 2 + 1] = za::String("vr_retro_all_apply_") + paramName(p);
        allValues[p].name = allNames[p * 2].cStr();
        allValues[p].string = paramInfo[p].def;
        allValues[p].flags = CVAR_ARCHIVE;
        allApplies[p].name = allNames[p * 2 + 1].cStr();
        allApplies[p].string = p == static_cast<int>(Param::On) ? "0" : "1"; // the categories' switches: yours
        allApplies[p].flags = CVAR_ARCHIVE;
        Cvar_RegisterVariable(&allValues[p]);
        Cvar_RegisterVariable(&allApplies[p]);
        Cvar_SetCallback(&allValues[p], onAllValueChanged);
    }
    for(int c = 0; c < categoryCount; c++)
    {
        allNames[paramCount * 2 + c] = za::String("vr_retro_all_to_") + categoryInfo[c].key;
        allTargets[c].name = allNames[paramCount * 2 + c].cStr();
        allTargets[c].string = "1";
        allTargets[c].flags = CVAR_ARCHIVE;
        Cvar_RegisterVariable(&allTargets[c]);
    }
    Cvar_RegisterVariable(&allLive);
    Cvar_RegisterVariable(&allFrom);
    Cmd_AddCommand("vr_retro_all_apply", allApply_f);
    Cmd_AddCommand("vr_retro_all_copy", allCopy_f);
}

} // namespace

cvar_s& allValue(Param p)
{
    return allValues[static_cast<int>(p)];
}

cvar_s& allApply(Param p)
{
    return allApplies[static_cast<int>(p)];
}

cvar_s& allTarget(Category c)
{
    return allTargets[static_cast<int>(c)];
}

cvar_s& allLiveCvar()
{
    return allLive;
}

cvar_s& allFromCvar()
{
    return allFrom;
}

void allApplyNow()
{
    allApply_f();
}

void allCopyChosen()
{
    copyFrom(za::clamp(static_cast<int>(allFrom.value), 0, categoryCount - 1));
}

void allCheckSettings(bool on)
{
    setAll(allApplies, paramCount, on);
}

void allCheckCategories(bool on)
{
    setAll(allTargets, categoryCount, on);
}

const char* allSummary()
{
    allSummaryText = za::String{va("%d settings checked, %d categories checked%s", checkedCount(allApplies, paramCount),
        checkedCount(allTargets, categoryCount), allLive.value != 0.f ? " (live)" : "")};
    return allSummaryText.cStr();
}

bool supports(Category c, Param p)
{
    const bool loose = c == Category::Decals || c == Category::Particles || c == Category::Sprites; // no bump maps
    switch(p)
    {
        case Param::Units: return c != Category::Decals && c != Category::Particles;
        case Param::Bump: return !loose;
        case Param::Detail:
            return c == Category::World || c == Category::Brush || c == Category::Items || c == Category::Props;
        default: return true;
    }
}

int categorySet(Category c)
{
    return setFor(c, nullptr, nullptr);
}

void bindForDraw(int lutUnit)
{
    if(uploadedBuffer != 0)
    {
        GL_BindBufferRange(GL_UNIFORM_BUFFER, QVR_RETRO_UBO_BINDING, uploadedBuffer, uploadedOffset,
            static_cast<GLsizeiptr>(sizeof(block)));
    }
    VR_RetroBind(lutUnit);
}

cvar_t& cvarOf(Category c, Param p)
{
    return cvars[static_cast<int>(c) * paramCount + static_cast<int>(p)];
}

const char* categoryLabel(Category c)
{
    return categoryInfo[static_cast<int>(c)].label;
}

const char* categoryHelp(Category c)
{
    return categoryInfo[static_cast<int>(c)].help;
}

const char* categoryKey(Category c)
{
    return categoryInfo[static_cast<int>(c)].key;
}

cvar_s& editValue(Param p)
{
    return editValues[static_cast<int>(p)];
}

cvar_s& editMode(Param p)
{
    return editModes[static_cast<int>(p)];
}

cvar_s& editKindCvar()
{
    return editKind;
}

bool hasTarget()
{
    return target.valid;
}

bool targetHasBoth()
{
    return target.valid && target.model.size() > 0 && target.texture.size() > 0;
}

const char* targetText()
{
    if(!target.valid)
    {
        return "Nothing picked yet: Pick (Point, 3 s) on the Retro Textures page, or vr_retro_pick.";
    }
    const bool shippedOne = shipped.find(targetKey()) != shipped.end();
    const bool yours = user.find(targetKey()) != user.end();
    return va("%s %s (%s)%s", editingTexture() ? "Texture" : "Model",
        editingTexture() ? target.texture.cStr() : target.model.cStr(), categoryLabel(targetCategory()),
        yours ? ": your override" : shippedOne ? ": the shipped override" : ": no override yet");
}

void useShipped()
{
    if(target.valid)
    {
        ensureLoaded();
        user.erase(targetKey());
        changed();
        loadEditor();
    }
}

void clearOverride()
{
    if(target.valid)
    {
        ensureLoaded();
        user[targetKey()] = Override{};
        changed();
        loadEditor();
    }
}

void saveNow()
{
    ensureLoaded();
    saveUser();
    userDirty = false;
}

void frame()
{
    if(userDirty && realtime - userDirtyAt > 1.0) // edits written a second after the last
    {
        saveNow();
    }
    if(!pick.armed)
    {
        return;
    }
    glm::vec3 start, dir;
    if(!pickRay(pick.fromHand, start, dir))
    {
        return;
    }
    const Hit h = traceScene(start, dir, pick.fromHand);
    if(realtime >= pick.at)
    {
        finishPick(h);
        return;
    }
    outline(h, start);
    const hands::State& s = hands::current();
    vec3_t a{0.f, s.headAngles.y, 0.f}, fwd, right, up;
    AngleVectors(a, fwd, right, up);
    const glm::vec3 at = s.head + glm::vec3{fwd[0], fwd[1], fwd[2]} * 28.f - glm::vec3{0.f, 0.f, 8.f};
    const za::String text = za::String{va("Retro override: %s in %d\n", pick.fromHand ? "point" : "look",
                                static_cast<int>(pick.at - realtime) + 1)} + hitLabel(h);
    text3d::queueOverlay(text, at, glm::vec3{0.f, s.headAngles.y, 0.f}, 0.04f);
}

void registerCvars()
{
    for(int c = 0; c < categoryCount; c++)
    {
        for(int p = 0; p < paramCount; p++)
        {
            const int i = c * paramCount + p;
            names[i] = za::String("vr_retro_") + categoryInfo[c].key + paramInfo[p].suffix;
            cvars[i].name = names[i].cStr();
            cvars[i].string = paramInfo[p].def;
            cvars[i].flags = CVAR_ARCHIVE;
            Cvar_RegisterVariable(&cvars[i]);
        }
    }
    for(int p = 0; p < paramCount; p++)
    {
        editNames[p * 2] = za::String("vr_retro_edit_") + paramName(p);
        editNames[p * 2 + 1] = editNames[p * 2] + "_mode";
        editValues[p].name = editNames[p * 2].cStr();
        editValues[p].string = paramInfo[p].def;
        editValues[p].flags = CVAR_NONE;
        editModes[p].name = editNames[p * 2 + 1].cStr();
        editModes[p].string = "0";
        editModes[p].flags = CVAR_NONE;
        Cvar_RegisterVariable(&editValues[p]);
        Cvar_RegisterVariable(&editModes[p]);
        Cvar_SetCallback(&editValues[p], onEditChanged);
        Cvar_SetCallback(&editModes[p], onEditChanged);
    }
    Cvar_RegisterVariable(&editKind);
    Cvar_SetCallback(&editKind, onEditKindChanged);
    Cmd_AddCommand("vr_retro_reset", reset_f);
    Cmd_AddCommand("vr_retro_list", list_f);
    Cmd_AddCommand("vr_retro_pick", pick_f);
    Cmd_AddCommand("vr_retro_override", override_f);
    Cmd_AddCommand("vr_retro_overrides_reload", reload_f);
    registerAll();
}

} // namespace qvr::retro

using namespace qvr;
using namespace qvr::retro;

// R_UploadFrameData: the categories' sets, the overrides' sets made so far and the palette for the shaders (uniform
// block 3).
extern "C" void VR_RetroUpload(void)
{
    block.info[0] = on() ? 1.f : 0.f;
    block.info[1] = gl_palette_lut != 0 ? 1.f : 0.f;
    block.info[2] = block.info[3] = 0.f;
    ZA_MEMCPY(block.palette, d_8to24table, sizeof(block.palette));
    for(int c = 0; c < categoryCount; c++)
    {
        fillSet(setOf(static_cast<Category>(c)), static_cast<Category>(c), nullptr, nullptr);
    }
    if(combosGeneration == generation)
    {
        for(za::SizeT i = 0; i < combos.size(); i++)
        {
            fillSet(firstComboSet + static_cast<int>(i), combos[i].c, combos[i].m, combos[i].t);
        }
    }
    upload();
}

// A world or model draw, its program in use: the palette's table (Ironwail's, made each frame by
// GLPalette_UpdateLookupTable) on the unit its shader reads it from.
extern "C" void VR_RetroBind(int unit)
{
    if(on())
    {
        GL_BindNative(static_cast<GLenum>(GL_TEXTURE0 + unit), GL_TEXTURE_3D, gl_palette_lut);
    }
}

// R_AddBModelCall: the texture's own size in Quake texels (the blocks' grid) and, where the texture has an override,
// 1 + the set for it on entity e (the batch's first; 1: off); 0: the instance's set.
extern "C" void VR_RetroCall(entity_t* e, const texture_t* t, float out[4])
{
    out[0] = t ? static_cast<float>(t->width) : 0.f;
    out[1] = t ? static_cast<float>(t->height) : 0.f;
    out[2] = out[3] = 0.f;
    if(!t || !e || !e->model || !on())
    {
        return;
    }
    if(const Override* to = overrideFor(t, true, t->name))
    {
        out[2] = static_cast<float>(1 + setFor(categoryOf(e), modelOverride(e), to));
    }
}

// R_InitBModelInstance: the entity's set (0 none).
extern "C" void VR_RetroInstance(entity_t* e, float out[4])
{
    out[0] = static_cast<float>(entitySet(e));
    out[1] = out[2] = out[3] = 0.f;
}

// R_FlushSpriteInstances: a sprite batch's set (x; 0 none) and its texture's size in Quake texels (yz: the frame's size
// over the part of its texture it fills), the block and table bound; showtris: none.
extern "C" void VR_RetroSprite(const entity_t* e, const mspriteframe_t* frame, int showtris, float out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0.f;
    if(showtris || !frame || frame->smax <= 0.f || frame->tmax <= 0.f)
    {
        return;
    }
    out[0] = static_cast<float>(entitySet(e));
    out[1] = static_cast<float>(frame->width) / frame->smax;
    out[2] = static_cast<float>(frame->height) / frame->tmax;
    if(out[0] > 0.f)
    {
        bindForDraw(QVR_RETRO_LUT_UNIT_SPRITE);
    }
}

// R_DrawParticles_Real: the Particles set for Quake's own particles (0 none), the block and table bound.
extern "C" float VR_RetroParticles(void)
{
    const int set = categorySet(Category::Particles);
    if(set > 0)
    {
        bindForDraw(QVR_RETRO_LUT_UNIT_SPRITE);
    }
    return static_cast<float>(set);
}

// VR_AliasInstance: the entity's set (0 none; none but in the standard draw) and its skin's Quake size; your body's
// parts by bone (RetroPart: bodyParts).
extern "C" void VR_RetroAlias(const entity_t* e, const void* aliashdr, int standard, float out[4], float part[4])
{
    out[0] = out[1] = out[2] = out[3] = 0.f;
    part[0] = part[1] = part[2] = part[3] = 0.f;
    if(!standard)
    {
        return;
    }
    out[0] = static_cast<float>(entitySet(e));
    if(out[0] > 0.f)
    {
        skinSize(static_cast<const aliashdr_t*>(aliashdr), e->skinnum, out[1], out[2]);
        if(isBody(e))
        {
            bodyParts(e->model, part);
        }
    }
}
