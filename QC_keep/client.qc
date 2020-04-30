/*======================================================================
 CLIENT FUNCTIONS
======================================================================*/
// Vanilla Quake Inventory reset combined flag
// IT_SUPERHEALTH, IT_KEY1, IT_KEY2 
// IT_INVISIBILITY, IT_INVULNERABILITY, IT_SUIT, IT_QUAD
float IT_ITEMRESET = 7864320;        // 65536++
float IT_MODRESET = 8380416;        // 8192++
float IT2_ITEMRESET = 7342016;
float ALL_WEAPONS = 8517759;

// client_camera.qc
void() SetupIntermissionCamera;
void() CycleIntermissionCamera;

// client_debuff.qc AND client_power.qc
void() ClientDeBuff;
void() ClientPowerups;
void() GrappleService;

// player.qc
void(entity inflictor, entity attacker, float damage) player_pain;
void() player_death;
void() player_stand1;

// triggers.qc
void() trigger_changelevel_finish;




//========================================================================
// SPECIAL parm# ENCODING CODE TO STORE MULTIPLE VALUES IN A SINGLE parm#

// boundary check function
// take a float value and return:

// < 0 - return 0
// 0 - MAX - return integer value, always rounding down
// > MAX - return MAX
// where MAX is the max bit value, 255 is used with standard encoder below

float(float f, float MAX) f_bound =
{
if (MAX > 16777215) MAX = 16777215;

if (f > MAX)
f = MAX;
else if (f < 0)
f = 0;
f = floor(f);
return f;
};

// parm* value encoding and decoding
// maximize storage of data in parm* values
// most ammo data has a maximum of 200, armor is 200, health is 100, etc.
// this can be encoded into 8 bits -
// the original code uses 1 parm* per data point - a serious waste of bits
// each parm* variable has 24 bits - 3 * 8 bit values

// encode 3 values according to formulas with all values bounded 0 - 255
// values are set into float value 0 - 16777215

// where
// s1 - into high 8 bits
// s2 - into middle 8 bits
// s3 - into low 8 bits

float(float s1, float s2, float s3) encode =
{
local float f;
f = f_bound(s1, 255) * 65536 + f_bound(s2, 255) * 256 + f_bound(s3, 255);
return f;
};

// generic encode

// s1 is value
// bits are max value - 1, 3, 7, 15, 31...255...65535...{n} - a series of 1 bits
// pos is the position in the parm* variable
// - position will be the next bit value above the previous bits
// - i.e. for 3 value bits at position 1, the next position is 4
// - for 7 value bits at position 4, the next position is 32
// binary math equivalent:
// - 00000011 - 3 value at 1
// - 00011100 - 7 value at 4
// - 11100000 - 7 value at 32

// you could encode any values in any position with this function
// it is included here as an example if you have values smaller or larger than 255 to encode
// such as the painkeep carry items with a limit of 3 on most inventory

float(float s1, float bits, float pos) g_encode =
{
local float f;
f = f_bound(s1, bits) * pos;
return f;
};

// recover an 8 bit value (0 - 255) from parm encoding

// parmval - passed value of parm* variable

// where which is
// 3 - return high 8 bits
// 2 - return middle 8 bits
// 1 - return low 8 bits
// any other value - return low 8 bits

// use these handy codes to avoid confusion
float HI8 = 3; // s3 value of encode() call
float MID8 = 2; // s2 value of encode() call
float LOW8 = 1; // s1 value of encode() call

float(float parmval, float which) decode =
{
local float f;

if (which == 1)
f = (parmval / 65536) & 255;
else if (which == 2)
f = (parmval / 256) & 255;
else
f = parmval & 255;
return f;
};

// generic decode - extract data from generic encode above

// parmval is the parm* variable data
// bits are max value - 1, 3, 7, 15, 31...255...65535...{n} - a series of 1 bits
// pos is the position in the parm* variable

// this uses the same bits & pos to extract any generic encoded data
// see notes under g_encode for more details

float(float parmval, float bits, float pos) g_decode =
{
local float f;

f = (parmval / pos) & bits;
return f;
};

//========================================================================
//========================================================================







/*======================================================================
 USED : Load (map command) NEW MAP or NEW GAME
 This is a function called via the engine for new games
 Resets all parm data back to default values, followed by Decode
======================================================================*/
void() SetNewParms =
{
    dprint("\b[CLIENT]\b SetNewParms\n");

    // Brand new game/map, reset start position settings
    update_configflag(SVR_SPAWN_BIT1, FALSE);
    update_configflag(SVR_SPAWN_BIT2, FALSE);
    update_configflag(SVR_SPAWN_BIT3, FALSE);
    fog_active = FALSE;
    
    parm1 = IT_SHOTGUN | IT_AXE;    // Default self.items, give shotgun and axe to start
    parm2 = encode(100, 0, 0);        // Default Health, Armortype, Armorvalue in that order
    //parm2 = 100;                     // Starting Health
    //parm3 = 0;                    // Armourtype
    //parm9 = 0;                    // Armourvalue

    parm3 = encode(25,0,0);            // Default shells, nails, rockets in that order
    //parm4 = 25;                        // Ammo qty shells
    //parm5 = 0;                        // Ammo qty nails
    //parm6 = 0;                        // Ammo qty rockets

    parm4 = 0;                        // Ammo qty cells, bolts, poison in that order
    //parm7 = 0;                        // Ammo qty cells
    
    parm5 = 0;                        // Ammo qty lava_nails, multi_rockets, plasma in that order

    parm8 = 1;                        // Current selected weapon
    parm10 = 0;                        // New MOD items
    parm11 = 0;                        // self.items2
    parm12 = 0;                        // globalstate
    parm13 = 0;                        // permanent powerups (perms) from Drake, can be reset this level using world.take_perms > 0

    // Update new player inventory based on config flag
    if (query_configflag(SVR_UPDAXE)) parm10 = parm10 | IT_UPGRADE_AXE;
    if (query_configflag(SVR_UPDSSG)) parm10 = parm10 | IT_UPGRADE_SSG;
    if (query_configflag(SVR_UPDLG)) parm10 = parm10 | IT_UPGRADE_LG;

    // Are map variables live?
    if (mapvar_cvar) dprint("\b[CLIENT]\b LIVE Map Variables Detected\n");
    else {
        dprint("\b[CLIENT]\b RESETTING Map Variables\n");
        // Reset map variables ready for use
        parm6 = parm7 = parm9 = parm14 = parm15 = parm16 = 0;
        mapvar_reset();
    }
};

