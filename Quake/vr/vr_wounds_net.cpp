// vr_wounds_net.cpp -- the wound events from the server: parsed (parseEvent, parseClear) and queued, painted within
// a frame's budget (vr_wounds_budget). Moved out of vr_wounds.cpp (see vr_wounds.hpp); what they share is
// vr_wounds_internal.hpp.

#include "vr_random.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_trace.hpp"
#include "vr_wounds_internal.hpp"
#include "vr_bloodprints.hpp"
#include "vr_dents.hpp"
#include "vr_bloodstream.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_modelcollide.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"
#include "vr_view.hpp"
#include "vr_avatar.hpp"
#include "vr_client.hpp"
#include "vr_held.hpp"
#include "vr_handrig.hpp"
#include "vr_progs.hpp"
#include "vr_water.hpp"
#include "vr_geom.hpp"
#include "vr_mem.hpp"

#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Remove.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Vocabulary/Span.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <string.h>

extern "C" qboolean Image_WritePNG(const char* name, byte* data, int width, int height, int bpp, qboolean upsidedown); // image.c
namespace qvr::wounds
{

using namespace detail;

namespace
{

constexpr int cleanFlag = 64; // QC vr_wounds.qc QVR_WOUND_CLEAN, added to the kind

// ----------------------------------------------------------------------------
// The frame's budget (vr_wounds_budget): a burst's events (an explosion's, a gib storm's, a dismembering: 400 at once,
// 10 ms) painted a budget a frame, the rest kept for the next frames in their body's own frame of reference (the
// entity's origin and turn when they came: a body moved or ragdolled since takes them where they struck it), dropped
// with their body (removed, gibbed: its model another). All of them painted within vr_wounds_budget_wait seconds: the
// rest of the wait shared out evenly over the frames left.

struct Pending
{
    Event ev;                       // its org and dir in the body's frame when `local`
    glm::vec3 worldOrg{0.f}, worldDir{0.f}; // as they came (taken as they are in the frame they came)
    const qmodel_t* model{nullptr}; // the body's model when it came
    double came{0.0};               // cl.time
    bool local{false};
    bool dropped{false};            // its body removed after it (a clear for it came)
};

struct WoundQueue
{
    za::Vector<Pending> items;
    za::SizeT head = 0;     // the next to paint
    double lastTime = -1.0; // cl.time at the last frame (the frame's length)
    float msPerEvent = 0.025f; // the painting's cost an event, measured (a running average)
    // vr_wounds_info's
    int mostQueued = 0, mostInFrame = 0, droppedTotal = 0, deferredTotal = 0;
};
WoundQueue queue;

// The kinds struck at a point of the body (kept in its frame); the liquids' are at a height of the world.
[[nodiscard]] bool bodyRelative(int kind)
{
    return kind == KindShot || kind == KindNail || kind == KindMelee || kind == KindBlast || kind == KindBurn || kind == KindZap;
}

// Entity `num`'s frame: its origin and turn (yours: your yaw alone), as its events are kept in.
bool bodyFrame(int num, glm::vec3& origin, glm::mat3& axes)
{
    if(num <= 0 || num >= cl.num_entities || !cl_entities[num].model)
    {
        return false;
    }
    const entity_t& e = cl_entities[num];
    vec3_t angles{e.angles[0], e.angles[1], e.angles[2]};
    if(num == cl.viewentity)
    {
        angles[0] = angles[2] = 0.f;
    }
    vec3_t f, r, u;
    AngleVectors(angles, f, r, u);
    axes = glm::mat3{geom::vec(f), glm::vec3{-r[0], -r[1], -r[2]}, geom::vec(u)};
    origin = geom::vec(e.origin);
    return true;
}

} // namespace

namespace detail
{

// The frame's new events into the queue, each in its body's frame; a removal drops what waits for that body.
void enqueue()
{
    for(const Event& ev : events)
    {
        Pending p;
        p.ev = ev;
        p.worldOrg = ev.org;
        p.worldDir = ev.dir;
        p.came = cl.time;
        if(ev.kind == KindClear)
        {
            for(za::SizeT i = queue.head; i < queue.items.size(); i++)
            {
                Pending& q = queue.items[i];
                if(q.ev.num == ev.num && q.ev.kind != KindClear && q.ev.kind != KindWeaponGone && !q.dropped)
                {
                    q.dropped = true;
                    queue.droppedTotal++;
                }
            }
        }
        glm::vec3 origin;
        glm::mat3 axes;
        if(bodyRelative(ev.kind) && bodyFrame(ev.num, origin, axes))
        {
            const glm::mat3 inv = glm::transpose(axes);
            p.ev.org = inv * (ev.org - origin);
            p.ev.dir = inv * ev.dir;
            p.model = cl_entities[ev.num].model;
            p.local = true;
        }
        queue.items.pushBack(p);
    }
    events.clear();
    queue.mostQueued = za::max(queue.mostQueued, static_cast<int>(queue.items.size() - queue.head));
}

} // namespace detail

namespace
{

// One kept event painted: back in the world from its body's frame now (dropped if its body is gone or another).
void applyPending(const Pending& p)
{
    if(p.dropped)
    {
        return;
    }
    if(!p.local || p.came == cl.time) // (in the frame it came: as it came)
    {
        Event ev = p.ev;
        ev.org = p.worldOrg;
        ev.dir = p.worldDir;
        apply(ev);
        return;
    }
    glm::vec3 origin;
    glm::mat3 axes;
    if(!bodyFrame(p.ev.num, origin, axes) || cl_entities[p.ev.num].model != p.model)
    {
        queue.droppedTotal++;
        return;
    }
    Event ev = p.ev;
    ev.org = origin + axes * p.ev.org;
    ev.dir = axes * p.ev.dir;
    apply(ev);
}

} // namespace

namespace detail
{

// The queue painted as far as the frame's budget allows (all of it with vr_wounds_budget 0, as before); returns how
// many were painted.
int paintQueue()
{
    const float budget = vr_wounds_budget.value;
    const double wait = za::max(vr_wounds_budget_wait.value, 0.f);
    const double frameTime = queue.lastTime >= 0.0 && cl.time > queue.lastTime ? cl.time - queue.lastTime : 1.0 / 90.0;
    queue.lastTime = cl.time;
    const za::SizeT waiting = queue.items.size() - queue.head;
    // The frame's allowance: the budget, or the rest shared out over the frames the oldest has left (all of it then).
    float allow = budget;
    if(budget > 0.f && waiting > 0)
    {
        const double left = wait - (cl.time - queue.items[queue.head].came);
        const double frames = za::max(1.0, za::floor(left / frameTime));
        allow = za::max(budget, static_cast<float>(queue.msPerEvent * static_cast<double>(waiting) / frames));
    }
    const double t0 = Sys_DoubleTime();
    int painted = 0;
    while(queue.head < queue.items.size())
    {
        const Pending p = queue.items[queue.head]; // (a copy: painting may not move the queue, but cheap to be sure)
        if(budget > 0.f && painted > 0 && cl.time - p.came < wait && (Sys_DoubleTime() - t0) * 1000.0 >= allow &&
            queue.items.size() <= 4096)
        {
            break;
        }
        queue.head++;
        applyPending(p);
        painted++;
    }
    if(painted > 0)
    {
        const float ms = static_cast<float>((Sys_DoubleTime() - t0) * 1000.0) / static_cast<float>(painted);
        queue.msPerEvent = queue.msPerEvent * 0.8f + ms * 0.2f;
    }
    queue.deferredTotal += static_cast<int>(queue.items.size() - queue.head);
    queue.mostInFrame = za::max(queue.mostInFrame, painted);
    if(queue.head >= queue.items.size())
    {
        queue.items.clear();
        queue.head = 0;
    }
    else if(queue.head >= 256)
    {
        queue.items.erase(queue.items.begin(), queue.items.begin() + static_cast<za::PtrDiffT>(queue.head));
        queue.head = 0;
    }
    return painted;
}

void clearQueue()
{
    queue.items.clear();
    queue.head = 0;
    queue.lastTime = -1.0;
}

void queueInfo()
{
    Con_Printf("wounds: %d events waiting (vr_wounds_budget %.2f ms, wait %.3f s; an event %.3f ms); most waiting %d, most painted "
               "in a frame %d, %d frames' waits so far, %d dropped with their body\n",
        static_cast<int>(queue.items.size() - queue.head), static_cast<double>(vr_wounds_budget.value),
        static_cast<double>(vr_wounds_budget_wait.value), static_cast<double>(queue.msPerEvent), queue.mostQueued, queue.mostInFrame,
        queue.deferredTotal, queue.droppedTotal);
}
} // namespace detail

void parseEvent()
{
    Event ev;
    ev.num = MSG_ReadShort();
    for(int i = 0; i < 3; i++)
    {
        ev.org[i] = MSG_ReadCoord(cl.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        ev.dir[i] = static_cast<float>(MSG_ReadChar()) / 127.f;
    }
    ev.kind = MSG_ReadByte();
    ev.amount = MSG_ReadByte();
    ev.extra = MSG_ReadByte();
    ev.clean = (ev.kind & cleanFlag) != 0; // (QC QVR_WOUND_CLEAN: a clean training dummy's, for its dents and streams)
    ev.kind &= ~cleanFlag;
    // A melee wound's extra: 1 a blunt blow, and over it whose and how (QC VR_Wound_Hit: bloodstream::Melee).
    int melee = bloodstream::MeleeNone;
    if(ev.kind == KindMelee)
    {
        melee = (ev.extra >> 1) & 3;
        ev.extra &= 1;
    }
    if(vr_wounds_debug.value >= 2)
    {
        Con_Printf("wounds: received kind %d on %d%s\n", ev.kind, ev.num, ev.clean ? " (clean dummy)" : "");
    }
    // A bullet hole may pour (vr_bloodstream.cpp; whether the wounds are painted or not; a clean dummy's with
    // vr_dummy_streams).
    if(!ev.clean || vr_dummy_streams.value)
    {
        bloodstream::onWound(ev.num, {ev.org[0], ev.org[1], ev.org[2]}, {ev.dir[0], ev.dir[1], ev.dir[2]}, ev.kind, ev.amount,
            melee);
    }
    if(enabled() && events.size() < 512)
    {
        events.pushBack(ev);
    }
}

void parseClear()
{
    Event ev;
    ev.num = MSG_ReadShort();
    ev.kind = KindClear;
    if(vr_wounds_debug.value >= 2)
    {
        Con_Printf("wounds: received clear on %d\n", ev.num);
    }
    bloodstream::forget(ev.num); // (its streams stop: what is falling lands)
    if(enabled() && events.size() < 512)
    {
        events.pushBack(ev);
    }
}

} // namespace qvr::wounds
