//
//
//
// ----------------------------------------------------------------------------
// Directional parry (vr_dirparry, experimental; ROUND23.md, "Directional parry"). His words (2026-10-31): parrying an
// armed humanoid's melee weapon by "the direction of the enemy weapon and strike": overhead strikes parried by holding
// your weapon horizontally against them, horizontal strikes by holding it vertically; no poses written in: the parrying
// weapon square to the enemy weapon during the strike, physically meeting it; sparks where the two weapons collide.
//
// - Its side: while it swings (VR_AGP_SWING, a melee weapon; not the chainsaw's thrust), every frame its blade's line as
//   drawn (VR_AG_WeaponLine: the engine's hitreact::drawnWeapon, held::drawnExtent) and last frame's give its axis and
//   its way (the sweep: the velocity of its points, square to the blade).
// - Yours: each hand's weapon's parry line, from the hand to its far end (VR_Parry_WeaponLine: VR_Melee_Line's, the
//   weapon's own shape: a sword's blade, the axe's handle and head, a gun's barrel to its muzzle). Fists: none (the
//   crossed arms' parry stays as it was).
// - They meet: its blade swept from last frame's line to this frame's (and, at its blow, on along its way for
//   vr_dirparry_window s) comes within vr_dirparry_contact of your line. Your line is best square to both its blade and
//   its way (the normal of the plane it sweeps): overhead (the blade ahead, going down) a weapon held level across,
//   a side swing (going sideways) one held upright. Its angle off that: full marks within vr_dirparry_angle, none at 90.
//   Score: that times the meeting's (1 within the contact distance, falling to 0 at twice it); vr_dirparry_threshold or
//   more: parried.
// - Parried: no damage; sparks at the meeting (the middle of the two lines' nearest points) and the clang; the parry's
//   own sound, haptics, hand knock, stamina, counter window and stagger (VR_Parry); its swing stopped (no blow from it;
//   a flinch of vr_dirparry_opening s if the parry balance gave none). Under the threshold: the blow lands, cut by
//   vr_dirparry_partial x score / threshold (0: off).
// - Met before its blow (VR_DRP_Frame, every frame of the swing's strike): parried there and then. At its blow
//   (VR_Parry): read on along its way (the window: where it would have met your weapon in the swing's rest).
// - Debug > Show Disarm Volumes: both lines, its way and the score (vr_disarmviz.cpp kinds 12-14).
// ----------------------------------------------------------------------------

float VR_DV_DIR_YOURS = 12; // Show Disarm Volumes' kinds (vr_disarmviz.cpp Kind: keep in step)
float VR_DV_DIR_BLADE = 13;
float VR_DV_DIR_WAY = 14;
float vr_drp_hand;      // the last reading's: the hand whose weapon met it best (-1: none)
float vr_drp_score;     // ... its score (0..1)
float vr_drp_gap;       // ... the two lines' least distance on its way (units)
float vr_drp_off;       // ... your line's angle off square to its blade and its way (degrees)
float vr_drp_tau;       // ... when on its way (s from its line now; negative: since last frame)
vector vr_drp_at;       // ... where they met (the middle of their nearest points)
vector vr_drp_pa;       // ... your weapon's parry line (VR_DRP_PlayerSeg's out)
vector vr_drp_pb;
vector vr_drp_ea;       // ... its blade's line then
vector vr_drp_eb;
vector vr_drp_sweep;    // ... its way there (unit)
float vr_drp_forced;    // VR_DRP_Frame's meeting: VR_Parry takes vr_drp_hand at once
float vr_drp_evals;     // readings (the tests)
float vr_drp_parries;   // directional parries (the tests)
float vr_drp_early;     // ... of them met before its blow
float vr_drp_fx_n;      // sparks thrown (the tests)
vector vr_drp_fx_at;    // ... where the last were

