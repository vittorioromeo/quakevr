//======================================================================
// Particle System
//======================================================================
// Particle management
float PARTICLE_MAXENTS = 1024;    // Maximum amount of particles active (default)
float PARTICLE_BANK = 64;        // How many particle in each setup bank
float PARTICLE_BUFFER = 128;    // How many particle to create before unlock
float        part_message;        // Timer for particle message (every 3s)
entity        part_control;        // Particle setup entity (drives nextthink)
entity        part_start;            // Pointers to start/end particle chain
entity        part_end;
entity        part_free;            // Current particle entity
float        part_total;            // Currently active particles
float        part_max;            // Maximum active particles
entity        part;                // Spawning particle emitters and particles

//----------------------------------------------------------------------
// Particle emitter parameters
float PARTICLE_START_OFF = 1;    // Particle emitter starts OFF
float PARTICLE_START_ON = 2;    // Particle emitter starts ON
float PARTICLE_WEATHER_SNOW = 8;    // Weather snow - White

.float        particlemax;        // Maximum about of ACTIVE particles per map
.entity        entchain;            // Entity pointer to next entity in chain
.entity        part_emitter;        // Particle emitter link on any entity
.entity        pemit_source;        // Source/owner of particle emitter
.entity        pemit_target;        // Particle emitter target entity
.float        target_valid;        // Test condition for valid target entity (not self or world)
.float        part_style;            // Pre-defined particle styles
.float        pemit_tcount;        // Particle emitter, total active particles 
.float        part_active;        // Is the particle active or not
.float        start_delay;        // Particle emitter start delay
.float        dpp_only;            // Is the particle a DPP only effect?
.string        dpp_name;            // DP particle effect name in effectinfo.txt file
.float        dpp_wait;            // DP particle re-trigger timer
.float        dpp_rnd;            // DP particle random multiplier for time
.vector        dpp_vel;            // Direction of DP particles to move towards

//----------------------------------------------------------------------
// Particle style parameters
.string        spr_name1;            // Sprite string name 1 (uses defs)
.string        spr_name2;            // Sprite string name 2
.string        spr_name3;            // Sprite string name 3
.float        spr_frame;            // Sprite frame type 1=all, 2=light, 3=dark
.float        part_limit;            // Maximum amount of particles to emit
.float        part_life;            // Life time
.vector        part_ofs;            // Offset from emitter
.float        part_veltype;        // Velocity types
.vector        part_velbase;        // Base velocity (always this direction)
.vector        part_vel;            // Random Velocity direction (vel+random*vel)
.vector        part_velrand;        // Extra random velocity wobble
.float        part_velrot;        // Velocity rotation (Y axis only)
.vector        part_vol;            // Spawning volume
.float        circular_angle;        // Circular rotation angle
.float        part_movetype;        // Movetype of particle
.float        wakeup_dist;        // Wake up distance for particle emitter
.float        wakeup_timer;        // How often to check distance check
.float        spawn_base;            // Spawn rate - base value
.float        spawn_rand;            // Spawn rate - random adjustment

// Compiler forward links
void() setup_particleprecache;

//----------------------------------------------------------------------
// The particle is finished with, update emitter particle counter
//----------------------------------------------------------------------
void() finish_particle =
{
    if (self.classtype != CT_PARTICLE) {
        dprint("\b[PARTICLE_RETURN]\b This is not a particle!?!\n");
    }
    else {
        if (self.owner.pemit_tcount > 0)
            self.owner.pemit_tcount = self.owner.pemit_tcount - 1;
        
        self.velocity = VEC_ORIGIN;        // no more movement
        self.avelocity = VEC_ORIGIN;
        self.origin = VEC_ORIGIN;
        self.part_active = FALSE;        // Finished with particle
        setmodel(self, "");                // Remove from world
        self.owner = world;                // No owner anymore
        self.skin = self.frame = self.flags = FALSE;
        self.gravity = 1;
        
        // Update active particle total
        if (part_total > 1) part_total = part_total - 1;

        if (part_free.classtype != CT_PARTICLE) {
            dprint("\b[PARTICLE_RETURN]\b Current free particle is wrong classtype!!\n");
        }
        else {
            // Add particle back into central particle list
            self.entchain = part_free.entchain;        // link new particle forward in chain
            part_free.entchain = self;                // link free particle to new particle
    
            if (part_debug == 2) {
                dprint("Return Free ("); dprint(ftos(part_free.cnt));
                dprint(") Next ("); dprint(ftos(part_free.entchain.cnt));
                dprint(")\n");            
            }
        }
    }
};

