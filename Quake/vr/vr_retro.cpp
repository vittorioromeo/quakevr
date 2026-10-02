// vr_retro.cpp -- see vr_retro.hpp (the settings) and vr_retro.h (the shaders' side).

#include "vr_retro.hpp"
#include "vr_retro.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_view.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

extern "C" GLuint gl_palette_lut; // gl_texmgr.c: the palette's nearest-colour table (128^3 indices)

namespace qvr::retro
{
namespace
{

struct ParamInfo
{
    const char* suffix;
    const char* def;
};

// The settings of every category, in Param's order; a category's own defaults below.
constexpr ParamInfo paramInfo[paramCount] = {
    {"", "1"},              // On
    {"_snap", "1"},         // Snap
    {"_block", "1"},        // Block
    {"_units", "0"},        // Units
    {"_average", "1"},      // Average
    {"_soft", "1"},         // Soft
    {"_fade", "1"},         // Fade
    {"_palette", "0"},      // Palette
    {"_dither", "0"},       // Dither
    {"_dither_scale", "1"}, // DitherScale
    {"_bump", "1"},         // Bump
    {"_detail", "0"},       // Detail: Quake's look has no grain inside a block
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
    {"body", "Your Body and Hands", "Your body, hands, the wrist gadget, the flashlight, pauldrons and pouches."},
    {"other", "Other Models", "Everything else drawn with a texture: projectiles, torches and flames, ..."},
};

za::String names[categoryCount * paramCount];
cvar_t cvars[categoryCount * paramCount];

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

[[nodiscard]] bool categoryOn(Category c)
{
    return on() && value(c, Param::On) != 0.f;
}

[[nodiscard]] int setOf(Category c)
{
    return static_cast<int>(c) + 1;
}

void fillSet(int set, Category c)
{
    float* p0 = block.sets[set * 3];
    float* p1 = block.sets[set * 3 + 1];
    float* p2 = block.sets[set * 3 + 2];
    p0[0] = za::clamp(value(c, Param::Block), 0.0625f, 64.f);
    p0[1] = za::clamp(value(c, Param::Soft), 0.f, 8.f);
    p0[2] = za::clamp(value(c, Param::Fade), 0.05f, 16.f);
    p0[3] = value(c, Param::Snap) != 0.f ? 1.f : 0.f;
    p1[0] = za::clamp(value(c, Param::Palette), 0.f, 1.f);
    p1[1] = za::clamp(value(c, Param::Dither), 0.f, 4.f);
    p1[2] = za::clamp(value(c, Param::DitherScale), 1.f, 64.f);
    p1[3] = za::clamp(value(c, Param::Bump), 0.f, 1.f);
    p2[0] = value(c, Param::Average) != 0.f ? 1.f : 0.f;
    p2[1] = value(c, Param::Units) != 0.f ? 1.f : 0.f;
    p2[2] = za::clamp(value(c, Param::Detail), 0.f, 1.f);
    p2[3] = 0.f;
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

[[nodiscard]] bool anyPrefix(const char* f, const char* const* prefixes, int count)
{
    for(int i = 0; i < count; i++)
    {
        if(startsWith(f, prefixes[i]))
        {
            return true;
        }
    }
    return false;
}

constexpr const char* gibPrefixes[] = {"gib1.", "gib2.", "gib3.", "zom_gib.", "statgib", "h_"};
constexpr const char* bodyPrefixes[] = {"vrbody", "hand", "finger_", "vrgadget", "vrpauldron", "vrpouch", "legholster",
    "vrflashlight"};
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
    if(m->type != mod_alias)
    {
        return Category::Other;
    }
    if(anyPrefix(f, gibPrefixes, sizeof(gibPrefixes) / sizeof(gibPrefixes[0])))
    {
        return Category::Gibs;
    }
    if(startsWith(f, "v_") || startsWith(f, "g_"))
    {
        return Category::Weapons;
    }
    if(anyPrefix(f, bodyPrefixes, sizeof(bodyPrefixes) / sizeof(bodyPrefixes[0])))
    {
        return Category::Body;
    }
    if(anyPrefix(f, propPrefixes, sizeof(propPrefixes) / sizeof(propPrefixes[0])))
    {
        return Category::Props;
    }
    if(anyPrefix(f, monsterFiles, sizeof(monsterFiles) / sizeof(monsterFiles[0])))
    {
        return Category::Monsters;
    }
    if((m->flags & EF_ROTATE) || anyPrefix(f, itemFiles, sizeof(itemFiles) / sizeof(itemFiles[0])))
    {
        return Category::Items;
    }
    return Category::Other;
}

// An entity's category: its model's, but a weapon in your hands or holsters is Held, anything else of yours your
// Body; a gib scaled down (the small gibs: vr_smallgibs.qc) a small gib.
[[nodiscard]] Category categoryOf(const entity_t* e)
{
    if(e == &cl_entities[0])
    {
        return Category::World;
    }
    const Category c = modelCategory(e->model);
    if(e->model->type == mod_alias && view::find(e) != nullptr)
    {
        return c == Category::Weapons || c == Category::Held ? Category::Held : Category::Body;
    }
    if(c == Category::Gibs && VR_EntityScale(e) < 0.8f)
    {
        return Category::SmallGibs;
    }
    return c;
}

// The set an entity's textures are drawn with (0: none).
[[nodiscard]] int entitySet(const entity_t* e)
{
    if(!e || !e->model)
    {
        return 0;
    }
    const Category c = categoryOf(e);
    return categoryOn(c) ? setOf(c) : 0;
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

// vr_retro_reset [category|all]: a category's settings (all: every one's) back to their defaults.
void reset_f()
{
    const char* which = Cmd_Argc() > 1 ? Cmd_Argv(1) : "all";
    bool any = false;
    for(int c = 0; c < categoryCount; c++)
    {
        if(ZA_STRCMP(which, "all") != 0 && q_strcasecmp(which, categoryInfo[c].key) != 0)
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
        Con_Printf("vr_retro_reset: no category \"%s\" (all, or one of:", which);
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
        Con_Printf("%3d %-28s %-20s set %d  skin %gx%g  texture %dx%d  scale %.2f\n", i, e->model->name,
            categoryInfo[static_cast<int>(c)].label, entitySet(e), w, h, tw, th, VR_EntityScale(e));
    }
}

} // namespace

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
        }
    }
    for(cvar_t& var : cvars)
    {
        Cvar_RegisterVariable(&var);
    }
    Cmd_AddCommand("vr_retro_reset", reset_f);
    Cmd_AddCommand("vr_retro_list", list_f);
}

} // namespace qvr::retro

