/*======================================================================
 COMBAT FUNCTIONS

 Returns true if the inflictor can directly damage the target.  
 Used for explosions and melee attacks.
======================================================================*/
void(entity targ, entity attacker) ClientObituary;

//-----------------------------------------------------------
float(entity targ, entity inflictor) CanDamage = {
    // Any entity marked with no damage is automatically excluded
    if (targ.takedamage == DAMAGE_NO) return FALSE;
    
    // bmodels need special checking because their origin is 0,0,0
    if (targ.movetype == MOVETYPE_PUSH) {
        traceline(inflictor.origin, 0.5 * (targ.absmin + targ.absmax), TRUE, inflictor);
        if (trace_fraction == 1) return TRUE;
        if (trace_ent == targ) return TRUE;
        return FALSE;
    }
    
    // Trace around the target to see if it can hit
    traceline(inflictor.origin, targ.origin, TRUE, inflictor);
    if (trace_fraction == 1) return TRUE;
    traceline(inflictor.origin, targ.origin + '15 15 0', TRUE, inflictor);
    if (trace_fraction == 1) return TRUE;
    traceline(inflictor.origin, targ.origin + '-15 -15 0', TRUE, inflictor);
    if (trace_fraction == 1) return TRUE;
    traceline(inflictor.origin, targ.origin + '-15 15 0', TRUE, inflictor);
    if (trace_fraction == 1) return TRUE;
    traceline(inflictor.origin, targ.origin + '15 -15 0', TRUE, inflictor);
    if (trace_fraction == 1) return TRUE;

    return FALSE;
};

//-----------------------------------------------------------
void(entity targ, entity attacker) Killed =
{
    local entity oself;

    // Switch self for death functions
    oself = self; self = targ;
    self.activate = attacker;        // Always keep track of what did damage

    // doors, triggers, etc
    if (self.movetype == MOVETYPE_PUSH || self.movetype == MOVETYPE_NONE ||
        self.classtype == CT_FUNCPUSHABLE) {    
        self.th_die ();
    }
    else {
        if (!self.deadflag && (self.flags & FL_MONSTER || self.flags & FL_CLIENT) ) {
            // Object is dying, stop re-triggering
            self.deadflag = DEAD_DYING;
            self.enemy = attacker;    // Enemy dealing final blow
            if (self.enemy.classtype == CT_WORLD) self.enemy = self;
        
            // bump the monster counter
            if (self.flags & FL_MONSTER) {
                if (!self.nomonstercount) {
                    killed_monsters = killed_monsters + 1;
                    WriteByte (MSG_ALL, SVC_KILLEDMONSTER);
                }
            }
        
            ClientObituary(self, attacker);
            self.touch = SUB_Null;
    
            if (self.flags & FL_MONSTER) monster_death_use();
            self.th_die ();
        }
        // Completely fuckup setup for the oldone boss with a weird target
        // field and mess of triggers around it! This elseif is just an
        // exception so it works, anyway no one fights the oldone!
        else if (!self.deadflag && self.classtype == CT_MONIDSHUB) {
            self.deadflag = DEAD_DEAD;
            killed_monsters = killed_monsters + 1;
            WriteByte (MSG_ALL, SVC_KILLEDMONSTER);
            self.th_die ();
        }
    }        
    // Restore self
    self = oself;
};

