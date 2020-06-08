float   ALPHA_GHOST     = 0.7;  // Used by wraiths.
float   ALPHA_ICE       = 0.7;  // Used by ice shards.
float   ALPHA_ICEBALL   = 0.9;  // Used by iceballs.

//vector  ARMOR_STATS_GREEN   = '0.3 100 1';
//vector  ARMOR_STATS_YELLOW  = '0.6 150 2';
//vector  ARMOR_STATS_RED     = '0.8 200 4';
// Note:  Dragonscale uses the same stats as red armor.
vector  ARMOR_STATS_COURAGE = '0.95 150 4';
vector  ARMOR_STATS_ONYX    = '0.5 0 8388608';
//vector  ARMOR_STATS_POWER   = '0.5 0 65536';    // Onyx with power shield.
vector  ARMOR_STATS_DEMON   = '0 100 4';
vector  ARMOR_STATS_SPIRIT  = '1 25 0';
vector  ARMOR_STATS_CHAOS   = '0 200 65536';
vector  ARMOR_STATS_ULTRA   = '1 30 65536';       // Ultra enforcer armor.

// Custom attack states.
//See defshipnotic.qc  float   AS_DODGING  = 5;    // One of Hipnotic's inventions for the scourge.  
float   AS_CHASING  = 9;    // Use shortest, direct path to chase target.
float   AS_VERTICAL = 10;    // Like AS_SLIDING, but vertical (up or down).
float   AS_FLANKING = 11;    // AS_SLIDING if close, AS_STRAIGHT otherwise.
float   AS_FLEEING  = 12;    // Reverse of AS_STRAIGHT, used by wraith.
float   AS_REAPER   = 13;   // Sucking in souls, used by wraith.
float   AS_COWARDLY = 14;   // Back off if too close.  Otherwise, as_straight.

float   ATTN_DAMAGE = 0.5;      // Default attenuation for radius damage.

// arrows -------------------------------------------------------------------
float   AKARROW_SPEED_FLY   = 1000;
//float   AKARROW_SPEED_TOSS  = 1200;
float   ARROW_FLY   = 2;    // Use 0=FALSE(toss) or 2=TRUE(flymissile).

// arrow model skins
float   ARROW_WOOD  = 0;
float   ARROW_EBONY = 1;
float   ARROW_ICE   = 2;
float   ARROW_MAGIC = 3;    // Orange.  Use skin + 3 to match laser colors.

// Burning damage constants.
float   BURN_PER_SECOND     = 4;    // 4 is slime dmg, 5 divides evenly with armor %s.
float   MAX_BURN_DAMAGE     = 60;   // Enough to kill standard zombies.
                                    // FIXME: No it isn't with regeneration!

float   CURSE_DAMAGE        = 1;    // Reverse Resistance
float   CURSE_WEAKEN        = 2;    // Reverse Strength
float   CURSE_SLOW          = 3;    // Reverse Haste
float   CURSE_ARMOR         = 4;    // Reverse Regeneration
float   CURSE_CONFUSE       = 5;
//float   CURSE_LIFETAP       = 6;

vector  VEC_360     = '360 360 0';
vector  VEC_QUAKE   = '5 4 4';      // Standard earthquake angle tilts.

// deathmatch values --------------------------------------------------------
float   DM_OLD_SCHOOL   = 2;    // Like Doom, weapons stay, nothing respawns.
                                // Deathmatch 2 is 'the silly old rules'.
//float   DM_DOOM         = 4;    // Use Doom style shotguns -- OBSOLETE!
float   DM_GRAPPLE      = 8;    // Players start with grapple.
float   DM_MONSTERS     = 16;   // SoE:  Monsters stay on the level.
float   DM_REVIVE       = 32;   //       Monsters resurrect after death.

// damage values ------------------------------------------------------------
// FIXME:  Redo damage types.
// acid = 1, cold = 2, fire = 3, lightning = 4, nether = 5.
float   DF_NONE         = 0;    // No flags -- just for readability.
float   DF_TIMED        = 1;    // Damage done slowly over time... usually.
float   DF_NO_ARMOR     = 2;    // Armor cannot absorb this damage.
float   DF_NO_KNOCK     = 4;    // Don't add to velocity.
float   DF_GIB          = 8;    // Attack will gib if it kills the target.
float   DF_LEECH        = 16;   // Attacker heals from target damage.
float   DF_PROXY        = 32;   // Indirect attack, no quad, no thorns.
                                //  Used by doors and the bloodcube.
