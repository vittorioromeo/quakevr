// vr_brushes.cpp -- runtime brushes (vr_brushes.hpp; docs/vr-port/BRUSH_TOOL.md): the brush (planes, textures,
// alignment) and its .map text, the brushes of the map (each an entity, func_qvr_brush), the brush model made from a
// brush (planes, hulls 0/1/2 as chains, faces), its slot in the brush buffers (vertices, indices, draw commands, its
// flat light) and the vr_brush_* commands.

#include "vr_brushes_internal.hpp"
#include "vr_geom.hpp"
#include "vr_progs.hpp"
#include "vr_trace.hpp"

#include "vr_box3d.hpp"
#include "vr_brushbake.hpp"
#include "vr_brushlight.hpp"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_files.hpp"
#include "vr_held.hpp"
#include "vr_ledges.hpp"
#include "vr_mem.hpp"
#include "vr_modelcollide.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Container/SmallVector.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::brushes
{
using namespace detail;

namespace
{

// ---------------------------------------------------------------------------------------------------------------------
// The brushes of the map.

struct Entry
{
    Brush brush;
    int edict{0};
    qmodel_t* model{nullptr};
    za::UniquePtr<Built> built;
    za::UniquePtr<Built> retired; // the one before (a pointer into it kept a frame somewhere is still good)
    za::U32 revision{0};
    bool dirty{false}; // its slot to write (upload)
    // Its light (vr_brushlight.cpp): its faces' lightmaps in its slot's rectangle and their texels (the flat light, then
    // the bake's), laid out at its upload after each change of its faces (lightReady false); the serial its bake carries
    // (a result for an older layout dropped); rebake: a brush near it changed (its shadows).
    za::Vector<brushlight::Map> maps;
    brushlight::Texels texels;
    bool lightReady{false};
    bool baked{false};
    bool fullBake{false}; // its light light.exe's (the relit world's: vr_brushbake.cpp), not the game's bake
    bool rebake{false};
    za::U32 lightSerial{0};
};

// An edit undone by vr_brush_undo: a brush made (undone: removed), changed or removed (undone: back as it was before).
struct UndoStep
{
    enum class Kind : int
    {
        Made,
        Changed,
        Removed
    } kind{Kind::Made};
    int id{0};
    Brush before;
};
constexpr za::SizeT undoDepth = 64;

// The listen server's brushes by id - 1 (an id: a slot in the brush buffers), the main thread's. Emptied with the world.
struct Registry
{
    za::Vector<za::UniquePtr<Entry>> entries;
    za::Vector<UndoStep> undo; // oldest first, up to undoDepth
    bool undoing{false};       // (an undo's own edits not recorded)
    za::U32 revisions{0};   // the models made (each its own revision)
    bool sweep{false};      // a world began: its brush entities without a brush removed (a saved game's), once
    bool uploadAll{false};  // the map's buffers made again (R_NewMap): every slot written again
    int lastSpawned{0};     // vr_brush_spawn's last thing (vr_brush_ent last)
    char texture[16]{};     // the texture new brushes get
    bool loadedSave{false}; // this world is a saved game's (its brushes from it: no brush file loaded for it)
    const qmodel_t* reserveWorld{nullptr}; // the world whose load made the brush buffers' reserve (R_NewMap)
};
Registry registry;

// A saved game's brush lines, read before its load (VR_ReadSaveInfo) for after it (VR_OnLoadGame); the text a save
// writes (saveLines). The main thread's.
struct SaveState
{
    za::String pending; // the loading save's brushes' text (allToText's)
    bool have{false};
    za::String lines;   // saveLines' last
};
SaveState saveState;

// The world's textures a brush may have (usableTexture), in its order, and its default (the most used plain one), found
// once a world (the panel asks each frame): the main thread's. Emptied at a map change (the world's textures go with
// the hunk; a later world may be in the same slot) and a game directory change.
struct TextureCache
{
    const qmodel_t* world{nullptr};
    za::Vector<texture_t*> usable;
    char defaultName[16]{};
    auto members() { return mem::list(world, usable, defaultName); }
};
mem::Cache<TextureCache> textureCache{"brush textures", mem::MapChange | mem::GameDirChange};

// A brush's lit faces as its bake is queued (frame; the queue keeps its own copy): the main thread's.
struct BakeScratch
{
    za::Vector<brushlight::Face> faces;
    auto members() { return mem::list(faces); }
};
mem::Scratch<BakeScratch> bakeScratch{"brush bakes"};

void recordUndo(UndoStep::Kind kind, int id, const Brush* before)
{
    if(registry.undoing)
    {
        return;
    }
    if(registry.undo.size() >= undoDepth)
    {
        registry.undo.erase(registry.undo.begin());
    }
    UndoStep s;
    s.kind = kind;
    s.id = id;
    if(before)
    {
        s.before = *before;
    }
    registry.undo.pushBack(ZA_MOVE(s));
}

[[nodiscard]] Entry* entryOf(int id)
{
    if(id <= 0 || id > static_cast<int>(registry.entries.size()))
    {
        return nullptr;
    }
    return registry.entries[static_cast<za::SizeT>(id - 1)].get();
}

[[nodiscard]] int idFromName(const char* name)
{
    const size_t n = strlen(modelPrefix);
    if(strncmp(name, modelPrefix, n) != 0)
    {
        return 0;
    }
    return atoi(name + n);
}

// How many brushes the map may have at once: its slots (made at its load: vr_brush_max then; before the client has made
// them for this map, as a saved game's brushes are made again, what it will make).
[[nodiscard]] int capacity()
{
    if(cls.state == ca_dedicated || !sv.worldmodel || registry.reserveWorld != sv.worldmodel)
    {
        return R_RuntimeBrushSlotsFor(za::clamp(static_cast<int>(vr_brush_max.value), 0, 4096));
    }
    return R_RuntimeBrushReserve()->slots;
}

// What else keeps things of a model learns that it changed: the ledges (climbing), the held weapons' triangles, the
// decals on its old faces (only those: the world's beside it stay).
void forgetModel(qmodel_t* mod, int id, const glm::vec3& oldLo, const glm::vec3& oldHi)
{
    ledges::forget(mod);
    modelcollide::forget(mod);
    if(oldLo.x <= oldHi.x)
    {
        decals::removeOn(id, oldLo - glm::vec3{2.f}, oldHi + glm::vec3{2.f});
    }
}

// The model of `e` made again from its brush (or empty), in place. `texturesOnly`: only its faces' textures changed (Paint,
// vr_brush_texture): its revision kept and nothing else told (Box3D's hulls, the ledges, the held weapons' triangles,
// the marks on it all stay as they are).
void rebuild(Entry& e, bool empty, bool texturesOnly = false)
{
    glm::vec3 oldLo{1.f}, oldHi{0.f};
    if(e.built && !e.built->surfaces.empty())
    {
        oldLo = e.built->origin + e.built->mins;
        oldHi = e.built->origin + e.built->maxs;
    }
    e.retired = ZA_MOVE(e.built);
    e.built = za::makeUnique<Built>();
    build(empty ? nullptr : &e.brush, *e.built);
    if(!texturesOnly)
    {
        e.revision = ++registry.revisions;
    }
    if(e.model)
    {
        apply(e.model, e.built.get(), e.brush.id);
        if(!texturesOnly)
        {
            forgetModel(e.model, e.brush.id, oldLo, oldHi);
        }
    }
    if(!texturesOnly)
    {
        // Its light laid out again at its upload; the brushes near where it was and is baked again (its shadows on them).
        e.lightReady = false;
        e.baked = false;
        e.lightSerial++;
        brushlight::cancel(e.brush.id);
        glm::vec3 lo = oldLo, hi = oldHi;
        if(!e.built->surfaces.empty())
        {
            const glm::vec3 nlo = e.built->origin + e.built->mins, nhi = e.built->origin + e.built->maxs;
            lo = lo.x <= hi.x ? glm::min(lo, nlo) : nlo;
            hi = oldLo.x <= oldHi.x ? glm::max(hi, nhi) : nhi;
        }
        if(lo.x <= hi.x)
        {
            const glm::vec3 reach{vr_brush_bake_reach.value};
            for(auto& other : registry.entries)
            {
                if(other && other.get() != &e && other->built && !other->built->surfaces.empty())
                {
                    const glm::vec3 olo = other->built->origin + other->built->mins, ohi = other->built->origin + other->built->maxs;
                    if(glm::all(glm::lessThan(olo, hi + reach)) && glm::all(glm::greaterThan(ohi, lo - reach)))
                    {
                        other->rebake = true;
                    }
                }
            }
        }
    }
    e.dirty = true;
}

// Whether two brushes' faces have the same texture alignment (their lightmaps' luxels in the same places).
[[nodiscard]] bool sameAlignment(const Brush& a, const Brush& b)
{
    if(a.faces.size() != b.faces.size())
    {
        return false;
    }
    for(za::SizeT i = 0; i < a.faces.size(); i++)
    {
        const Face& f = a.faces[i];
        const Face& g = b.faces[i];
        if(f.uAxis != g.uAxis || f.vAxis != g.vAxis || f.uOffset != g.uOffset || f.vOffset != g.vOffset || f.scaleU != g.scaleU ||
            f.scaleV != g.scaleV)
        {
            return false;
        }
    }
    return true;
}

// Whether two brushes have the same planes, in the same order (only their textures may differ).
[[nodiscard]] bool sameShape(const Brush& a, const Brush& b)
{
    if(a.faces.size() != b.faces.size())
    {
        return false;
    }
    for(za::SizeT i = 0; i < a.faces.size(); i++)
    {
        if(a.faces[i].normal != b.faces[i].normal || a.faces[i].dist != b.faces[i].dist)
        {
            return false;
        }
    }
    return true;
}

// The entity of brush `e` placed at its model (made if it has none).
bool placeEntity(Entry& e)
{
    const progs::ServerVm vm;
    char name[MAX_QPATH];
    q_snprintf(name, sizeof(name), "%s%d", modelPrefix, e.brush.id);
    int index = -1;
    for(int i = 1; i < MAX_MODELS && sv.model_precache[i]; i++)
    {
        if(!strcmp(sv.model_precache[i], name))
        {
            index = i;
            break;
        }
    }
    if(index < 0)
    {
        index = VR_LatePrecacheModel(Hunk_Strdup(name, "qvrbrush")); // (the list keeps the pointer; made by Mod_LoadModel)
    }
    if(index <= 0 || !sv.models[index])
    {
        Con_Printf("brush: no model slot for %s\n", name);
        return false;
    }
    qmodel_t* mod = sv.models[index];
    if(e.model != mod)
    {
        e.model = mod;
        apply(mod, e.built.get(), e.brush.id);
    }
    edict_t* ed = e.edict > 0 ? EDICT_NUM(e.edict) : nullptr;
    if(!ed || ed->free)
    {
        ed = ED_Alloc();
        e.edict = NUM_FOR_EDICT(ed);
        ed->v.classname = PR_SetEngineString(className);
        ed->v.solid = SOLID_BSP;
        ed->v.movetype = MOVETYPE_PUSH;
        ed->v.takedamage = 0.f; // (DAMAGE_NO)
        VR_EdictIndex_Touch(ed); // (the edict index: a classname set in C)
    }
    ed->v.model = PR_SetEngineString(sv.model_precache[index]);
    ed->v.modelindex = static_cast<float>(index);
    for(int i = 0; i < 3; i++)
    {
        ed->v.origin[i] = e.built->origin[i];
        ed->v.mins[i] = mod->clipmins[i];
        ed->v.maxs[i] = mod->clipmaxs[i];
        ed->v.size[i] = mod->clipmaxs[i] - mod->clipmins[i];
        ed->v.angles[i] = 0.f;
        ed->v.velocity[i] = 0.f;
    }
    SV_LinkEdict(ed, false);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// The slots: each brush's vertices, indices, draw commands and lightmap texels in the brush buffers' reserve.

// A brush's faces as its light sees them (world space): each surface's plane, polygon and texture axes, in the surfaces'
// order (the model's: the faces with area).
void litFaces(const Entry& e, za::Vector<brushlight::Face>& out)
{
    out.clear();
    const Built& b = *e.built;
    for(za::SizeT k = 0; k < b.surfaces.size(); k++)
    {
        const msurface_t& s = b.surfaces[k];
        const Face& f = e.brush.faces[static_cast<za::SizeT>(b.surfaceFace[k])];
        brushlight::Face lf;
        lf.normal = f.normal;
        lf.dist = f.dist;
        for(int i = 0; i < s.numedges; i++)
        {
            const int se = b.surfedges[static_cast<za::SizeT>(s.firstedge + i)];
            const float* v = b.vertexes[se >= 0 ? b.edges[static_cast<za::SizeT>(se)].v[0] : b.edges[static_cast<za::SizeT>(-se)].v[1]].position;
            lf.poly.pushBack(glm::dvec3{v[0], v[1], v[2]} + glm::dvec3{b.origin});
        }
        lf.s = glm::dvec4{f.uAxis / (f.scaleU != 0.0 ? f.scaleU : 1.0), f.uOffset};
        lf.t = glm::dvec4{f.vAxis / (f.scaleV != 0.0 ? f.scaleV : 1.0), f.vOffset};
        out.pushBack(ZA_MOVE(lf));
    }
}

// Its light laid out (its faces' lightmaps in its rectangle) and lit flat (the light where each face is), its bake
// queued (vr_brush_bake).
void layLight(Entry& e)
{
    // The map relit with it in it (vr_brush_bake_map): light.exe's light, its shape and textures the same.
    if(vr_brush_bake.value != 0.f && brushbake::fromWorld(e.brush, static_cast<int>(e.built->surfaces.size()), e.maps, e.texels))
    {
        e.lightReady = true;
        e.baked = true;
        e.fullBake = true;
        e.rebake = false;
        e.lightSerial++;
        return;
    }
    e.fullBake = false;
    za::Vector<brushlight::Face> faces;
    litFaces(e, faces);
    brushlight::layout(faces, e.maps);
    za::Vector<glm::vec3> colours;
    for(const brushlight::Face& f : faces)
    {
        colours.pushBack(brushlight::flatLight(f));
    }
    brushlight::fill(e.maps, colours, e.texels);
    e.lightReady = true;
    e.baked = false;
    e.rebake = false;
    e.lightSerial++;
    if(vr_brush_bake.value != 0.f)
    {
        brushlight::queue(e.brush.id, e.lightSerial, faces, e.maps);
    }
}

// Slot `slot` written for entry `e` (null: emptied).
void upload(int slot, Entry* e)
{
    const qvr_brush_reserve_t* r = R_RuntimeBrushReserve();
    if(slot < 0 || slot >= r->slots || !r->lmxy)
    {
        return;
    }
    if(!e || !e->model || !e->built || e->built->surfaces.empty())
    {
        R_RuntimeBrushUpload(slot, nullptr, 0, nullptr, 0, nullptr, 0);
        if(e && e->model)
        {
            for(int i = 0; i <= TEXTYPE_COUNT; i++)
            {
                e->model->texofs[i] = 0;
            }
        }
        return;
    }
    if(!e->lightReady || e->maps.size() != e->built->surfaces.size())
    {
        layLight(*e);
    }
    Built& b = *e->built;
    const int firstVert = r->firstvert + slot * QVR_BRUSH_SLOT_VERTS;
    const int firstIndex = r->firstindex + slot * QVR_BRUSH_SLOT_INDEXES;
    za::InPlaceVector<glvert_t, QVR_BRUSH_SLOT_VERTS> verts;
    za::InPlaceVector<unsigned, QVR_BRUSH_SLOT_INDEXES> indexes;
    za::InPlaceVector<bmodel_draw_indirect_t, QVR_BRUSH_SLOT_CMDS> cmds;
    const float rx = static_cast<float>(r->lmxy[slot * 2]), ry = static_cast<float>(r->lmxy[slot * 2 + 1]);
    const float lw = static_cast<float>(r->lmwidth), lh = static_cast<float>(r->lmheight);
    const int numSurfaces = static_cast<int>(b.surfaces.size());
    // The vertices a face at a time; the indices a texture at a time (its command's range).
    for(int k = 0; k < numSurfaces; k++)
    {
        msurface_t& s = b.surfaces[static_cast<za::SizeT>(k)];
        if(static_cast<int>(verts.size()) + s.numedges > QVR_BRUSH_SLOT_VERTS)
        {
            s.vbo_firstvert = -1;
            continue;
        }
        s.vbo_firstvert = firstVert + static_cast<int>(verts.size());
        const brushlight::Map& m = e->maps[static_cast<za::SizeT>(k)];
        const texture_t* t = b.textures[static_cast<za::SizeT>(s.texinfo->texnum)];
        const float tw = t && t->width ? static_cast<float>(t->width) : 64.f, th = t && t->height ? static_cast<float>(t->height) : 64.f;
        glm::vec3 centre{0.f};
        for(int i = 0; i < s.numedges; i++)
        {
            const int se = b.surfedges[static_cast<za::SizeT>(s.firstedge + i)];
            const float* v = b.vertexes[se >= 0 ? b.edges[static_cast<za::SizeT>(se)].v[0] : b.edges[static_cast<za::SizeT>(-se)].v[1]].position;
            centre += geom::vec(v) / static_cast<float>(s.numedges);
        }
        const unsigned styles = static_cast<unsigned>(m.styles[0]) | (static_cast<unsigned>(m.styles[1]) << 8) |
                                (static_cast<unsigned>(m.styles[2]) << 16) | (static_cast<unsigned>(m.styles[3]) << 24);
        for(int i = 0; i < s.numedges; i++)
        {
            const int se = b.surfedges[static_cast<za::SizeT>(s.firstedge + i)];
            const float* v = b.vertexes[se >= 0 ? b.edges[static_cast<za::SizeT>(se)].v[0] : b.edges[static_cast<za::SizeT>(-se)].v[1]].position;
            glvert_t gv{};
            VectorCopy(v, gv.pos);
            const float su = DotProduct(v, s.texinfo->vecs[0]) + s.texinfo->vecs[0][3];
            const float tv = DotProduct(v, s.texinfo->vecs[1]) + s.texinfo->vecs[1][3];
            float sc = su / tw, tc = tv / th;
            if(t && t->shift > 0) // (Q64's texture shift, as GL_BuildBModelVertexBuffer)
            {
                sc /= static_cast<float>(2 * t->shift);
                tc /= static_cast<float>(2 * t->shift);
            }
            gv.st[0] = sc;
            gv.st[1] = tc;
            // The lightmap: luxel (0, 0)'s middle at s0, t0 (model space's s and t are the world's: the offsets carry the
            // origin), `step` texture units a luxel.
            gv.st[2] = (rx + static_cast<float>(m.x) + (su - static_cast<float>(m.s0)) / static_cast<float>(m.step) + 0.5f) / lw;
            gv.st[3] = (ry + static_cast<float>(m.y) + (tv - static_cast<float>(m.t0)) / static_cast<float>(m.step) + 0.5f) / lh;
            gv.lmofs = static_cast<float>(m.w) / lw;
            gv.styles = styles;
            gv.swellpin = 0.f;
            geom::store(centre, gv.centre);
            verts.pushBack(gv);
        }
    }
    for(int j = 0; j < static_cast<int>(b.used.size()) && j < QVR_BRUSH_SLOT_CMDS; j++)
    {
        bmodel_draw_indirect_t cmd{};
        cmd.firstIndex = static_cast<GLuint>(firstIndex + static_cast<int>(indexes.size()));
        cmd.instanceCount = 1;
        for(int k = 0; k < numSurfaces; k++)
        {
            const msurface_t& s = b.surfaces[static_cast<za::SizeT>(k)];
            if(s.texinfo->texnum != b.used[static_cast<za::SizeT>(j)] || s.vbo_firstvert < 0)
            {
                continue;
            }
            for(int i = 2; i < s.numedges && static_cast<int>(indexes.size()) + 3 <= QVR_BRUSH_SLOT_INDEXES; i++)
            {
                indexes.pushBack(static_cast<unsigned>(s.vbo_firstvert));
                indexes.pushBack(static_cast<unsigned>(s.vbo_firstvert + i - 1));
                indexes.pushBack(static_cast<unsigned>(s.vbo_firstvert + i));
            }
        }
        cmd.count = static_cast<GLuint>(firstIndex + static_cast<int>(indexes.size())) - cmd.firstIndex;
        cmds.pushBack(cmd);
    }
    R_RuntimeBrushUpload(slot, verts.data(), static_cast<int>(verts.size()), indexes.data(), static_cast<int>(indexes.size()),
        cmds.data(), static_cast<int>(cmds.size()));
    R_RuntimeBrushLightmap(slot, e->texels.light.data(), e->texels.lux.data());
    if(developer.value)
    {
        Con_Printf("brush %d: slot %d, %d corners, %d indices, %d textures, luxels %.0f apart, %s\n", e->brush.id, slot,
            static_cast<int>(verts.size()), static_cast<int>(indexes.size()), static_cast<int>(cmds.size()),
            e->maps.empty() ? 0.0 : e->maps[0].step, e->baked ? "baked" : "flat");
    }
    qmodel_t* mod = e->model;
    mod->firstcmd = r->firstcmd + slot * QVR_BRUSH_SLOT_CMDS;
    const int drawn = za::min(static_cast<int>(b.used.size()), QVR_BRUSH_SLOT_CMDS);
    for(int i = 0; i <= TEXTYPE_COUNT; i++)
    {
        mod->texofs[i] = za::min(b.texofs[i], drawn);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Commands.

} // namespace
namespace detail
{
bool parseVec(int first, glm::dvec3& v)
{
    if(Cmd_Argc() < first + 3)
    {
        return false;
    }
    for(int i = 0; i < 3; i++)
    {
        v[i] = Q_atof(Cmd_Argv(first + i));
    }
    return true;
}
} // namespace detail
namespace
{

} // namespace
namespace detail
{
[[nodiscard]] bool serverReady()
{
    if(!sv.active || !sv.worldmodel)
    {
        Con_Printf("brush: no local game\n");
        return false;
    }
    return true;
}
} // namespace detail
namespace
{

// vr_brush_box <x y z> <X Y Z> [texture]: a box from corner to corner.
void box_f()
{
    glm::dvec3 a, c;
    if(!parseVec(1, a) || !parseVec(4, c))
    {
        Con_Printf("usage: vr_brush_box <x y z> <X Y Z> [texture]\n");
        return;
    }
    if(!serverReady())
    {
        return;
    }
    const int id = create(box(glm::min(a, c), glm::max(a, c), Cmd_Argc() > 7 ? Cmd_Argv(7) : currentTexture()));
    if(id)
    {
        Con_Printf("brush: made %d (entity %d)\n", id, entityOf(id));
    }
}

// vr_brush_here [size] [height] [texture] [distance]: a box `size` wide (64) and `height` tall (size) on the floor
// ahead of the player, its middle `distance` away (half its size and 48 more).
void here_f()
{
    if(!serverReady())
    {
        return;
    }
    const double size = Cmd_Argc() > 1 ? za::max(Q_atof(Cmd_Argv(1)), 1.0f) : 64.0;
    const double height = Cmd_Argc() > 2 ? za::max(Q_atof(Cmd_Argv(2)), 1.0f) : size;
    const double distance = Cmd_Argc() > 4 ? Q_atof(Cmd_Argv(4)) : size * 0.5 + 48.0;
    const progs::ServerVm vm;
    edict_t* player = progs::playerEdict();
    const double yaw = glm::radians(static_cast<double>(player->v.angles[1]));
    const glm::dvec3 fwd{za::cos(yaw), za::sin(yaw), 0.0};
    glm::dvec3 centre = glm::dvec3{player->v.origin[0], player->v.origin[1], player->v.origin[2]} + fwd * distance;
    const glm::vec3 start{centre};
    const trace_t tr = worldtrace::serverLine(start, start - glm::vec3{0.f, 0.f, 512.f}, player);
    const double floorZ = tr.fraction < 1.f && tr.plane.normal[2] > 0.999f ? tr.plane.dist : tr.endpos[2]; // (the floor's plane: on it)
    centre.x = za::round(centre.x / 8.0) * 8.0;
    centre.y = za::round(centre.y / 8.0) * 8.0;
    const glm::dvec3 lo{centre.x - size * 0.5, centre.y - size * 0.5, za::round(floorZ)};
    const int id = create(box(lo, lo + glm::dvec3{size, size, height}, Cmd_Argc() > 3 ? Cmd_Argv(3) : currentTexture()));
    if(id)
    {
        Con_Printf("brush: made %d (entity %d) at %.0f %.0f %.0f\n", id, entityOf(id), lo.x, lo.y, lo.z);
    }
}

[[nodiscard]] int idArg(int i)
{
    const int id = Cmd_Argc() > i ? Q_atoi(Cmd_Argv(i)) : 0;
    if(!find(id))
    {
        Con_Printf("brush: no brush %s\n", Cmd_Argc() > i ? Cmd_Argv(i) : "(none given)");
        return 0;
    }
    return id;
}

// vr_brush_set <id> <x y z> <X Y Z>: its box (a box brush) changed.
void set_f()
{
    glm::dvec3 a, c;
    const int id = idArg(1);
    if(!id || !parseVec(2, a) || !parseVec(5, c))
    {
        if(id)
        {
            Con_Printf("usage: vr_brush_set <id> <x y z> <X Y Z>\n");
        }
        return;
    }
    // (any shape: stretched from its bounds to these)
    Brush b = *find(id);
    glm::dvec3 lo, hi;
    if(!bounds(b, lo, hi))
    {
        return;
    }
    scaleTo(b, lo, hi, glm::min(a, c), glm::max(a, c));
    if(change(id, b))
    {
        Con_Printf("brush: %d is %.0f %.0f %.0f .. %.0f %.0f %.0f\n", id, a.x, a.y, a.z, c.x, c.y, c.z);
    }
}

// vr_brush_move <id> <dx dy dz>
void move_f()
{
    glm::dvec3 d;
    const int id = idArg(1);
    if(!id || !parseVec(2, d))
    {
        return;
    }
    Brush b = *find(id);
    translate(b, d);
    if(change(id, b))
    {
        Con_Printf("brush: %d moved %.0f %.0f %.0f\n", id, d.x, d.y, d.z);
    }
}

// vr_brush_texture <id> <texture> [face]: every face's (or one's) texture.
void texture_f()
{
    const int id = idArg(1);
    if(!id || Cmd_Argc() < 3)
    {
        return;
    }
    if(!worldTexture(Cmd_Argv(2)) || !usableTexture(worldTexture(Cmd_Argv(2))))
    {
        Con_Printf("brush: the map has no texture %s a brush can have\n", Cmd_Argv(2));
        return;
    }
    Brush b = *find(id);
    const int face = Cmd_Argc() > 3 ? Q_atoi(Cmd_Argv(3)) : -1;
    for(za::SizeT i = 0; i < b.faces.size(); i++)
    {
        if(face < 0 || face == static_cast<int>(i))
        {
            q_strlcpy(b.faces[i].texture, Cmd_Argv(2), sizeof(b.faces[i].texture));
        }
    }
    if(change(id, b))
    {
        Con_Printf("brush: %d textured %s\n", id, Cmd_Argv(2));
    }
}

// vr_brush_delete <id | all>
void delete_f()
{
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "all"))
    {
        int n = 0;
        for(za::SizeT i = 0; i < registry.entries.size(); i++)
        {
            if(registry.entries[i] && remove(static_cast<int>(i) + 1))
            {
                n++;
            }
        }
        Con_Printf("brush: removed %d\n", n);
        return;
    }
    const int id = idArg(1);
    if(id && remove(id))
    {
        Con_Printf("brush: removed %d\n", id);
    }
}

// vr_brush_duplicate <id> <dx dy dz>
void duplicate_f()
{
    glm::dvec3 d;
    const int id = idArg(1);
    if(!id || !parseVec(2, d))
    {
        return;
    }
    Brush b = *find(id);
    translate(b, d);
    const int made = create(b);
    if(made)
    {
        Con_Printf("brush: %d copied as %d (entity %d)\n", id, made, entityOf(made));
    }
}

void list_f()
{
    Con_Printf("brushes: %d of %d (vr_brush_max %d at the map's load)\n", count(), capacity(), static_cast<int>(vr_brush_max.value));
    forEach([](const Brush& b) {
        glm::dvec3 lo, hi;
        const bool ok = bounds(b, lo, hi);
        const Entry* e = entryOf(b.id);
        const int marks = decals::countOn(b.id, glm::vec3{lo} - glm::vec3{2.f}, glm::vec3{hi} + glm::vec3{2.f});
        Con_Printf("  %d: entity %d, rev %u, %d faces, %s, %.0f %.0f %.0f .. %.0f %.0f %.0f, %d decals%s\n", b.id, e ? e->edict : 0,
            b.rev, static_cast<int>(b.faces.size()), b.faces.empty() ? "-" : b.faces[0].texture, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z,
            marks, ok ? "" : " (no volume)");
    });
}

// vr_brush_light <id>: its faces' lightmaps (luxels, their spacing, styles) and their first style's light (the least,
// the mean and the most of a luxel's brightness, lightmap units: 128 full), flat or baked.
void light_f()
{
    const int id = idArg(1);
    if(!id)
    {
        return;
    }
    const Entry* e = entryOf(id);
    Con_Printf("brush light %d: %s, %d faces\n", id, !e->lightReady ? "unlaid" : e->fullBake ? "relit" : e->baked ? "baked" : "flat",
        static_cast<int>(e->maps.size()));
    for(za::SizeT k = 0; k < e->maps.size() && e->texels.light.size() == static_cast<za::SizeT>(brushlight::rectWidth * brushlight::rectHeight); k++)
    {
        const brushlight::Map& m = e->maps[k];
        double lo = 1e9, hi = 0.0, sum = 0.0;
        for(int y = m.y; y < m.y + m.h; y++)
        {
            for(int x = m.x; x < m.x + m.w; x++)
            {
                const unsigned c = e->texels.light[static_cast<za::SizeT>(y * brushlight::rectWidth + x)];
                const double v = ((c & 0xff) + ((c >> 8) & 0xff) + ((c >> 16) & 0xff)) / 3.0;
                lo = za::min(lo, v);
                hi = za::max(hi, v);
                sum += v;
            }
        }
        // (the other styles' means)
        double means[4]{0.0, 0.0, 0.0, 0.0};
        for(int s = 1; s < m.numStyles; s++)
        {
            for(int y = m.y; y < m.y + m.h; y++)
            {
                for(int x = m.x + s * m.w; x < m.x + (s + 1) * m.w; x++)
                {
                    const unsigned c = e->texels.light[static_cast<za::SizeT>(y * brushlight::rectWidth + x)];
                    means[s] += ((c & 0xff) + ((c >> 8) & 0xff) + ((c >> 16) & 0xff)) / 3.0 / (m.w * m.h);
                }
            }
        }
        const Face& f = e->brush.faces[static_cast<za::SizeT>(e->built->surfaceFace[k])];
        Con_Printf("  face %d (%.2f %.2f %.2f): %dx%d luxels %.0f apart, styles %d %d %d %d, light %.0f %.0f %.0f, styles' means %.0f %.0f %.0f\n",
            static_cast<int>(k), f.normal.x, f.normal.y, f.normal.z, m.w, m.h, m.step, m.styles[0], m.styles[1] == 255 ? -1 : m.styles[1],
            m.styles[2] == 255 ? -1 : m.styles[2], m.styles[3] == 255 ? -1 : m.styles[3], lo, sum / (m.w * m.h), hi, means[1], means[2],
            means[3]);
    }
}

// vr_brush_lights [x y z]: the map's lights as the bake has them; with a point, the nearest five.
void lights_f()
{
    glm::dvec3 p;
    if(parseVec(1, p))
    {
        const glm::vec3 q{p};
        brushlight::listLights(&q, false);
        return;
    }
    brushlight::listLights(nullptr, Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "styled"));
}

// vr_brush_rebake [id | all]: baked again (all: every brush).
void rebake_f()
{
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "all"))
    {
        for(auto& e : registry.entries)
        {
            if(e)
            {
                e->lightReady = false;
                e->dirty = true;
            }
        }
        return;
    }
    const int id = idArg(1);
    if(id)
    {
        entryOf(id)->lightReady = false;
        entryOf(id)->dirty = true;
    }
}

