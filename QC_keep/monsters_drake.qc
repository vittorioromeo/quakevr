//- - - - - - - - -
// Set contents of monster's purse.
void(float loop, float drop_chance, float gem_chance) monster_purse =
{
    self.purse = '0 0 0';
    self.purse_z = parms_pack (loop, drop_chance * 100, gem_chance * 100);
    while (loop > 0)
    {
        if (random() < drop_chance)
        {
            if (random() < gem_chance)
                self.purse_y = self.purse_y + 1;    // Gem
            else
                self.purse_x = self.purse_x + 1;    // Coin
        }
        loop = loop - 1;
    }
};

void() monster_null =
{
    //Burn_Null (self);   // Remove any flames burning on self.
    // FIXME:  This next one shouldn't be called here.  (Check wrath*.qc)
    //Pets_Free (self);   // In case of sudden removal, unlink all pets.

// Don't let other entities activate corpse as it decays.
//    self.solid      = SOLID_NOT;
    self.use        = self.touch    = SUB_Null;
    self.targetname = "";

// Eliminate any possibility of taking more damage.
    self.takedamage = DAMAGE_NO;    // So corpse cannot be gibbed.

// Removal sets entity's origin at '0 0 0', and C physics code may play
// splashing sound in the process.  Preset entity's water stats at origin
// now it won't splash when (re)moved.
    //if (pco <= CONTENT_WATER)
    //    {self.watertype = pco;  self.waterlevel = 1;}
    //else
    //    {self.watertype = CONTENT_EMPTY;  self.waterlevel = pco;}
};

//================
// monster_remove
//
// Removes a dead monster from the game, freeing the edict it used.
// Useful for speeding up gameplay.
//================
void() monster_remove =
{
//
// SoE:  Indian summer requires a custom engine, and all that I know of,
// except Acquire's version of software WinQuake, support alpha/transparency.
    // Fade into nothing.  Requires engine that supports transparent
    // entities, such as DarkPlaces, or else it vanishes without fading.
    monster_null ();
    if (!self.alpha)
        self.alpha = 1;
    self.nextthink = time + 0.1;
    self.think = monster_fade;
//
//- - - - - - - - -
// DISABLED - Code for standard Quake.
//    self.think = monster_erase;
//    monster_erase ();
};