/*======================================================================
 Story books
======================================================================*/
// Closed
$frame closed1 closed2 closed3 closed4 closed5 closed6 closed7 closed8 closed9 closed10

// Idle B - looking around turning left and then right (based on closed)
$frame idleb1 idleb2 idleb3 idleb4 idleb5 idleb6 idleb7 idleb8 idleb9 idleb10
$frame idleb11 idleb12 idleb13 idleb14 idleb15 idleb16 idleb17 idleb18 idleb19 idleb20

// Idle C - spin around 360 degrees (based on closed)
$frame idlec1 idlec2 idlec3 idlec4 idlec5 idlec6 idlec7 idlec8 idlec9 idlec10
$frame idlec11 idlec12 idlec13 idlec14 idlec15 idlec16 idlec17 idlec18 idlec19 idlec20

// Opening (closed -> open)
$frame opening1 opening2 opening3 opening4 opening5 opening6 opening7 opening8 opening9 opening10

// Open
$frame open1 open2 open3 open4 open5 open6 open7 open8 open9 open10

void() book_closed1;
void(float fadedir) info_book_fadesetup;

//----------------------------------------------------------------------
float MISCBOOK_NOMODEL = 1;        // Will not use any model and particles
float MISCBOOK_PLINTH1 = 2;        // setup and spawn on plinth1
float MISCBOOK_PLINTH2 = 4;        // setup and spawn on plinth2
float MISCBOOK_STORY = 8;        // setup book as part of story
float MISCBOOK_COLLISION = 16;    // Use bounding box collision for plinth
float MISCBOOK_ANGLEONLY = 32;  // Will only work when standing infront of entity
float MISCBOOK_NOEFFECTS = 128;    // Disable particles and effects

float MISCBOOK_FADEOUT = 192;    // Background density while reading books
float MISCBOOK_LOOPADVANCE = 0.01;

float MISCBOOK_CLOSED = 1;
float MISCBOOK_OPENING = 2;
float MISCBOOK_CLOSING = 4;
float MISCBOOK_OPEN = 8;
float MISCBOOK_STAYOPEN = 16;

//----------------------------------------------------------------------
// Idle B - looking around turning left and then right (based on closed)
//----------------------------------------------------------------------
void()    book_idleb1 =    [$idleb1, book_idleb2 ] {};
void()    book_idleb2 =    [$idleb2, book_idleb3 ] {};
void()    book_idleb3 =    [$idleb3, book_idleb4 ] {};
void()    book_idleb4 =    [$idleb4, book_idleb5 ] {};
void()    book_idleb5 =    [$idleb5, book_idleb6 ] {};
void()    book_idleb6 =    [$idleb6, book_idleb7 ] {};
void()    book_idleb7 =    [$idleb7, book_idleb8 ] {};
void()    book_idleb8 =    [$idleb8, book_idleb9 ] {};
void()    book_idleb9 =    [$idleb9, book_idleb10 ] {};
void()    book_idleb10 = [$idleb10, book_idleb11 ] {};
void()    book_idleb11 = [$idleb11, book_idleb12 ] {};
void()    book_idleb12 = [$idleb12, book_idleb13 ] {};
void()    book_idleb13 = [$idleb13, book_idleb14 ] {};
void()    book_idleb14 = [$idleb14, book_idleb15 ] {};
void()    book_idleb15 = [$idleb15, book_idleb16 ] {};
void()    book_idleb16 = [$idleb16, book_idleb17 ] {};
void()    book_idleb17 = [$idleb17, book_idleb18 ] {};
void()    book_idleb18 = [$idleb18, book_idleb19 ] {};
void()    book_idleb19 = [$idleb19, book_idleb20 ] {};
void()    book_idleb20 = [$idleb20, book_closed1 ] {};

