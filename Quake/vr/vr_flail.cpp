// vr_flail.cpp -- see vr_flail.hpp.

#include "vr_flail.hpp"

#include "vr_admelee.hpp"
#include "vr_box3d.hpp"
#include "vr_coil.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_held.hpp"
#include "vr_profile.hpp"
#include "vr_view.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"

namespace qvr::flail
{
namespace
{
const glm::vec3 keyLight = glm::normalize(glm::vec3{0.3f, 0.2f, 1.f});
constexpr float ringReach = 3.5f; // units: the head's middle to its ring (vr_box3d_flail.cpp's)

struct Hand
{
    entity_t ent{};
    coil::Cord chain;
    gfx::TubeBatch batch;
    int sides{0};
    bool shown{false};
};

struct State
{
    Hand hands[2];
    za::Vector<gfx::TubeRing> rings;
    int builtFrame{-1};
};
State state;

[[nodiscard]] coil::Style chainStyle()
{
    coil::Style style;
    style.chain = true;
    style.rigid = true;
    style.length = za::clamp(vr_flail_chain.value, 0.1f, 2.f);
    style.wireRadius = 0.0028f;
    style.linkLength = 0.022f;
    style.linkWidth = 0.011f;
    style.albedo = glm::vec3{0.22f, 0.25f, 0.32f}; // (AD's blue steel)
    style.rust = glm::vec3{0.2f, 0.16f, 0.12f};
    return style;
}
} // namespace

void frame(const hands::State& s)
{
    QVR_PROFILE("flail");
    for(int h = 0; h < 2; h++)
    {
        Hand& hand = state.hands[h];
        glm::vec3 pos, anchor;
        glm::quat rot;
        const bool held = sv.active && box3d::flailHead(cl.viewentity, h, pos, rot, anchor);
        // (Its model looked up only with a head: none asked for without AD's data.)
        qmodel_t* model = held ? view::viewModel(admelee::flailHeadModel) : nullptr;
        if(!model)
        {
            hand.shown = false;
            hand.chain.hide();
            continue;
        }
        // The chain from the handle's end as drawn now (the server's anchor is the frame before's).
        const glm::vec3 at = s.muzzleValid[h] ? s.muzzle[h] : anchor;
        const glm::vec3 handle = at - s.pos[h];
        const glm::vec3 out = glm::length(handle) > 0.5f ? glm::normalize(handle) : glm::vec3{0.f, 0.f, -1.f};
        glm::vec3 toChain = at - pos;
        // (Turned towards its chain: the chain's last link before it, as stepped last frame.)
        const za::Vector<glm::vec3>& nodes = hand.chain.nodes();
        if(hand.chain.visible() && nodes.size() >= 2 && glm::distance(nodes[nodes.size() - 2], pos) > 0.5f)
        {
            toChain = nodes[nodes.size() - 2] - pos;
        }
        const glm::vec3 fwd = glm::length(toChain) > 0.01f ? glm::normalize(toChain) : glm::vec3{0.f, 0.f, 1.f};
        glm::vec3 up = rot * glm::vec3{0.f, 0.f, 1.f};
        glm::vec3 left = glm::cross(up, fwd);
        if(glm::length(left) < 0.1f)
        {
            left = glm::cross(rot * glm::vec3{0.f, 1.f, 0.f}, fwd);
        }
        left = glm::normalize(left);
        up = glm::cross(fwd, left);
        const glm::vec3 ring = pos + fwd * ringReach;
        hand.chain.update(at, out, ring, -fwd, chainStyle());

        entity_t& e = hand.ent;
        if(e.model != model)
        {
            e = entity_t{};
            e.model = model;
            e.scale = ENTSCALE_DEFAULT;
            e.alpha = ENTALPHA_DEFAULT;
        }
        e.colormap = vid.colormap;
        e.lerpflags = LERP_RESETANIM | LERP_RESETMOVE;
        e.frame = 0;
        e.skinnum = 0;
        e.effects = 0;
        for(int i = 0; i < 3; i++)
        {
            e.origin[i] = pos[i];
        }
        held::anglesFromAxes(glm::mat3{fwd, left, up}, e.angles, false);
        hand.shown = true;
        if(cl_numvisedicts < MAX_VISEDICTS)
        {
            cl_visedicts[cl_numvisedicts++] = &e;
        }
    }
}

void drawOpaque()
{
    const bool build = state.builtFrame != host_framecount;
    state.builtFrame = host_framecount;
    for(Hand& hand : state.hands)
    {
        if(!hand.shown || !hand.chain.visible())
        {
            continue;
        }
        if(build)
        {
            QVR_PROFILE("flail chain build");
            const hands::State& s = hands::current();
            const bool built = hand.chain.build(0.5f * (s.eyeOrigin[0] + s.eyeOrigin[1]), state.rings, hand.sides);
            hand.batch = built ? gfx::uploadTube(state.rings) : gfx::TubeBatch{};
        }
        if(hand.batch.count)
        {
            QVR_GPU_PROFILE("flail chain draw");
            gfx::drawTube(hand.batch, hand.sides, hand.chain.albedo(), keyLight, hand.chain.rust(), hand.chain.flat());
        }
    }
}

} // namespace qvr::flail