float   DF_SCAR         = 64;   // Can kill zombies if applied continuously.
float   DF_ACID         = 128;
float   DF_COLD         = 256;
float   DF_FIRE         = 512;
float   DF_LIGHTNING    = 1024;
float   DF_ROCKET       = 2048; // Damage caused by rockets and equivalent.
float   DF_BURN         = 4096; // Target may ignite when damaged.
//float   DF_13           = 8192;
float   DF_MELEE        = 8192; // Melee attack, used to check a powerup.
float   DF_PERCENT      = 16384;    // Remove current hp% instead of hp.
// Radius only
float   DF_SAFE         = 32768;  // Attacker is immune to his attack.
float   DF_WATER_ONLY   = 65536;  // Takes damage only if in water.
// Special
//float   DF_17    = 131072;   // Matching class can't hurt each other.
float   DF_NO_GIB       = 131072;   // Attack cannot gib.
float   DF_ZKILL        = 262144;   // One-hit kill zombies on direct hit.
float   DF_CROSS_BLOCK  = 524288;   // Cross blocks all damage.
float   DF_PUSH_NPC     = 1048576;  // Apply knockback to npcs.
float   DF_NO_SHIELDS   = 2097152;  // Mega-enforcer shields can't stop this.
float   DF_NO_REACT     = 4194304;  // Don't get mad at attacker.
float   DF_BLESSED      = 8388608;  // Don't hit allies.
// FIXME:  Add more flags?  Must replace some of the above flags first.
// DF_HIT_ALL = Hit solid_not.
// DF_CLASSED = Ignore targets with same classname.
// DF_NO_HALF = Attacker takes full damage instead of half.
// DF_NETHER  = Hurts living, not undead.

// Combo values
//float   DFx_TYPES       = 7;
float   DFx_BURNING     = 69;  // Timed + No Knock + Scar
float   DFx_FIREBALL    = 576;  // Fire + Scar
//float   DFx_FIREBALL    = 4672; // Fire + Scar + Burn
//float   DFx_PLASMA      = 3072; // Lightning + Rocket
float   DFx_RESISTS     = 3969; // Acid + Cold + Fire + Lightning + Rocket

// Note to self, don't add centroid morph:
// * Proper view_ofs is too low.  Things look ugly when player sits inside
//  an item he cannot take.  (e.g., ammo or health when at max capacity.)
// * Fat shambler-sized forms are often difficult to navigate with.
//  In fact, I am tempted to replace the dragon morph with something
//  such as an imp.  Dragon stays due to legacy issues (i.e., Soul of Evil).
float   FORM_DRAGON     = 1;

// hit type -----------------------------------------------------------------
//.float  hittype;

float   HIT_NORMAL  = 0;    // Default.
float   HIT_METAL   = 1;    // Knights, centroids, and other armored guys.
float   HIT_STONE   = 2;    // Statues, golems.



// List of exotic armor.  Player can wear only one at a time.  Duh!
float   SUIT_COURAGE    = 1;    // Reflective half-armor.  From Zerstorer.
float   SUIT_DRAGON     = 2;    // Dragonscale and morph.
float   SUIT_ONYX       = 3;    // Indestructible.
float   SUIT_ONYX_POWER = 4;    // As Onyx, plus built-in power shield.
float   SUIT_BERSERKER  = 5;    // Demonhide - regeneration and life-saving.
float   SUIT_SPIRIT     = 6;    // Spirit shroud, like hyper enforcer shield.
float   SUIT_CHAOS      = 7;    // Variable protection, teleportation.
float   SUIT_NINJA      = 8;    // Jumpsuit with special powers.
float   SUIT_ULTRA      = 9;    // Ultra enforcer particle shield.  NPC only.

float   SUITx_ALL       = 15;   // Value must be a power of 2 minus 1.
                                // This means 3, 7, or 15.  No higher.

float   COURAGE_DAMAGE  = 10;   // Damage to armor if it reflects a rocket.

//==========================================================================
//  Sprite Frame Data

// FIXME:  put unused in 's_unused.spr'.

