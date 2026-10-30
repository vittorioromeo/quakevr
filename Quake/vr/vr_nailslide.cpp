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
    float travel{0.f};        // how far its frame 1 moves the slide (model units)
};

Hand hands[2];
ModelTravel measured;

// How far progs/v_nailpistol.mdl's frame 1 has its slide back from frame 0 (model units; 0: no such frame).
[[nodiscard]] float modelTravel(const aliashdr_t* hdr)
{
    if(hdr == measured.hdr)
    {
        return measured.travel;
    }
    measured.hdr = hdr;
    measured.travel = 0.f;
    if(hdr->numframes < 2)
    {
        return 0.f;
    }
    const int p0 = hdr->frames[0].firstpose, p1 = hdr->frames[1].firstpose;
    for(int v = 0; v < hdr->numverts; v++)
    {
        const glm::vec3 d = mdldequant::modelVertex(hdr, p0, p1, 1.f, v) - mdldequant::modelVertex(hdr, p0, p1, 0.f, v);
        measured.travel = za::max(measured.travel, glm::length(d));
    }
    return measured.travel;
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

// R_SetupAliasFrame's last word on a nail pistol (after VR_AliasPoseOverride): its frames 0 and 1 blended by how far
// its slide is drawn back, frame 1 being the model's full travel. The guns in the hands: their slide's place
// (nailslide::step); any other (an armed grunt's, a holstered one): its own frame 1's share.
extern "C" void VR_NailSlidePose(const entity_t* e, const void* aliashdr, short* pose1, short* pose2, float* blend)
{
    using namespace qvr;
    if(!e || !nailslide::isNailPistol(e->model))
    {
        return;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    const float full = nailslide::modelTravel(hdr);
    if(full <= 1e-3f)
    {
        return;
    }
    const float k = za::clamp(za::clamp(vr_nailpistol_slide_travel.value, 0.f, nailslide::maxTravel) / full, 0.f, 1.f);
    const short f0 = static_cast<short>(hdr->frames[0].firstpose), f1 = static_cast<short>(hdr->frames[1].firstpose);
    float w = (*pose1 == f1 ? 1.f - *blend : 0.f) + (*pose2 == f1 ? *blend : 0.f);
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
