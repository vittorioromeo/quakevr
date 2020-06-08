/*======================================================================
 PLAYER ONLY WEAPON FUNCTIONS

void() W_Precache; - moved to world.qc
float() crandom; - moved to subs.qc
vector() wall_velocity - merged into spawn_touchblood (only used once)
void(vector org, vector vel) SpawnChunk - Not used anywhere, removed

 W_SetCurrentAmmo   - Update HUD icons based on self.weapon value
 W_BestWeapon       - returns best weapon LG > SNG > SSG > NG > SG > Axe
                      interesting that the function does not return GL/RL
 W_CheckNoAmmo      - Ammo check for attack function
 W_Attack           - Main function that fire weapons (player animations)
                      Will automatically wakeup any nearby monsters
 W_ChangeWeapon     - Change weapon based on availabilty and ammo
 CycleWeaponCommand - Move forward or backward through active weapons
 CycleWeaponReverseCommand
 W_WeaponFrame      - Called every frame, catches impulse event
 SuperDamageSound   - Plays quad damage sound

======================================================================*/
void() player_run;                // Start of run cycle
void(entity targ) remoteplayer_run;
void() player_axe1;                // Different Axe animations
void() player_axeb1;
void() player_axec1;
void() player_axed1;
void() player_axee1;

void() player_axeold1;                // Different Axe animations for original axe
void() player_axeoldb1;
void() player_axeoldc1;
void() player_axeoldd1;

void() player_sg1;                // Shotgun
void() player_supersg1;            // Super Shotgun
void() player_nail1;            // Nailgun
void() player_snail1;            // Super Nailgun
void() player_rocket1;            // Rocket Launcher
void() player_grenade1;            // Grenade Launcher
void() player_multi_rocket1;        // Multi-Rocket Launcher
void() player_multi_grenade1;        // Multi-Grenade Launcher
void() player_light1;            // Thunderbolt
void() player_plasma1;            // Plasma Gun
void() player_plasmalight1;        // Rogue Plasma Gun
void() player_ssgreset;            // Reset weaponframe
void() player_sgreset;
void() player_grenreset;
void() player_rockreset;
void() player_crossbow1;
void() player_crossbow2;
void() player_crossbow3;
void() player_crossbow4;
void() player_crossbow5;
void() player_crossbow6;
void() player_laser1;
void() player_hammer1;
void() player_mjolnir1;
void() player_prox_grenade1;
void() player_wand1;
void() player_grapple1;
void() player_grapple3;
void() player_chainlight1;
void() player_blast1;
void() player_saw1;

void() W_FireSparks;
void(entity ent) ReflectSound;

/*======================================================================
 Weapon ammo run out and switching to next best weapon
======================================================================*/
void(float wait_time) forceweaponswitch = {
    local float nextweap;

    if (self.flags & FL_MONSTER) return;
    
    // has the player run out of ammo? work out next best weapon
    nextweap = W_BestWeapon (self);
    // Does the player need to switch weapons?
    if (self.weapon != nextweap) {
        self.weapon = nextweap;
        W_SetCurrentAmmo (self);
    }
};

/*======================================================================
 Hipnotic weapon functions
======================================================================*/
void (float stat) HIP_FireLaser = {
   local vector org;
   local vector dir;
   local vector out;
   local float ofs;
   local float aofs;
    local   float   glow;

   if ( !self.button0 ) {
      player_run ();
      return ;
   }
   if ((self.weapon == IT2_LASER_CANNON) && (self.ammo_cells < TRUE) ) {
      self.weapon = W_BestWeapon (self);
      W_SetCurrentAmmo (self);
      return ;
   }

   if ((self.weapon == IT_SHOTGUN) && (self.ammo_shells < TRUE) ) {
      self.weapon = W_BestWeapon (self);
      W_SetCurrentAmmo (self);
      return ;
   }

   self.effects = (self.effects | EF_MUZZLEFLASH);
   makevectors (self.v_angle);
   ofs = MOVETYPE_TOSS;
   out = v_forward;
   out_z = FALSE;
   out = normalize (out);
   org = ((self.origin + ((STAT_TOTALMONSTERS - ofs) * v_up)) + (STAT_TOTALMONSTERS * out));
   dir = aim (self,1000.000);
   aofs = (ofs * 0.707);
   if ( (stat == FALSE) ) {
      self.ammo_cells = (self.ammo_cells - TRUE);
      self.currentammo = (self.currentammo - TRUE);
      org = (org + (aofs * v_right));
      org = (org - (aofs * v_up));
      HIP_LaunchLaser (org,dir,FALSE);
      org = (org - ((FL_SWIM * aofs) * v_right));
      HIP_LaunchLaser (org,dir,FALSE);
   } else {
      if ( (stat == TRUE) ) {
             self.ammo_cells = (self.ammo_cells - TRUE);
             self.currentammo = (self.currentammo - TRUE);
             org = (org + (ofs * v_up));
             if ( (random () < 0.100) ) {
                HIP_LaunchLaser (org,dir,TRUE);
                newmis.dmg = 25.000;
             } else {
                HIP_LaunchLaser (org,dir,FALSE);
             }
        } else {
            // Using the modified shotgun-turned-blaster model...
            org = self.origin + '0 0 12';
            if (stat == 2)
                if (self.tracer_time <= time)
                    {glow = TRUE; self.tracer_time = time + 0.5;}

            self.ammo_shells = (self.ammo_shells - TRUE);  // blaster uses shells??  well, I guess that's fine whatevs
            self.currentammo = (self.currentammo - TRUE);
            HIP_LaunchLaser (org, dir, glow);
            newmis.volume = 0;      // No random chance of removal on impact.
            // Note to self:  Spread gun looks bad unless v_model is wide.
        }
    }
   self.punchangle_x = CONTENT_EMPTY;
};

void (float stat) W_FireLaser = { HIP_FireLaser(stat); }

void () HIP_FireMjolnir = {
   local vector org;
   local float damage;
   local vector source;

   self.attack_finished = (time + 0.400);
   source = (self.origin + '0.000 0.000 16.000');
   makevectors (self.v_angle);
   traceline (source,(source + (v_forward * RANGE_PLAYAXE)),FALSE,self);
   if ( ((trace_fraction == TRUE) && (self.ammo_cells >= 15.000)) ) {
      source = (source + (v_forward * RANGE_PLAYAXE));
      traceline (source,(source - (v_up * 50.000)),FALSE,self);
      if ( ((trace_fraction > 0.300) && (trace_fraction < TRUE)) ) {
         HIP_FireMjolnirLightning ();
         self.attack_finished = (time + 1.000);
         return ;
      }
   }
   org = (trace_endpos - (v_forward * 4));
   if ( trace_ent.takedamage ) {
      damage = 50.000;
      if ( (trace_ent.classgroup == CG_ZOMBIE) )
         damage = 70.000;

      trace_ent.axhitme = TRUE;
      SpawnBlood (trace_ent, org,v_forward,damage);
      T_Damage (trace_ent,self,self,damage, DAMARMOR);
   } else {
      if ( (trace_fraction != TRUE) ) {
         sound (self,CHAN_WEAPON,"hipweap/mjoltink.wav",TRUE,ATTN_NORM);
         WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
         WriteByte (MSG_BROADCAST,TE_GUNSHOT);
         WriteCoord (MSG_BROADCAST,org_x);
         WriteCoord (MSG_BROADCAST,org_y);
         WriteCoord (MSG_BROADCAST,org_z);
      } else {
         sound (self,CHAN_WEAPON,"knight/sword1.wav",TRUE,ATTN_NORM);
      }
   }
   self.attack_finished = (time + 0.400);
};

void () W_FireMjolnir = { HIP_FireMjolnir(); };

void () W_FireProximityGrenade = {
   local entity missile;

   if (self.attack_finished > time) return;
   if (self.ammo_rockets < 1) { forceweaponswitch(0.2); return;}
   NumProximityGrenades = (NumProximityGrenades + TRUE);
   self.ammo_rockets = (self.ammo_rockets - TRUE);
   self.currentammo = (self.currentammo - TRUE);
   sound (self,CHAN_WEAPON,"hipweap/proxbomb.wav",TRUE,ATTN_NORM);
   self.punchangle_x = CONTENT_SOLID;
   missile = spawn ();
   missile.owner = self;
   missile.lastvictim = self;
   missile.movetype = MOVETYPE_TOSS;
   missile.solid = SOLID_BBOX;
   missile.classname = "proximity_grenade";
   missile.takedamage = DAMAGE_NO;
   missile.health = MOVETYPE_FLY;
   missile.state = FALSE;
   makevectors (self.v_angle);
   if ( self.v_angle_x ) {
      missile.velocity = ((((v_forward * 600.000) + (v_up * 200)) + ((crandom () * v_right) * MOVETYPE_BOUNCE)) + ((crandom () * v_up) * MOVETYPE_BOUNCE));
   } else {
      missile.velocity = aim (self,10000.000);
      missile.velocity = (missile.velocity * 600.000);
      missile.velocity_z = 200;
   }
   missile.avelocity = '100.000 600.000 100.000';
   missile.angles = vectoangles (missile.velocity);
   missile.touch = ProximityGrenadeTouch;
   missile.nextthink = (time + FL_SWIM);
   missile.delay = ((time + 15.000) + (MOVETYPE_BOUNCE * random ()));
   missile.think = ProximityBomb;
   missile.th_die = ProximityGrenadeExplode;
   setmodel (missile,"progs/hipnotic/proxbomb.mdl");
   setorigin (missile,self.origin);
   setsize (missile,'-1.000 -1.000 -1.000','1.000 1.000 1.000');
   self.attack_finished = (time + 0.600);
};

