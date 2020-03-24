// Rogue Grapple Implementation
// Jan'97 by ZOID <zoid@threewave.com>
// Under contract to id software for Rogue Entertainment

// New entity fields
.entity hook; // this is my hook
.float on_hook; // we're on it
.float hook_out; // it's out

.entity hook2; // (off-hand) this is my hook
.float on_hook2; // (off-hand) we're on it
.float hook_out2; // (off-hand) it's out

.float handParm;

void(float xHand, entity v) setHook =
{
    if(xHand == 0) self.hook2 = v;
    else           self.hook = v;
}

void(float xHand, float v) setOnHook =
{
    if(xHand == 0) self.on_hook2 = v;
    else           self.on_hook = v;
}

void(float xHand, float v) setHookOut =
{
    if(xHand == 0) self.hook_out2 = v;
    else           self.hook_out = v;
}

entity(float xHand) getHook =
{
    if(xHand == 0) return self.hook2;
    else           return self.hook;
}

float(float xHand) getOnHook =
{
    if(xHand == 0) return self.on_hook2;
    else           return self.on_hook;
}

float(float xHand) getHookOut =
{
    if(xHand == 0) return self.hook_out2;
    else           return self.hook_out;
}

void(float xHand, float v) setOwnerOnHook =
{
    if(xHand == 0) self.owner.on_hook2 = v;
    else           self.owner.on_hook = v;
}

void(float xHand, float v) setOwnerHookOut =
{
    if(xHand == 0) self.owner.hook_out2 = v;
    else           self.owner.hook_out = v;
}

float(float xHand) getOwnerOnHook =
{
    if(xHand == 0) return self.owner.on_hook2;
    else           return self.owner.on_hook;
}

float(float xHand) getOwnerHookOut =
{
    if(xHand == 0) return self.owner.hook_out2;
    else           return self.owner.hook_out;
}

float(float xHand) ownerFireButtonPressed =
{
    if(xHand == 0) return self.owner.button3;
    else           return self.owner.button0;
}

// prototypes for WEAPONS.QC functions
float() crandom;
void(vector org, vector vel, float damage) SpawnBlood;

void(entity h, vector startPos, vector endPos) GrappleTrail =
{
    // draw a line to the hook
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_BEAM);
    WriteEntity (MSG_BROADCAST, h);
    WriteByte (MSG_BROADCAST, 0); /* disambiguator */
    WriteCoord (MSG_BROADCAST, startPos_x);
    WriteCoord (MSG_BROADCAST, startPos_y);
    WriteCoord (MSG_BROADCAST, startPos_z);
    WriteCoord (MSG_BROADCAST, endPos_x);
    WriteCoord (MSG_BROADCAST, endPos_y);
    WriteCoord (MSG_BROADCAST, endPos_z);
};

void() GrappleResetFromHook =
{
    local float xHand;
    xHand = self.handParm;

    setOwnerOnHook(xHand, FALSE);
    setOwnerHookOut(xHand, FALSE);
    setWeaponFrame(xHand, 0);
    setAttackFinished(xHand, time + 0.25);

    remove(self);
};

void(float xHand) GrappleResetFromPlayer =
{
    setOwnerOnHook(xHand, FALSE);
    setOwnerHookOut(xHand, FALSE);
    setWeaponFrame(xHand, 0);
    setAttackFinished(xHand, time + 0.25);

    remove(self);
};

void() GrappleTrack =
{
    local vector spray;
    local float xHand;

    xHand = self.handParm;

    // Release dead targets
    if (self.enemy.classname == "player" && self.enemy.health <= 0)
    {
        setOwnerOnHook(xHand, FALSE);
    }

    // drop the hook if owner is dead or has released the button
    if (!getOwnerOnHook(xHand) || self.owner.health <= 0)
    {
        GrappleResetFromHook();
        return;
    }

    if (self.enemy.classname == "player")
    {
        if (self.enemy.teleport_time > time)
        {
            GrappleResetFromHook();
            return;
        }

        // move the hook along with the player.  It's invisible, but
        // we need this to make the sound come from the right spot
        setorigin(self, self.enemy.origin);

//        sound (self, CHAN_WEAPON, "blob/land1.wav", 1, ATTN_NORM);
        sound (self, CHAN_WEAPON, "pendulum/hit.wav", 1, ATTN_NORM);
        T_Damage (self.enemy, self, self.owner, 1);
        makevectors (self.v_angle);
        spray_x = 100 * crandom();
        spray_y = 100 * crandom();
        spray_z = 100 * crandom() + 50;
        SpawnBlood(self.origin, spray, 20);
    }
    if (self.enemy.solid == SOLID_SLIDEBOX) {
        self.velocity = '0 0 0';
        setorigin(self, self.enemy.origin + self.enemy.mins +
            self.enemy.size * 0.5);
    } else
        self.velocity = self.enemy.velocity;

    self.nextthink = time + 0.1;
};

