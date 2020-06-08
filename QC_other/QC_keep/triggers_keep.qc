float COUNTER_TOGGLE    = 1.000;
float COUNTER_LOOP    = 2.000;
float COUNTER_STEP    = 4.000;
float COUNTER_RESET    = 8.000;
float COUNTER_RANDOM    = 16.000;
float COUNTER_FINISHCOUNT    = 32.000;
float COUNTER_START_ON    = 64.000;

void () counter_on_use;
void () counter_off_use;
void () oncount_use;

void () trigger_remove = { trigger_void(); };

void () trigger_setgravity_touch = {
    if (other.flags & FL_CLIENT) {
        other.gravity = self.gravity;
        other.gravtrigent = self;
        other.gravtrig = 1;
        //dprint("Gravity changed to:\n");  Removed since it spams the console every frame on developer 1
        //dprint(ftos(self.gravity));
        //dprint("\n");
    }
};

void () trigger_setgravity = {
   InitTrigger ();
   self.touch = trigger_setgravity_touch;
   if (self.gravity <= 0 || self.gravity > 1) self.gravity = 1;
};

float CMD_ACTIVATOR_ONLY = 2;
float CMD_UPDATE_ENV_VARIABLES = 4;
float CMD_DO_NOT_TERMINATE = 8;


void () command_use_server = {
    local entity pent;
    
    if (self.spawnflags & CMD_ACTIVATOR_ONLY) {
        pent = other;
        if (pent != world && pent.flags & FL_CLIENT) {
            stuffcmd(pent,self.message);
            if (!(self.spawnflags & CMD_DO_NOT_TERMINATE)) {
                stuffcmd(pent,"\n");
            }
        } else {
            if (activator != world && activator.flags & FL_CLIENT) {
                    stuffcmd(activator,self.message);
                if (!(self.spawnflags & CMD_DO_NOT_TERMINATE)) {
                    stuffcmd(activator,"\n");
                }
            }
        }
    } else {
        // Stuff commands to all players on the server
        pent = find(world,classname,"player");
        while (pent) {
            if (pent != world && pent.flags & FL_CLIENT) {
                stuffcmd(pent,self.message);
                if (!(self.spawnflags & CMD_DO_NOT_TERMINATE)) {
                    stuffcmd(pent,"\n");
                }
            }
            pent = find(world, classname, "player");
        }
    }
};

void () command_use = {
    localcmd (self.message);
    if (!(self.spawnflags & CMD_DO_NOT_TERMINATE)) {
        localcmd("\n");
    }
    if (self.classname == "info_command_spawn") {
        entity_hide(self);
    }
};

void () info_command_use_server = { command_use_server(); };
void () info_command_use = { command_use(); };

void () info_command = {
    if (self.message == "") { remove(self); return; }
    self.use = info_command_use;
    self.think = SUB_Null;
};
void () info_command_server = {
    if (self.message == "") { remove(self); return; }
    self.use = info_command_use_server;
    self.think = SUB_Null;
};

void () info_command_spawn = {
    if (self.message == "") { remove(self); return; }
    //self.spawnflags = self.spawnflags | CMD_RESEND_ON_LOAD;
    self.use = info_command_use;
    self.think = SUB_Null;
};

void () trigger_command_touch = { if (other.flags & FL_CLIENT) command_use(); };
void () trigger_command = {
    if (self.message == "") { remove(self); return; }
    InitTrigger ();
    self.touch = trigger_command_touch;
};

void () trigger_waterfall_touch = {
   if ( !(other.flags & FL_CLIENT) ) return ;
   other.velocity = (other.velocity + self.movedir);
   other.velocity_x = (other.velocity_x + (self.count * (random () - 0.500)));
   other.velocity_y = (other.velocity_y + (self.count * (random () - 0.500)));
};

void () trigger_waterfall = {
   InitTrigger ();
   self.touch = trigger_waterfall_touch;
   if ( (self.count == FALSE) ) self.count = 100.000;
   if ( (self.movedir == '0 0 0') ) self.movedir = '0 -90 0';
   if ( (self.speed == FALSE) ) {
      self.movedir = (self.movedir * 50.000);
   } else {
      self.movedir = (self.movedir * self.speed);
   }
};



void () counter_think = {
   self.cnt = (self.cnt + TRUE);
   if ( (self.spawnflags & COUNTER_RANDOM) ) {
      self.state = (random () * self.count);
      self.state = (floor (self.state) + TRUE);
   } else {
      self.state = self.cnt;
   }
   activator = other;
   SUB_UseTargets ();
   self.nextthink = (time + self.wait);
   if ( (self.spawnflags & COUNTER_STEP) ) counter_on_use ();
   if ( (self.cnt >= self.count) ) {
      self.cnt = FALSE;
      if ( (self.aflag || !(self.spawnflags & COUNTER_LOOP)) && self.spawnflags & COUNTER_TOGGLE ) { counter_on_use (); } else { remove (self); }
   }
};

void () counter_on_use = {
   if ( ((self.cnt != FALSE) && (self.spawnflags & COUNTER_FINISHCOUNT)) ) { self.aflag = TRUE; return; }
   self.use = counter_off_use;
   self.think = SUB_Null;
   self.aflag = FALSE;
};