//----------------------------------------------------------------------
// All requests for particles must go through one function
//----------------------------------------------------------------------
entity() fetch_particle =
{
    local entity tpart_ret;
    tpart_ret = world;        // Default - no particle returned

    // Make sure the system has 2 banks of particles setup first!
    if (part_control.count < PARTICLE_BUFFER) return tpart_ret;

    // Display particle limits if skill set to a debug number
    if (part_debug == 1) {
        sprint(client_ent, "CREATE Particle ("); sprint(client_ent, ftos(part_total));
        sprint(client_ent, "/"); sprint(client_ent, ftos(part_max));
        sprint(client_ent, ")\n");
    }
    
    // Are there any free particles?
    if (part_total >= part_max - 1) {
        if (part_message < time) {
            dprint("\b[PARTICLE]\b current limit ("); dprint(ftos(part_max));
            dprint(") too low!\n");
            part_message = time + 5;    // interval console spam
        }
    }
    else {
        if (part_free.classtype != CT_PARTICLE) {
            dprint("\b[PARTICLE_FETCH]\b Current free particle is wrong classtype!!\n");
        }
        else if (part_free.entchain.classtype != CT_PARTICLE) {
            dprint("\b[PARTICLE_FETCH]\b Next free particle is wrong classtype!!\n");
            dprint("Free ("); dprint(ftos(part_free.cnt));
            dprint(") Ctype ("); dprint(ftos(part_free.classtype));
            dprint(") Next ("); dprint(ftos(part_free.entchain.cnt));
            dprint(") Ctype ("); dprint(ftos(part_free.entchain.classtype));
            dprint(")\n");
        }
        else {
            tpart_ret = part_free.entchain;                // return next free particle
            part_free.entchain = tpart_ret.entchain;    // Skip forward in the chain
            
            // Update currently active particle total
            part_total = part_total + 1;
            tpart_ret.part_active = TRUE;

            if (part_debug == 2) {
                dprint("Fetch Free ("); dprint(ftos(part_free.cnt));
                dprint(") return ("); dprint(ftos(tpart_ret.cnt));
                dprint(")\n");
            }
        }
    }

    return (tpart_ret);
};

//----------------------------------------------------------------------
// Generate a chain of entities (one block at a time)
//----------------------------------------------------------------------
void() generate_particlechain =
{
    local entity part_prev, part_first;
    local float pcount;

    // Reset all variables
    part_first = part_prev = part = world;
    pcount = 0;
    
    // Is there anymore particles need to be created?
    if (self.count + 1 < part_max) {
        dprint("\b[PCHAIN]\b Part bank (");
        dprint(ftos(self.count)); dprint(" / ");
        dprint(ftos(part_max)); dprint(")\n");
        
        while (pcount < PARTICLE_BANK) {
            // Create a new particle entity
            part = spawn();
            part.solid = SOLID_NOT;
            part.owner = world;
            part.part_active = FALSE;
            part.classname = "particle";
            part.classtype = CT_PARTICLE;
            // If DP engine active remove particle shadow
            if (engine == ENG_DPEXT) part.effects = part.effects + EF_NOSHADOW;
            part.cnt = self.count + pcount;
            
            pcount = pcount + 1;        // Increase loop

            //----------------------------------------------------------------------
            // Link particle chains together
            if (part_prev.classtype != CT_PARTICLE)
                part_first = part;            // NEW CHAIN - setup all entity pointers
            else part_prev.entchain = part;    // EXISTING CHAIN - Link particles together

            part_prev = part;                // Setup previous particle in chain
            //----------------------------------------------------------------------
        }
        
        // Update total particles generated
        self.count = self.count + PARTICLE_BANK;

        //----------------------------------------------------------------------
        // Are there any previous banks of particles?
        if (part_start.classtype != CT_PARTICLE) {
            // NEW BANK - Setup start/end particle and close chain
            part_start = part.entchain = part_first;
            part_free = part_first.entchain;        // Move forward 1
            part_end = part;
        }
        else {
            // EXISTING BANK - Link new bank to existing banks
            part_end.entchain = part_first;            // Link end and first of new bank together
            part.entchain = part_start;                // Link current and start together
            part_end = part;                        // Move end of bank forward
        }
        //----------------------------------------------------------------------
    }
    
    //----------------------------------------------------------------------
    // Is there anymore particles need to be created?
    if (self.count + 1 < part_max) {
        self.think = generate_particlechain;
        self.nextthink = time + 0.2;
    }
    // Finished generating particle banks
    else self.waitmin = FALSE;
};

