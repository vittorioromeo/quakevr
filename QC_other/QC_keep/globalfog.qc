/*======================================================================
/*QUAKED info_stuffcmd (0 0.5 0) (-8 -8 -8) (8 8 8)
puts the specified message into the console when the client spawns.
Original idea from Honey codebase
-------- KEYS --------
message : Stuff command (r_wateralpha 0.3)
-------- SPAWNFLAGS --------
-------- NOTES --------
puts the specified message into the console when the client spawns.
======================================================================*/
void() stuffcmd_think =
{
    // wait key used for commands that are development mode only
    if (!self.wait || (self.wait && developer)) {
        if (client_ent && client_ent.flags & FL_CLIENT) {
            stuffcmd(client_ent, self.message);
            stuffcmd(client_ent, "\n");
        }
        // No client in game, check again later
        else self.nextthink = time + 0.2;
    }
}

void() info_stuffcmd =
{    
    // No message key, remove dead entity
    if(!self.message) {
        dprint("[info_stuffcmd] no message found, removing\n");
        SUB_Remove();
    }
    if(!self.wait) self.wait = 0; 

    // Wait for client to start game
    self.nextthink = time + 2;
    self.think = stuffcmd_think;
}

//----------------------------------------------------------------------
// Returns power of 10 for string format function below
//----------------------------------------------------------------------
float(float pow_loop) pow10 =
{
    local float pow_num, pow_counter;
    pow_num = 10;
    pow_counter = 1;
    if (pow_loop < 1) pow_loop = 1;
            
    while (pow_counter < pow_loop) {
        pow_num = pow_num * 10;
        pow_counter = pow_counter + 1;
    }
    return pow_num;
}; 

//----------------------------------------------------------------------
// (Long Float TO String) Format and print a number to the console
// - str_player : needs to be a client for various buffer commands
// - buffer : BUFFER_STUFFCMD, BUFFER_SPRINT, BUFFER_DPRINT
void(entity str_player, float str_num, float topbit, float botbit, float buffer) lftos =
{
    local float str_counter, str_pow, str_result, str_remain, str_first;
    
    str_counter = topbit;        // Upper number length
    str_pow = pow10(topbit);    // Setup initial base 10 power for checking
    str_remain = FALSE;            // Decimal point been done yet
    str_first = FALSE;            // First non-zero digit printed

    // Deal with negative state first
    if (str_num < 0) {
        if (buffer & BUFFER_STUFFCMD) stuffcmd(str_player, "-");
        else if (buffer & BUFFER_SPRINT) sprint(str_player, "-");
        else if (buffer & BUFFER_DPRINT) dprint("-");
    }
    // Get rid of negative state
    str_num = fabs(str_num);
    
    // If number larger than fixed length? - truncate number
    if (str_num > str_pow)
        str_num = str_num - (floor(str_num / str_pow) * str_pow);

    // Loop through upper part of number
    while(str_counter > 0) {
        // Move down one pow digit
        str_pow = str_pow / 10;

        // Divide number by pow to get divider
        str_result = floor(str_num / str_pow);
        
        if (str_result > 0 || str_first) {
            str_first = TRUE;
            // Print digit to buffer type
            if (buffer & BUFFER_STUFFCMD) stuffcmd(str_player, ftos(str_result));
            else if (buffer & BUFFER_SPRINT) sprint(str_player, ftos(str_result));
            else if (buffer & BUFFER_DPRINT) dprint(ftos(str_result));
        }
        
        // remove top part of number, decrease str length counter
        str_num = str_num - (str_result * str_pow);
        str_counter = str_counter - 1;
    }
    
    // Is there any remainder?
    if (str_counter < 1) {
        // Print decimal point to buffer type
        if (!str_first) {
            str_first = TRUE;
            if (buffer & BUFFER_STUFFCMD) stuffcmd(str_player, "0.");
            else if (buffer & BUFFER_SPRINT) sprint(str_player, "0.");
            else if (buffer & BUFFER_DPRINT) dprint("0.");
        }
        else {
            if (buffer & BUFFER_STUFFCMD) stuffcmd(str_player, ".");
            else if (buffer & BUFFER_SPRINT) sprint(str_player, ".");
            else if (buffer & BUFFER_DPRINT) dprint(".");
        }
        if (botbit <1) botbit=1;
        str_counter = botbit;
        while (botbit > 0) { 
            str_num = str_num * 10; 
            botbit = botbit -1;
        }
        str_pow = pow10(str_counter);
    }

    // Loop through lower part of number
    while(str_counter > 0) {
        // Move down one pow digit
        str_pow = str_pow / 10;

        // Divide number by pow to get divider
        str_result = floor(str_num / str_pow);
        
        if (str_result > 0 || str_first) {
            str_first = TRUE;
            // Print digit to buffer type
            if (buffer & BUFFER_STUFFCMD) stuffcmd(str_player, ftos(str_result));
            else if (buffer & BUFFER_SPRINT) sprint(str_player, ftos(str_result));
            else if (buffer & BUFFER_DPRINT) dprint(ftos(str_result));
        }
        
        // remove top part of number, decrease str length counter
        str_num = str_num - (str_result * str_pow);
        str_counter = str_counter - 1;
    }
};

