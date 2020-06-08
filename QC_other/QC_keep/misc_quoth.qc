void () misc_biobox_l = {
    self.mdl = "progs/b_bio_l.bsp";
    precache_model (self.mdl);
    precache_model ("progs/polypgib.mdl");
    precache_sound ("boglord/slime_explode.wav");
    self.noise = "boglord/slime_explode.wav";
    self.bbmins = '-16 -16 0';
    self.bbmaxs = '16 16 64';
    self.dmg = 80;
    self.skin_override = 3; // Green particle burst
    misc_explobox_setup();
};

void () misc_biobox_s = {
    self.mdl = "progs/b_bio_s.bsp";
    precache_model (self.mdl);
    precache_model ("progs/polypgib.mdl");
    precache_sound ("boglord/slime_explode.wav");
    self.noise = "boglord/slime_explode.wav";
    self.bbmins = '-16 -16 0';
    self.bbmaxs = '16 16 32';
    self.dmg = 80;
    self.skin_override = 3; // Green particle burst
    misc_explobox_setup();
};

void () misc_plasmabox_l = {
    self.mdl = "progs/b_plas_l.bsp";
    precache_model (self.mdl);
    precache_sound ("weapons/lg_explode2.wav");
    self.noise = "weapons/lg_explode2.wav";
    self.bbmins = '-16 -16 0';
    self.bbmaxs = '16 16 64';
    self.dmg = 200;
    self.skin_override = 2; // Blue particle burst
    misc_explobox_setup();
};

void () misc_plasmabox_s = {
    self.mdl = "progs/b_plas_s.bsp";
    precache_model (self.mdl);
    precache_sound ("weapons/lg_explode2.wav");
    self.noise = "weapons/lg_explode2.wav";
    self.bbmins = '-16 -16 0';
    self.bbmaxs = '16 16 32';
    self.dmg = 200;
    self.skin_override = 2; // Blue particle burst
    misc_explobox_setup();
};