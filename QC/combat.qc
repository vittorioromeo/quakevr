// combat.qc

float IsExplosionDamage;

// VR: what kind of blow the next T_Damage is, for the firing range's training dummy (vr_dummy.qc),
// which reports every hit. Attacks that their inflictor alone does not tell apart set these just
// before their T_Damage (VR_SetHitKind) and clear them after (VR_ClearHitKind); the rest (missiles,
// thrown things, deflected projectiles) the dummy works out from the inflictor.
float QVR_HITKIND_NONE     = 0; // worked out from the inflictor
float QVR_HITKIND_MELEE    = 1; // a swing of a hand (vr_hithand): fist, weapon or a gun used as a club
float QVR_HITKIND_HEADBUTT = 2;
float QVR_HITKIND_BASH     = 3; // a weapon's guard driven forward (vr_bash)
float QVR_HITKIND_SHOVE    = 4; // both empty hands driven forward (vr_bash)
float QVR_HITKIND_SHOT     = 5; // a hitscan weapon (vr_hithand's, or vr_hitweapon): shotguns, lightning
float QVR_HITKIND_SPLASH   = 6; // an explosion's (or a discharge's) radius damage

float vr_hitkind;     // QVR_HITKIND_*
float vr_hithand;     // the hand that struck or fired (-1: unknown)
string vr_hitweapon;  // what fired, when not the weapon in vr_hithand ("": that one)
vector vr_hitpos;     // where it struck ('0 0 0': unknown)
vector vr_hitdir;     // and going which way

// A shotgun blast's pellets on its target (ApplyMultiDamage), by where they struck.
float vr_hitpellets;
float vr_hitpellets_head;
float vr_hitpellets_limb;
float vr_hitpellets_leg;

// PositionalDamage's last answer (1 head, 2 extremity, 3 legs, 0 body), for which target, when.
float vr_hitpart;
entity vr_hitpart_target;
float vr_hitpart_time;
vector vr_hitpart_pos;

void(float xKind, float xHand, vector xPos, vector xDir) VR_SetHitKind =
{
    vr_hitkind = xKind;
    vr_hithand = xHand;
    vr_hitpos = xPos;
    vr_hitdir = xDir;
};

void() VR_ClearHitKind =
{
    vr_hitkind = QVR_HITKIND_NONE;
    vr_hithand = -1;
    vr_hitweapon = "";
    vr_hitpos = '0 0 0';
    vr_hitdir = '0 0 0';
};

//============================================================================

/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
float(entity targ, entity inflictor) CanDamage =
{
// bmodels need special checking because their origin is 0,0,0
    if(targ.movetype == MOVETYPE_PUSH)
    {
        traceline(inflictor.origin, 0.5 * (targ.absmin + targ.absmax), TRUE, self);
        if(trace_fraction == 1)
            return TRUE;
        if(trace_ent == targ)
            return TRUE;
        return FALSE;
    }

    traceline(inflictor.origin, targ.origin, TRUE, self);
    if(trace_fraction == 1)
        return TRUE;
    traceline(inflictor.origin, targ.origin + '15 15 0', TRUE, self);
    if(trace_fraction == 1)
        return TRUE;
    traceline(inflictor.origin, targ.origin + '-15 -15 0', TRUE, self);
    if(trace_fraction == 1)
        return TRUE;
    traceline(inflictor.origin, targ.origin + '-15 15 0', TRUE, self);
    if(trace_fraction == 1)
        return TRUE;
    traceline(inflictor.origin, targ.origin + '15 -15 0', TRUE, self);
    if(trace_fraction == 1)
        return TRUE;

    return FALSE;
};


