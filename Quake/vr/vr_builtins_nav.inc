// vr_builtins_nav.inc -- PF_ builtins: the grappling rope, perception through teleporters, navwalk/navplace.
// Split from vr_builtins.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

// The grappling hook's physical rope (vr_ropesim.cpp).
// float ropestep(entity hook, vector gun, vector game, vector end, float length, entity skipA, entity skipB): steps it
// (once a frame), the taut path's length.
void PF_ropestep()
{
    const auto vec = [](int ofs) {
        const float* v = G_VECTOR(ofs);
        return geom::vec(v);
    };
    const int skipA = NUM_FOR_EDICT(G_EDICT(OFS_PARM5));
    const int skipB = NUM_FOR_EDICT(G_EDICT(OFS_PARM6));
    G_FLOAT(OFS_RETURN) =
        ropesim::step(G_EDICT(OFS_PARM0), vec(OFS_PARM1), vec(OFS_PARM2), vec(OFS_PARM3), G_FLOAT(OFS_PARM4), skipA, skipB).path;
}

// vector ropepivot(entity hook, float which): the taut path's first corner from the game's end (0), from the hook (1), or
// (2) its length beyond the first, beyond the second, and in all.
void PF_ropepivot()
{
    const ropesim::Shape& s = ropesim::shape(G_EDICT(OFS_PARM0));
    const int which = static_cast<int>(G_FLOAT(OFS_PARM1));
    const glm::vec3 v = which == 0 ? s.pivotA : which == 1 ? s.pivotB : glm::vec3{s.beyondA, s.beyondB, s.path};
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// vector roperetract(entity hook, vector gun, vector end, float remaining): the quick reel-in along the rope's path: the
// hook's new place, `remaining` units of the path left from the gun (ropesim::retract).
void PF_roperetract()
{
    const auto vec = [](int ofs) {
        const float* v = G_VECTOR(ofs);
        return geom::vec(v);
    };
    const glm::vec3 p = ropesim::retract(G_EDICT(OFS_PARM0), vec(OFS_PARM1), vec(OFS_PARM2), G_FLOAT(OFS_PARM3));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = p.x;
    out[1] = p.y;
    out[2] = p.z;
}

// void ropesend(entity hook, entity owner, float beamId): its points to the clients, for that beam.
void PF_ropesend()
{
    ropesim::send(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)));
}

// void ropeend(entity owner, float beamId): that beam's rope ended (its hook's rope is another beam's now).
void PF_ropeend()
{
    ropesim::sendEnded(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1)));
}

void PF_carryreach()
{
    // The hand's place (the point given, OFS_PARM1) and angles are the `self` player's move's.
    G_FLOAT(OFS_RETURN) =
        carry2h::reaches(G_EDICT(OFS_PARM0), PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM2))) ? 1.f : 0.f;
}

// handimpact(hand, strength, dir): knock the `self` player's drawn hand (a parried blow).
void PF_handimpact()
{
    server::sendHandImpact(PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1),
        G_VECTOR(OFS_PARM2));
}

// catchblend(hand, e, org, ang): the `self` player's `hand` has just caught `e` (a force grab), which was at `org`
// turned `ang`: its client eases the weapon from there into the hand (vr_drawblend.cpp).
void PF_catchblend()
{
    server::sendCatchBlend(PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM0)),
        NUM_FOR_EDICT(G_EDICT(OFS_PARM1)), G_VECTOR(OFS_PARM2), G_VECTOR(OFS_PARM3));
}

// ejectcasings(hand, kind, count, delay, flags): spent casings out of the `self` player's weapon.
void PF_ejectcasings()
{
    server::sendEject(PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM0)),
        static_cast<int>(G_FLOAT(OFS_PARM1)), static_cast<int>(G_FLOAT(OFS_PARM2)), static_cast<int>(G_FLOAT(OFS_PARM4)),
        G_FLOAT(OFS_PARM3));
}

// weaponfired(shooter, hand): a weapon fired (vr_weaponfx.cpp: the drawn kick, the muzzle flash); hand -1 a monster's.
void PF_weaponfired()
{
    server::sendFired(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1)));
}