// vr_brush_text <id>: its .map text.
void text_f()
{
    const int id = idArg(1);
    if(!id)
    {
        return;
    }
    za::String text;
    toText(*find(id), text);
    Con_Printf("%s", text.cStr());
}

// vr_brush_spawn <classname> <x y z> [yaw] [drop] (tests): a thing made there by its spawn function (as the toolgun's
// Spawn tool: a monster on a box); "drop": a prop let fall from there (its bottom at z: a crate dropped onto a box).
void spawn_f()
{
    glm::dvec3 at;
    if(Cmd_Argc() < 5 || !parseVec(2, at))
    {
        Con_Printf("usage: vr_brush_spawn <classname> <x y z> [yaw]\n");
        return;
    }
    if(!serverReady())
    {
        return;
    }
    const float yaw = Cmd_Argc() > 5 ? Q_atof(Cmd_Argv(5)) : 0.f;
    edict_t* e = box3d::spawnClass(Cmd_Argv(1), glm::vec3{at}, glm::vec3{0.f, yaw, 0.f}, nullptr, nullptr, 0.f);
    if(e)
    {
        const progs::ServerVm vm;
        registry.lastSpawned = NUM_FOR_EDICT(e);
        Con_Printf("brush: spawned %d %s\n", registry.lastSpawned, Cmd_Argv(1));
        if(Cmd_Argc() > 6 && !q_strcasecmp(Cmd_Argv(6), "drop"))
        {
            // (a prop made there lies still, asleep, where its placing put it: held there awake, its bottom at z, it falls)
            Cbuf_InsertText(va("vr_physics_stack %d 1 %g %g %g\n", registry.lastSpawned, at.x, at.y, at.z));
        }
    }
}

