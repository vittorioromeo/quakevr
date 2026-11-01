#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_armedgrunt_aware_test.qc -- `vr_agrunt_test 200`: disarm attempts on armed grunts that don't know you're there
// (his voice notes 2026-11-01 18:03-18:46: a punch on an unaware grunt's weapon went through it, a shot weapon or a
// parried strike left it calm; disarming one of a dual wielder's weapons dropped both). In vrtesthall, you unseen
// (notarget, kept so: only the attempt itself may wake it) and kept alive, a fresh armed grunt 60 units ahead, standing
// idle (never told about you), for each case:
//   0 your off fist (closed) punched through its shotgun's drawn middle (the mock hand gliding, as test 19's): disarmed,
//     staggered, hunting you;
//   1 a shot on its shotgun, the roll lost (vr_agrunt_shotdisarm_chance and _chance_scale 0): still armed, hunting you;
//   2 a shot on its shotgun, the roll won (chance 1): shot out, staggered, hunting you;
//   3 a fist into its raised parry (held still, its parry up, vr_agrunt_strike_beat 0; it was after nobody): parried,
//     hunting you;
//   4 a fist on its sword's blade far from its hand (a blade contact: no disarm, no damage): still armed, hunting you;
//   5-8 a dual wielder (two swords, two nail pistols), your fist through one hand's grip, then (it calmed down again)
//     the other's: the struck weapon alone drops, the other stays held; 5 swords left then right, 6 right then left,
//     7 pistols left then right, 8 right then left;
//   9 two swords, floored gently (it keeps both), its left one struck out as it lies, its body knocked along the floor
//     by the blow: the other kept (the fall rule's impact is the ground's, not the blow's);
//   10 as 0, the punch straight at its gun (from you, through it and on into its body).
// Each check `agtest: PASS|FAIL aware <case>: ...`; ends on `agtest: test 200 over: P passed, F failed`.
// ----------------------------------------------------------------------------

float VR_AGAW_CASES = 11;
float vr_agaw_case;       // the case running
float vr_agaw_step;       // its step
float vr_agaw_until;      // the step's wait (a frame count's worth of game time) or its give-up time (safety net)
float vr_agaw_stag;       // the latest stagger end seen since the attempt (vr_parry_stagger_until)
float vr_agaw_round;      // dual: 0 the first weapon struck, 1 the second
entity vr_agaw_aim;       // dual: the weapon struck this round
entity vr_agaw_other;     // ... the other one
vector vr_agaw_way;       // the punch's way (case 0 across its gun, left to right; case 10 straight at it)
vector vr_agaw_home;      // the grunt's spawn spot (dual: put back there between rounds)
float vr_agaw_keep_chance;
float vr_agaw_keep_beat;
float vr_agaw_keep_scale;
float vr_agaw_keep_yank;
float vr_agaw_tries0;     // vr_ag_tries at the attempt (the shared disarm attempt event's count)
float vr_agaw_shots;      // case 2: blasts fired
float vr_agaw_gap;        // the punch's fist: its least gap to the struck weapon's or hand's hull (a miss's account)

string(float c) VR_AGAW_Name =
{
    if(c == 0) { return "punch on an unaware grunt's shotgun"; }
    if(c == 1) { return "a shot on its shotgun, roll lost, notarget"; }
    if(c == 2) { return "a shot on its shotgun, roll won, notarget"; }
    if(c == 3) { return "a strike into its raised parry, it after nobody"; }
    if(c == 4) { return "a blow on an unaware sword grunt's blade"; }
    if(c == 5) { return "dual swords, left then right"; }
    if(c == 6) { return "dual swords, right then left"; }
    if(c == 7) { return "dual nail pistols, left then right"; }
    if(c == 8) { return "dual nail pistols, right then left"; }
    if(c == 9) { return "dual swords, lying: the left one struck out"; }
    return "a straight punch at an unaware grunt's shotgun (on into its body)";
};

