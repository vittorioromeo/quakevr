// vr_builtins_melee.inc -- PF_ builtins: wounds, gore sounds, hit reactions, flame contact, the melee history and its iterators.
// Split from vr_builtins.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

// A wound on a model (vr_wounds.cpp): void(entity e, vector org, vector dir, float kind, float amount, float extra)
// woundevent. `e` hit at `org` going `dir` (unit), QVR_WOUND_* `kind`, `amount` (damage, 0..255), `extra` (pellets;
// a liquid's: its surface's height is org_z). Unreliable, to every client, as particles.
// The entities sent a wound this map: their removal is sent (onEdictFree).
za::Vector<za::U8> woundsSent;

void sendWoundEvent(int num, const float* org, const float* dir, int kind, int amount, int extra); // (below)

void PF_woundevent()
{
    sendWoundEvent(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), G_VECTOR(OFS_PARM1), G_VECTOR(OFS_PARM2),
        static_cast<int>(G_FLOAT(OFS_PARM3)), static_cast<int>(G_FLOAT(OFS_PARM4) + 0.5f), static_cast<int>(G_FLOAT(OFS_PARM5)));
}

void sendWoundEvent(int num, const float* org, const float* dir, int kind, int amount, int extra)
{
    if(sv.state != ss_active || sv.datagram.cursize > MAX_DATAGRAM - 32)
    {
        return;
    }

    MSG_WriteByte(&sv.datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.datagram, protocol::QVR_SVC_WOUND);
    MSG_WriteShort(&sv.datagram, num);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.datagram, org[i], sv.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteChar(&sv.datagram, CLAMP(-127, static_cast<int>(dir[i] * 127.f), 127));
    }
    MSG_WriteByte(&sv.datagram, CLAMP(0, kind, 255));
    MSG_WriteByte(&sv.datagram, CLAMP(0, amount, 255));
    MSG_WriteByte(&sv.datagram, CLAMP(0, extra, 255));
    VR_BroadcastMessageEnd(); // a boundary (vr_server.cpp)
    if(num >= static_cast<int>(woundsSent.size()))
    {
        woundsSent.resize(static_cast<za::SizeT>(num) + 1, 0);
    }
    woundsSent[static_cast<za::SizeT>(num)] = 1;
}

// A gore sound (vr_goresound.cpp): float(float kind, entity e, vector at, float strength) goresound. `kind` QVR_GORESND_*,
// `e` its entity (a choke's: the monster, whose cry is dropped for a moment), `strength` 0..1. Unreliable, to every
// client. 1: sent; 0: not (vr_gore 0, vr_gore_sound 0, a choke too soon).
void PF_goresound()
{
    const int kind = static_cast<int>(G_FLOAT(OFS_PARM0));
    const int num = NUM_FOR_EDICT(G_EDICT(OFS_PARM1));
    G_FLOAT(OFS_RETURN) = kind >= 0 && kind < static_cast<int>(goresound::Kind::Count) &&
                                  goresound::send(static_cast<goresound::Kind>(kind), num, G_VECTOR(OFS_PARM2), G_FLOAT(OFS_PARM3))
                              ? 1.f
                              : 0.f;
}

// A living monster struck (vr_hitreact.cpp: its hit reaction drawn): void(entity e, vector at, vector dir, float part,
// float damage, float share) hitreact. `at` the point struck, `dir` the push (unit), `part` PositionalDamage's (0 body,
// 1 head, 2 arm, 3 leg; -1 unknown), `damage` (0..255), `share` the share of its health the blow took (0..1).
// Unreliable, to every client.
void PF_hitreact()
{
    const int num = NUM_FOR_EDICT(G_EDICT(OFS_PARM0));
    const float* at = G_VECTOR(OFS_PARM1);
    const float* dir = G_VECTOR(OFS_PARM2);
    const int part = static_cast<int>(G_FLOAT(OFS_PARM3));
    const int damage = static_cast<int>(G_FLOAT(OFS_PARM4) + 0.5f);
    const float share = G_FLOAT(OFS_PARM5);

    if(sv.state != ss_active || sv.datagram.cursize > MAX_DATAGRAM - 32)
    {
        return;
    }

    MSG_WriteByte(&sv.datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.datagram, protocol::QVR_SVC_HITREACT);
    MSG_WriteShort(&sv.datagram, num);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.datagram, at[i], sv.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteChar(&sv.datagram, CLAMP(-127, static_cast<int>(dir[i] * 127.f), 127));
    }
    MSG_WriteByte(&sv.datagram, CLAMP(0, part + 1, 255));
    MSG_WriteByte(&sv.datagram, CLAMP(0, damage, 255));
    MSG_WriteByte(&sv.datagram, CLAMP(0, static_cast<int>(share * 255.f + 0.5f), 255));
    VR_BroadcastMessageEnd(); // a boundary (vr_server.cpp)
}

