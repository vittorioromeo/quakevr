// vr_box3d_flail.cpp -- AD support: the death sergeant's flail (QC vr_ad_melee.qc VR_ADMELEE_FLAIL; its handle
// progs/w_blueflail.mdl#melee, vr_admelee.cpp): held in a hand, its spiked head is a body of its own, hanging from the
// handle's end by its chain, swung by the hand (ROUND22.md, "The death sergeant's flail").
//
// For each hand holding it (its weapon model; a listen server's local player or another), each frame before the step: a
// kinematic body without shapes at the handle's end (the hand's muzzle: QC .muzzlepos, the view's drawn tip), following
// it as the hands' reach bodies follow theirs (its velocity the move over the frame: a swing whirls the head), and the
// head: a dynamic sphere of vr_flail_mass kg (the ball and its spikes' bases), continuous against the level and what
// moves (a bullet: it never passes through a wall however fast it whirls), held to the anchor by Box3D's distance joint
// as a rope no longer than vr_flail_chain m (slack: it swings, falls, wraps a little round what it meets). It meets the
// level, doors and lifts, monsters, props and corpses (a prop's category: they all meet it), not the player or his
// hands. A teleport (the anchor more than 1.5 m from where it was) takes the head along; a new chain length or mass
// makes it again; the hand letting go of the flail (or dying) takes it away.
//
// Its hits (Box3D's hit events, the head faster than 1 m/s into something), each hand's hardest of the frame on one
// thing, go to QC (VR_Flail_Hit: the hand's blow, its damage the head's mass and speed) after the step, the same thing
// again no sooner than hitCooldown s. The speed is the real one: bullet time slows the head with the world (its fall,
// its swing) with the hand: speeds are the hand's time's (handTime).
// The view draws the head and its chain (vr_flail.cpp) where the step left them (flailHead).

#include "vr_box3d_internal.hpp"

#include "vr_admelee.hpp"
#include "vr_timescale.hpp"

