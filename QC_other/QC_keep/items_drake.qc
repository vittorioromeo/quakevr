// Used by decapitation code to give headless "body double" the loot.
void(entity from, entity to) Pack_Copy =
{
    to.arsenal      = from.arsenal;
    to.war          = from.war;
    to.healamount   = from.healamount;
    to.healtype     = from.healtype;
    to.ammo_shells  = from.ammo_shells;
    to.ammo_nails   = from.ammo_nails;
    to.ammo_rockets = from.ammo_rockets;
    to.ammo_cells   = from.ammo_cells;
    to.ammo_super   = from.ammo_super;
    to.ammo_multi_rockets = from.ammo_multi_rockets;
    to.ammo_lava_nails = from.ammo_lava_nails;
    to.ammo_plasma = from.ammo_plasma;
    to.ammo_bolts = from.ammo_bolts;
    to.ammo_poison = from.ammo_poison;
};

float(entity who) Pack_Empty = {
    return (who.arsenal+who.war+who.healamount+who.healtype+who.ammo_shells
    +who.ammo_nails+who.ammo_rockets+who.ammo_cells+who.ammo_super
    +who.ammo_multi_rockets+who.ammo_lava_nails+who.ammo_plasma+who.ammo_bolts
    +who.ammo_poison);
};

//============
// --> QUAKED item_artifact_optimizer (0 .5 .8) (-16 -16 -24) (16 16 32)
// Buffs melee damage and gives
// life leech -- permanently.
//============

void () item_artifact_berserker = {
    self.mdl = "progs/drake/zerkhelm.mdl";
    precache_model (self.mdl);
    self.noise = "items/zerkhelm.wav";
    precache_sound (self.noise);
        self.netname = "Berserker Helm";
    self.items = 0;
    self.classtype = CT_ARTBERSERK;
    self.part_active = PARTICLE_STYLE_PENT;
    self.respawn_style = PARTICLE_BURST_RED + PARTICLE_BURST_CENTER;
    self.respawn_ofs = '0 0 16';
    artifact_setup();
};

//============
// --> QUAKED item_artifact_equalizer (0 .5 .8) (-16 -16 -24) (16 16 32)
// Nailguns fire two nails at the price
// of one -- permanently.
//
// Also fills nails to the maximum.
//============

void () item_artifact_equalizer = {
    self.mdl = "progs/drake/gemini.mdl";
    precache_model (self.mdl);
    self.noise = "items/equalize.wav";
    precache_sound (self.noise);
    self.netname = "Twin Shot";
    self.items = 0;
    self.classtype = CT_ARTEQUALIZER;
    self.part_active = PARTICLE_STYLE_SRING;
    self.respawn_style = PARTICLE_BURST_YELLOW + PARTICLE_BURST_CENTER;
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_nails = world.deventity.ammo_nails + AMMO_MAXNAILS;
    self.respawn_ofs = '0 0 16';
    artifact_setup();
};

//============
// --> QUAKED item_artifact_levitation (0 .5 .8) (-16 -16 -24) (16 16 32)
// Gives the player limited flying ability
//============
void () item_artifact_levitation = {
    self.mdl = "progs/drake/levitate.mdl";
    precache_model (self.mdl);
    self.noise = "items/levitate.wav";
    self.noise1 = "ambience/wingget.wav";
    self.noise2 = "ambience/wingend.wav";
    precache_sound (self.noise);
    precache_sound (self.noise1);
    precache_sound (self.noise2);
    self.netname = "Boots of Levitation";
    self.items = 0;
    self.classtype = CT_ARTLEVITATION;
    self.part_active = PARTICLE_STYLE_TOMEOFP;
    self.respawn_style = PARTICLE_BURST_WHITE + PARTICLE_BURST_RING;
    self.respawn_ofs = '0 0 16';
    artifact_setup();
};

// Original classname in Zerstorer.
// Note:  Replaced Zerstorer's Dark-Angel Wings with Boots of Levitation.
//void() item_wings = {self.classname = "item_artifact_levitation"; item_artifact_levitation(); };


//============
// --> QUAKED item_artifact_optimizer (0 .5 .8) (-16 -16 -24) (16 16 32)
// Shotguns fire faster -- permanently.
//
// Also fills shells to the maximum.
//============
void() item_artifact_optimizer = {
    self.mdl = "progs/drake/shotty.mdl";
    precache_model (self.mdl);
    self.noise = "items/optimize.wav";
    precache_sound (self.noise);
    self.netname = "Shotgun Optimizer";
    self.items = 0;
    self.classtype = CT_ARTOPTIMIZER;
    self.part_active = PARTICLE_STYLE_ARMOR;
    self.respawn_style = PARTICLE_BURST_YELLOW + PARTICLE_BURST_CENTER;
    self.respawn_ofs = '0 0 16';
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_shells = world.deventity.ammo_shells + AMMO_MAXSHELLS;
    artifact_setup();
};


//============
// --> QUAKED item_sigil_bane (0 .5 .8) (-16 -16 -24) (16 16 32)
// Unique item for nsoe5.  Pick up to activate final fight with Bane.
//============
void() item_sigil_bane = {
    self.mdl = "progs/drake/end_bane.mdl";
    precache_model (self.mdl);
    self.noise = "misc/runekey.wav";
    precache_sound (self.noise);
    self.netname = "seal of Bane!";
    self.items = 0;
    self.classtype = CT_SIGILBANE;
    self.part_active = PARTICLE_STYLE_SIGIL;
    self.respawn_style = PARTICLE_BURST_PURPLE + PARTICLE_BURST_CENTER;
    self.respawn_ofs = '0 0 16';
    artifact_setup();
};

void() weapon_blaster = {
    self.mdl = "progs/w_blaster.mdl"; // set by world.qc, mapper can pick to use AD or original axe
    precache_model (self.mdl);
    //self.weapon = IT_BLASTER;
    self.classtype = CT_BLASTERWEP;
    self.netname = "Laser Blaster";
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_shells = world.deventity.ammo_shells + AMMO_SHELLS_WPN; //TODO: change to cells?
    weapon_start();
};