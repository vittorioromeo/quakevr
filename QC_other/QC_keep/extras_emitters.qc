/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/* 
    Written by Frank Condello <pox@planetquake.com>
    
    Particle systems with QuakeC - packet overflows made easy!
    
    These were modeled after particle systems in high-end 3D apps, they are
    not the most user friendly entities around...
    
    EMITER ENTITIES:
    func_emitter, func_emitter_volume
    
    EFFECTOR TRIGGERS:
    t_effector_destroy, t_effector_push, t_effector_gravity,
    t_effector_trubulence, t_effector_attract, t_effector_friction
    
    Emitters generate a "particle" (either a .spr, .mdl or .bsp) and toss it in a direction
    between the set angle limits. They can use an optional "death" model to simulate
    anything from rain, falling rubble, blasts of flames, forcefields, whatever...
    
    Effectors change the behaviour of particles that touch them (the emitter must have
    the "USEEFFECTORS" spawnflag set).
    
    Emitters can be attached to a func_train_ext, for some really neat shit :)
    
    ----------------------------------------------------------------------------------------
    NOTES:
    
    Don't go too nuts with these, stay away from the built-in sounds for high
    frequency emitters (use fx_sound instead) and just use common sense (you don't
    need a gib fountain around every corner!).
    
    There are a LOT of options - these are really generic and need to be
    tailored to specifc purposes.
    
    The lifespan of animated particles may not be exact - it will always fall on
    a multiple of the framerate (this usually isn't a problem, since framerates
    are typically much smaller than lifespans) but you should be aware of this
    limitation.
    
    Particles won't "die" when they hit another particle - this is on purpose.
    
    Particles don't register a touch on water, but a t_effector_destroy trigger can be
    used to simulate this. Set KILLINFWATER to have them die in func_water volumes.
    
    ----------------------------------------------------------------------------------------
    ANGLES EXPLAINED:
    
    The angle limits may take a while to get used to, but it's a really powerful way to
    control the direction and spread. Start by setting just v1 and h1 to get a tight stream
    in the general direction, then adjust v1/v2 and h1/h2 to within a few degrees (plus and minus)
    of the original value to increase the spread for a cone-shaped fountain. Ommit either
    h2 or v2 for a fan shaped emission, or set v1=1 v2=360 h1=1 h2=360 for a fire-works style
    spherical burst.
    
    The "emitter_angles.png" image included with this mod is a diagram that visualizes the
    horizontal and vertical angle relationship (described below):
    
    When viewing a map from the top, an H angle value of 270 points up, and moves
    clockwise, making right 360 (or 0), down 90 and left 180. Same goes for the V angle
    when viewed from the front.
    
    The particle spread is always contained from the first angle (h1/v1) to the second angle (h2/v2),
    going clockwise. So h1=10 h2=350 is NOT the same as h1=350 h2=10. (the first is a near circular spread,
    while the second is a tight 20¼ fan shaped spread).
    
    Also note that although 0 would normally be a valid replacement for 360, you should use a range
    of 1 - 360 as a value of 0 is simply ignored (though this may be desirable in certain situations).
    
*/

//============================================================================

float ABSOLUTE = 2;
float TANGENTIAL = 16;
float NOT_X = 2;
float NOT_Y = 4;
float NOT_Z = 8;

float EXTRAS_START_ON = 1;
float EMITSOLID = 2;
float REMOVEONTOUCH = 4;
float DIEAFTERANIM = 8;
float SPINPARTICLES = 16;
float STAGGERFRAMES = 32;
float KILLINFWATER = 64;
float USEEFFECTORS = 128;
float STARTSTOPPED = 256;

.float extras_v1, extras_v2, extras_h1, extras_h2;
.float frate, frate2, nfrms, nfrms2;
.float lspan, lspan2, ppe, stagcnt;

void() effector_use =
{
    if (self.state)
        self.state = FALSE;// off
    else
        self.state = TRUE;// on
};

void() effector_destroy =
{
    if (!self.state) return;// Disabled
    if (other.classname != "particle") return;
    if (!(other.owner.spawnflags & USEEFFECTORS)) return;
    if (other.trigger_field == self) return;// Already affected by this trigger
    
    if (random() < self.count) {
        self = other;
        self.think1();// kill it        
    } else
        other.trigger_field = self;// Can no longer affect this particle
};

