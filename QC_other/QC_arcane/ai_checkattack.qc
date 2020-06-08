/*======================================================================
 SPECIFIC AI LOGIC ROUTINES

 * Specific routines for certain monster types to determine if
   they can melee or missile (range) attack
 * Always working 1 frame behind the ai_run function
 * Moved demon/wizard checkattack routines here (split locations)

 CheckAttack (generic)
======================================================================*/
void() CheckAttack =
{
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    //----------------------------------------------------------------------
    // Melee attack
    // enemy_range is checked before this function (ai_run - ai.qc)
    // If the monster is within melee range they instantly attack
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEEKNIGHT) && self.th_melee) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0 && self.th_missile) {
        if (time < self.attack_finished) return;
        SUB_AttackFinished (2 + random());
        self.attack_state = AS_MISSILE;
        return;
    }

    //----------------------------------------------------------------------
    // Range attack
    // The attack chance percentages are constant across skill levels
    //----------------------------------------------------------------------
    if (!self.th_missile) return;
    if (time < self.attack_finished) return;
    if (enemy_range == RANGE_FAR) return;

    // range < 120 map units
    if (enemy_range == RANGE_MELEE) {
        self.attack_chance = 0.9;
        self.attack_finished = 0;
    }
    // range < 500 map units
    else if (enemy_range == RANGE_NEAR) {
        if (self.th_melee) self.attack_chance = 0.2;
        else self.attack_chance = 0.4;
    }
    // range < 1000 map units
    else if (enemy_range == RANGE_MID) {
        if (self.th_melee) self.attack_chance = 0.05;
        else self.attack_chance = 0.1;
    }
    else self.attack_chance = 0;

    if (random () < self.attack_chance) {
        SUB_AttackFinished (2*random());
        self.th_missile ();
    }
};

/*======================================================================
 SoldierCheckAttack (Rocket version has melee)
======================================================================*/
void() SoldierCheckAttack =
{
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (visblocked(self.enemy)) return;
        SUB_AttackFinished (2 + random());
        self.attack_state = AS_MISSILE;
        return;
    }
    //----------------------------------------------------------------------
    // If enemy is within melee range, just keep on attacking!
    // Original ID behaviour is 10% chance to not attack in melee range
    // This has changed so that the soldier is more agressive up close
    // The army rocket has a special behaviour for melee range
    //----------------------------------------------------------------------
    if (self.classtype != CT_MONARMYROCKET && enemy_range == RANGE_MELEE) {
        self.attack_state = AS_MISSILE;
        return;
    }

    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;
    if (!visxray(self.enemy, self.attack_offset, '0 0 0', FALSE)) return;

    if (self.classtype == CT_MONARMYROCKET) {
        if (ai_checkmelee(MONAI_RANGEARMYR)) {
            self.oldorigin = self.origin;
            // If move backward works, run back animation
            if (walkmove(self.angles_y+180, 16)) {
                setorigin(self, self.oldorigin);
                self.attack_state = AS_MELEE;
                self.th_melee ();
                return;
            }
            else setorigin(self, self.oldorigin);
        }
    }

    //----------------------------------------------------------------------
    // Range attack (bullets)
    //----------------------------------------------------------------------
    if (time < self.attack_finished) return;
    if (enemy_range == RANGE_FAR) return;
    if (enemy_range == RANGE_MELEE) self.attack_chance = 0.9;        // range < 120 map units
    else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.4;    // range < 500 map units
    else if (enemy_range == RANGE_MID) self.attack_chance = 0.05;    // range < 1000 map units
    else self.attack_chance = 0;
    if (random () < self.attack_chance) {
        self.attack_state = AS_MISSILE;
        if (self.classtype == CT_MONARMYROCKET) SUB_AttackFinished (2 + random());
        else SUB_AttackFinished (1 + random());
        if (random() < 0.3) self.lefty = !self.lefty;
    }
};

/*======================================================================
 PyrpCheckAttack (Melee=FIRE!)
======================================================================*/
void() PyroCheckAttack =
{
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    if (ai_checkmelee(MONAI_RANGEPYRO)) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
        return;
    }
};

/*======================================================================
 ShamCheckAttack
======================================================================*/
void() ShamCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack(claws/overhead smash)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEESHAM)) {
        self.attack_state = AS_MELEE;
        // Don't wait for next frame, melee attack straight away
        // (different id behaviour)
        self.th_melee ();
        return;
    }

    //----------------------------------------------------------------------
    // Range attack (lightning)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;
    if (self.enemydist > MONAI_SHAMRANGE && !self.attack_sniper) return;

    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    SUB_AttackFinished (2 + (2*random()));
    self.attack_state = AS_MISSILE;
};

/*======================================================================
 GugCheckAttack
======================================================================*/
void() GugCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack(left/right smack)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEEGUG)) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
        return;
    }

    //----------------------------------------------------------------------
    // Range attack (Bile Bombs)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;
};

/*======================================================================
 BogLordCheckAttack (Shambler model)
======================================================================*/
void() BogLordCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack(overhead slime balls)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_BOGLORDMELEE)) {
        // Easy to dodge ball attack, bolts are more difficult
        if (self.spawnflags & MON_BOGL_STRONG)
            self.attack_chance = random();
        else {
            // Easy = 10%, Normal = 20%, Hard = 30%, NM = 40%
            self.attack_chance = 0.1 + (skill * 0.1);
        }
        if (random() < self.attack_chance) self.attack_state = AS_MISSILE;
        else { self.attack_state = AS_MELEE; self.th_melee (); }
        return;
    }

    //----------------------------------------------------------------------
    // Range attack (Fast volley of bolts)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;

    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    // Setup attack chance of using fast bolt attack more often
    // easy=0.0, norm=0.2, hard=0.4, nm=0.6
    // The stronger version is just pure random 50/50
    if (self.spawnflags & MON_BOGL_STRONG) self.attack_chance = random();
    else self.attack_chance = 0.0 + (skill * 0.2);

    // Give the monster a chance to move if not tethered
    if (!self.tethered) SUB_AttackFinished (1 + 2*random());

    // Check for random chance to keep doing overhead smash
    if (random() > self.attack_chance) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
    }
    // Range attack harder to dodge at higher projectile speed
    else self.attack_state = AS_MISSILE;
};

/*======================================================================
 SeekerCheckAttack
======================================================================*/
void() SeekerCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee punch
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELRAGESEEKER)) {
        // Need both arms for melee animation
        if (self.state == 0) {
            self.attack_state = AS_MELEE;
            self.th_melee ();
        }
        // Instantly go to rocket range attack
        // If the player is too close
        else {
            self.attack_state = AS_MISSILE;
            SUB_AttackFinished (1 + random());
        }
        return;
    }

    //----------------------------------------------------------------------
    // Range attack (rockets or lasers)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;

    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    self.attack_state = AS_MISSILE;
    SUB_AttackFinished (2 + 2*random());
};

/*======================================================================
 SantaCheckAttack
======================================================================*/
void() SantaCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (Antlers)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEESANTA)) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (visblocked(self.enemy)) return;
        SUB_AttackFinished (1 + 2*random());
        self.attack_state = AS_MISSILE;
        return;
    }
    //----------------------------------------------------------------------
    // Let sightsound play out before machine gun attack
    //----------------------------------------------------------------------
    if (self.attack_rage) {
        self.attack_finished = time + 3;
        self.attack_rage = FALSE;
    }
    //----------------------------------------------------------------------
    // Range attack (snowball machine gun)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;

    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;
    // Check if gun is being blocked (extra wide check)
    if ( visblocked_wide(self.enemy, self.attack_offset, '0 0 0') ) return;

    // Straight to snow machine gun!
    self.attack_state = AS_MISSILE;
    SUB_AttackFinished (2 + 2*random());
};

/*======================================================================
 RaindeerCheckAttack
======================================================================*/
void() RaindeerCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (Antlers)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEERAINDEER)) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (visblocked(self.enemy)) return;
        SUB_AttackFinished (1 + 2*random());
        self.attack_state = AS_MISSILE;
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (rockets)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    self.attack_state = AS_MISSILE;
    SUB_AttackFinished (2 + 2*random());
};

/*======================================================================
 SnowmanCheckAttack
======================================================================*/
void() SnowmanCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (Antlers)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEESNOWMAN)) {
        self.attack_state = AS_MELEE;
        // As soon as out of melee, range attack!
        self.attack_finished = 0;
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (rockets)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    self.attack_state = AS_MISSILE;
    SUB_AttackFinished (2 + 2*random());
};

