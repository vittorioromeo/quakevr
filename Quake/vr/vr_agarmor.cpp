// vr_agarmor.cpp -- armour for the armed humanoids, P0: shells made from the monster's own mesh, drawn on its authored
// rig (vr_agarmor.hpp has the method; ROUND22.md, "Armour for armed humanoids, P0").
//
// The self-check (vr_agarmor_check) is vr_agarmor_check.cpp; what the two share is vr_agarmor_internal.hpp.

#include "vr_testkit.hpp"
#include "vr_agarmor.hpp"
#include "vr_agarmor_internal.hpp"

#include "vr_api.h"
#include "vr_armourdata.hpp"
#include "vr_avatar.hpp"
#include "vr_view.hpp"
#include "vr_backend.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_drawnpose.hpp"
#include "vr_engine.hpp"
#include "vr_held.hpp"
#include "vr_hitmodel.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_mdldequant.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rig2.hpp"
#include "vr_rigpose.hpp"
#include "vr_geom.hpp"

#include "vr_lines.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/String/String.hpp"
#include "vr_api_render.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stdlib.h>
#include <string.h>

namespace qvr::agarmor
{

using namespace detail;

namespace
{

using geom::pi;
constexpr int maxWorn = 128;  // armour entities a frame (up to three a body: P2's wear)
constexpr int maxNamed = 256; // vr_agarmor_set's entities
constexpr const char* armorSuffix = "#armor";
constexpr const char* rigSuffix = "#rig2";
constexpr const char* skinModelName = "progs/vragarmor.mdl";
// A humanoid's thigh's plates start this far down its bone at the latest (of its length; Wearer::hipFrom).
constexpr float plateHipFrom = 0.12f;
// Who wears it, and where the cuirass stops behind (model x; -1e9 nowhere): the enforcer's power pack is its own, so its
// cuirass is a breastplate to its back's surface (x -10: the pack from -10.5 to -23, up past its head; P2).
// The head proper (the helmet's fit; helmetOf): up to headTop of the head bone's length above its pivot and headSide to
// its sides. The grunt's, the enforcer's and the knight's heads are inside the default (crown 1.02; sides 0.6, the
// enforcer's collar under its guards to 0.75).
struct Wearer
{
    const char* model;
    float backCut;
    float headTop{1.06f}, headSide{0.75f};
    float collar{0.5f}; // (the guards fit the head where it weighs this much on the head bone, not the collar)
    // The helmet's rows up the head bone's length (the brow -1: vr_agarmor_brow_front; the nape -1: vr_agarmor_brow_back),
    // the top's chamfer, how much wider its brow is than its crown, and how far the back's corners may be cut (of its half
    // width).
    float brow{-1.f}, cheek{0.3f}, jaw{0.04f}, nape{-1.f}, chamfer{0.18f}, taper{0.03f}, backCut2{0.7f};
    // Its guards below the ledge as plates of their own, not fitted to the vertices round them (-1: fitted): half width, half
    // width where they turn to the back, and their front edge's place and distance from the middle of the face at the cheek
    // row and at the jaw row.
    float plateWide{-1.f}, plateBack{-1.f}, plateFront[2]{-1.f, -1.f}, plateOpen[2]{-1.f, -1.f};
    float yoke{1.2f}; // (the cuirass's collar row over its region's top: plateYoke)
    float shoulderRidge{-1.f}; // (an upper arm's plates out over the skin round it at most this far: -1 plateRidge)
    // (its upper arms' plates start no higher than this up their bones (-1e9: as their shells reach); and both round one axis, the
    // left's and the mirrored right's mean, so the two are each other's mirror image in the bind pose whatever their rest poses)
    float shoulderTop{-1e9f};
    bool shoulderAxis{false};
    // (its thighs' plates start this far down their bones at the latest, of their length: below 0 above the hip's pivot, where its
    // skin weighs on the pelvis; plateHipFrom)
    float hipFrom{plateHipFrom};
    // (its upper arms' plates closed over the top by a dome this high, of their top ring's radius: -1 open; platesOf)
    float shoulderCap{-1.f};
};
// The armed knight and death knight (vr_agknight, vr_agdknight: their swordless models on their authored rigs) wear it
// too: every piece found on their own rigs' bones and skin weights as the grunt's are; nothing on their backs to cut round.
// The death knight's head proper is narrower and his crown higher: his skull 0.51 of its length to each side and 1.11
// up, rigid on it the horns' stalks from his temples (0.55-0.62 out, up from 0.93; their roots left out with them) and
// under it his gorget (to 0.9 out, weighing 0.52-0.78 on his head, his face 0.93 and more): fitted round, they made his
// helmet a hat 18 wide over a skull 10.7 wide (his ask, 2026-11-01). And it still came down over his eyes (their sockets
// 0.78-0.88 up), and it was wider than his head from behind (his ask, 2026-11-02): his sides to 0.44 (the skull's; what's out
// to 0.47 stays inside the shell's thickness), the brim at 0.97 (its ledge 0.91, over the eyes), a short top's chamfer and hardly
// a flare (they pushed its front and sides out), the guards ending at his cheekbones (0.52 up: 0.51 out under them) and the
// back's corners cut further round his narrow nape. It read as a hat from the front (his note, 2026-11-02): his face's
// vertices are left out of the guards' fit, so nothing held them out and they sank into his cheeks; they are plates now, down each
// side of his face to 0.42, outside the head's real width (0.53 at its most, at 0.2 forward), their front edges following his face as it widens down (0.35
// out at 0.53 forward at the cheek row, 0.46 at 0.4 at the jaw row), so they sweep back to a point. Outside 11.7 deep, 11.1 wide (the crown 10.3, his head 10.65 at its widest).
constexpr Wearer wearers[] = {{"progs/vrgrunt.mdl", -1e9f}, {"progs/soldier.mdl", -1e9f}, {.model = "progs/vrenforcer.mdl", .backCut = -10.5f, .hipFrom = -0.5f},
    {.model = "progs/enforcer.mdl", .backCut = -10.5f, .hipFrom = -0.5f},
    // The knight's red shoulder pads stand up to 2.5 units off his upper arms' skin, weighing on them 0.2-0.4 (the rest on his
    // chest): his upper arms' plates go out over them (his note, 2026-11-06: "exposed knight shoulder").
    // Their tops still showed through the pauldrons' open top ring, looking down at him (his note, 2026-11-07: "red pad visible
    // from the front"): his pauldrons are closed over the top by a low dome (shoulderCap).
    {"progs/vrknight.mdl", -1e9f, 1.06f, 0.75f, 0.5f, -1.f, 0.3f, 0.04f, -1.f, 0.18f, 0.03f, 0.7f, -1.f, -1.f, {-1.f, -1.f}, {-1.f, -1.f}, 1.2f, 1.8f,
        -1e9f, false, plateHipFrom, 0.8f},
    // His face too hidden by the helmet and the collar (his note, 2026-11-06): the guards end higher (jaw 0.42 -> 0.48), their front
    // edges further apart (0.37/0.44 from the middle -> 0.40/0.48), the collar lower (0.5).
    // His shoulders "asymmetric and too large, the left way too big" (2026-11-06): his upper arms rest differently (the left straight
    // down, the right out), so each side's plates round its own bone's axis were tilted apart: one axis mirrored for both; and his
    // shoulder shells' plates reached 10.8 up the bone, a cape: 5.
    {"progs/vrhknight.mdl", -1e9f, 1.12f, 0.44f, 0.8f, 0.97f, 0.68f, 0.48f, 0.45f, 0.06f, 0.01f, 0.9f, 0.49f, 0.44f, {0.54f, 0.42f}, {0.40f, 0.48f}, 0.5f, -1.f,
        -5.f, true, -0.3f},
    // The player's own body, each build (vr_player_plates; vr_agarmor.hpp, "The player's plates"): the same plates, on its IK.
    {"progs/vrbody.mdl", -1e9f}, {"progs/vrbody_bare.mdl", -1e9f}, {"progs/vrbody_lean.mdl", -1e9f},
    {"progs/vrbody_lean_bare.mdl", -1e9f}, {"progs/vrbody_brawny.mdl", -1e9f}, {"progs/vrbody_brawny_bare.mdl", -1e9f},
    {"progs/vrbody_fem.mdl", -1e9f}, {"progs/vrbody_fem_bare.mdl", -1e9f}};

// The atlas (make_agarmor.py's REGIONS): texels of its 256 x 128 skins.
struct Block
{
    float x0, y0, x1, y1;
};
constexpr Block cuirassBlock{0.f, 0.f, 160.f, 128.f};
constexpr Block helmetBlock{160.f, 0.f, 256.f, 64.f}; // the helmet's walls (mirrored left and right)
constexpr Block helmCapBlock{160.f, 64.f, 192.f, 96.f};
constexpr Block liningBlock{192.f, 64.f, 224.f, 96.f};
constexpr Block innerBlock{160.f, 96.f, 208.f, 128.f};
constexpr Block rimBlock{208.f, 96.f, 256.f, 128.f};
constexpr Block jointBlock{224.f, 64.f, 256.f, 96.f}; // an elbow's or a knee's sleeve: leather bellows round it (u round, v down to the joint)

// A limb piece's bone (its region: the triangles weighing mostly on it), by piece.
constexpr const char* limbBone[NumPieces] = {nullptr, nullptr, "upperarm_l", "upperarm_r", "forearm_l", "forearm_r", "thigh_l", "thigh_r",
    "shin_l", "shin_r"};

// ---- The body: the .mdl's rest pose as drawn, welded ----

void normalsOf(const za::Vector<glm::vec3>& p, const za::Vector<int>& tris, za::Vector<glm::vec3>& n)
{
    n.clear();
    n.resize(p.size(), glm::vec3{0.f});
    for(int t = 0; t + 2 < count(tris); t += 3)
    {
        const int a = at(tris, t), b = at(tris, t + 1), c = at(tris, t + 2);
        const glm::vec3 f = glm::cross(at(p, b) - at(p, a), at(p, c) - at(p, a)); // (area-weighted)
        at(n, a) += f;
        at(n, b) += f;
        at(n, c) += f;
    }
    for(glm::vec3& x : n)
    {
        x = geom::safeNormalize(x, glm::vec3{0.f, 0.f, 1.f}, 1e-6f);
    }
}

bool buildBody(qmodel_t* src, const aliashdr_t* sh, const rig2::Rig& rig, Body& b)
{
    b = Body{};
    const int nm = sh->numverts;
    if(nm <= 0 || sh->numverts_vbo <= 0)
    {
        return false;
    }
    mdldequant::prepare(src);
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(sh) + sh->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(sh) + sh->indexes);
    b.mdlRest.resize(static_cast<za::SizeT>(nm), glm::vec3{0.f});
    za::Vector<uint8_t> filled(static_cast<za::SizeT>(nm), uint8_t{0});
    for(int v = 0; v < sh->numverts_vbo; v++)
    {
        const int mv = za::clamp(static_cast<int>(desc[v].vertindex), 0, nm - 1);
        if(!at(filled, mv))
        {
            float xyz[3];
            signed char nrm[3];
            mdldequant::restVertex(sh, v, xyz, nrm);
            at(b.mdlRest, mv) = geom::vec(xyz);
            at(filled, mv) = 1;
        }
    }
    b.weld.resize(static_cast<za::SizeT>(nm), 0);
    for(int mv = 0; mv < nm; mv++)
    {
        int found = -1;
        for(int k = 0; k < count(b.rep) && found < 0; k++)
        {
            found = glm::length(at(b.p, k) - at(b.mdlRest, mv)) < 1e-3f ? k : -1;
        }
        if(found < 0)
        {
            found = count(b.rep);
            b.rep.pushBack(mv);
            b.p.pushBack(at(b.mdlRest, mv));
            W w;
            const rig2::Weights* rw = mv < count(rig.weights) ? &at(rig.weights, mv) : nullptr;
            for(int i = 0; rw && i < rw->n; i++)
            {
                w.w[za::clamp(rw->bone[i], 0, rig2::maxBones - 1)] += rw->w[i];
            }
            if(!rw || rw->n == 0)
            {
                w.w[0] = 1.f;
            }
            b.w.pushBack(w);
        }
        at(b.weld, mv) = found;
    }
    double vol = 0.0;
    for(int i = 0; i + 2 < sh->numindexes; i += 3)
    {
        int t[3];
        for(int k = 0; k < 3; k++)
        {
            t[k] = at(b.weld, za::clamp(static_cast<int>(desc[idx[i + k]].vertindex), 0, nm - 1));
        }
        if(t[0] == t[1] || t[1] == t[2] || t[0] == t[2])
        {
            continue;
        }
        const glm::vec3 &pa = at(b.p, t[0]), &pb = at(b.p, t[1]), &pc = at(b.p, t[2]);
        if(glm::length(glm::cross(pb - pa, pc - pa)) < 1e-6f)
        {
            continue;
        }
        vol += static_cast<double>(glm::dot(pa, glm::cross(pb, pc)));
        for(int k = 0; k < 3; k++)
        {
            b.tris.pushBack(t[k]);
        }
    }
    b.nativeFlip = vol < 0.0;
    for(int i = 0; b.nativeFlip && i + 2 < count(b.tris); i += 3)
    {
        const int x = at(b.tris, i + 1);
        at(b.tris, i + 1) = at(b.tris, i + 2);
        at(b.tris, i + 2) = x;
    }
    normalsOf(b.p, b.tris, b.n);
    return !b.tris.empty();
}

// ---- The player's body: a skeletal model, not a .mdl on an authored rig: its own bones taken for one ----
constexpr int maxPlayerRigs = 8; // (the builds, bare and not)
rig2::Rig playerRigs[maxPlayerRigs];
int numPlayerRigs = 0;

[[nodiscard]] bool isPlayerBody(const char* name)
{
    return name && !strncmp(name, "progs/vrbody", 12) && !strchr(name, '#');
}

// The bones of `src` by the names frameOf looks for (the calves its "shin"s), in the model's own order: the skinning
// matrices' (avatar::skinWithHead). The rig's weights stay empty: buildSkeletalBody reads the mesh's.
[[nodiscard]] const rig2::Rig* playerRig(qmodel_t* src)
{
    for(int i = 0; i < numPlayerRigs; i++)
    {
        if(!strcmp(playerRigs[i].model.cStr(), src->name))
        {
            return &playerRigs[i];
        }
    }
    const auto* sh = src->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(src)) : nullptr;
    if(!sh || sh->poseverttype != aliashdr_t::PV_IQM || sh->numbones > rig2::maxBones || numPlayerRigs >= maxPlayerRigs)
    {
        return nullptr;
    }
    rig2::Rig& r = playerRigs[numPlayerRigs++];
    r.model = za::String{src->name};
    r.numBones = sh->numbones;
    const auto* infos = reinterpret_cast<const boneinfo_t*>(reinterpret_cast<const byte*>(sh) + sh->boneinfo);
    for(int b = 0; b < sh->numbones; b++)
    {
        const char* name = infos[b].name;
        q_strlcpy(r.bones[b].name, !strcmp(name, "calf_l") ? "shin_l" : !strcmp(name, "calf_r") ? "shin_r" : name, sizeof(r.bones[b].name));
        r.bones[b].parent = infos[b].parent;
        const float* m = infos[b].inverse.mat; // [R^T | -R^T p]
        const glm::vec3 t{m[3], m[7], m[11]};
        r.bones[b].pivot = {-(m[0] * t.x + m[4] * t.y + m[8] * t.z), -(m[1] * t.x + m[5] * t.y + m[9] * t.z),
            -(m[2] * t.x + m[6] * t.y + m[10] * t.z)};
        r.bones[b].end = r.bones[b].pivot + glm::vec3{0.f, 0.f, 1.f};
    }
    constexpr const char* links[][2] = {{"spine", "chest"}, {"chest", "neck"}, {"neck", "head"}, {"upperarm_l", "forearm_l"},
        {"upperarm_r", "forearm_r"}, {"forearm_l", "hand_l"}, {"forearm_r", "hand_r"}, {"thigh_l", "shin_l"}, {"thigh_r", "shin_r"},
        {"shin_l", "foot_l"}, {"shin_r", "foot_r"}};
    for(const auto& link : links)
    {
        const int a = rig2::boneByName(r, link[0]), b = rig2::boneByName(r, link[1]);
        if(a >= 0 && b >= 0)
        {
            r.bones[a].end = r.bones[b].pivot;
        }
    }
    // (the head's end: its crown, straight up from its pivot)
    const int head = rig2::boneByName(r, "head");
    const auto* verts = reinterpret_cast<const iqmvert_t*>(reinterpret_cast<const byte*>(sh) + sh->vertexes);
    float crown = head >= 0 ? r.bones[head].pivot.z : 0.f;
    for(int v = 0; head >= 0 && v < sh->numverts_vbo; v++)
    {
        for(int k = 0; k < 4; k++)
        {
            crown = verts[v].idx[k] == head && verts[v].weight[k] > 127 ? za::max(crown, verts[v].xyz[2]) : crown;
        }
    }
    if(head >= 0)
    {
        r.bones[head].end = {r.bones[head].pivot.x, r.bones[head].pivot.y, crown};
    }
    return &r;
}

[[nodiscard]] const rig2::Rig* rigOf(qmodel_t* src)
{
    return isPlayerBody(src->name) ? playerRig(src) : rig2::rigFor(src);
}

// A skeletal body's rest pose as buildBody makes a .mdl's: welded, wound outward, its weights the mesh's own.
bool buildSkeletalBody(const aliashdr_t* sh, const rig2::Rig& rig, Body& b)
{
    b = Body{};
    const int nm = sh->numverts_vbo;
    if(nm <= 0 || sh->numindexes <= 0)
    {
        return false;
    }
    const auto* verts = reinterpret_cast<const iqmvert_t*>(reinterpret_cast<const byte*>(sh) + sh->vertexes);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(sh) + sh->indexes);
    b.mdlRest.resize(static_cast<za::SizeT>(nm), glm::vec3{0.f});
    b.weld.resize(static_cast<za::SizeT>(nm), 0);
    for(int mv = 0; mv < nm; mv++)
    {
        const glm::vec3 p{verts[mv].xyz[0], verts[mv].xyz[1], verts[mv].xyz[2]};
        at(b.mdlRest, mv) = p;
        int found = -1;
        for(int k = 0; k < count(b.rep) && found < 0; k++)
        {
            found = glm::length(at(b.p, k) - p) < 1e-3f ? k : -1;
        }
        if(found < 0)
        {
            found = count(b.rep);
            b.rep.pushBack(mv);
            b.p.pushBack(p);
            W w;
            float sum = 0.f;
            for(int i = 0; i < 4; i++)
            {
                const float x = static_cast<float>(verts[mv].weight[i]) / 255.f;
                w.w[za::clamp(static_cast<int>(verts[mv].idx[i]), 0, rig2::maxBones - 1)] += x;
                sum += x;
            }
            if(sum <= 0.f)
            {
                w.w[0] = 1.f;
            }
            b.w.pushBack(w);
        }
        at(b.weld, mv) = found;
    }
    // Which way it is wound: each triangle against the way out from its bone. (Not the volume round the origin as buildBody's:
    // this mesh is open at its seams, and that came out the wrong way, the plates inside the body.)
    double outward = 0.0;
    for(int i = 0; i + 2 < sh->numindexes; i += 3)
    {
        const int t[3] = {at(b.weld, za::clamp(static_cast<int>(idx[i]), 0, nm - 1)),
            at(b.weld, za::clamp(static_cast<int>(idx[i + 1]), 0, nm - 1)), at(b.weld, za::clamp(static_cast<int>(idx[i + 2]), 0, nm - 1))};
        if(t[0] == t[1] || t[1] == t[2] || t[0] == t[2])
        {
            continue;
        }
        const glm::vec3 &pa = at(b.p, t[0]), &pb = at(b.p, t[1]), &pc = at(b.p, t[2]);
        const glm::vec3 area = glm::cross(pb - pa, pc - pa);
        if(glm::length(area) < 1e-6f)
        {
            continue;
        }
        const rig2::Bone& bone = rig.bones[za::clamp(heaviest(at(b.w, t[0])), 0, rig.numBones - 1)];
        const glm::vec3 axis = bone.end - bone.pivot, mid = (pa + pb + pc) / 3.f;
        const float along = za::clamp(glm::dot(mid - bone.pivot, axis) / za::max(glm::dot(axis, axis), 1e-6f), 0.f, 1.f);
        outward += static_cast<double>(glm::dot(area, mid - (bone.pivot + axis * along)));
        for(int k : t)
        {
            b.tris.pushBack(k);
        }
    }
    b.nativeFlip = outward < 0.0;
    for(int i = 0; b.nativeFlip && i + 2 < count(b.tris); i += 3)
    {
        const int x = at(b.tris, i + 1);
        at(b.tris, i + 1) = at(b.tris, i + 2);
        at(b.tris, i + 2) = x;
    }
    normalsOf(b.p, b.tris, b.n);
    return !b.tris.empty();
}

// ---- A piece: where it is on the body ----
struct PieceFrame
{
    uint32_t bones{0};            // the region's bones
    glm::vec3 origin{0.f}, up{0.f, 0.f, 1.f}, fwd{1.f, 0.f, 0.f}, left{0.f, 1.f, 0.f};
    float length{1.f};
    float backCut{-1e9f}; // (the cuirass: nothing behind this model x; Wearer::backCut)
    float yoke{-1.f};     // (the cuirass: its collar row's height, Wearer::yoke; -1 plateYoke)
    float topAt{1e9f};    // (a humanoid's upper arm: its plates start this high at the latest, its other side's start; planGrid)
    float topMost{-1e9f}; // (and no higher than this: Wearer::shoulderTop)
    float cut{0.5f};      // (its region: the vertices weighing more than this on its bones; regionCut)
    const za::Vector<uint8_t>* over{nullptr}; // (a pauldron: the body's vertices in its region besides; girdleShells)
    bool ok{false};
    bool ends{false}; // (a humanoid's limb, not the player's: a forearm's and a shin's plates stop short of the wrist and the ankle)
    float ridge{-1.f}; // (fillRadii: -1 plateRidge; plateRidgeLimb on a humanoid's forearms and legs)
    bool hip{false};   // (a humanoid's thigh: its rings above its region on the whole skin; fillRadii)
    float hipFrom{plateHipFrom}; // (a humanoid's thigh: its rows start this far down its bone at the latest, of its length; Wearer::hipFrom)
    bool upper{false}; // (an upper arm or a thigh: its plates' far end at the joint)
    float cap{-1.f};   // (a knight's upper arm: its top ring closed by a dome this high, of the ring's radius; Wearer::shoulderCap)
    const Body* body{nullptr}; // (a limb: the whole skin, for a ring its region doesn't reach: fillRadii)
    // (armour.txt's tweaks, planGrid: the first and last rows this many units further along the piece, its rows; 0 the generator's)
    float shiftStart{0.f}, shiftEnd{0.f};
    int rows{0};
    bool hero{false}; // (its hero plates: vr_agarmor_hero and armour.txt's `hero`; heroRuns)
};

[[nodiscard]] float backCutOf(const char* model, int piece)
{
    for(const Wearer& w : wearers)
    {
        if(piece == Cuirass && model && !strcmp(model, w.model))
        {
            return w.backCut;
        }
    }
    return -1e9f;
}

[[nodiscard]] Wearer wearerOf(const char* model)
{
    for(const Wearer& w : wearers)
    {
        if(model && !strcmp(model, w.model))
        {
            return w;
        }
    }
    return Wearer{model, -1e9f};
}

[[nodiscard]] PieceFrame frameOf(const rig2::Rig& rig, int piece)
{
    PieceFrame f;
    const auto bit = [&](const char* name) {
        const int b = rig2::boneByName(rig, name);
        f.ok = f.ok || b >= 0;
        return b >= 0 ? 1u << static_cast<unsigned>(b) : 0u;
    };
    if(piece == Cuirass)
    {
        f.bones = bit("spine") | bit("chest") | (rig2::boneByName(rig, "clavicle_l") >= 0 ? bit("clavicle_l") : 0u) | (rig2::boneByName(rig, "clavicle_r") >= 0 ? bit("clavicle_r") : 0u);
        const int s = rig2::boneByName(rig, "spine"), c = rig2::boneByName(rig, "chest");
        f.ok = s >= 0 && c >= 0;
        if(f.ok)
        {
            f.origin = rig.bones[s].pivot;
            f.up = geom::safeNormalize(rig.bones[c].end - rig.bones[s].pivot, glm::vec3{0.f, 0.f, 1.f}, 1e-6f);
            f.length = glm::length(rig.bones[c].end - rig.bones[s].pivot);
        }
    }
    else if(limbBone[piece])
    {
        // A limb: round its bone, up it from its pivot (the texture's rows run down it).
        f.bones = bit(limbBone[piece]);
        // (the player's forearm hangs on its twist joints. The bracer on the elbow's half alone: its wrist's half is the wrist
        // gadget's and its straps', and the hand and what it holds stay clear)
        if(piece == BracerL || piece == BracerR)
        {
            const char* twist = piece == BracerL ? "foretwist1_l" : "foretwist1_r";
            f.bones |= rig2::boneByName(rig, twist) >= 0 ? bit(twist) : 0u;
        }
        const int b = rig2::boneByName(rig, limbBone[piece]);
        f.ok = b >= 0;
        if(f.ok)
        {
            const glm::vec3 d = rig.bones[b].end - rig.bones[b].pivot;
            f.up = geom::safeNormalize(d, glm::vec3{0.f, 0.f, 1.f}, 1e-6f);
            f.length = za::max(glm::length(d), 1.f);
            f.origin = rig.bones[b].pivot;
        }
    }
    else
    {
        f.bones = bit("head");
        const int h = rig2::boneByName(rig, "head");
        f.ok = h >= 0;
        if(f.ok)
        {
            const glm::vec3 d = rig.bones[h].end - rig.bones[h].pivot;
            f.up = geom::safeNormalize(d, glm::vec3{0.f, 0.f, 1.f}, 1e-6f);
            f.length = za::max(glm::length(d), 1.f);
            f.origin = rig.bones[h].pivot;
        }
    }
    f.fwd = geom::safeNormalize(glm::vec3{1.f, 0.f, 0.f} - f.up * f.up.x, glm::vec3{1.f, 0.f, 0.f}, 1e-6f);
    f.left = glm::cross(f.up, f.fwd);
    return f;
}

// The piece's field at a body vertex: >= 0 covered (its weight on the region's bones past 0.5).
[[nodiscard]] float fieldOf(const Body& b, int v, const PieceFrame& f)
{
    const W& w = at(b.w, v);
    float region = 0.f;
    for(int i = 0; i < rig2::maxBones; i++)
    {
        region += ((f.bones >> static_cast<unsigned>(i)) & 1u) ? w.w[i] : 0.f;
    }
    region = f.over && at(*f.over, v) ? 1.f : region;
    // (behind the back's cut: out, linear in x so the crossing lands on the cut)
    const float field = za::min(region - f.cut, (at(b.p, v).x - f.backCut) * 0.25f);
    return za::abs(field) < 1e-4f ? 1e-4f : field;
}

// The weight a region's vertices weigh past: half, or for a bone no vertex weighs half on (the armed knight's right
// upper arm, 0.38 at most: his rest pose's raised sword arm shared with his chest and forearm) 0.8 of its most, so its
// plate covers where it weighs most rather than nothing.
[[nodiscard]] float regionCut(const za::Vector<W>& w, uint32_t bones)
{
    float most = 0.f;
    for(const W& x : w)
    {
        float region = 0.f;
        for(int i = 0; i < rig2::maxBones; i++)
        {
            region += ((bones >> static_cast<unsigned>(i)) & 1u) ? x.w[i] : 0.f;
        }
        most = za::max(most, region);
    }
    return most >= 0.55f ? 0.5f : 0.8f * most;
}

// A pauldron covers the mesh's own shoulder shells as well as the upper arm's skin: each part of the body (its own
// triangles, joined to no other) that rides the upper arm's parent bone alone (every vertex wholly on it; the death
// knight's pauldrons and their spikes on his clavicles) and sits at the shoulder (its middle nearer the upper arm's
// pivot than its parent's). Those vertices go into the plate's region; the plate there keeps their weights, so it
// rides the bone the shell does (the girdle, not the arm). None on a mesh whose shoulders are its skin (the grunt's,
// the enforcer's, the knight's: their plates as before).
void girdleShells(const Body& b, const rig2::Rig& rig, int limb, za::Vector<uint8_t>& over)
{
    const int nv = count(b.p);
    over.clear();
    over.resize(static_cast<za::SizeT>(nv), uint8_t{0});
    const int girdle = limb >= 0 && limb < rig.numBones ? rig.bones[limb].parent : -1;
    if(girdle < 0 || girdle >= rig2::maxBones)
    {
        return;
    }
    // (the parts: union-find over the triangles' edges)
    za::Vector<int> up(static_cast<za::SizeT>(nv), 0);
    for(int v = 0; v < nv; v++)
    {
        at(up, v) = v;
    }
    const auto root = [&](int v) {
        while(at(up, v) != v)
        {
            at(up, v) = at(up, at(up, v));
            v = at(up, v);
        }
        return v;
    };
    for(int t = 0; t + 2 < count(b.tris); t += 3)
    {
        for(int k = 1; k < 3; k++)
        {
            const int r0 = root(at(b.tris, t)), r1 = root(at(b.tris, t + k));
            at(up, za::max(r0, r1)) = za::min(r0, r1);
        }
    }
    // (per part: its vertices, how many ride the girdle wholly, its middle)
    za::Vector<int> size(static_cast<za::SizeT>(nv), 0), rigid(static_cast<za::SizeT>(nv), 0);
    za::Vector<glm::vec3> mid(static_cast<za::SizeT>(nv), glm::vec3{0.f});
    for(int v = 0; v < nv; v++)
    {
        const int r = root(v);
        at(size, r)++;
        at(rigid, r) += at(b.w, v).w[girdle] >= 0.99f ? 1 : 0;
        at(mid, r) += at(b.p, v);
    }
    const glm::vec3 shoulder = rig.bones[limb].pivot, inner = rig.bones[girdle].pivot;
    for(int v = 0; v < nv; v++)
    {
        const int r = root(v);
        const glm::vec3 m = at(mid, r) / static_cast<float>(za::max(at(size, r), 1));
        at(over, v) = at(size, r) > 0 && at(rigid, r) == at(size, r) && glm::distance(m, shoulder) < glm::distance(m, inner) ? 1 : 0;
    }
}

// ---- The shell ----

// A piece's surface as made (before the shell): its vertices on the body (orig), smoothed (p), their weights.
struct Local
{
    za::Vector<glm::vec3> p, orig;
    za::Vector<W> w;
    za::Vector<int> tris;
};

void clipPiece(const Body& b, const PieceFrame& f, Local& L)
{
    L = Local{};
    const int nv = count(b.p);
    za::Vector<float> field(static_cast<za::SizeT>(nv), 0.f);
    for(int v = 0; v < nv; v++)
    {
        at(field, v) = fieldOf(b, v, f);
    }
    za::Vector<int> local(static_cast<za::SizeT>(nv), -1);
    EdgeMap crossings;
    crossings.init(count(b.tris));
    const auto vertOf = [&](int v) {
        if(at(local, v) < 0)
        {
            at(local, v) = count(L.p);
            L.p.pushBack(at(b.p, v));
            L.orig.pushBack(at(b.p, v));
            L.w.pushBack(at(b.w, v));
        }
        return at(local, v);
    };
    const auto crossingOf = [&](int i, int j) {
        bool fresh = false;
        int& slot = crossings.get(i, j, fresh);
        if(fresh)
        {
            const int a = za::min(i, j), c = za::max(i, j);
            const float t = at(field, a) / (at(field, a) - at(field, c));
            slot = count(L.p);
            const glm::vec3 p = at(b.p, a) + (at(b.p, c) - at(b.p, a)) * t;
            L.p.pushBack(p);
            L.orig.pushBack(p);
            L.w.pushBack(mixW(at(b.w, a), at(b.w, c), t));
        }
        return slot;
    };
    for(int t = 0; t + 2 < count(b.tris); t += 3)
    {
        int poly[4];
        int np = 0;
        for(int k = 0; k < 3; k++)
        {
            const int i = at(b.tris, t + k), j = at(b.tris, t + (k + 1) % 3);
            const bool in = at(field, i) >= 0.f, jin = at(field, j) >= 0.f;
            if(in)
            {
                poly[np++] = vertOf(i);
            }
            if(in != jin)
            {
                poly[np++] = crossingOf(i, j);
            }
        }
        for(int k = 1; k + 1 < np; k++)
        {
            L.tris.pushBack(poly[0]);
            L.tris.pushBack(poly[k]);
            L.tris.pushBack(poly[k + 1]);
        }
    }
    {
        // (a surface whose every edge is one or two triangles': the knights' meshes have edges three triangles share
        // (the death knight's skirt over his body, the knight's tabard) and triangles over others, which left the shells'
        // layers and rims not closed; a triangle that would make an edge's third, or that repeats a vertex, is dropped)
        za::Vector<int> kept;
        EdgeMap uses;
        uses.init(count(L.tris));
        for(int t = 0; t + 2 < count(L.tris); t += 3)
        {
            const int a = at(L.tris, t), b = at(L.tris, t + 1), c = at(L.tris, t + 2);
            if(a == b || b == c || c == a)
            {
                continue;
            }
            bool fresh[3] = {false, false, false};
            int* n[3] = {&uses.get(a, b, fresh[0]), &uses.get(b, c, fresh[1]), &uses.get(c, a, fresh[2])};
            bool full = false;
            for(int k = 0; k < 3; k++)
            {
                *n[k] = fresh[k] ? 0 : *n[k];
                full = full || *n[k] >= 2;
            }
            if(full)
            {
                continue;
            }
            for(int k = 0; k < 3; k++)
            {
                (*n[k])++;
            }
            kept.pushBack(a);
            kept.pushBack(b);
            kept.pushBack(c);
        }
        L.tris = ZA_MOVE(kept);
    }
    // Only its big parts (a stray patch of weight elsewhere dropped): components by shared vertices.
    const int nt = count(L.tris) / 3, lv = count(L.p);
    za::Vector<int> parent(static_cast<za::SizeT>(lv), 0);
    for(int v = 0; v < lv; v++)
    {
        at(parent, v) = v;
    }
    const auto find = [&](int v) {
        while(at(parent, v) != v)
        {
            at(parent, v) = at(parent, at(parent, v));
            v = at(parent, v);
        }
        return v;
    };
    for(int t = 0; t < nt; t++)
    {
        for(int k = 1; k < 3; k++)
        {
            const int a = find(at(L.tris, 3 * t)), c = find(at(L.tris, 3 * t + k));
            at(parent, za::max(a, c)) = za::min(a, c);
        }
    }
    za::Vector<int> size(static_cast<za::SizeT>(lv), 0);
    for(int t = 0; t < nt; t++)
    {
        at(size, find(at(L.tris, 3 * t)))++;
    }
    int biggest = 0;
    for(int v = 0; v < lv; v++)
    {
        biggest = za::max(biggest, at(size, v));
    }
    Local kept;
    za::Vector<int> remap(static_cast<za::SizeT>(lv), -1);
    for(int t = 0; t < nt; t++)
    {
        if(at(size, find(at(L.tris, 3 * t))) * 5 < biggest)
        {
            continue;
        }
        for(int k = 0; k < 3; k++)
        {
            const int v = at(L.tris, 3 * t + k);
            if(at(remap, v) < 0)
            {
                at(remap, v) = count(kept.p);
                kept.p.pushBack(at(L.p, v));
                kept.orig.pushBack(at(L.orig, v));
                kept.w.pushBack(at(L.w, v));
            }
            kept.tris.pushBack(at(remap, v));
        }
    }
    L = ZA_MOVE(kept);
}