// Your hand `h`'s weapon's parry line into vr_drp_pa .. vr_drp_pb: the hand to its far end (FALSE: an empty hand, a fist).
float(entity pl, float h) VR_DRP_PlayerSeg =
{
    entity was = self;
    self = pl; // (VR_Melee_Line's helpers read self: VR_Parry_HandFor)
    float ok = VR_Parry_WeaponLine(pl, h);
    vector grip = vr_mline_grip;
    self = was;
    if(!ok)
    {
        return FALSE;
    }
    vr_drp_pa = grip;
    vr_drp_pb = vr_parry_end;
    if(vlen(vr_drp_pb - vr_drp_pa) < 8)
    {
        vr_drp_pa = vr_parry_butt; // (held near its far end: all of it)
    }
    return TRUE;
};

// Whether `g`'s blow now is judged by its direction: vr_dirparry on, an armed humanoid swinging a melee weapon.
float(entity g) VR_DRP_Applies =
{
    if(!g.vr_ag || g.vr_ag_act != VR_AGP_SWING || g.vr_eshove_striking || !cvar_hget(cvarh_vr_dirparry))
    {
        return FALSE;
    }
    entity w = vr_drp_w ? vr_drp_w : g.vr_ag_prop;
    return w && VR_AG_Class(w.weapon) == VR_AG_CLASS_MELEE && w.weapon != WID_CHAINSAW;
};

// This swing of `g`'s was stopped by a directional parry already.
float(entity g) VR_DRP_Done =
{
    return g.vr_drp_swing > 0 && g.vr_drp_swing == g.vr_ag_act_at;
};

// One reading: `g`'s weapon `w` swung at `pl`, its way followed on `ahead` s from its line now. Out: vr_drp_* (the best
// hand's). Its score.
float(entity g, entity w, entity pl, float ahead) VR_DRP_Eval =
{
    vr_drp_hand = -1;
    vr_drp_score = 0;
    vr_drp_gap = 9999;
    vr_drp_off = 90;
    vr_drp_tau = 0;
    vr_drp_evals = vr_drp_evals + 1;
    VR_AG_WeaponLine(w);
    vector a1 = vr_ag_wa;
    vector b1 = vr_ag_wb;
    float dt = time - w.vr_drp_t0;
    vector va = '0 0 0';
    vector vb = '0 0 0';
    if(w.vr_drp_t0 > 0 && dt > 0.001 && dt < 0.1)
    {
        va = (a1 - w.vr_drp_a0) * (1 / dt);
        vb = (b1 - w.vr_drp_b0) * (1 / dt);
    }
    else
    {
        dt = 0;
    }
    float contact = max(0.5, cvar_hget(cvarh_vr_dirparry_contact));
    float lenient = min(89, max(0, cvar_hget(cvarh_vr_dirparry_angle)));
    float n = 4 + 2 * ceil(max(0, ahead) * 72); // (samples along its way: two a frame)
    float span = dt + max(0, ahead);
    vector keepA = '0 0 0';
    vector keepB = '0 0 0';
    for(float h = 0; h < 2; h++)
    {
        if(!VR_DRP_PlayerSeg(pl, h))
        {
            continue;
        }
        vector pa = vr_drp_pa;
        vector pb = vr_drp_pb;
        float best = 9999;
        float bt = 0;
        float bs = 0;
        float bp = 0;
        for(float k = 0; k <= n; k++)
        {
            float tau = -dt + span * k / n;
            float d = VR_AG_SegDist(a1 + va * tau, b1 + vb * tau, pa, pb);
            if(d < best)
            {
                best = d;
                bt = tau;
                bs = vr_ag_seg_s;
                bp = vr_ag_seg_t;
            }
            if(d <= contact)
            {
                break; // (the first meeting on its way)
            }
        }
        vector ea = a1 + va * bt;
        vector eb = b1 + vb * bt;
        vector axis = normalize(eb - ea);
        vector v = va + (vb - va) * bs;
        vector sw = v - axis * (v * axis);
        if(vlen(sw) < 20)
        {
            sw = pl.origin - g.origin; // (its blade still: its way taken as towards you)
            sw = sw - axis * (sw * axis);
        }
        sw = normalize(sw);
        vector sq = normalize(VR_Cross(axis, sw)); // (square to its blade and its way)
        vector px = normalize(pb - pa);
        float off = atan2(vlen(VR_Cross(px, sq)), fabs(px * sq)) * 57.29578;
        float angleScore = off <= lenient ? 1 : max(0, (90 - off) / max(1, 90 - lenient));
        float cs = best <= contact ? 1 : max(0, 1 - (best - contact) / contact);
        float score = angleScore * cs;
        if(vr_drp_hand < 0 || score > vr_drp_score)
        {
            vr_drp_hand = h;
            vr_drp_score = score;
            vr_drp_gap = best;
            vr_drp_off = off;
            vr_drp_tau = bt;
            vr_drp_ea = ea;
            vr_drp_eb = eb;
            vr_drp_sweep = sw;
            vr_drp_at = (ea + (eb - ea) * bs + pa + (pb - pa) * bp) * 0.5;
            keepA = pa;
            keepB = pb;
        }
    }
    if(vr_drp_hand >= 0)
    {
        vr_drp_pa = keepA;
        vr_drp_pb = keepB;
    }
    return vr_drp_score;
};