/*======================================================================
 GolemCheckAttack
======================================================================*/
void() GolemCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack(Punch/Pound)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (self.enemydist < MONAI_MELEEGOLEM) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // Floor stomp attack
    //----------------------------------------------------------------------
    if (self.enemydist < MONAI_GOLEMRANGE && random() < 0.3) {
        SUB_AttackFinished (1 + 2*random());
        self.th_slide ();
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (Rock Attack)
    //----------------------------------------------------------------------
    if (time < self.attack_finished) return;
    if (!self.th_missile) return;

    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (!visblocked_wide(self.enemy, self.attack_offset, '0 0 24')) {
        self.attack_state = AS_MISSILE;
        SUB_AttackFinished (2 + 2*random());
    }
};

/*======================================================================
 ShalCheckAttack
======================================================================*/
void() ShalCheckAttack =
{
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        // Slower spawnrate for minion eggs than voreballs
        if (!(self.spawnflags & MON_SHALRATH_MINIONS)) {
            if (visblocked(self.enemy)) return;
            SUB_AttackFinished (2*random());
        }
        else SUB_AttackFinished (1 + 2*random());
        self.attack_state = AS_MISSILE;
        return;
    }
    // Minion spawning shalraths should maintain distance
    // Use the new turn and side walk function to stay mid range
    if (self.spawnflags & MON_SHALRATH_MINIONS && self.enemy.flags & FL_CLIENT) {
        // Calculate a flat vector to ignore Z axis difference
        self.enemydist = range_distance(self.enemy, TRUE);
        // If too far away from enemy, move in a straight line
        if (self.enemydist > MONAI_RANGESHAL) {
            self.attack_state = AS_STRAIGHT;
        }
        else {
            // If range attack still blocked, move sideway/backwards
            if (time < self.attack_finished) {
                // If too close, move backwards and sideways so that
                // there is plenty of room to spawn more minions
                if (self.enemydist < MONAI_RANGESHAL2)
                    self.attack_state = AS_BACKWARD;
                else self.attack_state = AS_SIDESTEP;
                // straight away move
                return;
            }
        }
    }

    // Does the monster have a clear shot to the enemy?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    //----------------------------------------------------------------------
    // Range attack
    // The attack chance percentages are constant across skill levels
    //----------------------------------------------------------------------
    if (enemy_range == RANGE_FAR) return;
    if (time < self.attack_finished) return;

    // random chance based on distance to enemy
    if (enemy_range == RANGE_MELEE) {
        self.attack_chance = 0.9;    // < 120
        self.attack_finished = 0;    // If enemy really close, constantly fire!
    }
    else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.4;    // < 500
    else if (enemy_range == RANGE_MID) self.attack_chance = 0.1;    // < 1000
    else self.attack_chance = 0;

    // Random chance of range attack?
    if (random () < self.attack_chance) {
        // Slower spawnrate for minion eggs than voreballs
        if (self.spawnflags & MON_SHALRATH_MINIONS) SUB_AttackFinished (1 + 2*random());
        else SUB_AttackFinished (2*random());
        self.attack_state = AS_MISSILE;
    }
};

/*======================================================================
 EliminatorCheckAttack (Range=Plasma)
======================================================================*/
void() EliminatorCheckAttack =
{
    //----------------------------------------------------------------------
    // Range attack (Plasma)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;
    // This is a range check for firing plasma bolts
    // Could easily work with massive range and attack_sniper
    if (enemy_range == RANGE_FAR && !self.attack_sniper) return;
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    // Aggressive range attack, no % chances
    self.attack_state = AS_MISSILE;
    SUB_AttackFinished (1 + 2*random());
};

/*======================================================================
 DefenderCheckAttack (Melee=SSG / Range=GL)
======================================================================*/
void() DefenderCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (Super Shotgun to face)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_RANGEDEFSSG)) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (Grenades)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;
    // This is a range check for firing grenades
    // It need to be less than 600 units, otherwise likely to miss
    // No point having attack_sniper versions firing early
    if (enemy_range == RANGE_FAR) return;
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    // Check enemy for grenade resistance?
    if (self.enemy.bouncegrenade) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
    }
    else {
        // Aggressive range attack, no % chances
        self.attack_state = AS_MISSILE;
        SUB_AttackFinished (1 + 2*random());
    }
};

/*======================================================================
 OgreCheckAttack
 Vanilla, Hunter, Fishing and Mace versions
======================================================================*/
void() OgreCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (chainsaw)
    //----------------------------------------------------------------------
    if (self.enemydist < self.meleerange) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (visblocked(self.enemy)) return;
        SUB_AttackFinished (1 + 2*random());
        self.attack_state = AS_MISSILE;
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (grenades)
    // strangely enough the chance attack percentages are not used
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    // Check enemy for grenade resistance?
    // Move closer to enemy instead for melee range
    if (!self.enemy.bouncegrenade) {
        self.attack_state = AS_MISSILE;
        // Fishing ogre fires much faster than standard
        if (self.classtype == CT_MONOGREFISH)
            SUB_AttackFinished (1 + random());
        else SUB_AttackFinished (1 + 2*random());
    }
};

/*======================================================================
 OgreHamCheckAttack (Hammer only, much faster)
======================================================================*/
void() OgreHamCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (Special hammer attack)
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_HOGRE_METAL) {
        if (self.enemydist < MONAI_MELEEOGREHAM) {
            self.attack_state = AS_MELEE;
            return;
        }
    }
    else {
        // Default melee attack, just a simple hammer
        if (self.enemydist < self.meleerange) {
            self.attack_state = AS_MELEE;
            return;
        }
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (visblocked(self.enemy)) return;
        SUB_AttackFinished (1 + 2*random());
        self.attack_state = AS_MISSILE;
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (grenades)
    // strangely enough the chance attack percentages are not used
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    self.attack_state = AS_MISSILE;
    SUB_AttackFinished (1 + random());
};

/*======================================================================
 FreddieCheckAttack
======================================================================*/
void() FreddieCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (chainsaw)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEEFREDDIE)) {
        // Hard/Nightmare skill have chance to ignore melee attacks
        if (skill < SKILL_HARD || (skill > SKILL_NORMAL && random() < 0.7)) {
            self.attack_state = AS_MELEE;
            return;
        }
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (visblocked(self.enemy)) return;
        SUB_AttackFinished (1 + 2*random());
        self.attack_state = AS_MISSILE;
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (spikes/laser)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    // If Freddie too far away for stationary attack?
    if (self.enemydist > MONAI_RANGEFREDDIE && skill > SKILL_EASY) {
        // Work out vector angle of enemy infront
        makevectors (self.angles);
        self.pos1 = normalize (self.enemy.origin - self.origin);
        self.lip = self.pos1 * v_forward;
        // Is the enemy infront of freddie?
        if (self.lip > 0.8) {
            self.cnt = 0;
            self.attack_timer = TRUE;
            SUB_AttackFinished (1 + random());
        }
        else {
            // Small random chance of stop+fire
            if (random() < 0.2) {
                self.attack_timer = FALSE;
                self.attack_state = AS_MISSILE;
                SUB_AttackFinished (1 + 2*random());
            }
        }
    }
    else {
        // Close to enemy, stop+fire
        self.attack_timer = FALSE;
        self.attack_state = AS_MISSILE;
        SUB_AttackFinished (1 + 2*random());
    }
};

/*======================================================================
 WizardCheckAttack (No melee attack)
======================================================================*/
void() WizardCheckAttack =
{
    //----------------------------------------------------------------------
    // Range attack (spit)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (visblocked(self.enemy)) return;
        self.attack_state = AS_MISSILE;
        return;
    }
    //----------------------------------------------------------------------
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    // The original id behaviour is wizards run until within range
    //----------------------------------------------------------------------
    if (enemy_range == RANGE_FAR || visblocked(self.enemy)) {
        if (self.attack_state != AS_STRAIGHT) self.attack_state = AS_STRAIGHT;
        return;
    }

    if (enemy_range == RANGE_MELEE) self.attack_chance = 0.9;        // range < 120 map units
    else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.6;    // range < 500 map units
    else if (enemy_range == RANGE_MID) self.attack_chance = 0.2;    // range < 1000 map units
    else self.attack_chance = 0;
    if (random () < self.attack_chance) {
        self.attack_state = AS_MISSILE;
    }
    //----------------------------------------------------------------------
    // If turret, then do not move sideways
    // otherwise keep moving forward to enemy target
    //----------------------------------------------------------------------
    else if (enemy_range == RANGE_MID) {
        if (self.attack_state != AS_STRAIGHT) self.attack_state = AS_STRAIGHT;
    }
    else {
        if (self.attack_state != AS_SLIDING) {
            self.attack_state = AS_SLIDING;
            self.th_slide ();
        }
    }
};

