/*======================================================================
 AI GIB FUNCTIONS
======================================================================*/
.float        gibondeath;            // true/false always gib on death
.vector        gib_ofs;            // Vertical gib origin offset (override)

.string        gib1mdl;            // Gib 1 unique weapon/body part
.string        gib2mdl;            // Gib 2 unique weapon/body part
.string        gib3mdl;            // Gib 3 unique weapon/body part
.string        gib4mdl;            // Gib 4 unique weapon/body part
.string        gib5mdl;            // Gib 5 unique weapon/body part
.float        gib1skin;            // Special gib model unique skin no
.float        gib2skin;            // Special gib model unique skin no
.float        gib3skin;            // Special gib model unique skin no
.float        gib4skin;            // Special gib model unique skin no
.float        gib5skin;            // Special gib model unique skin no
.float        gib1frame;            // Frame range to randomly pick from
.float        gib2frame;            // Frame range to randomly pick from
.float        gib3frame;            // Frame range to randomly pick from
.float        gib1sound;            // Gib 1 - impact sound type
.float        gib2sound;            // Gib 2 - impact sound type
.float        gib3sound;            // Gib 3 - impact sound type
.vector        gib1origin;            // Gib 1 - Exact origin to spawn
.vector        gib2origin;            // Gib 2 - Exact origin to spawn
.vector        gib3origin;            // Gib 3 - Exact origin to spawn
.string        gib1soundx;            // Gib custom impact sound
.string        gib2soundx;            // Gib custom impact sound
.string        gib3soundx;            // Gib custom impact sound
.float        gibtype;            // What type of gibs (blood, stone, etc)
.float        blockudeath;        // ID udeath.wav sounds human, block it for non-humanoids
.float        gib1dmg;            // Special damage from touching
.float        gib1exp;            // Special damage particle explosion
.float        gibAnimTimer;        // Starting time for animation
.float        gibAnimstart;        // Starting frame no for animation
.float        gibAnimfinal;        // Final frame no for animation

.float        gibpartbase;        // Base quantity of particle from gibs
.float        gibpartrnd;            // Random quantity of particles from gibs
.float        gibpartlife;        // Lifetime of particles from gibs
.float        gibpartchance;        // The % percentage chance of particles
.float        gibpartstyle;        // Particle style

float GIB_IMPACT_LIGHT = 0;        // Light fleshy impact (default)
float GIB_IMPACT_HEAVY = 1;        // Heavy fleshy impact
float GIB_IMPACT_ACID = 5;        // Fleshy acid burning sound (dangerous)
float GIB_IMPACT_STONE = 10;    // Light stones dropping
float GIB_IMPACT_STONE2 = 11;    // Dropping pebbles
float GIB_IMPACT_STONE3 = 12;    // Dropping pebbles
float GIB_IMPACT_WOOD = 20;        // Heavy crossbow/chainsaw
float GIB_IMPACT_METALA = 40;    // Short metal impact
float GIB_IMPACT_METALB = 41;    // Long metal impact
float GIB_IMPACT_CHAIN = 42;    // Blue metal flail

float GIBTYPE_BLOOD = 0;        // Blood (default)
float GIBTYPE_STONE = 1;        // Stone/rubble
float GIBTYPE_WOOD = 2;            // Woodchip
float GIBTYPE_GLASS = 3;        // Glass impact sounds
float GIBTYPE_METAL = 4;        // Metal scraps
float GIBTYPE_POISON = 10;        // Poisoned flesh

float MON_BCOLOR_RED = 68;        // Red index colour
float MON_BCOLOR_GREEN = 52;    // Green index
float MON_BCOLOR_YELLOW = 192;    // Bright yellow, not gold
float MON_BCOLOR_SILVER = 36;    // Silver/Ice
float MON_BCOLOR_WHITE = 4;        // Grey/white

string GIB_SOUND_HEAVY = "gibs/gibheavy1.wav";
string GIB_SOUND_HEAVY2 = "gibs/gibheavy2.wav";
string GIB_SOUND_METALA = "gibs/gibmetala.wav";
string GIB_SOUND_METALB = "gibs/gibmetalb.wav";
string GIB_SOUND_CHAIN = "gibs/gibchain.wav";
string GIB_SOUND_WOOD = "gibs/gibwood.wav";
string GIB_SOUND_UDEATH = "player/udeath.wav";
string GIB_SOUND_ACID = "gibs/gibacid.wav";
string GIB_SOUND_SDEATH = "gibs/stonedeath.wav";