//----------------------------------------------------------------------
// Idle C - spin around 360 degrees (based on closed)
//----------------------------------------------------------------------
void()    book_idlec1 =    [$idlec1, book_idlec2 ] {};
void()    book_idlec2 =    [$idlec2, book_idlec3 ] {};
void()    book_idlec3 =    [$idlec3, book_idlec4 ] {};
void()    book_idlec4 =    [$idlec4, book_idlec5 ] {};
void()    book_idlec5 =    [$idlec5, book_idlec6 ] {};
void()    book_idlec6 =    [$idlec6, book_idlec7 ] {};
void()    book_idlec7 =    [$idlec7, book_idlec8 ] {};
void()    book_idlec8 =    [$idlec8, book_idlec9 ] {};
void()    book_idlec9 =    [$idlec9, book_idlec10 ] {};
void()    book_idlec10 = [$idlec10, book_idlec11 ] {};
void()    book_idlec11 = [$idlec11, book_idlec12 ] {};
void()    book_idlec12 = [$idlec12, book_idlec13 ] {};
void()    book_idlec13 = [$idlec13, book_idlec14 ] {};
void()    book_idlec14 = [$idlec14, book_idlec15 ] {};
void()    book_idlec15 = [$idlec15, book_idlec16 ] {};
void()    book_idlec16 = [$idlec16, book_idlec17 ] {};
void()    book_idlec17 = [$idlec17, book_idlec18 ] {};
void()    book_idlec18 = [$idlec18, book_idlec19 ] {};
void()    book_idlec19 = [$idlec19, book_idlec20 ] {};
void()    book_idlec20 = [$idlec20, book_closed1 ] {};

//----------------------------------------------------------------------
// Book closed
//----------------------------------------------------------------------
void()    book_closed1 =    [$closed1, book_closed2    ] {self.state = MISCBOOK_CLOSED;
    if (random() <    0.1 && self.search_time < time) {
        self.search_time = time + 2 + (random()*5);
        self.lefty = 1 - self.lefty;
        if (self.lefty > 0) book_idleb1();    // Turn left and right
        else book_idlec1();                    // spin around
    }
};
void()    book_closed2 =    [$closed2, book_closed3 ] {};
void()    book_closed3 =    [$closed3, book_closed4 ] {};
void()    book_closed4 =    [$closed4, book_closed5 ] {};
void()    book_closed5 =    [$closed5, book_closed6 ] {};
void()    book_closed6 =    [$closed6, book_closed7 ] {};
void()    book_closed7 =    [$closed7, book_closed8 ] {};
void()    book_closed8 =    [$closed8, book_closed9 ] {};
void()    book_closed9 =    [$closed9, book_closed10 ] {};
void()    book_closed10 =    [$closed10, book_closed1 ] {};

//----------------------------------------------------------------------
// Book closing (open -> close)
//----------------------------------------------------------------------
void()    book_closing1 =    [$opening10, book_closing2    ] {
    self.state = MISCBOOK_CLOSING;
    // make sure book idles don't start straight away
    self.search_time = time + 2 + (random()*5);
};
void()    book_closing2 =    [$opening9, book_closing3    ] {};
void()    book_closing3 =    [$opening8, book_closing4    ] {};
void()    book_closing4 =    [$opening7, book_closing5    ] {};
void()    book_closing5 =    [$opening6, book_closing6    ] {};
void()    book_closing6 =    [$opening5, book_closing7    ] {};
void()    book_closing7 =    [$opening4, book_closing8    ] {};
void()    book_closing8 =    [$opening3, book_closing9    ] {};
void()    book_closing9 =    [$opening2, book_closing10    ] {};
void()    book_closing10= [$opening1, book_closed1    ] {};

//----------------------------------------------------------------------
// Book always open (final stage of book)
//----------------------------------------------------------------------
void()    book_stayopen1 = [$open1, book_stayopen2 ] {self.state = MISCBOOK_STAYOPEN;};
void()    book_stayopen2 = [$open2, book_stayopen3 ] {};
void()    book_stayopen3 = [$open3, book_stayopen4 ] {};
void()    book_stayopen4 = [$open4, book_stayopen5 ] {};
void()    book_stayopen5 = [$open5, book_stayopen6 ] {};
void()    book_stayopen6 = [$open6, book_stayopen7 ] {};
void()    book_stayopen7 = [$open7, book_stayopen8 ] {};
void()    book_stayopen8 = [$open8, book_stayopen9 ] {};
void()    book_stayopen9 = [$open9, book_stayopen10 ] {};
void()    book_stayopen10 = [$open10, book_stayopen1 ] {};