void subdivide(Local& L)
{
    EdgeMap mids;
    mids.init(count(L.tris));
    const auto midOf = [&](int a, int b) {
        bool fresh = false;
        int& slot = mids.get(a, b, fresh);
        if(fresh)
        {
            slot = count(L.p);
            const glm::vec3 p = (at(L.p, a) + at(L.p, b)) * 0.5f, o = (at(L.orig, a) + at(L.orig, b)) * 0.5f;
            const W w = mixW(at(L.w, a), at(L.w, b), 0.5f);
            L.p.pushBack(p);
            L.orig.pushBack(o);
            L.w.pushBack(w);
        }
        return slot;
    };
    za::Vector<int> out;
    for(int t = 0; t + 2 < count(L.tris); t += 3)
    {
        const int a = at(L.tris, t), b = at(L.tris, t + 1), c = at(L.tris, t + 2);
        const int ab = midOf(a, b), bc = midOf(b, c), ca = midOf(c, a);
        const int quad[12] = {a, ab, ca, ab, b, bc, ca, bc, c, ab, bc, ca};
        for(int k : quad)
        {
            out.pushBack(k);
        }
    }
    L.tris = ZA_MOVE(out);
}

// Its edges: each vertex's neighbours, and along its open edge only for the open edge's vertices.
void neighbours(const Local& L, za::Vector<za::Vector<int>>& nb, za::Vector<uint8_t>& open, za::Vector<int>& openEdges)
{
    const int nv = count(L.p);
    nb.clear();
    nb.resize(static_cast<za::SizeT>(nv));
    open.clear();
    open.resize(static_cast<za::SizeT>(nv), uint8_t{0});
    openEdges.clear();
    EdgeMap uses;
    uses.init(count(L.tris));
    for(int t = 0; t + 2 < count(L.tris); t += 3)
    {
        for(int k = 0; k < 3; k++)
        {
            bool fresh = false;
            int& n = uses.get(at(L.tris, t + k), at(L.tris, t + (k + 1) % 3), fresh);
            n = fresh ? 1 : n + 1;
        }
    }
    for(int t = 0; t + 2 < count(L.tris); t += 3)
    {
        for(int k = 0; k < 3; k++)
        {
            const int a = at(L.tris, t + k), b = at(L.tris, t + (k + 1) % 3);
            bool fresh = false;
            const int n = uses.get(a, b, fresh);
            if(n == 1)
            {
                at(open, a) = at(open, b) = 1;
                openEdges.pushBack(a); // (in its triangle's winding: the rim faces out of it)
                openEdges.pushBack(b);
            }
        }
    }
    for(int t = 0; t + 2 < count(L.tris); t += 3)
    {
        for(int k = 0; k < 3; k++)
        {
            const int a = at(L.tris, t + k), b = at(L.tris, t + (k + 1) % 3);
            bool fresh = false;
            const int n = uses.get(a, b, fresh);
            const bool edgeOpen = n == 1;
            // An open vertex is smoothed along the open edge only; an inner one by all.
            for(int s = 0; s < 2; s++)
            {
                const int x = s ? b : a, y = s ? a : b;
                if(at(open, x) && !edgeOpen)
                {
                    continue;
                }
                bool have = false;
                for(int z : at(nb, x))
                {
                    have = have || z == y;
                }
                if(!have)
                {
                    at(nb, x).pushBack(y);
                }
            }
        }
    }
}

void smooth(Local& L, int iterations)
{
    za::Vector<za::Vector<int>> nb;
    za::Vector<uint8_t> open;
    za::Vector<int> openEdges;
    neighbours(L, nb, open, openEdges);
    const int nv = count(L.p);
    za::Vector<glm::vec3> d(static_cast<za::SizeT>(nv), glm::vec3{0.f});
    for(int it = 0; it < iterations * 2; it++)
    {
        const float k = (it & 1) ? -0.53f : 0.5f; // Taubin: shrink, then grow back
        for(int v = 0; v < nv; v++)
        {
            glm::vec3 sum{0.f};
            for(int u : at(nb, v))
            {
                sum += at(L.p, u);
            }
            at(d, v) = at(nb, v).empty() ? glm::vec3{0.f} : sum / static_cast<float>(at(nb, v).size()) - at(L.p, v);
        }
        for(int v = 0; v < nv; v++)
        {
            at(L.p, v) += at(d, v) * k;
        }
    }
}

[[nodiscard]] glm::vec2 toBlock(const Block& b, float a, float c)
{
    return glm::vec2{b.x0 + 0.5f + za::clamp(a, 0.f, 1.f) * (b.x1 - b.x0 - 1.f), b.y0 + 0.5f + za::clamp(c, 0.f, 1.f) * (b.y1 - b.y0 - 1.f)};
}

void addTri(Shell& s, int a, int b, int c)
{
    s.tris.pushBack(a);
    s.tris.pushBack(b);
    s.tris.pushBack(c);
}

// ---- The plates (ROUND24.md, "The armour's look") ----
// A piece is an organised set of lames: straight, short rows round its axis, in columns mirrored left and right about the body's
// centre line (a seam down the middle of the chest), each row's cross-section the body's own outline at that height, a small even gap
// off the skin. Each plate is a shingle: its top lies on the skin under the row above, its lower edge stands proud and laps over
// the row below, so every row shows a real step and its shadow. The feet share the grid's corners, weights included, so nothing shows
// between plates in any pose. 12 triangles a plate (no top edge to bevel: it's under the lip): the detail is in the paint and relief.
constexpr int plateTilesX = 5, plateTilesY = 6; // (make_agarmor.py's TILES_X, TILES_Y)
constexpr float plateBevelUv = 0.12f;           // (its BEVEL)
// The tiles' kinds (make_agarmor.py's TILE_*; ROUND24.md, "Armour look: fewer, bigger plates"): the plain ones by chance, the
// details by place (vr_agarmor_details): the chest's recessed panel either side of the seam, ribs down the belly, vents on the
// forearms' cuffs and the shins, a cop on each elbow and knee (raised), the emblem on an upper arm's top plate (vr_agarmor_emblem).
constexpr int tilePanel = 20, tilePanels = 3, tileVent = 23, tileVents = 3, tileRib = 26, tileRibs = 2, tileEmblem = 28, tileCop = 29;
// The plain tiles (rows 2 and 3 of the atlas hold the hero plates' wide tiles besides; 0-19 all plain till 2026-11-08).
constexpr int plainTiles[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 19};
constexpr int tilePlains = static_cast<int>(sizeof(plainTiles) / sizeof(plainTiles[0]));
// The hero plates' tiles (ROUND24.md, "Armour look: hero plates"; make_agarmor.py's TILE_WIDE_*): heroSpan atlas tiles wide each,
// named by their first: the breastplate (its recessed panel in the middle), a plain one (a pauldron's), the emblem's, a cop's.
constexpr int heroSpan = 2, tileWideBreast = 10, tileWidePlain = 12, tileWideEmblem = 15, tileWideCop = 17;
constexpr float heroChestFront = 0.6f; // the breastplate: the chest row's plates facing at least this far forward (their way out's x)
constexpr int heroLimbRun = 3;         // a cop's columns, round the elbow's or the knee's point; a pauldron's top row: two runs, half each
constexpr float copRaise = 1.5f; // a cop's step: this many times a plate's
constexpr float plateFit = 0.12f;               // units off the skin: the rows' feet
constexpr float plateStep = 0.62f;              // how far a plate's lower edge stands proud of its foot: the step to the row below
constexpr float plateSeam = 0.1f;               // units each side of a seam between neighbours in a row, at the lower edge
constexpr float plateFootApart = 0.003f;        // the feet's corners off the grid's: each slab closed by itself, nothing to see between
constexpr float plateLap = 0.34f;               // how far a plate's lower edge reaches past its row, over the row below
constexpr float plateBevelWide = 0.3f, plateBevelHigh = 0.16f;
constexpr float plateHighTorso = 3.4f, plateHighLimb = 4.2f, plateHighLeg = 3.6f; // wanted rows' heights (units; 2.5 and 2.9 (legs too) till 2026-11-08: fewer, bigger plates, his mockup)
constexpr float plateJointReach = 0.25f;        // a limb's plates past its region's ends, over the joint (units)
constexpr float plateRidge = 1.1f;              // a corner raised over the skin's vertices at most this much over its rays' radius
constexpr float plateShoulderReach = 1.2f;      // an upper arm's plates past its region's top, over the shoulder's ball
constexpr float plateYoke = 1.2f;               // the cuirass's first row's height over its region's top
constexpr float plateWaist = 1.2f;              // the cuirass past its region's lower edge
// The joints (elbows, knees) bare of plates, so the limb bends without the plates going through it (his notes, 2026-11-05: a plate
// floating at the elbow, the forearm coming out through the upper arm's plates): the upper arm's and the thigh's plates end this far
// short of the joint, the forearm's and the shin's start this far past it; a sleeve of leather bellows on the skin between (sleeveOf).
constexpr float plateElbowAbove = 1.6f, plateKneeAbove = 2.0f, plateElbowBelow = 0.9f, plateKneeBelow = 1.2f; // units
// A humanoid's forearm's plates stop this far short of the wrist, its shin's of the ankle (its bones' ends); its thigh's start: plateHipFrom.
constexpr float plateWristAbove = 0.9f, plateAnkleAbove = 0.6f;
// How far a humanoid's limb's corner may stand over its rays' radius for the skin's vertices between (plateRidge: the cuirass's and the
// player's): their low-poly limbs' corners made the plates wide and bulky (his notes, 2026-11-06).
constexpr float plateRidgeLimb = 0.5f, plateRidgeThigh = 0.8f; // (a thigh's more: the low-poly knee's point)
constexpr float sleeveFit = 0.22f, sleeveFitLower = 0.34f; // the sleeves' units off the skin (the lower part's over the upper's where they meet)
constexpr int sleeveRings = 4;       // rings down a sleeve
constexpr float sleeveRidge = 0.8f;  // units a sleeve's corner may go out for the skin's vertices between its rings
constexpr float sleeveRidgeHumanoid = 1.6f; // (a humanoid's: over the knee's and the elbow's points, which its limb's slimmer end rows no longer cover; eased round)
constexpr float sleevePast = 0.3f;   // units a sleeve may go past its joint, under the next part's (its region past the joint; 0.8 till
                                     // 2026-11-06: the two halves overlapped, his note "leather seam overlap")
constexpr float sleeveRoundLeast = 0.85f, sleeveRoundMost = 1.25f, sleeveRoundIn = 0.3f; // a humanoid's sleeve's corners within these of its ring's mean
constexpr float sleeveUnder = 0.45f; // how far up under the end row of plates a sleeve starts (of that row's height)
constexpr float capBulge = 1.18f;    // a knight's pauldron's dome (Wearer::shoulderCap): its middle ring this much wider than its rim
// The body's vertices weighing on a piece's bones less than its region's cut but at least this much (a deltoid between the
// cuirass and the upper arm, a buttock over the thigh) push its plates out over them too, if a plate needn't swell past
// plateRidge for them (else they're left to show: a little overlap rather than a bulky plate).
constexpr float plateFringe = 0.2f;
constexpr int plateMostRowsTorso = 5, plateMostRowsLong = 4, plateMostRowsArm = 2; // (8 and 6 columns: within the budget; 6, 5 and 3 till 2026-11-08)

[[nodiscard]] float plateRand(uint32_t seed, int a, int b, int c)
{
    uint32_t h = seed;
    for(const int x : {a, b, c})
    {
        h = (h ^ static_cast<uint32_t>(x)) * 16777619u;
        h ^= h >> 13u;
    }
    h *= 0x9E3779B1u;
    return static_cast<float>((h >> 12u) & 0xfffffu) / 1048575.f;
}

[[nodiscard]] uint32_t plateSeed(const char* model)
{
    uint32_t h = 2166136261u ^ static_cast<uint32_t>(static_cast<int>(vr_agarmor_seed.value));
    for(const char* c = model; c && *c; c++)
    {
        h = (h ^ static_cast<uint32_t>(*c)) * 16777619u;
    }
    return h;
}

[[nodiscard]] bool bodyExit(const Body& b, const glm::vec3& o, const glm::vec3& d, float most, float& t, W& w);

constexpr float plateJointSunk = 0.7f;     // a joint row's corner under this share of its mirror image's: sunk (mirrorRadii)
constexpr int planMostRings = 16;          // (a limb's rings: 10 rows at most)
constexpr float planAxisJump = 0.3f;       // a forearm's or shin's ring middle this far (of the limb's girth there) off the others'...
constexpr float planAxisJumpLeast = 0.5f;  // ... or this far (units), whichever is more: a jump, not the limb's own line

// A humanoid's forearm's and shin's rings' middles smooth along the limb: a ring whose middle jumps from the others' (their
// median) is put back on the line through its good neighbours, the elbow's ring (0) extrapolated from rings 1 and 2. The whole
// skin's slice near an elbow bent at the bind pose (the enforcer's gun arm) cut into the upper arm and the bicep: his right
// forearm's first ring sat 3.7 units off his arm, its radii 1.45 to 9.12, and the elbow leather's end ring there crossed itself
// (holes with the arm straight); the grunt's left forearm's middle ring 2.6 off, one corner of its first row sunk (his notes,
// 2026-11-09; ROUND24.md, "Elbow leather at rest").
void smoothAxis(PlateGrid& g, const float* reachOf)
{
    const int n = za::min(g.rows + 1, planMostRings);
    if(n < 3)
    {
        return;
    }
    float xs[planMostRings], ys[planMostRings];
    for(int i = 0; i < n; i++)
    {
        xs[i] = at(g.axis, i).x;
        ys[i] = at(g.axis, i).y;
    }
    for(int i = 1; i < n; i++) // (a few: insertion)
    {
        for(int k = i; k > 0 && xs[k - 1] > xs[k]; k--)
        {
            const float t = xs[k];
            xs[k] = xs[k - 1];
            xs[k - 1] = t;
        }
        for(int k = i; k > 0 && ys[k - 1] > ys[k]; k--)
        {
            const float t = ys[k];
            ys[k] = ys[k - 1];
            ys[k - 1] = t;
        }
    }
    const glm::vec2 ref = n % 2 ? glm::vec2{xs[n / 2], ys[n / 2]} : glm::vec2{xs[n / 2 - 1] + xs[n / 2], ys[n / 2 - 1] + ys[n / 2]} * 0.5f;
    float girth = 0.f;
    int girths = 0;
    for(int i = 0; i < n; i++)
    {
        girth += reachOf[i];
        girths += reachOf[i] > 0.f ? 1 : 0;
    }
    girth = girths ? girth / static_cast<float>(girths) : 0.f;
    const auto jumpAt = [&](int i) { return za::max(planAxisJumpLeast, planAxisJump * (reachOf[i] > 0.f ? reachOf[i] : girth)); };
    bool good[planMostRings];
    int goods = 0;
    for(int i = 0; i < n; i++)
    {
        good[i] = glm::distance(at(g.axis, i), ref) <= jumpAt(i);
    }
    // (the elbow's or the knee's ring against its two neighbours' line: the slice there is the one that crosses the joint)
    good[0] = good[0] && !(good[1] && good[2] && glm::distance(at(g.axis, 0), 2.f * at(g.axis, 1) - at(g.axis, 2)) > jumpAt(0));
    for(int i = 0; i < n; i++)
    {
        goods += good[i] ? 1 : 0;
    }
    glm::vec2 fixed[planMostRings];
    for(int i = 0; i < n; i++)
    {
        fixed[i] = at(g.axis, i);
        if(good[i])
        {
            continue;
        }
        int lo = -1, hi = -1;
        for(int k = i - 1; k >= 0 && lo < 0; k--)
        {
            lo = good[k] ? k : lo;
        }
        for(int k = i + 1; k < n && hi < 0; k++)
        {
            hi = good[k] ? k : hi;
        }
        // (between two good rings: on their line; past them: on the line through the nearest two, its step no more than a jump)
        int lo2 = -1, hi2 = -1;
        for(int k = (hi >= 0 ? hi + 1 : n); k < n && hi2 < 0; k++)
        {
            hi2 = good[k] ? k : hi2;
        }
        for(int k = (lo >= 0 ? lo - 1 : -1); k >= 0 && lo2 < 0; k--)
        {
            lo2 = good[k] ? k : lo2;
        }
        const auto lineAt = [&](int a, int b) {
            const glm::vec2 pa = at(g.axis, a), pb = at(g.axis, b);
            const glm::vec2 p = pa + (pb - pa) * (static_cast<float>(i - a) / static_cast<float>(b - a));
            const glm::vec2 step = p - (za::abs(i - a) < za::abs(i - b) ? pa : pb);
            const float most = jumpAt(i), len = glm::length(step);
            const bool past = i < za::min(a, b) || i > za::max(a, b);
            return past && len > most ? p - step * (1.f - most / len) : p;
        };
        fixed[i] = goods == 0 ? ref
                   : lo >= 0 && hi >= 0 ? lineAt(lo, hi)
                   : hi >= 0 && hi2 >= 0 ? lineAt(hi, hi2)
                   : lo >= 0 && lo2 >= 0 ? lineAt(lo2, lo)
                   : at(g.axis, hi >= 0 ? hi : lo);
    }
    for(int i = 0; i < n; i++)
    {
        at(g.axis, i) = fixed[i];
    }
}

// The grid's shape from the piece's extent: its rows and columns, its rings' middles.
void planGrid(const Local& L, const PieceFrame& f, int piece, PlateGrid& g)
{
    const bool torso = piece == Cuirass;
    float hLo = 1e9f, hHi = -1e9f;
    glm::vec2 lo{1e9f}, hi{-1e9f};
    for(const glm::vec3& p : L.orig)
    {
        const glm::vec3 q = p - f.origin;
        hLo = za::min(hLo, glm::dot(q, f.up));
        hHi = za::max(hHi, glm::dot(q, f.up));
        lo = glm::min(lo, glm::vec2{glm::dot(q, f.fwd), glm::dot(q, f.left)});
        hi = glm::max(hi, glm::vec2{glm::dot(q, f.fwd), glm::dot(q, f.left)});
    }
    g.centred = torso;
    const bool upperArm = piece == PauldronL || piece == PauldronR, thigh = piece == ThighL || piece == ThighR;
    const bool forearm = piece == BracerL || piece == BracerR, shin = piece == ShinL || piece == ShinR;
    g.hTop = torso ? hHi : hLo - (upperArm ? plateShoulderReach : plateJointReach);
    g.hBottom = torso ? hLo - plateWaist : hHi + plateJointReach;
    // (short of the elbow and the knee, and past them)
    // (a sleeve to the joint, or past it as far as the region goes, but not far under the next part's plates)
    const float below = (upperArm ? plateElbowBelow : plateKneeBelow) + sleevePast, above = (forearm ? plateElbowAbove : plateKneeAbove) - sleevePast;
    g.sleeveEnd = upperArm || thigh ? za::clamp(g.hBottom, f.length, f.length + below) : forearm || shin ? za::clamp(g.hTop, -above, 0.f) : 0.f;
    g.hBottom = upperArm || thigh ? za::min(g.hBottom, f.length - (upperArm ? plateElbowAbove : plateKneeAbove)) : g.hBottom;
    g.hTop = forearm || shin ? za::max(g.hTop, forearm ? plateElbowBelow : plateKneeBelow) : g.hTop;
    // (a humanoid's hand and foot stay bare: their skin weighs on the forearm and the shin well past the wrist and the ankle, so the
    // plates went over the hand and round the foot, below its sole; his notes, 2026-11-06)
    g.hBottom = f.ends && (forearm || shin) ? za::min(g.hBottom, f.length - (forearm ? plateWristAbove : plateAnkleAbove)) : g.hBottom;
    // (a thigh's plates up to the hip, whatever its weights: an enforcer's and a death knight's thighs weigh on them only near the knee)
    g.hTop = thigh && f.ends ? za::min(g.hTop, f.length * f.hipFrom) : g.hTop;
    g.hTop = upperArm && f.ends ? za::max(za::min(g.hTop, f.topAt), f.topMost) : g.hTop;
    g.hTopRaw = g.hTop;
    // (armour.txt: the rows' ends further along the piece, towards its far end: down the torso, down a limb's bone)
    const float along = g.hBottom >= g.hTop ? 1.f : -1.f;
    g.hTop = f.shiftStart != 0.f ? g.hTop + along * f.shiftStart : g.hTop;
    g.hBottom = f.shiftEnd != 0.f ? g.hBottom + along * f.shiftEnd : g.hBottom;
    const float tall = za::abs(g.hBottom - g.hTop);
    const bool leg = piece >= ThighL;
    g.rows = za::clamp(static_cast<int>(tall / (torso ? plateHighTorso : leg ? plateHighLeg : plateHighLimb) + 0.5f), 2, torso ? plateMostRowsTorso : leg ? plateMostRowsLong : plateMostRowsArm);
    g.rows += torso ? 1 : 0;
    g.rows = f.rows > 0 ? za::clamp(f.rows, 2, torso ? 12 : 10) : g.rows;
    g.yoke = torso ? (f.yoke >= 0.f ? f.yoke : plateYoke) : 0.f;
    g.cols = torso ? 8 : 6;
    g.axis.clear();
    g.axis.resize(static_cast<za::SizeT>(g.rows + 1), glm::vec2{0.f});
    const float band = za::max(tall / static_cast<float>(g.rows) * 0.6f, 0.6f);
    // (a humanoid's forearm's and shin's middles from their own skin, not the hand's or the foot's past the bone's end: an enforcer's
    // hand drew his wrist ring aside, its row 8.4 units wider than his arm)
    const float bandEnd = f.ends && (forearm || shin) ? f.length : 1e9f;
    float reachOf[planMostRings]{}; // (each ring's whole-skin exits' mean distance from its middle: the limb's girth there)
    for(int ring = 0; ring <= g.rows; ring++)
    {
        glm::vec2 bl{1e9f}, bh{-1e9f};
        for(const glm::vec3& p : L.orig)
        {
            const glm::vec3 q = p - f.origin;
            if(za::abs(glm::dot(q, f.up) - g.hAt(ring)) <= band && glm::dot(q, f.up) <= bandEnd)
            {
                bl = glm::min(bl, glm::vec2{glm::dot(q, f.fwd), glm::dot(q, f.left)});
                bh = glm::max(bh, glm::vec2{glm::dot(q, f.fwd), glm::dot(q, f.left)});
            }
        }
        at(g.axis, ring) = bl.x <= bh.x ? (bl + bh) * 0.5f : (lo + hi) * 0.5f;
        // (a humanoid's forearm, shin or thigh: the ring round the whole skin's middle at that height, as a sleeve's: its region's own
        // vertices there can be a sliver (an enforcer's forearm weighs on the upper arm and the hand half way down it, his thigh on
        // the hip only at its front), which put his right bracer's plates 1 unit to one side, sunk into the arm on the other, and his
        // thighs' top rings 3 units forward, bulky)
        // (a thigh's only where its region's slice is a sliver against the skin's: lower down its own skin is the leg)
        for(int pass = 0; pass < 2 && f.ends && (forearm || shin || thigh) && f.body; pass++)
        {
            const glm::vec3 o = f.origin + f.up * g.hAt(ring) + f.fwd * at(g.axis, ring).x + f.left * at(g.axis, ring).y;
            glm::vec2 mid{0.f}, el{1e9f}, eh{-1e9f};
            int found = 0;
            for(int col = 0; col < g.cols * 2; col++)
            {
                const float az = static_cast<float>(col) * pi / static_cast<float>(g.cols);
                const glm::vec3 d = f.fwd * za::cos(az) + f.left * za::sin(az);
                float t = 0.f;
                W w;
                if(bodyExit(*f.body, o, d, f.length, t, w))
                {
                    const glm::vec2 e{glm::dot(o + d * t - f.origin, f.fwd), glm::dot(o + d * t - f.origin, f.left)};
                    mid += e;
                    el = glm::min(el, e);
                    eh = glm::max(eh, e);
                    found++;
                }
            }
            const bool sliver = bl.x > bh.x || bh.x - bl.x < (eh.x - el.x) * 0.7f || bh.y - bl.y < (eh.y - el.y) * 0.7f;
            if(thigh && pass == 0 && !sliver)
            {
                break;
            }
            at(g.axis, ring) = found >= g.cols * (thigh ? 1 : 2) ? mid / static_cast<float>(found) : at(g.axis, ring);
            reachOf[za::min(ring, planMostRings - 1)] = found > 0 ? 0.25f * ((eh.x - el.x) + (eh.y - el.y)) : 0.f;
        }
        // (the torso's middle on the body's centre line: its seam)
        at(g.axis, ring).y = torso ? 0.f : at(g.axis, ring).y;
    }
    if(f.ends && (forearm || shin) && f.body)
    {
        smoothAxis(g, reachOf);
    }
    g.radius.clear();
    g.radius.resize(static_cast<za::SizeT>((g.rows + 1) * g.cols), 0.f);
}

// How far out the piece's skin is along o + t d: the outermost crossing of its region. False: none.
[[nodiscard]] bool skinAlong(const Local& L, const glm::vec3& o, const glm::vec3& d, float& t)
{
    bool found = false;
    t = 0.f;
    for(int k = 0; k + 2 < count(L.tris); k += 3)
    {
        const glm::vec3& a = at(L.orig, at(L.tris, k));
        const glm::vec3 e1 = at(L.orig, at(L.tris, k + 1)) - a, e2 = at(L.orig, at(L.tris, k + 2)) - a;
        const glm::vec3 pv = glm::cross(d, e2);
        const float det = glm::dot(e1, pv);
        if(za::abs(det) < 1e-7f)
        {
            continue;
        }
        const glm::vec3 tv = o - a;
        const float u = glm::dot(tv, pv) / det;
        const glm::vec3 qv = glm::cross(tv, e1);
        const float v = glm::dot(d, qv) / det;
        const float x = glm::dot(e2, qv) / det;
        if(u >= -1e-3f && v >= -1e-3f && u + v <= 1.001f && x > t)
        {
            t = x;
            found = true;
        }
    }
    return found;
}

// The radii: each corner the skin's own distance along its ray at that height (a ring just past the region's wavy edge: from a little
// way in), then raised just enough that a flat plate's chord clears the skin half way round its column and half way down its row.
[[nodiscard]] bool bodyExit(const Body& b, const glm::vec3& o, const glm::vec3& d, float most, float& t, W& w);

void fillRadii(const Local& L, const za::Vector<glm::vec3>& fringe, const PieceFrame& f, PlateGrid& g)
{
    const float ridge = f.ridge >= 0.f ? f.ridge : plateRidge;
    const auto dirOf = [&](float az) { return f.fwd * za::cos(az) + f.left * za::sin(az); };
    const float colWide = 2.f * pi / static_cast<float>(g.cols);
    const auto skinAt = [&](float h, const glm::vec2& c, float az, float back, float& t) {
        bool hit = false;
        for(const float by : {0.f, 0.5f, 1.f})
        {
            const float at0 = h + back * by;
            hit = hit || skinAlong(L, f.origin + f.up * at0 + f.fwd * c.x + f.left * c.y, dirOf(az), t);
        }
        return hit;
    };
    const int first = g.yoke > 0.f ? 1 : 0;
    float regionLo = 1e9f;
    for(const glm::vec3& p : L.orig)
    {
        regionLo = za::min(regionLo, glm::dot(p - f.origin, f.up));
    }
    for(int ring = first; ring <= g.rows; ring++)
    {
        const float h = g.hAt(ring);
        const float inward = ring == g.rows ? g.hAt(ring - 1) - h : ring == first ? g.hAt(ring + 1) - h : 0.f;
        for(int col = 0; col < g.cols; col++)
        {
            float t = 0.f;
            g.at2(ring, col) = skinAt(h, at(g.axis, ring), g.azOf(col), inward * 0.6f, t) ? t : 0.f;
            // (a humanoid's ring past its region (a thigh's top at the hip): where the whole skin is, out from the axis)
            W w;
            const glm::vec2 c = at(g.axis, ring);
            // (and a forearm's or a shin's ring where its region is a sliver: an enforcer's forearm half way down)
            g.at2(ring, col) = g.at2(ring, col) <= 0.01f && (f.hip || (f.ends && !f.upper)) && f.body &&
                                       bodyExit(*f.body, f.origin + f.up * h + f.fwd * c.x + f.left * c.y, dirOf(g.azOf(col)), f.length * 1.5f, t, w)
                                   ? t
                                   : g.at2(ring, col);
            // (a humanoid's thigh's rings above its region: never inside the whole skin there. Its region's few triangles that reach up
            // to the hip are the thigh's inner side, so its rays found them close in and the top rows sank into the leg: the enforcer's
            // and the death knight's thighs bare from the hip to the knee's row; his notes, 2026-11-06)
            g.at2(ring, col) = f.hip && f.body && h < regionLo &&
                                       bodyExit(*f.body, f.origin + f.up * h + f.fwd * c.x + f.left * c.y, dirOf(g.azOf(col)), f.length * 1.5f, t, w)
                                   ? za::max(g.at2(ring, col), t)
                                   : g.at2(ring, col);
        }
    }
    // (corners no ray found: from the ones next to them, round the ring first)
    for(int pass = 0; pass < g.rows + g.cols; pass++)
    {
        for(int ring = first; ring <= g.rows; ring++)
        {
            for(int col = 0; col < g.cols; col++)
            {
                if(g.at2(ring, col) > 0.01f)
                {
                    continue;
                }
                const float side0 = g.at2(ring, col - 1), side1 = g.at2(ring, col + 1);
                const float up = ring > first ? g.at2(ring - 1, col) : 0.f, down = ring < g.rows ? g.at2(ring + 1, col) : 0.f;
                g.at2(ring, col) = side0 > 0.01f && side1 > 0.01f ? (side0 + side1) * 0.5f : za::max(za::max(side0, side1), za::max(up, down));
            }
        }
    }
    // The chords: round each column, down each row.
    const float chord = za::max(za::cos(colWide * 0.5f), 0.5f);
    za::Vector<float> raised = g.radius;
    for(int ring = first; ring <= g.rows; ring++)
    {
        for(int col = 0; col < g.cols; col++)
        {
            float t = 0.f;
            const float azMid = g.azOf(col) + colWide * 0.5f;
            if(skinAlong(L, f.origin + f.up * g.hAt(ring) + f.fwd * at(g.axis, ring).x + f.left * at(g.axis, ring).y, dirOf(azMid), t))
            {
                const float lack = t / chord - (g.at2(ring, col) + g.at2(ring, col + 1)) * 0.5f;
                if(lack > 0.f)
                {
                    at(raised, ring * g.cols + col) = za::max(at(raised, ring * g.cols + col), g.at2(ring, col) + lack);
                    at(raised, ring * g.cols + (col + 1) % g.cols) = za::max(at(raised, ring * g.cols + (col + 1) % g.cols), g.at2(ring, col + 1) + lack);
                }
            }
            if(ring < g.rows)
            {
                const glm::vec2 c = (at(g.axis, ring) + at(g.axis, ring + 1)) * 0.5f;
                if(skinAlong(L, f.origin + f.up * ((g.hAt(ring) + g.hAt(ring + 1)) * 0.5f) + f.fwd * c.x + f.left * c.y, dirOf(g.azOf(col)), t))
                {
                    const float lack = t - (g.at2(ring, col) + g.at2(ring + 1, col)) * 0.5f;
                    if(lack > 0.f)
                    {
                        at(raised, ring * g.cols + col) = za::max(at(raised, ring * g.cols + col), g.at2(ring, col) + lack);
                        at(raised, (ring + 1) * g.cols + col) = za::max(at(raised, (ring + 1) * g.cols + col), g.at2(ring + 1, col) + lack);
                    }
                }
            }
        }
    }
    g.radius = raised;
    // The region's own vertices under the chord of their cell (a low-poly limb's corners stick out between the rays), just enough, each
    // corner by at most plateRidge: a spike doesn't bulge a row.
    const za::Vector<float> rayRadii = g.radius;
    const float rowHigh = g.hAt(first + 1) - g.hAt(first);
    const float hLo = za::min(g.hAt(first), g.hAt(g.rows)), hHi = za::max(g.hAt(first), g.hAt(g.rows));
    const int numOwn = count(L.orig);
    for(int pass = 0; pass < 3; pass++)
    {
        for(int i = 0; i < numOwn + count(fringe); i++)
        {
            const bool own = i < numOwn;
            const glm::vec3& p = own ? at(L.orig, i) : at(fringe, i - numOwn);
            const glm::vec3 q = p - f.origin;
            // (the fringe's within the rows alone; a humanoid limb's own nearly so: the hand, the foot, the joint's far side past its ends
            // swelled its end rows)
            // (an upper arm's or a thigh's joint end keeps its own's push a little further: past it, the knee's point showed)
            const float margin = own ? f.ends ? 0.3f : 1e9f : 0.f, marginEnd = own && f.ends && f.upper ? 1.0f : margin;
            if(glm::dot(q, f.up) < hLo - margin || glm::dot(q, f.up) > hHi + marginEnd)
            {
                continue;
            }
            const float rowF = za::clamp(static_cast<float>(first) + (glm::dot(q, f.up) - g.hAt(first)) / rowHigh, static_cast<float>(first), static_cast<float>(g.rows) - 1e-3f);
            const int row = static_cast<int>(rowF);
            const float t = rowF - static_cast<float>(row);
            const glm::vec2 c = at(g.axis, row) + (at(g.axis, row + 1) - at(g.axis, row)) * t;
            const glm::vec2 rel = glm::vec2{glm::dot(q, f.fwd), glm::dot(q, f.left)} - c;
            float az = za::atan2(rel.y, rel.x) - g.azOf(0);
            az = az < 0.f ? az + 2.f * pi : az;
            const float colF = za::min(az / colWide, static_cast<float>(g.cols) - 1e-3f);
            const int col = static_cast<int>(colF);
            const float u = colF - static_cast<float>(col);
            const float need = glm::length(rel) * za::cos((u - 0.5f) * colWide) / chord;
            const float w00 = (1.f - t) * (1.f - u), w01 = (1.f - t) * u, w10 = t * (1.f - u), w11 = t * u;
            const float have = g.at2(row, col) * w00 + g.at2(row, col + 1) * w01 + g.at2(row + 1, col) * w10 + g.at2(row + 1, col + 1) * w11;
            if(need > have && (own || need - have <= ridge))
            {
                const float k = (need - have) / za::max(w00 * w00 + w01 * w01 + w10 * w10 + w11 * w11, 1e-3f);
                g.at2(row, col) += k * w00;
                g.at2(row, col + 1) += k * w01;
                g.at2(row + 1, col) += k * w10;
                g.at2(row + 1, col + 1) += k * w11;
            }
        }
    }
    for(int i = 0; i < count(g.radius); i++)
    {
        at(g.radius, i) = za::min(at(g.radius, i), at(rayRadii, i) + ridge);
    }
}

// The yoke's top ring: round the neck (its radius from the skin weighing on the neck).
void closeYoke(const Body& b, const rig2::Rig& rig, const PieceFrame& f, PlateGrid& g)
{
    const int neck = rig2::boneByName(rig, "neck");
    const glm::vec2 c = at(g.axis, 0);
    float sum = 0.f;
    int n = 0;
    for(int v = 0; neck >= 0 && v < count(b.p); v++)
    {
        if(at(b.w, v).w[neck] > 0.4f)
        {
            const glm::vec3 q = at(b.p, v) - f.origin;
            sum += glm::length(glm::vec2{glm::dot(q, f.fwd), glm::dot(q, f.left)} - c);
            n++;
        }
    }
    float ring = 0.f;
    for(int col = 0; col < g.cols; col++)
    {
        ring += g.at2(1, col) / static_cast<float>(g.cols);
    }
    const float neckRadius = n ? sum / static_cast<float>(n) * 1.15f : ring * 0.5f;
    for(int col = 0; col < g.cols; col++)
    {
        g.at2(0, col) = za::min(neckRadius / za::cos(pi / static_cast<float>(g.cols)), g.at2(1, col));
    }
}

// The cuirass, its own mirror image: each column as far out as its image where the two are alike, half way where they aren't (a rest
// pose's lean: not the wider side's bulk on both).
void mirrorTorso(PlateGrid& g)
{
    for(int ring = 0; ring <= g.rows; ring++)
    {
        for(int col = 0; col < g.cols; col++)
        {
            const float x = g.at2(ring, col), y = g.at2(ring, g.mirrorOf(col));
            const float both = za::abs(x - y) < 0.5f ? za::max(x, y) : (x + y) * 0.5f;
            g.at2(ring, col) = both;
            g.at2(ring, g.mirrorOf(col)) = both;
        }
    }
}