string GIB1_BLOOD = "progs/gib_1.mdl";
string GIB2_BLOOD = "progs/gib_2.mdl";
string GIB3_BLOOD = "progs/gib_3.mdl";
string GIB4_BLOOD = "progs/gib_4.mdl";
string GIB5_BLOOD = "progs/gib_5.mdl";

string GIB1_STONE = "progs/gib_s1.mdl";
string GIB2_STONE = "progs/gib_s2.mdl";
string GIB3_STONE = "progs/gib_s3.mdl";
string GIB4_STONE = "progs/gib_s4.mdl";
string GIB5_STONE = "progs/gib_s5.mdl";

string GIB1_POISON = "progs/gib_p1.mdl";
string GIB2_POISON = "progs/gib_p2.mdl";
string GIB3_POISON = "progs/gib_p3.mdl";
string GIB4_POISON = "progs/gib_p4.mdl";
string GIB5_POISON = "progs/gib_p5.mdl";

string GIB1_SOUND = "gibs/gib1.wav";
string GIB3_SOUND = "gibs/gib3.wav";
string GIB5_SOUND = "gibs/gib5.wav";
string GIBHIT1_SOUND = "gibs/gibhit1.wav";
string GIBHIT2_SOUND = "gibs/gibhit2.wav";
string GIBHIT3_SOUND = "gibs/gibhit3.wav";
string GIBHIT4_SOUND = "gibs/gibhit4.wav";
string GIBHIT5_SOUND = "gibs/gibhit5.wav";
string GIBHIT1_STONESOUND = "gibs/gibstone1.wav";
string GIBHIT3_STONESOUND = "gibs/gibstone3.wav";
string GIBHIT5_STONESOUND = "gibs/gibstone5.wav";
string GIBHIT1_METALSOUND = "gibs/gibmetal1.wav";
string GIBHIT3_METALSOUND = "gibs/gibmetal3.wav";
string GIBHIT5_METALSOUND = "gibs/gibmetal5.wav";

float GIB_FADETIME = 10;

/*======================================================================
 precache_stonegibs, precache_poisongibs
  Only load extra gibs when there is a monster requiring them
  There are separate gib models because of particle trail colours
  These functions have to be called from monster spawn functions
  The gib system tests for the global variables defined below
  If the extra gibs are not active then default blood vers are used
======================================================================*/
void() precache_stonegibs =
{
    gibstone = TRUE;
    precache_model (GIB1_STONE);            // New gib - arm/leg
    precache_model (GIB2_STONE);            // New gib - body
    precache_model (GIB3_STONE);            // New gib - slice
    precache_model (GIB4_STONE);            // New gib - small chunk/ball
    precache_model (GIB5_STONE);            // New gib - large chunk/ball
};

//----------------------------------------------------------------------
void() precache_poisongibs =
{
    gibpoison = TRUE;
    precache_model (GIB1_POISON);            // New gib - arm/leg
    precache_model (GIB2_POISON);            // New gib - body
    precache_model (GIB3_POISON);            // New gib - slice
    precache_model (GIB4_POISON);            // New gib - small chunk/ball
    precache_model (GIB5_POISON);            // New gib - large chunk/ball
};

//----------------------------------------------------------------------
void(entity gibent) DPP_blood_trail =
{
    // Cannot add particle trails to the player!
    if (gibent.flags & FL_CLIENT) {
        dprint("\b[GIB]\b Cannot add particle trail to player!\n");
        return;
    }

    // Stone = dark black dust on the floor
    if (gibent.gibtype == GIBTYPE_STONE) {
        gibent.traileffectnum = particleeffectnum(DPP_TRSBLOOD);
    }
    // Glowing green blobs splashed about
    else if (gibent.gibtype == GIBTYPE_POISON) {
        gibent.traileffectnum = particleeffectnum(DPP_TRPBLOOD);
    }
    else {
        // DISABLED - Excessive blood trails can be bad for large maps
        // Typical blood stains (20% chance of more blood)
        //if (random() < 0.2) gibent.traileffectnum = particleeffectnum(DPP_ITSBLOOD);
        //else gibent.traileffectnum = particleeffectnum(DPP_TRBLOOD);
        gibent.traileffectnum = particleeffectnum(DPP_TRBLOOD);
    }
};