/*
============
Killed
============
*/
void(entity targ, entity attacker) Killed =
{
    entity oself;

    oself = self;
    self = targ;

    if(self.health < -99)
    {
        self.health = -99;        // don't let sbar look bad if a player
    }

//MED
    if(self.charmed)
    {
        self.effects = self.effects - (self.effects & EF_DIMLIGHT);
    }

    if(self.movetype == MOVETYPE_PUSH || self.movetype == MOVETYPE_NONE)
    {    // doors, triggers, etc
        self.th_die();
        self = oself;
        return;
    }

    self.enemy = attacker;

// bump the monster counter
    if(self.flags & FL_MONSTER)
    {
        killed_monsters = killed_monsters + 1;
        WriteByte(MSG_ALL, SVC_KILLEDMONSTER);
    }

// PGM - guarantee gib if killed by buzzsaw.
    if(attacker.classname == "buzzsaw" ||
        attacker.classname == "Vengeance" ||
        attacker.classname == "pendulum")
        self.health = -99;

// PGM - gravity fix!
    self.gravity = 1.0;
    ClientObituary(self, attacker);

    self.takedamage = DAMAGE_NO;
    self.touch = SUB_Null;

    monster_death_use();
    self.th_die();

    // VR: a head thrown by the death code (the monster itself) stays grabbable, whatever the
    // death code did after (a soldier's drop makes it SOLID_NOT).
    if(self.vr_gib)
    {
        self.solid = SOLID_NOT_BUT_TOUCHABLE;
    }

    self = oself;
};

float() getVRMeleeBloodlust =
{
    return cvar_hget(cvarh_vr_melee_bloodlust);
}

/*
============
T_Damage

The damage is coming from inflictor, but get mad at attacker
This should be the only function that ever reduces health.
============
*/
// VR: pushes `e` along `dir` (made horizontal) at `speed` units/s, lifted by `up`: players and
// walking monsters are thrown (their velocity), flying and swimming monsters stepped aside. Monsters
// bigger than a grunt move less (by their size), smaller ones more.
void(entity e, vector dir, float speed, float up) VR_Push =
{
    speed = speed * cvar_hget(cvarh_vr_push);
    up = up * cvar_hget(cvarh_vr_push);
    dir_z = 0;
    dir = normalize(dir);
    if(speed <= 0 || dir == '0 0 0' || e.solid == SOLID_BSP)
    {
        return;
    }

    if(e.flags & FL_MONSTER)
    {
        float mass = max(0.6, vlen(e.size) / vlen('32 32 56'));
        speed = speed / mass;
        up = up / mass;
        if(e.flags & (FL_FLY | FL_SWIM))
        {
            entity oldSelf = self;
            self = e;
            walkmove(vectoyaw(dir), speed * 0.08);
            self = oldSelf;
            return;
        }
        e.velocity = dir * speed + '0 0 1' * up;
        e.flags = e.flags - (e.flags & FL_ONGROUND);
        return;
    }

    if(e.movetype == MOVETYPE_WALK)
    {
        e.velocity = e.velocity + dir * speed + '0 0 1' * up;
        e.flags = e.flags - (e.flags & FL_ONGROUND);
    }
};

