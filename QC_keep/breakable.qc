/*======================================================================
 Breakable functions
======================================================================*/
float BREAK_STARTOFF = 1;        // Will wait for trigger to spawn
float BREAK_NOSHOOT = 2;        // Cannot be damaged/shot, trigger only
float BREAK_EXPLOSION = 4;        // Spawn sprite/particle explosion
float BREAK_SILENT = 8;            // No initial break sound
float BREAK_DAMAGE = 16;        // Rubble does damage on touch
float BREAK_NOMONSTER = 32;        // No damage to monsters from rubble
float BREAK_NOSOUND = 64;        // No impact sound for rubble
float BREAK_NOROTATE = 128;        // No Y rotation for rubble
float BREAK_MOVEDIR = 4096;        // Set by entity, movedir/angles active
float BREAK_USEMAP = 8192;        // Use func_breakable_template's from the map

float MONTRIG_NODELAY = 1;        // No delay between monster and breakable trigger
float MONTRIG_WAKEUPANIM = 2;    // Monster trigger does special wakeup animation

float BREAKWALL_START_ON = 1;    // Switchable bmodel for breakable setups
float BREAKWALL_SOLID = 2;        // Solid player collision when active
float BREAKWALL_FADEOUT = 4;    // Will fade out after a certain amount of time

float BTYPE_ROCK = 1;            // Default rock/brick
float BTYPE_WOOD = 2;
float BTYPE_GLASS = 3;
float BTYPE_METAL = 4;
float BTYPE_BRICK = 5;
float BTYPE_CERAMIC = 6;
float BTYPE_MAX = 6;
float BTYPE_CUSTOM = 10;

float BMODTYPE_SELF = 1;        // Single self models
float BMODTYPE_CUSTOM = 5;        // Custom models
float BMODTYPE_ROCK1 = 10;        // ID rock4_1 (dark large bricks)
float BMODTYPE_ROCK2 = 11;        // ID rock4_2 (light stone)
float BMODTYPE_ROCK3 = 12;        // ID rock3_8 (light vertical)
float BMODTYPE_ROCK4 = 13;        // ID city5_3 (white plaster)
float BMODTYPE_ROCK5 = 14;        // ID stone1_7b (dark blue ver)
float BMODTYPE_ROCK6 = 15;        // ID cliff4 (black rock)
float BMODTYPE_WOOD1 = 20;        // ID dung01_3 (dark)
float BMODTYPE_WOOD2 = 21;        // ID dung01_2 (light)
float BMODTYPE_WOOD3 = 22;        // ID wizwood1_7 (mouldy)
float BMODTYPE_GLASS1 = 30;        // ID window1_2 (blue sqr)
float BMODTYPE_GLASS2 = 31;        // ID window01_4 (red stain)
float BMODTYPE_GLASS3 = 32;        // ID window02_1 (yellow stain)
float BMODTYPE_GLASS4 = 33;        // ID window01_3 (purple stain)
float BMODTYPE_METAL1 = 40;        // ID metal1_2 (brown generic)
float BMODTYPE_METAL2 = 41;        // ID metal4_5 (metal4_4 generic)
float BMODTYPE_METAL3 = 42;        // ID metal4_7 (rivet metal panels)
float BMODTYPE_METAL4 = 43;        // ID cop1_1 (green generic)
float BMODTYPE_METAL5 = 44;        // ID metal2_8 (blue generic)
float BMODTYPE_BRICK1 = 50;        // ID wbrick1_5 (large brown)
float BMODTYPE_BRICK2 = 51;        // ID city2_3 (small sewer green)
float BMODTYPE_BRICK3 = 52;        // ID city6_8 (small drywall greyish)
float BMODTYPE_BRICK4 = 53;        // ID wiz1_4 (large white)
float BMODTYPE_BRICK5 = 54;        // ID city2_1 (small red brick)
float BMODTYPE_BRICK6 = 55;        // ID city1_6 (small brown brick)
float BMODTYPE_BRICK7 = 56;        // ID city4_5 (small blue brick)
float BMODTYPE_CERAMIC1 = 60;    // Blank atm

//----------------------------------------------------------------------
// Breakable objects explosion/impact sounds
string SOUND_BRK_ROCK = "break/rock_impact.wav";
string SOUND_BRK_WOOD = "break/wood_impact.wav";
string SOUND_BRK_GLASS = "break/glass_impact.wav";
string SOUND_BRK_METAL = "break/metal_impact.wav";
string SOUND_BRK_CERAMIC = "break/ceramic_impact.wav";

string SOUND_IMP_ROCK1 = "break/rock_i1.wav";
string SOUND_IMP_ROCK2 = "break/rock_i2.wav";
string SOUND_IMP_ROCK3 = "break/rock_i3.wav";
string SOUND_IMP_ROCK4 = "break/rock_i4.wav";
string SOUND_IMP_WOOD1 = "break/wood_i1.wav";
string SOUND_IMP_WOOD2 = "break/wood_i2.wav";
string SOUND_IMP_WOOD3 = "break/wood_i3.wav";
string SOUND_IMP_WOOD4 = "break/wood_i4.wav";
string SOUND_IMP_GLASS1 = "break/glass_i1.wav";
string SOUND_IMP_GLASS2 = "break/glass_i2.wav";
string SOUND_IMP_GLASS3 = "break/glass_i3.wav";
string SOUND_IMP_GLASS4 = "break/glass_i4.wav";
string SOUND_IMP_METAL1 = "break/metal_i1.wav";
string SOUND_IMP_METAL2 = "break/metal_i2.wav";
string SOUND_IMP_METAL3 = "break/metal_i3.wav";
string SOUND_IMP_METAL4 = "break/metal_i4.wav";
string SOUND_IMP_CERAMIC1 = "break/ceramic_i1.wav";
string SOUND_IMP_CERAMIC2 = "break/ceramic_i2.wav";
string SOUND_IMP_CERAMIC3 = "break/ceramic_i3.wav";
string SOUND_IMP_CERAMIC4 = "break/ceramic_i4.wav";

