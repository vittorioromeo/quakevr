// vr_adweapons.cpp -- see vr_adweapons.hpp. AD support.

#include "vr_mdlbuild.hpp"
#include "vr_bytes.hpp"
#include "vr_adweapons.hpp"

#include "vr_backend.hpp"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_gunvariants.hpp"
#include "vr_mem.hpp"
#include "vr_tunables.hpp"
#include "vr_hands.hpp"

#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strstr.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <stdio.h>
#include <string.h>

namespace qvr::adweapons
{
namespace
{

constexpr za::SizeT headerSize = 84;
constexpr int frameCount = 9;  // the Quake VR weapons' (their animations' frame numbers stay in range)
constexpr byte capColour = 2;  // the holes' fans: Quake's palette's dark grey (as the guns' dark metal)
constexpr int capRows = 4;     // its texels, under everything else in the skin
constexpr float capWeld = 0.05f; // the holes found on the surface welded this close (the rounding's hairline cracks closed)
constexpr int noteBins = 8;      // vr_adweapon_models: a made part's top in this many slices along x (Parts::note)
// The Widowmaker's turning barrels (his third notes, 2026-10-19: a Gatling auto shotgun): their axis (y 0, z spinZ, along
// x), the barrels' centres spinR from it; spinPoses frames after the still ones, a whole turn (each barrel its own
// texels), spinStep degrees apart (the frames blended between: 0.7% in at the middle, unseen).
constexpr float spinZ = 6.8f, spinR = 2.f;
constexpr int spinPoses = 27;
constexpr float spinStep = 360.f / static_cast<float>(spinPoses);

// A source model as measured (another release: not made).
struct Source
{
    const char* path;
    int numVerts, numTris, skinW, skinH;
    // Dwell support: the owned pack whose own file it is, read in place ("owned/<folder>/<path>": vr_gamedir.cpp
    // VR_OwnedFile), never another mod's file of the name (Dwell's v_shot3.mdl is AD's Widowmaker's name). Null: the
    // search paths' first of these counts.
    const char* folder{nullptr};
};

// A rectangle of the made skin's texels (AD's skin is its first rows), laid on a face: s across it and t along the
// weapon's x (swap: s along x, t across).
struct Rect
{
    float s0, t0, s1, t1;
    bool swap;
};
constexpr Rect noRect{0.f, 0.f, 0.f, 0.f, false};
constexpr const char* missingSourceWhy = "AD's model (the release measured) not found"; // (AD's data missing: no fault)

struct Parts;
struct SkinPatch;
void widowmakerParts(Parts& p);
void plasmaParts(Parts& p);
void rotaryParts(Parts& p);
void rotaryGauntletParts(Parts& p);
void lanceParts(Parts& p);

// One weapon: AD's view model in one pose (`frame`), its pieces holding `drop` taken off, laid into the Quake VR weapon's
// frame (src axis/up -> dst axis/up, scaled by k, srcPivot onto dstPivot), cut to dot(clipN, p) >= clipD, the Quake VR
// model's pieces holding `graftSeeds` put on as they are, the holes closed (the loop whose middle is within 3 units of
// apexNear fanned from apexAt, the others from their middles); packed from `corner` (the Quake VR model's).
struct Recipe
{
    const char* name;
    Source source;
    const char* frame;
    glm::vec3 srcAxis, srcUp, dstAxis, dstUp;
    float k;
    glm::vec3 srcPivot, dstPivot;
    int drop[3];
    bool clip;
    glm::vec3 clipN;
    float clipD;
    Source graft;
    int graftSeeds[3];
    glm::vec3 corner;
    bool apex;
    glm::vec3 apexNear, apexAt;
    const char* standIn;
    bool takeSource;        // AD's triangles laid in (the Widowmaker: only its skin; every part made)
    Rect cutCap;            // the cut's cap: these texels laid flat on it (noRect: the dark fan)
    void (*parts)(Parts&);  // the parts made of simple shapes in AD's texels (null: none)
    int liftSeed;           // a grafted piece (its vertex; -1 none) moved by `lift` onto the body
    glm::vec3 lift;
    // Dwell support: the source's own turning barrels (its pieces wholly at or past x spinFromX, in the source's frame),
    // turned in the spin frames about the axis through spinAxisAt (the source's frame) along the weapon's x; spinBarrels
    // of them ("quad0".. frames: 4). 0: none (the Widowmaker's made barrels mark themselves: widowmakerParts).
    int spinBarrels{0};
    float spinFromX{0.f};
    glm::vec3 spinAxisAt{0.f};
    int liftAlso{-1}; // a second grafted piece moved by `lift` (-1 none)
    // Dwell support (dwellwpn2, the Crystal Lance from Dwell's world model): its open pieces (single faces, as Dwell draws
    // them) kept open, no hole closed; and a model past the base model's corner moved so that, at the slot's Scale
    // (keepScale, about the packed corner: vr_view.cpp), every point draws where it would inside the base model's corner
    // (the base weapon's grip, hand and offsets keep their places). 0: placed as laid.
    bool keepOpen{false};
    float keepScale{0.f};
    // Dwell support (dwellrotary): a band of the source's skin painted over from other rows of it (null: none), and up to
    // eleven more pieces taken off (as `drop`; -1 none: a recipe that gives any lists all eleven).
    const SkinPatch* skinPatch{nullptr};
    int dropMore[11]{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
};

// Texels s0..s1 of rows t0..t1 of the made skin copied from rows further down the same columns: those before `split`
// from shiftLo rows down, the others from shiftHi (each face's band onto a like one, its seams kept where they were).
struct SkinPatch
{
    int s0, s1, t0, t1, split, shiftLo, shiftHi;
};
// Dwell's Rotary Shotgun (dwellrotary; his notes 2026-11-22: "at the bottom, I can see the muzzle flare texture"): its
// skin's muzzle flash (s 204..238, t 66..98: the dropped flash's texels) is painted inside the housing's column, on the
// rows its lower faces take (the lower inner diagonal t 43..80, the bottom 80..102). Painted over with the like rows
// of the other side (the lower outer diagonal t 102..138, the outer side 138..161).
constexpr SkinPatch rotaryFlashPatch{200, 242, 60, 102, 80, 59, 58};

// The measurements (taken on AD 1.80 patch 1's models; ROUND22.md "AD's weapons as VR models"):
// - The Shadow Axe (v_shadaxe3.mdl, frame0: AD's axe in its right hand): the arm and hand (the piece holding vertex 257)
//   off; the handle's axis (its vertices' principal axis) onto the Quake VR axe's (from (0.2 0 -3.55) along (4.2 0
//   11.55): vr_axestick.cpp), the blade's way out from it onto the axe's (0.9398 0 -0.3417); 0.85 of its size (the blade
//   12.6 units out, as the axe's 12; the handle as thick); the handle's foot half a unit up the axe's (its rim above the
//   axe's corner), 0.04 to the left (its head's side within the corner).
// - The Widowmaker (v_shot3.mdl, fire1: three barrels in a shell open below and behind, tilted 4.5 degrees down): level,
//   1.85 times (25.5 units from the breech plate to the muzzle, the super shotgun 28), the breech's bottom at the super
//   shotgun's receiver's back (1.2 0 2.8); the super shotgun's grip, trigger guard and trigger (v_shot2.mdl's pieces of
//   vertices 151, 241, 281); the open bottom fanned from its middle, flat (13.5 0 3).
// - The Plasma Gun (v_plasma.mdl, base1: level, centred): as it is, its pistol grip, trigger and stock's bar (the pieces
//   of vertices 78, 711, 409) off, the grip's place (-9 0 -13) onto the lightning gun's grip (-0.5 0 4.5), cut just
//   behind the hand (x -5.33: the stock out, the lightning gun's corner kept; the cut's face on the grid's third X byte,
//   two steps in front of the corner: room for the ammo screen's bezel and rivets, plasmaHousingSpec; it was -5.55, drawn at
//   the corner's byte); the lightning gun's grip, trigger guard and
//   trigger (v_light.mdl's pieces of vertices 234, 403, 493), the trigger 0.6 up (his notes 2026-10-19: it hung 0.48 below
//   the body, detached; vr_adweapon_models prints each grafted piece's least gap up to the body).
const Recipe recipes[] = {
    {"progs/v_shadaxe3.mdl#vr", {"progs/v_shadaxe3.mdl", 281, 292, 512, 128}, "frame0", {0.5949f, 0.2699f, 0.7571f},
        {0.6493f, 0.3939f, -0.6506f}, {0.3419f, 0.f, 0.9397f}, {0.9398f, 0.f, -0.3417f}, 0.85f, {16.1405f, -23.6240f, -36.1937f},
        {0.3710f, 0.04f, -3.0802f}, {257, -1, -1}, false, {0.f, 0.f, 0.f}, 0.f, {nullptr, 0, 0, 0, 0}, {-1, -1, -1},
        {-2.997f, -1.6813f, -3.9006f}, false, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, "progs/v_axe.mdl", true, noRect, nullptr, -1,
        {0.f, 0.f, 0.f}},
    {"progs/v_shot3.mdl#vr", {"progs/v_shot3.mdl", 206, 238, 256, 256}, "fire1", {12.85f, 0.f, -1.f}, {0.f, 0.f, 1.f},
        {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, 1.85f, {2.57f, 0.f, -8.6f}, {1.2f, 0.f, 2.8f}, {-1, -1, -1}, false, {0.f, 0.f, 0.f}, 0.f,
        {"progs/v_shot2.mdl", 0, 0, 0, 0}, {151, 241, 281}, {-15.5775f, -4.6938f, -4.2402f}, false, {0.f, 0.f, 0.f},
        {0.f, 0.f, 0.f}, "progs/v_shot2.mdl", false, noRect, widowmakerParts, -1, {0.f, 0.f, 0.f}},
    {"progs/v_plasma.mdl#vr", {"progs/v_plasma.mdl", 786, 427, 128, 128}, "base1", {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f},
        {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, 1.f, {-9.f, 0.f, -13.f}, {-0.5f, 0.f, 4.5f}, {78, 711, 409}, true, {1.f, 0.f, 0.f}, -5.33f,
        {"progs/v_light.mdl", 0, 0, 0, 0}, {234, 403, 493}, {-5.6211f, -9.3429f, -6.0018f}, false, {0.f, 0.f, 0.f},
        {0.f, 0.f, 0.f}, "progs/v_light.mdl", true, {1.f, 74.f, 31.f, 97.f, false}, plasmaParts, 493, {0.f, 0.f, 0.6f}},
    // Dwell support (DWELL.md 9.6; measured on Dwell v2.2's models, read in place from its folder): its view models have
    // no arms (nothing dropped but their muzzle flashes, pieces collapsed to a point), each laid level on the Quake VR
    // weapon it is a variant of, inside that model's corner (its slot's Scale keeps the grip's place), that weapon's grip,
    // trigger guard and trigger put on.
    // - The Rotary Shotgun (v_shot3.mdl, frame1: level, its four barrels about y -0.11 z -15.28): 0.8 of its size (32
    //   units from the housing's back to the barrels' front, the super shotgun 28), its housing's back bottom at the super
    //   shotgun's grip's top (0.6 0 2.6); its barrels, their clamps and the front block (the pieces from x 16.5) turn.
    // - The Crystal Lance (v_rail.mdl, frame1: tilted 15.9 degrees up): level, 0.88 (its blades' 20.5 span inside the
    //   lightning gun's corner), its wooden handle's back middle (11.94 -0.17 -18.17) over the lightning gun's grip
    //   (-4 0 5.8: the grip's top inside the handle). Its trigger guard and trigger 0.4 up (dwellgun: they hung 0.37
    //   under the blades' underside, 4.31 over them; vr_adweapon_models prints the least gaps).
    // Each an ammo screen housing on its rear face (rotaryParts, lanceParts). (Dwell's Rifle, v_rifle.mdl, is not made: no
    // Dwell map places it; removed 2026-11-23.)
    {"progs/dw_shot3.mdl#vr", {"progs/v_shot3.mdl", 260, 286, 256, 256, "dwell"}, "frame1", {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f},
        {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, 0.8f, {-2.88f, -0.11f, -20.71f}, {0.6f, 0.f, 2.6f}, {4, -1, -1}, false, {0.f, 0.f, 0.f},
        0.f, {"progs/v_shot2.mdl", 0, 0, 0, 0}, {151, 241, 281}, {-15.5775f, -4.6938f, -4.2402f}, false, {0.f, 0.f, 0.f},
        {0.f, 0.f, 0.f}, "progs/v_shot2.mdl", true, noRect, rotaryParts, -1, {0.f, 0.f, 0.f}, 4, 16.5f, {0.f, -0.11f, -15.28f},
        -1, false, 0.f, &rotaryFlashPatch, {0, 28, 106, 114, 172, 206, 228, 236, 241, -1, -1}},
    {"progs/dw_rail.mdl#vr", {"progs/v_rail.mdl", 311, 339, 256, 256, "dwell"}, "frame1", {23.6f, 0.f, 6.7f},
        {-6.7f, 0.f, 23.6f}, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, 0.88f, {11.94f, -0.17f, -18.17f}, {-4.f, 0.f, 6.2f}, {-1, -1, -1},
        false, {0.f, 0.f, 0.f}, 0.f, {"progs/v_light.mdl", 0, 0, 0, 0}, {234, 403, 493}, {-5.6211f, -9.3429f, -6.0018f}, false,
        {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, "progs/v_light.mdl", true, noRect, lanceParts, 403, {0.f, 0.f, 0.8f}, 0, 0.f,
        {0.f, 0.f, 0.f}, 493},
    // - The Crystal Lance as a lance held like a spear (lancespear; Dwell's world model g_rail.mdl, fr_0: a staff standing
    //   tilted 22.6 degrees off upright, 67.5 long, its pole 2.2 thick about the line through (-10.49 -0.15 9.78) along
    //   (0.3843 0 0.9231), its crystal head from 43 along it): a plain pole and its head, nothing put on (no handle, no
    //   trigger), laid as the axe's handle lies in the fist (v_axe.mdl: along (0.3419 0 0.9397), vr_axestick.cpp), 1.5
    //   times (101 long: 1.35 m at the slot's 0.34), the pole's point 20 along it (16 from the butt) where the axe's handle
    //   goes through the fist (1.72 0 0.63); then moved past the axe's corner (keepScale) so that slot 55's hand values,
    //   the axe's, close the fist round the pole. Its open faces kept open, as Dwell draws them.
    {"progs/dw_lance.mdl#vr", {"progs/g_rail.mdl", 672, 464, 256, 256, "dwell"}, "fr_0", {0.3843f, 0.f, 0.9231f},
        {-0.9231f, 0.f, 0.3843f}, {0.3419f, 0.f, 0.9397f}, {-0.9397f, 0.f, 0.3419f}, 1.5f, {-4.73f, -0.15f, 23.63f},
        {1.72f, 0.f, 0.63f}, {-1, -1, -1}, false, {0.f, 0.f, 0.f}, 0.f, {nullptr, 0, 0, 0, 0}, {-1, -1, -1},
        {-2.997f, -1.6813f, -3.9006f}, false, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, "progs/v_axe.mdl", true, noRect, nullptr,
        -1, {0.f, 0.f, 0.f}, 0, 0.f, {0.f, 0.f, 0.f}, -1, true, 0.34f},
    // - The Rotary Shotgun as a gauntlet (dwellwpn3; his notes 2026-11-21: "the same sort of grip as the super nailgun,
    //   so you put your hand inside it"): Dwell's view model as above, laid round the super nailgun's hand: as it is (1:
    //   40 long, the super nailgun 42.7; its housing 19 long, 13 wide and 11 tall, the super nailgun's 18 x 12 x 15), its
    //   housing's back bottom on the super nailgun's back (-2.6 0 -5.5: the barrels' axis on z 0, the hand's place 6.8
    //   -0.05 -1 inside the housing), nothing grafted (the hand holds inside it), packed from the super nailgun's corner so
    //   slot 56's hand values, the super nailgun's, hold it. Its window on the back and its shell port on the inner side
    //   (rotaryGauntletParts).
    {"progs/dw_rotary.mdl#vr", {"progs/v_shot3.mdl", 260, 286, 256, 256, "dwell"}, "frame1", {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, 1.f, {-2.88f, -0.11f, -20.71f}, {-2.6f, 0.f, -5.5f}, {4, -1, -1},
        false, {0.f, 0.f, 0.f}, 0.f, {"progs/v_nail2.mdl", 0, 0, 0, 0}, {-1, -1, -1}, {-5.1024f, -7.6303f, -8.4464f},
        false, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, "progs/v_nail2.mdl", true, noRect, rotaryGauntletParts, -1,
        {0.f, 0.f, 0.f}, 4, 16.5f, {0.f, -0.11f, -15.28f}, -1, false, 0.f, &rotaryFlashPatch,
        {0, 11, 129, 28, 106, 114, 172, 206, 228, 236, 241}},
};

// The Plasma Gun's cell (the lightning gun's magazine and well, made by make_mags.py in v_light.mdl's space: the cell
// hangs below its seat) turned a quarter about x so that it stands out of the gun's inner side (+y: towards the body
// held in the right hand; mirrored in the left), its seat moved into the receiver made on that side (plasmaParts). Named
// as the parts of "progs/v_plasma.mdl#vr" (vr_weapons.cpp slotForPart: the Plasma Gun's slot, its Scale and offsets;
// vr_view.cpp magMounts, loadPorts).
struct SidePart
{
    const char* name;
    const char* from;
};
constexpr SidePart sideParts[] = {
    {"progs/vr_mag_on_v_plasma.mdl#vr", "progs/vr_mag_on_v_light.mdl"},
    {"progs/vr_magwell_on_v_plasma.mdl#vr", "progs/vr_magwell_on_v_light.mdl"},
};
constexpr glm::vec3 lightSeat{11.6f, 0.f, 3.f};  // the lightning gun's (vr_view.cpp magMounts)
// The cell's top, inside the gun; its middle 3.6 further out (+y). (His notes 2026-10-18: 0.6 further out than it was,
// so that it shows a little more; vr_view.cpp magMounts, vr_view_internal.hpp loadPorts.)
constexpr glm::vec3 plasmaSeat{11.6f, 2.1f, 4.5f};

void put(za::Vector<byte>& out, int v)
{
    const int le = LittleLong(v);
    out.emplaceBackRange(reinterpret_cast<const byte*>(&le), 4);
}

void putf(za::Vector<byte>& out, float v)
{
    const float le = LittleFloat(v);
    out.emplaceBackRange(reinterpret_cast<const byte*>(&le), 4);
}

// A model read: one pose's positions, its texture coordinates and triangles, its first skin.
struct Mesh
{
    int skinW{0}, skinH{0};
    za::Vector<byte> skin;
    za::Vector<glm::vec3> pos;
    za::Vector<int> st;   // onseam, s, t per vertex
    za::Vector<int> tris; // facesfront, a, b, c per triangle
};

// `bytes` as `src` measures it; the pose named `frame` (null: the first).
[[nodiscard]] bool parse(const za::Vector<byte>& bytes, const Source& src, const char* frame, Mesh& m)
{
    const byte* b = bytes.data();
    const za::SizeT size = bytes.size();
    if(size < headerSize || qvr::bytes::le32(b, 0) != IDPOLYHEADER || qvr::bytes::le32(b, 4) != ALIAS_VERSION || qvr::bytes::le32(b, 48) < 1)
    {
        return false;
    }
    // A Source of counts 0 is Quake VR's own model (a graft's donor): whatever its art is now, its counts read from it (its
    // pieces found by their seed vertices, which its art's edits keep: new vertices are appended). Exact counts pin AD's
    // release measured. (Pinned donors broke the Widowmaker on 2026-11-06 and the Plasma Gun on 2026-11-08 when the super
    // shotgun's and the lightning gun's sights were redone: both fell back to their stand-ins.)
    const bool any = src.numVerts == 0;
    const int skinW = any ? qvr::bytes::le32(b, 52) : src.skinW, skinH = any ? qvr::bytes::le32(b, 56) : src.skinH;
    const int nv = any ? qvr::bytes::le32(b, 60) : src.numVerts, nt = any ? qvr::bytes::le32(b, 64) : src.numTris;
    if(qvr::bytes::le32(b, 52) != skinW || qvr::bytes::le32(b, 56) != skinH || qvr::bytes::le32(b, 60) != nv || qvr::bytes::le32(b, 64) != nt ||
        skinW <= 0 || skinH <= 0 || nv <= 0 || nt <= 0 || skinW > 4096 || skinH > 4096 || nv > 65536 || nt > 65536)
    {
        return false;
    }
    const int numSkins = qvr::bytes::le32(b, 48), numFrames = qvr::bytes::le32(b, 68);
    const glm::vec3 scale{qvr::bytes::lef32(b, 8), qvr::bytes::lef32(b, 12), qvr::bytes::lef32(b, 16)}, origin{qvr::bytes::lef32(b, 20), qvr::bytes::lef32(b, 24), qvr::bytes::lef32(b, 28)};
    const za::SizeT skinBytes = static_cast<za::SizeT>(skinW) * static_cast<za::SizeT>(skinH);
    za::SizeT at = headerSize;
    m.skin.clear();
    for(int s = 0; s < numSkins; s++)
    {
        if(at + 8 > size)
        {
            return false;
        }
        const byte* picture = nullptr;
        if(qvr::bytes::le32(b, at) == 0)
        {
            picture = b + at + 4;
            at += 4 + skinBytes;
        }
        else
        {
            const int n = za::max(qvr::bytes::le32(b, at + 4), 1);
            picture = b + at + 8 + static_cast<za::SizeT>(n) * 4;
            at += 8 + static_cast<za::SizeT>(n) * 4 + static_cast<za::SizeT>(n) * skinBytes;
        }
        if(s == 0)
        {
            if(picture + skinBytes > b + size)
            {
                return false;
            }
            m.skin.emplaceBackRange(picture, skinBytes);
        }
    }
    if(at + static_cast<za::SizeT>(nv) * 12 + static_cast<za::SizeT>(nt) * 16 > size)
    {
        return false;
    }
    m.skinW = skinW;
    m.skinH = skinH;
    m.st.clear();
    for(int i = 0; i < nv * 3; i++)
    {
        m.st.pushBack(qvr::bytes::le32(b, at + static_cast<za::SizeT>(i) * 4));
    }
    at += static_cast<za::SizeT>(nv) * 12;
    m.tris.clear();
    for(int i = 0; i < nt * 4; i++)
    {
        const int v = qvr::bytes::le32(b, at + static_cast<za::SizeT>(i) * 4);
        if(i % 4 && (v < 0 || v >= nv))
        {
            return false;
        }
        m.tris.pushBack(v);
    }
    at += static_cast<za::SizeT>(nt) * 16;
    za::SizeT poseAt = 0;
    for(int f = 0; f < numFrames && !poseAt; f++)
    {
        if(at + 4 > size)
        {
            return false;
        }
        const int type = qvr::bytes::le32(b, at);
        at += 4;
        int members = 1;
        if(type != 0)
        {
            members = za::max(qvr::bytes::le32(b, at), 1);
            at += 4 + 8 + static_cast<za::SizeT>(members) * 4;
        }
        for(int k = 0; k < members && !poseAt; k++)
        {
            const za::SizeT nameAt = at + 8, vertsAt = at + 24;
            at = vertsAt + static_cast<za::SizeT>(nv) * 4;
            if(at > size)
            {
                return false;
            }
            char name[17] = {};
            ZA_MEMCPY(name, b + nameAt, 16);
            if(!frame || !ZA_STRCMP(name, frame))
            {
                poseAt = vertsAt;
            }
        }
    }
    if(!poseAt)
    {
        return false;
    }
    m.pos.clear();
    for(int v = 0; v < nv; v++)
    {
        const byte* p = b + poseAt + static_cast<za::SizeT>(v) * 4;
        m.pos.pushBack(glm::vec3{p[0] * scale.x + origin.x, p[1] * scale.y + origin.y, p[2] * scale.z + origin.z});
    }
    return true;
}

[[nodiscard]] bool readFile(FILE* f, long offset, long length, za::Vector<byte>& out)
{
    if(length <= 0 || length > 16 * 1024 * 1024)
    {
        return false;
    }
    out.resize(static_cast<za::SizeT>(length));
    return fseek(f, offset, SEEK_SET) == 0 && fread(out.data(), 1, out.size(), f) == out.size();
}

// The search paths' first `src.path` that is the release measured (its pak's entry or its folder's file), whatever
// shadows it in the game's lookup (AD's v_plasma.mdl is Rogue's name too, and an official pak's: vr_ad_data.inc skips
// AD's; our Thunderbolt's plasma model, which had the name too, is v_light_plasma.mdl since 2026-11-08).
[[nodiscard]] bool load(const Source& src, const char* frame, Mesh& m, unsigned int* pathId)
{
    za::Vector<byte> bytes;
    if(src.folder) // (Dwell support: its own folder's file, in place, whichever campaign or mod is active)
    {
        char owned[MAX_QPATH];
        q_snprintf(owned, sizeof(owned), "owned/%s/%s", src.folder, src.path);
        unsigned int id = 0;
        byte* f = COM_LoadMallocFile(owned, &id);
        if(!f)
        {
            return false;
        }
        bytes.resize(static_cast<za::SizeT>(com_filesize > 0 ? com_filesize : 0));
        ZA_MEMCPY(bytes.data(), f, bytes.size());
        VR_HeapFree(f);
        if(!parse(bytes, src, frame, m))
        {
            return false;
        }
        if(pathId)
        {
            *pathId = id;
        }
        return true;
    }
    for(const searchpath_t* s = com_searchpaths; s; s = s->next)
    {
        bool read = false;
        if(s->pack)
        {
            for(int i = 0; i < s->pack->numfiles && !read; i++)
            {
                const packfile_t& e = s->pack->files[i];
                if(q_strcasecmp(e.name, src.path))
                {
                    continue;
                }
                if(FILE* f = Sys_fopen(s->pack->filename, "rb"))
                {
                    read = readFile(f, e.filepos, e.filelen, bytes);
                    fclose(f);
                }
                break;
            }
        }
        else
        {
            char path[MAX_OSPATH];
            q_snprintf(path, sizeof(path), "%s/%s", s->filename, src.path);
            if(FILE* f = Sys_fopen(path, "rb"))
            {
                const bool sized = fseek(f, 0, SEEK_END) == 0;
                const long length = sized ? ftell(f) : 0;
                read = sized && readFile(f, 0, length, bytes);
                fclose(f);
            }
        }
        if(read && parse(bytes, src, frame, m))
        {
            if(pathId)
            {
                *pathId = s->path_id;
            }
            return true;
        }
    }
    return false;
}

// Vertices at one place (to a 0.001 grid) are one.
[[nodiscard]] za::Vector<int> weldExact(const za::Vector<glm::vec3>& pos)
{
    ankerl::unordered_dense::map<uint64_t, int> at;
    za::Vector<int> w;
    for(za::SizeT i = 0; i < pos.size(); i++)
    {
        const auto q = [&](float x) { return static_cast<uint64_t>(static_cast<int64_t>(x * 1000.f + (x < 0.f ? -0.5f : 0.5f)) & 0x1fffff); };
        const uint64_t key = q(pos[i].x) | (q(pos[i].y) << 21) | (q(pos[i].z) << 42);
        w.pushBack(at.try_emplace(key, static_cast<int>(i)).first->second);
    }
    return w;
}

[[nodiscard]] int findRoot(za::Vector<int>& parent, int a)
{
    while(parent[a] != a)
    {
        parent[a] = parent[parent[a]];
        a = parent[a];
    }
    return a;
}

// Per triangle of `m`: whether it is in a welded piece holding one of `seeds` (-1: none).
template <int N>
[[nodiscard]] za::Vector<bool> piecesOf(const Mesh& m, const int (&seeds)[N])
{
    const za::Vector<int> w = weldExact(m.pos);
    za::Vector<int> parent;
    for(za::SizeT i = 0; i < m.pos.size(); i++)
    {
        parent.pushBack(static_cast<int>(i));
    }
    const za::SizeT nt = m.tris.size() / 4;
    for(za::SizeT t = 0; t < nt; t++)
    {
        const int a = w[m.tris[t * 4 + 1]], b = w[m.tris[t * 4 + 2]], c = w[m.tris[t * 4 + 3]];
        parent[findRoot(parent, a)] = findRoot(parent, b);
        parent[findRoot(parent, b)] = findRoot(parent, c);
    }
    za::Vector<bool> in(nt, false);
    for(const int seed : seeds)
    {
        if(seed < 0 || seed >= static_cast<int>(m.pos.size()))
        {
            continue;
        }
        const int root = findRoot(parent, w[seed]);
        for(za::SizeT t = 0; t < nt; t++)
        {
            in[t] = in[t] || findRoot(parent, w[m.tris[t * 4 + 1]]) == root;
        }
    }
    return in;
}

// Per triangle of `m`: whether its welded piece lies wholly at or past x `fromX` (Dwell support: a source's turning
// barrels, Recipe::spinFromX).
[[nodiscard]] za::Vector<bool> piecesPast(const Mesh& m, float fromX)
{
    const za::Vector<int> w = weldExact(m.pos);
    za::Vector<int> parent;
    for(za::SizeT i = 0; i < m.pos.size(); i++)
    {
        parent.pushBack(static_cast<int>(i));
    }
    const za::SizeT nt = m.tris.size() / 4;
    for(za::SizeT t = 0; t < nt; t++)
    {
        const int a = w[m.tris[t * 4 + 1]], b = w[m.tris[t * 4 + 2]], c = w[m.tris[t * 4 + 3]];
        parent[findRoot(parent, a)] = findRoot(parent, b);
        parent[findRoot(parent, b)] = findRoot(parent, c);
    }
    za::Vector<float> least(m.pos.size(), 1e30f);
    for(za::SizeT i = 0; i < m.pos.size(); i++)
    {
        float& l = least[static_cast<za::SizeT>(findRoot(parent, w[i]))];
        l = za::min(l, m.pos[i].x);
    }
    za::Vector<bool> past(nt, false);
    for(za::SizeT t = 0; t < nt; t++)
    {
        past[t] = least[static_cast<za::SizeT>(findRoot(parent, w[m.tris[t * 4 + 1]]))] >= fromX;
    }
    return past;
}

// The model being made: positions, texels (absolute: no seams), triangles (clockwise from outside, as Quake's), and each
// vertex's smoothing group (its normal is the surface's of the vertices at its place in its group: 0 AD's and the grafted
// parts', one each for a made part's flat faces and its rounded sides, and for each hole's fan).
struct Out
{
    za::Vector<glm::vec3> pos;
    za::Vector<int> s, t;
    za::Vector<int> group;
    za::Vector<int> tris;
    za::Vector<bool> spins; // a vertex of the part that turns about spinAxis in the spin frames (the Widowmaker's barrels)
    int groups{1};
    bool spinning{false};   // the vertices made now are of it

    int vert(const glm::vec3& p, int ss, int tt, int g = 0)
    {
        pos.pushBack(p);
        s.pushBack(ss);
        t.pushBack(tt);
        group.pushBack(g);
        spins.pushBack(spinning);
        return static_cast<int>(pos.size()) - 1;
    }

    int newGroup()
    {
        return groups++;
    }
};

// The triangles of `m` marked `keep`, their positions through `xf`, their texels moved by (ds, dt) (a back face's seam
// vertex its own copy, half a skin over: the skin may change its width).
template <typename Xf>
void take(Out& out, const Mesh& m, const za::Vector<bool>& keep, bool keepMarked, Xf&& xf, int ds, int dt)
{
    za::Vector<int> remap(m.pos.size() * 2, -1);
    for(za::SizeT tri = 0; tri < m.tris.size() / 4; tri++)
    {
        if(keep[tri] != keepMarked)
        {
            continue;
        }
        const bool front = m.tris[tri * 4] != 0;
        for(int c = 0; c < 3; c++)
        {
            const int v = m.tris[tri * 4 + 1 + c];
            const bool seam = m.st[static_cast<za::SizeT>(v) * 3] != 0 && !front;
            int& r = remap[static_cast<za::SizeT>(v) * 2 + (seam ? 1 : 0)];
            if(r < 0)
            {
                const int ss = m.st[static_cast<za::SizeT>(v) * 3 + 1] + (seam ? m.skinW / 2 : 0);
                r = out.vert(xf(m.pos[v]), ss + ds, m.st[static_cast<za::SizeT>(v) * 3 + 2] + dt);
            }
            out.tris.pushBack(r);
        }
    }
}

// The triangles from `first` on cut to dot(n, p) >= d.
void clip(Out& out, const glm::vec3& n, float d, za::SizeT first)
{
    za::Vector<int> tris;
    for(za::SizeT i = first; i < out.tris.size(); i++)
    {
        tris.pushBack(out.tris[i]);
    }
    out.tris.resize(first);
    ankerl::unordered_dense::map<uint64_t, int> cuts;
    const auto dist = [&](int v) { return glm::dot(n, out.pos[v]) - d; };
    const auto cut = [&](int a, int b) { // a inside, b outside
        const uint64_t key = (static_cast<uint64_t>(a) << 32) | static_cast<uint32_t>(b);
        if(const auto it = cuts.find(key); it != cuts.end())
        {
            return it->second;
        }
        const float da = dist(a), db = dist(b), k = da / (da - db);
        const int v = out.vert(out.pos[a] + (out.pos[b] - out.pos[a]) * k,
            out.s[a] + static_cast<int>(static_cast<float>(out.s[b] - out.s[a]) * k + 0.5f),
            out.t[a] + static_cast<int>(static_cast<float>(out.t[b] - out.t[a]) * k + 0.5f), out.group[a]);
        cuts[key] = v;
        return v;
    };
    for(za::SizeT i = 0; i + 2 < tris.size(); i += 3)
    {
        const int v[3] = {tris[i], tris[i + 1], tris[i + 2]};
        const bool in[3] = {dist(v[0]) >= 0.f, dist(v[1]) >= 0.f, dist(v[2]) >= 0.f};
        const int count = in[0] + in[1] + in[2];
        if(count == 3)
        {
            out.tris.pushBack(v[0]);
            out.tris.pushBack(v[1]);
            out.tris.pushBack(v[2]);
            continue;
        }
        for(int r = 0; r < 3 && count > 0; r++)
        {
            const int a = v[r], b = v[(r + 1) % 3], c = v[(r + 2) % 3];
            if(count == 1 && in[r])
            {
                const int ab = cut(a, b), ac = cut(a, c);
                out.tris.pushBack(a);
                out.tris.pushBack(ab);
                out.tris.pushBack(ac);
                break;
            }
            if(count == 2 && !in[r])
            {
                const int ba = cut(b, a), ca = cut(c, a);
                out.tris.pushBack(ba);
                out.tris.pushBack(b);
                out.tris.pushBack(c);
                out.tris.pushBack(ba);
                out.tris.pushBack(c);
                out.tris.pushBack(ca);
                break;
            }
        }
    }
}

// Vertices within `tol` of each other are one (the first of them).
[[nodiscard]] za::Vector<int> weldNear(const za::Vector<glm::vec3>& pos, float tol)
{
    za::Vector<int> w;
    for(za::SizeT i = 0; i < pos.size(); i++)
    {
        int to = static_cast<int>(i);
        for(za::SizeT j = 0; j < i; j++)
        {
            if(w[j] == static_cast<int>(j) && za::abs(pos[i].x - pos[j].x) <= tol && za::abs(pos[i].y - pos[j].y) <= tol &&
                za::abs(pos[i].z - pos[j].z) <= tol)
            {
                to = static_cast<int>(j);
                break;
            }
        }
        w.pushBack(to);
    }
    return w;
}

[[nodiscard]] uint64_t edgeKey(int a, int b)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(a)) << 32) | static_cast<uint32_t>(b);
}

// The surface's holes: the boundary loops (each the welded vertices in order, the hole's edges reversed), split where a
// walk comes back to a vertex. The degenerate triangles (two corners welded) dropped first. `open`: the boundary edges.
[[nodiscard]] za::Vector<za::Vector<int>> holes(Out& out, const za::Vector<int>& w, int& open)
{
    za::Vector<int> kept;
    for(za::SizeT i = 0; i + 2 < out.tris.size(); i += 3)
    {
        const int a = w[out.tris[i]], b = w[out.tris[i + 1]], c = w[out.tris[i + 2]];
        if(a != b && b != c && a != c)
        {
            kept.pushBack(out.tris[i]);
            kept.pushBack(out.tris[i + 1]);
            kept.pushBack(out.tris[i + 2]);
        }
    }
    out.tris = kept;
    ankerl::unordered_dense::map<uint64_t, int> count;
    for(za::SizeT i = 0; i + 2 < out.tris.size(); i += 3)
    {
        for(int c = 0; c < 3; c++)
        {
            count[edgeKey(w[out.tris[i + c]], w[out.tris[i + (c + 1) % 3]])]++;
        }
    }
    ankerl::unordered_dense::map<int, za::Vector<int>> outs; // boundary half-edges b -> a, sorted for a stable walk
    za::Vector<uint64_t> keys;
    for(const auto& [key, n] : count)
    {
        keys.pushBack(key);
    }
    for(za::SizeT i = 1; i < keys.size(); i++) // (insertion sort: a few thousand edges, once)
    {
        for(za::SizeT j = i; j > 0 && keys[j - 1] > keys[j]; j--)
        {
            const uint64_t k = keys[j];
            keys[j] = keys[j - 1];
            keys[j - 1] = k;
        }
    }
    open = 0;
    za::Vector<int> starts;
    for(const uint64_t key : keys)
    {
        const int a = static_cast<int>(key >> 32), b = static_cast<int>(key & 0xffffffffu);
        const auto twin = count.find(edgeKey(b, a));
        const int extra = count[key] - (twin == count.end() ? 0 : twin->second);
        for(int i = 0; i < extra; i++)
        {
            outs[b].pushBack(a);
            starts.pushBack(b);
            open++;
        }
    }
    za::Vector<za::Vector<int>> loops;
    for(const int s : starts)
    {
        while(!outs[s].empty())
        {
            za::Vector<int> walk;
            walk.pushBack(s);
            int v = outs[s].back();
            outs[s].popBack();
            while(v != s && !outs[v].empty())
            {
                walk.pushBack(v);
                const int next = outs[v].back();
                outs[v].popBack();
                v = next;
            }
            if(v != s)
            {
                continue; // no way back: left
            }
            za::Vector<int> stack;
            for(const int x : walk)
            {
                za::SizeT at = stack.size();
                for(za::SizeT i = 0; i < stack.size(); i++)
                {
                    if(stack[i] == x)
                    {
                        at = i;
                        break;
                    }
                }
                if(at < stack.size())
                {
                    za::Vector<int> part;
                    for(za::SizeT i = at; i < stack.size(); i++)
                    {
                        part.pushBack(stack[i]);
                    }
                    if(part.size() >= 3)
                    {
                        loops.pushBack(part);
                    }
                    stack.resize(at + 1);
                }
                else
                {
                    stack.pushBack(x);
                }
            }
            if(stack.size() >= 3)
            {
                loops.pushBack(stack);
            }
        }
    }
    return loops;
}

// Every hole fanned shut (dark texels at (capS, capT)); how many.
int seal(Out& out, const Recipe& r, int capS, int capT)
{
    const za::Vector<int> w = weldNear(out.pos, capWeld);
    int open = 0;
    const za::Vector<za::Vector<int>> loops = holes(out, w, open);
    for(const za::Vector<int>& loop : loops)
    {
        glm::vec3 middle{0.f};
        for(const int v : loop)
        {
            middle += out.pos[v];
        }
        middle /= static_cast<float>(loop.size());
        const glm::vec3 apex = r.apex && glm::length(middle - r.apexNear) < 3.f ? r.apexAt : middle;
        // The cut's cap (every corner on the cutting plane): the recipe's texels laid flat on it (y across, z up).
        bool onCut = r.clip && r.cutCap.s1 > r.cutCap.s0;
        glm::vec2 lo{1e30f}, hi{-1e30f};
        for(const int v : loop)
        {
            onCut = onCut && za::abs(glm::dot(r.clipN, out.pos[v]) - r.clipD) < 0.01f;
            lo = glm::min(lo, glm::vec2{out.pos[v].y, out.pos[v].z});
            hi = glm::max(hi, glm::vec2{out.pos[v].y, out.pos[v].z});
        }
        const auto texel = [&](const glm::vec3& p, int axis) {
            if(!onCut)
            {
                return axis == 0 ? capS : capT;
            }
            const glm::vec2 f = (glm::vec2{p.y, p.z} - lo) / glm::max(hi - lo, glm::vec2{1e-3f});
            return axis == 0 ? static_cast<int>(r.cutCap.s0 + (r.cutCap.s1 - r.cutCap.s0) * f.x + 0.5f)
                             : static_cast<int>(r.cutCap.t1 - (r.cutCap.t1 - r.cutCap.t0) * f.y + 0.5f);
        };
        const int g = out.newGroup(); // (flat: never smoothed with the surface round it)
        const int a = out.vert(apex, texel(apex, 0), texel(apex, 1), g);
        const za::SizeT first = out.pos.size();
        for(const int v : loop)
        {
            const glm::vec3 p = out.pos[v];
            out.vert(p, texel(p, 0), texel(p, 1), g);
        }
        for(za::SizeT i = 0; i < loop.size(); i++)
        {
            out.tris.pushBack(static_cast<int>(first + i));
            out.tris.pushBack(static_cast<int>(first + (i + 1) % loop.size()));
            out.tris.pushBack(a);
        }
    }
    return static_cast<int>(loops.size());
}

struct Built
{
    za::Vector<byte> file;
    int verts{0}, tris{0}, sealed{0}, openAfter{0}, grafted{0}, dropped{0}, made{0};
    glm::vec3 lo{0.f}, hi{0.f};
    bool cornerKept{false};
    glm::vec3 muzzle{0.f};          // a gun's: the middle of its front end; the axe's: the blade edge's top
    glm::vec3 edgeLo{0.f}, edgeHi{0.f}; // the axe's blade edge's ends (vr_axestick.cpp's table)
    glm::vec3 seedLo[3]{}, seedHi[3]{};  // each grafted piece's box (Recipe::graftSeeds; empty: none)
    float adAbove[3]{};                  // and the body's surface straight over its top (1e30: none)
    float adGap[3]{};                    // the least gap up to it
    const char* noteName{nullptr};       // a made part's box (Parts::note)
    glm::vec3 noteLo{0.f}, noteHi{0.f};
    float noteTop[noteBins]{};
    int spinVerts{0};                    // the vertices turned in the spin frames (spinFrames)
    glm::vec3 spinAxis{0.f};             // a point of their axis (along x)
    bool housing{false};                 // an ammo screen housing made (Parts::housing*)
    glm::vec3 housingGlass{0.f};         // its glass's middle as designed
    glm::vec2 housingWindow{0.f};        // its window's width and height
    int housingOnFace{0};                // its outline's corners on the gun's face behind it (of 4)
};

[[nodiscard]] const Recipe* recipeOf(const char* name)
{
    for(const Recipe& r : recipes)
    {
        if(!q_strcasecmp(r.name, name))
        {
            return &r;
        }
    }
    return nullptr;
}

// The basis of `axis` and the part of `up` across it: x along the axis, z up, y = z x x.
void basis(const glm::vec3& axis, const glm::vec3& up, glm::vec3& x, glm::vec3& y, glm::vec3& z)
{
    x = glm::normalize(axis);
    z = glm::normalize(up - x * glm::dot(up, x));
    y = glm::cross(z, x);
}

// What a recipe's made parts are laid with: the model being made, the Quake VR model the grip came from (its pieces laid
// again: take) and its texels' move, the dark texels (the caps' grey, the bores' black), and the muzzle they make.
struct Parts
{
    Out& o;
    const Mesh& graft;
    int graftDs, graftDt;
    int capS, capT, boreS, boreT;
    glm::vec3 muzzle{0.f};
    bool muzzleSet{false};
    // A made part's box and the made model's surface over its top (vr_adweapon_models: how it meets what is above it).
    const char* noteName{nullptr};
    glm::vec3 noteLo{0.f}, noteHi{0.f};
    float noteTop[noteBins]{};
    glm::vec3 corner{0.f}; // the recipe's: the model is packed from it (its grid: a made part laid on it, the housing)
    // The recipe's laying of the source (a source point q is drawn at layTo + (q - layFrom) * layK: its axes kept, as the
    // Rotary Shotgun's recipes have them; dwBarrels).
    float layK{1.f};
    glm::vec3 layFrom{0.f}, layTo{0.f};
    // An ammo screen housing made (vr_adweapon_models prints it: the table entry's numbers, vr_screenstyle.cpp).
    bool housing{false};
    glm::vec3 housingGlass{0.f};
    glm::vec2 housingWindow{0.f};
    int housingOnFace{0};
};

// The box of the vertices from `from` on as the part `name` (Parts::note*).
void note(Parts& p, const Out& o, za::SizeT from, const char* name)
{
    p.noteName = name;
    p.noteLo = glm::vec3{1e30f};
    p.noteHi = glm::vec3{-1e30f};
    for(za::SizeT i = from; i < o.pos.size(); i++)
    {
        p.noteLo = glm::min(p.noteLo, o.pos[i]);
        p.noteHi = glm::max(p.noteHi, o.pos[i]);
    }
    // Its top's height along x (its vertices' highest in each of noteBins slices of its box).
    for(int k = 0; k < noteBins; k++)
    {
        p.noteTop[k] = -1e30f;
    }
    for(za::SizeT i = from; i < o.pos.size(); i++)
    {
        const float f = (o.pos[i].x - p.noteLo.x) / za::max(p.noteHi.x - p.noteLo.x, 1e-3f);
        const int k = za::min(noteBins - 1, static_cast<int>(f * noteBins));
        p.noteTop[k] = za::max(p.noteTop[k], o.pos[i].z);
    }
}

[[nodiscard]] int texel(float v)
{
    return static_cast<int>(v + 0.5f);
}

// One triangle, turned to face `outward` (clockwise seen from there).
void face(Out& o, int a, int b, int c, const glm::vec3& outward)
{
    const glm::vec3 n = glm::cross(o.pos[c] - o.pos[a], o.pos[b] - o.pos[a]);
    const bool keep = glm::dot(n, outward) >= 0.f;
    o.tris.pushBack(a);
    o.tris.pushBack(keep ? b : c);
    o.tris.pushBack(keep ? c : b);
}

// The texel of `r` at the share `across` of its width and `along` of its length (swapped: the other way).
[[nodiscard]] glm::ivec2 texelOf(const Rect& r, float across, float along)
{
    const float u = r.swap ? along : across, v = r.swap ? across : along;
    return {texel(r.s0 + (r.s1 - r.s0) * u), texel(r.t0 + (r.t1 - r.t0) * v)};
}

// A flat end at `x` of the convex section `sec` (y z), facing `dir` (-1 or 1 along x): fanned from its middle, `r`'s
// texels laid on it (y across, z up).
void cap(Out& o, const za::Vector<glm::vec2>& sec, float x, float dir, const Rect& r)
{
    glm::vec2 mid{0.f}, lo{1e30f}, hi{-1e30f};
    for(const glm::vec2& q : sec)
    {
        mid += q;
        lo = glm::min(lo, q);
        hi = glm::max(hi, q);
    }
    mid /= static_cast<float>(sec.size());
    const glm::vec2 size = glm::max(hi - lo, glm::vec2{1e-3f});
    const int g = o.newGroup();
    const auto vert = [&](const glm::vec2& q) {
        const glm::vec2 f = (q - lo) / size;
        const glm::ivec2 st = texelOf(Rect{r.s0, r.t0, r.s1, r.t1, false}, f.x, 1.f - f.y);
        return o.vert(glm::vec3{x, q.x, q.y}, st.x, st.y, g);
    };
    const int c = vert(mid);
    za::Vector<int> ring;
    for(const glm::vec2& q : sec)
    {
        ring.pushBack(vert(q));
    }
    for(za::SizeT i = 0; i < ring.size(); i++)
    {
        face(o, c, ring[i], ring[(i + 1) % ring.size()], glm::vec3{dir, 0.f, 0.f});
    }
}

// The convex section `sec` (y z, in order round it) from x0 to x1. Each side flat with its own of `faces` (nFaces 1: the
// same for all) across it (perFace); else one rounded surface: the rect's s by the way round (mirror: across it over half
// the way, back over the other), t along x. Its ends closed flat with end0, end1 (null: left open).
void extrude(Out& o, const za::Vector<glm::vec2>& sec, float x0, float x1, const Rect* faces, int nFaces, bool perFace, bool mirror,
    const Rect* end0, const Rect* end1)
{
    const za::SizeT n = sec.size();
    glm::vec2 mid{0.f};
    za::Vector<float> along;
    float perimeter = 0.f;
    for(za::SizeT i = 0; i < n; i++)
    {
        mid += sec[i];
        along.pushBack(perimeter);
        perimeter += glm::length(sec[(i + 1) % n] - sec[i]);
    }
    mid /= static_cast<float>(n);
    const auto share = [&](za::SizeT i) {
        const float f = i >= n ? 1.f : along[i] / perimeter;
        return mirror ? 1.f - za::abs(2.f * f - 1.f) : f;
    };
    const int rounded = o.newGroup();
    for(za::SizeT i = 0; i < n; i++)
    {
        const Rect& r = faces[nFaces == 1 ? 0 : static_cast<int>(i) % nFaces];
        const glm::vec2 a = sec[i], b = sec[(i + 1) % n];
        const float ua = perFace ? 0.f : share(i), ub = perFace ? 1.f : share(i + 1);
        const int g = perFace ? o.newGroup() : rounded;
        const glm::ivec2 a0 = texelOf(r, ua, 0.f), a1 = texelOf(r, ua, 1.f), b0 = texelOf(r, ub, 0.f), b1 = texelOf(r, ub, 1.f);
        const int va0 = o.vert({x0, a.x, a.y}, a0.x, a0.y, g), va1 = o.vert({x1, a.x, a.y}, a1.x, a1.y, g);
        const int vb0 = o.vert({x0, b.x, b.y}, b0.x, b0.y, g), vb1 = o.vert({x1, b.x, b.y}, b1.x, b1.y, g);
        const glm::vec2 out = (a + b) * 0.5f - mid;
        face(o, va0, va1, vb1, glm::vec3{0.f, out.x, out.y});
        face(o, va0, vb1, vb0, glm::vec3{0.f, out.x, out.y});
    }
    if(end0)
    {
        cap(o, sec, x0, -1.f, *end0);
    }
    if(end1)
    {
        cap(o, sec, x1, 1.f, *end1);
    }
}

// A box from `lo` to `hi`: its sides `faces` (as extrude's section's edges: -z, +y, +z, -y), its ends end0 (-x), end1 (+x).
void box(Out& o, const glm::vec3& lo, const glm::vec3& hi, const Rect (&faces)[4], const Rect& end0, const Rect& end1)
{
    const za::Vector<glm::vec2> sec{{lo.y, lo.z}, {hi.y, lo.z}, {hi.y, hi.z}, {lo.y, hi.z}};
    extrude(o, sec, lo.x, hi.x, faces, 4, true, false, &end0, &end1);
}

constexpr int roundSides = 14;

// A circle round `c` (y z): from its -y side over its top and round (a strip mirrored on it has its middle on top).
[[nodiscard]] za::Vector<glm::vec2> circle(const glm::vec2& c, float r)
{
    za::Vector<glm::vec2> pts;
    for(int k = 0; k < roundSides; k++)
    {
        const float a = 3.14159265f * (1.f - 2.f * static_cast<float>(k) / static_cast<float>(roundSides));
        pts.pushBack(c + glm::vec2{za::cos(a), za::sin(a)} * r);
    }
    return pts;
}

// A barrel along x from x0 to x1 round `c` (y z), radius r: `body` up to its brake (the last brakeLen), `brake` there (each
// strip across its top half and back under it), its breech closed with `breech`; at the muzzle a flat ring in to the bore
// (radius rin), the bore `depth` deep, black.
void barrel(Parts& p, const glm::vec2& c, float r, float rin, float x0, float x1, float brakeLen, float depth, const Rect& body,
    const Rect& brake, const Rect& breech)
{
    Out& o = p.o;
    const za::Vector<glm::vec2> outer = circle(c, r), inner = circle(c, rin);
    extrude(o, outer, x0, x1 - brakeLen, &body, 1, false, true, &breech, nullptr);
    extrude(o, outer, x1 - brakeLen, x1, &brake, 1, false, true, nullptr, nullptr);
    const int ring = o.newGroup(), bore = o.newGroup(), bottom = o.newGroup();
    const za::SizeT n = outer.size();
    za::Vector<int> rim, mouth, deep;
    for(za::SizeT i = 0; i < n; i++)
    {
        rim.pushBack(o.vert({x1, outer[i].x, outer[i].y}, p.capS, p.capT, ring));
        mouth.pushBack(o.vert({x1, inner[i].x, inner[i].y}, p.capS, p.capT, ring));
    }
    za::Vector<int> wallTop, wallDeep;
    for(za::SizeT i = 0; i < n; i++)
    {
        wallTop.pushBack(o.vert({x1, inner[i].x, inner[i].y}, p.boreS, p.boreT, bore));
        wallDeep.pushBack(o.vert({x1 - depth, inner[i].x, inner[i].y}, p.boreS, p.boreT, bore));
        deep.pushBack(o.vert({x1 - depth, inner[i].x, inner[i].y}, p.boreS, p.boreT, bottom));
    }
    for(za::SizeT i = 0; i < n; i++)
    {
        const za::SizeT j = (i + 1) % n;
        face(o, rim[i], rim[j], mouth[j], glm::vec3{1.f, 0.f, 0.f});
        face(o, rim[i], mouth[j], mouth[i], glm::vec3{1.f, 0.f, 0.f});
        const glm::vec2 in = c - (inner[i] + inner[j]) * 0.5f; // (the bore's wall faces its axis)
        face(o, wallTop[i], wallTop[j], wallDeep[j], glm::vec3{0.f, in.x, in.y});
        face(o, wallTop[i], wallDeep[j], wallDeep[i], glm::vec3{0.f, in.x, in.y});
    }
    const int middle = o.vert({x1 - depth, c.x, c.y}, p.boreS, p.boreT, bottom);
    for(za::SizeT i = 0; i < n; i++)
    {
        face(o, middle, deep[i], deep[(i + 1) % n], glm::vec3{1.f, 0.f, 0.f});
    }
}

// The Widowmaker as a big two-handed gun (his note 2026-10-18: AD's view model is a shell of three half barrels, open
// below and at the muzzle; it read as a pistol): every part made, in AD's own texels (v_shot3.mdl's 256x256 skin), in the
// super shotgun's frame, its grip where the super shotgun's is (the rear handle: grafted with the trigger guard and the
// trigger, so the hand holds it as the super shotgun). Behind the barrels a receiver (x -2..12.5, 7.2 wide, 7.4 tall, its
// top's edges cut: AD's vented brake block on its sides, AD's band along its top, the breech plate's ring on its back);
// three whole barrels (radius 1.6: one on top, two below it, as AD's) to x 44, AD's barrel strips on them and its brakes on
// their last 7 units, their bores black, two of AD's bands round each; under them at x 24..31 a mount and the front handle
// (the super shotgun's grip again, 0.88 its size: the other hand's, vr_weapons.inc slot 34's hotspot). Everything above the
// super shotgun's corner (z -4.24: the settings scale about it).
void widowmakerParts(Parts& p)
{
    Out& o = p.o;
    const float cs = static_cast<float>(p.capS), ct = static_cast<float>(p.capT);
    const Rect dark{cs, ct, cs, ct, false};
    const Rect strips[3] = {{56.f, 125.f, 81.f, 255.f, false}, {25.f, 125.f, 50.f, 255.f, false}, {86.f, 125.f, 111.f, 255.f, false}};
    const Rect brakes[3] = {{56.f, 82.f, 81.f, 124.f, false}, {25.f, 82.f, 50.f, 124.f, false}, {86.f, 82.f, 111.f, 124.f, false}};
    const Rect band{2.f, 1.f, 130.f, 11.f, false};
    const Rect bandAlong{2.f, 1.f, 130.f, 11.f, true};
    const Rect vents{128.f, 132.f, 168.f, 176.f, false};
    const Rect breechPlate{118.f, 35.f, 158.f, 75.f, false};
    const Rect underside{28.f, 140.f, 46.f, 250.f, false};

    const za::Vector<glm::vec2> receiver{{-3.6f, 3.f}, {3.6f, 3.f}, {3.6f, 9.2f}, {2.4f, 10.4f}, {-2.4f, 10.4f}, {-3.6f, 9.2f}};
    const Rect receiverFaces[6] = {underside, vents, bandAlong, bandAlong, bandAlong, vents};
    extrude(o, receiver, -2.f, 12.5f, receiverFaces, 6, true, false, &breechPlate, &dark);

    // The shell port (his VR notes 2026-10-18: an auto shotgun loaded on the inner side, +y, by the rear handle;
    // vr_view_internal.hpp loadPorts: its middle 6.5 3.6 5.6): a dark opening on the receiver's side, framed by a raised
    // rim 0.5 proud (AD's band), its inner faces dark. Its spent shells leave by the slot on the other side (vr_shells.cpp).
    // His third notes (2026-10-19): half the size (it was x 2.6..10.4, z 4.5..6.7, its rim 0.4 wide, 0.5 proud), about
    // the same middle; vr_reload_port_widow_radius halved with it (1.25).
    const Rect rimFaces[4] = {bandAlong, bandAlong, bandAlong, bandAlong};
    const Rect rimTop[4] = {dark, bandAlong, bandAlong, bandAlong};   // (its underside faces the opening)
    const Rect rimBottom[4] = {bandAlong, bandAlong, dark, bandAlong}; // (its top faces it)
    constexpr float px0 = 4.55f, px1 = 8.45f, pz0 = 5.05f, pz1 = 6.15f, rim = 0.2f, side = 3.6f, proud = 0.25f;
    box(o, {px0, side - 0.3f, pz0}, {px1, side + 0.05f, pz1}, {dark, dark, dark, dark}, dark, dark);
    box(o, {px0 - rim, side - 0.1f, pz1}, {px1 + rim, side + proud, pz1 + rim}, rimTop, bandAlong, bandAlong);
    box(o, {px0 - rim, side - 0.1f, pz0 - rim}, {px1 + rim, side + proud, pz0}, rimBottom, bandAlong, bandAlong);
    box(o, {px0 - rim, side - 0.1f, pz0}, {px0, side + proud, pz1}, rimFaces, bandAlong, dark);
    box(o, {px1, side - 0.1f, pz0}, {px1 + rim, side + proud, pz1}, rimFaces, dark, bandAlong);
    box(o, {5.f, -side - 0.05f, 7.f}, {9.5f, -side + 0.3f, 8.4f}, {dark, dark, dark, dark}, dark, dark); // (the ejection slot)

    // The barrels (his third notes: a Gatling auto shotgun): an even triangle round the spin axis (y 0, z spinZ; they were
    // at 0 8.6 and +-1.85 5.9), turned about it in the spin frames (spinFrames): the barrel on top fires.
    o.spinning = true;
    const glm::vec2 centres[3] = {{0.f, spinZ + spinR}, {-0.8660254f * spinR, spinZ - 0.5f * spinR}, {0.8660254f * spinR, spinZ - 0.5f * spinR}};
    for(int i = 0; i < 3; i++)
    {
        barrel(p, centres[i], 1.6f, 1.f, 11.5f, 44.f, 7.f, 3.f, strips[i], brakes[i], dark);
        for(const float x : {18.f, 29.5f})
        {
            extrude(o, circle(centres[i], 1.9f), x, x + 1.6f, &band, 1, false, false, &dark, &dark);
        }
    }
    o.spinning = false;
    // The muzzle's mark: a speck (two faces back to back, 0.12 across) at the top barrel's bore's middle in the muzzle's
    // plane, still in the spin frames. The muzzle anchor (44 0 8.8) binds to the nearest triangle (vr_anchor.hpp): the
    // bore's middle is open, so it was the mouth's edge, a unit to the side.
    {
        const int g = o.newGroup();
        const glm::vec3 mark{44.f, 0.f, spinZ + spinR};
        const int a = o.vert(mark + glm::vec3{0.f, -0.06f, -0.04f}, p.boreS, p.boreT, g);
        const int b = o.vert(mark + glm::vec3{0.f, 0.06f, -0.04f}, p.boreS, p.boreT, g);
        const int c = o.vert(mark + glm::vec3{0.f, 0.f, 0.07f}, p.boreS, p.boreT, g);
        face(o, a, b, c, glm::vec3{1.f, 0.f, 0.f});
        face(o, a, b, c, glm::vec3{-1.f, 0.f, 0.f});
    }

    // (Its bottom at 2.3, was 3: the front handle's top slants down to 2.56 at its back, a gap under the mount his third
    // notes saw; vr_adweapon_models prints the handle's top along x.)
    const za::Vector<glm::vec2> mount{{-1.5f, 2.3f}, {1.5f, 2.3f}, {1.5f, 5.8f}, {-1.5f, 5.8f}};
    const Rect mountFaces[4] = {underside, bandAlong, bandAlong, bandAlong};
    extrude(o, mount, 24.f, 31.f, mountFaces, 4, true, false, &dark, &dark);
    const int gripSeed[3] = {151, -1, -1};
    const za::Vector<bool> grip = piecesOf(p.graft, gripSeed);
    const glm::vec3 top{3.3f, 0.f, 4.55f}, to{27.5f, 0.f, 3.2f};
    const za::SizeT handleFrom = o.pos.size();
    take(o, p.graft, grip, true, [&](const glm::vec3& q) { return to + (q - top) * 0.88f; }, p.graftDs, p.graftDt);
    note(p, o, handleFrom, "front handle");

    p.muzzle = glm::vec3{44.f, 0.f, spinZ + spinR}; // (the top barrel's: vr_weapons.inc slot 34's MuzzleAnchor)
    p.muzzleSet = true;
}

// The Plasma Gun's ammo screen housing (his mockup, kit scratch/adplasma_housing_mockup_1111.webp; ROUND24.md, "The
// Widowmaker's and the AD Plasma Gun's ammo screen housings"): a flat bronze bezel on the gun's rear face (the cut's cap)
// round the screen, its window chamfered, a round rivet each side. Made on the model's grid (its bytes: X 0.148 a step,
// Y 0.055, Z 0.073, packed from the recipe's corner; the parts made so far give its far corner, the housing is inside
// it): the cut's face on the third X byte, the bezel's front on the second (one step proud), the rivets' tops on the
// first (the corner's: nothing may go past it). h = the window's width:
// - the window 2 winY Y steps (1.20: the glass the screen had without a housing, so its digits keep the font's
//   size: the "plasma.mdl#vr" entry's fillMax 1) by winZ Z steps (0.91h; its middle half way between two), its
//   corners cut 2 Y steps along the top and bottom, 1 Z step down the sides (about 0.08h); its walls straight in to
//   the cut's face, dark (AD's dark slots);
// - the bezel 2 Z steps (0.12h) above and below, 6 Y steps (0.27h) at the sides (the mockup's: room for the rivets), one
//   X step proud (0.12h: the grid's least; the spec's 0.06h is half a step), flat, closed behind; AD's bronze plate;
// - the rivets in the side bands' middles, level with the window's middle: an octagon 0.22 across on the bezel (r about
//   0.09h) up to a small flat top on the corner's byte, smoothed (a low dome), gunmetal on bronze (one of AD's painted
//   rivets on its riveted panel);
// - the glass glassIn h behind the bezel's front (the entry's offset).
// Dwell support: the same housing on Dwell's three weapons' rear faces (DWELL.md 9.6, dwellHousings): its numbers a
// Housing, the face's byte measured on the made model (faceX) instead of the corner's third.
struct Housing
{
    glm::vec2 glassAt;    // y z: the window's middle (on the grid: Y to the nearest step, Z to the nearest half)
    int faceByte;         // the gun's face (the bezel's back) on this X byte from the corner (-1: faceX's nearest)
    float faceX;          // (faceByte -1) the face's x as made: the bezel's front one byte behind it, the rivets' tops two
    int winY, winZ;       // the window's half width, its height (grid steps; winZ odd: its middle between two rows)
    int sideY, bandZ;     // the bezel's bands (grid steps)
    int cutY, cutZ;       // the window's corners cut (grid steps)
    float rivetY;         // the rivets' middles from the window's (Y steps; a half: between two, with half outer/inner)
    float rivetOut, rivetIn; // the rivets' octagon: its corners' Y offsets (steps) at the middle rows and the top/bottom
    float glassIn;        // the glass behind the bezel's front, in h
    glm::vec2 plate;      // the bezel's middle's texel
    float plateK;         // its texels a unit
    glm::vec2 rivetTex;   // the rivets' texel (their middle)
    float darkS0, darkS1, darkT0, darkT1; // a dark strip: the window's walls (and the back, the rivets' bases at S1 T0);
                                          // darkS0 below 0: the caps' dark grey (Parts::capS, capT)
    // The rivets' octagon's Z offsets (steps) at the middle rows and the top/bottom, its middle this far above the
    // window's (the Plasma Gun's: half steps either side of the window's middle, between two rows). No two of its corners
    // nearer than capWeld (the holes' search welds them: a Z step can be under 0.05).
    float rivetZIn{0.5f}, rivetZOut{1.5f}, rivetZAt{0.f};
};
// The Plasma Gun's: on the corner's third byte; AD's bronze plate (33..64 x 1..12, 7 texels a unit) on the bezel, a
// painted rivet on AD's riveted panel (its dark dot), a dark slot (7..8 x 51..61) in the window. The glass's middle as
// weapons.txt slot 35 places it.
constexpr Housing plasmaHousingSpec{{-0.0617f, 9.2136f}, 2, 0.f, 11, 15, 6, 2, 2, 1, 14.f, 2.f, 1.f, 0.04f, {48.5f, 6.5f},
    7.f, {34.f, 55.f}, 7.f, 8.f, 51.f, 61.f};

void housing(Parts& p, const Housing& spec)
{
    Out& o = p.o;
    Housing h = spec;
    if(h.darkS0 < 0.f)
    {
        h.darkS0 = h.darkS1 = static_cast<float>(p.capS);
        h.darkT0 = h.darkT1 = static_cast<float>(p.capT);
    }
    const za::SizeT trisFrom = o.tris.size(); // (the gun so far: the housing's own after)
    glm::vec3 hi{-1e30f};
    for(const glm::vec3& q : o.pos)
    {
        hi = glm::max(hi, q);
    }
    const glm::vec3 lo = p.corner;
    const glm::vec3 step = (hi - lo) / 255.f;
    // The bezel's back on the face's byte (fb), its front on the one behind it (fb - 1), the rivets' tops on fb - 2.
    const float fb = h.faceByte >= 0 ? static_cast<float>(h.faceByte)
                                     : static_cast<float>(za::lround((h.faceX - lo.x) / step.x));
    const float xFront = fb - 1.f, xTop = fb - 2.f;
    const float jc = static_cast<float>(za::lround((h.glassAt.x - lo.y) / step.y));
    const float kb = static_cast<float>(za::lround((h.glassAt.y - lo.z) / step.z - 0.5f * static_cast<float>(h.winZ)));
    const float kt = kb + static_cast<float>(h.winZ), km = kb + 0.5f * static_cast<float>(h.winZ);
    const auto at = [&](float xb, float j, float k) { return lo + glm::vec3{step.x * xb, step.y * j, step.z * k}; };
    const glm::vec3 mid = at(xFront, jc, km);
    const float W = static_cast<float>(h.winY), S = static_cast<float>(h.sideY), B = static_cast<float>(h.bandZ);
    const float cY = static_cast<float>(h.cutY), cZ = static_cast<float>(h.cutZ);
    // The plate's texels laid flat on the bezel (right = -y, up = z), `out` units further out from its edge (a wall
    // folded out).
    const auto bronze = [&](const glm::vec3& q, const glm::vec2& outward, float out) {
        const glm::vec2 f{mid.y - q.y + outward.x * out, mid.z - q.z - outward.y * out}; // (right, down)
        return glm::ivec2{texel(h.plate.x + f.x * h.plateK), texel(h.plate.y + f.y * h.plateK)};
    };
    // The outline (O), the window's top/bottom edges' ends (E) and its sides' (F), each (side, up/down).
    const auto O = [&](float sy, float sz) { return glm::vec2{jc + sy * (W + S), sz > 0.f ? kt + B : kb - B}; };
    const auto E = [&](float sy, float sz) { return glm::vec2{jc + sy * (W - cY), sz > 0.f ? kt : kb}; };
    const auto F = [&](float sy, float sz) { return glm::vec2{jc + sy * W, sz > 0.f ? kt - cZ : kb + cZ}; };
    // The bezel's front (byte fb - 1) and back (byte fb, against the gun's face): bands and corner triangles.
    for(int side = 0; side < 2; side++)
    {
        const float xb = side == 0 ? xFront : fb;
        const glm::vec3 facing{side == 0 ? -1.f : 1.f, 0.f, 0.f};
        const int g = o.newGroup();
        const auto v = [&](const glm::vec2& jk) {
            const glm::vec3 q = at(xb, jk.x, jk.y);
            const glm::ivec2 st = side == 0 ? bronze(q, glm::vec2{0.f}, 0.f)
                                            : glm::ivec2{texel(h.darkS1), texel(h.darkT0)};
            return o.vert(q, st.x, st.y, g);
        };
        for(const float sz : {1.f, -1.f})
        {
            const int a = v(O(1.f, sz)), b = v(O(-1.f, sz)), c = v(E(-1.f, sz)), d = v(E(1.f, sz));
            face(o, a, b, c, facing);
            face(o, a, c, d, facing);
        }
        for(const float sy : {1.f, -1.f})
        {
            const int a = v(O(sy, 1.f)), b = v(O(sy, -1.f)), c = v(F(sy, -1.f)), d = v(F(sy, 1.f));
            face(o, a, b, c, facing);
            face(o, a, c, d, facing);
            for(const float sz : {1.f, -1.f})
            {
                face(o, v(O(sy, sz)), v(E(sy, sz)), v(F(sy, sz)), facing);
            }
        }
    }
    // Its outer walls (the plate folded out from the front's edge) and the window's walls (dark), byte fb - 1 to fb.
    const glm::vec2 outline[4] = {O(1.f, 1.f), O(-1.f, 1.f), O(-1.f, -1.f), O(1.f, -1.f)};
    for(int n = 0; n < 4; n++)
    {
        const glm::vec2 a = outline[n], b = outline[(n + 1) % 4];
        const glm::vec2 m = (a + b) * 0.5f - glm::vec2{jc, km};
        const glm::vec2 outward = za::abs(m.x * step.y) > za::abs(m.y * step.z) ? glm::vec2{m.x > 0.f ? -1.f : 1.f, 0.f}
                                                                                : glm::vec2{0.f, m.y > 0.f ? 1.f : -1.f};
        const int g = o.newGroup();
        const auto v = [&](const glm::vec2& jk, float xb) {
            const glm::vec3 q = at(xb, jk.x, jk.y);
            const glm::ivec2 st = bronze(at(xFront, jk.x, jk.y), outward, step.x * (xb - xFront));
            return o.vert(q, st.x, st.y, g);
        };
        const int a0 = v(a, xFront), b0 = v(b, xFront), b1 = v(b, fb), a1 = v(a, fb);
        const glm::vec3 wallOut{0.f, (m.x > 0.f ? 1.f : -1.f) * (outward.x != 0.f ? 1.f : 0.f), outward.y};
        face(o, a0, b0, b1, wallOut);
        face(o, a0, b1, a1, wallOut);
    }
    const glm::vec2 window[8] = {E(1.f, 1.f), E(-1.f, 1.f), F(-1.f, 1.f), F(-1.f, -1.f), E(-1.f, -1.f), E(1.f, -1.f),
        F(1.f, -1.f), F(1.f, 1.f)};
    for(int n = 0; n < 8; n++)
    {
        const glm::vec2 a = window[n], b = window[(n + 1) % 8];
        const glm::vec2 m = (a + b) * 0.5f - glm::vec2{jc, km};
        const glm::vec3 inward = -glm::normalize(glm::vec3{0.f, m.x * step.y, m.y * step.z});
        const int g = o.newGroup();
        const int a0 = o.vert(at(xFront, a.x, a.y), texel(h.darkS0), texel(h.darkT0), g);
        const int b0 = o.vert(at(xFront, b.x, b.y), texel(h.darkS0), texel(h.darkT1), g);
        const int b1 = o.vert(at(fb, b.x, b.y), texel(h.darkS1), texel(h.darkT1), g);
        const int a1 = o.vert(at(fb, a.x, a.y), texel(h.darkS1), texel(h.darkT0), g);
        face(o, a0, b0, b1, inward);
        face(o, a0, b1, a1, inward);
    }
    // The rivets: an octagon on the bezel's front (half steps up and down: the window's middle is half way between two
    // rows) to a flat top on byte fb - 2, its sides and top one smoothing group (a low dome); closed under it.
    const float ro = h.rivetOut, ri = h.rivetIn, zi = h.rivetZIn, zo = h.rivetZOut, rk = km + h.rivetZAt;
    const float ring[8][2] = {{ro, zi}, {ri, zo}, {-ri, zo}, {-ro, zi}, {-ro, -zi}, {-ri, -zo}, {ri, -zo}, {ro, -zi}};
    const float top[4][2] = {{ri, zi}, {-ri, zi}, {-ri, -zi}, {ri, -zi}};
    constexpr int topOf[8] = {0, 0, 1, 1, 2, 2, 3, 3};
    for(const float sy : {1.f, -1.f})
    {
        const float rj = jc + sy * h.rivetY;
        const int g = o.newGroup();
        const auto v = [&](const float (&d)[2], float xb, float spread) {
            return o.vert(at(xb, rj + d[0], rk + d[1]), texel(h.rivetTex.x - d[0] * spread), texel(h.rivetTex.y - d[1] * spread),
                g);
        };
        int rv[8], tv[4];
        for(int n = 0; n < 8; n++)
        {
            rv[n] = v(ring[n], xFront, 1.f);
        }
        for(int n = 0; n < 4; n++)
        {
            tv[n] = v(top[n], xTop, 0.5f);
        }
        const glm::vec3 c = at(xTop + 0.5f, rj, rk);
        for(int n = 0; n < 8; n++)
        {
            const int m = (n + 1) % 8;
            const glm::vec3 outward = (o.pos[rv[n]] + o.pos[rv[m]]) * 0.5f - c + glm::vec3{-0.05f, 0.f, 0.f};
            if(topOf[n] == topOf[m])
            {
                face(o, rv[n], rv[m], tv[topOf[n]], outward);
            }
            else
            {
                face(o, rv[n], rv[m], tv[topOf[m]], outward);
                face(o, rv[n], tv[topOf[m]], tv[topOf[n]], outward);
            }
        }
        face(o, tv[0], tv[1], tv[2], glm::vec3{-1.f, 0.f, 0.f});
        face(o, tv[0], tv[2], tv[3], glm::vec3{-1.f, 0.f, 0.f});
        const int gb = o.newGroup();
        int base[8];
        for(int n = 0; n < 8; n++)
        {
            base[n] = o.vert(o.pos[rv[n]], texel(h.darkS1), texel(h.darkT0), gb);
        }
        for(int n = 1; n + 1 < 8; n++)
        {
            face(o, base[0], base[n], base[n + 1], glm::vec3{1.f, 0.f, 0.f});
        }
    }
    // Its outline's corners on the gun's face: inside the gun's section a little in front of the cut (its cap is made
    // later, closing the holes: the gun's surfaces crossed an odd number of times along +z from there).
    p.housingOnFace = 0;
    for(const glm::vec2& c : outline)
    {
        const glm::vec3 q = at(fb, c.x, c.y) + glm::vec3{0.3f * step.x, 0.f, 0.f};
        int crossed = 0;
        for(za::SizeT t = 0; t + 2 < trisFrom; t += 3)
        {
            const glm::vec3 &a = o.pos[o.tris[t]], &b = o.pos[o.tris[t + 1]], &e = o.pos[o.tris[t + 2]];
            const float den = (b.y - e.y) * (a.x - e.x) + (e.x - b.x) * (a.y - e.y);
            if(za::abs(den) < 1e-9f)
            {
                continue;
            }
            const float u = ((b.y - e.y) * (q.x - e.x) + (e.x - b.x) * (q.y - e.y)) / den;
            const float v = ((e.y - a.y) * (q.x - e.x) + (a.x - e.x) * (q.y - e.y)) / den;
            const float z = u * a.z + v * b.z + (1.f - u - v) * e.z;
            crossed += u >= 0.f && v >= 0.f && u + v <= 1.f && z > q.z ? 1 : 0;
        }
        p.housingOnFace += crossed % 2;
    }
    p.housing = true;
    p.housingGlass = at(xFront, jc, km) + glm::vec3{h.glassIn * 2.f * W * step.y, 0.f, 0.f};
    p.housingWindow = {2.f * W * step.y, static_cast<float>(h.winZ) * step.z};
}

// The Plasma Gun's cell receiver (his note 2026-10-18: the cell on the inner side, not below as the lightning gun's): a
// block on its +y side round the cell's way out (plasmaSeat: the cell 3.7 along x, 3.2 tall, turned out of it), AD's
// texels (v_plasma.mdl's 128x128 skin): the riveted panel outside, the rusty plate round it.
// His second notes (2026-10-18): a real hole where the cell goes, seen with it out (it was flat): the block is a frame
// round a pocket a little bigger than the cell (3.9 along x, 3.5 tall), its inner faces and its floor dark.
void plasmaParts(Parts& p)
{
    // His third notes (2026-10-19): the hole's inside darker than the gun outside, so that it reads as going in: black
    // (the bores' texels), not the caps' dark grey; its floor over the gun's own side (y 3.25 there: it was 2.95, under
    // AD's riveted panel, which showed through the hole as the outside does).
    Out& o = p.o;
    const float bs = static_cast<float>(p.boreS), bt = static_cast<float>(p.boreT);
    const Rect dark{bs, bt, bs, bt, false};
    const Rect panel{32.f, 47.f, 64.f, 64.f, false};
    const Rect plate{33.f, 1.f, 64.f, 12.f, true};
    const float x = plasmaSeat.x, z = plasmaSeat.z, y0 = 2.9f, y1 = 4.6f;
    const float bx0 = x - 2.5f, bx1 = x + 2.6f, bz0 = z - 2.1f, bz1 = z + 2.1f; // (the block)
    const float hx0 = x - 1.95f, hx1 = x + 1.95f, hz0 = z - 1.75f, hz1 = z + 1.75f; // (its hole)
    const Rect sides[4] = {plate, panel, plate, plate};
    const Rect below[4] = {plate, panel, dark, plate}; // (its top faces the hole)
    const Rect above[4] = {dark, panel, plate, plate}; // (its underside faces it)
    box(o, {bx0, y0, bz0}, {hx0, y1, bz1}, sides, plate, dark);
    box(o, {hx1, y0, bz0}, {bx1, y1, bz1}, sides, dark, plate);
    box(o, {hx0, y0, bz0}, {hx1, y1, hz0}, below, plate, plate);
    box(o, {hx0, y0, hz1}, {hx1, y1, bz1}, above, plate, plate);
    // Its floor (his third notes: the gun's own side showed through the hole, its riveted panel as outside): just over
    // the gun's surface met looking in at the hole (the highest of a grid of rays in along -y), black.
    float floorY = y0;
    for(int i = 0; i <= 6; i++)
    {
        for(int j = 0; j <= 6; j++)
        {
            const float qx = hx0 + (hx1 - hx0) * static_cast<float>(i) / 6.f, qz = hz0 + (hz1 - hz0) * static_cast<float>(j) / 6.f;
            for(za::SizeT t = 0; t + 2 < o.tris.size(); t += 3)
            {
                const glm::vec3 &a = o.pos[o.tris[t]], &b = o.pos[o.tris[t + 1]], &e = o.pos[o.tris[t + 2]];
                const float den = (b.z - e.z) * (a.x - e.x) + (e.x - b.x) * (a.z - e.z);
                if(za::abs(den) < 1e-9f)
                {
                    continue;
                }
                const float u = ((b.z - e.z) * (qx - e.x) + (e.x - b.x) * (qz - e.z)) / den;
                const float v = ((e.z - a.z) * (qx - e.x) + (a.x - e.x) * (qz - e.z)) / den;
                const float y = u * a.y + v * b.y + (1.f - u - v) * e.y;
                if(u >= 0.f && v >= 0.f && u + v <= 1.f && y < y1 - 0.5f)
                {
                    floorY = za::max(floorY, y);
                }
            }
        }
    }
    const za::SizeT floorFrom = o.pos.size();
    box(o, {hx0, floorY - 0.4f, hz0}, {hx1, floorY + 0.05f, hz1}, {dark, dark, dark, dark}, dark, dark);
    note(p, o, floorFrom, "cell pocket's floor");
    housing(p, plasmaHousingSpec);
}

// Dwell support (DWELL.md 9.6, "dwellgun"): the Plasma Gun's housing on Dwell's two weapons' rear faces (measured on
// the made models, Dwell v2.2), upright as the Plasma Gun's, read from behind along the gun. Each window the glass the
// screen has without a housing (the Plasma Gun's 22.5 x 20.4 font pixels at the slot's wpntxt_scale: the digits at the
// font's size, fillMax 1), to the grid's steps; the bezel's bands, cut corners and rivets about the Plasma Gun's in h
// where the grid allows; the window's walls and the back the caps' dark grey. vr_adweapon_models prints each glass's
// place (vr_screenstyle.cpp's entries, weapons.txt slots 52-53's wpntxt_av). Their faces are holes closed after the
// housing is made, so its "corners on the gun's face" count says 0 (the shots show them on it).
// - The Rotary Shotgun: on its housing's flat octagonal back (x 0.51, y -4.44..4.48, z 2.62..11.37), in its middle (the
//   barrels' axis); window 2x13 Y by 17 Z steps (1.08 x 1.04 at wpntxt_scale 0.25), bands 6 Y / 2 Z, rivets 16 Y out;
//   Dwell's brown housing plate (its skin's 192 150) on the bezel, a bolt head (8 188) on the rivets.
// - The Crystal Lance: on its wooden handle's back (x -3.93 at its foot to -3.73 at its top, leaning: the bezel's back
//   on its foot's byte, the face leaning away above it; y -1.32..1.29, z 4.97..7.30), in its middle; window 2x10 Y by
//   25 Z steps (1.45 x 1.31 at 0.25), bands 5 Y / 2 Z, the rivets between two Y steps (12.5 out, 3 steps across: the Y
//   step is 0.072); the frame's dark metal (60 140) on the bezel, a crystal's violet (22 45) on the rivets.
constexpr Housing rotaryHousingSpec{{0.f, 7.f}, -1, 0.51f, 13, 17, 6, 2, 2, 1, 16.f, 2.f, 1.f, 0.04f, {192.f, 150.f}, 8.f,
    {8.f, 188.f}, -1.f, 0.f, 0.f, 0.f};
constexpr Housing lanceHousingSpec{{0.f, 6.14f}, -1, -3.93f, 10, 25, 5, 2, 2, 1, 12.5f, 1.5f, 0.5f, 0.04f, {60.f, 140.f},
    6.f, {22.f, 45.f}, -1.f, 0.f, 0.f, 0.f};

// The Rotary Shotgun's four barrels (dwellrotary; his notes 2026-11-22: "make the geometry in between the barrels a little
// bit deeper, so that it looks like there's actually separate barrels", and "the muzzle is completely solid. It should
// also have holes at the front that match the shapes of the barrels"). Dwell's are one cross-shaped shell (four flat
// sided lobes 3.2 wide, their inner corners 2.26 from the axis) with a solid front: taken off (Recipe::dropMore), made
// again in the source's frame (laid as the recipe lays it): four round barrels (radius 1.6, their axes 2.35 above,
// below and either side of the turning axis: the lobes' tips where they were, 3.95 out) from Dwell's lobes' start
// (x 16.66) to their front (37.23), separate down to 1.66 from the axis between them, round a core rod (radius 1.3) that
// stops 0.8 short of the front; each barrel's front a ring in to its bore (radius 0.95, 2 deep, black). Dwell's barrel
// strips on them (a lobe's three faces across, its front section on the last 4.63), all turning as Dwell's did.
void frameBox(Out& o, const glm::vec3& origin, const glm::vec3& u, const glm::vec3& v, const glm::vec3& n, const glm::vec3& lo,
    const glm::vec3& hi, const Rect& r);

void dwBarrels(Parts& p)
{
    Out& o = p.o;
    const auto lay = [&](const glm::vec3& q) { return p.layTo + (q - p.layFrom) * p.layK; };
    const float k = p.layK;
    const glm::vec3 axis = lay({0.f, -0.11f, -15.28f});
    const float x0 = lay({16.66f, 0.f, 0.f}).x, x1 = lay({37.23f, 0.f, 0.f}).x;
    const Rect body{12.f, 162.f, 54.f, 37.f, false};
    const Rect front{12.f, 34.f, 54.f, 1.f, false};
    const Rect breech{12.f, 37.f, 54.f, 37.f, false};
    const Rect core{54.f, 162.f, 65.f, 37.f, false};
    const glm::vec2 c{axis.y, axis.z};
    // (Their fronts' rings on Dwell's barrel metal, not the caps' dark grey. Both drew nearly white in a bright room: the
    // weapon reflections' mirror, not the texels; made models reflect less since capcolour: vr_envmap.cpp madeMetal.)
    const int capS = p.capS, capT = p.capT;
    p.capS = 40;
    p.capT = 20;
    o.spinning = true;
    for(const glm::vec2 d : {glm::vec2{0.f, 1.f}, glm::vec2{-1.f, 0.f}, glm::vec2{0.f, -1.f}, glm::vec2{1.f, 0.f}})
    {
        barrel(p, c + d * (2.35f * k), 1.6f * k, 0.95f * k, x0, x1, 4.63f * k, 2.f * k, body, front, breech);
    }
    extrude(o, circle(c, 1.3f * k), x0, x1 - 0.8f * k, &core, 1, false, true, &breech, &breech);
    // Dwell's clamps (open three-faced pieces in the cross's inner corners, seen through once the barrels are apart:
    // taken off too) made again closed: a block in each gap between two barrels, 1.4..3.3 out from the axis and 1 wide,
    // bridging them, at Dwell's two places along them (x 22.06..23.86, 29.26..31.31), on Dwell's clamp texels.
    const Rect clampTex{1.f, 182.f, 15.f, 204.f, false};
    for(const glm::vec2 xs : {glm::vec2{22.06f, 23.86f}, glm::vec2{29.26f, 31.31f}})
    {
        const float cx0 = lay({xs.x, 0.f, 0.f}).x, cx1 = lay({xs.y, 0.f, 0.f}).x;
        for(const glm::vec2 d : {glm::vec2{1.f, 1.f}, glm::vec2{-1.f, 1.f}, glm::vec2{-1.f, -1.f}, glm::vec2{1.f, -1.f}})
        {
            const glm::vec2 out = d * 0.70711f, side{-out.y, out.x};
            frameBox(o, glm::vec3{0.f, c.x, c.y}, glm::vec3{1.f, 0.f, 0.f}, glm::vec3{0.f, out.x, out.y},
                glm::vec3{0.f, side.x, side.y}, {cx0, 1.4f * k, -0.5f * k}, {cx1, 3.3f * k, 0.5f * k}, clampTex);
        }
    }
    o.spinning = false;
    p.capS = capS;
    p.capT = capT;
    p.muzzle = glm::vec3{x1, axis.y, axis.z}; // (the barrels' front's middle, on the turning axis)
    p.muzzleSet = true;
}

void rotaryParts(Parts& p)
{
    housing(p, rotaryHousingSpec);
    dwBarrels(p);
}

// A box in the frame (origin; u, v, n), from lo to hi along them: every face `r`'s texels, its corners on the rect's.
void frameBox(Out& o, const glm::vec3& origin, const glm::vec3& u, const glm::vec3& v, const glm::vec3& n, const glm::vec3& lo,
    const glm::vec3& hi, const Rect& r)
{
    const glm::vec3 ax[3] = {u, v, n};
    const auto at = [&](const glm::vec3& f) { return origin + u * f.x + v * f.y + n * f.z; };
    for(int k = 0; k < 3; k++)
    {
        for(int side = 0; side < 2; side++)
        {
            const int a = (k + 1) % 3, b = (k + 2) % 3;
            glm::vec3 f = lo;
            f[k] = side ? hi[k] : lo[k];
            const int g = o.newGroup();
            int vv[4];
            for(int c = 0; c < 4; c++)
            {
                glm::vec3 q = f;
                q[a] = (c == 1 || c == 2) ? hi[a] : lo[a];
                q[b] = c >= 2 ? hi[b] : lo[b];
                const glm::ivec2 st = texelOf(r, (c == 1 || c == 2) ? 1.f : 0.f, c >= 2 ? 1.f : 0.f);
                vv[c] = o.vert(at(q), st.x, st.y, g);
            }
            const glm::vec3 outward = ax[k] * (side ? 1.f : -1.f);
            face(o, vv[0], vv[1], vv[2], outward);
            face(o, vv[0], vv[2], vv[3], outward);
        }
    }
}

// The Rotary Shotgun as a gauntlet: its housing's outer size (Dwell's octagon, as wide as tall about the barrels' axis:
// y 0, z 0) at x: 5.46 out to x -1.83 (its back plate), narrowing to 4.53 at 7.25 (its front collar's); and its housing
// texels' s there (Dwell's housing column: 254 at the back plate, 198 at the collar).
[[nodiscard]] float rotaryHousingSize(float x)
{
    return x <= -1.83f ? 5.46f : 5.46f - (za::min(x, 7.25f) + 1.83f) * (0.93f / 9.08f);
}
[[nodiscard]] float rotaryHousingS(float x)
{
    return 254.f - (za::clamp(x, -2.6f, 7.85f) + 2.6f) * (68.f / 10.45f);
}

// Its ammo screen (dwellrotary item 6; his notes 2026-11-22: "on its top with a bit of a wedge like we did for the super
// nailgun"): the super nailgun's wedge (make_screen_housing_nail2.py; vr_screenstyle.cpp "nail2") on the housing's top
// at its back end, in window heights h (the window 46.04 x 28.56 font pixels as the super nailgun's, at weapons.txt
// slot 56's wpntxt_scale 0.3: h 2.013, a font pixel 0.0705): its rear face rises at 19 degrees (the super nailgun's:
// weapons.txt's wpntxt_pitch 70.5 tilts the glass with it, facing up to the eye) from the housing's back top edge (x
// -2.6, z 5.46), 0.08h below the bezel and 0.1h above it to a peak; its front slope down onto the housing's top at x 3;
// its sides upright 2.05 out (inside the top's edges, 2.26 out at the back); its bottom 0.3 under the top (in it). In the
// sloped face the bezel 1.8h x 1.2h standing 0.1h proud (a 0.1h frame of Dwell's brown housing plate round the window),
// the window's floor near black, a bolt head at each corner; the glass 0.05h behind the bezel's front, its middle at
// (-1.371 0 6.096: weapons.txt slot 56's wpntxt_av with vr_screenstyle.cpp's "nail2" offset taken out).
constexpr float rotaryH = 2.013f;
constexpr float rotarySlope = 19.f;

void rotaryScreenWedge(Parts& p)
{
    Out& o = p.o;
    const float sn = za::sin(glm::radians(rotarySlope)), cn = za::cos(glm::radians(rotarySlope));
    const glm::vec3 up{cn, 0.f, sn}, out{-sn, 0.f, cn}, across{0.f, 1.f, 0.f};
    const glm::vec3 back{-2.6f, 0.f, 5.46f};
    const float h = rotaryH, low = 0.08f * h, bezelV = 1.2f * h, peakV = low + bezelV + 0.1f * h;
    const glm::vec3 peak = back + up * peakV;
    constexpr float frontX = 3.f, halfY = 2.05f, under = 0.3f;
    const auto topZ = [](float x) { return rotaryHousingSize(x); };
    // The wedge's side profile (x z), round it: its back's foot, the back top edge, the peak, the front's top and foot.
    const glm::vec2 prof[5] = {{back.x, topZ(back.x) - under}, {back.x, back.z}, {peak.x, peak.z}, {frontX, topZ(frontX)},
        {frontX, topZ(frontX) - under}};
    glm::vec2 pc{0.f};
    float zLo = 1e30f, zHi = -1e30f;
    for(const glm::vec2& q : prof)
    {
        pc += q * 0.2f;
        zLo = za::min(zLo, q.y);
        zHi = za::max(zHi, q.y);
    }
    // Its faces in Dwell's housing texels: the slopes and bottom on the top's band (t 197..219 across), the sides on
    // the outer side's (t 138..161 up).
    for(int i = 0; i < 5; i++)
    {
        const glm::vec2 a = prof[i], b = prof[(i + 1) % 5];
        const int g = o.newGroup();
        const int v0 = o.vert({a.x, -halfY, a.y}, texel(rotaryHousingS(a.x)), 197, g);
        const int v1 = o.vert({b.x, -halfY, b.y}, texel(rotaryHousingS(b.x)), 197, g);
        const int v2 = o.vert({b.x, halfY, b.y}, texel(rotaryHousingS(b.x)), 219, g);
        const int v3 = o.vert({a.x, halfY, a.y}, texel(rotaryHousingS(a.x)), 219, g);
        const glm::vec2 m = (a + b) * 0.5f - pc;
        face(o, v0, v1, v2, glm::vec3{m.x, 0.f, m.y});
        face(o, v0, v2, v3, glm::vec3{m.x, 0.f, m.y});
    }
    for(const float sy : {-1.f, 1.f})
    {
        const int g = o.newGroup();
        const auto t = [&](float z) { return texel(161.f - (z - zLo) / (zHi - zLo) * 23.f); };
        const int c = o.vert({pc.x, sy * halfY, pc.y}, texel(rotaryHousingS(pc.x)), t(pc.y), g);
        int ring[5];
        for(int i = 0; i < 5; i++)
        {
            ring[i] = o.vert({prof[i].x, sy * halfY, prof[i].y}, texel(rotaryHousingS(prof[i].x)), t(prof[i].y), g);
        }
        for(int i = 0; i < 5; i++)
        {
            face(o, c, ring[i], ring[(i + 1) % 5], glm::vec3{0.f, sy, 0.f});
        }
    }
    // The bezel (a frame of four bars), the window's floor and the bolts, in the sloped face's frame: u across, v up the
    // slope, n out of it; its origin the window's middle on the face.
    const glm::vec3 origin = back + up * (low + 0.5f * bezelV);
    const float U = 0.9f * h, V = 0.6f * h, wu = 0.8f * h, wv = 0.5f * h, proud = 0.1f * h;
    const float bs = static_cast<float>(p.boreS), bt = static_cast<float>(p.boreT);
    const Rect plate{184.f, 142.f, 200.f, 158.f, false}; // (Dwell's brown housing plate)
    const Rect black{bs, bt, bs, bt, false};
    const Rect bolt{6.f, 186.f, 10.f, 190.f, false}; // (Dwell's bolt head)
    frameBox(o, origin, across, up, out, {wu, -V, -0.1f}, {U, V, proud}, plate);
    frameBox(o, origin, across, up, out, {-U, -V, -0.1f}, {-wu, V, proud}, plate);
    frameBox(o, origin, across, up, out, {-wu, wv, -0.1f}, {wu, V, proud}, plate);
    frameBox(o, origin, across, up, out, {-wu, -V, -0.1f}, {wu, -wv, proud}, plate);
    frameBox(o, origin, across, up, out, {-wu, -wv, -0.1f}, {wu, wv, 0.02f}, black);
    const float boltR = 0.07f * h, boltU = 0.5f * (U + wu), boltV = 0.5f * (V + wv);
    for(const float su : {-1.f, 1.f})
    {
        for(const float sv : {-1.f, 1.f})
        {
            frameBox(o, origin, across, up, out, {su * boltU - boltR, sv * boltV - boltR, proud - 0.02f},
                {su * boltU + boltR, sv * boltV + boltR, proud + 0.06f * h}, bolt);
        }
    }
    p.housing = true;
    p.housingGlass = origin + out * (proud - 0.05f * h);
    p.housingWindow = {2.f * wu, 2.f * wv};
    p.housingOnFace = 4;
}

// Its housing, hollow with a handle inside (dwellrotary item 2; his notes 2026-11-22: "the place where the hand goes, it
// should be actually a hole with a handle inside, just like we do for the super nailgun"). Dwell's housing (a solid
// octagon with a plate on its back) taken off (Recipe::dropMore) and made again as the super nailgun's is (v_nail2.mdl:
// a shell open at the back, a flat handle across its middle from top to bottom): its outside Dwell's octagon
// (rotaryHousingSize: 5.46 out at the back, 4.53 at its front collar x 7.25..8.45, closed at 8.45 where Dwell's neck
// goes on), each face on its band of Dwell's housing column (rotaryHousingS along it); open at the back (x -2.6) in a
// rim 0.6 wide, the hole black inside, its floor at x 4 (past the fingers round the handle); the handle
// where the super nailgun's is (the same hand values hold it: weapons.txt slot 56 is slot 4's), x -0.3..2.5, 0.9 wide,
// into the walls above and below, on Dwell's brown housing plate.
constexpr float rotaryWall = 0.6f, rotaryHoleFloor = 4.f;

[[nodiscard]] za::Vector<glm::vec2> rotaryOctagon(float s)
{
    constexpr float q = 0.4142f; // (tan 22.5 degrees: a regular octagon's corners)
    return {{s, q * s}, {q * s, s}, {-q * s, s}, {-s, q * s}, {-s, -q * s}, {-q * s, -s}, {q * s, -s}, {s, -q * s}};
}

void rotaryHousingShell(Parts& p)
{
    Out& o = p.o;
    // Each face's band of Dwell's housing column (t at its first corner, at its last), round from the inner side's
    // upper diagonal (y+ z+): Dwell's octagon unrolled.
    constexpr float band[8][2] = {{254.f, 219.f}, {219.f, 197.f}, {197.f, 161.f}, {161.f, 138.f}, {138.f, 102.f},
        {102.f, 80.f}, {80.f, 43.f}, {34.f, 11.f}};
    const int cs = p.boreS, ct = p.boreT; // (the hole black, so it reads as going in)
    // The faces from ring (xa, size sa) to ring (xb, sb): outside in the bands, or (inside) dark facing the axis.
    const auto ringToRing = [&](float xa, float sa, float xb, float sb, bool inside) {
        const za::Vector<glm::vec2> a = rotaryOctagon(sa), b = rotaryOctagon(sb);
        for(int i = 0; i < 8; i++)
        {
            const int j = (i + 1) % 8;
            const int g = o.newGroup();
            const auto v = [&](float x, const glm::vec2& q, float t) {
                return inside ? o.vert({x, q.x, q.y}, cs, ct, g) : o.vert({x, q.x, q.y}, texel(rotaryHousingS(x)), texel(t), g);
            };
            const int a0 = v(xa, a[i], band[i][0]), a1 = v(xa, a[j], band[i][1]);
            const int b0 = v(xb, b[i], band[i][0]), b1 = v(xb, b[j], band[i][1]);
            const glm::vec2 m = (a[i] + a[j]) * (inside ? -0.5f : 0.5f);
            face(o, a0, b0, b1, glm::vec3{0.f, m.x, m.y});
            face(o, a0, b1, a1, glm::vec3{0.f, m.x, m.y});
        }
    };
    constexpr float xBack = -2.6f, xPlate = -1.83f, xCollar = 7.25f, xFront = 8.45f;
    ringToRing(xBack, rotaryHousingSize(xBack), xPlate, rotaryHousingSize(xPlate), false);
    ringToRing(xPlate, rotaryHousingSize(xPlate), xCollar, rotaryHousingSize(xCollar), false);
    ringToRing(xCollar, rotaryHousingSize(xCollar), xFront, rotaryHousingSize(xFront), false);
    const Rect plate{184.f, 142.f, 200.f, 158.f, false}; // (Dwell's brown housing plate)
    cap(o, rotaryOctagon(rotaryHousingSize(xFront)), xFront, 1.f, plate);
    // The hole: its walls, its floor, the rim round it.
    const float sBack = rotaryHousingSize(xBack) - rotaryWall, sFloor = rotaryHousingSize(rotaryHoleFloor) - rotaryWall;
    ringToRing(xBack, sBack, rotaryHoleFloor, sFloor, true);
    const float bs = static_cast<float>(p.boreS), bt = static_cast<float>(p.boreT);
    cap(o, rotaryOctagon(sFloor), rotaryHoleFloor, -1.f, Rect{bs, bt, bs, bt, false});
    const za::Vector<glm::vec2> rimOut = rotaryOctagon(rotaryHousingSize(xBack)), rimIn = rotaryOctagon(sBack);
    for(int i = 0; i < 8; i++)
    {
        const int j = (i + 1) % 8;
        const int g = o.newGroup();
        const int s0 = texel(254.f), s1 = texel(250.f);
        const int a0 = o.vert({xBack, rimOut[i].x, rimOut[i].y}, s0, texel(band[i][0]), g);
        const int a1 = o.vert({xBack, rimOut[j].x, rimOut[j].y}, s0, texel(band[i][1]), g);
        const int b0 = o.vert({xBack, rimIn[i].x, rimIn[i].y}, s1, texel(band[i][0]), g);
        const int b1 = o.vert({xBack, rimIn[j].x, rimIn[j].y}, s1, texel(band[i][1]), g);
        face(o, a0, a1, b1, glm::vec3{-1.f, 0.f, 0.f});
        face(o, a0, b1, b0, glm::vec3{-1.f, 0.f, 0.f});
    }
    // The handle, top to bottom across the hole, into its walls.
    const float hz = rotaryHousingSize(-0.3f) - rotaryWall * 0.5f;
    const Rect plateAlong{184.f, 142.f, 200.f, 158.f, true};
    const Rect handleFaces[4] = {plate, plateAlong, plate, plateAlong};
    box(o, {-0.3f, -0.45f, -hz}, {2.5f, 0.45f, hz}, handleFaces, plate, plate);
}

// Its shell port (his notes 2026-11-21: "I cannot find a way to reload the rotary shotgun"): a block on the housing's
// inner side (+y: towards the body held in the right hand, mirrored in the left; the side slopes out from y 6.3 at x 8.2
// to 7.4 at x -1.8), its outer face flat at y 7.2, a dark opening in a raised rim there, the Widowmaker's way
// (widowmakerParts). vr_view_internal.hpp loadPorts (DwVrRotary): the opening's middle (5 7.25 0).
// His notes 2026-11-22 ("the place where you put the shells in should be a little bit bigger, maybe 10% bigger, and its
// internal color should be black and look a little bit more inset. At the moment, it looks a bit too shiny"): the
// opening 10% bigger each way (3.96 x 1.21, was 3.6 x 1.1), a pocket 0.6 deep into the block, its walls and floor black
// (the bores' texels: it was a box standing out of the face in the caps' dark grey, mirrored pale: vr_envmap.cpp
// madeMetal); the block reaches into the housing's wall (rotaryHousingShell: the side is 5.46 out at most).
void rotaryGauntletParts(Parts& p)
{
    rotaryHousingShell(p);
    rotaryScreenWedge(p);
    dwBarrels(p);
    Out& o = p.o;
    const float bs = static_cast<float>(p.boreS), bt = static_cast<float>(p.boreT);
    const Rect black{bs, bt, bs, bt, false};
    const Rect plate{184.f, 142.f, 200.f, 158.f, false}; // (Dwell's brown housing plate, as the bezel's)
    const Rect plateAlong{184.f, 142.f, 200.f, 158.f, true};
    constexpr float bx0 = 2.5f, bx1 = 7.5f, by0 = 4.7f, face = 7.1f, bz = 1.f, deep = 0.6f;
    constexpr float px0 = 3.02f, px1 = 6.98f, pz0 = -0.605f, pz1 = 0.605f, rim = 0.25f, proud = 0.25f;
    const Rect blockFaces[4] = {plate, plateAlong, plate, plate};
    box(o, {bx0, by0, -bz}, {bx1, face - deep, bz}, blockFaces, plate, plate);
    // The block's outer part a frame round the pocket (each part's face towards it black), the pocket's floor black.
    box(o, {bx0, face - deep, -bz}, {px0, face, bz}, blockFaces, plate, black);
    box(o, {px1, face - deep, -bz}, {bx1, face, bz}, blockFaces, black, plate);
    const Rect frameTop[4] = {black, plateAlong, plate, plate};    // (its underside faces the pocket)
    const Rect frameBottom[4] = {plate, plateAlong, black, plate}; // (its top faces it)
    box(o, {px0, face - deep, pz1}, {px1, face, bz}, frameTop, plate, plate);
    box(o, {px0, face - deep, -bz}, {px1, face, pz0}, frameBottom, plate, plate);
    box(o, {px0, face - deep - 0.05f, pz0}, {px1, face - deep + 0.03f, pz1}, {black, black, black, black}, black, black);
    // The raised rim round the opening, its inner faces black.
    const Rect rimFaces[4] = {plateAlong, plateAlong, plateAlong, plateAlong};
    const Rect rimTop[4] = {black, plateAlong, plateAlong, plateAlong};
    const Rect rimBottom[4] = {plateAlong, plateAlong, black, plateAlong};
    box(o, {px0 - rim, face - 0.05f, pz1}, {px1 + rim, face + proud, pz1 + rim}, rimTop, plateAlong, plateAlong);
    box(o, {px0 - rim, face - 0.05f, pz0 - rim}, {px1 + rim, face + proud, pz0}, rimBottom, plateAlong, plateAlong);
    box(o, {px0 - rim, face - 0.05f, pz0}, {px0, face + proud, pz1}, rimFaces, plateAlong, black);
    box(o, {px1, face - 0.05f, pz0}, {px1 + rim, face + proud, pz1}, rimFaces, black, plateAlong);
}

void lanceParts(Parts& p)
{
    housing(p, lanceHousingSpec);
}

// The vertices' welding for their normals: at one place (to a 0.001 grid) and in one smoothing group (Out::group).
[[nodiscard]] za::Vector<int> weldGrouped(const za::Vector<glm::vec3>& pos, const za::Vector<int>& group)
{
    ankerl::unordered_dense::map<uint64_t, int> at;
    za::Vector<int> w;
    for(za::SizeT i = 0; i < pos.size(); i++)
    {
        const auto q = [&](float x) { return static_cast<uint64_t>(static_cast<int64_t>(x * 1000.f + (x < 0.f ? -0.5f : 0.5f)) & 0x3ffff); };
        const uint64_t key = q(pos[i].x) | (q(pos[i].y) << 18) | (q(pos[i].z) << 36) | (static_cast<uint64_t>(group[i] & 0x3ff) << 54);
        w.pushBack(at.try_emplace(key, static_cast<int>(i)).first->second);
    }
    return w;
}

[[nodiscard]] bool build(const Recipe& r, Built& out, const char*& why, unsigned int* pathId)
{
    Mesh src;
    if(!load(r.source, r.frame, src, pathId))
    {
        why = missingSourceWhy;
        return false;
    }
    Mesh graft;
    if(r.graft.path && !load(r.graft, nullptr, graft, nullptr))
    {
        why = "Quake VR's model to take the grip from not found";
        return false;
    }
    for(const int seed : r.graftSeeds)
    {
        if(r.graft.path && seed >= static_cast<int>(graft.pos.size()))
        {
            why = "Quake VR's model to take the grip from has fewer vertices than its seeds";
            return false;
        }
    }

    // The grafted pieces' texels: their rectangle of the Quake VR model's skin, copied under AD's.
    za::Vector<bool> dropped = piecesOf(src, r.drop);
    const za::Vector<bool> droppedMore = piecesOf(src, r.dropMore);
    for(za::SizeT t = 0; t < dropped.size(); t++)
    {
        dropped[t] = dropped[t] || droppedMore[t];
    }
    const za::Vector<bool> grafted = r.graft.path ? piecesOf(graft, r.graftSeeds) : za::Vector<bool>{};
    int rx0 = 1 << 30, ry0 = 1 << 30, rx1 = -1, ry1 = -1;
    for(za::SizeT t = 0; t < grafted.size(); t++)
    {
        if(!grafted[t])
        {
            continue;
        }
        const bool front = graft.tris[t * 4] != 0;
        for(int c = 0; c < 3; c++)
        {
            const int v = graft.tris[t * 4 + 1 + c];
            const int s = graft.st[static_cast<za::SizeT>(v) * 3 + 1] + (graft.st[static_cast<za::SizeT>(v) * 3] && !front ? graft.skinW / 2 : 0);
            const int tt = graft.st[static_cast<za::SizeT>(v) * 3 + 2];
            rx0 = za::min(rx0, s);
            rx1 = za::max(rx1, s);
            ry0 = za::min(ry0, tt);
            ry1 = za::max(ry1, tt);
        }
    }
    const int gw = rx1 >= rx0 ? za::min(rx1, graft.skinW - 1) - rx0 + 1 : 0;
    const int gh = ry1 >= ry0 ? za::min(ry1, graft.skinH - 1) - ry0 + 1 : 0;
    const int skinW = za::max(src.skinW, gw), skinH = src.skinH + gh + capRows;
    za::Vector<byte> skin(static_cast<za::SizeT>(skinW) * static_cast<za::SizeT>(skinH), capColour);
    for(int y = 0; y < src.skinH; y++)
    {
        ZA_MEMCPY(skin.data() + static_cast<za::SizeT>(y) * skinW, src.skin.data() + static_cast<za::SizeT>(y) * src.skinW, static_cast<za::SizeT>(src.skinW));
    }
    if(const SkinPatch* sp = r.skinPatch)
    {
        for(int y = sp->t0; y <= sp->t1; y++)
        {
            const int from = y + (y < sp->split ? sp->shiftLo : sp->shiftHi);
            for(int x = sp->s0; x <= sp->s1 && from < src.skinH && x < src.skinW; x++)
            {
                skin[static_cast<za::SizeT>(y) * static_cast<za::SizeT>(skinW) + static_cast<za::SizeT>(x)] =
                    src.skin[static_cast<za::SizeT>(from) * static_cast<za::SizeT>(src.skinW) + static_cast<za::SizeT>(x)];
            }
        }
    }
    for(int y = 0; y < gh; y++)
    {
        ZA_MEMCPY(skin.data() + static_cast<za::SizeT>(src.skinH + y) * skinW, graft.skin.data() + static_cast<za::SizeT>(ry0 + y) * graft.skinW + rx0,
            static_cast<za::SizeT>(gw));
    }

    // AD's pieces kept, laid into the weapon's frame; cut; the grip put on; the holes closed.
    glm::vec3 sx, sy, sz, dx, dy, dz;
    basis(r.srcAxis, r.srcUp, sx, sy, sz);
    basis(r.dstAxis, r.dstUp, dx, dy, dz);
    const auto lay = [&](const glm::vec3& p) {
        const glm::vec3 d = p - r.srcPivot;
        return r.dstPivot + (dx * glm::dot(d, sx) + dy * glm::dot(d, sy) + dz * glm::dot(d, sz)) * r.k;
    };
    Out o;
    if(r.takeSource && r.spinBarrels > 0)
    {
        // (Dwell support: the source's turning pieces laid as spinning vertices, the rest as still ones.)
        const za::Vector<bool> turning = piecesPast(src, r.spinFromX);
        za::Vector<bool> still = dropped, turned = dropped;
        for(za::SizeT t = 0; t < dropped.size(); t++)
        {
            still[t] = dropped[t] || turning[t];
            turned[t] = !dropped[t] && turning[t];
        }
        take(o, src, still, false, lay, 0, 0);
        o.spinning = true;
        take(o, src, turned, true, lay, 0, 0);
        o.spinning = false;
    }
    else if(r.takeSource)
    {
        take(o, src, dropped, false, lay, 0, 0);
        if(r.clip)
        {
            clip(o, r.clipN, r.clipD, 0);
        }
    }
    const za::SizeT adTris = o.tris.size();
    const za::SizeT adPos = o.pos.size();
    if(r.graft.path)
    {
        // The pieces of r.liftSeed and r.liftAlso moved by r.lift (the Plasma Gun's trigger, the Lance's trigger guard and
        // trigger up onto the body), the others as they are.
        const int liftSeeds[3] = {r.liftSeed, r.liftAlso, -1};
        const za::Vector<bool> lifted = piecesOf(graft, liftSeeds);
        za::Vector<bool> still = grafted;
        for(za::SizeT t = 0; t < still.size(); t++)
        {
            still[t] = still[t] && !lifted[t];
        }
        take(o, graft, still, true, [](const glm::vec3& p) { return p; }, -rx0, src.skinH - ry0);
        take(o, graft, lifted, true, [&](const glm::vec3& p) { return p + r.lift; }, -rx0, src.skinH - ry0);
    }
    // Each grafted piece's box (as laid) and the body's surface over its top (vr_adweapon_models: how it meets the body).
    za::Vector<glm::vec3> tops[3];
    for(int k = 0; k < 3; k++)
    {
        out.seedLo[k] = glm::vec3{1e30f};
        out.seedHi[k] = glm::vec3{-1e30f};
        out.adAbove[k] = 1e30f;
        if(!r.graft.path || r.graftSeeds[k] < 0)
        {
            continue;
        }
        const int one[3] = {r.graftSeeds[k], -1, -1};
        const za::Vector<bool> piece = piecesOf(graft, one);
        const glm::vec3 move = r.graftSeeds[k] >= 0 && (r.graftSeeds[k] == r.liftSeed || r.graftSeeds[k] == r.liftAlso) ? r.lift : glm::vec3{0.f};
        for(za::SizeT t = 0; t < piece.size(); t++)
        {
            for(int c = 0; c < 3 && piece[t]; c++)
            {
                const glm::vec3 q = graft.pos[graft.tris[t * 4 + 1 + c]] + move;
                out.seedLo[k] = glm::min(out.seedLo[k], q);
                out.seedHi[k] = glm::max(out.seedHi[k], q);
            }
        }
        for(za::SizeT t = 0; t < piece.size(); t++)
        {
            for(int c = 0; c < 3 && piece[t]; c++)
            {
                tops[k].pushBack(graft.pos[graft.tris[t * 4 + 1 + c]] + move);
            }
        }
    }
    (void)adPos;
    const za::SizeT graftEnd = o.tris.size();
    out.grafted = static_cast<int>((o.tris.size() - adTris) / 3);
    out.dropped = 0;
    for(const bool d : dropped)
    {
        out.dropped += d;
    }
    // The bores' black: a few texels of the cap rows' corner (the dark grey's are at their other end).
    for(int y = skinH - capRows; y < skinH; y++)
    {
        for(int x = 0; x < 3; x++)
        {
            skin[static_cast<za::SizeT>(y) * static_cast<za::SizeT>(skinW) + static_cast<za::SizeT>(x)] = 0;
        }
    }
    Parts parts{o, graft, -rx0, src.skinH - ry0, skinW - 2, skinH - 2, 1, skinH - 2};
    parts.corner = r.corner;
    parts.layK = r.k;
    parts.layFrom = r.srcPivot;
    parts.layTo = r.dstPivot;
    if(r.parts)
    {
        r.parts(parts);
    }
    out.housing = parts.housing;
    out.housingGlass = parts.housingGlass;
    out.housingWindow = parts.housingWindow;
    out.housingOnFace = parts.housingOnFace;
    out.noteName = parts.noteName;
    out.noteLo = parts.noteLo;
    out.noteHi = parts.noteHi;
    ZA_MEMCPY(out.noteTop, parts.noteTop, sizeof(out.noteTop));
    out.made = static_cast<int>(o.tris.size() / 3) - static_cast<int>(adTris / 3) - out.grafted;
    out.sealed = r.keepOpen ? 0 : seal(o, r, skinW - 2, skinH - 2);
    // Straight up from each grafted piece's vertices within 0.3 of its top: the first triangle met that is not a grafted
    // piece's (AD's, a made part, a hole's fan): the least gap's height.
    for(int k = 0; k < 3; k++)
    {
        float best = 1e30f;
        for(const glm::vec3& q : tops[k])
        {
            if(q.z < out.seedHi[k].z - 0.3f)
            {
                continue;
            }
            for(za::SizeT i = 0; i + 2 < o.tris.size(); i += 3)
            {
                if(i >= adTris && i < graftEnd)
                {
                    continue;
                }
                const glm::vec3 &a = o.pos[o.tris[i]], &b = o.pos[o.tris[i + 1]], &e = o.pos[o.tris[i + 2]];
                const float den = (b.y - e.y) * (a.x - e.x) + (e.x - b.x) * (a.y - e.y);
                if(za::abs(den) < 1e-9f)
                {
                    continue;
                }
                const float u = ((b.y - e.y) * (q.x - e.x) + (e.x - b.x) * (q.y - e.y)) / den;
                const float v = ((e.y - a.y) * (q.x - e.x) + (a.x - e.x) * (q.y - e.y)) / den;
                const float z = u * a.z + v * b.z + (1.f - u - v) * e.z;
                if(u >= 0.f && v >= 0.f && u + v <= 1.f && z >= q.z - 0.05f && z - q.z < best)
                {
                    best = z - q.z;
                    out.adAbove[k] = z;
                    out.adGap[k] = z - q.z;
                }
            }
        }
    }
    {
        za::Vector<int> w = weldNear(o.pos, capWeld);
        Out copy = o;
        (void)holes(copy, w, out.openAfter);
    }

    // Only the vertices used; their normals (the welded surface's, area weighted).
    za::Vector<int> used(o.pos.size(), -1);
    Out c;
    for(int& v : o.tris)
    {
        if(used[v] < 0)
        {
            c.spinning = o.spins[v];
            used[v] = c.vert(o.pos[v], o.s[v], o.t[v], o.group[v]);
        }
        v = used[v];
    }
    c.tris = o.tris;
    const za::Vector<int> w = weldGrouped(c.pos, c.group);
    za::Vector<glm::vec3> normal(c.pos.size(), glm::vec3{0.f});
    for(za::SizeT i = 0; i + 2 < c.tris.size(); i += 3)
    {
        const glm::vec3& a = c.pos[c.tris[i]];
        const glm::vec3 n = glm::cross(c.pos[c.tris[i + 2]] - a, c.pos[c.tris[i + 1]] - a);
        for(int k = 0; k < 3; k++)
        {
            normal[w[c.tris[i + k]]] += n;
        }
    }

    // The spin frames (a model with a turning part: spinPoses after the frameCount still ones): its vertices and normals
    // turned about the spin axis, pose k by k of spinStep degrees, clockwise seen from behind (+x ahead, +y left).
    for(const bool sp : c.spins)
    {
        out.spinVerts += sp ? 1 : 0;
    }
    const int poses = frameCount + (out.spinVerts > 0 ? spinPoses : 0);
    const glm::vec3 axisAt = r.spinBarrels > 0 ? lay(r.spinAxisAt) : glm::vec3{0.f, 0.f, spinZ};
    out.spinAxis = axisAt;
    const auto posed = [&](int pose, za::SizeT i, const glm::vec3& v, bool point) {
        if(pose < frameCount || !c.spins[i])
        {
            return v;
        }
        const float a = -glm::radians(spinStep * static_cast<float>(pose - frameCount));
        const float ca = za::cos(a), sa = za::sin(a);
        const float y = point ? v.y - axisAt.y : v.y, z = point ? v.z - axisAt.z : v.z;
        return glm::vec3{v.x, (point ? axisAt.y : 0.f) + y * ca - z * sa, (point ? axisAt.z : 0.f) + y * sa + z * ca};
    };

    // Packed from the Quake VR model's corner (a vertex past it: the corner moved, placement off).
    glm::vec3 lo{1e30f}, hi{-1e30f};
    for(int pose = 0; pose < poses; pose += pose < frameCount ? frameCount : 1)
    {
        for(za::SizeT i = 0; i < c.pos.size(); i++)
        {
            const glm::vec3 p = posed(pose, i, c.pos[i], true);
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
    }
    out.cornerKept = lo.x >= r.corner.x - 1e-4f && lo.y >= r.corner.y - 1e-4f && lo.z >= r.corner.z - 1e-4f;
    lo = glm::min(lo, r.corner);
    if(r.keepScale > 0.f)
    {
        // Past the corner (Recipe::keepScale): moved by (corner - lo) (1 - scale), so that drawn at that Scale about its
        // own packed corner every point lands where it would about the base model's corner.
        const glm::vec3 move = (r.corner - lo) * (1.f - r.keepScale);
        for(glm::vec3& p : c.pos)
        {
            p += move;
        }
        lo += move;
        hi += move;
        out.cornerKept = true; // (moved to keep it: the settings' places hold)
    }
    out.lo = lo;
    out.hi = hi;
    const glm::vec3 pack = glm::max((hi - lo) / 255.f, glm::vec3{1e-4f});
    float radius = 0.f;
    for(const glm::vec3& p : c.pos)
    {
        radius = za::max(radius, glm::length(p));
    }

    // Where its muzzle is: a gun's front end's middle (its AD part's vertices within half a unit of its front); the axe's
    // blade edge (the AD part's vertices furthest out along the blade's way, within 2.5 units of the furthest: its ends along the handle).
    const bool axe = r.graft.path == nullptr;
    za::SizeT adVerts = 0;
    for(za::SizeT i = 0; i < adTris; i++)
    {
        adVerts = za::max(adVerts, static_cast<za::SizeT>(c.tris[i]) + 1);
    }
    float furthest = -1e30f;
    for(za::SizeT i = 0; i < adVerts; i++)
    {
        furthest = za::max(furthest, axe ? glm::dot(c.pos[i], r.dstUp) : c.pos[i].x);
    }
    int count = 0;
    out.edgeLo = glm::vec3{0.f, 0.f, 1e30f};
    out.edgeHi = glm::vec3{0.f, 0.f, -1e30f};
    out.muzzle = glm::vec3{0.f};
    for(za::SizeT i = 0; i < adVerts; i++)
    {
        const glm::vec3& p = c.pos[i];
        if((axe ? glm::dot(p, r.dstUp) : p.x) < furthest - (axe ? 2.5f : 0.5f))
        {
            continue;
        }
        out.muzzle += p;
        count++;
        if((axe ? glm::dot(p, r.dstAxis) : p.z) < (axe ? glm::dot(out.edgeLo, r.dstAxis) : out.edgeLo.z) || count == 1)
        {
            out.edgeLo = p;
        }
        if((axe ? glm::dot(p, r.dstAxis) : p.z) > (axe ? glm::dot(out.edgeHi, r.dstAxis) : out.edgeHi.z) || count == 1)
        {
            out.edgeHi = p;
        }
    }
    out.muzzle = axe ? out.edgeHi : out.muzzle / static_cast<float>(za::max(count, 1));
    if(parts.muzzleSet)
    {
        out.muzzle = parts.muzzle; // (its made barrels' front's middle)
    }

    za::Vector<byte>& f = out.file;
    f.clear();
    put(f, IDPOLYHEADER);
    put(f, ALIAS_VERSION);
    putf(f, pack.x);
    putf(f, pack.y);
    putf(f, pack.z);
    putf(f, lo.x);
    putf(f, lo.y);
    putf(f, lo.z);
    putf(f, radius);
    putf(f, 0.f);
    putf(f, 0.f);
    putf(f, 0.f);
    put(f, 1);
    put(f, skinW);
    put(f, skinH);
    put(f, static_cast<int>(c.pos.size()));
    put(f, static_cast<int>(c.tris.size() / 3));
    put(f, poses);
    put(f, 0); // synctype
    put(f, 0); // flags
    putf(f, 1.f);
    put(f, 0); // a single skin
    f.emplaceBackRange(skin.data(), skin.size());
    for(za::SizeT i = 0; i < c.pos.size(); i++)
    {
        put(f, 0);
        put(f, c.s[i]);
        put(f, c.t[i]);
    }
    for(za::SizeT i = 0; i + 2 < c.tris.size(); i += 3)
    {
        put(f, 1);
        put(f, c.tris[i]);
        put(f, c.tris[i + 1]);
        put(f, c.tris[i + 2]);
    }
    za::Vector<byte> packed;
    byte bmin[4], bmax[4];
    for(int frame = 0; frame < poses; frame++)
    {
        if(frame == 0 || frame >= frameCount) // (the still frames: one pose)
        {
            packed.clear();
            for(int k = 0; k < 3; k++)
            {
                bmin[k] = 255;
                bmax[k] = 0;
            }
            bmin[3] = bmax[3] = 0;
            for(za::SizeT i = 0; i < c.pos.size(); i++)
            {
                byte b[4];
                const glm::vec3 p = posed(frame, i, c.pos[i], true);
                for(int k = 0; k < 3; k++)
                {
                    const float q = (p[k] - lo[k]) / pack[k];
                    b[k] = static_cast<byte>(za::max(0, za::min(255, static_cast<int>(q + 0.5f))));
                    bmin[k] = za::min(bmin[k], b[k]);
                    bmax[k] = za::max(bmax[k], b[k]);
                }
                const glm::vec3 n = posed(frame, i, normal[w[i]], false);
                b[3] = qvr::mdlbuild::nearestNormal(glm::length(n) > 1e-12f ? glm::normalize(n) : glm::vec3{0.f, 0.f, 1.f});
                packed.emplaceBackRange(b, 4);
            }
        }
        put(f, 0); // a single frame
        f.emplaceBackRange(bmin, 4);
        f.emplaceBackRange(bmax, 4);
        char name[16] = {};
        if(frame < frameCount)
        {
            q_snprintf(name, sizeof(name), "vr%d", frame);
        }
        else
        {
            q_snprintf(name, sizeof(name), r.spinBarrels == 4 ? "quad%d" : "spin%d", frame - frameCount);
        }
        f.emplaceBackRange(reinterpret_cast<const byte*>(name), 16);
        f.emplaceBackRange(packed.data(), packed.size());
    }
    out.verts = static_cast<int>(c.pos.size());
    out.tris = static_cast<int>(c.tris.size() / 3);
    return true;
}

void setf(byte* f, za::SizeT at, float v)
{
    const float le = LittleFloat(v);
    ZA_MEMCPY(f + at, &le, 4);
}

// A side part (sideParts): the file of its source turned a quarter about x round lightSeat (its -z becomes +y: x' = x,
// y' = seat'.y - (z - seat.z), z' = seat'.z + (y - seat.y)) and lightSeat moved onto plasmaSeat; exact (the packed
// bytes: y' = 255 - z, z' = y, their scales swapped), its normals turned with it. Null: not found or not a model.
[[nodiscard]] byte* sideFile(const SidePart& part, unsigned int* path_id)
{
    const char* name = part.name;
    const char* base = part.from;
    byte* f = COM_LoadMallocFile(base, path_id);
    if(!f)
    {
        return nullptr;
    }
    const za::SizeT size = static_cast<za::SizeT>(com_filesize);
    if(size < headerSize || qvr::bytes::le32(f, 0) != IDPOLYHEADER || qvr::bytes::le32(f, 4) != ALIAS_VERSION)
    {
        VR_HeapFree(f);
        return nullptr;
    }
    const int numSkins = qvr::bytes::le32(f, 48), skinW = qvr::bytes::le32(f, 52), skinH = qvr::bytes::le32(f, 56), numVerts = qvr::bytes::le32(f, 60), numTris = qvr::bytes::le32(f, 64),
              numFrames = qvr::bytes::le32(f, 68);
    const glm::vec3 scale{qvr::bytes::lef32(f, 8), qvr::bytes::lef32(f, 12), qvr::bytes::lef32(f, 16)}, origin{qvr::bytes::lef32(f, 20), qvr::bytes::lef32(f, 24), qvr::bytes::lef32(f, 28)};
    const glm::vec3 eye{qvr::bytes::lef32(f, 36), qvr::bytes::lef32(f, 40), qvr::bytes::lef32(f, 44)};
    const glm::vec3 p = lightSeat, s = plasmaSeat;
    const glm::vec3 scale2{scale.x, scale.z, scale.y};
    const glm::vec3 origin2{origin.x - p.x + s.x, s.y + p.z - origin.z - 255.f * scale.z, origin.y - p.y + s.z};
    for(int i = 0; i < 3; i++)
    {
        setf(f, 8 + static_cast<za::SizeT>(i) * 4, scale2[i]);
        setf(f, 20 + static_cast<za::SizeT>(i) * 4, origin2[i]);
    }
    setf(f, 36, eye.x - p.x + s.x);
    setf(f, 40, s.y - (eye.z - p.z));
    setf(f, 44, s.z + (eye.y - p.y));
    const za::SizeT skinBytes = static_cast<za::SizeT>(za::max(skinW, 0)) * static_cast<za::SizeT>(za::max(skinH, 0));
    za::SizeT at = headerSize;
    for(int k = 0; k < numSkins && at + 8 <= size; k++)
    {
        at += qvr::bytes::le32(f, at) == 0 ? 4 + skinBytes : 8 + static_cast<za::SizeT>(za::max(qvr::bytes::le32(f, at + 4), 1)) * (4 + skinBytes);
    }
    at += static_cast<za::SizeT>(numVerts) * 12 + static_cast<za::SizeT>(numTris) * 16;
    const auto turnBox = [&](za::SizeT boxAt) {
        byte* lo = f + boxAt;
        byte* hi = f + boxAt + 4;
        const byte loY = lo[1], loZ = lo[2], hiY = hi[1], hiZ = hi[2];
        lo[1] = static_cast<byte>(255 - hiZ);
        hi[1] = static_cast<byte>(255 - loZ);
        lo[2] = loY;
        hi[2] = hiY;
    };
    float radius = 0.f;
    for(int fr = 0; fr < numFrames && at + 4 <= size; fr++)
    {
        const int type = qvr::bytes::le32(f, at);
        at += 4;
        int members = 1;
        if(type != 0)
        {
            members = za::max(qvr::bytes::le32(f, at), 1);
            turnBox(at + 4);
            at += 4 + 8 + static_cast<za::SizeT>(members) * 4;
        }
        for(int m = 0; m < members; m++)
        {
            const za::SizeT vertsAt = at + 24;
            if(vertsAt + static_cast<za::SizeT>(numVerts) * 4 > size)
            {
                VR_HeapFree(f);
                return nullptr;
            }
            turnBox(at);
            for(int v = 0; v < numVerts; v++)
            {
                byte* b = f + vertsAt + static_cast<za::SizeT>(v) * 4;
                const byte y = b[1], z = b[2];
                b[1] = static_cast<byte>(255 - z);
                b[2] = y;
                const int ni = za::min(static_cast<int>(b[3]), NUMVERTEXNORMALS - 1);
                b[3] = qvr::mdlbuild::nearestNormal(glm::vec3{r_avertexnormals[ni][0], -r_avertexnormals[ni][2], r_avertexnormals[ni][1]});
                const glm::vec3 q{b[0] * scale2.x + origin2.x, b[1] * scale2.y + origin2.y, b[2] * scale2.z + origin2.z};
                radius = za::max(radius, glm::length(q));
            }
            at = vertsAt + static_cast<za::SizeT>(numVerts) * 4;
        }
    }
    setf(f, 32, radius);
    Con_DPrintf("AD weapon model: %s made (%s turned out of the Plasma Gun's side)\n", name, base);
    return f;
}

// A hand's Widowmaker barrels (spinFrame): their turn (degrees, 0..360: 0 the model's own, a barrel on top each 120),
// speed (degrees a second), and, coasting down, the ease from where they were (p0, at speed v0, at t0) to a barrel on top
// (p1) in `ease` seconds (a cubic: v0 at its start, still at its end).
struct Spin
{
    float phase{0.f}, speed{0.f};
    bool coasting{false};
    float p0{0.f}, p1{0.f}, v0{0.f}, ease{0.f};
    double t0{0.0};
    double lastShot{-1.0}, lastTime{-1.0};
    bool held{false};
    glm::vec3 topFront{0.f}, axisFront{0.f}; // as drawn last: the top barrel's front middle, the spin axis's front (info)
};
Spin spins[2];

// Each hand's weapon as spinFrame saw it last (vr_ad_widowmaker_spin_info, when not held).
struct Drawn
{
    bool visible{false};
    const qmodel_t* model{nullptr};
};
Drawn drawn[2];
bool artLoaded = false; // the Widowmaker's model read last was the hand-made one (derivedFile)

// The Widowmaker drawn (its spin frames), checked once a model (and again when it is read anew: vr_model_reload keeps
// the model's slot). Its barrels: three ("spin0".. frames: the made one and the hand-made one, its export_mdl.py), a
// third of a turn a shot, at rest a barrel on top; a model with "twin0".. frames has two (a half turn a shot).
struct SpinModel
{
    const qmodel_t* model{nullptr};
    const void* hdr{nullptr};
    bool spins{false};
    int barrels{3};
    bool dwell{false}; // Dwell's Rotary Shotgun (its own rate of fire: dwRotaryRefire)
    bool ownFrames{false}; // the super nailgun: its own firing frames are the turn (ownFramesStep a frame), its own spin-up
                           // (vr_sng_spinup), the motor at vr_sng_motor's share
};
SpinModel spinModel;

constexpr const char* widowmakerName = "progs/v_shot3.mdl#vr";
constexpr const char* widowmakerArtName = "progs/v_widowmaker.mdl"; // the hand-made model
// Dwell support: the Rotary Shotgun (its four barrels turned as the Widowmaker's three, the same code in flat and VR
// play: spinFrame), a shot each 0.2 s (QC dw_items.qc DW_FireVariant).
constexpr const char* rotaryName = "progs/dw_shot3.mdl#vr";
constexpr const char* rotaryGauntletName = "progs/dw_rotary.mdl#vr"; // (dwellwpn3: held as a gauntlet)
constexpr float dwRotaryRefire = 0.2f;
// The super nailgun (his note 2026-11-23: its barrels snapped back to their start when the trigger was let go): Quake's
// firing frames 1..8 turn its barrels an eighth of a turn each (their baked kick taken out in memory: vr_kickcancel.hpp;
// frame 8 the rest pose's turn), a frame each 0.1 s (QC player_nail_BaseImpl). Driven by the Widowmaker's spin: up while
// the trigger is held, coasting down after to the nearest frame, never back to frame 0 (spinFrame).
constexpr const char* superNailgunName = "progs/v_nail2.mdl"; // (and its variants: the lava one, gunvariants::resolve)
constexpr int ownFramesTurn = 8;                              // frames 1..8: a whole turn
constexpr float ownFramesStep = 360.f / static_cast<float>(ownFramesTurn);
constexpr float superNailgunRefire = 0.1f; // s a frame (QC: a frame each think)

// (Each hand's weapon is asked about just before its spin is advanced: spinFrame; the last asked is the one cached.)
[[nodiscard]] bool spinsBarrels(const qmodel_t* m)
{
    if(!m || m->type != mod_alias)
    {
        return false;
    }
    const bool sng = !q_strcasecmp(gunvariants::resolve(m->name), superNailgunName);
    if(!sng && q_strcasecmp(m->name, widowmakerName) && q_strcasecmp(m->name, rotaryName) && q_strcasecmp(m->name, rotaryGauntletName))
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(m)));
    if(sng)
    {
        if(m != spinModel.model || hdr != spinModel.hdr)
        {
            spinModel = SpinModel{};
            spinModel.model = m;
            spinModel.hdr = hdr;
            spinModel.spins = hdr && hdr->numframes > ownFramesTurn;
            spinModel.barrels = ownFramesTurn;
            spinModel.ownFrames = true;
        }
        return spinModel.spins;
    }
    if(m != spinModel.model || hdr != spinModel.hdr)
    {
        spinModel.model = m;
        spinModel.hdr = hdr;
        spinModel.ownFrames = false;
        const bool framesOk = hdr && hdr->numframes == frameCount + spinPoses;
        const bool three = framesOk && !ZA_STRCMP(hdr->frames[frameCount].name, "spin0");
        const bool two = framesOk && !ZA_STRCMP(hdr->frames[frameCount].name, "twin0");
        const bool four = framesOk && !ZA_STRCMP(hdr->frames[frameCount].name, "quad0");
        spinModel.spins = three || two || four;
        spinModel.barrels = four ? 4 : two ? 2 : 3;
        spinModel.dwell = !q_strcasecmp(m->name, rotaryName) || !q_strcasecmp(m->name, rotaryGauntletName);
    }
    return spinModel.spins;
}

// The turn from a barrel to the next (degrees).
[[nodiscard]] float barrelStep()
{
    return 360.f / static_cast<float>(spinModel.barrels);
}

// Seconds between its shots (the Widowmaker's vr_ad_widowmaker_refire; Dwell's Rotary Shotgun's own).
[[nodiscard]] float spinRefire()
{
    return spinModel.ownFrames ? superNailgunRefire
           : spinModel.dwell   ? dwRotaryRefire
                               : za::max(vr_ad_widowmaker_refire.value, 0.05f);
}

// The barrels' fastest: a barrel a shot.
[[nodiscard]] float spinTop()
{
    return barrelStep() / spinRefire() * za::max(vr_ad_widowmaker_spin.value, 0.f);
}

void advance(Spin& s, int hand, float dt)
{
    const float top = spinTop();
    const float refire = spinRefire();
    const bool driven = top > 0.f && (client::attacking(hand) || (s.lastShot >= 0.0 && cl.time - s.lastShot < refire * 1.25));
    if(driven)
    {
        s.coasting = false;
        // (the super nailgun fires at once: its own, quicker spin-up, vr_sng_spinup)
        const float spinup = spinModel.ownFrames ? vr_sng_spinup.value : vr_ad_widowmaker_spinup.value;
        s.speed = za::min(top, s.speed + top / za::max(spinup, 0.01f) * dt);
        s.phase += s.speed * dt;
    }
    else if(s.speed > 0.f || s.coasting)
    {
        if(!s.coasting)
        {
            // The stop planned: the next barrel on top past the coast at the nominal rate (top over _spindown s), eased
            // there from this speed (as long as a steady slowing would take; at least a short settle).
            const float step = barrelStep();
            const float rate = za::max(top, step) / za::max(vr_ad_widowmaker_spindown.value, 0.05f);
            const float coast = s.speed * s.speed / (2.f * rate);
            s.p0 = s.phase;
            s.p1 = step * static_cast<float>(static_cast<int>((s.phase + coast) / step + 0.999f));
            s.p1 = za::max(s.p1, s.p0);
            s.v0 = s.speed;
            const float d = s.p1 - s.p0;
            s.ease = d <= 1e-3f ? 0.f : za::min(2.f * d / za::max(s.v0, 1e-3f), za::max(vr_ad_widowmaker_spindown.value, 0.3f));
            s.ease = za::max(s.ease, d > 1e-3f ? 0.15f : 0.f);
            s.ease = za::min(s.ease, d > 1e-3f ? 3.f * d / za::max(s.v0, 1e-3f) : 0.f); // (never past it)
            s.t0 = cl.time;
            s.coasting = true;
        }
        const float u = s.ease > 0.f ? za::min(1.f, static_cast<float>(cl.time - s.t0) / s.ease) : 1.f;
        const float d = s.p1 - s.p0, m = s.v0 * s.ease;
        s.phase = s.p0 + d * (3.f * u * u - 2.f * u * u * u) + m * (u - 2.f * u * u + u * u * u);
        s.speed = s.ease > 0.f ? (d * (6.f * u - 6.f * u * u) + m * (1.f - 4.f * u + 3.f * u * u)) / s.ease : 0.f;
        if(u >= 1.f)
        {
            s.phase = s.p1;
            s.speed = 0.f;
            s.coasting = false;
        }
    }
    if(s.phase >= 360.f)
    {
        s.phase -= 360.f;
        s.p0 -= 360.f;
        s.p1 -= 360.f;
    }
}

// The barrels' motor (his notes 2026-10-19: a motor noise ramping up and down with the spin): a looped whine
// (vr/widowmaker_motor.wav, synthesized: Misc/quakevr/make_adweapon_sounds.py) on the hand's own sound channel (QC
// CHAN_HAND, CHAN_HAND2: from the hand, a channel of its own: the shot on the gun's channel never cut off), its pitch and
// volume set each frame from the spin's share of the top speed; it fades out in motorRelease s when the gun goes (dropped,
// holstered) and stops when the barrels rest.
constexpr const char* motorSoundName = "vr/widowmaker_motor.wav";
constexpr float motorPitchLow = 0.45f; // at a crawl; 1 (as made) at the top speed
constexpr float motorRelease = 0.12f;  // s the motor takes to fade to nothing when its gun is gone at once
constexpr float motorRest = 0.005f;    // a volume under this: stopped

struct Motor
{
    sfx_t* sfx{nullptr};
    channel_t* ch[2]{nullptr, nullptr};
    float volume[2]{0.f, 0.f}, pitch[2]{1.f, 1.f}; // as last set
    float wanted[2]{0.f, 0.f}; // the volume the spin asked for last (heard or not: the tests run without sound)
    double lastTime[2]{-1.0, -1.0};
};
Motor motor;

[[nodiscard]] int motorChannel(int hand)
{
    return hand == HAND_OFF ? SND_CHAN_HAND2 : SND_CHAN_HAND;
}

// The channel still playing `hand`'s motor (another sound may have taken it since), or null. The mix done (S_MixJoin).
[[nodiscard]] channel_t* motorPlaying(int hand)
{
    channel_t* ch = motor.ch[hand];
    return ch && motor.sfx && ch->sfx == motor.sfx && ch->entnum == cl.viewentity && ch->entchannel == motorChannel(hand)
               ? ch
               : nullptr;
}

void motorStop(int hand)
{
    if(motor.ch[hand])
    {
        S_MixJoin(); // (a read of the channels on the main thread: the pool's mix done first, S_MixKick)
        if(channel_t* ch = motorPlaying(hand))
        {
            ch->sfx = nullptr;
        }
    }
    motor.ch[hand] = nullptr;
    motor.volume[hand] = 0.f;
}

// Every frame: `hand`'s motor as its barrels spin (`s`; not held: at rest), at `share` of Motor Volume (the super
// nailgun's vr_sng_motor).
void motorFrame(int hand, const Spin& s, float share = 1.f)
{
    const float dt = motor.lastTime[hand] >= 0.0 ? za::clamp(static_cast<float>(cl.time - motor.lastTime[hand]), 0.f, 0.1f) : 0.f;
    motor.lastTime[hand] = cl.time;
    const float top = spinTop();
    const float u = s.held && top > 0.f ? za::clamp(s.speed / top, 0.f, 1.f) : 0.f;
    const float full = za::clamp(vr_ad_widowmaker_motor.value * za::max(share, 0.f), 0.f, 1.f);
    float volume = cl.paused ? 0.f : full * za::min(1.f, u * 1.25f);
    if(volume < motor.volume[hand] && !s.held) // (gone at once: a short fade, not a cut)
    {
        volume = za::max(volume, motor.volume[hand] - full * dt / motorRelease);
    }
    motor.wanted[hand] = volume;
    if(!motor.sfx || volume < motorRest || cl.viewentity <= 0)
    {
        motorStop(hand);
        return;
    }
    if(s.held)
    {
        motor.pitch[hand] = motorPitchLow + (1.f - motorPitchLow) * u;
    }
    S_MixJoin(); // (a read of the channels on the main thread: the pool's mix done first, S_MixKick)
    channel_t* ch = motorPlaying(hand);
    if(!ch)
    {
        vec3_t org;
        VectorCopy(cl_entities[cl.viewentity].origin, org);
        S_StartSoundPitch(cl.viewentity, motorChannel(hand), motor.sfx, org, volume, 1.f, motor.pitch[hand]); // (ATTN_NORM)
        motor.ch[hand] = nullptr;
        for(int i = 0; i < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; i++)
        {
            channel_t& c = snd_channels[i];
            if(c.sfx == motor.sfx && c.entnum == cl.viewentity && c.entchannel == motorChannel(hand))
            {
                motor.ch[hand] = &c;
                break;
            }
        }
        ch = motor.ch[hand];
        if(!ch)
        {
            motor.volume[hand] = 0.f; // (no sound system, or not heard: tried again next frame)
            return;
        }
    }
    ch->master_vol = static_cast<int>(volume * 255.f);
    ch->pitch = motor.pitch[hand];
    ch->resampled = 1; // (its end reckoned again at the new rate: snd_mix.c)
    motor.volume[hand] = volume;
}

} // namespace

void fired(int hand)
{
    if(hand == HAND_MAIN || hand == HAND_OFF)
    {
        spins[hand].lastShot = cl.time;
    }
}

void spinFrame(view::ViewEntity (&weapons)[2])
{
    for(int hand = 0; hand < 2; hand++)
    {
        view::ViewEntity& w = weapons[hand];
        Spin& s = spins[hand];
        drawn[hand].visible = w.visible;
        drawn[hand].model = w.ent.model;
        if(!w.visible || !spinsBarrels(w.ent.model))
        {
            s = Spin{};
            motorFrame(hand, s);
            continue;
        }
        const float dt = s.held && s.lastTime >= 0.0 ? za::clamp(static_cast<float>(cl.time - s.lastTime), 0.f, 0.1f) : 0.f;
        s.held = true;
        s.lastTime = cl.time;
        advance(s, hand, dt);
        float at = vr_ad_widowmaker_spin_at.value >= 0.f ? vr_ad_widowmaker_spin_at.value : s.phase;
        at -= 360.f * static_cast<float>(static_cast<int>(at / 360.f));
        if(spinModel.ownFrames)
        {
            motorFrame(hand, s, vr_sng_motor.value); // (the Widowmaker's whirr, quieter: his note 2026-11-23)
            // The turn's two frames (k eighths of a turn: frame k; a whole turn frame 8 while turning, so the gun's firing
            // barrel is known, vr_gunfx.cpp firingBarrel; frame 0 at rest), its drawn frame the nearer.
            const float k = at / ownFramesStep;
            const int i = static_cast<int>(k);
            const bool still = s.speed <= 0.f && !s.coasting;
            const auto frameOf = [still](int j)
            {
                j %= ownFramesTurn;
                return j == 0 ? (still ? 0 : ownFramesTurn) : j;
            };
            w.poseFrame[0] = frameOf(i);
            w.poseFrame[1] = frameOf(i + 1);
            w.poseBlend = k - static_cast<float>(i);
            w.ent.frame = w.poseBlend < 0.5f ? w.poseFrame[0] : w.poseFrame[1];
            w.zeroBlend = 0.f;
            continue;
        }
        motorFrame(hand, s);
        const float k = at / spinStep;
        const int i = static_cast<int>(k);
        w.poseFrame[0] = frameCount + i % spinPoses;
        w.poseFrame[1] = frameCount + (i + 1) % spinPoses;
        w.poseBlend = k - static_cast<float>(i);
        w.zeroBlend = 0.f; // (the slot's blend towards frame 0 would pull the barrels back)
        if(vr_debug_weaponfx.value && !spinModel.dwell)
        {
            // (the hand-made model's: its outer barrel's front and its axis, 8.8 up: build_widowmaker.py)
            const bool two = spinModel.barrels == 2;
            s.topFront = view::modelPoint(w, two ? glm::vec3{44.f, -1.72f, 8.8f} : glm::vec3{44.f, 0.f, spinZ + spinR});
            s.axisFront = view::modelPoint(w, glm::vec3{44.f, 0.f, two ? 8.8f : spinZ});
        }
    }
}

void clear()
{
    for(Spin& s : spins)
    {
        s = Spin{};
    }
    spinModel = SpinModel{};
    for(int hand = 0; hand < 2; hand++)
    {
        motorStop(hand);
        motor.lastTime[hand] = -1.0;
    }
}

namespace
{
// Forgotten when the client's state is cleared (vr_mem.hpp's resets): the spins and the motors (AD support).
mem::Reset clientStateReset{"adweapons", mem::ResetEvent::ClientState, mem::ResetPhase::State, &clear};
} // namespace

void prepare()
{
    motor.sfx = S_PrecacheSound(motorSoundName);
}

void spinInfo_f()
{
    hands::State& hs = hands::current();
    for(int hand = 0; hand < 2; hand++)
    {
        const Spin& s = spins[hand];
        if(!s.held)
        {
            const qmodel_t* m = drawn[hand].model;
            const auto* hdr = m && m->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(m))) : nullptr;
            Con_Printf("widowmaker spin hand %d: not held (or no spin frames): drawn %s, visible %d, %d frames%s%s\n", hand,
                m ? m->name : "nothing", drawn[hand].visible ? 1 : 0, hdr ? hdr->numframes : 0,
                hdr && hdr->numframes > frameCount ? ", frame 9 " : "", hdr && hdr->numframes > frameCount ? hdr->frames[frameCount].name : "");
            continue;
        }
        (void)spinsBarrels(drawn[hand].model); // (this hand's model the one asked about: its barrels, its rate)
        if(spinModel.ownFrames)
        {
            const float k = s.phase / ownFramesStep;
            Con_Printf("widowmaker spin hand %d: turn %.1f deg, speed %.0f deg/s (top %.0f)%s, own frames %d %d blend %.2f, "
                       "trigger %d\n",
                hand, s.phase, s.speed, spinTop(), s.coasting ? ", coasting" : "", static_cast<int>(k) % ownFramesTurn,
                (static_cast<int>(k) + 1) % ownFramesTurn, k - static_cast<float>(static_cast<int>(k)), client::attacking(hand) ? 1 : 0);
            Con_Printf("  motor: %s, volume %.2f (wanted %.2f), pitch %.2f\n",
                !motor.sfx ? "no sound" : motor.ch[hand] ? "playing" : "stopped", motor.volume[hand], motor.wanted[hand],
                motor.pitch[hand]);
            Con_Printf("  model: the super nailgun (its own firing frames), %d barrels (%.0f deg a shot), at rest %s\n",
                spinModel.barrels, barrelStep(), !s.coasting && s.speed <= 0.f && fmodf(s.phase, barrelStep()) < 0.01f ? "yes" : "no");
            continue;
        }
        const float k = s.phase / spinStep;
        Con_Printf("widowmaker spin hand %d: turn %.1f deg, speed %.0f deg/s (top %.0f)%s, frames %d %d blend %.2f, trigger %d, "
                   "last shot %.2f s ago\n",
            hand, s.phase, s.speed, spinTop(), s.coasting ? ", coasting" : "", frameCount + static_cast<int>(k) % spinPoses,
            frameCount + (static_cast<int>(k) + 1) % spinPoses, k - static_cast<float>(static_cast<int>(k)),
            client::attacking(hand) ? 1 : 0, s.lastShot < 0.0 ? -1.0 : cl.time - s.lastShot);
        Con_Printf("  motor: %s, volume %.2f, pitch %.2f\n", !motor.sfx ? "no sound" : motor.ch[hand] ? "playing" : "stopped",
            motor.volume[hand], motor.pitch[hand]);
        Con_Printf("  model: %s, %d barrels (%.0f deg a shot), at rest %s\n",
            spinModel.dwell ? "Dwell's Rotary Shotgun (made from Dwell's)"
            : artLoaded     ? "hand-made (progs/v_widowmaker.mdl)"
                            : "made from AD's",
            spinModel.barrels, barrelStep(),
            !s.coasting && s.speed <= 0.f && fmodf(s.phase, barrelStep()) < 0.01f ? "yes" : "no");
        if(hs.muzzleValid[hand])
        {
            Con_Printf("  muzzle %.2f %.2f %.2f\n", hs.muzzle[hand].x, hs.muzzle[hand].y, hs.muzzle[hand].z);
        }
        if(vr_debug_weaponfx.value)
        {
            Con_Printf("  drawn: the top barrel's front %.2f %.2f %.2f (from the muzzle %.2f), the barrels' middle %.2f %.2f %.2f "
                       "(%.2f)\n",
                s.topFront.x, s.topFront.y, s.topFront.z, glm::distance(s.topFront, hs.muzzle[hand]), s.axisFront.x,
                s.axisFront.y, s.axisFront.z, glm::distance(s.axisFront, hs.muzzle[hand]));
        }
    }
}

void registerCommands()
{
    Cmd_AddCommand("vr_ad_widowmaker_spin_info", spinInfo_f);
}

bool isMade(const char* name)
{
    return name && ZA_STRSTR(name, suffix) && recipeOf(name);
}

byte* derivedFile(const char* name, unsigned int* path_id)
{
    for(const SidePart& part : sideParts)
    {
        if(!q_strcasecmp(name, part.name))
        {
            return sideFile(part, path_id);
        }
    }
    const Recipe* r = ZA_STRSTR(name, suffix) ? recipeOf(name) : nullptr;
    if(!r)
    {
        return nullptr;
    }
    // The Widowmaker: the hand-made model (in the same frame, nothing of AD's in it); made from AD's without its file.
    if(!q_strcasecmp(name, widowmakerName))
    {
        artLoaded = false;
        if(byte* art = COM_LoadMallocFile(widowmakerArtName, path_id))
        {
            Con_DPrintf("AD weapon model: %s is the hand-made %s\n", name, widowmakerArtName);
            artLoaded = true;
            return art;
        }
        Con_DPrintf("AD weapon model: %s: no %s, made from AD's\n", name, widowmakerArtName);
    }
    Built b;
    const char* why = "";
    if(!build(*r, b, why, path_id))
    {
        // (AD's data missing: by design, quietly; AD's model there but not made: a bug of ours, said aloud: the Plasma
        // Gun was drawn as the Thunderbolt when the lightning gun's art changed, 2026-11-08)
        if(ZA_STRCMP(why, missingSourceWhy) != 0)
        {
            Con_Warning("AD weapon model: %s not made (%s): %s stands in\n", name, why, r->standIn);
        }
        Con_DPrintf("AD weapon model: %s: %s: %s stands in\n", name, why, r->standIn);
        return COM_LoadMallocFile(r->standIn, path_id);
    }
    byte* out = static_cast<byte*>(VR_HeapMalloc(b.file.size() + 1));
    ZA_MEMCPY(out, b.file.data(), b.file.size());
    out[b.file.size()] = 0;
    com_filesize = static_cast<int>(b.file.size());
    Con_DPrintf("AD weapon model: %s made: %d triangles, %d vertices, %d holes closed\n", name, b.tris, b.verts, b.sealed);
    return out;
}

void info_f()
{
    Con_Printf("(the Widowmaker drawn: %s; below, the one made from AD's)\n", artLoaded ? widowmakerArtName : "made from AD's");
    int made = 0, notMade = 0, noData = 0;
    for(const Recipe& r : recipes)
    {
        Built b;
        const char* why = "";
        if(!build(r, b, why, nullptr))
        {
            Con_Printf("%s: %s\n", r.name, why);
            ++(ZA_STRCMP(why, missingSourceWhy) ? notMade : noData);
            continue;
        }
        ++made;
        // "vr_adweapon_models write": each made model also written to the game folder as made_<name>.mdl (for
        // Misc/quakevr/check_mdl_holes.py and a look at it; never shipped: made from the mods' own files)
        if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "write"))
        {
            char file[MAX_QPATH];
            q_snprintf(file, sizeof(file), "made_%s", COM_SkipPath(r.name));
            if(char* hash = strchr(file, '#'))
            {
                *hash = '\0';
            }
            COM_WriteFile(file, b.file.data(), static_cast<int>(b.file.size()));
            Con_Printf("(written: %s/%s)\n", com_gamedir, file);
        }
        Con_Printf("%s: %d triangles, %d vertices (%d of AD's off, %d of Quake VR's grip on, %d made), %d holes closed, %d open "
                   "edges left, box %.2f %.2f %.2f .. %.2f %.2f %.2f%s, muzzle %.4f %.4f %.4f\n",
            r.name, b.tris, b.verts, b.dropped, b.grafted, b.made, b.sealed, b.openAfter, b.lo.x, b.lo.y, b.lo.z, b.hi.x, b.hi.y, b.hi.z,
            b.cornerKept ? "" : " (its corner moved: past the settings')", b.muzzle.x, b.muzzle.y, b.muzzle.z);
        if(!r.graft.path)
        {
            Con_Printf("  blade edge %.2f %.2f %.2f .. %.2f %.2f %.2f\n", b.edgeLo.x, b.edgeLo.y, b.edgeLo.z, b.edgeHi.x, b.edgeHi.y, b.edgeHi.z);
        }
        for(int k = 0; k < 3; k++)
        {
            if(r.graft.path && r.graftSeeds[k] >= 0 && b.seedLo[k].x <= b.seedHi[k].x)
            {
                Con_Printf("  grafted piece of vertex %d%s: box %.2f %.2f %.2f .. %.2f %.2f %.2f, AD's surface straight over its "
                           "top %s\n",
                    r.graftSeeds[k], r.graftSeeds[k] == r.liftSeed || r.graftSeeds[k] == r.liftAlso ? " (lifted)" : "", b.seedLo[k].x, b.seedLo[k].y,
                    b.seedLo[k].z, b.seedHi[k].x, b.seedHi[k].y, b.seedHi[k].z,
                    b.adAbove[k] < 1e29f ? va("at z %.2f (the least gap %.2f)", b.adAbove[k], b.adGap[k]) : "none");
            }
        }
        if(b.housing)
        {
            Con_Printf("  ammo screen housing: glass %.4f %.4f %.4f, window %.4f x %.4f, %d of its outline's 4 corners on "
                       "the gun's face\n",
                b.housingGlass.x, b.housingGlass.y, b.housingGlass.z, b.housingWindow.x, b.housingWindow.y, b.housingOnFace);
        }
        if(b.noteName)
        {
            Con_Printf("  %s: box %.2f %.2f %.2f .. %.2f %.2f %.2f; its top along x:", b.noteName, b.noteLo.x, b.noteLo.y,
                b.noteLo.z, b.noteHi.x, b.noteHi.y, b.noteHi.z);
            for(const float z : b.noteTop)
            {
                Con_Printf(z > -1e29f ? " %.2f" : " -", z);
            }
            Con_Printf("\n");
        }
        if(b.spinVerts > 0)
        {
            Con_Printf("  spin frames: %d of %d vertices turned, %d frames after the %d still ones, axis y %.4f z %.4f\n",
                b.spinVerts, b.verts, spinPoses, frameCount, b.spinAxis.y, b.spinAxis.z);
        }
    }
    // (a test's verdict line: "0 not made" is right; AD's data missing is no fault)
    Con_Printf("vr_adweapon_models: %d made, %d not made, %d without AD's data\n", made, notMade, noData);
}

} // namespace qvr::adweapons