void() effector_push =
{
    if (!self.state) return;// Disabled
    if (other.classname != "particle") return;
    if (!(other.owner.spawnflags & USEEFFECTORS)) return;
    if (other.trigger_field == self) return;// Already affected by this trigger
    
    if (random() < self.count) {// push it
        other.velocity = (other.velocity - (other.velocity*self.cnt)) + self.mangle;
        if (self.spawnflags & TANGENTIAL)
            other.angles = vectoangles(other.velocity);    
    }
    
    other.trigger_field = self;// Can no longer affect this particle
};

void() effector_gravity =
{
    if (!self.state) return;// Disabled
    if (other.classname != "particle") return;
    if (!(other.owner.spawnflags & USEEFFECTORS)) return;
    if (other.trigger_field == self) return;// Already affected by this trigger
    
    if (random() < self.count) {// modify gravity
        
        if (self.spawnflags & ABSOLUTE)
            other.gravity = self.gravity;
        else
            other.gravity = other.gravity + self.gravity;
    }
    
    other.trigger_field = self;// Can no longer affect this particle
};

void() effector_turbulence =
{
    local float r;
    
    if (!self.state) return;// Disabled
    if (other.classname != "particle") return;
    if (!(other.owner.spawnflags & USEEFFECTORS)) return;
    if (other.trigger_field == self) return;// Already affected by this trigger
    
    if (random() < self.count) {// add turbulence
        
        if (random() > 0.5) r = -1; else r = 1;
        
        if (!(self.spawnflags & NOT_X))
            other.velocity_x = (other.velocity_x - (other.velocity_x*self.cnt)) + r*(random()*self.speed);
        if (!(self.spawnflags & NOT_Y))
            other.velocity_y = (other.velocity_y - (other.velocity_y*self.cnt)) + r*(random()*self.speed);
        if (!(self.spawnflags & NOT_Z))
            other.velocity_z = (other.velocity_z - (other.velocity_z*self.cnt)) + r*(random()*self.speed);
            
        if (self.spawnflags & TANGENTIAL)
            other.angles = vectoangles(other.velocity);    
    }
    
    other.trigger_field = self;// Can no longer affect this particle
};

// attrators keep influencing particles that are caught
void() effector_attract =
{
    local vector    dir;
    
    if (!self.state) return;// Disabled
    if (other.classname != "particle") return;
    if (!(other.owner.spawnflags & USEEFFECTORS)) return;
    if (other.frags) return;// Already failed to catch this one    
    if (other.teleport_time > time) return;// Timed out
    
    if (random() < self.count || other.trigger_field == self) {
        dir = normalize(self.dest - other.origin);
        other.velocity = (other.velocity - (other.velocity*self.cnt)) + (dir*self.speed);
        if (self.spawnflags & TANGENTIAL)
            other.angles = vectoangles(other.velocity);        
    }
    else
        other.frags = TRUE;// can't touch this one
    
    other.teleport_time = time + 0.1;// Time out
    other.trigger_field = self;// clear other triggers, and guarentee re-influence
    
};

// friction triggers keep influencing particles that are caught
void() effector_friction =
{
    if (!self.state) return;// Disabled
    if (other.classname != "particle") return;
    if (!(other.owner.spawnflags & USEEFFECTORS)) return;
    if (other.frags) return;// Already failed to catch this one    
    if (other.teleport_time > time) return;// Timed out
    
    if (random() < self.count || other.trigger_field == self)
        other.velocity = other.velocity - (other.velocity*self.speed);    
    else
        other.frags = TRUE;// can't touch this one
    
    other.teleport_time = time + 0.1;// Time out
    other.trigger_field = self;// clear other triggers, and guarentee re-influence
    
};