//----------------------------------------------------------------------
// Breakable objects explosion/impact models
// ID rock4_1, rock4_2, rock3_8, city5_3, stone1_7b, cliff4
string MODEL_BRK_ROCK1A = "maps/ad_brk/rock01.bsp";
string MODEL_BRK_ROCK1B = "maps/ad_brk/rock02.bsp";
string MODEL_BRK_ROCK1C = "maps/ad_brk/rock03.bsp";
string MODEL_BRK_ROCK1D = "maps/ad_brk/rock04.bsp";
string MODEL_BRK_ROCK2A = "maps/ad_brk/rock05.bsp";
string MODEL_BRK_ROCK2B = "maps/ad_brk/rock06.bsp";
string MODEL_BRK_ROCK2C = "maps/ad_brk/rock07.bsp";
string MODEL_BRK_ROCK2D = "maps/ad_brk/rock08.bsp";
string MODEL_BRK_ROCK3A = "maps/ad_brk/rock09.bsp";
string MODEL_BRK_ROCK3B = "maps/ad_brk/rock10.bsp";
string MODEL_BRK_ROCK3C = "maps/ad_brk/rock11.bsp";
string MODEL_BRK_ROCK3D = "maps/ad_brk/rock12.bsp";
string MODEL_BRK_ROCK4A = "maps/ad_brk/rock13.bsp";
string MODEL_BRK_ROCK4B = "maps/ad_brk/rock14.bsp";
string MODEL_BRK_ROCK4C = "maps/ad_brk/rock15.bsp";
string MODEL_BRK_ROCK4D = "maps/ad_brk/rock16.bsp";
string MODEL_BRK_ROCK5A = "maps/ad_brk/rock17.bsp";
string MODEL_BRK_ROCK5B = "maps/ad_brk/rock18.bsp";
string MODEL_BRK_ROCK5C = "maps/ad_brk/rock19.bsp";
string MODEL_BRK_ROCK5D = "maps/ad_brk/rock20.bsp";
string MODEL_BRK_ROCK6A = "maps/ad_brk/rock21.bsp";
string MODEL_BRK_ROCK6B = "maps/ad_brk/rock22.bsp";
string MODEL_BRK_ROCK6C = "maps/ad_brk/rock23.bsp";
string MODEL_BRK_ROCK6D = "maps/ad_brk/rock24.bsp";
// ID dung01_3 (dark), dung01_2 (light), wizwood1_7 (mouldy)
string MODEL_BRK_WOOD1A = "maps/ad_brk/wood01.bsp";
string MODEL_BRK_WOOD1B = "maps/ad_brk/wood02.bsp";
string MODEL_BRK_WOOD1C = "maps/ad_brk/wood03.bsp";
string MODEL_BRK_WOOD1D = "maps/ad_brk/wood04.bsp";
string MODEL_BRK_WOOD2A = "maps/ad_brk/wood05.bsp";
string MODEL_BRK_WOOD2B = "maps/ad_brk/wood06.bsp";
string MODEL_BRK_WOOD2C = "maps/ad_brk/wood07.bsp";
string MODEL_BRK_WOOD2D = "maps/ad_brk/wood08.bsp";
string MODEL_BRK_WOOD3A = "maps/ad_brk/wood09.bsp";
string MODEL_BRK_WOOD3B = "maps/ad_brk/wood10.bsp";
string MODEL_BRK_WOOD3C = "maps/ad_brk/wood11.bsp";
string MODEL_BRK_WOOD3D = "maps/ad_brk/wood12.bsp";
// ID window1_2 (blue sqr), window01_4 (red stain), 
// window02_1 (yellow stain), window01_3 (purple dragon)
string MODEL_BRK_GLASS1A = "maps/ad_brk/glass01.bsp";
string MODEL_BRK_GLASS1B = "maps/ad_brk/glass02.bsp";
string MODEL_BRK_GLASS1C = "maps/ad_brk/glass03.bsp";
string MODEL_BRK_GLASS1D = "maps/ad_brk/glass04.bsp";
string MODEL_BRK_GLASS2A = "maps/ad_brk/glass05.bsp";
string MODEL_BRK_GLASS2B = "maps/ad_brk/glass06.bsp";
string MODEL_BRK_GLASS2C = "maps/ad_brk/glass07.bsp";
string MODEL_BRK_GLASS2D = "maps/ad_brk/glass08.bsp";
string MODEL_BRK_GLASS3A = "maps/ad_brk/glass09.bsp";
string MODEL_BRK_GLASS3B = "maps/ad_brk/glass10.bsp";
string MODEL_BRK_GLASS3C = "maps/ad_brk/glass11.bsp";
string MODEL_BRK_GLASS3D = "maps/ad_brk/glass12.bsp";
string MODEL_BRK_GLASS4A = "maps/ad_brk/glass13.bsp";
string MODEL_BRK_GLASS4B = "maps/ad_brk/glass14.bsp";
string MODEL_BRK_GLASS4C = "maps/ad_brk/glass15.bsp";
string MODEL_BRK_GLASS4D = "maps/ad_brk/glass16.bsp";
// ID metal1_2 (brown generic), metal4_5 (metal4_4 generic), 
// metal4_7 (rivet metal panels), cop1_1 (green generic)
string MODEL_BRK_METAL1A = "maps/ad_brk/metal01.bsp";
string MODEL_BRK_METAL1B = "maps/ad_brk/metal02.bsp";
string MODEL_BRK_METAL1C = "maps/ad_brk/metal03.bsp";
string MODEL_BRK_METAL1D = "maps/ad_brk/metal04.bsp";
string MODEL_BRK_METAL2A = "maps/ad_brk/metal05.bsp";
string MODEL_BRK_METAL2B = "maps/ad_brk/metal06.bsp";
string MODEL_BRK_METAL2C = "maps/ad_brk/metal07.bsp";
string MODEL_BRK_METAL2D = "maps/ad_brk/metal08.bsp";
string MODEL_BRK_METAL3A = "maps/ad_brk/metal09.bsp";
string MODEL_BRK_METAL3B = "maps/ad_brk/metal10.bsp";
string MODEL_BRK_METAL3C = "maps/ad_brk/metal11.bsp";
string MODEL_BRK_METAL3D = "maps/ad_brk/metal12.bsp";
string MODEL_BRK_METAL4A = "maps/ad_brk/metal13.bsp";
string MODEL_BRK_METAL4B = "maps/ad_brk/metal14.bsp";
string MODEL_BRK_METAL4C = "maps/ad_brk/metal15.bsp";
string MODEL_BRK_METAL4D = "maps/ad_brk/metal16.bsp";
string MODEL_BRK_METAL5A = "maps/ad_brk/metal17.bsp";
string MODEL_BRK_METAL5B = "maps/ad_brk/metal18.bsp";
string MODEL_BRK_METAL5C = "maps/ad_brk/metal19.bsp";
string MODEL_BRK_METAL5D = "maps/ad_brk/metal20.bsp";
// ID wbrick1_5 (large brown), city2_3 (small sewer green), 
// city6_8 (small drywall greyish), wiz1_4 (large white)
// city2_1 (small red brick), city1_6 (small brown)
string MODEL_BRK_BRICK1A = "maps/ad_brk/brick01.bsp";
string MODEL_BRK_BRICK1B = "maps/ad_brk/brick02.bsp";
string MODEL_BRK_BRICK1C = "maps/ad_brk/brick03.bsp";
string MODEL_BRK_BRICK1D = "maps/ad_brk/brick04.bsp";
string MODEL_BRK_BRICK2A = "maps/ad_brk/brick05.bsp";
string MODEL_BRK_BRICK2B = "maps/ad_brk/brick06.bsp";
string MODEL_BRK_BRICK2C = "maps/ad_brk/brick07.bsp";
string MODEL_BRK_BRICK2D = "maps/ad_brk/brick08.bsp";
string MODEL_BRK_BRICK3A = "maps/ad_brk/brick09.bsp";
string MODEL_BRK_BRICK3B = "maps/ad_brk/brick10.bsp";
string MODEL_BRK_BRICK3C = "maps/ad_brk/brick11.bsp";
string MODEL_BRK_BRICK3D = "maps/ad_brk/brick12.bsp";
string MODEL_BRK_BRICK4A = "maps/ad_brk/brick13.bsp";
string MODEL_BRK_BRICK4B = "maps/ad_brk/brick14.bsp";
string MODEL_BRK_BRICK4C = "maps/ad_brk/brick15.bsp";
string MODEL_BRK_BRICK4D = "maps/ad_brk/brick16.bsp";
string MODEL_BRK_BRICK5A = "maps/ad_brk/brick17.bsp";
string MODEL_BRK_BRICK5B = "maps/ad_brk/brick18.bsp";
string MODEL_BRK_BRICK5C = "maps/ad_brk/brick19.bsp";
string MODEL_BRK_BRICK5D = "maps/ad_brk/brick20.bsp";
string MODEL_BRK_BRICK6A = "maps/ad_brk/brick21.bsp";
string MODEL_BRK_BRICK6B = "maps/ad_brk/brick22.bsp";
string MODEL_BRK_BRICK6C = "maps/ad_brk/brick23.bsp";
string MODEL_BRK_BRICK6D = "maps/ad_brk/brick24.bsp";
string MODEL_BRK_BRICK7A = "maps/ad_brk/brick25.bsp";
string MODEL_BRK_BRICK7B = "maps/ad_brk/brick26.bsp";
string MODEL_BRK_BRICK7C = "maps/ad_brk/brick27.bsp";
string MODEL_BRK_BRICK7D = "maps/ad_brk/brick28.bsp";

//----------------------------------------------------------------------
// Axe impact/swipe/miss sounds
string SOUND_AXE_SWIPE1 = "weapons/axe_swoosh1.wav";    // Fast swipe
string SOUND_AXE_SWIPE2 = "weapons/axe_swoosh2.wav";    // Faster swipe
string SOUND_AXE_WOOD = "weapons/axe_wood.wav";            // wood impact
string SOUND_AXE_GLASS = "weapons/axe_glass.wav";        // glass impact
string SOUND_AXE_METAL = "weapons/axe_metal.wav";        // metal impact
string SOUND_AXE_CERAMIC = "weapons/axe_ceramic.wav";    // ceramic impact
string SOUND_AXE_PLAYER = "player/axhit1.wav";            // ax hit meat (another player)
string SOUND_AXE_STONE = "player/axhit2.wav";            // stone impact

void() make_breakable_debris;