/*======================================================================
 SkullWizardCheckAttack (No melee attack)
======================================================================*/
void() SkullWizCheckAttack =
{
    //----------------------------------------------------------------------
    // Teleport away if player too close
    //----------------------------------------------------------------------
    if (self.enemydist < MONAI_MELEESKULLW && !self.bodystatic) {
        self.attack_state = AS_MELEE;
        return;
    }
    // make sure enemy is stationary (turret)
    else self.attack_state = AS_TURRET;

    //----------------------------------------------------------------------
    // Range / missile attack (skull attack)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    self.attack_state = AS_MISSILE;
    SUB_AttackFinished (1 + 2*random());
};

/*======================================================================
 LostCheckAttack
======================================================================*/
void() LostCheckAttack =
{
    //----------------------------------------------------------------------
    // setup enemytarget if one is not active
    //----------------------------------------------------------------------
    if (self.enemy.classtype != CT_ENEMYTARGET && self.height > 0) {
        SUB_setupEnemyTarget(self.enemy, self.height, MONAI_ABOVETIMER);
        if (self.enemytarget) self.enemy = self.enemytarget;
    }

    //----------------------------------------------------------------------
    // Melee attack (bite)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEELOSTSOUL)) {
        self.attack_state = AS_MELEE;
        return;
    }

    //----------------------------------------------------------------------
    // If too close, move sideways until can ram again
    // If too far, move closer
    //----------------------------------------------------------------------
    if (self.enemydist < MONAI_RANGELOSTNEAR) {
        self.attack_state = AS_SLIDING;
        return;
    }
    else if (self.enemydist > MONAI_RANGELOSTFAR) {
        self.attack_state = AS_STRAIGHT;
        return;
    }

    //----------------------------------------------------------------------
    // Cannot see enemy? go into guard mode
    //----------------------------------------------------------------------
    if (!enemy_vis && self.lostsearch == FALSE) {
        SUB_switchEnemyTarget();
        self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
        self.lostsearch = TRUE;
        self.lostenemy = self.enemy;
        self.losttimer = time + MONAI_LOSTTIMER + random()*5;
        self.enemy = self.goalentity = self.movetarget = world;
        self.th_altstand();
        return;
    }

    //----------------------------------------------------------------------
    // Range attack (Ramming Speed)
    //----------------------------------------------------------------------
    // Only range attack if cooldown has finished
    if (time > self.attack_finished) {
        if (!visblocked_wide(SUB_entEnemyTarget(), self.view_ofs, '0 0 24')) {
            SUB_AttackFinished(2 + random());
            self.attack_state = AS_MISSILE;
        }
        // No space for raming speed, strafe side to side
        else self.attack_state = AS_SLIDING;
    }
    // Wait for attack timer, better to face the player
    else self.attack_state = AS_SLIDING;
};

/*======================================================================
 JimCheckAttack (No melee attack)
 This is essentially the monster_centarion check
======================================================================*/
void() JimCheckAttack =
{
    // Cannot see enemy? stop chasing enemytarget
    if (!enemy_vis) {
        SUB_switchEnemyTarget();
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (!visblocked_wide(SUB_entEnemyTarget(), self.attack_offset, '0 0 0')) {
            SUB_AttackFinished(2 + random());
            self.attack_state = AS_MISSILE;
        }
        return;
    }
    //----------------------------------------------------------------------
    // setup enemytarget if one is not active
    //----------------------------------------------------------------------
    if (self.enemy.classtype != CT_ENEMYTARGET && self.height > 0) {
        SUB_setupEnemyTarget(self.enemy, self.height, MONAI_ABOVETIMER);
        if (self.enemytarget) self.enemy = self.enemytarget;
    }
    //----------------------------------------------------------------------
    // Range attack (Lasers)
    //----------------------------------------------------------------------
    // Only range attack if cooldown has finished
    if (time > self.attack_finished) {
        if (enemy_range == RANGE_MELEE) self.attack_chance = 0.9;        // range < 120 map units
        else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.6;    // range < 500 map units
        else if (enemy_range == RANGE_MID) self.attack_chance = 0.2;    // range < 1000 map units
        else self.attack_chance = 0;
        if (random () < self.attack_chance) {
            if (!visblocked_wide(SUB_entEnemyTarget(), self.attack_offset, '0 0 0')) {
                if (enemy_range != RANGE_MELEE)    SUB_AttackFinished(2 + random());
                self.attack_state = AS_MISSILE;
                return;
            }
        }
    }
    //----------------------------------------------------------------------
    // Make sure jim maintains its distance (strafe)
    //----------------------------------------------------------------------
    if (enemy_range >= RANGE_MID) {
        if (self.attack_state != AS_STRAIGHT) self.attack_state = AS_STRAIGHT;
    }
    else self.attack_state = AS_SLIDING;
};

/*======================================================================
 SentinelCheckAttack (No melee attack)
======================================================================*/
void() SentinelCheckAttack =
{
    // Cannot see enemy? stop chasing enemytarget
    if (!enemy_vis) {
        SUB_switchEnemyTarget();
        return;
    }
    //----------------------------------------------------------------------
    // setup enemytarget if one is not active
    //----------------------------------------------------------------------
    if (self.enemy.classtype != CT_ENEMYTARGET && self.height > 0) {
        SUB_setupEnemyTarget(self.enemy, self.height, MONAI_ABOVETIMER);
        if (self.enemytarget) self.enemy = self.enemytarget;
    }
    //----------------------------------------------------------------------
    // Range attack (Laser/Nail)
    //----------------------------------------------------------------------
    // Only range attack if cooldown has finished
    if (time > self.attack_finished) {
        if (!visblocked_wide(SUB_entEnemyTarget(), self.attack_offset, '0 0 0')) {
            SUB_AttackFinished(1);
            self.attack_state = AS_MISSILE;
            return;
        }
    }
};

/*======================================================================
 CenturionCheckAttack (No melee attack)
======================================================================*/
void() CenturionCheckAttack =
{
    // Cannot see enemy? stop chasing enemytarget
    if (!enemy_vis) {
        SUB_switchEnemyTarget();
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (!visblocked_wide(SUB_entEnemyTarget(), self.attack_offset, '0 0 0')) {
            SUB_AttackFinished(2 + random());
            self.attack_state = AS_MISSILE;
        }
        return;
    }
    //----------------------------------------------------------------------
    // setup enemytarget if one is not active
    //----------------------------------------------------------------------
    if (self.enemy.classtype != CT_ENEMYTARGET && self.height > 0) {
        SUB_setupEnemyTarget(self.enemy, self.height, MONAI_ABOVETIMER);
        if (self.enemytarget) self.enemy = self.enemytarget;
    }
    //----------------------------------------------------------------------
    // Range attack (Plasma)
    //----------------------------------------------------------------------
    // Only range attack if cooldown has finished
    if (time > self.attack_finished) {
        if (enemy_range == RANGE_MELEE) self.attack_chance = 0.9;        // range < 120 map units
        else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.6;    // range < 500 map units
        else if (enemy_range == RANGE_MID) self.attack_chance = 0.2;    // range < 1000 map units
        else self.attack_chance = 0;
        if (random () < self.attack_chance) {
            if (!visblocked_wide(SUB_entEnemyTarget(), self.attack_offset, '0 0 0')) {
                if (enemy_range != RANGE_MELEE)    SUB_AttackFinished(2 + random());
                self.attack_state = AS_MISSILE;
                return;
            }
        }
    }
    //----------------------------------------------------------------------
    // Make sure the centurion maintains its distance (strafe)
    //----------------------------------------------------------------------
    if (enemy_range >= RANGE_MID) {
        if (self.attack_state != AS_STRAIGHT) self.attack_state = AS_STRAIGHT;
    }
    else self.attack_state = AS_SLIDING;
};

