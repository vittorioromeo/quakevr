/*======================================================================
 Path_Corner FUNCTIONS
======================================================================*/
float PATHC_EXACT = 1;        // Exact route logic instead of random (def)
float PATHC_INSTANT = 2;    // Instantly move to this path_corner
float PATHC_REVERSE = 4;    // Train direction is reversed at this corner
float PATHC_NOPAUSE = 8;    // Train does not pause on this corner

float PATHC_RESET = 15;        // Use to reset spawnflags

float PATHC_TARGET1 = 16;    // Has target been found
float PATHC_TARGET2 = 32;    // Has target2 been found
float PATHC_BACKLINK = 64;    // Backward path route
float PATHC_DEADEND = 128;    // no targets found

float STATE_ROUTE1 = 1;        // Route 1 is the priority (forward)
float STATE_ROUTE2 = 2;        // Route 2 is the priority (forward)
float STATE_ROUTE3 = 3;        // Route 3 is the priority (backward)
float STATE_EXACT = 4;        // Switch to exact route selection
float STATE_RANDOM = 5;        // Switch to random route selection

/*======================================================================
/*QUAKED path_corner (0.5 0.3 0) (-8 -8 -24) (8 8 48) EXACT INSTANT REVERSE NOPAUSE
Node based path routing system
-------- KEYS --------
targetname   : Name of current path corner
corner_event : Name of target(s) to trigger when used/touched
corner_route : Change path_corner route (1-3 routes, 4=exact, 5=random)
corner_switch: Change spawnflags REVERSE (-1=NO, 1=YES, 2=Toggle)
corner_pause : Change spawnflags NOPAUSE (-1=NO, 1=YES, 2=Toggle)
state        : Starting route (1=forward, 2=alt forward, 3=backward)
target       : Targetname of FORWARD route
target2      : Alternative FORWARD route
targetback   : Override default BACKWARD route
wait   : fixed amount of time to pause at corner (=-1 stop)
delay  : random amount of time to pause at corner
alpha  : Override default for visual arrows (def=0.35)
speed  : Override default speed of func_train (def=100)
-------- SPAWNFLAGS --------
EXACT     : Exact route logic (def=random route choices)
INSTANT   : Move instantly to this corner if next destination
REVERSE   : Train direction is reversed for next corner
NOPAUSE   : Train does not pause on this path corner
-------- NOTES --------
Node based path routing system

======================================================================*/
entity(string arrowmdl, vector arroworg) path_corner_spawnarrow =
{
    local entity arrow;
    arrow = spawn();
    arrow.classtype = CT_ATTACHMENT;
    arrow.movetype = MOVETYPE_NONE;
    arrow.solid = SOLID_NOT;
    setmodel(arrow, arrowmdl);
    setorigin(arrow, arroworg);
    setsize(arrow, VEC_ORIGIN, VEC_ORIGIN);
    // If DP engine active remove particle shadow
    if (engine == ENG_DPEXT) arrow.effects = arrow.effects + EF_NOSHADOW;

    if (!self.alpha) arrow.alpha = 0.35;
    else arrow.alpha = self.alpha;
    return arrow;
};

//----------------------------------------------------------------------
void() path_corner_updatearrow =
{
    // Visual AI pathing only active in dev mode
    if (developer == 0) return;
    if (query_configflag(SVR_DEVHELPER)) return;
    if ( !(self.spawnflags & PATHC_TARGET2) ) return;

    // Switch colour of arrow based on route logic
    if (self.spawnflags & PATHC_TARGET1 && self.state == STATE_ROUTE1) {
        if (self.attachment) self.attachment.skin = 3;
        if (self.attachment2) self.attachment2.skin = 2;
        if (self.attachment3) self.attachment3.skin = 0;
    }
    else if (self.spawnflags & PATHC_TARGET2 && self.state == STATE_ROUTE2) {
        if (self.attachment) self.attachment.skin = 0;
        if (self.attachment2) self.attachment2.skin = 3;
        if (self.attachment3) self.attachment3.skin = 0;
    }
    else if (self.spawnflags & PATHC_BACKLINK && self.state == STATE_ROUTE3) {
        if (self.attachment) self.attachment.skin = 0;
        if (self.attachment2) self.attachment2.skin = 2;
        if (self.attachment3) self.attachment3.skin = 3;
    }
    else {
        // Default - Random logic has no route highlight
        if (self.attachment) self.attachment.skin = 0;
        if (self.attachment2) self.attachment2.skin = 2;
        if (self.attachment3) self.attachment3.skin = 0;
    }
};

