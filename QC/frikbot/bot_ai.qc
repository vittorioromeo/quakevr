/***********************************************
*                                              *
*            FrikBot General AI                *
*     "The I'd rather be playing Quake AI"     *
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

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

target_onstack

checks to see if an entity is on the bot's stack

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

float(entity scot) target_onstack =
{
    if(scot == world)
    {
        return FALSE;
    }

    if(self.target1 == scot)
    {
        return 1;
    }

    if(self.target2 == scot)
    {
        return 2;
    }

    if(self.target3 == scot)
    {
        return 3;
    }

    if(self.target4 == scot)
    {
        return 4;
    }

    return FALSE;
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

target_add

adds a new entity to the stack, since it's a
LIFO stack, this will be the bot's new target1

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void(entity ent) target_add =
{
    if(ent == world)
    {
        return;
    }

    if(target_onstack(ent))
    {
        return;
    }

    self.target4 = self.target3;
    self.target3 = self.target2;
    self.target2 = self.target1;
    self.target1 = ent;
    self.search_time = time + 5;
};


/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

target_drop

Removes an entity from the bot's target stack.
The stack will empty everything up to the object
So if you have target2 item_health, target1
waypoint, and you drop the health, the waypoint
is gone too.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void(entity ent) target_drop =
{
    local float tg;

    tg = target_onstack(ent);

    if(tg == 1)
    {
        self.target1 = self.target2;
        self.target2 = self.target3;
        self.target3 = self.target4;
        self.target4 = world;
    }
    else if(tg == 2)
    {
        self.target1 = self.target3;
        self.target2 = self.target4;
        self.target3 = self.target4 = world;
    }
    else if(tg == 3)
    {
        self.target1 = self.target4;
        self.target2 = self.target3 = self.target4 = world;
    }
    else if(tg == 4)
    {
        self.target1 = self.target2 = self.target3 = self.target4 = world;
    }
    else
    {
        bprint("Invalid target tg '");
        bprint(ftos(tg));
        bprint("'\n");
    }

    self.search_time = time + 5;
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bot_lost

Bot has lost its target.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void(entity targ, float success) bot_lost =
{
    if(!targ)
        return;

    target_drop(targ);
    if(targ.classname == "waypoint")
        targ.b_sound = targ.b_sound - (targ.b_sound & ClientBitFlag(self.b_clientno));

    // find a new route
    if(!success)
    {
        self.target1 = self.target2 = self.target3 = self.target4 = world;
        self.last_way = FindWayPoint(self.current_way);
        ClearMyRoute();
        self.b_aiflags = 0;
    }
    else
    {
        if(targ.classname == "item_artifact_invisibility")
        {
            if(self.items & 524288)
            {
                bot_start_topic(3);
            }
        }

        if(targ.flags & FL_ITEM)
        {
            if(targ.model == string_null)
                targ._last = world;
            else
                targ._last = self;
        }
    }


    if(targ.classname != "player")
        targ.search_time = time + 5;
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bot_check_lost

decide if my most immediate target should be
removed.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/
void(entity targ) bot_check_lost =
{
    if(targ == world)
        return;

    vector dist = realorigin(targ) - self.origin;
    dist_z = 0;

    // waypoints and items are lost if you get close enough to them

    if(targ.flags & FL_ITEM)
    {
        if(vlen(targ.origin - self.origin) < 32)
            bot_lost(targ, TRUE);
        else if(targ.model == string_null)
            bot_lost(targ, TRUE);
    }
    else if(targ.classname == "waypoint")
    {
        if(!(self.b_aiflags & (AI_SNIPER | AI_AMBUSH)))
        {
            if(self.b_aiflags & AI_RIDE_TRAIN)
            {
                if(vlen(targ.origin - self.origin) < 48)
                    bot_lost(targ, TRUE);
            }
            else if(self.b_aiflags & AI_PRECISION)
            {
                if(vlen(targ.origin - self.origin) < 24)
                    bot_lost(targ, TRUE);
            }
            else if(vlen(targ.origin - self.origin) < 32)
                bot_lost(targ, TRUE);
        }
    }
    else if(targ.classname == "temp_waypoint")
    {
        if(vlen(targ.origin - self.origin) < 32)
            bot_lost(targ, TRUE);
    }
    else if(targ.classname == "player")
    {
        if(targ.health <= 0)
            bot_lost(targ, TRUE);
        else if((coop) || (teamplay && targ.team == self.team))
        {
            if(targ.target1.classname == "player")
            {
                if(!targ.target1.ishuman)
                    bot_lost(targ, TRUE);
            }
            else if(targ.teleport_time > time)
            {
                // try not to telefrag teammates
                self.keys = self.keys & 960;
            }
            else if(vlen(targ.origin - self.origin) < 128)
            {
                if(vlen(targ.origin - self.origin) < 48)
                    frik_walkmove(self.origin - targ.origin);
                else
                {
                    self.keys = self.keys & 960;
                    bot_start_topic(4);
                }
                self.search_time = time + 5; // never time out
            }
            else if(!fisible(targ))
                bot_lost(targ, FALSE);
        }
        else if(waypoint_mode > WM_LOADED)
        {
            if(vlen(targ.origin - self.origin) < 128)
            {
                bot_lost(targ, TRUE);
            }
        }
    }

    // buttons are lost of their frame changes
    else if(targ.classname == "func_button")
    {
        if(targ.frame)
        {
            bot_lost(targ, TRUE);
            if(self.enemy == targ)
                self.enemy = world;
            //if(self.target1)
            //    bot_get_path(self.target1, TRUE);

        }
    }
    // trigger_multiple style triggers are lost if their thinktime changes
    else if((targ.movetype == MOVETYPE_NONE) && (targ.solid == SOLID_TRIGGER))
    {
        if(targ.nextthink >= time)
        {
            bot_lost(targ, TRUE);
            //if(self.target1)
            //    bot_get_path(self.target1, TRUE);
        }
    }
    // lose any target way above the bot's head
    // FIXME: if the bot can fly in your mod..
    if((targ.origin_z - self.origin_z) > 64)
    {
        dist = targ.origin - self.origin;
        dist_z = 0;
        if(vlen(dist) < 32)
            if(self.flags & FL_ONGROUND)
                if(!frik_recognize_plat(FALSE))
                    bot_lost(targ, FALSE);
    }
    else if(targ.classname == "train")
    {
        if(frik_recognize_plat(FALSE))
            bot_lost(targ, TRUE);
    }
    // targets are lost if the bot's search time has expired
    if(time > self.search_time)
        bot_lost(targ, FALSE);
};


/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bot_handle_ai

This is a 0.10 addition. Handles any action
based b_aiflags.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() bot_handle_ai =
{
    local entity newt;
    local vector v;

    // handle ai flags -- note, not all aiflags are handled
    // here, just those that perform some sort of action

    // wait is used by the ai to stop the bot until his search time expires / or route changes

    if(self.b_aiflags & AI_WAIT)
        self.keys = self.keys & 960;

    if(self.b_aiflags & AI_DOORFLAG) // was on a door when spawned
    {
        b_temp3 = self;
        self = self.last_way;
        if(!frik_recognize_plat(FALSE)) // if there is nothing there now
        {
            newt = FindThing("door"); // this is likely the door responsible (crossfingers)
            self = b_temp3;

            if(self.b_aiflags & AI_DOOR_NO_OPEN)
            {
                if(newt.nextthink)
                    self.keys = self.keys & 960; // wait until it closes
                else
                {
                    bot_lost(self.last_way, FALSE);
                }
            }
            else
            {
                if(newt.targetname != NULLSTR)
                {
                    newt = find(world, target, newt.targetname);
                    if(newt.health > 0)
                    {
                        self.enemy = newt;
                        bot_weapon_switch(1);
                    }
                    else
                    {
                    //    target_drop(self.last_way);
                        target_add(newt);
                    //    bot_get_path(newt, TRUE);
                    }
                }
                self.b_aiflags = self.b_aiflags - AI_DOORFLAG;
            }
        }
        else
            self = b_temp3;
    }

    if(self.b_aiflags & AI_JUMP)
    {
        if(self.flags & FL_ONGROUND)
        {
            bot_jump();
            self.b_aiflags = self.b_aiflags - AI_JUMP;
        }
    }
    else if(self.b_aiflags & AI_SUPER_JUMP)
    {
        if(self.weapon != WID_ROCKET_LAUNCHER)
            self.impulse = 7;

        else if(self.flags & FL_ONGROUND)
        {
            self.b_aiflags = self.b_aiflags - AI_SUPER_JUMP;
            if(bot_can_rj(self))
            {
                bot_jump();
                self.v_angle_x = self.b_angle_x = 80;
                self.button0 = TRUE;
            }
            else
                bot_lost(self.target1, FALSE);

        }
    }
    if(self.b_aiflags & AI_SURFACE)
    {
        if(self.waterlevel > 2)
          {
            self.keys = KEY_MOVEUP;
            self.button2 = TRUE; // swim!
            }
        else
            self.b_aiflags = self.b_aiflags - AI_SURFACE;
    }
    if(self.b_aiflags & AI_RIDE_TRAIN)
    {
        // simple, but effective
        // this can probably be used for a lot of different
        // things, not just trains (door elevators come to mind)
        b_temp3 = self;
        self = self.last_way;

        if(!frik_recognize_plat(FALSE)) // if there is nothing there now
        {
            self = b_temp3;
            self.keys = self.keys & 960;
        }
        else
        {
            self = b_temp3;
            if(frik_recognize_plat(FALSE))
            {
                v = realorigin(trace_ent) + trace_ent.origin - self.origin;
                v_z = 0;
                if(vlen(v) < 24)
                    self.keys = self.keys & 960;
                else
                {
                    self.b_aiflags = self.b_aiflags | AI_PRECISION;
                    self.keys = frik_KeysForDir(v);
                }
            }
        }
    }
    if(self.b_aiflags & AI_PLAT_BOTTOM)
    {
        newt = FindThing("plat");
        if(newt.state != 1)
        {
            v =  self.origin - realorigin(newt);
            v_z = 0;
            if(vlen(v) > 96)
                self.keys = self.keys & 960;
            else
                frik_walkmove(v);
        }
        else
            self.b_aiflags = self.b_aiflags - AI_PLAT_BOTTOM;
    }
    if(self.b_aiflags & AI_DIRECTIONAL)
    {
        if((normalize(self.last_way.origin - self.origin) * self.b_dir) > 0.4)
        {
            self.b_aiflags = self.b_aiflags - AI_DIRECTIONAL;
            bot_lost(self.target1, TRUE);
        }
    }
    if(self.b_aiflags & AI_SNIPER)
    {
        self.b_aiflags = (self.b_aiflags | AI_WAIT | AI_PRECISION) - AI_SNIPER;
        // FIXME: Add a switch to wep command
        // FIXME: increase delay?
    }
    if(self.b_aiflags & AI_AMBUSH)
    {
        self.b_aiflags = (self.b_aiflags | AI_WAIT) - AI_AMBUSH;
        // FIXME: Add a switch to wep command
        // FIXME: increase delay?
    }

};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bot_path

Bot will follow a route generated by the
begin_route set of functions in bot_way.qc.
This code, while it works pretty well, can get
confused

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() bot_path =
{
    local entity jj, tele;

    bot_check_lost(self.target1);

    if(!self.target1)
    {
        self.keys = 0;
        return;
    }

    if(target_onstack(self.last_way))
    {
        return; // old waypoint still being hunted
    }

    jj = FindRoute(self.last_way);
    if(!jj)
    {
        // this is an ugly hack
        if(self.target1.current_way != self.last_way)
        {
            if(self.target1.classname != "temp_waypoint")
                if(self.target1.classname != "player")
                    bot_lost(self.target1, FALSE);
        }

        return;
    }

    // update the bot's special ai features

    // Readahed types are AI conditions to perform while heading to a waypoint
    // point types are AI flags that should be executed once reaching a waypoint

    self.b_aiflags = (jj.b_aiflags & AI_READAHEAD_TYPES) | (self.last_way.b_aiflags & AI_POINT_TYPES);
    target_add(jj);
    if(self.last_way)
    {
        if(CheckLinked(self.last_way, jj) == 2) // waypoints are telelinked
        {
            tele = FindThing("trigger_teleport"); // this is probbly the teleport responsible
            target_add(tele);
        }

        traceline(self.last_way.origin, jj.origin, FALSE, self); // check for blockage

        if(trace_fraction != 1)
        {
            if(trace_ent.classname == "door" && !(self.b_aiflags & AI_DOOR_NO_OPEN)) // a door blocks the way
            {
                // linked doors fix
                if(trace_ent.owner)
                    trace_ent = trace_ent.owner;
                if((trace_ent.health > 0) && (self.enemy == world))
                {
                    self.enemy = trace_ent;
                    bot_weapon_switch(1);
                    self.b_aiflags = self.b_aiflags | AI_BLIND; // nick knack paddy hack
                }
                else if(trace_ent.targetname != NULLSTR)
                {
                    tele = find(world, target, trace_ent.targetname);
                    if(tele.health > 0)
                    {
                        self.enemy = tele;
                        bot_weapon_switch(1);
                    }
                    else
                    {
                    //    target_drop(jj);
                        target_add(tele);
                    //    bot_get_path(tele, TRUE);
                        self.b_aiflags = self.b_aiflags | AI_BLIND; // give a bot a bone
                        return;
                    }
                }
            }
            else if(trace_ent.classname == "func_wall")
            {
                // give up
                bot_lost(self.target1, FALSE);
                return;
            }
        }
    }
    // this is used for AI_DRIECTIONAL
    self.b_dir = normalize(jj.origin - self.last_way.origin);

    self.last_way = jj;
};


/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

Bot Priority Look. What a stupid name. This is where
the bot finds things it wants to kill/grab.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/
// priority scale
// 0 - 10 virtually ignore
// 10 - 30 normal item range
// 30 - 50 bot will consider this a target worth changing course for
// 50 - 90 bot will hunt these as vital items

// *!* Make sure you add code to bot_check_lost to remove the target *!*

float(float wid) get_weapon_priority =
{
    if(wid == WID_ROCKET_LAUNCHER)  { return 60; }
    if(wid == WID_LIGHTNING)        { return 50; }
    if(wid == WID_GRENADE_LAUNCHER) { return 45; }
    if(wid == WID_SUPER_NAILGUN)    { return 35; }
    if(wid == WID_NAILGUN)          { return 30; }
    if(wid == WID_SUPER_SHOTGUN)    { return 25; }

    return 5;
}

float(entity thing) priority_for_thing =
{
    if(cvar_hget(cvarh_vr_verbosebots))
    {
        bprint("priority_for_thing(");
        bprint(thing.netname);
        bprint(")\n");
    }

    local float thisp, isThrownWeapon;
    thisp = 0;
    isThrownWeapon = 0;
    // This is the most executed function in the bot. Careful what you do here.

    if(
        ((thing.flags & FL_ITEM) || thing.classname == "thrown_weapon") &&
        (thing.model != string_null) &&
        (thing.search_time < time)
    )
    {
        // ugly hack
        if(thing._last != self)
            thisp = 20;
        //~ ai fix start

        if(thing.classname == "thrown_weapon" &&
           !VR_WeaponUtil_EntHasWeapon(self, thing.weapon))
        {
            thisp = get_weapon_priority(thing.weapon) + 1;
            isThrownWeapon = 1;
        }
        else if(thing.classname == "item_artifact_super_damage")
        {
            thisp = 10*(random()) + 55;
        }
        else if(thing.classname == "item_artifact_invulnerability")
        {
            thisp = 10*(random()) + 55;
        }
        else if(thing.classname == "item_artifact_invisibility")
        {
            thisp = 10*(random()) + 55;
        }
        else if(thing.classname == "item_health")
        {
            if(thing.spawnflags & 2)
                thisp = 10*(random()) + 50;
            if(self.health <= 50)
                thisp = 70;
        }
        else if(thing.classname == "weapon_rocketlauncher")
        {
            thisp = get_weapon_priority(WID_ROCKET_LAUNCHER);
        }
        else if(thing.classname == "weapon_lightning")
        {
            thisp = get_weapon_priority(WID_LIGHTNING);
        }
        else if(thing.classname == "weapon_grenadelauncher")
        {
            thisp = get_weapon_priority(WID_GRENADE_LAUNCHER);
        }
        else if(thing.classname == "weapon_supernailgun")
        {
            thisp = get_weapon_priority(WID_SUPER_NAILGUN);
        }
        else if(thing.classname == "weapon_nailgun")
        {
            thisp = get_weapon_priority(WID_NAILGUN);
        }
        else if(thing.classname == "weapon_supershotgun")
        {
            thisp = get_weapon_priority(WID_SUPER_SHOTGUN);
        }
        else if(thing.model == "progs/armor.mdl")
        {
            if(self.armorvalue < 200)
            {
                if(self.armorvalue <= 50)
                    thisp = 60;
                else if(self.armorvalue <= 100 && thing.classname != "item_armor1")
                    thisp = 55;
                else if(self.armorvalue <= 150 && thing.classname == "item_armorInv")
                    thisp = 50;
            }
        }
        else if(thing.model == "progs/backpack.mdl")
            thisp = 45;
        else if(thing.classname == "item_artifact_envirosuit")
            thisp = 30;
    }
    //~ ai fix end
    else if((thing.flags & FL_MONSTER) && thing.health > 0)
        thisp = 45;
    else if(thing.classname == "player")
    {
        if(thing.health > 0)
        {
            if(thing == self)
                return 0;
            else
            {
                if(VR_ItemUtil_EntHasItem(thing, IID_INVISIBILITY)) //FIXME
                    thisp = 2;
                else if(coop)
                {
                    thisp = 200;
                    if(thing.target1.classname == "player")
                        if(!thing.target1.ishuman)
                            return 0;
                }
                else if(teamplay && thing.team == self.team)
                {
                    thisp = 100;
                    if(thing.target1.classname == "player")
                        return 0;
                }
                else thisp = 30;
            }
        }
    }
    else if(thing.classname == "waypoint")
    {
        if(thing.b_aiflags & AI_SNIPER)
            thisp = 45; //~ ai fix
        else if(thing.b_aiflags & AI_AMBUSH)
            thisp = 30;
    }

    if(!isThrownWeapon &&
       pointcontents(thing.origin) < -3)
    {
        return 0;
    }

    if(thisp)
    {
        if(thing.current_way)
        {
            // check to see if it's unreachable
            if(thing.current_way.items == -1)
                return 0;
            else
                thisp = thisp + (13000 - thing.current_way.items) * 0.05;
        }
        else if(isThrownWeapon &&
                vlen(self.origin - thing.origin) < getForcegrabRange())
        {
            sound(self, VRGetGunChannel(cVR_MainHand), "forcegrab.wav", 0.75, ATTN_NORM);
            wpnthrow_handtouch_impl(cVR_MainHand, thing, self, &self.weapon, &self.weaponflags);
            thisp = 0;
        }
    }

    return thisp;
};

void(float scope) bot_look_for_crap =
{
    local float radius;

    if(scope == 1)
    {
        radius = 13000;
    }
    else
    {
        radius = 500;
    }

    if(cvar_hget(cvarh_vr_verbosebots))
    {
        bprint("bot_look_for_crap(");
        bprint(ftos(scope));
        bprint(") [radius: ");
        bprint(ftos(radius));
        bprint("]\n");
    }

    local entity foe, best;
    local float thatp, bestp, dist;

    foe = findradius(self.origin, radius);

    bestp = 1;
    while(foe)
    {
        thatp = priority_for_thing(foe);

        if(cvar_hget(cvarh_vr_verbosebots))
        {
            bprint(" -> ");
            bprint(ftos(thatp));
            bprint("\n\n");
        }

        if(thatp && !scope)
        {
            if(!sisible(foe))
            {
                thatp = 0;
            }
        }

        if(thatp > bestp)
        {
            bestp = thatp;
            best = foe;
            dist = vlen(self.origin - foe.origin);
        }

        foe = foe.chain;
    }

    if(best == world)
    {
        if(cvar_hget(cvarh_vr_verbosebots))
        {
            bprint("found nothing (best == world)\n");
        }

        return;
    }

    if(!target_onstack(best))
    {
        target_add(best);

        if(cvar_hget(cvarh_vr_verbosebots))
        {
            bprint("added best:");
            bprint(best.netname);
            bprint("\n");
        }

        if(scope)
        {
            bot_get_path(best, FALSE);
            self.b_aiflags = self.b_aiflags | AI_WAIT;
        }
    }
};


/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bot_angle_set

Sets the bots look keys & b_angle to point at
the target - used for fighting and just
generally making the bot look good.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() bot_angle_set =
{

    if(self.enemy)
    {
        if(self.enemy.items & 524288)
        {
            if(random() > 0.01)
            {
                return;
            }
        }

        if(self.missile_speed == 0)
        {
            self.missile_speed = 10000;
        }

        vector view;
        if(self.enemy.solid == SOLID_BSP)
        {
            view = (((self.enemy.absmin + self.enemy.absmax) * 0.5) - self.origin);
        }
        else
        {
            float levels = 3;

            view = self.enemy.origin;

            while(levels)
            {
                float h = vlen(view - self.origin) / self.missile_speed;
                if(self.enemy.flags & FL_ONGROUND)
                    view = self.enemy.velocity * h + '0 0 -20';
                else
                    view = (self.enemy.velocity - (sv_gravity * '0 0 1') * h) * h;
                view = self.enemy.origin + view;
                traceline(self.enemy.origin, view, FALSE, self);
                view = trace_endpos;
                levels = levels - 1;
            }

            view = normalize(view - self.origin);
        }

        view = vectoangles(view);
        view_x = view_x * -1;
        self.b_angle = view;
    }
    else if(self.target1)
    {
        vector view = realorigin(self.target1);

        if(self.target1.flags & FL_ITEM)
        {
            view = view + '0 0 48';
        }

        view = view - (self.origin + self.view_ofs);
        view = vectoangles(view);
        view_x = view_x * -1;
        self.b_angle = view;
    }
    else
        self.b_angle_x = 0;
    // HACK HACK HACK HACK
    // The bot falls off ledges a lot because of "turning around"
    // so let the bot use instant turn around when not hunting a player
    if(self.b_skill == 3)
    {
        self.keys = self.keys & 63;
        self.v_angle = self.b_angle;
        while (self.v_angle_x < -180)
            self.v_angle_x = self.v_angle_x + 360;
        while (self.v_angle_x > 180)
            self.v_angle_x = self.v_angle_x - 360;

    }
    else if((self.enemy == world || self.enemy.movetype == MOVETYPE_PUSH) && self.target1.classname != "player")
    {
        self.keys = self.keys & 63;
        self.v_angle = self.b_angle;
        while (self.v_angle_x < -180)
            self.v_angle_x = self.v_angle_x + 360;
        while (self.v_angle_x > 180)
            self.v_angle_x = self.v_angle_x - 360;
    }
    else if(self.b_skill < 2) // skill 2 handled in bot_phys
    {
        if(self.b_angle_x > 180)
            self.b_angle_x = self.b_angle_x - 360;
        self.keys = self.keys & 63;

        if(angcomp(self.b_angle_y, self.v_angle_y) > 10)
            self.keys = self.keys | KEY_LOOKLEFT;
        else if(angcomp(self.b_angle_y, self.v_angle_y) < -10)
            self.keys = self.keys | KEY_LOOKRIGHT;
        if(angcomp(self.b_angle_x, self.v_angle_x) < -10)
            self.keys = self.keys | KEY_LOOKUP;
        else if(angcomp(self.b_angle_x, self.v_angle_x) > 10)
            self.keys = self.keys | KEY_LOOKDOWN;
    }
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

BotAI

This is the main ai loop. Though called every
frame, the ai_time limits it's actual updating

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/
float stagger_think;

void() BotAI =
{
    // am I dead? Fire randomly until I respawn
    // health < 1 is used because fractional healths show up as 0 on normal player
    // status bars, and the mod probably already compensated for that

    if(self.health < 1)
    {
        self.button0 = floor(random() * 2);
        self.button2 = 0;
        self.keys = 0;
        self.b_aiflags = 0;
        ClearMyRoute();
        self.target1 = self.target2 = self.target3 = self.target4 = self.enemy = world;
        self.last_way = world;
        return;
    }

    // stagger the bot's AI out so they all don't think at the same time, causing game
    // 'spikes'
    if(self.b_skill < 2)
    {
        if(self.ai_time > time)
            return;

        self.ai_time = time + 0.05;
        if(bot_count > 0)
        {
            if((time - stagger_think) < (0.1 / bot_count))
                self.ai_time = self.ai_time + 0.1 / (2 * bot_count);
        }
        else
            return;
    }

    if(self.view_ofs == '0 0 0')
    {
        bot_start_topic(7);
    }

    stagger_think = time;

    // shut the bot's buttons off, various functions will turn them on by AI end

    self.button2 = 0;
    self.button0 = 0;

    // switch empty weapons
    if(!W_CheckNoAmmo(cVR_MainHand) && random() > 0.3)
    {
        bot_weapon_switch(1);
    }

    // target1 is like goalentity in normal Quake monster AI.
    // it's the bot's most immediate target
    if(route_table == self)
    {
        if(busy_waypoints <= 0)
        {
            if(waypoint_mode < WM_EDITOR)
            {
               bot_look_for_crap(TRUE);
            }
        }
        self.b_aiflags = 0;
        self.keys = 0;
    }
    else if(self.target1)
    {
        frik_movetogoal();
        bot_path();
    }
    else
    {
        if(waypoint_mode < WM_EDITOR)
        {
            if(self.route_failed)
            {
                if(waypoint_mode == WM_DYNAMIC)
                        frik_bot_roam();
                    else
                        self.keys = 0;
                self.route_failed = 0;
            }
            else if(!begin_route())
            {
                bot_look_for_crap(FALSE);
                self.keys = 0;
            }
        }
        else
        {
            self.b_aiflags = AI_WAIT;
            self.keys = 0;
        }
    }

    // bot_angle_set points the bot at it's goal (self.enemy or target1)

    bot_angle_set();

    if(self.weapon == WID_FIST)
    {
        bot_weapon_switch(-1);
    }

    // fight my enemy. Enemy is probably a field QC coders will most likely use a lot
    // for their own needs, since it's unused on a normal player
    // FIXME
    if(self.enemy)
    {
        bot_fight_style();
    }
    else if(random() < 0.2)
        if(random() < 0.2)
            bot_weapon_switch(-1);
    bot_dodge_stuff();

    // checks to see if bot needs to start going up for air
    if(self.waterlevel > 2)
      {
        if(time > (self.air_finished - 2))
        {
            traceline (self.origin, self.origin + '0 0 6800', TRUE, self);
            if(trace_inopen)
            {
                self.keys = KEY_MOVEUP;
                self.button2 = TRUE; // swim!
                return; // skip ai flags for now - this is life or death
            }
        }
        }

    // b_aiflags handling


    if(self.b_aiflags)
        bot_handle_ai();
    else
        bot_chat(); // don't want chat to screw him up if he's rjing or something
};
