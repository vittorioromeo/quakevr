//
//
//
// ----------------------------------------------------------------------------
// Armed humanoids' AI behaviour (Combat > Armed Grunts > AI Behaviour; ROUND22.md, "Armed humanoids' AI behaviour")
// ----------------------------------------------------------------------------
//
// Knobs on the armed grunts' and enforcers' decisions (vr_armedgrunt.qc, vr_armedrepl.qc's own guns), each 1 today's
// behaviour, and the skill's factor on them (Difficulty Impact, vr_agrunt_ai_skill): a knob's value at a skill is its
// setting times 1 + impact * (the skill's step - 1), the step 1 at Normal, so Normal is always as set (and at its
// defaults exactly as before: every factor exactly 1, no random number drawn more or less). Quake's own skill rules for
// every monster stay as they were (Nightmare: no pause between attacks, a refire).
//
// - Aggressiveness (vr_agrunt_ai_aggression): their chance to attack when they could (VR_AG_CheckAttack's, a replacement's
//   own gun's) times it; their best and furthest fighting distances times 2 / (1 + it) (nearer, pressing you).
// - Accuracy (vr_agrunt_ai_accuracy): their aim error (vr_agrunt_aim_error) and their guns' spread (FireBullets', AD's
//   pellets: vr_fire_spread_extra) divided by it; an enforcer's own laser (no error of its own) gets one when under 1.
// - Reaction Time (vr_agrunt_ai_reaction): seconds from first seeing you to their first attack (HuntTarget's 1).
// - Retreat Readiness (vr_agrunt_ai_retreat): the distance a gun's holder backs off from (its least) times it, and the
//   health share below which it turns warier (0.35) times it; 0: it never backs off (its own blast's still).
// - Fire Rate (vr_agrunt_ai_firerate): their pause after an attack divided by it.
// - The parry's chance and reaction by skill (vr_armedgrunt_parry.qc) by the same impact.
//
// The skill's steps (impact 1):     Easy   Normal  Hard   Nightmare
//   Aggressiveness                  0.85   1       1.2    1.4
//   Accuracy                        0.8    1       1.25   1.5
//   Reaction Time                   1.3    1       0.7    0       (Nightmare: none, as id's)
//   Retreat Readiness               0.85   1       1.15   1.3
//   Fire Rate                       0.9    1       1.15   1.15    (Nightmare: no pauses anyway, id's)

// The skill's factor for a knob: `easy`, `hard`, `nightmare` its steps (Normal 1), by vr_agrunt_ai_skill.
float(float easy, float hard, float nightmare) VR_AGAI_Skill =
{
    float m = skill < 1 ? easy : skill < 2 ? 1 : skill < 3 ? hard : nightmare;
    return max(0, 1 + cvar_hget(cvarh_vr_agrunt_ai_skill) * (m - 1));
};

float() VR_AGAI_Aggression = { return max(0, cvar_hget(cvarh_vr_agrunt_ai_aggression)) * VR_AGAI_Skill(0.85, 1.2, 1.4); };
float() VR_AGAI_Accuracy = { return max(0.05, max(0, cvar_hget(cvarh_vr_agrunt_ai_accuracy)) * VR_AGAI_Skill(0.8, 1.25, 1.5)); };
float() VR_AGAI_Reaction = { return max(0, cvar_hget(cvarh_vr_agrunt_ai_reaction)) * VR_AGAI_Skill(1.3, 0.7, 0); };
float() VR_AGAI_Retreat = { return max(0, cvar_hget(cvarh_vr_agrunt_ai_retreat)) * VR_AGAI_Skill(0.85, 1.15, 1.3); };
float() VR_AGAI_FireRate = { return max(0.05, max(0, cvar_hget(cvarh_vr_agrunt_ai_firerate)) * VR_AGAI_Skill(0.9, 1.15, 1.15)); };

// The fighting distances' press by aggressiveness (1 at 1; nearer as it rises).
float() VR_AGAI_Press = { return 2 / (1 + VR_AGAI_Aggression()); };

// A pause after an attack (SUB_AttackFinished's), by the fire rate.
void(float normal) VR_AGAI_AttackFinished = { SUB_AttackFinished(normal / VR_AGAI_FireRate()); };

// The guns' spread multiplier's extra (vr_fire_spread_extra: 1 + it times the spread) with the accuracy on `extra`.
float(float extra) VR_AGAI_SpreadExtra =
{
    float acc = VR_AGAI_Accuracy();
    return acc == 1 ? extra : (1 + extra) / acc - 1;
};

// HuntTarget (ai.qc): an armed humanoid first seeing its target waits its reaction time before its first attack
// (id's SUB_AttackFinished(1) there; Nightmare's none: a pending pause kept unless the reaction asks for one).
void() VR_AGAI_React =
{
    float r = VR_AGAI_Reaction();
    if(skill != 3)
    {
        self.attack_finished = time + r;
    }
    else if(r > 0)
    {
        self.attack_finished = max(self.attack_finished, time + r);
    }
    vr_agai_reacts = vr_agai_reacts + 1;
};

// An enforcer's own laser aimed along `dir` from `org`: off by its aim error times how far under 1 its accuracy is.
vector(vector dir) VR_AGAI_LaserError =
{
    float acc = VR_AGAI_Accuracy();
    float err = cvar_hget(cvarh_vr_agrunt_aim_error) * (1 / acc - 1);
    if(err <= 0)
    {
        return dir;
    }
    vector a = vectoangles(dir);
    a_x = -a_x + crandom() * err;
    a_y = a_y + crandom() * err;
    makevectors(a);
    return v_forward * vlen(dir);
};
