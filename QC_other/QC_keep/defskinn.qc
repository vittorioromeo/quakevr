//======================================================================
// MOD    : Kinn
// Kept by the Keep Mod.  You keep what you kill (and kill with).
//======================================================================
float FL_NOSELECT = 8192;    //ignored by entity selector
float MONSTER_FLAK_OGRE    = 4;
.float spikecount;

// new stuff needed for q2 weapons
.float idleweaponframe;    // this is set to be the stationary idle weapon frame used when the player is not firing
                // it is different for each weapon and must therefore be set on each weapon change
.float monsterflags;
float MF_COMMANDER    = 1;        // beefier version