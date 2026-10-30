// vr_ragactive_cut.cpp -- active ragdolls dismembered (vr_ragdoll_active_dismember): the stump held, the writhe, the
// balance beat on one leg, the crawl. Moved out of vr_ragactive.cpp; what they share is vr_ragactive_internal.hpp.

#include "vr_ragactive_internal.hpp"

namespace qvr::ragactive
{

// ---- Dismemberment (ROUND22.md, "Active ragdolls: dismemberment"; vr_ragdoll_active_dismember) ----
// A driven death losing a limb (cutReaction, from vr_box3d.cpp ragdollCutLimb: the killing blow's cut, his ragdoll made by
// it from the frame he stood in and driven by his death animation from its start; or a cut on one still dying): his free
// hand to the stump (the cut joint's end on the part it was cut from), stronger and longer than a wound's hold, his torso
// and legs writhing a little for a few seconds once down. On one leg (killed standing by the cut) a balance beat first: his
// pelvis pinned near its height (at most balancePin body weights) while the leg left plants under him, his arms flung out
// and flailing, the pin's anchor drifting to the side lost and his torso bending to it, until his lean passes Balance
// Lean (or the time is up): the pin lets go, his death animation plays from there, he falls. No legs: the crawl on his
// arms (deathFrame's). The head: a collapse, as a headshot. A lost arm never holds anything (its reach stops).

namespace
{

constexpr float holdStrength = 1.f;     // a stump's hold (a wound's: 0.8)
constexpr float stumpReach = 0.85f;     // of the holding arm's length: a stump farther from its shoulder, his torso curls
constexpr float stumpCurlMost = 0.6f;   // rad: at most
constexpr float stumpCurlStrength = 0.6f; // his torso's strength while it curls
constexpr float bloodNear = 4.f;        // units: the hand this near the stump is on it (its palm bloodied)
constexpr float bloodEvery = 0.7f;      // s: another mark while it holds
constexpr int bloodMarksMost = 4;
constexpr float holdRamp = 0.25f;       // s: the hand going to it
constexpr float holdLetGo = 0.8f;       // s: and letting go
constexpr float writheRamp = 0.5f;      // s: the writhe eased in
constexpr float writheOut = 1.f;        // s: and out
constexpr float writheTorso = 0.12f;    // rad: its swing in the torso's joints
constexpr float writheLegs = 0.3f;      // rad: in the legs'
constexpr float writheStrength = 0.35f; // their strength while it lasts (a weak stirring)
constexpr float balancePin = 2.f;       // body weights: the pin holding him up on one leg, at most (12 Hz)
constexpr float braceStrength = 2.5f;   // the leg left: its joints bracing him (a dying leg: 1)
constexpr float tipAccel = 0.3f;        // m/s/s: its anchor drifting to the side lost
constexpr float sagDepth = 0.06f;       // m: and sagging
constexpr float tipBend = 0.35f;        // rad: his torso bent to the side lost by the beat's end (over its joints)
constexpr float flailHz = 1.6f;         // his arms' flail for balance
constexpr float hopLean = 7.f;          // deg: leaning this far from his line on one leg, he hops (at most hopsMost)
constexpr float hopTime = 0.2f;         // s: a hop
constexpr float hopLift = 0.1f;         // of his leg's length: his foot lifted that high
constexpr float hopReach = 0.4f;        // of his leg's length: a hop's way at most
constexpr int hopsMost = 3;
constexpr float pitchRate = 2.5f;       // rad/s: with no legs, killed standing, he pitches forward so

// (chestOf: the shared helper, above)

// The joint limb `limb` was cut at (a cut bone on it whose parent isn't cut); -1 none: it is whole.
[[nodiscard]] int cutJointOn(const State& s, const Parts& p, int limb)
{
    const int top = s.chain[static_cast<za::SizeT>(limb)][0];
    if(top < 0)
    {
        return -1;
    }
    for(int b = 1; b < p.count; b++)
    {
        const int parent = p.rig->bones[b].parent;
        if(cut(p, b) && parent >= 0 && !cut(p, parent) && onBone(*p.rig, b, top))
        {
            return b;
        }
    }
    return -1;
}

// The stump's end in the world (metres), `off` units out of it.
[[nodiscard]] glm::vec3 stumpPoint(const Dismember& m, const Parts& p, float off)
{
    return worldPoint(p, m.stumpPart, m.stumpAt) + bodyRot(p, m.stumpPart) * m.stumpOut * (off / p.m2u);
}

// His line from `foot` (world, metres) to his pelvis (unit): his lean on it (his pelvis pinned: his chest sways with his
// arms).
[[nodiscard]] glm::vec3 bodyLine(const Parts& p, const glm::vec3& foot)
{
    const glm::vec3 v = glmv(b3Body_GetWorldCenter(p.body[0])) - foot;
    return glm::length(v) > 1e-4f ? glm::normalize(v) : glm::vec3{0.f, 0.f, 1.f};
}

// Once he is down (s since the drive began): the writhe from then, the hold through it.
void planDown(State& s, float down)
{
    Dismember& m = s.dismember;
    const float dur = za::max(vr_ragdoll_active_dismember_time.value, 0.f);
    m.writheFrom = m.writheFrom < 0.f ? down : m.writheFrom;
    m.writheUntil = za::max(m.writheUntil, down + dur);
    m.holdUntil = za::max(m.holdUntil, za::max(m.writheUntil, down + 0.8f * s.death.length));
}

void startBalance(State& s, const Parts& p, const Context& c, float t, int lostLeg, int stand)
{
    Dismember& m = s.dismember;
    m.standLeg = stand;
    // (The side lost: his own, level: the rig's y is his left. A hunched ogre's hips are one ahead of the other.)
    m.lostSide = p.turn * glm::vec3{0.f, lostLeg == 2 ? 1.f : -1.f, 0.f};
    // A stumble's carried way (a monster cut as he ran) ends here, kept as far as it got (the pin's anchor doesn't
    // jump): carried on, it dragged his pelvis up to a metre off the planted foot and broke the beat at once.
    Death& d = s.death;
    const glm::vec3 carried = d.stumble ? d.walk * (stumbleTau * (1.f - za::exp(-t / stumbleTau))) : glm::vec3{0.f};
    d.stumble = false;
    d.walk = glm::vec3{0.f};
    m.origin0 = s.origin + carried;
    m.balanceUntil = t + za::clamp(vr_ragdoll_active_balance_time.value, 0.1f, 5.f);
    // The foot left: kept where it stands, braced; leaning, it hops under him (dismemberFrame: hopLean), the pin holding
    // him up meanwhile.
    m.plant = limbEnd(s, p, stand);
    m.line0 = bodyLine(p, m.plant);
    // The pose he stood in, held; every strength full; the pin.
    s.clip = Clip{m.liveFrame, m.liveFrame, static_cast<float>(m.liveFrame), c.now, 10.f, false};
    for(Fade& f : s.fade)
    {
        f = Fade{1.f, 1.f, 0.f, 0.f};
    }
    s.pin.force = balancePin;
    s.pin.hertz = 12.f; // (stiff: its turn holds his pelvis against his torso hanging over the side lost)
    s.pin.fade = Fade{1.f, 0.f, m.balanceUntil, m.balanceUntil + 0.12f};
    s.death.stepLeg = -1;
    s.death.woundArm = -1;
}

// The beat over: the pin lets go, his death animation plays from now (its strengths too); then the hold, the writhe.
void breakBalance(State& s, const Context& c, float t)
{
    Dismember& m = s.dismember;
    m.brokeAt = t;
    s.pin.fade = Fade{1.f, 0.f, t, t + 0.12f};
    s.clip = m.deathClip;
    s.clip.t0 = c.now;
    for(int ch = 0; ch < channelCount; ch++)
    {
        Fade& f = m.deathFade[static_cast<za::SizeT>(ch)];
        f.t0 += t;
        f.t1 += t;
        s.fade[static_cast<za::SizeT>(ch)] = f;
    }
    for(Reach& r : s.reach)
    {
        r.weight = 0.f;
    }
    m.holdFrom = t + 0.1f;
    if(!(s.death.crawl && (m.lost & 12u) == 12u))
    {
        planDown(s, t + 0.8f * s.death.length);
    }
}

} // namespace

namespace detail
{

void dismemberFrame(State& s, const Parts& p, const Context& c, float t)
{
    Dismember& m = s.dismember;
    const Death& d = s.death;
    if(d.collapse)
    {
        return;
    }
    const ragdoll::Rig& rig = *p.rig;
    const glm::vec3 up{0.f, 0.f, 1.f};
    const float twoPi = 6.2831853f;
    for(glm::vec3& w : m.spin)
    {
        w = glm::vec3{0.f};
    }
    // No legs, killed standing: he pitches forward (his whole body turned about his pelvis).
    if(m.pitch)
    {
        m.pitch = false;
        const glm::vec3 front = bodyRot(p, chestOf(p)) * glm::vec3{1.f, 0.f, 0.f};
        const glm::vec2 level{front.x, front.y};
        const glm::vec3 fwd = glm::length(level) > 1e-3f ? glm::vec3{glm::normalize(level), 0.f} : glm::vec3{1.f, 0.f, 0.f};
        const glm::vec3 w = glm::cross(up, fwd) * pitchRate;
        const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
        for(int b = 0; b < p.count; b++)
        {
            if(!cut(p, b) && rig.bones[b].joint != ragdoll::Joint::Loose)
            {
                const b3BodyId body = p.body[b];
                b3Body_SetAngularVelocity(body, b3v(glmv(b3Body_GetAngularVelocity(body)) + w));
                b3Body_SetLinearVelocity(body,
                    b3v(glmv(b3Body_GetLinearVelocity(body)) + glm::cross(w, glmv(b3Body_GetWorldCenter(body)) - pelvis)));
            }
        }
    }
    // On one leg, keeping his balance.
    if(m.balanceUntil >= 0.f && m.brokeAt < 0.f)
    {
        const int chest = chestOf(p);
        m.lean = glm::degrees(glm::acos(za::clamp(glm::dot(bodyLine(p, limbEnd(s, p, m.standLeg)), m.line0), -1.f, 1.f)));
        // A hop: leaning past hopLean, the foot lifted and put down under his pelvis (a little towards the side lost), his
        // line taken again from there; at most hopsMost. (His lean isn't judged while the foot is up.)
        bool hopping = m.hopFrom >= 0.f && t - m.hopFrom < hopTime;
        if(m.hopFrom >= 0.f && !hopping)
        {
            m.plant = m.hopB;
            m.line0 = bodyLine(p, limbEnd(s, p, m.standLeg));
            m.hopFrom = -1.f;
            m.hops++;
            m.lean = 0.f;
        }
        if(!hopping && m.hops < hopsMost && m.lean > hopLean && limbWhole(s, p, m.standLeg) && t < m.balanceUntil - hopTime)
        {
            const float len = limbLength(s, p, m.standLeg);
            const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
            glm::vec3 way = level(pelvis - m.plant) + m.lostSide * (0.05f * len);
            const float wl = glm::length(way);
            way = wl > hopReach * len ? way * (hopReach * len / wl) : way;
            m.hopA = m.plant;
            m.hopB = m.plant + way;
            m.hopB.z = m.plant.z + floorAt(c, glm::vec3{m.hopB.x, m.hopB.y, m.plant.z}, len, m.plant.z) -
                       floorAt(c, m.plant, len, m.plant.z);
            m.hopFrom = t;
            hopping = true;
            if(vr_debug_ragdoll.value)
            {
                Con_Printf("ragdoll: %d hops on his leg at %.2f s, leaning %.0f deg: %.1f units\n", c.num, t, m.lean,
                    glm::length(level(way)) * p.m2u);
            }
        }
        if(t >= m.balanceUntil || (!hopping && m.lean > za::max(vr_ragdoll_active_balance_lean.value, 1.f)) ||
            !limbWhole(s, p, m.standLeg))
        {
            breakBalance(s, c, t);
            if(vr_debug_ragdoll.value)
            {
                Con_Printf("ragdoll: %d loses his balance after %.2f s, leaning %.0f deg\n", c.num, t, m.lean);
            }
        }
        else
        {
            if(vr_debug_ragdoll.value >= 2.f)
            {
                const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0])) * p.m2u;
                const glm::vec3 foot = limbEnd(s, p, m.standLeg) * p.m2u;
                const glm::vec3 anchor = B3_IS_NON_NULL(s.pin.anchor) && b3Body_IsValid(s.pin.anchor)
                                             ? glmv(b3Body_GetPosition(s.pin.anchor)) * p.m2u
                                             : glm::vec3{0.f};
                Con_Printf("ragdoll: %d balancing %.2f s: lean %.1f deg, pelvis %.1f %.1f %.1f (pin %.1f %.1f %.1f), foot %.1f "
                           "%.1f %.1f (to %.1f %.1f %.1f)\n", c.num, t, m.lean, pelvis.x, pelvis.y, pelvis.z, anchor.x, anchor.y,
                    anchor.z, foot.x, foot.y, foot.z, m.plant.x * p.m2u, m.plant.y * p.m2u, m.plant.z * p.m2u);
            }
            // The pin's anchor drifting to the side lost, sagging a little; his torso bending to it.
            s.origin = m.origin0 + m.lostSide * (0.5f * tipAccel * t * t) - up * (sagDepth * smooth01(t / 0.4f));
            int torso = 0;
            for(int b = 1; b < p.count; b++)
            {
                torso += drivable(s, p, b) && channelOf(rig.bones[b]) == Channel::Torso ? 1 : 0;
            }
            // (About the rig's forward axis: +y, his left, is up turned towards it the negative way.)
            const float bend = tipBend * smooth01(t / za::max(m.balanceUntil, 0.1f)) / static_cast<float>(za::max(torso, 1)) *
                               (m.standLeg == 3 ? -1.f : 1.f);
            for(int b = 1; b < p.count; b++)
            {
                if(drivable(s, p, b) && channelOf(rig.bones[b]) == Channel::Torso)
                {
                    const auto i = static_cast<za::SizeT>(b);
                    s.target[i] = glm::normalize(glm::angleAxis(bend, glm::vec3{1.f, 0.f, 0.f}) * s.target[i]);
                }
            }
            // The leg left planted, bracing him.
            Reach& leg = s.reach[static_cast<za::SizeT>(m.standLeg)];
            leg.goal = m.plant;
            if(hopping)
            {
                const float u = za::clamp((t - m.hopFrom) / hopTime, 0.f, 1.f);
                leg.goal = glm::mix(m.hopA, m.hopB, smooth01(u)) +
                           up * (hopLift * limbLength(s, p, m.standLeg) * za::sin(3.14159265f * u));
            }
            leg.weight = smooth01(t / 0.05f);
            leg.strength = braceStrength;
            // His arms flung out, flailing.
            const glm::quat qc = bodyRot(p, chest);
            for(const int arm : {0, 1})
            {
                if(!limbWhole(s, p, arm))
                {
                    continue;
                }
                const int ua = s.chain[static_cast<za::SizeT>(arm)][0];
                const glm::vec3 shoulder = worldPoint(p, ua, rig.bones[ua].pivot);
                const glm::vec3 out = qc * glm::vec3{0.f, arm == 0 ? 1.f : -1.f, 0.f};
                const glm::vec3 fwd = qc * glm::vec3{1.f, 0.f, 0.f};
                // (The two nearly mirrored: their pull on his chest about cancels.)
                const float ph = twoPi * (flailHz * t + roll(m.seed, 3u)) + (arm == 0 ? 0.f : 0.6f);
                const glm::vec3 dir = glm::normalize(out * 0.75f + up * (0.1f + 0.3f * za::sin(ph)) + fwd * (0.25f * glm::cos(ph)));
                Reach& r = s.reach[static_cast<za::SizeT>(arm)];
                r.goal = shoulder + dir * (0.9f * limbLength(s, p, arm));
                r.weight = smooth01(t / 0.4f);
                r.strength = 0.5f;
            }
        }
    }
    const bool balancing = m.balanceUntil >= 0.f && m.brokeAt < 0.f;
    const bool crawling = d.crawlFrom >= 0.0;
    // A hand on the stump.
    float hold = 0.f;
    if(m.holdArm >= 0 && !balancing && !crawling)
    {
        Reach& r = s.reach[static_cast<za::SizeT>(m.holdArm)];
        if(!limbWhole(s, p, m.holdArm) || cut(p, m.stumpPart))
        {
            r.weight = 0.f;
            m.holdArm = -1;
        }
        else
        {
            hold = smooth01((t - m.holdFrom) / holdRamp) * (1.f - smooth01((t - m.holdUntil) / holdLetGo));
            r.goal = stumpPoint(m, p, handThick);
            r.weight = hold;
            r.strength = holdStrength;
            m.handGap = glm::distance(limbEnd(s, p, m.holdArm), stumpPoint(m, p, 0.f)) * p.m2u;
            // Its palm bloodied while on the wound: a mark as it gets there, another every bloodEvery while held.
            if(hold > 0.5f && m.handGap < bloodNear && m.bloodMarks < bloodMarksMost &&
                (m.bloodAt < 0.f || t - m.bloodAt >= bloodEvery))
            {
                m.bloodAt = t;
                m.bloodMarks++;
                m.bloodDue++;
            }
            if(vr_debug_ragdoll.value >= 2.f)
            {
                const int ua = s.chain[static_cast<za::SizeT>(m.holdArm)][0];
                Con_Printf("ragdoll: %d holding the stump %.2f s: the hand %.1f units off, the stump %.2f arm's lengths from "
                           "the shoulder\n", c.num, t, m.handGap,
                    glm::distance(worldPoint(p, ua, rig.bones[ua].pivot), r.goal) / za::max(limbLength(s, p, m.holdArm), 1e-4f));
            }
        }
    }
    // The writhe: his torso and legs a little strength (over his death's), their targets swung slowly about it.
    float env = 0.f;
    if(!balancing && !crawling && m.writheFrom >= 0.f && m.writheUntil > m.writheFrom)
    {
        env = smooth01((t - m.writheFrom) / writheRamp) * (1.f - smooth01((t - (m.writheUntil - writheOut)) / writheOut));
        for(const Channel ch : {Channel::Torso, Channel::Legs})
        {
            const auto i = static_cast<za::SizeT>(ch);
            const float v = za::max(fadeAt(m.deathFade[i], t), writheStrength * env);
            s.fade[i] = Fade{v, v, 0.f, 0.f};
        }
        const float w = t - m.writheFrom;
        for(int b = 1; b < p.count && env > 0.f; b++)
        {
            const ragdoll::Bone& bone = rig.bones[b];
            const Channel ch = channelOf(bone);
            if(!drivable(s, p, b) || (ch != Channel::Torso && ch != Channel::Legs))
            {
                continue;
            }
            bool led = false;
            for(int limb = 0; limb < limbCount; limb++)
            {
                const auto& chain = s.chain[static_cast<za::SizeT>(limb)];
                led = led || (s.reach[static_cast<za::SizeT>(limb)].weight > 0.f && (chain[0] == b || chain[1] == b));
            }
            if(led)
            {
                continue;
            }
            const auto ub = static_cast<uint32_t>(b);
            const float amp = (ch == Channel::Legs ? writheLegs : writheTorso) * env * za::max(vr_ragdoll_active_writhe.value, 0.f);
            const float hz = 0.7f + 0.8f * roll(m.seed, 16u + ub);
            const float phase = twoPi * (hz * w + roll(m.seed, 48u + ub));
            const float angle = amp * za::sin(phase);
            const glm::vec3 axis = bone.joint == ragdoll::Joint::Hinge
                                       ? glm::normalize(bone.hinge)
                                       : glm::normalize(glm::vec3{roll(m.seed, 80u + ub) - 0.5f, 1.f, 0.6f * (roll(m.seed, 112u + ub) - 0.5f)});
            const auto i = static_cast<za::SizeT>(b);
            s.target[i] = glm::normalize(glm::angleAxis(angle, axis) * s.target[i]);
            m.spin[i] = axis * (amp * twoPi * hz * glm::cos(phase));
        }
    }
    // The stump past his reach (the ogre's arms, a stump lying under him): his torso curls towards it, bringing the
    // holding shoulder nearer (its turn about his pelvis shared by his torso's joints), held up enough to do it.
    m.curl = 0.f;
    if(hold > 0.f && m.holdArm >= 0)
    {
        const int u = s.chain[static_cast<za::SizeT>(m.holdArm)][0];
        const glm::vec3 shoulder = worldPoint(p, u, rig.bones[u].pivot);
        const glm::vec3 stump = stumpPoint(m, p, 0.f);
        const float len = limbLength(s, p, m.holdArm);
        const float over = glm::distance(shoulder, stump) - stumpReach * len;
        const glm::vec3 mid = glmv(b3Body_GetWorldCenter(p.body[0]));
        const glm::vec3 axis = glm::cross(shoulder - mid, stump - mid);
        int n = 0;
        for(int b = 1; b < p.count; b++)
        {
            n += drivable(s, p, b) && channelOf(rig.bones[b]) == Channel::Torso ? 1 : 0;
        }
        if(over > 0.f && glm::length(axis) > 1e-5f && n > 0)
        {
            m.curl = za::min(over / za::max(glm::distance(shoulder, mid), 0.1f), stumpCurlMost) * hold;
            for(int b = 1; b < p.count; b++)
            {
                if(!drivable(s, p, b) || channelOf(rig.bones[b]) != Channel::Torso)
                {
                    continue;
                }
                const glm::quat qp = bodyRot(p, rig.bones[b].parent);
                const glm::quat turn = glm::angleAxis(m.curl / static_cast<float>(n), glm::normalize(axis));
                const auto i = static_cast<za::SizeT>(b);
                s.target[i] = glm::normalize(glm::inverse(qp) * turn * qp * s.target[i]);
            }
            const auto ti = static_cast<za::SizeT>(Channel::Torso);
            const float v = za::max(za::max(fadeAt(m.deathFade[ti], t), writheStrength * env), stumpCurlStrength * hold);
            s.fade[ti] = Fade{v, v, 0.f, 0.f};
            m.curling = true;
        }
    }
    if(m.curl <= 0.f && m.curling)
    {
        m.curling = false; // (its torso's strength its death's again, or the writhe's: above, next frame)
        const auto ti = static_cast<za::SizeT>(Channel::Torso);
        s.fade[ti] = m.deathFade[ti];
    }
    if(c.trace)
    {
        // (The tests: the hand from the stump, his lean, the beat (1 on it, 2 broken), when it broke, the hold's weight,
        // the writhe's, how his torso and legs stir (their parts' spin from the pelvis's, mean rad/s), the limbs lost.)
        float stir = 0.f;
        int n = 0;
        const glm::vec3 w0 = glmv(b3Body_GetAngularVelocity(p.body[0]));
        for(int b = 1; b < p.count; b++)
        {
            const Channel ch = channelOf(rig.bones[b]);
            if(drivable(s, p, b) && (ch == Channel::Torso || ch == Channel::Legs))
            {
                stir += glm::length(glmv(b3Body_GetAngularVelocity(p.body[b])) - w0);
                n++;
            }
        }
        Con_Printf("ragdis %d %.3f gap %.1f lean %.1f beat %d %.2f hold %.2f writhe %.2f stir %.2f lost %u curl %.2f face %d\n",
            c.num,
            static_cast<float>(c.now - c.born), m.handGap, m.lean, balancing ? 1 : (m.brokeAt >= 0.f ? 2 : 0), m.brokeAt, hold,
            env, stir / static_cast<float>(za::max(n, 1)), m.lost, m.curl, m.face ? 1 : 0);
    }
}

} // namespace detail