//MED 01/10/97 added empathyused variable
float empathyused;
float(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) T_DamageImpl =
{
    vector    dir;
    entity    oldself;
    float    save;
    float    take;

    if(!targ.takedamage)
    {
        return FALSE;
    }

    if(targ.classname == "trigger_explosion")
    {
        if(IsExplosionDamage != 1)
        {
            return FALSE;
        }
    }

    IsExplosionDamage = 0;

    if(VR_ItemUtil_EntHasItem(targ, IID_SHIELD))
    {
        // Allow some damage to pass through.
        damage = shield_hit(targ, inflictor, attacker, damage);

        if(damage == 0)
        {
            return FALSE;
        }
    }

    if(discharged && targ.wetsuit_finished)
    {
        return FALSE;
    }

    //MED
    // moved damage_attacker down a bit

    // HONEY
    // cg: check if this needs a specific damage type
    if((targ.flags & FL_SPECIFICDAMAGE) && !(attacker.damage_weapon & targ.items)) {
        return FALSE;
    }

    // check for quad damage powerup on the attacker
    if(attacker.super_damage_finished > time)
    {
        damage = damage * 4;
    }

    // VR: the Gameplay menu's damage multipliers.
    if(targ == attacker)
    {
        if(targ.flags & FL_CLIENT)
        {
            damage = damage * cvar_hget(cvarh_vr_damage_self);
        }
    }
    else if((targ.flags & FL_CLIENT) && !(attacker.flags & FL_CLIENT))
    {
        damage = damage * cvar_hget(cvarh_vr_damage_to_player);
    }
    else if((attacker.flags & FL_CLIENT) && (targ.flags & FL_MONSTER))
    {
        damage = damage * cvar_hget(cvarh_vr_damage_to_enemies);
    }

    // rune power up (Black Magic)
    if(deathmatch)
    {
        damage = RuneApplyBlack(damage, attacker);
    }

    //MED
    //check for empathy shields
    if(VR_ItemUtil_EntHasItem(targ, IID_EMPATHY_SHIELDS) &&
       !VR_ItemUtil_EntHasItem(inflictor, IID_EMPATHY_SHIELDS) &&
       (targ != attacker))
    {
        empathyused = 1;
        damage = damage/2;
        T_DamageImpl (attacker,targ,targ,damage,isVRMelee);
        empathyused = 0;
    }

    //MED
    // used by buttons and triggers to set activator for target firing
    damage_attacker = attacker;

    //MED
    // used to keep track of what hit us
    damage_inflictor = inflictor;

// save damage based on the target's armor level

    //ZOID--
    if(TeamArmorDam(targ, inflictor, attacker, damage))
    {
        save = ceil(targ.armortype * damage);
    }
    else
    {
        save = 0;
    }
    //--ZOID

    if(save >= targ.armorvalue)
    {
        save = targ.armorvalue;
        targ.armortype = 0;    // lost all armor

        VR_ItemUtil_EntDelItem(targ, IID_ARMOR1);
        VR_ItemUtil_EntDelItem(targ, IID_ARMOR2);
        VR_ItemUtil_EntDelItem(targ, IID_ARMOR3);
    }

    targ.armorvalue = targ.armorvalue - save;
    take = ceil(damage-save);

    // add to the damage total for clients, which will be sent as a single
    // message at the end of the frame
    // FIXME: remove after combining shotgun blasts?
    if(targ.flags & FL_CLIENT)
    {
        targ.dmg_take = targ.dmg_take + take;
        targ.dmg_save = targ.dmg_save + save;
        targ.dmg_inflictor = inflictor;
    }

    // figure momentum add
    if((inflictor != world) && (targ.movetype == MOVETYPE_WALK))
    {
        dir = targ.origin - (inflictor.absmin + inflictor.absmax) * 0.5;
        dir = normalize(dir);
        targ.velocity = targ.velocity + dir*damage*8;
    }

    // check for godmode or invincibility
    if(targ.flags & FL_GODMODE)
    {
        return FALSE;
    }

    if(targ.invincible_finished >= time)
    {
        if(targ.invincible_sound < time)
        {
            sound(targ, CHAN_ITEM, "items/protect3.wav", 1, ATTN_NORM);
            targ.invincible_sound = time + 2;
        }

        return TRUE;
    }

    //MED
    if(VR_ItemUtil_EntHasItem(targ, IID_EMPATHY_SHIELDS))
    {
        if(self.empathy_sound < time)
        {
            sound(targ, CHAN_ITEM, "hipitems/empathy2.wav", 1, ATTN_NORM);
            self.empathy_sound = time + 0.5;
        }
    }

    // team play damage avoidance
    //ZOID--
    if(!TeamHealthDam(targ, inflictor, attacker, damage))
    {
        return TRUE; // same team
    }
    //--ZOID

    if(targ.classname == "player")
    {
        SpawnBloodSplash(targ.origin, take / 2.0);
        SpawnBloodSplash(targ.handpos, take / 2.0);
        SpawnBloodSplash(targ.offhandpos, take / 2.0);

        if(targ.ishuman)
        {
            // VR: felt more in the hand on the side the blow came from, and more the harder it is.
            float amp = min(1, 0.35 + take / 40);
            float dur = min(0.5, 0.12 + take / 60);
            float side = 0;
            if(inflictor != world && inflictor != targ)
            {
                vector oldForward = v_forward;
                vector oldRight = v_right;
                vector oldUp = v_up;
                makevectors(targ.v_angle);
                side = normalize((inflictor.absmin + inflictor.absmax) * 0.5 - targ.origin) * v_right; // 1: right
                v_forward = oldForward;
                v_right = oldRight;
                v_up = oldUp;
            }
            entity oldHapticSelf = self;
            self = targ;
            haptic(cVR_MainHand, 0.0, dur, 30, amp * (0.4 + 0.3 * (1 + side)));
            haptic(cVR_OffHand, 0.0, dur, 30, amp * (0.4 + 0.3 * (1 - side)));
            self = oldHapticSelf;
        }
    }

    // rune power up (Earth Magic)
    if(deathmatch)
    {
        take = RuneApplyEarth(take, targ);
    }

    // VR: the firing range's training dummy reports the hit and takes no harm (vr_dummy.qc).
    if(targ.classname == "vr_dummy")
    {
        VR_Dummy_Hit(targ, inflictor, attacker, damage, isVRMelee);
        return TRUE;
    }

    // do the damage
    targ.health = targ.health - take;

    if(targ.health <= 0)
    {
        if(isVRMelee)
        {
            // VR melee takedown
            if(getVRMeleeBloodlust() == QVR_MELEEBLOODLUST_ENABLED)
            {
                float bloodlustAmount = ceil(1 + take / 12) *
                    cvar_hget(cvarh_vr_melee_bloodlust_mult) * 0.75;

                T_Heal(attacker, bloodlustAmount, 1);
            }
        }

        Killed (targ, attacker);
        return TRUE;
    }
    else
    {
        if(isVRMelee)
        {
            // VR melee damage
            if(getVRMeleeBloodlust() == QVR_MELEEBLOODLUST_ENABLED)
            {
                T_Heal(attacker, ceil(take / 12), 1);
            }
        }
    }

    // react to the damage
    oldself = self;
    self = targ;

    //MED 10/17/96 added charmed stuff
    if((self.flags & FL_MONSTER) && attacker != world)
    {
        // get mad unless of the same class (except for soldiers)
        if(self != attacker && attacker != self.enemy && (self.charmer != attacker))
        {
            if((self.classname != attacker.classname)
            || (self.classname == "monster_army") || (self.classname == "monster_armagon"))
            {
                if(self.classname != "monster_dragon")
                {
                    if(self.enemy.classname == "player")
                    {
                        self.oldenemy = self.enemy;
                    }

                    self.enemy = attacker;
                    FoundTarget();
                }
            }
        }
    }

    if(self.th_pain)
    {
        self.th_pain (attacker, take);
    // nightmare mode monsters don't go into pain frames often
        if(skill == 3)
            self.pain_finished = time + 5;
    }

    self = oldself;
    return TRUE;
};