// Each column as far out as its mirror image's where the two sides are alike (a body's small differences don't show); a wearer whose
// limbs really differ (a knight's sword arm) keeps each its own fit.
enum class Mirror
{
    Whole,     // the player's: the whole piece if alike, else each its own
    Shoulders, // a humanoid's upper arms: always, half way where they differ (its two shoulders' fits differed, a death knight's left
               // twice his right; his notes, 2026-11-06)
    Corners,   // a humanoid's other limbs: corner by corner, alike or each its own (a knight's and an enforcer's sides differ)
    ShouldersMost // a knight's upper arms: always, the larger (Wearer::shoulderRidge: over his shoulder pads)
};
// `jointRing`: the row at the joint the leather's end ring sits on (-1 none): there, in Corners, a corner sunk well under its
// mirror image's takes its image's (the grunt's left forearm's first row, one corner at 1.17 against his right's 2.76: the
// leather's end ring dented there, its cells turned in; his notes, 2026-11-09).
void mirrorRadii(PlateGrid& a, PlateGrid& b, Mirror how, int jointRing)
{
    for(int ring = 0; how != Mirror::Whole && ring <= za::min(a.rows, b.rows); ring++)
    {
        for(int col = 0; col < a.cols; col++)
        {
            const float x = a.at2(ring, col), y = b.at2(ring, b.mirrorOf(col));
            const bool sunk = how == Mirror::Corners && ring == jointRing && za::min(x, y) < plateJointSunk * za::max(x, y);
            const bool alike = za::abs(x - y) < 0.5f || how == Mirror::ShouldersMost || sunk;
            a.at2(ring, col) = alike ? za::max(x, y) : how == Mirror::Shoulders ? (x + y) * 0.5f : x;
            b.at2(ring, b.mirrorOf(col)) = alike ? za::max(x, y) : how == Mirror::Shoulders ? (x + y) * 0.5f : y;
        }
    }
    if(how != Mirror::Whole)
    {
        return;
    }
    float apart = 0.f;
    int n = 0;
    for(int ring = 0; ring <= a.rows; ring++)
    {
        for(int col = 0; col < a.cols; col++)
        {
            apart += za::abs(a.at2(ring, col) - b.at2(ring, b.mirrorOf(col)));
            n++;
        }
    }
    if(apart / static_cast<float>(za::max(n, 1)) > 0.6f)
    {
        return;
    }
    for(int ring = 0; ring <= a.rows; ring++)
    {
        for(int col = 0; col < a.cols; col++)
        {
            const float most = za::max(a.at2(ring, col), b.at2(ring, b.mirrorOf(col)));
            a.at2(ring, col) = most;
            b.at2(ring, b.mirrorOf(col)) = most;
        }
    }
}

// A plate's place in its tile, and in a hero plate (a run of a row's columns drawn as one, platesOf): u at its corners 0, 3's side
// and at 1, 2's (of a tile `span` atlas tiles wide), and which sides are inside the run: no seam, bevel or trim there, its foot, lip
// and face meeting its neighbour's on the column's line (so the run's plates make one face, one lip, one trim round its outside).
struct PlateSpan
{
    int tile{0}, span{1};
    float uL{0.f}, uR{1.f};
    bool openL{false}, openR{false};
};

// One plate, a shingle: its foot on the grid's corners (0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left, seen from outside),
// `outward` each corner's way out, `w` each corner's weights. Its top edge lies on its foot (under the row above); its lower edge
// stands `proud` out, laps down past its row and is drawn in from its neighbours. Vertices: plateVerts, the face's first in
// corner order (vr_agarmor.cpp plateWhere, vr_agarmor_look_check count on it), then its lower bevel's (edge 2, 3, face 2, 3).
// `sp`: its tile and its share of it, its sides inside a hero plate.

void addPlate(Shell& s, const glm::vec3* corners, const glm::vec3* outward, const W* w, const glm::vec3& out, float proud, int piece, const PlateSpan& sp)
{
    glm::vec3 n = geom::safeNormalize(glm::cross(corners[2] - corners[0], corners[3] - corners[1]), out, 1e-6f);
    const bool reversed = glm::dot(n, out) < 0.f;
    n = reversed ? -n : n;
    const glm::vec3 across = geom::safeNormalize((corners[1] - corners[0]) + (corners[2] - corners[3]), glm::vec3{1.f, 0.f, 0.f}, 1e-6f);
    const glm::vec3 down = geom::safeNormalize((corners[3] - corners[0]) + (corners[2] - corners[1]), glm::vec3{0.f, 0.f, -1.f}, 1e-6f);
    // (a side inside a hero plate: down its own column's line, which its neighbour's side shares, not across)
    const bool open[4] = {sp.openL, sp.openR, sp.openR, sp.openL};
    const glm::vec3 sideDown[2] = {geom::safeNormalize(corners[3] - corners[0], down, 1e-6f), geom::safeNormalize(corners[2] - corners[1], down, 1e-6f)};
    glm::vec3 foot[4], edge[4], face[4];
    for(int c = 0; c < 4; c++)
    {
        const bool lower = c == 2 || c == 3;
        const float side = c == 0 || c == 3 ? 1.f : -1.f;
        const glm::vec3 dn = open[c] ? sideDown[c == 0 || c == 3 ? 0 : 1] : down;
        // (an open side: only the feet's hair apart from its neighbour's, each slab still closed by itself)
        foot[c] = corners[c] + across * (side * plateFootApart) + dn * (lower ? -plateFootApart : plateFootApart);
        edge[c] = lower ? corners[c] + outward[c] * za::max(proud - plateBevelHigh, 0.05f) + across * (side * (open[c] ? plateFootApart : plateSeam)) + dn * plateLap : foot[c];
        face[c] = lower ? edge[c] + outward[c] * plateBevelHigh + across * (open[c] ? 0.f : side * plateBevelWide) - dn * plateBevelWide : foot[c];
    }
    const int tile = sp.tile;
    const float tx = cuirassBlock.x0 + static_cast<float>(tile % plateTilesX) * (cuirassBlock.x1 - cuirassBlock.x0) / static_cast<float>(plateTilesX);
    const float ty = cuirassBlock.y0 + static_cast<float>((tile / plateTilesX) % plateTilesY) * (cuirassBlock.y1 - cuirassBlock.y0) / static_cast<float>(plateTilesY);
    const float tw = (cuirassBlock.x1 - cuirassBlock.x0) / static_cast<float>(plateTilesX) * static_cast<float>(sp.span), th = (cuirassBlock.y1 - cuirassBlock.y0) / static_cast<float>(plateTilesY);
    // (the face's lower corners in from the tile's sides by the bevel's share, an open side's not; a wide tile's share the narrower)
    const auto tileAt = [&](int c, float inset) {
        const bool lower = c == 2 || c == 3;
        const bool right = c == 1 || c == 2;
        const float v = lower ? 1.f - inset : 0.f;
        const float uSide = right ? sp.uR : sp.uL, uOther = right ? sp.uL : sp.uR;
        const float in = lower && !open[c] ? inset / static_cast<float>(sp.span) : 0.f;
        const float us = uSide + (uOther > uSide ? in : -in);
        return glm::vec2{tx + (0.25f + us * (tw - 0.5f)), ty + (0.25f + v * (th - 0.5f))};
    };
    const auto put = [&](const glm::vec3& p, const glm::vec3& nrm, const glm::vec2& st, int layer, int c) {
        Vert o;
        o.p = p;
        o.n = nrm;
        o.st = st;
        o.w = w[c];
        o.piece = piece;
        o.layer = layer;
        s.verts.pushBack(o);
        return count(s.verts) - 1;
    };
    // (triangle a, b, c wound as seen from outside, whichever way round the corners run)
    const auto tri = [&](int a, int b, int c) {
        if(reversed)
        {
            addTri(s, a, c, b);
        }
        else
        {
            addTri(s, a, b, c);
        }
    };
    const auto normalOf = [](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& fallback, const glm::vec3& towards) {
        const glm::vec3 m = geom::safeNormalize(glm::cross(b - a, c - a), fallback, 1e-6f);
        return glm::dot(m, towards) < 0.f ? -m : m;
    };
    // The face.
    int f[4];
    for(int c = 0; c < 4; c++)
    {
        f[c] = put(face[c], n, tileAt(c, plateBevelUv), 0, c);
    }
    tri(f[0], f[1], f[2]);
    tri(f[0], f[2], f[3]);
    // The bevels: along the lower edge, and down each side (a triangle: nothing at the top).
    const glm::vec3 lipN = geom::safeNormalize(n + down * 1.2f, n, 1e-6f);
    const int b2 = put(edge[2], lipN, tileAt(2, 0.f), 0, 2), b3 = put(edge[3], lipN, tileAt(3, 0.f), 0, 3), bf2 = put(face[2], lipN, tileAt(2, plateBevelUv), 0, 2), bf3 = put(face[3], lipN, tileAt(3, plateBevelUv), 0, 3);
    tri(bf3, bf2, b2);
    tri(bf3, b2, b3);
    const glm::vec3 leftN = normalOf(edge[0], edge[3], face[3], n, n - across), rightN = normalOf(edge[1], edge[2], face[2], n, n + across);
    const int l0 = put(edge[0], leftN, tileAt(0, 0.f), 0, 0), l3 = put(edge[3], leftN, tileAt(3, 0.f), 0, 3), lf = put(face[3], leftN, tileAt(3, plateBevelUv), 0, 3);
    const int r1 = put(edge[1], rightN, tileAt(1, 0.f), 0, 1), r2 = put(edge[2], rightN, tileAt(2, 0.f), 0, 2), rf = put(face[2], rightN, tileAt(2, plateBevelUv), 0, 2);
    tri(l0, lf, l3);
    tri(r1, r2, rf);
    // The sides, down to the foot: under the lower edge (the step's face), and the seams (triangles: its top lies on its foot).
    const auto rim = [&](const glm::vec3& p, const glm::vec3& nrm, float u, float v, int c) { return put(p, nrm, toBlock(rimBlock, u, v), 2, c); };
    const glm::vec3 stepN = geom::safeNormalize(down * 1.5f + n * 0.3f, down, 1e-6f);
    const int s2 = rim(edge[2], stepN, 0.8f, 0.1f, 2), s3 = rim(edge[3], stepN, 0.2f, 0.1f, 3), sf2 = rim(foot[2], stepN, 0.8f, 0.9f, 2), sf3 = rim(foot[3], stepN, 0.2f, 0.9f, 3);
    tri(s3, s2, sf2);
    tri(s3, sf2, sf3);
    const int sl0 = rim(foot[0], -across, 0.2f, 0.9f, 0), sl3 = rim(edge[3], -across, 0.2f, 0.1f, 3), slf = rim(foot[3], -across, 0.2f, 0.9f, 3);
    const int sr1 = rim(foot[1], across, 0.8f, 0.9f, 1), sr2 = rim(edge[2], across, 0.8f, 0.1f, 2), srf = rim(foot[2], across, 0.8f, 0.9f, 2);
    tri(sl0, sl3, slf);
    tri(sr1, srf, sr2);
    // The underside (a plate off the body: a broken one's debris).
    int u[4];
    for(int c = 0; c < 4; c++)
    {
        u[c] = put(foot[c], -n, toBlock(innerBlock, c == 0 || c == 3 ? 0.2f : 0.8f, c < 2 ? 0.2f : 0.8f), 1, c);
    }
    tri(u[3], u[2], u[1]);
    tri(u[3], u[1], u[0]);
}

// The weights at a place: its region's vertices near it, blended by nearness.
[[nodiscard]] W plateWeights(const Local& L, const glm::vec3& at0)
{
    W w;
    float total = 0.f;
    float nearest = 1e9f;
    for(const glm::vec3& p : L.orig)
    {
        nearest = za::min(nearest, glm::distance(p, at0));
    }
    const float reach = nearest + 1.2f;
    for(int v = 0; v < count(L.orig); v++)
    {
        const float d = glm::distance(at(L.orig, v), at0);
        if(d > reach)
        {
            continue;
        }
        const float k = 1.f / (0.25f + d * d);
        for(int b = 0; b < rig2::maxBones; b++)
        {
            w.w[b] += at(L.w, v).w[b] * k;
        }
        total += k;
    }
    for(int b = 0; b < rig2::maxBones; b++)
    {
        w.w[b] /= za::max(total, 1e-6f);
    }
    return w;
}

// A plate's detail by its place (vr_agarmor_details): its tile (-1: a plain one by chance), its tile flipped (-1: by chance), its
// step's factor. `dirAt(col)`: that column's plate's way out, in the model's axes (x forward, y left).
struct PlateLook
{
    int tile{-1}, flip{-1};
    float raise{1.f};
};

template <typename DirAt>
[[nodiscard]] PlateLook plateLook(const PlateGrid& g, int piece, int r, int c, const DirAt& dirAt, uint32_t seed)
{
    PlateLook k;
    if(vr_agarmor_details.value == 0.f || g.rows < 1)
    {
        return k;
    }
    const bool right = piece >= PauldronL && (piece - PauldronL) % 2 == 1;
    const auto outerOf = [&](const glm::vec3& d) { return right ? -d.y : d.y; };
    const glm::vec3 dir = dirAt(c);
    const float front = dir.x;
    const int last = g.rows - 1;
    // (a row's details all one variant)
    const auto pick = [&](int first, int n) { return first + static_cast<int>(plateRand(seed, 8000 + piece * 16 + r, 0, 0) * static_cast<float>(n)) % n; };
    if(piece == Cuirass)
    {
        // (the two plates either side of the seam: the panel's recess on the seam's side of each, so the two make one panel; ribs
        // below it down to the waist's row)
        if(front > 0.6f && r == 1 && last >= 2)
        {
            k.tile = pick(tilePanel, tilePanels);
            k.flip = c == 0 ? 0 : 1;
        }
        else if(front > 0.6f && r >= 2 && r < last)
        {
            k.tile = pick(tileRib, tileRibs);
            k.flip = c == 0 ? 0 : 1;
        }
        return k;
    }
    const bool upperArm = piece == PauldronL || piece == PauldronR, forearm = piece == BracerL || piece == BracerR, shin = piece == ShinL || piece == ShinR;
    if(upperArm && r == 0)
    {
        const int e = static_cast<int>(vr_agarmor_emblem.value);
        if(!(e == 3 || (e == 1 && right) || (e == 2 && !right)))
        {
            return k;
        }
        // (the plate most to the outside, leaning to the front)
        int best = 0;
        float most = -1e9f;
        for(int col = 0; col < g.cols; col++)
        {
            const glm::vec3 d = dirAt(col);
            const float score = outerOf(d) + 0.35f * d.x;
            best = score > most ? col : best;
            most = za::max(most, score);
        }
        k.tile = c == best ? tileEmblem : -1;
        k.flip = c == best ? 0 : -1;
        return k;
    }
    // (a cop over the elbow's point, the back of the forearm's first row; over the knee, the front of the shin's)
    if((forearm && r == 0 && front < -0.8f) || (shin && r == 0 && front > 0.8f))
    {
        k.tile = tileCop;
        k.raise = copRaise;
        return k;
    }
    // (vents round the front of the forearm's cuff, its last row, and of the shin's last row)
    if((forearm || shin) && r == last && r > 0 && front > 0.3f)
    {
        k.tile = pick(tileVent, tileVents);
    }
    return k;
}

// A hero plate (vr_agarmor_hero, armour.txt's `hero`; ROUND24.md, "Armour look: hero plates"): `len` columns of a row from `first`
// (round the piece) drawn as one plate on one wide tile, every column's step `raise` times the first's.
struct HeroRun
{
    int first{0}, len{0}, tile{-1};
    float raise{1.f};
};

// Row r's hero plates (at most two): the cuirass's chest row's front plates, the breastplate (the recessed panel in its middle, on
// the seam); an upper arm's top row as two plates, half its columns each, the outer one round the emblem's column (the emblem's
// tile where vr_agarmor_emblem puts it); a cop round the elbow's point (the back of the forearm's first row) and the knee's (the
// front of the shin's), raised. `dirAt(col)`: as plateLook's. Without vr_agarmor_details: the plain wide tile.
template <typename DirAt>
[[nodiscard]] int heroRuns(const PlateGrid& g, int piece, int r, const DirAt& dirAt, HeroRun* out)
{
    const bool details = vr_agarmor_details.value != 0.f;
    const bool right = piece >= PauldronL && (piece - PauldronL) % 2 == 1;
    const auto wrap = [&](int c) { return ((c % g.cols) + g.cols) % g.cols; };
    const auto bestBy = [&](const auto& score) {
        int best = 0;
        float most = -1e9f;
        for(int col = 0; col < g.cols; col++)
        {
            const float sc = score(dirAt(col));
            best = sc > most ? col : best;
            most = za::max(most, sc);
        }
        return best;
    };
    if(piece == Cuirass)
    {
        if(r != 1 || g.rows < 3 || g.cols < 4)
        {
            return 0;
        }
        // (the run of plates facing forward: its first, the one whose left neighbour doesn't)
        int n = 0, first = -1;
        for(int col = 0; col < g.cols; col++)
        {
            const bool in = dirAt(col).x > heroChestFront;
            n += in ? 1 : 0;
            first = in && !(dirAt(wrap(col - 1)).x > heroChestFront) ? col : first;
        }
        if(n < 2 || n >= g.cols || first < 0)
        {
            return 0;
        }
        out[0] = HeroRun{first, n, details ? tileWideBreast : tileWidePlain, 1.f};
        return 1;
    }
    const bool upperArm = piece == PauldronL || piece == PauldronR, forearm = piece == BracerL || piece == BracerR, shin = piece == ShinL || piece == ShinR;
    if(r != 0 || g.cols < heroLimbRun + 2)
    {
        return 0;
    }
    if(upperArm)
    {
        // (round the plate most to the outside, leaning to the front: plateLook's emblem's)
        const int best = bestBy([&](const glm::vec3& d) { return (right ? -d.y : d.y) + 0.35f * d.x; });
        const int half = g.cols / 2;
        const int e = static_cast<int>(vr_agarmor_emblem.value);
        const bool emblem = details && (e == 3 || (e == 1 && right) || (e == 2 && !right));
        out[0] = HeroRun{wrap(best - (half - 1) / 2), half, emblem ? tileWideEmblem : tileWidePlain, 1.f};
        out[1] = HeroRun{wrap(best - (half - 1) / 2 + half), g.cols - half, tileWidePlain, 1.f};
        return 2;
    }
    if(forearm || shin)
    {
        const int best = bestBy([&](const glm::vec3& d) { return forearm ? -d.x : d.x; });
        out[0] = HeroRun{wrap(best - heroLimbRun / 2), heroLimbRun, details ? tileWideCop : tileWidePlain, copRaise};
        return 1;
    }
    return 0;
}

// The piece's plates from its grid.
void platesOf(Shell& s, const Local& L, const PieceFrame& f, const PlateGrid& g, int piece, uint32_t seed)
{
    const auto dirOf = [&](float az) { return f.fwd * za::cos(az) + f.left * za::sin(az); };
    const auto cornerAt = [&](int ring, int col) {
        const glm::vec2 c = at(g.axis, ring);
        return f.origin + f.up * g.hAt(ring) + f.fwd * c.x + f.left * c.y + dirOf(g.azOf(col)) * (g.at2(ring, col) + plateFit);
    };
    za::Vector<W> weights(static_cast<za::SizeT>((g.rows + 1) * g.cols));
    for(int ring = 0; ring <= g.rows; ring++)
    {
        for(int col = 0; col < g.cols; col++)
        {
            at(weights, ring * g.cols + col) = plateWeights(L, cornerAt(ring, col));
        }
    }
    const auto dirAt = [&](int col) {
        const int col1 = (col + 1) % g.cols;
        return dirOf(g.azOf(col) + za::atan2(za::sin(g.azOf(col1) - g.azOf(col)), za::cos(g.azOf(col1) - g.azOf(col))) * 0.5f);
    };
    const auto centreOf = [&](int r, int c) {
        const int c1 = (c + 1) % g.cols;
        return (cornerAt(r, c) + cornerAt(r, c1) + cornerAt(r + 1, c1) + cornerAt(r + 1, c)) * 0.25f;
    };
    // (behind where a wearer's cuirass stops: an enforcer's pack)
    const auto cut = [&](int r, int c) { return glm::dot(centreOf(r, c), glm::vec3{1.f, 0.f, 0.f}) < f.backCut; };
    za::Vector<int> plateFirst(static_cast<za::SizeT>(g.rows * g.cols), -1); // (each plate's first vertex: the hero plates' normals)
    HeroRun runs[2];
    za::Vector<int> runOf(static_cast<za::SizeT>(g.cols), -1), runAt(static_cast<za::SizeT>(g.cols), 0);
    for(int r = 0; r < g.rows; r++)
    {
        // (this row's hero plates: each column's run and its place in it; a run with a plate cut off: none)
        const int numRuns = f.hero ? heroRuns(g, piece, r, dirAt, runs) : 0;
        for(int c = 0; c < g.cols; c++)
        {
            at(runOf, c) = -1;
        }
        for(int k = 0; k < numRuns; k++)
        {
            bool whole = true;
            for(int i = 0; i < runs[k].len; i++)
            {
                whole = whole && !cut(r, (runs[k].first + i) % g.cols);
            }
            for(int i = 0; i < runs[k].len && whole; i++)
            {
                at(runOf, (runs[k].first + i) % g.cols) = k;
                at(runAt, (runs[k].first + i) % g.cols) = i;
            }
        }
        for(int c = 0; c < g.cols; c++)
        {
            const int c1 = (c + 1) % g.cols;
            const glm::vec3 corner[4] = {cornerAt(r, c), cornerAt(r, c1), cornerAt(r + 1, c1), cornerAt(r + 1, c)};
            const glm::vec3 outward[4] = {dirOf(g.azOf(c)), dirOf(g.azOf(c1)), dirOf(g.azOf(c1)), dirOf(g.azOf(c))};
            const W w[4] = {at(weights, r * g.cols + c), at(weights, r * g.cols + c1), at(weights, (r + 1) * g.cols + c1), at(weights, (r + 1) * g.cols + c)};
            const float azMid = g.azOf(c) + za::atan2(za::sin(g.azOf(c1) - g.azOf(c)), za::cos(g.azOf(c1) - g.azOf(c))) * 0.5f;
            if(cut(r, c))
            {
                continue;
            }
            PlateSpan sp;
            const int k = at(runOf, c);
            if(k >= 0)
            {
                // (a hero plate's column: its share of the run's wide tile, its step the run's first's)
                const HeroRun& run = runs[k];
                const int i = at(runAt, c);
                const float proud = (plateStep + (plateRand(seed, 3000 + piece * 16 + r, run.first, 0) - 0.5f) * 0.04f) * run.raise;
                sp.tile = run.tile;
                sp.span = heroSpan;
                sp.uL = static_cast<float>(i) / static_cast<float>(run.len);
                sp.uR = static_cast<float>(i + 1) / static_cast<float>(run.len);
                sp.openL = i > 0;
                sp.openR = i + 1 < run.len;
                at(plateFirst, r * g.cols + c) = count(s.verts);
                addPlate(s, corner, outward, w, dirOf(azMid), proud, piece, sp);
                continue;
            }
            float proud = plateStep + (plateRand(seed, 3000 + piece * 16 + r, c, 0) - 0.5f) * 0.04f;
            int tile = plainTiles[static_cast<int>(plateRand(seed, 5000 + piece * 16 + r, c, 0) * static_cast<float>(tilePlains)) % tilePlains];
            bool flipU = plateRand(seed, 6000 + piece * 16 + r, c, 0) < 0.5f;
            const PlateLook look = plateLook(g, piece, r, c, dirAt, seed);
            tile = look.tile >= 0 ? look.tile : tile;
            flipU = look.flip >= 0 ? look.flip != 0 : flipU;
            proud *= look.raise;
            sp.tile = tile;
            sp.uL = flipU ? 1.f : 0.f;
            sp.uR = flipU ? 0.f : 1.f;
            at(plateFirst, r * g.cols + c) = count(s.verts);
            addPlate(s, corner, outward, w, dirOf(azMid), proud, piece, sp);
        }
        // (a hero plate's faces one curved face: at each line inside it the two plates' face and lip normals shared)
        for(int c = 0; c < g.cols; c++)
        {
            const int c1 = (c + 1) % g.cols;
            const int a = at(plateFirst, r * g.cols + c), b = at(plateFirst, r * g.cols + c1);
            if(at(runOf, c) < 0 || at(runOf, c) != at(runOf, c1) || at(runAt, c1) != at(runAt, c) + 1 || a < 0 || b < 0)
            {
                continue;
            }
            // (a's right side, b's left: face 1 with 0, face 2 with 3; the lip's edge 2 with 3 and face 2 with 3)
            const int pairs[4][2] = {{a + 1, b + 0}, {a + 2, b + 3}, {a + 4, b + 5}, {a + 6, b + 7}};
            for(const auto& pr : pairs)
            {
                const glm::vec3 m = geom::safeNormalize(at(s.verts, pr[0]).n + at(s.verts, pr[1]).n, at(s.verts, pr[0]).n, 1e-6f);
                at(s.verts, pr[0]).n = m;
                at(s.verts, pr[1]).n = m;
            }
        }
    }
    // (a knight's upper arm: its top ring closed by a low dome over his shoulder pad, and a flat underside under it, so the cap is a
    // closed shell of its own; one plate tile stretched over it; the rim layer: not a plate's face for the fit's and hits' sums)
    if(f.cap > 0.f && g.rows > 0 && g.cols >= 3)
    {
        glm::vec3 mid{0.f}, next{0.f};
        for(int c = 0; c < g.cols; c++)
        {
            mid += cornerAt(0, c);
            next += cornerAt(1, c);
        }
        mid /= static_cast<float>(g.cols);
        next /= static_cast<float>(g.cols);
        float rad = 0.f;
        for(int c = 0; c < g.cols; c++)
        {
            rad += glm::distance(cornerAt(0, c), mid) / static_cast<float>(g.cols);
        }
        const glm::vec3 out = geom::safeNormalize(mid - next, -f.up, 1e-6f);
        const glm::vec3 apex = mid + out * (rad * f.cap + plateStep);
        const glm::vec3 inner = mid - out * rad; // (the dome's normals: from a point this far inside)
        const int tile = plainTiles[static_cast<int>(plateRand(seed, 7000 + piece, 0, 0) * static_cast<float>(tilePlains)) % tilePlains];
        const float tw = (cuirassBlock.x1 - cuirassBlock.x0) / static_cast<float>(plateTilesX), th = (cuirassBlock.y1 - cuirassBlock.y0) / static_cast<float>(plateTilesY);
        const float tx = cuirassBlock.x0 + static_cast<float>(tile % plateTilesX) * tw, ty = cuirassBlock.y0 + static_cast<float>(tile / plateTilesX) * th;
        const auto put = [&](const glm::vec3& p, const glm::vec3& n, float u, float v, const W& w) {
            Vert o;
            o.p = p;
            o.n = n;
            o.st = glm::vec2{tx + 0.25f + u * (tw - 0.5f), ty + 0.25f + v * (th - 0.5f)};
            o.w = w;
            o.piece = piece;
            o.layer = 2;
            s.verts.pushBack(o);
            return count(s.verts) - 1;
        };
        const int top = put(apex, out, 0.5f, 0.5f, plateWeights(L, apex));
        const int base = put(mid, -out, 0.5f, 0.5f, plateWeights(L, mid));
        // (the rim on the top ring, then a ring half way up bulging out past it (capBulge), so a pad's tip right by the rim is under
        // it too, then the apex)
        const int first = count(s.verts);
        for(int k = 0; k < 2; k++)
        {
            for(int c = 0; c < g.cols; c++)
            {
                const float az = 6.2831853f * static_cast<float>(c) / static_cast<float>(g.cols);
                const glm::vec3 rim = cornerAt(0, c);
                const glm::vec3 p = k == 0 ? rim : mid + (rim - mid) * capBulge + out * ((rad * f.cap + plateStep) * 0.5f);
                const float uv = k == 0 ? 0.3f : 0.2f; // (within the tile's face: clear of the under-layer at its top)
                put(p, geom::safeNormalize(p - inner, out, 1e-6f), 0.5f + uv * za::cos(az), 0.5f + uv * za::sin(az),
                    k == 0 ? at(weights, c) : plateWeights(L, p));
            }
        }
        for(int c = 0; c < g.cols; c++)
        {
            const int a = first + c, b = first + (c + 1) % g.cols, ma = a + g.cols, mb = b + g.cols;
            const glm::vec3 n = glm::cross(at(s.verts, a).p - apex, at(s.verts, b).p - apex);
            const bool flip = glm::dot(n, out) < 0.f;
            addTri(s, top, flip ? mb : ma, flip ? ma : mb);
            addTri(s, a, flip ? mb : b, flip ? b : mb);
            addTri(s, a, flip ? ma : mb, flip ? mb : ma);
            addTri(s, base, flip ? a : b, flip ? b : a);
        }
    }
    s.pieceOpen[piece] = 0;
}

// Where a ray from inside the body first leaves it: the skin's triangle crossed going out, how far, and the weights there.
[[nodiscard]] bool bodyExit(const Body& b, const glm::vec3& o, const glm::vec3& d, float most, float& t, W& w)
{
    bool found = false;
    t = most;
    for(int k = 0; k + 2 < count(b.tris); k += 3)
    {
        const int i0 = at(b.tris, k), i1 = at(b.tris, k + 1), i2 = at(b.tris, k + 2);
        const glm::vec3& a = at(b.p, i0);
        const glm::vec3 e1 = at(b.p, i1) - a, e2 = at(b.p, i2) - a;
        if(glm::dot(glm::cross(e1, e2), d) <= 0.f)
        {
            continue; // (going in)
        }
        const glm::vec3 pv = glm::cross(d, e2);
        const float det = glm::dot(e1, pv);
        if(za::abs(det) < 1e-9f)
        {
            continue;
        }
        const glm::vec3 tv = o - a, qv = glm::cross(tv, e1);
        const float u = glm::dot(tv, pv) / det, v = glm::dot(d, qv) / det, x = glm::dot(e2, qv) / det;
        if(u >= -1e-3f && v >= -1e-3f && u + v <= 1.001f && x > 0.f && x < t)
        {
            t = x;
            for(int i = 0; i < rig2::maxBones; i++)
            {
                w.w[i] = at(b.w, i0).w[i] * (1.f - u - v) + at(b.w, i1).w[i] * u + at(b.w, i2).w[i] * v;
            }
            found = true;
        }
    }
    return found;
}

// An elbow's or a knee's sleeve, the half on this limb's part: a band of leather bellows round the skin from under its end row of
// plates to the joint, weighted as the skin under it is, so it bends as the joint does (a part worn alone ends at the joint, the
// next part's skin carrying on from it). 36 triangles. `taperStart`, `taperEnd`: armour.txt's taper on the sleeve part (1 as made).
void sleeveOf(Shell& s, const Body& b, const PieceFrame& f, const PlateGrid& g, int piece, float taperStart, float taperEnd)
{
    const bool upper = piece == PauldronL || piece == PauldronR || piece == ThighL || piece == ThighR;
    const int endRing = upper ? g.rows : 0, nextRing = upper ? g.rows - 1 : 1;
    const float hEnd = g.hAt(endRing);
    const float h0 = hEnd + (g.hAt(nextRing) - hEnd) * sleeveUnder, h1 = g.sleeveEnd;
    const float fit = upper ? sleeveFit : sleeveFitLower;
    const glm::vec2 c = at(g.axis, endRing);
    float ringMean = 0.f;
    for(int col = 0; col < g.cols; col++)
    {
        ringMean += g.at2(endRing, col) / static_cast<float>(g.cols);
    }
    const auto dirOf = [&](float az) { return f.fwd * za::cos(az) + f.left * za::sin(az); };
    const int first = count(s.verts);
    const float most = ringMean * 1.6f + 1.f, chord = za::cos(pi / static_cast<float>(g.cols));
    constexpr int mostCols = 8;
    glm::vec3 centre[sleeveRings];
    float rad[sleeveRings][mostCols]{};
    for(int ring = 0; ring < sleeveRings; ring++)
    {
        const float along = static_cast<float>(ring) / static_cast<float>(sleeveRings - 1);
        const float h = h0 + (h1 - h0) * along;
        glm::vec3 o = f.origin + f.up * h + f.fwd * c.x + f.left * c.y;
        // (the ring round the limb's own middle at that height: where the skin is left from the plates' axis, its exits' middle)
        glm::vec3 mid{0.f};
        int found = 0;
        for(int col = 0; col < g.cols * 2; col++)
        {
            float t = 0.f;
            W w;
            const glm::vec3 d = dirOf(static_cast<float>(col) * pi / static_cast<float>(g.cols));
            if(bodyExit(b, o, d, most, t, w))
            {
                mid += o + d * t;
                found++;
            }
        }
        if(found >= g.cols)
        {
            mid /= static_cast<float>(found);
            o = mid - f.up * glm::dot(mid - o, f.up);
        }
        for(int col = 0; col <= g.cols; col++)
        {
            const float az = g.azOf(col % g.cols);
            const glm::vec3 d = dirOf(az);
            float t = 0.f;
            W w;
            if(!bodyExit(b, o, d, most, t, w))
            {
                t = ringMean * 0.8f;
                w = W{};
                const int bone = s.tubes[piece].bone;
                w.w[bone >= 0 ? bone : 0] = 1.f;
            }
            // (the skin half way round to each neighbour under the flat face between)
            float r = t;
            for(const float side : {-1.f, 1.f})
            {
                float tm = 0.f;
                W wm;
                r = bodyExit(b, o, dirOf(az + side * pi / static_cast<float>(g.cols)), most, tm, wm) ? za::max(r, tm / chord) : r;
            }
            // (under the plates at the start: no further out than their feet)
            r = ring == 0 ? za::min(r + fit, g.at2(endRing, col) + plateFit) : r + fit;
            centre[ring] = o;
            rad[ring][col % g.cols] = r;
            Vert v;
            v.p = o + d * r;
            v.n = d;
            v.st = toBlock(jointBlock, static_cast<float>(col) / static_cast<float>(g.cols), upper ? along : 1.f - along);
            v.w = w;
            v.piece = piece;
            v.layer = 3;
            s.verts.pushBack(v);
        }
    }
    // The skin's own vertices between the rings under the sleeve's flat faces, as the plates' (fillRadii): each corner out by at
    // most sleeveRidge for them (a low-poly knee's point past that is left to show, rather than a bulging sleeve).
    const float colWide = 2.f * pi / static_cast<float>(g.cols);
    const auto ridgeOut = [&]
    {
        for(int v = 0; v < count(b.p) && g.cols <= mostCols; v++)
        {
            const float h = glm::dot(at(b.p, v) - f.origin, f.up);
            const float ringF = (h - h0) / (h1 - h0 + (h1 >= h0 ? 1e-6f : -1e-6f)) * static_cast<float>(sleeveRings - 1);
            if(ringF < 0.f || ringF > static_cast<float>(sleeveRings - 1))
            {
                continue;
            }
            const int ring = za::min(static_cast<int>(ringF), sleeveRings - 2);
            const float t = ringF - static_cast<float>(ring);
            const glm::vec3 cen = centre[ring] + (centre[ring + 1] - centre[ring]) * t;
            glm::vec3 rel = at(b.p, v) - cen;
            rel -= f.up * glm::dot(rel, f.up);
            const float radial = glm::length(rel);
            if(radial > most)
            {
                continue;
            }
            float az = za::atan2(glm::dot(rel, f.left), glm::dot(rel, f.fwd)) - g.azOf(0);
            az = az < 0.f ? az + 2.f * pi : az;
            const float colF = za::min(az / colWide, static_cast<float>(g.cols) - 1e-3f);
            const int col = static_cast<int>(colF), col1 = (col + 1) % g.cols;
            const float u = colF - static_cast<float>(col);
            const float need = radial * za::cos((u - 0.5f) * colWide) / chord + fit;
            const float w00 = (1.f - t) * (1.f - u), w01 = (1.f - t) * u, w10 = t * (1.f - u), w11 = t * u;
            const float have = rad[ring][col] * w00 + rad[ring][col1] * w01 + rad[ring + 1][col] * w10 + rad[ring + 1][col1] * w11;
            if(need > have && need - have <= (f.ends ? sleeveRidgeHumanoid : sleeveRidge))
            {
                const float k = (need - have) / za::max(w00 * w00 + w01 * w01 + w10 * w10 + w11 * w11, 1e-3f);
                rad[ring][col] += k * w00;
                rad[ring][col1] += k * w01;
                rad[ring + 1][col] += k * w10;
                rad[ring + 1][col1] += k * w11;
            }
        }
    };
    ridgeOut();
    // (a humanoid's: each ring a clean band, its corners eased towards their neighbours' and none far past the ring's mean: a low-poly
    // knee's or elbow's point made them spiky and disjoint; his notes, 2026-11-06)
    for(int ring = 0; f.ends && ring < sleeveRings && g.cols <= mostCols; ring++)
    {
        float eased[mostCols]{};
        float mean = 0.f;
        for(int col = 0; col < g.cols; col++)
        {
            eased[col] = rad[ring][col] * 0.5f + (rad[ring][(col + g.cols - 1) % g.cols] + rad[ring][(col + 1) % g.cols]) * 0.25f;
            mean += eased[col] / static_cast<float>(g.cols);
        }
        for(int col = 0; col < g.cols; col++)
        {
            rad[ring][col] = za::max(za::clamp(eased[col], mean * sleeveRoundLeast, mean * sleeveRoundMost), rad[ring][col] - sleeveRoundIn); // (no further in: a knee's point showed)
        }
    }
    // (and the points it covered still under it: easing a corner in by up to sleeveRoundIn bared a grunt's knee's point)
    if(f.ends)
    {
        ridgeOut();
    }
    // (armour.txt's taper on the sleeve part: each ring's radii scaled round its middle, in the limb's order as the plates' rows
    // are: the taper at start on its end nearer the body (an upper part's under its plates, a lower part's at the joint), lerped
    // to the taper at end on the other. Its ring under the plates stays under their feet (below), so the seam holds.)
    if(taperStart != 1.f || taperEnd != 1.f)
    {
        for(int ring = 0; ring < sleeveRings && g.cols <= mostCols; ring++)
        {
            const float along = static_cast<float>(ring) / static_cast<float>(sleeveRings - 1);
            const float k = taperStart + (taperEnd - taperStart) * (upper ? along : 1.f - along);
            for(int col = 0; col < g.cols; col++)
            {
                rad[ring][col] = za::max(rad[ring][col] * k, 0.05f);
            }
        }
    }
    const int row = g.cols + 1;
    for(int ring = 0; ring < sleeveRings && g.cols <= mostCols; ring++)
    {
        for(int col = 0; col <= g.cols; col++)
        {
            const float r = ring == 0 ? za::min(rad[ring][col % g.cols], g.at2(endRing, col) + plateFit) : rad[ring][col % g.cols];
            at(s.verts, first + ring * row + col).p = centre[ring] + dirOf(g.azOf(col % g.cols)) * r;
        }
    }
    for(int ring = 0; ring + 1 < sleeveRings; ring++)
    {
        for(int col = 0; col < g.cols; col++)
        {
            const int a = first + ring * row + col, bb = a + 1, cc = a + row + 1, dd = a + row;
            const glm::vec3 out = dirOf(g.azOf(col) + pi / static_cast<float>(g.cols));
            const bool flip = glm::dot(glm::cross(at(s.verts, bb).p - at(s.verts, a).p, at(s.verts, cc).p - at(s.verts, a).p), out) < 0.f;
            addTri(s, a, flip ? cc : bb, flip ? bb : cc);
            addTri(s, a, flip ? dd : cc, flip ? cc : dd);
        }
    }
}