// torchflametouch(t): what of you the flame of torch `t`, held by you, touches (single player: the client's last
// frame; vr_walltorch.cpp walltorch::flameOnYou): bits 1 the hand holding it, 2 the other hand, 4 the holding arm, 8 the
// other arm, 16 the torso or legs, 32 the head, and 256 times the deepest one's bit (but the holding hand's); 0 nothing
// (or neither vr_burn_self nor vr_burn_drop on). trace_endpos:
// where (the deepest contact but the holding hand's, that one if alone), on the body; trace_plane_normal: the way out.
void PF_torchflametouch()
{
    glm::vec3 at{0.f}, out{0.f, 0.f, 1.f};
    const unsigned parts = walltorch::flameOnYou(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), at, out);
    G_FLOAT(OFS_RETURN) = static_cast<float>(parts);
    if(parts)
    {
        for(int i = 0; i < 3; i++)
        {
            pr_global_struct->trace_endpos[i] = at[i];
            pr_global_struct->trace_plane_normal[i] = out[i];
        }
    }
}

// A visible map flame capsule against the local tracked body, using the held torch contact shapes.
void PF_mapflametouch()
{
    G_FLOAT(OFS_RETURN) = 0.f;
    if(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)) != cl.viewentity || cls.state != ca_connected)
    {
        return;
    }
    selfcollide::keepShapes();
    if(G_FLOAT(OFS_PARM3) < 0.f)
    {
        return; // request shapes between the QC contact polls, even when the body solve is off
    }
    const float* a = G_VECTOR(OFS_PARM1);
    const float* b = G_VECTOR(OFS_PARM2);
    const auto t = selfcollide::flameTouch(geom::vec(a), geom::vec(b),
        za::max(0.f, G_FLOAT(OFS_PARM3)), -1, static_cast<unsigned>(G_FLOAT(OFS_PARM4)) & 63);
    G_FLOAT(OFS_RETURN) = static_cast<float>(t.parts | (t.deepest << 8));
    if(t.parts)
    {
        for(int i = 0; i < 3; i++)
        {
            pr_global_struct->trace_endpos[i] = t.at[i];
            pr_global_struct->trace_plane_normal[i] = t.out[i];
        }
    }
}

// float anglemod(float v): v wrapped into [0, 360) (AngleMod360, mathlib.c). Was QC's (ai.qc), a loop of a step per
// 360 degrees: a hanging pickup's anglemod(100 * time) took one per 3.6 s of level time, every think.
void PF_anglemod()
{
    G_FLOAT(OFS_RETURN) = AngleMod360(G_FLOAT(OFS_PARM0));
}

// The melee history (QC vr_melee.qc VR_Melee_Track): a hand's blow keeps its last VR_MELEE_HISTORY poses' velocities (the
// far end's, the grip's, the wrist's; m/s) and time steps in a ring, the newest at mh_hi; a time step of 0 ends it. The
// builtins below walk it back from the newest pose, as the QC loops they replace did (fteqcc made each `b.mh_hdt[k]` a
// call, a binary search), and do the same float operations in the same order as the VM (pr_exec.c: vectors a component
// at a time, a dot product's terms summed in order; PF_vlen and PF_normalize in double, PF_max), so their results are the
// QC's to the bit (vr_melee_shared.h's constants read as floats, as the QC's are).
struct MeleeRing
{
    edict_t* b;
    const MeleeHistoryFields& f;
    int k; // the pose looked at

    explicit MeleeRing(edict_t* xB) : b{xB}, f{bindings().melee}, k{static_cast<int>(E_FLOAT(xB, bindings().melee.hi))}
    {
        if(k < 0 || k >= VR_MELEE_HISTORY)
        {
            k = 0; // (never: VR_Melee_Track keeps it in the ring)
        }
    }

    [[nodiscard]] float dt() const
    {
        return E_FLOAT(b, f.hdt + k);
    }

    [[nodiscard]] const float* at(int xArray) const // the pose's vector of the ring xArray (f.hfar, f.hgrip, f.hwrist)
    {
        return E_VECTOR(b, xArray + 3 * k);
    }

    void older()
    {
        k = k > 0 ? k - 1 : VR_MELEE_HISTORY - 1;
    }
};

// `to` + `v` * `s`, as QC's `to = to + v * s` (OP_MUL_VF, OP_ADD_V).
void meleeAddScaled(float* to, const float* v, float s)
{
    for(int i = 0; i < 3; i++)
    {
        const float scaled = s * v[i];
        to[i] = to[i] + scaled;
    }
}

[[nodiscard]] float meleeDot(const float* a, const float* b) // QC's `a * b` (OP_MUL_V)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

[[nodiscard]] float meleeVlen(const float* v) // PF_vlen
{
    return static_cast<float>(sqrt(static_cast<double>(v[0]) * v[0] + static_cast<double>(v[1]) * v[1] +
                                   static_cast<double>(v[2]) * v[2]));
}