float(float c) VR_AGAW_Wid = { return (c == 4 || c == 5 || c == 6 || c == 9) ? WID_SWORD : ((c == 7 || c == 8) ? WID_NAILPISTOL : WID_SHOTGUN); };
float(float c) VR_AGAW_Dual = { return c >= 5 && c <= 9; };

string(entity g) VR_AGAW_Holds =
{
    return sprintf("%s, %s", g.vr_ag_prop ? g.vr_ag_prop.netname : "-", g.vr_ag_prop2 ? g.vr_ag_prop2.netname : "-");
};

// The off fist (closed) put 28 units before `at` along vr_agaw_way, then glided through it to 10 past in 0.2 s (a 5 m/s
// punch).
void(vector at) VR_AGAW_PunchFrom =
{
    entity p = vr_agt_player;
    makevectors('0 1 0' * p.angles_y);
    localcmd(sprintf("vr_mock_hand_glide 0\nvr_mock_hand_to off %s\nvr_mock_button off grip 1\n", VR_AGTP_Xyz(at - vr_agaw_way * 28)));
};
void(vector at) VR_AGAW_PunchThrough =
{
    entity p = vr_agt_player;
    makevectors('0 1 0' * p.angles_y);
    localcmd(sprintf("vr_mock_hand_glide 0.2\nvr_mock_hand_to off %s\n", VR_AGTP_Xyz(at + vr_agaw_way * 10)));
};

// Where the fist goes through `w`: a gun's drawn middle; a blade's grip (its hand: a blow on the blade away from it is a
// blade contact, never a disarm).
vector(entity g, entity w) VR_AGAW_Target =
{
    if(VR_AG_Class(w.weapon) == VR_AG_CLASS_MELEE)
    {
        VR_AGS_HandLine(g, w);
        return vr_ags_ha + (vr_ags_hb - vr_ags_ha) * 0.2;
    }
    VR_AG_WeaponLine(w);
    return (vr_ag_wa + vr_ag_wb) * 0.5;
};

// Calm again: after nobody, standing at its spot facing you, not staggered (dual: before its second round).
void(entity g) VR_AGAW_Calm =
{
    entity p = vr_agt_player;
    g.enemy = world;
    g.oldenemy = world;
    g.goalentity = world;
    g.vr_parry_stagger_until = 0;
    setorigin(g, vr_agaw_home);
    g.angles_y = vectoyaw(p.origin - g.origin);
    g.velocity = '0 0 0';
    entity was = self;
    self = g;
    self.th_stand();
    self = was;
};

void() VR_AGAW_End =
{
    entity p = vr_agt_player;
    localcmd("vr_mock_button off grip 0\nvr_mock_hand_glide 0\n");
    cvar_set("vr_agrunt_shotdisarm_chance", ftos(vr_agaw_keep_chance));
    cvar_set("vr_agrunt_shotdisarm_chance_scale", ftos(vr_agaw_keep_scale));
    localcmd(sprintf("tune vr_agrunt_strike_beat %g\n", vr_agaw_keep_beat)); // (a tunable: set by `tune`)
    cvar_set("vr_agrunt_disarm_yank", ftos(vr_agaw_keep_yank));
    VR_AGPT_Clear();
    vr_agt_grunt = world;
    p.enemy = world;
    p.flags = p.flags - (p.flags & (FL_GODMODE | FL_NOTARGET));
    p.vrbits0 = p.vrbits0 - (p.vrbits0 & QVR_VRBITS0_MAINHAND_GRABBING);
    vr_agaw_case = 0;
    vr_agaw_step = 0;
    cvar_set("vr_agrunt_test", "0");
};