/*QUAKED t_effector_friction (0 .5 .8) ? EXTRAS_START_ON
Applies friction to particles, slowing them down (negative values speed particles up)

"speed"            Amount of Friction (max 100 = dead stop, negative values can go further though -200 etc.)
"count"            Override the default 100% of particles affected (valid: 1-99 i.e 50 affects roughly 50% of the particles)
"targetname"    Can be enabled/disabled

Spawnflags
"EXTRAS_START_ON"        Start in enabled state even if targeted

*/
void() t_effector_friction =
{
    InitTrigger ();
    
    if (!self.count || self.count<0 || self.count>100)
        self.count = 100;
    
    self.count = self.count*0.01;
    
    self.speed = self.speed/100.0;
    if (self.speed > 1.0) self.speed = 1.0;
    
    if (self.targetname != string_null && !(self.spawnflags & EXTRAS_START_ON))
        self.state = FALSE;// off
    else
        self.state = TRUE;// on
        
    self.use = effector_use;
    self.touch = effector_friction;
};

/*QUAKED t_effector_attract (0 .5 .8) ? EXTRAS_START_ON
Particles touching this trigger are attracted to it's centre. Can also repel particles with
negative strength. This works best with particles that have a low gravity setting.

"speed"            Strength of the attraction
"cnt"            Percentage of original particle velocity to diminish
"count"            Override the default 100% of particles affected (valid: 1-99 i.e 50 affects roughly 50% of the particles)
"targetname"    Can be enabled/disabled

Spawnflags
"EXTRAS_START_ON"        Start in enabled state even if targeted
"TANGENENTAL"    Particles turn to face new direction

*/
void() t_effector_attract =
{
    InitTrigger ();
    
    if (!self.count || self.count<0 || self.count>100)
        self.count = 100;
    
    self.count = self.count*0.01;

    if (self.cnt<0) self.cnt = 0;
    if (self.cnt>100) self.cnt = 100;
    
    self.cnt = self.cnt*0.01;
    
    if (!self.speed)
        self.speed = 280;
    
    // Save out the physical center
    self.dest_x = (self.mins_x + self.maxs_x)*0.5;
    self.dest_y = (self.mins_y + self.maxs_y)*0.5;
    self.dest_z = (self.mins_z + self.maxs_z)*0.5;
    
    if (self.targetname != string_null && !(self.spawnflags & EXTRAS_START_ON))
        self.state = FALSE;// off
    else
        self.state = TRUE;// on
        
    self.use = effector_use;
    self.touch = effector_attract;
};


/*QUAKED t_effector_trubulence (0 .5 .8) ? EXTRAS_START_ON
Pushes particles at random speeds in all directions (can exlude certain axii)

"speed"            Max amount of turbulence
"cnt"            Percentage of original particle velocity to diminish
"count"            Override the default 100% of particles affected (valid: 1-99 i.e 50 affects roughly 50% of the particles)
"targetname"    Can be enabled/disabled

Spawnflags
"EXTRAS_START_ON"        Start in enabled state even if targeted
"NOT_X"            Don't affect X direction
"NOT_Y"            Don't affect Y direction
"NOT_Z"            Don't affect Z direction
"TANGENENTAL"    Particles turn to face new direction

*/
void() t_effector_trubulence =
{
    InitTrigger ();
    
    if (!self.count || self.count<0 || self.count>100)
        self.count = 100;
    
    self.count = self.count*0.01;
    
    if (!self.speed)
        objerror("t_effector_trubulence with no velocity");

    if (self.cnt<0) self.cnt = 0;
    if (self.cnt>100) self.cnt = 100;
    
    self.cnt = self.cnt*0.01;
    
    self.speed = fabs(self.speed);
    
    if (self.targetname != string_null && !(self.spawnflags & EXTRAS_START_ON))
        self.state = FALSE;// off
    else
        self.state = TRUE;// on
        
    self.use = effector_use;
    self.touch = effector_turbulence;
};


/*QUAKED t_effector_gravity (0 .5 .8) ? EXTRAS_START_ON ABSOLUTE
Modifies particle gravity

"gravity"        Add this to current gravity value, or change to this value if  "ABSOLUTE" is set
"count"            Override the default 100% of particles affected (valid: 1-99 i.e 50 affects roughly 50% of the particles)
"targetname"    Can be enabled/disabled

Spawnflags
"EXTRAS_START_ON"        Start in enabled state even if targeted
"ABSOLUTE"        Particles use new gravity outright

*/
void() t_effector_gravity =
{
    InitTrigger ();
    
    if (!self.count || self.count<0 || self.count>100)
        self.count = 100;
    
    self.count = self.count*0.01;
    
    if (self.targetname != string_null && !(self.spawnflags & EXTRAS_START_ON))
        self.state = FALSE;// off
    else
        self.state = TRUE;// on
        
    self.use = effector_use;
    self.touch = effector_gravity;
};