//----------------------------------------------------------------------
void() path_corner_setuparrow =
{
    // Visual AI pathing only active in dev mode
    if (developer == 0) return;
    if (query_configflag(SVR_DEVHELPER)) return;

    // Draw back link arrow
    if (self.spawnflags & PATHC_BACKLINK) {
        self.attachment3 = path_corner_spawnarrow( self.mdl, self.oldorigin);
        self.movedir = normalize(self.movetarget3.oldorigin - self.oldorigin);
        self.attachment3.angles = vectoangles(self.movedir);
    }
    // Missing backlink, this is a bad situation!
    // Highlight this problem with a pointing up RED arrow
    else {
        // There is no back link, so use forward target instead
        if (self.spawnflags & PATHC_TARGET1) self.oldenemy = self.movetarget;
        else self.oldenemy = self;
        self.attachment3 = path_corner_spawnarrow( self.mdl, self.oldorigin);
        self.movedir = normalize(self.oldenemy.oldorigin - self.oldorigin);
        self.attachment3.angles = vectoangles(self.movedir);
        self.attachment3.skin = 4;
        makevectors(self.movedir);
        self.attachment3.angles = self.attachment3.angles + vectoangles(v_up);
    }
    
    // Draw route choice 1 (target)
    if (self.spawnflags & PATHC_TARGET1) {
        self.attachment = path_corner_spawnarrow( self.headmdl, self.oldorigin);
        self.movedir = normalize(self.movetarget.oldorigin - self.oldorigin);
        self.attachment.angles = vectoangles(self.movedir);
    }
    // Missing forward link (deadend)
    // Highlight this issue with a pointing up RED arrow
    else {
        // There is no forward link, so use back link instead
        if (self.spawnflags & PATHC_BACKLINK) self.oldenemy = self.movetarget3;
        else self.oldenemy = self;
        self.attachment = path_corner_spawnarrow( self.headmdl, self.oldorigin);
        self.movedir = normalize(self.oldenemy.oldorigin - self.oldorigin);
        self.attachment.angles = vectoangles(self.movedir);
        self.attachment.skin = 4;
        makevectors(self.movedir);
        self.attachment.angles = self.attachment.angles + vectoangles(v_up);
    }
    
    // Draw route choice 2 (target2)
    if (self.spawnflags & PATHC_TARGET2) {
        self.attachment2 = path_corner_spawnarrow( self.headmdl, self.oldorigin);
        self.movedir = normalize(self.movetarget2.oldorigin - self.oldorigin);
        self.attachment2.angles = vectoangles(self.movedir);
        self.attachment2.skin = 2;
    }
    
    // Update arrows if route logic active
    path_corner_updatearrow();
};