// ----------------------------------------------------------------------------
// Parrying (vr_parry): a monster's melee blow on a VR player who holds a weapon sideways in front
// of them, between them and the monster, is partly blocked (vr_parry_reduction): a clang, sparks,
// the hand knocked back. Held in one hand, the weapon may be knocked out of it
// (vr_parry_drop_chance); held in two, never.

// Monsters that strike in melee (a blow comes from the monster itself, up close).
float(entity e) VR_Parry_MeleeMonster =
{
    string c = e.classname;
    return c == "monster_knight" || c == "monster_hell_knight" || c == "monster_ogre" || c == "monster_demon1" ||
           c == "monster_dog" || c == "monster_shambler" || c == "monster_fish" || c == "monster_gremlin" ||
           c == "monster_mummy";
};

// Whether `p`'s `hand` holds a weapon across a blow coming from `toAttacker` (horizontal, unit):
// square to it within vr_parry_angle, ahead of the body towards the attacker, between the hips
// and above the head.
float(entity p, float hand, vector toAttacker) VR_Parry_Blocks =
{
    if(VRIsHandEmpty(p, hand))
    {
        return FALSE;
    }

    makeforward(VRGetEntHandRot(p, hand));
    if(fabs(v_forward * toAttacker) > sin(cvar_hget(cvarh_vr_parry_angle) * 0.0174533))
    {
        return FALSE;
    }

    vector rel = VRGetEntHandPos(p, hand) - p.origin;
    float ahead = rel * toAttacker;
    return ahead > 2 && ahead < 44 && rel_z > -8 && rel_z < 50;
};