// float meleerun(entity b, float xGrip): how far the far end (xGrip: the grip) came towards where it goes now (m), looking
// back VR_MELEE_RUN_TIME at most (builtins.qc).
void PF_meleerun()
{
    G_FLOAT(OFS_RETURN) = 0.f;
    if(!bindings().melee.valid)
    {
        return;
    }
    MeleeRing r{G_EDICT(OFS_PARM0)};
    const bool grip = G_FLOAT(OFS_PARM1) != 0.f;
    const float* way = E_VECTOR(r.b, grip ? r.f.vgrip : r.f.vfar);
    float now[3] = {0.f, 0.f, 0.f}; // normalize(way): PF_normalize
    const double len = sqrt(static_cast<double>(way[0]) * way[0] + static_cast<double>(way[1]) * way[1] +
                            static_cast<double>(way[2]) * way[2]);
    if(len != 0)
    {
        const double inv = 1 / len;
        for(int i = 0; i < 3; i++)
        {
            now[i] = static_cast<float>(way[i] * inv);
        }
    }
    float moved[3] = {0.f, 0.f, 0.f};
    float run = 0.f;
    float back = 0.f;
    for(int n = 0; n < VR_MELEE_HISTORY; ++n)
    {
        const float dt = r.dt();
        if(dt <= 0.f)
        {
            break;
        }
        meleeAddScaled(moved, r.at(grip ? r.f.hgrip : r.f.hfar), dt);
        const float along = meleeDot(moved, now);
        if(run < along) // max(run, along): PF_max
        {
            run = along;
        }
        back = back + dt;
        if(back > static_cast<float>(VR_MELEE_RUN_TIME))
        {
            break;
        }
        r.older();
    }
    G_FLOAT(OFS_RETURN) = run;
}

// float meleewristspeed(entity b): the wrist's net travel over the last VR_MELEE_WRIST_TIME, over that time (m/s).
void PF_meleewristspeed()
{
    G_FLOAT(OFS_RETURN) = 0.f;
    if(!bindings().melee.valid)
    {
        return;
    }
    MeleeRing r{G_EDICT(OFS_PARM0)};
    const float window = static_cast<float>(VR_MELEE_WRIST_TIME);
    float moved[3] = {0.f, 0.f, 0.f};
    float back = 0.f;
    for(int n = 0; n < VR_MELEE_HISTORY; ++n)
    {
        const float dt = r.dt();
        if(dt <= 0.f)
        {
            break;
        }
        meleeAddScaled(moved, r.at(r.f.hwrist), dt);
        back = back + dt;
        if(back >= window)
        {
            break;
        }
        r.older();
    }
    const float over = back < window ? window : back; // max(back, window): PF_max
    G_FLOAT(OFS_RETURN) = meleeVlen(moved) / over;
}

// float meleewiggled(entity b): whether the grip went back the other way at VR_MELEE_WIGGLE_BACK of its speed now or
// more, within VR_MELEE_WIGGLE_TIME.
void PF_meleewiggled()
{
    G_FLOAT(OFS_RETURN) = 0.f;
    if(!bindings().melee.valid)
    {
        return;
    }
    MeleeRing r{G_EDICT(OFS_PARM0)};
    const float* vgrip = E_VECTOR(r.b, r.f.vgrip);
    const float speed = meleeVlen(vgrip);
    if(speed <= 0.f)
    {
        return;
    }
    const float inv = 1.f / speed;
    const float now[3] = {inv * vgrip[0], inv * vgrip[1], inv * vgrip[2]};
    const float against = static_cast<float>(VR_MELEE_WIGGLE_BACK) * speed;
    float back = 0.f;
    for(int n = 0; n < VR_MELEE_HISTORY; ++n)
    {
        const float dt = r.dt();
        if(dt <= 0.f)
        {
            break;
        }
        if(-meleeDot(r.at(r.f.hgrip), now) >= against)
        {
            G_FLOAT(OFS_RETURN) = 1.f;
            return;
        }
        back = back + dt;
        if(back >= static_cast<float>(VR_MELEE_WIGGLE_TIME))
        {
            break;
        }
        r.older();
    }
}