//----------------------------------------------------------------------
//  Setup particle chain in small banks
//----------------------------------------------------------------------
void() setup_particlechain =
{
    // Pre-cache all sprites
    setup_particleprecache();
    
    if (part_control.classtype != CT_PARTICLECONT) {
        part_control = spawn();
        part_control.solid = SOLID_NOT;
        part_control.classtype = CT_PARTICLECONT;
        
        dprint("\b[PCHAIN]\b Creating controller\n");
        part_free = part_end = part_start = world;
        part_total = part_control.count = 0;
        part_control.waitmin = TRUE;
    }
    // Setup next think
    part_control.think = generate_particlechain;
    part_control.nextthink = time + 0.3;
};

//----------------------------------------------------------------------
//  Extend particle chain (used by quickload issues)
//----------------------------------------------------------------------
void() extend_particlechain =
{
    local float new_part_max;
    
    // Check for particle chain controller FIRST!
    if (part_control.classtype != CT_PARTICLECONT) return;
    // Check if particle bank setup is running already
    if (part_control.waitmin) return;
    
    // Make sure particle banks are reset to default if low
    if (part_max < PARTICLE_MAXENTS) new_part_max = PARTICLE_MAXENTS;
    
    // Check for any worldspawn override value
    if (world.particlemax > 0) {
        if (new_part_max < world.particlemax) 
            new_part_max = world.particlemax;
    }

    // Does the particle bank max need to change?
    if (part_max > new_part_max) return;
    else part_max = new_part_max;
    
    dprint("\b[WORLD]\b New max particles (");
    dprint(ftos(part_max)); dprint(")\n");

    // Setup next think
    part_control.think = generate_particlechain;
    part_control.nextthink = time + 0.3;
    part_control.waitmin = TRUE;
};

//----------------------------------------------------------------------
float PARTICLE_ORIGIN_VOL = 0;            // Pick random point inside volume
float PARTICLE_ORIGIN_CIRCLE = 1;        // Move around circumference of circle
float PARTICLE_ORIGIN_RANDCIRCLE = 2;    // Randomly point on circumference
float PARTICLE_ORIGIN_SPIRAL = 3;        // Spiral outwards to circumference
float PARTICLE_ORIGIN_CENTER = 5;        // Explosion style from center

//----------------------------------------------------------------------
// particle BURST explosions
float PARTICLE_BURST_YELLOW = 1;        // gold, yellow, yellow
float PARTICLE_BURST_GREEN = 2;            // light green, green, green
float PARTICLE_BURST_RED = 4;            // grey, red, red
float PARTICLE_BURST_BLUE = 8;            // blue, grey, blue
float PARTICLE_BURST_PURPLE = 16;        // purple, grey, purple
float PARTICLE_BURST_FIRE = 32;            // gold, red, grey (used by ogre)
float PARTICLE_BURST_WHITE = 64;        // white, grey, grey