// ---- The helmet: the Ranger's (the player's face in the status bar), built round the head ----
// A flat-topped bucket over the crown (its top edge chamfered, its lower edge a ledge over the brow), cheek guards from
// under it down the sides of the face to the jaw line, the back down to the nape, the face open (the eyes under the
// brow; no side brackets: they stood off as flanges, 2026-10-29). Fitted close: a small gap and taper, round chamfers,
// the guards round the head proper (not the collar blended onto it). In the head bone's frame (x forward, y left, z up its length from its pivot) each ring of it is an
// octagon (a front, a back, two sides, four chamfers) fitted round the head's vertices a gap off them: the ring takes
// every vertex of the bands above and below it (a face vertex only above the guards), so any place between two rings is
// clear too. Rigid on the head bone. Texture: the walls mirrored left and right (u from the front's middle round the
// side to the back's, v down the rows: make_agarmor.py's HELM_U and HELM_V), the cap a disc, the open edges the lining.
struct Ring
{
    float F{0.f}, B{0.f}, W{0.f}; // front, back, half width
    float Df{0.f}, Db{0.f};       // the chamfers: x + |y| <= Df, -x + |y| <= Db
    float Wb{-1.f};               // the half width where the side meets the back chamfer (-1: W; less: the side slants in)
};
enum HelmRow : int
{
    HCap,    // the top's edge
    HCrown,  // below its chamfer
    HBrow,   // the bucket's lower edge
    HLedge,  // under it, stepped in: the guards' top (the face's top in front)
    HCheek,  // the cheekbones
    HJaw,    // the jaw line (lower at the back: the nape)
    NumHelmRows
};
constexpr float helmV[NumHelmRows] = {0.f, 0.08f, 0.38f, 0.46f, 0.7f, 1.f};
// The octagon's points on its left half, front to back: the front's middle, the front chamfer's ends, the back chamfer's
// ends, the back's middle; round the helmet: 0..5 down its left, 6..9 back up its right.
constexpr int numHelmPoints = 6, numHelmCols = 10;
constexpr float helmU[numHelmPoints] = {0.f, 0.16f, 0.3f, 0.7f, 0.84f, 1.f};
[[nodiscard]] int pointOf(int col)
{
    return col <= 5 ? col : 10 - col;
}
[[nodiscard]] glm::vec2 ringPoint(const Ring& r, int point, int col)
{
    const float side = col <= 5 ? 1.f : -1.f;
    glm::vec2 p{0.f};
    switch(point)
    {
    case 0: p = {r.F, 0.f}; break;
    case 1: p = {r.F, r.Df - r.F}; break;
    case 2: p = {r.Df - r.W, r.W}; break;
    case 3:
    {
        const float w = r.Wb >= 0.f ? r.Wb : r.W;
        p = {w - r.Db, w};
        break;
    }
    case 4: p = {-r.B, r.Db - r.B}; break;
    default: p = {-r.B, 0.f}; break;
    }
    return glm::vec2{p.x, p.y * side};
}
[[nodiscard]] Ring grown(const Ring& r, float d)
{
    constexpr float root2 = 1.41421356f;
    return Ring{r.F + d, r.B + d, r.W + d, r.Df + d * root2, r.Db + d * root2, r.Wb >= 0.f ? r.Wb + d : -1.f};
}
// The lowest row of the strip from column c to c + 1: the face's two strips end under the bucket.
[[nodiscard]] int stripBottom(int c)
{
    return c == 0 || c == numHelmCols - 1 ? HLedge : HJaw;
}

struct HelmBuild
{
    Shell& s;
    glm::vec3 origin, fwd, left, up;
    W w;
    glm::vec3 toModel(const glm::vec3& h) const { return origin + fwd * h.x + left * h.y + up * h.z; }
    // A flat face (3 or 4 corners, wound outward), its own vertices.
    void face(const glm::vec3* p, const glm::vec2* st, int n, int layer)
    {
        const glm::vec3 a = toModel(p[0]), b = toModel(p[1]), c = toModel(p[2]);
        const glm::vec3 nrm = geom::safeNormalize(glm::cross(b - a, c - a), up, 1e-6f);
        const int first = count(s.verts);
        for(int k = 0; k < n; k++)
        {
            Vert o;
            o.p = toModel(p[k]);
            o.n = nrm;
            o.st = st[k];
            o.w = w;
            o.piece = Helmet;
            o.layer = layer;
            s.verts.pushBack(o);
        }
        for(int k = 1; k + 1 < n; k++)
        {
            addTri(s, first, first + k, first + k + 1);
        }
    }
};

void helmetOf(Shell& s, const Body& b, const PieceFrame& f, int headBone, const Wearer& who)
{
    // (rigid on the head: closer than the body's plates, and thinner; vr_agarmor_helmet_gap, _thick)
    const float gap = za::max(vr_agarmor_helmet_gap.value, 0.f), thick = za::max(vr_agarmor_helmet_thick.value, 0.05f);
    const float L = f.length;
    constexpr float root2 = 1.41421356f;
    // (the head proper: up to this much of the head bone's length above its pivot and to its sides; Wearer)
    const float headTop = who.headTop, headSide = who.headSide;
    // The head: its surface where it is weighted to it (its neck's top too; points across each triangle, the low-poly
    // head's big triangles having no vertices between its crown and its jaw), in its frame, |y| (the helmet is symmetric).
    za::Vector<glm::vec3> pts;
    za::Vector<float> ptW; // (its weight on the head)
    float top = 0.f;
    constexpr int steps = 6;
    // (past the head proper: a triangle with a corner past its sides or its top; over its top, every triangle on that
    // triangle's corners too: the death knight's horns, rigid on his head, and their roots, not under the helmet)
    const auto past = [&](int i) {
        const glm::vec3 q = at(b.p, i) - f.origin;
        return glm::dot(q, f.up) > headTop * L || za::abs(glm::dot(q, f.left)) > headSide * L;
    };
    const auto over = [&](int i) { return glm::dot(at(b.p, i) - f.origin, f.up) > headTop * L; };
    za::Vector<uint8_t> rooted(static_cast<za::SizeT>(count(b.p)), uint8_t{0});
    for(int t = 0; t + 2 < count(b.tris); t += 3)
    {
        const int i0 = at(b.tris, t), i1 = at(b.tris, t + 1), i2 = at(b.tris, t + 2);
        const bool horn = over(i0) || over(i1) || over(i2);
        at(rooted, i0) = horn || past(i0) || at(rooted, i0);
        at(rooted, i1) = horn || past(i1) || at(rooted, i1);
        at(rooted, i2) = horn || past(i2) || at(rooted, i2);
    }
    for(int t = 0; t + 2 < count(b.tris); t += 3)
    {
        const int i0 = at(b.tris, t), i1 = at(b.tris, t + 1), i2 = at(b.tris, t + 2);
        const float w0 = at(b.w, i0).w[headBone], w1 = at(b.w, i1).w[headBone], w2 = at(b.w, i2).w[headBone];
        if(za::max(w0, w1, w2) < 0.2f || at(rooted, i0) || at(rooted, i1) || at(rooted, i2))
        {
            continue;
        }
        for(int u = 0; u <= steps; u++)
        {
            for(int v = 0; u + v <= steps; v++)
            {
                const float bu = static_cast<float>(u) / steps, bv = static_cast<float>(v) / steps, bw = 1.f - bu - bv;
                const float wp = w0 * bw + w1 * bu + w2 * bv;
                if(wp < 0.2f)
                {
                    continue;
                }
                const glm::vec3 q = at(b.p, i0) * bw + at(b.p, i1) * bu + at(b.p, i2) * bv - f.origin;
                const glm::vec3 h{glm::dot(q, f.fwd), za::abs(glm::dot(q, f.left)), glm::dot(q, f.up)};
                if(h.z > headTop * L || h.y > headSide * L)
                {
                    continue; // (past the head proper: the death knight's horns, rigid on his head, not under the helmet)
                }
                ptW.pushBack(wp);
                pts.pushBack(h);
                top = za::max(top, h.z);
            }
        }
    }
    if(pts.empty())
    {
        return;
    }
    // The rows' heights (inner surface) per point: the bucket's lower edge and the guards' rows lower toward the back,
    // the jaw's down to the nape there.
    const float chamfer = who.chamfer * L, faceHalf = 0.4f * L, taper = who.taper * L;
    const float zTop = top + gap;
    float rowZ[NumHelmRows][numHelmPoints];
    const float browAt = who.brow >= 0.f ? who.brow : vr_agarmor_brow_front.value;
    const float zRow[NumHelmRows] = {zTop, zTop - chamfer, browAt * L, (browAt - 0.06f) * L, who.cheek * L, who.jaw * L};
    constexpr float browDrop[numHelmPoints] = {0.f, 0.f, 0.04f, 0.13f, 0.2f, 0.2f}, cheekDrop[numHelmPoints] = {0.f, 0.f, 0.f, 0.03f, 0.07f, 0.07f};
    for(int r = 0; r < NumHelmRows; r++)
    {
        for(int k = 0; k < numHelmPoints; k++)
        {
            const float drop = r == HBrow || r == HLedge ? browDrop[k] : r == HCheek ? cheekDrop[k] : 0.f;
            rowZ[r][k] = zRow[r] - drop * L;
        }
    }
    const float nape = za::min((who.nape >= 0.f ? who.nape : vr_agarmor_brow_back.value) * L, rowZ[HCheek][5] - 0.05f * L);
    rowZ[HJaw][3] = nape; // (the side's back corner clear of the neck as the head turns)
    rowZ[HJaw][4] = rowZ[HJaw][5] = nape;
    // The fit: each ring round every vertex of its bands (the cap's ring the crown's, stepped in).
    Ring ring[NumHelmRows];
    for(Ring& r : ring)
    {
        r = Ring{0.1f * L, 0.1f * L, 0.1f * L, 0.2f * L, 0.2f * L};
    }
    const auto need = [&](Ring& r, const glm::vec3& p, float in) {
        r.F = za::max(r.F, p.x + gap + in);
        r.B = za::max(r.B, -p.x + gap + in);
        r.W = za::max(r.W, p.y + gap + in);
        r.Df = za::max(r.Df, p.x + p.y + (gap + in) * root2);
        r.Db = za::max(r.Db, -p.x + p.y + (gap + in) * root2);
    };
    // A row's height at a place front to back (the points' places about the head's front and back: its chamfers' ends
    // near half way to its middle).
    float xFront = 0.f, xBack = 0.f;
    for(const glm::vec3& p : pts)
    {
        xFront = za::max(xFront, p.x);
        xBack = za::max(xBack, -p.x);
    }
    const float knotX[4] = {xFront, 0.5f * xFront, -0.5f * xBack, -xBack};
    const auto zAt = [&](int r, float x) {
        if(x >= knotX[0])
        {
            return rowZ[r][1];
        }
        for(int i = 0; i < 3; i++)
        {
            if(x >= knotX[i + 1])
            {
                const float t = (knotX[i] - x) / za::max(knotX[i] - knotX[i + 1], 1e-3f);
                return rowZ[r][i + 1] + (rowZ[r][i + 2] - rowZ[r][i + 1]) * t;
            }
        }
        return rowZ[r][4];
    };
    constexpr float margin = 0.25f;
    for(int r = 0; r + 1 < NumHelmRows; r++)
    {
        for(int i = 0; i < count(pts); i++)
        {
            const glm::vec3& p = at(pts, i);
            const float hi = zAt(r, p.x), lo = zAt(r + 1, p.x);
            const bool face = r + 1 > HLedge && p.x > 0.f && p.y < faceHalf;
            // (the guards round the head proper: not the collar's vertices blended onto it, the enforcer's out to its
            // shoulders, that made its guards flanges)
            const bool collar = r >= HLedge && at(ptW, i) < who.collar;
            if(p.z > hi + margin || p.z < lo - margin || face || collar)
            {
                continue;
            }
            if(r == HCrown)
            {
                // the bucket's wall, its brow ring the crown's grown by the taper: the crown's ring as far in as that
                // lets it (a point a share s down the wall needs crown + taper s)
                const float s = za::clamp((hi - p.z) / za::max(hi - lo, 1e-3f), 0.f, 1.f);
                need(ring[HCrown], p, -taper * s);
                continue;
            }
            // (the cap's chamfer: a point a share s up it needs the crown's ring chamfer s further out)
            const float up = r == HCap ? za::clamp((p.z - lo) / za::max(hi - lo, 1e-3f), 0.f, 1.f) : 0.f;
            need(ring[r == HCap ? HCrown : r], p, chamfer * up);
            need(ring[r + 1], p, 0.f);
        }
    }
    // The guards no wider than the ledge over them and a little (what sticks out further below is the neck's and the
    // collar's, not the head's: the enforcer's shoulders made its guards side flanges).
    for(int r = HCheek; r < NumHelmRows; r++)
    {
        ring[r].W = za::min(ring[r].W, ring[HLedge].W + 0.1f * L);
    }
    // The style: the bucket a touch wider at its brow than its crown and a small ledge over the guards; its chamfers at
    // most 0.7 of its side (the guards' half: round, close to the head); the face's opening at least faceHalf wide.
    Ring& crown = ring[HCrown];
    Ring& brow = ring[HBrow];
    const Ring wider = grown(crown, taper), ledge = grown(ring[HLedge], 0.015f * L);
    brow = Ring{za::max(brow.F, wider.F, ledge.F), za::max(brow.B, wider.B, ledge.B), za::max(brow.W, wider.W, ledge.W),
        za::max(brow.Df, wider.Df, ledge.Df), za::max(brow.Db, wider.Db, ledge.Db)};
    for(int r = HCrown; r < NumHelmRows; r++)
    {
        Ring& x = ring[r];
        const float cut = (r <= HBrow ? 0.7f : 0.5f) * x.W; // the bucket rounder than the guards
        x.Df = za::clamp(za::max(x.Df, x.F + x.W - cut), 0.f, x.F + x.W);
        x.Db = za::clamp(za::max(x.Db, x.B + x.W - who.backCut2 * x.W), 0.f, x.B + x.W); // (the back round: clear of the upper back)
        x.Df = r >= HLedge ? za::clamp(x.F + faceHalf, x.Df, x.F + x.W) : x.Df;
    }
    ring[HCap] = grown(crown, -chamfer);
    // The guards as plates (Wearer), the back's chamfer as fitted.
    for(int r = HCheek; r < NumHelmRows && who.plateWide >= 0.f; r++)
    {
        Ring& x = ring[r];
        x.W = za::max(x.W, who.plateWide * L);
        x.Wb = za::max(x.Wb, who.plateBack * L);
        x.F = za::max(x.F, who.plateFront[r - HCheek] * L);
        x.Df = za::clamp(x.F + who.plateOpen[r - HCheek] * L, 0.f, x.F + x.W);
    }

    HelmBuild hb{s, f.origin, f.fwd, f.left, f.up, W{}};
    hb.w.w[headBone] = 1.f;
    // The grid: (column, row) -> its place, inner (layer 1) and outer (layer 0: grown a thickness, its top raised).
    const auto place = [&](int col, int row, int layer) {
        const Ring r = layer == 0 ? grown(ring[row], thick) : ring[row];
        const glm::vec2 xy = ringPoint(r, pointOf(col), col);
        const float z = rowZ[row][pointOf(col)] + (layer == 0 && row <= HCrown ? thick : 0.f);
        return glm::vec3{xy.x, xy.y, z};
    };
    const auto wallST = [&](int col, int row, int layer) {
        return toBlock(layer == 0 ? helmetBlock : innerBlock, helmU[pointOf(col)], helmV[row]);
    };
    for(int layer = 0; layer < 2; layer++)
    {
        for(int c = 0; c < numHelmCols; c++)
        {
            const int d = (c + 1) % numHelmCols;
            for(int r = 0; r < stripBottom(c); r++)
            {
                glm::vec3 p[4] = {place(c, r, layer), place(c, r + 1, layer), place(d, r + 1, layer), place(d, r, layer)};
                glm::vec2 st[4] = {wallST(c, r, layer), wallST(c, r + 1, layer), wallST(d, r + 1, layer), wallST(d, r, layer)};
                // (round from the front to the left: c, its row below, then d's, is wound outward)
                if(layer == 1)
                {
                    za::genericSwap(p[1], p[3]);
                    za::genericSwap(st[1], st[3]);
                }
                hb.face(p, st, 4, layer);
            }
        }
        // The cap: a fan from its middle.
        const Ring cr = layer == 0 ? grown(ring[HCap], thick) : ring[HCap];
        const float z = zTop + (layer == 0 ? thick : 0.f), rad = za::max(cr.F, cr.B, cr.W);
        const glm::vec3 mid{(cr.F - cr.B) * 0.5f, 0.f, z};
        const auto capST = [&](const glm::vec3& p) {
            return toBlock(layer == 0 ? helmCapBlock : innerBlock, 0.5f + 0.5f * p.x / rad, 0.5f - 0.5f * p.y / rad);
        };
        for(int c = 0; c < numHelmCols; c++)
        {
            const int d = (c + 1) % numHelmCols;
            glm::vec3 p[3] = {mid, place(c, HCap, layer), place(d, HCap, layer)};
            if(layer == 1)
            {
                za::genericSwap(p[1], p[2]);
            }
            const glm::vec2 st[3] = {capST(p[0]), capST(p[1]), capST(p[2])};
            hb.face(p, st, 3, layer);
        }
    }
    // The lining: a face along each open edge from the outer layer to the inner (the outer wall's edge a -> b, wound
    // outward, its rim b, a, then the inner's a, b).
    const auto rim = [&](int ca, int ra, int cb, int rb) {
        const glm::vec3 p[4] = {place(cb, rb, 0), place(ca, ra, 0), place(ca, ra, 1), place(cb, rb, 1)};
        const glm::vec2 st[4] = {toBlock(liningBlock, 0.75f, 0.1f), toBlock(liningBlock, 0.25f, 0.1f), toBlock(liningBlock, 0.25f, 0.9f),
            toBlock(liningBlock, 0.75f, 0.9f)};
        hb.face(p, st, 4, 2);
        s.pieceOpen[Helmet]++;
    };
    for(int c = 0; c < numHelmCols; c++)
    {
        const int d = (c + 1) % numHelmCols, bot = stripBottom(c);
        rim(c, bot, d, bot); // its lower edge (the outer quad's c, bot -> d, bot)
    }
    // The guards' front edges: down column 1 (the left; the face strip ends above it) and column 9 (the right).
    for(int r = HLedge; r < HJaw; r++)
    {
        rim(1, r, 1, r + 1);
        rim(numHelmCols - 1, r + 1, numHelmCols - 1, r);
    }
    const Ring& gk = ring[HCheek];
    // (its outside's size in the head's frame: front to back, side to side, top to bottom)
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(const Vert& v : s.verts)
    {
        const glm::vec3 q = v.p - f.origin;
        const glm::vec3 h{glm::dot(q, f.fwd), glm::dot(q, f.left), glm::dot(q, f.up)};
        lo = v.piece == Helmet ? glm::min(lo, h) : lo;
        hi = v.piece == Helmet ? glm::max(hi, h) : hi;
    }
    const glm::vec3 size = hi - lo;
    q_snprintf(s.helmFit, sizeof(s.helmFit),
        "head top %.2f, helmet top %.2f (inner); brow ring front %.2f back %.2f half width %.2f; cheek ring %.2f %.2f %.2f; "
        "face opening +-%.2f; brow %.2f, jaw %.2f, nape %.2f; outside %.2f deep, %.2f wide, %.2f high",
        static_cast<double>(top), static_cast<double>(zTop), static_cast<double>(brow.F), static_cast<double>(brow.B),
        static_cast<double>(brow.W), static_cast<double>(gk.F), static_cast<double>(gk.B), static_cast<double>(gk.W),
        static_cast<double>(gk.Df - gk.F), static_cast<double>(zRow[HBrow]), static_cast<double>(zRow[HJaw]),
        static_cast<double>(nape), static_cast<double>(size.x), static_cast<double>(size.y), static_cast<double>(size.z));
}

