void(entity inflictor, entity attacker, float damage) tbaby_mitosis;

void() tbaby_checknew = {
    if (!walkmove(0,0)) { remove(self); return; }
    
    self.enemy = self.enemy;
    self.th_stand = tbaby_stand1;
    self.th_walk = tbaby_walk1;
    self.th_run = tbaby_run1;
    self.th_missile = tbaby_jump1;
    self.th_melee = tbaby_jump1;
    self.th_die = tbaby_die1;       
    self.origin_z = self.origin_z + 1;      // raise off floor a bit
    droptofloor();

    if (!walkmove(0,0)) { self.owner.th_pain = tbaby_mitosis; remove(self); return; }

    self.takedamage = DAMAGE_AIM;
    self.ideal_yaw = self.angles * '0 1 0';
    if (!self.yaw_speed) self.yaw_speed = 20;
    self.view_ofs = '0 0 25';
    self.use = monster_use;
    self.flags = self.flags | FL_MONSTER;
    if (self.deathstring == "") self.deathstring = " was slimed by a Fresh Spawn\n";
    self.pausetime = 99999999;
    self.th_stand ();
    self.nextthink = self.nextthink + random()*0.5;
    total_monsters = total_monsters + 1;  
    
    WriteByte (MSG_BROADCAST, SVC_UPDATESTAT);
    WriteByte (MSG_BROADCAST, STAT_TOTALMONSTERS);
    WriteLong (MSG_BROADCAST, total_monsters);
};

void(entity inflictor, entity attacker, float damage) tbaby_mitosis = {
    local entity    newSpawn;
    
    if ( random() < 0.35 ) {
        newSpawn = spawn();
        newSpawn.classname = self.classname;
        newSpawn.movetype = MOVETYPE_STEP;
        setsize (newSpawn, '-16 -16 -24', '16 16 40');
        newSpawn.health = 80;
        //if (self.skin == 1)
            //newSpawn.health = 60;
        //else if (self.skin == 2)
            newSpawn.health = 100;

        newSpawn.owner = self;
        self.th_pain = SUB_Null_pain;
        setmodel (newSpawn, self.owner.mdl);
        newSpawn.solid = SOLID_SLIDEBOX;
        newSpawn.skin = self.skin;
        setorigin (newSpawn, self.origin);              
        newSpawn.think = tbaby_checknew;
        newSpawn.nextthink = time + 0.5;
            sound (newSpawn, CHAN_VOICE, "blob/mytosis.wav", 1, ATTN_NORM);
    }
};

void() monster_tarbaby_mitosis = {
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_tarbaby.mdl";
    self.headmdl = string_null;
    precache_model (self.mdl);
    self.idle_sound = "blob/sight1.wav";
    precache_sound (self.idle_sound);
    self.sight_sound = self.idle_sound;
    self.pain_sound = self.idle_sound;
    precache_sound ("blob/death1.wav");
    precache_sound ("blob/hit1.wav");
    precache_sound ("blob/land1.wav");
    precache_sound ("blob/sight1.wav");
    precache_sound ("blob/mytosis.wav");

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 100;
    if (self.skin < 0) self.skin = 1;
    self.gibhealth = MON_NEVERGIB;        // Cannot be gibbed by weapons
    self.gibbed = FALSE;
    self.pain_flinch = 20;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    if (self.deathstring == "") self.deathstring = " was slimed by a Mitosis Spawn\n";

    self.th_checkattack = SpawnCheckAttack;
    self.th_stand = tbaby_stand1;
    self.th_walk = tbaby_walk1;
    self.th_run = tbaby_run1;
    self.th_jump = tbaby_jump1;
    self.th_melee = tbaby_jump1;
    self.th_pain = tbaby_mitosis;
    self.th_die = tbaby_die;
    
    self.classtype = CT_MONSPAWN;
    self.classgroup = CG_SPAWN;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};