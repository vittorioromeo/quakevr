// ----------------------------------------------------------------------------
// vr_armedgrunt_dazzle.qc -- armed humanoids blinded by a flashlight (his idea, 2026-11-27; ROUND26.md "flashblind")
// ----------------------------------------------------------------------------
// A player's flashlight pointed straight at an armed humanoid's face dazzles it: a level (.vr_ag_dazzle, 0..1) that
// builds while the beam is on its eyes and fades after. Above vr_agrunt_dazzle_threshold its off hand comes up in front
// of its face (VR_AGP_Shield, on the shared pose path: QuakeC's VR_AGP_PoseQC and the engine's agpose both call it, the
// client's arm IK draws it; the pose message carries it to every client as any other off-hand target); a two-handed gun
// is then held and fired in one hand. Its shots are worse by the dazzle (VR_AG_DazzleAim: VR_AGAI_Accuracy divides by
// it, so every shot path has it: the layer's guns, the replacements' own guns, the enforcer's laser).
//
// The beam is the client's own (flashlightbeam: the lamp as his client lit it, VR or flat-screen, each coop player's);
// the server decides. Bots carry no flashlight. Tunables: Combat > Armed Grunts - Flashlight Dazzle (vr_agrunt_dazzle*).
// Test: vr_agrunt_test 232 (vr_armedgrunt_dazzle_test.qc).

float VR_AG_DAZZLE_FACE_COS = 0.17; // the lens must be ahead of its face (within about 80 degrees of where it looks)

// Its eyes (world): its view offset, else most of the way up its box.
vector(entity g) VR_AG_DazzleEye =
{
    if(g.view_ofs != '0 0 0')
    {
        return g.origin + g.view_ofs;
    }
    return g.origin + '0 0 1' * (g.maxs_z * 0.7);
};

// How strongly player `pl`'s lamp shines in `g`'s eyes now (0 none .. about 1.5: its dark share on top): on, within its
// reach and vr_agrunt_dazzle_range, within vr_agrunt_dazzle_cone degrees of its axis (full within 0.4 of it), the lens
// ahead of its face, nothing in between (VR_Stealth_Line: on through see-through walls, as its light goes).
float(entity g, entity pl) VR_AG_DazzleFrom =
{
    vector b = flashlightbeam(pl, 2);
    if(b_x < 1)
    {
        return 0;
    }
    vector lens = flashlightbeam(pl, 0);
    vector dir = flashlightbeam(pl, 1);
    vector eye = VR_AG_DazzleEye(g);
    vector to = eye - lens;
    float d = vlen(to);
    float far = min(b_y, cvar_hget(cvarh_vr_agrunt_dazzle_range));
    if(d < 1 || d >= far)
    {
        return 0;
    }
    vector n = to * (1 / d);
    float c = n * dir;
    if(c <= 0)
    {
        return 0;
    }
    float cone = max(0.5, cvar_hget(cvarh_vr_agrunt_dazzle_cone));
    float ang = atan2(vlen(n - dir * c), c) * 57.29578;
    float ax = min(1, max(0, (cone - ang) / (cone * 0.6)));
    if(ax <= 0)
    {
        return 0;
    }
    makevectors([0, g.angles_y, 0]);
    if((n * -1) * v_forward < VR_AG_DAZZLE_FACE_COS)
    {
        return 0; // (its back or its side to the lens)
    }
    float near = min(far - 1, max(0, cvar_hget(cvarh_vr_agrunt_dazzle_near)));
    float dk = d <= near ? 1 : (far - d) / (far - near);
    VR_Stealth_Line(lens, eye, FALSE, pl);
    if(trace_fraction < 1 && trace_ent != g)
    {
        return 0;
    }
    // (darker places count more: the light on its face as the level has it, 0 light .. vr_agrunt_dazzle_dark more;
    // unknown (a dedicated server): as lit)
    float lt = stealthlight(eye);
    float dark = lt < 0 ? 0 : 1 - min(1, lt / 160);
    return ax * dk * (1 + max(0, cvar_hget(cvarh_vr_agrunt_dazzle_dark)) * dark);
};