/*======================================================================
// USED : trigger CHANGE LEVEL
// Stores a copy of the parm data ready for death/level reset
======================================================================*/
void() SetChangeParms =
{
    dprint("\n\b[CLIENT]\b SetChangeParms\n");
    if (self.health < 1) { SetNewParms (); return;    }

    // remove temporary items (keys and powerups)
    self.items = self.items - (self.items & IT_ITEMRESET);
    parm1 = self.items;
    self.moditems = self.moditems - (self.moditems & IT_MODRESET);
    parm10 = self.moditems;
    dprint("\b[CLIENT]\b MOD weapons ("); dprint(ftos(parm10)); dprint(")\n");
    self.items2 = self.items2 - (self.items2 & IT2_ITEMRESET);
    parm11 = self.items2; // save the good stuff!  multi-weapons and such
    
    // cap super health
    if (self.health > HEAL_PLAYMAX) self.health = HEAL_PLAYMAX;
    if (self.health < 50) self.health = 50; //heal me on level change up to 50 from potentially 1
    parm2 = encode(self.health, self.armortype, self.armorvalue);
    //parm2 = self.health;
    //parm9 = self.armortype * 100;
    //parm3 = self.armorvalue;

    parm3 = encode(self.ammo_shells,self.ammo_nails,self.ammo_rockets);
    // Always make sure the player has 25 shells
    // Uh no, no we won't
    //if (self.ammo_shells < DEF_SHELLS) parm4 = DEF_SHELLS;
    //else parm4 = self.ammo_shells;
    //parm4 = self.ammo_shells;
    //parm5 = self.ammo_nails;
    //parm6 = self.ammo_rockets;
    parm4 = encode(self.ammo_cells,self.ammo_bolts,self.ammo_poison);
    //parm7 = self.ammo_cells;
    parm5 = encode(self.ammo_lava_nails,self.ammo_multi_rockets,self.ammo_plasma);
    //parm6;
    //parm7;
    parm8 = self.weapon;
    //parm9;
    //parm10 is self.moditems above
    //parm11 is self.items2 above
    parm12 = globalstate;
    parm13 = self.perms;
    //parm14;
    //parm15;
    //parm16;
};