// vr_brush_ent <entity> (tests): where it is, what it stands on (a brush's: its id) and whether it lives.
void ent_f()
{
    if(!serverReady() || Cmd_Argc() < 2)
    {
        return;
    }
    const progs::ServerVm vm;
    const int num = !q_strcasecmp(Cmd_Argv(1), "last") ? registry.lastSpawned : Q_atoi(Cmd_Argv(1));
    if(num <= 0 || num >= qcvm->num_edicts)
    {
        Con_Printf("brush ent: no entity %d\n", num);
        return;
    }
    edict_t* e = EDICT_NUM(num);
    const int ground = (static_cast<int>(e->v.flags) & FL_ONGROUND) ? NUM_FOR_EDICT(PROG_TO_EDICT(e->v.groundentity)) : -1;
    Con_Printf("brush ent: %d %s at %.1f %.1f %.1f, %s, ground %d%s, health %.0f, enemy %d\n", num,
        e->free ? "(free)" : PR_GetString(e->v.classname), e->v.origin[0], e->v.origin[1], e->v.origin[2],
        (static_cast<int>(e->v.flags) & FL_ONGROUND) ? "on ground" : "in the air", ground,
        ground > 0 && ofEntity(ground) ? va(" (brush %d)", ofEntity(ground)) : "", e->v.health,
        e->v.enemy ? NUM_FOR_EDICT(PROG_TO_EDICT(e->v.enemy)) : 0);
}