namespace box3d::detail
{
namespace
{
constexpr uint64_t catFlail = 8192; // its head's shape (as well as catProp: all that meets props meets it)
constexpr uint64_t flailMask = catWorld | catMover | catActor | catProp | catFixture | catCorpse;
constexpr int userBase = -1000;     // its head's shape's user number: userBase - (client * 2 + hand), no entity's
constexpr float headRadius = 4.4f;  // units: its ball and its spikes' bases (vr_admelee.cpp's head, drawn at its size)
constexpr float ringReach = qvr::admelee::flailRingReach; // units: from the ball's middle to the ring its chain holds
constexpr float hitCooldown = 0.2f; // s (game): the same thing struck again by the same head
constexpr float teleport = 1.5f;    // m: the anchor further than this from where it was: carried there
constexpr float handJump = 25.f;    // m/s (real): the handle's end moving faster than this jumped (carried, not swung)
constexpr float jumpLeast = 0.5f;   // m: and never a move shorter than this (the hand's place arrives in bursts in slow motion)
// The head never faster than vr_flail_max_speed m/s (real; the author's note vrfiringrange_2026-10-23_17-33-56: a
// heavy ball can't be whirled up without end), and slowed by the air as much as vr_flail_drag (Box3D's linear damping, a
// second's share lost; its spin's damping 1 + 4x it): whirling it takes the hand's work.
[[nodiscard]] float mostSpeed()
{
    return za::clamp(vr_flail_max_speed.value, 2.f, 40.f);
}

[[nodiscard]] float drag()
{
    return za::clamp(vr_flail_drag.value, 0.f, 10.f);
}
constexpr float ropeHertz = 120.f;  // its rope's stiffness (Box3D's joints' 60: a fast whirl stretched it 7 units)
constexpr float rattleSpeed = 6.f;  // m/s (real): the chain rattles (QC VR_Flail_Rattle) with the head this fast
constexpr float rattleEvery = 0.3f; // s (real): at most this often
constexpr float grace = 0.3f;      // s (game): a head just made (or carried) strikes nothing (pushed out of what it was in)
constexpr float takenFresh = 0.5f; // s (game): a flail taken from where it lay this long ago still gives the hand's head its head's place

struct Head
{
    b3BodyId anchor{b3_nullBodyId}, head{b3_nullBodyId};
    b3JointId rope{b3_nullJointId};
    float length{0.f}, mass{0.f}; // as made (m, kg)
    glm::vec3 pos{0.f}, vel{0.f}, anchorAt{0.f}; // units, after the last step (the view's)
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
    int lastTarget{-1};
    double lastTime{-1.0};
    double madeTime{0.0}; // (made or carried: grace; real time, as the cooldown's)
    double lastRattle{-1.0};
    glm::vec3 lastGood{0.f}; // units: its middle after the last frame, on the right side of the level's surfaces
};

// vr_flail_info: since the last one, the frames a head was held, the most its chain was stretched past its length
// (units), the frames the level stood between the handle's end and the head (a wall in the chain's way: wrapped round
// a corner, or through a wall), its fastest, its hits dealt.
struct Info
{
    int frames{0}, blocked{0}, hits{0}, made{0}, putBack{0}, taken{0}; // (taken: made where a head lying about was)
    float stretch{0.f}, fastest{0.f}, handle{0.f}; // (speeds real: bullet time's taken out)
};

struct Hit
{
    int client{0}, hand{0}, target{0};
    glm::vec3 at{0.f}, dir{0.f};
    float speed{0.f}; // m/s, real
};

struct FlailState
{
    b3WorldId world{}; // whose bodies the heads' are
    Head heads[MAX_SCOREBOARD + 1][2];
    za::Vector<Hit> hits;
    Info info;
};
FlailState flails;

[[nodiscard]] bool sameWorld(b3WorldId a, b3WorldId b)
{
    return a.index1 == b.index1 && a.generation == b.generation;
}

// Game seconds to a second of the hand's own time. Slow motion slows the hands with the world (they follow their
// controllers at the game's pace, timescale::filterHands): a swing is then the same swing in game time, and the head's
// game-time speed is the hand's (1). In Sandevistan the hands keep real time in the slowed world: a game second is the
// scale's share of one of theirs. The head lives in its hand's time: its fall (gravity), its rope's stiffness, its
// speed limits and its blows' speed (as melee's: the real motion counts the same in bullet time).
[[nodiscard]] float handTime()
{
    const float scale = za::clamp(qvr::timescale::current(), 0.05f, 1.f);
    const float hands = za::clamp(qvr::timescale::handScale(), 0.05f, 1.f);
    return za::clamp(scale / hands, 0.05f, 1.f);
}

void drop(Head& h)
{
    if(B3_IS_NON_NULL(h.rope) && b3Joint_IsValid(h.rope))
    {
        b3DestroyJoint(h.rope, true);
    }
    if(B3_IS_NON_NULL(h.head) && b3Body_IsValid(h.head))
    {
        b3DestroyBody(h.head);
    }
    if(B3_IS_NON_NULL(h.anchor) && b3Body_IsValid(h.anchor))
    {
        b3DestroyBody(h.anchor);
    }
    h = Head{};
}

// Whether `player`'s hand `h` holds the flail (its handle's model: the made one, AD's data there).
[[nodiscard]] bool holdsFlail(edict_t* player, int h)
{
    const FieldOffsets& f = fields();
    const char* name = h ? PR_GetString(player->v.weaponmodel)
                         : (f.weaponmodel2 >= 0 ? PR_GetString(fieldInt(player, f.weaponmodel2)) : "");
    return name && !strcmp(name, admelee::flailModel);
}

// The head's middle put at `want` from the anchor at `at`, short of the level between them by its radius.
[[nodiscard]] glm::vec3 clearOf(edict_t* player, const glm::vec3& at, const glm::vec3& want)
{
    vec3_t from, to, none{0.f, 0.f, 0.f};
    store(at, from);
    store(want, to);
    const trace_t tr = SV_Move(from, none, none, to, MOVE_NOMONSTERS, player);
    const glm::vec3 off = want - at;
    const float len = glm::length(off);
    if(len < 0.01f || tr.startsolid)
    {
        return at;
    }
    const float reach = za::min(len, za::max(0.f, tr.fraction * len - (tr.fraction < 1.f ? headRadius + 0.5f : 0.f)));
    return at + off * (reach / len);
}

void make(Head& h, edict_t* player, int client, int hand, const glm::vec3& at, const glm::vec3& out)
{
    const float mass = za::clamp(vr_flail_mass.value, 0.2f, 30.f);
    b3BodyDef def = b3DefaultBodyDef();
    def.type = b3_kinematicBody;
    def.position = world->toM(at);
    def.userData = userOf(userBase - (client * 2 + hand));
    h.anchor = b3CreateBody(world->id, &def);

    // The head just past the handle's end (out of it, along it), to fall from there: not in a wall the hand isn't in.
    // The flail just taken from where it lay with its head (QC VR_FlailHead_Taken: .vr_flail_taken, its head's place
    // and speed then): the head where that one was (no further than its chain), at its speed.
    const float length = za::clamp(vr_flail_chain.value, 0.1f, 2.f);
    glm::vec3 start = clearOf(player, at, at + out * (headRadius + ringReach));
    glm::vec3 startVel{0.f};
    const FieldOffsets& f = fields();
    const int takenOfs = hand ? f.vr_flail_taken : f.vr_flail_offtaken;
    const int posOfs = hand ? f.vr_flail_head : f.vr_flail_offhead;
    const int velOfs = hand ? f.vr_flail_headvel : f.vr_flail_offheadvel;
    bool taken = false;
    if(takenOfs >= 0 && posOfs >= 0 && velOfs >= 0 && fieldFloat(player, takenOfs) > 0.f)
    {
        const float since = static_cast<float>(qcvm->time) - fieldFloat(player, takenOfs);
        const glm::vec3 was = fieldVec(player, posOfs);
        if(since >= 0.f && since < takenFresh && was != glm::vec3{0.f})
        {
            glm::vec3 off = was - at;
            const float reach = length * world->m2u + ringReach;
            if(glm::length(off) > reach)
            {
                off *= reach / glm::length(off);
            }
            start = clearOf(player, at, at + off);
            startVel = fieldVec(player, velOfs);
            taken = true;
        }
        fieldFloat(player, takenOfs) = 0.f;
    }
    b3BodyDef hd = b3DefaultBodyDef();
    hd.type = b3_dynamicBody;
    hd.position = world->toM(start);
    hd.linearVelocity = b3v(startVel / world->m2u);
    hd.userData = def.userData;
    hd.isBullet = true;
    hd.linearDamping = drag();
    hd.angularDamping = 1.f + 4.f * drag();
    h.head = b3CreateBody(world->id, &hd);
    const float r = headRadius / world->m2u;
    b3ShapeDef shape = shapeDef(userBase - (client * 2 + hand), catProp | catFlail, flailMask);
    shape.density = mass / (4.f / 3.f * 3.14159265f * r * r * r);
    shape.baseMaterial.friction = 0.4f;
    shape.baseMaterial.restitution = 0.25f;
    shape.enableHitEvents = true;
    const b3Sphere sphere{b3Vec3_zero, r};
    b3CreateSphereShape(h.head, &shape, &sphere);

    b3DistanceJointDef jd = b3DefaultDistanceJointDef();
    jd.base.bodyIdA = h.anchor;
    jd.base.bodyIdB = h.head;
    jd.base.localFrameA = b3Transform{b3v(glm::vec3{0.f}), toB3(glm::quat{1.f, 0.f, 0.f, 0.f})};
    jd.base.localFrameB = jd.base.localFrameA;
    jd.base.collideConnected = false;
    jd.base.constraintHertz = ropeHertz;
    const float rope = length + ringReach / world->m2u; // (to the ball's middle)
    jd.length = rope;
    jd.enableSpring = true; // (a rope: no stiffness, held within its length by the limit; the fuel tanks')
    jd.hertz = 0.f;
    jd.enableLimit = true;
    jd.minLength = 0.f;
    jd.maxLength = rope;
    h.rope = b3CreateDistanceJoint(world->id, &jd);
    h.length = length;
    h.mass = mass;
    h.anchorAt = at;
    h.pos = start;
    h.vel = startVel;
    h.madeTime = realtime;
    h.lastGood = start;
    flails.info.made++;
    flails.info.taken += taken ? 1 : 0;
    if(vr_flail_debug.value)
    {
        Con_Printf("flail: client %d hand %d: head made (%.2f m chain, %.1f kg)%s at %.1f %.1f %.1f\n", client, hand, length,
            mass, taken ? ", the one lying about's" : "", start.x, start.y, start.z);
    }
}

// After the frame's steps: where each hand's head is, on the player for QC (VR_FlailHead_Attach: the flail let go of,
// its head lying about made there, at its speed); none: 0 (but for a head a hand is about to take: .vr_flail_taken).
void noteHeldHeads(edict_t* player, int client)
{
    const FieldOffsets& f = fields();
    for(int hand = 0; hand < 2; hand++)
    {
        const int posOfs = hand ? f.vr_flail_head : f.vr_flail_offhead;
        const int velOfs = hand ? f.vr_flail_headvel : f.vr_flail_offheadvel;
        const int takenOfs = hand ? f.vr_flail_taken : f.vr_flail_offtaken;
        if(posOfs < 0 || velOfs < 0 || takenOfs < 0)
        {
            return;
        }
        const Head& h = flails.heads[client][hand];
        if(B3_IS_NON_NULL(h.head) && b3Body_IsValid(h.head))
        {
            setFieldVec(player, posOfs, h.pos);
            setFieldVec(player, velOfs, h.vel);
            continue;
        }
        if(fieldFloat(player, takenOfs) > 0.f && static_cast<float>(qcvm->time) - fieldFloat(player, takenOfs) < takenFresh)
        {
            continue; // (a hand about to take its head: VR_FlailHead_Taken's place kept for make)
        }
        fieldFloat(player, takenOfs) = 0.f;
        setFieldVec(player, posOfs, glm::vec3{0.f});
        setFieldVec(player, velOfs, glm::vec3{0.f});
    }
}
} // namespace

// Once a frame before the step (after the hands' reach bodies): each hand holding the flail has its anchor and head.
void syncFlails(float dt)
{
    if(!sameWorld(flails.world, world->id))
    {
        for(auto& hs : flails.heads)
        {
            for(Head& h : hs)
            {
                h = Head{}; // (the old world's bodies went with it)
            }
        }
        flails.world = world->id;
    }
    const FieldOffsets& f = fields();
    for(int i = 1; i <= MAX_SCOREBOARD; i++)
    {
        edict_t* player = i <= svs.maxclients && i < qcvm->num_edicts ? EDICT_NUM(i) : nullptr;
        const bool live = player && !player->free && svs.clients[i - 1].active && player->v.health > 0.f &&
                          f.muzzlepos >= 0 && f.handpos >= 0;
        for(int h = 0; h < 2; h++)
        {
            Head& head = flails.heads[i][h];
            const glm::vec3 at = live ? fieldVec(player, h ? f.muzzlepos : f.offmuzzlepos) : glm::vec3{0.f};
            const glm::vec3 hand = live ? fieldVec(player, h ? f.handpos : f.offhandpos) : glm::vec3{0.f};
            if(!live || !holdsFlail(player, h) || at == glm::vec3{0.f} || hand == glm::vec3{0.f})
            {
                if(B3_IS_NON_NULL(head.anchor))
                {
                    drop(head);
                }
                continue;
            }
            const glm::vec3 out = glm::distance(at, hand) > 0.5f ? glm::normalize(at - hand) : glm::vec3{0.f, 0.f, -1.f};
            const bool valid = B3_IS_NON_NULL(head.anchor) && b3Body_IsValid(head.anchor) && b3Body_IsValid(head.head) &&
                               B3_IS_NON_NULL(head.rope) && b3Joint_IsValid(head.rope);
            if(!valid || za::fabs(head.length - za::clamp(vr_flail_chain.value, 0.1f, 2.f)) > 0.001f ||
                za::fabs(head.mass - za::clamp(vr_flail_mass.value, 0.2f, 30.f)) > 0.001f)
            {
                drop(head);
                make(head, player, i, h, at, out);
                continue;
            }
            const glm::vec3 now = world->toU(b3Body_GetPosition(head.anchor));
            // (A jump no hand makes, faster than handJump in real time: a stale muzzle as the flail is taken, a new
            // weapon's; carried too, not whipping the head at it.)
            const float realDt = dt / handTime();
            const float moved = glm::distance(now, at) / world->m2u;
            if(moved > teleport || moved > za::max(jumpLeast, handJump * realDt))
            {
                // (A teleport, a load: the head carried along, as it hung from the anchor.)
                // (Not into the level: short of what lies between the anchor and there, by its radius.)
                const glm::vec3 off = world->toU(b3Body_GetPosition(head.head)) - now;
                const glm::vec3 place = clearOf(player, at, at + off);
                b3Body_SetTransform(head.anchor, world->toM(at), toB3(glm::quat{1.f, 0.f, 0.f, 0.f}));
                b3Body_SetLinearVelocity(head.anchor, b3Vec3_zero);
                b3Body_SetTransform(head.head, world->toM(place), b3Body_GetRotation(head.head));
                b3Body_SetLinearVelocity(head.head, b3Vec3_zero);
                b3Body_SetAngularVelocity(head.head, b3Vec3_zero);
                head.madeTime = realtime;
                head.lastGood = place;
                continue;
            }
            b3Body_SetTargetTransform(
                head.anchor, b3WorldTransform{world->toM(at), toB3(glm::quat{1.f, 0.f, 0.f, 0.f})}, dt, true);
            // In its hand's time (handTime): Sandevistan's hand swings 1/scale as fast in game time, so the head falls
            // 1/scale^2 as hard and its rope is 1/scale as stiff: the same swing as at full speed, sped up as the hand
            // is. (Slowed hands: as at full speed.)
            const float scale = handTime();
            b3Body_SetGravityScale(head.head, 1.f / (scale * scale));
            b3Joint_SetConstraintTuning(head.rope, ropeHertz / scale, 2.f);
            b3Body_SetLinearDamping(head.head, drag() / scale); // (the air in its hand's time too)
            b3Body_SetAngularDamping(head.head, (1.f + 4.f * drag()) / scale);
            b3Body_SetAwake(head.head, true);
        }
    }
}

// After each step: the heads' hits, each hand's hardest (flailHits gathers them for the frame).
void noteFlailHits(const b3ContactEvents& events)
{
    const float timeScale = handTime();
    for(int i = 0; i < events.hitCount; i++)
    {
        const b3ContactHitEvent& e = events.hitEvents[i];
        if(!b3Shape_IsValid(e.shapeIdA) || !b3Shape_IsValid(e.shapeIdB))
        {
            continue;
        }
        const bool aHead = (b3Shape_GetFilter(e.shapeIdA).categoryBits & catFlail) != 0;
        const bool bHead = (b3Shape_GetFilter(e.shapeIdB).categoryBits & catFlail) != 0;
        if(aHead == bHead)
        {
            continue; // (none, or two heads)
        }
        const int key = userBase - numOf(aHead ? e.shapeIdA : e.shapeIdB);
        const int client = key / 2, hand = key % 2;
        const int target = numOf(aHead ? e.shapeIdB : e.shapeIdA);
        if(client < 1 || client > MAX_SCOREBOARD || target < 0 || target >= qcvm->num_edicts || (target >= 1 && target <= svs.maxclients))
        {
            continue;
        }
        const glm::vec3 n = glmv(e.normal); // (from A to B)
        Hit hit;
        hit.client = client;
        hit.hand = hand;
        hit.target = target;
        hit.at = world->toU(e.point);
        hit.dir = aHead ? n : -n;
        hit.speed = e.approachSpeed * timeScale;
        auto it = za::findIf(flails.hits.begin(), flails.hits.end(),
            [&](const Hit& x) { return x.client == client && x.hand == hand; });
        if(it == flails.hits.end())
        {
            flails.hits.pushBack(hit);
        }
        else if(hit.speed > it->speed)
        {
            *it = hit;
        }
    }
}

namespace
{
// After the frame's steps: head `head` (client `i`'s hand `h`) as the step left it, kept on its chain's length and
// out of the level, its speed capped; the chain's rattle; the info.
void stepped(Head& head, int i, int h)
{
    // Where the step left it, first: everything below (the chain's length, the level's surface, the view, QC's
    // .vr_flail_head) goes by it. (Lost once in the flailprop split: the head was then drawn, and pinned by the chain's
    // guard, where it was made, the body whirling at the speed cap round that spot: "it hovers", note
    // vrtesthall_2026-10-23_22-41-32.)
    head.pos = world->toU(b3Body_GetPosition(head.head));
    const float scale = handTime();
    const float most = mostSpeed();
    if(const glm::vec3 v = glmv(b3Body_GetLinearVelocity(head.head)); glm::length(v) * scale > most)
    {
        b3Body_SetLinearVelocity(head.head, b3v(v * (most / (glm::length(v) * scale))));
    }
    // The chain never longer than it is (the joint is soft: a fast whirl stretched it 7 units): the head put
    // back on its length, its speed outwards from the anchor taken away.
    {
        const glm::vec3 anchorNow = world->toU(b3Body_GetPosition(head.anchor));
        const float reach = head.length * world->m2u + ringReach;
        const glm::vec3 d = head.pos - anchorNow;
        const float len = glm::length(d);
        if(len > reach && len > 0.01f)
        {
            const glm::vec3 n = d / len;
            const glm::vec3 pulled = anchorNow + n * reach;
            // (Pulled as a rope pulls: the move given as speed too, position-based, so that the hand's pull
            // whirls the head however the joint's softness lagged it this frame: in bullet time it is soft to
            // a hand moving 1/scale as fast.)
            const float frame = za::max(static_cast<float>(host_frametime), 1e-4f);
            glm::vec3 v = glmv(b3Body_GetLinearVelocity(head.head)) + (pulled - head.pos) / (world->m2u * frame);
            head.pos = pulled;
            const float out = glm::dot(v - glmv(b3Body_GetLinearVelocity(head.anchor)), n);
            if(out > 0.f)
            {
                v -= n * out;
            }
            if(glm::length(v) * scale > most)
            {
                v *= most / (glm::length(v) * scale);
            }
            b3Body_SetTransform(head.head, world->toM(head.pos), b3Body_GetRotation(head.head));
            b3Body_SetLinearVelocity(head.head, b3v(v));
        }
    }
    // Its middle never crosses the level's surface (the ball keeps its radius off it): one that did (slammed
    // through the floor's mesh, which a sphere sunk past its middle is pushed further through) is put back
    // where it crossed, off the surface, its speed into it taken away.
    {
        vec3_t was, now, none{0.f, 0.f, 0.f};
        store(head.lastGood, was);
        store(head.pos, now);
        const trace_t back = SV_Move(was, none, none, now, MOVE_NOMONSTERS, EDICT_NUM(i));
        if(back.fraction < 1.f && !back.startsolid && !back.allsolid)
        {
            const glm::vec3 n{back.plane.normal[0], back.plane.normal[1], back.plane.normal[2]};
            const glm::vec3 at = glm::vec3{back.endpos[0], back.endpos[1], back.endpos[2]} + n * (headRadius + 0.5f);
            glm::vec3 v = glmv(b3Body_GetLinearVelocity(head.head));
            v -= n * za::min(0.f, glm::dot(v, n));
            b3Body_SetTransform(head.head, world->toM(at), b3Body_GetRotation(head.head));
            b3Body_SetLinearVelocity(head.head, b3v(v));
            head.pos = at;
            flails.info.putBack++;
        }
        head.lastGood = head.pos;
    }
    head.vel = world->toU(b3Body_GetLinearVelocity(head.head));
    if(const float real = glm::length(head.vel) / world->m2u * scale;
        real > rattleSpeed && realtime - head.lastRattle > rattleEvery && qvr::progs::bindings().Flail_Rattle)
    {
        head.lastRattle = realtime;
        QcCallGuard guard;
        pr_global_struct->time = qcvm->time;
        G_INT(OFS_PARM0) = EDICT_TO_PROG(EDICT_NUM(i));
        G_FLOAT(OFS_PARM1) = static_cast<float>(h);
        G_FLOAT(OFS_PARM2) = real;
        PR_ExecuteProgram(qvr::progs::bindings().Flail_Rattle);
    }
    head.rot = fromB3(b3Body_GetRotation(head.head));
    head.anchorAt = world->toU(b3Body_GetPosition(head.anchor));
    Info& info = flails.info;
    info.frames++;
    info.fastest = za::max(info.fastest, glm::length(head.vel) / world->m2u * scale);
    info.handle = za::max(info.handle, glm::length(glmv(b3Body_GetLinearVelocity(head.anchor))) * scale);
    info.stretch = za::max(info.stretch, glm::distance(head.pos, head.anchorAt) - (head.length * world->m2u + ringReach));
    vec3_t from, to, zero{0.f, 0.f, 0.f};
    store(head.anchorAt, from);
    store(head.pos, to);
    const trace_t tr = SV_Move(from, zero, zero, to, MOVE_NOMONSTERS, EDICT_NUM(i));
    info.blocked += tr.fraction < 1.f && !tr.startsolid ? 1 : 0;
    if(tr.fraction < 1.f && !tr.startsolid && vr_flail_debug.value >= 1.f && info.blocked % 20 == 1)
    {
        Con_Printf("flail: the level between the handle %.1f %.1f %.1f and the head %.1f %.1f %.1f (at %.2f, %s)\n",
            head.anchorAt.x, head.anchorAt.y, head.anchorAt.z, head.pos.x, head.pos.y, head.pos.z, tr.fraction, tr.ent ? PR_GetString(tr.ent->v.classname) : "-");
    }
    if(vr_flail_debug.value >= 2.f)
    {
        Con_Printf("flail: client %d hand %d: head %.1f %.1f %.1f, %.1f u/s, %.1f from the handle (%.1f %.1f %.1f)\n", i, h,
            head.pos.x, head.pos.y, head.pos.z, glm::length(head.vel), glm::distance(head.pos, head.anchorAt),
            head.anchorAt.x, head.anchorAt.y, head.anchorAt.z);
    }
}
} // namespace

// After the frame's steps: the heads' places for the view, and their hits to QC.
void callFlailHits()
{
    if(!sameWorld(flails.world, world->id))
    {
        flails.hits.clear();
        return;
    }
    for(int i = 1; i <= MAX_SCOREBOARD; i++)
    {
        for(int h = 0; h < 2; h++)
        {
            Head& head = flails.heads[i][h];
            if(B3_IS_NULL(head.head) || !b3Body_IsValid(head.head))
            {
                continue;
            }
            stepped(head, i, h);
        }
        if(i <= svs.maxclients && i < qcvm->num_edicts && !EDICT_NUM(i)->free)
        {
            noteHeldHeads(EDICT_NUM(i), i);
        }
    }
    const func_t fn = qvr::progs::bindings().Flail_Hit;
    for(const Hit& hit : flails.hits)
    {
        Head& head = flails.heads[hit.client][hit.hand];
        edict_t* player = EDICT_NUM(hit.client);
        edict_t* target = EDICT_NUM(hit.target);
        if(!fn || player->free || target->free)
        {
            continue;
        }
        if((head.lastTarget == hit.target && realtime - head.lastTime < hitCooldown) || realtime - head.madeTime < grace)
        {
            continue;
        }
        flails.info.hits++;
        head.lastTarget = hit.target;
        head.lastTime = realtime;
        if(vr_flail_debug.value)
        {
            Con_Printf("flail: client %d hand %d struck %d %s at %.1f m/s (hand time x%.2f)\n", hit.client, hit.hand, hit.target,
                hit.target ? PR_GetString(target->v.classname) : "the level", hit.speed, handTime());
        }
        QcCallGuard guard;
        pr_global_struct->time = qcvm->time;
        G_INT(OFS_PARM0) = EDICT_TO_PROG(player);
        G_FLOAT(OFS_PARM1) = static_cast<float>(hit.hand);
        G_INT(OFS_PARM2) = EDICT_TO_PROG(target);
        G_FLOAT(OFS_PARM3 + 0) = hit.at.x;
        G_FLOAT(OFS_PARM3 + 1) = hit.at.y;
        G_FLOAT(OFS_PARM3 + 2) = hit.at.z;
        G_FLOAT(OFS_PARM4 + 0) = hit.dir.x;
        G_FLOAT(OFS_PARM4 + 1) = hit.dir.y;
        G_FLOAT(OFS_PARM4 + 2) = hit.dir.z;
        G_FLOAT(OFS_PARM5) = hit.speed;
        G_FLOAT(OFS_PARM6) = head.mass;
        PR_ExecuteProgram(fn);
    }
    flails.hits.clear();
}
} // namespace box3d::detail

