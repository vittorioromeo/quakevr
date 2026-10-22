#include "vr_alloccount.h"
// vr_box3d_hands.cpp -- throws, gib contacts, what was born inside what, the hands' spheres and the reach bodies (hands
// and weapons).
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"

namespace box3d::detail
{
namespace
{
// ---------------------------------------------------------------------------------------------------------------------
// Throws (vr_box3d_throw_grace; ROUND21.md, "Throws leave the hand clean"): a thing thrown (a weapon, a prop let go of)
// left the hand's bodies (the open hand or fist it becomes, the fists' spheres) touching or sunk in them, and the hand,
// still moving (a wrist snap turning it, the follow-through catching up), knocked it as it left: the wrong way, the
// wrong spin. Thrown faster than vr_box3d_throw_grace_speed, it passes through both of its thrower's hands' bodies for
// vr_box3d_throw_grace seconds, and through its thrower's body (vr_box3d_throw_grace_body: an overhead wrist snap threw
// a box down into the player's capsule). A slower release (the hand opened, held still) keeps meeting them: it rests on the palm.

// How fast hand `h` of `player` throws a thing reaching `reach` m from its middle (m/s): the hand's throw estimate (as
// QC throws with: VRGetHandThrowVel), or the thing's tip turned by the hand's spin (a wrist snap), whichever is faster.
[[nodiscard]] float throwSpeed(edict_t* player, int h, float reach)
{
    const FieldOffsets& f = fields();
    const int velOfs = h ? f.handthrowvel : f.offhandthrowvel;
    const int spinOfs = h ? f.handavel : f.offhandavel;
    const float linear = glm::length(fieldVec(player, velOfs)); // (0 without the field: fieldVec)
    const float turn = glm::length(fieldVec(player, spinOfs)) * reach;
    return za::max(linear, turn);
}
} // namespace

// Whether prop `num` passes through the hands' bodies of client `player` now (a throw's grace).
[[nodiscard]] bool graced(int player, int num)
{
    for(World::Grace& g : world->graces)
    {
        if(g.num == num && g.player == player && g.skips && qcvm->time < g.until)
        {
            g.skipped.n.fetchAddRelaxed(1);
            return true;
        }
    }
    return false;
}
bool isSmallGib(edict_t* ent)
{
    const int f = fields().vr_prop_mass;
    return f >= 0 && fieldFloat(ent, f) > 0.f;
}
float monsterDropAge(edict_t* ent)
{
    const int f = fields().vr_monster_drop;
    const float made = f >= 0 ? fieldFloat(ent, f) : 0.f;
    return made > 0.f ? static_cast<float>(qcvm->time - made) : -1.f;
}

namespace
{
// A small gib's age (s since it was made: its QC .vr_sgib), -1 for anything else.
[[nodiscard]] float smallGibAge(edict_t* ent)
{
    const int f = fields().vr_sgib;
    const float made = f >= 0 ? fieldFloat(ent, f) : 0.f;
    return made > 0.f ? static_cast<float>(qcvm->time - made) : -1.f;
}
} // namespace

// Whether entity `num` is a small gib just torn out (vr_smallgibs_blow_grace): the hands' and weapons' reach bodies and
// held things pass through it (reachSkips, shouldCollide), as the QC's blows, nudges and batting do
// (VR_SmallGib_BlowSpared): the blade that tore it out went on through it and batted it away.
[[nodiscard]] bool smallGibSpared(int num)
{
    if(num <= svs.maxclients || num >= qcvm->num_edicts)
    {
        return false;
    }
    const float age = smallGibAge(EDICT_NUM(num));
    return age >= 0.f && age < vr_smallgibs_blow_grace.value;
}

// vr_smallgibs_trace (tests; ROUND21.md, "Small gibs batted by the blade"): after each step, the bodies touching each small
// gib in its first second that aren't the world's or other props': a hand's or a weapon's reach body, a fist, a held
// thing, a monster, a player.
void traceGibContacts()
{
    if(!vr_smallgibs_trace.value)
    {
        return;
    }
    const int idField = fields().vr_sgib_id;
    za::Array<b3ContactData, 16> contacts;
    for(int num = svs.maxclients + 1; num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts; num++)
    {
        const Slot& s = world->slots[num];
        if(s.kind != Kind::Prop || B3_IS_NULL(s.body))
        {
            continue;
        }
        edict_t* ent = EDICT_NUM(num);
        const float age = smallGibAge(ent);
        if(age < 0.f || age > 1.f)
        {
            continue;
        }
        const int count = b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size()));
        for(int i = 0; i < count; i++)
        {
            const b3ContactData& c = contacts[i];
            const bool isA = B3_ID_EQUALS(b3Shape_GetBody(c.shapeIdA), s.body);
            const b3ShapeId other = isA ? c.shapeIdB : c.shapeIdA;
            bool touching = false;
            for(int m = 0; m < c.manifoldCount; m++)
            {
                touching = touching || c.manifolds[m].pointCount > 0;
            }
            const uint64_t cat = b3Shape_GetFilter(other).categoryBits;
            const char* what = (cat & catReach) ? "reach body" : (cat & catHand) ? "fist" : (cat & catHeld) ? "held thing"
                             : (cat & catActor) ? "monster" : (cat & catPlayer) ? "player" : nullptr;
            if(!touching || !what)
            {
                continue;
            }
            Con_Printf("sgibtrace: #%.0f %.3f s: Box3D contact with %s %d (%s), %.0f u/s\n",
                idField >= 0 ? fieldFloat(ent, idField) : 0.f, age, what, numOf(other),
                PR_GetString(EDICT_NUM(za::max(0, numOf(other)))->v.classname),
                glm::length(glmv(b3Body_GetLinearVelocity(s.body))) * world->m2u);
        }
    }
}

namespace
{
[[nodiscard]] bool boxesApart(const b3AABB& a, const b3AABB& b)
{
    return a.upperBound.x < b.lowerBound.x || b.upperBound.x < a.lowerBound.x || a.upperBound.y < b.lowerBound.y ||
           b.upperBound.y < a.lowerBound.y || a.upperBound.z < b.lowerBound.z || b.upperBound.z < a.lowerBound.z;
}

// A small gib's body, just made (at the end of its grace, vr_smallgibs_grace: a rigid body from then), inside a
// monster's or a player's body, or a hand's or held weapon's reach body (torn out where the blade or the chainsaw's
// chain struck): it passes through each until clear of it, as a reach body passes through what was inside it as it was
// made (ignoreInside). Box3D pushed it out at 2 to 3 m/s, many times what Melee and Chainsaw Speed gave it (NOTES.md
// vrfiringrange_2026-10-02_19-11-19 and 19-18-12; ROUND21.md, "Small gibs pushed out of the body they came from").
// A body its shape is sunk in (deeper than 1 cm, not one it is only touching: a gib made just outside a monster
// meets it, Pass Through the Body 0); a reach body its box overlaps, a little grown (a blade a step away still passes).
// A monster's drop (a weapon, a backpack) made inside the player standing on it, or the monster, likewise
// (vr_prop_drop_pass_inside): pushed out, it flew into him at 280 u/s. `corpsesOnly`: any other prop just made (dropped
// through a corpse to the floor under it, let go of in one) passes through the corpses it is in (vr_corpse_collide).
void noteBornInside(int num, const Slot& s, bool corpsesOnly = false)
{
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.categoryBits = catProp;
    filter.maskBits = corpsesOnly ? catCorpse : catActor | catPlayer | catCorpse;
    struct Context
    {
        int num;
        double born;
    } context{num, s.born};
    constexpr float sink = 0.01f; // m (reachSink's)
    overlapShrunk(s.body, sink, filter,
        [](b3ShapeId shape, void* raw) {
            const auto& c = *static_cast<const Context*>(raw);
            const int other = numOf(shape);
            auto& in = world->inside;
            if(other > 0 && other != c.num &&
                !za::anyOf(in.begin(), in.end(), [&](const World::Inside& i) { return i.prop == c.num && i.other == other; }))
            {
                in.pushBack({c.num, other, c.born});
            }
            return true;
        },
        &context);
    constexpr float margin = 0.05f; // m
    if(corpsesOnly)
    {
        if(vr_debug_box3d.value &&
            za::anyOf(world->inside.begin(), world->inside.end(), [num](const World::Inside& i) { return i.prop == num; }))
        {
            Con_Printf("box3d: %d %s made inside a corpse: passes through until clear\n", num, PR_GetString(EDICT_NUM(num)->v.classname));
        }
        return;
    }
    b3AABB box = b3Body_ComputeAABB(s.body);
    box.lowerBound = b3v(glmv(box.lowerBound) - glm::vec3{margin});
    box.upperBound = b3v(glmv(box.upperBound) + glm::vec3{margin});
    int reaches = 0;
    for(size_t i = 1; i < world->hands.size() && i <= static_cast<size_t>(svs.maxclients); i++)
    {
        for(World::HandBody& hb : world->hands[i])
        {
            if(!B3_IS_NULL(hb.reach) && !boxesApart(box, b3Body_ComputeAABB(hb.reach)) &&
                za::find(hb.ignore.begin(), hb.ignore.end(), num) == hb.ignore.end())
            {
                hb.ignore.pushBack(num);
                reaches++;
            }
        }
    }
    if(vr_debug_box3d.value)
    {
        int bodies = 0;
        for(const World::Inside& i : world->inside)
        {
            bodies += i.prop == num;
        }
        if(bodies || reaches)
        {
            Con_Printf("box3d: %s %d made inside %d bodies and %d hands' or weapons': passes through until clear\n",
                isSmallGib(EDICT_NUM(num)) ? "small gib" : "monster drop", num, bodies, reaches);
        }
    }
}

// The small gibs made inside a body that are clear of it now (their boxes apart), or gone, or held: they meet it again.
void pruneInside()
{
    za::vectorEraseIf(world->inside, [](const World::Inside& i) {
        const int n = static_cast<int>(world->slots.size());
        if(i.prop >= n || i.other >= n || i.prop >= qcvm->num_edicts || i.other >= qcvm->num_edicts)
        {
            return true;
        }
        const Slot& p = world->slots[static_cast<size_t>(i.prop)];
        const Slot& o = world->slots[static_cast<size_t>(i.other)];
        if(p.kind != Kind::Prop || p.born != i.born || B3_IS_NULL(p.body) ||
            (o.kind != Kind::Actor && o.kind != Kind::Player && o.kind != Kind::Corpse) || B3_IS_NULL(o.body))
        {
            return true;
        }
        return boxesApart(b3Body_ComputeAABB(p.body), b3Body_ComputeAABB(o.body));
    });
}
} // namespace

