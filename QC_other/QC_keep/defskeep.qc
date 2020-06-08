//======================================================================
// MOD    : Keep.
// You keep what you kill (and kill with).
//======================================================================
string MOD_VERSION        = "\b[INFO]\b Keep Mod, version 0.50\n";
.string target3, target4;
//=======================================SELF.ITEMS1======================================================
float IT_SHOTGUN            = 1.000;        //Correct for standard HUD & HIP & DOE            W
float IT_SUPER_SHOTGUN       = 2.000;        //Correct for standard HUD & HIP & DOE            W
float IT_NAILGUN            = 4.000;        //Correct for standard HUD & HIP & DOE            W
float IT_SUPER_NAILGUN        = 8.000;        //Correct for standard HUD & HIP & DOE            W
float IT_GRENADE_LAUNCHER        = 16.000;        //Correct for standard HUD & HIP & DOE            W
float IT_ROCKET_LAUNCHER        = 32.000;        //Correct for standard HUD & HIP & DOE            W
float IT_LIGHTNING            = 64.000;        //Correct for standard HUD & HIP & DOE            W
float IT_SHELLS            = 128.000;        //Correct for DOE HUD                    I
float IT_NAILS            = 256.000;        //Correct for DOE HUD                    I
float IT_ROCKETS            = 512.000;        //Correct for DOE HUD                    I
float IT_CELLS            = 1024.000;        //Correct for DOE HUD                    I
float IT_AXE            = 2048.000;        //Correct for DOE HUD                    W
float IT_LAVA_NAILGUN        = 4096.000;        //Correct for DOE HUD (okay for standard and HIP)    W
float IT_LAVA_SUPER_NAILGUN        = 8192.000;        //Correct for DOE HUD (okay for standard and HIP)    W
float IT_MULTI_GRENADE        = 16384.000;        //Correct for DOE HUD (okay for standard and HIP)    W
float IT_MULTI_ROCKET        = 32768.000;        //Correct for DOE HUD (okay for standard and HIP)    W
float IT_PLASMA_GUN            = 65536.000;        //Correct for DOE HUD                    W
float IT_KEY1            = 131072.000;        //Correct for standard HUD & HIP & DOE            I
float IT_KEY2            = 262144.000;        //Correct for standard HUD & HIP & DOE            I
float IT_INVISIBILITY        = 524288.000;        //Correct for standard HUD & HIP & DOE            I
float IT_INVULNERABILITY        = 1048576.000;        //Correct for standard HUD & HIP & DOE            I
float IT_SUIT            = 2097152.000;        //Correct for standard HUD & HIP & DOE            I
float IT_QUAD            = 4194304.000;        //Correct for standard HUD & HIP & DOE            I
float IT_EXTRA_WEAPON          = 8388608.000;        //Correct for DOE HUD                    W
//=======================================SELF.ITEMS2======================================================
float IT2_ARMOR1            = 1.000;        //Correct for DOE HUD                    I
float IT2_ARMOR2            = 2.000;        //Correct for DOE HUD                    I
float IT2_ARMOR3            = 4.000;        //Correct for DOE HUD                    I
float IT2_LAVA_NAILS        = 8.000;        //Correct for DOE HUD                    I
float IT2_PLASMA_AMMO       = 16.000;        //Correct for DOE HUD                    I
float IT2_MULTI_ROCKETS        = 32.000;        //Correct for DOE HUD                    I
float IT2_SHIELD            = 64.000;        //Correct for DOE HUD                    I
float IT2_CHAINLIGHTNING    = 128.000;        //Zerstorer chain lightning                W    (I Antigrav for DOE HUD)
float IT2_SUPERHEALTH        = 256.000;        //Correct for DOE HUD                    I(available for a new weapon)
float IT2_EARTHQUAKE        = 512.000;        //Correct for DOE HUD                    I(available for a new weapon)
float IT2_V_SPHERE            = 1024.000;        //Correct for DOE HUD                    I(available for a new weapon)
float IT2_BOLTS                = 2048.000;        //Keep specific                        I
float IT2_POISON            = 4096.000;        //Keep specific                        I
float HIP_IT2_WETSUIT        = 8192.000;        //Keep specific (modified from HIP)            I
float HIP_IT2_EMPATHY_SHIELDS   = 16384.000;        //Keep specific (modified from HIP)            I
float HIP_IT2_HORN_OF_CONJURING = 32768.000;        //Keep specific (modified from HIP)            I
                   //= 65536.000;        EMPTY SLOT (skipped...same as IT_PLASMA_GUN)        I
