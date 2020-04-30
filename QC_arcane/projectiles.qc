/*======================================================================
 PROJECTILE Systems
 ------------------

 * Bullets (PROJECTILE)
 * Nails
 * Rockets
 * Grenades
 * Plasma

 PLAYER Projectile Collision (movetypes)
 ---------------------------------------
 
 * ID software Quake originally had most player projectiles (rockets/nails)
   use a large collision size (MOVETYPE_FLYMISSILE) and this especially
   helped with flying enemies (scrags) that strafe around quickly.
   
 * AD changed all the player projectile collision boxes to a smaller
   size (MOVETYPE_FLY) to add a greater challenge to combat and this
   is noticeable when fighting flying monsters.
   This change was linked to the setting of the sv_aim console command,
   which has been changed from 0.93 to 1 in most modern quake clients.
   The sv_aim command was used to help steer projectiles into monsters
   to ultimately make the combat easier for keyboard only players.

 * Projectiles move in discreet steps on the server and with a small
   movetype they can often skip through objects. This was probably the
   reason ID software had large collision and changing them to small
   only makes combat more difficult and less fun.
   
 * The projectile size is now linked to a worldspawn variables which
   defaults to 0 and will keep the original ID software default while
   giving map makers the choice to override this option.
   
   There is also an impulse command (105) to toggle this setting and
   its reported on the mod info table on the console (developer 1)
   
   The new worldspawn variable is - no_bigprojectiles
   
   This change will affect the following ammo type/weapons:
   shells(SG/SSG/WSG), nails(NG/SNG), rockets(RL) and cells(PG).
   
 * There is a big projectile collision for grenades but ID software
   never used it. All grenades are setup with MOVETYPE_BOUNCE
   which is always a small collision.

======================================================================*/
void() Particle_Bullet =
{
    // Is the touch function blocked?
    if (self.waitmin > time) return;
    if (self.delay < time) entity_remove(self, 1);
    else {
        if (random() < 0.5) {
            self.oldorigin = crandom() * '1 1 1';
            if (random() < 0.8) self.lip = rint(random()*4);
            else self.lip = 112 + rint(random()*4);
            particle (self.origin, self.oldorigin, self.lip, 1 + rint(random()*2));
        }
        self.think = Particle_Bullet;
        self.nextthink = time + self.speed;
    }
};

//----------------------------------------------------------------------
void() Touch_Bullet =
{
    if (self.touchedvoid) return;                // Marked for removal
    if (check_skycontent(self.origin)) {entity_remove(self, 0.1); return;}
    if (other == self.owner) return;            // Touching self, do nothing
    if (other.solid == SOLID_TRIGGER) return;    // trigger field, do nothing
    if (self.waitmin > time) return;            // bullet touch has been disabled
    entity_remove(self, 1);                        // Setup bullet for removal    
    
    //----------------------------------------------------------------------
    // Hit monster/interactive object, impact blood
    //----------------------------------------------------------------------
    if (other.takedamage) {
        // Due to how difficult projectile shotguns were to hit thing
        // An extra +4 damage (dead center tracer) was added to help
        // In AD 1.7+ the bullet projectiles are now using wide impact
        // against monsters which means more pellets will hit.
        // The extra tracer pellet damage is not required anymore.

        // Check pellet type for damage
        if (self.classtype == CT_PROJ_TRACEPART) self.dmg = 0;
        else if (self.classtype == CT_PROJ_TRACE) self.dmg = 0;
            // Old code kept for reference
            // No extra trace damage for zombies, it will kill them
            //if (other.classgroup == CG_ZOMBIE) self.dmg = 0;
            //else self.dmg = DAMAGE_PTSHELL;    
        // Default shell pellet damage
        else self.dmg = DAMAGE_PSHELL;    
        
        // Check for breakable/pushable no monster damage
        if (ai_immunedamage(self.owner, other)) {
            self.dmg = 0;
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, TE_GUNSHOT);
            WriteCoord (MSG_BROADCAST, self.origin_x);
            WriteCoord (MSG_BROADCAST, self.origin_y);
            WriteCoord (MSG_BROADCAST, self.origin_z);
        }
        // Show bullet resistance as small blood+gunshot+smoke
        else if (other.resist_shells > 0) {
            self.dmg = Resist_Damage(other, IT_SHELLS, self.dmg);
            Resist_Shells(other, self.origin, self.velocity, self.dmg);
        }
        else {
            // Hitting monsters does twice the amount of blood effects
            if (other.flags & FL_MONSTER) spawn_touchblood (self, other, self.dmg*2);
            else spawn_touchblood (self, other, self.dmg);
        }
        
        // Finally do the damage, if any available (after resistance)
        if (self.dmg > 0) T_Damage (other, self, self.owner, self.dmg, DAMARMOR);
    }
    //----------------------------------------------------------------------
    // Hit world/static object, impact particles
    //----------------------------------------------------------------------
    else {
        // Tracer bullet, dead center and inflight particle emitter
        // Originally did no damage, but changed to buff particle SG
        if (self.classtype == CT_PROJ_TRACE) {
            self.lip = random();
            // NG tink sound or SG ricochet sound (hardcoded client sounds)
            if (self.lip < 0.5) sound(self, CHAN_VOICE, "weapons/tink1.wav", random()*0.5, ATTN_LOW);
            else if (self.lip < 0.7) sound(self, CHAN_VOICE, "weapons/ric2.wav", random()*0.5, ATTN_LOW);
            else sound(self, CHAN_VOICE, "weapons/ric3.wav", random()*0.5, ATTN_LOW);
        }
        else {
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, TE_GUNSHOT);
            WriteCoord (MSG_BROADCAST, self.origin_x);
            WriteCoord (MSG_BROADCAST, self.origin_y);
            WriteCoord (MSG_BROADCAST, self.origin_z);
        }
    }
};

//----------------------------------------------------------------------
void(vector org, vector dir, float proj_type, float proj_speed) Launch_Bullet =
{
    newmis = spawn();
    newmis.owner = self;
    newmis.classname = "proj_bullet";    // obj name, not really used anymore
    newmis.classtype = proj_type;        // Class type number, quick identity
    newmis.classgroup = CG_PROJSHELLS;    // Ammo type
    //----------------------------------------------------------------------
    // Horrible hack! If the player has the TSG or quad, flag it for zombies
    //----------------------------------------------------------------------
    if (self.moditems & IT_UPGRADE_SSG || self.super_damage_finished > 0)
        newmis.weapon = TRUE;
    
    //----------------------------------------------------------------------
    // Setup player/monster projectile collision
    //----------------------------------------------------------------------
    if (self.classtype == CT_PLAYER) {
        if (playerprojsize == 0) 
            newmis.movetype = MOVETYPE_FLYMISSILE;    // Large collision
        else newmis.movetype = MOVETYPE_FLY;        // Small collision
    }
    else newmis.movetype = MOVETYPE_FLYMISSILE;    // Default = large collision
    
    //----------------------------------------------------------------------
    newmis.solid = SOLID_BBOX;
    newmis.touch = Touch_Bullet;                    // touch function
    newmis.waitmin = 0;                                // Touch function active

    // Particle tracer (true aim, no damage)
    if (proj_type == CT_PROJ_TRACE || proj_type == CT_PROJ_TRACEPART) {
        newmis.delay = time + LIFE_SHELLS;            // Maximum life time
        if (self.weapon & IT_SUPER_SHOTGUN) newmis.count = 1;
        else newmis.count = 1 + rint(random()*2);    // Default particles for SG
        newmis.speed = 0.02;                        // Next function interval (very high tick)
        newmis.think = Particle_Bullet;                // Particle trail
        newmis.nextthink = time + newmis.speed;
    }
    // SG/SSG shells (scatter effect)
    else {
        newmis.think = SUB_Remove;
        newmis.nextthink = time + LIFE_SHELLS;    // Stop projectile going forever
    }

    newmis.mdl = MODEL_PROJ_DIAM2;
    setmodel(newmis, newmis.mdl);                // Diamond model
    newmis.frame = random()*15;                    // Full range of sizes
    newmis.skin = 16 + random()*7;                // Bright colours

    newmis.velocity = dir * proj_speed;            // Constant speed multiplier
    newmis.angles = vectoangles(dir);            // Create direction angle
    setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);    // Zero size
    setorigin (newmis, org);                    // Move to starting position
    newmis.avelocity = vecrand(100,200,FALSE);
    
};

//----------------------------------------------------------------------
void(float bullet_count, vector bullet_spread, float bullet_type) Launch_Shells =
{
    local    vector src_origin, bullet_dir, spread_dir;
    local float bullet_speed, var_speed;
    
    // Is the player firing the shotgun?
    if (self.flags & FL_CLIENT) {
        makevectors(self.v_angle);
        // Infront of player model and down towards gun
        src_origin = self.origin + attack_vector('10 0 8');
        if (bullet_type == CT_PROJ_SG) bullet_speed = SPEED_PLAYERSG;
        else bullet_speed = SPEED_PLAYERSSG;
        // Either straight line or auto aim assist (builtin function 44)
        if (autoaim_cvar < 1) bullet_dir = aim(self, SPEED_PLAYAIM);
        else bullet_dir = normalize(v_forward * bullet_speed);
    }
    else {
        makevectors(self.angles);
        // At the end of the new soldier gun model
        src_origin = self.origin + attack_vector(self.attack_offset);
        bullet_speed = SPEED_MONSG + (skill*SPEED_MONSGMULT);
        // fire somewhat behind the player, so a dodging player is harder to hit
        bullet_dir = self.enemy.origin - self.enemy.velocity*0.2;
        bullet_dir = normalize (bullet_dir - self.origin);
    }

    // Setup particle emitter/tracer shot (true aim)
    Launch_Bullet(src_origin, bullet_dir, CT_PROJ_TRACE, bullet_speed);
    // Reduced the amount of visual noise infront of the player
    // The projectiles below are particle trail emitters only
    //    Launch_Bullet(src_origin+(v_right*(crandom()*10)), bullet_dir, CT_PROJ_TRACEPART, bullet_speed + (crandom()*30) );
    //    Launch_Bullet(src_origin+(v_right*(crandom()*10)), bullet_dir, CT_PROJ_TRACEPART, bullet_speed + (crandom()*30) );

    while (bullet_count > 0) {
        var_speed = crandom()*10 + bullet_speed;        // Slight speed variance
        spread_dir = bullet_dir + (crandom()*bullet_spread_x) * v_right + (crandom()*bullet_spread_y) * v_up;
        Launch_Bullet(src_origin, spread_dir, bullet_type, var_speed);

        bullet_count = bullet_count - 1;
    }
};

