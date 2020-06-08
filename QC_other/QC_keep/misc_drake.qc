// This spawns a gib from the blood fountain.
// Taken from Zerstorer and adapted to Drake.
void(string gibname, vector av) FtGib = {
    local   vector  vel;

    vel_x  = (random() * 100) - 50;
    vel_y  = (random() * 100) - 50;
    vel_z  = self.speed + (random() * 200);
    Gib_Spawn (self.origin, gibname, vel, av, 0, 0, TRUE, -1);
};

void() gibs_fly = {
    local   float   rnum; 

    rnum = random();
    if (rnum < 0.15)
    {
        FtGib ("progs/gib1.mdl", '100 200 300');    //Zer uses gib4.mdl.
        FtGib ("progs/gib2.mdl", '200 100 300');
        FtGib ("progs/gib3.mdl", '100 200 300');
        FtGib ("progs/h_player.mdl", '300 200 100');

        sound (self, CHAN_AUTO, "gibfnt/gf_sbig.wav", 1, ATTN_NORM);
    }
    else if (rnum < 0.45)
        FtGib ("progs/gib1.mdl", '100 200 300');
    else if (rnum < 0.85)
        FtGib ("progs/gib2.mdl", '100 200 300');    //Zer uses gib4.mdl.
    else
        FtGib ("progs/gib3.mdl", '100 200 300');

    if (random() < 0.5)
        sound (self, CHAN_VOICE, "gibfnt/gf_sprt1.wav", 1, ATTN_NORM);
    else
        sound (self, CHAN_VOICE, "gibfnt/gf_sprt2.wav", 1, ATTN_NORM);

    self.nextthink = time + (random() * 5) + 3;
    self.think = gibs_fly;
};

void() gibft_use = {
    if (self.nextthink == 0) {
        self.nextthink = time + self.delay;
    self.think = gibs_fly;
    } else if (self.lefty)
        self.nextthink = 0;
    else
        remove (self);
};

//============
// --> QUAKED misc_gibft (0 .5 .8) (-8 -8 -8) (8 8 8)
// Gib Fountain -- Zerstorer style.
//============
void() misc_gibft = {
    precache_model ("progs/gib1.mdl");
    precache_model ("progs/gib2.mdl");
    precache_model ("progs/gib3.mdl");
    precache_model ("progs/h_player.mdl");
    precache_sound ("gibfnt/gf_sprt1.wav");
    precache_sound ("gibfnt/gf_sprt2.wav");
    precache_sound ("gibfnt/gf_plop.wav");
    precache_sound ("gibfnt/gf_sbig.wav");
    //self.classname  = "blood";
    if (!self.wait) self.wait = 2;
    self.nextthink  = time + (random() * self.wait);
    self.think      = gibs_fly;

    if (!self.speed) self.speed = 1000;
    if (!self.dmg) self.dmg = 5;
    if (!self.delay) self.delay = 0.5;

    if (self.targetname) {
        self.lefty = self.spawnflags & 4;
        if (self.spawnflags & ENT_STARTOFF) {
            self.nextthink = 0;     // Start off.
        self.think = SUB_Null;
    }
        self.use = gibft_use;
    }
};