//=============================================================================
/*QUAKED func_breakable (0 .5 .8) ? STARTOFF NODAMAGE EXPLOSION SILENT DAMAGE NOMOSTER NOSOUND NOROTATE Not_Easy Not_Normal Not_Hard Not_DM
Spawn breakable objects from a bmodel
-------- KEYS --------
target     : targets to fire when breakable is dead/used (only used once)
target2    : Additional trigger function (need target to be defined as well)
style      : pre-defined sound/model types - 1=rock, 2=wood, 3=glass, 4=metal, 5=brick, 6=ceramic, 10=custom
brksound   : Initial breaking sound type (override style default)
brkimpsound : Impact sound type (override style default)
brkobjects : Breakable object model type (10-15=rocks, 20-22=woods, 30-32=glass, 40-42=metals, 50-54=brick, 60=ceramic)
noise     : Initial breaking sound (unique sound file)
noise1    : Custom Rubble Impact sounds (unique sound files, must have 1 defined)
noise2    : Custom Rubble Impact sound 2
noise3    : Custom Rubble Impact sound 3
noise4    : Custom Rubble Impact sound 4
brkobj1   : Custom Rubble bmodel objects (unique models, must have 1 defined)
brkobj2   : Custom Rubble bmodel objects 2
brkobj3   : Custom Rubble bmodel objects 3
brkobj4   : Custom Rubble bmodel objects 4
health    : amount of damage to take before breaking (def 1)
count     : minimum quantity to spawn (def 4)
cnt       : random quantity to spawn (def 4) =-1 no random qty
dmg       : explosive radius damage (emits from center of func object)
pos1      : x=start particle colour, y=random range, z=quantity
brkvelbase : Base amount for velocity of broken parts (def "50 50 100")
brkveladd : Random additions for velocity of broken parts (def "100 100 150")
brkavel   : Amount of breaking object angle velocity (def 200)
brkfade   : Fade time before rubble fades away (def 4+random()x4)
brkmondmg : Damage multiplier for monster damage against breakable
angles    : direction to throw rubble (override default = impact direction)
brktrigjump    : Trigger if damaged by jumping monster attack
brktrigmissile : Trigger if damaged by rocket/grenade/shalball/radiusdmg
brktrignoplayer: No player/clients can damage this breakable
brkgravity     : Change the gravity for rubble, useful for underwater (Value = 0-1)
-------- SPAWNFLAGS --------
STARTOFF  : Will wait for trigger to spawn
NODAMAGE  : Cannot be damaged or shot, trigger only
EXPLOSION : trigger sprite/particle explosion
SILENT    : No initial breakage sound
DAMAGE    : Spawning rubble can damage (def = 2, use dmg key for touch damage)
NOMONSTER : monsters cannot damage this breakable and/or spawning rubble will not damage monsters
NOSOUND   : Spawning rubble has no impact sounds
NOROTATE  : Spawning rubble has No Y rotation
-------- NOTES --------
Spawn breakable ojects from a bmodel
*/

//=============================================================================
/*QUAKED func_breakable_spawner (0.5 .5 .8) (-8 -8 -8) (8 8 8) x x EXPLOSION SILENT DAMAGE NOMOSTER NOSOUND NOROTATE Not_Easy Not_Normal Not_Hard Not_DM
Spawn breakable objects from a single point
-------- KEYS --------
target     : targets to fire when breakable is dead/used (only used once)
target2    : Additional trigger function (need target to be defined as well)
style      : pre-defined sound/model types - 1=rock, 2=wood, 3=glass, 4=metal, 5=brick, 6=ceramic, 10=custom
brksound   : Initial breaking sound type (override style default)
brkimpsound : Impact sound type (override style default)
brkobjects : Breakable object model type (10-15=rocks, 20-22=woods, 30-32=glass, 40-42=metals, 50-54=brick, 60=ceramic)
noise      : Initial breaking sound (unique sound file)
noise1    : Custom Rubble Impact sounds (unique sound files, must have 1 defined)
noise2    : Custom Rubble Impact sound 2
noise3    : Custom Rubble Impact sound 3
noise4    : Custom Rubble Impact sound 4
brkobj1   : Custom Rubble bmodel objects (unique models, must have 1 defined)
brkobj2   : Custom Rubble bmodel objects 2
brkobj3   : Custom Rubble bmodel objects 3
brkobj4   : Custom Rubble bmodel objects 4
health    : amount of damage to take before breaking (def 1)
count     : minimum quantity to spawn (def 4)
cnt       : random quantity to spawn (def 4) =-1 no random qty
dmg       : explosive radius damage (emits from center of func object)
pos1      : x=start particle colour, y=random range, z=quantity
brkvelbase : Base amount for velocity of broken parts (def "50 50 100")
brkveladd : Random additions for velocity of broken parts (def "100 100 150")
brkavel   : Amount of breaking object angle velocity (def 200)
brkfade   : Fade time before rubble fades away (def 4+random()x4)
angles    : direction to throw rubble (override default = impact direction)
brktrigjump    : Trigger if damaged by jumping monster attack
brktrigmissile : Trigger if damaged by rocket/grenade/shalball/radiusdmg
brkgravity     : Change the gravity for rubble, useful for underwater (Value = 0-1)
brkvol    : Spawning volume vector for breakable point entity
-------- SPAWNFLAGS --------
EXPLOSION : trigger sprite/particle explosion
SILENT    : No initial breakage sound
DAMAGE    : Spawning rubble can damage (def = 2, use dmg key for touch damage)
NOMONSTER : Spawning rubble will not damage monsters
NOSOUND   : Spawning rubble has no impact sounds
NOROTATE  : Spawning rubble has No Y rotation
-------- NOTES --------
Spawn breakable ojects from a single point
*/

//----------------------------------------------------------------------
void() breakable_remove =
{
    self.think = model_fade;
    self.nextthink = time + 0.1;
    self.ltime = self.nextthink;
};

//----------------------------------------------------------------------
void() breakable_checkfloor =
{
    // Is it time for the breakable to fade away?
    if (self.pain_finished > time) {
        // Check floor below breakable (global function)
        // Origin at base of model + 16 (typical step height)
        ent_floorcheck(self, FLOOR_TRACE_BREAK);

        // Keep checking
        self.think = breakable_checkfloor;
        self.nextthink = time + 0.1;
    }
    else breakable_remove();
};

//----------------------------------------------------------------------
void() breakable_particle =
{
    // Are particles disabled? has the model touched something?
    if (self.wait) {
        // Is there any time left to setup floor checks
        if (self.pain_finished > time + 0.1) {
            self.think = breakable_checkfloor;
        }
        else self.think = breakable_remove;
        self.nextthink = time + 0.1;
    }
    else {
        // Add a bit of randomness to the particles generated
        if (random() < 0.5) self.nextthink = time + 0.01;
        else self.nextthink = time + 0.02;
        particle (self.origin, self.oldorigin, rint(self.pos1_x + random()*self.pos1_y), self.pos1_z);
        // Check for removal timer
        if (self.pain_finished < time) self.think = breakable_remove;
    }
};

//----------------------------------------------------------------------
void() breakable_touch =
{
    if (check_skycontent(self.origin)) {entity_remove(self, 0.1); return;}
    if (self.touchedvoid) { entity_remove(self,0.1); return; }
    if (other.classtype == CT_FUNCBREAKOBJ) return;    // Ignore other breakables
    if (other.solid == SOLID_TRIGGER) return;        // Ignore trigger fields
    self.wait = TRUE;    // No more particles

    if (self.count < 1) {
        self.touch = SUB_Null;                // no more touching
        self.solid = SOLID_NOT;                // Turn off world interaction
        self.avelocity = '0 0 0';            // Stop any velocity rotation
    }
    else {
        // Does the spawning rubble hurt?
        if (self.owner.spawnflags & BREAK_DAMAGE) {
            if (other.takedamage) {
                if ( other.flags & FL_MONSTER && self.owner.spawnflags & BREAK_NOMONSTER ) {}
                else T_Damage(other, self, self, self.owner.dmg, DAMARMOR);
            }
        }
        self.count = self.count - 1;
        if (!(self.owner.spawnflags & BREAK_NOSOUND)) {
            // Randomize impact sound
            self.lip = random()*self.owner.brkimpqty;
            if (self.lip < 1) sound(self, CHAN_VOICE, self.owner.noise1, 1, ATTN_BREAK);
            else if (self.lip < 2) sound(self, CHAN_VOICE, self.owner.noise2, 1, ATTN_BREAK);
            else if (self.lip < 3) sound(self, CHAN_VOICE, self.owner.noise3, 1, ATTN_BREAK);
            else sound(self, CHAN_VOICE, self.owner.noise4, 1, ATTN_BREAK);
        }
    }
};

//----------------------------------------------------------------------
// Can't have multiple radius explosion on same frame
// Always delay them to prevent endless loops
//----------------------------------------------------------------------
void() funcbreakable_delayexplode =
{
    T_RadiusDamage (self.attachment, self.attachment, self.dmg, self.attachment, DAMAGEALL);
};