void cutReaction(State& s, const Parts& p, int joint, bool crawler, const Context& c)
{
    if(!s.on || !s.death.on || !p.rig)
    {
        return;
    }
    const ragdoll::Rig& rig = *p.rig;
    Death& d = s.death;
    Dismember& m = s.dismember;
    const float t = static_cast<float>(c.now - s.begun);
    // (What is left of him weighs less: the crawl's pull.)
    s.totalMass = 0.f;
    for(int b = 0; b < p.count; b++)
    {
        s.totalMass += !cut(p, b) && rig.bones[b].joint != ragdoll::Joint::Loose ? b3Body_GetMass(p.body[b]) : 0.f;
    }
    // Beheaded: a collapse, as a headshot (his strength almost none; no pin, hand, step or crawl).
    if(joint == rig.head || d.collapse)
    {
        if(!d.collapse && vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d beheaded %.2f s into his death: a collapse\n", c.num, t);
        }
        s.base = za::min(s.base, za::clamp(vr_ragdoll_active_headshot.value, 0.f, 1.f));
        d.collapse = true;
        d.stepLeg = d.woundArm = -1;
        d.crawl = false;
        m.on = false;
        for(Reach& r : s.reach)
        {
            r.weight = 0.f;
        }
        dropPin(s.pin);
        return;
    }
    if(vr_ragdoll_active_dismember.value == 0.f)
    {
        return;
    }
    uint32_t lost = 0;
    for(int limb = 0; limb < limbCount; limb++)
    {
        lost |= cutJointOn(s, p, limb) >= 0 ? 1u << limb : 0u;
    }
    if(!lost)
    {
        return;
    }
    const bool first = !m.on;
    if(first)
    {
        const int live = m.liveFrame;
        m = Dismember{};
        m.on = true;
        m.liveFrame = live;
        m.seed = hash(static_cast<uint32_t>(c.num), static_cast<uint32_t>(c.now * 72.0)) ^
                 static_cast<uint32_t>(za::max(vr_ragdoll_active_seed.value, 0.f)) * 40503u;
        m.deathFade = s.fade; // (his death's strengths: the writhe's floor over them)
        m.deathClip = s.clip;
    }
    m.lost = lost;
    const auto isLost = [&](int limb) { return (lost & (1u << limb)) != 0u; };
    const int legsLost = (isLost(2) ? 1 : 0) + (isLost(3) ? 1 : 0);
    // One leg lost as the cut killed him standing: he tries to keep his balance on the other. (A later cut taking that
    // leg ends it.)
    if(first && m.liveFrame >= 0 && legsLost == 1 && t < 0.05f && vr_ragdoll_active_balance_time.value > 0.f &&
        limbWhole(s, p, isLost(2) ? 3 : 2))
    {
        startBalance(s, p, c, t, isLost(2) ? 2 : 3, isLost(2) ? 3 : 2);
    }
    else if(m.balanceUntil >= 0.f && m.brokeAt < 0.f && (legsLost != 1 || !limbWhole(s, p, m.standLeg)))
    {
        breakBalance(s, c, t);
    }
    m.liveFrame = -1;
    const bool balancing = m.balanceUntil >= 0.f && m.brokeAt < 0.f;
    // No legs: he crawls on his arms (his kind crawls, both arms whole) once he lies face down.
    const bool legless = legsLost == 2 && crawler && limbWhole(s, p, 0) && limbWhole(s, p, 1);
    d.crawl = d.crawl || legless;
    if(legless)
    {
        m.writheFrom = m.writheUntil = -1.f; // (his arms crawl; no writhe)
        // No roll onto his front for him (the roll turns his whole body as one; a legless ogre's was thrown 85 units):
        // killed so standing, he pitches forward onto his front, his pose held (no death animation: it falls back).
        d.rollFrom = c.now - 100.0;
        if(t < 0.1f)
        {
            dropPin(s.pin);
            s.clip.first = -1;
            m.pitch = true; // (at his first drive: after the frame's knock is scaled, vr_limbs_body_speed)
        }
    }
    // The stump a hand holds: an arm's (the other hand), else a leg's (the hand on its side, else the other).
    int stumpLimb = -1, arm = -1;
    for(const int limb : {0, 1})
    {
        if(!legless && stumpLimb < 0 && isLost(limb) && limbWhole(s, p, 1 - limb))
        {
            stumpLimb = limb;
            arm = 1 - limb;
        }
    }
    for(const int limb : {2, 3})
    {
        if(legless || stumpLimb >= 0 || !isLost(limb))
        {
            continue;
        }
        const int same = limb - 2;
        arm = limbWhole(s, p, same) ? same : (limbWhole(s, p, 1 - same) ? 1 - same : -1);
        stumpLimb = arm >= 0 ? limb : -1;
    }
    if(arm >= 0)
    {
        const int j = cutJointOn(s, p, stumpLimb);
        const int part = rig.bones[j].parent;
        glm::vec3 mid{0.f};
        for(const glm::vec3& v : rig.bones[part].points)
        {
            mid += v;
        }
        mid /= static_cast<float>(za::max<za::SizeT>(rig.bones[part].points.size(), 1));
        if(m.holdArm != arm || m.stumpPart != part)
        {
            m.holdFrom = balancing ? 1e9f : t + 0.05f;
        }
        m.holdArm = arm;
        m.stumpPart = part;
        // (On its surface, the joint itself is inside it: the part's vertex nearest the joint, leaning towards the
        // holding arm's shoulder and his front, where a hand reaches it without going through him; within 0.75 of that
        // arm's length of its shoulder if any is: an ogre's arms don't reach across his chest.)
        const glm::vec3 shoulder = rig.bones[s.chain[static_cast<za::SizeT>(arm)][0]].pivot;
        const float reach = 0.75f * limbLength(s, p, arm) / za::max(p.k, 1e-6f);
        m.stumpAt = rig.bones[j].pivot;
        float best = 1e30f;
        for(const glm::vec3& v : rig.bones[part].points)
        {
            const float toShoulder = glm::distance(v, shoulder);
            const float score = glm::distance(v, rig.bones[j].pivot) + 0.5f * toShoulder - 0.3f * (v.x - mid.x) +
                                4.f * za::max(toShoulder - reach, 0.f);
            m.stumpAt = score < best ? v : m.stumpAt;
            best = za::min(best, score);
        }
        m.stumpOut = glm::length(m.stumpAt - mid) > 1e-3f ? glm::normalize(m.stumpAt - mid) : glm::vec3{0.f, 0.f, 1.f};
        // The cut face itself (the joint, facing along the lost limb's bone) when it is within that reach: the hand cups
        // the wound, not the skin beside it.
        const glm::vec3 along = rig.bones[j].end - rig.bones[j].pivot;
        m.face = false;
        if(glm::length(along) > 1e-3f && glm::distance(rig.bones[j].pivot, shoulder) <= reach)
        {
            m.face = true;
            m.stumpAt = rig.bones[j].pivot;
            m.stumpOut = glm::normalize(along);
        }
        if(d.woundArm == arm)
        {
            s.reach[static_cast<za::SizeT>(arm)].weight = 0.f;
            d.woundArm = -1; // (the stump first)
        }
    }
    else if(m.holdArm >= 0)
    {
        s.reach[static_cast<za::SizeT>(m.holdArm)].weight = 0.f;
        m.holdArm = -1;
    }
    // Once down (the end of his death animation; after the beat, from its break): the writhe, the hold through it.
    if(!balancing && !legless)
    {
        planDown(s, za::max(t + 0.3f, 0.8f * d.length));
    }
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d lost his %s%s%s%s (%s now) %.2f s into his death:%s%s%s%s, writhe %.1f-%.1f s\n", c.num,
            isLost(0) ? "left arm " : "", isLost(1) ? "right arm " : "", isLost(2) ? "left leg " : "", isLost(3) ? "right leg " : "",
            rig.bones[joint].name, t,
            arm >= 0 ? (arm == 0 ? " the left hand to the stump" : " the right hand to the stump") : " no hand to the stump",
            balancing ? ", balancing on one leg" : "", legless ? ", to crawl on his arms" : "", d.woundArm >= 0 ? ", a hand on his wound" : "",
            m.writheFrom, m.writheUntil);
    }
}