//----------------------------------------------------------------------
// Players has walked away, decide what to do with the book
//----------------------------------------------------------------------
void() book_finish =
{
    info_book_fadesetup(-1);
    // Clear client centerprint suppression
    if (self.enemy.flags & FL_CLIENT) {    
        self.enemy.suppressCenterPrint = FALSE;
        centerprint(self.enemy, "");
    }
    if (self.pain_finished > 0) book_stayopen1();
    else book_closing1();
};

//----------------------------------------------------------------------
// Book is open (waiting to close)
//----------------------------------------------------------------------
void()    book_open1 = [$open1, book_open2 ] {self.state = MISCBOOK_OPEN;};
void()    book_open2 = [$open2, book_open3 ] {if (self.wait < time) book_finish();};
void()    book_open3 = [$open3, book_open4 ] {};
void()    book_open4 = [$open4, book_open5 ] {if (self.wait < time) book_finish();};
void()    book_open5 = [$open5, book_open6 ] {};
void()    book_open6 = [$open6, book_open7 ] {if (self.wait < time) book_finish();};
void()    book_open7 = [$open7, book_open8 ] {};
void()    book_open8 = [$open8, book_open9 ] {if (self.wait < time) book_finish();};
void()    book_open9 = [$open9, book_open10 ] {};
void()    book_open10= [$open10, book_open1 ] {if (self.wait < time) book_finish();};

//----------------------------------------------------------------------
// Book opening (closed -> open)
//----------------------------------------------------------------------
void()    book_opening1 =    [$opening1, book_opening2 ] {self.state = MISCBOOK_OPENING;};
void()    book_opening2 =    [$opening2, book_opening3 ] {};
void()    book_opening3 =    [$opening3, book_opening4 ] {};
void()    book_opening4 =    [$opening4, book_opening5 ] {};
void()    book_opening5 =    [$opening5, book_opening6 ] {};
void()    book_opening6 =    [$opening6, book_opening7 ] {};
void()    book_opening7 =    [$opening7, book_opening8 ] {};
void()    book_opening8 =    [$opening8, book_opening9 ] {};
void()    book_opening9 =    [$opening9, book_opening10 ] {};
void()    book_opening10 = [$opening10, book_open1 ] {};

//----------------------------------------------------------------------
void() info_book_fade = 
{
    if (self.estate & ESTATE_BLOCK) return;

    // Fade direction needs to be -1 or +1 (0.48s fade time)
    self.lip = self.lip + (self.height * 4);
    // Check if fade has reached 0 or max screen fade value
    if (self.lip <= 0) self.lip = 0;
    else if (self.lip >= MISCBOOK_FADEOUT) self.lip = MISCBOOK_FADEOUT;
    else self.nextthink = time + MISCBOOK_LOOPADVANCE;
    
    // Only update the screen if the debuff system is NOT active
    // and the client does NOT have any powerups active
    if (!self.enemy.cshift_upd && self.enemy.items & ALL_ITARTIFACTS == 0) {    
        // Change screen background density (makes text easier to read)
        stuffcmd(self.enemy, "v_cshift 0 0 0 ");
        stuffcmd(self.enemy, ftos(self.lip));
        stuffcmd(self.enemy, "\n");
    }
};

//----------------------------------------------------------------------
void(float fadedir) info_book_fadesetup =
{
    if (self.estate & ESTATE_BLOCK) return;

    // Is the fade controller setup?
    if (self.attachment) {
        // Double check if screen fade is setup already
        if (fadedir > 0 && self.attachment.lip == MISCBOOK_FADEOUT) return;
        if (fadedir < 0 && self.attachment.lip == 0) return;
        
        // Copy over client and fadedir to controller
        self.attachment.enemy = self.enemy;
        self.attachment.height = fadedir;
        self.attachment.think = info_book_fade;
        self.attachment.nextthink = time + MISCBOOK_LOOPADVANCE;
        self.attachment.ltime = self.attachment.nextthink;
    }
};