[[nodiscard]] uint32_t fnv(uint32_t h, const void* data, size_t size)
{
    const auto* p = static_cast<const uint8_t*>(data);
    for(size_t i = 0; i < size; i++)
    {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

// Each vertex's four heaviest bones (renormalised), the rest dropped: as drawn.
void topFour(W& w)
{
    int keep[4] = {-1, -1, -1, -1};
    for(int k = 0; k < 4; k++)
    {
        int best = -1;
        for(int i = 0; i < rig2::maxBones; i++)
        {
            const bool taken = i == keep[0] || i == keep[1] || i == keep[2];
            best = !taken && w.w[i] > 1e-3f && (best < 0 || w.w[i] > w.w[best]) ? i : best;
        }
        keep[k] = best;
    }
    W o;
    float sum = 0.f;
    for(int k : keep)
    {
        sum += k >= 0 ? w.w[k] : 0.f;
    }
    for(int k : keep)
    {
        if(k >= 0)
        {
            o.w[k] = w.w[k] / za::max(sum, 1e-6f);
        }
    }
    w = o;
}

// armour.txt's tweaks (vr_armourdata.hpp) on the vertices [from, to) of a piece just made: `push` 1 a sleeve's stand-off, out
// from its middle square to `up` (its bone), 2 the helmet's, out from its middle; 0 none (a grid piece's is in its radii). Then
// scaled and turned round the middle of their box, and moved. Nothing set: not a vertex touched.
void tweakVerts(Shell& s, int from, int to, const armourdata::Override& o, int push, const glm::vec3& up, int boxTo = -1)
{
    boxTo = boxTo < 0 ? to : boxTo; // (the middle: of [from, boxTo) alone; a bridge's end ring after the plates moves with them)
    using armourdata::Override;
    const auto has = [&](int field) { return ((o.has >> static_cast<unsigned>(field)) & 1u) != 0u; };
    const bool pushed = has(armourdata::Standoff) && push != 0;
    const bool moved = has(armourdata::OffsetX) || has(armourdata::OffsetY) || has(armourdata::OffsetZ) || has(armourdata::Pitch) ||
        has(armourdata::Yaw) || has(armourdata::Roll) || has(armourdata::Scale);
    if(from >= to || (!pushed && !moved))
    {
        return;
    }
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(int i = from; i < boxTo; i++)
    {
        lo = glm::min(lo, at(s.verts, i).p);
        hi = glm::max(hi, at(s.verts, i).p);
    }
    const glm::vec3 mid = (lo + hi) * 0.5f;
    if(has(armourdata::Standoff) && push)
    {
        for(int i = from; i < to; i++)
        {
            glm::vec3 d = at(s.verts, i).p - mid;
            d = push == 1 ? d - up * glm::dot(d, up) : d;
            at(s.verts, i).p += geom::safeNormalize(d, glm::vec3{0.f}, 1e-6f) * o.v[armourdata::Standoff];
        }
    }
    const float deg = pi / 180.f;
    const float p = o.v[armourdata::Pitch] * deg, y = o.v[armourdata::Yaw] * deg, r = o.v[armourdata::Roll] * deg;
    // (yaw about up, pitch about left, roll about forward: the model's z, y, x)
    const glm::mat3 yawM{za::cos(y), za::sin(y), 0.f, -za::sin(y), za::cos(y), 0.f, 0.f, 0.f, 1.f};
    const glm::mat3 pitchM{za::cos(p), 0.f, -za::sin(p), 0.f, 1.f, 0.f, za::sin(p), 0.f, za::cos(p)};
    const glm::mat3 rollM{1.f, 0.f, 0.f, 0.f, za::cos(r), za::sin(r), 0.f, -za::sin(r), za::cos(r)};
    const glm::mat3 turn = yawM * pitchM * rollM;
    const float scale = has(armourdata::Scale) ? za::clamp(o.v[armourdata::Scale], 0.2f, 3.f) : 1.f;
    const glm::vec3 move{o.v[armourdata::OffsetX], o.v[armourdata::OffsetY], o.v[armourdata::OffsetZ]};
    for(int i = from; i < to; i++)
    {
        Vert& v = at(s.verts, i);
        v.p = mid + turn * ((v.p - mid) * scale) + move;
        v.n = turn * v.n;
    }
}

// ---- The joints' bridges (vr_agarmor_bridge.cpp makes them each frame; ROUND24.md, "Leather bridge at elbows and knees") ----
constexpr float bridgeSkinFit = 0.25f;   // units off the skin: a bridge's end where its part isn't worn
constexpr float bridgeUnderReach = 1.35f; // the skin kept under a bridge: within this many of its rows' mean radius of their line
                                          // (further out: a held gun's stock, a pad, not the limb's skin)
constexpr int bridgeJointUpper[NumJoints] = {PauldronL, PauldronR, ThighL, ThighR};

// A grid's ring's rest frame: z down the limb (the way its rings count), x its forward, y = z x x.
void endFrameOf(const PieceFrame& f, const PlateGrid& g, BridgeEnd& e)
{
    e.z = f.up * (g.hBottom >= g.hTop ? 1.f : -1.f);
    e.x = geom::safeNormalize(f.fwd - e.z * glm::dot(f.fwd, e.z), f.left, 1e-6f);
    e.y = glm::cross(e.z, e.x);
}

// A ring on the skin where ring `ring` of a piece's grid is, its corners in the grid's columns' directions just off the skin
// (none of its flat faces through it: the skin half way to each neighbour too), each carried as the skin there.
bool skinRingOf(const Body& b, const PieceFrame& f, const PlateGrid& g, int ring, int bone, BridgeEnd& e)
{
    const auto dirOf = [&](float az) { return f.fwd * za::cos(az) + f.left * za::sin(az); };
    const glm::vec2 c = at(g.axis, ring);
    glm::vec3 o = f.origin + f.up * g.hAt(ring) + f.fwd * c.x + f.left * c.y;
    float ringMean = 0.f;
    for(int col = 0; col < g.cols; col++)
    {
        ringMean += g.at2(ring, col) / static_cast<float>(g.cols);
    }
    const float most = ringMean * 1.6f + 1.f, chord = za::cos(pi / static_cast<float>(g.cols));
    // (round the limb's own middle there, as sleeveOf's rings)
    glm::vec3 mid{0.f};
    int found = 0;
    for(int k = 0; k < g.cols * 2; k++)
    {
        float t = 0.f;
        W w;
        const glm::vec3 d = dirOf(static_cast<float>(k) * pi / static_cast<float>(g.cols));
        if(bodyExit(b, o, d, most, t, w))
        {
            mid += o + d * t;
            found++;
        }
    }
    if(found >= g.cols)
    {
        mid /= static_cast<float>(found);
        o = mid - f.up * glm::dot(mid - o, f.up);
    }
    for(int col = 0; col < bridgeCols; col++)
    {
        const float az = g.azOf(col);
        const glm::vec3 d = dirOf(az);
        float t = 0.f;
        W w;
        if(!bodyExit(b, o, d, most, t, w))
        {
            t = ringMean * 0.8f;
            w = W{};
            w.w[bone] = 1.f;
        }
        float r = t;
        for(const float side : {-1.f, 1.f})
        {
            float tm = 0.f;
            W wm;
            r = bodyExit(b, o, dirOf(az + side * pi / static_cast<float>(g.cols)), most, tm, wm) ? za::max(r, tm / chord) : r;
        }
        topFour(w);
        e.ring[col] = skinnedOf(o + d * (r + bridgeSkinFit), w);
    }
    endFrameOf(f, g, e);
    e.bone = bone;
    e.ok = true;
    return true;
}

// Each joint's bridge from its two parts' grids (the plates' end rings already captured in `rows`), the skin under it and
// armour.txt's settings on its sleeve parts.
void bridgesOf(Shell& s, const rig2::Rig& rig, const PieceFrame* frames, const PlateGrid* grids, const armourdata::Override* sleeveTweak,
    const BridgeEnd* rows)
{
    const auto tweaked = [](const armourdata::Override& o, int field) { return ((o.has >> static_cast<unsigned>(field)) & 1u) != 0u; };
    s.bridgeVerts = 0;
    for(int j = 0; j < NumJoints; j++)
    {
        Bridge& br = s.bridges[j];
        br = Bridge{};
        const int up = bridgeJointUpper[j], lo = up + 2;
        br.upper = up;
        br.lower = lo;
        const armourdata::Override& tu = sleeveTweak[up];
        const armourdata::Override& tl = sleeveTweak[lo];
        // (armour.txt's `leather "0"` on either sleeve part: the joint's leather not made)
        const bool off = (tweaked(tu, armourdata::Leather) && tu.v[armourdata::Leather] < 0.5f) ||
            (tweaked(tl, armourdata::Leather) && tl.v[armourdata::Leather] < 0.5f);
        const int bu = s.tubes[up].bone, bl = s.tubes[lo].bone;
        if(off || !rows[up].ok || !rows[lo].ok || bu < 0 || bl < 0)
        {
            continue;
        }
        br.plates[0] = rows[up];
        br.plates[1] = rows[lo];
        br.pivot = rig.bones[bl].pivot;
        if(!skinRingOf(s.body, frames[up], grids[up], grids[up].rows, bu, br.skin[0]) ||
           !skinRingOf(s.body, frames[lo], grids[lo], 0, bl, br.skin[1]))
        {
            continue;
        }
        // One rest frame for both ends (the upper's forward, square to each end's direction: no twist between them at rest), and
        // the lower ends' corners in the upper's order round it (the parts' grids' columns needn't go round alike: lerped
        // corner by corner, a mismatched pair crossed the middle and the tube pinched shut there).
        const glm::vec3 x0 = br.plates[0].x;
        const glm::vec3 x1 = geom::safeNormalize(x0 - br.plates[1].z * glm::dot(x0, br.plates[1].z), br.plates[1].x, 1e-6f);
        for(BridgeEnd* e : {&br.plates[0], &br.skin[0], &br.plates[1], &br.skin[1]})
        {
            e->x = e == &br.plates[0] || e == &br.skin[0] ? x0 : x1;
            e->y = glm::cross(e->z, e->x);
        }
        float az[2][bridgeCols];
        for(int e = 0; e < 2; e++)
        {
            glm::vec3 m{0.f};
            for(const Skinned& c : br.plates[e].ring)
            {
                m += c.p / static_cast<float>(bridgeCols);
            }
            for(int c = 0; c < bridgeCols; c++)
            {
                const glm::vec3 d = br.plates[e].ring[c].p - m;
                az[e][c] = za::atan2(glm::dot(d, br.plates[e].y), glm::dot(d, br.plates[e].x));
            }
        }
        int bestShift = 0, bestDir = 1;
        float bestCost = 1e9f;
        for(const int dir : {1, -1})
        {
            for(int shift = 0; shift < bridgeCols; shift++)
            {
                float cost = 0.f;
                for(int c = 0; c < bridgeCols; c++)
                {
                    const float d = az[0][c] - az[1][(shift + dir * c + 2 * bridgeCols) % bridgeCols];
                    const float w = za::atan2(za::sin(d), za::cos(d));
                    cost += w * w;
                }
                if(cost < bestCost)
                {
                    bestCost = cost;
                    bestShift = shift;
                    bestDir = dir;
                }
            }
        }
        for(BridgeEnd* e : {&br.plates[1], &br.skin[1]})
        {
            Skinned was[bridgeCols];
            for(int c = 0; c < bridgeCols; c++)
            {
                was[c] = e->ring[c];
            }
            for(int c = 0; c < bridgeCols; c++)
            {
                e->ring[c] = was[(bestShift + bestDir * c + 2 * bridgeCols) % bridgeCols];
            }
        }
        // (the upper sleeve's value, else the lower's, else the global setting)
        const auto pick = [&](int field, float global) {
            return tweaked(tu, field) ? tu.v[field] : tweaked(tl, field) ? tl.v[field] : global;
        };
        br.folds = za::clamp(static_cast<int>(pick(armourdata::Folds, vr_agarmor_folds.value) + 0.5f), 0, bridgeMostFolds);
        br.rings = br.folds >= 3 ? 2 * br.folds + 1 : br.folds == 2 ? 9 : 7;
        br.depth = za::clamp(pick(armourdata::FoldDepth, vr_agarmor_fold_depth.value), 0.f, 2.f);
        br.standoff = pick(armourdata::Standoff, 0.f);
        br.scale = za::clamp(pick(armourdata::Scale, 1.f), 0.2f, 3.f);
        br.taper[0] = tweaked(tu, armourdata::TaperStart) ? za::clamp(tu.v[armourdata::TaperStart], 0.2f, 3.f) : 1.f;
        br.taper[1] = tweaked(tu, armourdata::TaperEnd) ? za::clamp(tu.v[armourdata::TaperEnd], 0.2f, 3.f) : 1.f;
        br.taper[2] = tweaked(tl, armourdata::TaperStart) ? za::clamp(tl.v[armourdata::TaperStart], 0.2f, 3.f) : 1.f;
        br.taper[3] = tweaked(tl, armourdata::TaperEnd) ? za::clamp(tl.v[armourdata::TaperEnd], 0.2f, 3.f) : 1.f;
        // The skin between the two rows (on the limb's two bones, near their line): kept under the leather at any bend.
        glm::vec3 p0{0.f}, p1{0.f};
        float mean = 0.f;
        for(int c = 0; c < bridgeCols; c++)
        {
            p0 += br.plates[0].ring[c].p / static_cast<float>(bridgeCols);
            p1 += br.plates[1].ring[c].p / static_cast<float>(bridgeCols);
        }
        for(int c = 0; c < bridgeCols; c++)
        {
            mean += (glm::distance(br.plates[0].ring[c].p, p0) + glm::distance(br.plates[1].ring[c].p, p1)) / (2.f * bridgeCols);
        }
        // (between: nearest the limb's line from the one row to the joint and on to the other at neither end; a limb bent at
        // rest has no room between the rows' planes: a knight's sword arm)
        const glm::vec3 joint = br.pivot;
        const auto between = [&](const glm::vec3& p, float& dist) {
            const glm::vec3 e1 = joint - p0, e2 = p1 - joint;
            const float u1 = glm::dot(p - p0, e1) / za::max(glm::dot(e1, e1), 1e-6f), u2 = glm::dot(p - joint, e2) / za::max(glm::dot(e2, e2), 1e-6f);
            const float d1 = glm::distance(p, p0 + e1 * za::clamp(u1, 0.f, 1.f)), d2 = glm::distance(p, joint + e2 * za::clamp(u2, 0.f, 1.f));
            dist = za::min(d1, d2);
            return d1 <= d2 ? u1 > 0.f : u2 < 1.f;
        };
        int candidates = 0;
        for(int pass = 0; pass < 2; pass++)
        {
            const int stride = pass == 0 ? 1 : za::max(1, (candidates + bridgeMostUnder - 1) / bridgeMostUnder);
            int seen = 0;
            for(int v = 0; v < count(s.body.p); v++)
            {
                const glm::vec3 p = at(s.body.p, v);
                const W& w = at(s.body.w, v);
                const float onLimb = w.w[bu] + w.w[bl];
                float dist = 0.f;
                if(onLimb < 0.5f || !between(p, dist) || dist > mean * bridgeUnderReach)
                {
                    continue;
                }
                if(pass == 0)
                {
                    candidates++;
                    continue;
                }
                if(seen++ % stride == 0 && br.numUnder < bridgeMostUnder)
                {
                    W tw = w;
                    topFour(tw);
                    // (its place along the line from the one row to the joint and on to the other, by length)
                    const glm::vec3 e1 = joint - p0, e2 = p1 - joint;
                    const float l1 = glm::length(e1), l2 = glm::length(e2);
                    const float u1 = za::clamp(glm::dot(p - p0, e1) / za::max(l1 * l1, 1e-6f), 0.f, 1.f);
                    const float u2 = za::clamp(glm::dot(p - joint, e2) / za::max(l2 * l2, 1e-6f), 0.f, 1.f);
                    const bool first = glm::distance(p, p0 + e1 * u1) <= glm::distance(p, joint + e2 * u2);
                    br.underAlong[br.numUnder] = (first ? u1 * l1 : l1 + u2 * l2) / za::max(l1 + l2, 1e-6f);
                    br.under[br.numUnder++] = skinnedOf(p, tw);
                }
            }
        }
        br.first = s.bridgeVerts;
        s.bridgeVerts += br.rings * (bridgeCols + 1);
        br.ok = true;
    }
    bridgeRest(s);
}

} // namespace

namespace detail
{

const rig2::Rig* rigOfShell(qmodel_t* src)
{
    return src ? rigOf(src) : nullptr;
}

uint32_t pieceRegionBones(const rig2::Rig& rig, int piece)
{
    return frameOf(rig, piece).bones;
}

void pieceAxes(const rig2::Rig& rig, int piece, glm::vec3& origin, glm::vec3& up, glm::vec3& fwd, glm::vec3& left)
{
    const PieceFrame f = frameOf(rig, piece);
    origin = f.origin;
    up = f.up;
    fwd = f.fwd;
    left = f.left;
}

bool buildShell(qmodel_t* src, Shell& s)
{
    const double t0 = Sys_DoubleTime();
    const rig2::Rig* rig = src ? rigOf(src) : nullptr;
    const auto* sh = src && src->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(src)) : nullptr;
    s.verts.clear();
    s.tris.clear();
    s.ok = false;
    s.numSlots = 0;
    const bool skeletal = rig && sh && isPlayerBody(src->name) && sh->poseverttype == aliashdr_t::PV_IQM;
    if(!rig || !sh || (!skeletal && sh->poseverttype != aliashdr_t::PV_QUAKE1) ||
       !(skeletal ? buildSkeletalBody(sh, *rig, s.body) : buildBody(src, sh, *rig, s.body)))
    {
        return false;
    }
    s.generation = rig->generation;
    s.tweaks = armourdata::signature(src->name);
    s.tweaksGen = armourdata::generation();
    // (armour.txt's tweaks: each piece's plates' and its sleeve's; nothing set, nothing applied)
    armourdata::Override tweak[NumPieces], sleeveTweak[NumPieces];
    for(int piece = 0; piece < NumPieces; piece++)
    {
        (void)armourdata::resolve(src->name, piece, false, tweak[piece]);
        (void)armourdata::resolve(src->name, piece, true, sleeveTweak[piece]);
    }
    const auto tweaked = [](const armourdata::Override& o, int field) { return (o.has >> static_cast<unsigned>(field)) & 1u; };
    Local locals[NumPieces];
    PieceFrame frames[NumPieces];
    za::Vector<uint8_t> overs[NumPieces];
    za::Vector<glm::vec3> fringes[NumPieces];
    PlateGrid grids[NumPieces];
    // (the joints' leather: a bridge made each frame (vr_agarmor_bridge 1), else each limb part's sleeve made here)
    const bool bridged = vr_agarmor_bridge.value != 0.f;
    BridgeEnd rows[NumPieces];
    for(int piece = 0; piece < NumPieces; piece++)
    {
        PieceFrame& f = frames[piece];
        f = frameOf(*rig, piece);
        f.shiftStart = tweaked(tweak[piece], armourdata::Start) ? tweak[piece].v[armourdata::Start] : 0.f;
        f.shiftEnd = tweaked(tweak[piece], armourdata::End) ? tweak[piece].v[armourdata::End] : 0.f;
        f.rows = tweaked(tweak[piece], armourdata::Rows) ? static_cast<int>(tweak[piece].v[armourdata::Rows] + 0.5f) : 0;
        f.hero = vr_agarmor_hero.value != 0.f && tweak[piece].v[armourdata::Hero] >= 0.5f;
        f.backCut = backCutOf(src->name, piece);
        f.yoke = piece == Cuirass && !skeletal ? wearerOf(src->name).yoke : -1.f;
        if(!skeletal && f.ok && (piece == PauldronL || piece == PauldronR))
        {
            const Wearer who = wearerOf(src->name);
            f.topMost = who.shoulderTop;
            const PieceFrame l = frameOf(*rig, PauldronL), r = frameOf(*rig, PauldronR);
            if(who.shoulderAxis && l.ok && r.ok)
            {
                const glm::vec3 m = geom::safeNormalize(l.up + glm::vec3{r.up.x, -r.up.y, r.up.z}, l.up, 1e-6f);
                f.up = piece == PauldronL ? m : glm::vec3{m.x, -m.y, m.z};
                f.fwd = geom::safeNormalize(glm::vec3{1.f, 0.f, 0.f} - f.up * f.up.x, glm::vec3{1.f, 0.f, 0.f}, 1e-6f);
                f.left = glm::cross(f.up, f.fwd);
            }
        }
        f.cut = regionCut(s.body.w, f.bones);
        f.ends = !skeletal && limbBone[piece] != nullptr;
        f.ridge = !f.ends || piece == PauldronL || piece == PauldronR ? plateRidge : piece == ThighL || piece == ThighR ? plateRidgeThigh : plateRidgeLimb;
        f.ridge = f.ends && (piece == PauldronL || piece == PauldronR) && wearerOf(src->name).shoulderRidge >= 0.f ? wearerOf(src->name).shoulderRidge : f.ridge;        f.hip = f.ends && (piece == ThighL || piece == ThighR);
        f.hipFrom = wearerOf(src->name).hipFrom;
        f.cap = f.ends && (piece == PauldronL || piece == PauldronR) ? wearerOf(src->name).shoulderCap : -1.f;
        f.upper = piece == PauldronL || piece == PauldronR || piece == ThighL || piece == ThighR;
        f.body = &s.body;
        if(f.ok && (piece == PauldronL || piece == PauldronR))
        {
            girdleShells(s.body, *rig, rig2::boneByName(*rig, limbBone[piece]), overs[piece]);
            f.over = &overs[piece];
        }
        if(f.ok && piece != Helmet)
        {
            Local& L = locals[piece];
            clipPiece(s.body, f, L);
            // (a limb's plate is small: not subdivided, a quarter of the triangles)
            for(int i = 0; i < (limbBone[piece] ? 0 : za::clamp(static_cast<int>(vr_agarmor_subdiv.value), 0, 2)); i++)
            {
                subdivide(L);
            }
            smooth(L, za::clamp(static_cast<int>(vr_agarmor_smooth.value), 0, 50));
            if(!L.tris.empty())
            {
                planGrid(L, f, piece, grids[piece]);
            }
            // (the body's vertices between its region and the next: plateFringe)
            for(int v = 0; v < count(s.body.p); v++)
            {
                float region = 0.f;
                for(int i = 0; i < rig2::maxBones; i++)
                {
                    region += ((f.bones >> static_cast<unsigned>(i)) & 1u) ? at(s.body.w, v).w[i] : 0.f;
                }
                if(region >= plateFringe && region <= f.cut && at(s.body.p, v).x >= f.backCut)
                {
                    fringes[piece].pushBack(at(s.body.p, v));
                }
            }
        }
    }
    // A humanoid's upper arms start as high as the higher of the two: a knight's right upper arm weighs on its bone only from 1.4
    // down it (his left from -0.9), so its plates started 1.7 lower and its red shoulder pad showed (his note, 2026-11-06). The
    // reach itself unchanged: raising it on every humanoid flared the enforcer's pauldrons into shells.
    if(frames[PauldronL].ends && grids[PauldronL].rows && grids[PauldronR].rows)
    {
        // (before armour.txt's `start`: planGrid adds each side's own again)
        const float top = za::min(grids[PauldronL].hTopRaw, grids[PauldronR].hTopRaw);
        for(const int piece : {PauldronL, PauldronR})
        {
            if(grids[piece].hTopRaw > top + 0.05f)
            {
                frames[piece].topAt = top;
                planGrid(locals[piece], frames[piece], piece, grids[piece]);
            }
        }
    }
    // The left and right limbs: the same rows and columns on each, each fitted to its own limb, mirrored where they're alike (mirrorRadii).
    for(int piece = PauldronL; piece + 1 < NumPieces; piece += 2)
    {
        PlateGrid& a = grids[piece];
        PlateGrid& b = grids[piece + 1];
        if(a.rows && b.rows && a.rows != b.rows)
        {
            const int rows = za::max(a.rows, b.rows);
            for(PlateGrid* g : {&a, &b})
            {
                const int have = g->rows;
                za::Vector<glm::vec2> axis(static_cast<za::SizeT>(rows + 1), glm::vec2{0.f});
                for(int ring = 0; ring <= rows; ring++)
                {
                    const float x = static_cast<float>(ring) * static_cast<float>(have) / static_cast<float>(rows);
                    const int low = za::min(static_cast<int>(x), have - 1);
                    at(axis, ring) = at(g->axis, low) + (at(g->axis, low + 1) - at(g->axis, low)) * (x - static_cast<float>(low));
                }
                g->axis = axis;
                g->rows = rows;
                g->radius.clear();
                g->radius.resize(static_cast<za::SizeT>((rows + 1) * g->cols), 0.f);
            }
        }
    }
    for(int piece = 0; piece < NumPieces; piece++)
    {
        if(grids[piece].rows)
        {
            fillRadii(locals[piece], fringes[piece], frames[piece], grids[piece]);
        }
    }
    if(grids[Cuirass].rows)
    {
        closeYoke(s.body, *rig, frames[Cuirass], grids[Cuirass]);
        mirrorTorso(grids[Cuirass]);
    }
    for(int piece = PauldronL; piece + 1 < NumPieces; piece += 2)
    {
        if(grids[piece].rows && grids[piece + 1].rows)
        {
            // (a knight's: the larger, so the pad's side's push isn't halved)
            mirrorRadii(grids[piece], grids[piece + 1], skeletal ? Mirror::Whole : piece != PauldronL ? Mirror::Corners : wearerOf(src->name).shoulderRidge >= 0.f ? Mirror::ShouldersMost : Mirror::Shoulders,
                piece == ThighL ? za::min(grids[piece].rows, grids[piece + 1].rows) : piece == BracerL || piece == ShinL ? 0 : -1);
        }
    }
    // (armour.txt's stand-off: every corner further off the skin)
    for(int piece = 0; piece < NumPieces; piece++)
    {
        for(float& r : grids[piece].radius)
        {
            r = tweaked(tweak[piece], armourdata::Standoff) ? za::max(r + tweak[piece].v[armourdata::Standoff], 0.05f) : r;
        }
        // (armour.txt's taper: each ring's radii scaled, its first row's taper_start lerped to its last row's taper_end)
        PlateGrid& g = grids[piece];
        if((tweaked(tweak[piece], armourdata::TaperStart) || tweaked(tweak[piece], armourdata::TaperEnd)) && g.rows > 0)
        {
            const float a = za::clamp(tweak[piece].v[armourdata::TaperStart], 0.2f, 3.f);
            const float b = za::clamp(tweak[piece].v[armourdata::TaperEnd], 0.2f, 3.f);
            for(int ring = 0; ring <= g.rows; ring++)
            {
                const float k = a + (b - a) * static_cast<float>(ring) / static_cast<float>(g.rows);
                for(int col = 0; col < g.cols; col++)
                {
                    g.at2(ring, col) = za::max(g.at2(ring, col) * k, 0.05f);
                }
            }
        }
    }
    for(int piece = 0; piece < NumPieces; piece++)
    {
        const PieceFrame& f = frames[piece];
        const int firstVert = count(s.verts), firstTri = count(s.tris);
        int sleeveVert = -1, ringVert = -1;
        s.tubes[piece] = PlateTube{};
        s.tubes[piece].grid = grids[piece];
        s.tubes[piece].origin = f.origin;
        s.tubes[piece].up = f.up;
        s.tubes[piece].fwd = f.fwd;
        s.tubes[piece].left = f.left;
        s.tubes[piece].length = f.length;
        s.tubes[piece].bone = limbBone[piece] ? rig2::boneByName(*rig, limbBone[piece]) : -1;
        if(f.ok && piece == Helmet)
        {
            helmetOf(s, s.body, f, rig2::boneByName(*rig, "head"), wearerOf(src->name));
        }
        else if(grids[piece].rows)
        {
            platesOf(s, locals[piece], f, grids[piece], piece, plateSeed(src->name));
            // (a joint's bridge: this part's end ring at the joint, its plates' feet there, weighted as they are; moved by
            // armour.txt's tweaks with the plates, then kept apart: bridgesOf)
            const PlateGrid& g = grids[piece];
            if(bridged && piece >= PauldronL && g.cols == bridgeCols && g.rows >= 1)
            {
                ringVert = count(s.verts);
                const int ring = f.upper ? g.rows : 0;
                const glm::vec2 c = at(g.axis, ring);
                for(int col = 0; col < bridgeCols; col++)
                {
                    const float az = g.azOf(col);
                    Vert v;
                    v.p = f.origin + f.up * g.hAt(ring) + f.fwd * c.x + f.left * c.y + (f.fwd * za::cos(az) + f.left * za::sin(az)) * (g.at2(ring, col) + plateFit);
                    v.w = plateWeights(locals[piece], v.p);
                    v.piece = piece;
                    s.verts.pushBack(v);
                }
            }
            // (armour.txt's `leather "0"` on a sleeve part: that sleeve not made at all)
            const bool leatherOff = tweaked(sleeveTweak[piece], armourdata::Leather) && sleeveTweak[piece].v[armourdata::Leather] < 0.5f;
            if(!bridged && piece >= PauldronL && grids[piece].cols == 6 && grids[piece].rows >= 2 && !leatherOff)
            {
                sleeveVert = count(s.verts);
                // (its taper: as the plates', clamped 0.2 .. 3)
                const armourdata::Override& st = sleeveTweak[piece];
                const float ta = tweaked(st, armourdata::TaperStart) ? za::clamp(st.v[armourdata::TaperStart], 0.2f, 3.f) : 1.f;
                const float tb = tweaked(st, armourdata::TaperEnd) ? za::clamp(st.v[armourdata::TaperEnd], 0.2f, 3.f) : 1.f;
                sleeveOf(s, s.body, f, grids[piece], piece, ta, tb);
            }
        }
        // (armour.txt's tweaks on what was made: the helmet pushed out round its middle, a sleeve round its bone's line; then
        // each moved, turned and scaled round its middle)
        const int plateEnd = sleeveVert >= 0 ? sleeveVert : count(s.verts);
        tweakVerts(s, firstVert, plateEnd, tweak[piece], piece == Helmet ? 2 : 0, f.up, ringVert >= 0 ? ringVert : plateEnd);
        if(sleeveVert >= 0)
        {
            tweakVerts(s, sleeveVert, count(s.verts), sleeveTweak[piece], 1, f.up);
        }
        if(ringVert >= 0)
        {
            BridgeEnd& e = rows[piece];
            for(int col = 0; col < bridgeCols; col++)
            {
                W w = at(s.verts, ringVert + col).w;
                topFour(w);
                e.ring[col] = skinnedOf(at(s.verts, ringVert + col).p, w);
            }
            endFrameOf(f, grids[piece], e);
            e.bone = s.tubes[piece].bone;
            e.ok = e.bone >= 0;
            s.verts.resize(static_cast<za::SizeT>(ringVert));
        }
        s.pieceVerts[piece] = count(s.verts) - firstVert;
        s.pieceTris[piece] = (count(s.tris) - firstTri) / 3;
    }
    for(Bridge& br : s.bridges)
    {
        br = Bridge{};
    }
    s.bridgeVerts = 0;
    s.bridgeRestP.clear();
    s.bridgeRestN.clear();
    s.bridgeTris.clear();
    if(bridged)
    {
        bridgesOf(s, *rig, frames, grids, sleeveTweak, rows);
    }
    // Slots: each piece's bones its own.
    int slotOf[NumPieces][rig2::maxBones];
    for(auto& row : slotOf)
    {
        for(int& x : row)
        {
            x = -1;
        }
    }
    for(Vert& v : s.verts)
    {
        topFour(v.w);
        for(int b = 0; b < rig2::maxBones; b++)
        {
            if(v.w.w[b] > 0.f && slotOf[v.piece][b] < 0 && s.numSlots < maxSlots)
            {
                slotOf[v.piece][b] = s.numSlots;
                s.slotBone[s.numSlots] = b;
                s.slotPiece[s.numSlots] = v.piece;
                s.numSlots++;
            }
        }
    }
    uint32_t h = 2166136261u;
    for(const Vert& v : s.verts)
    {
        h = fnv(h, &v.p, sizeof(v.p));
        h = fnv(h, &v.n, sizeof(v.n));
        h = fnv(h, &v.st, sizeof(v.st));
        h = fnv(h, v.w.w, sizeof(v.w.w));
    }
    h = fnv(h, s.tris.data(), sizeof(int) * s.tris.size());
    for(const glm::vec3& p : s.bridgeRestP) // (the joints' leather: its rest pose and settings)
    {
        h = fnv(h, &p, sizeof(p));
    }
    for(const Bridge& br : s.bridges)
    {
        const float k[11] = {br.ok ? 1.f : 0.f, static_cast<float>(br.folds), br.depth, br.standoff, br.scale, br.taper[0], br.taper[1],
            br.taper[2], br.taper[3], static_cast<float>(br.rings), static_cast<float>(br.numUnder)};
        h = fnv(h, k, sizeof(k));
    }
    s.hash = h;
    s.ok = !s.tris.empty();
    s.made = static_cast<float>((Sys_DoubleTime() - t0) * 1000.0);
    return s.ok;
}

} // namespace detail

namespace
{

// ---- Who wears what ----
struct Named
{
    int num{0}, type{0}, pieces{0};
};
za::Vector<Named> named; // vr_agarmor_set

za::Vector<Shell> shells; // made this session (per model)

// The Armour Studio's subject (studioView): its entity, its body's alpha, the pieces outlined (a mask) and whether their sleeves.
struct StudioView
{
    int ent{0};
    float alpha{1.f};
    int pieces{0};
    bool sleeve{false};
};
StudioView studio;

entity_t worn[maxWorn];
int wornSource[maxWorn]{}, wornPieces[maxWorn]{}, wornSlots[maxWorn]{};
int wornShell[maxWorn]{}; // (shells' index: a shell made meanwhile moves them)
int numWorn = 0;
float wornBones[maxWorn][maxSlots * 12]{};

struct Kind
{
    const qmodel_t* skinned{nullptr};
    char name[MAX_QPATH]{};
    qmodel_t* armor{nullptr};
    char armorName[MAX_QPATH]{}; // (what `armor` was loaded as: its slot may be emptied and given to another model since)
    qmodel_t* bridge{nullptr};    // its joints' leather ("<model>#bridge", asked for when first drawn; vr_agarmor_bridge.cpp)
    char bridgeName[MAX_QPATH]{};
    bool bridgeAsked{false};
};
za::Vector<Kind> kinds;

// The joints' leather drawn this frame: an entity per armoured body (relink), its bones made when first asked (bonePoses).
constexpr int maxBridges = 64;
constexpr const char* bridgeSuffix = "#bridge";
entity_t bridgeEnts[maxBridges];
int bridgeSource[maxBridges]{}, bridgeShell[maxBridges]{}, bridgePieces[maxBridges]{};
bool bridgeMade[maxBridges]{};
int numBridges = 0;
float bridgeBonesOf[maxBridges][bridgeMostVerts * 12]{};
// (the player's body's: vr_view_body.cpp's entity, made once a frame)
float playerBridgeBones[bridgeMostVerts * 12]{};
int playerBridgeFrame = -1, playerBridgePieces = -1;
float bridgeKey[6]{-1.f, -1.f, -1.f, -1.f, -1.f, -1.f}; // (vr_agarmor_bridge, _folds, _fold_depth, _details, _emblem, _hero as the shells were made with)

// Whether a kind's armour is still the model it found: a campaign switch (Mod_ResetAll) empties the slots of the models
// it does not keep, the "#armor" ones among them, and gives them to the next models loaded (a body it keeps finds its
// kind again: the plates drawn as a gib, a fish, a door).
[[nodiscard]] bool armorLive(const Kind& k)
{
    return k.armor && !k.armor->needload && k.armor->type == mod_alias && !strcmp(k.armor->name, k.armorName);
}
[[nodiscard]] bool bridgeLive(const Kind& k)
{
    return k.bridge && !k.bridge->needload && k.bridge->type == mod_alias && !strcmp(k.bridge->name, k.bridgeName);
}

[[nodiscard]] bool isWearer(const char* base)
{
    for(const Wearer& w : wearers)
    {
        if(!strcmp(base, w.model))
        {
            return true;
        }
    }
    return false;
}

} // namespace

namespace detail
{

int wornCount()
{
    return numWorn;
}

const Shell* wornAt(int k, int& entnum)
{
    entnum = wornSource[k];
    return wornShell[k] < count(shells) ? &at(shells, wornShell[k]) : nullptr;
}

[[nodiscard]] Shell* shellFor(qmodel_t* src)
{
    const rig2::Rig* rig = src ? rigOf(src) : nullptr;
    if(!rig)
    {
        return nullptr;
    }
    for(Shell& s : shells)
    {
        if(!strcmp(s.model.cStr(), src->name))
        {
            // (armour.txt changed since: made again only if its tweaks for this model did)
            if(s.tweaksGen != armourdata::generation() && s.generation == rig->generation)
            {
                s.tweaksGen = armourdata::generation();
                s.generation = s.tweaks != armourdata::signature(src->name) ? -1 : s.generation;
            }
            if(s.generation != rig->generation)
            {
                (void)buildShell(src, s);
            }
            return s.ok ? &s : nullptr;
        }
    }
    shells.pushBack(Shell{});
    Shell& s = shells.back();
    s.model = za::String{src->name};
    (void)buildShell(src, s);
    return s.ok ? &s : nullptr;
}

} // namespace detail

namespace
{

[[nodiscard]] int shellNamed(const char* model)
{
    for(int i = 0; i < count(shells); i++)
    {
        if(at(shells, i).ok && !strcmp(at(shells, i).model.cStr(), model))
        {
            return i;
        }
    }
    return -1;
}

// The armour model drawn over a body drawn as `skinned` ("<model>#rig2" of a wearer), else nullptr.
[[nodiscard]] qmodel_t* armorFor(const qmodel_t* skinned)
{
    for(const Kind& k : kinds)
    {
        if(k.skinned == skinned && !strcmp(k.name, skinned->name) && (!k.armor || armorLive(k)))
        {
            return k.armor;
        }
    }
    Kind k;
    k.skinned = skinned;
    q_strlcpy(k.name, skinned->name, sizeof(k.name));
    const size_t len = strlen(skinned->name), suffix = strlen(rigSuffix);
    const bool rigged = len > suffix && !strcmp(skinned->name + len - suffix, rigSuffix);
    if(rigged || isPlayerBody(skinned->name)) // (the player's body is drawn under its own name)
    {
        char base[MAX_QPATH];
        q_strlcpy(base, skinned->name, za::min(sizeof(base), (rigged ? len - suffix : len) + 1));
        if(isWearer(base))
        {
            char name[MAX_QPATH];
            q_snprintf(name, sizeof(name), "%s%s", base, armorSuffix);
            k.armor = Mod_ForName(name, false);
            k.armor = k.armor && k.armor->type == mod_alias ? k.armor : nullptr;
            q_strlcpy(k.armorName, name, sizeof(k.armorName));
        }
    }
    for(Kind& old : kinds)
    {
        if(old.skinned == skinned)
        {
            old = k;
            return k.armor;
        }
    }
    kinds.pushBack(k);
    return k.armor;
}

// The joints' leather's model over a body drawn as `skinned` (its armour's kind found first: armorFor), else nullptr.
[[nodiscard]] qmodel_t* bridgeFor(const qmodel_t* skinned)
{
    for(Kind& k : kinds)
    {
        if(k.skinned != skinned || strcmp(k.name, skinned->name) || !armorLive(k))
        {
            continue;
        }
        if(!k.bridgeAsked)
        {
            k.bridgeAsked = true;
            const size_t len = strlen(k.armorName), suffix = strlen(armorSuffix);
            char name[MAX_QPATH];
            q_strlcpy(name, k.armorName, za::min(sizeof(name), len - suffix + 1));
            q_strlcat(name, bridgeSuffix, sizeof(name));
            k.bridge = Mod_ForName(name, false);
            k.bridge = k.bridge && k.bridge->type == mod_alias ? k.bridge : nullptr;
            q_strlcpy(k.bridgeName, name, sizeof(k.bridgeName));
        }
        return bridgeLive(k) ? k.bridge : nullptr;
    }
    return nullptr;
}

// An entity's armour in a word (the server's, the client's): its type (bits 30-31), its worn pieces' wear (bits 20-29
// at a third of their pool or under, 10-19 at two thirds or under; P2's damage marks), its pieces (0-9).
[[nodiscard]] uint32_t packState(int type, int pieces, int wear)
{
    pieces &= allPieces;
    wear &= pieces | pieces << NumPieces;
    return type > 0 && pieces ? static_cast<uint32_t>(type) << 30u | static_cast<uint32_t>(wear) << 10u | static_cast<uint32_t>(pieces) : 0u;
}
[[nodiscard]] int stateType(uint32_t s)
{
    return static_cast<int>(s >> 30u);
}
[[nodiscard]] int statePieces(uint32_t s)
{
    return static_cast<int>(s & static_cast<uint32_t>(allPieces));
}
[[nodiscard]] int stateWear(uint32_t s)
{
    return static_cast<int>((s >> 10u) & 0xfffffu);
}

// The server's word on each entity (QVR_SVC_ARMOR: packState); and how many are armoured.
za::Vector<uint32_t> served;
int numServed = 0;

// Who wears what: vr_agarmor_set's entities, else vr_agarmor (debug: everyone, forced), else the server's word; `wear`
// its pieces' wear (packState's bits 10-29, shifted down: the server's word only, or vr_agarmor_wear's debug look).
void wornBy(int num, int& type, int& pieces, int& wear)
{
    type = za::clamp(static_cast<int>(vr_agarmor.value), 0, 4);
    type = type == 4 ? 1 + num % 3 : type;
    pieces = static_cast<int>(vr_agarmor_pieces.value) & allPieces;
    const int forced = za::clamp(static_cast<int>(vr_agarmor_wear.value), 0, 2);
    wear = forced == 0 ? 0 : forced == 1 ? allPieces : allPieces << NumPieces | allPieces;
    if(type == 0 && num >= 0 && num < count(served))
    {
        type = stateType(at(served, num));
        pieces = statePieces(at(served, num));
        wear = forced ? wear : stateWear(at(served, num));
    }
    for(const Named& n : named)
    {
        if(n.num == num)
        {
            type = n.type;
            pieces = n.pieces;
        }
    }
}

void forgetModels()
{
    // (the models made from the shells are made again when next drawn)
    for(Kind& k : kinds)
    {
        if(armorLive(k) && Cache_Check(&k.armor->cache)) // (a slot given to another model since: not its cache)
        {
            Cache_Free(&k.armor->cache, false);
        }
        if(bridgeLive(k) && Cache_Check(&k.bridge->cache))
        {
            Cache_Free(&k.bridge->cache, false);
        }
    }
    kinds.clear();
}

// ---- Commands ----
// vr_agarmor_info <model>: the fit of each piece of the models whose name has it: the region's extent up its bone against the
// plates' rows and the bone's length, each ring's mean radius against the skin's there, how far the left's outer face is from the
// right's mirrored about the body's middle (y 0: what the eye sees, rest poses and all), and the rig's bones.
void fitInfo(qmodel_t* src, const Shell& s)
{
    const rig2::Rig* rig = rigOf(src);
    if(!rig)
    {
        return;
    }
    for(int b = 0; b < rig->numBones; b++)
    {
        const rig2::Bone& bone = rig->bones[b];
        Con_Printf("agarmor:   bone %d %s (parent %d): pivot %.1f %.1f %.1f, end %.1f %.1f %.1f\n", b, bone.name, bone.parent, static_cast<double>(bone.pivot.x),
            static_cast<double>(bone.pivot.y), static_cast<double>(bone.pivot.z), static_cast<double>(bone.end.x), static_cast<double>(bone.end.y), static_cast<double>(bone.end.z));
    }
    for(int p = 0; p < NumPieces; p++)
    {
        const PlateTube& t = s.tubes[p];
        const PlateGrid& g = t.grid;
        PieceFrame f = frameOf(*rig, p);
        const float cut = regionCut(s.body.w, f.bones);
        float lo = 1e9f, hi = -1e9f;
        for(int v = 0; v < count(s.body.p); v++)
        {
            float region = 0.f;
            for(int i = 0; i < rig2::maxBones; i++)
            {
                region += ((f.bones >> static_cast<unsigned>(i)) & 1u) ? at(s.body.w, v).w[i] : 0.f;
            }
            if(region >= cut)
            {
                const float h = glm::dot(at(s.body.p, v) - f.origin, f.up);
                lo = za::min(lo, h);
                hi = za::max(hi, h);
            }
        }
        char rings[512] = "";
        for(int r = 0; r <= g.rows && g.cols > 0; r++)
        {
            float mean = 0.f;
            for(int c = 0; c < g.cols; c++)
            {
                mean += g.at2(r, c) / static_cast<float>(g.cols);
            }
            q_strlcat(rings, va(" %.1f", static_cast<double>(mean)), sizeof(rings));
        }
        float mirror = 0.f;
        int n = 0;
        const int other = p >= PauldronL ? (p - PauldronL) % 2 ? p - 1 : p + 1 : p;
        for(const Vert& v : s.verts)
        {
            if(v.piece != p || v.layer != 0 || p == Helmet)
            {
                continue;
            }
            const glm::vec3 want{v.p.x, -v.p.y, v.p.z};
            float best = 1e9f;
            for(const Vert& u : s.verts)
            {
                best = u.piece == other && u.layer == 0 ? za::min(best, glm::distance(want, u.p)) : best;
            }
            mirror += best;
            n++;
        }
        Con_Printf("fit %s: region %.1f..%.1f (bone %.1f), rows %d %.1f..%.1f, sleeve %.1f\n", pieceNames[p], static_cast<double>(lo), static_cast<double>(hi),
            static_cast<double>(f.length), g.rows, static_cast<double>(g.hTop), static_cast<double>(g.hBottom), static_cast<double>(g.sleeveEnd));
        Con_Printf("fit %s: mirror %.2f, radii%s\n", pieceNames[p], static_cast<double>(mirror / static_cast<float>(za::max(n, 1))), rings);
    }
}

void info_f()
{
    for(const Wearer& wearer : wearers)
    {
        const char* w = wearer.model;
        qmodel_t* src = Mod_ForName(w, false);
        Shell* s = src ? shellFor(src) : nullptr;
        if(!s)
        {
            Con_Printf("agarmor: %s: no shell (no authored rig?)\n", w);
            continue;
        }
        Con_Printf("agarmor: %s: %d vertices, %d triangles, %d bone slots, hash %08x, made in %.1f ms (body %d vertices, %d triangles)\n",
            w, count(s->verts), count(s->tris) / 3, s->numSlots, s->hash, static_cast<double>(s->made), count(s->body.p),
            count(s->body.tris) / 3);
        for(int p = 0; p < NumPieces; p++)
        {
            // (how far out its outer face is against its inner face from their middle, on average: above 0, outside it)
            glm::vec3 mid{0.f};
            int n[2]{0, 0};
            for(const Vert& v : s->verts)
            {
                mid += v.piece == p && v.layer == 1 ? v.p : glm::vec3{0.f};
                n[0] += v.piece == p && v.layer == 0 ? 1 : 0;
                n[1] += v.piece == p && v.layer == 1 ? 1 : 0;
            }
            mid /= static_cast<float>(za::max(n[1], 1));
            float out[2]{0.f, 0.f};
            for(const Vert& v : s->verts)
            {
                out[v.layer == 1 ? 1 : 0] += v.piece == p && v.layer < 2 ? glm::distance(v.p, mid) : 0.f;
            }
            Con_Printf("agarmor:   %s: %d vertices, %d triangles, %d rim edges, outer face out from inner %.2f (flip %d)\n", pieceNames[p], s->pieceVerts[p], s->pieceTris[p],
                s->pieceOpen[p], static_cast<double>(out[0] / static_cast<float>(za::max(n[0], 1)) - out[1] / static_cast<float>(za::max(n[1], 1))), s->body.nativeFlip ? 1 : 0);
        }
        Con_Printf("agarmor:   helmet fit: %s\n", s->helmFit);
        if(Cmd_Argc() > 1 && strstr(w, Cmd_Argv(1)))
        {
            fitInfo(src, *s);
        }
    }
    Con_Printf("agarmor: %d drawn last frame\n", numWorn);
}

// vr_agarmor_headcam [view] [distance] [set] [drop] [elbow]: (elbow 1 / 2: on its left / right elbow, not its head)
// (the mock headset; the helmet's eyeshots; drop: units lower, the
// joints' leather's) the eyes on two heads: the left on
// the nearest grunt's, the right on the nearest enforcer's (vrgrunt / vrenforcer, armoured or not; set 1: the nearest
// death knight's, the screenshot's eye, and knight's: vr_agarmor_test 8), `distance` units off
// (default 22) at the helmet's middle (its rest pose where the body stands): view 1 front, 2 side (its right), 3
// three-quarter (front right), 4 behind, 5 side (its left); 0 (or none) the eyes from the head again.
void headcam_f()
{
    const int view = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : 0;
    const float dist = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : 22.f;
    if(view <= 0 || view > 5)
    {
        setMockEyeCameras(false);
        Con_Printf("agarmor headcam: off\n");
        return;
    }
    const bool knights = Cmd_Argc() > 3 && atoi(Cmd_Argv(3)) == 1;
    const int elbow = Cmd_Argc() > 5 ? atoi(Cmd_Argv(5)) : 0;
    const float drop = Cmd_Argc() > 4 && elbow == 0 ? static_cast<float>(atof(Cmd_Argv(4))) : 0.f;
    const char* wanted[2] = {knights ? "progs/vrhknight.mdl" : "progs/vrgrunt.mdl", knights ? "progs/vrknight.mdl" : "progs/vrenforcer.mdl"};
    const glm::vec3 me = cl.viewentity > 0 && cl.viewentity < cl.num_entities ?
        glm::vec3{cl_entities[cl.viewentity].origin[0], cl_entities[cl.viewentity].origin[1], cl_entities[cl.viewentity].origin[2]} :
        glm::vec3{0.f};
    glm::vec3 camAt[2]{};
    float pitch[2] = {0.f, 0.f}, yaw[2] = {0.f, 0.f};
    for(int k = 0; k < 2; k++)
    {
        int found = -1;
        float best = 1e30f;
        for(int i = 1; i < cl.num_entities; i++)
        {
            const entity_t& e = cl_entities[i];
            const float d = glm::distance(geom::vec(e.origin), me);
            found = e.model && !strcmp(e.model->name, wanted[k]) && d < best ? i : found;
            best = e.model && !strcmp(e.model->name, wanted[k]) && d < best ? d : best;
        }
        const Shell* s = found > 0 ? shellFor(cl_entities[found].model) : nullptr;
        glm::vec3 mid{0.f};
        int n = 0;
        for(int v = 0; s && v < count(s->verts); v++)
        {
            mid += at(s->verts, v).piece == Helmet ? at(s->verts, v).p : glm::vec3{0.f};
            n += at(s->verts, v).piece == Helmet ? 1 : 0;
        }
        // (`elbow` 1 or 2: the eyes on that body's left or right elbow instead, its joint's rest place, `drop` ignored:
        // vr_pp_test 17's elbows, the head's cameras sat too high and far for them; ROUND24.md, "Elbow leather at rest")
        const int joint = elbow == 1 ? ElbowL : elbow == 2 ? ElbowR : -1;
        mid = s && joint >= 0 && s->bridges[joint].ok ? s->bridges[joint].pivot : mid;
        // (the pivot is one point: it was divided by the helmet's vertex count below, the camera put near the feet)
        n = s && joint >= 0 ? (s->bridges[joint].ok ? 1 : 0) : n;
        if(n == 0)
        {
            Con_Printf("agarmor headcam: no %s with a helmet in view\n", wanted[k]);
            continue;
        }
        const entity_t& e = cl_entities[found];
        mid /= static_cast<float>(n);
        const float y = e.angles[1] * (pi / 180.f);
        const glm::vec3 fw{za::cos(y), za::sin(y), 0.f}, left{-za::sin(y), za::cos(y), 0.f};
        // (`drop`: the eyes this many units lower, on the same level line: the elbows' and the knees' leather)
        const glm::vec3 head = geom::vec(e.origin) + fw * mid.x + left * mid.y + glm::vec3{0.f, 0.f, mid.z - drop};
        const glm::vec3 dirs[5] = {fw, -left, glm::normalize(fw - left), -fw, left};
        const float yaws[5] = {180.f, 90.f, 135.f, 0.f, -90.f};
        camAt[k] = head + dirs[view - 1] * dist;
        yaw[k] = e.angles[1] + yaws[view - 1];
        Con_Printf("agarmor headcam: %s (entity %d) head at %.1f %.1f %.1f, view %d\n", wanted[k], found, static_cast<double>(head.x),
            static_cast<double>(head.y), static_cast<double>(head.z), view);
    }
    setMockEyeCameras(true, camAt, pitch, yaw);
}

void rebuild_f()
{
    shells.clear();
    forgetModels();
    Con_Printf("agarmor: shells made again when next drawn\n");
}

void set_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_agarmor_set <entity> <type 0 none, 1 green, 2 yellow, 3 red | -1 vr_agarmor's> [pieces: 1 cuirass, 2 helmet, 4/8 "
                   "pauldrons, 16/32 bracers, 64/128 thighs, 256/512 shins; 1023 all]\n");
        return;
    }
    const int num = Q_atoi(Cmd_Argv(1)), type = Q_atoi(Cmd_Argv(2));
    const int pieces = Cmd_Argc() > 3 ? Q_atoi(Cmd_Argv(3)) : allPieces;
    if(!give(num, type, pieces))
    {
        setWorn(num, type, pieces);
    }
}

} // namespace