// float meleearc(entity b, float xFloor): heavy handling's deliberate swing (QC VR_Melee_Decide, vr_heavy_swing_*): how far
// (m) the far end has come along a smooth arc just now, looking back through the history while it went at least xFloor
// m/s and its way turned under 45 degrees a 72 Hz tick (arcTurnRate: per second, over the two poses' times, so a pose
// spanning two headset frames may turn twice as far) and under 150 from its way now (no reversal, no jitter, no pause);
// 0 if that lasted under vr_heavy_swing_time.
constexpr float arcTurnRate = 0.7853982f * 72.f; // rad/s (45 degrees a 72 Hz tick)
void PF_meleearc()
{
    G_FLOAT(OFS_RETURN) = 0.f;
    if(!bindings().melee.valid)
    {
        return;
    }
    MeleeRing r{G_EDICT(OFS_PARM0)};
    const float floor = za::max(G_FLOAT(OFS_PARM1), 0.01f);
    float arc = 0.f, time = 0.f;
    float newer[3] = {0.f, 0.f, 0.f}, now[3] = {0.f, 0.f, 0.f};
    const char* why = "the history's start";
    float newerDt = 0.f;
    int n = 0;
    for(; n < VR_MELEE_HISTORY; ++n)
    {
        const float dt = r.dt();
        if(dt <= 0.f)
        {
            break;
        }
        const float* v = r.at(r.f.hfar);
        const float speed = meleeVlen(v);
        if(speed < floor)
        {
            why = "slower";
            break;
        }
        const float dir[3] = {v[0] / speed, v[1] / speed, v[2] / speed};
        // (The two poses' velocities are their spans' means: their middles half of each apart.)
        const float turnMax = za::min(arcTurnRate * 0.5f * (dt + newerDt), 2.9f);
        if(n == 0)
        {
            memcpy(now, dir, sizeof(now));
        }
        else if(meleeDot(dir, newer) < za::cos(turnMax) || meleeDot(dir, now) < -0.866f)
        {
            why = "turned";
            break;
        }
        memcpy(newer, dir, sizeof(newer));
        newerDt = dt;
        arc += speed * dt;
        time += dt;
        r.older();
    }
    G_FLOAT(OFS_RETURN) = time >= vr_heavy_swing_time.value ? arc : 0.f;
    if(developer.value >= 2)
    {
        Con_Printf("meleearc: %.2f m over %.3f s (%d poses, at least %.2f m/s), back to %s\n", arc, time, n, floor, why);
    }
}

// float(string model, optional float kick) weaponheft: heavy handling's weight class of the weapon drawn with `model`
// (weight::heft: 0 light .. 1; 0 for no such weapon or vr_heavy 0). `kick` 1: its heavy recoil fired in one hand instead
// (deg/s its muzzle is flung up: weight::kickSpeed, vr_heavy_recoil*; 0 for a light one or vr_heavy_recoil 0): armed
// monsters' dual-wielded guns kick so (QC vr_armedgrunt.qc VR_AG_ShootProp). (An argument, not a builtin: the table is
// full.)
void PF_weaponheft()
{
    const int slot = weapons::slotForName(G_STRING(OFS_PARM0));
    if(qcvm->argc >= 2 && G_FLOAT(OFS_PARM1) == 1.f)
    {
        G_FLOAT(OFS_RETURN) = slot < 0 ? 0.f : weight::kickSpeed(slot, 0.f);
        return;
    }
    G_FLOAT(OFS_RETURN) = weight::heft(slot);
}

// vector meleegripago(entity b, float xBack): where the grip was xBack s ago (from the head, in the play space, metres;
// as far back as the history goes).
void PF_meleegripago()
{
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = out[1] = out[2] = 0.f;
    if(!bindings().melee.valid)
    {
        return;
    }
    MeleeRing r{G_EDICT(OFS_PARM0)};
    const float within = G_FLOAT(OFS_PARM1);
    float moved[3] = {0.f, 0.f, 0.f};
    float back = 0.f;
    for(int n = 0; n < VR_MELEE_HISTORY; ++n)
    {
        const float dt = r.dt();
        if(dt <= 0.f || back >= within)
        {
            break;
        }
        meleeAddScaled(moved, r.at(r.f.hgrip), dt);
        back = back + dt;
        r.older();
    }
    const float* rgrip = E_VECTOR(r.b, r.f.rgrip);
    for(int i = 0; i < 3; i++)
    {
        out[i] = rgrip[i] - moved[i];
    }
}

// particle2(origin, direction, preset, count): unreliable, like vanilla particle().
void particle2Message(const float* org, const float* dir, int preset, int count)
{
    if(sv.datagram.cursize > MAX_DATAGRAM - 24)
    {
        return;
    }

    MSG_WriteByte(&sv.datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.datagram, protocol::QVR_SVC_PARTICLE2);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.datagram, org[i], sv.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteChar(&sv.datagram, CLAMP(-128, static_cast<int>(dir[i] * 16.f), 127));
    }
    MSG_WriteByte(&sv.datagram, preset);
    MSG_WriteShort(&sv.datagram, count);
    VR_BroadcastMessageEnd(); // a boundary (vr_server.cpp)
}

void PF_particle2()
{
    particle2Message(G_VECTOR(OFS_PARM0), G_VECTOR(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)),
        static_cast<int>(G_FLOAT(OFS_PARM3)));
}