float PARTICLE_BURST_RING = 256;        // Circular particle burst
float PARTICLE_BURST_CENTER = 512;        // Central particle burst
float PARTICLE_BURST_UPWARD = 1024;        // Drifting upward burst
float PARTICLE_BURST_SHOCKWAVE = 4096;    // Floor impact shockwave
float PARTICLE_BURST_SKULLUP = 8192;    // Skull wizard teleport
float PARTICLE_BURST_LOSTUP = 16384;    // Lost souls particle burn
float PARTICLE_BURST_MINOTAUR = 32768;    // Minotaur magic attack over
//----------------------------------------------------------------------
// Map referenced particles
float PARTICLE_STYLE_CUSTOM = 1;        // Custom particle defined in map
float PARTICLE_STYLE_WEATHER = 5;        // Various weather type effects
float PARTICLE_STYLE_PORTAL = 10;        // White dot/bubbles floating outwards (volume)
float PARTICLE_STYLE_JUMPPAD = 15;        // Spiral pattern floating upward
float PARTICLE_STYLE_FCIRCLE = 20;        // Yellow dots floating upwards from circle on floor
float PARTICLE_STYLE_FFIELD = 25;        // Volume Forcefield custom direction

// Code referenced particles
float PARTICLE_STYLE_EMPTY = 0;            // Particle emitter is dead/broken
float PARTICLE_STYLE_BOOK = 50;            // Rotating white runes floating upwards (CLOSED)
float PARTICLE_STYLE_OPENBOOK = 55;        // Particle dots based on book colour (OPEN)
float PARTICLE_STYLE_ELECTRIC = 65;        // White/blue sparks floating towards target
float PARTICLE_STYLE_FLAMES = 70;        // Small Flame Yellow/red embers
float PARTICLE_STYLE_FLAMET = 72;        // Small Torch Yellow/red embers
float PARTICLE_STYLE_FLAMEL = 75;        // Large Flame and lots of particles
float PARTICLE_STYLE_SMOKE = 80;        // DP only effect, velocity driven smoke

float PARTICLE_STYLE_RESPAWN = 100;        // Red/yellow dots spawning from single point
float PARTICLE_STYLE_MEGAH = 105;        // Red dot/heal particles from center
float PARTICLE_STYLE_ARMOR = 110;        // Green/Yellow/Red dots floating up
float PARTICLE_STYLE_KEYSILVER = 120;    // Blue explosion from top of key
float PARTICLE_STYLE_KEYGOLD = 122;        // Yellow explosion from top of key
float PARTICLE_STYLE_KEYRED = 124;        // Red explosion from top of key
float PARTICLE_STYLE_KEYGREEN = 126;    // Green explosion from top of key
float PARTICLE_STYLE_KEYPURPLE = 128;    // Purple explosion from top of key
float PARTICLE_STYLE_KEYWHITE = 130;    // White explosion from top of key
float PARTICLE_STYLE_SIGIL = 140;        // Purple explosion from center of rune
float PARTICLE_STYLE_ALTAR = 145;        // Red/yellow dots raising around altar
float PARTICLE_STYLE_SKILL = 150;        // Burst of red/yellow from skill column
float PARTICLE_STYLE_BSKILL = 152;        // Burst of blue particles
float PARTICLE_STYLE_GSKILL = 154;        // Burst of green particles
float PARTICLE_STYLE_PSKILL = 156;        // Burst of purple particles
float PARTICLE_STYLE_BACKPACK = 175;    // Grey/White dots from center

float PARTICLE_STYLE_SUIT = 200;        // Green dots rising up
float PARTICLE_STYLE_PENT = 210;        // Red/Black dots from center
float PARTICLE_STYLE_SRING = 220;        // Yellow dots falling down
float PARTICLE_STYLE_QUAD = 230;        // Blue dots from center
float PARTICLE_STYLE_TOMEOFP = 240;        // Yellow explosion from skull
float PARTICLE_STYLE_SHARP = 250;        // Purple dots from center
float PARTICLE_STYLE_PIERCE = 260;        // Purple dots from center
float PARTICLE_STYLE_WETSUIT = 270;        // Blue dots rising up

//----------------------------------------------------------------------
// Darkplaces custom particles defined in effectinfo.txt
//void(entity gibent, float gibpoison) DPP_blood_trail;

// DarkPlaces Particles definition block names
// defined in effectinfo.txt (location - root of mod directory)
string DPP_TRBLOOD = "TR_BLOOD";        // Blood TRail for model flag
string DPP_ITSBLOOD = "TR_ITSBLOOD";    // Blood TRail (extra blood)
string DPP_TRSBLOOD = "TR_SBLOOD";        // Stone Blood TRail for model flag
string DPP_TRPBLOOD = "TR_PBLOOD";        // Poison Blood TRail for model flag