void overridesChanged()
{
    bool any = false;
    for(Shell& s : shells)
    {
        if(s.tweaks == armourdata::signature(s.model.cStr()))
        {
            s.tweaksGen = armourdata::generation();
            continue;
        }
        qmodel_t* src = Mod_ForName(s.model.cStr(), false);
        if(src)
        {
            (void)buildShell(src, s);
            any = true;
        }
    }
    if(any)
    {
        forgetModels();
    }
}

void studioView(int ent, float bodyAlpha, int pieces, bool sleeve)
{
    studio.ent = ent;
    studio.alpha = bodyAlpha;
    studio.pieces = pieces & allPieces;
    studio.sleeve = sleeve;
}

bool give(int num, int type, int pieces)
{
    // A live game's entity: its armour in QC (pools and all: vr_agarmor.qc VR_AgArmor_SetCmd), sent back as the server's.
    if(!sv.active || num <= svs.maxclients || num >= sv.qcvm.num_edicts)
    {
        return false;
    }
    bool done = false;
    {
        const progs::ServerVm vm;
        const func_t fn = progs::findFunction("VR_AgArmor_SetCmd");
        if(fn && !EDICT_NUM(num)->free)
        {
            pr_global_struct->time = qcvm->time;
            pr_global_struct->self = EDICT_TO_PROG(progs::playerEdict());
            G_INT(OFS_PARM0) = EDICT_TO_PROG(EDICT_NUM(num));
            G_FLOAT(OFS_PARM1) = static_cast<float>(type);
            G_FLOAT(OFS_PARM2) = static_cast<float>(pieces);
            PR_ExecuteProgram(fn);
            done = G_FLOAT(OFS_RETURN) != 0.f;
        }
    }
    if(done)
    {
        setWorn(num, -1, 0); // (the server's word, not a drawn-only override)
    }
    return done;
}

void setWorn(int num, int type, int pieces)
{
    for(za::SizeT i = 0; i < named.size(); i++)
    {
        if(named[i].num == num)
        {
            named.erase(&named[i]);
            break;
        }
    }
    if(type >= 0 && named.size() < static_cast<za::SizeT>(maxNamed))
    {
        named.pushBack(Named{num, za::clamp(type, 0, 3), pieces & allPieces});
    }
}

bool makeModel(qmodel_t* mod)
{
    // "<model>#bridge": the joints' leather (vr_agarmor_bridge.cpp)
    if(const size_t len = strlen(mod->name), suffix = strlen(bridgeSuffix); len > suffix && !strcmp(mod->name + len - suffix, bridgeSuffix))
    {
        char base[MAX_QPATH];
        q_strlcpy(base, mod->name, za::min(sizeof(base), len - suffix + 1));
        qmodel_t* src = isWearer(base) ? Mod_ForName(base, false) : nullptr;
        qmodel_t* skins = src ? Mod_ForName(skinModelName, false) : nullptr;
        const auto* kh = skins && skins->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(skins)) : nullptr;
        const Shell* s = kh ? shellFor(src) : nullptr;
        return s && s->bridgeVerts > 0 && makeBridgeModel(mod, *s, kh);
    }
    const size_t len = strlen(mod->name), suffix = strlen(armorSuffix);
    if(len <= suffix || strcmp(mod->name + len - suffix, armorSuffix))
    {
        return false;
    }
    char base[MAX_QPATH];
    q_strlcpy(base, mod->name, za::min(sizeof(base), len - suffix + 1));
    qmodel_t* src = isWearer(base) ? Mod_ForName(base, false) : nullptr;
    qmodel_t* skins = src ? Mod_ForName(skinModelName, false) : nullptr;
    const auto* kh = skins && skins->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(skins)) : nullptr;
    const Shell* s = kh ? shellFor(src) : nullptr;
    if(!s || s->numSlots <= 0)
    {
        Con_DPrintf("agarmor: %s not made (%s)\n", mod->name, !kh ? "no vragarmor.mdl" : "no shell");
        return false;
    }
    const int numVerts = count(s->verts), numIndexes = count(s->tris), numBones = s->numSlots;

    // (the skins' model as it is now: the hunk growing below may evict it from the cache and clear its memory, as a big model's
    // did: no skins, its plates not drawn)
    aliashdr_t skinHdr;
    memcpy(&skinHdr, kh, sizeof(aliashdr_t));
    kh = &skinHdr;

    const int start = Hunk_LowMark();
    auto* hdr = static_cast<aliashdr_t*>(Hunk_Alloc(static_cast<int>(sizeof(aliashdr_t))));
    auto* bones = static_cast<boneinfo_t*>(Hunk_Alloc(static_cast<int>(sizeof(boneinfo_t)) * numBones));
    auto* bind = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numBones));
    auto* poses = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numBones));
    auto* verts = static_cast<iqmvert_t*>(Hunk_Alloc(static_cast<int>(sizeof(iqmvert_t)) * numVerts));
    auto* indexes = static_cast<unsigned short*>(Hunk_Alloc(static_cast<int>(sizeof(unsigned short)) * numIndexes));

    hdr->ident = kh->ident;
    hdr->version = kh->version;
    hdr->scale[0] = hdr->scale[1] = hdr->scale[2] = 1.f;
    hdr->boundingradius = 64.f;
    hdr->numskins = kh->numskins;
    hdr->skinwidth = kh->skinwidth;
    hdr->skinheight = kh->skinheight;
    hdr->numverts = numVerts;
    hdr->numverts_vbo = numVerts;
    hdr->numtris = numIndexes / 3;
    hdr->numframes = 1;
    hdr->synctype = ST_SYNC;
    hdr->numindexes = numIndexes;
    hdr->numposes = 1;
    hdr->numbones = numBones;
    hdr->poseverttype = aliashdr_t::PV_IQM;
    hdr->nextsurface = 0;
    hdr->frames[0].firstpose = 0;
    hdr->frames[0].numposes = 1;
    hdr->frames[0].interval = 0.1f;
    q_strlcpy(hdr->frames[0].name, "armor", sizeof(hdr->frames[0].name));
    memcpy(hdr->gltextures, kh->gltextures, sizeof(hdr->gltextures));
    memcpy(hdr->fbtextures, kh->fbtextures, sizeof(hdr->fbtextures));
    memcpy(hdr->texels, kh->texels, sizeof(hdr->texels));
    const rig2::Rig* rig = rigOf(src);
    for(int b = 0; b < numBones; b++)
    {
        q_snprintf(bones[b].name, sizeof(bones[b].name), "%s:%s", pieceNames[s->slotPiece[b]],
            rig && s->slotBone[b] < rig->numBones ? rig->bones[s->slotBone[b]].name : "?");
        bones[b].parent = -1;
        for(int i = 0; i < 12; i++)
        {
            const float id = i % 5 == 0 ? 1.f : 0.f;
            bones[b].inverse.mat[i] = id;
            bind[b].mat[i] = id;
            poses[b].mat[i] = id;
        }
    }
    const float padW = static_cast<float>(TexMgr_PadConditional(kh->skinwidth)), padH = static_cast<float>(TexMgr_PadConditional(kh->skinheight));
    for(int v = 0; v < numVerts; v++)
    {
        const Vert& x = at(s->verts, v);
        iqmvert_t& o = verts[v];
        for(int i = 0; i < 3; i++)
        {
            o.xyz[i] = x.p[i];
            o.norm[i] = static_cast<int8_t>(za::clamp(static_cast<int>(x.n[i] * 127.f + (x.n[i] < 0.f ? -0.5f : 0.5f)), -127, 127));
        }
        o.norm[3] = 0;
        o.st[0] = x.st.x / padW;
        o.st[1] = x.st.y / padH;
        int left = 255, k = 0;
        int order[4] = {-1, -1, -1, -1};
        for(int b = 0; b < rig2::maxBones && k < 4; b++)
        {
            order[k] = x.w.w[b] > 0.f ? b : -1;
            k += x.w.w[b] > 0.f ? 1 : 0;
        }
        for(int i = 0; i < 4; i++)
        {
            o.weight[i] = 0;
            o.idx[i] = 0;
        }
        for(int i = 0; i < k; i++)
        {
            const int bone = order[i];
            int slot = 0;
            for(int q = 0; q < s->numSlots; q++)
            {
                slot = s->slotPiece[q] == x.piece && s->slotBone[q] == bone ? q : slot;
            }
            const int wb = i + 1 < k ? static_cast<int>(x.w.w[bone] * 255.f + 0.5f) : left;
            o.weight[i] = static_cast<uint8_t>(za::clamp(za::min(wb, left), 0, 255));
            o.idx[i] = static_cast<uint8_t>(slot);
            left -= o.weight[i];
        }
    }
    for(int i = 0; i + 2 < numIndexes; i += 3)
    {
        indexes[i] = static_cast<unsigned short>(at(s->tris, i));
        indexes[i + 1] = static_cast<unsigned short>(at(s->tris, s->body.nativeFlip ? i + 2 : i + 1));
        indexes[i + 2] = static_cast<unsigned short>(at(s->tris, s->body.nativeFlip ? i + 1 : i + 2));
    }
    hdr->boneinfo = reinterpret_cast<byte*>(bones) - reinterpret_cast<byte*>(hdr);
    hdr->bindpose = reinterpret_cast<byte*>(bind) - reinterpret_cast<byte*>(hdr);
    hdr->boneposedata = reinterpret_cast<byte*>(poses) - reinterpret_cast<byte*>(hdr);
    hdr->vertexes = reinterpret_cast<byte*>(verts) - reinterpret_cast<byte*>(hdr);
    hdr->indexes = reinterpret_cast<byte*>(indexes) - reinterpret_cast<byte*>(hdr);

    if(mod->meshvbo)
    {
        GLMesh_DeleteVertexBuffer(mod);
    }
    GLMesh_LoadVertexBuffer(mod, hdr);
    mod->type = mod_alias;
    mod->needload = false;
    mod->numframes = 1;
    mod->synctype = ST_SYNC;
    mod->flags = 0;
    mod->path_id = src->path_id;
    for(int i = 0; i < 3; i++)
    {
        const float box = isPlayerBody(src->name) ? 128.f : 64.f; // (the player's: reaching out from where it stands)
        mod->mins[i] = mod->ymins[i] = mod->rmins[i] = -box;
        mod->maxs[i] = mod->ymaxs[i] = mod->rmaxs[i] = box;
    }
    const int total = Hunk_LowMark() - start;
    Cache_Alloc(&mod->cache, total, mod->name);
    if(!mod->cache.data)
    {
        Hunk_FreeToLowMark(start);
        return false;
    }
    memcpy(mod->cache.data, hdr, static_cast<size_t>(total));
    Hunk_FreeToLowMark(start);
    Con_DPrintf("agarmor: %s made (%d vertices, %d triangles, %d bones)\n", mod->name, numVerts, numIndexes / 3, numBones);
    return true;
}

void relink()
{
    numWorn = 0;
    numBridges = 0;
    // (the joints' leather's settings changed: every shell made again, as vr_agarmor_rebuild)
    const float key[6] = {vr_agarmor_bridge.value != 0.f ? 1.f : 0.f, vr_agarmor_folds.value, vr_agarmor_fold_depth.value, vr_agarmor_details.value, vr_agarmor_emblem.value, vr_agarmor_hero.value};
    if(memcmp(key, bridgeKey, sizeof(key)))
    {
        const bool first = bridgeKey[0] < 0.f;
        memcpy(bridgeKey, key, sizeof(key));
        if(!first)
        {
            shells.clear();
            forgetModels();
        }
    }
    if(cls.state != ca_connected || !cl_entities || (vr_agarmor.value == 0.f && named.empty() && numServed == 0))
    {
        return;
    }
    const int visible = cl_numvisedicts;
    for(int i = 0; i < visible && numWorn < maxWorn && cl_numvisedicts < MAX_VISEDICTS; i++)
    {
        entity_t* e = cl_visedicts[i];
        if(!e || !e->model || e < cl_entities || e >= cl_entities + cl_max_edicts || !strchr(e->model->name, '#'))
        {
            continue;
        }
        const int num = static_cast<int>(e - cl_entities);
        int type = 0, pieces = 0, wear = 0;
        wornBy(num, type, pieces, wear);
        qmodel_t* armor = type > 0 && pieces ? armorFor(e->model) : nullptr;
        if(!armor)
        {
            continue;
        }
        char base[MAX_QPATH];
        const size_t len = strlen(e->model->name), suffix = strlen(rigSuffix);
        q_strlcpy(base, e->model->name, za::min(sizeof(base), len - suffix + 1));
        const int shell = shellNamed(base);
        if(shell < 0)
        {
            continue;
        }
        // P2's damage marks: the pieces at each wear (whole, two thirds, a third) drawn with that wear's skin (type - 1 +
        // 3 x wear: make_agarmor.py's nine), an entity each (the pieces of the others collapsed). An old vragarmor.mdl
        // of three skins: all whole.
        const int numSkins = armor->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(armor))->numskins : 0;
        const int w66 = wear & allPieces, w33 = (wear >> NumPieces) & allPieces;
        const int byWear[3] = {numSkins >= 9 ? pieces & ~w66 & ~w33 : pieces, numSkins >= 9 ? pieces & w66 & ~w33 : 0,
            numSkins >= 9 ? pieces & w33 : 0};
        for(int level = 0; level < 3 && numWorn < maxWorn && cl_numvisedicts < MAX_VISEDICTS; level++)
        {
            if(!byWear[level])
            {
                continue;
            }
            const int k = numWorn++;
            entity_t& a = worn[k];
            a = *e;
            a.model = armor;
            // (the Armour Studio's subject: its body a ghost, its plates whole)
            a.alpha = studio.ent > 0 && num == studio.ent ? static_cast<byte>(ENTALPHA_DEFAULT) : a.alpha;
            a.skinnum = type - 1 + 3 * level;
            a.frame = 0;
            a.previouspose = a.currentpose = 0;
            a.lerpflags &= static_cast<byte>(~(LERP_RESETANIM | LERP_RESETANIM2));
            wornSource[k] = num;
            wornPieces[k] = byWear[level];
            wornShell[k] = shell;
            wornSlots[k] = at(shells, shell).numSlots;
            cl_visedicts[cl_numvisedicts++] = &a;
        }
        // The joints' leather: an entity of its own over every piece worn (its bones made when first drawn: bonePoses).
        const Shell& sh = at(shells, shell);
        int limbs = 0;
        for(const Bridge& br : sh.bridges)
        {
            limbs |= br.ok ? (pieces & ((1 << br.upper) | (1 << br.lower))) : 0;
        }
        qmodel_t* bridge = limbs && sh.bridgeVerts > 0 && numBridges < maxBridges && cl_numvisedicts < MAX_VISEDICTS ? bridgeFor(e->model) : nullptr;
        if(bridge)
        {
            const int k = numBridges++;
            entity_t& b = bridgeEnts[k];
            b = *e;
            b.model = bridge;
            b.alpha = studio.ent > 0 && num == studio.ent ? static_cast<byte>(ENTALPHA_DEFAULT) : b.alpha;
            b.skinnum = type - 1; // (the leather is the same under every wear's paint)
            b.frame = 0;
            b.previouspose = b.currentpose = 0;
            b.lerpflags &= static_cast<byte>(~(LERP_RESETANIM | LERP_RESETANIM2));
            bridgeSource[k] = num;
            bridgeShell[k] = shell;
            bridgePieces[k] = pieces;
            bridgeMade[k] = false;
            cl_visedicts[cl_numvisedicts++] = &b;
        }
        if(studio.ent > 0 && num == studio.ent && studio.alpha < 0.999f)
        {
            e->alpha = ENTALPHA_ENCODE(za::clamp(studio.alpha, 0.05f, 1.f));
        }
    }
}

int bonePoses(const entity_t* e, const float** matrices)
{
    if(numBridges > 0 && e >= &bridgeEnts[0] && e < &bridgeEnts[numBridges])
    {
        // The joints' leather: made from its body's bones as drawn, once a frame (a vertex a bone: vr_agarmor_bridge.cpp)
        const int k = static_cast<int>(e - &bridgeEnts[0]);
        const Shell* s = bridgeShell[k] < count(shells) ? &at(shells, bridgeShell[k]) : nullptr;
        const int n = s ? s->bridgeVerts : 0;
        if(!matrices || n <= 0)
        {
            return n;
        }
        if(!bridgeMade[k])
        {
            bridgeMade[k] = true;
            const float* src = nullptr;
            const int have = VR_AliasBonePoses(&cl_entities[bridgeSource[k]], &src);
            (void)bridgeBones(*s, bridgePieces[k], src, src ? have : 0, bridgeBonesOf[k]);
            if(vr_agarmor_bridge_trace.value > 0.f && src)
            {
                bridgeTrace(*s, bridgeSource[k], s->model.cStr(), bridgePieces[k], src, have);
            }
        }
        *matrices = bridgeBonesOf[k];
        return n;
    }
    if(numWorn <= 0 || e < &worn[0] || e >= &worn[numWorn])
    {
        return 0;
    }
    const int k = static_cast<int>(e - &worn[0]);
    const int n = wornSlots[k];
    if(!matrices)
    {
        return n;
    }
    const float* src = nullptr;
    const int have = VR_AliasBonePoses(&cl_entities[wornSource[k]], &src);
    // (the shells made again since: collapsed, drawn from the next frame)
    const Shell* s = wornShell[k] < count(shells) && at(shells, wornShell[k]).numSlots == n ? &at(shells, wornShell[k]) : nullptr;
    float* out = wornBones[k];
    for(int slot = 0; slot < n; slot++)
    {
        const int b = s ? s->slotBone[slot] : 0;
        const bool shown = s && src && b < have && ((wornPieces[k] >> s->slotPiece[slot]) & 1);
        for(int i = 0; i < 12; i++)
        {
            out[slot * 12 + i] = shown ? src[b * 12 + i] : 0.f; // (a hidden piece collapsed: its triangles gone)
        }
    }
    *matrices = out;
    return n;
}

const entity_t* sourceOf(const entity_t* e)
{
    if(numBridges > 0 && e >= &bridgeEnts[0] && e < &bridgeEnts[numBridges])
    {
        return &cl_entities[bridgeSource[e - &bridgeEnts[0]]];
    }
    if(numWorn <= 0 || e < &worn[0] || e >= &worn[numWorn])
    {
        return nullptr;
    }
    return &cl_entities[wornSource[e - &worn[0]]];
}

// ---- P1: the server's armour (QC's .vr_ar_type, .vr_ar_mask) sent to the clients ----
namespace
{
za::Vector<uint32_t> sent; // per entity: what the clients were last told (packState)

[[nodiscard]] uint32_t stateOf(edict_t* e, bool player)
{
    const progs::FieldOffsets& f = progs::fields();
    if(e->free || f.vr_ar_type < 0 || f.vr_ar_mask < 0)
    {
        return 0u;
    }
    if(player) // (the player's plates are a look alone: fields of their own, never what combat.qc reads for a wearer)
    {
        return f.vr_pp_type < 0 || f.vr_pp_mask < 0 ? 0u :
            packState(za::clamp(static_cast<int>(progs::fieldFloat(e, f.vr_pp_type)), 0, 3), static_cast<int>(progs::fieldFloat(e, f.vr_pp_mask)) & allPieces, 0);
    }
    const int type = za::clamp(static_cast<int>(progs::fieldFloat(e, f.vr_ar_type)), 0, 3);
    const int mask = static_cast<int>(progs::fieldFloat(e, f.vr_ar_mask)) & allPieces;
    const int wear = f.vr_ar_wear >= 0 ? static_cast<int>(progs::fieldFloat(e, f.vr_ar_wear)) : 0;
    return packState(type, mask, wear);
}

void writeState(sizebuf_t* msg, int num, uint32_t state)
{
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_ARMOR);
    MSG_WriteShort(msg, num);
    MSG_WriteByte(msg, stateType(state));
    MSG_WriteShort(msg, statePieces(state));
    MSG_WriteShort(msg, stateWear(state) & allPieces);
    MSG_WriteShort(msg, stateWear(state) >> NumPieces);
}

// ---- A piece as a model of its own (the broken plate's debris) ----
// The piece's rigid frame: three body vertices under it (the nearest its middle, the farthest from that, the farthest
// from their line), its middle; rest -> as drawn.
struct PlateFrame
{
    int anchor[3]{-1, -1, -1}; // .mdl vertices
    int bone{-1};              // the bone they weigh on
    glm::vec3 mid{0.f};
    glm::mat3 rest{1.f};
    bool ok{false};
};

[[nodiscard]] glm::mat3 frameFrom(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 x = geom::safeNormalize(b - a, glm::vec3{1.f, 0.f, 0.f}, 1e-6f);
    const glm::vec3 z = geom::safeNormalize(glm::cross(x, c - a), glm::vec3{0.f, 0.f, 1.f}, 1e-6f);
    return glm::mat3{x, glm::cross(z, x), z};
}

[[nodiscard]] PlateFrame frameOnBone(const Shell& s, int bone, PlateFrame pf);

[[nodiscard]] PlateFrame plateFrameMade(const Shell& s, int piece);

// A piece's frame: made once for its shell (each hit asks for all of them, each a walk over the body's vertices).
[[nodiscard]] PlateFrame plateFrameOf(const Shell& s, int piece)
{
    const PlateTube& tube = s.tubes[piece];
    if(!tube.frameKnown)
    {
        const PlateFrame pf = plateFrameMade(s, piece);
        tube.frameKnown = true;
        tube.frameOk = pf.ok;
        tube.frameBone = pf.bone;
        tube.frameMid = pf.mid;
        tube.frameRest = pf.rest;
        for(int i = 0; i < 3; i++)
        {
            tube.frameAnchor[i] = pf.anchor[i];
        }
    }
    PlateFrame pf;
    pf.ok = tube.frameOk;
    pf.bone = tube.frameBone;
    pf.mid = tube.frameMid;
    pf.rest = tube.frameRest;
    for(int i = 0; i < 3; i++)
    {
        pf.anchor[i] = tube.frameAnchor[i];
    }
    return pf;
}

[[nodiscard]] PlateFrame plateFrameMade(const Shell& s, int piece)
{
    PlateFrame pf;
    int n = 0;
    for(const Vert& v : s.verts)
    {
        pf.mid += v.piece == piece ? v.p : glm::vec3{0.f};
        n += v.piece == piece ? 1 : 0;
    }
    if(n == 0)
    {
        return pf;
    }
    pf.mid /= static_cast<float>(n);
    // The body's vertices under the piece: the bone(s) most of its vertices weigh on.
    W sum;
    for(const Vert& v : s.verts)
    {
        for(int i = 0; i < rig2::maxBones && v.piece == piece; i++)
        {
            sum.w[i] += v.w.w[i];
        }
    }
    // (its middle: of the plates on that bone, not what laps onto its neighbours: the cuirass's, down from the clavicles and the neck)
    // (a limb's: its own bone, whatever else its plates weigh on: a knight's sword arm, hardly on its upper arm)
    const int bone = s.tubes[piece].bone >= 0 ? s.tubes[piece].bone : heaviest(sum);
    glm::vec3 onBone{0.f};
    int m = 0;
    for(const Vert& v : s.verts)
    {
        if(v.piece == piece && heaviest(v.w) == bone)
        {
            onBone += v.p;
            m++;
        }
    }
    pf.mid = m ? onBone / static_cast<float>(m) : pf.mid;
    if(piece == Cuirass && s.tubes[piece].grid.rows)
    {
        // (the cuirass's: half way up the spine and chest, round its axis there)
        const PlateTube& tube = s.tubes[piece];
        const glm::vec2 c = at(tube.grid.axis, tube.grid.rows / 2);
        pf.mid = tube.origin + tube.up * (tube.length * 0.5f) + tube.fwd * c.x + tube.left * c.y;
    }
    return frameOnBone(s, bone, pf);
}

// `pf` (its middle set) anchored on `bone`: three of the body's vertices weighing mostly on it, the one nearest the middle
// first, then the farthest from it, then the farthest from their line.
[[nodiscard]] PlateFrame frameOnBone(const Shell& s, int bone, PlateFrame pf)
{
    const Body& b = s.body;
    const float cut = regionCut(b.w, 1u << static_cast<unsigned>(bone));
    pf.bone = bone;
    int a = -1, c = -1, d = -1;
    float best = 1e9f;
    for(int v = 0; v < count(b.p); v++)
    {
        const float dist = glm::length(at(b.p, v) - pf.mid);
        a = at(b.w, v).w[bone] > cut && dist < best ? v : a;
        best = at(b.w, v).w[bone] > cut && dist < best ? dist : best;
    }
    if(a < 0)
    {
        return pf;
    }
    best = -1.f;
    for(int v = 0; v < count(b.p); v++)
    {
        const float dist = glm::length(at(b.p, v) - at(b.p, a));
        c = at(b.w, v).w[bone] > cut && dist > best ? v : c;
        best = at(b.w, v).w[bone] > cut && dist > best ? dist : best;
    }
    best = -1.f;
    const glm::vec3 axis = geom::safeNormalize(at(b.p, c) - at(b.p, a), glm::vec3{1.f, 0.f, 0.f}, 1e-6f);
    for(int v = 0; v < count(b.p); v++)
    {
        const glm::vec3 q = at(b.p, v) - at(b.p, a);
        const float dist = glm::length(q - axis * glm::dot(q, axis));
        d = at(b.w, v).w[bone] > cut && dist > best ? v : d;
        best = at(b.w, v).w[bone] > cut && dist > best ? dist : best;
    }
    if(c < 0 || d < 0 || c == a || d == a || best < 0.5f)
    {
        return pf;
    }
    pf.anchor[0] = at(b.rep, a);
    pf.anchor[1] = at(b.rep, c);
    pf.anchor[2] = at(b.rep, d);
    pf.rest = frameFrom(at(b.p, a), at(b.p, c), at(b.p, d));
    pf.ok = true;
    return pf;
}

[[nodiscard]] const char* baseOf(edict_t* ent)
{
    const char* m = ent && !ent->free && ent->v.model ? PR_GetString(ent->v.model) : "";
    return isWearer(m) ? m : nullptr;
}

[[nodiscard]] const Shell* serverShell(const char* base)
{
    qmodel_t* src = base ? Mod_ForName(base, false) : nullptr;
    return src ? shellFor(src) : nullptr;
}

void putWord(za::Vector<byte>& f, int v)
{
    for(int i = 0; i < 4; i++)
    {
        f.pushBack(static_cast<byte>((static_cast<unsigned>(v) >> (8u * static_cast<unsigned>(i))) & 0xffu));
    }
}
void putFloat(za::Vector<byte>& f, float v)
{
    int i = 0;
    memcpy(&i, &v, 4);
    putWord(f, i);
}
} // namespace

void serverFrame()
{
    if(!sv.active || progs::fields().vr_ar_type < 0)
    {
        return;
    }
    const int n = sv.qcvm.num_edicts;
    if(count(sent) < n)
    {
        sent.resize(static_cast<za::SizeT>(n), 0u);
    }
    for(int i = 1; i < count(sent); i++)
    {
        const uint32_t now = i < n ? stateOf(EDICT_NUM(i), i <= svs.maxclients) : 0u;
        if(now != at(sent, i) && sv.reliable_datagram.cursize + 16 < sv.reliable_datagram.maxsize)
        {
            writeState(&sv.reliable_datagram, i, now);
            at(sent, i) = now;
        }
    }
}

void serverWriteAll(sizebuf_t* msg)
{
    sent.clear();
    if(!sv.active || progs::fields().vr_ar_type < 0)
    {
        return;
    }
    writeState(msg, -1, 0u);
    sent.resize(static_cast<za::SizeT>(sv.qcvm.num_edicts), 0u);
    for(int i = 1; i < sv.qcvm.num_edicts; i++)
    {
        at(sent, i) = stateOf(EDICT_NUM(i), i <= svs.maxclients);
        if(at(sent, i) && msg->cursize + 16 < msg->maxsize)
        {
            writeState(msg, i, at(sent, i));
        }
    }
}

void parseEvent()
{
    const int num = MSG_ReadShort();
    const int type = MSG_ReadByte();
    const int pieces = MSG_ReadShort() & allPieces;
    const int w66 = MSG_ReadShort() & allPieces;
    const int w33 = MSG_ReadShort() & allPieces;
    if(num < 0)
    {
        served.clear();
        numServed = 0;
        view::platesResync(); // (a new level or a load: the player's plates as they come, no morph)
        return;
    }
    if(num >= count(served))
    {
        served.resize(static_cast<za::SizeT>(num + 1), 0u);
    }
    const uint32_t now = packState(za::clamp(type, 0, 3), pieces, w66 | w33 << NumPieces);
    numServed += (now != 0u ? 1 : 0) - (at(served, num) != 0u ? 1 : 0);
    at(served, num) = now;
}

namespace
{
int pieceOfBone(edict_t* ent, const float* p)
{
    const char* name = nullptr;
    if(box3d::boneAt(ent, geom::vec(p), &name) < 0 || !name)
    {
        return -2;
    }
    if(!q_strcasecmp(name, "head"))
    {
        return Helmet;
    }
    if(!q_strcasecmp(name, "spine") || !q_strcasecmp(name, "chest"))
    {
        return Cuirass;
    }
    for(int piece = 0; piece < NumPieces; piece++)
    {
        if(limbBone[piece] && !q_strcasecmp(name, limbBone[piece]))
        {
            return piece;
        }
    }
    return -1;
}

// A plate's three anchors as `ent` is drawn now (the pose shots meet: hitmodel's, an armed humanoid's on its authored rig
// as the client draws its armour; its animation frame's put the arms' plates where the arms are not; else its rest pose
// where it stands).
void posedAnchors(edict_t* ent, const Shell& s, const PlateFrame& pf, glm::vec3* posed)
{
    if(!hitmodel::drawnVertexPoints(ent, pf.anchor, 3, posed))
    {
        const glm::mat3 axes = held::axesFromAngles(ent->v.angles, false);
        for(int i = 0; i < 3; i++)
        {
            posed[i] = geom::vec(ent->v.origin) + axes * at(s.body.mdlRest, pf.anchor[i]);
        }
    }
}
} // namespace

int clientState(int num, int what)
{
    if(what >= 10)
    {
        return view::platesState(what); // the player's plates as drawn (the tests)
    }
    if(num < 0 || num >= count(served) || at(served, num) == 0u)
    {
        return -1;
    }
    const uint32_t s = at(served, num);
    return what == 0 ? stateType(s) : what == 1 ? statePieces(s) : stateWear(s);
}