//----------------------------------------------------------------------
void() info_book_displaystory =
{
    // is this book part of a storyline?
    // The controller has been setup/validated already
    if (self.spawnflags & MISCBOOK_STORY) {
        // Check controller once!
        self.spawnflags = self.spawnflags - (self.spawnflags & MISCBOOK_STORY);

        if (self.attachment.estate == ESTATE_OFF) {
            self.message = "Story controller broken!\n\n";
        }
        else {
            // Copy over story chapter
            self.message = self.attachment.oldenemy.message;
            self.message2 = self.attachment.oldenemy.message2;
            self.message3 = self.attachment.oldenemy.message3;
            self.message4 = self.attachment.oldenemy.message4;
        }

        // Work out which centerprint to use
        if (self.message3 != "" && self.message4 != "") self.cnt = 2;
        else if (self.message3 != "") self.cnt = 1;
        else self.cnt = 0;

        // Move controller on to next story chapter
        if (self.attachment.height < self.attachment.count) {
            self.attachment.count = self.attachment.count + 1;
            self.attachment.oldenemy = self.attachment.oldenemy.oldenemy;
        }
    }
        
    // Suppress any other centerprint messages and show story
    self.enemy.suppressCenterPrint = TRUE;
    // Check for any additional strings and use correct centerprint
    // The engine function will merge several strings together
    if (self.cnt == 1) centerprint_msg3(self.enemy, self.message, self.message2, self.message3);
    else if (self.cnt == 2) centerprint_msg4(self.enemy, self.message, self.message2, self.message3, self.message4);
    else centerprint_msg(self.enemy, self.message, self.message2);

    // Check for any additional trigger events
    if (self.target != "") {
        trigger_strs(self.target, self.enemy);
        self.target = "";
    }
};

//----------------------------------------------------------------------
void() info_book_touch =
{
    local float book_yaw, book_angle1, book_angle2, play_yaw, play_angle;

    if (self.estate & ESTATE_BLOCK) return;
    if (intermission_running) return;        // intermission or finale
    if ( !(other.flags & FL_CLIENT) ) return;
    if (other.health < 1) return;

    // Store player entity for later use    
    self.enemy = other;

    if (self.spawnflags & MISCBOOK_ANGLEONLY) {
        // Check the players facing direction against book angle direction
        // make sure the player angle is never negative, + 360 just in case
        play_angle = anglemod(self.enemy.v_angle_y+360);
        // Work out angle cone based on book angle direction
        // Use previously defined angles for viewing angle of book
        book_yaw = fabs(self.v_angle_y - self.v_angle_x);
        book_angle1 = anglemod(self.angles_y + 180 + (360-book_yaw));
        book_angle2 = anglemod(self.angles_y + 180 + book_yaw);
        // Is the players angle outside the book's viewing range?
        if (play_angle < book_angle1 || play_angle > book_angle2) return;
    }
    
    // The angle at the which the book is facing the player
    // book_yaw = vectoyaw(self.enemy.origin - self.origin);
    // book_angle = anglemod((self.angles_y - book_yaw) + 0);
    // The angle the player is facing towards the book (45 = forward)
    play_yaw = vectoyaw(self.origin - self.enemy.origin);
    play_angle = anglemod((self.enemy.v_angle_y - play_yaw) + 45);

    // Is the player looking at the book?
    if (play_angle > self.v_angle_x && play_angle < self.v_angle_y) {
        // Is the book closed?
        if (self.state == MISCBOOK_CLOSED) {
            self.wait = time + 1.3;
            book_opening1();
        }
        // Is the book currently open and been read before?
        else if (self.state == MISCBOOK_STAYOPEN) {
            info_book_fadesetup(1);
            self.wait = time + 0.3;
            self.attack_finished = time + 0.2;
            book_open1();
        }        
        // Has the book just opened and never been read before?
        else if (self.state == MISCBOOK_OPEN && self.pain_finished == FALSE) {
            self.pain_finished = TRUE;
            info_book_fadesetup(1);
            self.wait = time + 0.3;
            self.attack_finished = time + 0.2;
            if (self.part_emitter) misc_particle_update(self.part_emitter, PARTICLE_STYLE_OPENBOOK);
            info_book_displaystory();
        }
        // Is the book open and ready for a story?
        else if (self.state == MISCBOOK_OPEN) {
            info_book_fadesetup(1);
            self.wait = time + 0.3;
            if (self.attack_finished < time) {
                self.attack_finished = time + 0.2;
                info_book_displaystory();
            }
        }
    }
};