// The case's grunt: idle, `wid` (two: a dual wielder), 60 ahead, facing you.
void(float c) VR_AGAW_Spawn =
{
    VR_AGPT_Clear();
    float wid = VR_AGAW_Wid(c);
    vr_agt_weapon2 = VR_AGAW_Dual(c) ? wid : 0;
    vr_agt_grunt = VR_AGT_Spawn(wid, 60);
    vr_agt_weapon2 = 0;
    vr_agaw_home = vr_agt_grunt.origin;
    vr_agaw_round = 0;
};

// Dual: your main fist's 6 m/s blow through vr_agaw_aim's grip or middle as the melee tracker has it (test 94's
// VR_AGST_Blow), then both weapons' thinks as the frame runs them (the first one's, the second one's: its slots swapped
// there and back, VR_AG_PropThink).
void(entity g) VR_AGAW_DualBlow =
{
    vector at = VR_AGAW_Target(g, vr_agaw_aim);
    VR_AG_WeaponLine(vr_agaw_aim);
    vector side = normalize(VR_Cross(normalize(vr_ag_wb - vr_ag_wa), '0 0 1'));
    float st = 6 * VRMetersToUnits() / 72;
    VR_AGST_Blow(at + side * st * 0.5, '0 0 0', at - side * st * 0.5, '0 0 0', 1, side * -1, 6, TRUE, VR_MTHING_FIST);
    entity first = g.vr_ag_prop;
    entity second = g.vr_ag_prop2;
    entity was = self;
    self = first;
    VR_AG_PropThink();
    if(second && (g.vr_ag_prop == second || g.vr_ag_prop2 == second)) // (still held: not removed by a drop)
    {
        self = second;
        VR_AG_PropThink();
    }
    self = was;
};

// The stagger seen since the attempt (it may be over by the check).
void(entity g) VR_AGAW_Watch =
{
    vr_agaw_stag = max(vr_agaw_stag, g.vr_parry_stagger_until);
};