//----------------------------------------------------------------------
void() path_corner_setuptargets =
{
    local entity fdest;

    // Cycle through list for TARGET path_corner
    if (self.target != "") {
        fdest = find (world, targetname, self.target);
        while(fdest) {
            if (fdest.classtype == CT_PATHCORNER) {
                // Update (spawnflags) path_corner and save entity
                self.spawnflags = self.spawnflags | PATHC_TARGET1;
                self.movetarget = fdest;
                fdest = world;
            }
            else fdest = find(fdest, targetname, self.target);
        }
    }
    // Cycle through list for TARGET2 path_corner
    if (self.target2 != "") {
        fdest = find (world, targetname, self.target2);
        while(fdest) {
            if (fdest.classtype == CT_PATHCORNER) {
                // Update (spawnflags) path_corner and save entity
                self.spawnflags = self.spawnflags | PATHC_TARGET2;
                self.movetarget2 = fdest;
                fdest = world;
            }
            else fdest = find(fdest, targetname, self.target2);
        }
    }

    // Has the path corner back link override key been used?
    // If this override string does not exist, weird things will happen 
    if (self.targetback != "") {
        fdest = find (world, targetname, self.targetback);
        while(fdest) {
            // Update (spawnflags) path_corner and save entity
            if (fdest.classtype == CT_PATHCORNER) {
                self.spawnflags = self.spawnflags | PATHC_BACKLINK;
                self.movetarget3 = fdest;
                fdest = world;
            }
            else fdest = find(fdest, targetname, self.targetback);
        }
    }
    else {
        // Setup back link name, if none exist use targetname
        self.targetback = self.targetname;
        
        // Cycle through list for BACKLINK path_corner
        fdest = find (world, target, self.targetback);
        while(fdest) {
            // Update (spawnflags) path_corner and save entity
            if (fdest.classtype == CT_PATHCORNER) {
                self.spawnflags = self.spawnflags | PATHC_BACKLINK;
                self.movetarget3 = fdest;
                fdest = world;
            }
            else fdest = find(fdest, target, self.targetback);
        }
    }

    // Double check TARGET2 before declaring a deadend
    if ( !(self.spawnflags & PATHC_BACKLINK) ) {
        fdest = find (world, target2, self.targetback);
        while(fdest) {
            if (fdest.classtype == CT_PATHCORNER) {
                self.spawnflags = self.spawnflags | PATHC_BACKLINK;
                self.movetarget3 = fdest;
                fdest = world;
            }
            else fdest = find(fdest, target2, self.targetback);
        }
    }

    // Bad situation : Found a path corner with no back-link
    // label as deadend (direction reverse) and circular link
    if ( !(self.spawnflags & PATHC_BACKLINK) ) {
        dprint("\b[PATH_CORNER]\b ("); dprint(self.targetname);
        dprint(") - Deadend found\n");
        if (self.movetarget) self.movetarget3 = self.movetarget;
        else self.movetarget3 = self;
        self.spawnflags = self.spawnflags | PATHC_DEADEND;
    }
    
    // Random time interval as this is not important
    // Generate developer arrow models on path_corner
    if (developer > 0) {
        self.nextthink = time + 0.5 + random();
        self.think = path_corner_setuparrow;
    }
    
};