// entity findflags(entity start, .float field, float flags): the next entity after `start` (in edict order, free ones
// skipped) whose `field` has any of `flags` set; world when there is none (DP's extension). The frame's loops over the
// monsters (VR_Liquids_Frame, VR_Wounds_Frame) step through them alone, instead of every entity in the VM.
// findflags()'s answer: the next edict after `from` (in use) with any of `flags` in the float `field`; 0 when none.
[[nodiscard]] int nextFlagged(int from, int field, int flags)
{
    if(qcvm == &sv.qcvm)
    {
        // (the edict index's fields: the same edict as the walk; vr_edictindex.cpp)
        const int found = VR_EdictIndex_FindFlags(from, field, flags);
        if(found >= 0)
        {
            return found;
        }
    }
    for(int i = from + 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(!e->free && (static_cast<int>(E_FLOAT(e, field)) & flags))
        {
            return i;
        }
    }
    return 0;
}

void PF_findflags()
{
    const int from = NUM_FOR_EDICT(G_EDICT(OFS_PARM0));
    const int field = G_INT(OFS_PARM1);
    const int flags = static_cast<int>(G_FLOAT(OFS_PARM2));
    G_INT(OFS_RETURN) = EDICT_TO_PROG(EDICT_NUM(nextFlagged(from, field, flags)));
}

// The same live iterator with the frame loops' pure interval/eligibility checks in native code.
// No snapshot: every call resumes after start, so a due callback may change/remove/spawn later edicts.
// Keep <= (rather than rejecting >) and float truth tests: NaN deadlines are not due; NaN eligibility is true.
void PF_findflagsdue()
{
    int e = NUM_FOR_EDICT(G_EDICT(OFS_PARM0));
    const int field = G_INT(OFS_PARM1);
    const int flags = static_cast<int>(G_FLOAT(OFS_PARM2));
    const int deadline = G_INT(OFS_PARM3);
    const float now = G_FLOAT(OFS_PARM4);
    const int required = G_INT(OFS_PARM5);
    if(field < 0 || field >= qcvm->progs->entityfields || deadline < 0 || deadline >= qcvm->progs->entityfields ||
        required < 0 || required >= qcvm->progs->entityfields)
    {
        PR_RunError("findflagsdue: invalid field offset");
    }
    while((e = nextFlagged(e, field, flags)) != 0)
    {
        const edict_t* ed = EDICT_NUM(e);
        if(E_FLOAT(ed, deadline) <= now && E_FLOAT(ed, required) != 0.f)
        {
            break;
        }
    }
    G_INT(OFS_RETURN) = EDICT_TO_PROG(EDICT_NUM(e));
}

// entity findflagsinview(entity start, .float fld, float flags, vector eye, float range, float mincos): findflags(),
// past the entities whose box's centre ((absmin + absmax) * 0.5) is further from `eye` than `range` (vlen) or less than
// `mincos` ahead of it (normalize(centre - eye) * v_forward): the QuakeC test of the stealth AI's look about
// (VR_Stealth_LookAbout), in QuakeC's own float steps (OP_ADD_V, OP_MUL_VF, OP_SUB_V, PF_vlen, PF_normalize, OP_MUL_V),
// so the same entities pass. Its other tests and the trace stay in QuakeC.
void PF_findflagsinview()
{
    int e = NUM_FOR_EDICT(G_EDICT(OFS_PARM0));
    const int field = G_INT(OFS_PARM1);
    const int flags = static_cast<int>(G_FLOAT(OFS_PARM2));
    const float* eyep = G_VECTOR(OFS_PARM3);
    const float eye[3] = {eyep[0], eyep[1], eyep[2]};
    const float range = G_FLOAT(OFS_PARM4);
    const float minCos = G_FLOAT(OFS_PARM5);
    const float* forward = pr_global_struct->v_forward;
    while((e = nextFlagged(e, field, flags)) != 0)
    {
        const edict_t* ed = EDICT_NUM(e);
        float to[3];
        for(int i = 0; i < 3; i++)
        {
            const float sum = ed->v.absmin[i] + ed->v.absmax[i];
            const float at = 0.5f * sum;
            to[i] = at - eye[i];
        }
        const double len = sqrt(static_cast<double>(to[0]) * to[0] + static_cast<double>(to[1]) * to[1] +
                                static_cast<double>(to[2]) * to[2]);
        if(static_cast<float>(len) > range)
        {
            continue;
        }
        float dir[3] = {0.f, 0.f, 0.f};
        if(len != 0)
        {
            const double inv = 1 / len;
            for(int i = 0; i < 3; i++)
            {
                dir[i] = static_cast<float>(to[i] * inv);
            }
        }
        const float dot = dir[0] * forward[0] + dir[1] * forward[1] + dir[2] * forward[2];
        if(dot < minCos)
        {
            continue;
        }
        break;
    }
    G_INT(OFS_RETURN) = EDICT_TO_PROG(EDICT_NUM(e));
}

