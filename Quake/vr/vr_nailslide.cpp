// vr_nailslide.cpp -- the nail pistol's slide as drawn: see vr_nailslide.hpp.

#include "vr_nailslide.hpp"

#include "vr_view_internal.hpp"
#include "vr_cvars.hpp"
#include "vr_mdldequant.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::nailslide
{
namespace
{

constexpr float backShare = 1.f / 3.f; // the cycle's share going back (QC VR_NailPistol_SlideBack)
constexpr float maxTravel = 3.f;       // vr_nailpistol_slide_travel's most (the model's frame 1 is this far back)

struct Hand
{
    float pos{0.f};               // 0 home .. 1 back by the travel
    bool kick{false};             // sent back (an edge of the server's frame to 1): until it is all the way back
    bool back{false};             // the server's frame 1 last frame
    double time{-1.0};            // vr_gametime of its last step
    const qmodel_s* gun{nullptr}; // the model it was for (another: no animation)
};

struct ModelTravel
{
    const void* hdr{nullptr}; // the v_nailpistol.mdl header measured
    float travel[2]{};        // per look, how far its back frame moves the slide from its rest frame (model units)
};

Hand hands[2];
ModelTravel measured;

// The look drawn (vr_nailpistol_model): 0 classic (frames 0 rest, 1 back), 1 full slide (frames 2 rest, 3 back), when
// the model has it (build_nailpistol_slide.py).
[[nodiscard]] int look(const aliashdr_t* hdr)
{
    return vr_nailpistol_model.value >= 1.f && hdr->numframes >= 4 ? 1 : 0;
}

// How far progs/v_nailpistol.mdl's back frame of a look has its slide back from its rest frame (model units).
[[nodiscard]] float modelTravel(const aliashdr_t* hdr, int lk)
{
    if(hdr != measured.hdr)
    {
        measured.hdr = hdr;
        for(int k = 0; k < 2; k++)
        {
            measured.travel[k] = 0.f;
            if(hdr->numframes < 2 * k + 2)
            {
                continue;
            }
            const int p0 = hdr->frames[2 * k].firstpose, p1 = hdr->frames[2 * k + 1].firstpose;
            for(int v = 0; v < hdr->numverts; v++)
            {
                const glm::vec3 d =
                    mdldequant::modelVertex(hdr, p0, p1, 1.f, v) - mdldequant::modelVertex(hdr, p0, p1, 0.f, v);
                measured.travel[k] = za::max(measured.travel[k], glm::length(d));
            }
        }
    }
    return measured.travel[lk];
}

[[nodiscard]] bool isNailPistol(const qmodel_s* m)
{
    return m && m->type == mod_alias && modelmeta::is(m, modelmeta::Id::VNailPistol);
}

} // namespace

void step(int hand, const qmodel_s* gun, int frame)
{
    Hand& h = hands[hand];
    const bool np = isNailPistol(gun);
    const bool back = np && frame == 1;
    const float dt = h.time >= 0.0 ? static_cast<float>(za::clamp(vr_gametime - h.time, 0.0, 0.1)) : 0.f;
    h.time = vr_gametime;
    if(gun != h.gun)
    {
        h.gun = gun;
        h.pos = back ? 1.f : 0.f; // (a gun come to the hand: as it is)
        h.kick = false;
        h.back = back;
        return;
    }
    if(!np)
    {
        return;
    }
    const char* edge = back && !h.back ? "back" : !back && h.back ? "home" : nullptr;
    h.kick = h.kick || (back && !h.back);
    h.back = back;
    const float cycle = za::max(vr_nailpistol_slide_cycle.value, 0.01f);
    const float was = h.pos;
    if(h.kick || back)
    {
        h.pos = za::min(1.f, h.pos + dt / (cycle * backShare));
    }
    else
    {
        h.pos = za::max(0.f, h.pos - dt / (cycle * (1.f - backShare)));
    }
    if(h.pos >= 1.f)
    {
        h.kick = false;
    }
    if(vr_nailslide_debug.value && (h.pos != was || edge))
    {
        Con_Printf("nailslide: hand %d pos %.3f (travel %.2f) back %d%s%s\n", hand, h.pos,
            h.pos * za::clamp(vr_nailpistol_slide_travel.value, 0.f, maxTravel), back ? 1 : 0, edge ? " edge " : "",
            edge ? edge : "");
    }
}

float position(int hand)
{
    return hands[hand].pos;
}

} // namespace qvr::nailslide

// R_SetupAliasFrame's last word on a nail pistol (after VR_AliasPoseOverride): its look's rest and back frames (0 and
// 1 classic, 2 and 3 full slide: vr_nailpistol_model) blended by how far its slide is drawn back, the back frame being
// the model's full travel. The guns in the hands: their slide's place
// (nailslide::step); any other (an armed grunt's, a holstered one): its own frame 1's share.
extern "C" void VR_NailSlidePose(const entity_t* e, const void* aliashdr, short* pose1, short* pose2, float* blend)
{
    using namespace qvr;
    if(!e || !nailslide::isNailPistol(e->model))
    {
        return;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(hdr->numframes < 2)
    {
        return;
    }
    const int lk = nailslide::look(hdr);
    const float full = nailslide::modelTravel(hdr, lk);
    // The full slide's back frame is only ~0.65 units back (the model's box ends there: its scale origin, the corner the
    // gun is scaled about, is the classic one's): drawn further by blending past it (a pure translation, in whole
    // byte steps: every vertex alike). The classic back frame is the most travel.
    const float most = full > 1e-3f ? (lk ? nailslide::maxTravel / full : 1.f) : 0.f;
    const float k =
        full > 1e-3f ? za::clamp(za::clamp(vr_nailpistol_slide_travel.value, 0.f, nailslide::maxTravel) / full, 0.f, most)
                     : 0.f;
    // (the server's frame 1, back: its share whichever look is drawn)
    const short back = static_cast<short>(hdr->frames[1].firstpose);
    const short f0 = static_cast<short>(hdr->frames[2 * lk].firstpose);
    const short f1 = static_cast<short>(hdr->frames[2 * lk + 1].firstpose);
    float w = (*pose1 == back ? 1.f - *blend : 0.f) + (*pose2 == back ? *blend : 0.f);
    for(int hand = 0; hand < 2; hand++)
    {
        if(e == &view_detail::entities.weapon[hand].ent && nailslide::hands[hand].gun == e->model)
        {
            w = nailslide::hands[hand].pos;
        }
    }
    *pose1 = f0;
    *pose2 = f1;
    *blend = w * k;
}
