// vr_ragactive_hold.cpp -- active ragdolls holding on (vr_ragdoll_active_grab, vr_ragdoll_active_pullup): grabbing
// another monster while falling, a living one's climb up a ledge. Moved out of vr_ragactive.cpp; what they share is
// vr_ragactive_internal.hpp.

#include "vr_ragactive_internal.hpp"

namespace qvr::ragactive
{

namespace
{

// ---- Grabbing others (vr_ragdoll_active_grab) ----
// Falling (a living one's fall, a death's), another monster near a hand (standing: its box; knocked down or dead: its
// ragdoll's parts) above its waist: the nearer hand reaches for the nearest point of it (the way it falls first) and,
// there, holds on a moment (a capped motor joint to that body, as a lean's: it slows the fall, a ragdoll held is dragged
// along), then lets go (its time, its load past the cap, struck, landed, pulled away, the other gone). Once a fall.
constexpr float grabReach = 1.1f;  // (of its arm's length) another's nearest point this near a shoulder is grabbed
constexpr float grabCap = 1.f;      // body weights its hand's hold takes at most (a heavy one slips sooner: holdCap's)
constexpr float grabReachTime = 0.6f; // s: a hand not on it by then gives up

// The shapes round a falling body (findGrab's).
za::Vector<b3ShapeId> envOthers;

} // namespace

namespace detail
{

// Another monster near a hand as it falls (its chest `chestAt`, its motion `v`): that hand to it. Whether one did.
bool findGrab(State& s, const Parts& p, const Context& c, const glm::vec3& chestAt, const glm::vec3& v, float L)
{
    Env& e = s.env;
    if(!c.graspable || !c.othersMask || L <= 0.f)
    {
        return false;
    }
    b3AABB box;
    box.lowerBound = b3v(chestAt - glm::vec3{1.5f * L});
    box.upperBound = b3v(chestAt + glm::vec3{1.5f * L});
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.maskBits = c.othersMask;
    envOthers.clear();
    b3World_OverlapAABB(c.world, box, filter,
        [](b3ShapeId shape, void* out) {
            static_cast<za::Vector<b3ShapeId>*>(out)->pushBack(shape);
            return true;
        },
        &envOthers);
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    const glm::vec3 left = bodyRot(p, chestOf(p)) * glm::vec3{0.f, 1.f, 0.f};
    const float speed = glm::length(v);
    const glm::vec3 way = speed > 1e-3f ? v / speed : glm::vec3{0.f};
    int bestArm = -1;
    float best = 1e30f;
    glm::vec3 bestAt{0.f};
    b3ShapeId bestShape = b3_nullShapeId;
    for(const b3ShapeId shape : envOthers)
    {
        const int num = static_cast<int>(reinterpret_cast<intptr_t>(b3Shape_GetUserData(shape)));
        if(num == c.num || num <= 0 || !c.graspable(num))
        {
            continue;
        }
        for(int arm = 0; arm < 2; arm++)
        {
            const EnvHand& h = e.hand[static_cast<za::SizeT>(arm)];
            if(!limbWhole(s, p, arm) || h.mode != Grip::None)
            {
                continue;
            }
            const glm::vec3 shoulder = shoulderOf(s, p, arm);
            // (Its nearest point to the hand: where the arm reaches easily, not one at the shoulder.)
            const glm::vec3 hand = limbEnd(s, p, arm);
            const glm::vec3 q = glmv(b3Shape_GetClosestPoint(shape, b3v(hand)));
            const float d = glm::distance(q, shoulder);
            const float side = glm::dot(q - chestAt, left) * (arm == 0 ? 1.f : -1.f);
            // (Within reach, above its waist (a body lying: low too, it falls onto it), not across its body; the way it
            // falls first.)
            const float low = b3Body_GetType(b3Shape_GetBody(shape)) == b3_dynamicBody ? 0.9f : 0.1f;
            if(d > grabReach * L || d < 1e-3f || q.z < pelvis.z - low * L || side < -0.3f * L)
            {
                continue;
            }
            const float score = glm::distance(q, hand) - 0.4f * L * glm::dot(glm::normalize(q - shoulder), way);
            if(score < best)
            {
                best = score;
                bestArm = arm;
                bestAt = q;
                bestShape = shape;
            }
        }
    }
    if(bestArm < 0)
    {
        return false;
    }
    EnvHand& h = e.hand[static_cast<za::SizeT>(bestArm)];
    h.mode = Grip::Grab;
    h.on = b3Shape_GetBody(bestShape);
    h.local = glmv(b3Body_GetLocalPoint(h.on, b3v(bestAt)));
    h.other = static_cast<int>(reinterpret_cast<intptr_t>(b3Shape_GetUserData(bestShape)));
    h.goal = bestAt;
    h.normal = glm::vec3{0.f, 0.f, 1.f};
    h.since = c.now;
    h.touched = -1.0;
    h.want = 1.f;
    h.strength = 0.9f;
    h.holdUntil = 0.0;
    e.grabs++;
    if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
    {
        Con_Printf("ragenv: %d %s hand grabs at %s %d falling at %.1f m/s: %.0f units from its shoulder, at %.0f %.0f %.0f\n",
            c.num, armName(bestArm), b3Body_GetType(h.on) == b3_dynamicBody ? "the ragdoll of" : "monster", h.other, speed,
            glm::distance(bestAt, shoulderOf(s, p, bestArm)) * p.m2u, bestAt.x * p.m2u, bestAt.y * p.m2u, bestAt.z * p.m2u);
    }
    return true;
}

// A hand reaching for another (Grab) or holding it: held once there (holdOn's motor joint, capped), its upkeep.
void keepGrab(State& s, const Parts& p, const Context& c, int arm, float L)
{
    EnvHand& h = s.env.hand[static_cast<za::SizeT>(arm)];
    if(B3_IS_NULL(h.on) || !b3Body_IsValid(h.on) || (B3_IS_NON_NULL(h.joint) && !b3Joint_IsValid(h.joint)))
    {
        letGo(s, arm, &c, "its hold gone");
        return;
    }
    const glm::vec3 at = glmv(b3Body_GetWorldPoint(h.on, b3v(h.local)));
    const glm::vec3 hand = limbEnd(s, p, arm);
    if(B3_IS_NULL(h.joint))
    {
        h.goal = at;
        if(glm::distance(hand, at) < (s.env.living ? 0.55f : 0.4f) * L) // (held where the hand is: its hold pulls it the rest of the way; a fall is quick)
        {
            h.holdUntil = c.now + 0.5 + 0.6 * static_cast<double>(roll(s.env.seed, 63u + static_cast<uint32_t>(arm)));
            const float heavy = za::clamp(za::sqrt(90.f / za::max(s.totalMass, 1.f)), 0.6f, 1.2f); // (holdCap's)
            holdOn(s, p, c, arm, hand, grabCap * heavy * s.totalMass * c.gravity);
        }
        else if(c.now - h.since > grabReachTime)
        {
            letGo(s, arm, &c, va("out of reach: %.0f units off it", glm::distance(hand, at) * p.m2u));
        }
        return;
    }
    h.load = glm::length(glmv(b3Joint_GetConstraintForce(h.joint)));
    h.overSince = h.load >= 0.95f * h.cap ? (h.overSince < 0.0 ? c.now : h.overSince) : -1.0;
    h.goal = glmv(b3Body_GetWorldPoint(h.on, b3Joint_GetLocalFrameA(h.joint).p));
    h.strength = 0.8f;
    if(h.overSince >= 0.0 && c.now - h.overSince > slipTime)
    {
        letGo(s, arm, &c, "its load");
    }
    else if(c.now > h.holdUntil)
    {
        letGo(s, arm, &c, "its time");
    }
    else if(glm::distance(hand, h.goal) > 0.8f * L)
    {
        letGo(s, arm, &c, va("pulled away: the hand %.0f units off its hold", glm::distance(hand, h.goal) * p.m2u));
    }
}

} // namespace detail

// ---- Climbs (vr_ragdoll_active_pullup; vr_ragactive.hpp, Climb) ----
// A living knockdown's fall past a lip (livingLedge: drive()'s for a fall, beginClimb's for a body falling undriven):
// findLedge's hands reach for it and hold (the env's Ledge holds, a living one's grip climbGrip times a dying one's). It
// hangs a moment, turning to face the wall, then (by the chance) pulls itself up: its pelvis pinned (the pin, a capped
// motor joint as a get-up's) and carried along a way from where it hangs, up under the lip (its chest over the top),
// then up over the lip and in onto the top, its pelvis turned to face the wall and pitched forwards as it goes over (its
// body lying down on the top); its hands holding until it goes in, a leg reaching up onto the top. Not pulling, it drops
// after its hang as a death's. Its hands both let go (struck, its load) or the pin can't carry it: it falls on (a Fall,
// its arms bracing). Up: its pin dropped, its strength easing off, it lies on the top, a knockdown again.

namespace
{

// Its chest (part `b`) drawn towards `goal` (world, metres) as it climbs: a capped spring (3 Hz, as if 0.4 of its
// weight hung on it; at most climbChestPull body weights), its pin's way being its pelvis's alone.
void climbChest(const State& s, const Parts& p, const Context& c, int b, const glm::vec3& goal)
{
    const float w = twoPi * 3.f;
    const glm::vec3 x = glmv(b3Body_GetWorldCenter(p.body[b]));
    const glm::vec3 v = glmv(b3Body_GetLinearVelocity(p.body[b]));
    glm::vec3 f = (0.4f * s.totalMass) * (w * w * (goal - x) - 2.f * w * v);
    const float most = climbChestPull * s.totalMass * c.gravity * s.base;
    const float n = glm::length(f);
    if(n > most && n > 1e-4f)
    {
        f *= most / n;
    }
    pushPart(p, b, f);
}

// Its climb over (`up`: pulled up, else dropped): its holds let go, its pin dropped, its strength easing off (up: it lies
// on the top, a knockdown), or a fall from here (its arms bracing).
void endClimb(State& s, const Parts& p, const Context& c, bool up, const char* why)
{
    Climb& k = s.climb;
    const float t = static_cast<float>(c.now - s.begun);
    for(int arm = 0; arm < 2; arm++)
    {
        letGo(s, arm, &c, why);
    }
    k.on = false;
    k.pinned = false;
    k.phase = ClimbPhase::Done;
    k.ended = why;
    dropPin(s.pin);
    s.pin.force = 0.f;
    s.support = Fade{fadeAt(s.support, t), 0.f, t, t + 0.25f};
    for(int leg = 2; leg < 4; leg++)
    {
        s.reach[static_cast<za::SizeT>(leg)].weight = 0.f;
    }
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
    {
        Con_Printf("climb: %d %s (%s) after %.2f s: its chest %.0f units over the lip at most; its pelvis %.0f units in from the "
                   "lip, %.0f over the top\n",
            c.num, up ? "pulled itself up" : "dropped", why, c.now - k.since0, k.rose * p.m2u,
            -glm::dot(pelvis - k.lip, k.out) * p.m2u, (pelvis.z - k.top) * p.m2u);
    }
    if(up)
    {
        for(int ch = 0; ch < channelCount; ch++)
        {
            Fade& f = s.fade[static_cast<za::SizeT>(ch)];
            f = Fade{fadeAt(f, t), 0.f, t, t + 0.6f};
        }
    }
    else
    {
        beginFall(s, p, k.out, 0u, 0.f, false, c.now);
    }
}

} // namespace

namespace detail
{

void climbFrame(State& s, const Parts& p, const Context& c, float L)
{
    Climb& k = s.climb;
    Env& e = s.env;
    const glm::vec3 up{0.f, 0.f, 1.f};
    int holding = 0, reaching = 0;
    glm::vec3 sum{0.f};
    for(const EnvHand& h : e.hand)
    {
        if(h.mode == Grip::Ledge)
        {
            if(B3_IS_NON_NULL(h.joint))
            {
                holding++;
                sum += h.goal;
            }
            else
            {
                reaching++;
            }
        }
    }
    if(holding + reaching == 0 && !k.released)
    {
        endClimb(s, p, c, false, "lost its hold");
        return;
    }
    if(holding > 0)
    {
        k.lip = sum / static_cast<float>(holding);
    }
    const int chest = chestOf(p);
    const glm::vec3 chestAt = glmv(b3Body_GetWorldCenter(p.body[chest])), pelvis = glmv(b3Body_GetWorldCenter(p.body[0]));
    k.rose = za::max(k.rose, chestAt.z - k.top);
    const float pelvisIn = -glm::dot(pelvis - k.lip, k.out);
    if(c.trace)
    {
        // (vr_ragdoll_active_trace: its phase; its chest's and pelvis's height over the top and how far in from the lip,
        // units; the hands holding; the pelvis from its pin's anchor, units.)
        Con_Printf("ragclimb %d %.3f ph %d chest %.1f %.1f pelvis %.1f %.1f h %d pin %.1f\n", c.num,
            static_cast<float>(c.now - c.born), static_cast<int>(k.phase), (chestAt.z - k.top) * p.m2u,
            -glm::dot(chestAt - k.lip, k.out) * p.m2u, (pelvis.z - k.top) * p.m2u, pelvisIn * p.m2u, holding,
            k.pinned ? glm::distance(pelvis, k.pinAt) * p.m2u : -1.f);
    }
    const glm::vec3 wall = -k.out;
    const auto phaseTo = [&](ClimbPhase ph) {
        k.phase = ph;
        k.since = c.now;
        k.from = pelvis;
        k.prog = k.prog2 = 0.f;
        if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
        {
            Con_Printf("climb: %d %s at %.2f s: its chest %.0f units %s the lip, %.0f off the wall\n", c.num,
                ph == ClimbPhase::Pull ? "pulls itself up" : "goes over the lip", c.now - k.since0,
                za::abs(chestAt.z - k.top) * p.m2u, chestAt.z < k.top ? "under" : "over", glm::dot(chestAt - k.lip, k.out) * p.m2u);
        }
    };
    // Its way's points (its pelvis's: its body's origin), off the lip: under it (its chest over the top), over it, in on
    // the top; its pelvis pitched forwards (rad) over the lip and in.
    const glm::vec3 under = k.lip + k.out * (0.5f * L) - up * (0.35f * L);
    const glm::vec3 over = k.lip + k.out * (0.35f * L) + up * (0.6f * L);
    const glm::vec3 in = k.lip + wall * (1.f * L) + up * (0.3f * L);
    const glm::vec3 axis = glm::normalize(glm::cross(up, wall));
    const float lag = k.pinned ? glm::distance(pelvis, k.pinAt) : 0.f; // (its pelvis behind its way)
    switch(k.phase)
    {
        case ClimbPhase::Hang:
        {
            if(holding > 0 && k.pullFrom <= 0.0)
            {
                k.pullFrom = c.now + hangBefore * (1.f + roll(e.seed, 52u));
            }
            // Facing the wall (its chest's front towards it), gently.
            const glm::vec3 ahead = level(bodyRot(p, chest) * glm::vec3{1.f, 0.f, 0.f});
            if(holding > 0 && glm::length(ahead) > 0.3f)
            {
                const glm::vec3 a = glm::normalize(ahead);
                const float off = za::atan2(glm::cross(a, wall).z, glm::dot(a, wall));
                turnWhole(p, c, glm::normalize(glm::angleAxis(off, up) * bodyRot(p, 0)), 2.f, 1.5f, 0.6f);
            }
            // Hanging by one hand: the other reaches up for the lip beside it (again each half second), its pull waiting
            // for it a little (a one-handed hang swings off its middle, the pull failing).
            if(k.pulls && holding == 1 && reaching == 0 && c.now >= k.reachAgain && k.retries < 2)
            {
                const int held = B3_IS_NON_NULL(e.hand[0].joint) ? 0 : 1, free = 1 - held;
                EnvHand& h = e.hand[static_cast<za::SizeT>(free)];
                if(limbWhole(s, p, free) && h.mode == Grip::None)
                {
                    const glm::vec3 along = glm::normalize(glm::cross(up, k.out));
                    const glm::vec3 shoulder = shoulderOf(s, p, free);
                    const glm::vec3 at = k.lip + along * za::clamp(glm::dot(shoulder - k.lip, along), -0.5f * L, 0.5f * L);
                    h.mode = Grip::Ledge;
                    h.goal = at;
                    h.normal = up;
                    h.on = e.hand[static_cast<za::SizeT>(held)].on;
                    h.since = c.now;
                    h.touched = -1.0;
                    h.want = 1.f;
                    h.strength = 1.f;
                    h.holdUntil = c.now + 30.0;
                    k.pullFrom = za::max(k.pullFrom, c.now + 0.4);
                }
                k.reachAgain = c.now + 0.5;
                k.retries++;
            }
            // (Not hanging (its legs still on the top, its back over the lip): it doesn't pull, it drops after its hang.)
            if(k.pulls && holding > 0 && k.pullFrom > 0.0 && c.now >= k.pullFrom && pelvis.z > k.top - 0.25f * L &&
                pelvisIn > 0.f)
            {
                endClimb(s, p, c, true, "on the top already"); // (it caught itself toppling over the lip)
                return;
            }
            if(k.pulls && holding > 0 && k.pullFrom > 0.0 && c.now >= k.pullFrom && pelvis.z > k.top - 0.25f * L)
            {
                k.pulls = false;
                for(EnvHand& h : e.hand)
                {
                    h.holdUntil = za::min(h.holdUntil, c.now + 0.6);
                }
                if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
                {
                    Con_Printf("climb: %d not hanging (its pelvis %.0f units under the lip): it lets go\n", c.num,
                        (k.top - pelvis.z) * p.m2u);
                }
            }
            if(k.pulls && holding > 0 && k.pullFrom > 0.0 && c.now >= k.pullFrom)
            {
                for(EnvHand& h : e.hand)
                {
                    h.holdUntil = za::max(h.holdUntil, c.now + 30.0); // (held until it goes in)
                }
                // Its pin from where its pelvis is, turned as it is, towards facing the wall upright.
                const float t = static_cast<float>(c.now - s.begun);
                s.turn = glm::angleAxis(za::atan2(wall.y, wall.x), up);
                k.pinned = true;
                k.pinAt = pelvis;
                k.rot0 = bodyRot(p, 0);
                k.tilt = glm::quat{1.f, 0.f, 0.f, 0.f};
                k.turnBlend = 0.f;
                dropPin(s.pin);
                s.pin.force = climbPin;
                s.pin.fade = Fade{1.f, 1.f, t, t};
                s.pin.hertz = climbHertz;
                s.pin.turnHertz = -1.f;
                s.pin.turnShare = 0.06f; // (gently: held by a hand off its middle, a strong turn would wrench it about the hand)
                s.support = Fade{climbLift, climbLift, t, t}; // (its muscles hold its weight: the pin guides it)
                s.fade[static_cast<za::SizeT>(Channel::Arms)] = Fade{0.9f, climbArms, t, t + 0.2f};
                phaseTo(ClimbPhase::Pull);
            }
            break;
        }
        case ClimbPhase::Pull:
        {
            // (Its way goes on as fast as it keeps up: a quarter as fast while its pelvis lags past climbLag.)
            k.prog = za::min(1.f, k.prog + c.dt / pullTime * (lag < climbLag * L ? 1.f : 0.25f));
            const float u = smooth01(k.prog);
            k.pinAt = glm::mix(k.from, under, u);
            climbChest(s, p, c, chest, glm::mix(chestAt, k.lip + up * (0.3f * L) + k.out * (0.25f * L), u)); // (off the wall)
            k.turnBlend = u;
            if(k.prog >= 1.f && (glm::distance(pelvis, under) < climbLag * L || pelvis.z > under.z - 0.2f * L))
            {
                phaseTo(ClimbPhase::Over);
            }
            else if(c.now - k.since > pullTime + climbSlack)
            {
                endClimb(s, p, c, false, "too weak to rise");
            }
            break;
        }
        case ClimbPhase::Over:
        {
            // Up over the lip (its hands holding), then (its pelvis over the top) in onto the top, its hands let go,
            // pitched forwards as it goes. Its way goes on as fast as it keeps up (as Pull's).
            const float pace = lag < climbLag * L ? 1.f : 0.25f;
            if(!k.released)
            {
                k.prog = za::min(1.f, k.prog + c.dt / overUp * pace);
            }
            else
            {
                k.prog2 = za::min(1.f, k.prog2 + c.dt / overIn * pace);
            }
            const float u1 = smooth01(k.prog), u2 = smooth01(k.prog2);
            k.pinAt = !k.released ? glm::mix(k.from, over, u1) : glm::mix(over, in, u2);
            // (Its chest led in over the top ahead of its pelvis, not left at the lip's corner as its pelvis rises past it:
            // folded there, it tipped back off.)
            climbChest(s, p, c, chest, k.lip + up * (0.45f * L) + wall * ((!k.released ? 0.3f * u1 : 0.3f + 0.7f * u2) * L));
            k.tilt = glm::angleAxis(0.6f * u1 + 0.9f * u2, axis);
            // (Let go once its pelvis is over the top and its chest over the lip, not tipped back off it.)
            if(!k.released && k.prog >= 1.f && pelvis.z > k.top + 0.1f * L && -glm::dot(chestAt - k.lip, k.out) > -0.1f * L &&
                chestAt.z > k.top - 0.1f * L)
            {
                k.released = true;
                k.since2 = c.now;
                const float t = static_cast<float>(c.now - s.begun);
                s.support = Fade{climbLift, climbLift, t, t}; // (held up until it is in: endClimb lets it down)
                for(int arm = 0; arm < 2; arm++)
                {
                    letGo(s, arm, &c, "going in onto the top");
                }
            }
            // Its legs up onto the top (one leading, by its seed; the other as it goes in), drawn in after its pelvis (their
            // thighs not caught on the lip).
            const int leadLeg = roll(e.seed, 53u) < 0.5f ? 2 : 3;
            for(int leg = 2; leg < 4; leg++)
            {
                Reach& r = s.reach[static_cast<za::SizeT>(leg)];
                const float w = leg == leadLeg ? smooth01((k.prog - 0.4f) / 0.6f) : u2;
                r.goal = k.lip + wall * ((0.15f + 0.5f * u2) * L) + up * (0.1f * L);
                r.weight = limbWhole(s, p, leg) ? 0.8f * w : 0.f;
                r.strength = 0.8f;
            }
            if(k.released && k.prog2 >= 1.f && pelvisIn > 0.5f * L && pelvis.z > k.top - 0.1f * L)
            {
                endClimb(s, p, c, true, "in on the top");
            }
            else if(!k.released && c.now - k.since > overUp + climbSlack)
            {
                endClimb(s, p, c, false, "stuck at the lip");
            }
            else if(k.released && c.now - k.since2 > overIn + climbSlack)
            {
                endClimb(s, p, c, pelvisIn > 0.35f * L, pelvisIn > 0.35f * L ? "slowly in" : "slid back off the lip");
            }
            break;
        }
        default: break;
    }
}

} // namespace detail

bool climbing(const State& s)
{
    return s.on && s.climb.on;
}
bool beginClimb(State& s, const Parts& p, const Context& c)
{
    if(!p.rig || s.death.on || s.climb.on)
    {
        return false;
    }
    livingLedge(s, p, c);
    return s.climb.on;
}
} // namespace qvr::ragactive