/*======================================================================
 DirectionForDamage
  Takes the angle between self and enemy and projects backwards
======================================================================*/
vector(float gibforce) DirectionForDamage =
{
    local vector gvel;

    // Gentle fountain, not much upward motion
    if (self.max_health == MON_GIBFOUNTAIN) {
        gvel_x = crandom()*100;
        gvel_y = crandom()*100;
        gvel_z = 100 + (100 * random());
        return gvel;
    }
    // Party popper version (large explosion)
    else if (self.max_health == MON_GIBEXPLOSION) {
        gvel_x = crandom()*250;
        gvel_y = crandom()*250;
        gvel_z = 400 + (100 * random());
        return gvel;
    }
    else if (self.max_health == MON_XYGIBVELOCITY) {
        // Back to front vector so gib will travel away from enemy
        gvel = vectoangles(self.origin - self.enemy.origin);
        makevectors(gvel);
        gvel = v_forward * (50 + (100 * random()));
        gvel = gvel + (v_right * (200 * crandom()));
        gvel_z = 100 + (100 * random());
    }
    else if (self.max_health == MON_NOGIBVELOCITY) {
        // Back to front vector so gib will travel away from enemy
        gvel = vectoangles(self.origin - self.enemy.origin);
        makevectors(gvel);
        gvel = v_forward * (50 + (100 * random()));
        gvel = gvel + (v_right * (100 * crandom()));
        gvel_z = 30;
    }
    else {
        // Back to front vector so gib will travel away from enemy
        gvel = vectoangles(self.origin - self.enemy.origin);
        makevectors(gvel);
        gvel = v_forward * (50 + (100 * random()));
        gvel = gvel + (v_right * (100 * crandom()));
        gvel_z = 100 + (100 * random());
    }

    if (gibforce > -50) gvel = gvel * 1.25;
    return gvel;
};

//======================================================================
void() RemoveGib =
{
    // Stop any touch functions
    self.waitmin = LARGE_TIMER;
    // Fade away model
    self.think = model_fade;
    self.nextthink = time + 0.1;
    self.ltime = self.nextthink;
};

//----------------------------------------------------------------------
void() CheckFloorGib =
{
    // Is it time for the gib to fade away?
    if (self.pain_finished > time) {
        // Check for particles from gib
        if (self.gibpartstyle > 0) {
            if (random() < self.gibpartchance) {
                particle_explode(self.origin, self.gibpartbase+random()*self.gibpartrnd, self.gibpartlife+random(), self.gibpartstyle, 0);
            }
        }
        // Check floor below gib (global function)
        // Origin at base of model + 16 (typical step height)
        ent_floorcheck(self, FLOOR_TRACE_GIBS);

        // Check for final impact animation
        if (self.gibAnimfinal > 0 && self.gibAnimTimer < time) {
            if (self.walkframe > self.gibAnimfinal) {
                self.gibAnimfinal = 0;
                RemoveGib();
                return;
            }
            else {
                self.frame = self.gibAnimstart + self.walkframe;
                self.walkframe = self.walkframe + 1;
            }
        }

        // Keep checking
        self.think = CheckFloorGib;
        self.nextthink = time + 0.1;
    }
    else RemoveGib();
};