/*======================================================================
 PLASMA PROJECTILES 
 * requires special blue/white particle trail
 * Has radius and impact touch damage
 * used by Eliminator (enforcer), Soldier and Minotaur
======================================================================*/
void() Particle_Plasma =
{
    // projectile has touched something
    if (self.waitmin > time) return;
    
    // Generate sprite particles? DP not active?
    if (random() < 0.5 && !ext_dppart) {
        self.oldorigin = crandom() * '1 1 1';
        if (self.poisonous) self.lip = 56 + rint(random()*8);
        else self.lip = 40 + rint(random()*8);
        particle (self.origin, self.oldorigin, self.lip, 4 + rint(random()*4));
        if (self.poisonous) self.lip = 182 + rint(random()*8);
        else self.lip = 198 + rint(random()*4);
        particle (self.origin, self.oldorigin, self.lip, 2 + rint(random()*2));
    }

    // The Player plasma projectile grows over frames
    if (self.classproj == CT_PLAYER) {
        if (self.attack_finished < time) {
            self.frame = self.frame + 1;
            if (self.frame < 6) self.attack_finished = time + 0.1;
            else self.frame = 6;
        }
    }

    self.think = Particle_Plasma;
    self.nextthink = time + self.speed;    
};

//----------------------------------------------------------------------
// Compiler forward reference
void(vector org, vector dir, float proj_type, float proj_speed) launch_plasma;
//----------------------------------------------------------------------
void() Touch_PlasmaProjectile =
{
    local vector org, vec, dir;
    local entity tself;
    
    if (self.touchedvoid) return;                // Marked for removal
    if (check_skycontent(self.origin)) {entity_remove(self, 0.1); return;}
    if (other == self.owner) return;            // Touching self, do nothing
    if (other.solid == SOLID_TRIGGER) return;    // trigger field, do nothing
    if (self.waitmin > time) return;            // plasma touch has been disabled
    
    if (self.classproj == CT_PLAYER) self.pos1 = DAMAGE_PGPLAYER;
    else if (self.classproj == CT_REFLECTLIGHT) self.pos1 = DAMAGE_PGREFLECT;
    else if (self.classproj == CT_REFLECTPLASMA) self.pos1 = DAMAGE_PGREFLECT;
    else if (self.classproj == CT_MONMINOTAUR) self.pos1 = DAMAGE_PGMINOTAUR;
    else if (self.classproj == CT_MONGAUNT) self.pos1 = DAMAGE_PGGAUNT;
    else if (self.classproj == CT_MONNOUR) self.pos1 = DAMAGE_PGNOUR;
    else self.pos1 = DAMAGE_PLASMA;

    // Check for breakable/pushable immunity first
    if (ai_immunedamage(self.owner, other)) self.pos1 = '0 0 0';
    self.dmg = self.pos1_x + random()*self.pos1_y;
    
    // Plasma Splashdamage affects everything, check for 0 dmg first
    // This will also apply a poisonous debuff from the attacker
    if (self.pos1_z > 0) 
        T_RadiusDamage (self, self.owner, self.pos1_z, world, DAMAGEALL);

    //----------------------------------------------------------------------
    // Hit monster/interactive object, impact blood
    //----------------------------------------------------------------------
    if (other.takedamage) {
        // Check for breakable/pushable no monster damage
        if (ai_immunedamage(self.owner, other)) {
            // Using a cut down version of ammo resistance
            SpawnExplosion(EXPLODE_BURST_SMOKE, self.origin, SOUND_PLASMA_HIT);
            SpawnProjectileSmoke(self.origin, 200, 50, 150);
            if (random() < 0.5) SpawnProjectileSmoke(self.origin, 200, 50, 250);
            if (random() < 0.5) SpawnProjectileSmoke(self.origin, 300, 50, 150);
        }
        // Check for plasma/cell resistance
        else if (other.resist_cells > 0) {
            self.dmg = Resist_Damage(other, IT_CELLS, self.dmg);
            Resist_Plasma(other, self.origin);
        }
        else {
            // Hitting monsters does twice the amount of blood effects
            if (other.flags & FL_MONSTER) spawn_touchblood (self, other, self.dmg*2);
            else spawn_touchblood (self, other, self.dmg);

            // Switch poisonous to poison sprite, but keep plasma explosion
            // Otherwise this sounds confusing with a rocket explosion
            if (self.poisonous) self.lip = EXPLODE_POISON_SMALL;
            else {
                // Randomly pick between quoth electric and blue explosions
                if (random() < 0.3) self.lip = EXPLODE_PLASMA_SMALL;
                else self.lip = EXPLODE_ELECT_SMALL;
            }
            SpawnExplosion(self.lip, self.origin, SOUND_PLASMA_HIT);
        }
        
        // Plasma hits always kill any zombie (1 hit)
        if (other.classgroup == CG_ZOMBIE) self.dmg = DAMAGE_ZOMBIECLASS;
        // Don't bother with damage if there is none!
        if (self.dmg > 0) T_Damage (other, self, self.owner, self.dmg, DAMARMOR);

    }
    //----------------------------------------------------------------------
    // Hit world/static object, impact particles
    //----------------------------------------------------------------------
    else {
        // New special coloured particle explosion (rogue expansion)
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
        if (self.poisonous) WriteByte (MSG_BROADCAST, 51);
        else WriteByte (MSG_BROADCAST, 35);
        WriteByte (MSG_BROADCAST, 8);

        // Poisonous sprite explosion instead
        if (self.poisonous) self.lip = EXPLODE_POISON_SMALL;
        // Classic quoth electric impact explosion
        else self.lip = EXPLODE_ELECT_SMALL;
        SpawnExplosion(self.lip, self.origin, SOUND_PLASMA_HIT);
    }
    // Check for any plasma reflection? (player only function)
    if (self.classproj == CT_PLAYER && other.reflectplasma) {
        org = self.origin;
        // Random chance that plasma will reflect straight back
        if (random() < 0.2 && self.owner) 
            dir = normalize(self.owner.origin - org);
        else {
            // Pick random location instead
            vec = org + vecrand(0,1000,1);
            dir = normalize(vec - org);
        }
        // Switch around self to make sure reflection happens once
        tself = self; self = other;
        launch_plasma(org, dir, CT_REFLECTPLASMA, SPEED_REFLECTION);
        self = tself;
    }
    
    // Remove plasma bolt
    entity_remove(self, 1);
};