string DPP_TRLASER = "TR_LASER";        // Laser particle TRail

string DPP_TEEXPLODE = "TE_EXPLOSION";    // Default particle explosion

string DPP_TRPLASMA = "TR_PLASMA";        // Plasma particle TRail
string DPP_TEPLASMA = "TE_PLASMA";        // Plasma particle explosion
string DPP_TEPLASMABIG = "TE_PLASMABIG";// Plasma Big explosion

string DPP_TRPOISON = "TR_POISON";        // Poison particle TRail
string DPP_TEPOISON = "TE_POISON";        // Poison particle explosion
string DPP_TEPOISONMED = "TE_POISONBIG";// Poison Medium explosion

string DPP_TEBSMOKE = "TE_BSMOKE";        // A burst of Smoke/Steam
string DPP_TEBFLAME = "TE_BFLAME";        // A burst of red flame (smoke)
string DPP_TEBPOISON = "TE_BPOISON";    // A burst of green poison (smoke)

string DPP_PORTALFRONT = "DPP_ITSPORTALFRONT";    // 0,180 angle
string DPP_PORTALSIDE = "DPP_ITSPORTALSIDE";    // 90,270 angle
string DPP_PORTALDOWN = "DPP_ITSPORTALDOWN";    // -2 angle
string DPP_PORTALUP = "DPP_ITSPORTALUP";        // -1 angle
string DPP_FCIRCLE = "DPP_ITSFCIRCLE";
string DPP_JUMPPAD = "DPP_ITSJUMPPAD";
string DPP_BOOKBLUE = "DPP_ITSBOOKBLUE";
string DPP_BOOKRED = "DPP_ITSBOOKRED";
string DPP_BOOKGOLD = "DPP_ITSBOOKGOLD";
string DPP_OPENBOOKBLUE = "DPP_ITSOPENBOOKBLUE";
string DPP_OPENBOOKRED = "DPP_ITSOPENBOOKRED";
string DPP_OPENBOOKGOLD = "DPP_ITSOPENBOOKGOLD";
string DPP_ELECTRIC = "DPP_ITSELECTRIC";
string DPP_FLAMES = "DPP_ITSSFLAME";
string DPP_FLAMET = "DPP_ITSTFLAME";
string DPP_FLAMEL = "DPP_ITSLFLAME";
string DPP_SKILLPILLAR = "DPP_ITSSKILLPILLAR";
string DPP_BSKILLPILLAR = "DPP_ITSBSKILLPILLAR";
string DPP_GSKILLPILLAR = "DPP_ITSGSKILLPILLAR";
string DPP_PSKILLPILLAR = "DPP_ITSPSKILLPILLAR";
string DPP_ALTARRED = "DPP_ITSALTARRED";
string DPP_ALTARGREY = "DPP_ITSALTARGREY";
string DPP_RESPAWN = "DPP_ITSRESPAWN";
string DPP_MEGAH = "DPP_ITSMEGAH";
string DPP_ARMOR1 = "DPP_ITSARMOR1";
string DPP_ARMOR2 = "DPP_ITSARMOR2";
string DPP_ARMOR2BLUE = "DPP_ITSARMOR2BLUE";
string DPP_ARMOR3 = "DPP_ITSARMOR3";
string DPP_WEAPON = "DPP_ITSWEAPON";
string DPP_KEYSILVER = "DPP_ITSSILVERKEY";
string DPP_KEYGOLD = "DPP_ITSGOLDKEY";
string DPP_KEYRED = "DPP_ITSREDKEY";
string DPP_KEYGREEN = "DPP_ITSGREENKEY";
string DPP_KEYPURPLE = "DPP_ITSPURPLEKEY";
string DPP_KEYWHITE = "DPP_ITSWHITEKEY";
string DPP_SIGIL = "DPP_ITSSIGIL";
string DPP_BACKPACK = "DPP_ITSBACKPACK";
string DPP_BACKPACKB = "DPP_ITSBACKPACKB";
string DPP_BACKPACKG = "DPP_ITSBACKPACKG";
string DPP_BACKPACKR = "DPP_ITSBACKPACKR";
string DPP_BACKPACKY = "DPP_ITSBACKPACKY";
string DPP_SUIT = "DPP_ITSSUIT";
string DPP_PENT = "DPP_ITSPENT";
string DPP_SRING = "DPP_ITSSRING";
string DPP_QUAD = "DPP_ITSQUAD";
string DPP_TOMEOFP = "DPP_ITSTOMEOFP";
string DPP_SHARP = "DPP_ITSSHARP";
string DPP_SHARPG = "DPP_ITSSHARPG";
string DPP_PIERCE = "DPP_ITSPIERCE";
string DPP_WETSUIT = "DPP_ITSWETSUIT";