//======================================================================
// Main function for generating rubble
//======================================================================
void() funcbreakable_use =
{
    local vector dirvec, rorg, gvel, dirorg;
    local float content_loop, content_flag;

    if (self.waitmin == TRUE) return;    // Trigger once block

    //----------------------------------------------------------------------
    // Play initial breaking sound (exception - silent spawnflag)
    if (!(self.spawnflags & BREAK_SILENT)) 
        sound (self.attachment, CHAN_BODY, self.noise, 1, ATTN_NORM);

    //----------------------------------------------------------------------
    // Hide breakable func model (ignore if point entity)
    if (self.classtype == CT_FUNCBREAK) {
        self.waitmin = TRUE;            // Only fire breakable ONCE
        self.use = SUB_Null;            // No more triggers
        self.takedamage = DAMAGE_NO;    // No more pain/damage
        self.model = "";                // hide model
        self.solid = SOLID_NOT;            // no world interaction
        self.movetype = MOVETYPE_NONE;
        // Work out bottom corner of min/max bounding box
        self.oldorigin = self.attachment.origin - (self.size*0.5);
    }
    else if (self.classtype == CT_FUNCBREAKMDL) {
        self.waitmin = TRUE;            // Only fire breakable ONCE
        self.use = SUB_Null;            // No more triggers
        self.takedamage = DAMAGE_NO;    // No more pain/damage
        self.model = "";                // hide model
        self.solid = SOLID_NOT;            // no world interaction
        self.movetype = MOVETYPE_NONE;
    }
    // Breakable point entities just keep on spawning rubble
    else self.oldorigin = self.origin;
        
    //----------------------------------------------------------------------
    // Fire all targets (usually the target is the broken remains)
    // Only fire this target once and check based on target field only
    // Targets are fired before rubble so pointcontent check can
    // take into account any ruined sections and not spawn inside brushwork
    if (self.target != "") {
        // Check death entity (set in Killed in ai_combat.qc)
        if (self.activate.flags & FL_CLIENT) activator = self.activate;
        SUB_UseTargets();
    }
    // Only fire targets once, remove any further triggering
    clear_trigstrs(self);

    //----------------------------------------------------------------------
    // By default all breakables use direction force for rubble
    // If angles (movedir) set on breakable use that instead
    // Work out direction of impact (using activate passed from combat.qc or trigger.qc)
    // All bmodels are labelled with .bsporigin flag (origin = 0,0,0)
    // attachment points to self if funcbreakable_wall active
    if (self.spawnflags & BREAK_MOVEDIR) dirvec = self.movedir;
    else if (!self.activate) dirvec = '0 0 1';
    else {
        if (self.activate.bsporigin) dirorg = bmodel_origin(self.activate);
        else dirorg = self.activate.origin;
        dirvec = vectoangles(self.attachment.origin - dirorg);
    }
    
    // spawn custom templates instead of debris models
    if (self.spawnflags & BREAK_USEMAP) {
        make_breakable_debris ();
        return;
    }
    //----------------------------------------------------------------------
    // Main rubble loop
    while(self.count > 0 && self.brkobjqty > 0) {
        //----------------------------------------------------------------------
        // Work out position inside bounding box of breakable bmodel
        // Just in case no space to spawn anywhere, dont do an infinite loop
        content_loop = 4;
        
        while (content_loop > 0) {
        // Setup broken object inside area (min/max) of parent breakable object
            if (self.classtype == CT_FUNCBREAK) {
                rorg_x = self.oldorigin_x + random()*self.size_x;
                rorg_y = self.oldorigin_y + random()*self.size_y;
                rorg_z = self.oldorigin_z + random()*self.size_z;
            }
            else {
                // trigger version is a single origin point with slight wobble
                rorg_x = self.origin_x + crandom()*self.brkvol_x;
                rorg_y = self.origin_y + crandom()*self.brkvol_y;
                rorg_z = self.origin_z + crandom()*self.brkvol_z;
            }
            // Check point content and keep checking (limited loop cycles)
            content_flag = pointcontents(rorg);
            if (content_flag != CONTENT_SOLID && content_flag != CONTENT_SKY) content_loop = -1;
            else content_loop = content_loop - 1;
        }

        //----------------------------------------------------------------------
        // Was there any space to spawn rubble?
        if (content_loop < 0) {
            // Create new piece of rubble
            newmis = spawn();
            newmis.classname = "rubble";
            newmis.classtype = CT_FUNCBREAKOBJ;
            newmis.classgroup = CG_TEMPENT;
            newmis.owner = self;
            
            // Setup broken model/bsp (random selection if possible)
            self.lip = random()*self.brkobjqty;
            if (self.lip < 1) setmodel (newmis, self.brkobj1);
            else if (self.lip < 2) setmodel (newmis, self.brkobj2);
            else if (self.lip < 3) setmodel (newmis, self.brkobj3);
            else setmodel (newmis, self.brkobj4);
    
            // Setup origin based on previous content checks
            setorigin (newmis, rorg);    
            setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
    
            //----------------------------------------------------------------------
            // Setup movement and spin parameters
            newmis.movetype = MOVETYPE_BOUNCE;
            newmis.solid = SOLID_BBOX;
            if (!(self.spawnflags & BREAK_NOROTATE)) newmis.angles_y = random() * 360;
            newmis.avelocity = vecrand(0,self.brkavel,TRUE);
            
            //----------------------------------------------------------------------
            if (self.spawnflags & BREAK_MOVEDIR && self.movedir_y < 0) {
                // Up/down direction with slight X/Y wobble
                newmis.velocity_x = crandom() * (self.brkveladd_x/2);
                newmis.velocity_y = crandom() * (self.brkveladd_y/2);
                if (self.movedir_y == -2) newmis.velocity_z = 0 - random()*50;
                else newmis.velocity_z = self.brkvelbase_z + (random()*self.brkveladd_z);
            }
            else {
                // Directional velocity based on angle_y or activator entity
                makevectors(dirvec);
                gvel = v_forward * (self.brkvelbase_x + random() * self.brkveladd_x);
                gvel = gvel + v_right * (crandom() * self.brkveladd_y);
                gvel = gvel + v_up * (self.brkvelbase_z + random() * self.brkveladd_z);
                newmis.velocity = gvel;
            }
            //----------------------------------------------------------------------
            // How many bounce sounds - randomly pick 1-2
            if (random() < 0.2) newmis.count = 2;
            else newmis.count = 1;
        
            //----------------------------------------------------------------------
            // Touch and eventual fade functions        
            newmis.touch = breakable_touch;
            newmis.pain_finished = time + self.brkfade + random()*self.brkfade;
            newmis.nextthink = time + 0.01;
            newmis.think = breakable_particle;
    
            //----------------------------------------------------------------------
            // Setup particle colour and particle dirction (forward)
            newmis.pos1 = self.pos1;
            gvel = vectoangles(newmis.velocity);
            makevectors(gvel);
            newmis.oldorigin = v_forward;
            if (self.brkgravity) newmis.gravity = self.brkgravity;            
        }
        
        //----------------------------------------------------------------------
        // Keep on spawning rubble!
        self.count = self.count - 1;
    }
    
    //----------------------------------------------------------------------
    // breakable trigger entities can spawn debris multiple times
    if (self.wait != -1) {
        self.count = self.height;
    }
    else {
        // Don't need to be active in the world anymore
        setsize(self, VEC_ORIGIN, VEC_ORIGIN);
        // Produce explosion sprite/particle effect using attachment entity
        if (self.spawnflags & BREAK_EXPLOSION) {
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, TE_EXPLOSION);
            WriteCoord (MSG_BROADCAST, self.attachment.origin_x);
            WriteCoord (MSG_BROADCAST, self.attachment.origin_y);
            WriteCoord (MSG_BROADCAST, self.attachment.origin_z);
            
            SpawnExplosion(EXPLODE_SMALL, self.attachment.origin, SOUND_REXP3);
            // Do no remove breakable, the rumble is still using
            // the noise keys for impact sounds
            entity_hide(self);
        }
        //----------------------------------------------------------------------
        // create any explosive damage (do this last!)
        // setup the radius explosion as a delay to prevent endless loops
        if (self.dmg) {
            self.think = funcbreakable_delayexplode;
            self.nextthink = time + 0.05;
        }
    }
};

