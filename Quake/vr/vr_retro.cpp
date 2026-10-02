// vr_retro.cpp -- see vr_retro.hpp (the settings) and vr_retro.h (the shaders' side).

#include "vr_retro.hpp"
#include "vr_retro.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/Clamp.hpp"
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
};

constexpr CategoryInfo categoryInfo[categoryCount] = {
    {"world", "World"},
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
        Con_Printf("vr_retro_reset: no category \"%s\" (world, or all)\n", which);
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
    Cmd_AddCommand("vr_retro_reset", reset_f);
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

// R_AddBModelCall: the texture's own size in Quake texels (the blocks' grid) and its own set (0: the instance's).
extern "C" void VR_RetroCall(const texture_t* t, float out[4])
{
    out[0] = t ? static_cast<float>(t->width) : 0.f;
    out[1] = t ? static_cast<float>(t->height) : 0.f;
    out[2] = out[3] = 0.f;
}

// R_InitBModelInstance: the entity's set (0 none): the world's for the world.
extern "C" void VR_RetroInstance(entity_t* e, float out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0.f;
    if(e == &cl_entities[0] && categoryOn(Category::World))
    {
        out[0] = static_cast<float>(setOf(Category::World));
    }
}
