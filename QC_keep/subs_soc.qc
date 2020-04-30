//======================================================================
// Trigger/use all entities that match the supplied targetname
// self is switched around so target use function works correctly
//
//----------------------------------------------------------------------
void(string target_strs, entity activator_ent) trigger_strs =
{
    local entity tself, tother, tactivator, ent_list;

    // Some entities constantly use this function (<0.1s trigger)
    // This is just an entity key to block the debug info
    if (activator_ent.nodebuginfo == FALSE) {
        dprint("\b[STR_TRIG]\b Str ("); dprint(target_strs);
        dprint(") Activator ("); dprint(activator_ent.classname);
        if (target_strs == "") dprint(") - ERROR!!\n");
        else dprint(")\n");
    }

    // Prevent this function from triggering everything by
    // checking if the target_str is empty (empty = world)
    if (target_strs != "") {
        // Store self/other for later
        tself = self;
        tother = other;
        tactivator = activator;
        if (activator_ent.flags & FL_CLIENT) activator = activator_ent;
    
        // Build initial list from world
        ent_list = find(world, targetname, target_strs);
        // Cycle through list
        while(ent_list) {
            // Setup self/other ready for trigger/use function
            self = ent_list;
            other = tself;
            if (self.use) {
                self.activate = activator_ent;        // Link activator
                self.use();                            // Fire use function
            }
            // Find next trigger from intial list (not world)
            ent_list = find(ent_list, targetname, target_strs);
        }
        // Restore self/other from previous temp storage
        self = tself;
        other = tother;
        if (activator_ent.flags & FL_CLIENT) activator = tactivator;
    }
};

//======================================================================
// Trigger/use the entity supplied and activator
// self is switched around so target use function works correctly
//
//----------------------------------------------------------------------
void(entity target_ent, entity activator_ent) trigger_ent =
{
    local entity tself, tother, tactivator;

    dprint("\b[ENT_TRIG]\b Ent ("); dprint(ftos(target_ent.classtype));
    dprint(") Activator ("); dprint(activator_ent.classname);
    if (!target_ent) dprint(") - ERROR!!\n");
    else dprint(")\n");

    if (target_ent && target_ent.use) {
        // Save self/other for later
        tself = self;
        tother = other;
        tactivator = activator;
        if (activator_ent.flags & FL_CLIENT) activator = activator_ent;
        
        // Switch around self/other for USE function
        self = target_ent;
        other = tself;
        
        // Use trigger
        if (self.use) {
            self.activate = activator_ent;        // Link activator
            self.use();                            // Fire use function
        }
            
        // Restore self/other from previous temp storage
        self = tself;
        other = tother;
        if (activator_ent.flags & FL_CLIENT) activator = tactivator;
    }
};

//======================================================================
// Check if the entity has solid surface below 
// There is no entity type checks, can be any entity in theory!
// First check is checkbottom for completel bounding box coverage
// Second check is trace down to find floor surface below
// Typical trace down distance is 128 units with 24 unit difference
//
// Uses for this function:-
// ai_states.qc - ai_stand
// monsters.qc  - monster_deadbody_check
// ai_gibs.qc   - CheckFloorGib
// items.qc     - item_thinkloop
//
//----------------------------------------------------------------------
void(entity source, float floor_dist) ent_floorcheck =
{
    local vector trace_pos1;
    local float trace_dist;
    
    // if (!checkbottom(source)) = disabled
    //  too expensive, 5 traces (4 corners + center) and liquid check

    // Tracedown directly from origin for floor check
    trace_pos1 = source.origin;
    trace_pos1_z = trace_pos1_z - FLOOR_TRACE_DIST;
    traceline (source.origin, trace_pos1, TRUE, source);
    
    // Work out distance from origin to floor
    trace_dist = source.origin_z - trace_endpos_z;
    
    // Check if the floor is too far away? (usually 24 units)
    if (trace_dist > floor_dist && source.flags & FL_ONGROUND) {
        // Move entity up and do a trace/fall down (256 units max)
        source.oldorigin = source.origin;
        source.origin_z = source.origin_z + 1;
        droptofloor();
        if (source.oldorigin != source.origin) {
            // lift entity upward and remove onground flag
            source.flags = source.flags - (source.flags & FL_ONGROUND);
            source.velocity_z = 10 + 10*random();
            // Check for any attachment (lids) for ammo
            if (source.classgroup == CG_AMMOITEM && source.attachment) {
                // sync flag/velocity so lid falls correctly
                source.attachment.flags = source.attachment.flags - (source.attachment.flags & FL_ONGROUND);
                source.attachment.velocity = source.velocity;
            }
        }
        // Move item back to original pos before droptofloor
        setorigin(source, source.oldorigin);
    }
};