/*======================================================================
 GargoyleCheckAttack (No melee attack)
======================================================================*/
void() GargoyleCheckAttack =
{
    // Cannot see enemy? stop chasing enemytarget
    if (!enemy_vis) {
        SUB_switchEnemyTarget();
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (!visblocked_wide(SUB_entEnemyTarget(), self.attack_offset, '0 0 0')) {
            SUB_AttackFinished(2 + random());
            self.attack_state = AS_MISSILE;
        }
        return;
    }
    //----------------------------------------------------------------------
    // setup enemytarget if one is not active
    //----------------------------------------------------------------------
    if (self.enemy.classtype != CT_ENEMYTARGET && self.height > 0) {
        SUB_setupEnemyTarget(self.enemy, self.height, MONAI_ABOVETIMER);
        if (self.enemytarget) self.enemy = self.enemytarget;
    }
    //----------------------------------------------------------------------
    // Range attack (Fireball)
    //----------------------------------------------------------------------
    // Only range attack if cooldown has finished
    if (time > self.attack_finished) {
        if (enemy_range == RANGE_MELEE) self.attack_chance = 0.9;        // range < 120 map units
        else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.6;    // range < 500 map units
        else if (enemy_range == RANGE_MID) self.attack_chance = 0.2;    // range < 1000 map units
        else self.attack_chance = 0;
        if (random () < self.attack_chance) {
            if (!visblocked_wide(SUB_entEnemyTarget(), self.attack_offset, '0 0 0')) {
                if (enemy_range != RANGE_MELEE)    SUB_AttackFinished(2 + random());
                self.attack_state = AS_MISSILE;
                return;
            }
        }
    }

    //----------------------------------------------------------------------
    // Make sure the gargoyle maintains its distance (strafe)
    //----------------------------------------------------------------------
    if (enemy_range >= RANGE_MID) {
        if (self.attack_state != AS_STRAIGHT) self.attack_state = AS_STRAIGHT;
    }
    else self.attack_state = AS_SLIDING;
};

/*======================================================================
 GauntCheckAttack (No melee attack)
======================================================================*/
void() GauntCheckAttack =
{
    // Cannot see enemy? stop chasing enemytarget
    if (!enemy_vis) {
        SUB_switchEnemyTarget();
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (!visblocked_wide(SUB_entEnemyTarget(), self.attack_offset, '0 0 0')) {
            SUB_AttackFinished(2 + random());
            self.attack_state = AS_MISSILE;
        }
        return;
    }
    //----------------------------------------------------------------------
    // setup enemytarget if one is not active
    //----------------------------------------------------------------------
    if (self.enemy.classtype != CT_ENEMYTARGET && self.height > 0) {
        SUB_setupEnemyTarget(self.enemy, self.height, MONAI_ABOVETIMER);
        if (self.enemytarget) self.enemy = self.enemytarget;
    }
    //----------------------------------------------------------------------
    // Range attack (Plasma volley)
    //----------------------------------------------------------------------
    // Only range attack if cooldown has finished
    if (time > self.attack_finished) {
        if (enemy_range == RANGE_MELEE) self.attack_chance = 0.9;        // range < 120 map units
        else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.6;    // range < 500 map units
        else if (enemy_range == RANGE_MID) self.attack_chance = 0.2;    // range < 1000 map units
        else self.attack_chance = 0;
        if (random () < self.attack_chance) {
            if (!visblocked_wide(SUB_entEnemyTarget(), self.attack_offset, '0 0 0')) {
                if (enemy_range != RANGE_MELEE)    SUB_AttackFinished(2 + random());
                self.attack_state = AS_MISSILE;
                return;
            }
        }
    }

    //----------------------------------------------------------------------
    // Make sure the Gaunt maintains its distance (strafe)
    //----------------------------------------------------------------------
    if (enemy_range >= RANGE_MID) {
        if (self.attack_state != AS_STRAIGHT) self.attack_state = AS_STRAIGHT;
    }
    else self.attack_state = AS_SLIDING;
};

/*======================================================================
 FishCheckAttack (melee only)
======================================================================*/
void() FishCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (Bite)
    // Uses larger knight distance and closer bite check afterward
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEEKNIGHT)) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
        return;
    }
};

/*======================================================================
 EelCheckAttack (No melee attack)
======================================================================*/
void() EelCheckAttack =
{
    //----------------------------------------------------------------------
    // Range attack (Plasma bolt)
    //----------------------------------------------------------------------
    // Only range attack if cooldown has finished
    if (time > self.attack_finished) {
        if (!visblocked_wide(self.enemy, self.attack_offset, '0 0 0')) {
            if (enemy_range != RANGE_MELEE)    SUB_AttackFinished(2 + random());
            self.attack_state = AS_MISSILE;
            return;
        }
    }

    //----------------------------------------------------------------------
    // Make sure the Eel maintains its distance (strafe)
    //----------------------------------------------------------------------
    if (enemy_range >= RANGE_MID || !enemy_vis) {
        if (self.attack_state != AS_STRAIGHT) self.attack_state = AS_STRAIGHT;
    }
    else self.attack_state = AS_SLIDING;
};

/*======================================================================
 WraithCheckAttack (No melee attack)
======================================================================*/
void() WraithCheckAttack =
{
    //----------------------------------------------------------------------
    // Cannot see enemy? stop chasing enemytarget
    //----------------------------------------------------------------------
    if (!enemy_vis) {
        SUB_switchEnemyTarget();
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (!visblocked_wide(SUB_entEnemyTarget(), self.attack_offset, '0 0 0')) {
            SUB_AttackFinished(2 + random());
            self.attack_state = AS_MISSILE;
        }
        return;
    }
    //----------------------------------------------------------------------
    // setup enemytarget if one os not active
    //----------------------------------------------------------------------
    if (self.enemy.classtype != CT_ENEMYTARGET && self.height > 0) {
        SUB_setupEnemyTarget(self.enemy, self.height, MONAI_ABOVETIMER);
        if (self.enemytarget) self.enemy = self.enemytarget;
    }
    //----------------------------------------------------------------------
    // Range attack (burning and creature summons)
    // Much more aggressive decision on range attacks (ogre style)
    // No random chance percentages, logic with wraith magic function
    //----------------------------------------------------------------------
    // Only range attack if cooldown has finished
    if (time > self.attack_finished) {
        if (!visblocked_wide(SUB_entEnemyTarget(), self.attack_offset, '0 0 0')) {
            if (enemy_range != RANGE_MELEE)    SUB_AttackFinished(2 + random());
            self.attack_state = AS_MISSILE;
            return;
        }
    }

    //----------------------------------------------------------------------
    // If turret, then do not move sideways
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (self.attack_state != AS_STRAIGHT) self.attack_state = AS_STRAIGHT;
        return;
    }
    //----------------------------------------------------------------------
    // Make sure the wraith maintains its distance (strafe)
    //----------------------------------------------------------------------
    if (enemy_range >= RANGE_MID) {
        if (self.attack_state != AS_STRAIGHT) self.attack_state = AS_STRAIGHT;
    }
    else self.attack_state = AS_SLIDING;
};