// vector liquidentry(vector start, vector end): where the segment first goes into water, slime or
// lava from the open (or out of it: a shot from under water), on the surface; `start` itself if it
// crosses none. Leaves the trace globals alone (a shot's trace is still being used).
void PF_liquidentry()
{
    const float* start = G_VECTOR(OFS_PARM0);
    const float* end = G_VECTOR(OFS_PARM1);
    glm::vec3 at{start[0], start[1], start[2]};
    const bool crossed = physics::liquidEntry(at, {end[0], end[1], end[2]}, at);
    if(developer.value >= 3)
    {
        Con_Printf("liquidentry %.0f %.0f %.0f -> %.0f %.0f %.0f: %s\n", start[0], start[1], start[2], end[0], end[1], end[2],
            crossed ? "crosses" : "no");
    }
    float* out = G_VECTOR(OFS_RETURN);
    for(int i = 0; i < 3; i++)
    {
        out[i] = crossed ? at[i] : start[i];
    }
}

// void watersplash(vector org, vector dir, float strength, float sound): a splash on a liquid's
// surface at `org`, something going `dir` into it `strength` hard (4 a shot .. 50 a body), and its
// sound (QVR_SPLASH_*: none, a shot's plip, a thing's splash by strength).
void PF_watersplash()
{
    const float* org = G_VECTOR(OFS_PARM0);
    const float* dir = G_VECTOR(OFS_PARM1);
    physics::waterSplash({org[0], org[1], org[2]}, {dir[0], dir[1], dir[2]}, G_FLOAT(OFS_PARM2),
        static_cast<physics::SplashSound>(CLAMP(0, static_cast<int>(G_FLOAT(OFS_PARM3)), 2)));
}

// haptic(hand, delay, duration, frequency, amplitude), for the `self` player.
void PF_haptic()
{
    server::sendHaptic(PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM0)),
        G_FLOAT(OFS_PARM1), G_FLOAT(OFS_PARM2), G_FLOAT(OFS_PARM3), G_FLOAT(OFS_PARM4));
}

// carryangles(e, handangles, grab): a held object's angles, turning with the hand (vr_rigid.cpp). At a grab (a regrip:
// VR_Carry_Regrip) its place in the hand is kept as it is now (its .carry_offset set before), and its grip's settings
// still move it from there (vr_grip.hpp).
void PF_carryangles()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const bool grab = static_cast<int>(G_FLOAT(OFS_PARM2)) != 0;
    physics::carryAngles(e, G_VECTOR(OFS_PARM1), grab, G_VECTOR(OFS_RETURN));
    const FieldOffsets& f = fields();
    if(grab && f.carry_offset >= 0)
    {
        grip::serverKeep(e, G_VECTOR(OFS_PARM1), fieldVec(e, f.carry_offset));
    }
}

// vector(entity e, entity player, float hand, float lefthand, vector handangles, vector offset) carrygrip: as `player`'s
// `hand` (cVR_MainHand or cVR_OffHand; lefthand: the left one) at `handangles` takes `e`, whose origin is at `offset` in
// the hand (forward, right, up: where it was taken, as .carry_offset): its place in the hand by its Held Object Offsets'
// grip (vr_grip.hpp): its turn with the hand kept (its angles set now) and its origin's place returned (.carry_offset).
void PF_carrygrip()
{
    const float* o = G_VECTOR(OFS_PARM5);
    const glm::vec3 v = grip::serverTake(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)),
        G_FLOAT(OFS_PARM3) != 0.f, G_VECTOR(OFS_PARM4), geom::vec(o));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// vector(entity e, vector handangles, vector offset) carrypouch: `e`, a hand grenade just taken from the pouch (after