/*QUAKED t_effector_push (0 .5 .8) ? EXTRAS_START_ON ABSOLUTE
Pushes particles in a new direction

"mangle"        X Y Z speed
"cnt"            Percentage of original particle velocity to diminish
"count"            Override the default 100% of particles affected (valid: 1-99 i.e 50 affects roughly 50% of the particles)
"targetname"    Can be enabled/disabled

Spawnflags
"EXTRAS_START_ON"        Start in enabled state even if targeted
"TANGENENTAL"    Particles turn to face new direction

*/
void() t_effector_push =
{
    InitTrigger ();
    
    if (!self.count || self.count<0 || self.count>100)
        self.count = 100;
    
    self.count = self.count*0.01;
    
    if (self.cnt<0) self.cnt = 0;
    if (self.cnt>100) self.cnt = 100;
    
    self.cnt = self.cnt*0.01;
    
    // NOTE: mangle of '0 0 0' is ok (let gravity take over)
    
    if (self.targetname != string_null && !(self.spawnflags & EXTRAS_START_ON))
        self.state = FALSE;// off
    else
        self.state = TRUE;// on
        
    self.use = effector_use;
    self.touch = effector_push;
};

/*QUAKED t_effector_destroy (0 .5 .8) ? EXTRAS_START_ON
Kills particles that touch it

"count"            Override the default 100% obliteration (valid: 1-99 i.e 50 affects roughly 50% of the particles)
"targetname"    Can be enabled/disabled

Spawnflags
"EXTRAS_START_ON"        Start in enabled state even if targeted

*/
void() t_effector_destroy =
{
    InitTrigger ();
    
    if (!self.count || self.count<0 || self.count>100)
        self.count = 100;
    
    self.count = self.count*0.01;
    
    if (self.targetname != string_null && !(self.spawnflags & EXTRAS_START_ON))
        self.state = FALSE;// off
    else
        self.state = TRUE;// on
        
    self.use = effector_use;
    self.touch = effector_destroy;
};

//============================================================================

// Return a particle origin for a brush volume or point entity
vector() particle_origin = 
{
    local vector org;

    // Point
    if (!self.aflag) {    
        if (self.owner)// Adjust origin relative to the parent train
            org = self.origin + self.dest;
        else
            org = self.dest;
    
    // Volume (submodels always have an origin of '0 0 0' so they're always in sync with the parent train)
    } else {
        org_x = self.absmin_x + (random() * self.size_x);
        org_y = self.absmin_y + (random() * self.size_y);
        org_z = self.absmin_z + (random() * self.size_z);
    }
    
    return org;
};

// Play death model sequence
void() particle_death_anim =
{    
    if (self.frame+1 == self.owner.nfrms2) {// done
        remove(self);
        return;
    }
    
    self.frame = self.frame + 1;
    self.nextthink = time + self.owner.frate2;
};

// Particle is done
void() particle_die =
{
    self.classname = "p_dying";// So effectors ignore dying particles
    
    if (self.owner.noise2)
        sound (self, CHAN_VOICE, self.owner.noise2, 1, ATTN_NORM);    
    
    if (!self.owner.mdl2) {
        remove(self);
        return;
    }
    
    self.solid = SOLID_NOT;// Change this and DIE!
    self.movetype = MOVETYPE_NONE;
    self.velocity = '0 0 0';
    setorigin(self, self.origin + '0 0 4');// kick it up a bit
    self.frame = 0;
    setmodel (self, self.owner.mdl2);
    self.think = particle_death_anim;
    self.nextthink = time + self.owner.frate2;
};

// Animate Particles
void() particle_think =
{
    if (self.ltime < time) {// done
        particle_die();
        return;
    }
    
    if (self.frame+1 == self.owner.nfrms) {
        if (self.owner.spawnflags & DIEAFTERANIM) {// done    
            particle_die();
            return;
        } else
            self.frame = 0;// loop
    }
    else
        self.frame = self.frame + 1;
    
    self.nextthink = time + self.owner.frate;
};