/*======================================================================
 W_FireAxe
======================================================================*/
void() W_FireAxe = {
    local vector source, org, vec;
    local float src_dist, dotprod, axedmg;
    local entity onflr, tself, tother;

    makevectors (self.v_angle);                // Player forward angle
    source = self.origin + '0 0 16';        // Move source point up body

    if (self.flags & FL_GODMODE && self.flags & FL_NOTARGET) {
        traceline (source, source + (v_forward* RANGE_PLAYAXE), FALSE, self);
        if (trace_fraction < 1.0) {
            if (trace_ent.flags & FL_MONSTER && trace_ent.health > 0) {
                dprint("\n\b[DEBUG]\b system active, changing monster\n");
                trace_ent.debuglvl = 1 - trace_ent.debuglvl;
            }
        }
        return;
    }         

    if (self.moditems & IT_UPGRADE_AXE) {
        // See if there are any bodies lying around
        onflr = find(world, bodyonflr, MON_ONFLR);
        while (onflr) {
            src_dist = vlen(source - onflr.origin);
            if (src_dist < RANGE_CHOPAXE) {            // Is the body close to the player?
                org = onflr.origin - '0 0 16';        // move origin close to floor
                makevectors (self.v_angle);            // Calculate viewing angle
                vec = normalize (org - self.origin);
                dotprod = vec * v_forward;
                if (dotprod > 0.6) {                // Is the body infront of the player
                    onflr.origin = org;                // Move gib closer to floor
                    onflr.bodyonflr = string_null;    // no more body gibbing
                    tself = self; tother = other;    // save self/other
                    self = onflr; other = tself;    // switch around
                    if (onflr.classgroup == CG_ZOMBIE) {
                        self.health = 0;
                        Killed(onflr, tself);
                    }
                    else {
                        self.max_health = MON_NOGIBVELOCITY; // use minimal velocity
                        monster_ThrowGib();            // throw parts
                    }
                    
                    self = tself; other = tother;    // switch back
                    SpawnBlood (onflr, onflr.origin, '0 0 50', axedmg*4);
                    return;                                                                // Finished
                }
            }
            // See if there are anymore bodies close by
            onflr = find(onflr,bodyonflr, MON_ONFLR);
        }
    }

    // Trace forward and see if the axe has hit anything?    
    traceline (source, source + (v_forward* RANGE_PLAYAXE), FALSE, self);
    if (trace_fraction == 1.0) return;    // No contact, no hit
    
    org = trace_endpos - v_forward*4;    // Back 4 units to spawn blood

    if (trace_ent.takedamage) {
        trace_ent.axhitme = 1;            // special DM/Coop axe hit sound
        if (self.moditems & IT_UPGRADE_AXE) axedmg = DAMAGE_PLAYAXE2;
        else axedmg = DAMAGE_PLAYAXE1;
        
        // Spamn blood always up so player can see it
        SpawnBlood (trace_ent, org, '0 0 1', axedmg);
        T_Damage (trace_ent, self, self, axedmg, DAMARMOR);
        
        // Check with impact sound to play
        if (trace_ent.flags & FL_MONSTER) {
            if (random() < 0.5) sound (self, CHAN_WEAPON, GIB_SOUND_HEAVY, 1, ATTN_NORM);
            else sound (self, CHAN_WEAPON, GIB_SOUND_HEAVY2, 1, ATTN_NORM);
        }
        else if (trace_ent.classtype == CT_FUNCBREAK) {
            if (trace_ent.style == BTYPE_WOOD) 
                sound (self, CHAN_WEAPON, SOUND_AXE_WOOD, 1, ATTN_NORM);
            else if (trace_ent.style == BTYPE_GLASS) 
                sound (self, CHAN_WEAPON, SOUND_AXE_GLASS, 1, ATTN_NORM);
            else if (trace_ent.style == BTYPE_METAL) 
                sound (self, CHAN_WEAPON, SOUND_AXE_METAL, 1, ATTN_NORM);
            else
                sound (self, CHAN_WEAPON, SOUND_AXE_STONE, 1, ATTN_NORM);
        }
    }
    //----------------------------------------------------------------------
    // WORLD OBJECT : Target does not bleed, play stone hitting sound
    //----------------------------------------------------------------------
    else {
        sound (self, CHAN_WEAPON, SOUND_AXE_STONE, 1, ATTN_NORM);
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_GUNSHOT);
        WriteCoord (MSG_BROADCAST, org_x);
        WriteCoord (MSG_BROADCAST, org_y);
        WriteCoord (MSG_BROADCAST, org_z);
    }
};

/*======================================================================
 OLD STYLE BULLET SYSTEM (HITSCAN)
======================================================================*/

//----------------------------------------------------------------------
void(vector org, float marker_time) MarkAttack = {
    newmis = spawn();
    newmis.classtype = CT_DEVMARKER;
    newmis.movetype = MOVETYPE_NONE;
    newmis.solid = SOLID_NOT;
    setmodel(newmis, MODEL_BROKEN);
    newmis.skin = rint(random()*7);
    setorigin(newmis, org);
    setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
    newmis.think = SUB_Remove;
    newmis.nextthink = time + marker_time;
};

//----------------------------------------------------------------------
void(float damage, vector dir) TraceAttack = {
    local vector vel, org;
    
    vel = normalize(dir + v_up*crandom() + v_right*crandom());
    vel = vel + 2*trace_plane_normal;
    vel = vel * 200;

    org = trace_endpos - dir*4;

    // Check for sky content? skies don't impact or bleed
    if (!check_skycontent(org)) {
        if (trace_ent.takedamage) {
            // Show bullet resistance as small blood+gunshot+smoke
            if (trace_ent.resist_shells > 0) Resist_Shells(trace_ent, org, vel, damage);
            else {
                // Hitting monsters does twice the amount of blood effects
                if (trace_ent.flags & FL_MONSTER) SpawnBlood (trace_ent, org, vel*0.2, damage*2);
                else SpawnBlood (trace_ent, org, vel*0.2, damage);
            }
            // Keep adding up the damage
            AddMultiDamage (trace_ent, damage);
            
            // Check for target dummy (manually create marker)
            if (trace_ent.classtype == CT_TARGETDUMMY) 
                MarkAttack(org, trace_ent.wait);
        }
        else {
            // Hit something that does not bleed (often world)
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, TE_GUNSHOT);
            WriteCoord (MSG_BROADCAST, org_x);
            WriteCoord (MSG_BROADCAST, org_y);
            WriteCoord (MSG_BROADCAST, org_z);
        }
    }
};

//----------------------------------------------------------------------
void(float shotcount, vector spread) FireBullets = {
    local    vector direction, src, dir;
    
    makevectors(self.v_angle);

    if (self.flags & FL_CLIENT) {
        // Auto aim assist (builtin function 44)
        if ((autoaim_cvar < 1) && self.classtype != CT_MONGREMLIN) dir = aim(self, SPEED_PLAYAIM);
        // Straight line forward where crosshair is pointing
        else dir = normalize(v_forward * SPEED_PLAYAIM);
    }
    else {
        // A monster attacking a monster will not dodge
        if (self.enemy.flags & FL_MONSTER)
            dir = normalize(self.enemy.origin - self.origin);
        else {
            // fire somewhat behind the player
            // so a dodging player is harder to hit
            dir = self.enemy.origin - self.enemy.velocity*0.2;
            dir = normalize (dir - self.origin);
        }
    }

    src = self.origin + v_forward*10;
    src_z = self.absmin_z + self.size_z * 0.7;

    ClearMultiDamage ();
    while (shotcount > 0) {
        direction = dir + crandom()*spread_x*v_right + crandom()*spread_y*v_up;
        traceline (src, src + direction*2048, FALSE, self);
        if (trace_fraction != 1.0) TraceAttack (DAMAGE_SHELL, direction);
        shotcount = shotcount - 1;
    }
    ApplyMultiDamage ();
};

/*======================================================================
W_FireShotgun
======================================================================*/
void() W_FireShotgun = {
    local vector spread_pat;
    
    // Ran out of ammo, switch to next best weapon
    if (self.attack_finished > time) return;
    if (self.ammo_shells < 1) { forceweaponswitch(0.2); return;}
    
    // Change weapon sound if using Projectile Pellets
    // IMPORTANT - default for Projectile Pellets is ON = 0
    if (!query_configflag(SVR_SHOTGPROJ))
        sound (self ,CHAN_WEAPON, "weapons/sg2.wav", 1, ATTN_NORM);
    else
        sound (self ,CHAN_WEAPON, "weapons/sg1.wav", 1, ATTN_NORM);
    
    self.effects = self.effects | EF_MUZZLEFLASH;

    if (self.perms & POW_SHOTTYMAN) {
        self.attack_finished = time + 0.3;
    } else {
        self.attack_finished = time + 0.5;
    }
    self.punchangle_x = -2;
    if (self.classtype != CT_MONGREMLIN)
        player_sgreset();            // reset weaponframe
    
    self.currentammo = self.ammo_shells = self.ammo_shells - 1;

    if (self.moditems & IT_ARTSHARP) spread_pat = SPREAD_SG2;
    else spread_pat = SPREAD_SG;
    
    // Choose between a projectile or hitscan system
    if (query_configflag(SVR_SHOTGPROJ))
        FireBullets (QUANTITY_SG, spread_pat);
    else {
        Launch_Shells(QUANTITY_SGP, spread_pat, CT_PROJ_SG);
        Launch_ShellCasing(1);    // Shell casings
    }
};

/*======================================================================
W_FireSuperShotgun
======================================================================*/
void() W_FireSuperShotgun = {
    local float ssg_qty, shell_qty;
    local vector spread_pat;
    
    // Ran out of ammo, switch to next best weapon
    if (self.attack_finished > time) return;
    if (self.ammo_shells < 2 && self.classtype != CT_MONGREMLIN) { forceweaponswitch(0.2); return;}

    makevectors (self.v_angle);        // Special view angle for the player

    // if shot in the air - do Newton's 3rd law. (from zerstorer)
    if (!(self.flags & FL_ONGROUND))
        self.velocity = self.velocity - (v_forward * 35);
    
    // Change weapon sound if using Shotgun Upgrade or Projectile Pellets
    // IMPORTANT - default for Projectile Pellets is ON = 0
    if (self.moditems & IT_UPGRADE_SSG || !query_configflag(SVR_SHOTGPROJ)) {
        sound (self ,CHAN_WEAPON, "weapons/ssg2.wav", 1, ATTN_NORM);
    } else{ 
        sound (self ,CHAN_WEAPON, "weapons/ssg1.wav", 1, ATTN_NORM);
    }

    self.effects = self.effects | EF_MUZZLEFLASH;
    if (self.perms & POW_SHOTTYMAN) {
        self.attack_finished = time + 0.5;
    } else {
        self.attack_finished = time + 0.7;
    }
    self.punchangle_x = -4;
    if (self.classtype != CT_MONGREMLIN)
        player_ssgreset();            // reset weaponframe
    shell_qty = 2;                // Standard shell casings quantity

    // If the player has the Widowmaker, setup triple shot
    if (self.moditems & IT_UPGRADE_SSG) {
        // Only got 2 shells in the gun (reduced damage)
        if (self.ammo_shells < 3) {
            self.ammo_shells = 0;
            ssg_qty = QUANTITY_SSG * 1;
        }
        else {
            // 150% damage
            shell_qty = 3;
            self.ammo_shells = self.ammo_shells - 3;
            ssg_qty = QUANTITY_SSG * 1.5;
        }
    }
    else {
        // Default ID SSG damage
        self.ammo_shells = self.ammo_shells - 2;
        ssg_qty = QUANTITY_SSG * 1;
    }

    self.currentammo = self.ammo_shells;
    if (self.moditems & IT_ARTSHARP) spread_pat = SPREAD_SSG2;
    else spread_pat = SPREAD_SSG;

    // Choose between a projectile or hit-scan system
    if (query_configflag(SVR_SHOTGPROJ))
        FireBullets (ssg_qty, spread_pat);    // Hit-scan
    else {
        Launch_Shells(ssg_qty, spread_pat, CT_PROJ_SSG);
        Launch_ShellCasing(shell_qty);
    }
};