using namespace qvr;
using namespace qvr::retro;

// R_UploadFrameData: the categories' sets and the palette for the shaders (uniform block 3).
extern "C" void VR_RetroUpload(void)
{
    block.info[0] = on() ? 1.f : 0.f;
    block.info[1] = gl_palette_lut != 0 ? 1.f : 0.f;
    block.info[2] = block.info[3] = 0.f;
    ZA_MEMCPY(block.palette, d_8to24table, sizeof(block.palette));
    for(int c = 0; c < categoryCount; c++)
    {
        fillSet(setOf(static_cast<Category>(c)), static_cast<Category>(c));
    }
    GLuint buf;
    GLbyte* ofs;
    GL_Upload(GL_UNIFORM_BUFFER, &block, sizeof(block), &buf, &ofs);
    GL_BindBufferRange(GL_UNIFORM_BUFFER, QVR_RETRO_UBO_BINDING, buf, reinterpret_cast<GLintptr>(ofs),
        static_cast<GLsizeiptr>(sizeof(block)));
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

// R_AddBModelCall: the texture's own size in Quake texels (the blocks' grid) and its set on entity e (the batch's
// first; 0: the instance's).
extern "C" void VR_RetroCall(entity_t* e, const texture_t* t, float out[4])
{
    out[0] = t ? static_cast<float>(t->width) : 0.f;
    out[1] = t ? static_cast<float>(t->height) : 0.f;
    out[2] = out[3] = 0.f;
    (void)e;
}

// R_InitBModelInstance: the entity's set (0 none).
extern "C" void VR_RetroInstance(entity_t* e, float out[4])
{
    out[0] = static_cast<float>(entitySet(e));
    out[1] = out[2] = out[3] = 0.f;
}

// VR_AliasInstance: the entity's set (0 none; none but in the standard draw) and its skin's Quake size.
extern "C" void VR_RetroAlias(const entity_t* e, const void* aliashdr, int standard, float out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0.f;
    if(!standard)
    {
        return;
    }
    out[0] = static_cast<float>(entitySet(e));
    if(out[0] > 0.f)
    {
        skinSize(static_cast<const aliashdr_t*>(aliashdr), e->skinnum, out[1], out[2]);
    }
}