// Frames for 's_fancy.spr'.
float   X32_PUFF_ORANGE     = 0;
float   X32_PUFF_BLUE       = 5;
float   X32_PUFF_PURPLE     = 10;
float   X32_CLOUD_GREEN     = 15;
// Remove - - - - -
float   X32_PLASMA_ORANGE   = 21;
float   X32_PLASMA_RED      = 27;
float   X32_PLASMA_YELLOW   = 33;
float   X32_PLASMA_BLUE     = 39;
//- - - - - - - - -
float   X32_STAR_ORANGE     = 45;
float   X32_STAR_RED        = 50;
float   X32_STAR_YELLOW     = 55;
float   X32_STAR_BLUE       = 60;
//- - - - - - - - -
// 4 frames
float   X32_SPARK_ORANGE    = 65;
float   X32_SPARK_RED       = 69;
float   X32_SPARK_YELLOW    = 73;
float   X32_SPARK_BLUE      = 77;
float   X32_CUT_ORANGE      = 81;
float   X32_CUT_RED         = 85;
float   X32_CUT_YELLOW      = 89;
float   X32_CUT_BLUE        = 93;
//- - - - - - - - -
float   X56_PUFF_ORANGE     = 97;
float   X56_PUFF_BLUE       = 103;
float   X56_PUFF_PURPLE     = 109;
float   X56_CLOUD_GREEN     = 115;
float   X56_PLASMA_ORANGE   = 120;
float   X56_PLASMA_RED      = 126;
float   X56_PLASMA_YELLOW   = 132;
float   X56_PLASMA_BLUE     = 138;
float   X56_SPLASH_RED      = 144;
float   X56_SPLASH_GREEN    = 147;
float   X56_SPLASH_PURPLE   = 150;
float   X80_SPARK_ORANGE    = 153;
float   X80_SPARK_RED       = 159;
float   X80_SPARK_YELLOW    = 165;
float   X80_SPARK_BLUE      = 171;

// Frames for 's_wave.spr'.
float   RING_FIRE       = 0;    // Custent's flaming shockwave, modified.
float   RING_BLUE       = 8;    // Drake's airstrike impact.
float   RING_QUAKE      = 14;   // Drake's quake slam.
float   RING_ICE        = 19;   // Iceball shockwave.
// Ends at frame 21.

// Select frames in 's_wave2.spr'.  Each color lasts 3 frames.
float   RING2_QUAKE     = 0;
float   RING2_ICE       = 6;
// Ends at frame 56.

// If you want an object that does not spray gore when hit by the likes of
// a chainsaw, add +1 to one of the non-zero defined bloodtypes.
// For example, (ent.bloodtype = BLOOD_RED + 1;).
// The 'particle' function uses all colors within a range
// from (x*8) to (x*8)+7, where x ranges from 0 to 31.
//============
float   BLOOD_NONE      = 0;
float   BLOOD_DARK      = 33;       // Black-and-blue for shadow creatures.
float   BLOOD_GREEN     = 57;       // For acid-blooded ala Alien Quake.
float   BLOOD_RED       = 73;       // Standard blood red color.
float   BLOOD_SPARKS    = 105;      // For machines like drones or robots.
float   BLOOD_PURPLE    = 145;      // For tarbaby.
//float   BLOOD_BLUE      = 209;      // For cacodemons.
//float   BLOOD_SHAFT     = 225;      // Alternate blood red color.
float   BLOOD_FIRE      = 233;      // For lava monsters like Chthon.
float   CHUNKS_ICE      = -41;      // For ice chunks.

float   BB_GREEN    = 1; // Blood bits for bloodbank
float   BB_PURPLE   = 2;
float   BB_FIRE     = 4;

//float   FLx_MOVE            =    3;     // Fly + Swim
float   FLx_CREATURE        =   40;     // Client + Monster
float   FLx_NOT_MISSILE     =  296;     // Client + Monster + Item
//float   FLx_CHEATS          =  192;     // Godmode + Notarget
float   FLx_NOT_FALLING     =  515;     // Fly + Swim + Onground
float   FLx_JUMP_READY      = 4608;     // Onground + Jumpreleased

float   GIB_HEAD        = 1;    // Head gib.
float   GIB_EAT         = 2;    // Gib can be eaten for health.
float   GIB_DMV         = 4;    // Add damage velocity to gib velocity.
float   GIB_BODY_HALF   = 8;    // Spawn within the lower half of body.
float   GIB_BODY_FULL   = 16;   // Spawn anywhere from e.absmin to e.absmax.
float   GIB_SKIN_MATCH  = 32;   // Set gib.skin equal to self.skin, not 0.
float   GIB_BURN        = 64;   // Gib burns away.  Used for books.
float   GIBx_STATUE     = 12;   // Properties for statue gibs.  Dmv+Half.
float   GIBx_GIB        = 14;   // Normal properties for gibs.  Eat+Dmv+Half.
float   GIBx_RUBBLE     = 20;   // Properties for rubble.  Dmv+Full.

