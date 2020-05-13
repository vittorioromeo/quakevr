/***********************************************
*                                              *
*            FrikBot Movement AI               *
*     "The slightly better movement AI"        *
*                                              *
***********************************************/

/*

This program is in the Public Domain. My crack legal
team would like to add:

RYAN "FRIKAC" SMITH IS PROVIDING THIS SOFTWARE "AS IS"
AND MAKES NO WARRANTY, EXPRESS OR IMPLIED, AS TO THE
ACCURACY, CAPABILITY, EFFICIENCY, MERCHANTABILITY, OR
FUNCTIONING OF THIS SOFTWARE AND/OR DOCUMENTATION. IN
NO EVENT WILL RYAN "FRIKAC" SMITH BE LIABLE FOR ANY
GENERAL, CONSEQUENTIAL, INDIRECT, INCIDENTAL,
EXEMPLARY, OR SPECIAL DAMAGES, EVEN IF RYAN "FRIKAC"
SMITH HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES, IRRESPECTIVE OF THE CAUSE OF SUCH DAMAGES.

You accept this software on the condition that you
indemnify and hold harmless Ryan "FrikaC" Smith from
any and all liability or damages to third parties,
including attorney fees, court costs, and other
related costs and expenses, arising out of your use
of this software irrespective of the cause of said
liability.

The export from the United States or the subsequent
reexport of this software is subject to compliance
with United States export control and munitions
control restrictions. You agree that in the event you
seek to export this software, you assume full
responsibility for obtaining all necessary export
licenses and approvals and for assuring compliance
with applicable reexport restrictions.

Any reproduction of this software must contain
this notice in its entirety.

*/

void() bot_jump =
{
    // TODO check for precision, etc.
    self.button2 = TRUE;
};

float(entity e) bot_can_rj =
{
    // this returns true of the bot can rocket/superjump/hook
    // if your mod doesn't have an RL you can just return FALSE all the time
    // if it has a hook or some other means for the bot to get to high places
    // you can check here for that capability

    // am I dumb?
    if(e.b_skill == 0)
        return FALSE;

    // quad = bad
    if(e.items & 4194304)
        return FALSE;

    // do I have rockets & RL?
    if(!((e.items & 32) && (e.ammo_rockets > 0)))
        return FALSE;

    // do I have pent?
    if(e.items & 1048576)
        return TRUE;

    if(e.health > 50)
        return TRUE;
    else
        return FALSE;
};

float(float flag) frik_recognize_plat =
{
    if((self.classname != "waypoint") && !(self.flags & FL_ONGROUND))
        return FALSE;
    traceline(self.origin, self.origin - '0 0 64', TRUE, self);
    if(trace_ent != world)
    {
        if(flag) // afect bot movement too
        {
            if(self.keys & KEY_MOVEUP)
            {
                if(trace_ent.velocity_z > 0)
                    self.keys = self.keys & 960; // 960 is all view keys
            }
            else if(self.keys & KEY_MOVEDOWN)
            {
                if(trace_ent.velocity_z < 0)
                    self.keys = self.keys & 960;
            }
        }
        return TRUE;
    }
    else
        return FALSE;
};

float(vector sdir) frik_KeysForDir =
{

    local vector keydir;
    local float outkeys, tang;
    outkeys = 0;
    if(sdir_x || sdir_y)
    {
        // Everything is tested against 60 degrees,
        // this allows the bot to overlap the keys
        // 30 degrees on each diagonal  45 degrees
        // might look more realistic

         keydir = vectoangles(sdir);
        tang = angcomp(keydir_y, self.v_angle_y);
        if((tang <= 150) && (tang >= 30))
            outkeys = outkeys + KEY_MOVELEFT;
        else if((tang >= -150) && (tang <= -30))
            outkeys = outkeys + KEY_MOVERIGHT;
        if(fabs(tang) <= 60)
            outkeys = outkeys + KEY_MOVEFORWARD;
        else if(fabs(tang) >= 120)
            outkeys = outkeys + KEY_MOVEBACK;
    }
    if(sdir_z > 0)
        outkeys = outkeys + KEY_MOVEUP;
    else if(sdir_z < 0)
        outkeys = outkeys + KEY_MOVEDOWN;
    return outkeys;

};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

frik_obstructed

