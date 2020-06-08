/*======================================================================
Intermission Camera System

 SetupIntermissionCamera - Find/setup all camera(s)
 StartIntermissionCamera - Setup player ready for viewing
 NextIntermissionCamera - Move player to first/next camera
 CycleIntermissionCamera - wait for player input
 FinishIntermissionCamera - No more viewing
======================================================================*/
float CAMERA_NOINTERMIS = 1;    // Will not work if intermission active
float CAMERA_SOLID = 2;            // Intermission camera interacts with world
float INTERMISSION_ACTIVE = 2;    // Regular Intermission active
float INTERMISSION_RUNE = 3;    // Check for end of episode
float INTERMISSION_ENDGAME = 4;    // End of game!?! (set in oldone.qc)
float INTERMISSION_CAMERA = 10;    // Special Camera only mode

//----------------------------------------------------------------------
void() SetupIntermissionCamera = 
{
    local entity intercam, prevcam;

    // Is the infomission camera chain been setup already?
    if (camera_ent) return;    

    // Try to find the first intermission camera
    intercam = find (world, classname, "info_intermission");
    prevcam = world;
    
    if (intercam) {
        // Cycle through all intermission cameras and link them together
        while(intercam) {
            // first camera in chain? setup first and previous camera
            if (!prevcam) { camera_ent = intercam; camera_ent.count = 0;}
            else prevcam.owner = intercam;

            // Save current camera to previous ready for loop
            prevcam = intercam;
            camera_ent.count = camera_ent.count + 1;
            //dprint("[CAM] ("); dprint(ftos(camera_ent.count));
            //dprint(") Org ("); dprint(vtos(intercam.origin)); dprint(")\n");
                
            intercam = find(intercam, classname, "info_intermission");
        }
        // Close the camera chain
        prevcam.owner = camera_ent;
        dprint("\b[CAMERA]\b Intermissions found (");
        dprint(ftos(camera_ent.count)); dprint(")\n");
    }
    else {
        dprint("\b[CAMERA]\b Intermissions missing!\n");
        // Cannot find any info_intermission cameras, so use player start
        // If there is no player start then use player's last location!
        if (deathmatch) intercam = find (world, classname, "info_player_deathmatch");
        else intercam = find (world, classname, "info_player_start");
        if (!intercam) intercam = client_ent;
            
        // Spawn a new camera entity
        camera_ent = spawn();
        camera_ent.classtype = CT_CAMERA;
        setsize (camera_ent, VEC_ORIGIN, VEC_ORIGIN);
        camera_ent.movetype = MOVETYPE_NONE;
        camera_ent.solid = SOLID_NOT;
        setorigin(camera_ent, intercam.origin);
        setmodel(camera_ent, MODEL_EMPTY);
        camera_ent.mangle = intercam.angles;
        camera_ent.count = 1;
        camera_ent.owner = camera_ent;
    }
};