/*======================================================================
 USED : Load NEW MAP or trigger CHANGE LEVEL
 Not used when loading a saved file or quickload
======================================================================*/
void() DecodeWorldParms =
{
    local float parm_items, tlow8, tmid8, thi8;
    parm_items = parm1;

    dprint("\b[CLIENT]\b Decode Worldspawn Inv Parms\n");
    //----------------------------------------------------------------------
    // Show developer feedback on player inventory update
    if (world.give_weapons) {
        dprint("\b[CLIENT]\b Weapon Give ( ");
        if (world.give_weapons & IT_SHOTGUN) dprint("SG ");
        if (world.give_weapons & IT_SUPER_SHOTGUN) dprint("SSG ");
        if (world.give_weapons & IT_NAILGUN) dprint("NG ");
        if (world.give_weapons & IT_SUPER_NAILGUN) dprint("SNG ");
        if (world.give_weapons & IT_GRENADE_LAUNCHER) dprint("GL ");
        if (world.give_weapons & IT_ROCKET_LAUNCHER) dprint("RL ");
        if (world.give_weapons & IT_LIGHTNING) dprint("LG ");
        dprint(")\n");
    }
    if (world.take_weapons && (world.take_weapons < 99999999)) {
        dprint("\b[CLIENT]\b Weapon Take ( ");
        if (world.take_weapons & IT_SHOTGUN) dprint("SG ");
        if (world.take_weapons & IT_SUPER_SHOTGUN) dprint("SSG ");
        if (world.take_weapons & IT_NAILGUN) dprint("NG ");
        if (world.take_weapons & IT_SUPER_NAILGUN) dprint("SNG ");
        if (world.take_weapons & IT_GRENADE_LAUNCHER) dprint("GL ");
        if (world.take_weapons & IT_ROCKET_LAUNCHER) dprint("RL ");
        if (world.take_weapons & IT_LIGHTNING) dprint("LG ");
        dprint(")\n");
    }
    
    //----------------------------------------------------------------------
    // Adding weapons to existing inventory
    if (world.give_weapons) {
        parm_items = parm_items | (world.give_weapons & ALL_WEAPONS);        
    }
    //----------------------------------------------------------------------
    if (world.take_weapons > 0) {
        if (world.take_weapons > 999) {
            parm_items = 0;   //take all
            parm13 = parm13 - (parm13 & IT_BLASTER);
            parm8 = 0;
            parm4 = 0;
            dprint("\b[CLIENT]\b Weapon Take ( ");
            dprint("ALL WEAPONS! ");
            dprint(")\n");
        } else {
            if (world.take_weapons & IT_SHOTGUN && parm_items & IT_SHOTGUN) {
                parm_items = parm_items - IT_SHOTGUN;
                parm13 = parm13 - (parm13 & IT_BLASTER);
            }
            if (world.take_weapons & IT_SUPER_SHOTGUN && parm_items & IT_SUPER_SHOTGUN) {
                parm_items = parm_items - IT_SUPER_SHOTGUN;
                // Player cannot have upgrades without base item
                parm10 = parm10 - (parm10 & IT_UPGRADE_SSG);
            }
            if (world.take_weapons & IT_NAILGUN && parm_items & IT_NAILGUN) 
                parm_items = parm_items - IT_NAILGUN; 
            if (world.take_weapons & IT_SUPER_NAILGUN && parm_items & IT_SUPER_NAILGUN) 
                parm_items = parm_items - IT_SUPER_NAILGUN; 
            if (world.take_weapons & IT_GRENADE_LAUNCHER && parm_items & IT_GRENADE_LAUNCHER) 
                parm_items = parm_items - IT_GRENADE_LAUNCHER; 
            if (world.take_weapons & IT_ROCKET_LAUNCHER && parm_items & IT_ROCKET_LAUNCHER) 
                parm_items = parm_items - IT_ROCKET_LAUNCHER; 
            if (world.take_weapons & IT_LIGHTNING && parm_items & IT_LIGHTNING) {
                parm_items = parm_items - IT_LIGHTNING; 
                // Player cannot have upgrades without base item
                parm10 = parm10 - (parm10 & IT_UPGRADE_LG);
            }
        }
    }
    
    // Store result back to global variable
    parm1 = parm_items;
    
    // parm2 health | armortype | armorvalue
    tlow8 = decode(parm2,LOW8);
    tmid8 = decode(parm2,MID8);
    thi8 = decode(parm2,HI8);
    //----------------------------------------------------------------------
    // Update player health (reset or minimum value)
    if (world.reset_health > 0 && world.reset_health <= HEAL_PLAYMAX) {
        dprint("\b[CLIENT]\b Health ("); dprint(ftos(tlow8)); dprint(") To (");
        tlow8 = world.reset_health; //uses low8 position
        parm2 = encode(tlow8,tmid8,thi8);
        dprint(ftos(tlow8)); dprint(")\n");    
    }
    else if (world.max_health && (tlow8 < world.max_health) ) {
        dprint("\b[CLIENT]\b Health ("); dprint(ftos(tlow8)); dprint(") To (");
        if (tlow8 < world.max_health) {
            tlow8 = world.max_health; //uses low8 position
            parm2 = encode(tlow8,tmid8,thi8);
        }
        dprint(ftos(tlow8)); dprint(")\n");    
    }
    
    //----------------------------------------------------------------------
    // Update the inventory ammo quantities
    // currentammo = 1 to reset the inventory to worldspawn values
    // currentammo = 0 to use the worldspawn values as a minimum quantity

    // parm3 = shells | nails | rockets
    tlow8 = decode(parm3,LOW8);
    tmid8 = decode(parm3,MID8);
    thi8 = decode(parm3,HI8);
    if (world.ammo_shells || (world.currentammo && tlow8 > 0)) {
        dprint("\b[CLIENT]\b Ammo Shells ("); dprint(ftos(tlow8)); dprint(") To (");
        if (world.currentammo) tlow8 = world.ammo_shells;
        else if (tlow8 < world.ammo_shells) tlow8 = world.ammo_shells;
        if (tlow8 > AMMO_MAXSHELLS) tlow8 = AMMO_MAXSHELLS;
        else if (tlow8 < 0) tlow8 = 0;
        dprint(ftos(tlow8)); dprint(")\n");
        parm3 = encode(tlow8,tmid8,thi8); //store shells back in parm3
    }
    if (world.ammo_nails || (world.currentammo && tmid8 > 0)) {
        dprint("\b[CLIENT]\b Ammo Spikes ("); dprint(ftos(tmid8)); dprint(") To (");
        if (world.currentammo) tmid8 = world.ammo_nails;
        else if (tmid8 < world.ammo_nails) tmid8 = world.ammo_nails;
        if (tmid8 > AMMO_MAXNAILS) tmid8 = AMMO_MAXNAILS;
        else if (tmid8 < 0) tmid8 = 0;
        dprint(ftos(tmid8)); dprint(")\n");
        parm3 = encode(tlow8,tmid8,thi8); //store nails back in parm3
    }
    if (world.ammo_rockets || (world.currentammo && thi8 > 0)) {
        dprint("\b[CLIENT]\b Ammo Rockets ("); dprint(ftos(thi8)); dprint(") To (");
        if (world.currentammo) thi8 = world.ammo_rockets;
        else if (thi8 < world.ammo_rockets) thi8 = world.ammo_rockets;
        if (thi8 > AMMO_MAXROCKETS) thi8 = AMMO_MAXROCKETS;
        else if (thi8 < 0) thi8 = 0;
        dprint(ftos(thi8)); dprint(")\n");
        parm3 = encode(tlow8,tmid8,thi8); //store rockets back in parm3
    }

    // parm4 = cells | bolts | poison
    tlow8 = decode(parm4,LOW8);
    tmid8 = decode(parm4,MID8);
    thi8 = decode(parm4,HI8);
    if (world.ammo_cells || (world.currentammo && tlow8 > 0)) {
        dprint("\b[CLIENT]\b Ammo Cells ("); dprint(ftos(tlow8)); dprint(") To (");
        if (world.currentammo) tlow8 = world.ammo_cells;
        else if (tlow8 < world.ammo_cells) tlow8 = world.ammo_cells;
        if (tlow8 > AMMO_MAXROCKETS) tlow8 = AMMO_MAXROCKETS;
        else if (tlow8 < 0) tlow8 = 0;
        dprint(ftos(tlow8)); dprint(")\n");    
        parm4 = encode(tlow8,tmid8,thi8); //store cells back in parm4
    }
    if (world.ammo_bolts || (world.currentammo && tmid8 > 0)) {
        dprint("\b[CLIENT]\b Ammo Bolts ("); dprint(ftos(tmid8)); dprint(") To (");
        if (world.currentammo) tmid8 = world.ammo_bolts;
        else if (tmid8 < world.ammo_bolts) tmid8 = world.ammo_bolts;
        if (tmid8 > AMMO_MAXBOLTS) tmid8 = AMMO_MAXBOLTS;
        else if (tmid8 < 0) tmid8 = 0;
        dprint(ftos(tmid8)); dprint(")\n");
        parm4 = encode(tlow8,tmid8,thi8); //store bolts back in parm4
    }
    if (world.ammo_poison || (world.currentammo && thi8 > 0)) {
        dprint("\b[CLIENT]\b Ammo Poison ("); dprint(ftos(thi8)); dprint(") To (");
        if (world.currentammo) thi8 = world.ammo_poison;
        else if (thi8 < world.ammo_poison) thi8 = world.ammo_poison;
        if (thi8 > AMMO_MAXPOISON) thi8 = AMMO_MAXPOISON;
        else if (thi8 < 0) thi8 = 0;
        dprint(ftos(thi8)); dprint(")\n");
        parm4 = encode(tlow8,tmid8,thi8); //store poison back in parm4
    }

    // parm5 = lava_nails | multi_rockets | plasma
    tlow8 = decode(parm5,LOW8);
    tmid8 = decode(parm5,MID8);
    thi8 = decode(parm5,HI8);
    if (world.ammo_lava_nails || (world.currentammo && tlow8 > 0)) {
        dprint("\b[CLIENT]\b Ammo Lava Nails ("); dprint(ftos(tlow8)); dprint(") To (");
        if (world.currentammo) tlow8 = world.ammo_lava_nails;
        else if (tlow8 < world.ammo_lava_nails) tlow8 = world.ammo_lava_nails;
        if (tlow8 > AMMO_MAXLAVANAILS) tlow8 = AMMO_MAXLAVANAILS;
        else if (tlow8 < 0) tlow8 = 0;
        dprint(ftos(tlow8)); dprint(")\n");    
        parm5 = encode(tlow8,tmid8,thi8); //store lava_nails back in parm5
    }
    if (world.ammo_multi_rockets || (world.currentammo && tmid8 > 0)) {
        dprint("\b[CLIENT]\b Ammo Multi-Rockets ("); dprint(ftos(tmid8)); dprint(") To (");
        if (world.currentammo) tmid8 = world.ammo_multi_rockets;
        else if (tmid8 < world.ammo_multi_rockets) tmid8 = world.ammo_multi_rockets;
        if (tmid8 > AMMO_MAXMULTIROCKETS) tmid8 = AMMO_MAXMULTIROCKETS;
        else if (tmid8 < 0) tmid8 = 0;
        dprint(ftos(tmid8)); dprint(")\n");
        parm5 = encode(tlow8,tmid8,thi8); //store multi_rockets back in parm5
    }
    if (world.ammo_plasma || (world.currentammo && thi8 > 0)) {
        dprint("\b[CLIENT]\b Ammo Plasma ("); dprint(ftos(thi8)); dprint(") To (");
        if (world.currentammo) thi8 = world.ammo_plasma;
        else if (thi8 < world.ammo_plasma) thi8 = world.ammo_plasma;
        if (thi8 > AMMO_MAXPLASMA) thi8 = AMMO_MAXPLASMA;
        else if (thi8 < 0) thi8 = 0;
        dprint(ftos(thi8)); dprint(")\n");
        parm5 = encode(tlow8,tmid8,thi8); //store plasma back in parm5
    }


    //----------------------------------------------------------------------
    // Update player armour type and quantity
    if (world.armortype > 0) {
        // Remove any previous armour types from the HUD
        parm11 = parm11 - (parm11 & (IT2_ARMOR1 | IT2_ARMOR2 | IT2_ARMOR3));
        // Setup any armour quantity first, limit check with armour type
        if (world.armorvalue > 0) {
            tlow8 = decode(parm2,LOW8); // health
            tmid8 = decode(parm2,MID8); // armortype
            parm2 = encode(tlow8,tmid8,world.armorvalue);
        }

        // Work through the different armour types, resetting armour quantity
        if (world.armortype == 1) {
            parm11 = parm11 | IT2_ARMOR1;
            tlow8 = decode(parm2,LOW8);
            tmid8 = decode(parm2,MID8);
            thi8 = ARMOR_GRN_TYPE * 100;  //parm9 = ARMOR_GRN_TYPE * 100;
            if (!world.armorvalue || thi8 > ARMOR_GRN_VALUE) thi8 = ARMOR_GRN_VALUE;
            dprint("\b[CLIENT]\b Green Armor ("); dprint(ftos(thi8)); dprint(")\n");
            parm2 = encode(tlow8,tmid8,thi8);

        }
        else if (world.armortype == 2) {
            parm11 = parm11 | IT2_ARMOR2;    
            tlow8 = decode(parm2,LOW8);
            tmid8 = decode(parm2,MID8);
            thi8 = ARMOR_YEL_TYPE * 100;  //parm9 = ARMOR_YEL_TYPE * 100;
            if (!world.armorvalue || thi8 > ARMOR_YEL_VALUE) thi8 = ARMOR_YEL_VALUE;
            dprint("\b[CLIENT]\b Yellow Armor ("); dprint(ftos(thi8)); dprint(")\n");
            parm2 = encode(tlow8,tmid8,thi8);

        }
        else if (world.armortype == 3) {
            parm11 = parm11 | IT2_ARMOR3;    
            tlow8 = decode(parm2,LOW8);
            tmid8 = decode(parm2,MID8);
            thi8 = ARMOR_RED_TYPE * 100;  //parm9 = ARMOR_RED_TYPE * 100;
            if (!world.armorvalue || thi8 > ARMOR_RED_VALUE) thi8 = ARMOR_RED_VALUE;
            dprint("\b[CLIENT]\b Red Armor ("); dprint(ftos(thi8)); dprint(")\n");
            parm2 = encode(tlow8,tmid8,thi8);

        } else {
            // Default is no armour
            tlow8 = decode(parm2,LOW8);
            //parm3 = 0;
            thi8 = decode(parm2,HI8);
            parm2 = encode(tlow8,0,thi8);
        }
    }

    if (world.take_perms > 0) {
        parm13 = 0; // take away all permanent powerups
    }
};