// Tries to anchor the grapple to whatever it touches
void () GrappleAnchor =
{
    local float xHand;

    xHand = self.handParm;

    if (other == self.owner) // don't hook the guy that fired it
        return;

    if (pointcontents(self.origin) == CONTENT_SKY) {
        GrappleResetFromHook();
        return;
    }

    if (other.classname == "player")
    {
        // glance off of teammates
        // TODO VR: rogue
        // if (other.steam == self.owner.steam)
        // {
        //     GrappleReset();        // PGM - fix drift after teammate hit  01/20/97
        //     return;
        // }

        sound (self, CHAN_WEAPON, "player/axhit1.wav", 1, ATTN_NORM);
        T_Damage (other, self, self.owner, 10);
    } else {
        sound (self, CHAN_WEAPON, "player/axhit2.wav", 1, ATTN_NORM);

        // One point of damage inflicted upon impact. Subsequent
        // damage will only be done to PLAYERS... this way secret
        // doors and triggers will only be damaged once.
        if (other.takedamage)
        {
            T_Damage (other, self, self.owner, 1);
        }

        self.velocity = '0 0 0';
        self.avelocity = '0 0 0';
    }

    self.frame = 2; // anchored

    sound (self.owner, CHAN_WEAPON, "weapons/tink1.wav", 1, ATTN_NORM);
    particle2 (self.origin, '0 0 0', 0 /* bullet puff preset */, 48);

    if (!ownerFireButtonPressed(xHand)) {
        GrappleResetFromHook();
        return;
    }

    setOwnerOnHook(xHand, TRUE);
    if (self.owner.flags & FL_ONGROUND)
        self.owner.flags = self.owner.flags - FL_ONGROUND;

    self.enemy = other;// remember this guy!
    self.think = GrappleTrack;
    self.nextthink = time;
    self.solid = SOLID_NOT;
    self.touch = SUB_Null;
};

void(float xduration, float frequency, float amplitude) VRGunHaptic;

void (float xHand) W_FireGrapple =
{
    if (getHookOut(xHand))// reject subsequent calls from player.qc
        return;

    VRGunHaptic(0.55, 75, 1.0);
    particle2 (VRGetMuzzlePos(xHand), '0 0 0', 5 /* sparks preset */, 8);
    particle2 (VRGetMuzzlePos(xHand), '0 0 0', 6 /* gun smoke preset */, 1);

    self.punchangle_x = -2; // bump him

    // chain out sound (loops)
    sound (self, CHAN_WEAPON, "weapons/chain1.wav", 1, ATTN_NORM);

    newmis = spawn();
    newmis.handParm = xHand;
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid = SOLID_BBOX;
    newmis.owner = self;        // newmis belongs to me
    setHook(xHand, newmis);            // This is my newmis
    newmis.classname = "hook";

    makevectors (VRGetHandRot(xHand));
    newmis.velocity = v_forward * 1500;
//    newmis.avelocity = '0 0 -500';
    newmis.angles = vectoangles(v_forward);

    newmis.touch = GrappleAnchor;
    newmis.think = GrappleResetFromHook;
    // grapple only lives for two seconds, this gives max range on it
    newmis.nextthink = time + 1.8;
    newmis.frame = 1; // hook spread

    setmodel (newmis,"progs/hook.mdl");
    setorigin (newmis, VRGetMuzzlePos(xHand) + v_forward * 6);
    setsize(newmis, '0 0 0' , '0 0 0 ');

    setHookOut(xHand, TRUE);
};

// called each frame by CLIENT.QC if client has hook_out
void(float xHand) GrappleService =
{
    local vector vel, hookfwd;
    local float  v, dot;
    local entity xHook;

    xHook = getHook(xHand);

    makevectors (xHook.angles);
    hookfwd = v_forward;

    makevectors (VRGetHandRot(xHand));

    if (!getOnHook(xHand)) {
        // TODO VR: still wonky position on hook , dunno why
        GrappleTrail(xHook, xHook.origin - hookfwd * 6.5, VRGetMuzzlePos(xHand) + v_forward * 1.5);
        return;
    }

    // drop the hook if player lets go of button
    if ((!VRFireButtonPressed(xHand)) ||
        self.teleport_time > time ||
        VRGetWeapon(xHand) != WID_GRAPPLE) // TODO VR: assumes offhand (TODO dual-wielding)
    { // release when we get 'ported
        self = xHook;
        GrappleResetFromPlayer(xHand);
        return;
    }

    vel = xHook.origin - VRGetMuzzlePos(xHand);
    v = vlen (vel);

    // if (v <= 250)
    // {
    //     vel = normalize(vel) * 250;
    // }

    if (v >= 350)
    {
        vel = normalize(vel) * 350;
    }

    dot = vel * self.velocity;

    if (dot < 0 || vlen(self.velocity) < 800)
    {
        self.velocity = self.velocity + (vel * 8 * frametime);
    }

    GrappleTrail(xHook, xHook.origin - hookfwd * 6.5, VRGetMuzzlePos(xHand) + v_forward * 1.5);
};