// The reading's lines on Show Disarm Volumes (vr_disarmviz.cpp: 12 your line, green parried, red not; 13 its blade then;
// 14 its way), its score by the meeting.
void(entity g, entity w, float parried) VR_DRP_Show =
{
    if(!vr_dv_on || vr_drp_hand < 0)
    {
        return;
    }
    debugdisarm(w, VR_DV_DIR_YOURS, vr_drp_pa, vr_drp_pb, parried ? 1 : 0);
    debugdisarm(w, VR_DV_DIR_BLADE, vr_drp_ea, vr_drp_eb, 0);
    debugdisarm(w, VR_DV_DIR_WAY, vr_drp_at, vr_drp_at + vr_drp_sweep * 12, 0);
    debugdisarmnote(w, 0, vr_drp_at + '0 0 4', sprintf("dirparry %.2f%s: %.0f deg off square, gap %.1f", vr_drp_score,
        parried ? " PARRIED" : "", vr_drp_off, vr_drp_gap));
};

// VR_AGP_Pose, every frame `g` holds the melee weapon `w` (before its swing's pose and blow): its swing's strike met
// your weapon held across it: parried now (VR_Parry, forced), the swing stopped.
void(entity g, entity w) VR_DRP_Frame =
{
    if(!w || w.vr_drp_t0 == time)
    {
        return; // (once a frame)
    }
    entity pl = g.enemy;
    float live = cvar_hget(cvarh_vr_dirparry) && g.vr_ag_act == VR_AGP_SWING && w.weapon != WID_CHAINSAW && g.health > 0 &&
               pl && (pl.flags & FL_CLIENT) && pl.health > 0 && VRHandsTracked(pl);
    if(!live)
    {
        w.vr_drp_t0 = 0;
        return;
    }
    VR_AGP_Times(g);
    float s = time - g.vr_ag_act_at;
    float left = w.vr_ag_left && g.vr_ag_prop2;
    float struck = left ? g.vr_ag_act_hitl : g.vr_ag_act_hit;
    float parried = FALSE;
    if((s >= vr_agp_tw) && (s < vr_agp_tw + vr_agp_ts) && !struck && !VR_DRP_Done(g) &&
       (vlen(pl.origin - g.origin) < 150))
    {
        VR_DRP_Eval(g, w, pl, 0);
        parried = ((vr_drp_hand >= 0) && (vr_drp_gap <= max(0.5, cvar_hget(cvarh_vr_dirparry_contact))) &&
                   (vr_drp_score >= cvar_hget(cvarh_vr_dirparry_threshold)));
        VR_DRP_Show(g, w, parried);
        if(parried)
        {
            vr_drp_forced = TRUE;
            vr_drp_w = w;
            VR_Parry(pl, g, g, 20); // (its damage unknown here: the parry's events' only; nothing is dealt)
            vr_drp_w = world;
            vr_drp_forced = FALSE;
            vr_drp_early = vr_drp_early + 1;
            g.vr_ag_act_hit = 1; // (no blow from this swing)
            g.vr_ag_act_hitl = 1;
        }
    }
    VR_AG_WeaponLine(w);
    w.vr_drp_a0 = vr_ag_wa;
    w.vr_drp_b0 = vr_ag_wb;
    w.vr_drp_t0 = time;
};

