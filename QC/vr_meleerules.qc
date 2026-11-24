// vr_meleerules.qc -- enemy melee under one set of rules (ROUND24.md, "Melee rules"): each kind of blow's rule for
// Show Melee Arcs (fight.qc VR_MeleeArc_ShowFrame; the engine draws it: vr_bodycontact.cpp meleeArcDraw). After every
// monster's file: it names their attacks.

// m's blows' rule (VR_MELEE_RULE_*), -1: none shown (no blow of its own: the eel's zap, a burst all round it; the lava
// man's fireball; a zombie's thrown flesh, the MG3 shub zombie's; Armagon's; the spike mine's homing).
float(entity m) VR_MeleeRule_Of =
{
    if(m.classname == "monster_dragon")
    {
        return VR_MELEE_RULE_ROUND; // (its tail all round it: rogue_dragon.qc dragon_tail, dragon_tail_touch)
    }
    if(m.vr_ag)
    {
        return VR_MELEE_RULE_ARC; // (an armed humanoid's blows: its weapon's reach and the arc, vr_armedgrunt_pose.qc)
    }
    if(m.th_melee == tbaby_jump1)
    {
        return VR_MELEE_RULE_LEAP; // (the spawn's, the MG3 slime's)
    }
    if(m.th_melee == zombie_missile || m.th_melee == armagon_stop1 || m.th_melee == spikemine_Home ||
       m.th_melee == lavaman_fire1 || m.th_melee == eel_attack1 || m.th_melee == SUB_Null)
    {
        return -1;
    }
    if(m.th_melee || AD_HasMelee(m))
    {
        return VR_MELEE_RULE_ARC;
    }
    if(m.th_vrshove && cvar_hget(cvarh_vr_enemy_shove))
    {
        return VR_MELEE_RULE_SHOVE; // (a grunt's, an enforcer's, a vore's: vr_enemyshove.qc)
    }
    return -1;
};

// The reach Show Melee Arcs draws for a rule other than the arc's (units from its middle): a leap's from how far it
// leaps (Quake's melee range, 120), the dragon's tail's 250, a shove's (VR_EnemyShove_Hit's).
float(entity m) VR_MeleeRule_Reach =
{
    if(m.classname == "monster_dragon")
    {
        return 250;
    }
    if(m.th_melee == tbaby_jump1)
    {
        return 120;
    }
    if(m.th_vrshove && !m.th_melee)
    {
        return VR_EnemyShove_Reach(m) + 16;
    }
    if(m.vr_ag)
    {
        // An armed humanoid's: its weapon's real reach, grown as its blows' (vr_armedgrunt.qc VR_AG_BlowBase).
        entity os = self;
        self = m;
        float r = cvar_hget(cvarh_vr_enemy_melee_arc) ? VR_MeleeArc_Reach(VR_AG_BlowBase(m, m.vr_ag_prop)) :
                  VR_AG_BlowBase(m, m.vr_ag_prop);
        self = os;
        return r;
    }
    return m.vr_melee_reach;
};

// Show Melee Arcs: an armed humanoid's swing drawn as the player's sweeps are (Show Melee Blades' look and lifetime:
// vr_meleeshow.cpp addFoe). From VR_AGP_Pose, each frame it is posed while the option is on: each melee weapon it swings
// (a swing or a saw, after a player) sends its blade's line as drawn (VR_AG_WeaponLine: butt to tip), yellow in its
// blow's swing-through, white in its wind-up; VR_MeleeArc_ShowLanded marks its blow landed (red, a dot at the tip).
// Its punches and gun butts have no blade: their floor arc alone (VR_MeleeArc_ShowFrame).
float vr_meleearc_swing_sent; // the frames sent (the test, vr_meleerules_test.qc 216)
float vr_meleearc_swing_fast; // ... in a blow's swing-through
void(entity g, entity w) VR_MeleeArc_ShowSwing =
{
    if(!w || VR_AG_Class(w.weapon) != VR_AG_CLASS_MELEE)
    {
        return;
    }
    entity pair = w == g.vr_ag_prop ? g.vr_ag_prop2 : g.vr_ag_prop;
    if(pair && VR_AG_Class(pair.weapon) == VR_AG_CLASS_MELEE)
    {
        // (two blades held: the hand that swings, as the directional parry reads it: VR_AGP_RightSwings)
        if(w.vr_ag_left ? g.vr_ag_act_hand == 0 : g.vr_ag_act_hand == 1)
        {
            return;
        }
    }
    VR_AGP_Times(g);
    float s = time - g.vr_ag_act_at;
    VR_AG_WeaponLine(w);
    float fast = s >= vr_agp_tw && s < vr_agp_tw + vr_agp_ts;
    meleeshowfoe(w, 0, w.origin, vr_ag_wb, fast);
    meleeshowfoe(w, 1, vr_ag_wa, '0 0 0', 0);
    vr_meleearc_swing_sent = vr_meleearc_swing_sent + 1;
    vr_meleearc_swing_fast = vr_meleearc_swing_fast + fast;
};
void(entity g) VR_MeleeArc_ShowSwings =
{
    if((g.vr_ag_act != VR_AGP_SWING && g.vr_ag_act != VR_AGP_SAW) || g.health <= 0 || !(g.enemy.flags & FL_CLIENT))
    {
        return;
    }
    VR_MeleeArc_ShowSwing(g, g.vr_ag_prop);
    VR_MeleeArc_ShowSwing(g, g.vr_ag_prop2);
};
void(entity p) VR_MeleeArc_ShowLanded =
{
    VR_AG_WeaponLine(p);
    meleeshowfoe(p, 6, vr_ag_wb, '0 0 0', -1);
};