//----------------------------------------------------------------------
void() TouchGib =
{
    // Always check for sky and void functions first
    if (self.touchedvoid) return;                // Marked for removal

    //if (check_skycontent(self.origin)) {entity_remove(self, 0.1); return;}
    // Disabled - will cause extreme slowdowns with DP/FTE engines
    // This function will check the whole surface list which can be
    // crazy for extremely large maps

    // Check if the gib can damage (only players) on touch
    if (self.gib1dmg > 0 && other.flags & FL_CLIENT) {
        T_Damage(other, self, self, self.gib1dmg, DAMARMOR);
        self.gib1dmg = 0;
        SpawnExplosion(self.gib1exp,self.origin,"");
        entity_remove(self, 0.1);
        return;
    }

    // Check rest of touch restrictions
    if (self.waitmin > time) return;            // Touch function disabled
    if (other == self.owner) return;            // Ignore original monster
    if (other.classgroup == CG_TEMPENT) return;    // Ignore other temp entities

    // Block multiple touch of impact sound
    self.waitmin = time + 2 + random() * 2;

    // Is there any time left to setup floor checks
    if (self.pain_finished > time + 0.1) {
        self.think = CheckFloorGib;
        self.nextthink = time + 0.1;
    }

    // stop gibs constantly touching moving func bmodels
    // let the impact sound work once and then stop
    if (other.classgroup == CG_FUNCMOVER) {
        if (self.waitmin2 > time) return;
        self.waitmin2 = LARGE_TIMER;
    }

    //----------------------------------------------------------------------
    // Only need to pick the impact sound once
    // No point to keep randomly picking
    //----------------------------------------------------------------------
    if (self.noise == "") {
        // Setup defaults for gib impact sound
        self.volume = ATTN_GIB;
        self.noise = SOUND_EMPTY;

        // Check first for unique impact sound
        if (self.weapon > 0) {
            self.volume = ATTN_NORM;
            if (self.weapon == GIB_IMPACT_HEAVY) self.noise = GIB_SOUND_HEAVY;
            else if (self.weapon == GIB_IMPACT_METALA) self.noise = GIB_SOUND_METALA;
            else if (self.weapon == GIB_IMPACT_METALB) self.noise = GIB_SOUND_METALB;
            else if (self.weapon == GIB_IMPACT_CHAIN) self.noise = GIB_SOUND_CHAIN;
            else if (self.weapon == GIB_IMPACT_WOOD) self.noise = GIB_SOUND_WOOD;
            else if (self.weapon == GIB_IMPACT_ACID) self.noise = GIB_SOUND_ACID;
        }
        // Check default gib impact types (stone, metal, blood)
        else {
            if (self.gibtype == GIBTYPE_STONE) {
                self.lip = random() * 3;
                if (self.lip < 1) self.noise = GIBHIT1_STONESOUND;
                else if (self.lip < 2) self.noise = GIBHIT3_STONESOUND;
                else self.noise = GIBHIT5_STONESOUND;
            }
            else if (self.gibtype == GIBTYPE_METAL) {
                self.lip = random() * 3;
                if (self.lip < 1) self.noise = GIBHIT1_METALSOUND;
                else if (self.lip < 2) self.noise = GIBHIT3_METALSOUND;
                else self.noise = GIBHIT5_METALSOUND;
            }
            else {
                self.lip = random() * 5;
                if (self.lip < 1) self.noise = GIBHIT1_SOUND;
                else if (self.lip < 2) self.noise = GIBHIT2_SOUND;
                else if (self.lip < 3) self.noise = GIBHIT3_SOUND;
                else if (self.lip < 4) self.noise = GIBHIT4_SOUND;
                else self.noise = GIBHIT5_SOUND;
            }
        }
    }

    // Has any impact sounds been defined?
    if (self.noise != "") sound(self, CHAN_VOICE, self.noise, 1, self.volume);
};

