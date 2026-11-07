// vr_arenaxray.cpp -- see vr_arenaxray.hpp.

#include "vr_arenaxray.hpp"

#include "vr_api.h"
#include "vr_api_render.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr/vr_ea.h"

namespace qvr::arenaxray
{
namespace
{
// The outline's colour (the alias shader's coded glow: 16 levels a channel, r + 16g + 256b) -- a hot red-orange, the
// colour of a threat, apart from the loot beams' hues and the player's own -- and its strength (0..2): a pulse round
// `strength`, faded in over `fadeIn` units past the distance so a foe crossing it does not pop.
constexpr float colourCode = 15.f + 16.f * 4.f + 256.f * 2.f;
constexpr float strength = 1.55f;
constexpr float pulse = 0.25f;
constexpr float pulseHz = 0.8f;
constexpr float fadeIn = 96.f;
constexpr int maxShown = 64; // the foes outlined in one view at most (a wave's last few: a handful)

za::Vector<unsigned char> sent;   // the server's: per entity, what the clients were last told
za::Vector<unsigned char> marked; // the client's: per entity, marked by the server
int numMarked = 0;
entity_t* shown[maxShown];        // this view's outlined foes (VR_DrawArenaXray)
int lastLog[3] = {-1, -1, -1};    // the developer line's last numbers: marked, in the view, outlined

[[nodiscard]] unsigned char stateOf(edict_t* e)
{
    const int ofs = progs::fields().vr_xray;
    return !e->free && ofs >= 0 && progs::fieldFloat(e, ofs) > 0.f && e->v.health > 0.f ? 1 : 0;
}

void writeState(sizebuf_t* msg, int num, unsigned char on)
{
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_XRAY);
    MSG_WriteShort(msg, num);
    MSG_WriteByte(msg, on);
}

[[nodiscard]] bool markedOn(int num)
{
    return num >= 0 && num < static_cast<int>(marked.size()) && marked[static_cast<za::SizeT>(num)] != 0;
}
} // namespace

void serverFrame()
{
    if(!sv.active || progs::fields().vr_xray < 0)
    {
        return;
    }
    const int n = sv.qcvm.num_edicts;
    if(static_cast<int>(sent.size()) < n)
    {
        sent.resize(static_cast<za::SizeT>(n), 0);
    }
    for(int i = svs.maxclients + 1; i < static_cast<int>(sent.size()); i++)
    {
        const unsigned char now = i < n ? stateOf(EDICT_NUM(i)) : 0;
        unsigned char& was = sent[static_cast<za::SizeT>(i)];
        if(now != was && sv.reliable_datagram.cursize + 8 < sv.reliable_datagram.maxsize)
        {
            writeState(&sv.reliable_datagram, i, now);
            was = now;
        }
    }
}

void serverWriteAll(sizebuf_t* msg)
{
    sent.clear();
    if(!sv.active || progs::fields().vr_xray < 0)
    {
        return;
    }
    writeState(msg, -1, 0);
    sent.resize(static_cast<za::SizeT>(sv.qcvm.num_edicts), 0);
    EA_OPEN(4) for(int i = svs.maxclients + 1; i < sv.qcvm.num_edicts; i++)
    {
        const unsigned char now = stateOf(EDICT_NUM(i));
        sent[static_cast<za::SizeT>(i)] = now;
        if(now && msg->cursize + 8 < msg->maxsize)
        {
            writeState(msg, i, now);
        }
    } EA_CLOSE(4)
}

void parseEvent()
{
    const int num = MSG_ReadShort();
    const unsigned char on = MSG_ReadByte() != 0 ? 1 : 0;
    if(num < 0)
    {
        marked.clear();
        numMarked = 0;
        return;
    }
    if(num >= static_cast<int>(marked.size()))
    {
        marked.resize(static_cast<za::SizeT>(num + 1), 0);
    }
    unsigned char& m = marked[static_cast<za::SizeT>(num)];
    numMarked += on - m;
    m = on;
}

} // namespace qvr::arenaxray

// sv_main.c SV_WriteEntitiesToClient: a marked foe goes to the clients wherever it stands (its outline is the point).
extern "C" int VR_SV_SendPastPVS(edict_t* ent)
{
    return ent && qvr::arenaxray::stateOf(ent) != 0 ? 1 : 0;
}

// R_DrawAliasModel_Real's outline pass: the glow a marked foe is drawn with (the alias shader's coded rim, a power-up
// glow's code: -2 - 2 x colour - strength), pulsing, faded in past the distance.
extern "C" float VR_ArenaXrayGlow(const entity_t* e)
{
    using namespace qvr;
    using namespace qvr::arenaxray;
    vec3_t delta;
    VectorSubtract(e->origin, r_refdef.vieworg, delta);
    const float past = VectorLength(delta) - CLAMP(0.f, vr_arena_xray_distance.value, 100000.f);
    const float in = vr_arena_xray_distance.value > 0.f ? za::clamp(past / fadeIn, 0.f, 1.f) : 1.f;
    const float s = in * (strength + pulse * za::sin(static_cast<float>(cl.time) * pulseHz * 6.2831853f));
    return -2.f - 2.f * colourCode - za::clamp(s, 0.f, 1.95f);
}

// R_RenderScene, after the scene's translucent draws: each marked foe this view has, farther from its eyes than the
// distance, drawn again as its outline through whatever stands in front of it.
extern "C" void VR_DrawArenaXray(void)
{
    using namespace qvr;
    using namespace qvr::arenaxray;
    int n = 0;
    int inView = 0;
    if(vr_arena_xray.value != 0.f && numMarked > 0 && cls.state == ca_connected && cl_entities)
    {
        const float dist = CLAMP(0.f, vr_arena_xray_distance.value, 100000.f);
        for(int i = 0; i < cl_numvisedicts && n < maxShown; i++)
        {
            entity_t* e = cl_visedicts[i];
            if(!e || !e->model || e->model->type != mod_alias || e < cl_entities || e >= cl_entities + cl_max_edicts ||
                !markedOn(static_cast<int>(e - cl_entities)))
            {
                continue;
            }
            inView++;
            vec3_t delta;
            VectorSubtract(e->origin, r_refdef.vieworg, delta);
            if(VectorLength(delta) > dist)
            {
                shown[n++] = e;
            }
        }
        if(n > 0)
        {
            GL_BeginGroup("arena xray");
            R_DrawAliasModels_Xray(shown, n);
            GL_EndGroup();
        }
    }
    if(vr_arena_xray_shown.value != static_cast<float>(n))
    {
        Cvar_SetROM(vr_arena_xray_shown.name, va("%d", n)); // (read-only to the console)
    }
    if(numMarked != lastLog[0] || inView != lastLog[1] || n != lastLog[2]) // (developer: what changed, once)
    {
        lastLog[0] = numMarked;
        lastLog[1] = inView;
        lastLog[2] = n;
        Con_DPrintf("arena xray: %d marked, %d of them drawn in this view, %d outlined\n", numMarked, inView, n);
    }
}