//----------------------------------------------------------------------
void(vector org, vector dir, float proj_type, float proj_speed) launch_plasma =
{
    newmis = spawn ();
    newmis.owner = self;
    newmis.classname = "proj_plasma";    // obj name, not really used anymore
    newmis.classtype = CT_PROJ_PLASMA;    // Class type number, quick identity
    newmis.classgroup = CG_PROJCELLS;    // Ammo type
    newmis.classproj = proj_type;        // Monster type number
    newmis.attack_speed = proj_speed;    // Save for later

    // The player plasma projectile use to be linked to autoaim condition
    // Switched to larger collision, so that greater chance to hit enemies
    // Monsters always use large collision (otherwise infighting would be less)
    //----------------------------------------------------------------------
    if (proj_type == CT_PLAYER) {
        if (playerprojsize == 0) 
            newmis.movetype = MOVETYPE_FLYMISSILE;    // Large collision
        else newmis.movetype = MOVETYPE_FLY;        // Small collision
    }
    else {
        // Monster get the raw deal, small projectile collision
        newmis.movetype = MOVETYPE_FLY;
        // Minotaur, Gaunt is rapid fire, don't need glow effect (slow down)
        if (proj_type != CT_MONGAUNT && proj_type != CT_MONMINOTAUR) 
            newmis.effects = EF_DIMLIGHT;
    }

    //----------------------------------------------------------------------
    newmis.solid = SOLID_BBOX;
    newmis.touch = Touch_PlasmaProjectile;
    newmis.nextthink = time + LIFE_PROJECTILE;
    newmis.think = SUB_Remove;
    newmis.poisonous = newmis.owner.poisonous;
    
    // Setup model and special parameters (zombie, boss)
    //----------------------------------------------------------------------
    if (proj_type == CT_MONARMYPLASMA) {
        if (newmis.poisonous) setmodel (newmis, MODEL_PROJ_PLASMAGRN);
        else setmodel (newmis, MODEL_PROJ_PLASMA);
    }
    else if (proj_type == CT_MONELIMATOR) {
        if (newmis.poisonous) setmodel (newmis, MODEL_PROJ_PLASMAGRN);
        else setmodel (newmis, MODEL_PROJ_PLASMA);
    }
    else if (proj_type == CT_MONCENTURION) {
        if (newmis.poisonous) setmodel (newmis, MODEL_PROJ_PLASMAGRN);
        else setmodel (newmis, MODEL_PROJ_PLASMA);
    }
    else if (proj_type == CT_MONGAUNT) {
        if (newmis.poisonous) setmodel (newmis, MODEL_PROJ_GAPLASMAGRN);
        else setmodel (newmis, MODEL_PROJ_GAPLASMA);
    }
    else if (proj_type == CT_MONEEL) {
        if (newmis.poisonous) setmodel (newmis, MODEL_PROJ_PLASMAGRN);
        else setmodel (newmis, MODEL_PROJ_PLASMA);
    }
    else if (proj_type == CT_MONSEEKER) setmodel (newmis, MODEL_PROJ_PLASMA);
    else if (proj_type == CT_MONMINOTAUR) setmodel (newmis, MODEL_PROJ_MPLASMA);
    else if (proj_type == CT_MONNOUR) setmodel (newmis, MODEL_PROJ_NOUR3);
    else if (proj_type == CT_REFLECTLIGHT) setmodel (newmis, MODEL_PROJ_LIGHTNING2);
    else if (proj_type == CT_REFLECTPLASMA) {
        setmodel (newmis, MODEL_PROJ_PGPLASMA);
        newmis.avelocity = vecrand(0,300,TRUE);    // randomize avel
        newmis.angles_y = rint(random()*360);    // Random angle
        newmis.frame = 3;                        // Medium size
    }
    else {
        setmodel (newmis, MODEL_PROJ_PGPLASMA);
        newmis.attack_finished = time + 0.1;    // Update model frame every 0.1s
        newmis.avelocity = vecrand(0,300,TRUE);    // randomize avel
        newmis.angles_y = rint(random()*360);    // Random angle
        newmis.frame = 0;                        // Start really small
    }

    //----------------------------------------------------------------------
    // Is DP engine active for new particle trail?
    if (ext_dppart) {
        if (newmis.poisonous) newmis.traileffectnum = particleeffectnum(DPP_TRPOISON);
        else newmis.traileffectnum = particleeffectnum(DPP_TRPLASMA);
        newmis.effects = 0;
        // Extra glow and light colour (DP mostly)
        //newmis.effects = EF_BLUE;
        //newmis.glow_color = 246;    // Blue/gold    
    }

    // Manually generate special particle trail (blue or green)
    newmis.count = 1 + rint(random()*2);    // Default particles
    newmis.speed = 0.02;                    // Next function interval (very high tick)
    newmis.think = Particle_Plasma;            // Particle trail
    newmis.nextthink = time + newmis.speed;

    // Standard projectile setup (origin, size and velocity)
    //----------------------------------------------------------------------
    newmis.velocity = dir * newmis.attack_speed;
    if (proj_type != CT_PLAYER) newmis.angles = vectoangles(dir);
    setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);        
    setorigin (newmis, org);
};

/*======================================================================
 SMALL PROJECTILES - used all over the place ...
 
 * player (nailgun, super nailgun)
 * Wizards (double spit)
 * Hell Knights (magic spray)
 * spike shooters (regular, super, laser)
 * Enforcers (laser)
 * Crossbow Knights (bolts)
 * Zombies (flesh)
======================================================================*/
// Forward compiler reference because of peircing nails function
void(vector org, vector dir, float proj_type, float proj_speed) launch_projectile;

//----------------------------------------------------------------------
void() Touch_Projectile =
{
    local vector org, dir, vec;
    local float proj_type, proj_speed;
    local entity tself;
    
    if (self.touchedvoid) return;                // Marked for removal
    if (check_skycontent(self.origin)) {entity_remove(self, 0.1); return;}
    if (other == self.owner) return;            // no touching self
    if (other.solid == SOLID_TRIGGER) return;    // trigger field, do nothing

    // No more touch/world interaction
    self.touch = SUB_Null;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    
    //----------------------------------------------------------------------
    //    Laser impact/stop sound (precached by owner)
    if (self.classtype == CT_PROJ_LASER)
        sound (self, CHAN_WEAPON, "enforcer/enfstop.wav", 1, ATTN_STATIC);
    // Golem rock impact sound (precached by owner)
    else if (self.classtype == CT_PROJ_GROCK) {
        self.lip = random();
        self.volume = 0.5 + random()*0.5;
        if (self.lip < 0.25) sound (self, CHAN_WEAPON, SOUND_IMP_ROCK1, self.volume, ATTN_BREAK);
        else if (self.lip < 0.5) sound (self, CHAN_WEAPON, SOUND_IMP_ROCK2, self.volume, ATTN_BREAK);
        else if (self.lip < 0.75) sound (self, CHAN_WEAPON, SOUND_IMP_ROCK3, self.volume, ATTN_BREAK);
        else sound (self, CHAN_WEAPON, SOUND_IMP_ROCK4, self.volume, ATTN_BREAK);
    }

    //----------------------------------------------------------------------
    // hit something that bleeds
    if (other.takedamage) {
        // SNG spikes are essentially double damage (cheap way of 2 x nails)
        if (self.classtype == CT_PROJ_SNG) self.dmg = DAMAGE_SNGSPIKE;
        // Reflected nails (can only happen from the player)
        else if (self.classtype == CT_PROJ_REFNG) self.dmg = DAMAGE_NGREFSPIKE;
        // Bob and enforcer lasers are different
        else if (self.owner.classtype == CT_MONJIM) 
                self.dmg = DAMAGE_BOBLASER + (random() * DAMAGE_BOBLASER);
        else if (self.classtype == CT_PROJ_LASER) self.dmg = DAMAGE_LASER;
        // Special damage and sound effects for crossbow knights
        else if (self.classtype == CT_PROJ_BOLT1) {
            // hit4 = flesh wound, hit1 = ting sound
            if (random() < 0.3) sound (self, CHAN_WEAPON, "weapons/bolt_hit1.wav", 1, ATTN_NORM);
            else sound (self, CHAN_WEAPON, "weapons/bolt_hit4.wav", 1, ATTN_NORM);
            // Reduce the damage if the bolt has hit another dcrossbow knight
            if (other.classtype == self.owner.classtype) self.dmg = DAMAGE_BOLT0;
            else self.dmg = DAMAGE_BOLT1;
        }
        else if (self.classtype == CT_PROJ_BLBOLT || self.classtype == CT_PROJ_BLBOLT2) 
            self.dmg = DAMAGE_BOGLORD;
        // Zombies have special impact sound
        else if (self.classtype == CT_PROJ_FLESH) {
            sound (self, CHAN_WEAPON, "zombie/z_hit.wav", 1, ATTN_NORM);
            self.dmg = DAMAGE_FLESH;
        }
        // Scorpion spikes cannot hurt other spider types
        // This is to prevent the minion scorpion killing other minions
        else if (self.classtype == CT_PROJ_SCORP) {
            if (other.classgroup == self.owner.classgroup) self.dmg = 0;
            else self.dmg = DAMAGE_NGSPIKE;
        }
        // Rock projectiles cannot hurt stone monsters!
        else if (self.classtype == CT_PROJ_GROCK) {
            if (other.classgroup == self.owner.classgroup) self.dmg = 0;
            else self.dmg = DAMAGE_NGSPIKE;
        }
        // Default spike damage (nails)
        else self.dmg = DAMAGE_NGSPIKE;
        
        //----------------------------------------------------------------------
        // Check for breakable/pushable monster immunity
        if (ai_immunedamage(self.owner, other)) {
            // Zero damage and make sure no resistance checks
            self.dmg = self.classgroup = 0;
            // Remove poisonous effect
            self.poisonous = FALSE;
            // Show ammo resistance effect
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, TE_GUNSHOT);
            WriteCoord (MSG_BROADCAST, self.origin_x);
            WriteCoord (MSG_BROADCAST, self.origin_y);
            WriteCoord (MSG_BROADCAST, self.origin_z);
            if (random() < 0.5) SpawnProjectileSmoke(self.origin, 200, 50, 150);
        }

        //----------------------------------------------------------------------
        // Check for poison debuff (using poisonous flag)
        if (self.poisonous == TRUE) PoisonDeBuff(other);

        //----------------------------------------------------------------------
        // Check for NG/SNG nail resistance
        if (self.classgroup == CG_PROJNAILS && other.resist_nails > 0) {
            self.dmg = Resist_Damage(other, IT_NAILS, self.dmg);
            Resist_Nails(other, self.origin);
            self.projeffect = 0;
            // Check for any nail reflection?
            if (other.reflectnails && random() < other.resist_nails) {
                org = self.origin;
                // Random chance that nail will reflect straight back
                if (random() < 0.2 && self.owner) 
                    dir = normalize(self.owner.origin - org);
                else {
                    // Pick random location instead
                    // Flatten the Z axis so the reflection looks less random
                    vec = vecrand(0,100,1);
                    vec_z = random()*25;
                    vec = vec + org;
                    dir = normalize(vec - org);
                }
                // Switch around self to make sure reflection happens once
                tself = self; self = other;
                launch_projectile(org, dir, CT_PROJ_REFNG, SPEED_REFLECTION);
                self = tself;
            }
        }
        //----------------------------------------------------------------------
        // Produce blood particles at impact and apply damage to target
        if (self.dmg > 0) {
            spawn_touchblood (self, other, self.dmg);
            T_Damage (other, self, self.owner, self.dmg, DAMARMOR);
        }

        //----------------------------------------------------------------------
        // Nail Piercing affect, move nail through monster
        if (self.projeffect & IT_ARTPIERCE) {
            // a small pile of gibs!
            if (random() < 0.2) SpawnMeatSpray (self, other, crandom() * 100);
            // Setup projectile ready for monster tests
            self.movetype = MOVETYPE_FLY;
            self.solid = SOLID_BBOX;
            self.oldorigin = self.origin;
            self.count = 0;

            // Loop forward 8 times to find space on the other side
            while (self.count < 8) {
                // Trace forward from current position
                self.finaldest = self.oldorigin + self.finalangle*512;
                traceline(self.oldorigin, self.finaldest,FALSE,self);
                // Still inside bleeding object?
                if (trace_ent == other)
                    self.oldorigin = self.oldorigin + self.finalangle*16;
                // On the other side!
                else self.count = 8;
                self.count = self.count + 1;
            }
            // Is the other side free space to spawn?
            if (pointcontents(self.oldorigin) == CONTENT_EMPTY) {
                tself = self;
                org = self.oldorigin;
                dir = self.finalangle;
                proj_type = self.classtype;
                proj_speed = self.attack_speed;
                self = self.owner;
                // Once a projectile hits an object it is impossible
                // to reset its velocity/angles and carry on
                // It is easier to create a new projectile and delete
                // the currently damaged one instead!
                launch_projectile(org, dir, proj_type, proj_speed);
                self = tself;
            }
        }
    }
    //----------------------------------------------------------------------
    else {
        // Some projectiles need to be left lying around for a while
        if (self.bodyfadeaway) {
            if (self.classtype == CT_PROJ_FLESH) 
                sound (self, CHAN_WEAPON, "zombie/z_miss.wav", 1, ATTN_NORM);
            else if (self.classtype == CT_PROJ_SPID)
                sound (self, CHAN_WEAPON, "spider/miss.wav", 1, ATTN_NORM);
            else if (self.classtype == CT_PROJ_SWAMP)
                sound (self, CHAN_WEAPON, "swampling/miss.wav", 1, ATTN_NORM);
            else if (self.classtype == CT_PROJ_VORE)
                sound (self, CHAN_WEAPON, "voreling/miss.wav", 1, ATTN_NORM);
            else if (self.classtype == CT_PROJ_ELF)
                sound (self, CHAN_WEAPON, "xmas/elf/miss.wav", 1, ATTN_NORM);
            else if (self.classtype == CT_PROJ_BOLT1) {
                if (random() < 0.5) sound (self, CHAN_WEAPON, "weapons/bolt_hit2.wav", 1, ATTN_NORM);
                else sound (self, CHAN_WEAPON, "weapons/bolt_hit3.wav", 1, ATTN_NORM);
            }
                
            self.velocity = self.avelocity = '0 0 0';
            // Straight away make projectiles vanish
            self.nextthink = time + 2 + random();
            self.ltime = self.nextthink;
            self.think = model_fade;    
            return;
        }
        
        //----------------------------------------------------------------------
        // Standard engine impact particles and sounds
        else {
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            
            if (self.classtype == CT_PROJ_SNG) WriteByte (MSG_BROADCAST, TE_SUPERSPIKE);
            else if (self.classtype == CT_PROJ_LASER) WriteByte (MSG_BROADCAST, TE_KNIGHTSPIKE);
            else if (self.classtype == CT_PROJ_GROCK) WriteByte (MSG_BROADCAST, TE_GUNSHOT);
            else if (self.classtype == CT_PROJ_WIZ) WriteByte (MSG_BROADCAST, TE_WIZSPIKE);
            else if (self.classtype == CT_PROJ_SPID) WriteByte (MSG_BROADCAST, TE_WIZSPIKE);
            else if (self.classtype == CT_PROJ_SWAMP) WriteByte (MSG_BROADCAST, TE_WIZSPIKE);
            else if (self.classtype == CT_PROJ_VORE) WriteByte (MSG_BROADCAST, TE_SUPERSPIKE);
            else if (self.classtype == CT_PROJ_ELF) WriteByte (MSG_BROADCAST, TE_GUNSHOT);
            else if (self.classtype == CT_PROJ_FURY2) WriteByte (MSG_BROADCAST, TE_WIZSPIKE);
            else if (self.classtype == CT_PROJ_NOUR1) WriteByte (MSG_BROADCAST, TE_WIZSPIKE);
            else if (self.classtype == CT_PROJ_BLBOLT) WriteByte (MSG_BROADCAST, TE_WIZSPIKE);
            else if (self.classtype == CT_PROJ_BLBOLT2) WriteByte (MSG_BROADCAST, TE_KNIGHTSPIKE);
            else if (self.classtype == CT_PROJ_HKN) WriteByte (MSG_BROADCAST, TE_KNIGHTSPIKE);
            else WriteByte (MSG_BROADCAST, TE_SPIKE);
            
            WriteCoord (MSG_BROADCAST, self.origin_x);
            WriteCoord (MSG_BROADCAST, self.origin_y);
            WriteCoord (MSG_BROADCAST, self.origin_z);
        }
    }
    
    // Hide projectile and wait for any sounds to finish playing
    entity_remove(self,2);
};