// tracer(shooter, hand, from, to): a hitscan pellet's line, for the clients' bullet tracers (vr_weaponfx.cpp).
void PF_tracer()
{
    server::sendTracer(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1)), G_VECTOR(OFS_PARM2), G_VECTOR(OFS_PARM3));
}

// Ranged enemy perception through one active teleporter.
static glm::vec3 aiVec(const float* v) { return {v[0], v[1], v[2]}; }

void PF_portal_ai_sight()
{
    glm::vec3 image;
    G_FLOAT(OFS_RETURN) = static_cast<float>(portals::aiImage(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1),
        aiVec(G_VECTOR(OFS_PARM2)), aiVec(G_VECTOR(OFS_PARM3)), static_cast<int>(G_FLOAT(OFS_PARM4)), image));
}

void PF_portal_ai_map()
{
    const auto point = portals::aiMap(static_cast<int>(G_FLOAT(OFS_PARM0)), aiVec(G_VECTOR(OFS_PARM1)), G_FLOAT(OFS_PARM2) != 0.f);
    VectorCopy(&point.x, G_VECTOR(OFS_RETURN));
}

// portal_ai_gateinfo(gate, what): 0 the gates' count (0: off), 1 the gate's flags, 2 its face (portals::aiGateFlags).
void PF_portal_ai_gateinfo()
{
    const int gate = static_cast<int>(G_FLOAT(OFS_PARM0));
    const int what = static_cast<int>(G_FLOAT(OFS_PARM1));
    const int r = what == 0 ? portals::aiGateCount() : what == 1 ? portals::aiGateFlags(gate) : portals::aiGateFace(gate);
    G_FLOAT(OFS_RETURN) = static_cast<float>(r);
}

// portal_ai_gate(gate, what, p): its face's middle, normal, p held to its aperture, p carried through (aiGateVec).
void PF_portal_ai_gate()
{
    const auto v = portals::aiGateVec(static_cast<int>(G_FLOAT(OFS_PARM0)), static_cast<int>(G_FLOAT(OFS_PARM1)),
        aiVec(G_VECTOR(OFS_PARM2)));
    VectorCopy(&v.x, G_VECTOR(OFS_RETURN));
}

void PF_portal_ai_client()
{
    edict_t* observer = G_EDICT(OFS_PARM0);
    edict_t* result = qcvm->edicts;
    // Rotate candidates independently of checkclient's ordinary-room PVS.
    const int first = svs.maxclients > 0 ? static_cast<int>(qcvm->time * 10) % svs.maxclients : 0;
    for(int n = 0; n < svs.maxclients; n++)
    {
        edict_t* target = EDICT_NUM(1 + (first + n) % svs.maxclients);
        if(target->free || target->v.health <= 0.f || (static_cast<int>(target->v.flags) & FL_NOTARGET)) { continue; }
        glm::vec3 image;
        if(portals::aiImage(observer, target, aiVec(observer->v.origin) + aiVec(observer->v.view_ofs),
            aiVec(target->v.origin) + aiVec(target->v.view_ofs), 0, image)) { result = target; break; }
    }
    G_INT(OFS_RETURN) = EDICT_TO_PROG(result);
}

void PF_bodyshock()
{
    edict_t* target = G_EDICT(OFS_PARM0);
    server::sendShock(PROG_TO_EDICT(pr_global_struct->self), 3, target->v.origin,
        static_cast<float>(NUM_FOR_EDICT(target)), G_FLOAT(OFS_PARM1));
}