// The point of triangle a, b, c nearest `p`.
[[nodiscard]] glm::vec3 nearestOnTri(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f)
    {
        return a;
    }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if(d3 >= 0.f && d4 <= d3)
    {
        return b;
    }
    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
    {
        return a + ab * (d1 / (d1 - d3));
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if(d6 >= 0.f && d5 <= d6)
    {
        return c;
    }
    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
    {
        return a + ac * (d2 / (d2 - d6));
    }
    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
    {
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    }
    const float denom = 1.f / za::max(va + vb + vc, 1e-12f);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

// The plate nearest world point `p` on `ent` (each plate's own frame as drawn: the point taken back to its rest pose),
// within `reach` units; -1 none. What a hit there strikes: the ragdoll rig's nearest vertex (pieceOfBone) read a thigh
// plate's lower half as the shin's (the shin's vertices lie nearer the knee's skin than the thigh's do).
int plateNear(edict_t* ent, const Shell& s, const float* p, float reach, float& dist, int prefer = -1)
{
    bool ok[NumPieces]{};
    glm::vec3 rest[NumPieces]{};
    const glm::vec3 at3{p[0], p[1], p[2]};
    for(int piece = 0; piece < NumPieces; piece++)
    {
        const PlateFrame pf = plateFrameOf(s, piece);
        if(!pf.ok)
        {
            continue;
        }
        glm::vec3 posed[3];
        posedAnchors(ent, s, pf, posed);
        const glm::mat3 turn = frameFrom(posed[0], posed[1], posed[2]) * glm::transpose(pf.rest);
        rest[piece] = at(s.body.mdlRest, pf.anchor[0]) + glm::transpose(turn) * (at3 - posed[0]);
        ok[piece] = true;
    }
    int best = -1;
    float bestD = reach;
    for(int t = 0; t + 2 < count(s.tris); t += 3)
    {
        const Vert& v0 = at(s.verts, at(s.tris, t));
        if(!ok[v0.piece])
        {
            continue;
        }
        const glm::vec3& q = rest[v0.piece];
        const glm::vec3 n = nearestOnTri(q, v0.p, at(s.verts, at(s.tris, t + 1)).p, at(s.verts, at(s.tris, t + 2)).p);
        // (`prefer`, the bone's own piece, wins where two plates meet at a joint)
        const float d = za::max(glm::distance(q, n) - (v0.piece == prefer ? 0.02f : 0.f), 0.f);
        best = d < bestD || (d == bestD && v0.piece == prefer) ? v0.piece : best;
        bestD = za::min(d, bestD);
    }
    dist = bestD;
    return best;
}

// ---- What a shot meets first (ROUND23.md, "Armour: the plate a shot meets") ----
inline constexpr float shotBack = 64.f;      // units of its line looked along before where it met the model
inline constexpr float shotOn = 4.f;         // and past it (beyond the guns' tolerance), unless it crosses the skin first
inline constexpr float shotPastSkin = 0.05f; // (a plate on the skin's surface counts)
inline constexpr float debugOuterWidth = 0.35f; // vr_agarmor_debug's lines (world units): a plate's outer face's edges,
inline constexpr float debugInnerWidth = 0.12f; // its inner face's and rim's
// vr_agarmor_debug: each piece's colour (cuirass, helmet, pauldrons, bracers, thighs, shins; left and right differ).
const za::Array<glm::vec4, NumPieces> debugColours{glm::vec4{1.f, 0.85f, 0.1f, 0.8f}, glm::vec4{1.f, 0.2f, 0.2f, 0.8f},
    glm::vec4{0.2f, 0.6f, 1.f, 0.8f}, glm::vec4{0.1f, 0.9f, 1.f, 0.8f}, glm::vec4{0.2f, 1.f, 0.3f, 0.8f},
    glm::vec4{0.7f, 1.f, 0.2f, 0.8f}, glm::vec4{1.f, 0.3f, 1.f, 0.8f}, glm::vec4{0.7f, 0.4f, 1.f, 0.8f},
    glm::vec4{1.f, 0.55f, 0.1f, 0.8f}, glm::vec4{1.f, 1.f, 1.f, 0.8f}};

// Each unbroken plate posed as the hit test takes it, and as the debug view draws it: its rigid frame from its three
// anchors as drawn (posedAnchors), a rest point q at origin + turn * q in the world.
struct PlatePoses
{
    bool ok[NumPieces]{};
    glm::mat3 turn[NumPieces]{};
    glm::vec3 origin[NumPieces]{};
};

void posePlates(edict_t* ent, const Shell& s, int mask, PlatePoses& out)
{
    for(int piece = 0; piece < NumPieces; piece++)
    {
        const PlateFrame pf = (mask & (1 << piece)) ? plateFrameOf(s, piece) : PlateFrame{};
        if(!pf.ok)
        {
            continue;
        }
        glm::vec3 posed[3];
        posedAnchors(ent, s, pf, posed);
        out.turn[piece] = frameFrom(posed[0], posed[1], posed[2]) * glm::transpose(pf.rest);
        out.origin[piece] = posed[0] - out.turn[piece] * at(s.body.mdlRest, pf.anchor[0]);
        out.ok[piece] = true;
    }
}

// The entity's unbroken pieces (QC .vr_ar_mask; 0 none or no armour).
[[nodiscard]] int wornMask(edict_t* ent)
{
    const progs::FieldOffsets& f = progs::fields();
    if(!ent || ent->free || f.vr_ar_type < 0 || f.vr_ar_mask < 0 || progs::fieldFloat(ent, f.vr_ar_type) <= 0.f)
    {
        return 0;
    }
    return static_cast<int>(progs::fieldFloat(ent, f.vr_ar_mask)) & allPieces;
}

int entityPieces(int num)
{
    if(!sv.active || num <= svs.maxclients || num >= sv.qcvm.num_edicts)
    {
        return 0;
    }
    const progs::ServerVm vm; // (EDICT_NUM and the fields: the server's VM, whoever asks)
    return wornMask(EDICT_NUM(num));
}

// The first plate of `mask` the segment a..b crosses (its triangles, outer, inner and rim, both sides): its piece and
// `t` along it (0..1); -1 none.
int plateOnSegment(edict_t* ent, const Shell& s, const glm::vec3& a, const glm::vec3& b, int mask, float& t)
{
    PlatePoses pp;
    posePlates(ent, s, mask, pp);
    glm::vec3 ra[NumPieces], rd[NumPieces];
    for(int piece = 0; piece < NumPieces; piece++)
    {
        const glm::mat3 back = glm::transpose(pp.turn[piece]);
        ra[piece] = pp.ok[piece] ? back * (a - pp.origin[piece]) : glm::vec3{0.f};
        rd[piece] = pp.ok[piece] ? back * (b - a) : glm::vec3{0.f};
    }
    int best = -1;
    t = 1.f;
    for(int k = 0; k + 2 < count(s.tris); k += 3)
    {
        const Vert& v0 = at(s.verts, at(s.tris, k));
        if(v0.piece < 0 || v0.piece >= NumPieces || !pp.ok[v0.piece])
        {
            continue;
        }
        const glm::vec3& o = ra[v0.piece];
        const glm::vec3& d = rd[v0.piece];
        const glm::vec3 e1 = at(s.verts, at(s.tris, k + 1)).p - v0.p, e2 = at(s.verts, at(s.tris, k + 2)).p - v0.p;
        const glm::vec3 pv = glm::cross(d, e2);
        const float det = glm::dot(e1, pv);
        if(za::abs(det) < 1e-9f)
        {
            continue;
        }
        const float inv = 1.f / det;
        const glm::vec3 tv = o - v0.p;
        const float u = glm::dot(tv, pv) * inv;
        const glm::vec3 qv = glm::cross(tv, e1);
        const float v = glm::dot(d, qv) * inv;
        const float tt = glm::dot(e2, qv) * inv;
        // (a little slack: a line through a vertex or along an edge the triangles share meets one of them, not, by
        // rounding, none: test 10's lines at a plate's face vertex went through the cuirass and the shins)
        constexpr float slack = 1e-3f;
        if(u < -slack || u > 1.f + slack || v < -slack || u + v > 1.f + slack || tt < 0.f || tt >= t)
        {
            continue;
        }
        t = tt;
        best = v0.piece;
    }
    return best;
}

int shotPiece(edict_t* ent, const float* p, const float* dir)
{
    const Shell* s = serverShell(baseOf(ent));
    const int mask = wornMask(ent);
    const glm::vec3 d = geom::safeNormalize(geom::vec(dir), glm::vec3{0.f}, 1e-6f);
    if(!s || !mask || glm::dot(d, d) < 0.5f)
    {
        return s ? (mask ? pieceAt(ent, p) : -1) : -2;
    }
    // The shot's line from well before the body to past where it met the model grown by the guns' tolerance, cut where it
    // first crosses the skin itself: a plate on that stretch is what it met first (one beyond is on the far side).
    const glm::vec3 at3 = geom::vec(p);
    const glm::vec3 a = at3 - d * shotBack;
    const glm::vec3 b = at3 + d * (hitmodel::tolerance(hitmodel::Class::Guns) + shotOn);
    const float skin = hitmodel::skinCrossing(ent, a, b);
    const glm::vec3 end = (skin >= 0.f ? a + (b - a) * skin : b) + d * shotPastSkin;
    float t = 1.f;
    const int piece = plateOnSegment(ent, *s, a, end, mask, t);
    if(piece >= 0)
    {
        return piece;
    }
    // (none crossed: the line may run through a seam of a plate's own mesh, between its triangles (test 10's nails at a
    // plate's face middle, a vertex, 0.002 units off a line that met it, met none): the same line nudged a twentieth of
    // a unit four ways, the first plate any of them crosses)
    const glm::vec3 side = geom::safeNormalize(glm::cross(d, za::abs(d.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f}),
                                               glm::vec3{1.f, 0.f, 0.f}, 1e-6f);
    const glm::vec3 up = glm::cross(side, d);
    constexpr float nudge = 0.05f;
    const glm::vec3 nudges[4] = {side * nudge, -side * nudge, up * nudge, -up * nudge};
    int nudged = -1;
    float nudgedT = 1.f;
    for(const glm::vec3& n : nudges)
    {
        float tn = 1.f;
        const int pn = plateOnSegment(ent, *s, a + n, end + n, mask, tn);
        if(pn >= 0 && tn < nudgedT)
        {
            nudged = pn;
            nudgedT = tn;
        }
    }
    if(nudged >= 0 || skin < 0.f)
    {
        return nudged;
    }
    // (and the skin it reaches under a worn plate, within the plate's gap and thickness: that plate)
    const glm::vec3 skinAt = a + (b - a) * skin;
    const float reach = za::max(vr_agarmor_gap.value, 0.f) + za::max(vr_agarmor_thick.value, 0.05f) + 0.25f;
    float nearD = reach;
    const int under = plateNear(ent, *s, &skinAt.x, reach, nearD);
    return under >= 0 && (mask & (1 << under)) ? under : -1;
}

void debugDraw()
{
    // (the Armour Studio's: its subject's chosen piece alone, whatever vr_agarmor_debug)
    const bool picked = studio.ent > 0 && studio.pieces != 0;
    if((!vr_agarmor_debug.value && !picked) || !sv.active)
    {
        return;
    }
    const progs::ServerVm vm; // (the view's: EDICT_NUM and the fields need the server's VM)
    for(int i = svs.maxclients + 1; i < sv.qcvm.num_edicts; i++)
    {
        if(!vr_agarmor_debug.value && i != studio.ent)
        {
            continue;
        }
        edict_t* e = EDICT_NUM(i);
        const int mask = wornMask(e);
        const Shell* s = mask ? serverShell(baseOf(e)) : nullptr;
        if(!s)
        {
            continue;
        }
        PlatePoses pp;
        posePlates(e, *s, mask, pp);
        for(int k = 0; k + 2 < count(s->tris); k += 3)
        {
            const int piece = at(s->verts, at(s->tris, k)).piece;
            if(piece < 0 || piece >= NumPieces || !pp.ok[piece])
            {
                continue;
            }
            // (a sleeve's triangles: the atlas's joint block)
            const glm::vec2 st = at(s->verts, at(s->tris, k)).st;
            const bool sleeveTri = piece >= PauldronL && st.x >= jointBlock.x0 && st.y >= jointBlock.y0 && st.y <= jointBlock.y1;
            const bool studioOne = picked && i == studio.ent;
            if(studioOne && (!((studio.pieces >> piece) & 1) || sleeveTri != studio.sleeve))
            {
                continue;
            }
            glm::vec3 w[3];
            for(int c = 0; c < 3; c++)
            {
                w[c] = pp.origin[piece] + pp.turn[piece] * at(s->verts, at(s->tris, k + c)).p;
            }
            // (the outer face's edges bold, the inner face's and the rim's thin and faint: at 0.06 units every edge was too
            // thin to see on a flat screen or across a room)
            const bool outer = at(s->verts, at(s->tris, k)).layer == 0;
            const float width = outer ? debugOuterWidth : debugInnerWidth;
            glm::vec4 col = studioOne ? glm::vec4{0.2f, 1.f, 1.f, 1.f} : debugColours[static_cast<za::SizeT>(piece)];
            col.a = outer ? 1.f : 0.45f;
            lines::line(w[0], w[1], width, col, col);
            lines::line(w[1], w[2], width, col, col);
            lines::line(w[2], w[0], width, col, col);
        }
    }
}

// The limb whose plates enclose `p` as `ent` is drawn (-1 none): by each piece's grid, its point taken back to the rest pose by
// that plate's frame. Plates stand off a limb, round it: a hit inside them is theirs, whatever bone is nearest.
[[nodiscard]] int tubeAround(edict_t* ent, const Shell& s, const float* p, int only = -1, float within = 1.f)
{
    for(int piece = only >= 0 ? only : static_cast<int>(PauldronL); piece < (only >= 0 ? only + 1 : static_cast<int>(NumPieces)); piece++)
    {
        const PlateTube& tube = s.tubes[piece];
        const PlateFrame pf = tube.grid.rows ? plateFrameOf(s, piece) : PlateFrame{};
        if(!pf.ok)
        {
            continue;
        }
        glm::vec3 posed[3];
        posedAnchors(ent, s, pf, posed);
        const glm::mat3 turn = frameFrom(posed[0], posed[1], posed[2]) * glm::transpose(pf.rest);
        const glm::vec3 q = at(s.body.mdlRest, pf.anchor[0]) + glm::transpose(turn) * (geom::vec(p) - posed[0]) - tube.origin;
        const PlateGrid& g = tube.grid;
        const float h = glm::dot(q, tube.up);
        const float rowF = (h - g.hAt(0)) / (g.hAt(1) - g.hAt(0));
        if(rowF < 0.f || rowF >= static_cast<float>(g.rows))
        {
            continue;
        }
        const int row = static_cast<int>(rowF);
        const glm::vec2 c = at(g.axis, row) + (at(g.axis, row + 1) - at(g.axis, row)) * (rowF - static_cast<float>(row));
        const glm::vec2 rel = glm::vec2{glm::dot(q, tube.fwd), glm::dot(q, tube.left)} - c;
        float az = za::atan2(rel.y, rel.x) - g.azOf(0);
        az = az < 0.f ? az + 2.f * pi : az;
        const int col = za::min(static_cast<int>(az / (2.f * pi) * static_cast<float>(g.cols)), g.cols - 1);
        if(glm::length(rel) <= za::max(g.at2(row, col), g.at2(row + 1, col)) * within)
        {
            return piece;
        }
    }
    return -1;
}

int pieceAt(edict_t* ent, const float* p)
{
    int piece = pieceOfBone(ent, p);
    const char* plateBase = baseOf(ent);
    const Shell* ps = plateBase ? serverShell(plateBase) : nullptr;
    const float reach = za::max(vr_agarmor_gap.value, 0.f) + za::max(vr_agarmor_thick.value, 0.05f) + 1.5f;
    float d = reach;
    const int nearPlate = ps ? plateNear(ent, *ps, p, reach, d, piece) : -1;
    if(nearPlate >= 0 && limbBone[nearPlate] && d <= 0.3f)
    {
        piece = nearPlate; // (on a limb's plate itself)
    }
    else if(ps && piece >= PauldronL && tubeAround(ent, *ps, p, piece) == piece)
    {
        // (a limb's bone and inside its plates: that limb's, whatever other plate is nearer: a knight's forearm folded over his
        // upper arm)
    }
    else
    {
        const int core = ps && piece != Helmet ? tubeAround(ent, *ps, p, -1, 0.8f) : -1; // (well inside a limb's plates, whatever the bone)
        // (on a limb, or off every piece's bone: the limb plate struck, the nearest within a plate's reach of the skin
        // under it; the cuirass's and the helmet's hits stay their bones')
        // (and on a limb plate itself, or just under it: that plate, whatever bone is nearest: a ragdoll's arm on its chest)
        const bool onPlate = d <= za::max(vr_agarmor_gap.value, 0.f) + za::max(vr_agarmor_thick.value, 0.05f) + 0.25f;
        piece = core >= 0 ? core : nearPlate >= 0 && limbBone[nearPlate] && (piece < 0 || limbBone[piece] || onPlate) ? nearPlate : piece;
        // (inside a limb's plates, round it, or the cuirass's near its arm: that limb's; the head's stay the helmet's)
        const int around = ps && core < 0 && piece != Helmet && !(onPlate && nearPlate >= 0 && limbBone[nearPlate]) ? tubeAround(ent, *ps, p) : -1;
        piece = around >= 0 ? around : piece;
    }
    // (behind the cuirass's back cut, its own: the enforcer's power pack; the point taken back to the rest pose by the
    // cuirass's frame as drawn)
    const char* base = piece == Cuirass ? baseOf(ent) : nullptr;
    const float cut = backCutOf(base, Cuirass);
    const Shell* s = cut > -1e8f ? serverShell(base) : nullptr;
    const PlateFrame pf = s ? plateFrameOf(*s, Cuirass) : PlateFrame{};
    if(!pf.ok)
    {
        return piece;
    }
    glm::vec3 posed[3];
    posedAnchors(ent, *s, pf, posed);
    const glm::mat3 turn = frameFrom(posed[0], posed[1], posed[2]) * glm::transpose(pf.rest);
    const glm::vec3 rest = at(s->body.mdlRest, pf.anchor[0]) + glm::transpose(turn) * (geom::vec(p) - posed[0]);
    return rest.x < cut ? -1 : piece;
}

uint32_t pieceBones(edict_t* ent, int piece)
{
    if(piece < 0 || piece >= NumPieces)
    {
        return 0u;
    }
    const char* names[2] = {limbBone[piece], nullptr};
    if(piece == Helmet)
    {
        names[0] = "head";
    }
    else if(piece == Cuirass)
    {
        names[0] = "spine";
        names[1] = "chest";
    }
    uint32_t m = 0u;
    for(const char* n : names)
    {
        const int b = n ? box3d::boneNamed(ent, n) : -1;
        m |= b >= 0 && b < 32 ? 1u << b : 0u;
    }
    return m;
}

namespace
{
// ---- A broken plate in two halves (vr_agarmor_split; ROUND24.md, "Broken armour splits in two") ----
// Its line across its length (its vertices' longest spread: two shorter halves) or down it (the second longest: two long
// halves, the cuirass's left and right), by its kind (vr_agarmor_split_<kind>). Each small plate (a tile, its rims, a hero
// plate's columns each) goes whole to one side, so the seam follows the plates' edges; the line zigzags (splitJag,
// splitTeeth) so the seam is jagged; a joint's leather (one big piece) is torn along its triangles. The halves as many
// triangles each (the line at their median), each its own model ("<model>#plate<piece>h<half>s<axis>", derivedFile).
constexpr float splitJag = 0.15f; // the zigzag's depth either side of the line: this share of the piece's size across it
constexpr float splitTeeth = 3.f; // its teeth along the line
constexpr int splitWhole = 2 * plateVerts; // a joined run of vertices at most this many goes whole to one side
constexpr float splitShortest = 0.6f; // cut across its length only if it is at least this long for its width (else down it)
constexpr float splitWeld = 1e-4f;    // vertices this close are one place (a seam's copies: one plate)

[[nodiscard]] int splitAxisOf(int piece)
{
    const cvar_t& v = piece == Cuirass       ? vr_agarmor_split_cuirass
                      : piece <= PauldronR ? vr_agarmor_split_pauldron
                      : piece <= BracerR   ? vr_agarmor_split_bracer
                      : piece <= ThighR    ? vr_agarmor_split_thigh
                                           : vr_agarmor_split_shin;
    return v.value >= 0.5f ? 1 : 0;
}

struct SplitUnit
{
    float key{0.f};
    int id{0}, tris{0};
};

int compareSplitUnits(const void* a, const void* b)
{
    const float ka = static_cast<const SplitUnit*>(a)->key, kb = static_cast<const SplitUnit*>(b)->key;
    return ka < kb ? -1 : ka > kb ? 1 : 0;
}

[[nodiscard]] int splitRoot(za::Vector<int>& parent, int i)
{
    while(at(parent, i) != i)
    {
        at(parent, i) = at(parent, at(parent, i));
        i = at(parent, i);
    }
    return i;
}

// The covariance `m`'s strongest direction from `x` (power iteration).
[[nodiscard]] glm::vec3 splitPrincipal(const glm::mat3& m, glm::vec3 x)
{
    for(int i = 0; i < 48; i++)
    {
        x = geom::safeNormalize(m * x, x, 1e-12f);
    }
    return x;
}

// Piece `piece`'s triangles on half `half` (0, 1) of its split along `axis` (0 across its length, 1 down it): `out` each
// one's first index into s.tris; `mid` that half's middle (rest space: its triangles' corners averaged). False: it has
// fewer than two plates to share (no split).
struct SplitInfo
{
    int runs{0}, big{0}, biggest{0}; // its plates (runs of vertices joined), those torn along their triangles, the most vertices in one
};

[[nodiscard]] bool splitHalf(const Shell& s, int piece, int axis, int half, za::Vector<int>& out, glm::vec3& mid,
    glm::vec3* cutNormal = nullptr, SplitInfo* stats = nullptr)
{
    out.clear();
    glm::vec3 c{0.f};
    int n = 0;
    for(const Vert& v : s.verts)
    {
        c += v.piece == piece ? v.p : glm::vec3{0.f};
        n += v.piece == piece ? 1 : 0;
    }
    if(n < 6)
    {
        return false;
    }
    c /= static_cast<float>(n);
    glm::mat3 cov{0.f};
    for(const Vert& v : s.verts)
    {
        const glm::vec3 d = v.piece == piece ? v.p - c : glm::vec3{0.f};
        cov += glm::outerProduct(d, d);
    }
    const glm::vec3 e1 = splitPrincipal(cov, glm::normalize(glm::vec3{0.3f, 0.5f, 1.f}));
    const glm::mat3 rest2 = cov - glm::dot(e1, cov * e1) * glm::outerProduct(e1, e1);
    glm::vec3 e2 = splitPrincipal(rest2, geom::safeNormalize(glm::cross(e1, glm::vec3{0.3f, 1.f, 0.2f}), glm::vec3{0.f, 1.f, 0.f}));
    e2 = geom::safeNormalize(e2 - e1 * glm::dot(e2, e1), glm::vec3{0.f, 1.f, 0.f});
    // (its length: its tube's axis, its rings up it, where it has one; else its vertices' longest spread)
    const PlateTube& tube = s.tubes[piece];
    const bool tubed = tube.length > 0.f && glm::length(glm::cross(tube.up, tube.left)) > 0.5f;
    const glm::vec3 lengthWay = tubed ? glm::normalize(tube.up) : e1, acrossWay = tubed ? glm::normalize(tube.left) : e2;
    // (a piece shorter than it is wide (the player's bracers: the elbow's half alone) cut across would fall as two thin
    // rings: down its length instead)
    float lLo = 1e9f, lHi = -1e9f, wLo = 1e9f, wHi = -1e9f;
    for(const Vert& v : s.verts)
    {
        const bool on = v.piece == piece;
        lLo = on ? za::min(lLo, glm::dot(v.p, lengthWay)) : lLo;
        lHi = on ? za::max(lHi, glm::dot(v.p, lengthWay)) : lHi;
        wLo = on ? za::min(wLo, glm::dot(v.p, acrossWay)) : wLo;
        wHi = on ? za::max(wHi, glm::dot(v.p, acrossWay)) : wHi;
    }
    axis = lHi - lLo < splitShortest * (wHi - wLo) ? 1 : axis;
    const glm::vec3 nrm = axis ? acrossWay : lengthWay, along = axis ? lengthWay : acrossWay;
    if(cutNormal)
    {
        *cutNormal = nrm;
    }
    float nLo = 1e9f, nHi = -1e9f, aLo = 1e9f, aHi = -1e9f;
    for(const Vert& v : s.verts)
    {
        const bool on = v.piece == piece;
        nLo = on ? za::min(nLo, glm::dot(v.p, nrm)) : nLo;
        nHi = on ? za::max(nHi, glm::dot(v.p, nrm)) : nHi;
        aLo = on ? za::min(aLo, glm::dot(v.p, along)) : aLo;
        aHi = on ? za::max(aHi, glm::dot(v.p, along)) : aHi;
    }
    const float jag = splitJag * (nHi - nLo), period = za::max((aHi - aLo) / splitTeeth, 1e-3f);
    // Its plates: the runs of vertices its triangles join.
    const int numVerts = count(s.verts);
    za::Vector<int> parent(s.verts.size(), 0);
    za::Vector<int> size(s.verts.size(), 0);
    for(int v = 0; v < numVerts; v++)
    {
        at(parent, v) = v;
    }
    for(int i = 0; i + 2 < count(s.tris); i += 3)
    {
        if(at(s.verts, at(s.tris, i)).piece != piece)
        {
            continue;
        }
        const int a = splitRoot(parent, at(s.tris, i));
        const int b = splitRoot(parent, at(s.tris, i + 1));
        const int d = splitRoot(parent, at(s.tris, i + 2));
        at(parent, b) = a;
        at(parent, splitRoot(parent, d)) = a;
    }
    // (and its copies in one place: a plate's face, bevels and rim are runs of their own, joined only at its seams' copies
    // (their own texture places); split apart they fell open, a rim or a bevel alone a long thin sliver)
    {
        za::Vector<SplitUnit> byX;
        for(int v = 0; v < numVerts; v++)
        {
            if(at(s.verts, v).piece == piece)
            {
                byX.pushBack(SplitUnit{at(s.verts, v).p.x, v, 0});
            }
        }
        qsort(byX.data(), byX.size(), sizeof(SplitUnit), compareSplitUnits);
        for(int i = 0; i < count(byX); i++)
        {
            for(int k = i + 1; k < count(byX) && at(byX, k).key - at(byX, i).key < splitWeld; k++)
            {
                const int u = at(byX, i).id, v = at(byX, k).id;
                if(glm::length(at(s.verts, u).p - at(s.verts, v).p) < splitWeld)
                {
                    at(parent, splitRoot(parent, v)) = splitRoot(parent, u);
                }
            }
        }
    }
    for(int v = 0; v < numVerts; v++)
    {
        at(size, splitRoot(parent, v)) += at(s.verts, v).piece == piece ? 1 : 0;
    }
    if(stats)
    {
        *stats = SplitInfo{};
        for(int v = 0; v < numVerts; v++)
        {
            const bool root = at(s.verts, v).piece == piece && splitRoot(parent, v) == v;
            stats->runs += root ? 1 : 0;
            stats->big += root && at(size, v) > splitWhole ? 1 : 0;
            stats->biggest = root ? za::max(stats->biggest, at(size, v)) : stats->biggest;
        }
    }
    // Each plate one unit (a big run: each of its triangles one), its key its middle's place across the zigzag line.
    za::Vector<int> unitOf(s.verts.size(), -1);
    za::Vector<int> triUnit;
    za::Vector<glm::vec3> unitSum;
    za::Vector<int> unitTris;
    for(int i = 0; i + 2 < count(s.tris); i += 3)
    {
        if(at(s.verts, at(s.tris, i)).piece != piece)
        {
            continue;
        }
        const int r = splitRoot(parent, at(s.tris, i));
        int u = at(size, r) <= splitWhole ? at(unitOf, r) : -1;
        if(u < 0)
        {
            u = count(unitSum);
            unitSum.pushBack(glm::vec3{0.f});
            unitTris.pushBack(0);
            at(unitOf, r) = at(size, r) <= splitWhole ? u : -1;
        }
        at(unitSum, u) += (at(s.verts, at(s.tris, i)).p + at(s.verts, at(s.tris, i + 1)).p + at(s.verts, at(s.tris, i + 2)).p) / 3.f;
        at(unitTris, u) += 1;
        triUnit.pushBack(u);
    }
    const int numUnits = count(unitSum);
    if(numUnits < 2)
    {
        return false;
    }
    za::Vector<SplitUnit> units(static_cast<za::SizeT>(numUnits), SplitUnit{});
    for(int u = 0; u < numUnits; u++)
    {
        const glm::vec3 m = at(unitSum, u) / static_cast<float>(at(unitTris, u));
        const float x = glm::dot(m, along) / period;
        const float zig = 1.f - 4.f * za::abs(x - floorf(x) - 0.5f); // (-1..1, a triangle wave)
        at(units, u) = SplitUnit{glm::dot(m, nrm) - jag * zig, u, at(unitTris, u)};
    }
    qsort(units.data(), static_cast<size_t>(numUnits), sizeof(SplitUnit), compareSplitUnits);
    // The first units up to half the triangles one side (at least one either side).
    za::Vector<int> sideOf(static_cast<za::SizeT>(numUnits), 1);
    const int total = count(triUnit);
    int sum = 0;
    for(int k = 0; k < numUnits - 1 && (k == 0 || 2 * sum + at(units, k).tris <= total); k++)
    {
        at(sideOf, at(units, k).id) = 0;
        sum += at(units, k).tris;
    }
    mid = glm::vec3{0.f};
    int t = 0;
    for(int i = 0, k = 0; i + 2 < count(s.tris); i += 3)
    {
        if(at(s.verts, at(s.tris, i)).piece != piece)
        {
            continue;
        }
        if(at(sideOf, at(triUnit, k++)) == half)
        {
            out.pushBack(i);
            mid += at(s.verts, at(s.tris, i)).p + at(s.verts, at(s.tris, i + 1)).p + at(s.verts, at(s.tris, i + 2)).p;
            t += 3;
        }
    }
    mid /= static_cast<float>(za::max(t, 1));
    return !out.empty();
}

// Names made for halfModel (PR_SetEngineString keeps their pointers: never moved, never freed).
constexpr int maxHalfNames = 512;
char halfNames[maxHalfNames][MAX_QPATH]{};
int numHalfNames = 0;
} // namespace

const char* halfModel(edict_t* ent, int piece, int half)
{
    const int num = ent ? NUM_FOR_EDICT(ent) : 0;
    // (the player's plates' debris: vrbody.mdl's, whatever build is drawn, as QC's VR_PP_Shed names them)
    const char* base = num >= 1 && num <= svs.maxclients ? "progs/vrbody.mdl" : baseOf(ent);
    if(!base || piece < 0 || piece >= NumPieces || piece == Helmet || half < 0 || half > 1)
    {
        return "";
    }
    const Shell* s = serverShell(base);
    const int axis = splitAxisOf(piece);
    za::Vector<int> tris;
    glm::vec3 mid;
    if(!s || !plateFrameOf(*s, piece).ok || !splitHalf(*s, piece, axis, half, tris, mid))
    {
        return "";
    }
    char want[MAX_QPATH];
    q_snprintf(want, sizeof(want), "%s#plate%dh%ds%d", base, piece, half, axis);
    for(int i = 0; i < numHalfNames; i++)
    {
        if(!strcmp(halfNames[i], want))
        {
            return halfNames[i];
        }
    }
    if(numHalfNames >= maxHalfNames)
    {
        return "";
    }
    q_strlcpy(halfNames[numHalfNames], want, MAX_QPATH);
    return halfNames[numHalfNames++];
}

void plateWhere(edict_t* ent, int piece, int what, float* out)
{
    out[0] = out[1] = out[2] = 0.f;
    const int num = ent ? NUM_FOR_EDICT(ent) : 0;
    if(num >= 1 && num <= svs.maxclients)
    {
        // The player's plate: this machine's own player, as its body is drawn (another's: nowhere; QC does without).
        const entity_t* body = num == cl.viewentity ? avatar::posedEntity() : nullptr;
        const float* skin = body ? avatar::skinWithHead() : nullptr;
        const Shell* mine = skin && piece >= 0 && piece < NumPieces ? shellFor(body->model) : nullptr;
        const PlateFrame pf = mine ? plateFrameOf(*mine, piece) : PlateFrame{};
        if(!pf.ok || pf.bone < 0)
        {
            return;
        }
        const float* m = skin + pf.bone * 12;
        if(what == 0 || what == 6 || what == 7)
        {
            // (6, 7: a split half's middle, halfModel's)
            glm::vec3 p = pf.mid;
            za::Vector<int> tris;
            if(what != 0 && !splitHalf(*mine, piece, splitAxisOf(piece), what - 6, tris, p))
            {
                return;
            }
            const float k = avatar::modelScale(body); // (the skinning matrices stop short of the body's scale)
            for(int i = 0; i < 3; i++)
            {
                out[i] = body->origin[i] + k * (m[i * 4] * p.x + m[i * 4 + 1] * p.y + m[i * 4 + 2] * p.z + m[i * 4 + 3]);
            }
        }
        else if(what == 1)
        {
            const glm::mat3 rot{geom::safeNormalize(glm::vec3{m[0], m[4], m[8]}, glm::vec3{1.f, 0.f, 0.f}, 1e-6f),
                geom::safeNormalize(glm::vec3{m[1], m[5], m[9]}, glm::vec3{0.f, 1.f, 0.f}, 1e-6f),
                geom::safeNormalize(glm::vec3{m[2], m[6], m[10]}, glm::vec3{0.f, 0.f, 1.f}, 1e-6f)};
            held::anglesFromAxes(rot * pf.rest, out, false);
        }
        return;
    }
    const Shell* s = serverShell(baseOf(ent));
    if(!s || piece < 0 || piece >= NumPieces)
    {
        return;
    }
    const PlateFrame pf = plateFrameOf(*s, piece);
    if(!pf.ok)
    {
        return;
    }
    glm::vec3 posed[3];
    posedAnchors(ent, *s, pf, posed);
    const glm::mat3 now = frameFrom(posed[0], posed[1], posed[2]);
    const glm::mat3 turn = now * glm::transpose(pf.rest);
    if(what == 6 || what == 7)
    {
        // A split half's middle (halfModel's model's origin), as drawn.
        glm::vec3 p;
        za::Vector<int> tris;
        if(splitHalf(*s, piece, splitAxisOf(piece), what - 6, tris, p))
        {
            const glm::vec3 o = posed[0] + turn * (p - at(s->body.mdlRest, pf.anchor[0]));
            out[0] = o.x;
            out[1] = o.y;
            out[2] = o.z;
        }
        return;
    }
    if(what == 0 || (what >= 2 && what <= 5))
    {
        const glm::vec3 restA = at(s->body.mdlRest, pf.anchor[0]);
        // (2, 3, 4: a point on its outer face 15% / 85% / 50% of its length along its frame's first axis: its ends, its
        // middle; 5: the outer face's outward normal there, a direction)
        glm::vec3 restP = pf.mid;
        if(what != 0)
        {
            const glm::vec3 axis = pf.rest[0];
            float lo = 1e9f, hi = -1e9f;
            for(const Vert& v : s->verts)
            {
                const float x = glm::dot(v.p, axis);
                lo = v.piece == piece && v.layer == 0 ? za::min(lo, x) : lo;
                hi = v.piece == piece && v.layer == 0 ? za::max(hi, x) : hi;
            }
            const float want = lo + (hi - lo) * (what == 2 ? 0.15f : what == 3 ? 0.85f : 0.5f);
            // (the outer vertex nearest the face's own middle at that length: the outer face's centroid moved to it
            // along the axis; for a wrapped limb plate, the middle of its arc. Before, the first vertex of the ring at
            // that length, anywhere around it, up to its long edge)
            glm::vec3 centroid{0.f};
            int n = 0;
            int firstOf = -1;
            for(int vi = 0; vi < count(s->verts); vi++)
            {
                const Vert& v = at(s->verts, vi);
                firstOf = firstOf < 0 && v.piece == piece ? vi : firstOf;
                centroid += v.piece == piece && v.layer == 0 ? v.p : glm::vec3{0.f};
                n += v.piece == piece && v.layer == 0 ? 1 : 0;
            }
            centroid /= static_cast<float>(za::max(n, 1));
            const glm::vec3 aim = centroid + axis * (want - glm::dot(centroid, axis));
            float best = 1e9f;
            for(const Vert& v : s->verts)
            {
                const float d = glm::distance(v.p, aim);
                restP = v.piece == piece && v.layer == 0 && d < best ? v.p : restP;
                best = v.piece == piece && v.layer == 0 && d < best ? d : best;
            }
            // (a limb's or the cuirass's: the middle of that plate's face, its face's vertices first: nothing shared there)
            if(piece != Helmet && firstOf >= 0)
            {
                best = 1e9f;
                int plateFirst = firstOf;
                for(int vi = firstOf; vi + 3 < count(s->verts) && at(s->verts, vi).piece == piece && at(s->verts, vi).layer != 3; vi += plateVerts)
                {
                    const glm::vec3 faceMid = (at(s->verts, vi).p + at(s->verts, vi + 1).p + at(s->verts, vi + 2).p + at(s->verts, vi + 3).p) * 0.25f;
                    const float d = glm::distance(faceMid, restP);
                    plateFirst = d < best ? vi : plateFirst;
                    best = za::min(best, d);
                }
                restP = (at(s->verts, plateFirst).p + at(s->verts, plateFirst + 1).p + at(s->verts, plateFirst + 2).p) / 3.f; // (exactly on one of its face's triangles)
            }
        }
        if(what == 5)
        {
            // (the outer triangles at that vertex, their normals summed; turned away from the plate's inner face)
            glm::vec3 nrm{0.f};
            for(int k = 0; k + 2 < count(s->tris); k += 3)
            {
                const Vert& v0 = at(s->verts, at(s->tris, k));
                const Vert& v1 = at(s->verts, at(s->tris, k + 1));
                const Vert& v2 = at(s->verts, at(s->tris, k + 2));
                const bool touches = glm::distance(v0.p, restP) < 1e-3f || glm::distance(v1.p, restP) < 1e-3f ||
                                     glm::distance(v2.p, restP) < 1e-3f;
                nrm += v0.piece == piece && v0.layer == 0 && touches ? glm::cross(v1.p - v0.p, v2.p - v0.p) : glm::vec3{0.f};
            }
            glm::vec3 inner = restP;
            float innerD = 1e9f;
            for(const Vert& v : s->verts)
            {
                const float d = glm::distance(v.p, restP);
                inner = v.piece == piece && v.layer == 1 && d < innerD ? v.p : inner;
                innerD = v.piece == piece && v.layer == 1 && d < innerD ? d : innerD;
            }
            nrm = glm::dot(nrm, restP - inner) < 0.f ? -nrm : nrm;
            const glm::vec3 w = geom::safeNormalize(turn * nrm, glm::vec3{0.f}, 1e-9f);
            out[0] = w.x;
            out[1] = w.y;
            out[2] = w.z;
            return;
        }
        const glm::vec3 o = posed[0] + turn * (restP - restA);
        out[0] = o.x;
        out[1] = o.y;
        out[2] = o.z;
        return;
    }
    held::anglesFromAxes(now, out, false);
}

namespace
{
// progs/backpack.mdl's bounds (id1's; units): its front (+x) against the back, upright.
constexpr glm::vec3 packLo{-10.01f, -8.29f, 0.f}, packHi{5.23f, 8.14f, 24.78f};
constexpr float packChest = 0.62f; // its middle this far up the cuirass's height (its upper back)

// The fit's sliders, by model (each kind's page in the menu; read every frame: they show at once). Any other wearer model: the
// grunt's.
struct PackFit
{
    const char* model;
    const cvar_t& scale;
    const cvar_t& x;
    const cvar_t& y;
    const cvar_t& z;
    const cvar_t& pitch;
    const cvar_t& yaw;
    const cvar_t& roll;
};
const PackFit packFits[] = {
    {"progs/vrgrunt.mdl", vr_worn_backpack_grunt_scale, vr_worn_backpack_grunt_x, vr_worn_backpack_grunt_y, vr_worn_backpack_grunt_z,
        vr_worn_backpack_grunt_pitch, vr_worn_backpack_grunt_yaw, vr_worn_backpack_grunt_roll},
    {"progs/vrknight.mdl", vr_worn_backpack_knight_scale, vr_worn_backpack_knight_x, vr_worn_backpack_knight_y, vr_worn_backpack_knight_z,
        vr_worn_backpack_knight_pitch, vr_worn_backpack_knight_yaw, vr_worn_backpack_knight_roll},
    {"progs/vrhknight.mdl", vr_worn_backpack_dknight_scale, vr_worn_backpack_dknight_x, vr_worn_backpack_dknight_y, vr_worn_backpack_dknight_z,
        vr_worn_backpack_dknight_pitch, vr_worn_backpack_dknight_yaw, vr_worn_backpack_dknight_roll},
};

[[nodiscard]] const PackFit& packFitOf(const char* base)
{
    for(const PackFit& f : packFits)
    {
        if(base && !strcmp(base, f.model))
        {
            return f;
        }
    }
    return packFits[0];
}

// The pack in the model (QC vr_wornpack.qc's lift zone: nothing drawn, nothing moved), the box round it at rest: the enforcer's,
// its vertices behind the breastplate, Wearer::backCut -10.5, weighing mostly on the chest bone: x to -23.1, y -11..10, z 5 to 34
// over its head; the feet's and shins' stray ones under it left out.
constexpr const char* bakedPackModel = "progs/vrenforcer.mdl";
constexpr glm::vec3 bakedPackLo{-23.1f, -11.f, 5.f}, bakedPackHi{-10.5f, 10.f, 34.f};
} // namespace

bool packBaked(edict_t* ent)
{
    const char* base = baseOf(ent);
    return base && !strcmp(base, bakedPackModel);
}

void packModelBoxSized(float size, glm::vec3& mid, glm::vec3& half)
{
    const float k = za::clamp(size, 0.1f, 2.f);
    mid = (packLo + packHi) * 0.5f * k;
    half = (packHi - packLo) * 0.5f * k;
}

void packModelBox(edict_t* ent, glm::vec3& mid, glm::vec3& half)
{
    packModelBoxSized(packFitOf(baseOf(ent)).scale.value, mid, half);
}

namespace
{
// packRest's answer for a shell (by its address, checked against its make: generation, tweaks, hash, vertices) and the
// fit's cvars it was made with (ROUND24.md, "Tracy session 11-07": the worn packs' place asked for it for every wearer a frame).
struct PackRestCached
{
    const Shell* shell{nullptr};
    int generation{-1};
    int tweaksGen{-1};
    uint32_t hash{0};
    za::SizeT verts{0};
    float fit[7]{};
    bool ok{false};
    glm::vec3 restOrigin{0.f};
    glm::mat3 restAxes{1.f};
    glm::vec3 mid{0.f};
    glm::vec3 half{0.f};
};
za::Vector<PackRestCached> packRestCache;
constexpr za::SizeT maxPackRestCached = 64; // more than the packs worn at once; all dropped when it would hold more

bool packRestMake(const Shell& shell, const PackFit& fit, glm::vec3& restOrigin, glm::mat3& restAxes, glm::vec3& mid, glm::vec3& half);
} // namespace

bool packRest(edict_t* ent, glm::vec3& restOrigin, glm::mat3& restAxes, glm::vec3& mid, glm::vec3& half)
{
    const Shell* s = serverShell(baseOf(ent));
    if(!s)
    {
        return false;
    }
    if(packBaked(ent))
    {
        restOrigin = glm::vec3{0.f};
        restAxes = glm::mat3{1.f};
        mid = (bakedPackLo + bakedPackHi) * 0.5f;
        half = (bakedPackHi - bakedPackLo) * 0.5f;
        return true;
    }
    const PackFit& fit = packFitOf(baseOf(ent));
    // (made once for its shell as it is and its fit: it walked the shell's vertices twice for every wearer a frame)
    const float fitNow[7] = {fit.scale.value, fit.x.value, fit.y.value, fit.z.value, fit.pitch.value, fit.yaw.value, fit.roll.value};
    PackRestCached* cached = nullptr;
    for(PackRestCached& c : packRestCache)
    {
        cached = c.shell == s ? &c : cached;
    }
    if(!cached)
    {
        if(packRestCache.size() >= maxPackRestCached)
        {
            packRestCache.clear(); // (keyed by shell address: a session's remade shells would add one each, never used again)
        }
        packRestCache.pushBack(PackRestCached{});
        cached = &packRestCache.back();
        cached->shell = s;
    }
    if(cached->generation == s->generation && cached->tweaksGen == s->tweaksGen && cached->hash == s->hash &&
       cached->verts == s->verts.size() && !memcmp(cached->fit, fitNow, sizeof(fitNow)))
    {
        restOrigin = cached->restOrigin;
        restAxes = cached->restAxes;
        mid = cached->mid;
        half = cached->half;
        return cached->ok;
    }
    cached->generation = s->generation;
    cached->tweaksGen = s->tweaksGen;
    cached->hash = s->hash;
    cached->verts = s->verts.size();
    memcpy(cached->fit, fitNow, sizeof(fitNow));
    cached->ok = packRestMake(*s, fit, cached->restOrigin, cached->restAxes, cached->mid, cached->half);
    restOrigin = cached->restOrigin;
    restAxes = cached->restAxes;
    mid = cached->mid;
    half = cached->half;
    return cached->ok;
}

namespace
{
bool packRestMake(const Shell& shell, const PackFit& fit, glm::vec3& restOrigin, glm::mat3& restAxes, glm::vec3& mid, glm::vec3& half)
{
    const Shell* s = &shell;
    // At rest: the cuirass's height's span and its middle across; its back the outer face's least x where the pack lies
    // against it (within the pack's height and width there: not the small of the back below it).
    float lo = 1e9f, hi = -1e9f, across = 0.f;
    int n = 0;
    for(const Vert& v : s->verts)
    {
        const bool on = v.piece == Cuirass && v.layer == 0;
        lo = on ? za::min(lo, v.p.z) : lo;
        hi = on ? za::max(hi, v.p.z) : hi;
        across += on ? v.p.y : 0.f;
        n += on ? 1 : 0;
    }
    if(n == 0)
    {
        return false;
    }
    packModelBoxSized(fit.scale.value, mid, half);
    const float midZ = lo + (hi - lo) * packChest + fit.z.value;
    const float midY = across / static_cast<float>(n) + fit.y.value;
    float back = 1e9f;
    for(const Vert& v : s->verts)
    {
        const bool on = v.piece == Cuirass && v.layer == 0 && za::abs(v.p.z - midZ) <= half.z && za::abs(v.p.y - midY) <= half.y;
        back = on ? za::min(back, v.p.x) : back;
    }
    if(back > 1e8f)
    {
        return false;
    }
    const float k = za::clamp(fit.scale.value, 0.1f, 2.f);
    restOrigin = glm::vec3{back - packHi.x * k + fit.x.value, midY, midZ - mid.z};
    const float turnAngles[3] = {fit.pitch.value, fit.yaw.value, fit.roll.value};
    restAxes = held::axesFromAngles(turnAngles, false);
    return true;
}
} // namespace

int packBone(edict_t* ent)
{
    const char* base = baseOf(ent);
    const rig2::Rig* rig = base ? rig2::rigFor(Mod_ForName(base, false)) : nullptr;
    return rig ? rig2::boneByName(*rig, "chest") : -1;
}

bool packWhere(edict_t* ent, glm::vec3& origin, glm::mat3& axes, glm::vec3& mid, glm::vec3& half, bool drawnOk)
{
    glm::vec3 restOrigin;
    glm::mat3 restAxes;
    if(!packRest(ent, restOrigin, restAxes, mid, half))
    {
        return false;
    }
    // It rides the chest bone (the upper back it lies on: the cuirass's own frame is the spine's, which leans less) as the
    // body is drawn now (drawnpose::bone: a live one on the authored rig bent to take a weapon, a ragdoll's chest part)...
    const int chest = packBone(ent);
    rigpose::Rigid drawn;
    float scale = 1.f;
    if(drawnOk && chest >= 0 && drawnpose::bone(ent, chest, drawn, scale) != drawnpose::Source::None)
    {
        origin = drawn.pos + drawn.rot * (scale * restOrigin);
        axes = glm::mat3_cast(drawn.rot) * restAxes;
        mid = origin + axes * mid;
        return true;
    }
    // ... else (not drawn lately: a dedicated server, out of view) as its frame poses it: its anchors the chest's vertices
    // nearest the pack (hitmodel's pose); the cuirass's if the rig has no chest.
    const Shell* s = serverShell(baseOf(ent));
    PlateFrame near;
    near.mid = restOrigin + restAxes * mid;
    PlateFrame pf = chest >= 0 ? frameOnBone(*s, chest, near) : PlateFrame{};
    pf = pf.ok ? pf : plateFrameOf(*s, Cuirass);
    if(!pf.ok)
    {
        return false;
    }
    glm::vec3 posed[3];
    posedAnchors(ent, *s, pf, posed);
    const glm::mat3 turn = frameFrom(posed[0], posed[1], posed[2]) * glm::transpose(pf.rest);
    origin = posed[0] + turn * (restOrigin - at(s->body.mdlRest, pf.anchor[0]));
    axes = turn * restAxes;
    mid = origin + axes * mid;
    return true;
}

qmodel_t* playerArmorModel(qmodel_t* body)
{
    return body && isPlayerBody(body->name) ? armorFor(body) : nullptr;
}

qmodel_t* playerBridgeModel(qmodel_t* body)
{
    if(!body || !isPlayerBody(body->name) || !armorFor(body))
    {
        return nullptr;
    }
    const int i = shellNamed(body->name);
    return i >= 0 && at(shells, i).bridgeVerts > 0 ? bridgeFor(body) : nullptr;
}

int playerBridgePoses(const qmodel_t* body, int pieces, const float* bones, int have, const float** out)
{
    const int i = body ? shellNamed(body->name) : -1;
    const Shell* s = i >= 0 ? &at(shells, i) : nullptr;
    const int n = s ? s->bridgeVerts : 0;
    if(n <= 0 || !out)
    {
        return n;
    }
    // (once a frame: the views' passes differ in the head alone)
    if(playerBridgeFrame != host_framecount || playerBridgePieces != pieces)
    {
        playerBridgeFrame = host_framecount;
        playerBridgePieces = pieces;
        (void)bridgeBones(*s, pieces, bones, bones ? have : 0, playerBridgeBones);
    }
    *out = playerBridgeBones;
    return n;
}

void benchPose()
{
    const float bend = vr_agarmor_bend.value, frameAt = vr_agarmor_pose_frame.value;
    if((bend == 0.f && frameAt < 0.f) || cls.state != ca_connected || !cl_entities)
    {
        return;
    }
    // (every armoured body drawn on the authored rig: its rest pose, its elbows and knees bent `bend` degrees within their range)
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        entity_t* e = cl_visedicts[i];
        if(!e || !e->model || e < cl_entities || e >= cl_entities + cl_max_edicts || strchr(e->model->name, '#'))
        {
            continue;
        }
        const int num = static_cast<int>(e - cl_entities);
        int type = 0, pieces = 0, wear = 0;
        wornBy(num, type, pieces, wear);
        const rig2::Rig* rig = type > 0 && pieces ? rig2::rigFor(e->model) : nullptr;
        if(!rig)
        {
            continue;
        }
        rigpose::Rigid F[rig2::maxBones];
        const ragdoll::Rig* rr = frameAt >= 0.f ? ragdoll::rigFor(e->model) : nullptr;
        if(rr && rr->authored && rr->numBones == rig->numBones && rr->numPoses > 1)
        {
            // (vr_agarmor_pose_frame: its animation's frames as the game lerps them, held at one place: the leather's
            // steps, vr_agarmor_bridge_check's worst, seen)
            const int p = za::min(static_cast<int>(frameAt), rr->numPoses - 2);
            rig2::posedBetween(*rr, p, p + 1, za::min(frameAt - static_cast<float>(p), 1.f), F);
            rig2::drawPosed(num, e->model, F, rig->numBones);
            continue;
        }
        rig2::Pose pose = rig2::restPose(*rig);
        for(const char* name : {"forearm_l", "forearm_r", "shin_l", "shin_r"})
        {
            const int b = rig2::boneByName(*rig, name);
            if(b < 0 || rig->bones[b].joint != rig2::Joint::Hinge)
            {
                continue;
            }
            const glm::vec2 r = rig->bones[b].range;
            pose.a[b].x = r.y >= -r.x ? za::min(za::abs(bend), r.y) : za::max(-za::abs(bend), r.x);
        }
        rig2::forward(*rig, pose, F);
        rig2::drawPosed(num, e->model, F, rig->numBones);
    }
}