//======================================================================
// Check for contact with sky brushes
// - Use pointcontent() for Typical/Enhanced Quake engines
// - Use new FTE string functions and surface check for Darkplaces engine
//
//----------------------------------------------------------------------
float(vector targ_origin) check_skycontent =
{
    local float surfnum;
    local string texturename;

    // Does the engine supports extended surface checks?
    if (ext_dpsurf) {
        surfnum = getsurfacenearpoint(world, targ_origin);
        if (surfnum >= 0) {
            texturename = getsurfacetexture(world, surfnum);
            if (strncasecmp(texturename, "SKY", 3) == 0) return TRUE;
        }
    }
    else {
        // Original method for checking sky content (Fitz engines)
        if (pointcontents(targ_origin) == CONTENT_SKY) return TRUE;
    }
    return FALSE;
};

//----------------------------------------------------------------------
// Check point content for SOLID or SKY problems
//----------------------------------------------------------------------
float(vector pc_org) entity_pcontent =
{
    local float pcontent;
    pcontent = pointcontents(pc_org);
    if (pcontent == CONTENT_SOLID) return TRUE;
    if (check_skycontent(pc_org)) return TRUE;
    return FALSE;
}

//======================================================================
// Updates totals on the HUD (checks for client to exist)
// - Specify which total to update using constant
// - Uses MSG_ONE because it is more reliable across many clients
// - Other client updates (SVC_KILLEDMONSTER, SVC_FOUNDSECRET)
// 
//----------------------------------------------------------------------
void(float hud_item) update_hud_totals =
{
    // Has the client player been setup yet?
    if (client_ent && client_ent.flags & FL_CLIENT) {

        // Update total secrets
        if (hud_item & HUD_SECRETS) {
            msg_entity = client_ent;
            WriteByte (MSG_ONE, SVC_UPDATESTAT);
            WriteByte (MSG_ONE, STAT_TOTALSECRETS);
            WriteLong (MSG_ONE, total_secrets); 
        }
        // Update total monsters
        if (hud_item & HUD_MONSTERS) {
            msg_entity = client_ent;
            WriteByte (MSG_ONE, SVC_UPDATESTAT);
            WriteByte (MSG_ONE, STAT_TOTALMONSTERS);
            WriteLong (MSG_ONE, total_monsters); 
        }
    }
};

//======================================================================
// Reduce any entity down to a non-interactive empty marker
//
//----------------------------------------------------------------------
void(entity targ) entity_hide =
{
    targ.use = targ.touch = SUB_Null;    // No more touchy
    targ.think = SUB_Null;                // No more thinking/animation
    targ.nextthink = -1;                // Never fire think
    targ.estate = ESTATE_OFF;            // Entity state off
    setmodel (targ, "");                // invisible
    targ.takedamage = DAMAGE_NO;        // No pain/death triggers
    targ.movetype = MOVETYPE_NONE;        // Stationary
    targ.solid = SOLID_NOT;                // no world interaction
    setsize (targ, VEC_ORIGIN, VEC_ORIGIN);        // No size, no impact
    targ.velocity = targ.avelocity = '0 0 0';    // Frozen velocity
    targ.spawnflags = targ.effects = 0;            // Leave flags alone
    targ.waitmin = time + LARGE_TIMER;    // Block any touch function
};

//----------------------------------------------------------------------
void(entity targ, float removetime) entity_remove =
{
    if (targ == world) return;
    entity_hide(targ);
    targ.think = SUB_Remove;
    targ.nextthink = time + removetime;
};

//======================================================================
// Show where broken monster/items are located (developer only)
// SPNMARK_YELLOW, SPNMARK_BLUE, SPNMARK_GREEN, SPNMAKR_RED
//----------------------------------------------------------------------
void(vector marker_org, float marker_type) spawn_marker =
{
    local entity brokend;
    
    if (developer) {
        brokend = spawn();
        brokend.classtype = CT_DEVMARKER;
        brokend.movetype = MOVETYPE_NONE;
        brokend.solid = SOLID_NOT;
        setmodel(brokend, MODEL_BROKEN);
        setorigin(brokend, marker_org);
        setsize (brokend, VEC_ORIGIN, VEC_ORIGIN);
        // Setup marker type (skin colour)
        // YELLOW = error, BLUE = delay, GREEN = nocount, RED = nightmare
        brokend.skin = marker_type;
        brokend.frame = 0;
        // If DP engine active remove particle shadow
        if (engine == ENG_DPEXT) brokend.effects = brokend.effects + EF_NOSHADOW;
    }
};

