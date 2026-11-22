// vr_box3d_portaltest.cpp -- vr_physics_portal_sleep_test: props asleep by a teleporter's gate and away from every gate
// in vrteleporters (syncPortalCopies' sleeping props: a body asleep away from every gate is not looked at again until it
// wakes or moves; one asleep astride a gate keeps its copy). REVIEW_NEXT_2026-11-18.md, finding 15.
// vr_physics_gun_shapes_test: the guns lying in vrteleporters in their most pieces (vr_box3d_gun_pieces 32 or more): at
// most maxGunPieces each, and the one with the most put astride a gate has a copy with every one of them (finding 18).
// Split from vr_box3d_diag.cpp's tests; the shared declarations are in vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"
#include "vr_progs.hpp"
#include "vr_testkit.hpp"
#include "vr_cvars.hpp"
#include "Zancle/Base/Strcmp.hpp"

namespace box3d::detail
{
namespace
{
constexpr const char* portalTestName = "portalsleep";

// The test under way: its three crates (edict numbers), its phase and frames, what it counted.
struct PortalSleepTest
{
    bool on{false};
    int astride{0}; // A: straddling the flush player gate (x -256, its face y 640)
    int away{0};    // B: 48 units short of the large gate (x 0), then slid into it
    int moved{0};   // D: 80 units short of the wide gate (x 352), then moved astride it asleep (no wake)
    int phase{0};   // 0 settle, 1 hold, 2 B slid, 3 D moved, 4 D's copy
    int frames{0};  // in this phase
    bool astrideSlept{false}; // A fell asleep by itself while settling
    int held{0}, copied{0}, putToSleep{0}, lookedAt{0}, restedAway{0}, copiedAway{0};
    bool farSide{false};
    bool slidCopy{false}, slidRestCleared{false};
    testkit::Tally tally;
};
PortalSleepTest portalTest;

constexpr int portalSettleFrames = 1440; // 20 s at 72 Hz
constexpr int portalHoldFrames = 30;
constexpr int portalSlideFrames = 144;

[[nodiscard]] Slot* testSlot(int num)
{
    if(num <= 0 || num >= qcvm->num_edicts || num >= static_cast<int>(world->slots.size()) || EDICT_NUM(num)->free)
    {
        return nullptr;
    }
    Slot& s = world->slots[static_cast<za::SizeT>(num)];
    return s.kind == Kind::Prop && B3_IS_NON_NULL(s.body) && b3Body_IsValid(s.body) ? &s : nullptr;
}

[[nodiscard]] const World::PortalCopy* copyOf(b3BodyId body)
{
    for(const World::PortalCopy& c : world->portalCopies)
    {
        if(B3_ID_EQUALS(c.original, body) && b3Body_IsValid(c.copy))
        {
            return &c;
        }
    }
    return nullptr;
}

[[nodiscard]] bool restingAway(const Slot& s)
{
    return B3_ID_EQUALS(s.restBody, s.body);
}

void portalCheck(bool ok, const char* what)
{
    portalTest.tally.check(ok);
    Con_Printf("%s: %s %s\n", portalTestName, ok ? "PASS" : "FAIL", what);
}

void portalFinish()
{
    portalTest.tally.verdict(portalTestName);
    portalTest.on = false;
}

// A shape of `copy` in a small box at `at` (units): the copy collides there, in the room beyond the gate.
[[nodiscard]] bool copyCollidesAt(b3BodyId copy, const glm::vec3& at)
{
    struct Query
    {
        b3BodyId copy;
        bool found{false};
    } q{copy};
    b3AABB box;
    box.lowerBound = world->toM(at - glm::vec3{3.f});
    box.upperBound = world->toM(at + glm::vec3{3.f});
    b3QueryFilter filter = b3DefaultQueryFilter();
    b3World_OverlapAABB(world->id, box, filter,
        [](b3ShapeId shape, void* context) {
            Query& query = *static_cast<Query*>(context);
            query.found |= B3_ID_EQUALS(b3Shape_GetBody(shape), query.copy);
            return !query.found;
        },
        &q);
    return q.found;
}
} // namespace

// vr_physics_portal_sleep_test (vrteleporters): three crates, one straddling the flush player gate and two short of
// gates, left to settle; then, step by step: the one astride keeps its copy, asleep too (it is put to sleep before each
// step: by itself the copy's contacts keep it awake), and its copy collides in the room beyond; the two asleep away from
// the gates are not looked at and have none; one of them slid into its gate (woken by QC's velocity) gets its copy; the
// other moved astride its gate while asleep (b3Body_SetTransform: no wake) gets its copy at the next step.
void portalSleepTest_f()
{
    if(!sv.active || !world || svs.maxclients < 1 || ZA_STRCMP(sv.name, "vrteleporters") != 0)
    {
        Con_Printf("%s: FAIL needs map vrteleporters\n", portalTestName);
        testkit::verdict(portalTestName, 0, 1);
        return;
    }
    const progs::ServerVm vm;
    Cmd_ExecuteString("setpos -256 336 24 0 90 0", src_command); // (the player out of the way)
    portalTest = PortalSleepTest{};
    const auto crate = [](float x, float y) {
        edict_t* e = qvr::box3d::spawnClass("vr_crate", glm::vec3{x, y, 17.f}, glm::vec3{0.f});
        return e ? NUM_FOR_EDICT(e) : 0;
    };
    portalTest.astride = crate(-256.f, 640.f);
    portalTest.away = crate(0.f, 592.f);
    portalTest.moved = crate(352.f, 560.f);
    if(!portalTest.astride || !portalTest.away || !portalTest.moved)
    {
        portalCheck(false, "three crates spawned");
        portalFinish();
        return;
    }
    portalTest.on = true;
    Con_Printf("%s: crates %d (astride), %d (away, slid), %d (away, moved asleep)\n", portalTestName, portalTest.astride,
        portalTest.away, portalTest.moved);
}

// Each server frame, before the step: the last step's copies looked at, the next phase.
void portalSleepTestFrame()
{
    PortalSleepTest& t = portalTest;
    if(!t.on || !world)
    {
        return;
    }
    Slot* a = testSlot(t.astride);
    Slot* b = testSlot(t.away);
    Slot* d = testSlot(t.moved);
    if(!a || !b || !d)
    {
        if(t.phase == 0 && ++t.frames < 30)
        {
            return; // (their bodies made at the next sync)
        }
        portalCheck(false, "the crates have and keep their bodies");
        portalFinish();
        return;
    }
    t.frames++;
    if(t.phase == 0)
    {
        t.astrideSlept |= !b3Body_IsAwake(a->body);
        if(t.frames < 144 || b3Body_IsAwake(b->body) || b3Body_IsAwake(d->body)) // (2 s at least: settled)
        {
            if(t.frames >= portalSettleFrames)
            {
                portalCheck(false, "the crates away from the gates fell asleep");
                portalFinish();
            }
            return;
        }
        Con_Printf("%s: settled in %d frames; the crate astride %s\n", portalTestName, t.frames,
            t.astrideSlept ? "fell asleep by itself" : "never fell asleep by itself (its copy's contacts wake it)");
        portalCheck(copyOf(a->body) != nullptr, "the crate astride the gate has its copy once settled");
        t.phase = 1;
        t.frames = 0;
    }
    else if(t.phase == 1)
    {
        // (what the last step's syncPortalCopies did, A put to sleep before it)
        t.held++;
        const World::PortalCopy* c = copyOf(a->body);
        t.copied += c != nullptr;
        t.lookedAt += !restingAway(*a);
        t.restedAway += restingAway(*b) && restingAway(*d) && !b3Body_IsAwake(b->body) && !b3Body_IsAwake(d->body);
        t.copiedAway += copyOf(b->body) != nullptr || copyOf(d->body) != nullptr;
        if(c && !t.farSide)
        {
            // Its half past the far face (928): a shape of the copy there, as many shapes as the crate's.
            const glm::vec3 at = world->toU(b3Body_GetPosition(c->copy));
            t.farSide = at.y > 900.f && b3Body_GetShapeCount(c->copy) == b3Body_GetShapeCount(a->body) &&
                        copyCollidesAt(c->copy, at + glm::vec3{0.f, 8.f, 0.f});
        }
        if(t.frames >= portalHoldFrames)
        {
            portalCheck(t.copied == t.held, va("the crate astride the gate kept its copy every step (%d of %d)", t.copied, t.held));
            portalCheck(t.lookedAt == t.held, va("and was looked at every step (%d of %d)", t.lookedAt, t.held));
            portalCheck(t.putToSleep * 2 >= t.held, va("asleep at the step's start in %d of %d (at least half)", t.putToSleep, t.held));
            portalCheck(t.farSide, "its copy has its shapes and collides in the room beyond the gate");
            portalCheck(t.restedAway == t.held, va("the crates asleep away from the gates were not looked at (%d of %d steps)", t.restedAway, t.held));
            portalCheck(t.copiedAway == 0, va("and had no copy (%d steps with one)", t.copiedAway));
            // B slid into the large gate: QC's velocity (feedProp wakes it).
            store(glm::vec3{0.f, 300.f, 0.f}, EDICT_NUM(t.away)->v.velocity);
            t.phase = 2;
            t.frames = 0;
            return;
        }
        // (put to sleep for the coming step's syncPortalCopies; the copy's contacts may wake it in the step)
        b3Body_SetAwake(a->body, false);
        t.putToSleep += !b3Body_IsAwake(a->body);
    }
    else if(t.phase == 2)
    {
        t.slidCopy |= copyOf(b->body) != nullptr;
        t.slidRestCleared |= b3Body_IsAwake(b->body) && !restingAway(*b);
        if(!t.slidCopy && t.frames < portalSlideFrames)
        {
            return;
        }
        portalCheck(t.slidRestCleared, "the crate woken (slid) was looked at again");
        portalCheck(t.slidCopy, va("and got its copy going into the gate (after %d frames)", t.frames));
        portalCheck(!b3Body_IsAwake(d->body) && restingAway(*d), "the other crate is still asleep away from the gates");
        // D moved astride the wide gate asleep: its body only (no wake), turned as it lay.
        const b3WorldTransform pose = b3Body_GetTransform(d->body);
        glm::vec3 to = world->toU(pose.p);
        to.y = 640.f;
        b3Body_SetTransform(d->body, world->toM(to), pose.q);
        t.phase = 3;
        t.frames = 0;
    }
    else if(t.phase == 3)
    {
        // (the step after the move)
        portalCheck(copyOf(d->body) != nullptr, va("the crate moved astride a gate while asleep got its copy at the next step (%s)",
                                                    b3Body_IsAwake(d->body) ? "woken" : "still asleep"));
        portalCheck(!restingAway(*d), "and is looked at again");
        portalFinish();
    }
}

namespace
{
constexpr const char* gunTestName = "gunshapes";

// The test under way: the gun lying with the most shapes, its shapes, the frames since it was put astride the gate.
struct GunShapesTest
{
    bool on{false};
    int gun{0};
    int shapes{0};
    int frames{0};
    testkit::Tally tally;
};
GunShapesTest gunTest;

void gunCheck(bool ok, const char* what)
{
    gunTest.tally.check(ok);
    Con_Printf("%s: %s %s\n", gunTestName, ok ? "PASS" : "FAIL", what);
}

void gunFinish()
{
    gunTest.tally.verdict(gunTestName);
    gunTest.on = false;
}
} // namespace

// vr_physics_gun_shapes_test (vrteleporters, vr_box3d_gun_pieces 32 or more before the map: the guns lying there made in
// their most pieces): each gun lying has at most maxGunPieces pieces (one more with a seated magazine); the one with the
// most is put straddling the flush player gate (its entity moved, as QC would) and its copy there has as many shapes.
void gunShapesTest_f()
{
    gunTest = GunShapesTest{};
    if(!sv.active || !world || svs.maxclients < 1 || ZA_STRCMP(sv.name, "vrteleporters") != 0 || vr_box3d_gun_pieces.value < 32.f)
    {
        gunCheck(false, "needs map vrteleporters with vr_box3d_gun_pieces 32 or more");
        gunFinish();
        return;
    }
    const progs::ServerVm vm;
    Cmd_ExecuteString("setpos -256 336 24 0 90 0", src_command); // (the player out of the way)
    int guns = 0, within = 0, most = 0;
    for(int num = nextActiveSlot(0); num > 0 && num < static_cast<int>(world->slots.size()); num = nextActiveSlot(num))
    {
        const Slot& s = world->slots[static_cast<za::SizeT>(num)];
        edict_t* e = num < qcvm->num_edicts ? EDICT_NUM(num) : nullptr;
        if(!e || e->free || s.kind != Kind::Prop || !isGun(e) || B3_IS_NULL(s.body) || !b3Body_IsValid(s.body))
        {
            continue;
        }
        const int n = b3Body_GetShapeCount(s.body);
        guns++;
        within += n <= maxGunPieces + (s.mag ? 1 : 0);
        Con_Printf("%s: %d %s: %d shapes\n", gunTestName, num, PR_GetString(e->v.model), n);
        if(n > most)
        {
            most = n;
            gunTest.gun = num;
        }
    }
    gunCheck(guns > 0, va("guns lying (%d)", guns));
    gunCheck(within == guns, va("each in at most %d pieces, its magazine one more (%d of %d)", maxGunPieces, within, guns));
    gunCheck(most > 16, va("the most shapes a gun has, %d, are more than the copies' old 16", most));
    if(!gunTest.gun)
    {
        gunFinish();
        return;
    }
    // Straddling the gate, pointing through it: moved as QC moves a prop (feedProp: its body there, woken).
    edict_t* e = EDICT_NUM(gunTest.gun);
    store(glm::vec3{-256.f, 640.f, 24.f}, e->v.origin);
    store(glm::vec3{0.f, 90.f, 0.f}, e->v.angles);
    gunTest.shapes = most;
    gunTest.on = true;
}

// Each server frame, before the step: the gun's copy, made by the last step's syncPortalCopies.
void gunShapesTestFrame()
{
    GunShapesTest& t = gunTest;
    if(!t.on || !world)
    {
        return;
    }
    Slot* s = testSlot(t.gun);
    if(!s)
    {
        gunCheck(false, "the gun keeps its body");
        gunFinish();
        return;
    }
    const World::PortalCopy* c = copyOf(s->body);
    if(!c && ++t.frames < 10)
    {
        return;
    }
    gunCheck(c != nullptr, va("the gun straddling the gate has its copy (after %d frames)", t.frames));
    const int n = c ? b3Body_GetShapeCount(c->copy) : 0;
    gunCheck(n == b3Body_GetShapeCount(s->body), va("its copy has every one of its shapes (%d of %d)", n, b3Body_GetShapeCount(s->body)));
    gunFinish();
}
} // namespace box3d::detail
