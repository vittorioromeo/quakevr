// vr_brushlight.hpp -- the runtime brushes' light (docs/vr-port/BRUSH_TOOL.md 3.4; vr_brushes.cpp): each face's lightmap
// laid out in its brush's rectangle of the lightmap (r_brush.c's reserve, QVR_BRUSH_SLOT_LMW x LMH), lit at once with
// the light the map has where the face is (one colour), then baked in the game, a few milliseconds a frame:
//
// - the map's light entities as Quake's and ericw-tools' light computes them ("light", "wait", "delay", "_color",
//   spotlights by "mangle" or "target", "_anglescale"), each traced from the luxel to the light through the world's hull 0
//   and the other brushes (their shadows); each light in its own lightstyle's map (up to four a face, as Quake's), so a
//   brush in a flickering room flickers with it and a switched light's share comes and goes with the switch;
// - the map's sunlight ("_sunlight", "_sun_mangle", "_sunlight_color"), traced to the sky, and its minimum ("light",
//   "_minlight");
// - what the map's own light has where the face is that its lights don't explain (bounce, the sky's dome, glowing
//   textures, a relit map's extras): the world's light under a point off the face's middle less the same lights' sum at
//   that spot of the floor, added to the face (vr_brush_bake_fill).
//
// The light directions (deluxemaps) are the lights' (weighted by what each gives), in the world shader's frame. A
// brush's bake is dropped if it changed meanwhile (its light serial), and made again when a brush near it changes (its
// shadows). The bake's light values are light.exe's halved (its rangescale: a lightmap byte of 128 is full light).

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Vocabulary/FunctionRef.hpp"

namespace qvr::brushlight
{

constexpr int rectWidth = QVR_BRUSH_SLOT_LMW, rectHeight = QVR_BRUSH_SLOT_LMH;

// A face to light (world space): its plane (outward normal), its polygon, its texture's axes as a texinfo has them (s =
// dot(p, s.xyz) + s.w: the luxels lie on that grid, as Quake's).
struct Face
{
    glm::dvec3 normal{0.0, 0.0, 1.0};
    double dist{0.0};
    za::Vector<glm::dvec3> poly;
    glm::dvec4 s{1.0, 0.0, 0.0, 0.0}, t{0.0, -1.0, 0.0, 0.0};
};

// A face's lightmap in its brush's rectangle: w x h luxels a style, the styles side by side, `step` texture units apart,
// luxel (0, 0) at s0, t0.
struct Map
{
    int x{0}, y{0}, w{1}, h{1};
    double step{16.0}, s0{0.0}, t0{0.0};
    za::U8 styles[4]{0, 255, 255, 255};
    int numStyles{1};
};

// A rectangle's texels: the lightmap's (rectWidth x rectHeight, RGBA) and the light directions' (the lux's).
struct Texels
{
    za::Vector<unsigned> light, lux;
};

// The faces' maps laid out in the rectangle: their luxels vr_brush_luxel texture units apart (doubled until they all
// fit), each face's styles those of the map's lights that may reach it (0 first, up to four).
void layout(const za::Vector<Face>& faces, za::Vector<Map>& out);
// The maps (their sizes and styles as they are) placed in the rectangle on shelves, the tallest first; false: no room.
bool pack(za::Vector<Map>& maps);
// The light the map has where a face is, one colour (lightmap units): the world's light under a point 4 units off its
// middle (as a model there is lit), every lightstyle at its average (a box made in a flickering room is neither left as
// bright nor as dark as the frame it was made in), shaded by its way (vr_brush_shade). The client's world (main thread).
[[nodiscard]] glm::vec3 flatLight(const Face& f);
// Every face's map one colour (its first style's; the others dark), its light straight on.
void fill(const za::Vector<Map>& maps, const za::Vector<glm::vec3>& colours, Texels& out);

// The bakes (the main thread; the listen server's world and the client's light).
// Brush `id` baked (again): its faces and maps as they are now, `serial` handed back with the result (a stale one is
// the caller's to drop). Queued after the others (one already queued for it: replaced).
void queue(int id, za::U32 serial, const za::Vector<Face>& faces, const za::Vector<Map>& maps);
void cancel(int id);
void clear(); // (a new world)
// The queued work for up to `ms` milliseconds; each bake done handed to `done`.
void run(double ms, za::FunctionRef<void(int id, za::U32 serial, const Texels& texels)> done);
[[nodiscard]] int pending();
// The map's lights as the bake has them (vr_brush_lights): how many, the sun, the minimum; with a point, the nearest few;
// `styled`: every one of a lightstyle.
void listLights(const glm::vec3* nearPoint, bool styled);

} // namespace qvr::brushlight
