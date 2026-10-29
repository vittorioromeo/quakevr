// vr_box3d_presstest.cpp -- vr_prop_press_test: a hand, a held weapon or a held prop pressed down on a solid prop (a crate)
// lying on the floor, harder and harder (ROUND22.md, "Pressing on props: the hand gives"). The crate must not sink into
// the floor nor fly off; the hand's bodies are held back on its top (vr_box3d_press_back) instead.
// Split from vr_box3d_diag.cpp's tests; the shared declarations are in vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"
#include "vr_progs.hpp"

namespace box3d::detail
{
namespace
{
// The test under way (vr_prop_press_test): the prop pressed on, the hand's target over it, what was measured.
struct PressTest
{
    bool on{false};
    int prop{0};
    int which{1}; // 1 the main hand, 0 the off hand
    double start{0.0};      // server time
    glm::vec3 over{0.f};    // units: the prop's top's middle at the start
    float lowStart{0.f};    // units: the floor under the prop
    float startAbove{0.f};  // units: the hand's target over the top at the start
    float speed{20.f};      // units/s down
    float past{16.f};       // units the hand goes on down after its bodies first touch the prop
    float touchedAt{0.f};   // units over the top the hand was as they first touched
    bool touched{false};
    double holdUntil{0.0};  // pressing still, then up
    double upUntil{0.0};
    float hand{0.f};        // units: the hand's target over the top now
    float sent{0.f};        // and as last sent to the mock (moved by the change since: "by", not to a point the view pushes)
    bool placed{false};
    float sinkMost{0.f};    // units the prop's lowest point went down at most
    float speedMost{0.f};   // m/s the prop's fastest
    float speedAt{0.f};     // the hand over the top then
    double speedTime{0.0};  // and the time
    float pressMost{0.f};   // units the hand's bodies were held back at most (vr_box3d_press_back)
    bool gone{false};
    double nextLine{0.0};
};
PressTest press;

// Whether the prop's body touches one of the hands' bodies (a fist, a reach body, a held thing).
[[nodiscard]] bool touchesHand(b3BodyId prop)
{
    b3ContactData contacts[64];
    const int count = b3Body_GetContactData(prop, contacts, 64);
    for(int c = 0; c < count; c++)
    {
        const b3ContactData& cd = contacts[c];
        const bool propA = B3_ID_EQUALS(b3Shape_GetBody(cd.shapeIdA), prop);
        const b3ShapeId other = propA ? cd.shapeIdB : cd.shapeIdA;
        if((b3Shape_GetFilter(other).categoryBits & (catHand | catReach | catHeld)) != 0 && cd.manifoldCount > 0)
        {
            return true;
        }
    }
    return false;
}

// The lowest point of the body's hulls (units; its box's bottom if it has none).
[[nodiscard]] float lowestPoint(b3BodyId body)
{
    const b3WorldTransform xf = b3Body_GetTransform(body);
    b3ShapeId shapes[maxBodyShapes];
    const int n = b3Body_GetShapes(body, shapes, maxBodyShapes);
    float low = 1e30f;
    for(int i = 0; i < n; i++)
    {
        const b3HullData* hull = b3Shape_GetType(shapes[i]) == b3_hullShape ? b3Shape_GetHull(shapes[i]) : nullptr;
        const b3Vec3* points = hull ? b3GetHullPoints(hull) : nullptr;
        for(int k = 0; points && k < hull->vertexCount; k++)
        {
            low = za::min(low, (b3RotateVector(xf.q, points[k]).z + xf.p.z) * world->m2u);
        }
    }
    return low < 1e29f ? low : world->toU(b3Body_ComputeAABB(body).lowerBound).z;
}

void pressFinish(const char* why)
{
    Con_Printf("presstest: done (%s): sink %.2f units at most, fastest %.2f m/s, hand held back %.2f units at most, %s%s\n", why,
        press.sinkMost, press.speedMost, press.pressMost, press.gone ? "the prop gone (broken)" : "the prop whole",
        !press.touched ? ", never touched" : "");
    Con_Printf("presstest: fastest at %.2f s, the hand %+.1f over the top\n", press.speedTime, press.speedAt);
    const bool pass = !press.gone && press.touched && press.sinkMost < 1.f && press.speedMost < 1.f;
    Con_Printf("presstest: %s\n", pass ? "PASS" : "FAIL");
    press.on = false;
}
} // namespace

// vr_prop_press_test [main|off [<cm/s down> [<cm past the touch>]]]: that hand (as it is: empty, a weapon, a prop held) put over
// the solid prop nearest the first player, then down onto it at that speed (50) until that far past where its bodies
// first touched it (40), held there half a second, and up again. A line every 0.25 s and at the end (PASS: the prop
// sank less than a unit, never moved at 1 m/s or more, and is whole).
void pressTest_f()
{
    if(!sv.active || !world || svs.maxclients < 1)
    {
        Con_Printf("presstest: no game\n");
        return;
    }
    const progs::ServerVm vm;
    edict_t* player = EDICT_NUM(1);
    const glm::vec3 eye = vec(player->v.origin);
    int best = 0;
    float bestDist = 1e30f;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts && num < static_cast<int>(world->slots.size()); num++)
    {
        const Slot& s = world->slots[static_cast<size_t>(num)];
        edict_t* e = EDICT_NUM(num);
        if(e->free || s.kind != Kind::Prop || !isSolidProp(e) || B3_IS_NULL(s.body) || !b3Body_IsValid(s.body))
        {
            continue;
        }
        const float d = glm::distance(vec(e->v.origin), eye);
        if(d < bestDist)
        {
            best = num;
            bestDist = d;
        }
    }
    if(!best)
    {
        Con_Printf("presstest: no solid prop\n");
        return;
    }
    const b3AABB box = b3Body_ComputeAABB(world->slots[static_cast<size_t>(best)].body);
    const glm::vec3 lo = world->toU(box.lowerBound), hi = world->toU(box.upperBound);
    press = PressTest{};
    press.on = true;
    press.prop = best;
    press.start = qcvm->time;
    press.over = glm::vec3{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, hi.z};
    {
        // (Its top's near side, a third of the way in: the middle of a big crate is out of an arm's reach.)
        glm::vec3 toward = eye - press.over;
        toward.z = 0.f;
        if(glm::length(toward) > 1.f)
        {
            press.over += glm::normalize(toward) * (za::min(hi.x - lo.x, hi.y - lo.y) * 0.5f * 0.45f);
        }
    }
    {
        // The floor under it (its lowest point must not go below: sunk into it).
        edict_t* ent = EDICT_NUM(best);
        vec3_t from{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f}, to{from[0], from[1], lo.z - 64.f};
        const trace_t tr = SV_Move(from, vec3_origin, vec3_origin, to, MOVE_NOMONSTERS, ent);
        press.lowStart = tr.fraction < 1.f ? tr.endpos[2] : lo.z;
        const b3BodyId body = world->slots[static_cast<size_t>(best)].body;
        const float gap = lowestPoint(body) - press.lowStart;
        Con_Printf("presstest: its lowest point %.2f units over the floor\n", gap);
        if(gap > 0.25f)
        {
            // (A test crate lies asleep where it was lifted out of the floor it was made in: put down on it, woken.)
            b3WorldTransform xf = b3Body_GetTransform(body);
            xf.p.z -= (gap - 0.1f) / world->m2u;
            b3Body_SetTransform(body, xf.p, xf.q);
            b3Body_SetAwake(body, true);
            press.over.z -= gap - 0.1f;
        }
    }
    const float cm = world->m2u / 100.f;
    press.speed = (Cmd_Argc() > 2 ? Q_atof(Cmd_Argv(2)) : 50.f) * cm;
    press.past = (Cmd_Argc() > 3 ? Q_atof(Cmd_Argv(3)) : 40.f) * cm;
    press.which = Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "off") ? 0 : 1;
    if(world->hands.size() > 1)
    {
        const World::HandBody& hb = world->hands[1][static_cast<size_t>(press.which)];
        Con_Printf("presstest: the %s hand: fist %s, reach body %d (0 none, 1 open, 2 fist, 3 weapon), holding %d\n",
            press.which ? "main" : "off", hb.armed ? "armed" : "off", static_cast<int>(hb.key.what), hb.held);
    }
    press.startAbove = 80.f;
    press.hand = press.startAbove;
    Con_Printf("presstest: %d %s (%.1f kg), top at %.1f %.1f %.1f, %.0f units across; down at %.1f units/s, %.1f units past the touch\n",
        best, PR_GetString(EDICT_NUM(best)->v.classname), b3Body_GetMass(world->slots[static_cast<size_t>(best)].body),
        press.over.x, press.over.y, press.over.z, hi.x - lo.x, press.speed, press.past);
}