// Whether entities `a` and `b` don't meet: a small gib and the body it was made inside (noteBornInside).
[[nodiscard]] bool bornInside(int a, int b)
{
    return za::anyOf(world->inside.begin(), world->inside.end(),
        [a, b](const World::Inside& i) { return (i.prop == a && i.other == b) || (i.prop == b && i.other == a); });
}

// Once a frame, before the hands' bodies follow the hands (syncReach: `hb.held` is still what each hand carried last
// frame): the throws of the props made this frame (createBody), and the graces over. A throw: a prop let go of by a
// hand (thrown or dropped), or one whose .owner is a player (a thrown weapon: CreateThrownWeapon; a box thrown hard:
// VR_Carry_Release), from the hand nearest it.
void noteThrows()
{
    const float grace = za::max(vr_box3d_throw_grace.value, 0.f);
    const bool debug = vr_debug_box3d.value != 0.f;
    pruneInside();
    za::vectorEraseIf(world->graces, [debug](const World::Grace& g) {
        if(g.ragdoll)
        {
            const RagdollBodies* rp = ragdollOf(g.num);
            return !rp || rp->born != g.born || qcvm->time >= g.until;
        }
        const bool same = g.num < static_cast<int>(world->slots.size()) && world->slots[g.num].kind == Kind::Prop &&
                          world->slots[g.num].born == g.born;
        if(same && qcvm->time < g.until)
        {
            return false;
        }
        if(debug && same)
        {
            // Its velocity and spin now against as it left (gravity taken out): what the hands' bodies changed.
            const Slot& s = world->slots[g.num];
            const float t = static_cast<float>(qcvm->time - g.born);
            const glm::vec3 fall{0.f, 0.f, -world->gravity / world->m2u * s.gravityScale * t};
            const glm::vec3 v = glmv(b3Body_GetLinearVelocity(s.body)), w = glmv(b3Body_GetAngularVelocity(s.body));
            Con_Printf("box3d: throw %s for %d %s after %.2f s: velocity %.2f m/s, changed by %.2f (gravity out); spin "
                       "%.2f rad/s, changed by %.2f; %d hand contacts passed through\n",
                g.skips ? "grace over" : "watched", g.num, PR_GetString(EDICT_NUM(g.num)->v.classname), t,
                glm::length(v), glm::length(v - fall - g.velocity), glm::length(w), glm::length(w - g.spin), g.skipped.n.loadRelaxed());
        }
        return true;
    });
    const FieldOffsets& f = fields();
    for(const int num : world->made)
    {
        if(num <= svs.maxclients || num >= qcvm->num_edicts || num >= static_cast<int>(world->slots.size()))
        {
            continue;
        }
        const Slot& s = world->slots[num];
        if(s.kind != Kind::Prop || s.born != qcvm->time || B3_IS_NULL(s.body))
        {
            continue;
        }
        edict_t* ent = EDICT_NUM(num);
        // A monster's drop just made (as it died: not a body made again later, after a hold) inside a body (the player
        // standing on the monster, the monster): Box3D flung it out of him at 280 u/s and it hurt him
        // (vr_prop_drop_pass_inside; ROUND21.md, "Monster drops flung into you").
        const float dropAge = monsterDropAge(ent);
        if((vr_smallgibs_pass_inside.value && isSmallGib(ent)) ||
            (vr_prop_drop_pass_inside.value && dropAge >= 0.f && dropAge < 0.1f))
        {
            noteBornInside(num, s);
        }
        else if(corpseMode() != 0)
        {
            noteBornInside(num, s, true);
        }
        const b3AABB box = b3Body_ComputeAABB(s.body);
        const float reach = za::min(0.5f * glm::length(glmv(box.upperBound) - glmv(box.lowerBound)), 0.5f); // m
        int thrower = 0;
        float speed = 0.f;
        for(int i = 1; i <= svs.maxclients && i < static_cast<int>(world->hands.size()); i++)
        {
            for(int h = 0; h < 2; h++)
            {
                if(world->hands[static_cast<size_t>(i)][static_cast<size_t>(h)].held == num)
                {
                    thrower = i;
                    speed = za::max(speed, throwSpeed(EDICT_NUM(i), h, reach));
                }
            }
        }
        const int owner = NUM_FOR_EDICT(PROG_TO_EDICT(ent->v.owner));
        if(thrower == 0 && owner >= 1 && owner <= svs.maxclients && f.handpos >= 0 && f.offhandpos >= 0)
        {
            edict_t* player = EDICT_NUM(owner);
            const glm::vec3 at = vec(ent->v.origin);
            const int h = glm::distance(at, fieldVec(player, f.handpos)) <= glm::distance(at, fieldVec(player, f.offhandpos));
            thrower = owner;
            speed = throwSpeed(player, h, reach);
        }
        if(thrower == 0)
        {
            continue;
        }
        world->slots[num].flight = true; // (its spin aligns in the air: spinAlign)
        if(grace <= 0.f && !debug)
        {
            continue;
        }
        const bool fast = speed >= vr_box3d_throw_grace_speed.value;
        if(debug)
        {
            Con_Printf("box3d: %d %s thrown by %d at %.2f m/s (reach %.2f m): %s\n", num, PR_GetString(ent->v.classname),
                thrower, speed, reach,
                !fast ? "a slow release, it meets the hands" : grace > 0.f ? "passes through the hands" : "watched");
        }
        if(fast || debug)
        {
            World::Grace g;
            g.num = num;
            g.player = thrower;
            g.born = s.born;
            g.skips = fast && grace > 0.f;
            g.until = qcvm->time + (g.skips ? grace : 0.2);
            g.velocity = glmv(b3Body_GetLinearVelocity(s.body));
            g.spin = glmv(b3Body_GetAngularVelocity(s.body));
            world->graces.pushBack(g);
        }
    }
    world->made.clear();
}

namespace
{
// The players' hands as kinematic spheres at their fists (vr_box3d_hand_push): a hand pushes a solid prop (an
// explosive box) as it presses on it, where it presses (a tall box pushed high tips over, a stack pushed low slides),
// continuously while it is in contact, Box3D's friction and contacts doing the rest. Only against solid props (ammo boxes
// and the like are nudged by QC's touches, as before). A hand is not armed while it carries something, nor again until
// its sphere is clear of every solid prop (a box let go of isn't shoved away by the hand inside it).
// With vr_box3d_hand_push_fist, the listen server's own player's hands are their drawn fists instead (held::fist: the
// grab's spheres), turned with the hand: a box is pushed where the fist meets it, and the fist that pushes it touches it
// to take it (the sphere at the hand's point met a box up to 7 cm before the drawn fist did).
// The same fist within a twentieth of a unit (the view makes it again each frame, through the hand's placement).
[[nodiscard]] bool sameFist(const za::Vector<glm::vec4>& a, const za::Vector<glm::vec4>& b)
{
    if(a.size() != b.size())
    {
        return false;
    }
    for(za::SizeT k = 0; k < a.size(); k++)
    {
        const glm::vec4 d = glm::abs(a[k] - b[k]);
        if(za::max(za::max(d.x, d.y), za::max(d.z, d.w)) > 0.05f)
        {
            return false;
        }
    }
    return true;
}

const za::Vector<glm::vec4> noFist; // (syncHands: a hand that pushes no fist's spheres)
} // namespace