// Each pose (VR_AGP_Pose: QuakeC's or the engine's): its dazzle eased to the strongest lamp on its eyes (built over
// vr_agrunt_dazzle_build seconds, faded over vr_agrunt_dazzle_fade), once a server frame whoever poses it.
void(entity g) VR_AG_DazzleTick =
{
    float dt = time - g.vr_ag_dazzle_at;
    if(dt <= 0 || vr_agp_quiet)
    {
        return;
    }
    g.vr_ag_dazzle_at = time;
    dt = min(dt, 0.25);
    if(!cvar_hget(cvarh_vr_agrunt_dazzle) || g.health <= 0)
    {
        g.vr_ag_dazzle = 0;
        g.vr_ag_dazzle_lit = 0;
        return;
    }
    float lit = 0;
    vector keepF = v_forward;
    vector keepR = v_right;
    vector keepU = v_up;
    for(entity pl = VR_Player(); pl; pl = VR_NextPlayer(pl))
    {
        if(pl.health > 0)
        {
            lit = max(lit, VR_AG_DazzleFrom(g, pl));
        }
    }
    v_forward = keepF;
    v_right = keepR;
    v_up = keepU;
    g.vr_ag_dazzle_lit = lit;
    float to = min(1, lit);
    if(g.vr_ag_dazzle < to)
    {
        g.vr_ag_dazzle = min(to, g.vr_ag_dazzle + dt / max(0.05, cvar_hget(cvarh_vr_agrunt_dazzle_build)));
    }
    else
    {
        g.vr_ag_dazzle = max(to, g.vr_ag_dazzle - dt / max(0.05, cvar_hget(cvarh_vr_agrunt_dazzle_fade)));
    }
};

// Its shield's share by its dazzle: 0 at vr_agrunt_dazzle_threshold, 1 at full, times vr_agrunt_dazzle_pose.
float(entity g) VR_AG_DazzleShare =
{
    float thr = min(0.95, max(0, cvar_hget(cvarh_vr_agrunt_dazzle_threshold)));
    return VR_Smooth((g.vr_ag_dazzle - thr) / (1 - thr)) * min(1, max(0, cvar_hget(cvarh_vr_agrunt_dazzle_pose)));
};

// Its gun `p` is held in both hands (a long gun, its Weapon Holds row not One-Handed, its off hand not keyed Free): the
// shield takes it into one hand.
float(entity g, entity p) VR_AG_GunTwoHands =
{
    if(!p || g.vr_ag_prop2 || VR_AG_Class(p.weapon) == VR_AG_CLASS_MELEE || VR_AG_Class(p.weapon) == VR_AG_CLASS_FISTS)
    {
        return FALSE;
    }
    if(!VR_AGP_Long(p))
    {
        return FALSE;
    }
    float keepSet = vr_agh_set;
    float h = VR_AGH_Hands(g, p);
    vr_agh_set = keepSet;
    return h != 1;
};

// Its shots' aim error and spread times this (VR_AGAI_Accuracy divides by it): 1 + its dazzle x vr_agrunt_dazzle_spread,
// and a two-handed gun held in one hand behind its shield loses the two-hand spread reduction (as yours does).
float(entity g) VR_AG_DazzleAim =
{
    if(g.vr_ag_dazzle <= 0 && g.vr_ag_shieldw <= 0)
    {
        return 1;
    }
    float m = 1 + g.vr_ag_dazzle * max(0, cvar_hget(cvarh_vr_agrunt_dazzle_spread));
    if(g.vr_ag_shieldw > 0 && VR_AG_GunTwoHands(g, g.vr_ag_prop))
    {
        m = m + VR_Smooth(g.vr_ag_shieldw) * (1 / max(0.05, cvar_hget(cvarh_vr_2h_spread_reduction)) - 1);
    }
    return m;
};