// Do damage and play hit sound
void() particle_touch =
{
    // Don't die on other particles
    if (other.classname == "particle")
        return;
        
    if (self.cnt == 3)
        return; // Don't go nuts (especially if caught up)
    
    if (!self.cnt) {// Only play sound on first hit
        if (self.owner.noise1)
            sound (self, CHAN_VOICE, self.owner.noise1, 1, ATTN_NORM);    
    }
    
    self.cnt = self.cnt + 1;
        
    if (self.owner.dmg != 0)
    {
        if (other.takedamage)
        {
            if (self.attack_finished < time)
            {            
                if (self.owner.dmg > 0 && self.owner.weapon >= TE_SPIKE)// Negative damage heals
                    spawn_touchblood (self, other, self.owner.dmg);
                
                T_Damage (other, self, self, self.owner.dmg,DAMARMOR);
                self.attack_finished = time + 1;// Don't go nuts if particle doesn't die on touch
            }
        }
        // Do a temp ent effect
        else if (self.owner.weapon >= TE_SPIKE)
        {
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, self.owner.weapon);
            WriteCoord (MSG_BROADCAST, self.origin_x);
            WriteCoord (MSG_BROADCAST, self.origin_y);
            WriteCoord (MSG_BROADCAST, self.origin_z);
        }
    }
    
    if (self.owner.spawnflags & REMOVEONTOUCH)// done
        particle_die();
};

//============================================================================

.float ang_ofs;
void() emitter_pause;
void() emitter_emit;

float(string type) emitter_checkrelay =
{
    if (self.old_velocity == '0 0 0') return FALSE;// no spin
    
    if (type == "t_changedir") {        
        if (self.state == REVERSE)
            self.state = FORWARD;
        else
            self.state = REVERSE;
        return TRUE;
    }
    else if (type == "t_forward") {
        self.mangle = self.old_velocity;
        self.state = FORWARD;
        return TRUE;
    }
    else if (type == "t_reverse") {
        self.mangle = self.old_velocity;
        self.state = REVERSE;
        return TRUE;
    }
    else if (type == "t_go") {
        self.mangle = self.old_velocity;
        return TRUE;    
    }
    else if (type == "t_stop") {
        self.mangle = '0 0 0';
        return TRUE;
    }
    
    return FALSE;
};

void() emitter_start =
{    
    if (emitter_checkrelay(other.classname)) return;// spin trigger
    
    self.use = emitter_pause;// next use stops it
    self.nextthink = time + 0.01;
};

void() emitter_pause = {
    if (emitter_checkrelay(other.classname)) return;// spin trigger
    
    // Wait around for next use
    self.nextthink = -1;
    self.use = emitter_start;
};