//----------------------------------------------------------------------
void() info_book_on = 
{
    self.estate = ESTATE_ON;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_TRIGGER;
    if ( !(self.spawnflags & MISCBOOK_NOMODEL) ) {
        setmodel (self, self.mdl);
        self.skin = self.exactskin;
    }

    // Make sure particle emitter is turned on
    if (self.part_emitter) misc_particle_on(self.part_emitter);

    // Setup touch trigger and function
    self.touch = info_book_touch;
    setsize (self, self.pos1, self.pos2);
    
    // If no model, don't need book animations + attachment models
    if ( !(self.spawnflags & MISCBOOK_NOMODEL) ) {
        // Check for any plinth models
        if (self.attachment2) {
            // Use bounding box coillsion?
            if (self.spawnflags & MISCBOOK_COLLISION) self.attachment2.solid = SOLID_BBOX;
            else self.attachment2.solid = SOLID_NOT;
            setmodel (self.attachment2, self.noise1);
        }
        
        // Setup book in correct animation state
        self.search_time = time + 5 + (random()*5);    // delay idle animation
        if (self.state == MISCBOOK_CLOSED) book_closed1();
        else book_stayopen1();
    }
};

//----------------------------------------------------------------------
void() info_book_off = 
{
    self.estate = ESTATE_OFF;
    self.solid = SOLID_NOT;
    setmodel (self, "");
    
    // Stop any touch/animations
    self.touch = self.think = SUB_Null;

    // Check if the screen is faded?
    if (self.attachment) {
        if (self.attachment.lip != 0) {
            self.attachment.lip = 0;
            if (!self.enemy.cshift_upd && self.enemy.items & ALL_ITARTIFACTS == 0) {    
                stuffcmd(self.enemy, "v_cshift 0 0 0 0\n");
            }
        }
    }
    
    // If any plinth exists, turn that off as well
    if (self.attachment2) {
        setmodel(self.attachment2, "");
        self.attachment2.solid = SOLID_NOT;
    }
};