/*======================================================================
 USED : Load NEW MAP or trigger CHANGE LEVEL
 Double check the player inventory matches the server/config flags
 Check worldspawn for add/removal of upgrade items
======================================================================*/
void() SyncModInventory =
{
    //----------------------------------------------------------------------
    // Shadow Axe
    //----------------------------------------------------------------------
    // Check worldspawn for upgrade_axe first, add/deny inventory
    if (world.upgrade_axe < 0) {
        dprint("\b[MODINV]\b Worldspawn removing Axe Upgrade\n");
        parm10 = parm10 - (parm10 & IT_UPGRADE_AXE);
        update_configflag(SVR_UPDAXE, FALSE);
    }
    else if (world.upgrade_axe > 0) {
        dprint("\b[MODINV]\b Worldspawn adding Axe Upgrade\n");
        parm10 = parm10 | IT_UPGRADE_AXE;
        parm1 = parm1 | IT_AXE;
        update_configflag(SVR_UPDAXE, TRUE);
    }
    // Finally syncing the player inventory to serverflags
    else if (parm10 & IT_UPGRADE_AXE) {
        dprint("\b[MODINV]\b Inventory adding Axe Upgrade\n");
        update_configflag(SVR_UPDAXE, TRUE);
    }
    
    //----------------------------------------------------------------------
    // Widowmaker Shotgun
    //----------------------------------------------------------------------
    // Check worldspawn for upgrade_ssg first, add/deny inventory
    if (world.upgrade_ssg < 0) {
        dprint("\b[MODINV]\b Worldspawn removing SSG Upgrade\n");
        parm10 = parm10 - (parm10 & IT_UPGRADE_SSG);
        update_configflag(SVR_UPDSSG, FALSE);
    }
    else if (world.upgrade_ssg > 0) {
        dprint("\b[MODINV]\b Worldspawn adding SSG Upgrade\n");
        parm10 = parm10 | IT_UPGRADE_SSG;
        parm1 = parm1 | IT_SUPER_SHOTGUN;
        update_configflag(SVR_UPDSSG, TRUE);
    }
    // Syncing the player inventory to serverflags
    else if (parm10 & IT_UPGRADE_SSG) {
        dprint("\b[MODINV]\b Inventory adding SSG Upgrade\n");
        update_configflag(SVR_UPDSSG, TRUE);
    }

    //----------------------------------------------------------------------
    // Projectile Shotgun
    //----------------------------------------------------------------------
    // Check for worldspawn feature to turn off projectiles + casing
    if (world.no_sgprojectile > 0) 
        update_configflag(SVR_SHOTGPROJ, TRUE);
    if (world.no_sgcasing > 0)
        update_configflag(SVR_SHOTGCASE, TRUE);

    //----------------------------------------------------------------------
    // Plasma Gun
    //----------------------------------------------------------------------
    // Check worldspawn for upgrade_lg first, add/deny inventory
    if (world.upgrade_lg < 0) {
        dprint("\b[MODINV]\b Worldspawn removing LG Upgrade\n");
        parm10 = parm10 - (parm10 & IT_UPGRADE_LG);
        update_configflag(SVR_UPDLG, FALSE);
    }
    else if (world.upgrade_lg > 0) {
        dprint("\b[MODINV]\b Worldspawn adding LG Upgrade\n");
        parm10 = parm10 | IT_UPGRADE_LG;
        parm1 = parm1 | IT_LIGHTNING;
        update_configflag(SVR_UPDLG, TRUE);
    }
    // Finally syncing the player inventory to serverflags
    else if (parm10 & IT_UPGRADE_LG) {
        dprint("\b[MODINV]\b Inventory adding LG Upgrade\n");
        update_configflag(SVR_UPDLG, TRUE);
    }
};

/*======================================================================
 USED : Load NEW MAP or trigger CHANGE LEVEL
 Not used when loading a saved file or quickload
======================================================================*/
void() DecodeLevelParms =
{
    dprint("\b[CLIENT]\b Decode (Level) Parms\n");
    // Map Variables are active (do not reset)
    mapvar_cvar = TRUE;
    // Found any runes? going back to start map? reset inventory
    if (serverflags & SVR_RUNE_ALL) {
        // take away all stuff on starting new episode
        if (world.model == "maps/start.bsp") SetNewParms ();
    }
    
    DecodeWorldParms();        // Decode worldspawn client inv updates
    sync_serverflags();        // update/sync serverflags to worldspawn
    SyncModInventory();        // update/sync mod inventory items

    // Read all current params into client variables
    self.items = parm1;
    self.health = decode(parm2,LOW8);
    self.armortype = decode(parm2,MID8);
    self.armorvalue = decode(parm2,HI8);
    self.ammo_shells = decode(parm3,LOW8);
    self.ammo_nails = decode(parm3,MID8);
    self.ammo_rockets = decode(parm3,HI8);
    self.ammo_cells = decode(parm4,LOW8);
    self.ammo_bolts = decode(parm4,MID8);
    self.ammo_poison = decode(parm4,HI8);
    self.ammo_lava_nails = decode(parm5,LOW8);
    self.ammo_multi_rockets = decode(parm5,MID8);
    self.ammo_plasma = decode(parm5,HI8);
    // parm6;
    // parm7;
    self.weapon = parm8;
    // parm9;
    self.moditems = parm10;
    self.items2 = parm11;
    globalstate = parm12;
    self.perms = parm13;
    // parm14;
    // parm15;
    // parm16;

    dprint("\b[CLIENT]\b self.weapon == ");
    dprint(ftos(self.weapon));
    dprint("\n");

    dprint("\b[CLIENT]\b self.items == ");
    dprint(ftos(self.items));
    dprint("\n");

    dprint("\b[CLIENT]\b Reading Map Variables\n");
    // Read/Setup map variables (22 bits per parm, 132 total)
    mapvar[0] = parm6;
    mapvar[1] = parm7;
    mapvar[2] = parm9;
    mapvar[3] = parm14;
    mapvar[4] = parm15;
    mapvar[5] = parm16;

    // Check for any worldspawn map variable settings
    if (CheckZeroVector(world.mapvar_update) == FALSE)
        mapvar_range(world.mapvar_update);
};

//----------------------------------------------------------------------
void() GotoNextMap =
{
    // if samelevel is set, stay on same level
    if (cvar("samelevel")) changelevel (mapname);
    else changelevel (nextmap);
};