//======================================================================
// ThrowGib was originally in player.qc (moved here instead)
// Spawns a gib model and throws it up into the air to bounce around
//======================================================================
// TODO VR: (P2) (AD) - this is called just ThrowGib in AD
void(float gib_no, float gib_qty) ThrowGib_AD =
{

    while (gib_qty > 0) {
        gibitem = spawn();
        gibitem.owner = self;
        gibitem.classname = "item_gib";
        gibitem.classtype = CT_TEMPGIB;
        gibitem.classgroup = CG_TEMPENT;
        gibitem.frame = gibitem.skin = 0;

        // Check for any touch/contact damage / explosions
        if (self.gib1dmg > 0) gibitem.gib1dmg = self.gib1dmg;
        if (self.gib1exp > 0) gibitem.gib1exp = self.gib1exp;

        // Setup gib type (particle trail)
        if (self.gibtype) gibitem.gibtype = self.gibtype;

        // Create extra blood particles for DP engine
        if (ext_dppart) DPP_blood_trail(gibitem);

        // Arm/leg joint (small)
        if (gib_no == 1) {
            if (self.gibtype == GIBTYPE_STONE && gibstone) gibitem.mdl = GIB1_STONE;
            else if (self.gibtype == GIBTYPE_POISON && gibpoison) gibitem.mdl = GIB1_POISON;
            else gibitem.mdl = GIB1_BLOOD;
        }
        // Torso (large lump)
        else if (gib_no == 2) {
            if (self.gibtype == GIBTYPE_STONE && gibstone) gibitem.mdl = GIB2_STONE;
            else if (self.gibtype == GIBTYPE_POISON && gibpoison) gibitem.mdl = GIB2_POISON;
            else gibitem.mdl = GIB2_BLOOD;
        }
        // Slice (large and wedge like)
        else if (gib_no == 3) {
            if (self.gibtype == GIBTYPE_STONE && gibstone) gibitem.mdl = GIB3_STONE;
            else if (self.gibtype == GIBTYPE_POISON && gibpoison) gibitem.mdl = GIB3_POISON;
            else gibitem.mdl = GIB3_BLOOD;
        }
        // Blob (small and round)
        else if (gib_no == 4) {
            if (self.gibtype == GIBTYPE_STONE && gibstone) gibitem.mdl = GIB4_STONE;
            else if (self.gibtype == GIBTYPE_POISON && gibpoison) gibitem.mdl = GIB4_POISON;
            else gibitem.mdl = GIB4_BLOOD;
        }
        // Blob (large and round)
        else if (gib_no == 5) {
            if (self.gibtype == GIBTYPE_STONE && gibstone) gibitem.mdl = GIB5_STONE;
            else if (self.gibtype == GIBTYPE_POISON && gibpoison) gibitem.mdl = GIB5_POISON;
            else gibitem.mdl = GIB5_BLOOD;
        }
        // Custom Gib (defined by monster)
        else if (gib_no == 11) {
            // Check for custom model, skin and frame
            gibitem.mdl = self.gib1mdl;
            gibitem.weapon = self.gib1sound;
            if (self.gib1skin > 0) gibitem.skin = self.gib1skin;
            if (self.gib1frame) gibitem.frame = rint(random()*self.gib1frame);
            if (self.gib1origin) gibitem.oldorigin = self.gib1origin;

            // Check for any particles setup on gib
            if (self.gibpartstyle > 0) {
                if (self.gibpartbase > 0) gibitem.gibpartbase = self.gibpartbase;
                else gibitem.gibpartbase = 1;
                if (self.gibpartrnd > 0) gibitem.gibpartrnd = self.gibpartrnd;
                else gibitem.gibpartrnd = 1;
                if (self.gibpartlife > 0) gibitem.gibpartlife = self.gibpartlife;
                else gibitem.gibpartlife = 0.5;
                if (self.gibpartchance > 0) gibitem.gibpartchance = self.gibpartchance;
                else gibitem.gibpartchance = 0.5;
                gibitem.gibpartstyle = self.gibpartstyle;
            }
        }
        // Custom Gib (defined by monster)
        else if (gib_no == 12) {
            // Check for custom model, skin and frame
            gibitem.mdl = self.gib2mdl;
            gibitem.weapon = self.gib2sound;
            if (self.gib2skin > 0) gibitem.skin = self.gib2skin;
            if (self.gib2frame) gibitem.frame = rint(random()*self.gib2frame);
            if (self.gib2origin) gibitem.oldorigin = self.gib2origin;
        }
        // Custom Gib (defined by monster)
        else if (gib_no == 13) {
            // Check for custom model, skin and frame
            gibitem.mdl = self.gib3mdl;
            gibitem.weapon = self.gib3sound;
            if (self.gib3skin > 0) gibitem.skin = self.gib3skin;
            if (self.gib3frame) gibitem.frame = rint(random()*self.gib3frame);
            if (self.gib3origin) gibitem.oldorigin = self.gib3origin;
        }
        // Extra Custom Gib (just models, minimal paramters)
        else if (gib_no == 14) {
            gibitem.mdl = self.gib4mdl;
            if (self.gib4skin > 0) gibitem.skin = self.gib4skin;
        }
        // Extra Custom Gib (just models, minimal paramters)
        else if (gib_no == 15) {
            gibitem.mdl = self.gib5mdl;
            if (self.gib5skin > 0) gibitem.skin = self.gib5skin;
        }
        // Special head gib (based on headmdl and skin no)
        else if (gib_no == 25) {
            gibitem.mdl = self.headmdl;
            gibitem.skin = self.skin;
        }

        // Check for any animations on impact
        if (self.gibAnimfinal > 0) {
            gibitem.walkframe = 0;
            gibitem.gibAnimstart = self.gibAnimstart;
            gibitem.gibAnimfinal = self.gibAnimfinal;
            gibitem.gibAnimTimer = time + self.gibAnimTimer + random()*(self.gibAnimTimer*2);
        }

        // Bounce like a grenade have trigger/touch impact
        gibitem.movetype = MOVETYPE_BOUNCE;
        gibitem.solid = SOLID_TRIGGER;

        setmodel (gibitem, gibitem.mdl);
        setsize (gibitem, VEC_ORIGIN, VEC_ORIGIN);

        // 10 frames of different pose/sizes for regular gibs
        if (gib_no < 10) gibitem.frame = rint(random()*9);

        // Special head gib (plenty of upward motion)
        if (gib_no == 25) {
            gibitem.oldorigin = self.origin;
            setorigin(gibitem, gibitem.oldorigin);

            gibitem.velocity_x = 50 * crandom();
            gibitem.velocity_y = 50 * crandom();
            gibitem.velocity_z = 200 + 100 * random();

            gibitem.avelocity = '0 0 0';
            gibitem.avelocity_y = 200 + random() * 200;
            gibitem.angles_y = gibitem.ideal_yaw = random() * 360;
        }
        else {
            //----------------------------------------------------------------------
            // Spawn gib inside of owner model volume instead of from a single point
            // Not used anymore, does not cope very well with larger bound boxes
            //gibitem.oldorigin_x = self.absmin_x + random()*(self.maxs_x - self.mins_x);
            //gibitem.oldorigin_y = self.absmin_y + random()*(self.maxs_y - self.mins_y);
            //gibitem.oldorigin_z = 16 + (random()*12) + self.origin_z;

            //----------------------------------------------------------------------
            // This version copes with taller enemies by using all of the bounding box
            if (CheckZeroVector(gibitem.oldorigin)) {
                gibitem.oldorigin_x = crandom()*(self.bbmaxs_x*0.75);
                gibitem.oldorigin_y = crandom()*(self.bbmaxs_y*0.75);
                gibitem.oldorigin_z = (random()*self.bbmaxs_z)-12;
                gibitem.oldorigin = gibitem.oldorigin + self.origin + self.gib_ofs;
            }
            setorigin(gibitem, gibitem.oldorigin);

            //----------------------------------------------------------------------
            // NEW Directional gib velocity with modifier
            gibitem.velocity = DirectionForDamage(self.health);
            gibitem.angles_y = gibitem.ideal_yaw = random() * 360;
            if (gib_no < 10) gibitem.avelocity = vecrand(100,200,FALSE);
            else gibitem.avelocity = crandom() * '0 300 0';
        }

        //----------------------------------------------------------------------
        // Check for water content, adjust for water friction
        if (pointcontents(gibitem.oldorigin) == CONTENT_WATER) {
            gibitem.gravity = 0.35;
            gibitem.velocity_z = 0;
        }

        gibitem.touch = TouchGib;
        gibitem.nextthink = time + GIB_FADETIME + random() * GIB_FADETIME;
        gibitem.pain_finished = gibitem.nextthink;
        gibitem.think = RemoveGib;

        gib_qty = gib_qty - 1;
    }
};