void syncHands(float dt)
{
    QVR_PROFILE("box3d hands");
    const FieldOffsets& f = fields();
    world->hands.resize(static_cast<size_t>(svs.maxclients) + 1);
    const float radius = 0.045f; // m: a fist's
    for(int i = 1; i <= svs.maxclients && i < qcvm->num_edicts; i++)
    {
        edict_t* player = EDICT_NUM(i);
        const bool live = vr_box3d_hand_push.value && !player->free && svs.clients[i - 1].active && player->v.health > 0.f &&
                          f.handpos >= 0 && fieldFloatOr(player, f.ishuman, 0.f) != 0.f;
        for(int h = 0; h < 2; h++)
        {
            World::HandBody& hb = world->hands[static_cast<size_t>(i)][static_cast<size_t>(h)];
            const int heldOfs = h ? f.mainhand_held : f.offhand_held;
            const bool holding = heldOfs >= 0 && fieldInt(player, heldOfs) > 0;
            // The fist's middle: a radius behind the hand's point (the front of the fist), along its angles.
            const glm::vec3 tracked = fieldVec(player, h ? f.handpos : f.offhandpos);
            const glm::vec3 point = tracked == glm::vec3{0.f} ? tracked : tracked + hb.press; // (notePress)
            const glm::vec3 angles = fieldVec(player, h ? f.handrot : f.offhandrot);
            vec3_t in{angles.x, angles.y, angles.z}, fwd, right, up;
            AngleVectors(in, fwd, right, up);
            const za::Vector<glm::vec4>& fist = i == 1 && vr_box3d_hand_push_fist.value ? held::fist(h) : noFist;
            const bool byFist = !fist.empty();
            const glm::vec3 at = byFist ? point : point - vec(fwd) * (radius * world->m2u);
            const b3Quat turn = byFist ? toB3(glm::quat_cast(held::axesFromAngles(&angles[0], true))) : b3Quat_identity;
            if(B3_IS_NON_NULL(hb.body) && !sameFist(hb.fist, fist)) // the fist changed (its scale, the hand model): made again
            {
                b3DestroyBody(hb.body);
                hb.body = b3_nullBodyId;
                hb.armed = false;
            }
            if(!live || point == glm::vec3{0.f})
            {
                if(B3_IS_NON_NULL(hb.body))
                {
                    b3DestroyBody(hb.body);
                    hb = World::HandBody{};
                }
                continue;
            }
            if(B3_IS_NULL(hb.body))
            {
                b3BodyDef def = b3DefaultBodyDef();
                def.type = b3_kinematicBody;
                def.position = world->toM(at);
                def.userData = userOf(i);
                def.rotation = turn;
                hb.body = b3CreateBody(world->id, &def);
                b3ShapeDef shape = shapeDef(i, catHand, catSolid);
                shape.enablePreSolveEvents = true; // (preSolve: not what its player just threw)
                hb.fist = fist;
                if(byFist)
                {
                    for(const glm::vec4& s : fist)
                    {
                        const b3Sphere sphere{world->toM(glm::vec3{s}), s.w / world->m2u};
                        b3CreateSphereShape(hb.body, &shape, &sphere);
                    }
                }
                else
                {
                    const b3Sphere sphere{b3Vec3_zero, radius};
                    b3CreateSphereShape(hb.body, &shape, &sphere);
                }
                b3Body_Disable(hb.body);
                hb.armed = false;
                hb.at = at;
            }
            // Armed only when empty and clear of solid props (it would shove what it is in), and not on a prop's ledge
            // (vr_climb.cpp, "Holds on props": the hand on the hold would shove what it holds).
            bool want = !holding && climb::heldProp(player, h) == 0;
            if(want && !hb.armed)
            {
                const b3Vec3 zero = b3Vec3_zero;
                b3QueryFilter filter = b3DefaultQueryFilter();
                filter.categoryBits = catHand;
                filter.maskBits = catSolid;
                bool overlaps = false;
                const auto test = [&](const glm::vec3& centre, float r) {
                    const b3ShapeProxy proxy{&zero, 1, r};
                    b3World_OverlapShape(world->id, world->toM(centre), &proxy, filter,
                        [](b3ShapeId, void* context) {
                            *static_cast<bool*>(context) = true;
                            return false;
                        },
                        &overlaps);
                };
                if(byFist)
                {
                    const glm::mat3 axes = held::axesFromAngles(&angles[0], true);
                    for(za::SizeT k = 0; k < fist.size() && !overlaps; k++)
                    {
                        test(point + axes * glm::vec3{fist[k]}, fist[k].w / world->m2u);
                    }
                }
                else
                {
                    test(at, radius);
                }
                want = !overlaps;
            }
            if(want != hb.armed)
            {
                hb.armed = want;
                if(want)
                {
                    b3Body_Enable(hb.body);
                    b3Body_SetTransform(hb.body, world->toM(at), turn);
                    b3Body_SetLinearVelocity(hb.body, b3Vec3_zero);
                    hb.at = at;
                }
                else
                {
                    b3Body_Disable(hb.body);
                }
            }
            if(hb.armed)
            {
                // As a player's body: it shoves at most at vr_box3d_hand_push_speed, jumping the rest of its move (a
                // weapon taken moves the hand at once, a punch goes faster than a heavy box should fly), and what it
                // then overlaps is eased out of it.
                const float distance = glm::distance(at, hb.at);
                const float most = za::max(vr_box3d_hand_push_speed.value, 0.1f) * world->m2u * dt;
                if(distance > 0.5f * world->m2u) // a jump
                {
                    b3Body_SetTransform(hb.body, world->toM(at), turn);
                    b3Body_SetLinearVelocity(hb.body, b3Vec3_zero);
                }
                else
                {
                    if(distance > most)
                    {
                        b3Body_SetTransform(hb.body, world->toM(at - (at - hb.at) * (most / distance)), turn);
                    }
                    b3Body_SetTargetTransform(hb.body, b3WorldTransform{world->toM(at), turn}, dt, true);
                }
            }
            hb.at = at;
        }
    }
}

namespace
{
// ---------------------------------------------------------------------------------------------------------------------
// The reach bodies (ROUND21.md, "Hands and weapons as bodies"): each hand's own body besides the sphere above, following
// the hand at full speed as a carried prop does. Empty (vr_box3d_hand_props), the open hand (a box: the palm and the
// fingers) or the fist; holding a weapon (vr_box3d_weapon_push), the weapon's drawn hull (the local player's, as the
// view draws it: view::drawnWeapon; another player's, a capsule from the hand to the muzzle). They push the loose props
// (not solid ones: the sphere and the guns' nudge push those, as before) and hold them up: a thing let go of on an open
// palm stays there, one balances on a gun. Kinematic, not dynamic: the drawn hand is where the player's real hand is,
// whatever it meets (a body jointed to it would lag behind it, stop where it presses, and push back on nothing the
// player feels), and it bats a grenade away as hard as it swings.

// The empty hand's boxes (the drawn hand at the defaults, vr_dumpview's open hand and fist): cm from the hand's point
// in the main hand's frame (forward, left, up; the off hand's mirrored). The open hand's top is its palm (its middle is
// 0.8 cm right of the point): what rests there lies on the palm, not in the relaxed fingers' curl. It reaches 2 cm past
// the hand's upper edge (the index finger's side, -0.3 cm): a thing let go of from the fist is held at the hand's point,
// at that edge, and would roll off the palm turned up (the thumb and index hold it there).
struct HandBox
{
    glm::vec3 lo, hi;
};
constexpr HandBox openHandBox{{-15.5f, -4.4f, -12.f}, {5.5f, -0.8f, 2.f}};
constexpr HandBox fistBox{{-16.f, -4.3f, -12.1f}, {-0.9f, 3.2f, -3.1f}};
constexpr float reachSink = 0.01f;     // m: sunk no deeper in a new reach body, a prop rests on it; deeper, passes through
constexpr float reachCapsule = 0.015f; // m: another player's weapon's radius
} // namespace

// The hand whose reach body `body` is (of client `player`), or nullptr.
[[nodiscard]] World::HandBody* reachOf(b3BodyId body, int player)
{
    if(player < 1 || player >= static_cast<int>(world->hands.size()))
    {
        return nullptr;
    }
    for(const World::PortalCopy& copy : world->portalCopies)
    {
        if(B3_ID_EQUALS(copy.copy, body)) { body = copy.original; break; }
    }
    for(World::HandBody& hb : world->hands[static_cast<size_t>(player)])
    {
        if(B3_IS_NON_NULL(hb.reach) && B3_ID_EQUALS(hb.reach, body))
        {
            return &hb;
        }
    }
    return nullptr;
}