/*======================================================================
 MinotaurCheckAttack
======================================================================*/
void() MinotaurCheckAttack =
{
    //----------------------------------------------------------------------
    // If health is low enough, switch to rage mode
    // This does not affect the minion spawning version
    if ( !(self.spawnflags & MON_MINOTAUR_MINIONS) && self.movespeed >= 0) {
        if (self.health < self.max_health*0.5 && !self.attack_rage) {
            self.attack_rage = TRUE;
            self.th_charge();    // Short howl at the sky
            return;
        }
    }
    //----------------------------------------------------------------------
    // Melee attack (claws)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEEMINOTAUR)) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (visblocked(self.enemy)) return;
        // Keep firing rockets when at range
        SUB_AttackFinished (1 + 2*random());
        self.attack_state = AS_MISSILE;
        return;
    }
    //----------------------------------------------------------------------
    // RAGE mode (keep running at player)
    //----------------------------------------------------------------------
    if (self.attack_rage) {
        if (self.attack_state != AS_STRAIGHT) self.attack_state = AS_STRAIGHT;
        // Check range and sight for a low chance range attack
        if (!enemy_vis) return;
        if (time < self.attack_finished) return;
        // If enemy not infront or random chance, stop and range attack
        if (!infront(self.enemy) || random() < 0.1) {
            // Does the monster have a clear shot to the player?
            // sightline can be blocked by other monsters
            if (visblocked(self.enemy)) return;
            // Plasma bolt attack
            SUB_AttackFinished (2 + 2*random());
            self.attack_state = AS_MISSILE;
        }
    }
    //----------------------------------------------------------------------
    // PASSIVE mode (keep at distance)
    //----------------------------------------------------------------------
    else {
        // Is the player NOT visible? Keep getting closer
        if (!enemy_vis) {
            if (self.attack_state != AS_STRAIGHT)
                self.attack_state = AS_STRAIGHT;
        }
        else {
            //----------------------------------------------------------------------
            // Mid Range attack (JUMP) Not spawning dark version
            //----------------------------------------------------------------------
            if ( !(self.spawnflags & MON_MINOTAUR_MINIONS) && random() < 0.35) {
                // Jumped recently, facing right direction and not blocked?
                if ( self.jump_flag < time && infront(self.enemy)) {
                    if (!visblocked_wide(self.enemy, self.view_ofs, self.enemy.view_ofs) ) {
                        // Check for enemy above? (z axis)
                        if (self.enemy.origin_z <= self.origin_z) {
                            // Is the minotaur within the right range?
                            if (self.enemydist > MONAI_JUMPMINONEAR &&
                                self.enemydist < MONAI_JUMPMINOFAR) {
                                // Block any range attacks for a while
                                SUB_AttackFinished (random());
                                self.jumptouch = world;    // Reset last object touched
                                self.count = 0;            // Number of times jumped
                                self.th_jump ();
                                return;
                            }
                        }
                    }
                }
            }
            //----------------------------------------------------------------------
            // Range / missile attack (plasma bolts)
            //----------------------------------------------------------------------
            // Any chance of a range attack?
            if (time < self.attack_finished) {
                // Calculate a flat vector to ignore Z axis difference
                self.enemydist = range_distance(self.enemy, TRUE);
                // Don't always stay at absolute range, move closer
                if (random() < 0.15 && self.enemydist > MONAI_RANGEMINOTAUR) {
                    self.attack_sidedeny = time + 1 + random();
                    self.attack_state = AS_STRAIGHT;
                }
                else {
                    // If not blocked, turn and move sideways
                    if (self.attack_sidedeny < time)
                        self.attack_state = AS_SIDESTEP;
                }
            }
            else {
                // Does the monster have a clear shot to the player?
                // sightline can be blocked by other monsters
                if (visblocked(self.enemy)) return;
                // Keep firing plasma when at range
                SUB_AttackFinished (2 + 2*random());
                self.attack_state = AS_MISSILE;
            }
        }
    }
};

/*======================================================================
 DroleCheckAttack
======================================================================*/
void() DroleCheckAttack =
{
    //----------------------------------------------------------------------
    // Quoth setup - 500HP, with rage at 350HP
    // Converted it to a % so mappers can change health
    //----------------------------------------------------------------------
    if (self.movespeed >= 0) {
        if (self.health < self.max_health*0.7 && !self.attack_rage) {
            self.attack_rage = TRUE;
        }
    }
    //----------------------------------------------------------------------
    // Melee attack (claws)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEEDROLE2)) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        if (visblocked(self.enemy)) return;
        // Keep firing rockets when at range
        SUB_AttackFinished (1 + 2*random());
        self.attack_state = AS_MISSILE;
        return;
    }
    //----------------------------------------------------------------------
    // RAGE mode (keep running at player)
    //----------------------------------------------------------------------
    if (self.attack_rage) {
        if (self.attack_state != AS_STRAIGHT) self.attack_state = AS_STRAIGHT;
        // Check range and sight for a low chance range attack
        if (!enemy_vis) return;
        if (time < self.attack_finished) return;
        // Calculate a flat vector to ignore Z axis difference
        // Not convinced a flat vector is good for a melee only state
        // Switched to 3D distance chance so its not so dumb
        self.enemydist = range_distance(self.enemy, FALSE);
        if (self.enemydist > MONAI_RANGEDROLE && random() < 0.3) {
            // Does the monster have a clear shot to the player?
            // sightline can be blocked by other monsters
            if (visblocked(self.enemy)) return;
            // Standard rocket attack
            SUB_AttackFinished (1 + 2*random());
            self.attack_state = AS_MISSILE;
        }
    }
    //----------------------------------------------------------------------
    // PASSIVE mode (keep at distance)
    //----------------------------------------------------------------------
    else {
        // Is the player visible? Keep getting closer
        if (!enemy_vis) {
            if (self.attack_state != AS_STRAIGHT)
                self.attack_state = AS_STRAIGHT;
        }
        // Player in sight, fireball or loiter?
        else {
            // Any chance of a range attack?
            if (time < self.attack_finished) {
                // Calculate a flat vector to ignore Z axis difference
                // Not convinced a flat vector is good for a melee only state
                // Switched to 3D distance chance so its not so dumb
                self.enemydist = range_distance(self.enemy, FALSE);
                // Don't always stay at absolute range, move closer
                if (random() < 0.15 && self.enemydist > MONAI_RANGEDROLE) {
                    self.attack_sidedeny = time + 1 + random();
                    self.attack_state = AS_STRAIGHT;
                }
                else {
                    // If not blocked, turn and move sideways
                    if (self.attack_sidedeny < time)
                        self.attack_state = AS_SIDESTEP;
                }
            }
            else {
                // Does the monster have a clear shot to the player?
                // sightline can be blocked by other monsters
                if (visblocked(self.enemy)) return;
                // Keep firing rockets when at range
                SUB_AttackFinished (1 + 2*random());
                self.attack_state = AS_MISSILE;
            }
        }
    }
};

/*======================================================================
 Death Guard (Quoth) CheckAttack
======================================================================*/
void() DGuardQCheckAttack =
{
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;
    if (enemy_range == RANGE_FAR) return;

    //----------------------------------------------------------------------
    // Melee attack (Over head smash attack)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEEDGUARDQ)) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
        return;
    }

    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed > 0) {
        if (time < self.attack_finished) return;
        SUB_AttackFinished ((1.4 * random()) + 0.8);
        self.attack_state = AS_MISSILE;
        return;
    }

    //----------------------------------------------------------------------
    // Charge attack (Just outside of melee and before range)
    //----------------------------------------------------------------------
    if (self.enemydist > MONAI_CHARGEDGARDQ1 &&
        self.enemydist < MONAI_CHARGEDGARDQ2) {
        // Check for a random chance to break out from charging
        // and do a quick range attack instead
        if (time > self.attack_finished && random() < 0.2) {
            SUB_AttackFinished ((1.4 * random()) + 0.8);
            self.th_missile ();
        }
        else self.th_charge ();
        return;
    }

    //----------------------------------------------------------------------
    // Range attack - Fireball
    //----------------------------------------------------------------------
    if (self.enemydist > MONAI_RANGEDGARDQ) {
        if (time < self.attack_finished) return;
        if (random() < 0.5) {
            SUB_AttackFinished ((1.4 * random()) + 0.8);
            self.th_missile ();
        }
    }
};

/*======================================================================
 DSergeantCheckAttack
======================================================================*/
void() DSergeantCheckAttack =
{
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    //----------------------------------------------------------------------
    // Melee attack
    // enemy_range is checked before this function (ai_run - ai.qc)
    // If the monster is within melee range they instantly attack
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEEFRONT)) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        // If the sightline between self and player blocked by anything, keep moving
        if (!visxray(self.enemy, self.attack_offset, '0 0 10', FALSE)) return;
        self.attack_state = AS_MISSILE;
        return;
    }

    // If range blocked do charging instead
    if (time < self.attack_finished) {
        //----------------------------------------------------------------------
        // Charge attack
        // Player within certain range, height and charging not blocked?
        //----------------------------------------------------------------------
        self.height = fabs(self.origin_z - self.enemy.origin_z);
        if (ai_checkmelee(MONAI_CHARGEFLAIL) && self.height < MONAI_CHARGEZAXIS
                && self.attack_timer < time) {
            // If attack timer not active, bump up with a random amount
            if (self.attack_finished < time) SUB_AttackFinished (random());
            self.th_charge ();
            return;
        }
    }
    else {
        //----------------------------------------------------------------------
        // Range attack - Homing missile
        //----------------------------------------------------------------------
        // If the sightline between self and player blocked
        // Allow for monsters to be hit (infighting rules!)
        if (!visxray(self.enemy, self.attack_offset, '0 0 10', FALSE)) return;
        self.attack_state = AS_MISSILE;
    }
};