/*======================================================================
W_FireSpikes
======================================================================*/
void(float oz, float ox) W_FireSpikes = {
    local vector org, dir;
    
    // If run out of ammo, switch weapons to next best
    if (self.ammo_nails < 1) { forceweaponswitch(0.2); return; }

    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_finished = time + 0.2;
    self.punchangle_x = -2;
    makevectors (self.v_angle);

    // Check for auto aim state
    // Auto aim assist (builtin function 44)
    if ((autoaim_cvar < 1) && self.classtype != CT_MONGREMLIN) dir = aim(self, SPEED_PLAYSPIKE);
    // Straight line forward where crosshair is pointing
    else dir = normalize(v_forward * SPEED_PLAYSPIKE);

    // SNG setup, sound and ammo change
    if (self.ammo_nails > 1 && self.weapon == IT_SUPER_NAILGUN) {
        sound (self, CHAN_WEAPON, "weapons/spike2.wav", 1, ATTN_NORM);
        if (self.perms & POW_DOUBLE) {
            self.currentammo = self.ammo_nails = self.ammo_nails - 1;
        } else {
            self.currentammo = self.ammo_nails = self.ammo_nails - 2;
        }

        //-------------------------------------------------------------
        // Original setup - org = self.origin + '0 0 16';
        // - SNG projectile offset idea by Kinn
        // org = self.origin + dir*14 + v_right*ox;
        // org_z = org_z + oz;
        // The SNG offset idea was hiting far below the crosshair
        // which was causing impact problems.
        // Kept the barrel offset, removed the Z adjustment instead!
        //-------------------------------------------------------------
        org = self.origin + '0 0 16' + v_right*ox;
        launch_projectile (org, dir, CT_PROJ_SNG, SPEED_PLAYSPIKE);
    }
    // NG setup, sound and ammo change
    else {
        sound (self, CHAN_WEAPON, "weapons/rocket1i.wav", 1, ATTN_NORM);
        if (self.perms & POW_DOUBLE) {
            if (self.nailcosthalf) {
                self.currentammo = self.ammo_nails = self.ammo_nails - 1;
                self.nailcosthalf = 0;
            } else {
                self.nailcosthalf = 1;
            }
        } else {
            self.currentammo = self.ammo_nails = self.ammo_nails - 1;
        }
        
        // Original setup - org = self.origin + '0 0 16' + v_right*ox;
        // - NG projectile offset idea by Kinn
        // org = self.origin + '0 0 8' + dir*14 + v_right*ox;
        org = self.origin + '0 0 16' + v_right*ox;
        launch_projectile (org, dir, CT_PROJ_NG, SPEED_PLAYSPIKE);
    }
};

/*======================================================================
W_FireLavaSpikes
======================================================================*/
void(float oz, float ox) W_FireLavaSpikes = {
    local vector org, dir;
    
    // If run out of ammo, switch weapons to next best
    if (self.ammo_lava_nails < 1) { forceweaponswitch(0.2); return; }

    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_finished = time + 0.2;
    self.punchangle_x = -2;
    makevectors (self.v_angle);

    // Check for auto aim state
    // Auto aim assist (builtin function 44)
    if ((autoaim_cvar < 1) && self.classtype != CT_MONGREMLIN)  dir = aim(self, SPEED_PLAYSPIKE);
    // Straight line forward where crosshair is pointing
    else dir = normalize(v_forward * SPEED_PLAYSPIKE);

    // Lava SNG setup, sound and ammo change
    if (self.ammo_lava_nails > 1 && self.weapon == IT_LAVA_SUPER_NAILGUN) {
        sound (self, CHAN_WEAPON, "weapons/spike2.wav", 1, ATTN_NORM);
        if (self.perms & POW_DOUBLE) {
            self.currentammo = self.ammo_lava_nails = self.ammo_lava_nails - 1;
        } else {
            self.currentammo = self.ammo_lava_nails = self.ammo_lava_nails - 2;
        }

        //-------------------------------------------------------------
        // Original setup - org = self.origin + '0 0 16';
        // - SNG projectile offset idea by Kinn
        // org = self.origin + dir*14 + v_right*ox;
        // org_z = org_z + oz;
        // The SNG offset idea was hiting far below the crosshair
        // which was causing impact problems.
        // Kept the barrel offset, removed the Z adjustment instead!
        //-------------------------------------------------------------
        org = self.origin + '0 0 16' + v_right*ox;
        launch_projectile (org, dir, CT_PROJ_LAVASNG, SPEED_PLAYSPIKE);
    }
    // Lava NG setup, sound and ammo change
    else {
        sound (self, CHAN_WEAPON, "weapons/rocket1i.wav", 1, ATTN_NORM);
        if (self.perms & POW_DOUBLE) {
            if (self.nailcosthalf) {
                self.currentammo = self.ammo_lava_nails = self.ammo_lava_nails - 1;
                self.nailcosthalf = 0;
            } else {
                self.nailcosthalf = 1;
            }
        } else {
            self.currentammo = self.ammo_lava_nails = self.ammo_lava_nails - 1;
        }
        
        // Original setup - org = self.origin + '0 0 16' + v_right*ox;
        // - NG projectile offset idea by Kinn
        // org = self.origin + '0 0 8' + dir*14 + v_right*ox;
        org = self.origin + '0 0 16' + v_right*ox;
        launch_projectile (org, dir, CT_PROJ_LAVANG, SPEED_PLAYSPIKE);
    }
};

/*======================================================================
GRENADES
======================================================================*/
void() W_FireGrenade =
{
    local vector dir, avel;
    
    // Ran out of ammo, switch to next best weapon
    if (self.attack_finished > time) return;
    if (self.ammo_rockets < 1) { forceweaponswitch(0.2); return;}

    self.currentammo = self.ammo_rockets = self.ammo_rockets - 1;
    sound (self, CHAN_WEAPON, "weapons/grenade.wav", 1, ATTN_NORM);
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_finished = time + 0.6;
    self.punchangle_x = -2;
    player_grenreset();            // reset weaponframe

    makevectors (self.v_angle);
    // Has the player aimed left/right? then no auto aim assist
    if (self.v_angle_x) {
        dir = v_forward*SPEED_PLAYGRENADE + v_up * ELEV_ZAXIS + crandom()*v_right*10 + crandom()*v_up*10;
    }
    else {
        // Check for auto aim state
        if ((autoaim_cvar < 1) && self.classtype != CT_MONGREMLIN)  dir = aim(self, SPEED_PLAYAIM);
        else dir = normalize(v_forward * SPEED_PLAYGRENADE);
        // Work out default speed and elevation
        dir = dir * SPEED_PLAYGRENADE;
        dir_z = ELEV_ZAXIS;
    }
    avel = vecrand(100,200,FALSE);
    Launch_Grenade(self.origin, dir, avel, CT_PROJ_GL);
};

/*======================================================================
 ROCKETS
======================================================================*/
void() W_FireRocket =
{
    local vector org, dir;
    
    // Ran out of ammo, switch to next best weapon
    if (self.attack_finished > time) return;
    if (self.ammo_rockets < 1) { forceweaponswitch(0.2); return;}

    self.currentammo = self.ammo_rockets = self.ammo_rockets - 1;
    sound (self, CHAN_WEAPON, "weapons/sgun1.wav", 1, ATTN_NORM);
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_finished = time + 0.8;
    self.punchangle_x = -2;
    player_rockreset();            // reset weaponframe

    makevectors (self.v_angle);
    org = self.origin + v_forward * 8 + '0 0 16';

    // Auto aim assist (builtin function 44)
    if ((autoaim_cvar < 1) && self.classtype != CT_MONGREMLIN)  dir = aim(self, SPEED_RLPLAYER);
    // Straight line forward where crosshair is pointing
    else dir = normalize(v_forward * SPEED_RLPLAYER);
    
    Launch_Missile (org, dir, '0 0 0', CT_PROJ_ROCKET, SPEED_RLPLAYER);
};

void () W_Fire_Crossbow = {
    local vector org, dir;

    if (self.attack_finished > time) return;
    if (self.ammo_bolts < 1) { forceweaponswitch(0.2); return;}
    self.currentammo = self.ammo_bolts = self.ammo_bolts - 1;

    makevectors (self.v_angle);
    sound (self, CHAN_WEAPON, "weapons/crossbow_fire.wav", 1, ATTN_NORM);

    if ((autoaim_cvar < 1) && self.classtype != CT_MONGREMLIN) dir = aim(self, SPEED_PLAYSPIKE);
    // Straight line forward where crosshair is pointing
    else dir = normalize(v_forward * SPEED_DCROSS);

    self.attack_finished = time + 0.9;
    org = self.origin + '0 0 16';
    launch_projectile (org, dir, CT_PROJ_BOLT1, 1100);
};

void () W_Fire_Crossbow_Poison = {
    local vector org, dir;

    if (self.attack_finished > time) return;
    if (self.ammo_poison < 1) { forceweaponswitch(0.2); return;}
    self.currentammo = self.ammo_poison = self.ammo_poison - 1;

    makevectors (self.v_angle);
    sound (self, CHAN_WEAPON, "weapons/crossbow_fire.wav", 1, ATTN_NORM);

    if ((autoaim_cvar < 1) && self.classtype != CT_MONGREMLIN)  dir = aim(self, SPEED_PLAYSPIKE);
    // Straight line forward where crosshair is pointing
    else dir = normalize(v_forward * SPEED_DCROSS);

    self.attack_finished = time + 0.9;
    org = self.origin + '0 0 16';
    self.poisonous = 1;
    launch_projectile (org, dir, CT_PROJ_BOLT2, 1100);
};

/*======================================================================
PLASMA
======================================================================*/
void() W_FirePlasma = {
    local vector org, dir;
    
    // If run out of ammo, switch weapons to next best
    if (self.attack_finished > time) return;
    if (self.ammo_cells < 1) { forceweaponswitch(0.2); return; }

    // Do nothing if weapon is under water
    if (self.waterlevel > 1) {
        self.attack_finished = time + 0.2;
        sound (self, CHAN_WEAPON, "weapons/nofire.wav", 1, ATTN_NORM);
        return;
    }
    
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_finished = time + 0.2;
    self.punchangle_x = -2;

    makevectors (self.v_angle);

    // Check for auto aim state
    // Auto aim assist (builtin function 44)
    if ((autoaim_cvar < 1) && self.classtype != CT_MONGREMLIN)  dir = aim(self, SPEED_PLAYPLASMA);
    // Straight line forward where crosshair is pointing
    else dir = normalize(v_forward * SPEED_PLAYPLASMA);

    sound (self, CHAN_WEAPON, "weapons/plasma_fire.wav", 1, ATTN_NORM);
    self.currentammo = self.ammo_cells = self.ammo_cells - 1;
    
    org = self.origin + v_forward*8 + '0 0 16';
    launch_plasma (org, dir, CT_PROJ_PLASMA, SPEED_PLAYPLASMA);
};

