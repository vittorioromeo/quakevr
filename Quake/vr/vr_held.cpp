// vr_held.cpp -- see vr_held.hpp.

#include "vr_held.hpp"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_protocol.hpp"
#include "vr_weapons.hpp"

using namespace qvr;

namespace qvr::held
{

glm::mat3 axesFromAngles(const float* angles, bool brush)
{
    vec3_t a{brush ? angles[0] : -angles[0], angles[1], angles[2]}, f, r, u;
    AngleVectors(a, f, r, u);
    return glm::mat3{glm::vec3{f[0], f[1], f[2]}, -glm::vec3{r[0], r[1], r[2]}, glm::vec3{u[0], u[1], u[2]}};
}

void anglesFromAxes(const glm::mat3& m, float* out, bool brush)
{
    const glm::vec3 a = hands::anglesFromVectors(glm::normalize(m[0]), glm::normalize(m[2]));
    out[0] = brush ? a.x : -a.x;
    out[1] = a.y;
    out[2] = a.z;
}

void modelBox(const qmodel_t* model, const glm::vec3& scale, const glm::vec3& scaleOrigin, const glm::vec3& offset,
    glm::vec3& lo, glm::vec3& hi)
{
    lo = glm::vec3{0.f};
    hi = glm::vec3{0.f};
    if(!model || (model->type != mod_alias && model->type != mod_brush))
    {
        return;
    }

    const glm::vec3 netScale = glm::vec3{1.f} + scale;
    const bool alias = model->type == mod_alias;
    const auto* hdr = alias ? static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model))) : nullptr;
    const weapons::ModelTransform t = alias ? weapons::modelTransform(model) : weapons::ModelTransform{};
    const glm::vec3 so = hdr ? glm::vec3{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]} : glm::vec3{0.f};
    const glm::vec3 hs = hdr ? glm::vec3{hdr->scale[0], hdr->scale[1], hdr->scale[2]} : glm::vec3{1.f};

    lo = glm::vec3{1e9f};
    hi = glm::vec3{-1e9f};
    for(int i = 0; i < 8; i++)
    {
        const glm::vec3 v{(i & 1) ? model->maxs[0] : model->mins[0], (i & 2) ? model->maxs[1] : model->mins[1],
            (i & 4) ? model->maxs[2] : model->mins[2]};
        glm::vec3 p;
        if(!alias)
        {
            p = v + offset;
        }
        else
        {
            const glm::vec3 raw = (v - so) / hs;
            if(t.active)
            {
                p = (raw + offset) * t.scale;
                p = so + hs * p;
                p = (t.offset + p) * t.k;
            }
            else
            {
                p = so + hs * (raw + offset);
            }
        }
        p = scaleOrigin + (p - scaleOrigin) * netScale;
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
}

glm::vec3 drawnCentre(int num)
{
    const entity_t& e = cl_entities[num];
    const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
    if(!e.model || (e.model->type != mod_alias && e.model->type != mod_brush))
    {
        return origin;
    }

    const client::EntityVr* net = client::entityVr(num);
    const glm::vec3 zero{0.f};
    glm::vec3 lo, hi;
    modelBox(e.model, net ? net->scale : zero, net ? net->scaleOrigin : zero, net ? net->offset : zero, lo, hi);
    const bool brush = e.model->type == mod_brush;
    return origin + axesFromAngles(e.angles, brush) * ((lo + hi) * 0.5f * ENTSCALE_DECODE(e.scale)); // and Ironwail's scale
}

} // namespace qvr::held

