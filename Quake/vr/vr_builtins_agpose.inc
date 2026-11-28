// The armed humanoids' weapon-prop pose (QC vr_armedgrunt_pose.qc VR_AGP_Pose and its helpers) in the engine: the same
// float steps in the same order as the QuakeC VM takes them, on QC's own globals and fields (its helpers that stay QC's,
// the parry's, the blows', the chainsaw's engine, read and write them as before), so the results are QuakeC's (agent
// qcpu2; vr_agpose_native 2 compares them). Included by vr_builtins.cpp (qvr::progs, its anonymous namespace), after
// vr_builtins_held.inc and vr_builtins_hitmodel.inc (their builtins are called here as QuakeC calls them).
// Left to QuakeC (agpose answers FALSE before doing anything): the toolgun's ghost (vr_agp_quiet), the debug prints
// (vr_agrunt_debug 2), the Pose Studio's holds and Live mode, the Grip Editor's freeze, test 90's hold, a pole weapon
// (the spear's two-handed hold and thrust: VR_AGP_Pole).

// QuakeC's float arithmetic (pr_exec.c's ops, pr_cmds.c's builtins): float steps, -ffp-contract=off (quakevr.props).
struct AgV
{
    float x, y, z;
};
inline AgV operator+(AgV a, AgV b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline AgV operator-(AgV a, AgV b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline AgV operator*(AgV a, float f) { return {a.x * f, a.y * f, a.z * f}; }
inline float agDot(AgV a, AgV b) { return a.x * b.x + a.y * b.y + a.z * b.z; } // OP_MUL_V
inline bool agEq(AgV a, AgV b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
inline bool agZero(AgV a) { return a.x == 0.f && a.y == 0.f && a.z == 0.f; }
inline float agNeg(float f) { return 0.f - f; } // (FTEQCC's unary minus: 0 - x)
inline float agMin(float a, float b) { return a > b ? b : a; } // PF_min, PF_max
inline float agMax(float a, float b) { return a < b ? b : a; }
inline float agSin(float f) { return static_cast<float>(sin(static_cast<double>(f))); }
inline float agCos(float f) { return static_cast<float>(cos(static_cast<double>(f))); }
inline float agAtan2(float a, float b) { return static_cast<float>(atan2(static_cast<double>(a), static_cast<double>(b))); }
inline float agFloor(float f) { return static_cast<float>(floor(static_cast<double>(f))); }
inline float agFabs(float f) { return static_cast<float>(fabs(static_cast<double>(f))); }
inline float agRint(float f) { return agpsRint(f); } // PF_rint
inline float agAnd(float a, float b) { return static_cast<float>(static_cast<int>(a) & static_cast<int>(b)); } // OP_BITAND_F
inline float agOr(float a, float b) { return static_cast<float>(static_cast<int>(a) | static_cast<int>(b)); }
inline float agVlen(AgV v) // PF_vlen
{
    const double d = static_cast<double>(v.x) * v.x + static_cast<double>(v.y) * v.y + static_cast<double>(v.z) * v.z;
    return static_cast<float>(sqrt(d));
}
inline AgV agNormalize(AgV v) // PF_normalize
{
    double d = static_cast<double>(v.x) * v.x + static_cast<double>(v.y) * v.y + static_cast<double>(v.z) * v.z;
    d = sqrt(d);
    if(d == 0)
    {
        return {0.f, 0.f, 0.f};
    }
    d = 1 / d;
    return {static_cast<float>(v.x * d), static_cast<float>(v.y * d), static_cast<float>(v.z * d)};
}
inline AgV agCross(AgV a, AgV b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; } // VR_Cross
inline AgV agLerp(AgV a, AgV b, float t) { return a + (b - a) * t; } // VR_Lerp
inline float agSmooth(float x) // VR_Smooth
{
    x = agMin(1.f, agMax(0.f, x));
    return x * x * (3.f - 2.f * x);
}
inline AgV agMirror(AgV v) { return {v.x, agNeg(v.y), v.z}; } // VR_AGP_Mirror
inline AgV agLoad(const float* p) { return {p[0], p[1], p[2]}; }
inline void agStore(float* p, AgV v)
{
    p[0] = v.x;
    p[1] = v.y;
    p[2] = v.z;
}
constexpr float agPi = 3.14159265f;
constexpr float agToDeg = 180.f / agPi; // (QC's folded constants: 180 / 3.14159265, 3.14159265 / 180)
constexpr float agToRad = agPi / 180.f;

// QC's constants this port reads (FTEQCC keeps no names for them: vr_ad_guns_defs.qc, vr_armedgrunt.qc,
// vr_armedgrunt_pose.qc).
constexpr float agc_QVR_WPNFLAG_SWORD_KIND = 3840.f;
constexpr float agc_QVR_WPNFLAG_SWORD_KIND_UNIT = 256.f;
constexpr float agc_VR_ADMELEE_HAMMER = 5.f;
constexpr float agc_VR_ADMELEE_MACE = 6.f;
constexpr float agc_VR_ADMELEE_PHANTOM = 7.f;
constexpr float agc_VR_AG_CODE_SHADOWAXE = 101.f;
constexpr float agc_VR_AG_CODE_WIDOWMAKER = 102.f;
constexpr float agc_VR_AG_CODE_PLASMAGUN = 103.f;
constexpr float agc_VR_AG_CODE_ADMELEE = 110.f;
constexpr float agc_VR_AG_LIFT_TIME = 0.3f;
constexpr float agc_VR_AGK_TUG_EASE = 0.25f;

// QC's names this port reads and writes, found by name at each progs load (any missing: agpose leaves it to QuakeC).
#define AGP_FIELDS(X) \
    X(vr_ag_kind) X(vr_ag_act) X(vr_ag_act_tick) X(vr_ag_act_hit) X(vr_ag_act_at) X(vr_ag_act_cast) X(vr_ag_act_kind) \
    X(vr_ag_act_speed) X(vr_ag_act_hand) X(vr_ag_act_var) X(vr_ag_act_hitl) X(vr_ag_prop) X(vr_ag_prop2) \
    X(vr_ag_took_at) X(vr_ag_took_grab) X(vr_ag_took_org) X(vr_ag_took_fw) X(vr_ag_took_up) X(vr_agk_held) \
    X(vr_agk_live) X(vr_ag_aimw) X(vr_ag_aimdir) X(vr_ag_aim) X(vr_agp_twist) X(vr_ag_kick_at) X(vr_agp_rl_at) \
    X(vr_ag_readyw) X(vr_agp_rdy) X(vr_agp_runw) X(vr_agk_tugt) X(vr_agk_tugw) X(vr_ag_tug_pl) X(vr_ag_strikes_l) \
    X(weaponflags) X(weaponinst) X(vr_ag_left) X(vr_agp_geo) X(vr_agp_muz) X(vr_agp_butt) X(vr_agp_sec) X(vr_ag_hk) \
    X(vr_ag_hk_at) X(vr_ag_pfw) X(vr_ag_pup) X(wi_adup) X(vr_knockdown)
#define AGP_GLOBALS(X) \
    X(vr_agp_wr) X(vr_agp_v) X(vr_agp_fw) X(vr_agp_up) X(vr_agp_wo) X(vr_agp_flags) X(vr_agp_wl) X(vr_agp_vl) \
    X(vr_agp_f) X(vr_agp_r) X(vr_agp_quiet) X(vr_agp_events) X(vr_agk_rp) X(vr_agk_rr) X(vr_agk_lp) X(vr_agk_lr) X(vr_agk_er) \
    X(vr_agk_el) X(vr_agk_t) X(vr_agk_on) X(vr_agk_gp) X(vr_agk_gr) X(vr_agk_mode) X(vr_agk_gun) X(vr_agk_stretch) \
    X(vr_agk_tugs) X(vr_agk_hpose) X(vr_agk_hphase) X(vr_agk_hplay) X(vr_agk_test_hold) X(vr_agp_tw) X(vr_agp_ts) \
    X(vr_agp_th) X(vr_agp_tr) X(vr_agp_kw) X(vr_agp_kwfw) X(vr_agp_kc) X(vr_agp_ke) X(vr_agp_kefw) X(vr_agp_sawat) \
    X(vr_agp_sawfw) X(vr_agp_sfw) X(vr_agp_sup) X(vr_agp_gunat) X(vr_agp_onehand) X(vr_agp_gunpose) X(vr_agp_ww) \
    X(vr_agp_wf) X(vr_agp_tq) X(vr_agp_drawr) X(vr_agp_drawl) X(vr_agp_waist) X(vr_agp_elr) X(vr_agp_ell) X(vr_agp_shpole) \
    X(vr_agp_trq) X(vr_agp_trc) X(vr_agp_trs) X(vr_agk_ba) X(vr_agk_bb) X(vr_agk_bf) X(vr_agk_rep) X(vr_agk_repent) \
    X(vr_agk_k_rp) X(vr_agk_k_rr) X(vr_agk_k_lp) X(vr_agk_k_lr) X(vr_agk_k_er) X(vr_agk_k_el) X(vr_agk_k_t) \
    X(vr_agk_k_gp) X(vr_agk_k_gr) X(vr_agh_set) X(vr_agp_fwl) X(vr_agp_upl) X(vr_agpar_mw) X(vr_am_filled) \
    X(vr_am_kind) X(vr_am_anchor) X(vr_am_anchor_at) X(vr_am_sh_r) X(vr_am_sh_l) X(vr_am_arm) X(vr_am_grip_r) \
    X(vr_am_gun_fw) X(vr_am_fit_scale) X(vr_am_fit_shift) X(vr_am_hand) X(vr_am_gun_rear) X(vr_am_gun_muzzle) \
    X(vr_am_gun_roll) X(vr_am_f_run) X(vr_ad_data) \
    \
   
#define AGP_FUNCS(X) \
    X(VR_AGP_PoseQC) X(VR_AM_Row) X(VR_AGP_Geo) X(VR_AGP_Strike) X(VR_DRP_Frame) X(VR_AGPar_Pose) X(VR_AGPar_Meet) \
    X(VR_AGP_Saw) X(VR_AGP_Demo) X(VR_AGP_CastLands) X(VR_AGP_ShoveLands) X(VR_AI_Origin) X(VR_AGP_Shield)
#define AGP_CVARS(X) \
    X(vr_agrunt_debug) X(vr_agrunt_freeze) X(vr_agrunt_tug_ik_stretch) X(vr_agrunt_upper) X(vr_agrunt_aim_twist) \
    X(vr_agrunt_take_bend) X(vr_agrunt_take_bend_kneel) X(vr_aglegs) X(vr_agarms_shoulder) X(vr_agrunt_second_reach) \
    X(vr_agrunt_strike_torso) X(vr_agrunt_strike_shoulder) X(vr_agrunt_strike_demo) X(vr_agrunt_reload_time) \
    X(vr_agrunt_reload_ssg) X(vr_heavy_recoil_time) X(vr_grunt_burst)

struct AgpCvar // (as cvar_hget reads it: a tuning constant's stand-in, a per-player preference)
{
    const cvar_t* var{nullptr};
    int pref{-1};
};
struct AgpBind
{
    const dprograms_t* progs{nullptr}; // the progs bound for
    bool ok{false};
#define AGP_DECL_FIELD(n) int fo_##n{-1};
#define AGP_DECL_GLOBAL(n) float* g_##n{nullptr};
#define AGP_DECL_FUNC(n) func_t fn_##n{0};
#define AGP_DECL_CVAR(n) AgpCvar cv_##n;
    AGP_FIELDS(AGP_DECL_FIELD)
    AGP_GLOBALS(AGP_DECL_GLOBAL)
    AGP_FUNCS(AGP_DECL_FUNC)
    AGP_CVARS(AGP_DECL_CVAR)
#undef AGP_DECL_FIELD
#undef AGP_DECL_GLOBAL
#undef AGP_DECL_FUNC
#undef AGP_DECL_CVAR
};
AgpBind agp; // (the server's VM: the main thread)

[[nodiscard]] ddef_t* agpGlobalDef(const char* name) // (once a progs load)
{
    for(int i = 0; i < qcvm->progs->numglobaldefs; i++)
    {
        ddef_t* def = &qcvm->globaldefs[i];
        if(!ZA_STRCMP(PR_GetString(def->s_name), name))
        {
            return def;
        }
    }
    return nullptr;
}

[[nodiscard]] bool agpBound()
{
    if(agp.progs == qcvm->progs)
    {
        return agp.ok;
    }
    agp = AgpBind{};
    agp.progs = qcvm->progs;
    bool ok = true;
    const auto missing = [&ok](const char* what, const char* name) {
        Con_DPrintf("agpose: no %s %s: armed humanoids' poses by QuakeC\n", what, name);
        ok = false;
    };
#define AGP_BIND_FIELD(n) if((agp.fo_##n = ED_FindFieldOffset(#n)) < 0) { missing("field", #n); }
#define AGP_BIND_GLOBAL(n) \
    if(ddef_t* d = agpGlobalDef(#n)){ agp.g_##n = &qcvm->globals[d->ofs]; } else { missing("global", #n); }
#define AGP_BIND_FUNC(n) if(!(agp.fn_##n = findFunction(#n))) { missing("function", #n); }
#define AGP_BIND_CVAR(n) \
    { \
        const cvar_t* v = Cvar_FindVar(#n); \
        if(!v) { v = tunableFind(#n); } \
        if(!v) { v = dedicatedClientCvar(#n); } \
        if(!v) { missing("cvar", #n); } \
        agp.cv_##n = AgpCvar{v, v ? clientprefs::indexOf(v) : -1}; \
    }
    AGP_FIELDS(AGP_BIND_FIELD)
    AGP_GLOBALS(AGP_BIND_GLOBAL)
    AGP_FUNCS(AGP_BIND_FUNC)
    AGP_CVARS(AGP_BIND_CVAR)
#undef AGP_BIND_FIELD
#undef AGP_BIND_GLOBAL
#undef AGP_BIND_FUNC
#undef AGP_BIND_CVAR
    for(const float* g : bindings().agPose)
    {
        ok = ok && g != nullptr;
    }
    agp.ok = ok;
    return ok;
}

// cvar_hget's value (PF_cvar_hget).
[[nodiscard]] float agpCv(const AgpCvar& c)
{
    if(c.var && c.pref >= 0)
    {
        return clientprefs::forQc(c.pref, c.var);
    }
    return c.var ? c.var->value : 0.f;
}

#define AF(e, n) (reinterpret_cast<float*>(&(e)->v)[agp.fo_##n])
#define AFP(e, n) (&AF(e, n))
#define AFV(e, n) agLoad(AFP(e, n))
#define AFE(e, n) PROG_TO_EDICT(*reinterpret_cast<int*>(AFP(e, n)))
#define AG(n) (*agp.g_##n)
#define AGV(n) agLoad(agp.g_##n)
#define AGSET(n, v) agStore(agp.g_##n, (v))
#define ACV(n) agpCv(agp.cv_##n)

[[nodiscard]] inline edict_t* agWorld() { return qcvm->edicts; }
[[nodiscard]] inline bool agHas(const edict_t* e) { return e != qcvm->edicts; }
[[nodiscard]] inline float agTime() { return pr_global_struct->time; }
[[nodiscard]] inline AgV agOrigin(const edict_t* e) { return agLoad(e->v.origin); }

// A QC function's parameters (as the VM passes them) and its run (`self` left as it is: VR_AGP_Pose's).
inline void agArgE(int i, const edict_t* e) { G_INT(OFS_PARM0 + 3 * i) = EDICT_TO_PROG(e); }
inline void agArgF(int i, float f) { G_FLOAT(OFS_PARM0 + 3 * i) = f; }
inline void agArgV(int i, AgV v) { agStore(G_VECTOR(OFS_PARM0 + 3 * i), v); }
[[nodiscard]] inline AgV agRetV() { return agLoad(G_VECTOR(OFS_RETURN)); }
inline void agRun(func_t f, int argc)
{
    qcvm->argc = argc;
    PR_ExecuteProgram(f);
}

// Builtins as QuakeC calls them.
[[nodiscard]] AgV agWdp(const edict_t* e, float a, float what) // weapondrawnpose
{
    agArgE(0, e);
    agArgF(1, a);
    agArgF(2, what);
    qcvm->argc = 3;
    PF_weapondrawnpose();
    return agRetV();
}
[[nodiscard]] AgV agModelVertex(const edict_t* e, float v) // QC modelvertex (modelanchorpoint)
{
    AgV a{0.f, 0.f, 0.f};
    a.x = -2.f - v;
    agArgE(0, e);
    agArgV(1, a);
    qcvm->argc = 2;
    PF_modelanchorpoint();
    return agRetV();
}
void agMakeVectors(AgV a) // makevectors
{
    vec3_t ang{a.x, a.y, a.z};
    AngleVectors(ang, pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
}
[[nodiscard]] AgV agVecToAngles(AgV v) // PF_vectoangles
{
    float yaw, pitch;
    if(v.y == 0 && v.x == 0)
    {
        yaw = 0;
        pitch = v.z > 0 ? 90.f : 270.f;
    }
    else
    {
        // (C's double atan2 and sqrt, as pr_cmds.c's: not C++'s float overloads)
        yaw = static_cast<float>(static_cast<int>(atan2(static_cast<double>(v.y), static_cast<double>(v.x)) * 180 / M_PI));
        if(yaw < 0)
        {
            yaw += 360;
        }
        const float forward = static_cast<float>(sqrt(static_cast<double>(v.x * v.x + v.y * v.y)));
        pitch = static_cast<float>(static_cast<int>(atan2(static_cast<double>(v.z), static_cast<double>(forward)) * 180 /
            M_PI));
        if(pitch < 0)
        {
            pitch += 360;
        }
    }
    return {pitch, yaw, 0.f};
}
void agSetOrigin(edict_t* e, AgV o) // PF_setorigin
{
    agStore(e->v.origin, o);
    SV_LinkEdict(e, false);
}

// ---------------------------------------------------------------------------------------------------------------------
// The helpers (vr_armedgrunt_pose.qc, vr_armedmon.qc, vr_armedgrunt.qc), each as its QC.

// VR_AM_PoseKind: the kind whose pose keys, grips and holds row `k` takes (AD's knights, rows 4 and 5: their shared kind
// 4, agpose::adKnight; without AD's data the knight's, as VR_AM_Resolve; adposekind: before, this read the row itself,
// so in the game AD's rows took no keys at all)
[[nodiscard]] float agPoseKind(float k)
{
    return k >= 4.f ? (vr_ad_mounted.value != 0.f ? 4.f : 2.f) : k;
}

void agRow(float k) // VR_AM_Row (filled: at once)
{
    if(AG(vr_am_filled) != 0.f && k == AG(vr_am_kind))
    {
        return;
    }
    agArgF(0, k);
    agRun(agp.fn_VR_AM_Row, 1);
}
[[nodiscard]] AgV agFit(edict_t* g, AgV l) // VR_AM_Fit
{
    agRow(AF(g, vr_ag_kind));
    const AgV sc = AGV(vr_am_fit_scale), sh = AGV(vr_am_fit_shift);
    return {l.x * sc.x + sh.x, l.y * sc.y + sh.y, l.z * sc.z + sh.z};
}
void agBody(edict_t* g) // VR_AGP_Body
{
    agMakeVectors({0.f, g->v.angles[1], 0.f});
    AGSET(vr_agp_f, agLoad(pr_global_struct->v_forward));
    AGSET(vr_agp_r, agLoad(pr_global_struct->v_right));
}
[[nodiscard]] AgV agToWorld(AgV l) // VR_AGP_ToWorld
{
    return AGV(vr_agp_f) * l.x - AGV(vr_agp_r) * l.y + AgV{0.f, 0.f, 1.f} * l.z;
}
[[nodiscard]] AgV agToLocal(AgV w) // VR_AGP_ToLocal
{
    return {agDot(w, AGV(vr_agp_f)), agNeg(agDot(w, AGV(vr_agp_r))), w.z};
}
[[nodiscard]] AgV agNlerp(AgV a, AgV b, float t) // VR_AGP_Nlerp
{
    const AgV m = a + (b - a) * t;
    return agVlen(m) > 0.0001f ? agNormalize(m) : b;
}
[[nodiscard]] AgV agBezier(AgV a, AgV c, AgV b, float t) // VR_AGP_Bezier
{
    return a * ((1.f - t) * (1.f - t)) + c * (2.f * t * (1.f - t)) + b * (t * t);
}
[[nodiscard]] float agEase(float x, float to, float rate) // VR_AGP_Ease
{
    const float ft = pr_global_struct->frametime;
    return x < to ? agMin(to, x + rate * ft) : agMax(to, x - rate * ft);
}
[[nodiscard]] AgV agRotate(AgV v, AgV k, float deg) // VR_AG_Rotate
{
    const float c = agCos(deg * agToRad);
    const float s = agSin(deg * agToRad);
    AgV kv;
    kv.x = k.y * v.z - k.z * v.y;
    kv.y = k.z * v.x - k.x * v.z;
    kv.z = k.x * v.y - k.y * v.x;
    return v * c + kv * s + k * (agDot(k, v) * (1.f - c));
}
[[nodiscard]] float agClass(float wid) // VR_AG_Class
{
    if(wid < 0 || wid == WID_FIST)
    {
        return VR_AG_CLASS_FISTS;
    }
    if(wid == WID_AXE || wid == WID_SWORD || wid == WID_CROWBAR || wid == WID_CHAINSAW || wid == WID_MJOLNIR ||
       wid == WID_SUPERAXE)
    {
        return VR_AG_CLASS_MELEE;
    }
    if(wid == WID_GRENADE_LAUNCHER || wid == WID_ROCKET_LAUNCHER || wid == WID_PROXIMITY_GUN || wid == WID_ADROCKET ||
       wid == WID_ADGRENADE)
    {
        return VR_AG_CLASS_EXPLOSIVE;
    }
    if(wid == WID_NAILGUN || wid == WID_SUPER_NAILGUN || wid == WID_LIGHTNING || wid == WID_LASER_CANNON ||
       wid == WID_ADPYRO || wid == WID_ADFUMIGATOR)
    {
        return VR_AG_CLASS_AUTO;
    }
    if((wid == WID_GRUNTGUN && ACV(vr_grunt_burst) != 0.f) || wid == WID_ADPLASMA || wid == WID_ADCENTURION)
    {
        return VR_AG_CLASS_BURST;
    }
    return VR_AG_CLASS_GUN;
}
[[nodiscard]] float agMeleeKind(float flags) // VR_ADMelee_Kind
{
    return agFloor(agAnd(flags, agc_QVR_WPNFLAG_SWORD_KIND) / agc_QVR_WPNFLAG_SWORD_KIND_UNIT);
}
[[nodiscard]] bool agMeleeHeavy(float k) // VR_ADMelee_Heavy
{
    return k == agc_VR_ADMELEE_HAMMER || k == agc_VR_ADMELEE_MACE || k == agc_VR_ADMELEE_PHANTOM;
}
[[nodiscard]] bool agHeavy(edict_t* p) // VR_AGP_Heavy
{
    if(agHas(AFE(PROG_TO_EDICT(p->v.owner), vr_ag_prop2)))
    {
        return false;
    }
    const float wid = p->v.weapon;
    if(wid == WID_SWORD)
    {
        return agMeleeHeavy(agMeleeKind(AF(p, weaponflags)));
    }
    return wid == WID_AXE || wid == WID_MJOLNIR || wid == WID_CHAINSAW || wid == WID_SUPERAXE;
}
[[nodiscard]] float agHoldCode(edict_t* p) // VR_AGH_Code
{
    const float wid = p->v.weapon;
    if(AG(vr_ad_data) != 0.f && agHas(AFE(p, weaponinst)) && AF(AFE(p, weaponinst), wi_adup) != 0.f)
    {
        if(wid == WID_AXE) { return agc_VR_AG_CODE_SHADOWAXE; }
        if(wid == WID_SUPER_SHOTGUN) { return agc_VR_AG_CODE_WIDOWMAKER; }
        if(wid == WID_LIGHTNING) { return agc_VR_AG_CODE_PLASMAGUN; }
    }
    if(wid == WID_SWORD)
    {
        const float k = agMeleeKind(AF(p, weaponflags));
        return agMeleeHeavy(k) ? agc_VR_AG_CODE_ADMELEE + k : static_cast<float>(WID_SWORD);
    }
    return wid;
}
[[nodiscard]] float agHands(edict_t* g, edict_t* p) // VR_AGH_Hands
{
    AG(vr_agh_set) = 0.f;
    if(!agHas(p) || !agHas(g) || agHas(AFE(g, vr_ag_prop2)))
    {
        return 0.f;
    }
    const AgV h = agWdp(agWorld(), agPoseKind(AF(g, vr_ag_kind)) * 1000.f + agHoldCode(p), 24.f);
    AG(vr_agh_set) = h.y;
    return h.x;
}
[[nodiscard]] bool agTwoHanded(edict_t* p) // VR_AGP_TwoHanded
{
    edict_t* owner = PROG_TO_EDICT(p->v.owner);
    if(agHas(AFE(owner, vr_ag_prop2)))
    {
        return false;
    }
    const float h = agHands(owner, p);
    if(twohand::poleWeapon(weapons::slotForName(PR_GetString(p->v.model))))
    {
        return AG(vr_agh_set) != 1.f;
    }
    return h != 0.f ? (h == 2.f) : agHeavy(p);
}

constexpr float agPunch = 1.f, agSwing = 2.f, agSaw = 3.f, agShove = 4.f, agCast = 5.f; // VR_AGP_PUNCH...
constexpr float agfLeft = 2.f, agfLeftWpn = 8.f, agfFree = 4.f, agfTwo = 16.f, agfBody = 32.f, agfFixed = 1.f;
constexpr float agfAnchor = 64.f, agfReach = 128.f;

[[nodiscard]] float agSpeed(edict_t* g) // VR_AGP_Speed
{
    const float sp = AF(g, vr_ag_act_speed);
    const float act = AF(g, vr_ag_act);
    return (sp > 0 && sp < 1 && (act == agSwing || act == agPunch)) ? agMax(0.1f, sp) : 1.f;
}
void agTimes(edict_t* g) // VR_AGP_Times
{
    float tw, ts, th = 0.f, tr;
    const float act = AF(g, vr_ag_act);
    AG(vr_agp_th) = 0.f;
    if(act == agPunch)
    {
        const float k = AF(g, vr_ag_act_kind);
        tw = k == 0 ? 0.08f : k == 1 ? 0.12f : 0.14f;
        ts = k == 0 ? 0.1f : k == 1 ? 0.11f : 0.13f;
        tr = k == 0 ? 0.18f : 0.24f;
    }
    else if(act == agSaw)
    {
        tw = 0.3f;
        ts = 0.15f;
        th = 0.7f;
        tr = 0.3f;
    }
    else if(act == agCast)
    {
        tw = 0.3f; // VR_AGP_CAST_RAISE, _HOLD, _LOWER
        ts = 0.45f;
        tr = 0.3f;
    }
    else if(act == agShove)
    {
        tw = 0.18f; // VR_AGP_SHOVE_WIND, _PUSH, _RECOVER, _BACK
        ts = 0.12f;
        th = 0.1f;
        tr = 0.16f;
    }
    else
    {
        edict_t* prop = AFE(g, vr_ag_prop);
        const bool heavy = agHas(prop) && agHeavy(prop);
        tw = heavy ? 0.36f : 0.28f;
        ts = heavy ? 0.18f : 0.15f;
        tr = heavy ? 0.36f : 0.3f;
    }
    const float sp = agSpeed(g);
    if(sp < 1)
    {
        tw = tw / sp;
        ts = ts / sp;
        th = th / sp;
        tr = tr / sp;
    }
    AG(vr_agp_tw) = tw;
    AG(vr_agp_ts) = ts;
    AG(vr_agp_th) = th;
    AG(vr_agp_tr) = tr;
}
[[nodiscard]] AgV agTargetLocal(edict_t* g) // VR_AGP_TargetLocal
{
    edict_t* t = PROG_TO_EDICT(g->v.enemy);
    if(!agHas(t))
    {
        return {40.f, 0.f, 10.f};
    }
    agArgE(0, t);
    agRun(agp.fn_VR_AI_Origin, 1);
    const AgV o = agRetV();
    const float up = agAnd(t->v.flags, FL_CLIENT) != 0.f ? 8.f : (t->v.mins[2] + t->v.maxs[2]) * 0.5f;
    const AgV at = o + AgV{0.f, 0.f, 1.f} * up;
    return agToLocal(at - agOrigin(g));
}
void agStrike(edict_t* g, edict_t* p, AgV from, AgV dir) // VR_AGP_Strike (QuakeC's)
{
    agArgE(0, g);
    agArgE(1, p);
    agArgV(2, from);
    agArgV(3, dir);
    agRun(agp.fn_VR_AGP_Strike, 4);
}
void agCallGE(func_t f, edict_t* g, edict_t* p)
{
    agArgE(0, g);
    agArgE(1, p);
    agRun(f, 2);
}
[[nodiscard]] AgV agHang(float left) // VR_AGP_Hang
{
    const AgV sh = left != 0.f ? AGV(vr_am_sh_l) : AGV(vr_am_sh_r);
    const AgV d = agNormalize(left != 0.f ? AgV{0.3f, 0.15f, -1.f} : AgV{0.3f, -0.15f, -1.f});
    return sh + d * (0.8f * AG(vr_am_arm) + 2.5f);
}

void agFists(edict_t* g, AgV handL) // VR_AGP_Fists
{
    (void)handL;
    AF(g, vr_ag_readyw) = agEase(AF(g, vr_ag_readyw), agHas(PROG_TO_EDICT(g->v.enemy)) ? 0.75f : 0.f, 3.f);
    float k = AF(g, vr_ag_readyw);
    AgV r{9.f, -5.f, 13.f}; // VR_AGP_GUARD_R
    AgV l{11.f, 5.f, 15.f}; // VR_AGP_GUARD_L
    const float s = agTime() - AF(g, vr_ag_act_at);
    if(AF(g, vr_ag_act) == agPunch)
    {
        agTimes(g);
        const float tw = AG(vr_agp_tw), ts = AG(vr_agp_ts), tr = AG(vr_agp_tr);
        const float kind = AF(g, vr_ag_act_kind);
        const bool left = kind == 0 || kind == 2;
        const AgV sh = left ? AgV{0.f, 9.7f, 12.f} : AgV{0.f, -9.7f, 12.f}; // VR_AGP_SHOULDER_L, _R
        const AgV guard = left ? l : r;
        AgV tl = agTargetLocal(g);
        if(kind == 4)
        {
            tl.z = tl.z + 4.f;
        }
        const AgV d = tl - sh;
        const AgV hit = sh + agNormalize(d) * agMin(agVlen(d) - 2.f, 21.f); // VR_AGP_ARM_REACH
        AgV wind = guard - AgV{3.f, 0.f, 0.f};
        AgV ctl = (guard + hit) * 0.5f;
        if(kind == 2 || kind == 3)
        {
            wind = guard + (left ? AgV{-4.f, 8.f, 0.f} : AgV{-4.f, -8.f, 0.f});
            ctl = sh + (left ? AgV{12.f, 12.f, 3.f} : AgV{12.f, -12.f, 3.f});
        }
        else if(kind == 4)
        {
            wind = {6.f, -6.f, 2.f};
            ctl = {13.f, -5.f, 3.f};
        }
        AgV fist;
        float m = 1.f;
        if(s < tw)
        {
            fist = agLerp(guard, wind, agSmooth(s / tw));
            m = k + (1.f - k) * agSmooth(s * agSpeed(g) / 0.06f);
        }
        else if(s < tw + ts)
        {
            const float u = (s - tw) / ts;
            const float pe = agSmooth(u);
            fist = agBezier(wind, ctl, hit, pe);
            if(AF(g, vr_ag_act_hit) == 0.f && u >= 0.8f)
            {
                AF(g, vr_ag_act_hit) = 1.f;
                agBody(g);
                const AgV fw = agToWorld(fist - wind);
                agStrike(g, agWorld(), agOrigin(g) + agToWorld(fist) - agNormalize(fw) * 6.f, agNormalize(fw));
                agBody(g);
            }
        }
        else
        {
            const float u2 = (s - AG(vr_agp_tw) - AG(vr_agp_ts)) / AG(vr_agp_tr);
            (void)tr;
            fist = agLerp(hit, guard, agSmooth(u2));
            m = 1.f - (1.f - k) * agSmooth((u2 - 0.6f) / 0.4f);
        }
        k = m;
        if(left) { l = fist; } else { r = fist; }
    }
    agRow(AF(g, vr_ag_kind));
    const float kg = agMin(1.f, k / 0.75f);
    AG(vr_agp_wr) = 1.f;
    AGSET(vr_agp_v, agLerp(agHang(0.f), agFit(g, r), kg) - AGV(vr_am_anchor_at));
    AG(vr_agp_flags) = agfLeft;
    AG(vr_agp_wl) = 1.f;
    AGSET(vr_agp_vl, agLerp(agHang(1.f), agFit(g, l), kg) - AGV(vr_am_anchor_at));
}

void agSwingKeys(float kind) // VR_AGP_SwingKeys
{
    if(kind == 0) { AGSET(vr_agp_kw, (AgV{0, -7, 24})); AGSET(vr_agp_kwfw, (AgV{-0.55f, 0.05f, 0.83f})); AGSET(vr_agp_kc, (AgV{16, -6, 22})); AGSET(vr_agp_ke, (AgV{17, -3, 2})); AGSET(vr_agp_kefw, (AgV{0.6f, 0, -0.8f})); }
    else if(kind == 1) { AGSET(vr_agp_kw, (AgV{1, -14, 21})); AGSET(vr_agp_kwfw, (AgV{-0.4f, -0.55f, 0.73f})); AGSET(vr_agp_kc, (AgV{17, -10, 16})); AGSET(vr_agp_ke, (AgV{14, 7, 0})); AGSET(vr_agp_kefw, (AgV{0.5f, 0.55f, -0.67f})); }
    else if(kind == 2) { AGSET(vr_agp_kw, (AgV{3, -17, 12})); AGSET(vr_agp_kwfw, (AgV{-0.45f, -0.85f, 0.25f})); AGSET(vr_agp_kc, (AgV{20, -6, 12})); AGSET(vr_agp_ke, (AgV{9, 13, 10})); AGSET(vr_agp_kefw, (AgV{0.25f, 0.95f, 0.1f})); }
    else { AGSET(vr_agp_kw, (AgV{8, 12, 6})); AGSET(vr_agp_kwfw, (AgV{0.1f, 0.8f, -0.55f})); AGSET(vr_agp_kc, (AgV{19, 2, 12})); AGSET(vr_agp_ke, (AgV{10, -14, 20})); AGSET(vr_agp_kefw, (AgV{0.3f, -0.7f, 0.65f})); }
}
[[nodiscard]] bool agRightSwings(edict_t* g) // VR_AGP_RightSwings
{
    return !agHas(AFE(g, vr_ag_prop2)) || AF(g, vr_ag_act_hand) != 1.f;
}
[[nodiscard]] bool agSwingOneHand(edict_t* g, edict_t* p) // VR_AGP_SwingOneHand
{
    return AF(g, vr_ag_act) == agSwing && agHas(p) && p->v.weapon != WID_CHAINSAW && !agHas(AFE(g, vr_ag_prop2));
}
[[nodiscard]] AgV agArc(AgV v, AgV a, AgV b) // VR_AGP_Arc
{
    AgV k = agCross(a, b);
    const float s = agVlen(k);
    const float c = agDot(a, b);
    if(s < 0.00001f)
    {
        if(c > 0)
        {
            return v;
        }
        k = agCross(a, agFabs(a.z) < 0.9f ? AgV{0.f, 0.f, 1.f} : AgV{1.f, 0.f, 0.f});
        return agRotate(v, agNormalize(k), 180.f);
    }
    return agRotate(v, k * (1.f / s), agAtan2(s, c) * agToDeg);
}
void agGeo(edict_t* p) // VR_AGP_Geo (QuakeC's, once a weapon)
{
    if(AF(p, vr_agp_geo) != 0.f)
    {
        return;
    }
    agArgE(0, p);
    agRun(agp.fn_VR_AGP_Geo, 1);
}
void agSawHold(edict_t* g, float rdy) // VR_AGP_SawHold
{
    agRow(AF(g, vr_ag_kind));
    const AgV g0 = agNormalize(AGV(vr_am_gun_fw));
    const AgV d = rdy != 0.f ? agNormalize(AgV{0.88f, 0.45f, 0.1f}) : agNormalize(g0 + AgV{0.5f, 0.f, -0.25f});
    const AgV at = AGV(vr_am_sh_r) + agArc(AGV(vr_am_grip_r) - AGV(vr_am_sh_r), g0, d);
    const AgV sh = AGV(vr_am_fit_shift), sc = AGV(vr_am_fit_scale);
    AGSET(vr_agp_sawat, (AgV{(at.x - sh.x) / sc.x, (at.y - sh.y) / sc.y, (at.z - sh.z) / sc.z}));
    AGSET(vr_agp_sawfw, d);
}
[[nodiscard]] AgV agSawReach(edict_t* p, AgV at, AgV fw) // VR_AGP_SawReach
{
    agGeo(p);
    const AgV sec = AFV(p, vr_agp_sec);
    if(agZero(sec) || sec.z >= 999)
    {
        return at;
    }
    fw = agNormalize(fw);
    const AgV up = agNormalize(AgV{0.f, 0.f, 1.f} - fw * fw.z);
    const AgV side = agCross(up, fw);
    const AgV off = fw * sec.x + side * sec.y + up * sec.z;
    const float most = 0.9f * AG(vr_am_arm);
    const AgV shL = AGV(vr_am_sh_l), shR = AGV(vr_am_sh_r);
    for(float i = 0; i < 3; i++)
    {
        const AgV h = at + off;
        const float d = agVlen(h - shL);
        if(d > most)
        {
            at = at - agNormalize(h - shL) * (d - most);
        }
        const float mr = 0.92f * AG(vr_am_arm);
        const float dr = agVlen(at - shR);
        if(dr > mr)
        {
            at = shR + (at - shR) * (mr / dr);
        }
    }
    return at;
}
void agOrient(AgV t, AgV b, AgV r, AgV hint) // VR_AGP_Orient
{
    b = agNormalize(b);
    AgV x0 = agArc({1.f, 0.f, 0.f}, t, b);
    AgV z0 = agArc({0.f, 0.f, 1.f}, t, b);
    const AgV r0 = agArc(r, t, b);
    AgV rp = r0 - b * agDot(r0, b);
    AgV hp = hint - b * agDot(hint, b);
    if(agVlen(rp) > 0.001f && agVlen(hp) > 0.001f)
    {
        rp = agNormalize(rp);
        hp = agNormalize(hp);
        const float ang = agAtan2(agDot(agCross(rp, hp), b), agDot(rp, hp)) * agToDeg;
        x0 = agRotate(x0, b, ang);
        z0 = agRotate(z0, b, ang);
    }
    const AgV fw = agNormalize(x0);
    AGSET(vr_agp_fw, fw);
    AGSET(vr_agp_up, agNormalize(z0 - fw * agDot(z0, fw)));
}
[[nodiscard]] AgV agInFrame(AgV l) // VR_AGP_InFrame
{
    const AgV fw = AGV(vr_agp_fw), up = AGV(vr_agp_up);
    return fw * l.x + agCross(up, fw) * l.y + up * l.z;
}
[[nodiscard]] AgV agRunBob(edict_t* g) // VR_AGP_RunBob
{
    agRow(AF(g, vr_ag_kind));
    const float fr = AG(vr_am_f_run);
    const bool running = g->v.frame >= fr && g->v.frame < fr + 8;
    AF(g, vr_agp_runw) = agEase(AF(g, vr_agp_runw), running ? 1.f : 0.f, 4.f);
    const float ph = agTime() * 1.25f * 6.2832f;
    const float w = AF(g, vr_agp_runw);
    return AgV{0.f, 0.f, 1.f} * (0.8f * w * agSin(ph * 2.f)) + AgV{0.f, 1.f, 0.f} * (0.6f * w * agSin(ph));
}
void agBladeHand(edict_t* p, AgV fw, AgV edge) // VR_AGP_BladeHand
{
    agGeo(p);
    const AgV muz = AFV(p, vr_agp_muz);
    const AgV t = agVlen(muz) > 1 ? agNormalize(muz) : AgV{1.f, 0.f, 0.f};
    agOrient(t, fw, {1.f, 0.f, 0.f}, edge);
}

void agMelee(edict_t* g, edict_t* p) // VR_AGP_Melee
{
    const bool saw = p->v.weapon == WID_CHAINSAW;
    AF(g, vr_ag_readyw) = agEase(AF(g, vr_ag_readyw), 0.7f, 3.f);
    AgV ready = saw ? AgV{9.f, -7.f, 1.f} : AgV{10.f, -7.f, 4.f}; // VR_AGP_SAW_READY, VR_AGP_READY
    AgV readyFw = agNormalize(saw ? AgV{0.95f, 0.05f, 0.25f} : AgV{0.3f, 0.1f, 0.95f});
    AF(g, vr_agp_rdy) = agEase(AF(g, vr_agp_rdy), agHas(PROG_TO_EDICT(g->v.enemy)) ? 1.f : 0.f, 2.5f);
    const float rk = agSmooth(AF(g, vr_agp_rdy));
    const bool two = agTwoHanded(p);
    if(saw)
    {
        agSawHold(g, 0.f);
        const AgV sawCarry = AGV(vr_agp_sawat);
        const AgV sawCarryFw = AGV(vr_agp_sawfw);
        agSawHold(g, 1.f);
        ready = agLerp(sawCarry, AGV(vr_agp_sawat), rk);
        readyFw = agNlerp(sawCarryFw, AGV(vr_agp_sawfw), rk);
    }
    else
    {
        ready = agLerp(two ? AgV{9.f, -5.f, 3.f} : AgV{6.f, -9.f, 3.f}, ready, rk); // VR_AGP_PORT, VR_AGP_CARRY_R
        readyFw = agNlerp(agNormalize(two ? AgV{0.35f, 0.55f, 0.75f} : AgV{0.95f, -0.1f, 0.15f}), readyFw, rk);
    }
    AgV pos = ready;
    AgV fw = readyFw;
    AgV edge{1.f, 0.f, 0.f};
    const float s = agTime() - AF(g, vr_ag_act_at);
    const float act = AF(g, vr_ag_act);
    if((act == agSwing || act == agSaw) && agRightSwings(g))
    {
        agTimes(g);
        const AgV tl = agTargetLocal(g);
        const float dz = agMin(12.f, agMax(-12.f, tl.z - 8.f));
        AgV w, wfw, c, e, efw;
        const float kind = AF(g, vr_ag_act_kind);
        if(saw)
        {
            w = {5.f, -8.f, 4.f};
            wfw = {0.95f, 0.f, 0.3f};
            e = {14.f, -2.f, 0.f};
            e.z = e.z + dz;
            efw = agNormalize(tl - e);
            c = (w + e) * 0.5f;
        }
        else
        {
            agSwingKeys(kind);
            w = AGV(vr_agp_kw);
            wfw = AGV(vr_agp_kwfw);
            c = AGV(vr_agp_kc);
            e = AGV(vr_agp_ke);
            efw = AGV(vr_agp_kefw);
        }
        if(!saw)
        {
            c.z = c.z + dz * 0.5f;
            e.z = e.z + dz;
        }
        wfw = agNormalize(wfw);
        efw = agNormalize(efw);
        const AgV cfw = agNormalize(c - AgV{0.f, -9.7f, 12.f}) * 2.f;
        edge = agNormalize(efw - wfw);
        const float tw = AG(vr_agp_tw), ts = AG(vr_agp_ts), th = AG(vr_agp_th), tr = AG(vr_agp_tr);
        const float tStrike = tw + ts;
        if(s < tw)
        {
            const float u = agSmooth(s / tw);
            pos = agLerp(ready, w, u);
            fw = agNlerp(readyFw, wfw, u);
        }
        else if(s < tStrike)
        {
            const float u1 = agSmooth((s - tw) / ts);
            pos = agBezier(w, c, e, u1);
            fw = agNormalize(agBezier(wfw, cfw, efw, u1));
            if(AF(g, vr_ag_act_hit) == 0.f && u1 >= 0.55f && !saw)
            {
                AF(g, vr_ag_act_hit) = 1.f;
                agBody(g);
                const AgV from = agOrigin(g) + agToWorld(pos);
                agStrike(g, p, from, agNormalize(agToWorld(tl - pos)));
                agBody(g);
            }
        }
        else if(s < tStrike + th)
        {
            const float u3 = (s - tStrike) / agMax(th, 0.01f);
            pos = e + efw * (1.5f * agSin(u3 * 6.2832f * 3.f));
            fw = efw;
        }
        else
        {
            const float u2 = agSmooth((s - tStrike - th) / tr);
            pos = agLerp(e, ready, u2);
            fw = agNlerp(efw, readyFw, u2);
        }
    }
    edge = agNormalize(edge - fw * agDot(edge, fw));
    if(agZero(edge)) { edge = {0.f, 0.f, 1.f}; }
    agBody(g);
    agRow(AF(g, vr_ag_kind));
    AG(vr_agp_wr) = 1.f;
    AGSET(vr_agp_v, agFit(g, pos) - AGV(vr_am_anchor_at));
    if(saw)
    {
        AGSET(vr_agp_v, agSawReach(p, AGV(vr_agp_v) + AGV(vr_am_anchor_at), fw) - AGV(vr_am_anchor_at));
    }
    AGSET(vr_agp_fw, agToWorld(fw));
    AGSET(vr_agp_up, agToWorld(edge));
    AG(vr_agp_wo) = 0.f;
    if(!two || agSwingOneHand(g, p))
    {
        AG(vr_agp_flags) = agfLeft;
        AG(vr_agp_wl) = 1.f;
        AGSET(vr_agp_vl, agLerp(agHang(1.f), agFit(g, {7.f, 9.f, 4.f}), rk) - AGV(vr_am_anchor_at)); // VR_AGP_FREE_L
    }
    else if((agHands(g, p) == 2.f) && (AG(vr_agh_set) == 2.f))
    {
        AG(vr_agp_flags) = agfTwo;
    }
}

void agMeleeLeft(edict_t* g, edict_t* p) // VR_AGP_MeleeLeft
{
    const float r = agSmooth(AF(g, vr_agp_rdy));
    const AgV carry = agMirror({6.f, -9.f, 3.f});
    const AgV carryFw = agMirror({0.95f, -0.1f, 0.15f});
    AgV pos = agLerp(carry, {10.f, 7.f, 4.f}, r); // VR_AGP_READY_L
    const AgV readyFw = agNlerp(agNormalize(carryFw), agNormalize({0.3f, -0.1f, 0.95f}), r);
    AgV fw = readyFw;
    const AgV ready = pos;
    AgV edge{1.f, 0.f, 0.f};
    const float s = agTime() - AF(g, vr_ag_act_at);
    if(AF(g, vr_ag_act) == agSwing && AF(g, vr_ag_act_hand) != 0.f)
    {
        agTimes(g);
        const AgV tl = agTargetLocal(g);
        const float dz = agMin(12.f, agMax(-12.f, tl.z - 8.f));
        agSwingKeys(AF(g, vr_ag_act_kind));
        const AgV w = agMirror(AGV(vr_agp_kw));
        const AgV wfw = agNormalize(agMirror(AGV(vr_agp_kwfw)));
        AgV c = agMirror(AGV(vr_agp_kc));
        AgV e = agMirror(AGV(vr_agp_ke));
        const AgV efw = agNormalize(agMirror(AGV(vr_agp_kefw)));
        c.z = c.z + dz * 0.5f;
        e.z = e.z + dz;
        const AgV cfw = agNormalize(c - AgV{0.f, 9.7f, 12.f}) * 2.f;
        edge = agNormalize(efw - wfw);
        const float tw = AG(vr_agp_tw), ts = AG(vr_agp_ts), tr = AG(vr_agp_tr);
        const float tStrike = tw + ts;
        if(s < tw)
        {
            const float u = agSmooth(s / tw);
            pos = agLerp(ready, w, u);
            fw = agNlerp(readyFw, wfw, u);
        }
        else if(s < tStrike)
        {
            const float u1 = agSmooth((s - tw) / ts);
            pos = agBezier(w, c, e, u1);
            fw = agNormalize(agBezier(wfw, cfw, efw, u1));
            if(AF(g, vr_ag_act_hitl) == 0.f && AF(g, vr_ag_act_hit) != 2.f && u1 >= 0.55f)
            {
                AF(g, vr_ag_act_hitl) = 1.f;
                agBody(g);
                agStrike(g, p, agOrigin(g) + agToWorld(pos), agNormalize(agToWorld(tl - pos)));
                AF(g, vr_ag_strikes_l) = AF(g, vr_ag_strikes_l) + 1.f;
            }
        }
        else
        {
            const float u2 = agSmooth((s - tStrike) / tr);
            pos = agLerp(e, ready, u2);
            fw = agNlerp(efw, readyFw, u2);
        }
    }
    edge = agNormalize(edge - fw * agDot(edge, fw));
    if(agZero(edge)) { edge = {0.f, 0.f, 1.f}; }
    agBody(g);
    AGSET(vr_agp_vl, agFit(g, pos));
    AGSET(vr_agp_fwl, agToWorld(fw));
    AGSET(vr_agp_upl, agToWorld(edge));
}
void agLeftBlade(edict_t* g, edict_t* pL) // VR_AGP_LeftBlade
{
    agMeleeLeft(g, pL);
    const AgV keepFw = AGV(vr_agp_fw);
    const AgV keepUp = AGV(vr_agp_up);
    agBladeHand(pL, AGV(vr_agp_fwl), AGV(vr_agp_upl));
    AGSET(vr_agp_fwl, AGV(vr_agp_fw));
    AGSET(vr_agp_upl, AGV(vr_agp_up));
    AGSET(vr_agp_fw, keepFw);
    AGSET(vr_agp_up, keepUp);
    agRow(AF(g, vr_ag_kind));
    AGSET(vr_agp_vl, AGV(vr_agp_vl) - AGV(vr_am_anchor_at));
    AG(vr_agp_wl) = 1.f;
    AG(vr_agp_flags) = agOr(agfLeft, agfLeftWpn);
}

[[nodiscard]] bool agLong(edict_t* p) // VR_AGP_Long
{
    agGeo(p);
    const AgV sec = AFV(p, vr_agp_sec);
    return (!agZero(sec) && sec.z < 999) || AFV(p, vr_agp_butt).x < -7;
}
void agArmsOne(edict_t* g, edict_t* p, float left, float a, AgV tl) // VR_AGP_ArmsOne
{
    (void)g;
    agGeo(p);
    const AgV sh = left != 0.f ? AGV(vr_am_sh_l) : AGV(vr_am_sh_r);
    const float arm = AG(vr_am_arm);
    const float reach = arm + 2.f;
    const AgV side = left != 0.f ? AgV{0.f, 1.f, 0.f} : AgV{0.f, -1.f, 0.f};
    const AgV sh2 = sh - AgV{0.f, 0.f, 3.f} * (arm / 16.7f);
    const AgV hold = sh2 + agNormalize(tl - sh2) * (0.85f * reach);
    const AgV low = sh + agNormalize(AgV{0.35f, 0.f, -0.94f} + side * 0.35f) * (0.8f * reach);
    const AgV at = agLerp(low, hold, a);
    AgV d = agNormalize(agLerp(agNormalize(AgV{0.75f, 0.f, -0.65f} + side * 0.1f), agNormalize(tl - at), a));
    agOrient({1.f, 0.f, 0.f}, d, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f});
    if(a > 0)
    {
        d = agNormalize(agLerp(d, agNormalize(tl - (at + agInFrame(AFV(p, vr_agp_muz)))), a));
        agOrient({1.f, 0.f, 0.f}, d, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f});
    }
    AGSET(vr_agp_gunat, at);
}
[[nodiscard]] AgV agSecondReach(edict_t* p, AgV at, float most) // VR_AGP_SecondReach
{
    agGeo(p);
    const AgV sec = AFV(p, vr_agp_sec);
    if(agZero(sec) || sec.z >= 999)
    {
        return at;
    }
    const AgV off = agInFrame(sec);
    const float reachL = most;
    const AgV shL = AGV(vr_am_sh_l), shR = AGV(vr_am_sh_r);
    for(float i = 0; i < 3; i++)
    {
        const AgV h = at + off;
        const float d = agVlen(h - shL);
        if(d <= reachL)
        {
            break;
        }
        at = at - agNormalize(h - shL) * (d - reachL);
        const AgV fromSh = at - shR;
        if(agVlen(fromSh) > most)
        {
            at = shR + agNormalize(fromSh) * most;
        }
    }
    return at;
}
[[nodiscard]] float agKickLeft(edict_t* p) // VR_AG_KickLeft
{
    const float t = agMax(0.01f, ACV(vr_heavy_recoil_time));
    return AF(p, vr_ag_hk) * agMax(0.f, 1.f - (agTime() - AF(p, vr_ag_hk_at)) / t);
}
[[nodiscard]] float agReloadTime(edict_t* g) // VR_AG_ReloadTime
{
    float t = agMax(0.02f, ACV(vr_agrunt_reload_time));
    edict_t* prop = AFE(g, vr_ag_prop);
    if(agHas(prop) && prop->v.weapon == WID_SUPER_SHOTGUN)
    {
        t = t * agMax(0.05f, ACV(vr_agrunt_reload_ssg));
    }
    return t;
}

void agArmsGun(edict_t* g, edict_t* p, edict_t* pL) // VR_AGP_ArmsGun
{
    agRow(AF(g, vr_ag_kind));
    const AgV aim = AFV(g, vr_ag_aim);
    if(!agZero(aim))
    {
        agStore(AFP(g, vr_ag_aimdir), aim);
    }
    AF(g, vr_ag_aimw) = agEase(AF(g, vr_ag_aimw), !agZero(aim) ? 1.f : 0.f, !agZero(aim) ? 7.f : 4.f);
    const float a = agSmooth(AF(g, vr_ag_aimw));
    agBody(g);
    AgV tl = agHas(PROG_TO_EDICT(g->v.enemy)) ? agTargetLocal(g) : AgV{200.f, 0.f, 10.f};
    const AgV aimdir = AFV(g, vr_ag_aimdir);
    if(!agZero(aimdir) && agHas(p))
    {
        tl = agToLocal(agOrigin(p) - agOrigin(g)) + agToLocal(aimdir) * agMax(64.f, agVlen(tl));
    }
    const float fl = agVlen({tl.x, tl.y, 0.f});
    if(fl > 1 && tl.x < fl * 0.866f)
    {
        const float ty = tl.y > 0 ? 0.5f : -0.5f;
        tl.x = fl * 0.866f;
        tl.y = fl * ty;
    }
    const float arm = AG(vr_am_arm);
    const float scale = arm / 16.7f;
    AgV gR, fwR, upR;
    const AgV shR = AGV(vr_am_sh_r), gripR = AGV(vr_am_grip_r);
    if(agHas(pL) || AG(vr_agp_onehand) != 0.f || !agLong(p))
    {
        agArmsOne(g, p, 0.f, a, tl);
        gR = AGV(vr_agp_gunat);
    }
    else
    {
        const AgV g0 = agNormalize(AGV(vr_am_gun_fw));
        AgV butt = AFV(p, vr_agp_butt);
        butt.x = agMin(butt.x, -0.5f * arm);
        const AgV pocket = shR + AgV{1.5f, 2.5f, -1.5f} * scale;
        const float sh = a * agMin(1.f, agMax(0.f, ACV(vr_agarms_shoulder)));
        AgV d = agNormalize(agLerp(agNormalize(g0 + AgV{0.5f, 0.f, -0.25f}), agNormalize(tl - agLerp(gripR, pocket, sh)), a));
        agOrient({1.f, 0.f, 0.f}, d, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f});
        gR = agLerp(shR + agArc(gripR - shR, g0, d), pocket - agInFrame(butt), sh);
        if(a > 0)
        {
            d = agNormalize(agLerp(d, agNormalize(tl - (gR + agInFrame(AFV(p, vr_agp_muz)))), a));
            agOrient({1.f, 0.f, 0.f}, d, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f});
            gR = agLerp(shR + agArc(gripR - shR, g0, d), pocket - agInFrame(butt), sh);
        }
        const AgV fromSh = gR - shR;
        const float most = 0.92f * (arm + 2.5f);
        if(agVlen(fromSh) > most)
        {
            gR = shR + agNormalize(fromSh) * most;
        }
        if(ACV(vr_agrunt_second_reach) != 0.f)
        {
            gR = agLerp(agSecondReach(p, gR, most), gR, a);
        }
    }
    fwR = AGV(vr_agp_fw);
    upR = AGV(vr_agp_up);
    const float kickAt = agHas(pL) ? AF(p, vr_ag_kick_at) : AF(g, vr_ag_kick_at);
    const float k = 1.f - (agTime() - kickAt) / 0.2f;
    const AgV right = agCross(fwR, upR);
    if(k > 0 && kickAt > 0)
    {
        const float kk = k * k * (agClass(p->v.weapon) == VR_AG_CLASS_EXPLOSIVE ? 1.5f : 1.f);
        gR = gR - fwR * (3.f * kk) + AgV{0.f, 0.f, 1.f} * kk;
        fwR = agRotate(fwR, right, 12.f * kk);
        upR = agRotate(upR, right, 12.f * kk);
    }
    const float hk = agKickLeft(p);
    if(hk > 0)
    {
        fwR = agRotate(fwR, right, hk);
        upR = agRotate(upR, right, hk);
    }
    const float rl = AF(g, vr_agp_rl_at) > 0 ? (agTime() - AF(g, vr_agp_rl_at)) / agMax(0.1f, agReloadTime(g)) : -1.f;
    if((rl >= 0) && (rl <= 1) && (!agHas(pL)))
    {
        const float s = agSin(rl * agPi);
        fwR = agRotate(fwR, right, -25.f * s);
        upR = agRotate(upR, right, -25.f * s);
        upR = agRotate(upR, fwR, -30.f * s);
        gR = gR + AgV{1.f, 2.f, -1.f} * s;
        const AgV belt = AGV(vr_am_sh_l) + AgV{3.f, -4.f, -15.f} * scale;
        AGSET(vr_agp_fw, fwR);
        AGSET(vr_agp_up, upR);
        AgV magL = AFV(p, vr_agp_muz) * 0.4f;
        const AgV sec = AFV(p, vr_agp_sec);
        if((!agZero(sec)) && (sec.z < 999))
        {
            magL = sec;
        }
        const AgV mag = gR + agInFrame(magL);
        const float u = rl < 0.4f ? agSmooth(rl / 0.4f) : 1.f - agSmooth((rl - 0.4f) / 0.45f);
        AG(vr_agp_flags) = agOr(AG(vr_agp_flags), agfLeft);
        AG(vr_agp_wl) = 1.f;
        AGSET(vr_agp_vl, agLerp(mag, belt, u) - AGV(vr_am_anchor_at));
    }
    AG(vr_agp_wr) = 1.f;
    AGSET(vr_agp_v, gR - AGV(vr_am_anchor_at));
    AGSET(vr_agp_fw, agToWorld(fwR));
    AGSET(vr_agp_up, agToWorld(upR));
    AG(vr_agp_wo) = 0.f;
    if(agHas(pL))
    {
        agArmsOne(g, pL, 1.f, a, tl);
        AgV gL = AGV(vr_agp_gunat);
        AgV fwL = AGV(vr_agp_fw);
        AgV upL = AGV(vr_agp_up);
        const float kl = 1.f - (agTime() - AF(pL, vr_ag_kick_at)) / 0.2f;
        if(kl > 0 && AF(pL, vr_ag_kick_at) > 0)
        {
            const AgV rightL = agCross(fwL, upL);
            const float kkl = kl * kl;
            gL = gL - fwL * (3.f * kkl) + AgV{0.f, 0.f, 1.f} * kkl;
            fwL = agRotate(fwL, rightL, 12.f * kkl);
            upL = agRotate(upL, rightL, 12.f * kkl);
        }
        AGSET(vr_agp_fwl, agToWorld(fwL));
        AGSET(vr_agp_upl, agToWorld(upL));
        AGSET(vr_agp_fw, agToWorld(fwR));
        AGSET(vr_agp_up, agToWorld(upR));
        AG(vr_agp_wl) = 1.f;
        AGSET(vr_agp_vl, gL - AGV(vr_am_anchor_at));
        AG(vr_agp_flags) = agOr(agfLeft, agfLeftWpn);
    }
}

void agPhase(edict_t* g) // VR_AGP_Phase
{
    agTimes(g);
    const float tw = AG(vr_agp_tw), ts = AG(vr_agp_ts), th = AG(vr_agp_th), tr = AG(vr_agp_tr);
    const float s = agTime() - AF(g, vr_ag_act_at);
    const float tS = tw + ts;
    const bool punch = AF(g, vr_ag_act) == agPunch;
    const float follow = AF(g, vr_ag_act) == agSaw ? 0.f : 1.f;
    const float h = punch ? 0.8f : 0.55f;
    float ww = 0.f, wf = 0.f;
    if(s < tw)
    {
        ww = agSmooth(s / tw);
    }
    else if(s < tS)
    {
        float u = (s - tw) / ts;
        if(!punch)
        {
            u = agSmooth(u);
        }
        if(u < h)
        {
            ww = punch ? 1.f - agSmooth(u / h) : agCos(1.5708f * u / h);
        }
        else
        {
            wf = follow * (punch ? agSmooth((u - h) / (1.f - h)) : agSin(1.5708f * (u - h) / (1.f - h)));
        }
    }
    else if(s < tS + th)
    {
        wf = follow;
    }
    else
    {
        wf = follow * (1.f - agSmooth((s - tS - th) / agMax(tr, 0.01f)));
    }
    AG(vr_agp_ww) = ww;
    AG(vr_agp_wf) = wf;
}
[[nodiscard]] bool agTorsoNow(edict_t* g) // VR_AGP_TorsoNow
{
    const AgV zero{0.f, 0.f, 0.f};
    AGSET(vr_agp_tq, zero);
    AGSET(vr_agp_drawr, zero);
    AGSET(vr_agp_drawl, zero);
    AGSET(vr_agp_elr, zero);
    AGSET(vr_agp_ell, zero);
    const float act = AF(g, vr_ag_act);
    if(act != agPunch && act != agSwing && act != agSaw)
    {
        return false;
    }
    const float torso = ACV(vr_agrunt_strike_torso);
    const float shoulder = ACV(vr_agrunt_strike_shoulder);
    if(torso <= 0 && shoulder <= 0)
    {
        return false;
    }
    agPhase(g);
    AgV w, f, draw, elbow;
    bool left = false;
    bool both = false;
    const float kind = AF(g, vr_ag_act_kind);
    if(act == agPunch)
    {
        left = (kind == 0 || kind == 2);
        if(kind == 0)      { w = {-6, -1, -1};  f = {12, 4, 2};   draw = {-2, 0, 1};  elbow = {-1, -0.3f, -0.5f}; }
        else if(kind == 1) { w = {-16, -3, -3}; f = {26, 6, 4};   draw = {-4, 0, 1};  elbow = {-1, -0.4f, -0.3f}; }
        else if(kind == 4) { w = {-12, 8, 5};   f = {16, -7, -4}; draw = {-2, 0, -4}; elbow = {-0.6f, -0.3f, -1}; }
        else               { w = {-22, 0, -4};  f = {30, 4, 5};   draw = {-2, -3, 1}; elbow = {-0.3f, -1, 0.5f}; }
    }
    else if(act == agSaw)
    {
        w = {-14, -5, -2}; f = {0, 0, 0}; draw = {-4, 0, 0}; elbow = {-1, -0.5f, -0.3f};
    }
    else
    {
        if(kind == 0)      { w = {-14, -9, -7}; f = {12, 15, 6};   draw = {0, 0, 0};  elbow = {-0.4f, -0.7f, 0.8f}; }
        else if(kind == 1) { w = {-28, -5, -8}; f = {24, 10, 8};   draw = {-5, -1, 4}; elbow = {-0.5f, -0.8f, 0.6f}; }
        else if(kind == 2) { w = {-32, 0, -3};  f = {28, 4, 3};    draw = {-5, -2, 1}; elbow = {-0.6f, -0.8f, 0.3f}; }
        else               { w = {16, 4, 5};    f = {-20, -5, -6}; draw = {3, 3, 1};  elbow = {0.3f, -0.6f, 0.5f}; }
        edict_t* prop = AFE(g, vr_ag_prop);
        if(agHas(prop) && agHeavy(prop))
        {
            w = w * 1.15f;
            f = f * 1.15f;
        }
        if(agHas(AFE(g, vr_ag_prop2)))
        {
            left = AF(g, vr_ag_act_hand) == 1.f;
            both = AF(g, vr_ag_act_hand) == 2.f;
        }
    }
    const float ww = AG(vr_agp_ww), wf = AG(vr_agp_wf);
    const float v = AF(g, vr_ag_act_var);
    const float a = 0.8f + 0.4f * v;
    const float v2 = v * 7.31f;
    const float b = 0.8f + 0.4f * (v2 - agFloor(v2));
    const float side = left ? -1.f : 1.f;
    const float twist = both ? 0.f : 1.f;
    AgV tq;
    tq.x = (w.x * ww + f.x * wf) * a * torso * side * twist;
    tq.y = (w.y * ww + f.y * wf) * b * torso;
    tq.z = (w.z * ww + f.z * wf) * b * shoulder * side * twist;
    AGSET(vr_agp_tq, tq);
    draw = draw * (ww * shoulder * b);
    elbow = agNormalize(elbow) * (0.8f * ww * agMin(1.f, shoulder));
    if(!left || both)
    {
        AGSET(vr_agp_drawr, draw);
        AGSET(vr_agp_elr, elbow);
    }
    if(left || both)
    {
        AGSET(vr_agp_drawl, agMirror(draw));
        AGSET(vr_agp_ell, agMirror(elbow));
    }
    return true;
}
[[nodiscard]] AgV agTurnDir(AgV d) // VR_AGP_TurnDir (turnrpy)
{
    const AgV tq = AGV(vr_agp_tq);
    if(!agEq(tq, AGV(vr_agp_trq)))
    {
        AGSET(vr_agp_trq, tq);
        const float r = tq.z * agToRad;
        const float p = tq.y * agToRad;
        const float y = tq.x * agToRad;
        AGSET(vr_agp_trc, (AgV{agCos(r), agCos(p), agCos(y)}));
        AGSET(vr_agp_trs, (AgV{agSin(r), agSin(p), agSin(y)}));
    }
    agArgV(0, d);
    agArgV(1, AGV(vr_agp_trc));
    agArgV(2, AGV(vr_agp_trs));
    qcvm->argc = 3;
    PF_turnrpy();
    return agRetV();
}
[[nodiscard]] AgV agTurnPose(edict_t* g, AgV posL, AgV handL) // VR_AGP_TurnPose
{
    const AgV piv = agFit(g, {-4.7f, 0.9f, 4.8f}); // VR_AGP_WAIST
    const AgV o = agFit(g, {0.f, 0.f, 0.f});
    agRow(AF(g, vr_ag_kind));
    const float arm = AG(vr_am_arm);
    const AgV shR = AGV(vr_am_sh_r), shL = AGV(vr_am_sh_l);
    const AgV mv = handL - AGV(vr_am_anchor_at);
    const float fr = agMin(1.f, agMax(0.f, (0.85f * arm - agVlen(posL - (shR + mv))) / (0.2f * arm)));
    const float fl = agMin(1.f, agMax(0.f, (0.85f * arm - agVlen(handL * AG(vr_agp_wl) + AGV(vr_agp_vl) - (shL + mv))) /
        (0.2f * arm)));
    AgV pr = posL + (agFit(g, AGV(vr_agp_drawr)) - o) * fr;
    pr = piv + agTurnDir(pr - piv);
    AgV pl = handL * AG(vr_agp_wl) + AGV(vr_agp_vl) + (agFit(g, AGV(vr_agp_drawl)) - o) * fl;
    pl = piv + agTurnDir(pl - piv);
    agRow(AF(g, vr_ag_kind));
    const AgV moved = handL - AGV(vr_am_anchor_at);
    float far = agVlen(posL - (shR + moved));
    AgV sh = piv + agTurnDir(shR + moved - piv);
    if(agVlen(pr - sh) > far)
    {
        pr = sh + agNormalize(pr - sh) * far;
    }
    far = agVlen(handL * AG(vr_agp_wl) + AGV(vr_agp_vl) - (shL + moved));
    sh = piv + agTurnDir(shL + moved - piv);
    if(agAnd(AG(vr_agp_flags), agfLeft) != 0.f && agVlen(pl - sh) > far)
    {
        pl = sh + agNormalize(pl - sh) * far;
    }
    AGSET(vr_agp_v, AGV(vr_agp_v) + (pr - posL));
    AGSET(vr_agp_vl, pl - handL * AG(vr_agp_wl));
    agBody(g);
    AGSET(vr_agp_fw, agToWorld(agTurnDir(agToLocal(AGV(vr_agp_fw)))));
    AGSET(vr_agp_up, agToWorld(agTurnDir(agToLocal(AGV(vr_agp_up)))));
    if(agAnd(AG(vr_agp_flags), agfLeftWpn) != 0.f)
    {
        AGSET(vr_agp_fwl, agToWorld(agTurnDir(agToLocal(AGV(vr_agp_fwl)))));
        AGSET(vr_agp_upl, agToWorld(agTurnDir(agToLocal(AGV(vr_agp_upl)))));
    }
    return pr;
}

// The Pose Studio's keys (VR_AGK_*).
[[nodiscard]] bool agkIsGun(float kclass) // VR_AGK_IsGun
{
    return kclass == VR_AGK_GUN || kclass == VR_AGK_TWOGUNS || (kclass >= VR_AGK_PISTOL && kclass <= VR_AGK_WIDOWMAKER);
}
[[nodiscard]] float agkClass(edict_t* g, edict_t* p) // VR_AGK_Class
{
    if(!agHas(p))
    {
        return VR_AGK_FISTS;
    }
    const float c = agClass(p->v.weapon);
    if(agHas(AFE(g, vr_ag_prop2)))
    {
        return c == VR_AG_CLASS_MELEE ? VR_AGK_TWOMELEE : VR_AGK_TWOGUNS;
    }
    if(c == VR_AG_CLASS_MELEE)
    {
        if(p->v.weapon != WID_CHAINSAW && twohand::poleWeapon(weapons::slotForName(PR_GetString(p->v.model))))
        {
            return VR_AGK_POLE; // (QC's: a pole weapon's own keys; its holder posed by QuakeC, agPose)
        }
        return p->v.weapon == WID_CHAINSAW ? VR_AGK_SAW : (agTwoHanded(p) ? VR_AGK_MELEE2 : VR_AGK_MELEE);
    }
    const AgV h = agWdp(agWorld(), agPoseKind(AF(g, vr_ag_kind)) * 1000.f + agHoldCode(p), 24.f);
    return h.z != 0.f ? h.z : static_cast<float>(VR_AGK_GUN);
}
[[nodiscard]] float agkTugStretch(edict_t* g) // VR_AGK_TugStretch
{
    const float dt = agMin(0.1f, agMax(0.f, agTime() - AF(g, vr_agk_tugt)));
    AF(g, vr_agk_tugt) = agTime();
    const float want = agHas(AFE(g, vr_ag_tug_pl)) ? 1.f : 0.f;
    const float step = dt / agc_VR_AGK_TUG_EASE;
    const float tw = AF(g, vr_agk_tugw);
    AF(g, vr_agk_tugw) = want > tw ? agMin(want, tw + step) : agMax(want, tw - step);
    const float t = AF(g, vr_agk_tugw);
    const float w = t * t * (3.f - 2.f * t);
    AG(vr_agk_tugs) = 1.f + (agMax(1.f, ACV(vr_agrunt_tug_ik_stretch)) - 1.f) * w;
    return AG(vr_agk_tugs);
}
void agkAdd(float code, float w, bool mirror) // VR_AGK_Add
{
    if(w <= 0.001f)
    {
        return;
    }
    if(AG(vr_agk_rep) != 0.f)
    {
        const float wr = agMin(w, 0.99f);
        (void)agWdp(PROG_TO_EDICT(*reinterpret_cast<int*>(agp.g_vr_agk_repent)), mirror ? agNeg(code + wr) : code + wr,
            23.f);
    }
    agArgF(0, code);
    qcvm->argc = 1;
    PF_agposekey();
    if(agRetV().x == 0.f)
    {
        return;
    }
    AgV rp = AGV(vr_agk_k_rp), rr = AGV(vr_agk_k_rr), lp = AGV(vr_agk_k_lp), lr = AGV(vr_agk_k_lr);
    AgV er = AGV(vr_agk_k_er), el = AGV(vr_agk_k_el), t = AGV(vr_agk_k_t);
    if(mirror)
    {
        AgV k = rp;
        rp = agMirror(lp);
        lp = agMirror(k);
        k = rr;
        rr = {agNeg(lr.x), lr.y, agNeg(lr.z)};
        lr = {agNeg(k.x), k.y, agNeg(k.z)};
        k = er;
        er = agMirror(el);
        el = agMirror(k);
        t = {agNeg(t.x), t.y, agNeg(t.z)};
    }
    AGSET(vr_agk_rp, AGV(vr_agk_rp) + rp * w);
    AGSET(vr_agk_rr, AGV(vr_agk_rr) + rr * w);
    AGSET(vr_agk_lp, AGV(vr_agk_lp) + lp * w);
    AGSET(vr_agk_lr, AGV(vr_agk_lr) + lr * w);
    AGSET(vr_agk_er, AGV(vr_agk_er) + er * w);
    AGSET(vr_agk_el, AGV(vr_agk_el) + el * w);
    AGSET(vr_agk_t, AGV(vr_agk_t) + t * w);
    if(!mirror)
    {
        AGSET(vr_agk_gp, AGV(vr_agk_gp) + AGV(vr_agk_k_gp) * w);
        AGSET(vr_agk_gr, AGV(vr_agk_gr) + AGV(vr_agk_k_gr) * w);
    }
    AG(vr_agk_on) = 1.f;
}
[[nodiscard]] float agkMode(float code) // VR_AGK_Mode
{
    return agRint(agWdp(agWorld(), code, 27.f).x);
}
void agkBlend(edict_t* g, float code) // VR_AGK_Blend
{
    agTimes(g);
    agArgF(0, code);
    agArgF(1, agTime() - AF(g, vr_ag_act_at));
    agArgV(2, {AG(vr_agp_tw), AG(vr_agp_ts), AG(vr_agp_th)});
    agArgF(3, AG(vr_agp_tr));
    qcvm->argc = 4;
    PF_agstrikeblend();
    const AgV b = agRetV();
    AG(vr_agk_ba) = b.x;
    AG(vr_agk_bb) = b.y;
    AG(vr_agk_bf) = b.z;
}
void agkAddBlend(float code, bool mirror) // VR_AGK_AddBlend
{
    if(AG(vr_agk_ba) >= 0)
    {
        agkAdd(code + AG(vr_agk_ba), 1.f - AG(vr_agk_bf), mirror);
    }
    if(AG(vr_agk_bb) >= 0)
    {
        agkAdd(code + AG(vr_agk_bb), AG(vr_agk_bf), mirror);
    }
}
void agkKeys(edict_t* g, edict_t* p) // VR_AGK_Keys
{
    const AgV zero{0.f, 0.f, 0.f};
    AGSET(vr_agk_rp, zero);
    AGSET(vr_agk_rr, zero);
    AGSET(vr_agk_lp, zero);
    AGSET(vr_agk_lr, zero);
    AGSET(vr_agk_er, zero);
    AGSET(vr_agk_el, zero);
    AGSET(vr_agk_t, zero);
    AGSET(vr_agk_gp, zero);
    AGSET(vr_agk_gr, zero);
    AG(vr_agk_mode) = 0.f;
    AG(vr_agk_on) = 0.f;
    AG(vr_agk_rep) = AF(g, vr_agk_live);
    *reinterpret_cast<int*>(agp.g_vr_agk_repent) = EDICT_TO_PROG(g);
    const float kclass = agkClass(g, p);
    const float kind = agPoseKind(AF(g, vr_ag_kind));
    const float base = kind * VR_AGK_KIND + kclass * VR_AGK_CLASS;
    const AgV st = agWdp(agWorld(), kind * 100.f + kclass, 28.f);
    AG(vr_agk_stretch) = st.x * agkTugStretch(g);
    const float act = AF(g, vr_ag_act);
    float wc = 0.f;
    if(act == agCast)
    {
        agTimes(g);
        const float tw = AG(vr_agp_tw), ts = AG(vr_agp_ts), tr = AG(vr_agp_tr);
        const float sc = agTime() - AF(g, vr_ag_act_at);
        wc = sc < tw ? agSmooth(sc / tw) : sc < tw + ts ? 1.f : 1.f - agSmooth((sc - tw - ts) / agMax(tr, 0.01f));
    }
    float wp = 0.f;
    if(AF(g, vr_ag_took_at) > 0 && agHas(p))
    {
        if(agTime() < AF(g, vr_ag_took_grab))
        {
            wp = agSmooth((agTime() - AF(g, vr_ag_took_at)) / agMax(0.01f, AF(g, vr_ag_took_grab) - AF(g, vr_ag_took_at)));
        }
        else
        {
            wp = 1.f - agSmooth((agTime() - AF(g, vr_ag_took_grab)) / agc_VR_AG_LIFT_TIME);
        }
    }
    float wa;
    if(kclass == VR_AGK_FISTS)
    {
        wa = agMin(1.f, AF(g, vr_ag_readyw) / 0.75f);
    }
    else if(agkIsGun(kclass))
    {
        wa = agSmooth(AF(g, vr_ag_aimw));
    }
    else
    {
        wa = agSmooth(AF(g, vr_agp_rdy));
    }
    wp = agMax(0.f, wp - wc) + wc;
    agkAdd(base, (1.f - wp) * (1.f - wa), false);
    agkAdd(base + VR_AGK_AIM * VR_AGK_POSE, (1.f - wp) * wa, false);
    const float wrun = (1.f - wp) * AF(g, vr_agp_runw) * (1.f - wa);
    agkAdd(base + VR_AGK_RUN * VR_AGK_POSE, wrun, false);
    agkAdd(base + VR_AGK_PICKUP * VR_AGK_POSE, wp - wc, false);
    agkAdd(base + VR_AGK_CAST * VR_AGK_POSE, wc, false);
    AG(vr_agk_gun) = 0.f;
    if(agHas(p) && agkIsGun(kclass) && kclass != VR_AGK_TWOGUNS)
    {
        AG(vr_agk_gun) = 1.f;
    }
    if(AG(vr_agk_gun) != 0.f)
    {
        const float mp = agkMode(base + VR_AGK_PICKUP * VR_AGK_POSE);
        const float mr = agkMode(base + VR_AGK_RUN * VR_AGK_POSE);
        const float mc = agkMode(base + VR_AGK_CAST * VR_AGK_POSE);
        if(wc >= 0.5f && mc != 0.f)
        {
            AG(vr_agk_mode) = mc;
        }
        else if(wp - wc >= 0.5f && mp != 0.f)
        {
            AG(vr_agk_mode) = mp;
        }
        else if((1.f - wp) * wa >= 0.5f)
        {
            AG(vr_agk_mode) = agkMode(base + VR_AGK_AIM * VR_AGK_POSE);
        }
        else if((1.f - wp) * AF(g, vr_agp_runw) >= 0.5f && mr != 0.f)
        {
            AG(vr_agk_mode) = mr;
        }
        else
        {
            AG(vr_agk_mode) = agkMode(base);
        }
    }
    if(act == agPunch || act == agSwing || act == agSaw)
    {
        const float k = AF(g, vr_ag_act_kind);
        float pose = VR_AGK_SAWTHRUST;
        bool mir = false;
        if(act == agPunch)
        {
            pose = k == 0 ? VR_AGK_JAB : k == 1 ? VR_AGK_CROSS : k == 4 ? VR_AGK_UPPERCUT : VR_AGK_HOOK;
            mir = k == 2;
        }
        else if(act == agSwing)
        {
            pose = kclass == VR_AGK_POLE ? VR_AGK_THRUST : VR_AGK_OVERHEAD + agMin(3.f, k);
            if(agHas(AFE(g, vr_ag_prop2)) && AF(g, vr_ag_act_hand) == 2.f)
            {
                pose = VR_AGK_DUAL;
            }
            else if(agHas(AFE(g, vr_ag_prop2)))
            {
                mir = AF(g, vr_ag_act_hand) == 1.f;
            }
        }
        agkBlend(g, base + pose * VR_AGK_POSE);
        agkAddBlend(base + pose * VR_AGK_POSE, mir);
    }
    else if(act == agShove)
    {
        float sb = base;
        if(AG(vr_agk_gun) != 0.f && kclass != VR_AGK_PISTOL && agHands(g, p) == 1.f)
        {
            sb = kind * VR_AGK_KIND + VR_AGK_PISTOL * VR_AGK_CLASS;
        }
        sb = sb + VR_AGK_SHOVE * VR_AGK_POSE;
        agkBlend(g, sb);
        agkAddBlend(sb, false);
        const float wka = AG(vr_agk_ba) >= 0 ? 1.f - AG(vr_agk_bf) : 0.f;
        const float wkb = AG(vr_agk_bb) >= 0 ? AG(vr_agk_bf) : 0.f;
        if(AG(vr_agk_gun) != 0.f && wka + wkb >= 0.5f)
        {
            const float ph = wkb > wka ? AG(vr_agk_bb) : AG(vr_agk_ba);
            const float ms = agkMode(sb + ph);
            if(ms != 0.f)
            {
                AG(vr_agk_mode) = ms;
            }
        }
    }
}
void agkQuery(edict_t* g, edict_t* p) // VR_AGK_Query
{
    const AgV h = agWdp(g, agPoseKind(AF(g, vr_ag_kind)) * 100.f + agkClass(g, p), 19.f);
    AG(vr_agk_hpose) = h.x;
    AG(vr_agk_hphase) = h.y;
    AG(vr_agk_hplay) = h.z;
}
void agTurnFrame(AgV fw0, AgV up0, AgV fw1, AgV up1, float u) // VR_AGP_TurnFrame
{
    fw0 = agNormalize(fw0);
    fw1 = agNormalize(fw1);
    AgV k = agCross(fw0, fw1);
    float s = agVlen(k);
    const float th = agAtan2(s, agDot(fw0, fw1)) * agToDeg;
    if(s < 0.00001f)
    {
        k = agCross(fw0, agFabs(fw0.z) < 0.9f ? AgV{0.f, 0.f, 1.f} : AgV{1.f, 0.f, 0.f});
        s = agVlen(k);
    }
    k = k * (1.f / s);
    AgV upArc = agRotate(up0, k, th);
    upArc = agNormalize(upArc - fw1 * agDot(upArc, fw1));
    const AgV u1 = agNormalize(up1 - fw1 * agDot(up1, fw1));
    const float r = agAtan2(agDot(agCross(upArc, u1), fw1), agDot(upArc, u1)) * agToDeg;
    const AgV sfw = agNormalize(agRotate(fw0, k, th * u));
    AGSET(vr_agp_sfw, sfw);
    const AgV up = agRotate(agRotate(up0, k, th * u), sfw, r * u);
    AGSET(vr_agp_sup, agNormalize(up - sfw * agDot(up, sfw)));
}
[[nodiscard]] AgV agAxeStickAngles(AgV fw, AgV up) // VR_AxeStick_Angles
{
    AgV a = agVecToAngles(fw);
    a.x = a.x * -1.f;
    a.z = 0.f;
    agMakeVectors(a);
    AgV vu = agLoad(pr_global_struct->v_up);
    AgV c;
    c.x = vu.y * up.z - vu.z * up.y;
    c.y = vu.z * up.x - vu.x * up.z;
    c.z = vu.x * up.y - vu.y * up.x;
    const float roll = agAtan2(agDot(c, fw), agDot(vu, up)) * agToDeg;
    a.z = roll;
    agMakeVectors(a);
    vu = agLoad(pr_global_struct->v_up);
    if(agDot(vu, up) < 0.999f)
    {
        a.z = agNeg(roll);
    }
    return a;
}

// ---------------------------------------------------------------------------------------------------------------------
// What the engine's poses went through (vr_agpose_compare prints it: the comparison's coverage).
struct AgpCoverage
{
    int fists{0}, melee{0}, gun{0}, dual{0}, gunBlade{0}, saw{0}, punch{0}, swing{0}, took{0}, reaching{0}, reload{0},
        body{0}, keys{0}, meet{0}, cast{0}, shove{0}, kick{0};
};
AgpCoverage agpCov; // (the server's VM: the main thread)

// VR_AGP_Pose. False: left to QuakeC (nothing done).
[[nodiscard]] bool agPose(edict_t* g, edict_t* p)
{
    if(AG(vr_agp_quiet) != 0.f || ACV(vr_agrunt_debug) >= 2 || AG(vr_agk_test_hold) != 0.f || vr_agposesend.value == 0.f)
    {
        return false;
    }
    // A pole weapon (the spear: its slot's Other Hand Anywhere 3; QC VR_AGP_Pole): its two-handed hold and thrust are
    // QuakeC's alone (spearfix: rare, one copy of the pose).
    if(p && p != qcvm->edicts && twohand::poleWeapon(weapons::slotForName(PR_GetString(p->v.model))))
    {
        return false;
    }
    AG(vr_agk_hpose) = -1.f;
    agkQuery(g, p);
    const bool frozen = ACV(vr_agrunt_freeze) != 0.f && g->v.health > 0 && AF(g, vr_agk_live) == 0.f; // VR_AG_Still
    if(frozen || AG(vr_agk_hpose) >= 0 || AF(g, vr_agk_held) != 0.f || AF(g, vr_agk_live) != 0.f)
    {
        return false;
    }
    const float time = agTime();
    // VR_AM_Hand (its vertices' middle: only its trace_fraction is used now)
    agRow(AF(g, vr_ag_kind));
    {
        const AgV hv = AGV(vr_am_hand);
        const AgV h = agModelVertex(g, hv.x) + agModelVertex(g, hv.y) + agModelVertex(g, hv.z);
        (void)(h * (1.f / 3.f));
    }
    if(pr_global_struct->trace_fraction == 0)
    {
        return true;
    }
    agRow(AF(g, vr_ag_kind)); // VR_AM_Anchor
    const AgV hand = agModelVertex(g, AG(vr_am_anchor));
    agRow(AF(g, vr_ag_kind)); // VR_AM_GunRear, _GunMuzzle, _GunUp
    const AgV rearV = AGV(vr_am_gun_rear), muzV = AGV(vr_am_gun_muzzle), rollV = AGV(vr_am_gun_roll);
    const AgV rear = (agModelVertex(g, rearV.x) + agModelVertex(g, rearV.y)) * 0.5f;
    agRow(AF(g, vr_ag_kind));
    const AgV muzzle = (agModelVertex(g, muzV.x) + agModelVertex(g, muzV.y)) * 0.5f;
    const AgV gx = agNormalize(muzzle - rear);
    agRow(AF(g, vr_ag_kind));
    AgV gz = agModelVertex(g, rollV.x) - agModelVertex(g, rollV.y);
    gz = agNormalize(gz - gx * agDot(gz, gx));
    if(agZero(gz)) { gz = {0.f, 0.f, 1.f}; }
    const AgV gy = agCross(gz, gx);
    if(AF(g, vr_ag_act) != 0.f && time - AF(g, vr_ag_act_tick) > 0.25f)
    {
        AF(g, vr_ag_act) = 0.f;
    }
    if(AF(g, vr_ag_act) == agCast && AF(g, vr_ag_act_hit) == 0.f && time - AF(g, vr_ag_act_tick) < 0.15f)
    {
        agTimes(g);
        if(time - AF(g, vr_ag_act_at) >= AG(vr_agp_tw) - 0.001f)
        {
            agpCov.cast++;
            agArgE(0, g);
            agRun(agp.fn_VR_AGP_CastLands, 1);
        }
    }
    if(AF(g, vr_ag_act) == agShove && AF(g, vr_ag_act_hit) == 0.f && time - AF(g, vr_ag_act_tick) < 0.15f)
    {
        agTimes(g);
        if(time - AF(g, vr_ag_act_at) >= AG(vr_agp_tw) + AG(vr_agp_ts) - 0.001f)
        {
            agpCov.shove++;
            agArgE(0, g);
            agRun(agp.fn_VR_AGP_ShoveLands, 1);
        }
    }
    edict_t* pL = agWorld();
    if(agHas(p) && agHas(AFE(g, vr_ag_prop2)))
    {
        edict_t* pa = AFE(g, vr_ag_prop);
        edict_t* pb = AFE(g, vr_ag_prop2);
        p = AF(pa, vr_ag_left) != 0.f ? pb : pa;
        pL = AF(pa, vr_ag_left) != 0.f ? pa : pb;
    }
    agBody(g);
    const AgV handL = agToLocal(hand - agOrigin(g));
    AG(vr_agp_flags) = 0.f;
    AG(vr_agp_gunpose) = 0.f;
    AG(vr_agpar_mw) = 0.f;
    AG(vr_agp_wl) = 0.f;
    AGSET(vr_agp_vl, (AgV{0.f, 0.f, 0.f}));
    AGSET(vr_agp_fw, gx);
    AGSET(vr_agp_up, gz);
    AG(vr_agp_wo) = 1.f;
    const float c = agHas(p) ? agClass(p->v.weapon) : static_cast<float>(VR_AG_CLASS_FISTS);
    // VR_AGP_Demo (its QuakeC when it may begin one)
    if(!(ACV(vr_agrunt_strike_demo) == 0.f || agHas(PROG_TO_EDICT(g->v.enemy)) || AF(g, vr_ag_act) != 0.f ||
         g->v.health <= 0 || AF(g, vr_knockdown) != 0.f || AF(g, vr_ag_took_at) > 0 ||
         (c != VR_AG_CLASS_FISTS && c != VR_AG_CLASS_MELEE)))
    {
        agArgE(0, g);
        agArgF(1, c);
        agRun(agp.fn_VR_AGP_Demo, 2);
    }
    agkKeys(g, p);
    agpCov.fists += c == VR_AG_CLASS_FISTS ? 1 : 0;
    agpCov.melee += c == VR_AG_CLASS_MELEE ? 1 : 0;
    agpCov.gun += (c != VR_AG_CLASS_FISTS && c != VR_AG_CLASS_MELEE) ? 1 : 0;
    agpCov.dual += agHas(pL) ? 1 : 0;
    agpCov.saw += (agHas(p) && p->v.weapon == WID_CHAINSAW) ? 1 : 0;
    agpCov.punch += AF(g, vr_ag_act) == agPunch ? 1 : 0;
    agpCov.swing += (AF(g, vr_ag_act) == agSwing || AF(g, vr_ag_act) == agSaw) ? 1 : 0;
    agpCov.took += AF(g, vr_ag_took_at) > 0 ? 1 : 0;
    agpCov.reload += AF(g, vr_agp_rl_at) > 0 ? 1 : 0;
    agpCov.kick += AF(g, vr_ag_kick_at) > 0 && time - AF(g, vr_ag_kick_at) < 0.2f ? 1 : 0;
    agpCov.keys += AG(vr_agk_on) != 0.f ? 1 : 0;
    if(c == VR_AG_CLASS_MELEE)
    {
        if(agRightSwings(g))
        {
            agCallGE(agp.fn_VR_DRP_Frame, g, p);
        }
        if(agHas(pL) && AF(g, vr_ag_act_hand) != 0.f)
        {
            agCallGE(agp.fn_VR_DRP_Frame, g, pL);
        }
    }
    else if(agHas(pL) && AFE(g, vr_ag_prop) == pL && agClass(pL->v.weapon) == VR_AG_CLASS_MELEE)
    {
        agCallGE(agp.fn_VR_DRP_Frame, g, pL);
    }
    if(c == VR_AG_CLASS_FISTS)
    {
        agFists(g, handL);
    }
    else if(c == VR_AG_CLASS_MELEE)
    {
        agMelee(g, p);
        agCallGE(agp.fn_VR_AGPar_Pose, g, p);
        if(p->v.weapon == WID_CHAINSAW)
        {
            agOrient({1.f, 0.f, 0.f}, AGV(vr_agp_fw), {0.f, 0.f, 1.f}, agToWorld({0.f, 0.f, 1.f}));
        }
        else
        {
            agBladeHand(p, AGV(vr_agp_fw), AGV(vr_agp_up));
        }
        if(agHas(pL))
        {
            agLeftBlade(g, pL);
        }
    }
    else
    {
        const bool bladeL = agHas(pL) && agClass(pL->v.weapon) == VR_AG_CLASS_MELEE;
        agpCov.gunBlade += bladeL ? 1 : 0;
        float gunHands = agHas(pL) ? 0.f : agHands(g, p);
        const float offMode = agHas(pL) ? 0.f : AG(vr_agk_mode);
        if(offMode == VR_AGK_OFF_GUN)
        {
            gunHands = 2.f;
        }
        AG(vr_agp_onehand) = (bladeL || (gunHands == 1.f)) ? 1.f : 0.f;
        agArmsGun(g, p, bladeL ? agWorld() : pL);
        AG(vr_agp_onehand) = 0.f;
        if(gunHands == 1.f || offMode == VR_AGK_OFF_FREE)
        {
            AG(vr_agp_flags) = agOr(AG(vr_agp_flags), agfFree);
        }
        else if((gunHands == 2.f) && (AG(vr_agh_set) == 2.f || offMode == VR_AGK_OFF_GUN))
        {
            AG(vr_agp_flags) = agOr(AG(vr_agp_flags), agfTwo);
        }
        AG(vr_agp_gunpose) = 1.f;
        agCallGE(agp.fn_VR_AGPar_Pose, g, p);
        if(bladeL)
        {
            const AgV keepFwR = AGV(vr_agp_fw);
            const AgV keepUpR = AGV(vr_agp_up);
            const AgV keepVR = AGV(vr_agp_v);
            const float keepWR = AG(vr_agp_wr);
            const float fl = AG(vr_agp_flags);
            const float keepFl = fl - agAnd(fl, agOr(agfLeft, agfLeftWpn));
            agLeftBlade(g, pL);
            AGSET(vr_agp_fw, keepFwR);
            AGSET(vr_agp_up, keepUpR);
            AGSET(vr_agp_v, keepVR);
            AG(vr_agp_wr) = keepWR;
            AG(vr_agp_flags) = agOr(agOr(keepFl, agfLeft), agfLeftWpn);
        }
    }
    agCallGE(agp.fn_VR_AGP_Shield, g, p); // (dazzled by a flashlight: its off hand before its eyes; QC vr_armedgrunt_dazzle.qc)
    const AgV bob = agRunBob(g);
    AGSET(vr_agp_v, AGV(vr_agp_v) + bob);
    AGSET(vr_agp_vl, AGV(vr_agp_vl) + bob * AG(vr_agp_wl));
    AG(vr_agp_flags) = agOr(AG(vr_agp_flags), agfAnchor);
    agRow(AF(g, vr_ag_kind));
    const AgV moved = handL - AGV(vr_am_anchor_at);
    const float most = 0.93f * AG(vr_am_arm) + 2.5f;
    AgV grip = handL * AG(vr_agp_wr) + AGV(vr_agp_v);
    AgV fromSh = grip - (AGV(vr_am_sh_r) + moved);
    if(agVlen(fromSh) > most)
    {
        AGSET(vr_agp_v, AGV(vr_am_sh_r) + moved + agNormalize(fromSh) * most - handL * AG(vr_agp_wr));
    }
    if(agAnd(AG(vr_agp_flags), agfLeft) != 0.f)
    {
        grip = handL * AG(vr_agp_wl) + AGV(vr_agp_vl);
        fromSh = grip - (AGV(vr_am_sh_l) + moved);
        if(agVlen(fromSh) > most)
        {
            AGSET(vr_agp_vl, AGV(vr_am_sh_l) + moved + agNormalize(fromSh) * most - handL * AG(vr_agp_wl));
        }
    }
    agBody(g);
    AgV posL = handL * AG(vr_agp_wr) + AGV(vr_agp_v);
    bool reaching = false;
    float takeBend = 0.f;
    float takeW = 0.f;
    if(AF(g, vr_ag_took_at) > 0 && agHas(p) && agHas(pL) && AG(vr_agp_wl) > 0)
    {
        AGSET(vr_agp_vl, handL * AG(vr_agp_wl) + AGV(vr_agp_vl));
        AG(vr_agp_wl) = 0.f;
    }
    if(AF(g, vr_ag_took_at) > 0 && agHas(p))
    {
        const float tookAt = AF(g, vr_ag_took_at), grab = AF(g, vr_ag_took_grab);
        float u = agMin(1.f, agMax(0.f, (time - grab) / agc_VR_AG_LIFT_TIME));
        if(u >= 1)
        {
            AF(g, vr_ag_took_at) = 0.f;
        }
        u = u * u * (3.f - 2.f * u);
        const AgV lay = agToLocal(AFV(g, vr_ag_took_org) - agOrigin(g));
        float dk = (time - tookAt) / agMax(0.01f, grab - tookAt);
        dk = agMin(1.f, agMax(0.f, dk));
        takeW = time < grab ? dk * dk * (3.f - 2.f * dk) : 1.f - u;
        takeBend = (ACV(vr_aglegs) != 0.f ? ACV(vr_agrunt_take_bend_kneel) : ACV(vr_agrunt_take_bend)) * takeW;
        if(!agHas(pL) && time < grab)
        {
            float d = (time - tookAt) / agMax(0.01f, grab - tookAt);
            d = agMin(1.f, agMax(0.f, d));
            d = d * d * (3.f - 2.f * d);
            AG(vr_agp_wr) = 1.f - d;
            agRow(AF(g, vr_ag_kind));
            AGSET(vr_agp_v, (agHang(0.f) - AGV(vr_am_anchor_at)) * (1.f - d) + lay * d);
            AG(vr_agp_flags) = agOr(AG(vr_agp_flags), agfReach);
            reaching = true;
            agpCov.reaching++;
            posL = lay;
            AGSET(vr_agp_fw, AFV(g, vr_ag_took_fw));
            AGSET(vr_agp_up, AFV(g, vr_ag_took_up));
        }
        else if(agHas(pL))
        {
            AGSET(vr_agp_vl, lay + (AGV(vr_agp_vl) - lay) * u);
            agTurnFrame(AFV(g, vr_ag_took_fw), AFV(g, vr_ag_took_up), AGV(vr_agp_fwl), AGV(vr_agp_upl), u);
            AGSET(vr_agp_fwl, AGV(vr_agp_sfw));
            AGSET(vr_agp_upl, AGV(vr_agp_sup));
        }
        else
        {
            posL = lay + (posL - lay) * u;
            AG(vr_agp_wr) = 0.f;
            AGSET(vr_agp_v, posL);
            agTurnFrame(AFV(g, vr_ag_took_fw), AFV(g, vr_ag_took_up), AGV(vr_agp_fw), AGV(vr_agp_up), u);
            AGSET(vr_agp_fw, AGV(vr_agp_sfw));
            AGSET(vr_agp_up, AGV(vr_agp_sup));
        }
    }
    bool body = AF(g, vr_ag_took_at) <= 0 && agTorsoNow(g);
    const bool keyBody = AG(vr_agk_on) != 0.f &&
        (!agZero(AGV(vr_agk_t)) || !agZero(AGV(vr_agk_er)) || !agZero(AGV(vr_agk_el)));
    if(!body && keyBody && AF(g, vr_ag_took_at) <= 0)
    {
        body = true;
    }
    if(body)
    {
        AGSET(vr_agp_tq, AGV(vr_agp_tq) + AGV(vr_agk_t));
        AGSET(vr_agp_elr, AGV(vr_agp_elr) + AGV(vr_agk_er));
        AGSET(vr_agp_ell, AGV(vr_agp_ell) + AGV(vr_agk_el));
        posL = agTurnPose(g, posL, handL);
        agpCov.body++;
        AG(vr_agp_flags) = agOr(AG(vr_agp_flags), agfBody);
    }
    else if(takeBend > 0.1f || keyBody)
    {
        AgV tq = AGV(vr_agk_t);
        tq.y = tq.y + takeBend;
        AGSET(vr_agp_tq, tq);
        AGSET(vr_agp_elr, AGV(vr_agk_er));
        AGSET(vr_agp_ell, AGV(vr_agk_el));
        AG(vr_agp_flags) = agOr(agOr(AG(vr_agp_flags), agfBody), agfFixed);
        body = true;
    }
    if(AG(vr_agp_gunpose) != 0.f && agAnd(AG(vr_agp_flags), agfFixed) == 0.f && ACV(vr_agrunt_upper) != 0.f)
    {
        AgV tw{0.f, 0.f, 0.f};
        const float aw = agSmooth(AF(g, vr_ag_aimw));
        const AgV aimdir = AFV(g, vr_ag_aimdir);
        if(aw > 0 && !agZero(aimdir))
        {
            const AgV al = agToLocal(aimdir);
            const float share = ACV(vr_agrunt_aim_twist) * aw;
            tw.x = agMax(-25.f, agMin(25.f, share * agAtan2(al.y, al.x) * agToDeg));
            tw.y = agMax(-20.f, agMin(20.f, agNeg(share) * agAtan2(al.z, agVlen({al.x, al.y, 0.f})) * agToDeg));
        }
        float* twist = AFP(g, vr_agp_twist);
        twist[0] = agEase(twist[0], tw.x, 60.f);
        twist[1] = agEase(twist[1], tw.y, 60.f);
        AgV tq{twist[0], twist[1], 0.f};
        const float ke = time - AF(g, vr_ag_kick_at);
        if(ke >= 0 && ke < 0.4f && AF(g, vr_ag_kick_at) > 0)
        {
            tq.y = tq.y - 3.f * agSin(ke / 0.4f * agPi);
        }
        const float rlp = AF(g, vr_agp_rl_at) > 0 ? (time - AF(g, vr_agp_rl_at)) / agMax(0.1f, agReloadTime(g)) : -1.f;
        if(rlp >= 0 && rlp <= 1)
        {
            const float rs = agSin(rlp * agPi);
            tq.x = tq.x + 8.f * rs;
            tq.y = tq.y + 6.f * rs;
        }
        if(!agZero(tq))
        {
            AGSET(vr_agp_tq, agAnd(AG(vr_agp_flags), agfBody) != 0.f ? AGV(vr_agp_tq) + tq : tq);
            AG(vr_agp_flags) = agOr(AG(vr_agp_flags), agfBody);
            body = true;
        }
    }
    if(!agZero(AGV(vr_agp_shpole)) && agAnd(AG(vr_agp_flags), agfFixed) == 0.f)
    {
        // (dazzled: its shielding elbow out to its side, QC VR_AGP_Shield; on a strike's or the keys' poles, or alone)
        if(!body)
        {
            const AgV zero{0.f, 0.f, 0.f};
            AGSET(vr_agp_tq, zero);
            AGSET(vr_agp_elr, zero);
            AGSET(vr_agp_ell, zero);
        }
        AGSET(vr_agp_ell, AGV(vr_agp_ell) + AGV(vr_agp_shpole));
        AG(vr_agp_flags) = agOr(AG(vr_agp_flags), agfBody);
        body = true;
    }
    AGSET(vr_agp_shpole, (AgV{0.f, 0.f, 0.f}));
    if(agVlen(AGV(vr_agp_elr)) > 1) { AGSET(vr_agp_elr, agNormalize(AGV(vr_agp_elr))); }
    if(agVlen(AGV(vr_agp_ell)) > 1) { AGSET(vr_agp_ell, agNormalize(AGV(vr_agp_ell))); }
    if(AG(vr_agk_on) != 0.f)
    {
        agBody(g);
        if(!reaching)
        {
            posL = posL + AGV(vr_agk_rp);
        }
        AGSET(vr_agp_v, AGV(vr_agp_v) + AGV(vr_agk_rp));
        const AgV rr = AGV(vr_agk_rr);
        if(agVlen(rr) > 0.01f)
        {
            const AgV kax = agNormalize(agToWorld(rr));
            AGSET(vr_agp_fw, agRotate(AGV(vr_agp_fw), kax, agVlen(rr)));
            AGSET(vr_agp_up, agRotate(AGV(vr_agp_up), kax, agVlen(rr)));
        }
        if(agAnd(AG(vr_agp_flags), agfLeft) != 0.f)
        {
            AGSET(vr_agp_vl, AGV(vr_agp_vl) + AGV(vr_agk_lp));
        }
        const AgV lr = AGV(vr_agk_lr);
        if(agAnd(AG(vr_agp_flags), agfLeftWpn) != 0.f && agVlen(lr) > 0.01f)
        {
            const AgV kaxl = agNormalize(agToWorld(lr));
            AGSET(vr_agp_fwl, agRotate(AGV(vr_agp_fwl), kaxl, agVlen(lr)));
            AGSET(vr_agp_upl, agRotate(AGV(vr_agp_upl), kaxl, agVlen(lr)));
        }
    }
    if(agHas(p))
    {
        const AgV a = agAxeStickAngles(AGV(vr_agp_fw), AGV(vr_agp_up));
        agStore(p->v.v_angle, a);
        agStore(p->v.angles, a);
        p->v.angles[0] = agNeg(a.x);
        agStore(AFP(p, vr_ag_pfw), AGV(vr_agp_fw));
        agStore(AFP(p, vr_ag_pup), AGV(vr_agp_up));
        agSetOrigin(p, agOrigin(g) + agToWorld(posL));
        AgV meet{0.f, 0.f, 0.f};
        if(!reaching)
        {
            agCallGE(agp.fn_VR_AGPar_Meet, g, p);
            meet = agRetV();
        }
        if(!agZero(meet))
        {
            agpCov.meet++;
            agRow(AF(g, vr_ag_kind));
            const AgV sh = AGV(vr_am_sh_r) + moved;
            const float reach = agMax(most, agVlen(posL - sh));
            AgV met = posL + meet;
            if(agVlen(met - sh) > reach)
            {
                met = sh + agNormalize(met - sh) * reach;
            }
            meet = met - posL;
            posL = met;
            AGSET(vr_agp_v, AGV(vr_agp_v) + meet);
            agBody(g);
            agSetOrigin(p, agOrigin(g) + agToWorld(posL));
        }
        if(agAnd(AG(vr_agp_flags), agfLeftWpn) != 0.f)
        {
            const AgV al = agAxeStickAngles(AGV(vr_agp_fwl), AGV(vr_agp_upl));
            agStore(pL->v.v_angle, al);
            agStore(pL->v.angles, al);
            pL->v.angles[0] = agNeg(al.x);
            agStore(AFP(pL, vr_ag_pfw), AGV(vr_agp_fwl));
            agStore(AFP(pL, vr_ag_pup), AGV(vr_agp_upl));
            agSetOrigin(pL, agOrigin(g) + agToWorld(handL * AG(vr_agp_wl) + AGV(vr_agp_vl)));
        }
        if(p->v.weapon == WID_CHAINSAW)
        {
            agCallGE(agp.fn_VR_AGP_Saw, g, p);
            agBody(g);
        }
    }
    AG(vr_agk_on) = 0.f;
    float offFl = 0.f;
    if(agHas(p) && AG(vr_agk_gun) != 0.f && !reaching && agAnd(AG(vr_agp_flags), agOr(agfLeft, agfLeftWpn)) == 0.f)
    {
        if(agVlen(AGV(vr_agk_gp)) > 0.001f || agVlen(AGV(vr_agk_gr)) > 0.01f)
        {
            offFl = agOr(offFl, 4.f);
        }
        if(agVlen(AGV(vr_agk_lp)) > 0.001f || agVlen(AGV(vr_agk_lr)) > 0.01f)
        {
            offFl = agOr(offFl, 8.f);
        }
        if(!agHas(pL) && AG(vr_agk_mode) == VR_AGK_OFF_GUN)
        {
            offFl = agOr(offFl, 32.f);
        }
    }
    const float stretchB = agRint(agMin(255.f, agMax(0.f, (AG(vr_agk_stretch) - 1.f) * 500.f)));
    if(stretchB > 0 && !reaching)
    {
        offFl = agOr(offFl, 16.f);
    }
    float sendFl = AG(vr_agp_flags);
    if(offFl != 0.f)
    {
        const float fl = AG(vr_agp_flags);
        offFl = agOr(agOr(offFl, agAnd(fl, agfFree) != 0.f ? 1.f : 0.f), agAnd(fl, agfTwo) != 0.f ? 2.f : 0.f);
        sendFl = agOr(agOr(sendFl, agfFree), agfTwo);
    }
    float takeB = 0.f;
    if(body)
    {
        AGSET(vr_agp_waist, agFit(g, {-4.7f, 0.9f, 4.8f}));
        if(agAnd(AG(vr_agp_flags), agfFixed) != 0.f)
        {
            takeB = agRint(agMin(1.f, agMax(0.f, takeW)) * 127.f) + (agHas(pL) ? 128.f : 0.f);
        }
    }
    agArgE(0, g);
    agArgE(1, reaching ? agWorld() : p);
    agArgE(2, pL);
    agArgF(3, sendFl + offFl * 256.f + (body ? 65536.f : 0.f));
    agArgF(4, stretchB + takeB * 256.f);
    agArgV(5, gx);
    agArgV(6, gy);
    agArgV(7, gz);
    qcvm->argc = 8;
    PF_agposesend();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// vr_agpose_native 2: each pose by QuakeC on a copy of the state (its globals; the humanoid's, its weapons', its enemy's
// fields), its results kept, the state put back, then by the engine; every QC global named vr_* (and v_*, trace_*), every
// field of those entities and the pose message's arguments compared. Undone after QuakeC's run: the globals, every
// edict's fields (the edicts it spawned freed), the server's random stream. Not undone: the rest (a sound, a message,
// physics bodies: twice in this mode); a pose that struck a blow compares only its own state.
struct AgpCompareScratch
{
    za::Vector<float> globals;   // the state before: its globals,
    za::Vector<float> world;     // ... every edict's fields, one after another,
    za::Vector<byte> wasFree;    // ... and which were free
    za::Vector<float> qcGlobals; // QuakeC's results
    za::Vector<float> qcFields;  // ... the compared entities' fields
    auto members() { return mem::list(globals, world, wasFree, qcGlobals, qcFields); }
};
mem::Scratch<AgpCompareScratch> agpCompareScratch{"agpose compare"};
struct AgpCompareStats // since the last vr_agpose_compare (per QC def: the largest difference, how many differed)
{
    const dprograms_t* progs{nullptr};
    za::Vector<float> globalMax, fieldMax;
    za::Vector<int> globalCount, fieldCount;
    int poses{0}, sendDiffs{0}, sendMissing{0}, leftToQc{0}, struck{0};
    int differing{0}, contexts{0}; // (values differing, all told; the poses' context lines printed)
    int struckDiffering{0};        // poses with a blow struck that differ: not comparable (below)
    bool noting{false};            // (agpNote: the pose being compared struck a blow)
    float sendMax{0.f};
};
AgpCompareStats agpStats; // (the server's VM: the main thread)

[[nodiscard]] bool agpComparedName(const char* n)
{
    return !ZA_STRNCMP(n, "vr_", 3) || !ZA_STRNCMP(n, "v_", 2) || !ZA_STRNCMP(n, "trace_", 6);
}
[[nodiscard]] bool agpPoseName(const char* n) // (the pose's own globals)
{
    return !ZA_STRNCMP(n, "vr_agp_", 7) || !ZA_STRNCMP(n, "vr_agk_", 7) || !ZA_STRNCMP(n, "vr_agh_", 7) ||
        !ZA_STRNCMP(n, "vr_am_", 6) || !ZA_STRNCMP(n, "v_", 2);
}
[[nodiscard]] int agpDefWidth(const ddef_t& d)
{
    const int t = d.type & ~DEF_SAVEGLOBAL;
    return t == ev_vector ? 3 : (t == ev_float || t == ev_entity) ? 1 : 0;
}
void agpNote(float& maxd, int& count, float a, float b, bool isInt, const char* kind, const ddef_t& def)
{
    float d;
    if(isInt)
    {
        int ia, ib;
        ZA_MEMCPY(&ia, &a, sizeof(int));
        ZA_MEMCPY(&ib, &b, sizeof(int));
        d = ia != ib ? 1.f : 0.f;
    }
    else
    {
        d = (a != a && b != b) ? 0.f : (a == b ? 0.f : (a != a || b != b) ? 1e30f : static_cast<float>(fabs(a - b)));
    }
    if(d > 0.f && agpStats.noting)
    {
        // A blow struck in QuakeC's run: its effects on the humanoid itself (a parry's knockdown, a ragdoll made, a
        // shove back) live in engine state not put back (Box3D's bodies, the ragdolls'), so the engine's run met a
        // different world from that blow on. Counted apart: the strike's own path is QuakeC's (VR_AGP_Strike).
        agpStats.differing++;
        return;
    }
    if(d > 0.f)
    {
        if(count == 0)
        {
            // (each value's first: a suite run with vr_agpose_native 2 finds them in its logs)
            Con_Printf("agpose compare: MISMATCH %s %s: QuakeC %g, engine %g\n", kind, PR_GetString(def.s_name), a, b);
        }
        count++;
        agpStats.differing++;
        maxd = d > maxd ? d : maxd;
    }
}

[[nodiscard]] bool agpCompare(edict_t* g, edict_t* p)
{
    const dprograms_t* pr = qcvm->progs;
    if(agpStats.progs != pr)
    {
        agpStats = AgpCompareStats{};
        agpStats.progs = pr;
        Con_Printf("agpose compare: on (vr_agpose_native 2): each pose by QuakeC and by the engine, compared\n");
        agpStats.globalMax.resize(static_cast<za::SizeT>(pr->numglobaldefs));
        agpStats.globalCount.resize(static_cast<za::SizeT>(pr->numglobaldefs));
        agpStats.fieldMax.resize(static_cast<za::SizeT>(pr->numfielddefs));
        agpStats.fieldCount.resize(static_cast<za::SizeT>(pr->numfielddefs));
        for(za::SizeT i = 0; i < agpStats.globalMax.size(); i++) { agpStats.globalMax[i] = 0.f; agpStats.globalCount[i] = 0; }
        for(za::SizeT i = 0; i < agpStats.fieldMax.size(); i++) { agpStats.fieldMax[i] = 0.f; agpStats.fieldCount[i] = 0; }
    }
    // The entities whose fields are compared and put back.
    edict_t* ents[6];
    int n = 0;
    const auto add = [&](edict_t* e) {
        if(!agHas(e) || e->free)
        {
            return;
        }
        for(int i = 0; i < n; i++)
        {
            if(ents[i] == e)
            {
                return;
            }
        }
        ents[n++] = e;
    };
    add(g);
    add(p);
    add(AFE(g, vr_ag_prop));
    add(AFE(g, vr_ag_prop2));
    const int nOwn = n; // (g and its weapons: the enemy's after them)
    add(PROG_TO_EDICT(g->v.enemy));
    const int nf = pr->entityfields;
    const int ng = pr->numglobals;
    AgpCompareScratch& s = agpCompareScratch;
    s.globals.resize(static_cast<za::SizeT>(ng));
    s.qcGlobals.resize(static_cast<za::SizeT>(ng));
    s.qcFields.resize(static_cast<za::SizeT>(nf * n));
    const auto fieldsOf = [](edict_t* e) { return reinterpret_cast<float*>(&e->v); };
    const auto edictAt = [](int i) { return reinterpret_cast<edict_t*>(reinterpret_cast<byte*>(qcvm->edicts) + i * qcvm->edict_size); };
    const size_t entBytes = static_cast<size_t>(nf) * sizeof(float);
    const int numBefore = qcvm->num_edicts;
    s.world.resize(static_cast<za::SizeT>(nf) * static_cast<za::SizeT>(numBefore));
    s.wasFree.resize(static_cast<za::SizeT>(numBefore));
    ZA_MEMCPY(s.globals.data(), qcvm->globals, static_cast<size_t>(ng) * sizeof(float));
    for(int i = 0; i < numBefore; i++)
    {
        edict_t* e = edictAt(i);
        s.wasFree[static_cast<za::SizeT>(i)] = e->free ? 1 : 0;
        ZA_MEMCPY(s.world.data() + static_cast<za::SizeT>(i) * nf, fieldsOf(e), entBytes);
    }
    const za::FastNonCryptoRng rngBefore = srvrandom::streamState();
    const auto restore = [&]() {
        ZA_MEMCPY(qcvm->globals, s.globals.data(), static_cast<size_t>(ng) * sizeof(float));
        srvrandom::setStreamState(rngBefore);
        for(int i = 1; i < qcvm->num_edicts; i++)
        {
            edict_t* e = edictAt(i);
            if(i >= numBefore || s.wasFree[static_cast<za::SizeT>(i)])
            {
                if(!e->free)
                {
                    ED_Free(e); // (spawned by QuakeC's run)
                }
                continue;
            }
            float* v = fieldsOf(e);
            const float* old = s.world.data() + static_cast<za::SizeT>(i) * nf;
            if(e->free || !ZA_MEMCMP(v, old, entBytes))
            {
                continue;
            }
            const bool moved = ZA_MEMCMP(e->v.origin, old + (e->v.origin - reinterpret_cast<float*>(&e->v)), 3 * sizeof(float)) ||
                ZA_MEMCMP(e->v.mins, old + (e->v.mins - reinterpret_cast<float*>(&e->v)), 6 * sizeof(float));
            ZA_MEMCPY(v, old, entBytes);
            if(moved)
            {
                SV_LinkEdict(e, false); // (its link where it is again; its fields as they were, absmin and absmax too)
                ZA_MEMCPY(v, old, entBytes);
            }
        }
    };
    // QuakeC's run (its message not sent)
    agpsCapture = AgPoseSendCapture{};
    agpsCapture.capture = 1;
    agArgE(0, g);
    agArgE(1, p);
    const float eventsBefore = AG(vr_agp_events);
    const float hitBefore = AF(g, vr_ag_act_hit), hitlBefore = AF(g, vr_ag_act_hitl);
    agRun(agp.fn_VR_AGP_PoseQC, 2);
    // (a blow, a cut, a shove's push or a cast landed, or anything spawned: its effects on the rest of the world happen
    // twice here, some in engine state not put back; only the pose's own globals and its entities' own fields compared)
    bool struck = AG(vr_agp_events) != eventsBefore || AF(g, vr_ag_act_hit) != hitBefore ||
        AF(g, vr_ag_act_hitl) != hitlBefore || qcvm->num_edicts != numBefore;
    for(int i = 1; i < numBefore && !struck; i++)
    {
        struck = s.wasFree[static_cast<za::SizeT>(i)] && !edictAt(i)->free;
    }
    const AgPoseSendCapture qcSend = agpsCapture;
    ZA_MEMCPY(s.qcGlobals.data(), qcvm->globals, static_cast<size_t>(ng) * sizeof(float));
    for(int i = 0; i < n; i++)
    {
        ZA_MEMCPY(s.qcFields.data() + i * nf, fieldsOf(ents[i]), entBytes);
    }
    restore();
    // The engine's
    agpsCapture = AgPoseSendCapture{};
    agpsCapture.capture = 2;
    const bool done = agPose(g, p);
    const AgPoseSendCapture nativeSend = agpsCapture;
    agpsCapture = AgPoseSendCapture{};
    if(!done)
    {
        restore();
        agArgE(0, g);
        agArgE(1, p);
        agRun(agp.fn_VR_AGP_PoseQC, 2);
        agpStats.leftToQc++;
        return true;
    }
    agpStats.poses++;
    agpStats.struck += struck ? 1 : 0;
    const int differedBefore = agpStats.differing;
    agpStats.noting = struck;
    for(int i = 0; i < pr->numglobaldefs; i++)
    {
        const ddef_t& d = qcvm->globaldefs[i];
        const int w = agpDefWidth(d);
        const char* name = PR_GetString(d.s_name);
        if(!w || !agpComparedName(name) || (struck && !agpPoseName(name)))
        {
            continue;
        }
        const bool isInt = (d.type & ~DEF_SAVEGLOBAL) == ev_entity;
        for(int k = 0; k < w; k++)
        {
            agpNote(agpStats.globalMax[i], agpStats.globalCount[i], s.qcGlobals[d.ofs + k], qcvm->globals[d.ofs + k], isInt,
                "global", d);
        }
    }
    for(int e = 0; e < (struck ? nOwn : n); e++)
    {
        const float* now = fieldsOf(ents[e]);
        const float* qc = s.qcFields.data() + e * nf;
        for(int i = 0; i < pr->numfielddefs; i++)
        {
            const ddef_t& d = qcvm->fielddefs[i];
            const int w = agpDefWidth(d);
            const bool isInt = (d.type & ~DEF_SAVEGLOBAL) == ev_entity;
            if(struck && isInt)
            {
                continue; // (the blow's own entities: its hand's state, made twice)
            }
            for(int k = 0; k < w; k++)
            {
                agpNote(agpStats.fieldMax[i], agpStats.fieldCount[i], qc[d.ofs + k], now[d.ofs + k], isInt, "field", d);
            }
        }
    }
    agpStats.noting = false;
    agpStats.struckDiffering += (struck && agpStats.differing != differedBefore) ? 1 : 0;
    if(!struck && agpStats.differing != differedBefore && agpStats.contexts++ < 12)
    {
        // (where it happened: the humanoid's state then, as the pose left it)
        edict_t* pr2 = AFE(g, vr_ag_prop2);
        Con_Printf("agpose compare: context: %s weapon %g (two %d), act %g kind %g hand %g s %.3f hit %g, took %g, rl %g, "
                   "aimw %g, kick %.3f, tug %d, readyw %g rdy %g, struck %d, flags %g, enemy %s\n",
            PR_GetString(g->v.classname), agHas(p) ? p->v.weapon : -1.f, agHas(pr2) ? 1 : 0, AF(g, vr_ag_act),
            AF(g, vr_ag_act_kind), AF(g, vr_ag_act_hand), agTime() - AF(g, vr_ag_act_at), AF(g, vr_ag_act_hit),
            AF(g, vr_ag_took_at), AF(g, vr_agp_rl_at), AF(g, vr_ag_aimw), agTime() - AF(g, vr_ag_kick_at),
            agHas(AFE(g, vr_ag_tug_pl)) ? 1 : 0, AF(g, vr_ag_readyw), AF(g, vr_agp_rdy), struck ? 1 : 0,
            AG(vr_agp_flags), PR_GetString(PROG_TO_EDICT(g->v.enemy)->v.classname));
    }
    if(qcSend.got != nativeSend.got)
    {
        agpStats.sendMissing++;
    }
    else if(qcSend.got)
    {
        bool differs = false;
        for(int i = 0; i < 14; i++)
        {
            const float d = static_cast<float>(fabs(qcSend.args[i] - nativeSend.args[i]));
            differs = differs || d > 0.f;
            agpStats.sendMax = d > agpStats.sendMax ? d : agpStats.sendMax;
        }
        if(differs && agpStats.sendDiffs == 0)
        {
            Con_Printf("agpose compare: MISMATCH message arguments\n");
        }
        agpStats.sendDiffs += differs ? 1 : 0;
    }
    return true;
}

// vr_agpose_compare: what vr_agpose_native 2 found since the last (then zeroed); its last line the verdict.
void agposeCompareReport()
{
    AgpCompareStats& st = agpStats;
    float worst = 0.f;
    int values = 0, lines = 0;
    const auto show = [&](const char* kind, const ddef_t* defs, const za::Vector<float>& mx, const za::Vector<int>& cnt) {
        for(za::SizeT i = 0; i < mx.size(); i++)
        {
            if(cnt[i] == 0)
            {
                continue;
            }
            values += cnt[i];
            worst = mx[i] > worst ? mx[i] : worst;
            if(lines++ < 40)
            {
                Con_Printf("  %s %s: %d differ, largest %g\n", kind, PR_GetString(defs[i].s_name), cnt[i], mx[i]);
            }
        }
    };
    if(st.progs && st.progs == qcvm->progs)
    {
        show("global", qcvm->globaldefs, st.globalMax, st.globalCount);
        show("field", qcvm->fielddefs, st.fieldMax, st.fieldCount);
    }
    const AgpCoverage& c = agpCov;
    Con_Printf("vr_agpose_compare: through fists %d, melee %d, guns %d, two weapons %d (gun and blade %d), chainsaw %d, "
               "punching %d, swinging %d, taking a weapon up %d (reaching %d), reloading %d, kicked %d, body turned %d, "
               "keys %d, parry met %d, casts %d, shoves %d\n", c.fists, c.melee, c.gun, c.dual, c.gunBlade, c.saw, c.punch,
        c.swing, c.took, c.reaching, c.reload, c.kick, c.body, c.keys, c.meet, c.cast, c.shove);
    const bool pass = st.poses > 0 && worst <= 0.001f && st.sendMax <= 0.001f && st.sendMissing == 0;
    Con_Printf("vr_agpose_compare: %d poses compared (%d with a blow struck: its own state only, %d of them "
               "not comparable, its world changed by the blow twice; %d left to QuakeC), %d values differ (largest %g), message "
               "arguments differ in %d (largest %g), %d sent by one only: %s\n",
        st.poses, st.struck, st.struckDiffering, st.leftToQc, values, worst, st.sendDiffs, st.sendMax, st.sendMissing,
        !pass ? "FAIL" : (values == 0 && st.sendDiffs == 0) ? "PASS exact" : "PASS within 0.001");
    agpCov = AgpCoverage{};
    st = AgpCompareStats{}; // (sized again at the next pose compared)
}

// float(entity g, entity p) agpose: VR_AGP_Pose by the engine (vr_agpose_native). FALSE: not done (QuakeC's to do).
void PF_agpose()
{
    edict_t* g = G_EDICT(OFS_PARM0);
    edict_t* p = G_EDICT(OFS_PARM1);
    const int mode = static_cast<int>(vr_agpose_native.value);
    bool done = false;
    if(mode > 0 && agpBound())
    {
        done = mode >= 2 ? agpCompare(g, p) : agPose(g, p);
    }
    G_FLOAT(OFS_RETURN) = done ? 1.f : 0.f;
}

#undef AF
#undef AFP
#undef AFV
#undef AFE
#undef AG
#undef AGV
#undef AGSET
#undef ACV