//----------------------------------------------------------------------
void(vector org, vector dir, float proj_type, float proj_speed) launch_projectile =
{
    newmis = spawn ();
    newmis.owner = self;
    newmis.classname = "proj_nail";        // obj name, not really used anymore
    newmis.classtype = proj_type;        // Class type number, quick identity

    if (newmis.classtype == CT_PROJ_NG || newmis.classtype == CT_PROJ_SNG) 
        newmis.classgroup = CG_PROJNAILS;    // Ammo type
    else newmis.classgroup = CG_PROJALL;    // Ammo type (default)

    // Switch model collision based on auto aim functionality
    // Monsters always use large collision (otherwise infighting would be less)
    //----------------------------------------------------------------------
    if (self.classtype == CT_PLAYER) {
        if (playerprojsize == 0) 
            newmis.movetype = MOVETYPE_FLYMISSILE;    // Large collision
        else newmis.movetype = MOVETYPE_FLY;        // Small collision
        // Check if the player has the nail piercer effect active
        if (self.moditems & IT_ARTPIERCE && newmis.classgroup == CG_PROJNAILS) {
            newmis.projeffect = IT_ARTPIERCE;
        }
    }
    // lasers/plasma are small and glowing
    else if (proj_type == CT_PROJ_LASER) {            
        newmis.movetype = MOVETYPE_FLY;
        newmis.effects = EF_DIMLIGHT;
    }
    // Crossbow bolts are small and stick around
    else if (proj_type == CT_PROJ_BOLT1) {
        newmis.movetype = MOVETYPE_FLY;
        newmis.bodyfadeaway = TRUE;
    }
    // Default = large collision
    else newmis.movetype = MOVETYPE_FLYMISSILE;

    //----------------------------------------------------------------------
    newmis.solid = SOLID_BBOX;
    newmis.touch = Touch_Projectile;
    newmis.nextthink = time + LIFE_PROJECTILE;
    newmis.think = SUB_Remove;
    newmis.poisonous = newmis.owner.poisonous;

    // Setup model
    //----------------------------------------------------------------------
    if (proj_type == CT_PROJ_NG) setmodel (newmis, MODEL_PROJ_NG);
    else if (proj_type == CT_PROJ_SNG) setmodel (newmis, MODEL_PROJ_SNG);
    else if (proj_type == CT_PROJ_REFNG) setmodel (newmis, MODEL_PROJ_NGRED);
    // Santa Snowball machine gun!
    else if (proj_type == CT_PROJ_SANTA) setmodel (newmis, MODEL_PROJ_SNOWBALL);
    // Crossbow Knight has poison/regular bolts
    else if (proj_type == CT_PROJ_BOLT1) {
        if (newmis.poisonous) setmodel (newmis, MODEL_PROJ_BOLT2);
        else setmodel (newmis, MODEL_PROJ_BOLT1);
    }
    // Hell Knight + Death Knight magic attack (can be poisonous)
    else if (proj_type == CT_PROJ_HKN) {
        if (newmis.poisonous) setmodel (newmis, MODEL_PROJ_HKNGRN);
        else setmodel (newmis, MODEL_PROJ_HKN);
    }
    // Death Lord - like Death Knight magic attack (spike ball instead)
    else if (proj_type == CT_PROJ_DLORD) {
        setmodel (newmis, MODEL_PROJ_DLORD1);
        // If can see enemy, steer the spike ball
        if (visible(self.enemy)) {
            newmis.enemy = self.enemy;
            newmis.nextthink = time + 0.1;
            newmis.think = self.th_updmissile;
        }
    }
    // Enforcer (can be poisonous)
    else if (proj_type == CT_PROJ_LASER) {
        if (newmis.poisonous) setmodel (newmis, MODEL_PROJ_LASERGRN);
        else setmodel (newmis, MODEL_PROJ_LASER);
        // If DP/FTE engine use new particle trail, glowing for Fitz engines
        if (ext_dppart) {
            if (newmis.poisonous) newmis.traileffectnum = particleeffectnum(DPP_TRPOISON);
            else newmis.traileffectnum = particleeffectnum(DPP_TRLASER);
            newmis.effects = 0;    // Remove any glows
        }
    }
    // Wraith fire bones (nails) can be poisonous as well
    else if (proj_type == CT_PROJ_WBONE) {
        if (newmis.poisonous) setmodel (newmis, MODEL_PROJ_WPOISON);
        else setmodel (newmis, MODEL_PROJ_WBONE);
    }
    // Wizards/Scrags can be poisonous and have a diff skin (head+gibs)
    // Cannot show change with projectile because its already green!
    else if (proj_type == CT_PROJ_WIZ) setmodel (newmis, MODEL_PROJ_WIZ);

    // mpoison = Darker Minotaur, always poisonous
    else if (proj_type == CT_PROJ_MPOISON) setmodel (newmis, MODEL_PROJ_MPOISON);

    // monng = Red Coloured Nails
    else if (proj_type == CT_PROJ_MONNG) setmodel (newmis, MODEL_PROJ_NGRED);
    else if (proj_type == CT_PROJ_SCORP) setmodel (newmis, MODEL_PROJ_SCORP);
    else if (proj_type == CT_PROJ_FURY2) setmodel (newmis, MODEL_PROJ_FURY2);
    else if (proj_type == CT_PROJ_NOUR1) setmodel (newmis, MODEL_PROJ_NOUR1);
    // Boglord / Fire Shambler nail/lightning attack
    else if (proj_type == CT_PROJ_BLBOLT) {
        if (self.spawnflags & MON_BOGL_STRONG) {
            newmis.classtype = CT_PROJ_BLBOLT2;
            setmodel (newmis, MODEL_PROJ_BLORDBOLT2);
        }
        else setmodel (newmis, MODEL_PROJ_BLORDBOLT1);
    }
    // Golem rock storm attack
    else if (proj_type == CT_PROJ_GROCK) {
        if (random() < 0.5) setmodel (newmis, MODEL_PROJ_GROCK1);
        else setmodel (newmis, MODEL_PROJ_GROCK2);
        newmis.frame = rint(random()*9);
        newmis.avelocity = vecrand(0,200,TRUE);
    }

    // Standard projectile setup (origin, size and velocity)
    //----------------------------------------------------------------------
    newmis.attack_speed = proj_speed;
    newmis.finalangle = dir;
    newmis.velocity = newmis.finalangle * newmis.attack_speed;
    newmis.angles = vectoangles(newmis.finalangle);
    setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);        
    setorigin (newmis, org);
};