//----------------------------------------------------------------------
void() path_corner_use =
{
    // Change spawnflags NOPAUSE (-1=NO, 1=YES, 2=Toggle)
    if (other.corner_pause) {
        // Store the parameter locally so it can modified
        self.corner_pause = other.corner_pause;
        if (self.corner_pause == 2) {
            if (self.spawnflags & PATHC_NOPAUSE) self.corner_pause = 1;
            else self.corner_pause = -1;
        }
        // The toggle state uses the on/off code by storing the value
        // locally and modifying it beforehand
        if (self.corner_pause > 0) {
            if (self.spawnflags & PATHC_NOPAUSE)
                self.spawnflags = self.spawnflags - PATHC_NOPAUSE;
            self.wait = -1;
        }
        else if (self.corner_pause < 0) {
            self.spawnflags = self.spawnflags | PATHC_NOPAUSE;
            self.wait = 0;
        }
    }
        
    // Change spawnflags REVERSE (-1=NO, 1=YES, 2=Toggle)
    if (other.corner_switch) {
        // Store the parameter locally so it can modified
        self.corner_switch = other.corner_switch;
        if (self.corner_switch == 2) {
            if (self.spawnflags & PATHC_REVERSE) self.corner_switch = -1;
            else self.corner_switch = 1;
        }
        // The toggle state uses the on/off code by storing the value
        // locally and modifying it beforehand
        if (self.corner_switch > 0) {
            self.spawnflags = self.spawnflags | PATHC_REVERSE;
        }
        else if (self.corner_switch < 0) {
            if (self.spawnflags & PATHC_REVERSE)
                self.spawnflags = self.spawnflags - PATHC_REVERSE;
        }
    }
    
    // Change speed of train moving towards path_corner (default=100)
    if (other.corner_speed > 0) self.speed = other.corner_speed;

    // Changing routes?
    if (other.corner_route) {
        // Double check that the routes exist before setting them
        if (other.corner_route == STATE_ROUTE1 && 
            self.spawnflags & PATHC_TARGET1) self.state = STATE_ROUTE1;
        else if (other.corner_route == STATE_ROUTE2 && 
            self.spawnflags & PATHC_TARGET2) self.state = STATE_ROUTE2;
        else if (other.corner_route == STATE_ROUTE3 && 
            self.spawnflags & PATHC_BACKLINK) self.state = STATE_ROUTE3;
        // If changing routing logic, make sure spawnflag is updated
        else if (other.corner_route == STATE_EXACT) {
            self.spawnflags = self.spawnflags | PATHC_EXACT;
            self.state = STATE_ROUTE1;
        }
        // When routing logic is changed, need to reset state
        else if (other.corner_route == STATE_RANDOM) {
            if (self.spawnflags & PATHC_EXACT)
                self.spawnflags = self.spawnflags - PATHC_EXACT;
            self.state = STATE_ROUTE1;
        }
        dprint("Corner ("); dprint(self.targetname);
        dprint(") Route ("); dprint(ftos(self.state));
        dprint(")\n");
    }
    else {
        // Toggle route selection if EXACT routing logic is active
        // and the path corner has multiple routes available
        if (self.spawnflags & PATHC_EXACT && self.spawnflags & PATHC_TARGET2) {
            if (self.state == STATE_ROUTE1) self.state = STATE_ROUTE2;
            else if (self.state == STATE_ROUTE2) self.state = STATE_ROUTE3;
            else self.state = STATE_ROUTE1;
        }
    }

    // Update arrows if route logic active
    path_corner_updatearrow();
};

//----------------------------------------------------------------------
void() path_corner_stop =
{
    other.goalentity = other.movetarget = world;
    other.pausetime = time + LARGE_TIMER;
    other.think = other.th_stand;
    other.nextthink = time + 0.05;
};

//----------------------------------------------------------------------
void() path_corner_touch =
{
    if (!(other.flags & FL_MONSTER)) return;    // ONLY Monsters can use path corners
    if (other.health < 1) return;                // Dead things don't need path guidance
    if (other.enemy) return;                    // In combat, no time to follow paths
    if (other.movetarget != self) return;        // Suppose to follow this corner?

    // If ogre, play chainsaw drag sound (50% of the time)
    if (other.classtype == CT_MONOGRE || other.classtype == CT_MONHOGRE)
        if (random() > 0.5) sound (other, CHAN_VOICE, "ogre/ogdrag.wav", 1, ATTN_IDLE);

    // Fire any event targets (activator = monster)
    if (self.corner_event) trigger_strs(self.corner_event,other);
    // Check if this path_corner is a dead end? (no target)
    if (self.spawnflags & PATHC_DEADEND) path_corner_stop();
    // Has this path corner been linked yet? Try again?
    if ( !(self.spawnflags & PATHC_TARGET1) ) path_corner_setuptargets();

    // Is there a target to move towards?
    if (self.spawnflags & PATHC_TARGET1) {
        // Check for a second random path?
        if (self.spawnflags & PATHC_TARGET2) {
            // Check route logic (only use forward routes)
            if (self.spawnflags & PATHC_EXACT) {
                if (self.state == STATE_ROUTE1) other.goalentity = self.movetarget;
                else other.goalentity = self.movetarget2;
            }
            else {
                // Default = random routes
                if (random() < 0.5) other.goalentity = self.movetarget;
                else other.goalentity = self.movetarget2;
            }
        }
        // Default, 1 path = 1 choice
        else other.goalentity = self.movetarget;
        // Setup goal for monster to move toward
        other.movetarget = other.goalentity;
        
        // Does the monster need to pause at current path_corner?
        if (self.wait < 0) path_corner_stop();
        else if (self.wait > 0) {
            other.pausetime = time + self.wait + random()*self.delay;
            other.think = other.th_stand;
            other.nextthink = time + 0.05;
        }
        // no pause, turn and keep walking towards next goal
        else {
            other.ideal_yaw = vectoyaw(other.goalentity.origin - other.origin);
            other.pausetime = 0;
        }
    }
    // No new target to follow, stand around
    else path_corner_stop();
};