void() emitter_emit = {
    local entity p;
    local vector dir;
    
    if (self.noise != string_null && self.count == 1)// Play emitter noise
        sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
        
    // Emit a particle
    p = spawn ();
    p.owner = self;
    p.origin = particle_origin();
    setorigin (p, p.origin);
    setmodel (p, self.mdl);
    setsize (p, VEC_ORIGIN, VEC_ORIGIN);
    
    p.solid = self.cnt;
    p.movetype = self.style;
    p.gravity = self.gravity;
    p.effects = self.drag;
    p.touch = particle_touch;
    p.classname = "particle";
    p.think1 = particle_die;// So other triggers can kill'em
    
    // Get the direction for this particle    
    dir_x = SUB_RandomRange(self.extras_v1, self.extras_v2);    
    dir_y = SUB_RandomRange(self.extras_h1, self.extras_h2);
    dir_z = 0;

//    if (dir_x >= 180) dir_x = dir_x - 360;
//    if (dir_y >= 180) dir_y = dir_y - 360;

    // Add emitter rotation
    if (self.mangle != '0 0 0') {    
    
        if (self.state == FORWARD)
            self.ang_ofs = self.ang_ofs + 0.01;
        else
            self.ang_ofs = self.ang_ofs - 0.01;

        if (self.ang_ofs == 360) self.ang_ofs = 0;
        if (self.ang_ofs == -360) self.ang_ofs = 0;
        
        dir = dir + (self.mangle*self.ang_ofs);
    }    
    
    // Fling it...
    makevectors(dir);
    p.velocity = v_forward * SUB_RandomRange(self.speed, self.speed2);
    
    // Face the move direction (doesn't affect parallel sprites)
    p.angles = vectoangles(p.velocity);
    
    // Put a spin on the particle (doesn't affect parallel sprites)
    if (self.spawnflags & SPINPARTICLES) {
        p.avelocity = dir;
        p.avelocity_z = random()*280;
    }
    
    // Life span
    p.ltime = time + SUB_RandomRange(self.lspan, self.lspan2);
    
    // Think
    if (!self.nfrms) {// Not animated    
        p.frame = self.frame;
        p.think = particle_die;    
        p.nextthink = p.ltime;
    }
    else {// Animated

        if (self.spawnflags & STAGGERFRAMES) {// Stagger start frames
            if (self.stagcnt >= self.nfrms)
                self.stagcnt = 0;
            else {
                p.frame = self.stagcnt;
                self.stagcnt = self.stagcnt + 1;
            }
        }
        
        p.think = particle_think;
        p.nextthink = time + self.frate;    
    }
    
    // keep emitting till ppe is met
    if (self.ppe > 1) {    
        if (self.count == self.ppe)// done
            self.count = 1;
        else {
            self.count = self.count + 1;
            self.frags = self.frags + 1;
            
            if (self.frags > 12) {// Avoid stack overflows
                self.nextthink = time+0.01;
                self.frags = 0;
                return;
            } else
                emitter_emit();// This may not be a good idea...

            return;
        }
    }
    
    // Done for now
    if (self.wait == -1) {
        emitter_pause();
        return;
    }
    
    // Fire off another burst when ready
    self.nextthink = time + SUB_RandomRange(self.wait, self.wait2);
};

void() emitter_find =
{
    // Check for a paret train
    //if (self.target) train_ext_linkchild();
    
    // Start it up
    self.think = emitter_emit;
    if (self.targetname != string_null && !(self.spawnflags & EXTRAS_START_ON))
        emitter_pause();
    else
        emitter_start();
};