void textures_f()
{
    Con_Printf("brush textures: %d (current %s)\n", textureCount(), currentTexture());
    for(int i = 0; i < textureCount(); i++)
    {
        const texture_t* t = textureAt(i);
        Con_Printf("  %d: %s %ux%u%s\n", i, t->name, t->width, t->height, t->type == TEXTYPE_CUTOUT ? " (fence)" : "");
    }
}

// vr_brush_shape <box | wedge | cylinder | pyramid> <x y z> <X Y Z> [sides] [up] [rise] [texture]: a shape filling the box
// from corner to corner; `up` and `rise` an axis with its way (z, -z, x, ...: the default z, the wedge rising along x).
[[nodiscard]] bool parseAxis(const char* s, int& axis, double& sign)
{
    sign = s[0] == '-' ? -1.0 : 1.0;
    const char c = s[s[0] == '-' || s[0] == '+' ? 1 : 0];
    axis = c == 'x' || c == 'X' ? 0 : c == 'y' || c == 'Y' ? 1 : c == 'z' || c == 'Z' ? 2 : -1;
    return axis >= 0;
}

void shape_f()
{
    glm::dvec3 a, c;
    if(Cmd_Argc() < 8 || !parseVec(2, a) || !parseVec(5, c))
    {
        Con_Printf("usage: vr_brush_shape <box | wedge | cylinder | pyramid> <x y z> <X Y Z> [sides] [up] [rise] [texture]\n");
        return;
    }
    if(!serverReady())
    {
        return;
    }
    int up = 2, rise = 0;
    double upSign = 1.0, riseSign = 1.0;
    if(Cmd_Argc() > 9 && !parseAxis(Cmd_Argv(9), up, upSign))
    {
        up = 2;
    }
    if(Cmd_Argc() > 10 && !parseAxis(Cmd_Argv(10), rise, riseSign))
    {
        rise = 0;
    }
    const Shape s = shapeFromName(Cmd_Argv(1));
    const int sides = Cmd_Argc() > 8 ? Q_atoi(Cmd_Argv(8)) : 8;
    const int id = create(shape(s, glm::min(a, c), glm::max(a, c), up, upSign, rise, riseSign, sides,
        Cmd_Argc() > 11 ? Cmd_Argv(11) : currentTexture()));
    if(id)
    {
        Con_Printf("brush: made %d, a %s of %d faces (entity %d)\n", id, shapeName(s), static_cast<int>(find(id)->faces.size()),
            entityOf(id));
    }
}