float   HOOK_NEAR   = 50;

float   INF_SAFE    =  1000000;     // Use this if operations is a possibility
float   INFINITY    = 16777215;     // High as we can go without rollover.

// Unlike in other games (e.g., Doom, Quake2).  All we need mass for are:
// 1) Who gets pulled when grappled.
// 2) Who gets shaken and damaged by earthquakes.
// For these purposes, a few weight classes are sufficient.
// Do not use them to calculate velocities, such as v = vel * (1 / mass).
// Besides, the velocity effects in Quake look best without varying mass
// mucking up expected flight paths of gibs and the like.
//- - - - - - - - -
float   MASS_LIGHT      = 1;  // Under 0.5x player mass.
float   MASS_MEDIUM     = 2;  // From 0.5x to 1.5x player mass.
float   MASS_HEAVY      = 3;  // From 1.5x to 5x player mass.
float   MASS_SUPER      = 4;  // Over 5x player mass.
//float   MASS_X          = 0;  // Over 10x player mass.
//- - - - - - - - -
// Note:  Zero mass is treated as beyond superheavy.
// Explanation of mass/weight classes.
//- - - - - - - - -
// Light = Target gets pulled toward grappler.
// Medium = Neither target nor grappler are pulled.
// Heavy = Grappler gets pulled toward target.
// Superheavy = As heavy, but target is immune to quakes and resists pushing.
// X = As superheavy, plus immune to forced movement of any kind.
//- - - - - - - - -

// rocket damage types ------------------------------------------------------
float   MDAM_RANDOM     = 0;    // damage = damage * (5+random()) / 6
float   MDAM_GRENADE    = 1;    // Use radius damage, with minimum of 10% max.
float   MDAM_FULL       = 2;    // Full damage on contact.
float   MDAM_TWENTY     = 3;    // Damage ranges from dmg-20 to dmg.

// Dragon flags (NOTE:  Also used for other monsters...)
float   RYU_ELEMENTS    = 3;
    float   RYU_USE_FIRE    = 0;
    float   RYU_USE_ACID    = 1;
    float   RYU_USE_LITE    = 2;
    float   RYU_USE_ICE     = 3;
float   RYU_BOMB        = 4;
//float   RYU_03          = 8;
//float   RYU_04          = 16;
//float   RYU_05          = 32;
float   RYU_ALERT       = 64;       // Has better awareness and reflexes.
float   RYU_CRANKED     = 128;      // Stronger, and may use special attacks.
float   RYU_NO_WATER    = 256;      // Hates and avoids water.
float   RYU_STAY        = 512;      // Stay if on, move if off.
float   RYU_CALM        = 1024;     // Ignore targets if on.
float   RYU_PATROL      = 2048;     // Glide back-and-forth.
float   RYU_FORCED      = 4096;     // Commanded by player to shoot.
//float   RYU_GREEDY      = 8192;     // PET:  Take every item with loot rank.
//float   RYU_LOADED      = 16384;    // PET:  Currently holds loot.
//float   RYU_RUNAWAY     = 32768;    // PET:  Ignore items.
//float   RYU_DRACO       = 65536;    // PLAYER:  Draco is alive!
//float   RYU_TELEPORT    = 131072;   // PET:  Draco was told to teleport.
float   RYU_BERSERK     = 65536;    // For drole.
// FIXME:  Next three bits could be moved to somewhere else?
float   RYU_JUSTHIT     = 131072;   // Cyberdemon: shoot back after pain.
//float   RYU_SLOW        = 262144;   // PET:  Move slowly for one frame.
float   RYU_ANGRY       = 262144;   // Infight even if alert.  Fix for Hell.
float   RYU_INSANE      = 524288;   // Attack ally if hurt by it.
float   RYU_PATH        = 1048576;  // DoE:  Stay on path, for DoE dragon.
//float   RYU_FLANKER     = 2097152;  // Move like Quoth enforcers.
float   RYU_DOOM        = 2097152;  // Use Doom AI.
float   RYU_REBEL       = 4194304;  // SoE:  For rebel archers in nsoe2.
float   RYU_REAPER      = 8388608;  // Soul collector.
//float   RYU_NO_SIGHT    = ???       // No Line of sight needed for attacks.

// Speed for various missiles.
float   SPEED_HOOK      = 500;      // Was 500 in Roman, 1000 in original.
float   SPEED_BURIZA    = 2000;     // Was 1000.