/*======================================================================
 NextLevel (DM ONLY)
 - A special function designed for MP only, will cycle around
   the first map of each episode until someone quits
   gives the player runes as well to tell which to load
======================================================================*/
void() NextLevel =
{
    local entity o;

    if (mapname == "start") {
        if (!cvar("registered")) mapname = "e1m1";
        else if (query_configflag(SVR_RUNE_KEY1) == FALSE) {
            mapname = "e1m1";
            update_configflag(SVR_RUNE_KEY1, TRUE);
        }
        else if (query_configflag(SVR_RUNE_KEY2) == FALSE) {
            mapname = "e2m1";
            update_configflag(SVR_RUNE_KEY1, TRUE);
        }
        else if (query_configflag(SVR_RUNE_KEY3) == FALSE) {
            mapname = "e3m1";
            update_configflag(SVR_RUNE_KEY1, TRUE);
        }
        else if (query_configflag(SVR_RUNE_KEY4) == FALSE) {
            mapname = "e4m1";
            update_configflag(SVR_RUNE_KEY1, FALSE);
            update_configflag(SVR_RUNE_KEY2, FALSE);
            update_configflag(SVR_RUNE_KEY3, FALSE);
        }

        o = spawn();
        o.map = mapname;
    }
    else {
        // find a trigger changelevel
        o = find(world, classname, "trigger_changelevel");

        // go back to start if no trigger_changelevel
        if (!o) {
            mapname = "start";
            o = spawn();
            o.map = mapname;
        }
    }

    nextmap = o.map;
    gameover = TRUE;
    
    if (o.nextthink < time) {
        o.think = trigger_changelevel_finish;
        o.nextthink = time + 0.1;
    }
};

/*======================================================================
 CheckRules (DM ONLY)
 - Check death timers (time/frag limits)
   and decide if to load a new map or not
======================================================================*/
void() CheckRules =
{
    local float timelimit, fraglimit;

    if (deathmatch == 0) return;    // Playing SP?
    if (gameover) return;            // someone else quit the game already
        
    timelimit = cvar("timelimit") * 60;
    fraglimit = cvar("fraglimit");
    
    if (timelimit && time >= timelimit) NextLevel();
    else if (fraglimit && self.frags >= fraglimit) NextLevel();
};

//----------------------------------------------------------------------
void() SetupSpawnCoopLocations =
{
    local entity cooploc, prevloc;
    
    // The first spawn for coop is always the SP start location
    cooploc = find (world, classname, "info_player_coop");
    prevloc = world;
    
    // Any coop spawns exist?
    if (cooploc) {
        // Create a list
        while(cooploc) {
            // first spawn in chain? setup first and previous spawns
            if (!prevloc) { 
                coop_ent = cooploc; 
                coop_ent.count = 0;
            }
            else prevloc.owner = cooploc;

            // Save current spawn location to previous ready for loop
            prevloc = cooploc;
            coop_ent.count = coop_ent.count + 1;
                
            cooploc = find(cooploc, classname, "info_player_coop");
        }
        // Close the chain
        prevloc.owner = coop_ent;
        dprint("\b[COOP]\b Spawn locations found (");
        dprint(ftos(coop_ent.count)); dprint(")\n");
    }
    else {
        // No coop spawns, use start location
        dprint("\b[COOP]\b Spawn locations Missing!\n");
        coop_ent = find (world, classname, "info_player_start");
        // no start location, WTF!?!
        if (!coop_ent) coop_ent = client_ent;
        coop_ent.owner = coop_ent;
    }
};

//----------------------------------------------------------------------
// Find a location for the player to spawn at
//----------------------------------------------------------------------
entity() SelectSpawnPoint =
{
    local entity spot, spot2, thing;
    local float  pcount, start2;
    
    // Always use a player start as default position
    if (!lastspawn) lastspawn = find (world, classname, "info_player_start");

    // Cycle through list of info_player_coop location
    if (coop > 0) {
        // Check for coop spawn list
        if (!coop_ent) return lastspawn;
        // If first entry of coop list start player then its broken
        else if (coop_ent.classtype == CT_SPAWNSP) return lastspawn;
        else {
            // Setup coop list if last location isstart player
            if (lastspawn.classtype == CT_SPAWNSP) lastspawn = coop_ent;
            // Move forward in coop spawn location list
            lastspawn = lastspawn.owner;
            // Check if spawn location is active?
            if (lastspawn.estate & ESTATE_BLOCK) {
                pcount = coop_ent.count;
                while(pcount > 0) {
                    lastspawn = lastspawn.owner;
                    if (lastspawn.estate == ESTATE_ON) pcount = -1;
                    else pcount = pcount - 1;
                }
            }
            // Double check if spawn location free?
            if (lastspawn.estate & ESTATE_BLOCK) {
                // Default back to player start if not available
                lastspawn = find (world, classname, "info_player_start");
            }
            return lastspawn;
        }
    }
    // choose a info_player_deathmatch point
    else if (deathmatch > 0) {
        spot = lastspawn;
        while (1) {
            spot = find(spot, classname, "info_player_deathmatch");
            if (spot == world) spot = find (world, classname, "info_player_start");
            if (spot != world) {
                if (spot == lastspawn) return lastspawn;
                pcount = 0;
                thing = findradius(spot.origin, 32);
                while(thing) {
                    if (thing.flags & FL_CLIENT) pcount = pcount + 1;
                    thing = thing.chain;
                }
                if (pcount == 0) {
                    lastspawn = spot;
                    return spot;
                }
            }
        }
    }

    // Check for any info_player_start2 special spawn locations
    start2 = mathlib_bitvalue( query_configflag(SVR_SPAWN_BIT1), SVR_SPAWN_BIT1);
    start2 = start2 + mathlib_bitvalue( query_configflag(SVR_SPAWN_BIT2), SVR_SPAWN_BIT2)*2;
    start2 = start2 + mathlib_bitvalue( query_configflag(SVR_SPAWN_BIT3), SVR_SPAWN_BIT3)*4;
    if (start2 > 0) dprint("\b[CLIENT]\b Unique Info_Player_Start2 Active\n");
    
    // have a rune or new spawnflag option? Pick second spawn position
    if ( query_configflag(SVR_RUNE_ALL) || start2 > 0 ) {
        // Read any spawn location number (High/low bit flag, 0-3 values)
        spot = find (world, classname, "info_player_start2");
        if (spot) {
            spot2 = spot;
            while (spot) {
                if (spot.startspawn2 == start2) spot2 = spot;
                spot = find(spot, classname, "info_player_start2");
            }
            return spot2;
        }
    }
    
    // Last chance to find a spawn location before giving up!
    spot = find (world, classname, "info_player_start");
    if (!spot) dprint ("\b[CLIENT_SPAWN]\b missing info_player_start!");
    
    return spot;
};

float CMD_RESEND_ON_LOAD = 16;

void () RunCommandSpawn = {
    local entity cmd;
    cmd = find(cmd, classname, "info_command_spawn");
    if (cmd != world && cmd.spawnflags & CMD_RESEND_ON_LOAD) cmd.use();
};

