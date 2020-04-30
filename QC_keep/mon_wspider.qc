void() SpidMissile;

void() spid_stand = [0, spid_stand] { ai_stand(); };

void() spid_walk1 = [0, spid_walk2] { if (random() < 0.2) sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE); ai_walk(MOVETYPE_STEP); };
void() spid_walk2 = [1, spid_walk3] { sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE); ai_walk(MOVETYPE_STEP); };
void() spid_walk3 = [2, spid_walk4] { ai_walk(MOVETYPE_STEP); };
void() spid_walk4 = [3, spid_walk5] { sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE); ai_walk(MOVETYPE_STEP); };
void() spid_walk5 = [4, spid_walk6] { ai_walk(MOVETYPE_STEP); };
void() spid_walk6 = [5, spid_walk7] { sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE); ai_walk(MOVETYPE_STEP); };
void() spid_walk7 = [6, spid_walk8] { ai_walk(MOVETYPE_STEP); };
void() spid_walk8 = [7, spid_walk9] { sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE); ai_walk(MOVETYPE_STEP); };
void() spid_walk9 = [8, spid_walk10] { ai_walk(MOVETYPE_STEP); };
void() spid_walk10 = [9, spid_walk1] { sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE); ai_walk(MOVETYPE_STEP); };

void() spid_run1 = [0, spid_run2] { if (random() < 0.2) sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE); ai_run(12); };
void() spid_run2 = [2, spid_run3] { sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE); ai_run(12); };
void() spid_run3 = [4, spid_run4] { ai_run(12); };
void() spid_run4 = [6, spid_run5] { sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE);ai_run(12); };
void() spid_run5 = [8, spid_run6] { ai_run(12); };
void() spid_run6 = [0, spid_run7] { sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE);ai_run(12); };
void() spid_run7 = [2, spid_run8] { ai_run(12); };
void() spid_run8 = [4, spid_run9] { sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE);ai_run(12); };
void() spid_run9 = [6, spid_run10] { ai_run(12); };
void() spid_run10 = [8, spid_run1] { sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE);ai_run(12); };

void() spid_attack1 = [0, spid_attack2] { sound(self, CHAN_VOICE, "spider/fire.wav", 1, ATTN_NORM); ai_charge(12); };
void() spid_attack2 = [2, spid_attack3] { ai_charge(12); };
void() spid_attack3 = [4, spid_attack4] { ai_charge(12); };
void() spid_attack4 = [6, spid_attack5] { ai_charge(12); };
void() spid_attack5 = [8, spid_attack6] { ai_charge(12); };
void() spid_attack6 = [0, spid_attack7] { ai_charge(12); };
void() spid_attack7 = [2, spid_attack8] { ai_charge(12); if (skill > 1) SpidMissile(); };
void() spid_attack8 = [4, spid_attack9] { ai_charge(12); sound(self, CHAN_VOICE, "spider/tap.wav", 1, ATTN_IDLE); };
void() spid_attack9 = [6, spid_attack10] { ai_charge(12); };
void() spid_attack10 = [8, spid_run1] { SpidMissile();};

void() spid_pain1 = [20, spid_pain2] {};
void() spid_pain2 = [21, spid_pain3] {};
void() spid_pain3 = [22, spid_pain4] {};
void() spid_pain4 = [23, spid_pain5] {};
void() spid_pain5 = [24, spid_pain6] {};
void() spid_pain6 = [25, spid_pain7] {};
void() spid_pain7 = [26, spid_pain8] {};
void() spid_pain8 = [27, spid_pain9] {};
void() spid_pain9 = [28, spid_pain10] {};
void() spid_pain10 = [29, spid_pain11] {};
void() spid_pain11 = [30, spid_run1] {};

void(entity inflictor, entity attacker, float damage) spid_pain = {
    if (self.pain_finished > time) return;

    sound(self, CHAN_VOICE, "spider/pain.wav", 1, ATTN_NORM);
    self.velocity_x = -50 - 50 * random();
    self.velocity_y = 0;
    self.velocity_z = MIN_CHARMER_DISTANCE + MIN_CHARMER_DISTANCE * random();
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.pain_finished = time + MOVETYPE_WALK;
    spid_pain1();
    
};