Bot has hit a ledge or wall that he should
manuever around.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void(vector whichway, float danger) frik_obstructed =
{
    local float dist;
    local vector disway, org;
// TODO: something
    if(self.b_aiflags & AI_BLIND)
        return;
    org = realorigin(self.target1);

    if(danger)
    {
        self.b_aiflags = self.b_aiflags | AI_DANGER;
        self.keys = frik_KeysForDir('0 0 0' - whichway);
    }
    if(self.b_aiflags & AI_PRECISION)
        return;


    if(self.target1)
    {
        if(self.b_aiflags & AI_OBSTRUCTED)
        {
            if(!(self.b_aiflags & AI_DANGER))
            {
                self.b_aiflags = self.b_aiflags - AI_OBSTRUCTED;
                return;
            }
            else if(!danger)
                return;
        }
        self.obs_dir = whichway;
        disway_x = whichway_y * -1;
        disway_y = whichway_x;
        dist = vlen(org - (self.origin + disway));
        disway_x = whichway_y;
        disway_y = whichway_x * -1;
        self.wallhug = vlen(org - (self.origin + disway)) > dist;
        self.b_aiflags = self.b_aiflags | AI_OBSTRUCTED;

    }
    else
    {
        disway_x = whichway_y * -1;
        disway_y = whichway_x;
        dist = vlen(disway - self.obs_dir);
        disway_x = whichway_y;
        disway_y = whichway_x * -1;
        self.wallhug = vlen(disway - self.obs_dir) < dist;
        self.obs_dir = whichway;

        self.b_aiflags = self.b_aiflags | AI_OBSTRUCTED;
    }
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

frik_obstacles

Detects small bumps the bot needs to jump over
or ledges the bot should avoid falling in.

Also responsible for jumping gaps.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() frik_obstacles =
{
    local vector start, stop, ang;
    local float test, conts, dist, hgt;

    if(!(self.flags & FL_ONGROUND))
        return;
    if(self.b_aiflags & AI_BLIND)
        return;

    ang = normalize(self.velocity);
    ang_z = 0;
    start = self.origin + ang * 32; // ahem
    start_z = self.origin_z + self.maxs_z;
    stop = start;
    stop_z = self.origin_z + self.mins_z;
    traceline(start, stop - '0 0 256', TRUE, self);
    if(trace_allsolid || trace_startsolid)
        return;
    hgt = trace_endpos_z - stop_z;

    if(hgt > 18)
    {
        bot_jump();
        return;
    }
    if(hgt >= 0)
        return;

    conts = pointcontents(trace_endpos + '0 0 4');
    start = stop - '0 0 8';
    stop = start + ang *  256;
    traceline(start, stop, TRUE, self);
    test = vlen(trace_endpos - start);
    if(test <= 20)
        return; // it's a walkable gap, do nothing
    ang_x = self.velocity_y * -1;
    ang_y = self.velocity_x;
    ang = normalize(ang);
    traceline(start - (ang * 10), start + (ang * 10), TRUE, self);
    if((trace_fraction != 1) || trace_startsolid)
        return; // gap is only 20 wide, walkable
    ang = self.velocity;
    ang_z = 0;
    dist = ((540 / sv_gravity) * vlen(ang))/* + 32*/;
    if(test > dist) // I can't make it
    {
        if(conts < -3) // bad stuff down dare
        {
            frik_obstructed(ang, TRUE);
            return;
        }
        else
        {
            if(self.target1)
            {
                stop = realorigin(self.target1);
                if((stop_z - self.origin_z) < -32)
                    return; // safe to fall
            }
            frik_obstructed(ang, FALSE);
            return;
        }
    }
    else
    {
        ang = normalize(ang);
        //look for a ledge
        traceline(self.origin, self.origin + (ang * (test + 20)), TRUE, self);
        if(trace_fraction != 1)
        {
            if(conts < -3) // bad stuff down dare
            {
                frik_obstructed(ang, TRUE);
                return;
            }
            else
            {
                if(self.target1)
                {
                    stop = realorigin(self.target1);
                    if((stop_z - self.origin_z) < -32)
                        return; // safe to fall
                }
                frik_obstructed(ang, FALSE);
                return;
            }
        }

        if(self.target1)
        {
            // getting furter away from my target?
            test = vlen(self.target1.origin - (ang + self.origin));
            if(test > vlen(self.target1.origin - self.origin))
            {
                if(conts < -3) // bad stuff down dare
                {
                    frik_obstructed(ang, TRUE);
                    return;
                }
                else
                {
                    frik_obstructed(ang, FALSE);
                    return;
                }
            }
        }
    }
    if(hgt < -18)
    {
        if(conts < -3) // bad stuff down dare
        {
            frik_obstructed(ang, TRUE);
            return;
        }
        if(self.target1)
        {
            stop = realorigin(self.target1);
            if((stop_z - self.origin_z) < -32)
            {

                return; // safe to fall
            }
        }
        bot_jump();
    }
    // go for it
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

After frik_obstructed, the bot uses the
following funtion to move "around" the obstacle

I have no idea how well it will work

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() frik_dodge_obstruction =
{
    local vector way, org;
    local float oflags, yaw;

    if(!(self.b_aiflags & AI_OBSTRUCTED))
        return;
    if((self.b_aiflags & (AI_BLIND | AI_PRECISION)) || !(self.flags & FL_ONGROUND))
    {
        self.b_aiflags = self.b_aiflags - AI_OBSTRUCTED;
        return;
    }

    // perform a walkmove check to see if the obs_dir is still obstructed
    // walkmove is less forgiving than frik_obstacles, so I dunno
    // how well this will work

    oflags = self.flags;
    org = self.origin;

    yaw = vectoyaw(self.obs_dir);
    if(walkmove(yaw, 32))
        self.b_aiflags = self.b_aiflags - AI_OBSTRUCTED;
    else
    {
        if(self.b_aiflags & AI_DANGER)
        {
            way = '0 0 0' - self.obs_dir;
        }
        else if(self.wallhug)
        {
            way_x = self.obs_dir_y * -1;
            way_y = self.obs_dir_x;
        }
        else
        {
            way_x = self.obs_dir_y;
            way_y = self.obs_dir_x * -1;
        }
        self.keys = self.keys & 960 + frik_KeysForDir(way);
    }

    // fix the bot

    self.origin = org;
    self.flags = oflags;
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

movetogoal and walkmove replacements

blah

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() frik_movetogoal =
{
    local vector way;
    local float g;

    if(self.target1 == world)
    {
        makeforward(self.v_angle);
        frik_walkmove(v_forward);
        return;
    }
    way = realorigin(self.target1) - self.origin;
    if(vlen(way) < 25)
    {
        self.keys = self.keys & 960;
        return;
    }

    way = normalize(way);
    self.keys = self.keys & 960 + frik_KeysForDir(way);

    frik_dodge_obstruction();
    frik_recognize_plat(TRUE);

    if(self.b_aiflags & AI_PRECISION)
    {
        g = angcomp(self.v_angle_x, self.b_angle_x);

        if(fabs(g) > 10)
        {
            self.keys = self.keys & 960;
        }

        g = angcomp(self.v_angle_y, self.b_angle_y);

        if(fabs(g) > 10)
        {
            self.keys = self.keys & 960;
        }
    }
};

float(vector weird) frik_walkmove =
{
    // okay so it's not walkmove
    // sue me
    self.keys = self.keys & 960 + frik_KeysForDir(weird);

    frik_dodge_obstruction();
    frik_recognize_plat(TRUE);
    if(self.b_aiflags & AI_OBSTRUCTED)
        return FALSE;
    else
        return TRUE;
};


/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

The "hook" method of navigation. This nav
system is copyrighted 1999 by Ryan "Frika C"
Smith, keep that in mind when you steal it.

I brought this back because normal roaming
won't work - the bot gets distracted by it's
own waypoints.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() frik_bot_roam =
{
    local vector org, ang, org1;
    local float loopcount, flag, dist;

    loopcount = 26;
    flag = FALSE;
    while((loopcount > 0) && !flag)
    {
        loopcount = loopcount - 1;
        org = self.origin + self.view_ofs;
        ang = self.angles;
        ang_y = frik_anglemod(ang_y - 90 + (random() * 180));
        ang_x = 0; // avoid upward sloping
        makeforward(ang);
        traceline(org, org + v_forward * 2300, TRUE, self);
        if(trace_fraction != 1)
        {
            org1 = trace_endpos;
            ang = normalize(trace_plane_normal);
            ang_z = 0; // avoid upward sloping
            traceline(org1, org1 + (ang * 2300), TRUE, self);
            if((trace_fraction != 1) && (vlen(trace_endpos - org1) >= 64))
            {
                org = trace_endpos;
                traceline(org, self.origin + self.view_ofs, TRUE, self);
                if(trace_fraction != 1)
                {
                    dist = vlen(org1 - org) /2;
                    org = org1 + (ang * dist);
                    traceline(org, org - '0 0 48', TRUE, self);
                    if(trace_fraction != 1)
                    {
                        SpawnTempWaypoint(org);
                        flag = TRUE;
                    }
                }
            }
        }
    }
    self.b_angle_y = self.v_angle_y + 10;
    frik_walkmove(v_forward);
};