// --> Spawnflags <--
// Note:  sf32, 64, and 128 are the same as in Quoth.
// Note#2:  Zerstorer used sf64 for delay spawn, Quake2 used sf2.
float   SPAWN_AMBUSH    = 1;        // Standard.  Wake up on sight or pain.
//float   SPAWN_BOSS      = 2;        // Upgrades some to boss versions.
float   SPAWN_STATUE    = 4;        // Statue mode for select monsters.
float   SPAWN_WHITE     = 16;       // Monster teams up and helps client.
float   SPAWN_SILENT    = 32;       // Slient sf64.
float   SPAWN_NORMAL    = 64;       // Spawns when triggered; classic sf64.
float   SPAWN_AWARE     = 128;      // Monsters:  Spawn mad at activator.
float   SPAWN_RESPAWN   = 128;      // Items:  Always respawn.
// Next four are used by Quake engine to remove entities during level load.
//float   SPAWN_NOT_EASY  = 256;
//float   SPAWN_NOT_NORM  = 512;
//float   SPAWN_NOT_HARD  = 1024;
//float   SPAWN_NOT_DM    = 2048;

// Note to self, don't add IKguns:
// * Models are off to the side, while Quake weapons are centered.
// * Plasma gun Doom/IK style is redundant with super nailgun, lasers,
//  and lightning guns.  (Quoth style plasma on the other hand...)
// * Asha is not necessary now that Tome of Power modifies shotguns to
//  fire similar effects.
// There, I said it!  Here so I don't think about adding the asha back.

// Note 1/31/2010:  Plasma can kill zombies, but lasers can't, so plasma
// may need to be added back in case Quoth compatibility is needed.
// Also, chainsaw could be added back.  Just use standard model and
// lightning gun frames.  Worked for Doom.

// Note 3/31/2010:  Added Quoth's plasma gun.  Why? Two reasons.
// One, ToP induced BFG10K mode is better suited for plasma weaponry
// instead of chain lightning.  Two, it's the perfect weapon for a
// high-level soldier NPC.  (Quoth eliminator, DoomRL former commando.)

//- - - - - - - - -
// Bits are sorted for 'RankForWeapon'.  Higher ranking weapons use high
// bits, and lower ranking weapons use low bits.  I use the "bigger is
// better" paradigm because it is possible for a player to have
// NO weapons, and the weapon value for an unarmed player is zero.
// Unarmed players cannot attack -- at all.  Having even a crappy
// low-bit weapon is still better than having no weapon.
//- - - - - - - - -
// Note:  Axe and sword are interchangable.
//
// Update:  Moved Mjolnir to the high end, since it was powered-up.
// An upgrade item will be required in Drake, but not in Tronyn's mods.
//- - - - - - - - -
// Update 4/20/2011:  Forget Mjolnir upgrade, leave it powered up.
//- - - - - - - - -
// Update 5/26/2011:  I'll level with you.  Any plans I had for building
// maps have been scrapped.  Tronyn is wrapping up his maps.  No point in
// entertaining feature creep if they won't be used.  What this means is
// the removal of some weapons.  Gone are the following:  Monster tamer,
// auto shotgun, and railgun.  Monster tamer, I really wanted, as the
// replacement for the Draco bot, but building a good model is too much
// work for me, and implementation without interfering with other stuff
// is a problem.  Auto shotgun, the one from Nehahra is somewhat crude
// and not very Quakey.  Railgun, standard Quake does not handle such
// effects very well, both engine and level/AI design.  I also need to
// build a good model, or reskin the grapple -- I'm not doing that.
// Pity, railgun is a classic weapon.
//
// Note 6/17/2011:  Railgun effect may return as a crossbow upgrade item.
//- - - - - - - - -
float   WAR_GRAPPLE          =       1;     // Built as a tool, not a weapon.
float   WAR_AXE              =       2;     // Knives aren't as good as guns.
float   WAR_CHAINSAW         =       4;     // 50% stronger than axe.
float   WAR_SHOTGUN          =       8;
float   WAR_ZAPPER           =      16;     // Low base damage, but chargable.
float   WAR_NAILGUN          =      32;
float   WAR_SUPER_SHOTGUN    =      64;     // Buffed to riotgun/Q3 strength.
float   WAR_BLASTER          =     128;
float   WAR_GRENADE_LAUNCHER =     256;
float   WAR_SUPER_NAILGUN    =     512;
float   WAR_SIDEWINDER       =    1024;     // Proximity gun replacement.
float   WAR_ROCKET_LAUNCHER  =    2048;
float   WAR_LASER_CANNON     =    4096;
float   WAR_PLASMA_GUN       =    8192;     // Quoth's, with less damage.
float   WAR_THUNDERBOLT      =   16384;
float   WAR_CHAIN_LIGHTNING  =   32768;
float   WAR_CRUISE_MISSILE   =   65536;     // Most destructive gun.
float   WAR_MJOLNIR          =  131072;     // Modified to BFG-like power.
float   WAR_BURIZA           =  262144;     // Insta-kill crossbow.
//float   WAR_19               =  524288;
//float   WAR_20               = 1048576;
//float   WAR_21               = 2097152;
//float   WAR_22               = 4194304;
//float   WAR_23               = 8388608;     // Highest bit, and best weapon.
// All 24 bits on equals 16777215.

