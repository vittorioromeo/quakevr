/***********************************************
*                                              *
*             FrikBot Fight Code               *
*      "Because I ain't no Ghandi code"        *
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

.entity avoid;

float(entity e, float wid) ent_is_holding =
{
    return e.weapon == wid || e.weapon2 == wid;
}

float(entity e) bot_size_player =
{
    local float sz;

    sz = e.health + e.armorvalue * e.armortype;

    if(ent_is_holding(e, WID_ROCKET_LAUNCHER))
        sz = sz + 60;
    else if(ent_is_holding(e, WID_LIGHTNING))
        sz = sz + 60;
    else if(ent_is_holding(e, WID_GRENADE_LAUNCHER))
        sz = sz + 50;
    else if(ent_is_holding(e, WID_SUPER_NAILGUN))
        sz = sz + 50;
    else if(ent_is_holding(e, WID_NAILGUN))
        sz = sz + 40;
    else if(ent_is_holding(e, WID_SUPER_SHOTGUN))
        sz = sz + 40;
    else if(ent_is_holding(e, WID_SHOTGUN))
        sz = sz + 10;
    else if(ent_is_holding(e, WID_AXE))
        sz = sz - 50;

    if(VR_ItemUtil_EntHasItem(e, IID_QUAD))
        sz = sz + 200;

    if(VR_ItemUtil_EntHasItem(e, IID_INVULNERABILITY))
        sz = sz + 300;

    if(VR_ItemUtil_EntHasItem(e, IID_INVISIBILITY))
        sz = sz + 250;

    return sz;
};

void() bot_dodge_stuff =
{
    local entity foe;
    local float avdist, foesz, flen, tsz;
    local vector v;

    if(waypoint_mode > WM_LOADED)
        return;

    self.avoid = world;


    if(self.enemy)
    {
        v = self.origin - realorigin(self.enemy);

        foesz = bot_size_player(self.enemy);
        foesz = foesz + vlen(v) * 0.5;
    }
    else
        foesz = 9999999;
    avdist = 256;

    foe = find(world, classname, "grenade");
    while(foe)
    {
        flen = vlen(foe.origin - self.origin);
        if(flen < avdist)
        {
            avdist = flen;
            self.avoid = foe;
        }
        foe = find(foe, classname, "grenade");
    }
    if(!self.avoid)
    {
        foe = find(world, classname, "missile");
        while(foe)
        {
            if(foe.owner != self)
            {
                flen = vlen(foe.origin - self.origin);
                if(flen < avdist)
                {
                    avdist = flen;
                    self.avoid = foe;
                }
            }
            foe = find(foe, classname, "missile");
        }
        if(!self.avoid)
        {
            foe = find(world, classname, "spike");
            while(foe)
            {
                if(foe.owner != self)
                {
                    flen = vlen(foe.origin - self.origin);
                    if(flen < avdist)
                    {
                        avdist = flen;
                        self.avoid = foe;
                    }
                }
                foe = find(foe, classname, "spike");
            }
        }
    }
    if(coop)
    {
        if(!self.enemy)
        {
            foe = findradius(self.origin, 9999);
            while(foe)
            {
                if(foe.flags & FL_MONSTER)
                {
                    if(foe.health > 0)
                    {
                        tsz = bot_size_player(foe) + vlen(foe.origin - self.origin) * 0.5;
                        if(tsz < foesz)
                        {
                            if(fisible(foe))
                            {
                                self.enemy = foe;
                                foesz = tsz;
                            }
                        }
                    }
                }
                foe = foe.chain;
            }
        }
    }
    else
    {
        foe = player_head;
        while(foe)
        {
            if(foe != self)
            {
                if(foe.modelindex != 0)
                {
                    if(foe.health > 0)
                    {
                        if(!(teamplay && self.team == foe.team))
                        {
                            tsz = bot_size_player(foe) + vlen(foe.origin - self.origin) * 0.5;
                            if(tsz < foesz)
                            {
                                if(fov(foe) || foe.b_sound > time || self.b_skill == 3)
                                {
                                    if(fisible(foe))
                                    {
                                        self.enemy = foe;
                                        foesz = tsz;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            foe = foe._next;
        }
    }
};




/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

weapon_range

_x "sweet spot range" - try to maintain this range if possible
_y minimum range bot can be to be effective (rl/gl) (move away)
_z maximum range bot can be to be effective (lg/axe) (move in)
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

vector(float wid) weapon_range =
{
    if(wid == WID_AXE)              { return '48 0 64'; }
    if(wid == WID_MJOLNIR)          { return '48 0 64'; }
    if(wid == WID_SHOTGUN)          { return '128 8 3000'; }
    if(wid == WID_SUPER_SHOTGUN)    { return '128 0 3000'; }
    if(wid == WID_NAILGUN)          { return '180 0 3000'; }
    if(wid == WID_SUPER_NAILGUN)    { return '180 0 3000'; }
    if(wid == WID_GRENADE_LAUNCHER) { return '180 48 3000'; }
    if(wid == WID_PROXIMITY_GUN)    { return '180 48 3000'; }
    if(wid == WID_ROCKET_LAUNCHER)  { return '180 48 3000'; }
    if(wid == WID_LIGHTNING)        { return '350 0 512'; }
    if(wid == WID_LASER_CANNON)     { return '350 0 512'; }

    bprint("Invalid wid in bot weapon_range: '");
    bprint(ftos(wid));
    bprint("'\n");

    return '128 8 3000';
};
/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bot_weapon_switch

Pick a weapon based on range / ammo

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

float(float brange) bot_pick_weapon =
{
    local float wid;
    local vector v;

    if( (VR_WeaponUtil_EntHasWeapon(self, WID_ROCKET_LAUNCHER)) &&
        (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_ROCKETS) >= 1))
    {
        wid = WID_ROCKET_LAUNCHER;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    if(self.waterlevel <= 1 &&
       (VR_WeaponUtil_EntHasWeapon(self, WID_LIGHTNING)) &&
       (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_CELLS) >= 1))
    {
        wid = WID_LIGHTNING;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    if((VR_WeaponUtil_EntHasWeapon(self, WID_SUPER_NAILGUN)) &&
       (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_NAILS) >= 2))
    {
        wid = WID_SUPER_NAILGUN;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    if((VR_WeaponUtil_EntHasWeapon(self, WID_GRENADE_LAUNCHER)) &&
       (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_ROCKETS) >= 1))
    {
        wid = WID_GRENADE_LAUNCHER;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    if((VR_WeaponUtil_EntHasWeapon(self, WID_SUPER_SHOTGUN)) &&
       (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_SHELLS) >= 2))
    {
        wid = WID_SUPER_SHOTGUN;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    if((VR_WeaponUtil_EntHasWeapon(self, WID_NAILGUN)) &&
       (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_NAILS) >= 1))
    {
        wid = WID_NAILGUN;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    if((VR_WeaponUtil_EntHasWeapon(self, WID_SHOTGUN)) &&
       (VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_SHELLS) >= 1))
    {
        wid = WID_SHOTGUN;
        v = weapon_range(wid);

        if(brange == -1 || (brange >= v_y && brange <= v_z))
        {
            return wid;
        }
    }

    wid = WID_AXE;
    return wid;
};

void(float brange) bot_weapon_switch =
{
    self.weapon = bot_pick_weapon(brange);
    W_SetCurrentAmmo();
};

void() bot_setvrfields =
{
    if(self.ishuman)
        return;

    self.handrot = self.v_angle;
    self.offhandrot = self.v_angle;

    self.handpos = self.origin + '0 0 16';
    self.offhandpos = self.origin + '0 0 16';

    self.muzzlepos = self.origin + '0 0 16';
    self.offmuzzlepos = self.origin + '0 0 16';

    self.mainhand_grabbing = TRUE;
    self.mainhand_prevgrabbing = TRUE;

    self.offhand_grabbing = TRUE;
    self.offhand_prevgrabbing = TRUE;
}

void() bot_shoot =
{
    // quick little function to stop making him shoot the wrong way ! Argh
    local float g;

    g = angcomp(self.v_angle_x, self.b_angle_x);
    if(fabs(g) > 30)
        return; // argh, too far away

    g = angcomp(self.v_angle_y, self.b_angle_y);
    if(fabs(g) > 30)
        return; // not again!

    bot_setvrfields();
    self.button0 = TRUE;
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

Bot_fight_style

This is the core of the bot's thinking when
attacking an enemy.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() bot_fight_style =
{
    local vector v, v1, v2, org;
    local float foedist, mysz, foesz;

    if(self.enemy.health <= 0)
    {
        self.enemy = world;
        return;
    }
    else if(!self.enemy.takedamage)
    {
        self.enemy = world;
        return;
    }
    else if(!fisible(self.enemy))
    {
        self.enemy = world;
        return;
    }

    org = realorigin(self.enemy);
    makevectors(self.v_angle);

    // decide if I should shoot

    foedist = vlen(org - self.origin);

    v = weapon_range(self.weapon);

    if(foedist > v_y && foedist < v_z)
    {
        traceline(self.origin + self.view_ofs, self.origin + self.view_ofs + v_forward * v_z, FALSE, self);
        if(vlen(trace_endpos - (self.origin + self.view_ofs)) >= v_y)
        {
            // try to avoid shooting teammates
            if(trace_ent.classname == "player")
                if((trace_ent.team == self.team && teamplay) || (coop))
                    return;
            bot_shoot();
        }
    }
    else
        bot_weapon_switch(foedist);

    if(!(self.b_aiflags & (AI_PRECISION | AI_BLIND | AI_OBSTRUCTED)))
    {
        foesz = bot_size_player(self.enemy);
        mysz = bot_size_player(self) + 5;

        if(foesz > mysz)
        {
            if(teamplay)
            {
                if(random() < 0.02)
                {
                    bot_start_topic(5);
                    self.b_chattime = 1;
                }
            }

            return;
        }
        else if(mysz < 140)
            return;
        else if(self.avoid)
        {
            if(self.avoid.velocity)
                v = self.avoid.velocity;
            else
                v = normalize(self.avoid.origin - self.origin);
            v1_x = v_y * -1;
            v1_y = v_x;
            v2_x = v_y;
            v2_y = v_x * -1;
            foedist = vlen(self.avoid.origin - (self.origin + v1));
            if(foedist < vlen(self.avoid.origin - (self.origin + v2)))
                frik_walkmove(v2);
            else
                frik_walkmove(v1);
        }
        else if(!(self.enemy.flags & FL_MONSTER))
        {
            if(foedist + 32 <  v_x)
            frik_walkmove(self.origin - org);
            else if(foedist - 32 >  v_x)
                frik_walkmove(org - self.origin);
            else if(self.wallhug)
                frik_walkmove(v_right);
            else
                frik_walkmove(v_right * -1);
        }
    }
    else
    {
        foesz = bot_size_player(self.enemy);
        mysz = bot_size_player(self) + 5;

        if(foesz > mysz)
            return;
        else if(mysz < 140)
            return;
        self.keys = self.keys & 960;
    }
};