// bodyshockdeath(target, duration): lightning's lasting shock on a body it killed or struck (QC vr_shock.qc): Quad's arcs
// crawling over it for `duration` s, fading (vr_shock.cpp, KindBodyDeath).
// collectfx(hand, e, hotspot, modelindex, origin, angles) (QC VR_CollectFx_Send; self is the player): the thing `e` that
// hand put away at `hotspot` (a holster's, a pouch's), its model and pose as it was before it was taken: that player's
// client draws it going in (vr_collectfx.cpp). Visual only.
// Into a gun lying about (collectfx::intoPropHotspot, `e` the gun): to every client that can see the gun instead, self
// or not (a loose round by contact has no player).
void PF_collectfx()
{
    edict_t* e = G_EDICT(OFS_PARM1);
    const int hotspot = static_cast<int>(G_FLOAT(OFS_PARM2));
    if(hotspot == collectfx::intoPropHotspot)
    {
        server::sendCollectSeen(e, static_cast<int>(G_FLOAT(OFS_PARM0)), hotspot, NUM_FOR_EDICT(e),
            static_cast<int>(G_FLOAT(OFS_PARM3)), G_VECTOR(OFS_PARM4), G_VECTOR(OFS_PARM5));
        return;
    }
    server::sendCollect(PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM0)), hotspot,
        NUM_FOR_EDICT(e), static_cast<int>(G_FLOAT(OFS_PARM3)), G_VECTOR(OFS_PARM4), G_VECTOR(OFS_PARM5));
}

void PF_bodyshockdeath()
{
    edict_t* target = G_EDICT(OFS_PARM0);
    server::sendShock(PROG_TO_EDICT(pr_global_struct->self), 4, target->v.origin,
        static_cast<float>(NUM_FOR_EDICT(target)), G_FLOAT(OFS_PARM1));
}

// bodysmoulder(target, left): `target`'s fire goes out in `left` s (0: it has just gone out), and it smokes while it burns
// and a while after (QC vr_burning.qc; vr_smoulder.cpp); `left` below 0: put out in a liquid, it stops smoking.
void PF_bodysmoulder()
{
    edict_t* target = G_EDICT(OFS_PARM0);
    const float left = G_FLOAT(OFS_PARM1);
    server::sendShock(PROG_TO_EDICT(pr_global_struct->self), left < 0.f ? 6 : 5, target->v.origin,
        static_cast<float>(NUM_FOR_EDICT(target)), left < 0.f ? 0.f : left);
}

extern "C" void VR_PortalCarry(edict_t* box, edict_t* player, int hand, int begin);
void PF_portal_carry()
{
    VR_PortalCarry(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)), G_FLOAT(OFS_PARM3) != 0.f);
}

// playershock(player, damage, at): `player` struck by lightning (a shambler's bolt, another's lightning gun, a trap's, the
// water's shock spreading) for `damage` at `at`: its client draws arcs over its arms, hands and body a while
// (vr_shock.cpp, KindSelfHit; how long: vr_shock_self_time, by the damage).
void PF_playershock()
{
    server::sendShock(G_EDICT(OFS_PARM0), shock::KindSelfHit, G_VECTOR(OFS_PARM2), G_FLOAT(OFS_PARM1), 0.f);
}

// viewmessage(player, text, seconds): `text` in the middle of `player`'s view for `seconds` (vr_server.hpp).
void PF_viewmessage()
{
    server::sendViewMessage(G_EDICT(OFS_PARM0), G_STRING(OFS_PARM1), G_FLOAT(OFS_PARM2));
}

// ----------------------------------------------------------------------------
// navwalk(start, grid, most) / navplace(i): the places the player can walk to from `start` (Survival Arena's supplies,
// QC vr_arena_supply.qc): a walk out over a lattice `grid` apart, every move between two cells taken the way he walks
// (his box, a step up of navStepUp at most and down as much, never off a ledge, never onto a slope too steep to stand
// on), a narrow way (a bridge, a stair) between two cells looked for navAside to either side. The QC keeps none of it:
// its globals are near the id format's limit.

struct NavWalk
{
    struct Place
    {
        float o[3]; // the player's origin standing there
    };
    za::Vector<Place> places;
    za::Vector<int> slots; // a lattice cell's place + 1 (0: none), open addressing
    float org[3]{};
    float grid{48.f};
};
NavWalk navWalk;