// Whether `p` holds both empty hands crossed in front (an X, wrists together), facing a blow from
// `toAttacker`: each hand on the other's side of the body, close to the other, ahead of the body,
// between the hips and above the head (vr_parry_unarmed).
float(entity p, vector toAttacker) VR_Parry_ArmsCrossed =
{
    if(!cvar_hget(cvarh_vr_parry_unarmed) || !VRIsHandEmpty(p, cVR_MainHand) || !VRIsHandEmpty(p, cVR_OffHand))
    {
        return FALSE;
    }
    vector m = VRGetEntHandPos(p, cVR_MainHand) - p.origin;
    vector o = VRGetEntHandPos(p, cVR_OffHand) - p.origin;
    if(vlen(m - o) > 16)
    {
        return FALSE;
    }
    for(float i = 0; i < 2; i++)
    {
        vector rel = i == 0 ? m : o;
        float ahead = rel * toAttacker;
        if(ahead < 2 || ahead > 40 || rel_z < -4 || rel_z > 50)
        {
            return FALSE;
        }
    }
    // Crossed: the main hand on the off hand's side (the left for the right-handed).
    vector right;
    right_x = toAttacker_y;
    right_y = -toAttacker_x;
    right_z = 0;
    float mainRight = cvar("vr_lefthanded") ? -1 : 1;
    return (m * right - o * right) * mainRight < -1;
};