/*======================================================================
 DLordCheckAttack
======================================================================*/
void() DLordCheckAttack =
{
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    //----------------------------------------------------------------------
    // Melee attack
    // enemy_range is checked before this function (ai_run - ai.qc)
    // If the monster is within melee range they instantly attack
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEEFRONT)) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
        return;
    }

    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        // If the sightline between self and player blocked by anything, keep moving
        if (!visxray(self.enemy, self.attack_offset, '0 0 0', FALSE)) return;
        self.attack_state = AS_MISSILE;
        return;
    }

    //----------------------------------------------------------------------
    // Range attack
    //----------------------------------------------------------------------
    if (time < self.attack_finished) return;
    // If the sightline between self and player blocked by anything, keep moving
    if (!visxray(self.enemy, self.attack_offset, '0 0 0', FALSE)) return;

    // Strong lightning attack (melee again)
    if (self.enemydist < MONAI_RANGEDLORD && random() < 0.4) {
        SUB_AttackFinished (3 + random()*2);
        self.attack_state = AS_MELEE;
        return;
    }

    // Long volley of spike balls (NG damage)
    if (self.enemydist >= MONAI_RANGEDLORD && random() < 0.6) {
        SUB_AttackFinished (5 + random());
        self.attack_state = AS_MISSILE;
        return;
    }
};

/*======================================================================
 DFuryCheckAttack
======================================================================*/
void() DFuryCheckAttack =
{
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    //----------------------------------------------------------------------
    // Melee attack
    // enemy_range is checked before this function (ai_run - ai.qc)
    // If the monster is within melee range they instantly attack
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEEFRONT)) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        // If the sightline between self and player blocked by anything, keep moving
        if (!visxray(self.enemy, self.attack_offset, '0 0 0', FALSE)) return;
        self.attack_state = AS_MISSILE;
        return;
    }
    //----------------------------------------------------------------------
    // is the enemy close enough for a double sword slice attack?
    //----------------------------------------------------------------------
    if (self.enemydist < MONAI_JUMPFURYNEAR) {
        self.attack_state = AS_MELEE;
        self.th_slide ();
        return;
    }
    //----------------------------------------------------------------------
    // Mid Range attack (JUMP)
    //----------------------------------------------------------------------
    // Jumped recently, facing right direction and not blocked?
    if ( self.jump_flag < time && infront(self.enemy) && !visblocked(self.enemy) ) {
        // Check for enemy above? (z axis)
        if (self.enemy.origin_z <= self.origin_z) {
            // Is the fury knight within the right range?
            if (self.enemydist > MONAI_JUMPFURYNEAR &&
                self.enemydist < MONAI_JUMPFURYFAR) {
                // Block any range attacks for a while
                SUB_AttackFinished (random());
                self.jumptouch = world;        // Reset last object touched
                self.count = 0;                // Number of times jumped
                if (random() < 0.65) self.th_jump ();
                else self.th_charge ();
                return;
            }
        }
    }
    //----------------------------------------------------------------------
    // Range attack
    // The attack chance percentages are constant across skill levels
    //----------------------------------------------------------------------
    if (time < self.attack_finished) return;
    // If the sightline between self and player blocked by anything, keep moving
    if (!visxray(self.enemy, self.attack_offset, '0 0 0', FALSE)) return;

    if (enemy_range == RANGE_MELEE) self.attack_chance = 0.9;        // range < 120 map units
    else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.4;    // range < 500 map units
    // If jump ability blocked, be more aggressive with range
    else if (enemy_range > RANGE_NEAR && self.jump_flag == LARGE_TIMER) self.attack_chance = 0.4;
    else if (enemy_range == RANGE_MID) self.attack_chance = 0.05;    // range < 1000 map units
    else self.attack_chance = 0;
    if (random () < self.attack_chance) self.attack_state = AS_MISSILE;
};

/*======================================================================
 DCrossCheckAttack
======================================================================*/
void() DCrossCheckAttack =
{
    if (!enemy_vis) return;
    //----------------------------------------------------------------------
    // Melee attack (blunt end of crossbow)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEEKNIGHT)) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, check range attack only
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        // If sight blocked by another monster, do nothing
        if (!visblocked_wide(self.enemy, self.attack_offset, '0 0 0')) return;
        self.attack_state = AS_MISSILE;
        return;
    }
    //----------------------------------------------------------------------
    // Range / missile attack (slow bolts)
    // The range logic is done via two set of animations (hold/slide)
    // Once within a certain range, stay there and snipe at the enemy
    //----------------------------------------------------------------------
    // range < 500 map units
    if (enemy_range == RANGE_NEAR || self.enemymaxdist) {
        // If sight blocked by another monster, slide to the side
        if (!visblocked_wide(self.enemy, self.attack_offset, '0 0 0')) {
            self.attack_state = AS_SLIDING;
            self.th_slide ();
        }
        else {
            // No monster in the way, hold still and start aiming
            self.attack_state = AS_MISSILE;
            self.th_missile ();
        }
    }
    else self.attack_state = AS_STRAIGHT;
};

/*======================================================================
 ZombiekCheckAttack (Has no range attack)
 The player is in view, so decide to jump or melee
======================================================================*/
void() ZombiekCheckAttack =
{
    if (!enemy_vis) return;
    //----------------------------------------------------------------------
    // Melee attack (rusty sword)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEEKNIGHT)) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, randomly do melee attacks
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (random() < MONAI_TURRETMODE) return;
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // Range / missile attack (jumping)
    //----------------------------------------------------------------------
    if (time < self.attack_finished) return;
    self.attack_chance = 0.3 + skill*0.1;
    if (random() < self.attack_chance) {
        // Is the enemy the right distance away and the random chance is correct?
        if (self.enemydist > MONAI_JUMPZKNEAR && self.enemydist < MONAI_JUMPZKFAR) {
            SUB_AttackFinished (2 + random()*2);
            self.jumptouch = world;            // Reset last object touched
            self.count = 0;                    // Number of times jumped
            self.attack_state = AS_JUMP;    // JUMP JUMP JUMP!
        }
    }
};

/*======================================================================
 BoilCheckAttack (Has no range attack)
 The player is in view, so blow up!
======================================================================*/
void() BoilCheckAttack =
{
    if (!enemy_vis) return;
    // Only has one attack, run at player and explode
    // Does one simple range check regardless if turret
    if (self.enemydist < MONAI_MELEEBOIL) self.attack_state = AS_MELEE;
};

/*======================================================================
 SpawnCheckAttack
======================================================================*/
void() SpawnCheckAttack =
{
    // Spawns don't start jumping straight away unless they can directly
    // see the player.     // They slowly crawl around which can make them
    // tricky to plan for an ambush.
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (visblocked(self.enemy)) return;

    //----------------------------------------------------------------------
    // JUMP Melee attack
    // This pretty much a close quarter jump in the face attack!
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEESPAWN)) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, randomly do melee attacks
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (random() < MONAI_TURRETMODE) return;
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // JUMP Range attack
    // The attack chance percentages are constant across skill levels
    //----------------------------------------------------------------------
    if (time < self.attack_finished) return;

    // range < 120 map units
    if (enemy_range == RANGE_MELEE) {
        self.attack_chance = 0.9;
        self.attack_finished = 0;
    }
    // range < 500 map units
    else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.2;
    // range < 1000 map units
    else if (enemy_range == RANGE_MID) self.attack_chance = 0.05;
    else self.attack_chance = 0;

    if (random () < self.attack_chance) {
        SUB_AttackFinished (2*random());
        self.jumptouch = world;            // Reset last object touched
        self.count = 0;                    // Number of times jumped
        self.attack_state = AS_JUMP;    // JUMP JUMP JUMP!
    }
};