// carrygrip, `offset` its .carry_offset): turned in the hand by vr_grenade_pouch_hold_* (vr_grip.hpp); its angles set now
// and its origin's place returned (.carry_offset).
void PF_carrypouch()
{
    const float* o = G_VECTOR(OFS_PARM2);
    const glm::vec3 v = grip::serverFromPouch(G_EDICT(OFS_PARM0), G_VECTOR(OFS_PARM1), geom::vec(o));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// vector(entity e, vector handangles, vector offset) carryfrontpouch: `e`, a launcher's round just taken from the ammo pouch
// (after carrygrip, `offset` its .carry_offset): as carrypouch, turned in the hand by vr_reload_front_hold_*.
void PF_carryfrontpouch()
{
    const float* o = G_VECTOR(OFS_PARM2);
    const glm::vec3 v = grip::serverFromPouch(G_EDICT(OFS_PARM0), G_VECTOR(OFS_PARM1), geom::vec(o), true);
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// vector(entity e, vector handangles, vector offset) carryplace: each frame `e` is held in one hand: its angles set
// (turning with the hand) and its origin's place in the hand returned (.carry_offset, given as `offset`): placed again
// at once when its Held Object Offsets changed (vr_grip.hpp).
void PF_carryplace()
{
    const float* o = G_VECTOR(OFS_PARM2);
    const glm::vec3 v = grip::serverFrame(G_EDICT(OFS_PARM0), G_VECTOR(OFS_PARM1), geom::vec(o));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// vector(entity e, vector handpos, vector palm) carryfit: how far to move an object a hand grips so
// that it sits against the palm, not sunk into the fist (vr_held.cpp; vr_held_surface_fit).
void PF_carryfit()
{
    const float* h = G_VECTOR(OFS_PARM1);
    const float* p = G_VECTOR(OFS_PARM2);
    const glm::vec3 v = held::surfaceFit(G_EDICT(OFS_PARM0), geom::vec(h), geom::vec(p));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// vector(entity e, entity player, float grab) carry2h: a prop held in both hands (vr_carry2h.cpp): at the second
// hand's grip (grab 1) the grips are kept and its origin returned; after, where it goes (its angles set at once).
void PF_carrybody()
{
    const glm::vec3 target = progs::vecParm(OFS_PARM2);
    edict_t* ent = G_EDICT(OFS_PARM0);
    const glm::vec3 result = carry2h::bodyPlace(ent, G_EDICT(OFS_PARM1), target);
    const int index = static_cast<int>(ent->v.modelindex);
    const bool brush = index > 0 && index < MAX_MODELS && sv.models[index] && sv.models[index]->type == mod_brush;
    const carry2h::Frame at{result, carry2h::fromAngles(ent->v.angles, brush)};
    pr_global_struct->trace_allsolid = carry2h::bodyFits(ent, G_EDICT(OFS_PARM1), at) ? 0.f : 1.f;
    for(int i = 0; i < 3; i++) { G_VECTOR(OFS_RETURN)[i] = result[i]; }
}

void PF_carry2h()
{
    const glm::vec3 v = carry2h::serverPlace(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1), G_FLOAT(OFS_PARM2) != 0.f);
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// float(entity e, entity player) carry2hoff: which hands of `player` have been pulled off `e`, held in both (bit 1 the off
// hand, 2 the main hand; vr_carry2h.cpp detached).
void PF_carry2hoff()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(carry2h::detached(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1)));
}

// carry2hkeep(entity e, entity player, float hand): `e`, held in both of `player`'s hands, is held by `hand` alone from
// now on (the other let go): moved onto that hand's grip (vr_carry2h.cpp keep).
void PF_carry2hkeep()
{
    carry2h::keep(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)));
}

// float(entity e, vector point, float hand) carryreach: whether the `self` player's `hand` can take hold of `e` (its fist
// touches its drawn surface, as a hand touching it could), for a carried one (not solid).
// physicsblast(at, damage): T_RadiusDamage's explosion throws the rigid bodies round it (vr_box3d.cpp; Box3D only).
void PF_physicsblast()
{
    const float* p = G_VECTOR(OFS_PARM0);
    box3d::blast(geom::vec(p), G_FLOAT(OFS_PARM1));
}

// float(entity e, string key) propvalue: a setting of the prop `e` is (Held Object Offsets, vr_props.inc, by its
// model): "mass" its mass in kg (the setting, else Box3D's), "throw" how much of the hand's throw it keeps (the
// setting, else from its mass), any other key its value (its default for a model with no settings; 0 for no key).
void PF_propvalue()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const char* name = G_STRING(OFS_PARM1);
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const int slot = model ? props::slotForModel(model) : -1;
    const props::Key key = props::keyByName(name);
    float out = 0.f;
    if(key == props::Key::Mass)
    {
        out = box3d::propMass(e);
    }
    else if(key == props::Key::Throw)
    {
        out = props::throwScale(slot, box3d::propMass(e));
    }
    else if(key != props::Key::Count && key != props::Key::ID)
    {
        out = props::scaledValue(slot, key); // (a length in its model's axes: times its Size)
    }
    else
    {
        Con_DPrintf("propvalue: no key \"%s\"\n", name);
    }
    G_FLOAT(OFS_RETURN) = out;
}

// float(string model, string key) weaponvalue: a setting of the weapon drawn with `model` ("progs/v_rock2.mdl"; Weapon
// Offsets and Weapon Weights, vr_weapons.inc, as its cvars name it: "w_mass" kg, "w_meleedmg", "w_throwdmg"...), what
// it inherits followed (0: no such weapon or key).
void PF_weaponvalue()
{
    const int slot = weapons::slotForName(G_STRING(OFS_PARM0));
    const weapons::Key key = weapons::keyByName(G_STRING(OFS_PARM1));
    G_FLOAT(OFS_RETURN) = slot >= 0 && key != weapons::Key::Count ? weapons::value(slot, key) : 0.f;
}

// float(float mass) weightdamage: the damage multiplier of a melee blow or a throw by a thing of `mass` kg (weight and
// damage, vr_weight_damage_*; 1 for no mass). float(float mass) weightleniency: the factor on the speed thresholds of
// its strikes and throws (heavy leniency, vr_weight_lenient*; 1 for anything up to vr_weight_lenient_from).
void PF_weightdamage()
{
    G_FLOAT(OFS_RETURN) = weight::damageMultiplier(G_FLOAT(OFS_PARM0));
}

void PF_weightleniency()
{
    G_FLOAT(OFS_RETURN) = weight::leniency(G_FLOAT(OFS_PARM0));
}