// The shield (VR_AGP_PoseQC and the engine's agpose call it after the hands' poses, before the run's sway): its off hand
// raised before its eyes (vr_agrunt_dazzle_hand_*: the grunt's body, fitted to each kind), eased in and out; a two-handed
// gun taken into the gun hand alone (its one-handed hold blended in). Not with two weapons held, nor while that hand
// punches, swings a two-handed weapon, casts or reaches for a weapon.
void(entity g, entity p) VR_AGP_Shield =
{
    float target = 0;
    float c = p ? VR_AG_Class(p.weapon) : VR_AG_CLASS_FISTS;
    float keepSet = vr_agh_set;
    if(!g.vr_ag_prop2 && g.health > 0 && g.vr_ag_took_at <= 0)
    {
        target = VR_AG_DazzleShare(g);
        if(g.vr_ag_act && (c == VR_AG_CLASS_FISTS || g.vr_ag_act == VR_AGP_CAST || (c == VR_AG_CLASS_MELEE && VR_AGP_TwoHanded(p))))
        {
            target = 0;
        }
    }
    vr_agh_set = keepSet;
    if(!vr_agp_quiet)
    {
        g.vr_ag_shieldw = VR_AGP_Ease(g.vr_ag_shieldw, target, 4);
    }
    float w = VR_Smooth(g.vr_ag_shieldw);
    if(w <= 0 || g.vr_ag_prop2)
    {
        return;
    }
    VR_AM_Row(g.vr_ag_kind);
    VR_AGP_Body(g);
    // Where the off hand is now (its body's space): its own target, a two-handed gun's second grip, else hanging.
    vector from = VR_AGP_Hang(1);
    if(vr_agp_flags & VR_AGP_LEFT)
    {
        from = vr_am_anchor_at * vr_agp_wl + vr_agp_vl;
    }
    else if(VR_AG_GunTwoHands(g, p))
    {
        vector keepV = vr_agp_v;
        vector keepFw = vr_agp_fw;
        vector keepUp = vr_agp_up;
        vector keepL = vr_agp_vl;
        float keepWl = vr_agp_wl;
        float keepFl = vr_agp_flags;
        float keepAim = g.vr_ag_aimw;
        vector fwL = VR_AGP_ToLocal(keepFw);
        vector upL = VR_AGP_ToLocal(keepUp);
        vector sec = (p.vr_agp_sec != '0 0 0' && p.vr_agp_sec_z < 999) ? p.vr_agp_sec : p.vr_agp_muz * 0.4;
        from = keepV + vr_am_anchor_at + fwL * sec_x + VR_Cross(upL, fwL) * sec_y + upL * sec_z;
        // (its one-handed hold, blended in: the gun out from its shoulder in the right hand)
        vr_agp_onehand = 1;
        VR_AGP_ArmsGun(g, p, world);
        vr_agp_onehand = 0;
        g.vr_ag_aimw = keepAim; // (eased once this frame already)
        vr_agp_v = VR_Lerp(keepV, vr_agp_v, w);
        vr_agp_fw = VR_AGP_Nlerp(keepFw, vr_agp_fw, w);
        vr_agp_up = VR_AGP_Nlerp(keepUp, vr_agp_up, w);
        vr_agp_vl = keepL;
        vr_agp_wl = keepWl;
        vr_agp_flags = keepFl;
        VR_AGP_Body(g);
    }
    vector hand;
    hand_x = cvar_hget(cvarh_vr_agrunt_dazzle_hand_fwd);
    hand_y = cvar_hget(cvarh_vr_agrunt_dazzle_hand_left);
    hand_z = cvar_hget(cvarh_vr_agrunt_dazzle_hand_up);
    vector face = VR_AM_Fit(g, hand);
    vr_agp_flags = (vr_agp_flags - (vr_agp_flags & (VR_AGP_TWO | VR_AGP_LEFTWPN))) | VR_AGP_LEFT;
    vr_agp_wl = 1;
    vr_agp_vl = VR_Lerp(from, face, w) - vr_am_anchor_at;
    if(!vr_agp_quiet)
    {
        g.vr_ag_shield_at = g.origin + VR_AGP_ToWorld(VR_Lerp(from, face, w)); // (the tests: where its hand is sent)
    }
};