// Each server frame, before the step: the hand's next target, what the last steps did to the prop.
void pressTestFrame()
{
    if(!press.on || !world)
    {
        return;
    }
    const float dt = static_cast<float>(host_frametime);
    const Slot* s = press.prop < static_cast<int>(world->slots.size()) ? &world->slots[static_cast<size_t>(press.prop)] : nullptr;
    if(!s || s->kind != Kind::Prop || B3_IS_NULL(s->body) || !b3Body_IsValid(s->body) || EDICT_NUM(press.prop)->free)
    {
        press.gone = true;
        pressFinish("the prop is gone");
        return;
    }
    press.sinkMost = za::max(press.sinkMost, press.lowStart - lowestPoint(s->body));
    if(const float v = glm::length(glmv(b3Body_GetLinearVelocity(s->body))); v > press.speedMost && qcvm->time - press.start > 0.5)
    {
        press.speedMost = v;
        press.speedAt = press.hand;
        press.speedTime = qcvm->time - press.start;
    }
    float held = 0.f;
    if(world->hands.size() > 1)
    {
        held = glm::length(world->hands[1][static_cast<size_t>(press.which)].press);
    }
    press.pressMost = za::max(press.pressMost, held);
    if(!press.touched && (touchesHand(s->body) || held > 0.f))
    {
        press.touchedAt = press.hand;
        press.touched = true;
        Con_Printf("presstest: touched with the hand %.1f units over the top\n", press.hand);
    }
    if(press.upUntil > 0.0)
    {
        if(qcvm->time >= press.upUntil)
        {
            pressFinish("up again");
            return;
        }
        press.hand += press.speed * 2.f * dt;
    }
    else if(press.holdUntil > 0.0)
    {
        if(qcvm->time >= press.holdUntil)
        {
            press.upUntil = qcvm->time + 1.0;
        }
    }
    else
    {
        press.hand -= press.speed * dt;
        const float floor = press.touched ? press.touchedAt - press.past : -press.past;
        if(press.hand <= floor)
        {
            press.hand = floor;
            press.holdUntil = qcvm->time + 0.5;
        }
    }
    const glm::vec3 at = press.over + glm::vec3{0.f, 0.f, press.hand};
    if(!press.placed)
    {
        Cmd_ExecuteString(va("vr_mock_hand_to %s %.2f %.2f %.2f", press.which ? "main" : "off", at.x, at.y, at.z), src_command);
        press.placed = true;
    }
    else
    {
        Cmd_ExecuteString(va("vr_mock_hand_to %s by 0 0 %.3f", press.which ? "main" : "off", press.hand - press.sent), src_command);
    }
    press.sent = press.hand;
    if(qcvm->time >= press.nextLine)
    {
        press.nextLine = qcvm->time + (press.touched && press.hand > press.touchedAt - 2.f ? 0.0 : 0.25);
        const int ofs = progs::handFields(press.which).pos;
        const float game = ofs >= 0 ? fieldVec(EDICT_NUM(1), ofs).z - press.over.z : 0.f;
        Con_Printf("presstest: t %.2f hand %+.1f over the top (the game's %+.1f); prop low %+.2f, %.2f m/s; held back %.2f\n",
            qcvm->time - press.start, press.hand, game, lowestPoint(s->body) - press.lowStart,
            glm::length(glmv(b3Body_GetLinearVelocity(s->body))), held);
    }
    if(qcvm->time - press.start > 20.0)
    {
        pressFinish("timed out");
    }
}
} // namespace box3d::detail
