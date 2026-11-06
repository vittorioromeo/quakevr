// vr_selfshot.cpp -- see vr_selfshot.hpp.

#include "vr_selfshot.hpp"
#include "vr_avatar.hpp"
#include "vr_flatplay.hpp"
#include "vr_geom.hpp"
#include "vr_progs.hpp"
#include "vr_view.hpp"

#include "Zancle/Math/MinMax.hpp"

namespace qvr::selfshot
{

namespace
{

// The last segment's meeting (selfhit_info 0) and the precise wounds painted (1..3): one record, the tests read it.
struct Record
{
    int lastPart{-1};
    int preciseWounds{0};
    float preciseOff{-1.f};
    int precisePart{-1};
};
Record record;

constexpr float chinUnderEyes = 6.f; // units: the torso's top below the eyes (the view's height) for your own shots

// The distance from p to the capsule's axis, less its radius (and the swept sphere's): <= 0 inside.
[[nodiscard]] float gap(const Capsule& c, const glm::vec3& p, float radius)
{
    return glm::length(p - geom::closestOnSegment(p, c.a, c.b)) - c.r - radius;
}

} // namespace

Capsules build(entity_t* const own[3])
{
    Capsules out;
    const glm::vec3 up{0.f, 0.f, 1.f};
    if(own[0])
    {
        const glm::vec3 pelvis{own[0]->origin[0], own[0]->origin[1], own[0]->origin[2]};
        out.pushBack({pelvis - up * 4.f, pelvis + up * 24.f, 7.5f, PartTorso});
        out.pushBack({pelvis - up * 30.f, pelvis - up * 4.f, 6.f, PartLegs});
    }
    for(int hand = 0; hand < 2; hand++) // (HAND_OFF 0, HAND_MAIN 1)
    {
        const int forearmPart = hand == 0 ? PartOffForearm : PartMainForearm;
        glm::vec3 wrist, dir;
        if(avatar::forearm(hand, wrist, dir))
        {
            avatar::ForearmFrame f;
            const float len = avatar::forearmFrame(hand, 1.f, f) && f.length > 1.f ? f.length : 10.f;
            out.pushBack({wrist - dir * len, wrist, 2.3f, forearmPart});
            out.pushBack({wrist + dir * 1.f, wrist + dir * 5.5f, 3.2f, forearmPart + 1});
        }
        else if(own[1 + hand])
        {
            const glm::vec3 c{own[1 + hand]->origin[0], own[1 + hand]->origin[1], own[1 + hand]->origin[2]};
            out.pushBack({c, c, 3.5f, forearmPart + 1});
        }
    }
    return out;
}

Capsules current()
{
    entity_t* own[3]{};
    view::woundTargets(own);
    Capsules caps = build(own);
    // The torso's top under the chin (the wounds' reaches round the head: an enemy's hit there is painted on the
    // body): your own shot at your face is not met (the head is no part).
    const float chin = r_refdef.vieworg[2] - chinUnderEyes;
    for(Capsule& c : caps)
    {
        if(c.part == PartTorso && c.b.z + c.r > chin)
        {
            c.b.z = za::max(c.a.z, chin - c.r);
        }
    }
    return caps;
}

bool segment(const Capsules& caps, const glm::vec3& a, const glm::vec3& b, float radius, int skip, Hit& out)
{
    bool found = false;
    for(const Capsule& c : caps)
    {
        if(skip & (1 << c.part))
        {
            continue;
        }
        // The distance to a convex shape along a line is convex: its least on the segment (a ternary search), then
        // where it first comes within the capsule before there (bisection).
        float t = 0.f;
        if(gap(c, a, radius) > 0.f)
        {
            float lo = 0.f, hi = 1.f;
            for(int i = 0; i < 48; i++)
            {
                const float m1 = lo + (hi - lo) / 3.f, m2 = hi - (hi - lo) / 3.f;
                if(gap(c, a + (b - a) * m1, radius) < gap(c, a + (b - a) * m2, radius))
                {
                    hi = m2;
                }
                else
                {
                    lo = m1;
                }
            }
            const float least = (lo + hi) * 0.5f;
            if(gap(c, a + (b - a) * least, radius) > 0.f)
            {
                continue;
            }
            lo = 0.f;
            hi = least;
            for(int i = 0; i < 32; i++)
            {
                const float mid = (lo + hi) * 0.5f;
                (gap(c, a + (b - a) * mid, radius) > 0.f ? lo : hi) = mid;
            }
            t = hi;
        }
        if(found && t >= out.fraction)
        {
            continue;
        }
        found = true;
        out.fraction = t;
        out.at = a + (b - a) * t;
        const glm::vec3 d = out.at - geom::closestOnSegment(out.at, c.a, c.b);
        const float l = glm::length(d);
        out.normal = l > 1e-4f ? d / l : (glm::length(b - a) > 1e-4f ? -glm::normalize(b - a) : glm::vec3{0.f, 0.f, 1.f});
        out.part = c.part;
    }
    return found;
}

bool isLocalPlayer(const edict_t* ent)
{
    if(!ent || !sv.active || cls.state != ca_connected || cls.demoplayback || flatplay::active())
    {
        return false;
    }
    const int num = static_cast<int>((reinterpret_cast<const byte*>(ent) - reinterpret_cast<const byte*>(qcvm->edicts)) / qcvm->edict_size);
    return num > 0 && num <= svs.maxclients && num == cl.viewentity;
}

void notePreciseWound(float off, int part)
{
    record.preciseWounds++;
    record.preciseOff = off;
    record.precisePart = part;
}

// float selfhit_segment(entity pl, vector a, vector b, float radius, float skip): the segment a..b (a sphere of
// `radius` swept along it) against `pl`'s own drawn body and hands (pl the listen server's own player in a headset;
// else nothing): how far along it first meets one not in `skip` (1 << QVR_SS_PART_*; 0: it starts inside), -1 none.
// Its part: selfhit_info(0).
void PF_selfhit_segment()
{
    G_FLOAT(OFS_RETURN) = -1.f;
    record.lastPart = -1;
    const edict_t* pl = G_EDICT(OFS_PARM0);
    if(!isLocalPlayer(pl))
    {
        return;
    }
    Hit hit;
    if(segment(current(), geom::vec(G_VECTOR(OFS_PARM1)), geom::vec(G_VECTOR(OFS_PARM2)), za::max(0.f, G_FLOAT(OFS_PARM3)),
           static_cast<int>(G_FLOAT(OFS_PARM4)), hit))
    {
        G_FLOAT(OFS_RETURN) = hit.fraction;
        record.lastPart = hit.part;
    }
}

// float selfhit_info(float what): 0 the part the last selfhit_segment or missile met (-1 none); 1 the precise wounds
// painted on you so far (a self-hit's, at its point); 2 how far the last one landed from its point; 3 its part; 4 the
// capsules drawn now.
void PF_selfhit_info()
{
    const int what = static_cast<int>(G_FLOAT(OFS_PARM0));
    float v = -1.f;
    switch(what)
    {
        case 0: v = static_cast<float>(record.lastPart); break;
        case 1: v = static_cast<float>(record.preciseWounds); break;
        case 2: v = record.preciseOff; break;
        case 3: v = static_cast<float>(record.precisePart); break;
        case 4: v = static_cast<float>(current().size()); break;
        default: break;
    }
    G_FLOAT(OFS_RETURN) = v;
}

// vector selfhit_point(float part, float along): the point `along` the axis of the drawn capsule of `part` (0 its
// first end, 1 its second: a forearm's elbow and wrist, a hand's wrist and fingers, the torso's and the legs' bottom
// and top), '0 0 0' if it is not drawn (the tests' aim).
void PF_selfhit_point()
{
    const int part = static_cast<int>(G_FLOAT(OFS_PARM0));
    const float along = G_FLOAT(OFS_PARM1);
    glm::vec3 p{0.f};
    for(const Capsule& c : current())
    {
        if(c.part == part)
        {
            p = c.a + (c.b - c.a) * along;
        }
    }
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = p.x;
    out[1] = p.y;
    out[2] = p.z;
}

} // namespace qvr::selfshot