/*======================================================================
 T_Damage
  The damage is coming from inflictor, but get mad at attacker
    This should be the only function that ever reduces health.
    Parameters:
    targ = entity receiving damage (monster)
    inflictor = entity causing damage (bullets)
    attacker = parent entity of inflictor (player firing bullets)
    damage = quantity (before modifiers, quad etc)
    checkarmor = can bypass armour (DAMARMOR, NOARMOR)
======================================================================*/
void(entity targ, entity inflictor, entity attacker, float damage, float checkarmor) T_Damage =
{
    local vector dir;
    local entity oldself;
    local float save, take, flinch, misc;

    // Cannot hurt the world or enemy targets (fake flying markers)
    if (targ.classtype == CT_WORLD) return;
    if (targ.classtype == CT_ENEMYTARGET) return;

    // Is the monster dying and going through death animation?
    if (targ.flags & FL_MONSTER && targ.health < 1) {
        if (targ.health > targ.gibhealth) {
            // SSG projectiles need to do more damage (higher chance of gib)
            if (inflictor.classtype == CT_PROJ_SSG) take = 1 + random()*0.25;
            // Lowered the chance of extra gibs from other weapons
            // way to much gibbing was happening to all monsters
            else take = 1;
            
            // Keep taking health off after death
            targ.health = targ.health - (damage*take);
        }
        return;
    }

    // If target cannot take damage, no point doing damage
    if (targ.takedamage == DAMAGE_NO) return;

    // Check for breakable/pushables being immune to monster damage
    // This check is present in all projectiles functions instead
    // Its not really necessary here because its about monsters
    // if (ai_immunedamage(attacker, targ)) return;

    // used by buttons and triggers to set activator for target firing
    damage_attacker = attacker;

    // check for quad damage powerup on the attacker
    if (attacker.super_damage_finished > 0) damage = damage * 4;

    if ((attacker.perms & POW_VAMPIRE) && (attacker.weapon == IT_AXE || attacker.weapon == IT2_MJOLNIR)) damage = damage * 1.25; //Apply Berserker Helm melee damage boost, Drake permanent powerup

    if (targ.classname == "monster_death_lord" && targ.shield == 1)
        damage = damage * 0.5;

    // co-op team play damage avoidance
    if ( (teamplay == 1) && (targ.team > 0) && (targ.team == attacker.team) ) return;

    // Poor zombies, they have so many exceptional ways to die!
    if (targ.classgroup == CG_ZOMBIE) {
        // If a zombie falls into slime or lava, instant death
        if (targ.watertype == CONTENT_SLIME || targ.watertype == CONTENT_LAVA) {
            damage = fabs(targ.gibhealth * 4);
            //dprint("\b[T_DAMAGE]\b Instant death, in slime or lava\n");
        }
        // Exploding boils always destroy zombies
        if (attacker.classtype == CT_MONBOIL) damage = fabs(targ.gibhealth * 2);
    }
    
    // Are monsters infighting and is there a damage modifier active?
    if (attacker.infightextra && targ.flags & FL_MONSTER)
        damage = damage * attacker.infightextra;

    //----------------------------------------------------------------------
    // Check for plasma burn/explosion on fatal hit
    if (inflictor.classtype == CT_PROJ_PLASMA && attacker.flags & FL_CLIENT) {
        if ((targ.health-damage < 0) && attacker.plasma_burn < time && random() < 0.2) {
            // Stop the plasma burn effect not happening alot at once
            attacker.plasma_burn = time + 4 + random()*4;
            // Don't add anymore damage if quad active
            if (attacker.super_damage_finished == 0) 
                damage = (targ.health - targ.gibhealth);
            // Big blue sprite explosion + particles
            particle_explode(targ.origin-'0 0 16', 50+random()*50, 2, PARTICLE_BURST_BLUE, PARTICLE_BURST_UPWARD);
            SpawnExplosion(EXPLODE_PLASMA_BIG, inflictor.origin+'0 0 8', SOUND_REXP3);
        }
    }
    
    //----------------------------------------------------------------------
    // save damage based on the target's armor level
    //----------------------------------------------------------------------
    save = 0;
    if (checkarmor == DAMARMOR) {
        save = ceil(targ.armortype*damage);
        if (save >= targ.armorvalue) {
            save = targ.armorvalue;
            targ.armortype = 0;    // lost all armor
            targ.items2 = targ.items2 - (targ.items2 & (IT2_ARMOR1 | IT2_ARMOR2 | IT2_ARMOR3));
        }
        // Reduce armour based on save forumla above
        targ.armorvalue = targ.armorvalue - save;
        // CEIL function will round up any fractional damage
        // TBH it is better to round up than round down
        take = ceil(damage-save);
    }
    else take = damage;
    
    //----------------------------------------------------------------------
    // add to the damage total for clients, which will be sent as a single
    // message at the end of the frame
    // FIXME: remove after combining shotgun blasts?
    //----------------------------------------------------------------------
    if (targ.flags & FL_CLIENT) {
        targ.dmg_take = targ.dmg_take + take;
        targ.dmg_save = targ.dmg_save + save;
        targ.dmg_inflictor = inflictor;
    }

    // figure momentum add
    if (inflictor != world && targ != world && (targ.movetype == MOVETYPE_WALK || targ.classname == "player")) {
        dir = targ.origin - (inflictor.absmin + inflictor.absmax) * 0.5;
        dir = normalize(dir);
        dir = dir*damage*8;
        if (targ.mass == MASS_SUPER) dir = dir*0.5; // Harder to shove.
        damage_velocity = dir;
        targ.velocity = targ.velocity + dir;
    }
    
    if (attacker.weapon == IT2_CROSS) {
        float z_add = 150;
        if (inflictor.classtype == CT_WANDBIGSPARK) z_add = z_add * 2;
        if (targ.flags & FL_MONSTER) {
            targ.flags = targ.flags - FL_ONGROUND;
            targ.velocity = targ.velocity + dir;
            if (targ.velocity_z < z_add) targ.velocity_z = z_add;    
        }
    }

    // check for godmode or invincibility
    if (targ.flags & FL_GODMODE || targ.invincible_finished >= time) {
        if (targ.invincible_sound < time) {
            sound (targ, CHAN_ITEM, SOUND_ARTPENT3, 1, ATTN_NORM);
            targ.invincible_sound = time + 2;
        }
        return;
    }

    //----------------------------------------------------------------------
    // do the damage
    //----------------------------------------------------------------------
    targ.health = targ.health - take;

    // Apply berserker helm life leech to heal the attacker if the target still has health
    if (attacker.perms & POW_VAMPIRE && targ.health > 0 && (attacker.weapon == IT_AXE || attacker.weapon == IT2_MJOLNIR)) {
        misc = ceil(take * 0.1); //10% of the damage done gets leeched.  Damage was already done so doesn't affect damage amount
        if (misc < 1) misc = 1; // minimum of 1 leech
        attacker.health = attacker.health + misc;
        if (attacker.health > HEAL_MEGAMAX) attacker.health = HEAL_MEGAMAX;
    }
    
    //----------------------------------------------------------------------
    // Check monster turret is to be released based on % health
    // Fire any targets (once only) when this condition is met
    //----------------------------------------------------------------------
    if (targ.turrethealth > 0 && targ.movespeed < 0) {
        if (targ.health < (targ.max_health * targ.turrethealth)) {
            if (targ.turrettarget != "") {
                // Fire all targets and remove trigger string
                trigger_strs(targ.turrettarget, attacker);
                targ.turrettarget = "";
            }
            // Release turret, free movement
            targ.movespeed = 1;
        }
    }

    //----------------------------------------------------------------------
    // When the SG projectile system is active each monster needs to keep
    //track of cumulative damage so that pain flinches work correctly
    // AmmoShells classgroup is only active when SG projectiles are fired
    //----------------------------------------------------------------------
        // Is the Shotguns projectile system active?
    if (inflictor.classgroup == CG_PROJSHELLS) {
        // Does the frame damage acculator need to be reset?
        if (targ.dmgtimeframe <= time) {
            targ.dmgtimeframe = time + 0.1;
            targ.dmgcombined = 0;
        }
        // Accumulate damage until next fromae
        targ.dmgcombined = targ.dmgcombined + take;
    }
    
    //----------------------------------------------------------------------
    // When the SG projectile system is active ZOMBIES need to know if
    // there has been enough damage over a single frame to die or not
    // AmmoShells classgroup is only active when SG projectiles are fired
    //----------------------------------------------------------------------
    if (targ.classgroup == CG_ZOMBIE) {
        // Zombies MUST ALWAYS RUN their pain function!?!
        self.pain_finished = 0;
        // Horrible Hack - Launch_Bullet sets the weapon flag on projectiles
        // Only register damage against zombies if SSG upgrade or Quad!
        if (inflictor.classgroup == CG_PROJSHELLS && inflictor.weapon) {
            // Has the accumulator gone over the max health?
            if (targ.dmgcombined > targ.max_health) {
                targ.health = targ.gibhealth;
            }
        }
    }

    /*----------------------------------------------------------------------
    // Axe upgrade will gib enemies if they are killed with the axe
    // A bit excessive on the gibbing, disabled for the moment
    if (attacker.moditems & IT_SHADAXE && attacker.weapon == IT_AXE) {
        if (targ.health < 1 && targ.gibhealth) targ.health = targ.gibhealth;
    } */

    //----------------------------------------------------------------------
    // Has the monster died?
    //----------------------------------------------------------------------
    if (targ.health < 1) {
        Killed (targ, attacker);
        return;
    }
    
    //----------------------------------------------------------------------
    // Switch self (foundtarget and pain function need self correct)
    // self restored before end of function
    oldself = self;
    self = targ;
    //----------------------------------------------------------------------
    // react to the damage - only if a monster
    // No reaction if attacker is world (env trap, door, etc) 
    //----------------------------------------------------------------------
    if ( (self.flags & FL_MONSTER) && attacker != world && inflictor.classtype != CT_PROJ_BOLT1 && inflictor.classtype != CT_PROJ_BOLT2) {
        // Is the current attacker not self and not current enemy target?
        if (self != attacker && attacker != SUB_entEnemyTarget()) {
            if ( self.noinfighting && attacker.flags & FL_MONSTER 
                || attacker.noinfighting) {
                // Prevent monsters from infighting
                // but let them react to the player
            }
            else {
                // If previous switch target dead, focus on new target
                if (self.switchattacker.health < 1) self.switchtimer = 0;
                // Is the attacker a different classgroup and switching target available?
                if ( self.classgroup != attacker.classgroup && self.switchtimer < time) {
                    // Prevent the monster from rapidily switching targets
                    if (self.switchattacker != attacker) self.switchtimer = time + 1;
                    else self.switchtimer = time + 0.1 + random();
                    // Switch to new target
                    self.switchattacker = attacker;

                    // Shift previous enemy to old enemy ready for new target
                    // Only switch if the new enemy is the player! (id logic)
                    // Check for new enemy target system before switching
                    if (SUB_flagsEnemyTarget(FL_CLIENT)) self.oldenemy = SUB_entEnemyTarget();
                    if (self.enemy.classtype == CT_ENEMYTARGET) SUB_switchEnemyTarget();
                    self.enemy = attacker;        // attack the attacker!
                    FoundTarget ();                // Run+turn towards new enemy
                }
            }
        }
    }
    self = oldself;

    //----------------------------------------------------------------------
    // Check for ammo type resistance and immunity to pain
    // Targetdummies rely on the pain function to display stats
    if (targ.classtype != CT_TARGETDUMMY) {
        if (targ.resist_shells > 0 && inflictor.classgroup == CG_PROJSHELLS) return;
        else if (targ.resist_nails > 0 && inflictor.classgroup == CG_PROJNAILS) return;
        else if (targ.resist_rockets > 0 && inflictor.classgroup == CG_PROJROCKETS) return;
        else if (targ.resist_cells > 0 && inflictor.classgroup == CG_PROJCELLS) return;
    }
    
    //----------------------------------------------------------------------
    // Switch self (foundtarget and pain function need self correct)
    // self restored before end of function
    oldself = self;
    self = targ;
    //----------------------------------------------------------------------

    if (self.th_pain && self.pain_finished < time) {
        // Save pain finished state for later        
        self.pain_finstate = self.pain_finished;

        // The damage from projectile shotguns trinkle at the monster
        // which means it cannot trigger the flinch function
        // Let the combined damage over 1 frame accumulate first
        // Exception - Zombies don't flinch, they must run pain function
        if (inflictor.classgroup == CG_PROJSHELLS && targ.classgroup != CG_ZOMBIE) {
            // Demon, Shambler, Shalrath and Wizard all have high flinch values
            if (self.pain_flinch > 40) flinch = 40;
            else flinch = self.pain_flinch + 1;
            if (self.dmgcombined < flinch) {self = oldself; return;}
        }

        // Block pain re-trigger event
        self.pain_finished = time + self.pain_timeout;
        
        // Ignoring pain from other monsters? (infighting)
        if (self.pain_ignore && attacker.flags & FL_MONSTER)
            self.pain_finished = self.pain_finished + 1 + random();
        else {
            // If accumulated pain is higher, use that instead
            // Added classgroup exception otherwise damage qty is wrong
            if (inflictor.classgroup == CG_PROJSHELLS && take < self.dmgcombined)
                self.th_pain (inflictor, attacker, self.dmgcombined);
            else self.th_pain (inflictor, attacker, take);
    
            // nightmare mode monsters don't go into pain frames often
            if (skill == SKILL_NIGHTMARE && targ.classgroup != CG_ZOMBIE) self.pain_finished = time + 5;

            // Exceptions to pain not being blocked (even nightmare!)
            // If the monster is in slime or lava always go into pain
            if (self.watertype == CONTENT_SLIME || self.watertype == CONTENT_LAVA)
                self.pain_finished = time + 0.3;
        }
    }
    self = oldself;
};