void(vector lstart, entity lsource, float ldamage) LightningReflection;

//----------------------------------------------------------------------
void(vector p1, vector p2, entity from, float damage) LightningDamage = {
    local entity e1, e2;
    local vector f;
    local float lighthit, temp_classgroup;
    
    f = p2 - p1;
    normalize (f);
    f_x = 0 - f_y;
    f_y = f_x;
    f_z = 0;
    f = f*16;

    e1 = e2 = world;
    lighthit = FALSE;
    //    LightningDamage (org, trace_endpos, self, 10);
    //    dprint("Light ("); dprint(trace_ent.classname); dprint(")\n");
    traceline (p1, p2, FALSE, self);
    if (trace_ent.takedamage) {
        lighthit = TRUE;
        // Some weird MP velocity hack!
        if (self.classtype == CT_PLAYER) {
            if (other.classtype == CT_PLAYER)
                trace_ent.velocity_z = trace_ent.velocity_z + 400;
        }
    }
    else {
        e1 = trace_ent;

        traceline (p1 + f, p2 + f, FALSE, self);
        if (trace_ent != e1 && trace_ent.takedamage) lighthit = TRUE;
        else {
            e2 = trace_ent;

            traceline (p1 - f, p2 - f, FALSE, self);
            if (trace_ent != e1 && trace_ent != e2 && trace_ent.takedamage)
                lighthit = TRUE;
        }
    }

    // Found a target to hit?
    if (lighthit) {
        //----------------------------------------------------------------------
        // Check for breakable/pushable monster immunity
        if (ai_immunedamage(self, trace_ent)) {
            // This is resist lightning function without pain sound
            // Don't spawn smoke constantly (let the sprite finish)
            if (self.lightning_timer < time) {
                self.lightning_timer = time + 0.3;
                SpawnExplosion(EXPLODE_BURST_SMOKE, trace_endpos, "");
            }
            SpawnProjectileSmoke(trace_endpos, 200, 50, 150);
        }

        // Check for any cell/lightning resistance
        else if (trace_ent.resist_cells > 0) {
            damage = damage * trace_ent.resist_cells;
            Resist_Lightning(trace_ent, trace_endpos);
            if (damage > 0) {
                // Need to fool T_Damage that damage is coming from LG
                temp_classgroup = from.classgroup;
                from.classgroup = CG_PROJCELLS;
                T_Damage (trace_ent, from, from, damage, DAMARMOR);
                from.classgroup = temp_classgroup;
            }
        }
        else {
            // Originally used 225 blood colour
            SpawnBlood(trace_ent, trace_endpos, '0 0 100', damage*4);
            T_Damage (trace_ent, from, from, damage, DAMARMOR);
        }
        // Check for any lightning reflection abilitites
        if (trace_ent.reflectlightning) {
            // Source = Lightning originally came from player
            LightningReflection(trace_endpos, trace_ent, damage*0.5);
        }
    }    
};

//----------------------------------------------------------------------
// New ability to spawn lighting strikes in random directions
//----------------------------------------------------------------------
void(vector lstart, entity lsource, float ldamage) LightningReflection =
{
    local vector lfinish, dir;

    // Setup a random XYZ direction (+/-)
    lfinish = lstart + vecrand(0,1000,1);
    
    // Trace line in random direction
    traceline(lstart, lfinish, FALSE, lsource);
    
    // Random chance of a plasma/lightning bolt!
    if (random() < 0.5) {
        dir = normalize(trace_endpos - lstart);
        launch_plasma(lstart, dir, CT_REFLECTLIGHT, SPEED_REFLECTION);
    }
    else {
        // Draw lighting beam (32 model unit chunks)
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
        WriteEntity (MSG_BROADCAST, lsource);
        WriteCoord (MSG_BROADCAST, lstart_x);
        WriteCoord (MSG_BROADCAST, lstart_y);
        WriteCoord (MSG_BROADCAST, lstart_z);
        WriteCoord (MSG_BROADCAST, trace_endpos_x);
        WriteCoord (MSG_BROADCAST, trace_endpos_y);
        WriteCoord (MSG_BROADCAST, trace_endpos_z);
        // Check for damage with new lightning beam
        LightningDamage(lstart, trace_endpos, world, ldamage);
    }
};

//----------------------------------------------------------------------
// This lightning damage check is designed for the lightning gun
// with many modifiers designed for the player and MP combat
// This function should not be used for general lightning damage
//----------------------------------------------------------------------
void(vector p1, vector p2, entity from, float damage) PlayerLightningDamage =
{
    local entity e1, e2;
    local vector f;
    local float lighthit, temp_classgroup;
    
    f = p2 - p1;
    normalize (f);
    f_x = 0 - f_y;
    f_y = f_x;
    f_z = 0;
    f = f*16;

    e1 = e2 = world;
    lighthit = FALSE;
    //    LightningDamage (org, trace_endpos, self, 10);
    //    dprint("Light ("); dprint(trace_ent.classname); dprint(")\n");
    traceline (p1, p2, FALSE, self);
    if (trace_ent.takedamage) {
        lighthit = TRUE;
        // Some weird MP velocity hack!
        // The story about this code (from quakeworld.nu)
        //
        // This code appears to have been there for a long, long time.
        // It was there in Quake 1.06 Shareware release, and it may have
        // been there before that. Someone must have been experimenting
        // with something and forgot it there, but no one noticed because
        // in that function, the 'other' entity is not usually set to
        // anything in particular, and the code never worked.
        //
        // But then QW came along, and in the QW engine it turns out
        // that 'other' will be set to 'self' if the player is touching
        // a platform. And so the dormant code stared working! It was
        // discovered by players, and players started using it to their
        // advantage. Now it's an integral part of dm6 gameplay.
        //
        // The code was apparently discovered and removed when Quake QC 
        // code was cleaned up before being released to public, so you
        // won't normally see it in NQ mods. But it is there in the
        // progs.dat in pak0.pak; and it will work in QuakeWorld engines
        // supporting progs.dat (currently FTE and ZQuake).
        //
        if (self.classtype == CT_PLAYER) {
            if (other.classtype == CT_PLAYER)
                trace_ent.velocity_z = trace_ent.velocity_z + 400;
        }
    }
    else {
        e1 = trace_ent;

        traceline (p1 + f, p2 + f, FALSE, self);
        if (trace_ent != e1 && trace_ent.takedamage) lighthit = TRUE;
        else {
            e2 = trace_ent;

            traceline (p1 - f, p2 - f, FALSE, self);
            if (trace_ent != e1 && trace_ent != e2 && trace_ent.takedamage)
                lighthit = TRUE;
        }
    }

    // Found a target to hit?
    if (lighthit) {
        // Check for any cell/lightning resistance
        if (trace_ent.resist_cells > 0) {
            damage = Resist_Damage(trace_ent, IT_CELLS, damage);
            Resist_Lightning(trace_ent, trace_endpos);
            if (damage > 0) {
                // Need to fool T_Damage that damage is coming from LG
                temp_classgroup = from.classgroup;
                from.classgroup = CG_PROJCELLS;
                T_Damage (trace_ent, from, from, damage, DAMARMOR);
                from.classgroup = temp_classgroup;
            }
        }
        else {
            // Originally used 225 blood colour
            SpawnBlood(trace_ent, trace_endpos, '0 0 100', damage*4);
            T_Damage (trace_ent, from, from, damage, DAMARMOR);
        }
        // Check for any lightning reflection abilitites
        if (trace_ent.reflectlightning) {
            // Source = Lightning originally came from player
            LightningReflection(trace_endpos, trace_ent, damage*0.5);
        }
    }    
};


//----------------------------------------------------------------------
void() W_FireLightning = {
    local vector dir;
    local float cells;

    // Ran out of ammo, switch to next best weapon
    if (self.ammo_cells < 1) { forceweaponswitch(0.2); return; }

    // explode if under water
    if (self.waterlevel > 1) {
        cells = self.ammo_cells;
        self.ammo_cells = 0;
        W_SetCurrentAmmo (self);
        T_RadiusDamage (self, self, 35*cells, world, DAMAGEALL);
        return;
    }

    // Time for a new LG hit sound?
    if (self.t_width < time) {
        sound (self, CHAN_WEAPON, "weapons/lhit.wav", 1, ATTN_NORM);
        self.t_width = time + 0.6;
    }
    
    self.currentammo = self.ammo_cells = self.ammo_cells - 1;
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_finished = time + 0.2;
    self.punchangle_x = -2;

    dir = self.origin + '0 0 16';
    traceline (dir, dir + v_forward*6000, TRUE, self);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
    WriteEntity (MSG_BROADCAST, self);
    WriteCoord (MSG_BROADCAST, dir_x);
    WriteCoord (MSG_BROADCAST, dir_y);
    WriteCoord (MSG_BROADCAST, dir_z);
    WriteCoord (MSG_BROADCAST, trace_endpos_x);
    WriteCoord (MSG_BROADCAST, trace_endpos_y);
    WriteCoord (MSG_BROADCAST, trace_endpos_z);

    PlayerLightningDamage (self.origin, trace_endpos + v_forward*4, self, DAMAGE_LGPLAYER);
};

// Fires chain lightning.
// Replaces 'W_FireLightning' and custom 'W_FireBurstLight'.
void(float cl) W_FireThunderbolt = {
    local   vector  org;
    local   float   cells, damage;     // Used as ammo cost first, then as damage.

    // Ran out of ammo, switch to next best weapon
    if (self.ammo_cells < 1) { forceweaponswitch(0.2); return; }

    // explode if under water
    if (self.waterlevel > 1) {
        cells = self.ammo_cells;
        self.ammo_cells = 0;
        W_SetCurrentAmmo (self);
        T_RadiusDamage (self, self, 35*cells, world, DAMAGEALL);
        return;
    }

    if (self.t_width < time) {
        sound (self, CHAN_WEAPON, "weapons/lhit.wav", 1, ATTN_NORM);
        self.t_width = time + 0.6;
    }
    self.punchangle_x = -2;

    damage = 2;
    if (self.tome_finished)
        damage = damage + 1;
    if (self.ammo_cells < damage)
        damage = self.ammo_cells;

    self.currentammo = self.ammo_cells = self.ammo_cells - damage;
    damage = damage * 15;

// Grrr, when a beam is drawn from a client, it always starts from the
// origin no matter the source point passed.  QuakeC can't fix this
// without causing some other problem sore to the eyes.
    makevectors(self.v_angle);
    org = self.origin + '0 0 16';
// Note:  Changing 1st parm won't make lightning start elsewhere from client.
    if (cl)
        BurstLit_Bolt (self.origin, org + v_forward*600, self, damage, 350);
    else
        LightningBolt (self.origin, org + v_forward*600, self, damage, self, TE_LIGHTNING2, "thunderbolt");
};