// SV_PushEntity: a hand's missile (.vr_ss_skip, set by QC VR_ShotLaunched) meets its shooter's own drawn body and
// hands before what its move met: it stops there, struck (.vr_ss_struck, its time: its touch meets him, QC
// VR_OwnerSpared), and never again (.vr_ss_skip 0). Started inside one (a muzzle against your forearm): there.
extern "C" void VR_SelfShotMove(edict_t* missile, trace_t* trace, const float end[3])
{
    using namespace qvr;
    const progs::FieldOffsets& f = progs::fields();
    if(f.vr_ss_skip < 0 || f.vr_ss_struck < 0 || progs::fieldFloat(missile, f.vr_ss_skip) < 64.f || !missile->v.owner)
    {
        return;
    }
    edict_t* owner = PROG_TO_EDICT(missile->v.owner);
    if(!selfshot::isLocalPlayer(owner))
    {
        return;
    }
    const int skip = static_cast<int>(progs::fieldFloat(missile, f.vr_ss_skip)) - 64;
    const glm::vec3 a = geom::vec(missile->v.origin);
    const glm::vec3 b = geom::vec(end);
    const float radius = za::min(4.f, za::max(0.f, (missile->v.maxs[0] - missile->v.mins[0]) * 0.5f));
    selfshot::Hit hit;
    if(!selfshot::segment(selfshot::current(), a, b, radius, skip, hit) || hit.fraction > trace->fraction)
    {
        return;
    }
    selfshot::record.lastPart = hit.part;
    const float len = glm::length(b - a);
    const float back = len > 0.01f ? za::min(hit.fraction, 0.1f / len) : 0.f; // (just before its surface)
    trace->fraction = hit.fraction - back;
    const glm::vec3 at = a + (b - a) * trace->fraction;
    for(int i = 0; i < 3; i++)
    {
        trace->endpos[i] = at[i];
        trace->plane.normal[i] = hit.normal[i];
    }
    trace->plane.dist = glm::dot(hit.normal, at);
    trace->allsolid = trace->startsolid = false;
    trace->ent = owner;
    progs::fieldFloat(missile, f.vr_ss_skip) = 0.f;
    progs::fieldFloat(missile, f.vr_ss_struck) = static_cast<float>(qcvm->time);
}
