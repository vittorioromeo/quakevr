// combat.qc

float IsExplosionDamage;

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
            haptic(cVR_OffHand, 0.0, 0.5, 30, 1.0);
            haptic(cVR_MainHand, 0.0, 0.5, 30, 1.0);
        }
    }

    // rune power up (Earth Magic)
    if(deathmatch)
    {
        take = RuneApplyEarth(take, targ);
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

    vector rot = p.handrot;
    if(hand == cVR_OffHand)
    {
        rot = p.offhandrot;
    }
    makeforward(rot);
    if(fabs(v_forward * toAttacker) > sin(cvar_hget(cvarh_vr_parry_angle) * 0.0174533))
    {
        return FALSE;
    }

    vector rel = VRGetEntHandPos(p, hand) - p.origin;
    float ahead = rel * toAttacker;
    return ahead > 2 && ahead < 44 && rel_z > -8 && rel_z < 50;
};

float(entity targ, entity inflictor, entity attacker, float damage) VR_Parry =
{
    if(!cvar_hget(cvarh_vr_parry) || damage <= 0 || targ.classname != "player" || !VRHandsTracked(targ) ||
       inflictor != attacker || !(attacker.flags & FL_MONSTER) || !VR_Parry_MeleeMonster(attacker))
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

    // VR: a monster's melee blow that was not parried throws the player back (vr_melee_push_player;
    // Quake's own push is weak).
    if(r && parried == damage && targ.classname == "player" && inflictor == attacker &&
       (attacker.flags & FL_MONSTER) && VR_Parry_MeleeMonster(attacker) && vlen(attacker.origin - targ.origin) < 150)
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
void(entity inflictor, entity attacker, float damage, entity ignore) T_RadiusDamage =
{
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
                        if(head.classname == "monster_shambler")
                            T_Damage(head, inflictor, attacker, points*0.5);
                        else
                            T_Damage(head, inflictor, attacker, points);
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
                    if(head.classname == "monster_shambler")
                        T_Damage(head, attacker, attacker, points*0.5);
                    else
                        T_Damage(head, attacker, attacker, points);
                }
            }
        }
        head = head.chain;
    }
};