void() spid_death1 = [10, spid_death3] {};
void() spid_death2 = [10, spid_death3] {monster_check_gib();};
void() spid_death3 = [11, spid_death4] {monster_check_gib(); self.solid = SOLID_NOT; };
void() spid_death4 = [12, spid_death5] {};
void() spid_death5 = [13, spid_death6] {};
void() spid_death6 = [14, spid_death7] {};
void() spid_death7 = [15, spid_death8] {};
void() spid_death8 = [16, spid_death9] {};
void() spid_death9 = [17, spid_death10] {};
void() spid_death10 = [18, spid_death11] {monster_death_postcheck();};
void() spid_death11 = [19, spid_death11] {monster_deadbody_check();};

void() spid_die = {
    monster_death_precheck();

    if (!self.gibbed) {
        sound(self, CHAN_VOICE, "shalrath/death.wav", 1, ATTN_NORM);
        self.movetype = MOVETYPE_BOUNCE;
        self.velocity_x = -50 + 100 * random();
        self.velocity_y = -50 + 100 * random();
        self.velocity_z = 300 + 300 * random();
        self.avelocity_x = -20 + 40 * random();
        self.avelocity_y = -200 + 400 * random();
        self.avelocity_z = 0;
        self.flags = self.flags - (self.flags & FL_ONGROUND);
        spid_death1();
        
    }
};

float() CheckSpidJump =  {
    local vector dist;
    local float d;

    if (self.origin_z + self.mins_z > self.enemy.origin_z + self.enemy.mins_z + 0.75 * self.enemy.size_z)
        return 0;

    if (self.origin_z + self.maxs_z < self.enemy.origin_z + self.enemy.mins_z + 0.25 * self.enemy.size_z)
        return 0;

    dist = self.enemy.origin - self.origin;
    dist_z = 0;
    d = vlen(dist);
    if (d < 100) return 0;

    if (d > MAX_CHARMER_DISTANCE)
        if (random() < 0.5) return 0;

    return 1;
};

void() spid_jump = {
    if (!FL_ONGROUND) return;

    ai_face();
    makevectors(self.angles);
    self.origin_z = self.origin_z + 1;
    self.velocity = v_forward * MIN_CHARMER_DISTANCE + '0 0 600';
    if (self.flags & FL_ONGROUND) self.flags = self.flags - FL_ONGROUND;
};

void() spid_attack = {
    if (CheckSpidJump)
        spid_jump();

    spid_attack1();
};

void() SpidMissile = {
    local vector org;
    local vector dir;
    local vector yrand, zrand;

    ai_face();
    makevectors(self.angles);
    org = self.origin + v_forward*20;
    
    yrand = (crandom()*12)*v_right;
    zrand = ((crandom()*4)-16)*v_up;
    org = self.origin + attack_vector(self.attack_offset) + yrand + zrand;
    
    dir = (self.enemy.origin - org);
    dir = normalize(dir);
    launch_projectile (org, dir, CT_PROJ_WILLY, 300);
};

void() monster_wspider = {
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/spider.mdl";
    self.headmdl = "progs/gib2.mdl";
    self.gib1mdl = "progs/ad171/gib_spidlegg.mdl";    // Single Leg
    precache_model ("progs/ad171/gib_spidlegg.mdl");
    precache_model ("progs/gib2.mdl");
    precache_model2("progs/spider.mdl");
    precache_model2("progs/sp_spike.mdl");
    precache_sound2("spider/fire.wav");
    precache_sound2("shalrath/attack2.wav");
    precache_sound2("shalrath/death.wav");
    precache_sound2("spider/tap.wav");
    precache_sound2("spider/pain.wav");
    precache_sound2("shalrath/sight.wav");

    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.model = "progs/spider.mdl";
    self.bboxtype = BBOX_WILLY;
    self.sight_sound = "spider/tap.wav";
    self.pain_sound = "spider/pain.wav";

    if (self.health < 1) self.health = 200;
    self.gibhealth = -90;
    self.pain_flinch = 40;
    self.th_stand = spid_stand;
    self.th_walk = spid_walk1;
    self.th_run = spid_run1;
    self.th_die = spid_die;
    self.th_pain = spid_pain;
    self.th_missile = spid_attack1;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    if (self.deathstring == "") self.deathstring = " was entangled by Willie the Spider\n";

    self.classtype = CT_MONWILLYSPIDER;
    self.classgroup = CG_SPIDER;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};