//======================================================================
// Explode monster into a shower of gibs (throw head as well)
//======================================================================
void() monster_ThrowGib =
{
    self.gibbed = TRUE;        // Monster has offical gone to pieces ;)

    //----------------------------------------------------------------------
    // New set of gib sounds - gib1, gib3, gib5
    // FIXME - Sadly the source for these files is UT :(
    // Zombies have special gib sound because they always go that way!
    // Stone enemies are just a pile of rubble and dust!
    //----------------------------------------------------------------------
    if (self.classgroup == CG_ZOMBIE)
        sound (self, CHAN_VOICE, "zombie/z_gib.wav", 1, ATTN_NORM);
    else if (self.gibtype == GIBTYPE_STONE)
        sound(self, CHAN_VOICE, GIB_SOUND_SDEATH, 1, ATTN_NORM);
    else {
        self.lip = random() * 4;
        if (self.lip < 1) sound (self, CHAN_VOICE, GIB1_SOUND, 1, ATTN_NORM);
        else if (self.lip < 2) sound (self, CHAN_VOICE, GIB3_SOUND, 1, ATTN_NORM);
        else {
            // Is the monster humanoid? (udeath is the original id gib sound)
            if (self.blockudeath) sound (self, CHAN_VOICE, GIB5_SOUND, 1, ATTN_NORM);
            else sound (self, CHAN_VOICE, GIB_SOUND_UDEATH, 1, ATTN_NORM);
        }
    }

    // Some monsters have special death setups
    //----------------------------------------------------------------------
    if (self.th_gibdie) { self.th_gibdie(); return; }

    //----------------------------------------------------------------------
    // If any special gib models defined, throw them
    //----------------------------------------------------------------------
    if (self.gib1mdl != "") ThrowGib_AD(11, 1);
    if (self.gib2mdl != "") ThrowGib_AD(12, 1);
    if (self.gib3mdl != "") ThrowGib_AD(13, 1);
    if (self.gib4mdl != "") ThrowGib_AD(14, 1);

    //----------------------------------------------------------------------
    // Work through each monster type (default = humanoid)
    // GIB 1 - arm/leg (small)
    // GIB 2 - torso (large)
    // GIB 3 - slice (medium)
    // GIB 4 - blob (small)
    // GIB 5 - blob (medium)
    //----------------------------------------------------------------------
    if (self.classtype == CT_MONDEMON) {
        ThrowGib_AD(4, 1 + rint(random()*2));
        ThrowGib_AD(5, 3 + rint(random()*2));
    }
    else if (self.classtype == CT_MONDROLE) {
        ThrowGib_AD(4, 2 + rint(random()*2));
        ThrowGib_AD(5, 4 + rint(random()*2));
    }
    else if (self.classtype ==     CT_MONDOG) {
        ThrowGib_AD(4, 1 + rint(random()*3));
        ThrowGib_AD(5, 1 + rint(random()*2));
    }
    else if (self.classtype == CT_MONWRAITH) {
        ThrowGib_AD(4, 3 + rint(random()*2));
    }
    else if (self.classtype == CT_MONSHAL) {
        ThrowGib_AD(4, 2 + rint(random()*4));
        ThrowGib_AD(5, 2 + rint(random()*2));
    }
    else if (self.classtype == CT_MONGOLEM) {
        ThrowGib_AD(11, 4 + rint(random()*2));
        ThrowGib_AD(12, 8 + rint(random()*2));
    }
    else if (self.classtype == CT_MONSCORPION) {
        ThrowGib_AD(4, 2 + rint(random()*4));
        ThrowGib_AD(5, 2 + rint(random()*2));
    }
    // Includes brown/green spiders and vorelings
    else if (self.classgroup == CG_SPIDER) {
        ThrowGib_AD(4, rint(random()*3));
        ThrowGib_AD(11, rint(random()*2));
    }
    else if (self.classtype ==     CT_MONELF) {
        ThrowGib_AD(4, 3 + rint(random()*3));
    }
    else if (self.classtype == CT_MONSANTA) {
        ThrowGib_AD(3, 1);
        ThrowGib_AD(4, 8 + rint(random()*4));
        ThrowGib_AD(5, 4 + rint(random()*2));
        ThrowGib_AD(13, 8 + rint(random()*4));
    }
    else if (self.classgroup == CG_FISH) {
        ThrowGib_AD(4, 3 + rint(random()*3));
        ThrowGib_AD(5, 1);
    }
    else if (self.classtype == CT_MONBOIL) {
        ThrowGib_AD(3, 1 + rint(random()*2));
        ThrowGib_AD(4, 4 + rint(random()*2));
        ThrowGib_AD(4, 5 + rint(random()*3));
    }
    else if (self.classgroup == CG_ZOMBIE) {
        ThrowGib_AD(4, 3 + rint(random()*3));
        ThrowGib_AD(5, 1 + rint(random()*2));
    }
    // Includes vanilla and new hunter ogres
    else if (self.classgroup == CG_OGRE) {
        ThrowGib_AD(4, 1 + rint(random()*3));
        ThrowGib_AD(5, 2 + rint(random()*2));
    }
    else if (self.classgroup == CG_DCAPTAIN) {
        ThrowGib_AD(3, 1);
        ThrowGib_AD(4, 2 + rint(random()*4));
        ThrowGib_AD(5, 2 + rint(random()*2));
    }
    else if (self.classtype == CT_MONGARGOYLE) {
        ThrowGib_AD(4, 2 + rint(random()*3));
        ThrowGib_AD(5, 1 + rint(random()*2));
    }
    else if (self.classtype == CT_MONGAUNT) {
        ThrowGib_AD(4, 3 + rint(random()*2));
        ThrowGib_AD(5, 2 + rint(random()*2));
    }
    else if (self.classtype == CT_MONWIZARD) {
        ThrowGib_AD(4, 3 + rint(random()*3));
    }
    else if (self.classtype == CT_MONSHAM) {
        ThrowGib_AD(3, 1);
        ThrowGib_AD(4, 2 + rint(random()*4));
        ThrowGib_AD(5, 2 + rint(random()*4));
    }
    else if (self.classtype ==     CT_MONBOGLORD) {
        ThrowGib_AD(4, 5 + rint(random()*5));
        ThrowGib_AD(5, 10 + rint(random()*10));
    }
    else if (self.classtype ==     CT_MONMINOTAUR) {
        ThrowGib_AD(3, 1);
        ThrowGib_AD(4, 2 + rint(random()*4));
        ThrowGib_AD(5, 2 + rint(random()*4));
    }
    // Default gib setup
    else {
        // Is humanoid sound blocked?
        if (self.blockudeath) {
            // Throw large slice gib
            ThrowGib_AD(3, rint(random()*2));
        }
        // Throw humanoid torso gib
        else if (random()<0.5) ThrowGib_AD(2, 1);
        // Regular gibs (4=small, 5=large)
        ThrowGib_AD(4, 3 + rint(random()*3));
        ThrowGib_AD(5, 1 + rint(random()*1));
    }

    //----------------------------------------------------------------------
    // Throw head last because all previous gibs rely on the origin
    //----------------------------------------------------------------------
    if (self.headmdl != "") {
        // If monster set for low velocity throw extra gibs instead
        if (self.max_health == MON_NOGIBVELOCITY)
            ThrowGib_AD(4, rint(random()*4));
        // Throw a head gib instead of switching to one
        else ThrowGib_AD(25, 1);
    }
    // Finally hide monster
    entity_hide (self);
};