// Weapon combo values
float   WARx_QUAKE_WEAPONS  =    19306;     // Original Quake weapons
float   WARx_AXE_FRAMES     =   131074;     // Use axe frames on this weapon.
float   WARx_WEAPON_FREE    =   131095;     // These don't need ammo to use.
float   WARx_NO_STEALING    =   131103;     // Monsters can't steal these.
float   WARx_LASERS         =     4224;     // Blaster/Laser Cannon guns.
float   WARx_SHAFT          =    49152;     // Chain/Thunderbolt guns.
float   WARx_DISCHARGE      =   188416;     // Discharge when used underwater.
// WARx_AI* are used by monster code only.
float   WARx_AI_RAPID_FIRE  =    53792;     // Nails, LC, Shafts.
float   WARx_AI_GUNS        =   131048;     // Guns used by monster_army npcs.

float   WARx_USES_SHELLS    =       72;
float   WARx_USES_NAILS     =      544;
float   WARx_USES_ROCKETS   =    68864;
float   WARx_USES_CELLS     =   192640;

// IMPORTANT!!!  This next one determines what weapons are usable and
// available in the game.  Not even the 'legalized' field can override it.
float   WARx_ALL_WEAPONS    =   524287;     // All usable weapon bits on!

// This links weapons to icons on the sbar.
float   WARx_WEAPON1    =      24;  // items/weapon ==   1 (Shotgun)
float   WARx_WEAPON2    =      64;  // items/weapon ==   2 (Super Shotgun)
float   WARx_WEAPON3    =      32;  // items/weapon ==   4 (Nailgun)
float   WARx_WEAPON4    =    4736;  // items/weapon ==   8 (Super Nailgun)
float   WARx_WEAPON5    =    1280;  // items/weapon ==  16 (Grenades)
float   WARx_WEAPON6    =   67584;  // items/weapon ==  32 (Rockets)
float   WARx_WEAPON7    =  319488;  // items/weapon ==  64 (Thunderbolt)
//float   WARx_WEAPON7    =   57344;  // items/weapon ==  64 (Thunderbolt)
//float   WARx_WEAPON8    =  262144;  // items/weapon == b23 (Laser Cannon)
float   WARx_WEAPON8    =       0;  // items/weapon == 128 (Mjolnir)
float   WARx_WEAPON9    =       0;  // items/weapon == 128 (Mjolnir)

float   XFL_HUNGRY      = 1;    // Eats items.  (Was Draco before removal.)
float   XFL_ITEMS       = 2;    // Can pickup items.
float   XFL_GHOUL       = 4;    // Allowed to eat gibs.  "Brains!"
float   XFL_IMMUNITY    = 8;    // Entity is immune to poison.
//float   XFL_TELEPORT    = 16;   // Teleporters can transport this entity.
float   XFL_WIND        = 32;     // Wind tunnels can push this entity.
float   XFL_USE_AMMO    = 64;     // Monster needs ammo to shoot guns.
float   XFL_NO_AIR      = 128;    // Doesn't need to breathe.
//float   XFL_WATER       = 256;    // Breathes in water instead of air.
// FIXME:  Add rank.
float   XFL_BOSS        = 512;    // Boss -- Don't infight if coaligned.
float   XFL_INEDIBLE    = 1024;   // Gremlin can't eat this corpse.
float   XFL_ZOMBIE      = 2048;   // Hurt only by 1-hit kill or scar damage.
float   XFL_NO_SCAN     = 4096;   // Ignore this monster during a worldscan.
float   XFL_SHIELDS     = 8192;   // Has DAMAGE_NO because of shields.
float   XFL_ARMORED     = 16384;  // Magic-resistant/immune to knightspike.
float   XFL_GHOST_BANE  = 32768;  // Able to hurt ghosts.
float   XFL_ADD_TO_KILL = 65536;    // Add one to kills when killed.
float   XFL_FIREANGEL   = 131072;   // Immune to splash damage (cyberdemon).
//float   XFL_GRENADE     = 262144;   // Use different damage algorithm.
float   XFL_PET         = 524288;   // Use to decrement pet counter.
float   XFL_NO_BURN     = 1048576;  // Cannot ignite.
float   XFL_IGNORE      = 2097152;  // Don't infight this entity.
float   XFL_SLIMER      = 4194304;  // Not harmed by slime.
float   XFL_FLUX        = 8388608;  // Made solid by ghost touch code.