// Special trigger effects
string DPP_TEVORESPIKE = "TE_VORESPIKE";
string DPP_WRAITHEXPLODE = "DPP_ITSWRAITHEXPLODE";
string DPP_INTERACTIVE = "DPP_ITSINTERACTIVE";
string DPP_BURSTFLAME = "DPP_ITSFLAME_BURST";
string DPP_FCIRCLE_RING = "DPP_ITSFCIRCLE_RING";
string DPP_SIGILPICKUP = "DPP_ITSSIGIL_PICKUP";
string DPP_BURSTSHOCKWAVE1 = "DPP_ITSBURSTSHOCKWAVE1";
string DPP_BURSTSHOCKWAVE2 = "DPP_ITSBURSTSHOCKWAVE2";
string DPP_PYROFLAME1 = "DPP_ITSPYROFLAME1";
string DPP_PYROFLAME2 = "DPP_ITSPYROFLAME2";
string DPP_PYROFLAME3 = "DPP_ITSPYROFLAME3";

// Velocity driven smoke
string DPP_VELSMOKEGREY1 = "DPP_ITSVELSMOKEGREY1";
string DPP_VELSMOKEGREY2 = "DPP_ITSVELSMOKEGREY2";
string DPP_VELSMOKEWHITE = "DPP_ITSVELSMOKEWHITE";
string DPP_VELSMOKETOXIC = "DPP_ITSVELSMOKETOXIC";
string DPP_VELSMOKEGREEN = "DPP_ITSVELSMOKEGREEN";
string DPP_VELSMOKEPURPLE = "DPP_ITSVELSMOKEPURPLE";
string DPP_VELSMOKERED = "DPP_ITSVELSMOKERED";
string DPP_VELSMOKEFIRE = "DPP_ITSVELSMOKEFIRE";

// Respawn effects
string DPP_RINGPARTY = "DPP_ITSPART_RINGY";
string DPP_RINGPARTG = "DPP_ITSPART_RINGG";
string DPP_RINGPARTR = "DPP_ITSPART_RINGR";
string DPP_RINGPARTB = "DPP_ITSPART_RINGB";
string DPP_RINGPARTP = "DPP_ITSPART_RINGP";
string DPP_RINGPARTW = "DPP_ITSPART_RINGW";

string DPP_BURSTPARTY = "DPP_ITSPART_BURSTY";
string DPP_BURSTPARTG = "DPP_ITSPART_BURSTG";
string DPP_BURSTPARTR = "DPP_ITSPART_BURSTR";
string DPP_BURSTPARTB = "DPP_ITSPART_BURSTB";
string DPP_BURSTPARTP = "DPP_ITSPART_BURSTP";
string DPP_BURSTPARTW = "DPP_ITSPART_BURSTW";
// Forcefield volumes
string DPP_FFIELDPARTY = "DPP_ITSPART_FFIELDY";
string DPP_FFIELDPARTG = "DPP_ITSPART_FFIELDG";
string DPP_FFIELDPARTR = "DPP_ITSPART_FFIELDR";
string DPP_FFIELDPARTB = "DPP_ITSPART_FFIELDB";
string DPP_FFIELDPARTP = "DPP_ITSPART_FFIELDP";
string DPP_FFIELDPARTW = "DPP_ITSPART_FFIELDW";