// vector(vector vel, vector flick, float mass, float hands) throwvelocity: throws by weight (weight::throwVelocity): the
// velocity (m/s) a thing of `mass` kg leaves `hands` hands (1 or 2) at, from the hand's throw estimate `vel` (m/s) and
// the part of it the wrist's flick gives (`flick`): the flick its mass keeps, the gain of a real throw, the limit.
void PF_throwvelocity()
{
    const float* v = G_VECTOR(OFS_PARM0);
    const float* f = G_VECTOR(OFS_PARM1);
    const weight::ThrowOut out = weight::throwVelocity(
        geom::vec(v), geom::vec(f), G_FLOAT(OFS_PARM2), static_cast<int>(G_FLOAT(OFS_PARM3)));
    float* r = G_VECTOR(OFS_RETURN);
    r[0] = out.vel.x;
    r[1] = out.vel.y;
    r[2] = out.vel.z;
}

// float(float what, float mass, ...) throwweight (one builtin: the VR builtins' range is full): a thrown thing's hits
// and spin by weight. `what` 0 (mass): the factor on its speed thresholds (weight::throwScale); 1 (mass, minmult): the
// least speed (units/s) it hurts at (throwHitSpeed); 2 (mass, minmult, curvemult, speed, fullspeed): its damage factor
// hitting at `speed` units/s, 0 under it (throwDamage); 3 (mass, hands, rate): a throw's spin rate (rad/s) limited by its
// mass (throwSpin).
void PF_throwweight()
{
    const int what = static_cast<int>(G_FLOAT(OFS_PARM0));
    const float mass = G_FLOAT(OFS_PARM1);
    float r = 0.f;
    switch(what)
    {
        case 0: r = weight::throwScale(mass); break;
        case 1: r = weight::throwHitSpeed(mass, G_FLOAT(OFS_PARM2)); break;
        case 2:
            r = weight::throwDamage(G_FLOAT(OFS_PARM4), G_FLOAT(OFS_PARM5), mass, G_FLOAT(OFS_PARM2), G_FLOAT(OFS_PARM3));
            break;
        case 3:
            r = glm::length(weight::throwSpin(
                glm::vec3{G_FLOAT(OFS_PARM3), 0.f, 0.f}, mass, static_cast<int>(G_FLOAT(OFS_PARM2))));
            break;
        default: break;
    }
    G_FLOAT(OFS_RETURN) = r;
}

// vector(entity e, vector handangles, float lefthand) propgrip: a prop held the same way every time (Grip Mode 1): from
// now on it turns with the hand as its Grip Pitch, Yaw and Roll say (its angles set now), and its origin's place in
// the hand is returned (forward, right, up, as .carry_offset: Grip X, Y (left) and Z; the left hand's mirrored).
void PF_propgrip()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const float* handAngles = G_VECTOR(OFS_PARM1);
    const bool left = G_FLOAT(OFS_PARM2) != 0.f;
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const int slot = model ? props::slotForModel(model) : -1;
    using props::Key;
    glm::vec3 place = glm::vec3{props::value(slot, Key::GripX), props::value(slot, Key::GripY), props::value(slot, Key::GripZ)} *
                      props::size(slot); // (a point of it: grows with its Size)
    glm::vec3 turn{props::value(slot, Key::GripPitch), props::value(slot, Key::GripYaw), props::value(slot, Key::GripRoll)};
    if(left)
    {
        place.y = -place.y;
        turn.y = -turn.y;
        turn.z = -turn.z;
    }
    // Pitch up for every model (vr_grip.cpp's fixed grip; carrygrip is what the carry uses now).
    const float angles[3]{-turn.x, turn.y, turn.z};
    physics::setCarryTurn(e, handAngles, held::axesFromAngles(angles, true));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = place.x;
    out[1] = -place.y;
    out[2] = place.z;
}

// vector(entity e, vector angles, vector point) modelpoint: where the point `point` of `e`'s model (units in its model's
// axes: x forward, y left, z up) is from its origin when it is turned by `angles` (as the renderer turns it: an alias
// model's pitch is inverted) and scaled as it is drawn. The melee's line of a carried club (a wall torch's head, a
// brick's end: vr_melee.qc).
void PF_modelpoint()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const float* angles = G_VECTOR(OFS_PARM1);
    const float* p = G_VECTOR(OFS_PARM2);
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    // Scaled as it is drawn: the networked scale (an offset from 1) about its origin (a rock's or a brick's size,
    // vr_debris.cpp); an unscaled model (a wall torch) as it is.
    const FieldOffsets& f = fields();
    const glm::vec3 scale = fieldVec(e, f.model_scale), about = fieldVec(e, f.model_scale_origin);
    const glm::vec3 point = (geom::vec(p) - about) * (glm::vec3{1.f} + scale) + about;
    const glm::vec3 v = held::axesFromAngles(angles, model && model->type == mod_brush) * point;
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}