//----------------------------------------------------------------------
// PutClientInServer - called each time a player is spawned
//----------------------------------------------------------------------
void() PutClientInServer =
{
    local    entity spot;

    client_ent = self;
    self.classname = "player";
    self.classtype = CT_PLAYER;
    self.classgroup = CG_PLAYERS;
    // This entity can never be switched off
    self.estate = ESTATE_ON;
    self.estate_off = SUB_Null;

    // Setup entity chains for coop, dm and intermission
    SetupSpawnCoopLocations();
    SetupIntermissionCamera();
    
    // Find a spawn location in the map
    spot = SelectSpawnPoint ();
    // Fire any targets on spawn location
    if (spot.target) self.target2 = spot.target;
    else self.target2 = "";

    self.headmdl = MODEL_PLAYER_HEAD;
    self.health = self.max_health = HEAL_PLAYMAX;
    self.takedamage = DAMAGE_AIM;
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_WALK;
    self.classmove = MON_MOVEWALK;
    self.show_hostile = 0;
    self.gibhealth = -40;
    self.gibbed = FALSE;
    self.flags = FL_CLIENT;
    self.air_finished = time + WATER_AIR;
    self.dmg = WATER_DAMAGE;
    self.effects = 0;
    self.steplast = 1;

    // Drake variables
    self.mist = world;
    self.hook = world; // for grapple service
    self.mirv1 = world;
    
    self.super_damage_finished = self.super_time = 0;
    self.radsuit_finished = self.rad_time = 0;
    self.invisible_finished = self.invisible_time = 0;
    self.invincible_finished = self.invincible_time = 0;
    
    self.sharpshoot_finished = self.sharpshoot_time = 0;
    self.nailpiercer_finished = self.nailpiercer_time = 0;
    self.wetsuit_finished = self.wetsuit_time = 0;
    self.empathy_finished = self.empathy_time = 0;
    
    self.antigrav_finished = self.antigrav_time = 0;
    self.shield_finished = self.shield_time = 0;

    self.cross_finished = self.cross_time = 0;
    self.trinity_finished = self.trinity_time = 0;

    self.wing_finished = self.wing_time = 0;

    self.steptype = FS_TYPEMEDIUM;        // Standard boots
    self.suppressCenterPrint = FALSE;    // No centerprint active

    ResetDebuffSystem(self);            // Reset/initialize debuff system
    
    self.enemy = world;
    DecodeLevelParms ();
    if (globalstate > 0)
        sprint (self,"Global states are set!\n");
    else
        sprint (self,"No global states are set.\n");
    self.weapon = W_BestWeapon (self);    // Pick best weapon from inventory
    W_SetCurrentAmmo (self);            // Update hud icons and v_model

    self.attack_finished = time;
    self.th_pain = player_pain;
    self.th_die = player_death;
    
    self.deadflag = DEAD_NO;            // The player is alive!
    self.pausetime = 0;                    // make sure player has 0 velocity when spawning

    update_hud_totals(HUD_MONSTERS);    // Make screen total is correct
    update_hud_totals(HUD_SECRETS);        // Update monsters and secrets
    
    //    spot = SelectSpawnPoint ();
    self.origin = spot.origin + '0 0 1';
    if (spot.mangle) self.angles = spot.mangle;
    else self.angles = spot.angles;
    self.v_angle = self.angles;
    self.fixangle = TRUE;                // turn this way immediately

    // oh, this is a hack!
    setmodel (self, MODEL_PLAYER_EYES);
    modelindex_eyes = self.modelindex;

       setmodel (self,"progs/hipnotic/playham.mdl");
       modelindex_hammer = self.modelindex;

    setmodel (self, MODEL_PLAYER);
    modelindex_player = self.modelindex;

    setmodel (self, "progs/drake/playham0.mdl");
    mindex_hamout = self.modelindex;

    setmodel (self, "progs/drake/playsaw.mdl");
    mindex_saw = self.modelindex;

    self.bbmins = VEC_HULLSHORT_MIN;    // -16 -16 -24, 16 16 32
    self.bbmaxs = VEC_HULLSHORT_MAX;
    setsize (self, self.bbmins, self.bbmaxs);
    self.view_ofs = '0 0 22';
    player_stand1 ();

    if (deathmatch || coop) {
        makevectors(self.angles);
        spawn_tfog (self.origin + v_forward*20);
    }

    spawn_tdeath (self.origin, self);

    RunCommandSpawn();
};

// Forward compiler links
void() PlayerJump;
void() WaterMove;
void() CheckWaterJump;
void() ClientDeath;

//----------------------------------------------------------------------
// PlayerPreThink - Called every frame before physics are run
//----------------------------------------------------------------------
void() PlayerPreThink =
{
    // Once the map has loaded (prethink) set loadflag (never saved)
    if (!prethink) {
        prethink = TRUE;            // prethink flag complete
        postthink = FALSE;            // postthink flag reset
        sync_configflag();            // Setup and sync serverflag
        // Some clients load faster, prevent player gasping for air
        self.air_finished = time + WATER_AIR;
        
        // Double check which engine is active?
        // A DP/FTE Quickload could be happening and the
        // Particle system needs to cope with the change
        ext_active = cvar("pr_checkextension");
        if (!ext_active) {
            // Revert to default = Fitz engine setup
            engine = ENG_FITZ;
            ext_dppart = ext_dpfog = ext_dpsurf = FALSE;
            ext_dprain = ext_dpsnow = FALSE;
        }
        
        // Check for trigger CD track/Skybox change
        // Load/Quickload does not keep track of these changes
        // Use a new global variable instead
        if (trig_cdtrack > 0) trigger_cdtrack_change();
        if (trig_skybox != "") trigger_skybox_change(self);
    }
            
    // If Intermission system active check for button0-2 keys
    if (intermission_running > 0) { CycleIntermissionCamera(); return; }

    if (earthquake_active)
    {
        if ( self.flags & FL_ONGROUND )
        {
            self.velocity_x = self.velocity_x + 
                                (random() * earthquake_intensity * 2) -
                                earthquake_intensity;
            self.velocity_y = self.velocity_y + 
                                (random() * earthquake_intensity * 2) -
                                earthquake_intensity;
            self.velocity_z = self.velocity_z + 
                                (random() * earthquake_intensity * 2) -
                                earthquake_intensity;
        }
    }

    makevectors (self.v_angle);    
    CheckRules ();
    WaterMove ();

    if (self.movetype == MOVETYPE_FLY) {
        self.oldorigin = self.origin;
        if (vlen(self.velocity) >= 70) {
            self.waterlevel = 3;    // Swim mode gives player full 3D control.
            self.flags = self.flags - (self.flags & FL_ONGROUND); // If onground, moving will cause the pitch to center automatically if mouselook is off, so remove the flag while the player swims in mid-air.
            if (self.h2olevel < 2) self.velocity = self.velocity * 1.429;    // This negates the underwater speed penalty.  Don't forget to undo the changes made here in PlayerPostThink.  Inspired by Hip's wetsuit code. But only in the air. 10/7 or 1.428571...
        } else {
            self.waterlevel = 0;    // So player doesn't sink while not moving.
            self.flags = self.flags | FL_ONGROUND; // While not in swim mode, the onground flag gives the player full control in the air, so make sure the flag is on.
        }
        self.flags = self.flags - (self.flags & FL_JUMPRELEASED); // Since the player is flying, make sure the jumpreleased flag is off at all times so when the player loses flight, he can't jump in mid-air in case the onground flag is on.
        // A 'groundentity' is the entity the player stands on.  If the groundentity
        // is not the world, the player is assumed to be standing on something that
        // may move, such as an elevator, a train, trap door. etc.  If groundentity
        // moves, the player tags along for the ride.  The problem comes in if a
        // player, flying in mid-air, has a non-world, moving groundentity.  This
        // lets the groundentity pull the player along when it shouldn't.  To fix
        // this, always set the groundentity to the world, which can never move.
        self.groundentity = world;

        // The jump button works a bit differently while in flymode.
        if (self.button2) {
            // Rise if in water.  Otherwise, slam on the brakes.
            if (self.h2olevel) {
                // WaterRise
                if (self.h2otype == CONTENT_WATER)
                    self.velocity_z = 100;
                else if (self.h2otype == CONTENT_SLIME)
                    self.velocity_z = 80;
                else
                    self.velocity_z = 50;

                // Play swimming sound
                if (self.swim_flag < time) {
                    self.swim_flag = time + 1;
                    if (random() < 0.5) sound (self, CHAN_BODY, "misc/water1.wav", 1, ATTN_NORM);
                    else sound (self, CHAN_BODY, "misc/water2.wav", 1, ATTN_NORM);
                }
                // end WaterRise
                if (self.h2olevel < 2)
                    self.velocity_z = self.velocity_z * 1.429;  // 10/7 or 1.428571...
            } else { self.velocity_z = 0; }
        }
    } else {
        // Not movetype_fly...normal movement
        if (self.waterlevel == 2) CheckWaterJump ();

        if (self.deadflag >= DEAD_DEAD) {
            ClientDeath ();
            return;
        }
        
        if (self.deadflag == DEAD_DYING) return;    // dying, so do nothing

        //----------------------------------------------------------------------
        // Rubicon Ladder Code - johnfitz
        // Modified by me to support diff climb sounds and on/off/toggle states
        //----------------------------------------------------------------------
        if (self.onladder) {
            self.onladder = 0;        // Reset ladder touch function
            if (self.button2) {        // Is jump key being pressed?
                // Reset velocity upwards and all sideways movement
                // so that the player stays on the ladder and climbs
                // straight up with very little sidways movement
                self.velocity = '0 0 0';
                self.velocity_z = self.entladder.speed;
                self.gravity = 0.0000001;

                if (self.timeladder < time) {
                    // Reset timer for next sound to play
                    self.timeladder = time + self.entladder.waitmin;
                    // Check for any sounds and query if footsteps are active
                    if (self.entladder.sounds != 4 && query_configflag(SVR_FOOTSTEP) == FALSE) {
                        self.lip = random();
                        if (self.lip < 0.25 && self.entladder.noise1) 
                            sound (self, CHAN_BODY, self.entladder.noise1, 1, ATTN_NORM);
                        else if (self.lip < 0.5 && self.entladder.noise2) 
                            sound (self, CHAN_BODY, self.entladder.noise2, 1, ATTN_NORM);
                        else if (self.lip < 0.75 && self.entladder.noise3) 
                            sound (self, CHAN_BODY, self.entladder.noise3, 1, ATTN_NORM);
                        else if (self.entladder.noise4) 
                            sound (self, CHAN_BODY, self.entladder.noise4, 1, ATTN_NORM);
                    }
                }
            }
            else {
                // Jump key released, stay floating on the ladder
                self.flags = self.flags | FL_JUMPRELEASED;
                self.velocity = 0.9 * self.velocity;
                self.velocity_z = 0;
            }
        } else {
            // Reset any ladder gravity settings
            if (self.antigrav_finished > time) {
                self.gravity = 0.25;
            } else {
                if (self.gravtrig == 1) {
                    self.gravity = self.gravtrigent.gravity;
                } else {
                    self.gravity = 1;
                }
            }

            // Reset gravtrig each frame requiring trigger to reset it so we don't get stuck in low gravity even after leaving the trigger
            self.gravtrig = 0;

            // Original jump conditions
            if (self.button2) {
                PlayerJump ();
            } else {
                if (self.wing_finished)
                    self.gravity = fabs(self.gravity);

                self.flags = self.flags | FL_JUMPRELEASED;
            }
        }
    }
        
    // When the player/client starts in the map they don't want any velocity    
    if (time < self.pausetime) self.velocity = '0 0 0';
};