// Clean up the mess of vars into something we can use
void() emitter_init =
{
    local float t;
    
    if (!self.mdl)
        objerror("func_emitter: No model specified");
    else
        precache_model(self.mdl);
    
    if (self.mdl2) precache_model(self.mdl2);
    
    // Copy off the angle velocity
    if (self.avelocity != '0 0 0' && !(self.spawnflags & STARTSTOPPED)) {
        self.mangle = self.avelocity;
        self.old_velocity = self.avelocity;
        self.avelocity = '0 0 0';// probably don't have to do this, but...
    }
    
    self.state = FORWARD;// spin this way
    
    if ((!self.wait || self.wait < 0) && self.wait != -1) self.wait = 0.1;
    if (self.wait2 < 0) self.wait2 = 0;
    
    // wait2 has to be greater than wait if defined
    if (self.wait != -1) {
        if (self.wait2 && (self.wait2 < self.wait)) {
            t = self.wait;
            self.wait = self.wait2;
            self.wait2 = t;
        }
    }
    
    // cap the horizontal angle
    if (self.extras_h1 > 360) self.extras_h1 = 360;
    if (self.extras_h1 < 0) self.extras_h1 = 0;
    if (self.extras_h2 < 0) self.extras_h2 = 0;// ignored
    if (self.extras_h2 > 360) self.extras_h2 = 360;
    
    // invert H so everything's clockwise
    if (self.extras_h1 != 0 && self.extras_h1 != 360) self.extras_h1 = 360 - self.extras_h1;
    if (self.extras_h2 != 0 && self.extras_h2 != 360) self.extras_h2 = 360 - self.extras_h2;
    if (self.extras_h2) {
        t = self.extras_h1;
        self.extras_h1 = self.extras_h2;
        self.extras_h2 = t;
    }

    // h2 has to be greater than h1 if defined
    if (self.extras_h2 && (self.extras_h2 < self.extras_h1)) {
        self.extras_h1 = self.extras_h1 - 360;// So it spans the right way
    }
    
    // cap the vertical angle
    if (self.extras_v1 > 360) self.extras_v1 = 360;
    if (self.extras_v1 < 0) self.extras_v1 = 0;
    if (self.extras_v2 < 0) self.extras_v2 = 0;// ignored
    if (self.extras_v2 > 360) self.extras_v2 = 360;
    // extras_v2 has to be greater than v1 if defined
    if (self.extras_v2 && (self.extras_v2 < self.extras_v1)) {
        self.extras_v1 = self.extras_v1 - 360;// So it spans the right way
    }
    
//    if (self.extras_h1 == 0) self.extras_h1 = 360;
//    if (self.extras_v1 == 0) self.extras_v1 = 360;
    
    if (self.noise) precache_sound(self.noise);
    if (self.noise1) precache_sound(self.noise1);
    if (self.noise2) precache_sound(self.noise2);
    
    // keep the movetype in check
    if (self.style) {// MOVETYPE_NONE is ok, but not very exciting
        if (self.style != MOVETYPE_TOSS && self.style != MOVETYPE_BOUNCE && self.style != MOVETYPE_FLY)
            self.style = MOVETYPE_TOSS;
    }
    
    // check for EF_ flags (should really only allow one at a time here)
    if (self.effects) {
        self.drag = self.effects;
        self.effects = 0;
    }
    
    if (self.speed < 0) self.speed = 0;
    if (self.speed2 < 0) self.speed2 = 0;
    
    // speed2 has to be greater than speed if defined
    if (self.speed2 && (self.speed2 < self.speed)) {
        t = self.speed;
        self.speed = self.speed2;
        self.speed2 = t;
    }
    
    if (!self.lspan || self.lspan < 0) self.lspan = 3;
    if (self.lspan2 < 0) self.lspan2 = 0;
    
    // lspan2 has to be greater than lspan if defined
    if (self.lspan2 && (self.lspan2 < self.lspan)) {
        t = self.lspan;
        self.lspan = self.lspan2;
        self.lspan2 = t;
    }
    
    if (self.nfrms < 0) self.nfrms = 0;
    if (self.nfrms2 < 0) self.nfrms2 = 0;
    if (!self.frate || self.frate < 0) self.frate = 0.1;
    if (!self.frate2 || self.frate2 < 0) self.frate2 = 0.1;
    
    self.stagcnt = self.frame;// start frame may not be 0
    
    self.count = 1;// ppe is not zero based
    
    // save off solid type to be copied off to particles
    if (self.spawnflags & EMITSOLID)
        self.cnt = SOLID_BBOX;// Blocking
    else if (self.dmg || self.spawnflags & REMOVEONTOUCH || self.spawnflags & USEEFFECTORS)
        self.cnt = SOLID_TRIGGER;// Passthrough, but still touch
    else
        self.cnt = SOLID_NOT;// Passthrough, no touch
    
    if (self.weapon > TE_TELEPORT)
        self.weapon = -1;
    
    // wait for targets to spawn
    self.nextthink = time + 0.2;// start after first frame to allow train to move to it's startpoint
    self.think = emitter_find;
};

/*QUAKED func_emitter_volume (0 .5 .8) ? EXTRAS_START_ON EMITSOLID REMOVEONTOUCH DIEAFTERANIM STAGGERFRAMES KILLINFWATER USEEFFECTORS STARTSTOPPED
Emit particles from a random spot within a brush volume.

*/
void() func_emitter_volume =
{
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    setmodel (self, self.model);
    setsize (self, self.mins , self.maxs);
    setorigin (self, self.origin);
    self.modelindex = 0;
    self.model = "";
    self.classname = "emitter";
    
    self.aflag = TRUE;// volume origin
    emitter_init();
};

/*QUAKED func_emitter (0 .5 .8) (-8 -8 -8) (8 8 8) ? EXTRAS_START_ON EMITSOLID REMOVEONTOUCH DIEAFTERANIM STAGGERFRAMES KILLINFWATER USEEFFECTORS STARTSTOPPED
Emit particles from a fixed point.

*/
void() func_emitter =
{
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.dest = self.origin;// in case it's linked
    self.classname = "emitter";
    
    self.aflag = FALSE;// point origin
    emitter_init();
};