/*======================================================================
 This missile touch function is designed for monsters NOT players
 * No DIRECT damage, just radius and a lot lower (40 vs 110)
 * Half damage to shamblers (inside T_RadiusDamage)
 * Will work with homing or direct missile attacks
 * checks for breakables that can be destroyed with explosives
 * uses default explosion function 
======================================================================*/
void() Touch_HomingMissile =
{
    if (self.touchedvoid) return;                // Marked for removal
    if (check_skycontent(self.origin)) {entity_remove(self, 0.1); return;}
    if (other.solid == SOLID_TRIGGER) return;    // trigger field, do nothing
    
    //----------------------------------------------------------------------
    // Check for breakable/pushable monster immunity
    if (ai_immunedamage(self.oldenemy, other)) {
        // Show ammo resistance on bmodel
        SpawnExplosion(EXPLODE_BURST_SMOKE,self.origin,SOUND_RESIST_ROCKET);
        SpawnProjectileSmoke(self.origin, 200, 50, 150);
        SpawnProjectileSmoke(self.origin, 200, 50, 250);
        SpawnProjectileSmoke(self.origin, 300, 50, 150);
    }
    else {
        //----------------------------------------------------------------------
        // Check for breakables that can be triggered
        if (ai_foundbreakable(self.oldenemy, other, TRUE) && other.brktrigmissile != 0) {
            // Found a breakable which is prone to explosive damage
            trigger_ent(other, self.oldenemy);
        }
        else {
            // Homing Missiles always kill any zombie class
            if (other.classgroup == CG_ZOMBIE) 
                T_Damage (other, world, world, DAMAGE_ZOMBIECLASS, NOARMOR);
            else {
                // Missile explosion base/rnd/splash
                self.pos1 = '0 0 0';
                if (self.classtype == CT_PROJ_SHUB1) self.pos1 = self.oldenemy.pos2;
                // Shalrath/DSerg are using classic voreball damage
                else self.pos1_z = DAMAGE_MONROCKET;
            
                // pre-calculate missile damage
                self.dmg = self.pos1_x + (random() * self.pos1_y);
                // Check for any rocket ammo resistance
                if (other.resist_rockets > 0) {
                    self.dmg = Resist_Damage(other, IT_ROCKETS, self.dmg);
                }

                // Only call T_ functions if there is damage to do!
                if (self.dmg > 0 && other.health > 0) 
                    T_Damage (other, self, self.oldenemy, self.dmg, DAMARMOR);
                
                if (self.pos1_z > 0) {
                    // Stop multiple enemies of the same class type (same as wraiths)
                    // killing themselves with their own homing missiles!
                    // Radius damage also applies poison debuff if present on attacker
                    T_RadiusDamage (self, self.oldenemy, self.pos1_z, self.oldenemy, IGNORECLASS);
                }
                else {
                    // Check for poison debuff (using poisonous flag)
                    if (self.poisonous == TRUE) PoisonDeBuff(other);
                }
            }
        }

        //----------------------------------------------------------------------
        // Rocket resistance is shown with puffs of smoke
        if (other.resist_rockets > 0) Resist_Rockets(other, self.origin);
        else {
            // Use special effect for voreball explosions in DP
            if (ext_dppart) {
                // DP effect name set before homing missile launched
                if (self.poisonous == TRUE) self.dpp_name = DPP_TEPOISON;
                pointparticles(particleeffectnum(self.dpp_name), self.origin, '0 0 0', 1);
                // Play standard explosion sound
                sound(self, CHAN_WEAPON, SOUND_REXP3, 1, ATTN_NORM);
            }
            else {
                if (self.poisonous) {
                    // New special coloured particle explosion (rogue expansion)
                    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
                    WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
                    WriteCoord (MSG_BROADCAST, self.origin_x);
                    WriteCoord (MSG_BROADCAST, self.origin_y);
                    WriteCoord (MSG_BROADCAST, self.origin_z);
                    WriteByte (MSG_BROADCAST, 51);
                    WriteByte (MSG_BROADCAST, 8);
                    // Sprite explosion for Fitz engines
                    SpawnExplosion(EXPLODE_POISON_SMALL, self.origin, SOUND_REXP3);
                }
                else {
                    // Standard explosion
                    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
                    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
                    WriteCoord (MSG_BROADCAST, self.origin_x);
                    WriteCoord (MSG_BROADCAST, self.origin_y);
                    WriteCoord (MSG_BROADCAST, self.origin_z);
                    // Sprite explosion for Fitz engines
                    SpawnExplosion(EXPLODE_SMALL, self.origin, SOUND_REXP3);
                }
            }
        }
    }

    // Hide+Delete homing missile, no longer needed
    // Wait for any sounds to finish playing
    entity_remove(self, 4);
};

//----------------------------------------------------------------------
// Re-direct any map hacks to the new function replacement
void() ShalMissileTouch = { Touch_HomingMissile(); };

//----------------------------------------------------------------------
void() Steer_HomingMissile =
{
    local vector    dir, vtemp;
    
    // If tracking enemies dies or end level? remove homing missiles
    if (self.enemy.health < 1) { remove(self); return; }
    if (intermission_running > 1) { remove(self); return; }

    vtemp = self.enemy.origin + self.v_angle;
    dir = normalize(vtemp - self.origin);
    self.velocity = dir * self.attack_speed;
    
    // After 1s let homing missiles impact on ower
    if (self.waitmin < time) self.owner = self;

    // Slow speed update that the missile can steer around corners
    // sloppy slow updates allows for a better game experience        
    self.nextthink = time + 0.2;
    self.think = Steer_HomingMissile;    
};

//----------------------------------------------------------------------
void(vector orgofs, vector targofs, float proj_type, float proj_speed) Launch_HomingMissile =
{
    local    vector    org, dir;
    local    float    dist, flytime;

    // Check if there is space to spawn entity
    makevectors(self.angles);
    org = self.origin + attack_vector(orgofs);
    if (entity_pcontent(org)) return;
    
    newmis = spawn ();
    // Owner is always excluded from touch functions, save in oldenemy
    // and after one second remove owner so will touch everything
    newmis.owner = newmis.oldenemy = self;
    newmis.classname = "proj_rocket";        // obj name, not really used anymore
    newmis.classtype = proj_type;            // Class type number, quick identity
    newmis.classgroup = CG_PROJROCKETS;        // Ammo type

    newmis.movetype = MOVETYPE_FLYMISSILE;    // Default = large collision
    newmis.enemy = self.enemy;                // Used for homing target
    newmis.v_angle = targofs;                // Store for later

    //----------------------------------------------------------------------
    // Aim the missile slightly above enemy
    dir = normalize((self.enemy.origin + newmis.v_angle) - org);
    dist = vlen (self.enemy.origin - org);
    flytime = dist * 0.002;
    if (flytime < 0.1) flytime = 0.1;

    //----------------------------------------------------------------------
    newmis.solid = SOLID_BBOX;
    newmis.touch = Touch_HomingMissile;
    newmis.nextthink = flytime + time;
    newmis.think = Steer_HomingMissile;
    newmis.attack_speed = proj_speed;
    // Allow 3s for the homing missile to travel away
    newmis.waitmin = time + 3;
    newmis.poisonous = newmis.owner.poisonous;

    // Setup model for each missile type
    //----------------------------------------------------------------------
    if (proj_type == CT_PROJ_SHAL) {
        if (newmis.poisonous) setmodel(newmis, MODEL_PROJ_SHALGRN);
        else setmodel(newmis, MODEL_PROJ_SHAL);
        newmis.dpp_name = DPP_TEVORESPIKE;
    }
    else if (proj_type == CT_PROJ_SERG) {
        if (newmis.poisonous) setmodel(newmis, MODEL_PROJ_SHALGRN);
        else setmodel(newmis, MODEL_PROJ_SERG);
        newmis.dpp_name = DPP_TEEXPLODE;
    }
    else if (proj_type == CT_PROJ_SHUB1) {
        setmodel(newmis, MODEL_PROJ_SHUB1);
        newmis.dpp_name = DPP_TEEXPLODE;
        // Push the homing missing up from center
        makevectors(self.angles);
        // The angles key will reverse direction
        // for up-side-down Shub
        dir = v_up*0.5;
    }

    // Standard projectile setup (origin, size and velocity)
    //----------------------------------------------------------------------
    newmis.velocity = dir * newmis.attack_speed;
    newmis.avelocity = vecrand(100,200,FALSE);
    newmis.angles = vectoangles(dir);
    setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);        
    setorigin (newmis, org);
};