//----------------------------------------------------------------------

//----------------------------------------------------------------------
void() path_corner =
{
    // Really need to prevent path corners with no targetname
    // otherwise it will cause problems with linking later
    if (self.targetname == "") {
        dprint("\b[PATH_CORNER]\b Missing targetname!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        remove(self);
        return;
    }
    
    // Cache arrow models if devmode active
    if (developer > 0) {
        self.headmdl = MODEL_CORNER1;    // Top of Arrow (forward)
        self.mdl = MODEL_CORNER2;        // Bottom of Arrow (backward)
        precache_model(self.headmdl);
        precache_model(self.mdl);
    }
    
    // Setup time delay at corner ( wait + delay*random() )
    if (self.wait == 0) self.wait = 0;
    if (self.delay <= 0) self.delay = 0;
    
    // Reset all path targets/entities, spawnflags (low 4 bits only)
    self.spawnflags = self.spawnflags & PATHC_RESET;
    self.movetarget = self.movetarget2 = self.movetarget3 = world;
    
    self.solid = SOLID_TRIGGER;                // always touchable
    self.movetype = MOVETYPE_NONE;            // Does not move
    self.classtype = CT_PATHCORNER;            // Self identification
    setsize (self, '-8 -8 -16', '8 8 16');    // Size of trigger
    self.touch = path_corner_touch;            // Useable by monsters
    self.use = path_corner_use;                // Used by trigger events
    
    // Work out which (random/toggle) route logic is active
    if (self.spawnflags & PATHC_EXACT) {
        // If state is specified and outside range, reset
        if (self.state < STATE_ROUTE1 || self.state > STATE_ROUTE3)
            self.state = STATE_ROUTE1;
    }
    else self.state = STATE_ROUTE1;
    // Cannot start with route 2 (alt forward) if target2 is empty!?!
    if (self.state == STATE_ROUTE2 && self.target2 == "") self.state = STATE_ROUTE1;

    // Many of the ID maps have path_corners buried in solid architecture
    // which makes it difficult to see where they are and impossible to draw
    // visual arrows on them.
    // 
    // This code checks for solid content and keeps nudging the
    // temporary origin upwards 8 units at a time (max 64 units)
    // If it cannot find any empty space above, it will give up and
    // use the original origin point instead
    //
    self.oldorigin = self.origin;
    if (pointcontents(self.oldorigin) == CONTENT_SOLID) {
        self.lip = 8;
        while (self.lip > 0) {
            self.lip = self.lip - 1;
            self.oldorigin = self.oldorigin + '0 0 8';
            if (pointcontents(self.oldorigin) != CONTENT_SOLID)
                self.lip = -1;
        }
    }
    // Origin is still in something solid, reset
    if (self.lip != -1) self.oldorigin = self.origin;
    
    // Check for dead end path_corner
    if (self.target == "") self.spawnflags = self.spawnflags | PATHC_DEADEND;

    // Allow 1 frame for path_corners to spawn
    // Got to start linking before func_train (2nd frame)
    self.think = path_corner_setuptargets;
    self.nextthink = time + 0.1;
    
};