//.float      xf2;

float   XF2_QUAKE       = 1;    // Safe for quake to shake.
float   XF2_HORN        = 2;    // Enemy may be charmed by horn.
//float   XF2_ETHEREAL    = 4;    // Enemy has fazed out.
float   XF2_BOOMERANG   = 8;    // Missile that can return to owner.
float   XF2_CLONE       = 16;   // For Rogue Guardian.

//.float      xsf;        // Extra spawnflags used by func_spawn and sf64.

float   XSF_MOVETYPE    = 15;   // Monster's old movetype.  *Do NOT change!*
float   XSF_FUNC_SPAWN  = 16;   // Spawned by func_spawn.
float   XSF_SPAWNSILENT = 32;   // No fog effects when spawned.
float   XSF_SPAWNAWARE  = 64;   // Mad at activator when spawned.
float   XSF_NO_FALL     = 128;  // Temp add fly flag to stop prethink fall.
float   XSF_STATUE      = 256;  // Spawned as statue.
float   XSF_DMSP        = 512;  // Spawned by dmsp monster spawner.
float   XSF_GO          = 1024; // Player:  TRUE = Repeat spawn.
float   XSF_CHEST_HACK  = 2048;
float   XSF_BODY_DOUBLE = 4096; // Spawned decapitated body double.
float   XSF_CUBE_SPAWN  = 8192; // Spawned by cube, used in telefrag check.
float   XSF_GHOST_HACK  = 16384;    // nsoe1:  Ghost can be hit by any melee.


float bloodbank;      // Non-red blood types known to be precached.
float cranked;    // TRUE = Stronger monsters and brutal falling rules.
float cutscene;       // Set to TRUE during a cutscene.
float doomed;     // TRUE = Monsters respawn after death.
float gloat;      // GLObal fLOAT -- like hknight_type.
float respawning; // Toggle used when respawning monsters.
float soul_evil;  // TRUE = Soul of Evil mode.  Merger code.
float super_active;

// hit type -----------------------------------------------------------------
.float      hittype;
vector      damage_velocity;    // Set by 'T_Damage' for gib velocity.

.float      bloodtype;      // For blood other than red, like in Alien Quake.
.float      form_active;        // Non-zero if player is in monster form.
.float      ryuflags;   // AI flags for monsters.
.float      mass;
.float      perms;  // Permanent powerups.
.vector     purse;      // Used by monsters to drop money when killed.
                        //  x=coins, y=gems, z=random data.
                        //  z1=max items, z2=spawn chance, z3=gem chance.
.float      spawned;    // TRUE if created/summoned after a level load.
.float spawnskin;
.void()     th_revive;  // Resurrection scene.
.void()     th_spawn;   // Respawn function.
.void(float hello)  th_sight;   // Sight sound when spotting a target.
.float      tronyn;     // Flags that do special things in his maps.
.float      waitmin, waitmax;
.float      xfl, xf2;   // Extra flags without C code connections.
.float      xsf;        // Extra spawnflags used by func_spawn and sf64.

//===========================================================================
//  Weapons

// weapon values ------------------------------------------------------------
.float      saw_time;       // Time to play chainsaw idle sound.
.float      tracer_time;    // Used by rapid-fire energy weapons.
.float      zapper_time;    // Time to recharge flare gun/wand.
.float      arsenal;        // Up to 24 weapons can be stored here.
.float      war;            // Short for 'Weapon ARmed'.  Current weapon.
.float      drama1;         // DRAke Monster Arsenal.  For release 1.
.float      ammo_super;     // Super shots for crossbow.
.float      shots_max;      // Maximum shots in an attack.
.float      shots_left;     // Like currentammo.
.float      hit_points;     // True health, used to stop 'give h <#>'.
.float      curse_finished, curse_type, curse_ping;     // Necromancer debuffs.
.float      curse_slow;     // Delay nextthink to time here.
// DISABLED -- Stopping 'give' ammo cheats requires adding code in
// too many places.  With the other cheats removed, combined with the
// monsters' ferocity in Hell, the player can give himself as much ammo
// as he wants, and he will probably still die anyway.
// real_* is used to stop the give ammo cheats.