//----------------------------------------------------------------------
void() FinishIntermissionCamera =
{
    // skip any text in deathmatch
    if (deathmatch) { GotoNextMap (); return; }
    
    intermission_exittime = time + 1;
    intermission_running = intermission_running + 1;

    //----------------------------------------------------------------------
    // run some text if at the end of an each episode
    //----------------------------------------------------------------------
    if (intermission_running == INTERMISSION_RUNE) {
        if (world.model == "maps/e1m7.bsp") {
            WriteByte (MSG_ALL, SVC_CDTRACK);
            WriteByte (MSG_ALL, 2);
            WriteByte (MSG_ALL, 3);
            if (!cvar("registered")) {
                WriteByte (MSG_ALL, SVC_FINALE);
                WriteString (MSG_ALL, "As the corpse of the monstrous entity\nChthon sinks back into the lava whence\nit rose, you grip the Rune of Earth\nMagic tightly. Now that you have\nconquered the Dimension of the Doomed,\nrealm of Earth Magic, you are ready to\ncomplete your task in the other three\nhaunted lands of Quake. Or are you? If\nyou don't register Quake, you'll never\nknow what awaits you in the Realm of\nBlack Magic, the Netherworld, and the\nElder World!");
            }
            else {
                WriteByte (MSG_ALL, SVC_FINALE);
                WriteString (MSG_ALL, "As the corpse of the monstrous entity\nChthon sinks back into the lava whence\nit rose, you grip the Rune of Earth\nMagic tightly. Now that you have\nconquered the Dimension of the Doomed,\nrealm of Earth Magic, you are ready to\ncomplete your task. A Rune of magic\npower lies at the end of each haunted\nland of Quake. Go forth, seek the\ntotality of the four Runes!");
            }
            return;
        }
        else if (world.model == "maps/e2m6.bsp") {
            WriteByte (MSG_ALL, SVC_CDTRACK);
            WriteByte (MSG_ALL, 2);
            WriteByte (MSG_ALL, 3);

            WriteByte (MSG_ALL, SVC_FINALE);
            WriteString (MSG_ALL, "The Rune of Black Magic throbs evilly in\nyour hand and whispers dark thoughts\ninto your brain. You learn the inmost\nlore of the Hell-Mother; Shub-Niggurath!\nYou now know that she is behind all the\nterrible plotting which has led to so\nmuch death and horror. But she is not\ninviolate! Armed with this Rune, you\nrealize that once all four Runes are\ncombined, the gate to Shub-Niggurath's\nPit will open, and you can face the\nWitch-Goddess herself in her frightful\notherworld cathedral.");
            return;
        }
        else if (world.model == "maps/e3m6.bsp") {
            WriteByte (MSG_ALL, SVC_CDTRACK);
            WriteByte (MSG_ALL, 2);
            WriteByte (MSG_ALL, 3);

            WriteByte (MSG_ALL, SVC_FINALE);
            WriteString (MSG_ALL, "The charred viscera of diabolic horrors\nbubble viscously as you seize the Rune\nof Hell Magic. Its heat scorches your\nhand, and its terrible secrets blight\nyour mind. Gathering the shreds of your\ncourage, you shake the devil's shackles\nfrom your soul, and become ever more\nhard and determined to destroy the\nhideous creatures whose mere existence\nthreatens the souls and psyches of all\nthe population of Earth.");
            return;
        }
        else if (world.model == "maps/e4m7.bsp") {
            WriteByte (MSG_ALL, SVC_CDTRACK);
            WriteByte (MSG_ALL, 2);
            WriteByte (MSG_ALL, 3);

            WriteByte (MSG_ALL, SVC_FINALE);
            WriteString (MSG_ALL, "Despite the awful might of the Elder\nWorld, you have achieved the Rune of\nElder Magic, capstone of all types of\narcane wisdom. Beyond good and evil,\nbeyond life and death, the Rune\npulsates, heavy with import. Patient and\npotent, the Elder Being Shub-Niggurath\nweaves her dire plans to clear off all\nlife from the Earth, and bring her own\nfoul offspring to our world! For all the\ndwellers in these nightmare dimensions\nare her descendants! Once all Runes of\nmagic power are united, the energy\nbehind them will blast open the Gateway\nto Shub-Niggurath, and you can travel\nthere to foil the Hell-Mother's plots\nin person.");
            return;
        }
        // No special end text? go straight to next map
        GotoNextMap();
    }
    //----------------------------------------------------------------------
    // If the player has all four runes, print more text!?!
    //----------------------------------------------------------------------
    else if (intermission_running == INTERMISSION_ENDGAME) {
        if (!cvar("registered")) {    
            // shareware episode has been completed, go to sell screen
            WriteByte (MSG_ALL, SVC_SELLSCREEN);
            return;
        }
        
        if ( (serverflags & SVR_RUNE_ALL) == SVR_RUNE_ALL) {
            WriteByte (MSG_ALL, SVC_FINALE);
            WriteString (MSG_ALL, "Now, you have all four Runes. You sense\ntremendous invisible forces moving to\nunseal ancient barriers. Shub-Niggurath\nhad hoped to use the Runes Herself to\nclear off the Earth, but now instead,\nyou will use them to enter her home and\nconfront her as an avatar of avenging\nEarth-life. If you defeat her, you will\nbe remembered forever as the savior of\nthe planet. If she conquers, it will be\nas if you had never been born.");
            return;
        }
    }
    // Finished episode text and shub waking up text, then finally next map!
    else GotoNextMap();
};

