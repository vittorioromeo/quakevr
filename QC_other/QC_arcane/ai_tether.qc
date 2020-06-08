/*======================================================================
 MONSTER TETHER SYSTEM

======================================================================*/
// new entity keys

.string        tethertarget;        // String name to link tethering too
.entity        tethertarg;            // Entity to track for tether system
.void()        th_tether;            // Function to call when at max range
.float        tethered;            // is the monster tethered during combat
.float        tetherrange;        // The circular range around tether point
.float        tetherdist;            // Distance from nonster to tether point
.float        tetherenemy;        // Distance from enemy to tether point
.float        tetherwait;            // Amount of time to wait before returning
.float        tethertimer;        // Time before returning to tether point
.float        tetherturn;            // Make sure monster turns towards enemy
.float        tetherpause;        // % to not pause attack at max range
.entity        tetherfocus;        // Enemy to turn towards while waiting

float TETHER_RADIUS = 300;        // Default tether radius
float TETHER_WAIT = 30;            // Default timer to wait before reset

// Defined under defs.qc
//.float tetherlock;        // Is the monster locked from movement

//----------------------------------------------------------------------
// Check for tether point (find and setup entity)
//----------------------------------------------------------------------
void() setup_tethersystem =
{
    // Check if the tether system be setup already
    if (self.tethertarget != "") {
        // Check for range attack, cannot tether if no range
        if (!self.th_missile) {
            dprint("\b[TETHER]\b No range attack for ");
            dprint(self.tethertarget);
            dprint("\n");
            self.tethertarget = "";
            return;
        }
    
        // Find the tether point (has to be path corner)
        self.tethertarg = find(world, targetname, self.tethertarget);
        if (self.tethertarg != world) {
            // Tether system is active (reset lock)
            self.tethered = TRUE;
            self.tetherlock = FALSE;
            // Setup default tether range
            if (self.tetherrange < 32) self.tetherrange = TETHER_RADIUS;
            if (self.tetherwait <= 0) self.tetherwait = TETHER_WAIT;
            // Reset distances (updated in check function)
            self.tetherdist = self.tetherenemy = 0;
            // No stand/walk timer active yet
            self.tethertimer = LARGE_TIMER;
            // Check for tether function (def=missile)
            if (!self.th_tether) self.th_tether = self.th_missile;
            // Don't need tethertarget anymore
            self.tethertarget = "";
        }
    }
};

//----------------------------------------------------------------------
// Check for tether range and return TRUE if at radius limit
//----------------------------------------------------------------------
float() check_tethersystem =
{
    local vector org;
    
    // Is the system active on the monster?
    if (self.tethered == FALSE) return FALSE;
    
    // Read tether target for BSP/Ent origin
    if (self.tethertarg.bsporigin) org = bmodel_origin(self.tethertarg);
    else org = self.tethertarg.origin;
    
    // Setup tether distance for monster and enemy
    self.tetherdist = vlen(self.origin - org);
    // Check for enemy entity first before distance check
    if (self.enemy) self.tetherenemy = vlen(self.enemy.origin - org);
    else self.tetherenemy = 0;
    
    // Reset lock
    self.tetherlock = FALSE;

    // If inside tether range, keep moving
    if (self.tetherdist < self.tetherrange) return FALSE;
    else {
        // If enemy behind tether point, move closer
        if (infront(self.tethertarg) && infront(self.enemy))
            return FALSE;
        // Reach tether limit
        else {
            // Can monster see the enemy?
            if (enemy_vis) return TRUE;
            else {
                // Check for no pause override
                if (self.tetherpause > 0) {
                    if (random() <= self.tetherpause) return TRUE;
                }
                // At max range + cannot see enemy, exit combat
                self.tetherfocus = self.enemy;    // Save for later
                self.enemy = self.goalentity = self.movetarget = world;
                self.attack_state = AS_STRAIGHT;
                self.think = self.th_stand;        // Wait for enemy
                self.tethertimer = time + self.tetherwait;
            }
        }
    }
    return FALSE;
};

//----------------------------------------------------------------------
// Check tether timer for stand function
//----------------------------------------------------------------------
void() check_tethertimer =
{
    if (self.tethered == FALSE) return;        // System active?
    if (!self.tethertarg) return;            // No target active
    if (self.enemy) return;                    // In combat?
    
    // Has the timer been reached?
    if (self.tethertimer < time) {
        self.tetherlock = FALSE;            // Make sure not set
        self.goalentity = self.movetarget = self.tethertarg;
        self.tethertimer = LARGE_TIMER;        // Timer no longer needed
        self.pausetime = 0;                    // Start walking
    }
    else {
        // Check to see if facing enemy is active?
        if (self.tetherturn && self.tetherfocus) {
            self.ideal_yaw = vectoyaw(self.tetherfocus.origin - self.origin);
            ChangeYaw ();            
        }
    }
};