float IT2_PROXIMITY_GUN        = 131072.000;        //Keep specific.Unique number for self.weapon calls     W
float IT2_MJOLNIR         = 262144.000;        //Keep specific    ""                    W
float IT2_LASER_CANNON        = 524288.000;        //Keep specific    ""                    W
float IT2_GRAPPLE        = 1048576.000;        //Keep specific "" W Rogue grapple
float IT2_TRINITY        = 2097152.000;        //Quoth
float IT2_CROSS            = 4194304.000;        //Quoth                            I && W2 (used by Wand)
float IT2_WAND            = 8388608.000;         //Drake magic wand                    W2 (special check to prevent IT_EXTRA_WEAPON conflict


// More powerups -----------------------------------------------------------
// Bits 00-07 (values 1-128) are reserved for health.
// Bits 08-11 (values 256-2048) are reserved for armor.
float   POW_ARMOR1      = 256;      // Divide this into perms for suit value.
float   POW_POWER_ARMOR = 4096;     // Power shield toggle.
// Note:  Do NOT use the next three for exotic armor!
//  That is because item flags use those values for basic armor,
//float   POW_RESIST      = 8192;     // Half damage.
//float   POW_REGEN       = 16384;    // Regeneration.
//- - - - - - - - -
float   POW_DOUBLE      = 65536;    // Twin shot -- nailgun buff.
float   POW_SHOTTYMAN   = 131072;   // Shotgun buff.
float   POW_VAMPIRE     = 262144;   // Melee buff.
float   POW_PACK        = 524288;   // Extra ammo capacity.
float IT_BLASTER = 1048576; // Drake laser blaster                     W  stored in self.perms.  was POW_20 in Drake, unused
float IT_CHAINSAW = 2097152; // Drake chainsaw                        W stored in self.pems.  was POW_21 in Drake, unused.  Chainsaw is modified version of Zerstorer
float   POW_HINT_ARMOR  = 4194304;  // Armor instructions.
float   POW_HINT_FALL   = 8388608;  // nsoe4:  slow falling message.

float   POWx_HEALTH     = 255;      // Extra max health.  First 8 bits.
float   POWx_ARMOR      = 3840;     // Mutually exclustive exotic armor.


//========================================================================================================
float BBOX_ARMAGON = 23;    // -48 -48 -24, 48 48 84
float BBOX_VERMIS = 24;        // -64 -64 -512, 64 64 256
float ARMOR_BLUE = 2;            // Display a blue armour
float AMMO_MAXLAVANAILS = 200;
float AMMO_MAXMULTIROCKETS = 100;
float AMMO_MAXPLASMA = 100;
float AMMO_MAXPOISON = 50;
float AMMO_MAXBOLTS = 100;
float AMMO_POISON_LGR = 10;
float AMMO_POISON_SML = 5;
float AMMO_BOLTS_LGR = 16;
float AMMO_BOLTS_SML = 8;
float GREMLIN_AMMO_TAKE = 30;
float OGRE_MULTI = 2;
string MODEL_GWEAP_LNG = "progs/g_lava.mdl";
string MODEL_GWEAP_LSNG = "progs/g_lava2.mdl";
string MODEL_GWEAP_PROX = "progs/hipnotic/g_prox.mdl";
string MODEL_GWEAP_LASER = "progs/hipnotic/g_laserg.mdl";
string MODEL_GWEAP_MJOL = "progs/hipnotic/g_hammer.mdl";
string MODEL_GWEAP_WAND= "progs/drake/g_wand.mdl";
string MODEL_VWEAP_LAVANG = "progs/v_lava.mdl";            // Lava nailgun
string MODEL_VWEAP_LAVASNG = "progs/v_lava2.mdl";        // Lava Super Nailgun
string MODEL_VWEAP_MULTG = "progs/v_multi.mdl";            // Multi-Grenade Launcher
string MODEL_VWEAP_MULTR = "progs/v_multi2.mdl";        // Multi-Rocket Launcher
string MODEL_VWEAP_PLASM = "progs/v_plasma2.mdl";        // Plasma Gun
string MODEL_VWEAP_PROX = "progs/hipnotic/v_prox.mdl";            // Proximity Gun
string MODEL_VWEAP_MJOL = "progs/hipnotic/v_hammer.mdl";            // Mjolnir
string MODEL_VWEAP_LCAN = "progs/hipnotic/v_laserg.mdl";            // Laser Cannon
string MODEL_PROJ_GRENADEMULTI = "progs/rogue/mervup.mdl";            // Multi-Grenade for Monsters
string SOUND_ARTPSHIELD1 = "shield/pickup.wav";            // Pickup
string SOUND_ARTPSHIELD2 = "shield/hit.wav";            // Active
string SOUND_ARTPSHIELD3 = "shield/fadeout.wav";        // Fading
string SOUND_ARTCROSS2 = "items2/crosuse.wav";            // Active
string SOUND_ARTEMPATH2 = "hipitems/empathy2.wav";        // Active