// vr_brush_rotate <id> <pitch yaw roll> [lock]: turned about its bounds' middle (lock 0: the textures stay the world's).
void rotate_f()
{
    glm::dvec3 ang;
    const int id = idArg(1);
    if(!id || !parseVec(2, ang))
    {
        return;
    }
    Brush b = *find(id);
    glm::dvec3 lo, hi;
    if(!bounds(b, lo, hi))
    {
        return;
    }
    const glm::dmat3 r = turnFromAngles(ang);
    rotate(b, r, glm::floor((lo + hi) * 0.5 + 0.5), Cmd_Argc() > 5 ? Q_atoi(Cmd_Argv(5)) != 0 : true);
    if(change(id, b))
    {
        glm::dvec3 l2, h2;
        (void)bounds(b, l2, h2);
        Con_Printf("brush: %d turned %.0f %.0f %.0f, %.1f %.1f %.1f .. %.1f %.1f %.1f\n", id, ang.x, ang.y, ang.z, l2.x, l2.y, l2.z,
            h2.x, h2.y, h2.z);
    }
}

void undo_f()
{
    if(serverReady())
    {
        (void)undo();
    }
}

// vr_brush_align <id> <face | all> <reset | shift du dv | scale su sv | turn degrees>: a face's texture alignment.
void align_f()
{
    const int id = idArg(1);
    if(!id || Cmd_Argc() < 4)
    {
        if(id)
        {
            Con_Printf("usage: vr_brush_align <id> <face | all> <reset | shift du dv | scale su sv | turn degrees>\n");
        }
        return;
    }
    Brush b = *find(id);
    const bool all = !q_strcasecmp(Cmd_Argv(2), "all");
    const int face = all ? -1 : Q_atoi(Cmd_Argv(2));
    const char* op = Cmd_Argv(3);
    const double x = Cmd_Argc() > 4 ? Q_atof(Cmd_Argv(4)) : 0.0, y = Cmd_Argc() > 5 ? Q_atof(Cmd_Argv(5)) : x;
    for(za::SizeT i = 0; i < b.faces.size(); i++)
    {
        if(!all && face != static_cast<int>(i))
        {
            continue;
        }
        Face& f = b.faces[i];
        if(!q_strcasecmp(op, "reset"))
        {
            defaultAlignment(f);
        }
        else if(!q_strcasecmp(op, "shift"))
        {
            f.uOffset += x;
            f.vOffset += y;
        }
        else if(!q_strcasecmp(op, "scale") && x != 0.0 && y != 0.0)
        {
            f.scaleU = x;
            f.scaleV = y;
        }
        else if(!q_strcasecmp(op, "turn"))
        {
            // The axes turned about the face's normal (Valve 220's rotation: the number kept with them).
            const glm::dmat3 r = glm::mat3_cast(glm::angleAxis(glm::radians(x), f.normal));
            f.uAxis = r * f.uAxis;
            f.vAxis = r * f.vAxis;
            f.rotation = za::remainder(f.rotation + x, 360.0);
        }
    }
    if(change(id, b))
    {
        Con_Printf("brush: %d aligned (%s %s)\n", id, Cmd_Argv(2), op);
    }
}

[[nodiscard]] za::String brushFile(const char* name); // (below)

// vr_brush_save [name]: this map's brushes to <game folder>/brushes/<map>[-name].brushes.
void save_f()
{
    if(!serverReady())
    {
        return;
    }
    const za::String path = brushFile(Cmd_Argc() > 1 ? Cmd_Argv(1) : "");
    za::String text;
    allToText(text);
    (void)files::createDirectories(za::String{files::parentPath(path)}.cStr());
    if(!files::writeText(path.cStr(), text))
    {
        Con_Printf("brush: couldn't write %s\n", path.cStr());
        return;
    }
    Con_Printf("brush: %d brushes saved to %s\n", count(), path.cStr());
}

// vr_brush_load [name] [add]: the brushes of <game folder>/brushes/<map>[-name].brushes made (the map's removed first,
// unless "add").
void load_f()
{
    if(!serverReady())
    {
        return;
    }
    const char* name = Cmd_Argc() > 1 && q_strcasecmp(Cmd_Argv(1), "add") ? Cmd_Argv(1) : "";
    const bool add = !q_strcasecmp(Cmd_Argv(Cmd_Argc() - 1), "add");
    const za::String path = brushFile(name);
    za::String text, warning;
    if(!files::readText(path.cStr(), text))
    {
        Con_Printf("brush: no brush file %s\n", path.cStr());
        return;
    }
    const int made = fromAllText(text, !add, warning);
    Con_Printf("brush: %d brushes made from %s%s%s\n", made, path.cStr(), warning.empty() ? "" : ": ", warning.cStr());
}

// vr_brush_bake_write <name> [fake <level>] (tests): the full bake's input without light: this map with its brushes
// as submodels, solid (func_wall entities), their notes in its entities, each face lit `level` if given (the light read
// back without light.exe), written to <game folder>/maps/<name>.bsp. `map <name>` loads it.
void bakeWrite_f()
{
    if(!serverReady() || Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_brush_bake_write <name> [fake <level>]\n");
        return;
    }
    const char* name = Cmd_Argv(1);
    for(const char* p = name; *p; ++p)
    {
        if(!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-'))
        {
            Con_Printf("brush: invalid output map name\n");
            return;
        }
    }
    if(!name[0] || !q_strcasecmp(name, sv.name) || COM_FileExists(va("maps/%s.bsp", name), nullptr))
    {
        Con_Printf("brush: output must be a new map; existing maps are never overwritten\n");
        return;
    }
    unsigned int pathId = 0;
    byte* data = COM_LoadMallocFile(va("maps/%s.bsp", sv.name), &pathId);
    if(!data)
    {
        Con_Printf("brush: maps/%s.bsp can't be read\n", sv.name);
        return;
    }
    brushbake::Options o;
    o.classname = "func_wall";
    o.entitiesIn = true;
    o.sourceEntities = sv.worldmodel ? sv.worldmodel->entities : nullptr;
    o.fakeLight = Cmd_Argc() > 3 && !q_strcasecmp(Cmd_Argv(2), "fake") ? za::clamp(Q_atoi(Cmd_Argv(3)), 0, 255) : -1;
    za::Vector<unsigned char> out;
    za::String entities, notes, error;
    const bool ok = brushbake::augment(data, static_cast<za::SizeT>(com_filesize), out, entities, notes, error, o);
    VR_HeapFree(data);
    if(!ok)
    {
        Con_Printf("brush: not written: %s\n", error.cStr());
        return;
    }
    const za::String path{va("%s/maps/%s.bsp", com_gamedir, Cmd_Argv(1))};
    if(!files::writeBytes(path.cStr(), out.data(), out.size()))
    {
        Con_Printf("brush: can't write %s\n", path.cStr());
        return;
    }
    VR_FileCacheForget();
    Con_Printf("brush: %s written (%d brushes as submodels, %d bytes)\n", path.cStr(), count(), static_cast<int>(out.size()));
}

// vr_brush_texture_set <name>: the texture new brushes get.
void textureSet_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("brush: new brushes get %s\n", currentTexture());
        return;
    }
    setCurrentTexture(Cmd_Argv(1));
    Con_Printf("brush: new brushes get %s\n", currentTexture());
}

} // namespace


// ---------------------------------------------------------------------------------------------------------------------
// The registry.

namespace
{
// The entity create gives the next brush instead of a new one (restoreAfterLoad: the saved game's), 0: a new one.
int adoptEdict = 0;
} // namespace

int create(const Brush& b, int wantId)
{
    const int adopt = adoptEdict;
    adoptEdict = 0;
    if(!sv.active || !sv.worldmodel)
    {
        return 0;
    }
    if(const char* why = invalid(b))
    {
        Con_Printf("brush: not made: %s\n", why);
        return 0;
    }
    const int cap = capacity();
    int id = 0;
    if(wantId > 0 && wantId <= cap && !entryOf(wantId))
    {
        id = wantId;
    }
    for(int i = 0; i < cap && !id; i++)
    {
        if(i >= static_cast<int>(registry.entries.size()) || !registry.entries[static_cast<za::SizeT>(i)])
        {
            id = i + 1;
            break;
        }
    }
    if(!id)
    {
        Con_Printf("brush: not made: the map has room for %d brushes (vr_brush_max at its load)\n", cap);
        return 0;
    }
    if(id > static_cast<int>(registry.entries.size()))
    {
        registry.entries.resize(static_cast<za::SizeT>(id));
    }
    registry.entries[static_cast<za::SizeT>(id - 1)] = za::makeUnique<Entry>();
    Entry& e = *registry.entries[static_cast<za::SizeT>(id - 1)];
    e.brush = b;
    e.brush.id = id;
    e.brush.rev = b.rev > 0 ? b.rev : 1;
    e.edict = adopt;
    rebuild(e, false);
    if(!placeEntity(e))
    {
        registry.entries[static_cast<za::SizeT>(id - 1)].reset();
        return 0;
    }
    recordUndo(UndoStep::Kind::Made, id, nullptr);
    return id;
}

bool undo()
{
    if(registry.undo.empty())
    {
        Con_Printf("brush: nothing to undo\n");
        return false;
    }
    const UndoStep s = registry.undo.back();
    registry.undo.popBack();
    registry.undoing = true;
    bool ok = false;
    switch(s.kind)
    {
    case UndoStep::Kind::Made:
        ok = remove(s.id);
        Con_Printf("brush: undone: %d made%s\n", s.id, ok ? " (removed)" : " (already gone)");
        break;
    case UndoStep::Kind::Changed:
        ok = change(s.id, s.before);
        Con_Printf("brush: undone: %d changed%s\n", s.id, ok ? " (as it was)" : " (gone)");
        break;
    case UndoStep::Kind::Removed:
    {
        const int id = create(s.before, s.id);
        ok = id != 0;
        Con_Printf("brush: undone: %d removed (back as %d)\n", s.id, id);
        break;
    }
    }
    registry.undoing = false;
    return ok;
}