namespace
{
// The weapon `player`'s hand `h` holds: its own mass (kg, Weapon Weights: Mass; 0: none, or not a weapon).
[[nodiscard]] float heldWeaponMass(edict_t* player, int h)
{
    const FieldOffsets& f = fields();
    const char* name = h ? PR_GetString(player->v.weaponmodel)
                         : (f.weaponmodel2 >= 0 ? PR_GetString(fieldInt(player, f.weaponmodel2)) : "");
    const int slot = name && *name ? weapons::slotForName(name) : -1;
    return slot >= 0 ? za::max(weapons::value(slot, weapons::Key::Mass), 0.f) : 0.f;
}

// What hand `h` of client `i` is now (`last`: what it was), and for a drawn weapon its entity in the hand's frame.
[[nodiscard]] World::ReachKey reachKey(edict_t* player, int i, int h, const World::ReachKey& last, const glm::vec3& point,
    const glm::vec3& angles, glm::mat4& inHand)
{
    using Key = World::ReachKey;
    const FieldOffsets& f = fields();
    Key key;
    const int heldOfs = h ? f.mainhand_held : f.offhand_held;
    if(heldOfs >= 0 && fieldInt(player, heldOfs) > 0)
    {
        return key; // (what it carries is a body of its own: Kind::Held)
    }
    if(climb::heldProp(player, h) != 0)
    {
        return key; // (on a prop's ledge: none, the hand would shove what it holds; vr_climb.cpp, "Holds on props")
    }
    const float weapon = h ? player->v.weapon : fieldFloatOr(player, f.weapon2, 0.f);
    if(weapon == 0.f) // QC's WID_FIST
    {
        if(!vr_box3d_hand_props.value)
        {
            return key;
        }
        const int bits = static_cast<int>(fieldFloatOr(player, f.vrbits0, 0.f));
        key.what = (bits & (h ? 8 : 2)) ? Key::Fist : Key::Open; // (QC's QVR_VRBITS0_*HAND_GRABBING)
        key.scale = weapons::offsetScale();
        return key;
    }
    if(!vr_box3d_weapon_push.value)
    {
        return key;
    }
    const int modelOfs = f.weaponmodel2;
    const char* name = h ? PR_GetString(player->v.weaponmodel)
                         : (modelOfs >= 0 ? PR_GetString(fieldInt(player, modelOfs)) : "");
    if(i == 1 && cls.state == ca_connected) // the local player: the weapon as drawn
    {
        const view::DrawnWeapon& d = view::drawnWeapon(h);
        if(!d.model || vr_gametime - d.when > 0.5 || strcmp(d.model->name, name) != 0)
        {
            return key; // (not drawn yet)
        }
        key.what = Key::Weapon;
        key.model = d.model;
        key.mirrored = d.mirrored;
        key.mag = vr_reload_mag_collide.value ? d.mag : nullptr;
        key.generation = shapeGeneration;
        inHand = d.inHand;
        return key;
    }
    const int muzzleOfs = h ? f.muzzlepos : f.offmuzzlepos;
    const glm::vec3 muzzle = muzzleOfs >= 0 ? fieldVec(player, muzzleOfs) : point;
    if(glm::distance(muzzle, point) < 1.f)
    {
        return key;
    }
    key.what = Key::Capsule;
    key.muzzle = glm::transpose(held::axesFromAngles(&angles[0], true)) * (muzzle - point);
    if(last.what == Key::Capsule && glm::distance(last.muzzle, key.muzzle) < 2.f)
    {
        key.muzzle = last.muzzle; // (the same capsule: it follows the gun's muzzle within 2 units)
    }
    return key;
}

// What the new reach body of `hb` has sunk in it deeper than reachSink: passed through until clear of it (a thing thrown
// from the hand, a weapon let go of, a gun taken where props lie). What is only touching it rests on it, and so does
// what the empty hand let go of this frame without throwing it (`letGo`: slower than 1 m/s; not a weapon): it is eased
// out of the hand, onto the palm turned up.
void ignoreInside(World::HandBody& hb, uint64_t category, int letGo)
{
    hb.ignore.clear();
    za::Array<b3ShapeId, maxBodyShapes> shapes;
    const int count = b3Body_GetShapes(hb.reach, shapes.data(), static_cast<int>(shapes.size()));
    const b3WorldTransform xf = b3Body_GetTransform(hb.reach);
    za::Array<b3Vec3, B3_MAX_SHAPE_CAST_POINTS> points;
    for(int i = 0; i < count; i++)
    {
        int n = 0;
        float radius = 0.f;
        if(b3Shape_GetType(shapes[i]) == b3_hullShape)
        {
            n = shrunkCorners(shapes[i], xf, reachSink, points);
        }
        else if(b3Shape_GetType(shapes[i]) == b3_capsuleShape)
        {
            const b3Capsule c = b3Shape_GetCapsule(shapes[i]);
            points[0] = b3RotateVector(xf.q, c.center1);
            points[1] = b3RotateVector(xf.q, c.center2);
            n = 2;
            radius = za::max(0.f, c.radius - reachSink);
        }
        if(n == 0)
        {
            continue;
        }
        const b3ShapeProxy proxy{points.data(), n, radius};
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = category;
        filter.maskBits = catProp;
        struct Context
        {
            za::Vector<int>& ignore;
            int placed; // (let go of onto the hand)
        } context{hb.ignore, 0};
        if(letGo > 0 && letGo < static_cast<int>(world->slots.size()) && world->slots[letGo].kind == Kind::Prop &&
            !isWeaponLike(EDICT_NUM(letGo)) && b3Length(b3Body_GetLinearVelocity(world->slots[letGo].body)) < 1.f)
        {
            context.placed = letGo;
        }
        b3World_OverlapShape(world->id, xf.p, &proxy, filter,
            [](b3ShapeId shape, void* raw) {
                auto& c = *static_cast<Context*>(raw);
                const int num = numOf(shape);
                if(num > 0 && num != c.placed && num < static_cast<int>(world->slots.size()) &&
                    world->slots[num].kind == Kind::Prop &&
                    za::find(c.ignore.begin(), c.ignore.end(), num) == c.ignore.end())
                {
                    c.ignore.pushBack(num);
                }
                return true;
            },
            &context);
    }
    if(vr_debug_box3d.value && !hb.ignore.empty())
    {
        Con_Printf("box3d: a reach body made with %d props sunk in it: it passes through them until clear\n",
            static_cast<int>(hb.ignore.size()));
    }
}

// The reach body of `hb` made again for `key` at `pos`, `rot` (none for None); `letGo`: what the hand let go of.
void makeReach(
    World::HandBody& hb, int i, int h, const World::ReachKey& key, const glm::vec3& pos, const glm::quat& rot, int letGo)
{
    using Key = World::ReachKey;
    if(B3_IS_NON_NULL(hb.reach))
    {
        b3DestroyBody(hb.reach);
        hb.reach = b3_nullBodyId;
    }
    hb.ignore.clear();
    hb.outline.clear();
    if(key.what == Key::None)
    {
        return;
    }
    b3BodyDef def = b3DefaultBodyDef();
    def.type = b3_kinematicBody;
    def.position = world->toM(pos);
    def.rotation = toB3(rot);
    def.userData = userOf(i);
    hb.reach = b3CreateBody(world->id, &def);
    const bool weapon = key.what == Key::Weapon || key.what == Key::Capsule;
    const uint64_t category = weapon ? catReachWeapon : catReachHand;
    b3ShapeDef shape = shapeDef(i, category, catProp | catCorpse);
    shape.enableCustomFiltering = true; // (shouldCollide: reachMeets)
    shape.enablePreSolveEvents = true;  // (preSolve)
    switch(key.what)
    {
    case Key::Open:
    case Key::Fist:
    {
        const HandBox& b = key.what == Key::Open ? openHandBox : fistBox;
        glm::vec3 lo = b.lo, hi = b.hi;
        if(h == 0) // the off hand: mirrored
        {
            lo.y = -b.hi.y;
            hi.y = -b.lo.y;
        }
        const float cm = 0.01f * key.scale; // metres
        const glm::vec3 half = (hi - lo) * 0.5f * cm, centre = (lo + hi) * 0.5f * cm;
        const b3BoxHull box = b3MakeOffsetBoxHull(half.x, half.y, half.z, b3v(centre));
        b3CreateHullShape(hb.reach, &shape, &box.base);
        break;
    }
    case Key::Weapon:
    {
        za::Vector<glm::vec3>& vertices = scratch.propVerts;
        // Its convex pieces (gunPieces), else its one hull.
        // (The off hand's mirrored from the main hand's.)
        const PropHullKey pieceKey{key.model, -2, {}};
        const za::Vector<b3HullData*>* pieces = &gunPieces(
            pieceKey, [&](za::Vector<glm::vec3>& out) { return held::modelTriangles(key.model, false, out); });
        if(key.mirrored && !pieces->empty())
        {
            pieces = &mirroredPieces(pieceKey, *pieces);
        }
        // Its seated magazine (vr_reload_mag_collide): a hull of its own, with the gun's.
        const b3HullData* mag = nullptr;
        if(key.mag)
        {
            za::Vector<glm::vec3>& magVertices = scratch.magVerts;
            if(held::magazineVertices(key.model, key.mag, key.mirrored, magVertices))
            {
                mag = magazineHull(key.mag, key.mirrored ? -5 : -4, magVertices);
            }
        }
        za::Vector<b3Vec3> all;
        if(mag)
        {
            b3CreateHullShape(hb.reach, &shape, mag);
            all.emplaceBackRange(b3GetHullPoints(mag), static_cast<size_t>(mag->vertexCount));
        }
        if(!pieces->empty())
        {
            for(const b3HullData* piece : *pieces)
            {
                b3CreateHullShape(hb.reach, &shape, piece);
                all.emplaceBackRange(b3GetHullPoints(piece), static_cast<size_t>(piece->vertexCount));
            }
            if(vr_debug_box3d.value)
            {
                Con_Printf("box3d: %s hand's weapon %s: %d convex pieces%s\n", h ? "main" : "off", key.model->name,
                    static_cast<int>(pieces->size()), mag ? " and its magazine" : "");
            }
        }
        else if(held::modelVertices(key.model, key.mirrored, vertices) && vertices.size() >= 4)
        {
            za::Vector<b3Vec3> points;
            points.reserve(vertices.size());
            for(const glm::vec3& v : vertices)
            {
                points.pushBack(world->toM(v));
            }
            if(b3HullData* hull = fittedHull(points, 32, world->m2u))
            {
                b3CreateHullShape(hb.reach, &shape, hull); // (the world keeps its own copy)
                if(mag)
                {
                    all.emplaceBackRange(b3GetHullPoints(hull), static_cast<size_t>(hull->vertexCount));
                }
                if(vr_debug_box3d.value)
                {
                    Con_Printf("box3d: %s hand's weapon %s: a hull of %d vertices%s\n", h ? "main" : "off", key.model->name,
                        hull->vertexCount, mag ? " and its magazine" : "");
                }
                b3DestroyHull(hull);
            }
        }
        // (Swept as one: the pieces' and the magazine's hull, sweepReach.)
        if(!pieces->empty() || mag)
        {
            if(b3HullData* outline = b3CreateHull(all.data(), static_cast<int>(all.size()), 64))
            {
                hb.outline.emplaceBackRange(b3GetHullPoints(outline), static_cast<size_t>(outline->vertexCount));
                b3DestroyHull(outline);
            }
        }
        break;
    }
    case Key::Capsule:
    {
        const b3Capsule capsule{b3Vec3_zero, world->toM(key.muzzle), reachCapsule};
        b3CreateCapsuleShape(hb.reach, &shape, &capsule);
        break;
    }
    default: break;
    }
    if(vr_debug_box3d.value)
    {
        static constexpr const char* names[] = {"none", "open hand", "fist", "weapon", "capsule"};
        const glm::vec3 palm = glm::mat3_cast(rot) * glm::vec3{0.f, h ? 1.f : -1.f, 0.f};
        const b3AABB box = b3Body_ComputeAABB(hb.reach);
        const glm::vec3 lo = world->toU(box.lowerBound), hi = world->toU(box.upperBound);
        Con_Printf("box3d: %s hand's reach body: %s at %.1f %.1f %.1f (%.1f %.1f %.1f to %.1f %.1f %.1f), the palm facing "
                   "%.2f %.2f %.2f\n",
            h ? "main" : "off", names[key.what], pos.x, pos.y, pos.z, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z, palm.x, palm.y,
            palm.z);
    }
    ignoreInside(hb, category, weapon ? 0 : letGo);
}
} // namespace