float(entity targ, entity inflictor, entity attacker, float damage) VR_Parry =
{
    if(inflictor != attacker || !(attacker.flags & FL_MONSTER) || damage <= 0 || !cvar_hget(cvarh_vr_parry) ||
       targ.classname != "player" || !VRHandsTracked(targ) || !VR_Parry_MeleeMonster(attacker))
    {
        return damage;
    }

    vector to = attacker.origin - targ.origin;
    to_z = 0;
    // Up close only (a shambler's lightning also comes from the shambler itself).
    if(vlen(to) > 150 || (attacker.classname == "monster_shambler" && vlen(to) > 110))
    {
        return damage;
    }
    to = normalize(to);

    // The globals the monster's attack code may still use after the damage.
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;

    float hand = -1;
    if(VR_Parry_Blocks(targ, cVR_MainHand, to))
    {
        hand = cVR_MainHand;
    }
    else if(VR_Parry_Blocks(targ, cVR_OffHand, to))
    {
        hand = cVR_OffHand;
    }

    // Unarmed: arms crossed in front take the blow on the forearms (both hands knocked, less taken
    // off than with a weapon).
    if(hand < 0 && VR_Parry_ArmsCrossed(targ, to))
    {
        entity oldSelfX = self;
        self = targ;
        sound(self, CHAN_AUTO, "fisthit.wav", 1, ATTN_NORM);
        sound(self, CHAN_AUTO, "demon/dland2.wav", 0.5, ATTN_NORM);
        haptic(cVR_MainHand, 0.0, 0.3, 60, 1.0);
        haptic(cVR_OffHand, 0.0, 0.3, 60, 1.0);
        handimpact(cVR_MainHand, 4, to * -1);
        handimpact(cVR_OffHand, 4, to * -1);
        particle2(VRGetHandPos(cVR_MainHand) + to * 4, (to * -1 + '0 0 0.5') * 60, QVR_PARTICLE_PRESET_BULLETPUFF, 12);
        self = oldSelfX;

        VR_Push(attacker, to, 260 * cvar_hget(cvarh_vr_parry_push_enemy), 90 * cvar_hget(cvarh_vr_parry_push_enemy));
        VR_Push(targ, to * -1, 200 * cvar_hget(cvarh_vr_parry_push_player), 40 * cvar_hget(cvarh_vr_parry_push_player));
        dprint("parry: "); dprint(attacker.classname); dprint(" with crossed arms\n");
        damage = damage * (1 - min(1, max(0, cvar_hget(cvarh_vr_parry_unarmed_reduction))));
    }

    if(hand >= 0)
    {
        entity oldSelf = self;
        self = targ;

        vector at = VRGetHandPos(hand) + to * 4;
        sound(self, CHAN_AUTO, "player/axhit2.wav", 1, ATTN_NORM);
        sound(self, CHAN_AUTO, "weapons/tink1.wav", 0.6, ATTN_NORM);
        particle2(at, (to * -1 + '0 0 0.5') * 80, QVR_PARTICLE_PRESET_SPARKS, 24);
        haptic(hand, 0.0, 0.3, 90, 1.0);
        handimpact(hand, 5, to * -1);

        float twoHanded = VRGet2HAiming(self);
        if(twoHanded)
        {
            float other = VRGetOtherHand(hand);
            haptic(other, 0.0, 0.25, 90, 0.8);
            handimpact(other, 3, to * -1);
        }
        else if(random() < cvar_hget(cvarh_vr_parry_drop_chance))
        {
            DropWeaponInHand(self, hand, VRGetEntWeapon(self, hand), VRGetEntWeaponFlags(self, hand),
                VRGetEntWeaponClip(self, hand));
            sprint(self, "The blow knocks the weapon out of your hand!\n");
        }

        self = oldSelf;

        // Both are pushed apart (vr_parry_push_enemy, vr_parry_push_player).
        VR_Push(attacker, to, 320 * cvar_hget(cvarh_vr_parry_push_enemy), 120 * cvar_hget(cvarh_vr_parry_push_enemy));
        VR_Push(targ, to * -1, 180 * cvar_hget(cvarh_vr_parry_push_player), 40 * cvar_hget(cvarh_vr_parry_push_player));

        dprint("parry: "); dprint(attacker.classname); dprint(" with hand "); dprint(ftos(hand));
        dprint(twoHanded ? " (two hands)\n" : "\n");
        damage = damage * (1 - min(1, max(0, cvar_hget(cvarh_vr_parry_reduction))));
    }

    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return damage;
};

float(entity targ, entity inflictor, entity attacker, float damage) T_Damage =
{
    float parried = VR_Parry(targ, inflictor, attacker, damage);
    float r = T_DamageImpl(targ, inflictor, attacker, parried, 0);

    // VR: hit reactions: a heavy hit shoves a monster (vr_hit_push), a killing blow throws the body
    // (vr_kill_push; an explosion's away from it).
    if(r && (targ.flags & FL_MONSTER) && (attacker.flags & FL_CLIENT))
    {
        vector from = inflictor == attacker ? attacker.origin : (inflictor.absmin + inflictor.absmax) * 0.5;
        float hitPush = cvar_hget(cvarh_vr_hit_push);
        if(targ.health <= 0)
        {
            float killPush = cvar_hget(cvarh_vr_kill_push);
            VR_Push(targ, targ.origin - from, min(700, damage * 14) * killPush, min(260, damage * 5) * killPush);
        }
        else if(damage >= 15)
        {
            VR_Push(targ, targ.origin - from, min(350, damage * 6) * hitPush, min(80, damage * 1.5) * hitPush);
        }
    }

    // VR: a monster's melee blow that was not parried throws the player back (vr_melee_push_player;
    // Quake's own push is weak).
    if(r && parried == damage && inflictor == attacker && (attacker.flags & FL_MONSTER) &&
       targ.classname == "player" && VR_Parry_MeleeMonster(attacker) && vlen(attacker.origin - targ.origin) < 150)
    {
        VR_Push(targ, targ.origin - attacker.origin, damage * 12 * cvar_hget(cvarh_vr_melee_push_player),
            damage * 3 * cvar_hget(cvarh_vr_melee_push_player));
    }
    return r;
};