namespace qvr::box3d
{
void flailInfo_f()
{
    using namespace ::box3d::detail;
    const Info& i = flails.info;
    Con_Printf("flail info: %d frames held, %d heads made (%d where a head lying about was), chain stretched %.2f units at most, the level between "
               "handle and head %d frames, fastest %.1f m/s (the handle's end %.1f), %d hits dealt, put back out of the level %d times\n",
        i.frames, i.made, i.taken, i.stretch, i.blocked, i.fastest, i.handle, i.hits, i.putBack);
    flails.info = Info{};
    // Lying about (QC vr_ad_melee.qc "The flail lying about"): the handles and heads, paired (each naming the other),
    // heads without their handle, handles without a head; the ropes (syncPropRopes), each's ends apart against its most.
    const int link = fields().vr_fuellink;
    if(!world || !sv.active || link < 0)
    {
        return;
    }
    qcvm_t* old = nullptr;
    PR_PushQCVM(&sv.qcvm, &old);
    int handles = 0, heads = 0, paired = 0, orphans = 0, bare = 0;
    for(int n = svs.maxclients + 1; n < qcvm->num_edicts; n++)
    {
        edict_t* e = EDICT_NUM(n);
        if(e->free)
        {
            continue;
        }
        const char* cls = PR_GetString(e->v.classname);
        const bool head = !strcmp(cls, "vr_flailhead");
        const bool handle = !strcmp(cls, "thrown_weapon") && !strcmp(PR_GetString(e->v.model), qvr::admelee::flailModel);
        if(!head && !handle)
        {
            continue;
        }
        const int ofs = fieldInt(e, link);
        edict_t* other = ofs ? PROG_TO_EDICT(ofs) : nullptr;
        const bool mutual = other && !other->free && fieldInt(other, link) == EDICT_TO_PROG(e) &&
                            !strcmp(PR_GetString(other->v.classname), head ? "thrown_weapon" : "vr_flailhead");
        heads += head ? 1 : 0;
        handles += handle ? 1 : 0;
        paired += head && mutual ? 1 : 0;
        orphans += head && !mutual ? 1 : 0;
        bare += handle && !mutual ? 1 : 0;
    }
    int ropes = 0, joined = 0;
    forEachFlailRope([&](int, int, float, bool j, float) {
        ropes++;
        joined += j ? 1 : 0;
    });
    Con_Printf("flail world: %d handles lying about, %d heads, %d paired, %d orphan heads, %d handles without a head; %d ropes "
               "(%d joined)\n",
        handles, heads, paired, orphans, bare, ropes, joined);
    forEachFlailRope([&](int handle, int head, float length, bool j, float apart) {
        Con_Printf("flail world rope: handle %d, head %d, %s, most %.2f units, the tip and the ring %.2f apart (%s)\n", handle,
            head, j ? "joined" : "not joined", length, apart, apart <= length + 1.f ? "within" : "PAST");
    });
    PR_PopQCVM(old);
}

bool flailHead(int client, int hand, glm::vec3& pos, glm::quat& rot, glm::vec3& anchor)
{
    using namespace ::box3d::detail;
    if(!world || client < 1 || client > MAX_SCOREBOARD || hand < 0 || hand > 1 || !sameWorld(flails.world, world->id))
    {
        return false;
    }
    const Head& h = flails.heads[client][hand];
    if(B3_IS_NULL(h.head) || !b3Body_IsValid(h.head))
    {
        return false;
    }
    pos = h.pos;
    rot = h.rot;
    anchor = h.anchorAt;
    return true;
}
} // namespace qvr::box3d