bool change(int id, const Brush& b)
{
    Entry* e = entryOf(id);
    if(!e)
    {
        return false;
    }
    if(const char* why = invalid(b))
    {
        Con_Printf("brush: %d not changed: %s\n", id, why);
        return false;
    }
    const za::U32 rev = e->brush.rev;
    const bool painted = e->built && !e->built->surfaces.empty() && sameShape(e->brush, b);
    const bool aligned = sameAlignment(e->brush, b);
    recordUndo(UndoStep::Kind::Changed, id, &e->brush);
    e->brush = b;
    e->brush.id = id;
    e->brush.rev = rev + 1;
    rebuild(*e, false, painted);
    if(painted && !aligned)
    {
        // (its textures moved on its faces: its luxels with them; its shadows on others the same)
        e->lightReady = false;
        e->baked = false;
        e->lightSerial++;
        brushlight::cancel(id);
    }
    return placeEntity(*e);
}

bool remove(int id)
{
    Entry* e = entryOf(id);
    if(!e)
    {
        return false;
    }
    const int num = e->edict;
    recordUndo(UndoStep::Kind::Removed, id, &e->brush);
    e->edict = 0; // (its entity's free below finds no brush: edictFreed)
    rebuild(*e, true);
    upload(id - 1, e);
    registry.entries[static_cast<za::SizeT>(id - 1)].reset();
    if(num > 0 && sv.active)
    {
        const progs::ServerVm vm;
        edict_t* ed = EDICT_NUM(num);
        if(!ed->free)
        {
            ED_Free(ed);
        }
    }
    return true;
}

const Brush* find(int id)
{
    const Entry* e = entryOf(id);
    return e ? &e->brush : nullptr;
}