/*======================================================================
 ROCKETS
 * Half damage to shamblers (T_MissileTouch and T_RadiusDamage)
 * checks for breakables that can be destroyed with explosives
 * All rockets have direct & radius damage (based on projectile type)
======================================================================*/
void() Touch_Missile =
{
    if (self.touchedvoid) return;                // Marked for removal
    if (check_skycontent(self.origin)) {entity_remove(self, 0.1); return;}
    if (other == self.owner) return;            // Touching self, do nothing
    if (other.solid == SOLID_TRIGGER) return;    // trigger field, do nothing
    if (self.oldenemy) {        // stop packs of owners killing each other
        if (other.classtype == self.oldenemy.classtype) return;    }
    // Guardian skull wizards have special poison explosion
    if (self.classtype == CT_PROJ_SKULLW && self.th_updmissile != SUB_Null) {
        self.th_updmissile(); return; }

    //----------------------------------------------------------------------
    // Check for breakable/pushable monster immunity
    if (ai_immunedamage(self.owner, other)) {
        // Show ammo resistance
        SpawnExplosion(EXPLODE_BURST_SMOKE,self.origin,SOUND_RESIST_ROCKET);
        SpawnProjectileSmoke(self.origin, 200, 50, 150);
        if (random() < 0.5) SpawnProjectileSmoke(self.origin, 200, 50, 250);
        if (random() < 0.5) SpawnProjectileSmoke(self.origin, 300, 50, 150);
    }
    else {
        //----------------------------------------------------------------------
        // Check for breakables that can be triggered
        if (ai_foundbreakable(self.owner, other, TRUE) && other.brktrigmissile != 0) {
            // Found a breakable which is prone to explosive damage
            trigger_ent(other, self.owner);
        }
        else {
            // Stop any updates to the missile velocity
            self.nextthink = time + 1;
            self.think = SUB_Null;
        
            // Setup rocket damage (vector = base + random and splash)
            if (self.classtype == CT_PROJ_ROCKET) self.pos1 = DAMAGE_RLPLAYER;
            // Boss ID - E1M7
            else if (self.classtype == CT_PROJ_LAVA) self.pos1 = DAMAGE_RLPLAYER;
            // BOSS Custom version with custom damage (self.pos2)
            else if (self.classtype == CT_PROJ_CHTHON) self.pos1 = self.owner.pos2;
            // BOSS Custom for ad_mountain (Chthon model)
            else if (self.classtype == CT_PROJ_FIRETOP) self.pos1 = DAMAGE_RLFIRETOP;
            // BOSS Custom for ad_magma (Stone demon model)
            else if (self.classtype == CT_PROJ_EIDO1) self.pos1 = DAMAGE_RLEIDO;
            // BOSS Custom for ad_azad (Ice Golem model)
            else if (self.classtype == CT_PROJ_ICEG1) self.pos1 = DAMAGE_RLICEG;
            // BOSS Custom for ad_sepulcher (Fire Shambler)
            else if (self.classtype == CT_PROJ_BLORD2) self.pos1 = DAMAGE_RLBLORD;
            // BOSS Custom for ad_sepulcher (Green Shambler)
            else if (self.classtype == CT_PROJ_BLORD) {
                self.pos1 = DAMAGE_RLBLORD;
                // Don't spawn gib models at impact (could be solid)
                self.oldorigin = self.origin;
                self.origin = self.origin - 8*normalize(self.velocity);
                // Special gib model, frame and movement pattern
                self.gib1mdl = MODEL_PROJ_BLORD1S;
                self.gib1frame = 9;
                // Throw gib quantity based on skill level
                ThrowGib(11, rint(0.5 + random()*3) );
                // Restore origin for rest of rocket impact
                self.origin = self.oldorigin;
            }
            else if (self.classtype == CT_PROJ_ARMY) self.pos1 = DAMAGE_RLARMY;
            else if (self.classtype == CT_PROJ_DGUARDQ) self.pos1 = DAMAGE_RLDGUARDQ;
            else if (self.classtype == CT_PROJ_DROLE) self.pos1 = DAMAGE_RLDROLE;
            else if (self.classtype == CT_PROJ_FURY1) self.pos1 = DAMAGE_RLFURY;
            else if (self.classtype == CT_PROJ_GARG) self.pos1 = DAMAGE_RLGARG;
            else if (self.classtype == CT_PROJ_JIM2) self.pos1 = DAMAGE_RLJIM2;
            else if (self.classtype == CT_PROJ_SKULLW) self.pos1 = DAMAGE_RLSKULLW;
            else if (self.classtype == CT_PROJ_SEEKER) self.pos1 = DAMAGE_RLSEEKER;
            else if (self.classtype == CT_PROJ_RAINDEER) self.pos1 = DAMAGE_RLRAINDEER;

            // Instant death explosion for zombies from rocket explosions!
            // Is doesn't make sense that this is not a default for explosives
            // This will affect _zombie, _zombiek, _mummy monster types 
            // The damage is direct, no splashdamage just in case anything close by
            if (other.classgroup == CG_ZOMBIE) self.pos1 = DAMAGE_RLKILLZOM;

            // pre-calculate rocket damage
            self.dmg = self.pos1_x + (random() * self.pos1_y);
            // Check for any rocket ammo resistance
            if (other.resist_rockets > 0) {
                self.dmg = Resist_Damage(other, IT_ROCKETS, self.dmg);
            }
            
            // Only call T_ functions if there is damage to do!
            if (self.dmg > 0 && other.health > 0) 
                T_Damage (other, self, self.owner, self.dmg, DAMARMOR);
            if (self.pos1_z > 0) 
                T_RadiusDamage (self, self.owner, self.pos1_z, other, DAMAGEALL);
        }
        
        // Rocket resistance is shown with puffs of smoke
        if (other.resist_rockets > 0) Resist_Rockets(other, self.origin);
        else {
            //----------------------------------------------------------------------
            // Check for poison debuff (using poisonous flag)
            if (self.poisonous == TRUE) {
                // Use new poison explosion
                self.height = EXPLODE_POISON_MED;
                // Poisonous projectiles
                PoisonDeBuff(other);
            }

            // Move the explosion effect higher up from point of contact
            self.origin = self.origin - 8*normalize(self.velocity);

            // Default ID particle explosion for fire explosions
            if (self.height < EXPLODE_PLASMA_SMALL) {
                WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
                WriteByte (MSG_BROADCAST, TE_EXPLOSION);
                WriteCoord (MSG_BROADCAST, self.origin_x);
                WriteCoord (MSG_BROADCAST, self.origin_y);
                WriteCoord (MSG_BROADCAST, self.origin_z);
            }

            // Play original explosion sound or replacement
            if (self.noise == "") self.noise = SOUND_REXP3;
            SpawnExplosion(self.height, self.origin, self.noise);
        }
    }
    
    // Hide + Delete rocket, no longer needed
    entity_remove(self, 1);
};

//----------------------------------------------------------------------
// Re-direct any map hacks to the new function replacement
void() T_MissileTouch = { Touch_Missile(); };