/*======================================================================
 GLOBAL FOG
 Changes fog colour and density to a certain value
======================================================================*/
void(entity client) change_fog  =
{
    if (!fog_control) return;
    
    // Update fog parameters to console buffer
    stuffcmd(client, "\nfog ");
    if (ext_dpfog) lftos(client, fog_control.fog_currden+0.05, 1,3, BUFFER_STUFFCMD); 
    else lftos(client, fog_control.fog_currden, 1,3, BUFFER_STUFFCMD);
    
    stuffcmd(client, " "); 
    lftos(client, fog_control.fog_currcol_x, 1,3, BUFFER_STUFFCMD); stuffcmd(client, " "); 
    lftos(client, fog_control.fog_currcol_y, 1,3, BUFFER_STUFFCMD); stuffcmd(client, " "); 
    lftos(client, fog_control.fog_currcol_z, 1,3, BUFFER_STUFFCMD); 
    // Extra fog parameters for DarkPlaces extended engines
    if (ext_dpfog) {
        stuffcmd(client, " ");
        stuffcmd(client, fog_control.fog_dpextra);
    }
    stuffcmd(client, "\n");
};

//----------------------------------------------------------------------
// Make value is not below or above limits and or negative!
//----------------------------------------------------------------------
float(float fogrange) fog_check_range =
{
    if (fogrange < 0.01) return 0.01;
    if (fogrange > 1) return 1;
    return fabs(fogrange);
};

//----------------------------------------------------------------------
void() fog_controller_think =
{
    // Has there been a change detected?
    if (self.state == STATE_SETUP) {
        // If fog fade time too low, instantly switch
        if (self.fog_difftime <= FADEFOG_TIME) {
            self.state = STATE_OFF;
            self.fog_currden = self.fog_targden;
            self.fog_currcol = self.fog_targcol;
            change_fog(self.owner);
        }
        else {
            // Setup gradual fade of fog over time
            self.state = STATE_ON;
            // divide the fade time into segments (minimum 0.05)
            // Multiple the time by segments so diff is correct
            // Time segements less than 0.05 does not work v well
            self.fog_steptime = self.fog_difftime / FADEFOG_TIME;
            self.fog_difftime = self.fog_steptime * FADEFOG_TIME;
            
            // Work out fade directions for density/colour
            self.fog_diffden = self.fog_targden - self.fog_currden;
            self.fog_diffcol = self.fog_targcol - self.fog_currcol;

            // Debug info only
            /*----------------------------------------------------------------------
            dprint("Curr ("); 
            lftos(self.owner, self.fog_currden, 1,3, BUFFER_DPRINT); dprint(")(");
            lftos(self.owner, self.fog_currcol_x, 1,3, BUFFER_DPRINT); dprint(" ");
            lftos(self.owner, self.fog_currcol_y, 1,3, BUFFER_DPRINT); dprint(" ");
            lftos(self.owner, self.fog_currcol_z, 1,3, BUFFER_DPRINT); dprint(")\n");
            dprint("Targ ("); 
            lftos(self.owner, self.fog_targden, 1,3, BUFFER_DPRINT); dprint(")(");
            lftos(self.owner, self.fog_targcol_x, 1,3, BUFFER_DPRINT); dprint(" ");
            lftos(self.owner, self.fog_targcol_y, 1,3, BUFFER_DPRINT); dprint(" ");
            lftos(self.owner, self.fog_targcol_z, 1,3, BUFFER_DPRINT); dprint(")\n");
            dprint("Diff ("); 
            lftos(self.owner, self.fog_diffden, 1,3, BUFFER_DPRINT); dprint(")(");
            lftos(self.owner, self.fog_diffcol_x, 1,3, BUFFER_DPRINT); dprint(" ");
            lftos(self.owner, self.fog_diffcol_y, 1,3, BUFFER_DPRINT); dprint(" ");
            lftos(self.owner, self.fog_diffcol_z, 1,3, BUFFER_DPRINT); dprint(")\n");
            //----------------------------------------------------------------------*/

            // Work out step ammount to fade smoothly over time
            self.fog_diffden = self.fog_diffden / self.fog_steptime;
            self.fog_diffcol_x = self.fog_diffcol_x / self.fog_steptime;
            self.fog_diffcol_y = self.fog_diffcol_y / self.fog_steptime;
            self.fog_diffcol_z = self.fog_diffcol_z / self.fog_steptime;
        }
    }
    // Is the fog fade over? Finite amount of steps
    else if (self.fog_steptime < 0 && self.state != STATE_OFF) {
        self.state = STATE_OFF;
        self.fog_currden = self.fog_targden;
        self.fog_currcol = self.fog_targcol;
        
        // Check density and colour values are within range
        self.fog_currden = fog_check_range(self.fog_currden);
        self.fog_currcol_x = fog_check_range(self.fog_currcol_x);
        self.fog_currcol_y = fog_check_range(self.fog_currcol_y);
        self.fog_currcol_z = fog_check_range(self.fog_currcol_z);
        
        change_fog(self.owner);
    }
    // If the fade is active, keep adding steps
    else if (self.state == STATE_ON) {
        self.fog_steptime = self.fog_steptime - 1;
        self.fog_currden = self.fog_currden + self.fog_diffden;
        self.fog_currcol = self.fog_currcol + self.fog_diffcol;
        change_fog(self.owner);
    }

    // Keep on looping around using min time step
    self.think = fog_controller_think;
    self.nextthink = time + FADEFOG_TIME;
};