/*======================================================================
 DemonCheckAttack
======================================================================*/
void() DemonCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (CLAWS)
    //----------------------------------------------------------------------
    // Check that within range of a claw attack
    if (ai_checkmelee(MONAI_MELEEDEMON)) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, randomly do melee attacks
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (random() < MONAI_TURRETMODE) return;
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (JUMP)
    //----------------------------------------------------------------------
    // Time for another jump?
    if (self.jump_flag < time) {
        // Stop the demon over or under jumping the enemy
        if (self.origin_z + self.mins_z > self.enemy.origin_z + self.enemy.mins_z
        + 0.75 * self.enemy.size_z) return;
        if (self.origin_z + self.maxs_z < self.enemy.origin_z + self.enemy.mins_z
        + 0.25 * self.enemy.size_z) return;

        // Check for closeness, but not long range!
        self.enemydist = range_distance(self.enemy, TRUE);
        if (self.enemydist < MONAI_JUMPDEMONNEAR) return;

        // Check for low ceilings directly above the demon
        traceline(self.origin, self.origin + '0 0 256', TRUE, self);
        self.height = fabs(vlen(trace_endpos - self.origin));
        // Ceiling is too low (looks dumb hitting ceilings)
        if (self.height < MONAI_JUMPDEMONCHECK) return;

        // ** QC code from necros **
        // Move the demon forward 16 units and check if blocked
        self.pos1 = self.origin;
        self.ideal_yaw = vectoyaw(self.enemy.origin - self.pos1);
        // If move forward fails, move back and indicate no jump
        if (!walkmove(self.ideal_yaw, 16)) {
            setorigin(self, self.pos1);
            return;
        }

        setorigin(self, self.pos1);            // walkmove successful, move demon back
        self.jumptouch = world;                // Reset last object touched
        self.count = 0;                        // Number of times jumped
        self.attack_state = AS_JUMP;        // JUMP JUMP JUMP!
    }
};

/*======================================================================
 ScorpionCheckAttack (high damage pincher)
======================================================================*/
void() ScorpionCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (CLAWS)
    //----------------------------------------------------------------------
    if (ai_checkmelee(MONAI_MELEESCORPION)) {
        self.attack_state = AS_MELEE;
        self.th_melee ();
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, do range attacks
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (time < self.attack_finished) return;
        SUB_AttackFinished (2 + random());
        self.attack_state = AS_MISSILE;
        return;
    }

    //----------------------------------------------------------------------
    // Range attack (JUMP/TAIL)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;

    if (self.spawnflags & MON_SCORPION_STINGER) {
        // check for extra wide space to jump
        if (!visblocked_wide(self, self.attack_offset, self.enemy.view_ofs)) return;

        // Time for another jump?
        if (self.jump_flag < time) {
            self.enemydist = range_distance(self.enemy, TRUE);
            if (self.enemydist < MONAI_JUMPSCORPNEAR) return;
            if (self.enemydist > MONAI_JUMPSCORPFAR) return;

            self.jumptouch = world;        // Reset last object touched
            self.count = 0;                // Number of times jumped
            self.attack_state = AS_JUMP;
        }
    }
    else {
        if (enemy_range == RANGE_MELEE) self.attack_chance = 0.9;        // range < 120 map units
        else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.6;    // range < 500 map units
        else if (enemy_range == RANGE_MID) self.attack_chance = 0.2;    // range < 1000 map units
        else self.attack_chance = 0;
        if (random () < self.attack_chance) {
            SUB_AttackFinished (2 + random());
            self.attack_state = AS_MISSILE;
        }
    }
};

/*======================================================================
 DogCheckAttack
======================================================================*/
void()    DogCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (BITE)
    //----------------------------------------------------------------------
    // Check that within range of a bite attack
    if (ai_checkmelee(MONAI_MELEEDOG)) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // If setup to be a turret, randomly do melee attacks
    //----------------------------------------------------------------------
    if (self.movespeed < 0) {
        if (random() < MONAI_TURRETMODE) return;
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (JUMP)
    //----------------------------------------------------------------------
    // Has the dog jumped less than 2 seconds ago?
    if (self.jump_flag < time) {
        // Stop the dog over or under jumping the enemy
        if (self.origin_z + self.mins_z > self.enemy.origin_z + self.enemy.mins_z
        + 0.75 * self.enemy.size_z) return;
        if (self.origin_z + self.maxs_z < self.enemy.origin_z + self.enemy.mins_z
        + 0.25 * self.enemy.size_z) return;

        self.enemydist = range_distance(self.enemy, TRUE);
        if (self.enemydist < MONAI_JUMPDOGNEAR) return;
        if (self.enemydist > MONAI_JUMPDOGFAR) return;

        // Check for enemy above? (z axis)
        if (self.enemy.origin_z > self.origin_z) return;
        self.jumptouch = world;            // Reset last object touched
        self.count = 0;                    // Number of times jumped
        self.attack_state = AS_JUMP;
    }
};

/*======================================================================
 SpiderCheckAttack
======================================================================*/
void() SpiderCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (BITE)
    //----------------------------------------------------------------------
    // Check that within range of a bite attack
    if (ai_checkmelee(MONAI_MELEESPIDER)) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (Large Green Spider = SPIT)
    // Behaves like a wizards; strafe, spit goo
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_SPIDER_LARGE) {
        if (!enemy_vis) return;
        if (time < self.attack_finished) return;

        // Does the monster have a clear shot to the player?
        // sightline can be blocked by other monsters
        if (enemy_range == RANGE_FAR || visblocked(self.enemy)) {
            if (self.attack_state != AS_STRAIGHT) {
                self.attack_state = AS_STRAIGHT;
                self.th_run ();
            }
            return;
        }

        if (enemy_range == RANGE_MELEE) self.attack_chance = 0.9;        // range < 120 map units
        else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.6;    // range < 500 map units
        else if (enemy_range == RANGE_MID) self.attack_chance = 0.2;    // range < 1000 map units
        else self.attack_chance = 0;
        if (random () < self.attack_chance) {
            SUB_AttackFinished (2 + random());
            self.attack_state = AS_MISSILE;
        }
        else if (enemy_range == RANGE_MID) {
            if (self.attack_state != AS_STRAIGHT) {
                self.attack_state = AS_STRAIGHT;
                self.th_run ();
            }
        }
        else {
            if (self.attack_state != AS_SLIDING) {
                self.attack_state = AS_SLIDING;
                self.th_slide ();
            }
        }
    }
    //----------------------------------------------------------------------
    // Range attack (Small Brown Spider = JUMP)
    // Behaves like a dog; run and jump
    //----------------------------------------------------------------------
    else {
        // Is it time to jump?
        if (self.jump_flag < time) {
            // Stop the spider over or under jumping the enemy
            if (self.origin_z + self.mins_z > self.enemy.origin_z + self.enemy.mins_z
            + 0.75 * self.enemy.size_z) return;
            if (self.origin_z + self.maxs_z < self.enemy.origin_z + self.enemy.mins_z
            + 0.25 * self.enemy.size_z) return;

            self.enemydist = range_distance(self.enemy, TRUE);
            if (self.enemydist < MONAI_JUMPSPIDERNEAR) return;
            if (self.enemydist > MONAI_JUMPSPIDERFAR) return;
            // Check for enemy above? (z axis)
            if (self.enemy.origin_z > self.origin_z) return;
            self.jumptouch = world;        // Reset last object touched
            self.count = 0;                // Number of times jumped
            self.attack_state = AS_JUMP;
        }
    }
};

/*======================================================================
 ElfCheckAttack
======================================================================*/
void() ElfCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (CANE)
    //----------------------------------------------------------------------
    // Check that within range of a cane attack
    if (ai_checkmelee(MONAI_MELEEELF)) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (Elf 2 (Black pants) = Magic)
    // Behaves like a wizards; strafe, spit goo
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_ELF_MAGIC) {
        if (!enemy_vis) return;
        if (time < self.attack_finished) return;

        // Does the monster have a clear shot to the player?
        // sightline can be blocked by other monsters
        if (enemy_range == RANGE_FAR || visblocked(self.enemy)) {
            if (self.attack_state != AS_STRAIGHT) {
                self.attack_state = AS_STRAIGHT;
                self.th_run ();
            }
            return;
        }

        if (enemy_range == RANGE_MELEE) self.attack_chance = 0.9;        // range < 120 map units
        else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.6;    // range < 500 map units
        else if (enemy_range == RANGE_MID) self.attack_chance = 0.2;    // range < 1000 map units
        else self.attack_chance = 0;
        if (random () < self.attack_chance) {
            SUB_AttackFinished (2 + random());
            self.attack_state = AS_MISSILE;
        }
        else if (enemy_range == RANGE_MID) {
            if (self.attack_state != AS_STRAIGHT) {
                self.attack_state = AS_STRAIGHT;
                self.th_run ();
            }
        }
        else {
            if (self.attack_state != AS_SLIDING) {
                self.attack_state = AS_SLIDING;
                self.th_slide ();
            }
        }
    }
    //----------------------------------------------------------------------
    // Range attack (Elf 1 (green/red pants) = JUMP)
    // Behaves like a dog; run and jump
    //----------------------------------------------------------------------
    else {
        // Is it time to jump?
        if (self.jump_flag < time) {
            // Stop the elf over or under jumping the enemy
            if (self.origin_z + self.mins_z > self.enemy.origin_z + self.enemy.mins_z
            + 0.75 * self.enemy.size_z) return;
            if (self.origin_z + self.maxs_z < self.enemy.origin_z + self.enemy.mins_z
            + 0.25 * self.enemy.size_z) return;

            self.enemydist = range_distance(self.enemy, TRUE);
            if (self.enemydist < MONAI_JUMPELFNEAR) return;
            if (self.enemydist > MONAI_JUMPELFFAR) return;
            // Check for enemy above? (z axis)
            if (self.enemy.origin_z > self.origin_z) return;
            self.jumptouch = world;        // Reset last object touched
            self.count = 0;                // Number of times jumped
            self.attack_state = AS_JUMP;
        }
    }
};