// Whether the reach body of `hb` (of client `player`) passes through prop `num` now: what it passes through until clear
// (sunk in it as it was made: its ignore list), what its player just threw (graced), a small gib just torn out
// (smallGibSpared), an empty hand a grenade (reachMeets), a prop flying to a hand (a force grab's pull: .fg_state 1),
// the player's own grenade in its first quarter second (leaving the launcher's muzzle, inside the gun).
[[nodiscard]] bool reachSkips(const World::HandBody& hb, int player, int num)
{
    if(za::find(hb.ignore.begin(), hb.ignore.end(), num) != hb.ignore.end() || graced(player, num) || smallGibSpared(num))
    {
        return true;
    }
    if(num <= svs.maxclients || num >= qcvm->num_edicts || num >= static_cast<int>(world->slots.size()))
    {
        return false;
    }
    const Slot& s = world->slots[num];
    if(s.kind != Kind::Prop)
    {
        return false;
    }
    const bool grenade = isGrenade(s.model);
    if(grenade && hb.key.what != World::ReachKey::Weapon && hb.key.what != World::ReachKey::Capsule)
    {
        return true;
    }
    edict_t* e = EDICT_NUM(num);
    if(const int fg = fields().fg_state; fg >= 0 && fieldFloat(e, fg) == 1.f)
    {
        return true;
    }
    // A loose round at the loading port of this hand's gun, lying the way it goes in (QC vr_reload.qc
    // VR_Reload_LooseFrame: .vr_ammo_passer the player, .vr_ammo_pass the hands, 1 off, 2 main): it passes through the
    // gun's hull, which covers the port (the receiver's opening, the well), to reach it.
    if(const FieldOffsets& f = fields(); f.vr_ammo_pass >= 0 && f.vr_ammo_passer >= 0 &&
        (hb.key.what == World::ReachKey::Weapon || hb.key.what == World::ReachKey::Capsule))
    {
        const int hands = static_cast<int>(fieldFloat(e, f.vr_ammo_pass));
        const bool main = player < static_cast<int>(world->hands.size()) && &hb == &world->hands[player][1];
        if((hands & (main ? 2 : 1)) && fieldInt(e, f.vr_ammo_passer) == EDICT_TO_PROG(EDICT_NUM(player)))
        {
            return true;
        }
    }
    return grenade && e->v.owner == EDICT_TO_PROG(EDICT_NUM(player)) && qcvm->time - s.born < 0.25;
}

namespace
{
struct ReachPose
{
    glm::vec3 pos; // units
    glm::quat rot;
};

// Pushes by mass (ROUND21.md, "Pushes by mass"): the hands, their weapons and what they carry are kinematic bodies (of
// no give), which knock whatever they meet as if it weighed nothing: a flick of the wrist sent a 40 kg box across the
// room. Each is given a mass instead, what an arm puts behind it: an empty hand's vr_box3d_hand_mass, a held weapon's
// own (Weapon Weights: Mass) and vr_box3d_weapon_arm_mass, a carried prop's own and each hand's holding it. A prop of mass `m` struck by `pusher` kg keeps
// pusher / (pusher + m) of the velocity the kinematic body gave it (the share of two masses meeting: a 0.4 kg grenade
// most of it, a 40 kg box a tenth). 1: no limit (the setting 0).
[[nodiscard]] float pushShare(float pusher, float m)
{
    return pusher > 0.f && m > 0.f ? pusher / (pusher + m) : 1.f;
}
} // namespace

// How much of the velocity `gain` (m/s) a hand's body of `pusher` kg gives the prop `s` of `m` kg in this step it keeps:
// the first push a hit (pushShare), the steps after (pushed in the step before, or struck by a swing this frame: the body
// kinematic goes on at the hand's speed, which a real arm would not) a shove: no more than the arm's force
// (vr_box3d_push_force) gives its mass in the step, so that a heavy prop moves as far as it is pushed and stops, and a
// light one is carried along. Notes the push (Slot::pushedStep).
[[nodiscard]] float pushKeep(Slot& s, float pusher, float m, float gain, float dt)
{
    const bool shove = s.pushedStep >= world->steps - 1;
    s.pushedStep = world->steps;
    float keep = pushShare(pusher, m);
    if(shove && pusher > 0.f && m > 0.f && gain > 0.f && vr_box3d_push_force.value > 0.f)
    {
        keep = za::min(keep, vr_box3d_push_force.value * dt / (m * gain));
    }
    return keep;
}

// A hand's body's mass: an empty hand's vr_box3d_hand_mass; a weapon's its own (Weapon Weights: Mass) and the arm's
// behind it (vr_box3d_weapon_arm_mass), so the heavy ones (the rocket launcher, the hammer) bat harder than the light.
[[nodiscard]] float reachMass(const World::HandBody& hb)
{
    const bool weapon = hb.key.what == World::ReachKey::Weapon || hb.key.what == World::ReachKey::Capsule;
    return weapon ? za::max(vr_box3d_weapon_arm_mass.value, 0.f) + hb.weaponMass : za::max(vr_box3d_hand_mass.value, 0.f);
}

