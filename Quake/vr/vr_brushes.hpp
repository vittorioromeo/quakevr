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

// The shapes the tool makes (vr_toolgun_brush_shape), each filling a box.
enum class Shape : int
{
    Box,
    Wedge,    // a ramp: its slope rising from the low end's bottom edge to the high end's top edge
    Cylinder, // a prism of `sides` faces round the up axis (its polygon stretched to the box)
    Pyramid,  // `sides` faces from the base's polygon to the top's middle
    Count
};
[[nodiscard]] const char* shapeName(Shape s);
[[nodiscard]] Shape shapeFromName(const char* name); // a name or a number; Box if neither
// A `s` filling lo..hi: its up (the cylinder's and pyramid's axis, the wedge's height) along axis `upAxis` the way
// `upSign` (the base on the other side), the wedge rising along `riseAxis` the way `riseSign` (another axis); a cylinder
// or pyramid of `sides` (3..12) faces round it. Every face `texture`, aligned to the world.
[[nodiscard]] Brush shape(Shape s, const glm::dvec3& lo, const glm::dvec3& hi, int upAxis, double upSign, int riseAxis,
    double riseSign, int sides, const char* texture);
// The turn of Quake's angles (pitch yaw roll, degrees) as a brush model's turn it (held::axesFromAngles(.., true)'s:
// columns forward, left, up), in doubles (a quarter turn exact).
[[nodiscard]] glm::dmat3 turnFromAngles(const glm::dvec3& angles);
// Turned by `r` (a rotation) about `pivot`. `lockTexture`: the textures turn with the faces (as they lay on them);
// else each face gets the world's alignment (Quake's default axes for its new way).
void rotate(Brush& b, const glm::dmat3& r, const glm::dvec3& pivot, bool lockTexture);
// Stretched from its bounds lo..hi to lo2..hi2 (each axis on its own; the textures stay where the world puts them).
void scaleTo(Brush& b, const glm::dvec3& lo, const glm::dvec3& hi, const glm::dvec3& lo2, const glm::dvec3& hi2);
// Each face's polygon (world space, clockwise seen from outside; faces without area left out), with its face's index.
void forEachPolygon(const Brush& b, za::FunctionRef<void(int face, const glm::dvec3* points, int count)> f);
// Whether every face's normal is one of the world's axes (a box, or a box's kin: the Resize handles move its faces).
[[nodiscard]] bool axial(const Brush& b);
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

// Made (a new id, its entity spawned), returning the id; 0 if refused (said on the console: why). `wantId`: that id
// if it is free (undo, a saved game's brushes), else a new one.
int create(const Brush& b, int wantId = 0);
// The last create, change or removal undone (this map's, up to 64 back); false: nothing to undo. Said on the console.
bool undo();
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
// Keeping them: a saved game's lines and the brush files (quakevr/brushes/<map>.brushes).
//
// The text of all of them (a brush file's): `// qvrbrushes 1 map <name> bsp <the map's planes' hash>`, then each brush's
// .map text (its header with its id). Plain text, hand-editable; a brush with no header gets a new id.
void allToText(za::String& out);
// Brushes from such a text (or any run of .map brushes): made (`replace`: the map's own removed first; their ids kept
// where free). The number made; `warning` set if the text is another map's (made all the same).
int fromAllText(za::StringView text, bool replace, za::String& warning);
// The lines a saved game keeps (`// qvrb ` and a line of allToText, after its model list: comments COM_Parse skips);
// empty without brushes. Valid until the next call (SaveData_Fill, the main thread).
[[nodiscard]] const char* saveLines();
// A saved game being loaded: its brush lines read (VR_ReadSaveInfo, before the old game ends), and its brushes made again
// once its entities are back (VR_OnLoadGame): each on its own entity (its model's name has its id), their ids kept.
void readSaveLines(const char* saveText);
void restoreAfterLoad();

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
// clockwise seen from outside) with their outward normals and their brush's id: the decals laid on them.
void facesIn(const glm::vec3& lo, const glm::vec3& hi,
    za::FunctionRef<void(const za::Vector<glm::vec3>& poly, const glm::vec3& normal, int id)> f);
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
