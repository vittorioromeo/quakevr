// vr_attrs_defs.qc -- enemy attributes (vr_attrs.qc; ROUND26.md, "elite"): the field, the table's bits and stats, and
// the hooks ai.qc, combat.qc and the armed humanoids' files call before vr_attrs.qc defines them.

.float vr_attrs; // its attributes' bits (VR_ATTR_*); "infected" is also its row in .vr_mg3_infected (what it rises as)

// The attributes (one bit each; a monster can carry any of them together: an elite that is also infected).
float VR_ATTR_INFECTED = 1; // Dawn of the Machine's infected: dying, it bursts and rises as its row's monster (no stats)
float VR_ATTR_ELITE = 2;    // much harder to fight: faster fire, reaction, aim, melee, steps; shrugs pains (Tunables > Elite)
float VR_ATTR_LAST = 2;     // the highest bit (the table's loops)

// The stats an attribute scales (each 1 without one).
float VR_ATTR_ST_FIRERATE = 0;   // its fire rate (VR_AGAI_FireRate: the pause after an attack divided by it)
float VR_ATTR_ST_REACTION = 1;   // its reaction time on first seeing you (VR_AGAI_Reaction)
float VR_ATTR_ST_ACCURACY = 2;   // its accuracy (VR_AGAI_Accuracy: its aim error and spread divided by it)
float VR_ATTR_ST_AGGRESSION = 3; // its aggressiveness (VR_AGAI_Aggression)
float VR_ATTR_ST_SPEED = 4;      // its steps (VR_AG_Speed)
float VR_ATTR_ST_MELEE = 5;      // its swings' and punches' speed (vr_ag_act_speed)
float VR_ATTR_ST_PAIN = 6;       // the share of its pains it takes (the rest shrugged off: T_Damage)
float VR_ATTR_ST_AIMLAG = 7;     // its aim's lag behind a moving target (VR_AG_AimPoint; 0 where you are now)
float VR_ATTR_ST_TURN = 8;       // its turning speed (yaw_speed, on gaining it)
float VR_ATTR_ST_HEALTH = 9;     // its health (on gaining it)

float VR_HUM_LOOK_ELITE = 8192; // .vr_hum_look's elite bit (the engine's humlook::eliteBit: the elite dye)

float(entity e) VR_Attrs;                       // its attributes' bits (infected from its row too)
float(entity e, float stat) VR_Attr_Mult;       // its attributes' factor on a stat (1 with none)
void(entity e, float bit, float on) VR_Attr_Set; // an attribute given or taken (its gains, its look)
float(entity e) VR_Attr_ShrugsPain;             // T_Damage: TRUE when it shrugs this pain off (no th_pain)
void(entity e) VR_Attr_Look;                    // its look's elite bit by its attributes (VR_HUM_Look calls it)
void(entity e) VR_Attr_Spawned;                 // VR_AM_Spawn's end: a map's bits taken up, the skill's roll