//----------------------------------------------------------------------
void() info_book_setup =
{
    // Setup book fade controller
    self.attachment = spawn();
    self.attachment.owner = self;
    self.attachment.classtype = CT_CONTROL;
    self.attachment.movetype = MOVETYPE_NONE;
    setorigin(self.attachment,self.origin + '0 0 32');
    setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);

    // Check for plinth and spawn/setup
    if (self.mdl != "" && self.noise1 != "") {
        self.attachment2 = spawn();
        self.attachment2.owner = self;
        self.attachment2.classtype = CT_MISCMODEL;
        self.attachment2.movetype = MOVETYPE_NONE;
        
        // Trace downward to find the ground and place plinth
        traceline (self.origin, self.origin + '0 0 -512', TRUE, self);
        self.origin = trace_endpos + '0 0 32';
        setorigin(self.attachment2, self.origin);
        self.attachment2.angles = self.angles;
        
        // Plinth2 has flat surface which conflicts with book angle
        if (self.spawnflags & MISCBOOK_PLINTH2) self.origin = self.origin + '0 0 4';
    }

    // Check for random skin options
    if (self.randomskin > 1) 
        self.exactskin = rint(random()*(self.randomskin-1));
    // The particle colours are based on the book skin
    if (self.exactskin < 1) self.exactskin = 0;
    if (self.exactskin >= 6) self.style = MISCBOOK_RED;
    else if (self.exactskin >= 4) self.style = MISCBOOK_BLUE;
    else self.style = MISCBOOK_GREEN;
    
    // Default viewing angle is min 30 and max 60 looking at origin
    if (CheckZeroVector(self.v_angle)) self.v_angle = '30 60 0';
    if (self.angles_y == 0) self.angles_y = 360;

    // Check for story book setup first
    // Story is supplied from story system when book is opened
    if (self.spawnflags & MISCBOOK_STORY) {
        // Reset all storybook message strings
        self.message = "Waiting for story!"; 
        self.message2 = "";
        self.message3 = ""; self.message4 = "";
        self.cnt = 0;
        self.attachment = find(world, targetname, self.target2);
        // Is this entity a storybook controller?
        if (!self.attachment || self.attachment.classtype != CT_STORYCONTROL)
        {
            dprint("\b[STORYBOOK]\b Missing controller!\n");
            spawn_marker(self.origin, SPNMARK_YELLOW);
            entity_hide(self);
            return;
        }
    }
    else {
        // setup default message for testing
        if (self.message == "") self.message = "\b--[ Dark Elder Magic ]--\b\n\n";
        if (self.message2 == "") self.message2 = "Despite the awful might of the Elder\nWorld, you have achieved the Rune of\nElder Magic, capstone of all types of\narcane wisdom. Beyond good and evil,\nbeyond life and death, the Rune\npulsates, heavy with import.\n";

        // Check for additional strings to display
        // This is to fix quark editor limit of 128 characters per string
        if (self.message3 != "" && self.message4 != "") self.cnt = 2;
        else if (self.message3 != "") self.cnt = 1;
        else self.cnt = 0;
    }
    
    // Spawn particle emitter if particles active and not blocked
    if ( !(self.spawnflags & MISCBOOK_NOMODEL) ) {
        self.part_active = PARTICLE_STYLE_BOOK;
        if (self.spawnflags & ENT_STARTOFF) self.count = PARTICLE_START_OFF;
        else self.count = PARTICLE_START_ON;
        if (query_configflag(SVR_PARTICLES) == SVR_PARTICLES) {
            if (!(self.spawnflags & MISCBOOK_NOEFFECTS) && self.part_active > 0)
                self.part_emitter = spawn_pemitter(self, self, self.part_active, self.count);
        }
    }
    
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = info_book_on;
    self.estate_off = info_book_off;
    if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    else self.estate_on();
};

/*======================================================================
/*QUAKED misc_textbook (0 0.5 0.5) (-16 -16 -8) (16 16 8) NOMODEL PLINTH1 PLINTH2 STORY COLLISION ANGLEONLY STARTOFF NOEFFECTS
Display custom text messages
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
target     : trigger event when book is opened (only works once)
target2    : name of story controller (only works with spawnflag)
angle      : facing angle for model or direction player must be standing
v_angle    : The viewing angle the book is active (def=30,60,0)
exactskin  : skin number for book (1-8, Brown1/2,Green1/2,Blue1/2,Red1/2)
pos1       : Touch trigger minimin size (def=-48 -48 -32)
pos2       : Touch trigger maximum size (def=48 48 32)
message    : header message
message2   : Body Text 1 (need to add linefeeds)
message3   : Body Text 2 (displayed after message2)
message4   : Body Text 3 (displayed after message3)
-------- SPAWNFLAGS --------
NOMODEL   : Will not use book model + particles
PLINTH1   : Setup book on top of plinth1 model
PLINTH2   : Setup book on top of plinth2 model
STORY     : Book is part of a story (use target2)
COLLISION : Use bounding box collision for plinth
ANGLEONLY : Will only work when standing infront of entity (angle key)
STARTOFF  : Starts off and waits for trigger
NOEFFECTS : No particle or effects active
-------- NOTES --------
Display custom text messages
Maximum size of message is 1024 characters, 256 is the original limit

======================================================================*/
void() misc_textbook =
{
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    self.classtype = CT_STORYBOOK;
    // Setup large bounding box for touch trigger
    if (CheckZeroVector(self.pos1)) self.pos1 = '-48 -48 -32';
    if (CheckZeroVector(self.pos2)) self.pos2 = '48 48 32';
    self.takedamage = DAMAGE_NO;    // No damage from anything

    if (self.spawnflags & MISCBOOK_NOMODEL) {
        self.mdl = "";
        self.state = MISCBOOK_STAYOPEN;    // no book animation
        self.pain_finished = TRUE;        // No read delay
    }
    else {
        self.state = MISCBOOK_CLOSED;    // Current state
        self.pain_finished = FALSE;        // Book not read yet
        // Default medieval book
        self.mdl = "progs/ad171/misc_textbook.mdl";
        precache_model (self.mdl);
        self.noise1 = "";
        // Check for any predefined plinth model setups
        if (self.spawnflags & MISCBOOK_PLINTH1) {
            self.noise1 = "progs/ad171/misc_plinth1.mdl";
            precache_model(self.noise1);
        }
        else if (self.spawnflags & MISCBOOK_PLINTH2) {
            self.noise1 = "progs/ad171/misc_plinth2.mdl";
            precache_model(self.noise1);
        }
    }
        
    // Don't spawn straight away
    self.nextthink = time + 0.1 + random()*0.4;
    self.think = info_book_setup;
};