//----------------------------------------------------------------------
void() funcbreakable_touch =
{
    // This only works for monster and special jumping break impacts
    if (!(other.flags & FL_MONSTER)) return;
    if (other.flags & FL_ONGROUND) return;
    if (!self.brktrigjump) return;
    self.touch = SUB_Null;
    self.use();
};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage)    funcbreakable_pain =
{
    local float loop_count;
    local vector vel;
    // As always there are exceptions
    // Grenades bounce and don't impact breakables, cannot tell impact point
    // If inflictor the same as attacker then particle impact done already
    if (inflictor.classtype == CT_PROJ_GL) return;
    if (inflictor == attacker) return;
    // Check for player exception on damage to breakable
    if (inflictor.flags & FL_CLIENT && self.brktrignoplayer) return;
    if (attacker.flags & FL_CLIENT && self.brktrignoplayer) return;
    
    // Something is trying to wear down the breakable with damage
    // work out facing angle and project particles upward
    makevectors(inflictor.angles);
    vel = -v_up*2;
    while(loop_count < 4) {
        particle (inflictor.origin, vel*0.1, self.bleedcolour + rint(random()*7), damage);
        loop_count = loop_count + 1;
    }
};

//----------------------------------------------------------------------
void() funcbreakable_death =
{
    if (self.spawnflags & BREAK_NOSHOOT) return;
    // Check for player exception on damage to breakable
    else if (self.activate.flags & FL_CLIENT && self.brktrignoplayer) return;
    else funcbreakable_use();
};




// Qmaster's custom template breakable setup
.string break_template1;
.string break_template2;
.string break_template3;
.string break_template4;
.string break_template5;
.float brk_obj_count1;
.float brk_obj_count2;
.float brk_obj_count3;
.float brk_obj_count4;
.float brk_obj_count5;

void() break_template_setup = {
    if (self.break_template1 != "") precache_model(self.break_template1);
    if (self.break_template2 != "") precache_model(self.break_template2);
    if (self.break_template3 != "") precache_model(self.break_template3);
    if (self.break_template4 != "") precache_model(self.break_template4);
    if (self.break_template5 != "") precache_model(self.break_template5);
};

// from Qmaster -- dumptruck_ds
void() make_breakable_debris = {
    local float i;
    local entity new;

    i = 0;
    if (self.break_template1 != "") {
        while (i < self.brk_obj_count1) {
            new = spawn();
            new.model = self.break_template1;
            new.origin_x = (self.maxs_x - self.mins_x)*random() + self.mins_x;
            new.origin_y = (self.maxs_y - self.mins_y)*random() + self.mins_y;
            new.origin_z = (self.maxs_z - self.mins_z)*random() + self.mins_z;
            setmodel (new, new.model); //dumptruck_ds
            setsize (new, '0 0 0', '0 0 0');
            new.velocity = VelocityForDamage (self.health*2);
            new.movetype = MOVETYPE_BOUNCE;
            new.solid = SOLID_NOT;
            new.avelocity_x = random()*600;
            new.avelocity_y = random()*600;
            new.avelocity_z = random()*600;
            new.think = SUB_Remove;
            new.ltime = time;
            new.nextthink = time + 10 + random()*10;
            new.flags = 0;
            i++;
        }
    }

    i = 0;
    if (self.break_template2 != "") {
        while (i < self.brk_obj_count2) {
            new = spawn();
            new.model = self.break_template2;
            new.origin_x = (self.maxs_x - self.mins_x)*random() + self.mins_x;
            new.origin_y = (self.maxs_y - self.mins_y)*random() + self.mins_y;
            new.origin_z = (self.maxs_z - self.mins_z)*random() + self.mins_z;
            setmodel (new, new.model); //dumptruck_ds
            setsize (new, '0 0 0', '0 0 0');
            new.velocity = VelocityForDamage (self.health*2);
            new.movetype = MOVETYPE_BOUNCE;
            new.solid = SOLID_NOT;
            new.avelocity_x = random()*600;
            new.avelocity_y = random()*600;
            new.avelocity_z = random()*600;
            new.think = SUB_Remove;
            new.ltime = time;
            new.nextthink = time + 10 + random()*10;
            new.flags = 0;
            i++;
        }
    }

    i = 0;
    if (self.break_template3 != "") {
        while (i < self.brk_obj_count3) {
            new = spawn();
            new.model = self.break_template3;
            new.origin_x = (self.maxs_x - self.mins_x)*random() + self.mins_x;
            new.origin_y = (self.maxs_y - self.mins_y)*random() + self.mins_y;
            new.origin_z = (self.maxs_z - self.mins_z)*random() + self.mins_z;
            setmodel (new, new.model); //dumptruck_ds
            setsize (new, '0 0 0', '0 0 0');
            new.velocity = VelocityForDamage (self.health*2);
            new.movetype = MOVETYPE_BOUNCE;
            new.solid = SOLID_NOT;
            new.avelocity_x = random()*600;
            new.avelocity_y = random()*600;
            new.avelocity_z = random()*600;
            new.think = SUB_Remove;
            new.ltime = time;
            new.nextthink = time + 10 + random()*10;
            new.flags = 0;
            i++;
        }
    }

    i = 0;
    if (self.break_template4 != "") {
        while (i < self.brk_obj_count4) {
            new = spawn();
            new.model = self.break_template4;
            new.origin_x = (self.maxs_x - self.mins_x)*random() + self.mins_x;
            new.origin_y = (self.maxs_y - self.mins_y)*random() + self.mins_y;
            new.origin_z = (self.maxs_z - self.mins_z)*random() + self.mins_z;
            setmodel (new, new.model); //dumptruck_ds
            setsize (new, '0 0 0', '0 0 0');
            new.velocity = VelocityForDamage (self.health*2);
            new.movetype = MOVETYPE_BOUNCE;
            new.solid = SOLID_NOT;
            new.avelocity_x = random()*600;
            new.avelocity_y = random()*600;
            new.avelocity_z = random()*600;
            new.think = SUB_Remove;
            new.ltime = time;
            new.nextthink = time + 10 + random()*10;
            new.flags = 0;
            i++;
        }
    }

    i = 0;
    if (self.break_template5 != "") {
        while (i < self.brk_obj_count5) {
            new = spawn();
            new.model = self.break_template5;
            new.origin_x = (self.maxs_x - self.mins_x)*random() + self.mins_x;
            new.origin_y = (self.maxs_y - self.mins_y)*random() + self.mins_y;
            new.origin_z = (self.maxs_z - self.mins_z)*random() + self.mins_z;
            setmodel (new, new.model); //dumptruck_ds
            setsize (new, '0 0 0', '0 0 0');
            new.velocity = VelocityForDamage (self.health*2);
            new.movetype = MOVETYPE_BOUNCE;
            new.solid = SOLID_NOT;
            new.avelocity_x = random()*600;
            new.avelocity_y = random()*600;
            new.avelocity_z = random()*600;
            new.think = SUB_Remove;
            new.ltime = time;
            new.nextthink = time + 10 + random()*10;
            new.flags = 0;
            i++;
        }
    }
};

