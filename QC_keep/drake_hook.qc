//==========================================================================
//
//  GRAPPLE (Based on Rogue's, heavily modified for Drake.)
//
//==========================================================================

// Rogue Grapple Implementation
// Jan'97 by ZOID <zoid@threewave.com>
// Under contract to id software for Rogue Entertainment

// PM:  Added grapple support to Drake just in case others want to use it.
// I am not too fond of the grapple since it alters fundamental gameplay
// too much.  It has its uses though.
//
// Note 9/11/10:  Point is moot now since Tronyn used it in some of his maps.

// Moved custom fields to defs.qc.

// draw a line to the hook
void(entity h, entity player) GrappleTrail =
    {Tent_Beam (13, h, h.origin, player.origin + '0 0 16');};

void(entity h) GrappleSever =
{
    local   entity  src;

    if (!h)
        return;     // The world is not a hook!

    src = h.master;
    if (src)
    if (src.hook == h)
        h.count = TRUE;
};

void() GrappleReset =
{
    local   entity  src;

    src = self.master;
    if (src)
    if (src.hook == self)
    {
        src.hook = world;
        if (src.weapon == IT2_GRAPPLE)
        {
            local   float   reload;

            // Add delay so player has a chance to release button
            // and not shoot another hook immediately.
            reload = time + 0.2;    // Was 0.25.
            if (src.attack_finished < reload)
                src.attack_finished = reload;

            if (src.flags & FL_CLIENT)
                src.weaponframe = 0;
        }
    }
    remove (self);
};

float() GrappleCutCheck =
{
    local   vector  p1, p2;

    p1 = self.origin;
    p2 = self.master.origin + '0 0 16';
    if (vlen (p2 - p1) > self.distance)
        return TRUE;    // Exceeded maximum distance.
    traceline (p1, p2, TRUE, self);
    if (trace_fraction < 1)
        return TRUE;    // Not in line-of-sight.

    return FALSE;
};

void() GrappleTrack =
{
    local   vector  spray;

    if (self != self.master.hook)
        {remove (self);  return;}   // PM:  Not using this hook.

    // drop the hook if owner is dead or has released the button
    // Update 7/11/09:  ...or has left the level.
    if (!self.state || self.master.health <= 0 || !self.modelindex)
        {GrappleReset();  return;}
    // Player always drop the hook during a cutscene.
    if (cutscene)
        if (self.master.flags & FLx_CREATURE == FL_CLIENT)
            {GrappleReset();  return;}

    // PM:  Must have a clear path between hook and source.
    //  Do this here instead of the grapple check because this is
    //  called only 10/sec while the other is called more often than that.
    if (GrappleCutCheck ())
        {GrappleReset();  return;}

    if (self.state != 2)
    {
        local   float   dflags;

        if (self.enemy.solid <= SOLID_TRIGGER)
            {GrappleReset();  return;}      // Release insubstantial targets.
        if (self.enemy.teleport_time > time)
            {GrappleReset();  return;}
        
        // move the hook along with the player.  It's invisible, but
        // we need this to make the sound come from the right spot
        self.velocity = '0 0 0';
//        if (self.enemy.flags & FL_CLIENT)
//            self.origin = self.enemy.origin;
//        else
            self.origin = Midpoint (self.enemy);    //self.enemy.origin + self.enemy.mins + self.enemy.size * 0.5
        setorigin (self, self.origin);

        if (self.master.tome_finished)
            dflags = DF_LEECH;
        else
            dflags = DF_NONE;
        // Use Zerstorer's chainsaw grinding noise.
        sound (self, CHAN_WEAPON, "weapons/sawguts.wav", 1, ATTN_NORM);     //"pendulum/hit.wav"
        T_Damage (self.enemy, self, self.master, self.dmg, DAMARMOR);
        // PM:  Removed 'makevectors (self.v_angle)'.
        spray_x = 100 * crandom();
        spray_y = 100 * crandom();
        spray_z = 100 * crandom() + 50;
        SpawnBlood (self.enemy,self.origin, spray, 20);
    }
    else
        self.velocity = self.enemy.velocity;

    self.nextthink = time + 0.1;
};


// Tries to anchor the grapple to whatever it touches
void() GrappleAnchor =
{
    local   string  sfx;

    if (pointcontents(self.origin) == CONTENT_SKY)
        {GrappleReset();  return;}
    //if (Reflected_Damage (self.dmg))
    //    return;

    if (!self.dmg)
        self.dmg = 10;
    if (!self.cnt)
        self.cnt = 1;

    self.touch = SUB_Null;      // Stack overflow prevention.
    if (other.takedamage || (other.solid != SOLID_BSP))
    {   // PM:  Removed teammate immunity.
        if (other.hittype == HIT_METAL)
            sfx = "weapons/clang.wav";
        else if (other.hittype == HIT_STONE)
            sfx = "weapons/axhitwal.wav";
        else if ((self.master.hook == self) && !self.count)
            sfx = "shambler/smack.wav";
        else    // Not grabbing, so just thump.
            sfx = "zombie/z_hit.wav";
    }
    else
    {
        sfx = "player/axhit2.wav";
        self.velocity = '0 0 0';
        self.avelocity = '0 0 0';
    }
    sound (self, CHAN_AUTO, sfx, 1, ATTN_NORM);
    if (other.takedamage) {
        spawn_touchblood (self,other,self.dmg);
        T_Damage (other, self, self.master, self.dmg, DAMARMOR);
    }

// Remove the hook if one of...
// 1) Button isn't held and grappler is current weapon.
// 2) Hook hits its master, possible with reflection.
// 3) Cable was severed.
// 4) Hook isn't connected with its master.
    if ( (!self.master.button0 && self.weapon == IT2_GRAPPLE)
    || (other == self.master)
    || (self.count)
    || (self.master.hook != self) )
    {
        if (!other.takedamage)
        {
            local   vector  org;

            org = self.origin - 8*normalize(self.velocity);
            Tent_Point (TE_GUNSHOT, org);
        }
        GrappleReset();
        return;
    }

// Anchored.
    if (other.solid == SOLID_BSP)
        self.state = 2;
    else
        self.state = TRUE;
    self.dmg   = self.cnt;
    self.enemy = other;     // remember this guy!
    self.think = GrappleTrack;
    self.nextthink = 0.01;
    self.solid = SOLID_NOT;
    self.touch = SUB_Null;
};

