// LAVA Weapon Routines


void() lavaspike_think =
{
    self.nextthink = time + 0.01;

    particle2(self.origin, '0 0 0', 10 /* lava spike preset */, 5);

    if(time >= self.lip)
    {
        SUB_Remove();
    }
}

/*
===============
launch_lava_spike

Used for both the player and the ogre
===============
*/
entity(vector org, vector dir) launch_lava_spike =
{
    local entity newmis;

    newmis = spawn ();
    newmis.owner = self;
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid = SOLID_BBOX;

    newmis.angles = vectoangles(dir);

    newmis.touch = lavaspike_touch;
    newmis.classname = "lava_spike";

    newmis.think = lavaspike_think;
    newmis.nextthink = time + 0.01;
    newmis.lip = time + 6; // deletion time

    newmis.effects = EF_VERYDIMLIGHT;

    setmodel (newmis, "progs/lspike.mdl");
    setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
    setorigin (newmis, org);

    newmis.velocity = dir * 1750;

    return newmis;
};

void(float xHand) W_FireSuperLavaSpikes =
{
    local vector dir;
    local entity newmis;

    sound(self, VRGetGunChannel(xHand), "weapons/spike2.wav", 1, ATTN_NORM);
    VRSetAttackFinished(xHand, time + 0.2);
    delAmmoAndUpdateCounter(xHand, AID_LAVA_NAILS, 2);

    makeforward(VRGetWeaponFireRot(xHand));
    dir = v_forward;

    newmis = launch_lava_spike(VRGetMuzzlePos(xHand), dir);
    newmis.touch = superlavaspike_touch;

    setmodel(newmis, "progs/lspike.mdl");
    setsize(newmis, VEC_ORIGIN, VEC_ORIGIN);

    self.punchangle_x = -2;

    VRGunHaptic(xHand, 0.05, 150, 1.0);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', 6 /* gun smoke preset */, 1);
};

void(float xHand, float ox) W_FireLavaSpikes =
{
    local vector dir;

    makevectors(VRGetWeaponFireRot(xHand));

    if(self.ammo_lava_nails >= 2 && VRGetWeapon(xHand) == WID_SUPER_NAILGUN)
    {
        W_FireSuperLavaSpikes(xHand);
        return;
    }

    if(self.ammo_lava_nails < 1)
    {
        return;
    }

    sound(self, VRGetGunChannel(xHand), "weapons/rocket1i.wav", 1, ATTN_NORM);
    VRSetAttackFinished(xHand, time + 0.2);
    delAmmoAndUpdateCounter(xHand, AID_LAVA_NAILS, 1);

    dir = v_forward;
    launch_lava_spike(VRGetMuzzlePos(xHand) + v_right*ox, dir);

    self.punchangle_x = -2;

    VRGunHaptic(xHand, 0.05, 150, 1.0);
    particle2(VRGetMuzzlePos(xHand) + v_right*ox, '0 0 0', 6 /* gun smoke preset */, 1);
};

// =============== Lava Spike Touch Routines

void() lavaspike_touch =
{
local float rand;
local float old_armortype;
local float old_armorvalue;
local float old_armormask;

    if(other == self.owner)
        return;

    if(other.solid == SOLID_TRIGGER)
        return;    // trigger field, do nothing

    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    makeforward_mdl(self.angles);
    traceline(self.origin - v_forward * 8, self.origin + v_forward * 8, FALSE, self);

    // hit something that bleeds
    if(other.takedamage)
    {
        spawn_touchblood (9);
        if(other.classname == "player")
        {
            // old_armortype = other.armortype;
            // old_armorvalue = other.armorvalue;
            // old_armormask = other.items2 & (IT2_ARMOR1|IT2_ARMOR2|IT2_ARMOR3);
            // other.armortype = 0;
            // other.armorvalue = 0;

            // TODO VR: (P0) pass a flag to `T_Damage` to ignore armor
            T_Damage (other, self, self.owner, 9);

            // other.armortype = old_armortype;
            // other.armorvalue = old_armorvalue;
            // other.items2 = other.items2 | old_armormask;
        }
        else        // is a monster
        {
            if(other.classname != "monster_lava_man")
            {
                T_Damage (other, self, self.owner, 15);
            }
        }
    }
    else
    {
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);

        if(self.classname == "wizspike")
            WriteByte (MSG_BROADCAST, TE_WIZSPIKE);
        else if(self.classname == "knightspike")
            WriteByte (MSG_BROADCAST, TE_KNIGHTSPIKE);
        else
        {
            spike_bullethole();
            WriteByte (MSG_BROADCAST, TE_SPIKE);
        }
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
    }

    remove(self);

};

void() superlavaspike_touch =
{
local float rand;
local float old_armortype;
//local float old_armorvalue;

    if(other == self.owner)
        return;

    if(other.solid == SOLID_TRIGGER)
        return;    // trigger field, do nothing

    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    makeforward_mdl(self.angles);
    traceline(self.origin - v_forward * 8, self.origin + v_forward * 8, FALSE, self);

    // hit something that bleeds
    if(other.takedamage)
    {
        spawn_touchblood (18);

        // halve the effectiveness of the armor for players..
        if(other.classname == "player")
        {
            // save the old armor values...
            old_armortype = other.armortype;
//            old_armorvalue = other.armorvalue;
            other.armortype = other.armortype * 0.5;
//            other.armorvalue = 0;
            T_Damage (other, self, self.owner, 18);

            // if the damage didn't wipe out the armor, armortype
            if(other.armortype != 0)
            {
                other.armortype = old_armortype;
//                other.armorvalue = old_armorvalue;
            }
        }
        else        // is a monster, do 50% more damage
        {
            if( other.classname != "monster_lava_man")
            {
                T_Damage (other, self, self.owner, 30);
            }
        }
    }
    else
    {
        spike_bullethole();
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_SUPERSPIKE);
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
    }

    remove(self);

};