//----------------------------------------------------------------------
void(vector org, vector dir, vector avel, float proj_type, float proj_speed) Launch_Missile =
{
    // Check if there is space to spawn entity
    if (entity_pcontent(org)) return;
    
    newmis = spawn ();
    newmis.owner = self;
    newmis.classname = "proj_rocket";    // obj name, not really used anymore
    newmis.classtype = proj_type;        // Class type number, quick identity
    newmis.classgroup = CG_PROJROCKETS;    // Ammo type

    // Player rockets need to take into account autoaim
    if (self.classtype == CT_PLAYER) {
        if (playerprojsize == 0) 
            newmis.movetype = MOVETYPE_FLYMISSILE;    // Large collision
        else newmis.movetype = MOVETYPE_FLY;        // Small collision
    }
    else {
        // default monster missile width is thin!
        newmis.movetype = MOVETYPE_FLY;
        // Horrible exception for the missile touch function
        // Gargoyle rockets need to pass through other gargoyles
        // otherwise a pack of them would kill each other!
        if (self.classtype == CT_MONGARGOYLE) newmis.oldenemy = self;
    }

    //----------------------------------------------------------------------
    newmis.solid = SOLID_BBOX;
    newmis.touch = Touch_Missile;
    newmis.nextthink = time + LIFE_ROCKET;
    newmis.think = SUB_Remove;
    newmis.height = EXPLODE_SMALL;
    newmis.poisonous = newmis.owner.poisonous;

    // Setup model for each missile type
    //----------------------------------------------------------------------
    if (proj_type == CT_PROJ_ROCKET) setmodel(newmis, MODEL_PROJ_ROCKET);
    // Monsters with poisonous flag awareness
    else if (proj_type == CT_PROJ_GARG) {
        if (newmis.poisonous) setmodel(newmis, MODEL_PROJ_GARGOYLEGRN);
        else setmodel(newmis, MODEL_PROJ_GARGOYLE);
    }
    else if (proj_type == CT_PROJ_FURY1) {
        if (newmis.poisonous) setmodel(newmis, MODEL_PROJ_SHALGRN);
        else setmodel(newmis, MODEL_PROJ_FURY1);
    }
    else if (proj_type == CT_PROJ_CHTHON) {
        if (self.spawnflags & MON_CHTHON_GREEN) setmodel (newmis, MODEL_PROJ_SLIME);
        else setmodel (newmis, MODEL_PROJ_LAVA);
    }
    else if (proj_type == CT_PROJ_ARMY) {
        if (newmis.poisonous) setmodel(newmis, MODEL_PROJ_ROCKETGRN);
        else setmodel(newmis, MODEL_PROJ_ROCKET);
        newmis.noise = "soldier/rocket_hit.wav";
        newmis.nextthink = time + 0.1;
        newmis.think = self.th_updmissile;
        // If can see enemy, steer the rocket towards them
        if (visible(self.enemy)) newmis.enemy = self.enemy;
    }
    else if (proj_type == CT_PROJ_DGUARDQ) {
        setmodel (newmis, MODEL_PROJ_DGUARDQ);
        newmis.noise = "dguard/hit.wav";
    }
    else if (proj_type == CT_PROJ_DROLE) {
        if (newmis.poisonous) setmodel (newmis, MODEL_PROJ_DROLEGRN);
        else setmodel (newmis, MODEL_PROJ_DROLE);
        newmis.noise = "drole/r_explode.wav";
        newmis.nextthink = time + 0.025;
        newmis.think = self.th_updmissile;
    }
    else if (proj_type == CT_PROJ_SKULLW) {
        // Start with SUB_Null because its tested by impact function
        newmis.th_updmissile = SUB_Null;
        if (newmis.poisonous) {
            setmodel (newmis, MODEL_PROJ_SWSKULLP);
            // Copy over poison explosive function just incase
            // Skull wizard dies before projectile hits something
            newmis.th_updmissile = self.th_updmissile;
        }
        // Classic skullwizard flaming skull projectile!
        else setmodel(newmis, MODEL_PROJ_SWSKULL);
    }

    // All robot and boss monsters block poisonous flag
    else if (proj_type == CT_PROJ_JIM2) {
        setmodel(newmis, MODEL_PROJ_ROCKET);
        newmis.noise = "jim/rocket_hit.wav";
    }
    else if (proj_type == CT_PROJ_SEEKER) {
        setmodel(newmis, MODEL_PROJ_ROCKET);
        newmis.noise = "seeker/rocket_hit.wav";
    }
    else if (proj_type == CT_PROJ_RAINDEER) {
        setmodel(newmis, MODEL_PROJ_RAIND);
        newmis.noise = "xmas/raindeer/hit.wav";
    }
    else if (proj_type == CT_PROJ_LAVA) setmodel (newmis, MODEL_PROJ_LAVA);
    else if (proj_type == CT_PROJ_FIRETOP) setmodel (newmis, MODEL_PROJ_LAVA);
    else if (proj_type == CT_PROJ_EIDO1) {
        setmodel(newmis, MODEL_PROJ_EIDO1);
        newmis.frame = 7;
        if (random() < 0.5) newmis.noise = "eidolon/rock_hit1.wav";
        else newmis.noise = "eidolon/rock_hit2.wav";
        if (random() < 0.5) newmis.height = EXPLODE_MED;
    }
    else if (proj_type == CT_PROJ_ICEG1) {
        setmodel (newmis, MODEL_PROJ_GSHARD);
        newmis.noise = "golem/iceshard_impact.wav";
        newmis.height = EXPLODE_ICE_BIG;
        newmis.nextthink = time + 0.1;
        newmis.think = self.th_updmissile;
        newmis.pos1 = self.enemy.origin;
        newmis.attack_timer = 0;
    }
    else if (proj_type == CT_PROJ_BLORD) {
        // If the boglord dies before the impact of the 'rocket'
        // then the self.owner field will be invalid. 
        // Setup the 'correct' projectile type beforehand instead!
        if (self.spawnflags & MON_BOGL_STRONG) {
            newmis.classtype = CT_PROJ_BLORD2;
            setmodel (newmis, MODEL_PROJ_BLORD2B);
        }
        else setmodel (newmis, MODEL_PROJ_BLORD1B);
        newmis.frame = 7; // Large size
        newmis.noise = "boglord/slime_explode.wav";
    }

    // Standard projectile setup (origin, size and velocity)
    //----------------------------------------------------------------------
    newmis.velocity = dir * proj_speed;
    newmis.avelocity = avel;
    newmis.angles = vectoangles(newmis.velocity);
    setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
    setorigin (newmis, org);
};

/*======================================================================
 GRENADES
 * Half damage to shamblers (T_MissileTouch and T_RadiusDamage)
 * checks for breakables that can be destroyed with explosives
 * Grenade damage is based on dmg key (default = player)
======================================================================*/
void() Explode_Grenade =
{
    // Block touch functions
    if (self.state == STATE_DISABLED) return;
    self.touch = SUB_Null;
    self.state = STATE_DISABLED;
    
    // Customize damage based on projectile type
    if (self.classtype == CT_PROJ_GLMON) self.dmg = DAMAGE_MONGRENADE;
    else if (self.classtype == CT_PROJ_MEGG) self.dmg = 0;
    else if (self.classtype == CT_PROJ_NOUR2) {
        self.dmg = DAMAGE_MONGRENADE;
        // Don't spawn gib models at impact (could be solid)
        self.oldorigin = self.origin;
        self.origin = self.origin - 8*normalize(self.velocity);
        // Special gib model, frame and movement pattern
        self.gib1mdl = MODEL_PROJ_NOUR2S;
        self.gib1sound = GIB_IMPACT_ACID;
        self.max_health = MON_GIBFOUNTAIN;
        self.gib1frame = 9;
        self.gibtype = GIBTYPE_POISON;
        // Setup particles from gib on floor
        self.gibpartstyle = PARTICLE_BURST_YELLOW;
        // Setup damage and impact explosion
        self.gib1dmg = rint( 2 + (random() * skill) );
        self.gib1exp = EXPLODE_BURST_POISON;
        // Throw gib quantity based on skill level
        ThrowGib(11, rint( 1 + random() * (1 + skill) ));
        // Restore origin for rest of grenade impact
        self.origin = self.oldorigin;
        self.noise = "nour/explode2.wav";
    }
    // Default player grenade
    else if (!self.dmg) self.dmg = DAMAGE_PLAYGRENADE; 
    
    // Rocket resistance is reduced in RadiusDamage
    if (self.dmg > 0) T_RadiusDamage (self, self.owner, self.dmg, world, DAMAGEALL);

    // Show Rocket resistance with puffs of smoke
    if (other.resist_rockets > 0) Resist_Rockets(other, self.origin);
    else {
        //----------------------------------------------------------------------
        // Check for poison debuff (using poisonous flag)
        if (self.poisonous == TRUE) {
            // Use new poison explosion
            self.height = EXPLODE_POISON_MED;
            // Poisonous projectiles
            PoisonDeBuff(other);
        }

        // Default ID particle explosion for fire explosions
        if (self.height < EXPLODE_PLASMA_SMALL) {
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, TE_EXPLOSION);
            WriteCoord (MSG_BROADCAST, self.origin_x);
            WriteCoord (MSG_BROADCAST, self.origin_y);
            WriteCoord (MSG_BROADCAST, self.origin_z);
        }

        // Play original explosion sound
        SpawnExplosion(self.height, self.origin, SOUND_REXP3);

    }
    // Hide grenade, no longer needed
    entity_remove(self, 1);
};

//----------------------------------------------------------------------
void() Touch_Grenade =
{
    if (entity_pcontent(self.origin)) {remove(self); return;}
    if (self.touchedvoid) return;                // Marked for removal
    if (other == self.owner) return;            // Touching self, do nothing
    if (other.solid == SOLID_TRIGGER) return;    // trigger field, do nothing
    
    // Let minion eggs re-bounce in opposite direction
    // Don't want them to explode like grenades
    if (self.classtype == CT_PROJ_MEGG && other.takedamage == DAMAGE_AIM) {
        if (CheckZeroVector(self.velocity) == FALSE) {
            self.angles = vectoangles(self.mangle);
            self.angles_y = anglemod(self.angles_y + 180);
            self.velocity = -self.mangle;    // Reverse direction
            self.mangle = self.velocity;    // Update new direction
            sound (self, CHAN_WEAPON, self.noise, 1, ATTN_NORM);
            // Reset egg timer if still bouncing a lot
            if ( vlen(self.velocity) > 100 ) self.nextthink = time + LIFE_EGG;
        }
    }
    else {
        //----------------------------------------------------------------------
        // Hit something that bleeds? (allows for grenade impact)
        if (other.takedamage == DAMAGE_AIM) {Explode_Grenade(); return;}
        else {
            //----------------------------------------------------------------------
            // Check for breakables that can be triggered
            // Any monster firing a grenade at a breakable will explode without bounce
            if (ai_foundbreakable(self.owner, other, TRUE) && (self.owner.flags & FL_MONSTER)) {
                // Found a breakable which is prone to explosive damage
                trigger_ent(other, self.owner);
                Explode_Grenade();
                return;
            }
            
            // bounce sound and stop spinning
            sound (self, CHAN_WEAPON, self.noise, 1, ATTN_NORM);
            if (CheckZeroVector(self.velocity)) self.avelocity = '0 0 0';
        }
    }
};