void () W_FireMultiGrenade = {
   local entity missile;

    if (self.attack_finished > time) return;

   self.attack_finished = time + 0.6;
   self.ammo_multi_rockets = (self.ammo_multi_rockets - TRUE);
   self.currentammo = (self.currentammo - TRUE);
   sound (self,CHAN_WEAPON,"weapons/grenade.wav",TRUE,ATTN_NORM);
   self.punchangle_x = CONTENT_SOLID;
   missile = spawn ();
   missile.owner = self;
   missile.movetype = MOVETYPE_BOUNCE;
   missile.solid = SOLID_BBOX;
   missile.classname = "MultiGrenade";
   makevectors (self.v_angle);
   if ( self.v_angle_x ) {
      missile.velocity = ((((v_forward * 600.000) + (v_up * 200.000)) + ((crandom () * v_right) * MOVETYPE_BOUNCE)) + ((crandom () * v_up) * MOVETYPE_BOUNCE));
   } else {
      missile.velocity = aim (self,10000.000);
      missile.velocity = (missile.velocity * 600.000);
      missile.velocity_z = 200.000;
   }
   missile.avelocity = '300.000 300.000 300.000';
   missile.angles = vectoangles (missile.velocity);
   missile.touch = MultiGrenadeTouch;
   missile.nextthink = (time + TRUE);
   missile.think = MultiGrenadeExplode;
   setmodel (missile,"progs/rogue/mervup.mdl");
   setsize (missile,VEC_ORIGIN,VEC_ORIGIN);
   setorigin (missile,self.origin);
};

void () W_FireMultiRocket = {
    if (self.attack_finished > time) return;
   self.ammo_multi_rockets = (self.ammo_multi_rockets - TRUE);
   self.currentammo = (self.currentammo - TRUE);
   sound (self,CHAN_WEAPON,"weapons/sgun1.wav",TRUE,ATTN_NORM);
   self.punchangle_x = CONTENT_SOLID;
   MultiRocketLaunch (-10.000,FL_SWIM);
   MultiRocketLaunch (CONTENT_LAVA,MOVETYPE_WALK);
   MultiRocketLaunch (MOVETYPE_FLY,FALSE);
   MultiRocketLaunch (MOVETYPE_BOUNCE,TRUE);
    self.attack_finished = time + 0.9;
};

void () W_FirePlasmaGun = {
   local entity missile;

    if (self.attack_finished > time) return;
   if ( (self.waterlevel > TRUE) ) {
      W_SetCurrentAmmo (self);
      discharged = TRUE;
      T_RadiusDamage (self,self,(35.000 * self.ammo_plasma),world,DAMAGEALL);
      discharged = FALSE;
      self.ammo_plasma = FALSE;
      return ;
   }
   self.ammo_plasma = (self.ammo_plasma - TRUE);
   self.currentammo = (self.currentammo - TRUE);
   sound (self,CHAN_WEAPON,"plasma/fire.wav",0.500,ATTN_NORM);
   self.punchangle_x = CONTENT_SOLID;
   missile = spawn ();
   missile.owner = self;
   missile.movetype = MOVETYPE_FLYMISSILE;
   missile.solid = SOLID_BBOX;
   missile.classname = "plasma";
   makevectors (self.v_angle);
   missile.velocity = aim (self,1250);
   missile.velocity = (missile.velocity * 1250);
   missile.avelocity = '300.000 300.000 300.000';
   missile.angles = vectoangles (missile.velocity);
   missile.touch = PlasmaTouch;
   setmodel (missile,"progs/plasma.mdl");
   setsize (missile,VEC_ORIGIN,VEC_ORIGIN);
   setorigin (missile,((self.origin + (v_forward * FL_CLIENT)) + '0.000 0.000 16.000'));
   sound (missile,CHAN_WEAPON,"plasma/flight.wav",TRUE,ATTN_NORM);
   missile.think = PlasmaGroundOut;
   missile.nextthink = (time + MOVETYPE_FLY);
   self.attack_finished = time + 0.8;
};

/*======================================================================
 PLAYER WEAPON UPDATES AND AMMO CHECKS
 
 W_SetCurrentAmmo   - Update HUD icons based on self.weapon value
 W_BestWeapon       - returns best weapon LG > SNG > SSG > NG > SG > Axe
                      interesting that the function does not return GL/RL
 W_CheckNoAmmo      - Ammo check for attack function
 W_Attack           - Main function that fire weapons (player animations)
                      Will automatically wakeup any nearby monsters
 W_ChangeWeapon     - Change weapon based on availabilty and ammo
 CycleWeaponCommand - Move forward or backward through active weapons
 CycleWeaponReverseCommand
 W_WeaponFrame      - Called every frame, catches impulse event
 SuperDamageSound   - Plays quad damage sound

======================================================================*/
void(entity targ) W_SetCurrentAmmo = {
    if (intermission_running > 0) return;    // intermission or finale
    remoteplayer_run (targ);            // get out of any weapon firing states

    // Reset all Ammo types for gfx on sbar (engine code hack - sbar.c)
    targ.items = targ.items - ( targ.items & (IT_SHELLS | IT_NAILS | IT_ROCKETS | IT_CELLS) );
      targ.items2 = (targ.items2 - (targ.items2 & ((IT2_LAVA_NAILS | IT2_PLASMA_AMMO) | IT2_MULTI_ROCKETS)));
        
    if (targ.weapon == IT_AXE) {
        targ.currentammo = 0;
        if (targ.moditems & IT_UPGRADE_AXE) {
            targ.weaponmodel = MODEL_VWEAP_UPAXE;
        } else {
            targ.weaponmodel = v_axemodel;
        }

        if (targ.perms & IT_CHAINSAW) {
            targ.weaponmodel = "progs/drake/v_saw.mdl"; // override axes
        }
    }
    else if (targ.weapon == IT_SHOTGUN) {
        targ.currentammo = targ.ammo_shells;
        if (targ.perms & IT_BLASTER) {
            targ.weaponmodel = "progs/drake/v_laserb.mdl";
        } else {
            if (targ.items & IT_SHOTGUN) {
                targ.weaponmodel = MODEL_VWEAP_SG;
            } else {
                targ.weaponmodel = "";
                targ.currentammo = 0;
            }
        }
        targ.items = targ.items | IT_SHELLS;
    }
    else if (targ.weapon == IT_SUPER_SHOTGUN) {
        targ.currentammo = targ.ammo_shells;
        if (targ.moditems & IT_UPGRADE_SSG) targ.weaponmodel = MODEL_VWEAP_UPSSG;
        else targ.weaponmodel = MODEL_VWEAP_SSG;
        targ.items = targ.items | IT_SHELLS;
    }
    else if (targ.weapon == IT_LAVA_NAILGUN) {
        targ.currentammo = targ.ammo_lava_nails;
        targ.weaponmodel = MODEL_VWEAP_LAVANG;
        targ.items2 = targ.items2 | IT2_LAVA_NAILS;
    }
    else if (targ.weapon == IT_NAILGUN) {
        targ.currentammo = targ.ammo_nails;
        targ.weaponmodel = MODEL_VWEAP_NG;
        targ.items = targ.items | IT_NAILS;
    }
    else if (targ.weapon == IT_LAVA_SUPER_NAILGUN) {
        targ.currentammo = targ.ammo_lava_nails;
        targ.weaponmodel = MODEL_VWEAP_LAVASNG;
        targ.items2 = targ.items2 | IT2_LAVA_NAILS;
    }
    else if (targ.weapon == IT_SUPER_NAILGUN) {
        targ.currentammo = targ.ammo_nails;
        targ.weaponmodel = MODEL_VWEAP_SNG;
        targ.items = targ.items | IT_NAILS;
    }
    else if (targ.weapon == IT_MULTI_GRENADE) {
        targ.currentammo = targ.ammo_multi_rockets;
        targ.weaponmodel = MODEL_VWEAP_MULTG;
        targ.items2 = targ.items2 | IT2_MULTI_ROCKETS;
    }
    else if (targ.weapon == IT_GRENADE_LAUNCHER) {
        targ.currentammo = targ.ammo_rockets;
        targ.weaponmodel = MODEL_VWEAP_GL;
        targ.items = targ.items | IT_ROCKETS;
    }
    else if (targ.weapon == IT2_PROXIMITY_GUN) {
        targ.currentammo = targ.ammo_rockets;
        targ.weaponmodel = MODEL_VWEAP_PROX;
        //targ.weaponmodel = "progs/drake/v_rocks.mdl";
        targ.items = targ.items | IT_ROCKETS;
    }
    else if (targ.weapon == IT_MULTI_ROCKET) {
        targ.currentammo = targ.ammo_multi_rockets;
        targ.weaponmodel = MODEL_VWEAP_MULTR;
        targ.items2 = targ.items2 | IT2_MULTI_ROCKETS;
    }
    else if (targ.weapon == IT_ROCKET_LAUNCHER) {
        targ.currentammo = targ.ammo_rockets;
        targ.weaponmodel = MODEL_VWEAP_RL;
        targ.items = targ.items | IT_ROCKETS;
    }
    else if (targ.weapon == IT_LIGHTNING) {
        targ.currentammo = targ.ammo_cells;
        if (targ.moditems & IT_UPGRADE_LG) targ.weaponmodel = MODEL_VWEAP_UPLG;
        else targ.weaponmodel = MODEL_VWEAP_LG;
        targ.items = targ.items | IT_CELLS;
    }
    else if (targ.weapon == IT2_CHAINLIGHTNING) {
        targ.currentammo = targ.ammo_cells;
        targ.weaponmodel = "progs/drake/v_light2.mdl";
        targ.items = targ.items | IT_CELLS;
    }
    else if (targ.weapon == IT_PLASMA_GUN) {
        targ.currentammo = targ.ammo_plasma;
        targ.weaponmodel = MODEL_VWEAP_PLASM;
        targ.items2 = targ.items2 | IT2_MULTI_ROCKETS;
    }
    else if (targ.weapon == IT2_LASER_CANNON) {
        targ.currentammo = targ.ammo_cells;
        //targ.weaponmodel = MODEL_VWEAP_LCAN;
        targ.weaponmodel = "progs/drake/v_laserg.mdl";
        targ.items = targ.items | IT_CELLS;
    }
    else if (targ.weapon == IT_EXTRA_WEAPON) {
        if (targ.ammo_poison > 0) {
            targ.currentammo = targ.ammo_poison;
            targ.weaponmodel = "progs/v_crossbow2.mdl";
            targ.items2 = targ.items2 | IT2_POISON;
        } else {
            targ.currentammo = targ.ammo_bolts;
            targ.weaponmodel = "progs/v_crossbow1.mdl";
            targ.items2 = targ.items2 | IT2_BOLTS;
        }
    }
    else if (targ.weapon == IT2_MJOLNIR) {
        targ.currentammo = targ.ammo_cells;
        targ.weaponmodel = MODEL_VWEAP_MJOL;
        targ.items = targ.items | IT_CELLS;
    }
    else if (targ.weapon == IT2_CROSS) {
        targ.weaponmodel =  "progs/drake/v_wand.mdl";
    }
    else if (targ.weapon == IT2_GRAPPLE) {
        targ.currentammo = 0;
        targ.weaponmodel = "progs/drake/v_grpple.mdl";
        if (self.hook) {
            if (self.hook.count) {
                self.weaponframe = 1;   // Hook is still out.
            }
        }
    }
    else {
        // Cinematic mode, no viewmodel
        targ.currentammo = 0;
        targ.weaponmodel = "";
    }
    targ.weaponframe = 0;        // reset weapon frame
};