constexpr float navStepUp = 18.f; // sv_move's STEPSIZE
constexpr float navSub = 16.f;    // a move walked in steps this long (a stair's tread)
constexpr float navLevel = 48.f;  // two places in one cell this far apart in height are two (a deck over a floor)
constexpr float navAside = 16.f;
constexpr float navFloorNormal = 0.7f;

trace_t navTrace(const float* from, const float* to)
{
    vec3_t a{from[0], from[1], from[2]}, b{to[0], to[1], to[2]};
    vec3_t mins{-16.f, -16.f, -24.f}, maxs{16.f, 16.f, 32.f};
    return SV_Move(a, mins, maxs, b, MOVE_NOMONSTERS, qcvm->edicts);
}

bool navKey(const float* o, za::U64& key)
{
    const long long ix = static_cast<long long>(floorf((o[0] - navWalk.org[0]) / navWalk.grid + 0.5f)) + 32768;
    const long long iy = static_cast<long long>(floorf((o[1] - navWalk.org[1]) / navWalk.grid + 0.5f)) + 32768;
    const long long iz = static_cast<long long>(floorf((o[2] - navWalk.org[2]) / navLevel + 0.5f)) + 32768;
    if(ix < 0 || ix > 65535 || iy < 0 || iy > 65535 || iz < 0 || iz > 65535)
    {
        return false;
    }
    key = (static_cast<za::U64>(ix) << 32) | (static_cast<za::U64>(iy) << 16) | static_cast<za::U64>(iz);
    return true;
}

// The slot of o's cell: holding its place, or the empty one it would take (-1: off the lattice).
int navSlot(const float* o)
{
    za::U64 key = 0;
    if(!navKey(o, key))
    {
        return -1;
    }
    const za::SizeT mask = navWalk.slots.size() - 1;
    za::U64 h = key * 0x9E3779B97F4A7C15ull;
    za::SizeT s = static_cast<za::SizeT>(h >> 40) & mask;
    for(za::SizeT k = 0; k <= mask; k++)
    {
        const int v = navWalk.slots[s];
        if(v == 0)
        {
            return static_cast<int>(s);
        }
        za::U64 other = 0;
        if(navKey(navWalk.places[static_cast<za::SizeT>(v - 1)].o, other) && other == key)
        {
            return static_cast<int>(s);
        }
        s = (s + 1) & mask;
    }
    return -1;
}

bool navAdd(const float* o, za::SizeT most)
{
    if(navWalk.places.size() >= most)
    {
        return false;
    }
    const int s = navSlot(o);
    if(s < 0 || navWalk.slots[static_cast<za::SizeT>(s)] != 0)
    {
        return false;
    }
    navWalk.places.pushBack(NavWalk::Place{{o[0], o[1], o[2]}});
    navWalk.slots[static_cast<za::SizeT>(s)] = static_cast<int>(navWalk.places.size());
    return true;
}

// The player walking from `a` to `b`'s x and y; true: `end` is where he stands.
bool navMove(const float* a, const float* b, float* end)
{
    const float dx = b[0] - a[0], dy = b[1] - a[1];
    const int steps = za::max(1, static_cast<int>(ceilf(sqrtf(dx * dx + dy * dy) / navSub)));
    float cur[3] = {a[0], a[1], a[2]};
    for(int i = 1; i <= steps; i++)
    {
        const float up[3] = {cur[0], cur[1], cur[2] + navStepUp};
        trace_t tr = navTrace(cur, up);
        if(tr.startsolid)
        {
            return false;
        }
        const float top[3] = {tr.endpos[0], tr.endpos[1], tr.endpos[2]};
        const float f = static_cast<float>(i) / static_cast<float>(steps);
        const float to[3] = {a[0] + dx * f, a[1] + dy * f, top[2]};
        tr = navTrace(top, to);
        if(tr.fraction < 1.f || tr.startsolid)
        {
            return false;
        }
        const float down[3] = {to[0], to[1], cur[2] - navStepUp};
        tr = navTrace(to, down);
        if(tr.fraction >= 1.f || tr.startsolid || tr.allsolid || tr.plane.normal[2] < navFloorNormal)
        {
            return false; // (a ledge, a hole, a slope too steep)
        }
        cur[0] = tr.endpos[0];
        cur[1] = tr.endpos[1];
        cur[2] = tr.endpos[2];
    }
    end[0] = cur[0];
    end[1] = cur[1];
    end[2] = cur[2];
    return true;
}