/*======================================================================
    T_RadiusDamage
    
    inflictor -(self) = projectile
    attacker - (self.owner) = monster/player OR (self) = direct attack
    ignore   - (other) = Original target OR (world) = no exceptions
======================================================================*/
void(entity inflictor, entity attacker, float damage, entity ignore, float checkclass) T_RadiusDamage =
{
    local    float     points;
    local    entity    head;
    local    vector    org, imporg;

    // Cannot hurt the world or enemy targets (fake flying markers)
    if (attacker.noradiusdmg) return;
    if (attacker.classtype == CT_WORLD) return;
    if (attacker.classtype == CT_ENEMYTARGET) return;

    // check if inflictor is a bmodel (different origin location)
    if (inflictor.bsporigin) imporg = bmodel_origin(inflictor);
    else imporg = inflictor.origin;
        
    head = findradius(imporg, damage+40);
    
    while (head) {
        if (head != ignore) {
            // This is used mostly for wraiths so when they die they don't
            // kill each other, can't get the code logic to work so its
            // setup as a dead exception path
            if (checkclass == IGNORECLASS && head.classtype == ignore.classtype) { }
            else {
                // Check for breakable/pushable monster immunity
                // Do nothing, radius damage has no miss effect
                if (ai_immunedamage(attacker, head)) {
                }
                // Check for any breakable which is prone to explosive damage
                else if (ai_foundbreakable(attacker, head,TRUE) && head.brktrigmissile !=0) {
                    trigger_ent(head, attacker);
                }
                else {
                    // Can be damaged and NOT immune to radius (splash) damage
                    if (head.takedamage > 0 && head.noradiusdmg == 0) {
                        // This formula is using bounding box size which means
                        // the damage amount is lower if the monster is larger
                        // It should really be based on bounding box edge
                        // not monster origin
                        org = head.origin + (head.mins + head.maxs)*0.5;
                        points = 0.5*vlen (imporg - org);
                        if (points < 0) points = 0;
                        points = damage - points;
                        
                        // Half damage if caught by own explosion
                        if (head == attacker) points = points * 0.5;

                        if (points > 0 && head.health > 0) {
                            // Need CanDamage to check for anything blocking LoS
                            // It will do several traces (source->target) to check
                            // for any blocking entities (lifts, doors, walls etc)
                            if (CanDamage (head, inflictor)) {
                                // Splash damage is part of rocket resistance
                                if (head.resist_rockets > 0)
                                    points = Resist_Damage(head, IT_ROCKETS, points);
                                
                                // Any damage left? Check for poison as well
                                if (points > 0) T_Damage (head, inflictor, attacker, points, DAMARMOR);
                                if (attacker.poisonous) PoisonDeBuff(head);
                            }
                        }
                    }
                }
            }
        }
        head = head.chain;
    }
};

