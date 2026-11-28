// The armed humanoids' weapon-prop think (QC vr_armedgrunt.qc VR_AG_PropThink) in the engine where it settles nothing:
// its quiet frames. QuakeC asks first (agstruck); the engine answers only when it can show QuakeC's own steps would end
// there, with the same writes, and hands every other case back before writing anything (-1: QuakeC's to do). So the
// results are QuakeC's by construction; vr_qc_native_think 2 runs QuakeC after each answer and compares (agent qcnative;
// vr_agthink_compare). Included by vr_builtins.cpp after vr_builtins_agpose.inc (its QuakeC arithmetic and cvar reads).
//
// agstruck: VR_AG_WeaponStruck (vr_armedgrunt_play.qc) with nothing coming at the weapon: nothing moving within the shot
// scan's first look (VR_AGS_Projectiles' findradiusmoving), no roll pending, no tug, no player within VR_AG_StrikeFrame's
// 160 units, the throw scan settled (vr_qc_scan_share 1) or finding nothing. Anything else: QuakeC, as before.
// agparry: VR_AGPar_Frame (vr_armedgrunt_parry.qc) with nothing to read: its guard (VR_AGG_Frame) idle or recovering with
// no raise due, no player within the wind-up reach (VR_AGG_ReadReach, once worked out this frame) nor a preventive check
// due, then no player within 160 units and no monster melee begun in 1.5 s.

// QC's constants this port reads (FTEQCC keeps no names for them: vr_armedgrunt_defs.qc).
constexpr float agtc_VR_AG_LINE_REACH = 192.f;

#define AGT_FIELDS(X) \
    X(vr_ags_pend) X(vr_ag_tug_pl) X(vr_ag_prop) X(vr_ag) X(vr_agpar_w) X(vr_agpar_since) X(vr_agpar_seen) X(vr_agpar_ready) \
    X(vr_agg_state) X(vr_agg_until) X(vr_agg_next) X(vr_agg_wup0) X(vr_agg_wread0) X(vr_agg_wup1) X(vr_agg_wread1)
#define AGT_GLOBALS(X) \
    X(vr_ags_quiet) X(vr_ags_quiet_at) X(vr_ags_proj_won) X(vr_agg_reach) X(vr_agg_reach_at) X(vr_melee_begun_last)
#define AGT_FUNCS(X) X(VR_AG_WeaponStruckQC) X(VR_AGPar_FrameQC)
#define AGT_CVARS(X) \
    X(vr_agrunt_shotdisarm) X(vr_agrunt_shotdisarm_block) X(vr_agrunt_shotdisarm_debug) X(vr_agrunt_disarm) X(vr_qc_scan_share) \
    X(vr_agrunt_parry) X(vr_agrunt_preparry) X(vr_agrunt_windup_parry)

struct AgtBind
{
    const dprograms_t* progs{nullptr}; // the progs bound for
    bool ok{false};
#define AGT_DECL_FIELD(n) int fo_##n{-1};
#define AGT_DECL_GLOBAL(n) float* g_##n{nullptr};
#define AGT_DECL_FUNC(n) func_t fn_##n{0};
#define AGT_DECL_CVAR(n) AgpCvar cv_##n;
    AGT_FIELDS(AGT_DECL_FIELD)
    AGT_GLOBALS(AGT_DECL_GLOBAL)
    AGT_FUNCS(AGT_DECL_FUNC)
    AGT_CVARS(AGT_DECL_CVAR)
#undef AGT_DECL_FIELD
#undef AGT_DECL_GLOBAL
#undef AGT_DECL_FUNC
#undef AGT_DECL_CVAR
};
AgtBind agt; // (the server's VM: the main thread)

// Since the last vr_agthink_compare (the main thread).
struct AgtStats
{
    long long struckAsked{0}, struckNative{0}, struckCompared{0}, struckDiffered{0};
    long long parryAsked{0}, parryNative{0}, parryCompared{0}, parryDiffered{0};
};
AgtStats agtStats;