bool playerSlots(const qmodel_t* body, int& numSlots, const int*& slotBone, const int*& slotPiece)
{
    const int i = body ? shellNamed(body->name) : -1;
    if(i < 0)
    {
        return false;
    }
    const Shell& s = at(shells, i);
    numSlots = s.numSlots;
    slotBone = s.slotBone;
    slotPiece = s.slotPiece;
    return numSlots > 0;
}

unsigned char* derivedFile(const char* name, unsigned int* path_id)
{
    const char* hash = strstr(name, "#plate");
    if(!hash)
    {
        return nullptr;
    }
    char base[MAX_QPATH];
    q_strlcpy(base, name, za::min(sizeof(base), static_cast<size_t>(hash - name) + 1));
    const int piece = Q_atoi(hash + 6);
    // ("<model>#plate<piece>h<half>s<axis>": a split half, halfModel's)
    const char* halfAt = strchr(hash + 6, 'h');
    const int half = halfAt ? Q_atoi(halfAt + 1) : -1;
    const char* axisAt = halfAt ? strchr(halfAt, 's') : nullptr;
    const int axis = axisAt ? Q_atoi(axisAt + 1) : 0;
    const Shell* s = isWearer(base) ? serverShell(base) : nullptr;
    if(!s || piece < 0 || piece >= NumPieces)
    {
        return nullptr;
    }
    const PlateFrame pf = plateFrameOf(*s, piece);
    unsigned int skinPath = 0;
    byte* skin = pf.ok ? COM_LoadMallocFile(skinModelName, &skinPath) : nullptr;
    if(!skin)
    {
        return nullptr;
    }
    const size_t skinSize = static_cast<size_t>(com_filesize);
    int hdr[3] = {0, 0, 0}; // numskins, width, height
    memcpy(hdr, skin + 48, sizeof(hdr));
    const size_t skinBytes = static_cast<size_t>(hdr[0]) * (4u + static_cast<size_t>(hdr[1]) * static_cast<size_t>(hdr[2]));
    if(skinSize < 84u + skinBytes || hdr[0] <= 0)
    {
        VR_HeapFree(skin);
        return nullptr;
    }
    // A half: its triangles and their vertices alone, round its own middle (plateWhere 6 / 7 puts it there).
    za::Vector<int> halfTris;
    glm::vec3 mid = pf.mid;
    glm::vec3 cutNormal{0.f};
    const bool split = half >= 0 && splitHalf(*s, piece, axis, half, halfTris, mid, &cutNormal);
    if(half >= 0)
    {
        // (developer 1: each half's line, its normal in the body's rest space: the cuirass down the middle +-y)
        Con_DPrintf("agarmor: %s: %s, %d triangles, its line's normal %.2f %.2f %.2f\n", name, split ? "a half" : "no split (whole)",
            count(halfTris), cutNormal.x, cutNormal.y, cutNormal.z);
    }
    za::Vector<char> used(s->verts.size(), split ? 0 : 1);
    za::Vector<char> triUsed(s->tris.size() / 3u, split ? 0 : 1);
    for(const int i : halfTris)
    {
        at(triUsed, i / 3) = 1;
        for(int k = 0; k < 3; k++)
        {
            at(used, at(s->tris, i + k)) = 1;
        }
    }
    // Its vertices in its own frame (rest -> the frame's axes, round its middle).
    za::Vector<int> map(s->verts.size(), -1);
    za::Vector<glm::vec3> local;
    za::Vector<glm::vec2> st;
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(int v = 0; v < count(s->verts); v++)
    {
        if(at(s->verts, v).piece == piece && at(used, v))
        {
            at(map, v) = count(local);
            const glm::vec3 q = glm::transpose(pf.rest) * (at(s->verts, v).p - mid);
            local.pushBack(q);
            st.pushBack(at(s->verts, v).st);
            lo = glm::min(lo, q);
            hi = glm::max(hi, q);
        }
    }
    za::Vector<int> tris;
    for(int i = 0; i + 2 < count(s->tris); i += 3)
    {
        if(at(map, at(s->tris, i)) >= 0 && at(triUsed, i / 3))
        {
            tris.pushBack(at(map, at(s->tris, i)));
            tris.pushBack(at(map, at(s->tris, s->body.nativeFlip ? i + 2 : i + 1)));
            tris.pushBack(at(map, at(s->tris, s->body.nativeFlip ? i + 1 : i + 2)));
        }
    }
    const glm::vec3 scale = glm::max((hi - lo) / 255.f, glm::vec3{1e-4f});
    za::Vector<byte> f;
    putWord(f, IDPOLYHEADER);
    putWord(f, ALIAS_VERSION);
    for(int i = 0; i < 3; i++)
    {
        putFloat(f, scale[i]);
    }
    for(int i = 0; i < 3; i++)
    {
        putFloat(f, lo[i]);
    }
    putFloat(f, glm::length(glm::max(glm::abs(lo), glm::abs(hi))));
    for(int i = 0; i < 3; i++)
    {
        putFloat(f, 0.f);
    }
    putWord(f, hdr[0]);
    putWord(f, hdr[1]);
    putWord(f, hdr[2]);
    putWord(f, count(local));
    putWord(f, count(tris) / 3);
    putWord(f, 1);
    putWord(f, 0); // synctype
    putWord(f, 0); // flags
    putFloat(f, 8.f);
    for(size_t i = 0; i < skinBytes; i++)
    {
        f.pushBack(skin[84 + i]);
    }
    VR_HeapFree(skin);
    for(const glm::vec2& t : st)
    {
        putWord(f, 0);
        putWord(f, za::clamp(static_cast<int>(t.x), 0, hdr[1] - 1));
        putWord(f, za::clamp(static_cast<int>(t.y), 0, hdr[2] - 1));
    }
    for(int i = 0; i + 2 < count(tris); i += 3)
    {
        putWord(f, 1);
        putWord(f, at(tris, i));
        putWord(f, at(tris, i + 1));
        putWord(f, at(tris, i + 2));
    }
    putWord(f, 0); // a single frame
    const byte box[8] = {0, 0, 0, 0, 255, 255, 255, 0};
    for(byte b : box)
    {
        f.pushBack(b);
    }
    for(int i = 0; i < 16; i++)
    {
        f.pushBack(static_cast<byte>(i < 5 ? "plate"[i] : 0));
    }
    for(const glm::vec3& q : local)
    {
        for(int i = 0; i < 3; i++)
        {
            f.pushBack(static_cast<byte>(za::clamp(static_cast<int>((q[i] - lo[i]) / scale[i] + 0.5f), 0, 255)));
        }
        f.pushBack(0);
    }
    byte* out = static_cast<byte*>(VR_HeapMalloc(f.size()));
    memcpy(out, f.data(), f.size());
    com_filesize = static_cast<int>(f.size());
    if(path_id)
    {
        *path_id = skinPath;
    }
    return out;
}

namespace
{
// ---- vr_agarmor_split_check: every split half's geometry as its model has it (ROUND24.md, "Armour halves' geometry") ----
// Each half model derivedFile makes, read back: its edges (by place, as quantised) each used twice and wound against each
// other (closed, no face turned in), one island (its plates no further apart than splitCheckNear), no triangle (but those
// no higher than its step, drawn as lines) thinner than splitCheckLeast (4 sqrt3 area over the squared edges summed: 1
// equilateral) unless the whole plate has one as thin (a hero plate's long lip: within splitCheckSlack of the whole's
// thinnest, as its own quantising moves it), none of its islands a thin strip (its second spread at least
// splitCheckStrip of its first), and no plate torn along its triangles.
constexpr float splitCheckNear = 0.6f;
constexpr float splitCheckLeast = 0.05f;
constexpr float splitCheckSlack = 0.5f;
constexpr float splitCheckStrip = 0.2f;

struct SplitStats
{
    int tris{0}, islands{0}, open{0}, flipped{0}, slivers{0}, flat{0};
    float worstQ{1.f}, thinnest{1.f};
    float worstLong{0.f}, worstArea{0.f}; // (the thinnest triangle's longest edge and its area)
};

[[nodiscard]] int readInt(const unsigned char* f, size_t at)
{
    int v = 0;
    memcpy(&v, f + at, sizeof(v));
    return v;
}

[[nodiscard]] bool measureModel(const unsigned char* f, size_t size, SplitStats& st)
{
    st = SplitStats{};
    if(size < 84u)
    {
        return false;
    }
    glm::vec3 scale, lo;
    memcpy(&scale, f + 8, sizeof(scale));
    memcpy(&lo, f + 20, sizeof(lo));
    const int numSkins = readInt(f, 48), w = readInt(f, 52), h = readInt(f, 56), numVerts = readInt(f, 60), numTris = readInt(f, 64);
    const size_t trisAt = 84u + static_cast<size_t>(numSkins) * (4u + static_cast<size_t>(w) * static_cast<size_t>(h)) + static_cast<size_t>(numVerts) * 12u;
    const size_t vertsAt = trisAt + static_cast<size_t>(numTris) * 16u + 28u;
    if(numVerts <= 0 || numTris <= 0 || vertsAt + static_cast<size_t>(numVerts) * 4u > size)
    {
        return false;
    }
    // (welded by place: the quantised bytes)
    za::Vector<int> weld(static_cast<za::SizeT>(numVerts), 0);
    za::Vector<glm::vec3> p(static_cast<za::SizeT>(numVerts), glm::vec3{0.f});
    for(int v = 0; v < numVerts; v++)
    {
        const unsigned char* b = f + vertsAt + static_cast<size_t>(v) * 4u;
        at(p, v) = lo + scale * glm::vec3{static_cast<float>(b[0]), static_cast<float>(b[1]), static_cast<float>(b[2])};
        at(weld, v) = v;
        for(int u = 0; u < v; u++)
        {
            const unsigned char* c = f + vertsAt + static_cast<size_t>(u) * 4u;
            if(at(weld, u) == u && c[0] == b[0] && c[1] == b[1] && c[2] == b[2])
            {
                at(weld, v) = u;
                break;
            }
        }
    }
    EdgeMap uses, turn;
    uses.init(numTris * 3);
    turn.init(numTris * 3);
    za::Vector<int> parent(static_cast<za::SizeT>(numVerts), 0);
    for(int v = 0; v < numVerts; v++)
    {
        at(parent, v) = at(weld, v);
    }
    st.tris = numTris;
    for(int t = 0; t < numTris; t++)
    {
        int idx[3];
        for(int k = 0; k < 3; k++)
        {
            idx[k] = za::clamp(readInt(f, trisAt + static_cast<size_t>(t) * 16u + 4u + static_cast<size_t>(k) * 4u), 0, numVerts - 1);
        }
        const glm::vec3 a = at(p, idx[0]), b = at(p, idx[1]), c = at(p, idx[2]);
        const float area = 0.5f * glm::length(glm::cross(b - a, c - a));
        const float sq = glm::dot(b - a, b - a) + glm::dot(c - b, c - b) + glm::dot(a - c, a - c);
        const float q = sq > 1e-12f ? 6.92820323f * area / sq : 0.f;
        const float longest = sqrtf(za::max(glm::dot(b - a, b - a), za::max(glm::dot(c - b, c - b), glm::dot(a - c, a - c))));
        // (flat: no higher than the model's step across its longest edge, drawn as a line: a bevel's lip the bytes flattened)
        const bool flat = 2.f * area < longest * za::max(scale.x, za::max(scale.y, scale.z));
        st.flat += flat ? 1 : 0;
        st.slivers += !flat && q < splitCheckLeast ? 1 : 0;
        if(!flat && q < st.worstQ)
        {
            st.worstQ = q;
            st.worstArea = area;
            st.worstLong = longest;
        }
        for(int k = 0; k < 3; k++)
        {
            const int e0 = at(weld, idx[k]), e1 = at(weld, idx[(k + 1) % 3]);
            if(e0 == e1)
            {
                continue;
            }
            bool fresh = false;
            int& n = uses.get(e0, e1, fresh);
            n = fresh ? 1 : n + 1;
            int& d = turn.get(e0, e1, fresh);
            d = (fresh ? 0 : d) + (e0 < e1 ? 1 : -1);
            const int r0 = splitRoot(parent, e0), r1 = splitRoot(parent, e1);
            at(parent, r1) = r0;
        }
    }
    for(za::SizeT i = 0; i < uses.keys.size(); i++)
    {
        if(uses.keys[i] == ~0ull)
        {
            continue;
        }
        st.open += (uses.vals[i] & 1) != 0 ? 1 : 0;
        bool fresh = false;
        const int e0 = static_cast<int>(uses.keys[i] & 0xffffffffull), e1 = static_cast<int>(uses.keys[i] >> 32u);
        st.flipped += uses.vals[i] == 2 && turn.get(e0, e1, fresh) != 0 ? 1 : 0;
    }
    // Its islands: the runs its triangles join, joined again where they come within splitCheckNear.
    for(int v = 0; v < numVerts; v++)
    {
        for(int u = 0; u < v; u++)
        {
            if(at(weld, u) == u && at(weld, v) == v && splitRoot(parent, u) != splitRoot(parent, v) &&
               glm::length(at(p, u) - at(p, v)) < splitCheckNear)
            {
                at(parent, splitRoot(parent, v)) = splitRoot(parent, u);
            }
        }
    }
    for(int v = 0; v < numVerts; v++)
    {
        if(at(weld, v) != v || splitRoot(parent, v) != v)
        {
            continue;
        }
        st.islands++;
        // (its spread: its vertices' covariance's two strongest ways)
        glm::vec3 c{0.f};
        int n = 0;
        for(int u = 0; u < numVerts; u++)
        {
            const bool on = at(weld, u) == u && splitRoot(parent, u) == v;
            c += on ? at(p, u) : glm::vec3{0.f};
            n += on ? 1 : 0;
        }
        c /= static_cast<float>(za::max(n, 1));
        glm::mat3 cov{0.f};
        for(int u = 0; u < numVerts; u++)
        {
            const glm::vec3 d = at(weld, u) == u && splitRoot(parent, u) == v ? at(p, u) - c : glm::vec3{0.f};
            cov += glm::outerProduct(d, d);
        }
        const glm::vec3 e1 = splitPrincipal(cov, glm::normalize(glm::vec3{0.3f, 0.5f, 1.f}));
        const float l1 = glm::dot(e1, cov * e1);
        const glm::mat3 rest2 = cov - l1 * glm::outerProduct(e1, e1);
        const glm::vec3 e2 = splitPrincipal(rest2, geom::safeNormalize(glm::cross(e1, glm::vec3{0.3f, 1.f, 0.2f}), glm::vec3{0.f, 1.f, 0.f}));
        const float l2 = glm::dot(e2, rest2 * e2);
        st.thinnest = za::min(st.thinnest, l1 > 1e-8f ? sqrtf(za::max(l2, 0.f) / l1) : 0.f);
    }
    return true;
}

[[nodiscard]] bool measureNamed(const char* name, SplitStats& st)
{
    unsigned char* f = derivedFile(name, nullptr);
    if(!f)
    {
        return false;
    }
    const bool ok = measureModel(f, static_cast<size_t>(com_filesize), st);
    VR_HeapFree(f);
    return ok;
}

constexpr const char* splitCheckModels[] = {"progs/vrgrunt.mdl", "progs/vrenforcer.mdl", "progs/vrknight.mdl", "progs/vrhknight.mdl", "progs/vrbody.mdl"};

// vr_agarmor_split_check [model | all] [verbose]: each piece's two halves, split either way (vr_agarmor_split_<kind>'s 0 and
// 1), as their models have them; ends on "agarmor split check: N passed, M failed".
void splitCheck_f()
{
    const bool all = Cmd_Argc() < 2 || !q_strcasecmp(Cmd_Argv(1), "all");
    const bool verbose = Cmd_Argc() > 2 && Q_atoi(Cmd_Argv(2)) != 0;
    int passed = 0, failed = 0;
    for(const char* m : splitCheckModels)
    {
        if(!all && q_strcasecmp(m, Cmd_Argv(1)))
        {
            continue;
        }
        const Shell* s = serverShell(m);
        if(!s)
        {
            Con_Printf("agarmor split check: FAIL %s: no shell\n", m);
            failed++;
            continue;
        }
        for(int piece = 0; piece < NumPieces; piece++)
        {
            char name[MAX_QPATH];
            q_snprintf(name, sizeof(name), "%s#plate%d", m, piece);
            SplitStats whole;
            if(piece == Helmet || !plateFrameOf(*s, piece).ok || !measureNamed(name, whole))
            {
                continue;
            }
            for(int axis = 0; axis < 2; axis++)
            {
                SplitStats h[2];
                bool made = true;
                for(int half = 0; half < 2; half++)
                {
                    q_snprintf(name, sizeof(name), "%s#plate%dh%ds%d", m, piece, half, axis);
                    made = measureNamed(name, h[half]) && made;
                }
                SplitInfo info;
                za::Vector<int> tris;
                glm::vec3 mid;
                (void)splitHalf(*s, piece, axis, 0, tris, mid, nullptr, &info);
                bool ok = made && h[0].tris + h[1].tris == whole.tris && info.big == 0;
                for(const SplitStats& x : h)
                {
                    ok = ok && x.open == 0 && x.flipped == 0 && x.islands == 1 && x.thinnest >= splitCheckStrip &&
                         (x.worstQ >= splitCheckLeast || x.worstQ >= splitCheckSlack * whole.worstQ);
                }
                (ok ? passed : failed)++;
                if(!ok || verbose)
                {
                    Con_Printf("agarmor split check: %s %s piece %d axis %d: whole %d tris (open %d, slivers %d, flat %d, islands %d, "
                               "worst %.3f); plates %d, torn %d, most vertices %d\n",
                        ok ? "PASS" : "FAIL", m, piece, axis, whole.tris, whole.open, whole.slivers, whole.flat, whole.islands, whole.worstQ,
                        info.runs, info.big, info.biggest);
                    for(int half = 0; half < 2; half++)
                    {
                        Con_Printf("agarmor split check:   half %d: %d tris, open %d, flipped %d, islands %d, slivers %d, flat %d, worst %.3f, "
                                   "(%.2f long, area %.4f), thinnest %.2f\n",
                            half, h[half].tris, h[half].open, h[half].flipped, h[half].islands, h[half].slivers, h[half].flat, h[half].worstQ,
                            h[half].worstLong, h[half].worstArea, h[half].thinnest);
                    }
                }
            }
        }
    }
    qvr::testkit::verdict("agarmor split check", passed, failed);
}
} // namespace

void init()
{
    Cmd_AddCommand("vr_agarmor_split_check", splitCheck_f);
    Cmd_AddCommand("vr_agarmor_info", info_f);
    Cmd_AddCommand("vr_player_plates_info", view::platesInfo_f);
    Cmd_AddCommand("vr_player_plates_model_check", view::platesModelCheck_f);
    Cmd_AddCommand("vr_agarmor_rebuild", rebuild_f);
    Cmd_AddCommand("vr_agarmor_set", set_f);
    Cmd_AddCommand("vr_agarmor_check", check_f);
    Cmd_AddCommand("vr_agarmor_bridge_check", bridgeCheck_f);
    Cmd_AddCommand("vr_agarmor_bridge_stats", bridgeStats_f);
    Cmd_AddCommand("vr_agarmor_look_check", lookCheck_f);
    Cmd_AddCommand("vr_agarmor_pop_check", popCheck_f);
    Cmd_AddCommand("vr_agarmor_pop_sample", popSample_f);
    Cmd_AddCommand("vr_agarmor_pop_report", popReport_f);
    Cmd_AddCommand("vr_agarmor_headcam", headcam_f);
}

} // namespace qvr::agarmor