//----------------------------------------------------------------------
void(float gf_den, vector gf_col, float gf_time) fade_fog =
{
    // No controller active?
    if (!fog_control) return;

    // Check density and colour values are within range
    gf_den = fog_check_range(gf_den);
    gf_col_x = fog_check_range(gf_col_x);
    gf_col_y = fog_check_range(gf_col_y);
    gf_col_z = fog_check_range(gf_col_z);
        
    // Setup fog controller with new fog values
    fog_control.fog_targden = gf_den;
    fog_control.fog_targcol = gf_col;
    fog_control.fog_difftime = gf_time;

    // Force fog controller to calculate difference
    fog_control.state = STATE_SETUP;
};

//----------------------------------------------------------------------
void() setup_fog_controller =
{
    if (!fog_control) {
        // Start off with the fog controller disabled
        // if the worldspawn setup correctly then enable
        // the controller and allow fog changes
        fog_active = FALSE;
        
        fog_control = spawn();
        fog_control.owner = self;                // Link back to client
        fog_control.classtype = CT_CONTROL;
        fog_control.solid = SOLID_NOT;            // No world interaction
        fog_control.movetype = MOVETYPE_NONE;    // Static item, no movement
        setorigin(fog_control, '0 0 0');
        setsize(fog_control, VEC_ORIGIN, VEC_ORIGIN);
        
        if (world.fog_density != 0) {
            fog_active = TRUE;                    // engine and QC insync
            fog_control.fog_currden = world.fog_density;
        }
        else fog_control.fog_currden = 0.1;        // Bad default
        fog_control.fog_targden = 0;            // Target
        fog_control.fog_diffden = 0;            // Difference

        if (CheckZeroVector(world.fog_colour))
            fog_control.fog_currcol = '0.1 0.1 0.1';    // Bad Default
        else {
            fog_active = TRUE;                    // engine and QC insync
            fog_control.fog_currcol = world.fog_colour;
        }
        fog_control.fog_targcol = '0 0 0';        // Target
        fog_control.fog_diffcol = '0 0 0';        // Difference

        // Bat shit crazy DP extra fog parameters that no other engine uses
        // dpextra = alpha mindist maxdist top fadedepth (def=1 0 8192 1024 32)
        if (world.fog_dpextra != "") fog_control.fog_dpextra = world.fog_dpextra;
        else fog_control.fog_dpextra = DEF_FOGEXT;

        // This is a horrible hack, it should not be here
        // This parameter should be in the map file worldspawn
        //if (world.model == "maps/ad_end.bsp") 
        //    fog_control.fog_dpextra = "1 0 2048 8192 32";
        
        fog_control.state = STATE_OFF;    // Not fading atm
        fog_control.fog_difftime = 0;    // fading fog time (seconds)
        fog_control.fog_steptime = 0;    // How many steps to fade

        // Setup think process to monitor for fog changes
        // If this entity already exists, then its thinking!
        fog_control.think = fog_controller_think;
        fog_control.nextthink = time + FADEFOG_TIME;
        
        if (fog_active) change_fog(self);
    }
};