//.float oldgravity;
.float versustype;
.float therm_finished;
.float therm_time;
.float ammo_bolts;
.float ammo_poison;
.float devdata;
.entity deventity;
.float enemyhealth;

float DEV_AMMO_N_HEALTH = 1;

// Versus gamemode
float COOP_VERSUS = 3;
float VT_SURVIVOR = 2;
float VT_GAMEMASTER = 4;

float GLOBALSTATE_1     = 1.000;
float GLOBALSTATE_2      = 2.000;
float GLOBALSTATE_3        = 4.000;
float GLOBALSTATE_4    = 8.000;
float GLOBALSTATE_5        = 16.000;
float GLOBALSTATE_6        = 32.000;
float GLOBALSTATE_7        = 64.000;
float GLOBALSTATE_8        = 128.000;
float GLOBALSTATE_9        = 256.000;
float GLOBALSTATE_10    = 512.000;
float GLOBALSTATE_11    = 1024.000;
float GLOBALSTATE_12    = 2048.000;
float GLOBALSTATE_13    = 4096.000;
float GLOBALSTATE_14       = 8192.000;
float GLOBALSTATE_15    = 16384.000;
float GLOBALSTATE_16    = 32768.000;
float GLOBALSTATE_17        = 65536.000;
float GLOBALSTATE_18    = 131072.000;
float GLOBALSTATE_19    = 262144.000;
float GLOBALSTATE_20    = 524288.000;
float GLOBALSTATE_21    = 1048576.000;
float GLOBALSTATE_22    = 2097152.000;
float GLOBALSTATE_23    = 4194304.000;
float GLOBALSTATE_24      = 8388608.000;
float globalstate;
.float stateflags;
.string statetarget1;
.string statetarget2;
.string statetarget3;
.string statetarget4;
.string statetarget5;
.string statetarget6;
.string statetarget7;
.string statetarget8;
.string statetarget9;
.string statetarget10;
.string statetarget11;
.string statetarget12;
.string statetarget13;
.string statetarget14;
.string statetarget15;
.string statetarget16;
.string statetarget17;
.string statetarget18;
.string statetarget19;
.string statetarget20;
.string statetarget21;
.string statetarget22;
.string statetarget23;
.string statetarget24;

float numsurvivor;
float numgamemaster;
//float versus_setup_istrue;
.float spritestarted;
.float useold_axe;
.float take_perms;
.float nailcosthalf; // bool for checking if it is time to subtract a nail from ammo
float coins;  // Currency for survival player(s)
float bones;  // Currency for gamemaster player(s)
string g_axemodel;
string v_axemodel;

.vector dest;    // Could not find this in the source, added
//.vector lastpos;

entity annie; // anaconda entity

.entity gravtrigent; //the trigger_setgravity we are in
.float gravtrig; //bit to say whether or not we are in a gravity trigger