float(entity targ, entity inflictor, entity attacker, float damage) T_Damage_VRMelee =
{
    float r = T_DamageImpl(targ, inflictor, attacker, damage, 1);

    // VR: melee blows push what they hit, dead or alive (vr_melee_push).
    if(targ != attacker && targ.takedamage)
    {
        VR_Push(targ, targ.origin - attacker.origin, min(600, damage * 10) * cvar_hget(cvarh_vr_melee_push),
            min(200, damage * 3) * cvar_hget(cvarh_vr_melee_push));
    }
    return r;
};

/*
============
T_RadiusDamage
============
*/
// VR: an explosion rumbles in the hands of the players near it (vr_explosion_rumble), fading with
// distance.
void(vector xOrigin, float xPower) VR_ExplosionRumble =
{
    if(!cvar_hget(cvarh_vr_explosion_rumble))
    {
        return;
    }
    float reach = 300 + xPower * 3;
    entity oldSelf = self;
    entity p = find(world, classname, "player");
    while(p)
    {
        float d = vlen(p.origin - xOrigin);
        if(p.ishuman && d < reach)
        {
            float a = 1 - d / reach;
            self = p;
            haptic(cVR_MainHand, 0.0, 0.2 + 0.3 * a, 45, a);
            haptic(cVR_OffHand, 0.0, 0.2 + 0.3 * a, 45, a);
        }
        p = find(p, classname, "player");
    }
    self = oldSelf;
};

void(entity inflictor, entity attacker, float damage, entity ignore) T_RadiusDamage =
{
    VR_ExplosionRumble(inflictor.origin, damage);

    float     points;
    entity    head;
    vector    org;

    IsExplosionDamage = 1;

    head = findradius(inflictor.origin, damage+40);

    while(head)
    {
        if(head != ignore)
        {
            if(head.takedamage)
            {
                org = head.origin + (head.mins + head.maxs)*0.5;
                points = 0.5*vlen(inflictor.origin - org);
                if(points < 0)
                    points = 0;
                points = damage - points;
                if(head == attacker)
                    points = points * 0.5;
                if(points > 0)
                {
                    if(CanDamage(head, inflictor))
                    {    // shambler takes half damage from all explosions
                        float oldHitKind = vr_hitkind;
                        vr_hitkind = QVR_HITKIND_SPLASH;
                        if(head.classname == "monster_shambler")
                            T_Damage(head, inflictor, attacker, points*0.5);
                        else
                            T_Damage(head, inflictor, attacker, points);
                        vr_hitkind = oldHitKind;
                    }
                }
            }
        }
        head = head.chain;
    }
};

/*
============
T_BeamDamage
============
*/
void(entity attacker, float damage) T_BeamDamage =
{
    float     points;
    entity    head;

    head = findradius(attacker.origin, damage+40);

    while(head)
    {
        if(head.takedamage)
        {
            points = 0.5*vlen(attacker.origin - head.origin);
            if(points < 0)
                points = 0;
            points = damage - points;
            if(head == attacker)
                points = points * 0.5;
            if(points > 0)
            {
                if(CanDamage(head, attacker))
                {
                    float oldHitKind = vr_hitkind;
                    vr_hitkind = QVR_HITKIND_SPLASH;
                    if(head.classname == "monster_shambler")
                        T_Damage(head, attacker, attacker, points*0.5);
                    else
                        T_Damage(head, attacker, attacker, points);
                    vr_hitkind = oldHitKind;
                }
            }
        }
        head = head.chain;
    }
};