/*======================================================================
 VorelingCheckAttack
======================================================================*/
void() VorelingCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (BITE)
    //----------------------------------------------------------------------
    // Check that within range of a bite attack
    if (ai_checkmelee(MONAI_MELEEVORELING)) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (Large Purple Voreling = SPIT)
    // Behaves like a wizards; strafe, spit goo
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_VORELING_LARGE) {
        if (!enemy_vis) return;
        if (time < self.attack_finished) return;

        // Does the monster have a clear shot to the player?
        // sightline can be blocked by other monsters
        if (enemy_range == RANGE_FAR || visblocked(self.enemy)) {
            if (self.attack_state != AS_STRAIGHT) {
                self.attack_state = AS_STRAIGHT;
                self.th_run ();
            }
            return;
        }

        if (enemy_range == RANGE_MELEE) self.attack_chance = 0.9;        // range < 120 map units
        else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.6;    // range < 500 map units
        else if (enemy_range == RANGE_MID) self.attack_chance = 0.2;    // range < 1000 map units
        else self.attack_chance = 0;
        if (random () < self.attack_chance) {
            SUB_AttackFinished (2 + random());
            self.attack_state = AS_MISSILE;
        }
        else if (enemy_range == RANGE_MID) {
            if (self.attack_state != AS_STRAIGHT) {
                self.attack_state = AS_STRAIGHT;
                self.th_run ();
            }
        }
        else {
            if (self.attack_state != AS_SLIDING) {
                self.attack_state = AS_SLIDING;
                self.th_slide ();
            }
        }
    }
    //----------------------------------------------------------------------
    // Range attack (Small White Voreling = JUMP)
    // Behaves like a dog; run and jump
    //----------------------------------------------------------------------
    else {
        // Is it time to jump?
        if (self.jump_flag < time) {
            // Stop the voreling over or under jumping the enemy
            if (self.origin_z + self.mins_z > self.enemy.origin_z + self.enemy.mins_z
            + 0.75 * self.enemy.size_z) return;
            if (self.origin_z + self.maxs_z < self.enemy.origin_z + self.enemy.mins_z
            + 0.25 * self.enemy.size_z) return;

            // Too close/far?
            self.enemydist = range_distance(self.enemy, TRUE);
            if (self.enemydist < MONAI_JUMPVORELINGNEAR) return;
            if (self.enemydist > MONAI_JUMPVORELINGFAR) return;
            // Check for enemy above? (z axis)
            if (self.enemy.origin_z > self.origin_z) return;
            self.jumptouch = world;            // Reset last object touched
            self.count = 0;                    // Number of times jumped
            self.attack_state = AS_JUMP;
        }
    }
};

/*======================================================================
 SwamplingCheckAttack
======================================================================*/
void() SwamplingCheckAttack =
{
    //----------------------------------------------------------------------
    // Melee attack (BITE)
    //----------------------------------------------------------------------
    // Check that within range of a bite attack
    if (ai_checkmelee(MONAI_MELEESWAMPLING)) {
        self.attack_state = AS_MELEE;
        return;
    }
    //----------------------------------------------------------------------
    // Range attack (Large Green Swampling = SPIT)
    // Behaves like a wizards; strafe, spit goo
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_SWAMPLING_LARGE) {
        if (!enemy_vis) return;
        if (time < self.attack_finished) return;

        // Does the monster have a clear shot to the player?
        // sightline can be blocked by other monsters
        if (enemy_range == RANGE_FAR || visblocked(self.enemy)) {
            if (self.attack_state != AS_STRAIGHT) {
                self.attack_state = AS_STRAIGHT;
                self.th_run ();
            }
            return;
        }

        if (enemy_range == RANGE_MELEE) self.attack_chance = 0.9;        // range < 120 map units
        else if (enemy_range == RANGE_NEAR) self.attack_chance = 0.6;    // range < 500 map units
        else if (enemy_range == RANGE_MID) self.attack_chance = 0.2;    // range < 1000 map units
        else self.attack_chance = 0;
        if (random () < self.attack_chance) {
            SUB_AttackFinished (2 + random());
            self.attack_state = AS_MISSILE;
        }
        else if (enemy_range == RANGE_MID) {
            if (self.attack_state != AS_STRAIGHT) {
                self.attack_state = AS_STRAIGHT;
                self.th_run ();
            }
        }
        else {
            if (self.attack_state != AS_SLIDING) {
                self.attack_state = AS_SLIDING;
                self.th_slide ();
            }
        }
    }
    //----------------------------------------------------------------------
    // Range attack (Small Light Green Swampling = JUMP)
    // Behaves like a dog; run and jump
    //----------------------------------------------------------------------
    else {
        // Is it time to jump?
        if (self.jump_flag < time) {
            // Stop the swampling over or under jumping the enemy
            if (self.origin_z + self.mins_z > self.enemy.origin_z + self.enemy.mins_z
            + 0.75 * self.enemy.size_z) return;
            if (self.origin_z + self.maxs_z < self.enemy.origin_z + self.enemy.mins_z
            + 0.25 * self.enemy.size_z) return;

            // Too close/far?
            self.enemydist = range_distance(self.enemy, TRUE);
            if (self.enemydist < MONAI_JUMPSWAMPLINGNEAR) return;
            if (self.enemydist > MONAI_JUMPSWAMPLINGFAR) return;
            // Check for enemy above? (z axis)
            if (self.enemy.origin_z > self.origin_z) return;
            self.jumptouch = world;            // Reset last object touched
            self.count = 0;                    // Number of times jumped
            self.attack_state = AS_JUMP;
        }
    }
};

/*======================================================================
 Check if the AI is blocked by a breakable object in the way
======================================================================*/
void() CheckBlockedBreakable =
{
    local vector spot1, spot2, brkorg;
    local float brklen;

    // Still busy attacking? do nothing
    if (time < self.attack_finished) return;

    // FIXME : there needs to be check on the breakable health
    // otherwise the monster will be stuck constantly attacking the breakable

    // Check for breakables from middle of models (not head height)
    spot1 = self.origin;
    spot2 = self.enemy.origin;
    traceline (spot1, spot2, TRUE, self);    // see through other monsters

    if (trace_ent.classtype != CT_FUNCBREAK) {
        // Check for breakables from top of models
        spot1 = self.origin + self.view_ofs;
        spot2 = self.enemy.origin + self.enemy.view_ofs;
        traceline (spot1, spot2, TRUE, self);    // see through other monsters
    }

    if (ai_foundbreakable(self, trace_ent,FALSE)) {
        // Find origin of breakable, check for bmodel 0,0,0 origins
        if (trace_ent.bsporigin) brkorg = bmodel_origin(trace_ent);
        else brkorg = trace_ent.origin;
        brklen = fabs(vlen(self.origin - brkorg));
        SUB_AttackFinished (1+random());

        // Double check a melee range has been setup
        if (self.th_melee && !self.meleerange)
            self.meleerange = MONAI_MELEEKNIGHT;

        // Check breakable within melee/missile range?
        if (brklen < self.meleerange && self.th_melee) self.attack_state = AS_MELEE;
        else if (self.th_missile) self.attack_state = AS_MISSILE;
    }
};