//----------------------------------------------------------------------
void() NextIntermissionCamera = 
{
    // This method is unreliable, it is better to move the players
    // origin like the orginal method used by the id QC
    //msg_entity = self;
    //WriteByte(MSG_ALL, SVC_SETVIEW);
    //WriteEntity(MSG_ALL, self.goalentity);
    
    // Exclude the special intermission cameras (special impulse)
    if (camera_ent.count > 1 && intermission_running > 0
        && intermission_running < INTERMISSION_CAMERA) {
        // Cycle through list trying to find a camera
        // designed for the intermission at the end of a map
        // Can have special cameras designed for impulse 145!
        while(self.goalentity.spawnflags & CAMERA_NOINTERMIS) {
            self.goalentity = self.goalentity.owner;
        }
    }
    
    // Switch player view to intermission camera
    self.view_ofs = '0 0 0';
    setorigin(self, self.goalentity.origin);
    self.angles = self.v_angle = self.goalentity.mangle;
    self.fixangle = TRUE; 
    
    // Change the camera to allow for environment interaction
    if (self.goalentity.spawnflags & CAMERA_SOLID) {
        self.solid = SOLID_TRIGGER;
        setsize(self, VEC_HULLE_MIN, VEC_HULLE_MAX);
    }
    // no interaction, static
    else {
        self.solid = SOLID_NOT;
        setsize(self, VEC_ORIGIN, VEC_ORIGIN);
    }
    
    // If target exists on camera fire triggers (once)
    if (self.goalentity.target) {
        trigger_strs(self.goalentity.target, self); 
        self.goalentity.target = "";
    }

    // Test for any fog changes on intermission entity
    if (fog_active && self.goalentity.fog_density > 0) {
        // Double check for quake.rc parameter
        if (query_configflag(SVR_NOFOGCMDS)) self.goalentity.fog_density = 0;
        else fade_fog(self.goalentity.fog_density, self.goalentity.fog_colour, self.goalentity.speed);
    }
};

//----------------------------------------------------------------------
void() CycleIntermissionCamera =
{
    // Is the intermission camera still being setup?
    if (intermission_running < INTERMISSION_ACTIVE) return;
    if (self.waitmin > time) return;        // delay input from keyboard
    
    // check for jump button to exit
    if (self.button1 || self.button2 || (self.button0 && camera_ent.count == 1) ) {
        self.button0 = self.button1 = self.button2 = 0;
        if (intermission_running != INTERMISSION_CAMERA) {
            self.waitmin = time + 1;
            if (time > intermission_exittime) FinishIntermissionCamera ();
        }
    }
    // check for attack button to switch cameras
    else if (self.button0) {
        self.button0 = self.button1 = self.button2 = 0;
        self.waitmin = time + 0.5;

        // Shift forward to next camera
        self.goalentity = self.goalentity.owner;
        NextIntermissionCamera();
    }
};

//----------------------------------------------------------------------
void() StartIntermissionCamera = 
{
    if (camera_ent) {
        // Switch self to client (stored when client is setup)
        self = client_ent;
        
        // Switch on notarget so monsters don't attack player
        // Add god mode to prevent any damage, is reset on map change
        self.flags = self.flags + (FL_NOTARGET || FL_GODMODE);
        self.flags = self.flags - (self.flags & FL_ONGROUND);
    
        // reset player model to nothing and no world interaction
        self.view_ofs = self.velocity = self.avelocity = '0 0 0';
        self.solid = SOLID_NOT;
        self.movetype = MOVETYPE_NONE;        // no more movement
        self.takedamage = DAMAGE_NO;        // no more damage
        setmodel(self, MODEL_EMPTY);        // Hide player model
        setsize(self, VEC_ORIGIN, VEC_ORIGIN);
        self.weaponmodel = string_null;        // No vmodel updates
        self.frame = self.weaponframe = 0;    // reset all frames
        self.waitmin = time + 0.5;            // Delay before start
        self.think = SUB_Null;                // Stop player.qc

        // Finish all other artifacts - stop all the timers
        self.radsuit_finished = self.invisible_finished = 0;
        self.invincible_finished = self.super_damage_finished = 0;
        self.sharpshoot_finished = self.nailpiercer_finished = 0;
        self.wetsuit_finished = 0;

        // Setup default - first camera to look through
        self.goalentity = camera_ent;
        // Is there more than 1 camera and intermission time?
        // Always start at the first camera if using camera test mode
        if (camera_ent.count > 1 && intermission_running > 0) {
            // Randomly pick a camera to cycle forward to
            self.lip = rint(random() * camera_ent.count);
            while (self.lip > 0) {
                self.goalentity = self.goalentity.owner;// Cycle through list
                self.lip = self.lip - 1;                // Keep looping
            }
        }

        // Switch player to the currently selected camera
        NextIntermissionCamera();
        
        // Check if end of map intermission is really running? 
        // Can enter this function via camera test mode (impulse xxx)
        if (intermission_running == 0) intermission_running = INTERMISSION_CAMERA;
        else intermission_running = INTERMISSION_ACTIVE;
    }
    // If camera entity not setup, go straight to next map
    else FinishIntermissionCamera ();
};
