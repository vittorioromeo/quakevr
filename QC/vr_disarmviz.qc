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
// - its line (VR_AG_WeaponLine) and the reach round it a strike on it (VR_AG_HandStrikeSpeed: 5) and a parry's sweep
//   (VR_AGPAR_R) meet it within;
// - with shot disarms or blocking on (VR_AGS_Frame) and no tug: its shot volumes (VR_AGS_MeetOne: the weapon's line and
//   its hand and forearm, VR_AGS_HandLine, by vr_agrunt_shotdisarm_parts and _size; their near misses' too);
// - with disarms on: the spheres round its grip a body blow disarms within (VR_AG_Hit: VR_AG_DISARM_HAND, a limb's x1.6);
// - a ragdoll: the limb its weapon rides (VR_AG_RagProp's bone);
// - a tug over it: the hand to its grip, and its progress (VR_AG_TugFrame's share, or time over VR_AG_TugNeed);
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
float VR_DV_STRIKE_R = 5;  // VR_AG_HandStrikeSpeed's reach (units round the weapon's line)

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
    VR_AG_WeaponLine(p);
    vector wa = vr_ag_wa;
    vector wb = vr_ag_wb;
    debugdisarm(p, VR_DV_W_HIT, wa, wb, max(VR_DV_STRIKE_R, VR_AGPAR_R));
    if((cvar_hget(cvarh_vr_agrunt_shotdisarm) || cvar_hget(cvarh_vr_agrunt_shotdisarm_block)) && !g.vr_ag_tug_pl)
    {
        float parts = cvar_hget(cvarh_vr_agrunt_shotdisarm_parts);
        float size = max(0, cvar_hget(cvarh_vr_agrunt_shotdisarm_size));
        float near = max(0, cvar_hget(cvarh_vr_agrunt_shotdisarm_near));
        if(parts != 1)
        {
            debugdisarm(p, VR_DV_W_SHOT, wa, wb, VR_AGS_W_RADIUS * size);
            if(near > 0)
            {
                debugdisarm(p, VR_DV_W_NEAR, wa, wb, VR_AGS_W_RADIUS * size + near);
            }
        }
        if(parts != 0)
        {
            VR_AGS_HandLine(g, p);
            debugdisarm(p, VR_DV_H_SHOT, vr_ags_ha, vr_ags_hb, VR_AGS_H_RADIUS * size);
            if(near > 0)
            {
                debugdisarm(p, VR_DV_H_NEAR, vr_ags_ha, vr_ags_hb, VR_AGS_H_RADIUS * size + near);
            }
        }
    }
    if(cvar_hget(cvarh_vr_agrunt_disarm))
    {
        debugdisarm(p, VR_DV_GRIP_BLOW, p.origin, p.origin, VR_AG_DISARM_HAND);
        debugdisarm(p, VR_DV_LIMB_BLOW, p.origin, p.origin, VR_AG_DISARM_HAND * 1.6);
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
        s = sprintf("TUG %.0f/100%s", min(1, share) * 100, g.vr_ag_tug_rag ? " (physical)" : "");
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
