// vr_brushes.hpp -- runtime brushes (docs/vr-port/BRUSH_TOOL.md): convex brushes made in the game (the toolgun's Brush
// tool, the vr_brush_* commands) that are the map's own kind of geometry. Each is a server entity, `func_qvr_brush`
// (SOLID_BSP, MOVETYPE_PUSH), whose model `*qvrbrush/<id>` is a brush model made in memory from the brush's planes: its
// own planes, hull 0 as a chain of nodes (one a face, the last behind them all solid), hulls 1 and 2 as chains of the
// planes pushed out by Quake's boxes, its faces as surfaces with the map's textures. So everything that reads brush
// models meets it as it meets a door: Quake's collision (players stand and climb on it, monsters walk on it and can't
// see through it), Box3D's movers (props and ragdolls), the ledges (climbing, mantling), the toolgun's aim, traces. It is
// drawn by Ironwail's brush path from a slot of its own at the end of the brush buffers (r_brush.c's reserve): the world
// shader's light, dynamic lights and their shadows, decals, detail; its lightmap one texel a face (the light where the
// face is: R_LightPoint, flat).
//
// The brush is the truth, everything else is made from it: planes (outward normals and distances, world space) with
// each face's texture and its Valve 220 alignment, the same text as a .map's brush (toText / fromText), so a save's
// sidecar, a duplicate, the network and a bake can all carry it as text. Stage 1: axis-aligned boxes from the tool;
// the model maker takes any convex set (its polygons cut from the planes).
//
// The model's name is per brush id (not per change): an id is reused after its brush goes, its model made again in
// place (the same qmodel_t: the precache list never fills), and the systems that keep things per model learn of the
// change (revision(): Box3D's mover hulls; the ledges and decals told).

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/Vocabulary/FunctionRef.hpp"

namespace qvr::brushes
{

constexpr const char* className = "func_qvr_brush";
constexpr const char* modelPrefix = "*qvrbrush/"; // + the id
constexpr int maxFaces = 16;                       // a brush's planes (its lightmap strip has a texel each)

// One plane of a convex brush: its outward normal and distance (dot(p, normal) <= dist inside), world space; the
// texture by name (one of the map's), and its alignment as a Valve 220 .map writes it: the texture's s and t axes
// (world), their offsets in texels, the rotation (informative: the axes carry it) and the texels' scale.
struct Face
{
    glm::dvec3 normal{0.0, 0.0, 1.0};
    double dist{0.0};
    char texture[16]{};
    glm::dvec3 uAxis{1.0, 0.0, 0.0}, vAxis{0.0, -1.0, 0.0};
    double uOffset{0.0}, vOffset{0.0};
    double rotation{0.0};
    double scaleU{1.0}, scaleV{1.0};
};

enum class Kind : int
{
    Solid, // (stage 1: the only one; detail, non-solid and clip later)
};

struct Brush
{
    int id{0};       // stable while it lives (its model's name); 0: not made
    za::U32 rev{0};  // changed with each edit
    int group{0};    // 0: alone (groups later)
    Kind kind{Kind::Solid};
    za::Vector<Face> faces;
};

// ---------------------------------------------------------------------------------------------------------------------
// The brush itself.

// An axis-aligned box lo..hi, every face `texture`, aligned to the world (Quake's default axes).
[[nodiscard]] Brush box(const glm::dvec3& lo, const glm::dvec3& hi, const char* texture);
// A face's Valve axes as Quake's default for its plane (the world's axis nearest its normal), offsets 0, scale 1.
void defaultAlignment(Face& f);
// Its polyhedron's bounds; false if it has no volume (fewer than four faces with area, or a side under a unit).
[[nodiscard]] bool bounds(const Brush& b, glm::dvec3& lo, glm::dvec3& hi);
// Null if it is a brush that can be made, else why not.
[[nodiscard]] const char* invalid(const Brush& b);
// Moved by `delta` (its planes and its textures with it: the texture stays on the faces).
void translate(Brush& b, const glm::dvec3& delta);

// The .map text of a brush (Valve 220): a header comment (id, revision, group, kind), then "{", a line a face (three
// points on its plane, clockwise seen from outside, its texture, [ u ] [ v ] rotation scales), "}".
void toText(const Brush& b, za::String& out);
// One brush from that text (the header comment optional; its id kept if there); false with `error` set if it isn't one.
bool fromText(za::StringView text, Brush& out, za::String& error);

// ---------------------------------------------------------------------------------------------------------------------
// The brushes of this map (the listen server's).

// Made (a new id, its entity spawned), returning the id; 0 if refused (said on the console: why).
int create(const Brush& b);
// Changed (its model made again in place, its entity moved to it); false if there is no such brush or it can't be made.
bool change(int id, const Brush& b);
// Removed with its entity.
bool remove(int id);
[[nodiscard]] const Brush* find(int id);
[[nodiscard]] int ofEntity(int num); // the brush entity `num` is (0: none)
[[nodiscard]] int entityOf(int id);  // its entity (0: none)
[[nodiscard]] int count();
// Each brush, in id order.
void forEach(za::FunctionRef<void(const Brush&)> f);

// The map's textures a brush may use (the world's; no sky, liquids or animated frames past the first), and whether one is.
[[nodiscard]] int textureCount();
[[nodiscard]] texture_t* textureAt(int i);
[[nodiscard]] bool usableTexture(const texture_t* t);
// The texture new brushes get (the last one chosen, else the map's first wall-like one).
[[nodiscard]] const char* currentTexture();
void setCurrentTexture(const char* name);

// ---------------------------------------------------------------------------------------------------------------------
// The models (any thread that reads models: the main one).

[[nodiscard]] bool isRuntime(const qmodel_t* model);
[[nodiscard]] za::U32 revision(const qmodel_t* model); // changes each time its model is made again (0: not one)

// vr_hull's narrower box (vr_hull_width) against a brush entity: Quake 2's box against convex planes (the brush's and
// its axial bevels), in its model's space from `start` to `end`; Quake's trace (the 1/32 back-off). False: not a runtime
// brush's.
bool clipBox(const qmodel_t* model, const glm::vec3& start, const glm::vec3& mins, const glm::vec3& maxs,
    const glm::vec3& end, trace_t& trace);

// The client's brush faces whose box meets lo..hi (world space: the entities as drawn), as polygons (world space,
// clockwise seen from outside) with their outward normals: the decals laid on them.
void facesIn(const glm::vec3& lo, const glm::vec3& hi,
    za::FunctionRef<void(const za::Vector<glm::vec3>& poly, const glm::vec3& normal)> f);
// The nearest brush face along the line (the client's entities): where, its normal, how far along (0..1); false: none.
bool traceLine(const glm::vec3& start, const glm::vec3& end, glm::vec3& where, glm::vec3& normal, float& fraction);

// ---------------------------------------------------------------------------------------------------------------------
// Hooks.

void registerCommands();
// A new world (vr_progs.cpp resetServerWorld): the brushes gone with the map.
void reset();
// An entity removed (vr_builtins.cpp onEdictFree): its brush goes with it.
void edictFreed(int num);
// Once a host frame (vr_main.cpp): the slots written for the brushes made or changed (their light found), entities
// left from a saved game without their brush (brushes aren't saved yet) removed.
void frame();

} // namespace qvr::brushes
