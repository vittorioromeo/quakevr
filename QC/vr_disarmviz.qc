//
//
//
// ----------------------------------------------------------------------------
// Debug > Show Disarm Volumes (vr_debug_disarm; drawn by the engine: vr_disarmviz.cpp)
// ----------------------------------------------------------------------------
//
// Each server frame while it is on (StartFrame, before the weapons' thinks test them), each weapon a living armed
// humanoid holds, standing or an active ragdoll, sends the volumes its disarm tests use, computed by those tests' own
// functions and constants (debugdisarm):
// The volumes are hulls of the triangles drawn (vr_disarmhull.cpp: the weapon's model where drawn, the hand and forearm
// holding it as drawn), each grown by its own units (Weapons > Armed Enemies' "Volume:" rows):
// - its line (VR_AG_WeaponLine: the parry's) and the strike volumes (VR_AG_StrikeGap: vr_agrunt_vol_strike) your blow's
//   sweep meets, its own thickness on top (VR_AG_STRIKE_R);
// - with shot disarms or blocking on (VR_AGS_Frame) and no tug: its shot volumes (VR_AGS_MeetOne: the weapon's and the
//   hand's, vr_agrunt_vol_shot_weapon and _hand, by vr_agrunt_shotdisarm_parts; their near misses' too);
// - with disarms on: the volumes a body blow disarms on (VR_AG_Hit: VR_AG_BlowGap, vr_agrunt_vol_blow);
// - a ragdoll: the limb its weapon rides (VR_AG_RagProp's bone);
// - a tug over it: the hand to its grip, and its progress (VR_AG_TugFrame's share, or time over VR_AG_TugNeed); the tug
//   readout over the weapon (VR_AG_TugReadout: each rule's number against its threshold), and as it ends its verdict
//   (won or lost, by which rule) with the last readout, kept a few seconds (VR_AG_TugEnd: debugdisarmnote 6);
// - its state (debugdisarmnote 0): a tug, or what a grab does now (VR_AG_TugEasy: at once, or a tug).
// And each player's hands (while any is held): the line a strike is measured from (the hand to its weapon's muzzle) and
// its blow's line the parry sweeps (VR_Blow's last). The engine adds the fists as the grab measures them (which weapon a
// grip takes now), and the hits the tests mark (VR_DV_Mark: vr_armedgrunt*.qc, guarded by vr_dv_on).

float VR_DV_W_HIT = 1;     // the kinds (vr_disarmviz.cpp Kind: keep in step)
float VR_DV_W_SHOT = 2;
float VR_DV_W_NEAR = 3;
float VR_DV_H_SHOT = 4;
float VR_DV_H_NEAR = 5;
float VR_DV_GRIP_BLOW = 6;
float VR_DV_LIMB_BLOW = 7;
float VR_DV_RAG = 8;
float VR_DV_P_STRIKE = 9;
float VR_DV_P_SWEEP = 10;
float VR_DV_TUG = 11;
float VR_DV_H_HIT = 20;    // (12-19: the directional parry's)

void(entity e, vector at, float what, string why) VR_DV_Mark =
{
    if(vr_dv_on)
    {
        debugdisarmnote(e, what, at, why);
    }
};