.entity     master;     // Like owner, without the C code bondage.
                        //  HEAVILY USED by monster summoning code!
                        //  Update:  Used by all missiles for reflection.

// FIXME:  Can we find another field that can be used as this?
.float      encore;     // TRUE = followup attack in a series.

// FIXME:  tamed is unused.
//.entity     tamed;          // Link to monster stored in capture weapon.
.entity     mist;           // Link to monster capturing entity.
.entity     chain2;
.entity     mirv1;          // Link to most recently launched MIRV.
.entity     hook;           // Link to most recently launched grappling hook.
// FIXME:  Do I need this?
//.entity     rain1;          // Link to rainstorm entity.

// extra flags --------------------------------------------------------------
//.float      xfl, xf2;   // Extra flags without C code connections.
//.float      xsf;        // Extra spawnflags used by func_spawn and sf64.

// persistent attacks -------------------------------------------------------
.entity     burner;         // Flame that is burning on the entity.
.vector     burn_ofs;       // Where flame burns relative to target origin.
                            //  x = v_forward, y = v_right, z = v_up.

// More powerups -----------------------------------------------------------
.float      thorns_finished, thorns_time, thorns_sound; // Empathy shields
.float      wing_finished, wing_time, wing_sound;       // Dark-Angel Wings *
.float      mirror_finished, mirror_time, mirror_sound, mirror_ping;  // Reflection
.float      slow_finished, slow_time, slow_ping;    // For those hit by ice arrow.
.float      tome_finished, tome_time;       // Tome of Power.

.float      fighting;   // Checks if monster is really fighting while
                        //  running.  Monsters can run to follow friends.

void SpawnMeatSprayDrake(vector org, vector vel, float btype);

.float(float yaw, float dist)   th_move;    // 'walkmove' or equivalent.
.void(float dist)   th_goal;    // 'movetogoal' or equivalent.
.void()             th_win;     // Monsters:  sound when killing a player.
                                // Missiles:  code prior to origin shift.
                                // Items:  additional code after spawning

.float mindex2;            // used for storing modelindex when phasing in/out
.float      h2olevel;       // 'waterlevel' without the C code baggage.
.float      h2otype;        // Ditto, except for 'watertype'.
.float level; // Used for monster_baron to set it as a boss of different types

// cool.qc
.entity     divine_attacker;    // Who gets credit when lightning strikes.
.float      divine_finished;    // If non-zero, when an entity gets zapped.
.float      divine_hits;        // How many zaps an attacker is allowed.
.float movecount;

// Ruby wand stats.  Normal mode.
// Use 0.3 delay for Heretic wand speed, or 0.4 delay for Doom pistol speed.
float   WAND_CHARGE     = 0.4;      // Delay between charges.
float   WAND_DELAY      = 0.35;     // Reload time and delay between charges.
float   WAND_FULL       = 3;        // Maximum shots.  Super shot is max+1.
float   WAND_DAMAGE     = 16;       // Damage per charge.
float   WAND_RANGE      = 600;

float       multi_headinjury;   // If >=25, blow off his head!

// Power armor damage absorption.
float   DAMAGE_PER_CELL   = 2;

// Note:  Until dragonscale armor was settled on, the dragon morph was
// triggered by an artifact weapon named "Eye/Mark of Bahamut".
// Thus, some names are legacy code written prior to morphing armors.
.float      baha_ammo;          // Renewable energy for dragon form.
.float      baha_time;          // Time it takes for ammo to recharge.
.float      baha_click;         // Helps check for double-clicking.

float       mindex_saw;     // Player with chainsaw.
float        mindex_hammer;
float        mindex_hamout;

float PUSH_ONCE = 1;
float PUSH_CURRENT = 2;     // Zerstorer -- "Silent" PUSH for water currents.
float PUSH_JUMP_ONLY = 4;   // Drake -- Push only when jump is pressed.

.float jump_time;        // Used by custom trigger_jump Q3 style jump pad