namespace
{

// For this long after the server first says a hand holds an object, its place in the hand is
// still taken from where the server has it (a caught object's jump to the hand may come a packet
// late); then it is kept.
constexpr double placeTime = 0.1;

// Let go, it eases back to where the server has it over this long.
constexpr double easeTime = 0.2;

struct Held
{
    int ent{0};
    const qmodel_t* model{nullptr};
    double since{0.0};
    bool placed{false};
    glm::vec3 pos{0.f};   // in the hand's frame
    glm::mat3 rot{1.f};
    bool drawn{false};    // drawn in the hand last frame, at:
    glm::vec3 lastPos{0.f};
    glm::mat3 lastRot{1.f};
};

struct Easing
{
    int ent{0};
    const qmodel_t* model{nullptr};
    double since{0.0};
    bool started{false};
    glm::vec3 lastPos{0.f};
    glm::mat3 lastRot{1.f};
    glm::vec3 offset{0.f};
    glm::quat turn{1.f, 0.f, 0.f, 0.f};
};

Held holding[2];  // [0] off hand, [1] main hand (as hands::State)
Easing easing[2];

[[nodiscard]] bool valid(int ent, const qmodel_t* model)
{
    return ent > 0 && ent < cl.num_entities && cl_entities[ent].model &&
           (!model || cl_entities[ent].model == model) && cl_entities[ent].model->type != mod_sprite;
}

void reset()
{
    for(int h = 0; h < 2; h++)
    {
        holding[h] = Held{};
        easing[h] = Easing{};
    }
}

void place(entity_t& e, const glm::vec3& pos, const glm::mat3& rot)
{
    e.origin[0] = pos.x;
    e.origin[1] = pos.y;
    e.origin[2] = pos.z;
    held::anglesFromAxes(rot, e.angles, e.model->type == mod_brush);
}

void holdFrame(int h, const hands::State& s)
{
    Held& hd = holding[h];
    const int want = cl.stats[h == 1 ? protocol::STAT_QVR_CARRYMAIN : protocol::STAT_QVR_CARRYOFF];
    if(want != hd.ent)
    {
        if(hd.ent && hd.drawn)
        {
            easing[h] = Easing{hd.ent, hd.model, cl.time, false, hd.lastPos, hd.lastRot};
        }
        hd = Held{};
        hd.ent = want;
        hd.since = cl.time;
    }

    hd.drawn = false;
    if(!hd.ent || !s.valid || !valid(hd.ent, hd.placed ? hd.model : nullptr))
    {
        return;
    }

    entity_t& e = cl_entities[hd.ent];
    const bool brush = e.model->type == mod_brush;
    const glm::mat3 hand = held::axesFromAngles(&s.rot[h][0], true);
    if(!hd.placed || cl.time - hd.since < placeTime)
    {
        const glm::vec3 o{e.msg_origins[0][0], e.msg_origins[0][1], e.msg_origins[0][2]};
        hd.pos = glm::transpose(hand) * (o - s.pos[h]);
        hd.rot = glm::transpose(hand) * held::axesFromAngles(e.msg_angles[0], brush);
        hd.model = e.model;
        hd.placed = true;
    }

    hd.lastPos = s.pos[h] + hand * hd.pos;
    hd.lastRot = hand * hd.rot;
    place(e, hd.lastPos, hd.lastRot);
    hd.drawn = true;
}

void easeFrame(Easing& ea)
{
    const double t = cl.time - ea.since;
    if(!ea.ent || holding[0].ent == ea.ent || holding[1].ent == ea.ent || t >= easeTime || t < 0.0 || !valid(ea.ent, ea.model))
    {
        ea = Easing{};
        return;
    }

    entity_t& e = cl_entities[ea.ent];
    const bool brush = e.model->type == mod_brush;
    const glm::vec3 serverPos{e.origin[0], e.origin[1], e.origin[2]};
    const glm::mat3 serverRot = held::axesFromAngles(e.angles, brush);
    if(!ea.started)
    {
        ea.started = true;
        ea.offset = ea.lastPos - serverPos;
        ea.turn = glm::quat_cast(ea.lastRot * glm::transpose(serverRot));
        if(glm::length(ea.offset) > 64.f) // gone somewhere else (a teleport): no easing
        {
            ea = Easing{};
            return;
        }
    }

    float w = 1.f - static_cast<float>(t / easeTime);
    w *= w;
    place(e, serverPos + ea.offset * w,
        glm::mat3_cast(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, ea.turn, w)) * serverRot);
}

} // namespace

// End of CL_RelinkEntities: the local player's held objects are drawn in the hands drawn this
// frame (and ease back to the server's position when let go).
extern "C" void VR_RelinkHeld(void)
{
    if(!(cl.protocolflags & PRFL_QUAKEVR) || cls.state != ca_connected || cls.demoplayback || !vr_carry_local.value)
    {
        reset();
        return;
    }

    const hands::State& s = hands::current();
    for(int h = 0; h < 2; h++)
    {
        holdFrame(h, s);
    }
    for(Easing& ea : easing)
    {
        easeFrame(ea);
    }
}