//----------------------------------------------------------------------
// Re-direct any map hacks to the new function replacement
void() OgreGrenadeExplode = { self.classtype = CT_PROJ_GLMON; Explode_Grenade(); };
void() GrenadeExplode = { self.classtype = CT_PROJ_GLMON; Explode_Grenade(); };
void() GrenadeTouch = { self.classtype = CT_PROJ_GLMON; Touch_Grenade(); };

// Compile forward functions for Launch_Grenade
void() Touch_ShellCasing;

//----------------------------------------------------------------------
void(vector org, vector dir, vector avel, float proj_type) Launch_Grenade =
{
    // Check if there is space to spawn entity
    if (entity_pcontent(org)) return;
    
    newmis = spawn ();
    newmis.owner = self;
    newmis.classname = "proj_grenade";    // obj name, not really used anymore
    newmis.classtype = proj_type;        // Class type number, quick identity
    newmis.classgroup = CG_PROJROCKETS;    // Ammo type
    newmis.movetype = MOVETYPE_BOUNCE;

    //----------------------------------------------------------------------
    newmis.solid = SOLID_BBOX;
    newmis.touch = Touch_Grenade;
    newmis.nextthink = time + LIFE_GRENADE;
    newmis.think = Explode_Grenade;
    newmis.noise = "weapons/bounce.wav";
    newmis.bbmins = newmis.bbmaxs = VEC_ORIGIN;
    newmis.height = EXPLODE_SMALL;
    newmis.poisonous = newmis.owner.poisonous;

    // Setup model for each missile type
    //----------------------------------------------------------------------
    // proj_gl is used by the player (separate so easier to change)
    if (proj_type == CT_PROJ_GL) setmodel(newmis, MODEL_PROJ_GRENADE);
    // proj_glmon is used by any monster firing grenades
    else if (proj_type == CT_PROJ_GLMON) {
        if (newmis.poisonous) setmodel(newmis, MODEL_PROJ_GRENADEGRN);
        else setmodel(newmis, MODEL_PROJ_GRENADE);
    }
    else if (proj_type == CT_PROJ_FLESH) {
        if (newmis.poisonous) setmodel(newmis, MODEL_PROJ_FLESHP);
        else setmodel (newmis, MODEL_PROJ_FLESH);
    }
    else if (proj_type == CT_PROJ_MEGG) {
        if (self.classtype == CT_MONWRAITH) {
            setmodel( newmis, MODEL_PROJ_WEGG);
            newmis.noise = "wraith/bounce.wav";
        }
        else if (self.classtype == CT_MONSHAL) {
            setmodel( newmis, MODEL_PROJ_SEGG);
            newmis.noise = "shalrath/bounce.wav";
        }
        newmis.classgroup = CG_MINIONEGG;        // Proper group type
        newmis.enemy = SUB_entEnemyTarget();    // Make sure got right enemy
        newmis.bbmins = VEC_HULLE_MIN;            // Small egg size
        newmis.bbmaxs = VEC_HULLE_MAX;
        newmis.frame = self.attachment.frame;    // Current ball size (frame)
        newmis.think = Hatch_Egg;                // Eventually hatch
        newmis.nextthink = time + LIFE_EGG;        // Short timer
    }
    else if (proj_type == CT_PROJ_SPID) {
        setmodel (newmis, MODEL_PROJ_SPID);
        newmis.frame = rint((random() * 9));
    }
    else if (proj_type == CT_PROJ_ELF) {
        setmodel (newmis, MODEL_PROJ_ELF);
        newmis.frame = rint((random() * 9));
    }
    else if (proj_type == CT_PROJ_SWAMP) {
        setmodel (newmis, MODEL_PROJ_SWAMP);
        newmis.frame = rint((random() * 9));
    }
    else if (proj_type == CT_PROJ_VORE) {
        setmodel (newmis, MODEL_PROJ_VORE);
        newmis.frame = rint((4 + random() * 4));
    }
    else if (proj_type == CT_PROJ_NOUR2) {
        setmodel (newmis, MODEL_PROJ_NOUR2B);
        newmis.skin = self.skin;
        newmis.frame = 7; // Large size
        newmis.noise = "nour/bounce.wav";
    }
    else if (proj_type == CT_PROJ_CHTHON2) {
        setmodel (newmis, MODEL_PROJ_CHTHON1);
        newmis.skin = self.skin;
        newmis.frame = rint((random() * 9));
        newmis.noise = "chthon/bounce.wav";
        newmis.dmg = self.pos3_z;
    }
    else if (proj_type == CT_PROJ_SHUB2) {
        setmodel (newmis, MODEL_PROJ_SHUB2);
        newmis.frame = rint((random() * 9));
        newmis.noise = "shub/bounce.wav";
        newmis.dmg = self.pos3_z;
    }
    else if (proj_type == CT_PROJ_SHELLC) {
        setmodel(newmis, MODEL_PROJ_SHELLC);
        newmis.touch = Touch_ShellCasing;
        newmis.think = model_fade;
        newmis.nextthink = time + random() + LIFE_SHELLS;
        newmis.ltime = newmis.nextthink;
    }
    
    // These projectile fly like grenades but are really spikes!
    //----------------------------------------------------------------------
    if (proj_type == CT_PROJ_SPID || proj_type == CT_PROJ_VORE ||
        proj_type == CT_PROJ_SWAMP || proj_type == CT_PROJ_ELF ||
        proj_type == CT_PROJ_FLESH) {
        newmis.touch = Touch_Projectile;
        newmis.nextthink = time + LIFE_PROJECTILE;
        newmis.think = SUB_Remove;
        newmis.bodyfadeaway = TRUE;
    }
    
    // Standard projectile setup (origin, size and velocity)
    //----------------------------------------------------------------------
    newmis.mangle = dir;    // Save for later
    newmis.velocity = newmis.mangle;
    newmis.avelocity = avel;
    newmis.angles = vectoangles(newmis.velocity);
    setsize (newmis, newmis.bbmins, newmis.bbmaxs);
    setorigin (newmis, org);
};

//======================================================================
// Generic functions for firing grenades from monsters
// MonsterGrenadeSound = play generic load grenade sound
// MonsterGrenadeSpeed = return generic attack speed
// MonsterFireGrenade = fire grenade at enemy origin
//
//----------------------------------------------------------------------
void() MonsterGrenadeSound =
{ sound (self, CHAN_WEAPON, "weapons/gl_loadshort.wav", 0.1+random()*0.5, ATTN_LOW); };
float() MonsterGrenadeSpeed =
{ return SPEED_MONGRENADE + (skill * SPEED_MONGLSKILL); };

//----------------------------------------------------------------------
void(vector grenade_org, vector grenade_enemyorg) MonsterFireGrenade =
{
    local vector ang, dir, avel;
    
    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "weapons/grenade.wav", 1, ATTN_NORM);

    // Is Z aware disabled?
    if ( query_configflag(SVR_ZAWARE) || self.no_zaware ) {
        makevectors (self.angles);
        dir = normalize(grenade_enemyorg - grenade_org);
        // Default grenade speed (player = 600)
        dir = dir * SPEED_PLAYGRENADE;
        dir_z = ELEV_ZAXIS;
    }
    else {
        // Z Aware tracking is ENABLED (AI track player much better)
        // One final angle adjustment (based on actual projectile origin)
        self.attack_speed = MonsterGrenadeSpeed();
        self.attack_elev = SUB_Elevation(self.attack_elev, grenade_org, grenade_enemyorg, self.attack_speed);
        ang = vectoangles(grenade_enemyorg - grenade_org);
        ang_x = -self.attack_elev;
        makevectors (ang);
         dir = v_forward * self.attack_speed;
    }

    avel = vecrand(100,200,FALSE);
    Launch_Grenade(grenade_org, dir, avel, CT_PROJ_GLMON);
};

/*======================================================================
 Ejecting Shell Casing for SG/SSG/Upgrade
======================================================================*/
void() Touch_ShellCasing =
{
    if (self.touchedvoid) return;                // Marked for removal
    if (check_skycontent(self.origin)) {entity_remove(self, 0.1); return;}
    if (other == self.owner) return;            // Touching self, do nothing
    if (other.solid == SOLID_TRIGGER) return;    // trigger field, do nothing

    self.touch = SUB_Null;
    if (random() < 0.5)  
        sound(self, CHAN_VOICE, "weapons/shellc.wav", random()*0.25, ATTN_LOW);        
};

//----------------------------------------------------------------------
void(float shell_qty) Launch_ShellCasing =
{
    local vector org, dir, avel;
    
    if ( self.health < 1 ) return;
    if (query_configflag(SVR_SHOTGCASE)) return;

    if (self.flags & FL_CLIENT) makevectors (self.v_angle);
    else makevectors (self.angles);
    org = self.origin + v_up*10;
    
    while (shell_qty > 0) {
        dir = -v_right*75 + v_forward*(random()*50) + v_up*(100 + random()*100);
        avel = vecrand(0,300,TRUE);
        Launch_Grenade(org, dir, avel, CT_PROJ_SHELLC);
        shell_qty = shell_qty - 1;
    }
};