/*======================================================================
 Return best weapon based on current ammo quantities, excludes rockets and multi-rockets for obvious heat of the moment self inflicted damage avoidance
======================================================================*/
float(entity targ) W_BestWeapon =
{

    if (targ.waterlevel < 1 && targ.ammo_cells > 0 && (targ.items & IT2_LASER_CANNON) )
            return IT2_LASER_CANNON;
    if (targ.waterlevel < 1 && targ.ammo_cells > 0 && (targ.items2 & IT2_CHAINLIGHTNING) )
            return IT2_CHAINLIGHTNING;
    if (targ.waterlevel < 1 && targ.ammo_cells > 0 && (targ.items & IT_LIGHTNING) )
            return IT_LIGHTNING;
    if (targ.waterlevel < 1 && targ.ammo_cells > 0 && (targ.items & IT2_MJOLNIR) )
            return IT2_MJOLNIR;
    if(targ.ammo_lava_nails > 1 && (targ.items & IT_LAVA_SUPER_NAILGUN) )
        return IT_LAVA_SUPER_NAILGUN;
    if(targ.ammo_nails > 1 && (targ.items & IT_SUPER_NAILGUN) )
        return IT_SUPER_NAILGUN;
    if(targ.ammo_lava_nails > 1 && (targ.items & IT_LAVA_NAILGUN) )
        return IT_LAVA_NAILGUN;
    if(targ.ammo_bolts > 1 && (targ.items & IT_EXTRA_WEAPON) )
        return IT_EXTRA_WEAPON;
    if(targ.ammo_shells > 2 && (targ.items & IT_SUPER_SHOTGUN) )
        return IT_SUPER_SHOTGUN; // greater than 2 in case of upgrade triple shotgun Widowmaker
    if(targ.ammo_nails > 0 && (targ.items & IT_NAILGUN) )
        return IT_NAILGUN;
    if(targ.ammo_shells > 0 && (targ.items & IT_SHOTGUN || targ.perms | IT_BLASTER) )
        return IT_SHOTGUN;
    if (targ.waterlevel > 0 && targ.ammo_cells < 1 && (targ.items & IT2_MJOLNIR) )
            return IT2_MJOLNIR;
    if (targ.items & IT_AXE)
        return IT_AXE;
    if (targ.items2 & IT2_CROSS)
        return IT2_CROSS; // WAND

    return FALSE;
};

//----------------------------------------------------------------------
float(entity targ) W_CheckNoAmmo =
{
    if (targ.currentammo > 0) return TRUE;
    if (targ.weapon == IT_AXE || targ.weapon == IT2_MJOLNIR || targ.weapon == IT2_CROSS || targ.weapon == IT2_GRAPPLE) return TRUE;
    
    targ.weapon = W_BestWeapon (targ);
    W_SetCurrentAmmo (targ);
    
    // drop the weapon down
    return FALSE;
};

void() player_lava_nail1;
void() player_lava_snail1;

/*======================================================================
W_Attack
======================================================================*/
void() W_Attack = {
    if (intermission_running > 0) return;    // intermission or finale
    if ( !W_CheckNoAmmo(self) ) return;        // Out of ammo?

    makevectors    (self.v_angle);        // calculate forward angle for velocity

    if (self.weapon != IT_EXTRA_WEAPON)
        self.show_hostile = time + 1;    // wake monsters up

    //----------------------------------------------------------------------
    // Axe - Mighty chopper
    //----------------------------------------------------------------------
    if (self.weapon == IT_AXE) {
        if (self.perms & IT_CHAINSAW) {
            // Chainsaw (overrides axes once acquired)
            player_saw1 ();
            self.attack_finished = time + 0.55;
            sound (self, CHAN_AUTO, "weapons/sawatck.wav", 1, ATTN_NORM);
        } else {
            // Axe and Shadowaxe
            if (random() < 0.5) sound (self, CHAN_WEAPON, SOUND_AXE_SWIPE1, 1, ATTN_NORM);
            else sound (self, CHAN_WEAPON, SOUND_AXE_SWIPE2, 1, ATTN_NORM);
            // Work out which axe swing to play (never play swing twice in a row)
            self.lip = self.meleeattack;
            while (self.meleeattack == self.lip) {
                self.lip = rint(random()*4.4);
            }
            self.meleeattack = self.lip;
            if (v_axemodel == "progs/ad171/v_shadaxe0.mdl") {
                if (self.meleeattack == 0) player_axe1();
                else if (self.meleeattack == 1) player_axeb1();
                else if (self.meleeattack == 2) player_axec1();
                else if (self.meleeattack == 3) player_axed1();
                else player_axee1();
            } else {
                if (self.meleeattack == 0) player_axeold1();
                else if (self.meleeattack == 1) player_axeoldb1();
                else if (self.meleeattack == 2) player_axeoldc1();
                else player_axeoldd1();
            }    
            self.attack_finished = time + 0.5;
        }
    }
    //----------------------------------------------------------------------
    else if (self.weapon == IT_SHOTGUN) {
        if (self.perms & IT_BLASTER) {
            player_blast1(); // use laser blaster only once we have it, still uses shells
        } else {
            player_sg1(); // use standard single barrel shotgun
        }
    }
    else if (self.weapon == IT_SUPER_SHOTGUN) player_supersg1();
    else if (self.weapon == IT_NAILGUN) player_nail1();
    else if (self.weapon == IT_LAVA_NAILGUN) { player_lava_nail1(); self.lavaGunFired = TRUE; }
    else if (self.weapon == IT_SUPER_NAILGUN) player_snail1();
    else if (self.weapon == IT_LAVA_SUPER_NAILGUN) {player_lava_snail1();  self.lavaGunFired = TRUE; }
    else if (self.weapon == IT_MULTI_GRENADE) player_multi_grenade1();
    else if (self.weapon == IT_GRENADE_LAUNCHER) player_grenade1();
    else if (self.weapon == IT_MULTI_ROCKET) player_multi_rocket1();
    else if (self.weapon == IT_ROCKET_LAUNCHER) player_rocket1();
    else if (self.weapon == IT_PLASMA_GUN) player_plasmalight1();
    else if (self.weapon == IT_LIGHTNING) {
        if (self.moditems & IT_UPGRADE_LG) player_plasma1();
        else {
            player_light1(); 
            sound (self, CHAN_AUTO, "weapons/lstart.wav", 1, ATTN_NORM); 
        }
    }
    else if (self.weapon == IT_EXTRA_WEAPON) player_crossbow1();
    else if (self.weapon == IT2_LASER_CANNON) player_laser1();
    else if (self.weapon == IT2_MJOLNIR) {
        if (self.ammo_cells < 30) {
            self.attack_finished = time + 0.4;
            player_hammer1();
        } else {
            self.attack_finished = time + 0.4;
            player_mjolnir1();
        }
    }
    else if (self.weapon == IT2_GRAPPLE) {
            if (self.hook) {
                // Assume hook was improperly removed, so reset links.
                if (!self.hook.modelindex) {   
                    self.hook = world;  // So W_FireGrapple fires a hook.
                    player_grapple1 ();
                    return;
                }
                if (self.hook.count)
                    return;
                player_grapple3 ();
            }
            else
                player_grapple1 ();
            // Modified animation takes three frames to complete.
            self.attack_finished = time + 0.3;
    }
    else if (self.weapon == IT2_PROXIMITY_GUN) {
        player_prox_grenade1();
        /*           player_rocket1();
            if (self.tome_finished)
            {   W_FireHydra();
                W_Reload (1.5);
            }
            else
            {   W_FireSidewinder();
                W_Reload (1);
            }
        */
    }
    else if (self.weapon == IT2_CHAINLIGHTNING) {
        // Quake cannot draw very many bolts, so rapid-fire like
        // normal Thunderbolt is a bad idea.
        // Update 11/20/09:  Use DoE plasma code with fewer bolts.
        //  We may be able to stay under limits (24 bolts).
        //if (self.tome_finished) { TODO: Uncomment this if statement once I figure out if tome_finished is a viable powerup from Drake?
        //    player_snake1 ();
        //    W_FireDeathRay ();
        //    W_Reload (0.4);
        //} else {
            player_chainlight1();
            self.attack_finished = time + 0.1;
            sound (self, CHAN_AUTO, "weapons/lstart.wav", 1, ATTN_NORM);
        //}
    }
    else if (self.weapon == IT2_CROSS) {
        player_wand1();
        W_FireSparks();
    }
};

float (entity targ, float testValue) InItems = {
   if ( (targ.items & testValue) )
      return ( TRUE );

   return ( FALSE );
};

float (entity targ, float testValue) InItems2 = {
   if ( (targ.items2 & testValue) )
      return ( TRUE );

   return ( FALSE );
};

float (entity targ, float testValue) InModItems = {
   if ( (targ.moditems & testValue) )
      return ( TRUE );

   return ( FALSE );
};