//======================================================================
// Setup all model/sound pre-cache
//----------------------------------------------------------------------
void() breakable_cache =
{
    if (self.spawnflags & BREAK_USEMAP) {
        break_template_setup();
        return;
    }

    //----------------------------------------------------------------------
    // If a breakable style key setup, update all undefined sounds/model keys
    if (self.style > 0 && self.style < BTYPE_CUSTOM) {
        if (!self.brksound) self.brksound = self.style;
        if (!self.brkimpsound) self.brkimpsound = self.style;
        if (!self.brkobjects) self.brkobjects = self.style*10;
    }
    else return;
    
    //----------------------------------------------------------------------
    // make sure initial break sound is within range types (def=rock)
    if (self.brksound < BTYPE_ROCK || self.brksound > BTYPE_CUSTOM) 
        self.brksound = BTYPE_ROCK;
        
    if (self.brksound == BTYPE_ROCK || self.brksound == BTYPE_BRICK) {
        self.noise = SOUND_BRK_ROCK;
        // Heavy rocks don't move around too much
        if (!self.brkvelbase) self.brkvelbase = '0 0 50';
        if (!self.brkveladd) self.brkveladd = '100 100 100';
    }
    else if (self.brksound == BTYPE_WOOD) {
        self.noise = SOUND_BRK_WOOD;
        if (!self.brkvelbase) self.brkvelbase = '50 50 50';
        if (!self.brkveladd) self.brkveladd = '100 100 50';
    }
    else if (self.brksound == BTYPE_GLASS) {
        self.noise = SOUND_BRK_GLASS;
        if (!self.brkvelbase) self.brkvelbase = '50 50 50';
        if (!self.brkveladd) self.brkveladd = '100 100 150';
    }
    else if (self.brksound == BTYPE_METAL) {
        self.noise = SOUND_BRK_METAL;
        if (!self.brkvelbase) self.brkvelbase = '50 50 100';
        if (!self.brkveladd) self.brkveladd = '100 100 150';
    }
    else if (self.brksound == BTYPE_CERAMIC) {
        self.noise = SOUND_BRK_CERAMIC;
        if (!self.brkvelbase) self.brkvelbase = '50 50 50';
        if (!self.brkveladd) self.brkveladd = '100 100 150';
    }
    else if (self.noise == "") {
        self.noise = SOUND_EMPTY;
        if (!self.brkvelbase) self.brkvelbase = '50 50 50';
        if (!self.brkveladd) self.brkveladd = '100 100 150';
    }
    precache_sound(self.noise);    

    //----------------------------------------------------------------------
    // make sure impack sounds are within range types (def=rock)
    if (self.brkimpsound < BTYPE_ROCK || self.brkimpsound > BTYPE_CUSTOM) 
        self.brkimpsound = BTYPE_ROCK;
    // All the impact sounds come in sets of 4 for random variety
    self.brkimpqty = 4;
        
    if (self.brkimpsound == BTYPE_ROCK || self.brkimpsound == BTYPE_BRICK) {
        if (self.noise1 == "") self.noise1 = SOUND_IMP_ROCK1;
        if (self.noise2 == "") self.noise2 = SOUND_IMP_ROCK2;
        if (self.noise3 == "") self.noise3 = SOUND_IMP_ROCK3;
        if (self.noise4 == "") self.noise4 = SOUND_IMP_ROCK4;
    }
    else if (self.brkimpsound == BTYPE_WOOD) {
        if (self.noise1 == "") self.noise1 = SOUND_IMP_WOOD1;
        if (self.noise2 == "") self.noise2 = SOUND_IMP_WOOD2;
        if (self.noise3 == "") self.noise3 = SOUND_IMP_WOOD3;
        if (self.noise4 == "") self.noise4 = SOUND_IMP_WOOD4;
    }
    else if (self.brkimpsound == BTYPE_GLASS) {
        if (self.noise1 == "") self.noise1 = SOUND_IMP_GLASS1;
        if (self.noise2 == "") self.noise2 = SOUND_IMP_GLASS2;
        if (self.noise3 == "") self.noise3 = SOUND_IMP_GLASS3;
        if (self.noise4 == "") self.noise4 = SOUND_IMP_GLASS4;
    }
    else if (self.brkimpsound == BTYPE_METAL) {
        if (self.noise1 == "") self.noise1 = SOUND_IMP_METAL1;
        if (self.noise2 == "") self.noise2 = SOUND_IMP_METAL2;
        if (self.noise3 == "") self.noise3 = SOUND_IMP_METAL3;
        if (self.noise4 == "") self.noise4 = SOUND_IMP_METAL4;
    }
    else if (self.brkimpsound == BTYPE_CERAMIC) {
        if (self.noise1 == "") self.noise1 = SOUND_IMP_CERAMIC1;
        if (self.noise2 == "") self.noise2 = SOUND_IMP_CERAMIC2;
        if (self.noise3 == "") self.noise3 = SOUND_IMP_CERAMIC3;
        if (self.noise4 == "") self.noise4 = SOUND_IMP_CERAMIC4;
    }
    else {
        // Workout total amount of active impact sounds (custom can have <4)
        // empty slots are filled up with empty sounds for precache reasons
        if (self.noise2 != "") {
            if (self.noise3 != "") {
                if (self.noise4 != "") self.brkimpqty = 4;
                else self.brkimpqty = 3;
            }
            else self.brkimpqty = 2;
        }
        else self.brkimpqty = 1;
        // Cannot have no impact sounds, always setup one sound by default
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '80 8 1';    // Green/Brown
        if (self.noise1 == "") self.noise1 = SOUND_IMP_ROCK1;
        if (self.noise2 == "") self.noise2 = SOUND_EMPTY;
        if (self.noise3 == "") self.noise3 = SOUND_EMPTY;
        if (self.noise4 == "") self.noise4 = SOUND_EMPTY;
    }
    precache_sound (self.noise1);
    precache_sound (self.noise2);
    precache_sound (self.noise3);
    precache_sound (self.noise4);

    //----------------------------------------------------------------------
    // make sure breakable objects are within range types (def=rock)
    //----------------------------------------------------------------------
    if (self.brkobjects < BMODTYPE_SELF) self.brkobjects = BMODTYPE_ROCK1;

    // Two ways the models can be defined, using the default bmodel types
    // or specifying the models via the brkobjs strings
    // If the first string is empty, then the default = 4 (pre-defined)
    // If first string defined, then count the model strings

    // If the first string is not empty then the mapper has to specify
    // exactly what rubble models to choose from (randomly)
    // The auto fill option (style/brkobjects) only if brkobj1 = empty
    
    if (self.brkobj1 == "") self.brkobjqty = 4; // Default
    else {
        // Need to check for custom breakable quantity first
        if (self.brkobj2 != "") {
            if (self.brkobj3 != "") {
                if (self.brkobj4 != "") self.brkobjqty = 4;
                else self.brkobjqty = 3;
            }
            else self.brkobjqty = 2;
        }
        else self.brkobjqty = 1;
    }
    
    // All the broken models come in sets of 4 for random variety
    // if brkobjects is set to a custom value then model strings will be filled in
    // with a blank string and the brkobjqty will be adjusted to actual quanity    
    if (self.brkobjects == BMODTYPE_ROCK1) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '80 8 1';    // Green/Brown
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_ROCK1A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_ROCK1B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_ROCK1C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_ROCK1D;
    }
    else if (self.brkobjects == BMODTYPE_ROCK2) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '48 8 1';    // Green
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_ROCK2A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_ROCK2B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_ROCK2C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_ROCK2D;
    }
    else if (self.brkobjects == BMODTYPE_ROCK3) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '20 8 1';    // Light Brown
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_ROCK3A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_ROCK3B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_ROCK3C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_ROCK3D;
    }
    else if (self.brkobjects == BMODTYPE_ROCK4) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '168 8 1';    // Whiteish
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_ROCK4A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_ROCK4B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_ROCK4C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_ROCK4D;
    }
    else if (self.brkobjects == BMODTYPE_ROCK5) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '32 8 1';    // Blue
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_ROCK5A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_ROCK5B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_ROCK5C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_ROCK5D;
    }
    else if (self.brkobjects == BMODTYPE_ROCK6) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '1 8 1';    // Black
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_ROCK6A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_ROCK6B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_ROCK6C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_ROCK6D;
    }
    else if (self.brkobjects == BMODTYPE_WOOD1) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '16 8 1';    // Dark Brown
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_WOOD1A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_WOOD1B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_WOOD1C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_WOOD1D;
    }
    else if (self.brkobjects == BMODTYPE_WOOD2) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '112 8 1';    // Light Brown
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_WOOD2A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_WOOD2B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_WOOD2C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_WOOD2D;
    }
    else if (self.brkobjects == BMODTYPE_WOOD3) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '80 8 1';    // Green/Brown
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_WOOD3A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_WOOD3B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_WOOD3C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_WOOD3D;
    }
    else if (self.brkobjects == BMODTYPE_GLASS1) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '32 8 1';    // Blue
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_GLASS1A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_GLASS1B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_GLASS1C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_GLASS1D;
    }
    else if (self.brkobjects == BMODTYPE_GLASS2) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '64 8 1';    // Red
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_GLASS2A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_GLASS2B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_GLASS2C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_GLASS2D;
    }
    else if (self.brkobjects == BMODTYPE_GLASS3) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '96 8 1';    // Pink/Yellow
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_GLASS3A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_GLASS3B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_GLASS3C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_GLASS3D;
    }
    else if (self.brkobjects == BMODTYPE_GLASS4) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '128 4 1';    // Purple
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_GLASS4A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_GLASS4B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_GLASS4C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_GLASS4D;
    }
    else if (self.brkobjects == BMODTYPE_METAL1) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '16 8 1';    // Brown
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_METAL1A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_METAL1B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_METAL1C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_METAL1D;
    }
    else if (self.brkobjects == BMODTYPE_METAL2) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '80 8 1';    // Green/Brown
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_METAL2A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_METAL2B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_METAL2C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_METAL2D;
    }
    else if (self.brkobjects == BMODTYPE_METAL3) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '48 8 1';    // Green
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_METAL3A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_METAL3B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_METAL3C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_METAL3D;
    }
    else if (self.brkobjects == BMODTYPE_METAL4) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '48 8 1';    // Green
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_METAL4A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_METAL4B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_METAL4C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_METAL4D;
    }
    else if (self.brkobjects == BMODTYPE_METAL5) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '32 8 1';    // Blue
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_METAL5A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_METAL5B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_METAL5C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_METAL5D;
    }
    else if (self.brkobjects == BMODTYPE_BRICK1) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '16 8 1';    // Brown
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_BRICK1A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_BRICK1B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_BRICK1C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_BRICK1D;
    }
    else if (self.brkobjects == BMODTYPE_BRICK2) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '80 8 1';    // Green/Brown
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_BRICK2A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_BRICK2B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_BRICK2C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_BRICK2D;
    }
    else if (self.brkobjects == BMODTYPE_BRICK3) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '48 8 1';    // Green
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_BRICK3A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_BRICK3B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_BRICK3C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_BRICK3D;
    }
    else if (self.brkobjects == BMODTYPE_BRICK4) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '0 8 1';    // Grey/White
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_BRICK4A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_BRICK4B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_BRICK4C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_BRICK4D;
    }
    else if (self.brkobjects == BMODTYPE_BRICK5) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '16 8 1';    // Brown
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_BRICK5A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_BRICK5B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_BRICK5C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_BRICK5D;
    }
    else if (self.brkobjects == BMODTYPE_BRICK6) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '16 8 1';    // Brown
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_BRICK6A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_BRICK6B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_BRICK6C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_BRICK6D;
    }
    else if (self.brkobjects == BMODTYPE_BRICK7) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '32 8 1';    // Blue
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_BRICK7A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_BRICK7B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_BRICK7C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_BRICK7D;
    }
    else if (self.brkobjects == BMODTYPE_CERAMIC1) {
        if (self.pos1_x + self.pos1_y == 0) self.pos1 = '96 8 1';    // Pink/Yellow
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_GLASS3A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_GLASS3B;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_BRK_GLASS3C;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_BRK_GLASS3D;
    }
    else {
        // Cannot have no breakable models, always setup one model by default
        if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_ROCK1A;
        if (self.brkobj2 == "") self.brkobj2 = MODEL_EMPTY;
        if (self.brkobj3 == "") self.brkobj3 = MODEL_EMPTY;
        if (self.brkobj4 == "") self.brkobj4 = MODEL_EMPTY;
    }

    precache_model(self.brkobj1);
    precache_model(self.brkobj2);
    precache_model(self.brkobj3);
    precache_model(self.brkobj4);
};
    