void() VR_AGAW_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float c = vr_agaw_case;
    p.flags = p.flags | FL_GODMODE | FL_NOTARGET;
    if(vr_agaw_step == 0)
    {
        if(time < vr_agaw_until)
        {
            return; // (the last punch's fist on its way back; the last grunt gone a while)
        }
        if(g)
        {
            // The last case's grunt gone first, a while before the next comes: it woke (FoundTarget), and a monster
            // that just saw you wakes those near for 0.1 s (sight_entity) and shares you (vr_stealth.qc).
            VR_AGPT_Clear();
            vr_agt_grunt = world;
            vr_agaw_until = time + 0.3;
            return;
        }
        if(c == 0)
        {
            vr_agaw_keep_chance = cvar("vr_agrunt_shotdisarm_chance");
            vr_agaw_keep_scale = cvar("vr_agrunt_shotdisarm_chance_scale");
            vr_agaw_keep_beat = cvar("vr_agrunt_strike_beat");
            vr_agaw_keep_yank = cvar("vr_agrunt_disarm_yank");
            cvar_set("vr_agrunt_disarm_yank", "0"); // (no grab by the fist's own touch: a punch)
            VR_AGPT_EmptyHand(p);
            p.weapon2 = WID_FIST;
            cvar_set("vr_mock_hand_pull", "0");
            cvar_set("vr_mock_hand_pull_off", "0");
            cvar_set("vr_mock_hand_lift", "0");
        }
        if(c >= VR_AGAW_CASES)
        {
            VR_AGAW_End();
            return;
        }
        VR_AGAW_Spawn(c);
        vr_agaw_step = 1;
        vr_agaw_until = time + 0.5; // (armed and standing: its weapons placed, drawn)
        return;
    }
    if(vr_agaw_step == 1)
    {
        if(time < vr_agaw_until) { return; }
        VR_AGT_Check(sprintf("aware %g (%s): set up: armed (%s), after nobody (%s)", c, VR_AGAW_Name(c), VR_AGAW_Holds(g),
            g.enemy ? g.enemy.classname : "-"), g.vr_ag_prop != world && (!VR_AGAW_Dual(c) || vr_agaw_round == 1 || g.vr_ag_prop2 != world) &&
            g.enemy != p);
        vr_agaw_stag = 0;
        vr_agaw_tries0 = vr_ag_tries;
        vr_agaw_shots = 0;
        if(c == 1 || c == 2)
        {
            vr_agaw_shots = 1;
            cvar_set("vr_agrunt_shotdisarm_chance", c == 1 ? "0" : "1");
            cvar_set("vr_agrunt_shotdisarm_chance_scale", c == 1 ? "0" : ftos(vr_agaw_keep_scale));
            vr_agst_dist = 64;
            vr_agst_kind = 0;
            vr_agst_aim = 0;
            VR_AGST_Fire(g, g.vr_ag_prop); // (one shotgun blast on its gun's middle: vr_armedgrunt_shot_test.qc)
            vr_agaw_step = 4;
            vr_agaw_until = time + 0.3; // (its roll: the next frame)
            return;
        }
        if(c == 3 || c == 4)
        {
            // Your main fist's 6 m/s blow as the melee tracker has it (test 94's VR_AGST_Blow), on its weapon's far half.
            if(c == 3)
            {
                localcmd("tune vr_agrunt_strike_beat 0\n");
                VR_AGST_Hold(g, 2);
                g.enemy = world; // (its parry up, but after nobody: you came at it, VR_AGPar_Able's other way)
                p.enemy = g;
            }
            vr_agaw_step = 3;
            return;
        }
        // A punch with the off fist through its weapon (dual: the left one first in 5 and 7, the right one in 6 and 8).
        entity left = g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop : g.vr_ag_prop2;
        entity right = g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop2 : g.vr_ag_prop;
        vr_agaw_aim = g.vr_ag_prop;
        if(VR_AGAW_Dual(c))
        {
            float leftFirst = (c == 5 || c == 7 || c == 9);
            vr_agaw_aim = (leftFirst == (vr_agaw_round == 0)) ? left : right;
        }
        if(vr_agaw_round == 1)
        {
            vr_agaw_aim = g.vr_ag_prop; // (the one left: in its right hand now, VR_AG_Settle)
        }
        vr_agaw_other = vr_agaw_aim == g.vr_ag_prop ? g.vr_ag_prop2 : g.vr_ag_prop;
        if(c == 9 && vr_agaw_step == 1)
        {
            // Floored first (a gentle topple: it keeps both, VR_AG_FallDrop's rule), then the blow as it lies.
            VR_Knockdown_Start(g);
            makevectors('0 1 0' * g.angles_y);
            ragdollshovetopple(g, v_forward * -1, 1, FALSE, 40);
            vr_agaw_step = 7;
            vr_agaw_until = time + 1.5;
            return;
        }
        if(VR_AGAW_Dual(c))
        {
            VR_AGAW_DualBlow(g);
            vr_agaw_step = 5;
            vr_agaw_until = time; // (checked now)
            return;
        }
        makevectors('0 1 0' * p.angles_y);
        vr_agaw_way = c == 10 ? v_forward : v_right;
        VR_AGT_HandToHeight(p, vr_agaw_aim.origin_z);
        VR_AGAW_PunchFrom(VR_AGAW_Target(g, vr_agaw_aim));
        vr_agaw_step = 2;
        vr_agaw_until = time + 0.3; // (the fist there: the mock's hand at rest)
        return;
    }
    if(vr_agaw_step == 2)
    {
        if(time < vr_agaw_until) { return; }
        VR_AGAW_PunchThrough(VR_AGAW_Target(g, vr_agaw_aim));
        vr_agaw_gap = 999;
        vr_agaw_step = 5;
        vr_agaw_until = time + 0.5; // (safety net: the punch takes 0.2 s)
        return;
    }
    if(vr_agaw_step == 3)
    {
        entity w = g.vr_ag_prop;
        VR_AG_WeaponLine(w);
        vector along = normalize(vr_ag_wb - vr_ag_wa);
        vector side = normalize(VR_Cross(along, '0 0 1'));
        vector at = vr_ag_wa + (vr_ag_wb - vr_ag_wa) * (c == 4 ? 0.9 : 0.75);
        float st = 6 * VRMetersToUnits() / 72;
        VR_AGST_Blow(at + side * st * 0.5, '0 0 0', at - side * st * 0.5, '0 0 0', 1, side * -1, 6, TRUE, VR_MTHING_FIST);
        float pn = vr_agpar_n;
        float bn = vr_agbc_n;
        VR_AG_WeaponStruck(g, w);
        p.enemy = world;
        if(c == 3)
        {
            VR_AGT_Check(sprintf("aware 3: parried (%s; %s): hunting you (%s)", vr_ag_strike_why, VR_AGAW_Holds(g),
                g.enemy ? g.enemy.classname : "-"), vr_agpar_n > pn && g.vr_ag_prop == w && g.enemy == p);
        }
        else
        {
            VR_AGT_Check(sprintf("aware 4: a blade contact (%s): still armed, hunting you (%s)", vr_agbc_why,
                g.enemy ? g.enemy.classname : "-"), vr_agbc_n > bn && g.vr_ag_prop == w && g.enemy == p);
        }
        VR_AGT_Check(sprintf("aware %g: one disarm attempt counted (%g)", c, vr_ag_tries - vr_agaw_tries0),
            vr_ag_tries - vr_agaw_tries0 == 1);
        vr_agaw_case = vr_agaw_case + 1;
        vr_agaw_step = 0;
        return;
    }
    if(vr_agaw_step == 4)
    {
        VR_AGAW_Watch(g);
        if(time < vr_agaw_until) { return; }
        if(c == 2 && g.vr_ag_prop && vr_agaw_shots < 3)
        {
            // (its chance tops out at 0.95: another blast, its cooldown spent)
            g.vr_ag_prop.vr_ags_cool = 0;
            VR_AGST_Fire(g, g.vr_ag_prop);
            vr_agaw_shots = vr_agaw_shots + 1;
            vr_agaw_until = time + 0.3;
            return;
        }
        if(c == 1)
        {
            VR_AGT_Check(sprintf("aware 1: the roll lost: still armed (%s), hunting you (%s), attempts %g", VR_AGAW_Holds(g),
                g.enemy ? g.enemy.classname : "-", vr_ag_tries - vr_agaw_tries0), g.vr_ag_prop != world && g.enemy == p &&
                vr_ag_tries > vr_agaw_tries0);
        }
        else
        {
            VR_AGT_Check(sprintf("aware 2: shot out (%s), staggered (%.2f s on), hunting you (%s)", VR_AGAW_Holds(g),
                vr_agaw_stag - time, g.enemy ? g.enemy.classname : "-"), !g.vr_ag_prop && vr_agaw_stag > 0 && g.enemy == p);
        }
        cvar_set("vr_agrunt_shotdisarm_chance", ftos(vr_agaw_keep_chance));
        cvar_set("vr_agrunt_shotdisarm_chance_scale", ftos(vr_agaw_keep_scale));
        vr_agaw_case = vr_agaw_case + 1;
        vr_agaw_step = 0;
        return;
    }
    if(vr_agaw_step == 7)
    {
        if(time < vr_agaw_until) { return; }
        float u = VRMetersToUnits();
        VR_AGT_Check(sprintf("aware 9: floored gently, it holds both (%s; knockdown %g)", VR_AGAW_Holds(g), g.vr_knockdown),
            g.vr_knockdown == 1 && g.vr_ag_prop && g.vr_ag_prop2);
        // Its left weapon struck as it lies, the blow's knock flinging its body along the floor (a punch's on a ragdoll).
        VR_AGAW_DualBlow(g);
        makevectors('0 1 0' * g.angles_y);
        ragdollfling(g, (v_forward * -1 + '0 0 0.3') * 6 * u);
        vr_agaw_step = 8;
        vr_agaw_until = time + 1; // (its body come to rest: the fall rule's impacts measured)
        return;
    }
    if(vr_agaw_step == 8)
    {
        if(time < vr_agaw_until) { return; }
        float gone9 = g.vr_ag_prop != vr_agaw_aim && g.vr_ag_prop2 != vr_agaw_aim;
        float kept9 = vr_agaw_other != world && (g.vr_ag_prop == vr_agaw_other || g.vr_ag_prop2 == vr_agaw_other);
        VR_AGT_Check(sprintf("aware 9: lying, its left weapon struck out, its body knocked: the other kept (%s; strike: %s; "
            "hardest impact %.1f m/s)", VR_AGAW_Holds(g), vr_ag_strike_why, g.vr_ag_fall_hit / VRMetersToUnits()), gone9 && kept9);
        vr_agaw_case = vr_agaw_case + 1;
        vr_agaw_step = 0;
        return;
    }
    if(vr_agaw_step == 5)
    {
        // The punch on its way: the off hand's velocity the engine's own, the glide's (test 19's note).
        VR_AGAW_Watch(g);
        float gone = g.vr_ag_prop != vr_agaw_aim && g.vr_ag_prop2 != vr_agaw_aim;
        if(!gone)
        {
            vector hp = VRGetEntHandPos(p, cVR_OffHand);
            vr_agaw_gap = min(vr_agaw_gap, min(VR_AGS_HullGap(g, vr_agaw_aim, 1, hp, hp, 0),
                VR_AGS_HullGap(g, vr_agaw_aim, 2, hp, hp, 0)));
        }
        if(!gone && time < vr_agaw_until)
        {
            return;
        }
        // (the fist opened and taken back beside you, behind: away from where the next grunt stands)
        makevectors('0 1 0' * p.angles_y);
        localcmd(sprintf("vr_mock_button off grip 0\nvr_mock_hand_glide 0\nvr_mock_hand_to off %s\n",
            VR_AGTP_Xyz(p.origin - v_forward * 12 - v_right * 20 + '0 0 8')));
        if(!gone)
        {
            VR_AGT_Print(sprintf("aware %g: the fist's least gap to the struck weapon or its hand %.1f", c, vr_agaw_gap));
        }
        vr_agaw_until = time + 0.1;
        if(c == 0 || c == 10)
        {
            VR_AGT_Check(sprintf("aware %g: punched out of its hand (%s; strike: %s), staggered (%.2f s on), hunting you (%s)",
                c, VR_AGAW_Holds(g), vr_ag_strike_why, vr_agaw_stag - time, g.enemy ? g.enemy.classname : "-"),
                gone && vr_agaw_stag > 0 && g.enemy == p);
            vr_agaw_case = vr_agaw_case + 1;
            vr_agaw_step = 0;
            return;
        }
        float kept = g.vr_ag_prop == vr_agaw_other || g.vr_ag_prop2 == vr_agaw_other;
        VR_AGT_Check(sprintf("aware %g round %g: the struck %s weapon alone dropped: holds %s (strike: %s), hunting you %g",
            c, vr_agaw_round + 1, vr_agaw_aim.vr_ag_left ? "left" : "right", VR_AGAW_Holds(g), vr_ag_strike_why, g.enemy == p),
            gone && (vr_agaw_round == 1 || kept) && g.enemy == p);
        if(vr_agaw_round == 0 && g.vr_ag_prop)
        {
            vr_agaw_round = 1;
            VR_AGAW_Calm(g);
            vr_agaw_step = 1;
            vr_agaw_until = time + 0.5; // (its weapon placed in its other hand: drawn)
            return;
        }
        vr_agaw_case = vr_agaw_case + 1;
        vr_agaw_step = 0;
        return;
    }
};
#endif