/* The following apply to both func_emitter and func_emitter_volume

-EMITTER OPTIONS-
"wait"            Delay between particle emissions (-1 is a single burst, then pause)
"wait2"            If set, delay is a random number between "wait" and "wait2" (unless "wait" is -1)
"ppe"            Particles per emission (default is 1)
"h1"            Toss particles along this horizontal (yaw) angle (1¼ - 360¼)
"h2"            If set, tossess particles along a random horizontal angle between "h1" and "h2"
"v1"            Toss particles along this vertical (pitch) angle (1¼ - 360¼) 270 is up, 90 is down.
"v2"            If set, tossess particles along a random vertical angle between "v1" and "v2"
"avelocity"        X Y Z emitter angle velocity (IMPORTANT: rotates after each emittion, this is NOT time based!)
"noise"            Noise the emitter makes when spawning a new particle (USE WITH CAUTION WITH HIGH FREQUENCY EMITTERS!)
"target"        Attach the emitter to a func_train_ext
"targetname"     Can be triggered on/off

-PARTICLE OPTIONS-
"style"            Particle movetype (MOVETYPE_BOUNCE, MOVETYPE_TOSS, MOVETYPE_FLY or MOVETYPE_NONE)
"speed"            Speed of the particles
"speed2"        If set, speed is a random number between "speed" and "speed2"
"lspan"            Life span of particles in seconds
"lspan2"        If set, life span is a random number between "lspan" and "lspan2"
"mdl"            Path to the sprite or model file to use as particles
"frame"            Start frame (model/sprite frame)
"nfrms"            Number of animation frames
"frate"            Override the default 10fps (0.1) animation rate
"gravity"        Alter gravity G = sv_gravity*self.gravity (NOTE: 0 or 1 is unaltered - use 0.001 for "zero G")
"dmg"            Damage particles do when they hit something
"effects"        EF_MUZZLEFLASH, EF_DIMLIGHT, EF_BRIGHTLIGHT or EF_BRIGHTFIELD
"noise1"        Noise particles make when they hit something (USE WITH CAUTION WITH HIGH FREQUENCY EMITTERS!)
"noise2"        Noise particles make when they die (USE WITH CAUTION WITH HIGH FREQUENCY EMITTERS!)
"message"        Death message if player is killed by a particle

-DEATH MODEL OPTIONS-
"weapon"        If "dmg" is set (and positive) a particle acts like a projectile and spawns blood/sparks when doing damage or
                this TE_ effect when hitting the world. (from 0 - 11) TE_SPIKE, TE_SUPERSPIKE, TE_GUNSHOT, TE_EXPLOSION,
                TE_TAREXPLOSION, TE_LIGHTNING1, TE_LIGHTNING2, TE_WIZSPIKE, TE_KNIGHTSPIKE, TE_LIGHTNING3, TE_LAVASPLASH, TE_TELEPORT
                (use -1 for no effect, default is TE_SPIKE)
"mdl2"            Path to the sprite or model to become when life span expires
"nfrms2"        Number of animation frames for "death" animations
"frate2"        Override the default 10fps (0.1) animation rate for the "death" model

-SPAWNFLAGS-
"EXTRAS_START_ON"        Emitters with targetnames will wait for a trigger event unless this is set
"EMITSOLID"        Particles are solid, block players, and collide with each other
"REMOVEONTOUCH" Remove particles (or do death animation) on touch
"DIEAFTERANIM"    Remove particles (or do death animation) after reaching the last animation frame (loop animation otherwise)
"SPINPARTICLES" Gives particles semi-random angle velocity (doesn't affect parallel sprites)
"STAGGERFRAMES" Each new particle begins with the next sequential animation frame
                (NOTE: DIEAFTERANIM will still kill it after the absolute "last" frame)
"KILLINFWATER"    Remove particles (or do death animation) when in contact with func_water
"USEEFFECTORS"    Partciles generated by this emitter are affected by effector triggers
"STARTSTOPPED"    Emitter won't spin until it's used by a trigger_go/reverse/forward (only affects emitters with avelocity)

*/

//============================================================================