// The held weapon `p` of `g` (`pl`: the player a grab would be by).
void(entity g, entity p, entity pl) VR_DV_Weapon =
{
    // (The volumes are the hulls of the weapon and of the hand holding it as drawn, each grown by its own units: the engine
    // draws them where they are drawn this frame. The hand's: `a` its side, '0 0 0' the main weapon's.)
    VR_AG_WeaponLine(p);
    vector wa = vr_ag_wa;
    vector wb = vr_ag_wb;
    vector hs = p.vr_ag_left ? '1 0 0' : '0 0 0';
    float strike = cvar_hget(cvarh_vr_agrunt_vol_strike);
    debugdisarm(p, VR_DV_W_HIT, wa, wb, strike);
    debugdisarm(g, VR_DV_H_HIT, hs, hs, strike);
    if((cvar_hget(cvarh_vr_agrunt_shotdisarm) || cvar_hget(cvarh_vr_agrunt_shotdisarm_block)) && !g.vr_ag_tug_pl)
    {
        float parts = cvar_hget(cvarh_vr_agrunt_shotdisarm_parts);
        float near = max(0, cvar_hget(cvarh_vr_agrunt_shotdisarm_near));
        float gw = cvar_hget(cvarh_vr_agrunt_vol_shot_weapon);
        float gh = cvar_hget(cvarh_vr_agrunt_vol_shot_hand);
        if(parts != 1)
        {
            debugdisarm(p, VR_DV_W_SHOT, wa, wb, gw);
            if(near > 0)
            {
                debugdisarm(p, VR_DV_W_NEAR, wa, wb, gw + near);
            }
        }
        if(parts != 0)
        {
            debugdisarm(g, VR_DV_H_SHOT, hs, hs, gh);
            if(near > 0)
            {
                debugdisarm(g, VR_DV_H_NEAR, hs, hs, gh + near);
            }
        }
    }
    if(cvar_hget(cvarh_vr_agrunt_disarm))
    {
        float blow = cvar_hget(cvarh_vr_agrunt_vol_blow); // (VR_AG_Hit's: VR_AG_BlowGap)
        debugdisarm(p, VR_DV_GRIP_BLOW, wa, wb, blow);
        debugdisarm(g, VR_DV_LIMB_BLOW, hs, hs, blow);
    }
    float rag = isragdoll(g);
    if(rag && p.vr_ag_rb > 0)
    {
        debugdisarm(g, VR_DV_RAG, p.origin, p.origin, p.vr_ag_rb - 1);
    }
    string s;
    if(g.vr_ag_tug_pl && g.vr_ag_prop == p)
    {
        float share = g.vr_ag_tug_rag ? g.vr_ag_tug_prog : g.vr_ag_tug_prog / max(0.01, VR_AG_TugNeed(g));
        debugdisarm(p, VR_DV_TUG, VRGetEntHandPos(g.vr_ag_tug_pl, g.vr_ag_tug_hand), p.origin, share);
        s = VR_AG_TugReadout(g); // (every number its rules read; its verdict kept a few seconds as it ends: VR_AG_TugEnd)
    }
    else if(!cvar_hget(cvarh_vr_agrunt_disarm_yank))
    {
        s = "grab: off (vr_agrunt_disarm_yank)";
    }
    else if(g.vr_ag_tug_pl)
    {
        s = "grab: none (a tug over its other)";
    }
    else
    {
        string why = pl ? VR_AG_TugEasy(g, pl) : "";
        s = why != "" ? sprintf("grab: at once (%s)", why) : "grab: a tug";
    }
    string gs = VR_AGG_Label(g); // (its guard: preparry, windup, recovering; vr_armedgrunt_guard.qc)
    if(gs != "")
    {
        s = sprintf("%s; %s", s, gs);
    }
    debugdisarmnote(p, 0, p.origin + '0 0 16', rag ? strcat(s, "; ragdoll") : s);
};

// A player's hand `h`: its strike's line and its blow's.
void(entity pl, float h) VR_DV_Hand =
{
    vector hp = VRGetEntHandPos(pl, h);
    vector mz = VR_HAND_FIELD(pl, h, muzzlepos, offmuzzlepos);
    if(VRIsHandEmpty(pl, h) || vlen(mz - hp) > 48)
    {
        mz = hp;
    }
    debugdisarm(pl, VR_DV_P_STRIKE, hp, mz, 0);
    entity was = self;
    self = pl;
    entity b = VR_Blow(h);
    self = was;
    float n = b.mh_n;
    if(n >= 1 && fabs(time - b.mh_time) <= 0.2)
    {
        debugdisarm(pl, VR_DV_P_SWEEP, b.mh_pp[n - 1], b.mh_cp[n - 1], 0);
    }
};

// StartFrame: everything above, while vr_debug_disarm is on (off: one cvar read).
void() VR_DV_Frame =
{
    vr_dv_on = cvar_hget(cvarh_vr_debug_disarm);
    if(!vr_dv_on)
    {
        return;
    }
    entity pl = VR_Player();
    float held = 0;
    entity p = find(world, classname, "vr_ag_weapon");
    while(p)
    {
        entity g = p.owner;
        if(g.vr_ag && g.health > 0 && (g.vr_ag_prop == p || g.vr_ag_prop2 == p) && p.model != "")
        {
            VR_DV_Weapon(g, p, pl);
            held = held + 1;
        }
        p = find(p, classname, "vr_ag_weapon");
    }
    while(held && pl)
    {
        if(pl.health > 0)
        {
            VR_DV_Hand(pl, cVR_MainHand);
            VR_DV_Hand(pl, cVR_OffHand);
        }
        pl = VR_NextPlayer(pl);
    }
};