[[nodiscard]] bool agtBound()
{
    if(agt.progs == qcvm->progs)
    {
        return agt.ok;
    }
    agt = AgtBind{};
    agt.progs = qcvm->progs;
    bool ok = true;
    const auto missing = [&ok](const char* what, const char* name) {
        Con_DPrintf("agthink: no %s %s: armed humanoids' weapon thinks by QuakeC\n", what, name);
        ok = false;
    };
#define AGT_BIND_FIELD(n) if((agt.fo_##n = ED_FindFieldOffset(#n)) < 0) { missing("field", #n); }
#define AGT_BIND_GLOBAL(n) \
    if(ddef_t* d = agpGlobalDef(#n)){ agt.g_##n = &qcvm->globals[d->ofs]; } else { missing("global", #n); }
#define AGT_BIND_FUNC(n) if(!(agt.fn_##n = findFunction(#n))) { missing("function", #n); }
#define AGT_BIND_CVAR(n) \
    { \
        const cvar_t* v = Cvar_FindVar(#n); \
        if(!v) { v = tunableFind(#n); } \
        if(!v) { v = dedicatedClientCvar(#n); } \
        if(!v) { missing("cvar", #n); } \
        agt.cv_##n = AgpCvar{v, v ? clientprefs::indexOf(v) : -1}; \
    }
    AGT_FIELDS(AGT_BIND_FIELD)
    AGT_GLOBALS(AGT_BIND_GLOBAL)
    AGT_FUNCS(AGT_BIND_FUNC)
    AGT_CVARS(AGT_BIND_CVAR)
#undef AGT_BIND_FIELD
#undef AGT_BIND_GLOBAL
#undef AGT_BIND_FUNC
#undef AGT_BIND_CVAR
    agt.ok = ok;
    return ok;
}

#define TF(e, n) (reinterpret_cast<float*>(const_cast<entvars_t*>(&(e)->v))[agt.fo_##n])
#define TFE(e, n) PROG_TO_EDICT(*reinterpret_cast<int*>(&TF(e, n)))
#define TG(n) (*agt.g_##n)
#define TGE(n) PROG_TO_EDICT(*reinterpret_cast<int*>(agt.g_##n))
#define TCV(n) agpCv(agt.cv_##n)

// VR_Player / VR_NextPlayer: find(e, classname, "player")'s walk (the edict index's).
[[nodiscard]] edict_t* agtNextPlayer(const edict_t* from)
{
    const int e = edictindex::nextClassname(NUM_FOR_EDICT(const_cast<edict_t*>(from)), "player");
    return e > 0 ? EDICT_NUM(e) : nullptr;
}

// VR_AG_StrikeFrame's look for a player: one alive within 160 units of `g` (QuakeC's vlen(pl.origin - g.origin) < 160).
[[nodiscard]] bool agtPlayerWithin160(const edict_t* g)
{
    for(edict_t* pl = agtNextPlayer(agWorld()); pl; pl = agtNextPlayer(pl))
    {
        if(pl->v.health > 0 && agVlen(agOrigin(pl) - agOrigin(g)) < 160.f)
        {
            return true;
        }
    }
    return false;
}

// findradiusmoving(org, rad, speed) == world (the same search: its chain written only when it finds something, and then
// QuakeC's own call, the same, writes it again).
[[nodiscard]] bool agtNothingMoving(const float* org, float rad, float speed)
{
    return VR_FindRadiusMoving(org, rad, speed) == agWorld();
}

// What VR_AG_WeaponStruck's quiet run writes (agstruck: QuakeC's answer FALSE).
struct AgtStruckQuiet
{
    bool looked{false};  // the shot scan's first look ran (and found nothing): vr_ags_quiet = p at p.origin, won = FALSE
    bool pendOff{false}; // p.vr_ags_pend = FALSE
};

// VR_AG_WeaponStruck's steps for `g`'s weapon `p` while each stays quiet; false: something to do (QuakeC's), nothing
// written.
[[nodiscard]] bool agtStruckQuiet(edict_t* g, edict_t* p, AgtStruckQuiet& out)
{
    out = AgtStruckQuiet{};
    // VR_AGS_Frame
    const float sd = TCV(vr_agrunt_shotdisarm);
    const bool tug = agHas(TFE(g, vr_ag_tug_pl));
    const edict_t* prop = TFE(g, vr_ag_prop);
    if(!(sd != 0.f || TCV(vr_agrunt_shotdisarm_block) != 0.f) || tug || prop != p)
    {
        out.pendOff = true;
    }
    else
    {
        if(TCV(vr_agrunt_shotdisarm_debug) >= 2.f)
        {
            return false; // (its volumes shown: VR_AGS_Show)
        }
        // VR_AGS_Projectiles' first look
        if(!agtNothingMoving(p->v.origin, 128.f + agtc_VR_AG_LINE_REACH, 0.f))
        {
            return false;
        }
        out.looked = true;
        if(sd == 0.f)
        {
            out.pendOff = true;
        }
        else if(TF(p, vr_ags_pend) != 0.f)
        {
            return false; // (its roll due)
        }
    }
    if(tug)
    {
        return false; // (VR_AG_TugStrike)
    }
    // VR_AG_StrikeFrame
    if(agHas(prop) && p == prop && agtPlayerWithin160(g))
    {
        return false;
    }
    // The throw scan
    if(TCV(vr_agrunt_disarm) == 0.f || !agHas(prop))
    {
        return true;
    }
    const float share = TCV(vr_qc_scan_share);
    if(out.looked && share == 1.f)
    {
        return true;
    }
    if(out.looked && share == 2.f)
    {
        return false; // (looked for all the same and compared: VR_ScanShare_Check)
    }
    return agtNothingMoving(p->v.origin, 48.f + agtc_VR_AG_LINE_REACH, 99.f);
}

void agtStruckWrite(edict_t* p, const AgtStruckQuiet& q)
{
    *reinterpret_cast<int*>(agt.g_vr_ags_quiet) = EDICT_TO_PROG((q.looked ? p : agWorld()));
    if(q.looked)
    {
        agStore(agt.g_vr_ags_quiet_at, agOrigin(p));
        TG(vr_ags_proj_won) = 0.f;
    }
    if(q.pendOff)
    {
        TF(p, vr_ags_pend) = 0.f;
    }
}

// vr_qc_native_think 2: QuakeC's own run after the engine's answer, its writes against the answer's.
[[nodiscard]] float agtStruckCompare(edict_t* g, edict_t* p, const AgtStruckQuiet& q)
{
    // The answer's writes, worked out on copies.
    const AgV atWas = agLoad(agt.g_vr_ags_quiet_at);
    const float wonWas = TG(vr_ags_proj_won);
    const float pendWas = TF(p, vr_ags_pend);
    const int quietWant = EDICT_TO_PROG((q.looked ? p : agWorld()));
    const AgV atWant = q.looked ? agOrigin(p) : atWas;
    const float wonWant = q.looked ? 0.f : wonWas;
    const float pendWant = q.pendOff ? 0.f : pendWas;
    agArgE(0, g);
    agArgE(1, p);
    agRun(agt.fn_VR_AG_WeaponStruckQC, 2);
    const float r = G_FLOAT(OFS_RETURN);
    const bool same = r == 0.f && *reinterpret_cast<int*>(agt.g_vr_ags_quiet) == quietWant &&
                      agEq(agLoad(agt.g_vr_ags_quiet_at), atWant) && TG(vr_ags_proj_won) == wonWant &&
                      TF(p, vr_ags_pend) == pendWant;
    agtStats.struckCompared++;
    if(!same)
    {
        if(agtStats.struckDiffered++ < 8)
        {
            Con_Printf("agthink: agstruck differs (edict %d): QuakeC %g, quiet %d/%d, pend %g/%g, won %g/%g\n",
                NUM_FOR_EDICT(p), r, *reinterpret_cast<int*>(agt.g_vr_ags_quiet), quietWant, TF(p, vr_ags_pend), pendWant,
                TG(vr_ags_proj_won), wonWant);
        }
    }
    return r;
}

// The parry's fields VR_AGPar_Frame's quiet run writes (agparry), in AgtParry's order.
constexpr int agtParryFields = 10;

// What VR_AGPar_Frame's quiet run leaves in them.
struct AgtParry
{
    float v[agtParryFields];
};

[[nodiscard]] AgtParry agtParryRead(const edict_t* g)
{
    return {{TF(g, vr_agpar_w), TF(g, vr_agpar_since), TF(g, vr_agpar_ready), TF(g, vr_agg_state), TF(g, vr_agg_next),
        TF(g, vr_agg_wup0), TF(g, vr_agg_wread0), TF(g, vr_agg_wup1), TF(g, vr_agg_wread1), 0.f}};
}

void agtParryWrite(edict_t* g, const AgtParry& s)
{
    TF(g, vr_agpar_w) = s.v[0];
    TF(g, vr_agpar_since) = s.v[1];
    TF(g, vr_agpar_ready) = s.v[2];
    TF(g, vr_agg_state) = s.v[3];
    TF(g, vr_agg_next) = s.v[4];
    TF(g, vr_agg_wup0) = s.v[5];
    TF(g, vr_agg_wread0) = s.v[6];
    TF(g, vr_agg_wup1) = s.v[7];
    TF(g, vr_agg_wread1) = s.v[8];
}

constexpr float agtc_VR_AGG_IDLE = 0.f; // (vr_armedgrunt_guard.qc)
constexpr float agtc_VR_AGG_REACTING = 1.f;
constexpr float agtc_VR_AGG_RAISED = 2.f;
constexpr float agtc_VR_AGG_RECOVERING = 3.f;

// VR_AGPar_Guards
[[nodiscard]] bool agtGuards(const edict_t* g)
{
    return TF(g, vr_ag) != 0.f && agHas(TFE(g, vr_ag_prop)) && TCV(vr_agrunt_parry) != 0.f;
}

// VR_AGPar_Frame's steps for `g` while nothing comes (VR_AGG_Frame's guard idle or recovering, no raise read, no
// preventive check due, no player within the 160 units the hands are looked for at, no monster's melee begun in 1.5 s):
// `out` the parry's fields after them. False: something to do (QuakeC's), nothing written.
[[nodiscard]] bool agtParryQuiet(const edict_t* g, AgtParry& out)
{
    out = agtParryRead(g);
    if(!agtGuards(g))
    {
        out.v[0] = 0.f;
        out.v[1] = 0.f;
        return true;
    }
    const float now = agTime();
    // VR_AGG_Frame
    const float pre = TCV(vr_agrunt_preparry);
    const float wup = TCV(vr_agrunt_windup_parry);
    float& state = out.v[3];
    if(g->v.health <= 0 || (pre == 0.f && wup == 0.f))
    {
        if(state == agtc_VR_AGG_RAISED)
        {
            out.v[2] = -1.f;
        }
        state = agtc_VR_AGG_IDLE;
    }
    else
    {
        // The nearest player (alive), across the floor.
        const edict_t* best = nullptr;
        float bestD = 100000.f;
        for(edict_t* pl = agtNextPlayer(agWorld()); pl; pl = agtNextPlayer(pl))
        {
            AgV d = agOrigin(pl) - agOrigin(g);
            d.z = 0.f;
            if(pl->v.health > 0 && agVlen(d) < bestD)
            {
                bestD = agVlen(d);
                best = pl;
            }
        }
        // The timers: a raise due, or held (VR_AGPar_Able, the rolls): QuakeC's.
        if(state == agtc_VR_AGG_RAISED || (state == agtc_VR_AGG_REACTING && now >= TF(g, vr_agg_until)))
        {
            return false;
        }
        if(state == agtc_VR_AGG_RECOVERING && now >= TF(g, vr_agg_until))
        {
            state = agtc_VR_AGG_IDLE;
            out.v[4] = now;
        }
        if(best)
        {
            // Your raises: none read from this far (VR_AGG_ReadReach, worked out once a frame: not yet, QuakeC's).
            if(TG(vr_agg_reach_at) != now)
            {
                return false;
            }
            if(!(wup == 0.f || bestD > TG(vr_agg_reach)))
            {
                return false;
            }
            out.v[5] = 0.f;
            out.v[6] = 0.f;
            out.v[7] = 0.f;
            out.v[8] = 0.f;
            if(pre != 0.f && state == agtc_VR_AGG_IDLE && now >= out.v[4])
            {
                return false; // (the preventive check due)
            }
        }
    }
    // Back in VR_AGPar_Frame: nothing can be coming (its guard down, no parry up, no blow seen in 0.35 s).
    if(!(state != agtc_VR_AGG_RAISED && out.v[0] == 0.f && now - TF(g, vr_agpar_seen) >= 0.35f &&
         now - TG(vr_melee_begun_last) >= 1.5f))
    {
        return false;
    }
    if(agtPlayerWithin160(g))
    {
        return false;
    }
    out.v[1] = 0.f;
    return true;
}

// vr_qc_native_think 2: QuakeC's run after the engine's answer, the fields against the answer's.
void agtParryCompare(edict_t* g, const AgtParry& want)
{
    agArgE(0, g);
    agRun(agt.fn_VR_AGPar_FrameQC, 1);
    const AgtParry got = agtParryRead(g);
    agtStats.parryCompared++;
    for(int i = 0; i < agtParryFields; i++)
    {
        if(!(got.v[i] == want.v[i]))
        {
            if(agtStats.parryDiffered++ < 8)
            {
                Con_Printf("agthink: agparry differs (edict %d): field %d QuakeC %g, engine %g\n", NUM_FOR_EDICT(g), i,
                    got.v[i], want.v[i]);
            }
            return;
        }
    }
}

// float(entity g) agparry: VR_AGPar_Frame answered by the engine while nothing comes (TRUE: its writes made), else FALSE:
// VR_AGPar_FrameQC's to do (vr_qc_native_think 0, a blow or a raise to read).
void PF_agparry()
{
    edict_t* g = G_EDICT(OFS_PARM0);
    const int mode = static_cast<int>(vr_qc_native_think.value);
    bool done = false;
    if(mode > 0 && agtBound())
    {
        agtStats.parryAsked++;
        AgtParry s;
        if(agtParryQuiet(g, s))
        {
            agtStats.parryNative++;
            if(mode >= 2)
            {
                agtParryCompare(g, s);
            }
            else
            {
                agtParryWrite(g, s);
            }
            done = true;
        }
    }
    G_FLOAT(OFS_RETURN) = done ? 1.f : 0.f;
}

// vr_agthink_compare: what vr_qc_native_think did since the last (then zeroed); its last line the verdict.
void agthinkCompareReport()
{
    const AgtStats& s = agtStats;
    Con_Printf("agthink: agstruck %lld asked, %lld answered by the engine; agparry %lld asked, %lld answered\n",
        s.struckAsked, s.struckNative, s.parryAsked, s.parryNative);
    const long long compared = s.struckCompared + s.parryCompared;
    const long long differed = s.struckDiffered + s.parryDiffered;
    Con_Printf("agthink: %lld compared, %lld differed (agstruck %lld/%lld, agparry %lld/%lld): %s\n", compared, differed,
        s.struckCompared, s.struckDiffered, s.parryCompared, s.parryDiffered,
        compared == 0 ? "nothing compared (vr_qc_native_think 2)" : differed == 0 ? "PASS" : "FAIL");
    agtStats = AgtStats{};
}

// float(entity g, entity p) agstruck: VR_AG_WeaponStruck answered by the engine on a quiet frame (0: not struck, its
// writes made), else -1: VR_AG_WeaponStruckQC's to do (vr_qc_native_think 0, something coming).
void PF_agstruck()
{
    edict_t* g = G_EDICT(OFS_PARM0);
    edict_t* p = G_EDICT(OFS_PARM1);
    const int mode = static_cast<int>(vr_qc_native_think.value);
    float r = -1.f;
    if(mode > 0 && agtBound())
    {
        agtStats.struckAsked++;
        AgtStruckQuiet q;
        if(agtStruckQuiet(g, p, q))
        {
            agtStats.struckNative++;
            if(mode >= 2)
            {
                r = agtStruckCompare(g, p, q);
            }
            else
            {
                agtStruckWrite(p, q);
                r = 0.f;
            }
        }
    }
    G_FLOAT(OFS_RETURN) = r;
}

#undef TF
#undef TFE
#undef TG
#undef TGE
#undef TCV