int ofEntity(int num)
{
    if(num <= 0)
    {
        return 0;
    }
    for(za::SizeT i = 0; i < registry.entries.size(); i++)
    {
        if(registry.entries[i] && registry.entries[i]->edict == num)
        {
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

int entityOf(int id)
{
    const Entry* e = entryOf(id);
    return e ? e->edict : 0;
}

int count()
{
    int n = 0;
    for(const auto& e : registry.entries)
    {
        n += e ? 1 : 0;
    }
    return n;
}

void forEach(za::FunctionRef<void(const Brush&)> f)
{
    for(const auto& e : registry.entries)
    {
        if(e)
        {
            f(e->brush);
        }
    }
}

bool bakeShape(const Brush& b, BakeShape& out)
{
    out = BakeShape{};
    Built built;
    build(&b, built);
    if(built.surfaces.empty())
    {
        return false;
    }
    const glm::dvec3 origin{built.origin};
    out.lo = origin + glm::dvec3{built.mins};
    out.hi = origin + glm::dvec3{built.maxs};
    for(za::SizeT k = 0; k < built.surfaces.size(); k++)
    {
        const msurface_t& s = built.surfaces[k];
        const Face& f = b.faces[static_cast<za::SizeT>(built.surfaceFace[k])];
        BakeShape::Surface o;
        o.normal = f.normal;
        o.dist = f.dist;
        for(int i = 0; i < s.numedges; i++)
        {
            const int se = built.surfedges[static_cast<za::SizeT>(s.firstedge + i)];
            const float* v = built.vertexes[se >= 0 ? built.edges[static_cast<za::SizeT>(se)].v[0] : built.edges[static_cast<za::SizeT>(-se)].v[1]].position;
            o.poly.pushBack(glm::dvec3{v[0], v[1], v[2]} + origin);
        }
        o.vecs[0] = glm::dvec4{f.uAxis / (f.scaleU != 0.0 ? f.scaleU : 1.0), f.uOffset};
        o.vecs[1] = glm::dvec4{f.vAxis / (f.scaleV != 0.0 ? f.scaleV : 1.0), f.vOffset};
        const texture_t* t = built.textures[static_cast<za::SizeT>(s.texinfo->texnum)];
        q_strlcpy(o.texture, t ? t->name : f.texture, sizeof(o.texture));
        out.surfaces.pushBack(ZA_MOVE(o));
    }
    for(int h = 1; h <= 2; h++)
    {
        for(const glm::dvec4& p : built.hullPlanes[h])
        {
            out.hulls[h - 1].pushBack(glm::dvec4{glm::dvec3{p}, p.w + glm::dot(glm::dvec3{p}, origin)});
        }
    }
    return true;
}

unsigned long long bakeHash(const Brush& b)
{
    // Its faces' numbers rounded (a sloped face read back from a save's text moves in its last digits), and textures.
    unsigned long long h = 1469598103934665603ull;
    const auto add = [&h](const void* p, za::SizeT n) {
        for(za::SizeT i = 0; i < n; i++)
        {
            h = (h ^ static_cast<const unsigned char*>(p)[i]) * 1099511628211ull;
        }
    };
    for(const Face& f : b.faces)
    {
        const double v[14] = {f.normal.x, f.normal.y, f.normal.z, f.dist / 64.0, f.uAxis.x, f.uAxis.y, f.uAxis.z, f.uOffset / 64.0,
            f.vAxis.x, f.vAxis.y, f.vAxis.z, f.vOffset / 64.0, f.scaleU, f.scaleV};
        for(const double x : v)
        {
            const long long q = static_cast<long long>(za::round(x * 1e4));
            add(&q, sizeof(q));
        }
        add(f.texture, strlen(f.texture));
    }
    return h;
}

// ---------------------------------------------------------------------------------------------------------------------
// Keeping them.

namespace
{

// The map's shape (its planes: a relit copy has the same), as a brush file names it.
[[nodiscard]] unsigned mapHash()
{
    unsigned h = 2166136261u;
    const qmodel_t* w = sv.worldmodel;
    if(!w || !w->planes)
    {
        return 0u;
    }
    const unsigned char* bytes = nullptr;
    for(za::SizeT i = 0; i < static_cast<za::SizeT>(w->numplanes); i++)
    {
        const mplane_t& p = w->planes[i];
        const float v[4] = {p.normal[0], p.normal[1], p.normal[2], p.dist};
        bytes = reinterpret_cast<const unsigned char*>(v);
        for(za::SizeT k = 0; k < sizeof(v); k++)
        {
            h = (h ^ bytes[k]) * 16777619u;
        }
    }
    return h;
}

// The brushes in a text: each "{ ... }" with the lines before it (its header) since the last.
bool parseAll(za::StringView text, za::Vector<Brush>& out, za::String& error)
{
    out.clear();
    za::SizeT start = 0, at = 0;
    while(at < text.size())
    {
        za::SizeT end = at;
        while(end < text.size() && text[end] != '\n')
        {
            end++;
        }
        // (a line that is "}" alone, spaces aside, ends a brush)
        za::SizeT a = at, z = end;
        while(a < z && (text[a] == ' ' || text[a] == '\t' || text[a] == '\r'))
        {
            a++;
        }
        while(z > a && (text[z - 1] == ' ' || text[z - 1] == '\t' || text[z - 1] == '\r'))
        {
            z--;
        }
        if(z - a == 1 && text[a] == '}')
        {
            Brush b;
            za::String why;
            if(!fromText(za::StringView{text.data() + start, end - start}, b, why))
            {
                error = va("brush %d: %s", static_cast<int>(out.size()) + 1, why.cStr());
                return false;
            }
            out.pushBack(ZA_MOVE(b));
            start = end + 1;
        }
        at = end + 1;
    }
    return true;
}

// The brush file's path for `name` (empty: the map's own): <game folder>/brushes/<map>[-name].brushes.
[[nodiscard]] za::String brushFile(const char* name)
{
    if(name && name[0] == '@')
    {
        return za::String{va("%s/brushes/%s.brushes", com_gamedir, name + 1)}; // (another map's: @<map>[-name])
    }
    return za::String{va("%s/brushes/%s%s%s.brushes", com_gamedir, sv.name, name && name[0] ? "-" : "", name ? name : "")};
}

} // namespace

void allToText(za::String& out)
{
    out.clear();
    out += va("// qvrbrushes 1 map %s bsp %08x\n", sv.name, mapHash());
    za::String one;
    forEach([&](const Brush& b) {
        toText(b, one);
        out += one;
    });
}

int fromAllText(za::StringView text, bool replace, za::String& warning)
{
    warning.clear();
    za::Vector<Brush> list;
    za::String error;
    if(!parseAll(text, list, error))
    {
        warning = error;
        return 0;
    }
    char map[64] = "";
    unsigned hash = 0;
    const za::String copy{text.data(), text.size()};
    const char* header = strstr(copy.cStr(), "// qvrbrushes ");
    if(header && sscanf(header, "// qvrbrushes %*d map %63s bsp %x", map, &hash) == 2 && (q_strcasecmp(map, sv.name) || hash != mapHash()))
    {
        warning = va("made for %s (bsp %08x), this is %s (bsp %08x)", map, hash, sv.name, mapHash());
    }
    if(replace)
    {
        for(za::SizeT i = 0; i < registry.entries.size(); i++)
        {
            if(registry.entries[i])
            {
                remove(static_cast<int>(i) + 1);
            }
        }
    }
    int made = 0;
    for(const Brush& b : list)
    {
        made += create(b, b.id) ? 1 : 0;
    }
    return made;
}

const char* saveLines()
{
    SaveState& s = saveState;
    s.lines.clear();
    if(!count())
    {
        return "";
    }
    za::String all;
    allToText(all);
    za::SizeT at = 0;
    while(at < all.size())
    {
        za::SizeT end = at;
        while(end < all.size() && all[end] != '\n')
        {
            end++;
        }
        s.lines += "// qvrb ";
        s.lines += za::StringView{all.data() + at, end - at};
        s.lines += '\n';
        at = end + 1;
    }
    return s.lines.cStr();
}

void readSaveLines(const char* saveText)
{
    SaveState& s = saveState;
    s.pending.clear();
    s.have = false;
    for(const char* p = saveText ? strstr(saveText, "\n// qvrb ") : nullptr; p; p = strstr(p + 1, "\n// qvrb "))
    {
        const char* line = p + 9;
        const za::SizeT n = strcspn(line, "\r\n");
        s.pending += za::StringView{line, n};
        s.pending += '\n';
        s.have = true;
    }
}

void restoreAfterLoad()
{
    SaveState& s = saveState;
    registry.loadedSave = true;
    if(!s.have)
    {
        return;
    }
    s.have = false;
    za::Vector<Brush> list;
    za::String error;
    if(!parseAll(s.pending, list, error))
    {
        Con_Printf("brush: the saved game's brushes can't be read (%s)\n", error.cStr());
        return;
    }
    const progs::ServerVm vm;
    registry.undoing = true;
    int made = 0;
    for(const Brush& b : list)
    {
        // Its entity: the one whose model is its own.
        char name[MAX_QPATH];
        q_snprintf(name, sizeof(name), "%s%d", modelPrefix, b.id);
        int num = 0;
        for(int i = 1; i < qcvm->num_edicts && !num; i++)
        {
            edict_t* ed = EDICT_NUM(i);
            if(!ed->free && !strcmp(PR_GetString(ed->v.classname), className) && !strcmp(PR_GetString(ed->v.model), name) && !ofEntity(i))
            {
                num = i;
            }
        }
        adoptEdict = num;
        made += create(b, b.id) ? 1 : 0;
    }
    adoptEdict = 0;
    registry.undoing = false;
    s.pending.clear();
    Con_Printf("brush: %d brushes of the saved game made again\n", made);
}

// ---------------------------------------------------------------------------------------------------------------------
// Textures.

bool usableTexture(const texture_t* t)
{
    if(!t || !t->name[0] || !t->gltexture)
    {
        return false;
    }
    if(t->type != TEXTYPE_DEFAULT && t->type != TEXTYPE_CUTOUT)
    {
        return false; // (sky, liquids)
    }
    if(t->name[0] == '+' && t->name[1] != '0' && t->name[1] != 'a' && t->name[1] != 'A')
    {
        return false; // (an animation's later frames: its first is the one)
    }
    return q_strncasecmp(t->name, "trigger", 7) != 0 && q_strncasecmp(t->name, "clip", 4) != 0 &&
           q_strncasecmp(t->name, "skip", 4) != 0 && q_strncasecmp(t->name, "hint", 4) != 0 &&
           q_strncasecmp(t->name, "notexture", 9) != 0;
}

namespace
{

// The world's usable textures and its default (textureCache), found for the world now (again after a map change).
[[nodiscard]] const TextureCache& textures()
{
    TextureCache& c = textureCache;
    const qmodel_t* w = sv.worldmodel ? sv.worldmodel : cl.worldmodel;
    if(c.world == w && !c.usable.empty())
    {
        return c;
    }
    c.world = w;
    c.usable.clear();
    c.defaultName[0] = 0;
    if(!w || !w->textures)
    {
        return c;
    }
    for(int i = 0; i < w->numtextures; i++)
    {
        if(usableTexture(w->textures[i]))
        {
            c.usable.pushBack(w->textures[i]);
        }
    }
    // The map's most used plain texture (its walls').
    const texture_t* best = c.usable.empty() ? nullptr : c.usable[0];
    if(w->surfaces)
    {
        za::Vector<int> uses(static_cast<za::SizeT>(w->numtextures), 0);
        for(int i = 0; i < w->numsurfaces; i++)
        {
            const mtexinfo_t* ti = w->surfaces[i].texinfo;
            if(ti && ti->texnum >= 0 && ti->texnum < w->numtextures)
            {
                uses[static_cast<za::SizeT>(ti->texnum)]++;
            }
        }
        int most = 0;
        for(int i = 0; i < w->numtextures; i++)
        {
            const texture_t* t = w->textures[i];
            // (a wall's texture: not a trim strip, a light's or a button's)
            if(usableTexture(t) && t->type == TEXTYPE_DEFAULT && t->name[0] != '+' && t->width >= 32 && t->height >= 32 &&
                uses[static_cast<za::SizeT>(i)] > most)
            {
                most = uses[static_cast<za::SizeT>(i)];
                best = t;
            }
        }
    }
    q_strlcpy(c.defaultName, best ? best->name : "base", sizeof(c.defaultName));
    return c;
}

} // namespace

int textureCount()
{
    return static_cast<int>(textures().usable.size());
}

texture_t* textureAt(int index)
{
    const TextureCache& c = textures();
    return index >= 0 && index < static_cast<int>(c.usable.size()) ? c.usable[static_cast<za::SizeT>(index)] : nullptr;
}

const char* currentTexture()
{
    if(registry.texture[0] && usableTexture(worldTexture(registry.texture)))
    {
        return registry.texture;
    }
    const TextureCache& c = textures();
    return c.defaultName[0] ? c.defaultName : "base";
}

void setCurrentTexture(const char* name)
{
    texture_t* t = worldTexture(name);
    if(usableTexture(t))
    {
        q_strlcpy(registry.texture, t->name, sizeof(registry.texture));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The models.

bool isRuntime(const qmodel_t* model)
{
    return model && model->name[0] == '*' && idFromName(model->name) > 0;
}

za::U32 revision(const qmodel_t* model)
{
    if(!isRuntime(model))
    {
        return 0;
    }
    const Entry* e = entryOf(idFromName(model->name));
    return e && e->model == model ? e->revision : 0u;
}

bool clipBox(const qmodel_t* model, const glm::vec3& start, const glm::vec3& mins, const glm::vec3& maxs,
    const glm::vec3& end, trace_t& trace)
{
    if(!isRuntime(model))
    {
        return false;
    }
    memset(&trace, 0, sizeof(trace));
    trace.fraction = 1.f;
    trace.inopen = true;
    VectorCopy(end, trace.endpos);
    const Entry* e = entryOf(idFromName(model->name));
    if(!e || e->model != model || !e->built || e->built->clipnodes[1].empty())
    {
        return true; // (nothing solid)
    }
    // The brush's planes (model space) and its bevels: hulls 1 and 2's before they were pushed out.
    const za::Vector<glm::vec4>& planes = e->built->clipPlanes;
    float enter = -1.f, leave = 1.f;
    int lead = -1;
    bool startOut = false, getOut = false;
    for(za::SizeT i = 0; i < planes.size(); i++)
    {
        const glm::vec3 n{planes[i]};
        glm::vec3 ofs;
        for(int k = 0; k < 3; k++)
        {
            ofs[k] = n[k] < 0.f ? maxs[k] : mins[k];
        }
        const float dist = planes[i].w - glm::dot(ofs, n);
        const float d1 = glm::dot(start, n) - dist, d2 = glm::dot(end, n) - dist;
        if(d2 > 0.f)
        {
            getOut = true;
        }
        if(d1 > 0.f)
        {
            startOut = true;
        }
        if(d1 > 0.f && d2 >= d1)
        {
            return true; // (wholly in front of a plane: never in it)
        }
        if(d1 <= 0.f && d2 <= 0.f)
        {
            continue;
        }
        if(d1 > d2)
        {
            const float f = (d1 - distEpsilon) / (d1 - d2);
            if(f > enter)
            {
                enter = f;
                lead = static_cast<int>(i);
            }
        }
        else
        {
            const float f = (d1 + distEpsilon) / (d1 - d2);
            leave = za::min(leave, f);
        }
    }
    if(!startOut)
    {
        trace.startsolid = true;
        trace.allsolid = !getOut;
        trace.inopen = !trace.allsolid;
        if(trace.allsolid)
        {
            return true;
        }
        // (starting in it, getting out: Quake's trace from within a solid goes on to the end)
        return true;
    }
    if(enter < leave && enter > -1.f && lead >= 0)
    {
        const float f = za::max(enter, 0.f);
        trace.fraction = f;
        for(int k = 0; k < 3; k++)
        {
            trace.endpos[k] = start[k] + f * (end[k] - start[k]);
        }
        const glm::vec3 n{planes[static_cast<za::SizeT>(lead)]};
        glm::vec3 ofs;
        for(int k = 0; k < 3; k++)
        {
            ofs[k] = n[k] < 0.f ? maxs[k] : mins[k];
        }
        VectorCopy(n, trace.plane.normal);
        trace.plane.dist = planes[static_cast<za::SizeT>(lead)].w - glm::dot(ofs, n);
    }
    return true;
}

void facesIn(const glm::vec3& lo, const glm::vec3& hi,
    za::FunctionRef<void(const za::Vector<glm::vec3>& poly, const glm::vec3& normal, int id)> f)
{
    za::Vector<glm::vec3> poly;
    for(const auto& ep : registry.entries)
    {
        if(!ep || !ep->built || ep->built->surfaces.empty() || ep->edict <= 0 || ep->edict >= cl.num_entities)
        {
            continue;
        }
        const Built& b = *ep->built;
        const entity_t& ent = cl_entities[ep->edict];
        if(ent.model != ep->model)
        {
            continue;
        }
        const glm::vec3 origin{ent.origin[0], ent.origin[1], ent.origin[2]};
        if(glm::any(glm::lessThan(origin + b.maxs, lo)) || glm::any(glm::greaterThan(origin + b.mins, hi)))
        {
            continue;
        }
        for(const msurface_t& s : b.surfaces)
        {
            const glm::vec3 smin = origin + geom::vec(s.mins);
            const glm::vec3 smax = origin + geom::vec(s.maxs);
            if(glm::any(glm::lessThan(smax, lo)) || glm::any(glm::greaterThan(smin, hi)))
            {
                continue;
            }
            poly.clear();
            for(int i = 0; i < s.numedges; i++)
            {
                const int se = b.surfedges[static_cast<za::SizeT>(s.firstedge + i)];
                const float* v = b.vertexes[se >= 0 ? b.edges[static_cast<za::SizeT>(se)].v[0] : b.edges[static_cast<za::SizeT>(-se)].v[1]].position;
                poly.pushBack(origin + geom::vec(v));
            }
            glm::vec3 n{s.plane->normal[0], s.plane->normal[1], s.plane->normal[2]};
            if(s.flags & SURF_PLANEBACK)
            {
                n = -n;
            }
            f(poly, n, ep->brush.id);
        }
    }
}

bool blocks(const glm::vec3& a, const glm::vec3& b, int except)
{
    for(const auto& ep : registry.entries)
    {
        if(!ep || !ep->built || ep->built->surfaces.empty() || ep->brush.id == except)
        {
            continue;
        }
        const Built& bt = *ep->built;
        const glm::vec3 pa = a - bt.origin, pb = b - bt.origin;
        if(glm::any(glm::lessThan(glm::max(pa, pb), bt.mins)) || glm::any(glm::greaterThan(glm::min(pa, pb), bt.maxs)))
        {
            continue;
        }
        // (the segment against its planes: in front of one all along, out; else the part behind them all)
        float enter = 0.f, leave = 1.f;
        bool out = false;
        for(const glm::vec4& p : bt.clipPlanes)
        {
            const glm::vec3 n{p};
            const float d1 = glm::dot(n, pa) - p.w, d2 = glm::dot(n, pb) - p.w;
            if(d1 > 0.f && d2 > 0.f)
            {
                out = true;
                break;
            }
            if(d1 <= 0.f && d2 <= 0.f)
            {
                continue;
            }
            const float f = d1 / (d1 - d2);
            if(d1 > d2)
            {
                enter = za::max(enter, f);
            }
            else
            {
                leave = za::min(leave, f);
            }
            if(enter > leave)
            {
                out = true;
                break;
            }
        }
        if(!out)
        {
            return true;
        }
    }
    return false;
}

bool traceLine(const glm::vec3& start, const glm::vec3& end, glm::vec3& where, glm::vec3& normal, float& fraction)
{
    bool hit = false;
    fraction = 1.f;
    for(const auto& ep : registry.entries)
    {
        if(!ep || !ep->built || ep->built->surfaces.empty() || ep->edict <= 0 || ep->edict >= cl.num_entities)
        {
            continue;
        }
        const entity_t& ent = cl_entities[ep->edict];
        if(ent.model != ep->model)
        {
            continue;
        }
        const glm::vec3 origin{ent.origin[0], ent.origin[1], ent.origin[2]};
        trace_t tr;
        if(clipBox(ep->model, start - origin, glm::vec3{0.f}, glm::vec3{0.f}, end - origin, tr) && !tr.startsolid &&
            tr.fraction < fraction)
        {
            fraction = tr.fraction;
            where = geom::vec(tr.endpos) + origin;
            normal = geom::vec(tr.plane.normal);
            hit = true;
        }
    }
    return hit;
}

// ---------------------------------------------------------------------------------------------------------------------
// Hooks.

void reset()
{
    // The models keep their names (the next map's load may find them in mod_known): empty until made again.
    for(auto& e : registry.entries)
    {
        if(e && e->model)
        {
            apply(e->model, nullptr, e->brush.id);
        }
    }
    registry.entries.clear();
    registry.undo.clear();
    brushlight::clear();
    registry.sweep = true;
    registry.loadedSave = false;
}

void edictFreed(int num)
{
    const int id = ofEntity(num);
    if(!id)
    {
        return;
    }
    Entry* e = entryOf(id);
    e->edict = 0; // (being freed: not again)
    remove(id);
}

void frame()
{
    if(registry.sweep && sv.active && sv.state == ss_active)
    {
        registry.sweep = false;
        const progs::ServerVm vm;
        int strays = 0;
        progs::forEachEdict(svs.maxclients + 1, [&](edict_t* ed) {
            if(!strcmp(PR_GetString(ed->v.classname), className) && !ofEntity(NUM_FOR_EDICT(ed)))
            {
                ED_Free(ed);
                strays++;
            }
        });
        if(strays)
        {
            Con_Printf("brush: %d brush entities without their brush left out (a save of a build that didn't keep them)\n", strays);
        }
        // A new map (not a saved game's): its brush file's brushes, if it has one and vr_brush_autoload is on.
        if(!registry.loadedSave && vr_brush_autoload.value != 0.f && files::exists(brushFile("").cStr()))
        {
            za::String text, warning;
            if(files::readText(brushFile("").cStr(), text))
            {
                registry.undoing = true;
                const int made = fromAllText(text, false, warning);
                registry.undoing = false;
                Con_Printf("brush: %d brushes made from %s%s%s\n", made, brushFile("").cStr(), warning.empty() ? "" : ": ",
                    warning.cStr());
            }
        }
    }
    // (the client in the game, its light styles come: the flat light and the bake read the world's light with them)
    if(cls.state == ca_dedicated || !cl.worldmodel || cls.signon != SIGNONS)
    {
        return;
    }
    const bool all = registry.uploadAll;
    registry.uploadAll = false;
    for(za::SizeT i = 0; i < registry.entries.size(); i++)
    {
        Entry* e = registry.entries[i].get();
        if(e && (e->dirty || all))
        {
            e->dirty = false;
            upload(static_cast<int>(i), e);
        }
    }
    // The bakes: the brushes a change near them darkened or lit queued again, then a few milliseconds of the work.
    if(vr_brush_bake.value == 0.f || sv.worldmodel != cl.worldmodel)
    {
        return;
    }
    for(auto& e : registry.entries)
    {
        if(e && e->rebake && e->fullBake)
        {
            e->rebake = false; // (light.exe's light kept: the next full bake has the change)
        }
        if(e && e->rebake && e->lightReady && e->built && !e->built->surfaces.empty())
        {
            e->rebake = false;
            e->lightSerial++;
            litFaces(*e, bakeScratch.faces);
            brushlight::queue(e->brush.id, e->lightSerial, bakeScratch.faces, e->maps);
        }
    }
    brushlight::run(za::max(static_cast<double>(vr_brush_bake_ms.value), 0.1), [](int id, za::U32 serial, const brushlight::Texels& texels) {
        Entry* e = entryOf(id);
        if(e && e->lightSerial == serial && e->lightReady)
        {
            e->texels = texels;
            e->baked = true;
            e->dirty = true;
        }
    });
}

void registerCommands()
{
    Cmd_AddCommand("vr_brush_box", box_f);
    Cmd_AddCommand("vr_brush_here", here_f);
    Cmd_AddCommand("vr_brush_set", set_f);
    Cmd_AddCommand("vr_brush_move", move_f);
    Cmd_AddCommand("vr_brush_texture", texture_f);
    Cmd_AddCommand("vr_brush_texture_set", textureSet_f);
    Cmd_AddCommand("vr_brush_delete", delete_f);
    Cmd_AddCommand("vr_brush_duplicate", duplicate_f);
    Cmd_AddCommand("vr_brush_list", list_f);
    Cmd_AddCommand("vr_brush_text", text_f);
    Cmd_AddCommand("vr_brush_textures", textures_f);
    registerDiagCommands(); // vr_brush_selftest, vr_brush_trace (vr_brushes_diag.cpp)
    Cmd_AddCommand("vr_brush_spawn", spawn_f);
    Cmd_AddCommand("vr_brush_ent", ent_f);
    Cmd_AddCommand("vr_brush_shape", shape_f);
    Cmd_AddCommand("vr_brush_rotate", rotate_f);
    Cmd_AddCommand("vr_brush_undo", undo_f);
    Cmd_AddCommand("vr_brush_align", align_f);
    Cmd_AddCommand("vr_brush_save", save_f);
    Cmd_AddCommand("vr_brush_load", load_f);
    Cmd_AddCommand("vr_brush_light", light_f);
    Cmd_AddCommand("vr_brush_lights", lights_f);
    Cmd_AddCommand("vr_brush_rebake", rebake_f);
    Cmd_AddCommand("vr_brush_bake_write", bakeWrite_f);
}

} // namespace qvr::brushes

// ---------------------------------------------------------------------------------------------------------------------
// The engine's hooks.

extern "C" int VR_RuntimeBrushModel(qmodel_t* mod)
{
    using namespace qvr::brushes;
    const int id = idFromName(mod->name);
    if(mod->name[0] != '*' || id <= 0)
    {
        return 0;
    }
    Entry* e = entryOf(id);
    if(e && e->built)
    {
        e->model = mod;
        apply(mod, e->built.get(), id);
        e->dirty = true;
    }
    else
    {
        apply(mod, nullptr, id); // (a save's brush, or one gone: empty)
    }
    return 1;
}

extern "C" int VR_IsRuntimeBrushModel(const qmodel_t* mod)
{
    return qvr::brushes::isRuntime(mod);
}

extern "C" int VR_RuntimeBrushSlots(void)
{
    return za::clamp(static_cast<int>(qvr::vr_brush_max.value), 0, 4096);
}

extern "C" void VR_RuntimeBrushesUpload(void)
{
    // The map's buffers made again: every brush's slot written again (its model not drawn until then).
    for(auto& e : qvr::brushes::registry.entries)
    {
        if(e && e->model)
        {
            for(int i = 0; i <= TEXTYPE_COUNT; i++)
            {
                e->model->texofs[i] = 0;
            }
        }
    }
    qvr::brushes::registry.uploadAll = true;
    qvr::brushes::registry.reserveWorld = cl.worldmodel;
}

extern "C" const char* VR_SaveBrushLines(void)
{
    return qvr::brushes::saveLines();
}