/*======================================================================
/*QUAKED misc_textstory (0 0.5 1.0) (-16 -16 -8) (16 16 8) x
Text Book Story Chapters
-------- KEYS --------
targetname : name of current chapter
target     : name of next chapter
message    : header message
message2   : Body Text 1 (need to add linefeeds)
message3   : Body Text 2 (displayed after message2)
message4   : Body Text 3 (displayed after message3)
-------- SPAWNFLAGS --------
-------- NOTES --------
Text Book Story Chapters

/*QUAKED misc_textstoryctrl (0 0.1 1.0) (-16 -16 -8) (16 16 8) x
Text Book Story Controller
-------- KEYS --------
targetname : name of controller for story
target : name of first chapter
-------- SPAWNFLAGS --------
-------- NOTES --------
Text Book Story Controller

======================================================================*/
void() info_bookcont_setup = 
{
    local entity storychap, prevchap;
    
    // Is there any story chapters?
    storychap = find(world, targetname, self.target);
    if (!storychap) {
        dprint("\b[STORYCTRL]\b Missing start chapter!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        return;
    }

    // Can only link to story chapter entities
    if (storychap.classtype != CT_STORYCHAPTER) {
        dprint("\b[STORYCTRL]\b First chapter wrong entity!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        return;
    }
    
    // Setup controller
    prevchap = self;    // Start controller at chapter 1
    self.count = 0;        // Total chapters in story
    self.height = 1;    // Current chapter in story
    
    // Work through list
    while (storychap) {
        // Visual debug for chapters
        spawn_marker(storychap.origin, SPNMARK_BLUE);

        // Link chapters and move forward
        prevchap.oldenemy = storychap;
        prevchap = storychap;
        self.count = self.count + 1;

        // Check for next target in chain
        if (prevchap.target == "") storychap = world;
        else storychap = find(world, targetname, prevchap.target);
    }
    // Point final chapter at itself
    prevchap.oldenemy = prevchap;
    
    // Controller finally active
    self.estate = ESTATE_ON;
    // Visual debug for controller
    spawn_marker(self.origin, SPNMARK_GREEN);
    // Console message for chapters found
    dprint("\b[STORYCTRL]\b Chapters found (");
    dprint(ftos(self.count));
    dprint(")\n");
};

//----------------------------------------------------------------------
void() misc_textstoryctrl =
{
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    self.classtype = CT_STORYCONTROL;
    // Start in disabled state until chapters setup
    self.estate = ESTATE_OFF;

    if (self.target == "") {
        dprint("\b[STORYCTRL]\b Missing story start!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        return;
    }
    
    // Wait for chapter and books to spawn first
    self.nextthink = time + 1 + random();
    self.think = info_bookcont_setup;
};

//----------------------------------------------------------------------
void() misc_textstory =
{
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    self.classtype = CT_STORYCHAPTER;
};