void() PlayerFly_PostThink = {
    // PM:  If compiling for DarkPlaces, comment the code below out so that
    // nothing is done.
    if (self.waterlevel)
        if (self.h2olevel < 2)
            self.velocity = self.velocity * 0.7;
};
    
//----------------------------------------------------------------------
// PlayerPostThink - Called every frame after physics are run
//----------------------------------------------------------------------
void() PlayerPostThink =
{
    if (prethink && !postthink) {
        // Check for the player? 
        // - first couple of frames the player is not setup correctly
        if (self.flags & FL_CLIENT) {
            postthink = TRUE;            // postthink flag complete
            mapvar_cvar = TRUE;            // Map Variable are live
            // Reset any screen cshift parameters, eng often leaves them
            if (!self.cshift_upd) self.cshift_time = -1;
            
            // Check worldspawn water alpha parameters?
            // liquid alpha is used by the monster visible system
            if (world.water_alpha) {
                liquid_alpha = world.water_alpha;
                stuffcmd(self, "\nr_wateralpha ");
                lftos(self, liquid_alpha ,1,3, BUFFER_STUFFCMD);
                stuffcmd(self, "\n");
                dprint("\b[CLIENT]\b Water Alpha (");
                lftos(self, liquid_alpha ,1,3, BUFFER_DPRINT);
                dprint(")\n");
                
            }
            else liquid_alpha = cvar("r_wateralpha");    

            // Check engine for actual autoaim setting
            // Always reset with newgame and quickload
            autoaim_cvar = cvar("sv_aim");

            // Setup/change global fog if enabled (default)
            if (!(query_configflag(SVR_NOFOGCMDS))) {
                // Create fog controller and/or update engine fog
                if (fog_active) change_fog(self);
                else setup_fog_controller();
            }

            // Update console with MOD settings
            display_configflag();
            display_version();
                if (globalstate > 0)
        sprint (self,"Global states are set!\n");
    else
        sprint (self,"No global states are set.\n");
        }
    }
    if (intermission_running > 0) return;    // intermission or finale
    if (self.deadflag) return;                // Dead

    // Any spawn targets to update?
    if (self.target2 != "") { 
        trigger_strs(self.target2, self); self.target2 = "";
    }
    
    if (self.movetype == MOVETYPE_FLY) PlayerFly_PostThink ();  // flight think if we have boots of levitation

    W_WeaponFrame ();                        // Impulse commands and Quad updates

    // check to see if player landed and play landing sound    
    if ((self.jump_flag < -300) && (self.flags & FL_ONGROUND) && (self.health > 0)) {
        if (self.watertype == CONTENT_WATER)
            sound (self, CHAN_BODY, "player/h2ojump.wav", 1, ATTN_NORM);
        else if (self.jump_flag < -650) {
            T_Damage (self, world, world, 5, DAMARMOR); 
            sound (self, CHAN_VOICE, "player/land2.wav", 1, ATTN_NORM);
            self.deathtype = "falling";
        }
        else
            sound (self, CHAN_VOICE, "player/land.wav", 1, ATTN_NORM);

        self.jump_flag = 0;
    }

    // Check for any fake water exit sounds
    if (self.touchedliquid < time && self.touchedsound != "") {
        sound (self, CHAN_BODY, self.touchedsound, 1, ATTN_NORM);
        self.touchedsound = "";
    }

    // If flying through the air, store player Z velocity in jump flag
    if (!(self.flags & FL_ONGROUND)) self.jump_flag = self.velocity_z;
    ClientPowerups ();
    ClientDeBuff ();

// Update 7/24/09:  Added grappler weapon from DoE.
    if (self.hook)
        GrappleService ();
};