//----------------------------------------------------------------------
// Setup defaults
//----------------------------------------------------------------------
void() breakable_defaults =
{
    // Reset trigger ONCE function
    self.waitmin = FALSE;

    // Setup rubble counter = count + random*cnt
    if (self.count <= 0) self.count = 4;
    if (self.cnt == 0) self.cnt = 4;
    
    // If cnt = -1 then don't do any random factor, just exact amount
    if (self.cnt < 0) self.height = self.count;
    else self.height = self.count + rint(random()*self.cnt);
    
    // Backup rubble count for later (point entity)
    self.count = self.height;    
    
    // Special conditions - jump only, missile only, no health
    if (self.brktrigjump < 0 || self.brktrigmissile < 0 || self.health < 0) 
        self.spawnflags = self.spawnflags | BREAK_NOSHOOT;

    // Setup default health
    if (self.health == 0) self.health = 1;
    
    // Rubble damage overrides any explosion damage
    if (self.spawnflags & BREAK_DAMAGE) {
        if (self.dmg <= 0) self.dmg = 2;
    }
    else if (self.spawnflags & BREAK_EXPLOSION) {
        if (self.dmg <= 0) self.dmg = DAMAGE_MONROCKET;
    }
    
    // Setup default base + additional velocity and angle spin
    if (!self.brkvelbase) self.brkvelbase = '50 50 100';
    if (!self.brkveladd) self.brkveladd = '100 100 150';
    if (self.brkavel <= 0) self.brkavel = 200;
    if (self.brkfade <= 0) self.brkfade = 4;

    // angles has to be 0 0 0 otherwise brush model is twisted
    // Is there any angle direction defined for impact force direction
    if (self.angles_y) {
        self.movedir_y = self.angles_y;
        self.spawnflags = self.spawnflags | BREAK_MOVEDIR;
    }
    self.mangle = self.angles = '0 0 0';
    
    // reset velocity / avelocity just in case of rogue key fields
    self.velocity = self.avelocity = '0 0 0';
    
    // Can fire other triggers, make sure no delay on breakable function
    self.delay = 0;
};

//----------------------------------------------------------------------
void() breakable_on =
{
    // Make sure use function reset
    self.use = SUB_Null;
    
    // Setup collision based on bmodel type
    if (self.bsporigin) {
        self.solid = SOLID_BSP;
        self.movetype = MOVETYPE_PUSH;
        setmodel (self, self.model);
        setorigin (self, self.origin);    
        setsize (self, self.mins , self.maxs);

        // Used for sound, radius damage and explosion
        self.attachment = spawn();
        self.attachment.origin = bmodel_origin(self);
        setorigin(self.attachment, self.attachment.origin);
    }
    else {
        self.solid = SOLID_BBOX;
        self.movetype = MOVETYPE_NONE;
        setmodel (self, self.mdl);
        setorigin (self, self.origin);    
        setsize (self, self.bbmins , self.bbmaxs);
        self.angles = self.pos2;
        
        // Used for sound, radius damage and explosion
        self.attachment = self;
    }
    
    if (self.spawnflags & BREAK_NOSHOOT) {
        self.takedamage = DAMAGE_NO;
        self.th_pain = SUB_Null_pain;
        self.th_die = SUB_Null;
    }
    else {
        self.takedamage = DAMAGE_YES;
        self.th_pain = funcbreakable_pain;
        self.th_die = funcbreakable_death;
        self.bleedcolour = self.pos1_x;
        // Setup location (origin) to spawn particles
        if (self.bsporigin) self.oldorigin = bmodel_origin(self);
        else self.oldorigin = self.origin;
    }
    
    // Originally had this as a targetname condition because
    // if the breakable is used then it needs a referenced name
    // Since adding brktrigjump, brktrigmissile ent keys there are
    // external trigger events which need breakable entities to react
    self.use = funcbreakable_use;
    // Jumping monsters don't often hit the breakable object easily
    // Easier to trap jump impact triggering with a touch function
    if (self.brktrigjump) self.touch = funcbreakable_touch;
};

//======================================================================
// Basic workhorse of breakable system (bmodel)
//======================================================================
void() func_breakable =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    // reset out of range styles and setup default = BTYPE_ROCK (1)
    if (self.style < BTYPE_ROCK || self.style > BTYPE_MAX) self.style = BTYPE_ROCK;
    // precache all sound/model stuff
    breakable_cache();
    breakable_defaults();

    // No point setting up a breakable if it cannot be triggered or shoot
    if (self.targetname == "" && self.spawnflags & BREAK_NOSHOOT) {
        dprint("\b[BRKMDL]\b Cannot be triggered or shoot, removed\n");
        setmodel (self, self.model);    // set size and link into world
        setorigin (self, self.origin);    
        setsize (self, self.mins , self.maxs);
        spawn_marker(bmodel_origin(self), SPNMARK_YELLOW);
        remove(self);
        return;
    }

    self.classtype = CT_FUNCBREAK;
    self.classgroup = CG_BREAKABLE;
    self.bsporigin = TRUE;        // bmodel origin 0,0,0
    self.wait = -1;                // Always work once, cannot unbreak

    if (self.spawnflags & BREAK_STARTOFF) self.use = breakable_on;
    else breakable_on();
};