//----------------------------------------------------------------------
// GENERIC particle filename strings
string PART_BUBBLE_BLUE = "progs/s_bubble_blue1.spr";
string PART_DOTMED_BLUE = "progs/s_dotmed_blue.spr";
string PART_DOTSML_BLUE = "progs/s_dotsml_blue.spr";
string PART_DOTMED_DBLUE = "progs/s_dotmed_dblue.spr";
string PART_DOTSML_DBLUE = "progs/s_dotsml_dblue.spr";
string PART_BUBBLE_YELLOW = "progs/s_bubble_yell.spr";
string PART_DOTMED_YELLOW = "progs/s_dotmed_yell.spr";
string PART_DOTSML_YELLOW = "progs/s_dotsml_yell.spr";
string PART_DOTSML_GOLD = "progs/s_dotsml_gold.spr";
string PART_BUBBLE_RED = "progs/s_bubble_red1.spr";
string PART_DOTMED_RED = "progs/s_dotmed_red.spr";
string PART_DOTSML_RED = "progs/s_dotsml_red.spr";
string PART_DOTMED_GREEN = "progs/s_dotmed_grn.spr";
string PART_DOTSML_GREEN = "progs/s_dotsml_grn.spr";
string PART_DOTMED_LGREEN = "progs/s_dotmed_lgrn.spr";
string PART_DOTSML_LGREEN = "progs/s_dotsml_lgrn.spr";
string PART_DOTSML_PURP = "progs/s_dotsml_purp.spr";
string PART_DOTMED_PURP = "progs/s_dotmed_purp.spr";
string PART_BUBBLE_WHITE = "progs/s_bubble_wht.spr";
string PART_DOTSML_WHITE = "progs/s_dotsml_wht.spr";
string PART_DOTMED_GREY = "progs/s_dotmed_grey.spr";
string PART_DOTSML_GREY = "progs/s_dotsml_grey.spr";

// SPECIFIC particle filename strings
string PART_TORCH1 = "progs/s_dotmed_tor1.spr";
string PART_TORCH2 = "progs/s_dotmed_tor2.spr";
string PART_BOOKRUNE1 = "progs/s_bookrune1.spr";
string PART_BOOKRUNE2 = "progs/s_bookrune2.spr";

//----------------------------------------------------------------------
// Particle sprites
//----------------------------------------------------------------------
void() setup_particleprecache =
{
    precache_model (PART_TORCH1);            // Embers - Red/Yellow
    precache_model (PART_TORCH2);            // Embers - Red/Yellow
    precache_model (PART_BOOKRUNE1);        // Celtic Book Runes
    precache_model (PART_BOOKRUNE2);        // Celtic Book Runes
    
    precache_model (PART_BUBBLE_BLUE);        // Bubbles - Blue
    precache_model (PART_DOTMED_BLUE);        // Dots - Medium Blue
    precache_model (PART_DOTSML_BLUE);        // Dots - Small Blue
    precache_model (PART_DOTMED_DBLUE);        // Dots - Medium Dark Blue
    precache_model (PART_DOTSML_DBLUE);        // Dots - Small Dark Blue
    precache_model (PART_BUBBLE_YELLOW);    // Bubbles - Yellow
    precache_model (PART_DOTMED_YELLOW);    // Dots - Medium Yellow
    precache_model (PART_DOTSML_YELLOW);    // Dots - Small Yellow
    precache_model (PART_DOTSML_GOLD);        // Dots - Small Gold
    precache_model (PART_BUBBLE_RED);        // Bubbles - Red
    precache_model (PART_DOTMED_RED);        // Dots - Medium Red
    precache_model (PART_DOTSML_RED);        // Dots - Small Red
    precache_model (PART_DOTMED_GREEN);        // Dots - Medium Green
    precache_model (PART_DOTSML_GREEN);        // Dots - Small Green
    precache_model (PART_DOTMED_LGREEN);    // Dots - Medium Light Green
    precache_model (PART_DOTSML_LGREEN);    // Dots - Small Light Green
    precache_model (PART_DOTSML_PURP);        // Dots - Purple/Pink
    precache_model (PART_DOTMED_PURP);        // Dots - Purple/Pink
    precache_model (PART_BUBBLE_WHITE);        // Bubbles - White
    precache_model (PART_DOTSML_WHITE);        // Dots - Small White
    precache_model (PART_DOTMED_GREY);        // Dots - Medium Grey
    precache_model (PART_DOTSML_GREY);        // Dots - Small Grey
};
