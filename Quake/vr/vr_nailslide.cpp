// vr_nailslide.cpp -- the nail pistol's slide as drawn: see vr_nailslide.hpp.

#include "vr_nailslide.hpp"

#include "vr_view_internal.hpp"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_protocol.hpp"
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
    float travel{0.f};        // how far its back frame (1) moves the slide from its rest frame (0) (model units)
    glm::vec3 bytes{0.f};     // and that move in the model's byte space (anchor::posedAnchor's: the screen's shift)
};

Hand hands[2];
ModelTravel measured;

// How far progs/v_nailpistol.mdl's back frame has its slide back from its rest frame (model units; build_nailpistol_slide.py).
[[nodiscard]] float modelTravel(const aliashdr_t* hdr)
{
    if(hdr != measured.hdr)
    {
        measured = ModelTravel{hdr};
        const int p0 = hdr->frames[0].firstpose, p1 = hdr->frames[1].firstpose;
        for(int v = 0; v < hdr->numverts; v++)
        {
            const glm::vec3 d = mdldequant::modelVertex(hdr, p0, p1, 1.f, v) - mdldequant::modelVertex(hdr, p0, p1, 0.f, v);
            if(glm::length(d) > measured.travel)
            {
                measured.travel = glm::length(d);
                measured.bytes = d / glm::vec3{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
            }
        }
    }
    return measured.travel;
}

[[nodiscard]] bool isNailPistol(const qmodel_s* m)
{
    return m && m->type == mod_alias && modelmeta::is(m, modelmeta::Id::VNailPistol);
}

// How far the back frame is blended for the slide back by vr_nailpistol_slide_travel. The back frame is only ~0.65 units
// back (the model's box ends there: its scale origin is the corner the gun is scaled about): drawn further by blending
// past it (a pure translation, in whole byte steps: every vertex alike).
[[nodiscard]] float travelBlend(const aliashdr_t* hdr)
{
    const float full = modelTravel(hdr);
    if(full <= 1e-3f)
    {
        return 0.f;
    }
    return za::clamp(vr_nailpistol_slide_travel.value, 0.f, maxTravel) / full;
}

// One of the guns in the hands that the slide's animation draws (step): its hand, else -1.
[[nodiscard]] int handOf(const entity_t* e)
{
    for(int hand = 0; hand < 2; hand++)
    {
        if(e == &view_detail::entities.weapon[hand].ent && hands[hand].gun == e->model)
        {
            return hand;
        }
    }
    return -1;
}

} // namespace

bool holsterLocked(const qmodel_s* model, int clip, int flags)
{
    if(!isNailPistol(model) || clip < 0)
    {
        return false;
    }
    if(cl.stats[protocol::STAT_QVR_RELOADMODE] == 0)
    {
        return cl.stats[STAT_NAILS] < 1;
    }
    return clip == 0 || (flags & view_detail::weaponFlagNoMag) != 0;
}

bool propLocked(const entity_t& e)
{
    if(!isNailPistol(e.model) || &e < cl_entities || &e >= cl_entities + cl_max_edicts)
    {
        return false;
    }
    const client::EntityVr* net = client::entityVr(static_cast<int>(&e - cl_entities));
    return net && net->weaponUid != 0 && net->clip >= 0 && (net->clip == 0 || net->noMag);
}

glm::vec3 screenShift(const entity_t& e)
{
    if(vr_nailpistol_screen_ride.value == 0.f || !isNailPistol(e.model))
    {
        return glm::vec3{0.f};
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e.model));
    if(hdr->numframes < 2)
    {
        return glm::vec3{0.f};
    }
    const int hand = handOf(&e);
    const float w = hand >= 0 ? hands[hand].pos : propLocked(e) || e.frame == 1 ? 1.f : 0.f;
    const float k = travelBlend(hdr); // (measures the model first)
    return measured.bytes * (w * k);
}

void probe()
{
    const entity_t& gun = view_detail::entities.weapon[HAND_MAIN].ent;
    float screen = 0.f;
    if(isNailPistol(gun.model))
    {
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(gun.model));
        screen = glm::length(screenShift(gun) * glm::vec3{hdr->scale[0], hdr->scale[1], hdr->scale[2]});
    }
    int holstered = 0;
    for(const view::ViewEntity& ve : view_detail::entities.holster)
    {
        if(ve.visible && isNailPistol(ve.ent.model))
        {
            holstered += ve.ent.frame == 1 ? 1 : 100;
        }
    }
    int props = 0;
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const entity_t* e = cl_visedicts[i];
        if(e && isNailPistol(e->model) && e >= cl_entities && e < cl_entities + cl_max_edicts && e != &cl_entities[cl.viewentity])
        {
            props += propLocked(*e) || e->frame == 1 ? 1 : 100;
        }
    }
    Cvar_SetValueQuick(&vr_np_seen_screen, screen);
    Cvar_SetValueQuick(&vr_np_seen_holster, static_cast<float>(holstered));
    Cvar_SetValueQuick(&vr_np_seen_prop, static_cast<float>(props));
}

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

// R_SetupAliasFrame's last word on a nail pistol (after VR_AliasPoseOverride): its rest and back frames (0 and 1) blended
// by how far its slide is drawn back (past the back frame for more: travelBlend). The guns in the hands: their slide's place
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
    const float k = nailslide::travelBlend(hdr);
    // (the server's frame 1, back: its share; a gun lying about empty: locked back)
    const short f0 = static_cast<short>(hdr->frames[0].firstpose);
    const short f1 = static_cast<short>(hdr->frames[1].firstpose);
    float w = (*pose1 == f1 ? 1.f - *blend : 0.f) + (*pose2 == f1 ? *blend : 0.f);
    if(const int hand = nailslide::handOf(e); hand >= 0)
    {
        w = nailslide::hands[hand].pos;
    }
    else if(nailslide::propLocked(*e))
    {
        w = 1.f;
    }
    *pose1 = f0;
    *pose2 = f1;
    *blend = w * k;
}