// The walk from `start` (a standing origin, the first place): how many places.
int navWalkFrom(const float* start, float grid, int mostIn)
{
    const double t0 = Sys_DoubleTime();
    navWalk.grid = za::max(16.f, grid);
    const za::SizeT most = static_cast<za::SizeT>(CLAMP(1, mostIn, 65536));
    za::SizeT tableSize = 16;
    while(tableSize < most * 2)
    {
        tableSize *= 2;
    }
    navWalk.places.clear();
    navWalk.slots.clear();
    navWalk.slots.resize(tableSize, 0);
    navWalk.org[0] = start[0];
    navWalk.org[1] = start[1];
    navWalk.org[2] = start[2];
    navAdd(start, most);

    int moves = 0;
    for(za::SizeT head = 0; head < navWalk.places.size(); head++)
    {
        const NavWalk::Place here = navWalk.places[head];
        const float ix = floorf((here.o[0] - navWalk.org[0]) / navWalk.grid + 0.5f);
        const float iy = floorf((here.o[1] - navWalk.org[1]) / navWalk.grid + 0.5f);
        for(int dx = -1; dx <= 1; dx++)
        {
            for(int dy = -1; dy <= 1; dy++)
            {
                if(dx == 0 && dy == 0)
                {
                    continue;
                }
                const float c[3] = {navWalk.org[0] + (ix + static_cast<float>(dx)) * navWalk.grid,
                    navWalk.org[1] + (iy + static_cast<float>(dy)) * navWalk.grid, here.o[2]};
                const int s = navSlot(c);
                if(s < 0 || navWalk.slots[static_cast<za::SizeT>(s)] != 0)
                {
                    continue; // (walked to already at this height, or off the lattice)
                }
                const bool straight = dx == 0 || dy == 0;
                for(int t = 0; t < (straight ? 3 : 1); t++)
                {
                    const float side = t == 0 ? 0.f : (t == 1 ? navAside : -navAside);
                    const float to[3] = {c[0] + (dx == 0 ? side : 0.f), c[1] + (dx != 0 ? side : 0.f), c[2]};
                    float end[3];
                    moves++;
                    if(navMove(here.o, to, end))
                    {
                        navAdd(end, most);
                        break;
                    }
                }
            }
        }
    }
    Con_DPrintf("navwalk: %d places from %.0f %.0f %.0f (grid %.0f, %d moves) in %.1f ms\n",
        static_cast<int>(navWalk.places.size()), start[0], start[1], start[2], navWalk.grid, moves,
        (Sys_DoubleTime() - t0) * 1000.0);
    return static_cast<int>(navWalk.places.size());
}

// vector navplace(float i, vector start, float grid, float most): i < 0 walks from `start` (the places found in its
// x); else the last walk's place i ('0 0 0' past its last). One builtin for both: the VR builtins' range is full.
void PF_navplace()
{
    const int i = static_cast<int>(G_FLOAT(OFS_PARM0));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = out[1] = out[2] = 0.f;
    if(i < 0)
    {
        out[0] = static_cast<float>(navWalkFrom(G_VECTOR(OFS_PARM1), G_FLOAT(OFS_PARM2),
            static_cast<int>(G_FLOAT(OFS_PARM3))));
        return;
    }
    if(static_cast<za::SizeT>(i) < navWalk.places.size())
    {
        const NavWalk::Place& p = navWalk.places[static_cast<za::SizeT>(i)];
        out[0] = p.o[0];
        out[1] = p.o[1];
        out[2] = p.o[2];
    }
}

void PF_watershock()
{
    server::sendShock(PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM0)), G_VECTOR(OFS_PARM1),
        G_FLOAT(OFS_PARM2), G_FLOAT(OFS_PARM3));
}