// VR_Parry, `g`'s blow on `pl` (VR_DRP_Applies): the hand whose weapon parried it (-1: none; vr_drp_cut a partial
// parry's share), vr_parry_at the meeting. Met before its blow (VR_DRP_Frame): that reading's.
float(entity pl, entity g) VR_DRP_Blow =
{
    float threshold = cvar_hget(cvarh_vr_dirparry_threshold);
    if(!vr_drp_forced)
    {
        entity w = vr_drp_w ? vr_drp_w : g.vr_ag_prop;
        VR_DRP_Eval(g, w, pl, max(0, cvar_hget(cvarh_vr_dirparry_window)));
        VR_DRP_Show(g, w, vr_drp_score >= threshold);
    }
    vr_drp_cut = 0;
    if(vr_drp_hand < 0 || vr_drp_score < threshold || vr_drp_score <= 0)
    {
        if(vr_drp_score > 0 && threshold > 0)
        {
            vr_drp_cut = max(0, cvar_hget(cvarh_vr_dirparry_partial)) * min(1, vr_drp_score / threshold);
        }
        dprint(sprintf("dirparry: %s's blow lands: score %.2f (%.0f deg off square, gap %.1f units), cut %.2f\n",
            g.classname, vr_drp_score, vr_drp_off, vr_drp_gap, vr_drp_cut));
        return -1;
    }
    g.vr_drp_swing = g.vr_ag_act_at;
    vr_drp_parries = vr_drp_parries + 1;
    vr_parry_at = vr_drp_at;
    dprint(sprintf("dirparry: %s's blow parried by hand %g: score %.2f (%.0f deg off square, gap %.1f units, %.3f s %s)\n",
        g.classname, vr_drp_hand, vr_drp_score, vr_drp_off, vr_drp_gap, fabs(vr_drp_tau),
        vr_drp_tau > 0 ? "ahead" : "ago"));
    if(vr_dv_on)
    {
        VR_DV_Mark(vr_drp_w ? vr_drp_w : g.vr_ag_prop, vr_drp_at, VR_DV_PARRY, sprintf("dirparry %.2f", vr_drp_score));
    }
    return vr_drp_hand;
};

// VR_Parry's directional parry by your hand `h`: sparks where the two weapons met (vr_parry_at), off both, and the clang
// of metal on metal (the impact table's rows, as blade on blade's).
void(entity pl, entity g, float h) VR_DRP_Effects =
{
    vector x = vr_parry_at;
    vector way = vr_drp_sweep != '0 0 0' ? vr_drp_sweep : normalize(pl.origin - g.origin);
    VR_ImpactFx(VR_MAT_METAL, VR_HIT_BLOW, g, x, way * -1 + '0 0 0.5', 60); // (sparks off both, as blade on blade's)
    vr_drp_fx_n = vr_drp_fx_n + 1;
    vr_drp_fx_at = x;
    string snd = impactsound(VR_MAT_METAL, VR_HIT_BLADE, VR_IMPACT_SOUND);
    if(snd != "")
    {
        sound(g, CHAN_AUTO, snd, 0.9, ATTN_NORM);
    }
};