//----------------------------------------------------------------------
entity(vector spawn_org) spawn_devmarker =
{
    local entity spawn_ent;
    
    spawn_ent = spawn();
    spawn_ent.classtype = CT_DEVMARKER;
    spawn_ent.movetype = MOVETYPE_NONE;
    spawn_ent.solid = SOLID_NOT;
    setmodel(spawn_ent, MODEL_BROKEN);
    setorigin(spawn_ent, spawn_org);
    setsize (spawn_ent, VEC_ORIGIN, VEC_ORIGIN);
    return spawn_ent;
};

//----------------------------------------------------------------------
// Check for SKILL spawning conditions (frame 0 check ONLY)
// Returns FALSE = do nothing  TRUE = inhibit spawning
//----------------------------------------------------------------------
float() check_nightmare =
{
    // Check for nightmare skill only spawning
    if (self.nightmare == TRUE) {
        // Double check the coop console variable, not setup till frame 1+
        // Use a nosave variable to prevent constant cvar command checking
        if (skill_cvar != TRUE) { skill = cvar("skill"); skill_cvar = TRUE; }
        if (skill == SKILL_NIGHTMARE) return FALSE;
        else {
            // Check for BSP entities has origin is different
            if (self.bsporigin) spawn_marker(bmodel_origin(self), SPNMARK_RED);
            else spawn_marker(self.origin, SPNMARK_RED);
            // Hide entity instead of removing it
            // Because of possible active touch functions
            entity_hide(self);
            return TRUE;
        }
    }
    return FALSE;
};

//----------------------------------------------------------------------
// Check for COOP spawning conditions (frame 0 check ONLY)
// Returns FALSE = do nothing  TRUE = inhibit spawning
//----------------------------------------------------------------------
float() check_coop =
{
    // Check for coop only spawning conditions
    if (self.cooponly == TRUE || self.cooponly == NEGATIVE) {
        // Double check the coop console variable, not setup till frame 1+
        // Use a nosave variable to prevent constant cvar command checking
        if (coop_cvar != TRUE) { coop = cvar("coop"); coop_cvar = TRUE; }
        
        // Only one condition will let a cooponly item spawn
        // cooponly =  1  coop = 0 inhibit = TRUE
        // cooponly =  1  coop = 1 inhibit = FALSE
        // cooponly = -1  coop = 0 inhibit = TRUE
        // cooponly = -1  coop = 1 inhibit = TRUE
        if (self.cooponly == TRUE && coop == TRUE) return FALSE;
        else {
            // Check for BSP entities has origin is different
            if (self.bsporigin) spawn_marker(bmodel_origin(self), SPNMARK_PURPLE);
            else spawn_marker(self.origin, SPNMARK_PURPLE);
            // Hide entity instead of removing it
            // Because of possible active touch functions
            entity_hide(self);
            return TRUE;
        }
    }
    return FALSE;
};

//======================================================================
// Clear all trigger strings
//
//----------------------------------------------------------------------
void (entity targ) clear_trigstrs =
{
    if (targ.target != "") targ.target = string_null;
    if (targ.target2 != "") targ.target2 = string_null;
    if (targ.killtarget != "") targ.killtarget = string_null;
};

//======================================================================
// Vector randomizer, used mostly for avelocity setups
// Used all over the place; monsters, weapons and gibs!
// 
//----------------------------------------------------------------------
vector(float base, float rndmix, float plusminus) vecrand =
{
    local vector vecmix;
    if (plusminus) {
        vecmix_x = base + crandom() * rndmix; 
        vecmix_y = base + crandom() * rndmix; 
        vecmix_z = base + crandom() * rndmix; 
    }
    else {
        vecmix_x = base + random() * rndmix; 
        vecmix_y = base + random() * rndmix; 
        vecmix_z = base + random() * rndmix; 
    }
    return vecmix;
};

//======================================================================
// model_fade : Gradually fade out a model over time 
// * Optionally entity removal if height = 0
//
//----------------------------------------------------------------------
void() model_fade =
{
    self.alpha = 1 - ((time - self.ltime) / 1);

    if (self.alpha > 1) self.alpha = 1;
    else if (self.alpha <= 0) {
        self.alpha = 0;
        self.model = "";
        // Remove model by default
        if (!self.height) {
            self.think = SUB_Remove;
            self.nextthink = time + 1;
         }
        return;
    }

    self.nextthink = time + FADEMODEL_TIME;
};