void() Grapple_Think =
{
    if (self.delay < time)
        {GrappleReset ();  return;}

    if (GrappleCutCheck ())
    {
        GrappleSever (self);
        self.think      = GrappleReset;
        self.nextthink  = self.delay;
    }
    else
        self.nextthink  = time + 0.1;
};

void(entity attacker, vector start, vector dir) Grapple_Launch =
{
    newmis = spawn();
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid    = SOLID_BBOX;
    newmis.master   = newmis.owner  = attacker;
    newmis.classname= "hook";

    newmis.count    = FALSE;    // Cable is uncut.
    newmis.state    = FALSE;    // Hook is out flying.
    newmis.movedir  = dir;
    newmis.speed    = 800;
    newmis.dmg      = 10;
    newmis.cnt      = 2;    // Grind damage.  Was 1.

    newmis.velocity = dir * newmis.speed;
    newmis.avelocity= '0 0 -500';       // PM:  Enabled spin.
    newmis.angles   = vectoangles(dir);

    newmis.touch    = GrappleAnchor;
    newmis.think    = Grapple_Think;    //GrappleReset;
    // grapple only lives for X seconds, this gives max range on it
    newmis.delay    = time + 1.5;       // Was T+2.
    newmis.nextthink= 0.01;
    newmis.distance = 1500;     // Maximum cable range.

    newmis.frame    = 1;    // hook spread
    setall (newmis, "progs/drake/hook.mdl", '0 0 0', '0 0 0', start);

    if (attacker)
        attacker.hook     = newmis;
};

void() W_FireGrapple =
{
    if (self.hook)      // reject subsequent calls from player.qc
        return;

    self.punchangle_x = -2; // bump him

    // chain out sound (loops)
    sound (self, CHAN_WEAPON, "weapons/chain1.wav", 1, ATTN_NORM);

    makevectors (self.v_angle);
    Grapple_Launch (self, self.origin + v_forward * 16 + '0 0 16', v_forward);
};


//- - - - - - - - -
entity(entity attacker) Grapple_Pull_Who =
{
    if (attacker.hook)
    {
        local   entity  targ;

        targ = attacker.hook.enemy;
        if (targ.mass == MASS_LIGHT)
            return targ;
        if (targ.mass == MASS_MEDIUM)
            return world;   // No one gets pulled.
    }
    return attacker;
};

float() Grapple_Pull_Me =
{
    if (Grapple_Pull_Who (self) == self)
        return TRUE;
    return FALSE;
};

// Called each frame by CLIENT.QC if client has a hook.
void() GrappleService =
{
    local   vector  /*delta,*/ vel;
    local   float   sped, mx;
    local   entity  targ;       // Who gets pulled.

    if (!self.hook)
        return;
    if (self.hook.count)
        return;     // Already severed.

    if (!self.hook.state)
    {   // Hook is flying and not attached to anything yet.
        // just draw a line to the hook
        if (vlen(self.hook.origin - self.origin) > HOOK_NEAR)
            GrappleTrail (self.hook, self);
        if (!self.button0 && self.weapon == IT2_GRAPPLE)
            GrappleSever (self.hook);
        return;
    }

// From here on, we know the hook is attached to something.
    // PM:  We cannot check cutscene/intermission status here because
    // This function isn't called during such as intermission.
    // drop the hook if player lets go of button
    if ((!self.button0 && self.weapon == IT2_GRAPPLE)
    || (!(InItems2(self, IT2_GRAPPLE)))  // 6/10/11:  Sever if weapon is stolen.
    || self.form_active     // PM:  Can't use grapple while morphed.
    || (self.teleport_time > time)) {
        // release when we get 'ported
        if (self.hook)
            SUB_Think (self.hook, GrappleReset);
        return;
    }
    
    // PM:  Made to changes to pulling...
    // 1) Smaller monsters are pulled toward the player.
    // 2) Pulling speed was greatly reduced from 1000.
    targ = Grapple_Pull_Who (self);
    if (targ)
    {
        makevectors (self.angles);
        // The jump button bumps the player up a little bit.
        vel = self.hook.origin - ( self.origin + (v_up * 16 *
                (!self.button2)) + (v_forward * 16));

        if (targ == self)
            mx = SPEED_HOOK;
        else    // Pulling target toward attacker.
            {mx = 350;  vel = vel * -1;}

        sped = vlen (vel) * 10;
        if (sped > mx)      // Was 1000 -- way too fast!
            sped = mx;

    // DISABLED -- Combining velocity causes more trouble than it's worth.
//        vel = normalize(vel) * 80 + targ.velocity;
//        if (vlen(vel) > sped)
            vel = normalize(vel) * sped;
        targ.velocity = vel;
        targ.flags = targ.flags - (targ.flags & FL_ONGROUND);
    }

    if (vlen(self.hook.origin - self.origin) > HOOK_NEAR)
        GrappleTrail (self.hook, self);
};


//===========================/  END OF FILE  /===========================//