namespace
{
// A fast move of the reach body of `hb` over a step `dt` (a swing): Box3D collides once a step, at its start, so a
// kinematic body moving further in a step than a thin thing is across jumps over it (an axe swung at 10 m/s moves 11 cm
// a frame; a grenade is a few cm across). Its shape is swept from `from` to `to` in pieces moving no point of it more than
// 1.5 cm (Box3D's contacts reach 2 cm ahead); each prop it meets there that it doesn't touch at `from` is struck as the
// step would have: where the body's surface first meets it, its velocity along the surface's normal (the prop's centre
// from its nearest point) made the body's point's there and more by the prop's restitution, as off anything a
// kinematic body (of no give) hits. It then leaves ahead of the body, which goes on through where it was.
void sweepReach(const World::HandBody& hb, int player, const ReachPose& from, const ReachPose& to, float dt)
{
    constexpr float piece = 0.015f; // m
    za::Array<b3ShapeId, maxBodyShapes> shapes;
    const int shapeCount = b3Body_GetShapes(hb.reach, shapes.data(), static_cast<int>(shapes.size()));
    if(shapeCount < 1)
    {
        return;
    }
    // The shape's points (its frame, m) and radius (a weapon in convex pieces: their hull's).
    za::Array<b3Vec3, 64> local;
    int count = 0;
    float radius = 0.f;
    if(!hb.outline.empty())
    {
        count = za::min(static_cast<int>(hb.outline.size()), static_cast<int>(local.size()));
        za::copy(hb.outline.data(), hb.outline.data() + count, local.begin());
    }
    else if(b3Shape_GetType(shapes[0]) == b3_hullShape)
    {
        const b3HullData* hull = b3Shape_GetHull(shapes[0]);
        count = za::min(hull->vertexCount, static_cast<int>(local.size()));
        za::copy(b3GetHullPoints(hull), b3GetHullPoints(hull) + count, local.begin());
    }
    else if(b3Shape_GetType(shapes[0]) == b3_capsuleShape)
    {
        const b3Capsule c = b3Shape_GetCapsule(shapes[0]);
        local[0] = c.center1;
        local[1] = c.center2;
        count = 2;
        radius = c.radius;
    }
    if(count == 0)
    {
        return;
    }
    const glm::vec3 p0 = glmv(world->toM(from.pos)), p1 = glmv(world->toM(to.pos));
    float most = 0.f;
    for(int i = 0; i < count; i++)
    {
        const glm::vec3 v = glmv(local[static_cast<size_t>(i)]);
        most = za::max(most, glm::distance(p0 + from.rot * v, p1 + to.rot * v));
    }
    const int pieces = za::min(static_cast<int>(za::ceil(most / piece)), 24);
    if(pieces <= 1 || dt <= 0.f)
    {
        return;
    }
    const auto poseAt = [&](int k) {
        const float t = static_cast<float>(k) / static_cast<float>(pieces);
        return ReachPose{glm::mix(from.pos, to.pos, t), glm::slerp(from.rot, to.rot, t)};
    };
    // What it meets over the points of two poses (their convex hull: the swept piece), relative to the second's place.
    za::Vector<int> found;
    const uint64_t category = b3Shape_GetFilter(shapes[0]).categoryBits;
    const auto meets = [&](const ReachPose& a, const ReachPose& b) {
        za::Array<b3Vec3, 128> points;
        const glm::vec3 origin = glmv(world->toM(b.pos)), shift = glmv(world->toM(a.pos)) - origin;
        for(int i = 0; i < count; i++)
        {
            const glm::vec3 v = glmv(local[static_cast<size_t>(i)]);
            points[static_cast<size_t>(i)] = b3v(b.rot * v);
            points[static_cast<size_t>(count + i)] = b3v(shift + a.rot * v);
        }
        const b3ShapeProxy proxy{points.data(), 2 * count, radius};
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = category;
        filter.maskBits = catProp;
        found.clear();
        b3World_OverlapShape(world->id, b3v(origin), &proxy, filter,
            [](b3ShapeId shape, void* context) {
                auto& out = *static_cast<za::Vector<int>*>(context);
                const int num = numOf(shape);
                if(za::find(out.begin(), out.end(), num) == out.end())
                {
                    out.pushBack(num);
                }
                return true;
            },
            &found);
    };
    meets(from, from);
    za::Vector<int> struck = found; // (touching it at the start: Box3D's contact)

    // The body's motion over the step: its origin's velocity, and its spin (m/s, rad/s).
    const glm::vec3 linear = (p1 - p0) / dt;
    glm::quat turn = to.rot * glm::inverse(from.rot);
    if(turn.w < 0.f)
    {
        turn = -turn;
    }
    const float angle = 2.f * za::acos(za::min(turn.w, 1.f));
    const float sine = za::sqrt(za::max(0.f, 1.f - turn.w * turn.w));
    const glm::vec3 spin = sine > 1e-6f ? glm::vec3{turn.x, turn.y, turn.z} / sine * (angle / dt) : glm::vec3{0.f};
    const glm::quat rot0 = from.rot;
    for(int k = 1; k <= pieces; k++)
    {
        const ReachPose a = poseAt(k - 1);
        meets(a, poseAt(k));
        for(const int num : found)
        {
            if(num <= svs.maxclients || num >= static_cast<int>(world->slots.size()) ||
                world->slots[num].kind != Kind::Prop ||
                za::find(struck.begin(), struck.end(), num) != struck.end() || reachSkips(hb, player, num))
            {
                continue;
            }
            struck.pushBack(num);
            const b3BodyId prop = world->slots[num].body;
            za::Array<b3ShapeId, 1> propShape;
            if(b3Body_GetShapes(prop, propShape.data(), 1) < 1)
            {
                continue;
            }
            // Where the body's surface is nearest the prop's centre with the body at `a` (asked of the shape where it is
            // now, `from`: the centre taken into its frame there and back).
            const glm::vec3 pa = glmv(world->toM(a.pos));
            const glm::vec3 centre = glmv(b3Body_GetWorldCenter(prop));
            const glm::vec3 inBody = glm::inverse(a.rot) * (centre - pa);
            const glm::vec3 target0 = p0 + rot0 * inBody;
            glm::vec3 near0 = glmv(b3Shape_GetClosestPoint(shapes[0], b3v(target0)));
            for(int i = 1; i < shapeCount; i++) // (the nearest of its pieces)
            {
                const glm::vec3 q = glmv(b3Shape_GetClosestPoint(shapes[i], b3v(target0)));
                if(glm::distance(q, target0) < glm::distance(near0, target0))
                {
                    near0 = q;
                }
            }
            const glm::quat back = a.rot * glm::inverse(rot0);
            const glm::vec3 at = pa + back * (near0 - p0);
            const glm::vec3 point = linear + glm::cross(spin, at - pa);
            const glm::vec3 was = glmv(b3Body_GetLinearVelocity(prop));
            glm::vec3 normal = back * (target0 - near0);
            if(glm::length(normal) < 1e-4f) // (its centre inside the body: along the body's motion there)
            {
                normal = point - was;
            }
            if(glm::length(normal) < 1e-6f)
            {
                continue;
            }
            normal = glm::normalize(normal);
            const float closing = glm::dot(point - was, normal);
            if(closing <= 0.f)
            {
                continue;
            }
            const float restitution = b3Shape_GetRestitution(propShape[0]);
            const float gain = closing * (1.f + restitution);
            const float share = pushKeep(world->slots[num], reachMass(hb), b3Body_GetMass(prop), gain, dt); // (by mass)
            const glm::vec3 now = was + normal * (gain * share);
            b3Body_SetLinearVelocity(prop, b3v(now));
            b3Body_SetAwake(prop, true);
            if(vr_smallgibs_trace.value && smallGibAge(EDICT_NUM(num)) >= 0.f && smallGibAge(EDICT_NUM(num)) <= 1.f)
            {
                const int idField = fields().vr_sgib_id;
                Con_Printf("sgibtrace: #%.0f %.3f s: struck by a swing's sweep (player %d), %.0f u/s after\n",
                    idField >= 0 ? fieldFloat(EDICT_NUM(num), idField) : 0.f, smallGibAge(EDICT_NUM(num)), player,
                    glm::length(now) * world->m2u);
            }
            if(vr_debug_box3d.value)
            {
                Con_Printf("box3d: a reach body's swing (%.1f cm and %.1f degrees this frame, piece %d of %d) strikes %d %s "
                           "at %.0f u/s (x%.2f by mass): %.0f u/s after (%.0f %.0f %.0f)\n",
                    most * 100.f, glm::degrees(angle), k, pieces, num, PR_GetString(EDICT_NUM(num)->v.classname),
                    closing * world->m2u, share, glm::length(now) * world->m2u, now.x * world->m2u, now.y * world->m2u,
                    now.z * world->m2u);
            }
        }
    }
}
} // namespace