//======================================================================
// Check health for gib condition
//======================================================================
void() monster_check_gib =
{
    // There are always exceptions to the every rule!
    // Spawns=Explode, Wraith=Explode, Jim=Explode
    // SkullWiz=Fade away, Lost Souls=Explode
    if (self.gibhealth == MON_NEVERGIB) return;
    if (self.touchedvoid) { entity_hide(self); return; }

    // Can only gib a monster once!
    if (self.gibbed) return;

    // Make sure gibondeath has gib velocity (health value)
    if (self.gibondeath) self.health = self.gibhealth;
    // Is it time to throw a gib party?
    if ( self.health < (self.gibhealth + 1) || self.gibondeath)
        monster_ThrowGib_AD();
};

//======================================================================
// Forward compiler reference
void(entity targ, entity attacker) Killed;

//======================================================================
// Gib a body on the floor (special setup)
//======================================================================
void(entity onflr, float impactdmg) monster_flrbody_gib =
{
    local entity tself, tother;

    onflr.bodyonflr = string_null;    // no more body gibbing
    tself = self; tother = other;    // save self/other
    self = onflr; other = tself;    // switch around
    if (onflr.classgroup == CG_ZOMBIE) {
        self.health = 0;
        Killed(onflr, tself);
    }
    else {
        self.max_health = MON_NOGIBVELOCITY; // use minimal velocity
        monster_ThrowGib_AD();            // throw parts
    }

    self = tself; other = tother;    // switch back
    SpawnBlood_AD (onflr, onflr.origin, '0 0 50', impactdmg*4);
    if (onflr.bodyonflrtrig) trigger_strs(onflr.bodyonflrtrig,self);
    // Hide/Remove original body/shadow
    entity_hide(onflr);
};