//----------------------------------------------------------------------
// Point entity version of breakbles
// Good for producing rubble from areas unreachable players
// Earthquake and rubble and dust from ceilings
//----------------------------------------------------------------------
void() func_breakable_spawner =
{
    // reset out of range styles and setup default = BTYPE_ROCK (1)
    if (self.style < BTYPE_ROCK || self.style > BTYPE_MAX) self.style = BTYPE_ROCK;
    // precache all sound/model stuff
    breakable_cache();
    breakable_defaults();

    // Check for targetname
    if (self.targetname == "") {
        dprint("\b[BRKTRIG]\b Missing targetname\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        remove(self);
        return;
    }

    self.classtype = CT_FUNCBREAKSPN;
    self.classgroup = CG_BREAKABLE;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    if (self.brkvol_x < 8) self.brkvol_x = 8;
    if (self.brkvol_y < 8) self.brkvol_y = 8;
    if (self.brkvol_z < 8) self.brkvol_z = 8;
    
    // Already a point entity, don't need to generate anything else
    self.attachment = self;
    self.use = funcbreakable_use;
};

//======================================================================
// General purpose bmodel with toggle states and solid/fade functions
// Could be used for all sorts of situations not involving breakables
//======================================================================
/*QUAKED func_breakable_wall (0 .5 .8) ? START_ON SOLID FADEOUT
Switchable bmodel for breakable setups with optional collision
-------- KEYS --------
targetname : trigger entity (works with entity state system)
wait       : set to -1 for trigger once condition (def=0)
waitmin    : random time to wait before fading out
-------- SPAWNFLAGS --------
START_ON : Will spawn visible and wait for trigger
SOLID    : Will block player/monster movement
FADEOUT  : Will fade out after a certain amount of time if visible!
-------- NOTES --------
Switchable bmodel for breakable setups with optional collision

=============================================================================*/
void() breakable_wall_state =
{
    // Show model and setup collision state
    if (self.state == STATE_ENABLED) {
        // Does the bmodel require any solid collision?
        if (self.spawnflags & BREAKWALL_SOLID) {
            self.solid = SOLID_BSP;
            self.movetype = MOVETYPE_PUSH;
        }
        else {
            // Bmodel visiable, no collision
            self.solid = SOLID_NOT;
            self.movetype = MOVETYPE_NONE;
        }
        // Add bmodel to the world
        setmodel (self, self.mdl);
    }
    // hide model
    else {
        self.solid = SOLID_NOT;
        self.movetype = MOVETYPE_NONE;
        self.model = "";
    }
};

//----------------------------------------------------------------------
void() breakable_wall_use =
{
    if (self.estate & ESTATE_BLOCK) return;
    if (self.attack_finished > time) return;

    // Setup to trigger once?
    if (self.wait < 0) self.attack_finished = LARGE_TIMER;

    // Toggle bmodel visible state
    if (self.state == STATE_ENABLED) self.state = STATE_DISABLED;
    else self.state = STATE_ENABLED;
    
    // make sure model state is correct
    breakable_wall_state();

    // Is the breakable wall visible and designed to fade away?
    if (self.spawnflags & BREAKWALL_FADEOUT && self.state == STATE_ENABLED) {
        // Setup random timer and fade away!
        self.nextthink = time + self.waitmin + random()*self.waitmin;
        self.think = breakable_remove;
        // Change the model type and state otherwise will not alpah fade
        self.movetype = MOVETYPE_NONE;
        self.solid = SOLID_BBOX;
    }    
};    

//----------------------------------------------------------------------
void() func_breakable_wall = {
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_FUNCBREAKWALL;
    self.classgroup = CG_BREAKABLE;
    self.bsporigin = TRUE;        // bmodel origin 0,0,0
    self.angles = '0 0 0';        // Stop model twisting
    self.mdl = self.model;        // Save for later

    if (check_bmodel_keys()) return;    // Check for bmodel errors

    // Make sure the bmodel is active in the world before changing state
    self.solid = SOLID_BSP;
    self.movetype = MOVETYPE_PUSH;
    setmodel (self, self.mdl);

    // Something that fades away does it only once
    if (self.spawnflags & BREAKWALL_FADEOUT) self.wait = -1;
    // Default fade time = time+fade+random()*fade
    if (!self.waitmin) self.waitmin = 4;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = breakable_wall_use;
    self.estate = ESTATE_ON;
    
    if (self.spawnflags & BREAKWALL_START_ON) self.state = STATE_ENABLED;
    else self.state = STATE_DISABLED;
    breakable_wall_state();
};

//======================================================================
// A point entity which triggers a breakable and monster together
//======================================================================
/*QUAKED trigger_monsterbreak (.8 .5 .8) (-8 -8 -8) (8 8 8) NODELAY WAKEANIM
Trigger (once) breakable and monster together
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target     : points to func_breakable (single target)
target2    : points to a monster (single target)
wait       : time before breakable is triggered (def 0.2)
-------- SPAWNFLAGS --------
NODELAY  : No delay between monster and breakable trigger
WAKEANIM : Will do special wakeup animation when triggered
-------- NOTES --------
Trigger (once) breakable and monster together

======================================================================*/
void() trig_monbreak_delay =
{
    // Check for breakable targetname first
    if (self.target != "") {
        // Find breakable (single target only)
        self.owner = find(world,targetname,self.target);
        if (self.owner.classtype == CT_FUNCBREAK) {
            self.owner.activate = self;
            self = self.owner;
            self.use();
        }
    }
};

//----------------------------------------------------------------------
void() trig_monbreak_use =
{
    if (self.estate & ESTATE_BLOCK) return;
    if (self.attack_finished > time) return;
    
    // Trigger once
    self.attack_finished = LARGE_TIMER;

    // Any monster defined to wakeup?
    if (self.target2 != "") {
        // Find monster (single target only)
        self.enemy = find(world,targetname,self.target2);
        if (self.enemy.flags & FL_MONSTER) {
            if (self.spawnflags & MONTRIG_WAKEUPANIM) self.enemy.wakeuptrigger = TRUE;
            trigger_ent(self.enemy, other);
        }
    }

    // Time to explode breakable?
    if (self.spawnflags & MONTRIG_NODELAY) trig_monbreak_delay();
    else {
        // setup delay to trigger breakable wall
        self.nextthink = time + self.wait;
        self.think = trig_monbreak_delay;
    }
};

//----------------------------------------------------------------------
void() trigger_monsterbreak =
{
    self.classtype = CT_TRIGMONBREAK;
    self.classgroup = CG_BREAKABLE;
    if (self.wait <= 0) self.wait = 0.2;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trig_monbreak_use;
    self.estate = ESTATE_ON;
};

/*======================================================================
/*QUAKED misc_breakable_pot1 (1 .5 .25) (-24 -24 -32) (24 24 32) STARTOFF NODAMAGE EXPLOSION SILENT DAMAGE NOMOSTER NOSOUND NOROTATE
{ model(":progs/misc_pot1.mdl"); }
Breakable ceramic Pot 1 with handles
-------- KEYS --------
target    : Additional targets to fire when model breaks
mangle    : Override model orientation (Pitch Yaw Roll)
exactskin : 0=Default, 1=pattern 2, 2=pattern 3, 3= pattern 4
health    : amount of damage to take before breaking (def 1)
count     : minimum quantity to spawn (def 4)
cnt       : random quantity to spawn (def 4) final qty = count + random(cnt)
dmg       : explosive radius damage (emits from center of func object)
pos1      : x=start particle colour, y=random range, z=quantity
brkvelbase : Base amount for velocity of broken parts (def "50 50 100")
brkveladd : Random additions for velocity of broken parts (def "100 100 150")
brkavel   : Amount of breaking object angle velocity (def 200)
brkfade   : Fade time before rubble fades away (def 4+random()x4)
angles    : direction to throw rubble (override default = impact direction)
brkgravity : will change the gravity for rubble, useful for underwater
-------- SPAWNFLAGS --------
STARTOFF  : Will wait for trigger to spawn
NODAMAGE  : Cannot be damaged or shot, trigger only
EXPLOSION : trigger sprite/particle explosion
SILENT    : No initial breakage sound
DAMAGE    : Spawning rubble can damage (def = 2, use dmg key for touch damage)
NOMONSTER : monsters cannot damage this breakable and/or spawning rubble will not damage monsters
NOSOUND   : Spawning rubble has no impact sounds
NOROTATE  : Spawning rubble has No Y rotation
-------- NOTES --------
Breakable ceramic Pot 1 with handles
======================================================================*/
void() misc_breakable_pot1 = 
{
    self.classtype = CT_FUNCBREAKMDL;
    self.classgroup = CG_BREAKABLE;
    self.bbmins = '-16 -16 0';
    self.bbmaxs = '16 16 48';
    self.brkvol = '32 32 48';
    self.mdl = "progs/ne_ruins/pot1.mdl";
    precache_model(self.mdl);
    self.skin = self.exactskin;

    self.style = BTYPE_CERAMIC;
    self.brkobjects = BMODTYPE_CUSTOM;
    self.brkobj1 = "progs/ne_ruins/potshard1.mdl";
    self.brkobj2 = "progs/ne_ruins/potshard2.mdl";
    self.brkobj3 = "progs/ne_ruins/potshard3.mdl";
    self.brkobj4 = "progs/ne_ruins/potshard4.mdl";
    self.pos1 = '16 8 1';    // Brown
    self.wait = -1;            // Only break once

    // Save any custom angles, otherwise random Y rotation
    self.pos2 = '0 0 0';
    if (CheckZeroVector(self.mangle) == FALSE) self.pos2 = self.mangle;
    else if (query_configflag(SVR_ITEMROTATE) == FALSE)
        self.pos2_y = rint(random()*359);
    
    // precache all sound/model stuff
    breakable_cache();
    breakable_defaults();

    if (self.spawnflags & BREAK_STARTOFF) self.use = breakable_on;
    else breakable_on();
};