//----------------------------------------------------------------------
void() PlayerJump =
{
    if (self.flags & FL_WATERJUMP) return;
    
    if (self.waterlevel >= 2) {
        if (self.watertype == CONTENT_WATER) self.velocity_z = 100;
        else if (self.watertype == CONTENT_SLIME) self.velocity_z = 80;
        else self.velocity_z = 50;

        // play swiming sound
        if (self.swim_flag < time) {
            self.swim_flag = time + 1;
            if (random() < 0.5) sound (self, CHAN_BODY, "misc/water1.wav", 1, ATTN_NORM);
            else sound (self, CHAN_BODY, "misc/water2.wav", 1, ATTN_NORM);
        }
        return;
    }

    // Levitation - - - - - - - - - - - - -
    if (self.wing_finished) {
        if (self.wing_sound <= time) {
            //if (powcache & PCH_WING_USE)
            sound (self, CHAN_AUTO, "items/levitate.wav", 1, ATTN_NORM);
            self.wing_sound = time + 1;
        }
        // Gravity reversal was the simplest method to implement.
        // Using fancy math to get reverse standard gravity can cause problems
        // if the player gets hit with other effects that change gravity field.
        self.gravity = 0 - fabs(self.gravity);
        
        return;
    }
    //- - - - - - - - - - - - - - - - - - -

    if (!(self.flags & FL_ONGROUND)) return;
    if ( !(self.flags & FL_JUMPRELEASED) ) return;        // don't pogo stick

    self.flags = self.flags - (self.flags & FL_JUMPRELEASED);
    self.flags = self.flags - FL_ONGROUND;    // don't stairwalk
    
    self.button2 = 0;
    // player jumping sound
    sound (self, CHAN_BODY, "player/plyrjmp8.wav", 1, ATTN_NORM);
    self.velocity_z = self.velocity_z + 270;
};

//----------------------------------------------------------------------
// Deals with water, slime and lava
//----------------------------------------------------------------------
void() WaterMove =
{
    //dprint (ftos(self.waterlevel));
    // Fix noclip water gasping bug by giving air to the player
    if (self.movetype == MOVETYPE_NOCLIP) {
        self.air_finished = time + 1; 
        return;
    }
    if (self.health < 0) return;

    if (self.waterlevel != 3) {
        if (self.air_finished < time)
            sound (self, CHAN_VOICE, "player/gasp2.wav", 1, ATTN_NORM);
        else if (self.air_finished < time + 9)
            sound (self, CHAN_VOICE, "player/gasp1.wav", 1, ATTN_NORM);
        self.air_finished = time + 12;
        self.dmg = 2;
    }
    else if (self.air_finished < time) {    // drown!
        if (self.pain_finished < time) {
            self.dmg = self.dmg + 2;
            if (self.dmg > 15) self.dmg = 10;
            T_Damage (self, world, world, self.dmg, DAMARMOR);
            self.pain_finished = time + 1;
        }
    }
    
    if (!self.waterlevel) {
        if (self.flags & FL_INWATER) {    
            // play leave water sound
            sound (self, CHAN_BODY, "misc/outwater.wav", 1, ATTN_NORM);
            self.flags = self.flags - FL_INWATER;
        }
        return;
    }

    if (self.watertype == CONTENT_LAVA) {    // do damage
        if (self.dmgtime < time && self.therm_finished < time) {
            if (self.radsuit_finished > time) self.dmgtime = time + 1;
            else self.dmgtime = time + 0.2;
            T_Damage (self, world, world, 10*self.waterlevel, DAMARMOR);
        }
    }
    else if (self.watertype == CONTENT_SLIME) {    // do damage
        if (self.dmgtime < time && self.radsuit_finished < time && self.therm_finished < time) {
            self.dmgtime = time + 1;
            T_Damage (self, world, world, 4*self.waterlevel, DAMARMOR);
        }
    }
    
    if ( !(self.flags & FL_INWATER) ) {     // player enter water sound
        if (self.watertype == CONTENT_LAVA)
            sound (self, CHAN_BODY, "player/inlava.wav", 1, ATTN_NORM);
        if (self.watertype == CONTENT_WATER)
            sound (self, CHAN_BODY, "player/inh2o.wav", 1, ATTN_NORM);
        if (self.watertype == CONTENT_SLIME)
            sound (self, CHAN_BODY, "player/slimbrn2.wav", 1, ATTN_NORM);

        self.flags = self.flags + FL_INWATER;
        self.dmgtime = 0;
    }
    
    if (! (self.flags & FL_WATERJUMP) )
        self.velocity = self.velocity - 0.8*self.waterlevel*frametime*self.velocity;
};

//----------------------------------------------------------------------
void() CheckWaterJump =
{
    local vector start, end;

    // check for a jump-out-of-water
    makevectors (self.angles);
    start = self.origin;
    start_z = start_z + 8; 
    v_forward_z = 0;
    normalize(v_forward);
    end = start + v_forward*24;
    traceline (start, end, TRUE, self);
    
    if (trace_fraction < 1) {    // solid at waist
        start_z = start_z + self.maxs_z - 8;
        end = start + v_forward*24;
        self.movedir = trace_plane_normal * -50;
        traceline (start, end, TRUE, self);
        if (trace_fraction == 1) {    // open at eye level
            self.flags = self.flags | FL_WATERJUMP;
            self.velocity_z = 225;
            self.flags = self.flags - (self.flags & FL_JUMPRELEASED);
            self.teleport_time = time + 2;    // safety net
            return;
        }
    }
};

/*======================================================================
 CLIENT GAME EDGE FUNCTIONS
======================================================================*/
// This function cannot be move, ref an exact frame in players model
void() set_suicide_frame;        // In player.qc

//----------------------------------------------------------------------
void() ClientRespawn =
{
    if (coop) {
        CopyToBodyQue (self);    // make a copy of the dead body for appearances sake
        setspawnparms (self);    // get the spawn parms as they were at level start
        PutClientInServer ();    // respawn
    }
    else if (deathmatch) {
        CopyToBodyQue (self);    // make a copy of the dead body for appearances sake
        SetNewParms ();            // set default spawn parms
        PutClientInServer ();    // respawn
    }
    // restart the entire server
    else localcmd ("restart\n");
};

//----------------------------------------------------------------------
void() ClientDeath =
{
    local float forward;

    if ((self.flags & FL_ONGROUND)) {
        forward = vlen (self.velocity);
        forward = forward - 20;
        if (forward <= 0)
            self.velocity = '0 0 0';
        else    
            self.velocity = forward * normalize(self.velocity);
    }

    // wait for all buttons released
    if (self.deadflag == DEAD_DEAD) {
        if (self.button2 || self.button1 || self.button0) return;
        self.deadflag = DEAD_RESPAWNABLE;
        return;
    }

    // wait for any button down
    if (!self.button2 && !self.button1 && !self.button0) return;
    self.button0 = self.button1 = self.button2 = 0;
    ClientRespawn();
};

//----------------------------------------------------------------------
// Player entered the suicide command
//----------------------------------------------------------------------
void() ClientKill =
{
    bprint (self.netname);
    bprint (" suicides\n");
    set_suicide_frame ();
    self.modelindex = modelindex_player;
    self.frags = self.frags - 2;    // extra penalty
    ClientRespawn ();
};

//----------------------------------------------------------------------
// Called when a player connects to a server
//----------------------------------------------------------------------
void() ClientConnect =
{
    bprint (self.netname);
    bprint (" entered the game\n");
    
    // a client connecting during an intermission can cause problems
    if (intermission_running > 0) GotoNextMap ();
};

//----------------------------------------------------------------------
// Called when a player disconnects from a server
//----------------------------------------------------------------------
void() ClientDisconnect =
{
    if (gameover) return;
    // if the level end trigger has been activated, just return
    // since they aren't *really* leaving

    // let everyone else know
    bprint (self.netname);
    bprint (" left the game with ");
    bprint (ftos(self.frags));
    bprint (" frags\n");
    sound (self, CHAN_BODY, "player/tornoff2.wav", 1, ATTN_NONE);
    set_suicide_frame ();
};