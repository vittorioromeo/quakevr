// The armed humanoids' weapon-prop think (QC vr_armedgrunt.qc VR_AG_PropThink) in the engine where it settles nothing:
// its quiet frames. QuakeC asks first (agstruck); the engine answers only when it can show QuakeC's own steps would end
// there, with the same writes, and hands every other case back before writing anything (-1: QuakeC's to do). So the
// results are QuakeC's by construction; vr_qc_native_think 2 runs QuakeC after each answer and compares (agent qcnative;
// vr_agthink_compare). Included by vr_builtins.cpp after vr_builtins_agpose.inc (its QuakeC arithmetic and cvar reads).
//
// agstruck: VR_AG_WeaponStruck (vr_armedgrunt_play.qc) with nothing coming at the weapon: nothing moving within the shot
// scan's first look (VR_AGS_Projectiles' findradiusmoving), no roll pending, no tug, no player within VR_AG_StrikeFrame's
// 160 units, the throw scan settled (vr_qc_scan_share 1) or finding nothing. Anything else: QuakeC, as before.

// QC's constants this port reads (FTEQCC keeps no names for them: vr_armedgrunt_defs.qc).
constexpr float agtc_VR_AG_LINE_REACH = 192.f;

#define AGT_FIELDS(X) X(vr_ags_pend) X(vr_ag_tug_pl) X(vr_ag_prop)
#define AGT_GLOBALS(X) X(vr_ags_quiet) X(vr_ags_quiet_at) X(vr_ags_proj_won)
#define AGT_FUNCS(X) X(VR_AG_WeaponStruckQC)
#define AGT_CVARS(X) \
    X(vr_agrunt_shotdisarm) X(vr_agrunt_shotdisarm_block) X(vr_agrunt_shotdisarm_debug) X(vr_agrunt_disarm) X(vr_qc_scan_share)

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

#define TF(e, n) (reinterpret_cast<float*>(&(e)->v)[agt.fo_##n])
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

// vr_agthink_compare: what vr_qc_native_think did since the last (then zeroed); its last line the verdict.
void agthinkCompareReport()
{
    const AgtStats& s = agtStats;
    Con_Printf("agthink: agstruck %lld asked, %lld answered by the engine\n", s.struckAsked, s.struckNative);
    const bool pass = s.struckDiffered == 0;
    Con_Printf("agthink: %lld compared, %lld differed: %s\n", s.struckCompared, s.struckDiffered,
        s.struckCompared == 0 ? "nothing compared (vr_qc_native_think 2)" : pass ? "PASS" : "FAIL");
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