void syncReach(float dt)
{
    QVR_PROFILE("box3d reach");
    using Key = World::ReachKey;
    const FieldOffsets& f = fields();
    world->hands.resize(static_cast<size_t>(svs.maxclients) + 1);
    for(int i = 1; i <= svs.maxclients && i < qcvm->num_edicts; i++)
    {
        edict_t* player = EDICT_NUM(i);
        const bool live = !player->free && svs.clients[i - 1].active && player->v.health > 0.f && f.handpos >= 0 &&
                          fieldFloatOr(player, f.ishuman, 0.f) != 0.f;
        for(int h = 0; h < 2; h++)
        {
            World::HandBody& hb = world->hands[static_cast<size_t>(i)][static_cast<size_t>(h)];
            const glm::vec3 tracked = live ? fieldVec(player, h ? f.handpos : f.offhandpos) : glm::vec3{0.f};
            if(!live)
            {
                hb.press = glm::vec3{0.f};
                hb.pressFull = false;
            }
            const glm::vec3 point = tracked == glm::vec3{0.f} ? tracked : tracked + hb.press; // (notePress)
            const glm::vec3 angles = live ? fieldVec(player, h ? f.handrot : f.offhandrot) : glm::vec3{0.f};
            glm::mat4 inHand{1.f};
            const Key key = live && point != glm::vec3{0.f} ? reachKey(player, i, h, hb.key, point, angles, inHand) : Key{};
            hb.weaponMass = key.what == Key::Weapon || key.what == Key::Capsule ? heldWeaponMass(player, h) : 0.f;

            // Its frame: the hand's (forward, left, up), or the drawn weapon's entity.
            glm::mat4 frame{held::axesFromAngles(&angles[0], true)};
            frame[3] = glm::vec4{point, 1.f};
            if(key.what == Key::Weapon)
            {
                frame = frame * inHand;
            }
            const glm::vec3 pos{frame[3]};
            const glm::quat rot = glm::normalize(glm::quat_cast(glm::mat3{frame}));
            // What it carries (to know what it lets go of).
            const int heldOfs = h ? f.mainhand_held : f.offhand_held;
            const int held = live && heldOfs >= 0 && fieldInt(player, heldOfs) > 0
                                 ? NUM_FOR_EDICT(PROG_TO_EDICT(fieldInt(player, heldOfs)))
                                 : 0;
            const int letGo = held == 0 ? hb.held : 0;
            hb.held = held;
            if(!(key == hb.key))
            {
                makeReach(hb, i, h, key, pos, rot, letGo);
                hb.key = key;
                continue;
            }
            if(B3_IS_NULL(hb.reach))
            {
                continue;
            }

            // What it passes through, until clear of it.
            if(!hb.ignore.empty())
            {
                const b3AABB mine = b3Body_ComputeAABB(hb.reach);
                za::vectorEraseIf(hb.ignore, [&](int num) {
                    if(num >= static_cast<int>(world->slots.size()) || world->slots[num].kind != Kind::Prop)
                    {
                        return true;
                    }
                    const b3AABB theirs = b3Body_ComputeAABB(world->slots[num].body);
                    return mine.upperBound.x < theirs.lowerBound.x || theirs.upperBound.x < mine.lowerBound.x ||
                           mine.upperBound.y < theirs.lowerBound.y || theirs.upperBound.y < mine.lowerBound.y ||
                           mine.upperBound.z < theirs.lowerBound.z || theirs.upperBound.z < mine.lowerBound.z;
                });
            }

            // As a carried prop's body: the velocity that gets it where the hand is by the end of the step, or there at
            // once for a jump (a teleport).
            const b3WorldTransform now = b3Body_GetTransform(hb.reach);
            const glm::vec3 at = world->toU(now.p);
            const bool moved = glm::distance(at, pos) > 0.01f || za::fabs(glm::dot(fromB3(now.q), rot)) < 0.999999f;
            if(!moved && !b3Body_IsAwake(hb.reach))
            {
                continue;
            }
            if(glm::distance(at, pos) > 0.5f * world->m2u)
            {
                b3Body_SetTransform(hb.reach, world->toM(pos), toB3(rot));
                b3Body_SetLinearVelocity(hb.reach, b3Vec3_zero);
                b3Body_SetAngularVelocity(hb.reach, b3Vec3_zero);
            }
            else
            {
                b3Body_SetTargetTransform(hb.reach, b3WorldTransform{world->toM(pos), toB3(rot)}, dt, true);
                if(vr_debug_box3d.value >= 2.f)
                {
                    Con_Printf("box3d: %s reach body at %.1f %.1f %.1f, the hand's at %.1f %.1f %.1f: %.0f u/s\n",
                        h ? "main" : "off", at.x, at.y, at.z, pos.x, pos.y, pos.z,
                        glm::length(world->toU(b3Body_GetLinearVelocity(hb.reach))));
                }
                // A swing that would jump over what it meets strikes it (sweepReach).
                sweepReach(hb, i, ReachPose{at, fromB3(now.q)}, ReachPose{pos, rot}, dt);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Pressing on props (vr_box3d_press_back; ROUND22.md, "Pressing on props: the hand gives"): the hands' bodies are
// kinematic (of no give), so a hand, a gun or a plank pressed lightly down on a crate lying on the floor pushed it into
// the floor, Box3D's contacts squeezing it between the two until it sank through, shot out sideways or broke. A prop
// with nowhere to go along the push (braced: it rests against the level, a door or lift, a fixture, or a prop braced so,
// the other way: noteBraces, before each step) is not pushed by a hand's body there (preSolve: pressHit) and holds the
// hand back instead: after the frame's steps, how deep each of the hand's bodies would be in it (the contact's
// separation, moved on by the step's motion) is taken out of the hand, which the next frame's bodies (and the drawn
// hand: VR_SetupViewEntities) are moved back by. Clear of it, the hand goes back to where it is tracked (gently: a drawn
// hand jumping is a swing). Held back vr_box3d_press_max at most; pressed deeper, its bodies go into the prop by the
// rest, still without pushing it. Nor do QC's pushes (physicspush: the hands' nudges, blows) go into what holds a prop
// (unbraced).
namespace
{
constexpr float pressSlop = 0.004f;    // m: no deeper than this in a prop is resting on it
constexpr float pressHoldGap = 0.01f;  // m: within this of a braced prop's surface the hand stays held back
constexpr float pressReturn = 0.06f;   // s (time constant): let back towards the tracked hand once clear
constexpr float pressReturnMost = 0.6f; // m/s: and no faster
constexpr float braceAgainst = -0.75f; // a support's normal into the prop against the push at least this much (cos):
                                       // pushed down and forward at 45 degrees, a crate still slides
constexpr float braceGap = 0.01f;      // m: a support this near holds it
constexpr int braceDepth = 3;          // props in a stack braced through

za::Vector<b3ContactData> braceContacts; // (supportsOf: every contact, not the first few: a drawn fist touches in dozens)

void addSupport(za::Array<glm::vec3, 4>& out, int& count, const glm::vec3& into)
{
    for(int i = 0; i < count; i++)
    {
        if(glm::dot(out[static_cast<za::SizeT>(i)], into) > 0.95f)
        {
            return;
        }
    }
    if(count < static_cast<int>(out.size()))
    {
        out[static_cast<za::SizeT>(count++)] = into;
    }
}

// The normals (into `prop`) of what holds it that won't give: the level, a door or lift, a fixture, a kinematic body;
// through `depth` props more, one resting on a prop held so the same way.
void supportsOf(b3BodyId prop, za::Array<glm::vec3, 4>& out, int& count, int depth)
{
    za::Vector<b3ContactData>& contacts = braceContacts;
    contacts.resize(static_cast<za::SizeT>(za::max(b3Body_GetContactCapacity(prop), 1)));
    const int n = b3Body_GetContactData(prop, contacts.data(), static_cast<int>(contacts.size()));
    struct Under
    {
        b3BodyId body;
        glm::vec3 into;
    };
    za::Array<Under, 8> under; // (asked after the loop: the contacts' buffer is theirs then)
    int unders = 0;
    for(int c = 0; c < n; c++)
    {
        const b3ContactData& cd = contacts[static_cast<za::SizeT>(c)];
        const bool propA = B3_ID_EQUALS(b3Shape_GetBody(cd.shapeIdA), prop);
        const b3ShapeId other = propA ? cd.shapeIdB : cd.shapeIdA;
        const uint64_t cat = b3Shape_GetFilter(other).categoryBits;
        if((cat & (catWorld | catMover | catFixture | catProp)) == 0 || (cat & (catHand | catReach | catHeld)) != 0)
        {
            continue;
        }
        const b3BodyId body = b3Shape_GetBody(other);
        for(int i = 0; i < cd.manifoldCount; i++)
        {
            const b3Manifold& mf = cd.manifolds[i];
            float nearest = 1e30f;
            for(int k = 0; k < mf.pointCount; k++)
            {
                nearest = za::min(nearest, mf.points[k].separation);
            }
            if(mf.pointCount == 0 || nearest > braceGap)
            {
                continue;
            }
            const glm::vec3 into = propA ? -glmv(mf.normal) : glmv(mf.normal); // (Box3D's points from A to B)
            if((cat & catProp) == 0 || b3Body_GetType(body) != b3_dynamicBody)
            {
                addSupport(out, count, into);
            }
            else if(depth > 0 && unders < static_cast<int>(under.size()))
            {
                under[static_cast<za::SizeT>(unders++)] = Under{body, into};
            }
        }
    }
    for(int u = 0; u < unders; u++)
    {
        za::Array<glm::vec3, 4> sub{};
        int subs = 0;
        supportsOf(under[static_cast<za::SizeT>(u)].body, sub, subs, depth - 1);
        for(int k = 0; k < subs; k++)
        {
            if(glm::dot(sub[static_cast<za::SizeT>(k)], under[static_cast<za::SizeT>(u)].into) > 0.5f)
            {
                addSupport(out, count, under[static_cast<za::SizeT>(u)].into);
                break;
            }
        }
    }
}

// The bodies of hand `hb`: its fist's (armed), its reach body, what it carries.
void handBodies(const World::HandBody& hb, b3BodyId (&out)[3])
{
    const Slot* held = hb.held > 0 && hb.held < static_cast<int>(world->slots.size()) ? &world->slots[hb.held] : nullptr;
    out[0] = hb.armed ? hb.body : b3_nullBodyId;
    out[1] = hb.reach;
    out[2] = held && held->kind == Kind::Held ? held->body : b3_nullBodyId;
}
} // namespace

void noteBraces(float dt)
{
    world->pressHitCount.storeRelaxed(0);
    if(!vr_box3d_press_back.value)
    {
        return;
    }
    for(const auto& player : world->hands)
    {
        for(const World::HandBody& hb : player)
        {
            b3BodyId bodies[3];
            handBodies(hb, bodies);
            for(const b3BodyId body : bodies)
            {
                if(B3_IS_NULL(body) || !b3Body_IsValid(body) || !b3Body_IsEnabled(body))
                {
                    continue;
                }
                // The props near it: its box grown by how far it goes this step (and 5 cm).
                b3AABB box = b3Body_ComputeAABB(body);
                const float reach = 0.05f + glm::length(glmv(b3Body_GetLinearVelocity(body))) * dt;
                box.lowerBound = b3v(glmv(box.lowerBound) - glm::vec3{reach});
                box.upperBound = b3v(glmv(box.upperBound) + glm::vec3{reach});
                b3QueryFilter filter = b3DefaultQueryFilter();
                filter.maskBits = catProp;
                b3World_OverlapAABB(world->id, box, filter,
                    [](b3ShapeId shape, void*) {
                        const int num = numOf(shape);
                        if(num <= svs.maxclients || num >= static_cast<int>(world->slots.size()))
                        {
                            return true;
                        }
                        Slot& s = world->slots[static_cast<za::SizeT>(num)];
                        if(s.kind != Kind::Prop || s.braceStep == world->steps || !b3Body_IsValid(s.body))
                        {
                            return true;
                        }
                        s.braceStep = world->steps;
                        s.braceCount = 0;
                        supportsOf(s.body, s.braces, s.braceCount, braceDepth);
                        return true;
                    },
                    nullptr);
            }
        }
    }
}

glm::vec3 unbraced(b3BodyId body, const glm::vec3& velocity)
{
    if(!vr_box3d_press_back.value || !b3Body_IsValid(body) || b3Body_GetType(body) != b3_dynamicBody)
    {
        return velocity;
    }
    za::Array<glm::vec3, 4> into{};
    int count = 0;
    supportsOf(body, into, count, braceDepth);
    glm::vec3 v = velocity;
    for(int i = 0; i < count; i++)
    {
        const float c = glm::dot(v, into[static_cast<za::SizeT>(i)]);
        if(c < 0.f)
        {
            v -= into[static_cast<za::SizeT>(i)] * c;
        }
    }
    return v;
}

bool pressHit(b3ShapeId hand, b3ShapeId prop, const glm::vec3& n, float separation)
{
    if(!vr_box3d_press_back.value)
    {
        return false;
    }
    const int num = numOf(prop);
    if(num <= svs.maxclients || num >= static_cast<int>(world->slots.size()))
    {
        return false;
    }
    const Slot& s = world->slots[static_cast<za::SizeT>(num)];
    if(s.kind != Kind::Prop || s.braceStep != world->steps)
    {
        return false;
    }
    // Whose: a carried prop's hand, or the client's hand whose fist or reach body it is.
    const b3BodyId body = b3Shape_GetBody(hand);
    int client = -1, which = -1;
    if(b3Shape_GetFilter(hand).categoryBits & catHeld)
    {
        const int carried = numOf(hand);
        for(za::SizeT i = 1; i < world->hands.size() && which < 0; i++)
        {
            for(int h = 0; h < 2; h++)
            {
                if(world->hands[i][static_cast<za::SizeT>(h)].held == carried)
                {
                    client = static_cast<int>(i);
                    which = h;
                }
            }
        }
    }
    else if(const int i = numOf(hand); i >= 1 && i < static_cast<int>(world->hands.size()))
    {
        for(int h = 0; h < 2; h++)
        {
            const World::HandBody& hb = world->hands[static_cast<za::SizeT>(i)][static_cast<za::SizeT>(h)];
            if(B3_ID_EQUALS(hb.body, body) || B3_ID_EQUALS(hb.reach, body))
            {
                client = i;
                which = h;
            }
        }
    }
    if(which < 0)
    {
        return false;
    }
    // Braced against the push; or any way, once that hand is held back all it may be (pressed deep into it, its
    // bodies meet it on every side).
    const bool full = world->hands[static_cast<za::SizeT>(client)][static_cast<za::SizeT>(which)].pressFull;
    bool braced = full && s.braceCount > 0;
    for(int i = 0; i < s.braceCount && !braced; i++)
    {
        braced = glm::dot(s.braces[static_cast<za::SizeT>(i)], n) < braceAgainst;
    }
    if(!braced)
    {
        return false;
    }
    const int at = world->pressHitCount.fetchAddRelaxed(1);
    if(at < static_cast<int>(world->pressHits.size()))
    {
        world->pressHits[static_cast<za::SizeT>(at)] = World::PressHit{client, which, num, n, separation, body};
    }
    return true;
}

void notePress(float dt)
{
    const bool on = vr_box3d_press_back.value != 0.f;
    const float most = za::max(vr_box3d_press_max.value, 1.f) * world->m2u / 100.f;
    const int hits = za::min(world->pressHitCount.loadRelaxed(), static_cast<int>(world->pressHits.size()));
    for(za::SizeT i = 1; i < world->hands.size(); i++)
    {
        for(int h = 0; h < 2; h++)
        {
            World::HandBody& hb = world->hands[i][static_cast<za::SizeT>(h)];
            if(!on)
            {
                hb.press = glm::vec3{0.f};
                hb.pressFull = false;
                continue;
            }
            // The planes it must be out of (each hit's, moved on by the step's motion), and the least move out of them
            // all (Gauss-Seidel).
            struct Plane
            {
                glm::vec3 n; // from the hand to the prop
                float depth; // m
            };
            za::Array<Plane, 32> planes;
            int count = 0;
            bool touching = false;
            int deepest = 0;
            float depthMost = -1e30f;
            for(int k = 0; k < hits; k++)
            {
                const World::PressHit& hit = world->pressHits[static_cast<za::SizeT>(k)];
                if(hit.client != static_cast<int>(i) || hit.hand != h || !b3Body_IsValid(hit.body))
                {
                    continue;
                }
                const Slot& s = world->slots[static_cast<za::SizeT>(hit.prop)];
                const glm::vec3 vProp = b3Body_IsValid(s.body) ? glmv(b3Body_GetLinearVelocity(s.body)) : glm::vec3{0.f};
                const float sep = hit.sep - glm::dot(glmv(b3Body_GetLinearVelocity(hit.body)) - vProp, hit.n) * dt;
                if(sep > pressHoldGap)
                {
                    continue;
                }
                touching = true;
                const float depth = -sep - pressSlop;
                if(depth > depthMost)
                {
                    depthMost = depth;
                    deepest = hit.prop;
                }
                if(depth > 0.f && count < static_cast<int>(planes.size()))
                {
                    planes[static_cast<za::SizeT>(count++)] = Plane{hit.n, depth};
                }
            }
            glm::vec3 out{0.f};
            for(int sweep = 0; sweep < 8 && count > 0; sweep++)
            {
                float worst = 0.f;
                for(int k = 0; k < count; k++)
                {
                    const Plane& pl = planes[static_cast<za::SizeT>(k)];
                    const float short_ = pl.depth + glm::dot(out, pl.n);
                    if(short_ > 0.f)
                    {
                        out -= pl.n * short_;
                        worst = za::max(worst, short_);
                    }
                }
                if(worst < 1e-4f)
                {
                    break;
                }
            }
            const float length = glm::length(hb.press);
            if(touching)
            {
                // Pressed deeper than vr_box3d_press_max: held back no further; its bodies go into the prop by the rest
                // (still no push, whichever way they meet it: pressHit), drawn so, until they are clear of it.
                if(!hb.pressFull)
                {
                    hb.press += out * world->m2u;
                }
                if(glm::length(hb.press) >= most)
                {
                    hb.press *= most / glm::length(hb.press);
                    hb.pressFull = true;
                }
            }
            else if(length > 0.f)
            {
                // Back to the tracked hand, eased and no faster than pressReturnMost (a hand drawn jumping back is a
                // swing: a blow where it lands).
                hb.pressFull = false;
                const float back = za::min(length * (1.f - za::exp(-dt / pressReturn)), pressReturnMost * world->m2u * dt);
                hb.press = length - back < 0.01f ? glm::vec3{0.f} : hb.press * ((length - back) / length);
            }
            if(vr_debug_box3d.value >= 2.f && (touching || hb.press != glm::vec3{0.f}))
            {
                Con_Printf("box3d: hand pressed %s: held back %.2f units (%.2f %.2f %.2f) out of %d\n",
                    touching ? "on a braced prop" : "(clear)", glm::length(hb.press), hb.press.x, hb.press.y, hb.press.z,
                    deepest);
            }
        }
    }
}

glm::vec3 heldPress(int num)
{
    if(!world || num <= 0)
    {
        return glm::vec3{0.f};
    }
    for(const auto& player : world->hands)
    {
        for(const World::HandBody& hb : player)
        {
            if(hb.held == num)
            {
                return hb.press;
            }
        }
    }
    return glm::vec3{0.f};
}
} // namespace box3d::detail