int limbJointOf(const ragdoll::Rig& rig, int limb, bool lower)
{
    const bool leg = limb >= 2;
    for(int b = 0; b < rig.numBones; b++)
    {
        const char* n = rig.bones[b].name;
        const bool upper = leg ? std::strncmp(n, "thigh_", 6) == 0 : (std::strncmp(n, "upperarm_", 9) == 0 || std::strncmp(n, "arm_", 4) == 0);
        if(!upper || sideOf(n) != (limb & 1))
        {
            continue;
        }
        if(!lower)
        {
            return b;
        }
        for(int c = 0; c < rig.numBones; c++)
        {
            const char* cn = rig.bones[c].name;
            const bool below = leg ? std::strncmp(cn, "shin_", 5) == 0 : (std::strncmp(cn, "forearm_", 8) == 0 || std::strncmp(cn, "hand_", 5) == 0);
            if(rig.bones[c].parent == b && below)
            {
                return c;
            }
        }
    }
    return -1;
}

bool takeHandBlood(State& s, const Parts& p, glm::vec3& at, glm::vec3& dir)
{
    Dismember& m = s.dismember;
    if(m.bloodDue <= 0 || m.holdArm < 0 || !limbWhole(s, p, m.holdArm) || m.stumpPart < 0 || cut(p, m.stumpPart))
    {
        m.bloodDue = 0;
        return false;
    }
    m.bloodDue--;
    at = limbEnd(s, p, m.holdArm);
    const glm::vec3 to = stumpPoint(m, p, 0.f) - at;
    dir = glm::length(to) > 1e-4f ? glm::normalize(to) : glm::vec3{0.f, 0.f, -1.f};
    return true;
}
} // namespace qvr::ragactive