/*======================================================================
    T_BeamDamage (never used, leftover dev crap!)
======================================================================
void(entity attacker, float damage) T_BeamDamage =
{
    local float points;
    local entity head;
    
    // Cannot hurt the world or enemy targets (fake flying markers)
    if (attacker.classtype == CT_WORLD) return;
    if (attacker.classtype == CT_ENEMYTARGET) return;

    head = findradius(attacker.origin, damage+40);
    
    while (head) {
        if (head.takedamage) {
            points = 0.5*vlen (attacker.origin - head.origin);
            if (points < 0) points = 0;
            points = damage - points;
            if (head == attacker) points = points * 0.5;
            if (points > 0) {
                if (CanDamage (head, attacker)) {
                    if (head.resist_rockets > 0) 
                        points = points * head.resist_rockets;
                    T_Damage (head, attacker, attacker, points,DAMARMOR);
                }
            }
        }
        head = head.chain;
    }
}; */

//============
// T_Repair
//
// Add armor to an entity, within limits.  Similar to T_Heal, but for armor.
//============
float(entity e, float heal, float ignore) T_Repair =
{
    local   float   full;

    if (e.health <= 0)
        return 0;   // Dead.
    //if (Armor_NoPatch (e))
        //return 0;

    if (ignore)
        full = 250;     // armorvalue on hud is restricted to max of 255.
    else if (e.armortype == ARMOR_RED_TYPE)
        full = ARMOR_RED_VALUE;
    else if (e.armortype == ARMOR_YEL_TYPE)
        full = ARMOR_YEL_VALUE;
    else
        full = ARMOR_GRN_VALUE;

    if (e.armorvalue >= full)
        return 0;   // Armor already fully restored.

    if (e.armorvalue + heal >= full)
        heal = full - e.armorvalue;
    if (heal < 1)
        return 0;   // Don't damage the armor.

    e.armorvalue = e.armorvalue + heal;
    if (!e.armortype)
    //if (!Armor_Exotic (e)) {
        // Some of the custom armors are cursed!
        // Add green armor.
        e.armortype = ARMOR_GRN_TYPE;
        if (e.flags & FL_CLIENT)
            if (!(e.items2 & IT2_ARMOR1) || !(e.items2 & IT2_ARMOR2) || !(e.items2 & IT2_ARMOR3))
                e.items2 = e.items2 | IT2_ARMOR1;
    //}
    return heal;        // Return amount healed instead of TRUE.
};