/*======================================================================
 W_ChangeWeapon
 Check if got weapon and ammo and switch to relevant weapon
======================================================================*/
void(entity targ) W_ChangeWeapon = {
    local float it, it2, am, fl;
    local float gotgun;
    local float gotprox;
    local float gotgren;
    local float gotmultgren;
    
    if (intermission_running > 0) return;    // intermission or finale
    it = targ.items;
    it2 = targ.items2;
    am = FALSE;
    
    gotprox = InItems2(targ, IT2_PROXIMITY_GUN);
    gotgren = InItems(targ, IT_GRENADE_LAUNCHER);
    gotmultgren = InItems(targ, IT_MULTI_GRENADE);
    gotgun = FALSE;
    if (targ.impulse == 1) {
        if ((targ.items2 & IT2_GRAPPLE) && (targ.weapon == IT_AXE)) {
            fl = IT2_GRAPPLE;
            gotgun = InItems2(targ, fl);
        } else {
            fl = IT_AXE;
            gotgun = InItems(targ, IT_AXE);
            if (InModItems(targ, IT_UPGRADE_AXE)) gotgun = TRUE;
            if (self.perms & IT_CHAINSAW) gotgun = TRUE;
        }

        if ( !(targ.items & IT_AXE)) {
            fl = IT2_GRAPPLE;
            gotgun = InItems2(targ, fl);
        }
    }
    else if (targ.impulse == 2) {
        fl = IT_SHOTGUN;
        if (targ.ammo_shells < 1) am = TRUE;
        gotgun = InItems(targ, IT_SHOTGUN);
    }
    else if (targ.impulse == 3) {
        fl = IT_SUPER_SHOTGUN;
        if (targ.ammo_shells < 2) am = TRUE;
        gotgun = InItems(targ, IT_SUPER_SHOTGUN);
        if (InModItems(targ, IT_UPGRADE_SSG)) gotgun = TRUE;
    }        
    else if (targ.impulse == 4) {
        if ((targ.items & IT_LAVA_NAILGUN) &&(targ.weapon == IT_NAILGUN) || (targ.ammo_nails < 1)) {
            fl = IT_LAVA_NAILGUN;
            if (targ.ammo_lava_nails < 1) am = TRUE;
            gotgun = InItems(targ, fl);
        } else {
            fl = IT_NAILGUN;
            if (targ.ammo_nails < 1) am = TRUE;
            gotgun = InItems(targ, fl);
        }
    }
    else if (targ.impulse == 5) {
        if ((targ.items & IT_LAVA_SUPER_NAILGUN) && (targ.weapon == IT_SUPER_NAILGUN) || (targ.ammo_nails < 2)) {
            fl = IT_LAVA_SUPER_NAILGUN;
            if (targ.ammo_lava_nails < 2) am = TRUE;
            gotgun = InItems(targ, fl);
        } else {
            fl = IT_SUPER_NAILGUN;
            if (targ.ammo_nails < 2) am = TRUE;
            gotgun = InItems(targ, fl);
        }
    }
    else if (targ.impulse == 6) {
        if (targ.weapon == IT_GRENADE_LAUNCHER) {
            if (gotmultgren) {
                fl = IT_MULTI_GRENADE;
                gotgun = TRUE;
            } else {
                fl = IT2_PROXIMITY_GUN;
                gotgun = InItems2(targ, fl);
            }
        } else {
            if (targ.weapon == IT_MULTI_GRENADE) {
                if (gotprox) {
                    fl = IT2_PROXIMITY_GUN;
                    gotgun = TRUE;
                } else {
                    fl = IT_GRENADE_LAUNCHER;
                    gotgun = InItems(targ, fl);
                }
            }  else {
                if (targ.weapon == IT2_PROXIMITY_GUN) {
                    if (gotgren) {
                        fl = IT_GRENADE_LAUNCHER;
                        gotgun = TRUE;
                    } else {
                        fl = IT_MULTI_GRENADE;
                        gotgun = InItems(targ, fl);
                    }
                } else {
                    if (gotgren) {
                        fl = IT_GRENADE_LAUNCHER;
                        gotgun = TRUE;
                    } else {
                        if (gotmultgren) {
                            fl = IT_MULTI_GRENADE;
                            gotgun = TRUE;
                        } else {
                            if (gotprox) {
                                fl = IT2_PROXIMITY_GUN;
                                gotgun = TRUE;
                            }
                        }
                    }
                }
            }
        }
        if (fl == IT_MULTI_GRENADE) {
            if (targ.ammo_multi_rockets < 1) am = TRUE;
        } else {
            if (targ.ammo_rockets < 1) am = TRUE;
        }
    }
    else if (targ.impulse == 7) {
        if ((targ.items & IT_MULTI_ROCKET) && (targ.weapon == IT_ROCKET_LAUNCHER) || (targ.ammo_rockets < 1)) {
            fl = IT_MULTI_ROCKET;
            if (targ.ammo_multi_rockets < 1) am = TRUE;
            gotgun = InItems(targ, fl);
        } else {
            fl = IT_ROCKET_LAUNCHER;
            if (targ.ammo_rockets < 1) am = TRUE;
            gotgun = InItems(targ, fl);
        }
    }
    else if (targ.impulse == 8) {
        if ((targ.items & IT_PLASMA_GUN) && (targ.weapon == IT_LIGHTNING) || (targ.ammo_cells < 1)) {
            fl = IT_PLASMA_GUN;
            if (targ.ammo_plasma < 1) am = TRUE;
            gotgun = InItems(targ, fl);
        } else {
            fl = IT_LIGHTNING;
            if (targ.ammo_cells < 1) am = TRUE;
            gotgun = InItems(targ, fl);
            if (InModItems(targ, IT_UPGRADE_LG)) gotgun = TRUE;
        }
    }
    else if (targ.impulse == 9) {
        if ((targ.items & IT_EXTRA_WEAPON) && (targ.weapon == IT2_LASER_CANNON) || (targ.ammo_cells < 1)) {
            fl = IT_EXTRA_WEAPON;
            if (targ.ammo_bolts < 1) am = TRUE;
            gotgun = InItems(targ, fl);
        } else {
            fl = IT2_LASER_CANNON;
            if (targ.ammo_cells < 1) am = TRUE;
            gotgun = InItems2(targ, fl);
        }
    }
    else if (targ.impulse == 60) {
        fl = IT_LAVA_NAILGUN;
        if (targ.ammo_lava_nails < 1) am = TRUE;
        gotgun = InItems(targ, fl);
    }
    else if (targ.impulse == 61) {
        fl = IT_LAVA_SUPER_NAILGUN;
        if (targ.ammo_lava_nails < 2) am = TRUE;
        gotgun = InItems(targ, fl);
    }
    else if (targ.impulse == 62) {
        fl = IT_MULTI_GRENADE;
        if (targ.ammo_multi_rockets < 1) am = TRUE;
        gotgun = InItems(targ, fl);
    }
    else if (targ.impulse == 63) {
        fl = IT_MULTI_ROCKET;
        if (targ.ammo_multi_rockets < 1) am = TRUE;
        gotgun = InItems(targ, fl);
    }
    else if (targ.impulse == 64) {
        fl = IT_PLASMA_GUN;
        if (targ.ammo_plasma < 1) am = TRUE;
        gotgun = InItems(targ, fl);
    }
    else if (targ.impulse == 175) {
        fl = IT2_LASER_CANNON;
        if (targ.ammo_cells < 1) am = TRUE;
        gotgun = InItems2(targ, fl);
    }
    else if (targ.impulse == 176) {
        fl = IT2_PROXIMITY_GUN;
        gotgun = InItems2(targ, fl);
    }
    else if (targ.impulse == 177) {
        if ((targ.items & IT_EXTRA_WEAPON) && (targ.weapon == IT2_LASER_CANNON) || (targ.ammo_cells < 1)) {
            fl = IT_EXTRA_WEAPON;
            if (targ.ammo_bolts < 1) am = TRUE;
            gotgun = InItems(targ, fl);
        } else {
            fl = IT2_LASER_CANNON;
            if (targ.ammo_cells < 1) am = TRUE;
            gotgun = InItems2(targ, fl);
        }

        if ((targ.items2 & IT2_CROSS) && ((targ.weapon == IT2_MJOLNIR) || !InItems(targ,IT2_MJOLNIR))) {
            fl = IT2_CROSS;
            gotgun = InItems2(targ, fl);
        } else {
            fl = IT2_MJOLNIR;
            gotgun = InItems2(targ, fl);
        }
    }
    else if (targ.impulse == 178) {
        fl = IT_EXTRA_WEAPON;
        gotgun = InItems(targ, fl);
    }
    else if (targ.impulse == 179) {
        fl = IT2_CROSS;
        gotgun = InItems2(targ, fl);
    }
    else if (targ.impulse == 180) {
        fl = IT2_GRAPPLE;
        gotgun = InItems2(targ, fl);
    }
    else if (targ.impulse == 181) {
        fl = IT2_CHAINLIGHTNING;
        gotgun = InItems2(targ, fl);
    }
    targ.impulse = 0;        // Clear impulse
    
    //----------------------------------------------------------------------
    // FL = don't have the weapon
    // AM = out of ammo
    //----------------------------------------------------------------------
    //Do we have this weapon?
    if ( !(gotgun) ) {
        sprint (targ,"no weapon.\n");
        return ;
    }

    //Do we have ammo?
    if ( am ) {
        sprint (targ,"not enough ammo.\n");
        return ;
    }

   if ( (targ.weapon != fl) ) {
      if ( ((targ.weapon == IT_LAVA_NAILGUN) || (targ.weapon == IT_LAVA_SUPER_NAILGUN)) ) {
         if ( ((fl == IT_NAILGUN) || (fl == IT_SUPER_NAILGUN)) ) {
            sprint (targ,"Normal Nails\n");
         }
      } else {
         if ( (targ.weapon == IT_MULTI_GRENADE) ) {
            if ( (fl == IT_GRENADE_LAUNCHER) ) {
               sprint (targ,"Normal Grenades\n");
            }
         } else {
            if ( (targ.weapon == IT_MULTI_ROCKET) ) {
               if ( (fl == IT_ROCKET_LAUNCHER) ) {
                  sprint (targ,"Normal Rockets\n");
               }
            } else {
               if ( (targ.weapon == IT_PLASMA_GUN) ) {
                  if ( (fl == IT_LIGHTNING) ) {
                     if (targ.moditems & IT_UPGRADE_LG) {
                        sprint (targ,"Plasma Gun\n");
                     } else {
                        sprint (targ,"Lightning Gun\n");
                     }
                  }
               } else {
                  if ( ((fl == IT_LAVA_NAILGUN) || (fl == IT_LAVA_SUPER_NAILGUN)) ) {
                     sprint (targ,"Lava Nails!\n");
                  } else {
                     if ( (fl == IT_MULTI_GRENADE) ) {
                        sprint (targ,"Multi-Grenades!\n");
                     } else {
                        if ( (fl == IT_MULTI_ROCKET) ) {
                           sprint (targ,"Multi-Rockets!\n");
                        } else {
                           if ( (fl == IT_PLASMA_GUN) ) {
                              sprint (targ,"Plasma Gun!\n");
                           } else {
                              if ( (fl == IT2_PROXIMITY_GUN) ) {
                                 sprint (targ,"Proximity Grenades!\n");
                              } else {
                                 if ( (fl == IT2_LASER_CANNON) ) {
                                    sprint (targ,"Laser Cannon!\n");
                                 }  else {
                                     if ( (fl == IT2_CROSS) ) {
                                           sprint (targ,"Magic Wand!\n");
                                     }  else {
                                            if ( (fl == IT2_MJOLNIR) ) {
                                                   sprint (targ,"Mjolnir's Hammer!\n");
                                            }
                                    }
                                 }
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }
   targ.weapon = fl;
   W_SetCurrentAmmo (targ);
};

/*======================================================================
CycleWeaponCommand
======================================================================*/
void() CycleWeaponCommand =
{
    local    float    am;
    
    if (intermission_running > 0) return;        // intermission or finale
    self.impulse = 0;            // reset impulse

    if (self.weapon == 0 && self.items == 0) return;

    // Keep cycling around weapon list until found a weapon with ammo
    while (1) {
        am = 0;
        if (self.weapon == IT2_MJOLNIR) {
            self.weapon = IT2_CHAINLIGHTNING;
            if (self.ammo_cells < 1) am = 1;
        }
        else if (self.weapon == IT2_CHAINLIGHTNING) {
            self.weapon = IT_AXE;
        }
        else if (self.weapon == IT_AXE) {
            self.weapon = IT_SHOTGUN;
            if (self.ammo_shells < 1) am = 1;
        }
        else if (self.weapon == IT_SHOTGUN) {
            self.weapon = IT_SUPER_SHOTGUN;
            if (self.ammo_shells < 2) am = 1;
        }        
        else if (self.weapon == IT_SUPER_SHOTGUN) {
            self.weapon = IT_NAILGUN;
            if (self.ammo_nails < 1) am = 1;
        }
        else if (self.weapon == IT_NAILGUN) {
            self.weapon = IT_LAVA_NAILGUN;
            if (self.ammo_lava_nails < 2) am = 1;
        }
        else if (self.weapon == IT_LAVA_NAILGUN) {
            self.weapon = IT_SUPER_NAILGUN;
            if (self.ammo_nails < 2) am = 1;
        }
        else if (self.weapon == IT_SUPER_NAILGUN) {
            self.weapon = IT_LAVA_SUPER_NAILGUN;
            if (self.ammo_lava_nails < 2) am = 1;
        }
        else if (self.weapon == IT_LAVA_SUPER_NAILGUN) {
            self.weapon = IT_GRENADE_LAUNCHER;
            if (self.ammo_rockets < 1) am = 1;
        }
        else if (self.weapon == IT_GRENADE_LAUNCHER) {
            self.weapon = IT2_PROXIMITY_GUN;
            if (self.ammo_rockets < 1) am = 1;
        }
        else if (self.weapon == IT2_PROXIMITY_GUN) {
            self.weapon = IT_MULTI_GRENADE;
            if (self.ammo_multi_rockets < 1) am = 1;
        }
        else if (self.weapon == IT_MULTI_GRENADE) {
            self.weapon = IT_ROCKET_LAUNCHER;
            if (self.ammo_rockets < 1) am = 1;
        }
        else if (self.weapon == IT_ROCKET_LAUNCHER) {
            self.weapon = IT_MULTI_ROCKET;
            if (self.ammo_multi_rockets < 1) am = 1;
        }
        else if (self.weapon == IT_MULTI_ROCKET) {
            self.weapon = IT_LIGHTNING;
            if (self.ammo_cells < 1) am = 1;
        }
        else if (self.weapon == IT_LIGHTNING) {
            self.weapon = IT_PLASMA_GUN;
            if (self.ammo_plasma < 1) am = 1;
        }
        else if (self.weapon == IT_PLASMA_GUN) {
            self.weapon = IT2_LASER_CANNON;
            if (self.ammo_cells < 1) am = 1;
        }
        else if (self.weapon == IT2_LASER_CANNON) {
            self.weapon = IT_EXTRA_WEAPON;
            if (self.ammo_bolts < 1) am = 1;
        }
        else if (self.weapon == IT_EXTRA_WEAPON) {
            self.weapon = IT2_CROSS;
        }
        else if (self.weapon == IT2_CROSS) {
            self.weapon = IT2_MJOLNIR;
        }
        else if (self.weapon == IT2_MJOLNIR) {
            self.weapon = IT2_CHAINLIGHTNING;
            if (self.ammo_cells < 1) am = 1;
        }
        else if (self.weapon == FALSE || self.items == 0) {
            return; // prevent crash in "cinematic mode"
        }    

        // Has the player got the weapons and ammo to switch?
        if (self.weapon == IT2_MJOLNIR || self.weapon == IT2_LASER_CANNON || self.weapon == IT2_PROXIMITY_GUN || self.weapon == IT2_CROSS || self.weapon == IT2_CHAINLIGHTNING) {
            if (self.weapon == IT2_CROSS) {
                if ( (self.items2 & IT2_CROSS) && !am) {
                    W_SetCurrentAmmo (self);
                    return;
                }
            } else {
                if ( (self.items2 & self.weapon) && !am) {
                    W_SetCurrentAmmo (self);
                    return;
                }
            }    
        } else {
            if ( (self.items & self.weapon) && !am) {
                W_SetCurrentAmmo (self);
                return;
            }
        }
    }
};

/*======================================================================
CycleWeaponReverseCommand
======================================================================*/
void() CycleWeaponReverseCommand =
{
    local    float    am;
    
    if (intermission_running > 0) return;        // intermission or finale
    self.impulse = 0;            // reset impulse

    if (self.weapon == 0 && self.items == 0) return;

    // Keep cycling around weapon list until found a weapon with ammo
    while (1) {
        am = 0;
        if (self.weapon == IT2_MJOLNIR) {
            self.weapon = IT2_CROSS;
        }
        else if (self.weapon == IT2_CROSS) {
            self.weapon = IT_EXTRA_WEAPON;
            if (self.ammo_bolts < 1) am = 1;
        }
        else if (self.weapon == IT_EXTRA_WEAPON) {
            self.weapon = IT2_LASER_CANNON;
            if (self.ammo_cells < 1) am = 1;
        }
        else if (self.weapon == IT2_LASER_CANNON) {
            self.weapon = IT_PLASMA_GUN;
            if (self.ammo_plasma < 1) am = 1;
        }
        else if (self.weapon == IT_PLASMA_GUN) {
            self.weapon = IT_LIGHTNING;
            if (self.ammo_cells < 1) am = 1;
        }
        else if (self.weapon == IT_LIGHTNING) {
            self.weapon = IT_MULTI_ROCKET;
            if (self.ammo_multi_rockets < 1) am = 1;
        }
        else if (self.weapon == IT_MULTI_ROCKET) {
            self.weapon = IT_ROCKET_LAUNCHER;
            if (self.ammo_rockets < 1) am = 1;
        }
        else if (self.weapon == IT_ROCKET_LAUNCHER) {
            self.weapon = IT_MULTI_GRENADE;
            if (self.ammo_multi_rockets < 1) am = 1;
        }
        else if (self.weapon == IT_MULTI_GRENADE) {
            self.weapon = IT2_PROXIMITY_GUN;
            if (self.ammo_rockets < 1) am = 1;
        }
        else if (self.weapon == IT2_PROXIMITY_GUN) {
            self.weapon = IT_GRENADE_LAUNCHER;
            if (self.ammo_rockets < 1) am = 1;
        }
        else if (self.weapon == IT_GRENADE_LAUNCHER) {
            self.weapon = IT_LAVA_SUPER_NAILGUN;
            if (self.ammo_lava_nails < 2) am = 1;
        }
        else if (self.weapon == IT_LAVA_SUPER_NAILGUN) {
            self.weapon = IT_SUPER_NAILGUN;
            if (self.ammo_nails < 2) am = 1;
        }
        else if (self.weapon == IT_SUPER_NAILGUN) {
            self.weapon = IT_LAVA_NAILGUN;
            if (self.ammo_lava_nails < 1) am = 1;
        }
        else if (self.weapon == IT_LAVA_NAILGUN) {
            self.weapon = IT_NAILGUN;
            if (self.ammo_nails < 1) am = 1;
        }
        else if (self.weapon == IT_NAILGUN) {
            self.weapon = IT_SUPER_SHOTGUN;
            if (self.ammo_shells < 2) am = 1;
        }        
        else if (self.weapon == IT_SUPER_SHOTGUN) {
            self.weapon = IT_SHOTGUN;
            if (self.ammo_shells < 1) am = 1;
        }
        else if (self.weapon == IT_SHOTGUN) {
            self.weapon = IT_AXE;
        }
        else if (self.weapon == IT_AXE) {
            self.weapon = IT2_CHAINLIGHTNING;
            if (self.ammo_cells < 1) am = 1;
        }
        else if (self.weapon == IT2_CHAINLIGHTNING) {
            self.weapon = IT2_MJOLNIR;
        } else if (self.weapon == FALSE || self.items == 0) {
            return; // prevent crash in "cinematic mode"
        }    
    
        // Has the player got the weapons and ammo to switch?
        if (self.weapon == IT2_MJOLNIR || self.weapon == IT2_LASER_CANNON || self.weapon == IT2_PROXIMITY_GUN || self.weapon == IT2_CROSS || self.weapon == IT2_CHAINLIGHTNING) {
            if (self.weapon == IT2_CROSS) {
                if ( (self.items2 & IT2_CROSS) && !am) {
                    W_SetCurrentAmmo (self);
                    return;
                }
            } else {
                if ( (self.items2 & self.weapon) && !am) {
                    W_SetCurrentAmmo (self);
                    return;
                }
            }    
        } else {
            if ( (self.items & self.weapon) && !am) {
                W_SetCurrentAmmo (self);
                return;
            }
        }
    }
};

/*======================================================================
W_WeaponFrame
======================================================================*/
void() W_WeaponFrame = {
    if (self.lavaGunFired) {
        if (self.weapon == IT_LAVA_NAILGUN || self.weapon == IT_LAVA_SUPER_NAILGUN) {
            sound (self, CHAN_BODY, "lavagun/snail.wav", 1, ATTN_NORM);
        }
        self.lavaGunFired = 0;
    }

    ImpulseCommands ();
    // Allow for impulse commands before weapon lockout
    if (time < self.attack_finished) return;

    // Change crossbow skin if bolts are poisoned
    if (self.ammo_poison > 0) {
        W_SetCurrentAmmo (self);
    }

    // check for attack
    if (self.button0) {
        // originally - SuperDamageSound
        // Only play one powerup sound at once
        if (self.super_damage_finished > 0 && self.powerup_sound < time) {
            if (self.super_sound < time) {
                self.super_sound = time + 1;
                self.powerup_sound = time + 1;
                sound (self, CHAN_BODY, SOUND_ARTQUAD3, 1, ATTN_NORM);
            }
        }
        // Only play one powerup sound at once
        if (self.sharpshoot_finished > 0 && self.powerup_sound < time) {
            // Only works with the shotguns
            if (self.weapon == IT_SHOTGUN || self.weapon == IT_SUPER_SHOTGUN) {
                if (self.sharpshooter_sound < time) {
                    self.sharpshooter_sound = time + 0.5;
                    // Only play the sound every other shot
                    self.powerup_sound = time + 1;
                    sound (self, CHAN_BODY, SOUND_ARTSHARP3, 1, ATTN_NORM);
                }
            }
        }
        // Only play one powerup sound at once
        if (self.nailpiercer_finished > 0 && self.powerup_sound < time) {
            // Only works with nailgun + super nailgun
            if (self.weapon == IT_NAILGUN || self.weapon == IT_SUPER_NAILGUN) {
                if (self.nailpiercer_sound < time) {
                    self.nailpiercer_sound = time + 0.5;
                    self.powerup_sound = time + 1;
                    sound (self, CHAN_BODY, SOUND_ARTNAILP3, 1, ATTN_NORM);
                }
            }
        }
        // Check for weapon updates
        W_Attack ();
    }
};
