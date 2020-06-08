//==========================================================================
//
//  BLOOD
//
//==========================================================================

//==========================================================================
//  MEAT SPRAY
//==========================================================================

// Precaches and enables the use of a requested blood model.
// Note:  BLOOD_RED is standard and automatically precached by worldspawn.
void(float btype) precache_blood =
{
    if (btype == BLOOD_GREEN)
    {
        precache_model ("progs/drake/blood_gr.mdl");
        precache_model ("progs/drake/gr_gib.mdl");
        bloodbank = bloodbank | BB_GREEN;
    }
    else if (btype == BLOOD_PURPLE)
    {
//        precache_model ("progs/blood_pu.mdl");
        precache_model ("progs/drake/pur_gib.mdl");
        bloodbank = bloodbank | BB_PURPLE;
    }
    else if (btype == BLOOD_FIRE)
    {
//        precache_model ("progs/smoke.mdl");
        precache_model ("progs/drake/ember.mdl");
        bloodbank = bloodbank | BB_FIRE;
    }
};

// Spawns a particle trail for an invisible blood model.
// Useful for any monster with blood of unusual color (like black or blue).
// It could be motor oil, antifreeze, milk, or fruit juice for all we know.
void() MeatSprayThink =
{
    if (self.delay <= time)
        {remove(self); return;}

    local   vector  dir;

    dir = normalize (self.velocity);
    particle (self.origin, dir, self.bloodtype, 10);

    self.nextthink = time + 0.100;
};

//==========================================================================
//  BLOOD SPRAYS
//==========================================================================
// Creates blood spawning entities.  This is a variation of SpawnMeatSpray.
// Used by decapitation code.
void(vector org, vector vel) SpawnBloodSpray = {
    local   entity  gib;
    local   string  text;

    if (self.bloodtype == BLOOD_RED)
        text = "progs/drake/blood.mdl";
    else if ((self.bloodtype == BLOOD_GREEN) && (bloodbank & BB_GREEN))
        text = "progs/drake/blood_gr.mdl";
    else if (self.bloodtype == BLOOD_PURPLE)
        {if (bloodbank & BB_PURPLE) text = "progs/drake/pur_gib.mdl";}
    else
        return;

    gib = spawn ();
    gib.owner       = self;
    gib.movetype    = MOVETYPE_TOSS;
    gib.solid       = SOLID_NOT;

    gib.velocity    = vel;
    gib.velocity_z  = gib.velocity_z + 250 + 50*random();

// set missile duration
    gib.nextthink   = time + 1;
    gib.think       = SUB_Remove;
    setmodel (gib, text);

    setsize (gib, '0 0 0', '0 0 0');
    setorigin (gib, org);
};

// Sprays blood from a given point.  Used by decapitation code.
void(vector org, float loop) blood_spray = {
    local   vector  vel;

    makevectors (self.angles);
    org = self.origin + (v_forward * org_x) + (v_right * org_y) + (v_up * org_z);
    if (loop == -1)
    {   // Cheesy check to make one final blood spurt ahead.
        vel = (v_forward * (random() * 100 + 100));     //+ (v_up * 50);
        SpawnBloodSpray (org, vel);
        return;
    }
    while (loop > 0)
    {
        vel_x = crandom();  vel_y = crandom();  vel_z = 0;
        vel = normalize(vel) * 200;
        SpawnBloodSpray (org, vel);

        loop = loop - 1;
    }
};