void () counter_off_use = {
   self.aflag = FALSE;
   if ( (self.spawnflags & COUNTER_TOGGLE) ) {
      self.use = counter_on_use;
   } else {
      self.use = SUB_Null;
   }
   if ( (self.spawnflags & COUNTER_RESET) ) {
      self.cnt = FALSE;
      self.state = FALSE;
   }
   self.think = counter_think;
   if ( self.delay ) {
      self.nextthink = (time + self.delay);
   } else {
      counter_think ();
   }
};
float (entity counter) counter_GetCount = {
   if ( (counter.classname == "counter") ) return ( counter.state );
   return ( FALSE );
};

void () func_counter = {
   if ( !self.wait ) self.wait = TRUE;
   self.count = floor (self.count);
   if ( (self.count <= FALSE) ) self.count = MOVETYPE_BOUNCE;
   self.cnt = FALSE;
   self.state = FALSE;
   self.classname = "counter";
   self.use = counter_off_use;
   self.think = SUB_Null;
   if ( (self.spawnflags & COUNTER_START_ON) ) {
      self.think = counter_off_use;
      self.nextthink = (time + 0.100);
   }
};

void () oncount_use = {
   if ( (counter_GetCount (other) == self.count) ) {
      activator = other;
      SUB_UseTargets ();
   }
};

void () func_oncount = {
   self.count = floor (self.count);
   if ( (self.count <= FALSE) ) self.count = TRUE;
   self.classname = "oncount";
   self.use = oncount_use;
   self.think = SUB_Null;
};

void() change_cvar = {
    local entity check;

    cvar_set (self.netname, self.message);
    bprint("\n\n");
    self.classname = "cvar_done";
    check = find(world, classname, "player");
    if(!check) check = find(world, classname, "camera");
    check.ideal_yaw = -1;
    self.nextthink = time + 0.02;
    self.think = SUB_UseTargets;
};

void() cvarset_touch = {
    if (other.health <= 0 || other.classname != "player") return;
    change_cvar();
};

void() trigger_cvarset = {
    if (deathmatch || coop) remove (self);
    InitTrigger ();
    self.use = change_cvar;
    self.touch = cvarset_touch;
};

//- - - - - - - - -
// Update 8/19/09:  Jump/Dive Triggers

// Hipnotic's cnt stuff - - - - - - - -
// Code common to various triggers with Hip countdown on touch.
void() HipCntDown =
{
    if (self.cnt > 0)
    {
        self.cnt = self.cnt - 1;
        if (self.cnt == 0)
        {
            self.solid      = SOLID_NOT;    // PM:  Another safeguard.
            self.touch      = SUB_Null;
            self.nextthink  = time + 0.1;
            self.think      = SUB_Remove;
        }
    }
};

// Override velocity if a player jumps within one.
void() tjump_touch =
{
    if (other.health <= 0)
        return;
    if (other.classname != "player")    //if (!(other.flags & FLx_CREATURE))
        return;     // Only creatures can use jump pads.

    if (self.spawnflags & PUSH_JUMP_ONLY)
    {   // Jump only when player presses jump button.
//        if (other.fly_sound < time)
//        {
//            other.fly_sound = time + 1.5;
//            if (!(self.spawnflags & PUSH_CURRENT))
//                sound (other, CHAN_AUTO, "ambience/windfly.wav", 1, ATTN_NORM);
//        }
        if (!other.jump_time || (other.jump_time + 0.2 < time))
            return;     // Didn't jump recently.
        other.jump_time = 0;    // Time to fly!
    }
    else
    {   // Automatic use.
        if (other.flags & FL_CLIENT)    // Only clients use falling damage.
            other.jump_flag = 0;    // Don't take damage if fell on trigger.
        if (other.flags & FL_ONGROUND == 0)
            return;
    }

// Up, up, and away!
    if (!(self.spawnflags & PUSH_CURRENT))
    {   // First sound overrides jump sound.  Second sound is standard push sound.
        sound (other, CHAN_BODY, "items/reflect.wav", 1, ATTN_NORM);
        sound (other, CHAN_AUTO, "ambience/windfly.wav", 1, ATTN_NORM);
    }
    if (self.speed)
    {   // Unlike trigger_monsterjump, jump at a direction 'other' is facing.
        local   vector  vec;

        vec = '0 0 0';
        vec_y = other.angles_y;
        makevectors (vec);
        other.velocity = v_forward * self.speed;
    }
    other.velocity_z = self.height;
    other.flags = other.flags - (other.flags & FL_ONGROUND);

//    if (self.spawnflags & PUSH_ONCE)
//        {remove (self);  return;}

// Hipnotic's cnt stuff - - - - - - - -
    HipCntDown ();
//- - - - - - - - - - - - - - - - - - -
};

//============
// --> QUAKED trigger_jump (.5 .5 .5) ? PUSH_ONCE PUSH_CURRENT PUSH_JUMP_ONLY
// Pushes the player up
//============
void() trigger_jump_spawn =
{
    precache_sound ("ambience/windfly.wav");
    precache_sound ("items/reflect.wav");       // Bounce sound.

    // 'InitTrigger' already done elsewhere.
    self.touch = tjump_touch;

    if (!self.height)
        self.height = 1000;     // PM:  Use 900 in roman3 and unforgiven2.
// Hipnotic's cnt stuff - - - - - - - -
    if (!self.cnt)
        self.cnt = -1;
//- - - - - - - - - - - - - - - - - - -
};

void() trigger_jump = {InitTrigger ();  trigger_jump_spawn ();};