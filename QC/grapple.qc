// Rogue Grapple Implementation
// Jan'97 by ZOID <zoid@threewave.com>
// Under contract to id software for Rogue Entertainment

// New entity fields
.entity hook; // this is my hook
.float on_hook; // we're on it
.float hook_out; // it's out

// prototypes for WEAPONS.QC functions
float() crandom;
void(vector org, vector vel, float damage) SpawnBlood;

void(entity h, vector startPos, vector endPos) GrappleTrail =
{
    // draw a line to the hook
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_BEAM);
    WriteEntity (MSG_BROADCAST, h);
    WriteCoord (MSG_BROADCAST, startPos_x);
    WriteCoord (MSG_BROADCAST, startPos_y);
    WriteCoord (MSG_BROADCAST, startPos_z);
    WriteCoord (MSG_BROADCAST, endPos_x);
    WriteCoord (MSG_BROADCAST, endPos_y);
    WriteCoord (MSG_BROADCAST, endPos_z);
};

void() GrappleReset =
{
    self.owner.on_hook = FALSE;
    self.owner.hook_out = FALSE;
    // TODO VR: assumes offhand (TODO dual-wielding)
    self.owner.weaponframe2 = 0;
    self.owner.offhand_attack_finished = time + 0.25;

    remove(self);
};

void() GrappleTrack =
{
    local   vector  spray;

    // Release dead targets
    if (self.enemy.classname == "player" && self.enemy.health <= 0)
    {
        self.owner.on_hook = FALSE;
    }

    // drop the hook if owner is dead or has released the button
    if (!self.owner.on_hook || self.owner.health <= 0)
    {
        GrappleReset();
        return;
    }

    if (self.enemy.classname == "player")
    {
        if (self.enemy.teleport_time > time)
        {
            GrappleReset();
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
    if (other == self.owner) // don't hook the guy that fired it
        return;

    if (pointcontents(self.origin) == CONTENT_SKY) {
        GrappleReset();
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

    if (!self.owner.button3) {
        GrappleReset();
        return;
    }

    self.owner.on_hook = TRUE;
    if (self.owner.flags & FL_ONGROUND)
        self.owner.flags = self.owner.flags - FL_ONGROUND;

    self.enemy = other;// remember this guy!
    self.think = GrappleTrack;
    self.nextthink = time;
    self.solid = SOLID_NOT;
    self.touch = SUB_Null;
};

void(float xduration, float frequency, float amplitude) VRGunHaptic;

void () W_FireGrapple =
{
    if (self.hook_out)// reject subsequent calls from player.qc
        return;

    VRGunHaptic(0.55, 75, 1.0);
    particle2 (self.offmuzzlepos, '0 0 0', 5 /* sparks preset */, 8);
    particle2 (self.offmuzzlepos, '0 0 0', 6 /* gun smoke preset */, 1);

    self.punchangle_x = -2; // bump him

    // chain out sound (loops)
    sound (self, CHAN_WEAPON, "weapons/chain1.wav", 1, ATTN_NORM);

    newmis = spawn();
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid = SOLID_BBOX;
    newmis.owner = self;        // newmis belongs to me
    self.hook = newmis;            // This is my newmis
    newmis.classname = "hook";

    makevectors (self.offhandrot);
    newmis.velocity = v_forward * 1500;
//    newmis.avelocity = '0 0 -500';
    newmis.angles = vectoangles(v_forward);

    newmis.touch = GrappleAnchor;
    newmis.think = GrappleReset;
    // grapple only lives for two seconds, this gives max range on it
    newmis.nextthink = time + 1.8;
    newmis.frame = 1; // hook spread

    setmodel (newmis,"progs/hook.mdl");
    setorigin (newmis, self.offhandpos + v_forward * 6);
    setsize(newmis, '0 0 0' , '0 0 0 ');

    self.hook_out = TRUE;
};

// called each frame by CLIENT.QC if client has hook_out
void() GrappleService =
{
    local   vector  o,  vel, hookfwd;
    local    float    v, dot;

    makevectors (self.hook.angles);
    hookfwd = v_forward;

    makevectors (self.offhandrot);

    if (!self.on_hook) {
        // TODO VR: still wonky position on hook , dunno why
        GrappleTrail(self.hook, self.hook.origin - hookfwd * 6.5, self.offmuzzlepos + v_forward * 1.5);
        return;
    }

    // drop the hook if player lets go of button
    if ((!self.button3) ||
        self.teleport_time > time ||
        self.weapon2 != WID_GRAPPLE) // TODO VR: assumes offhand (TODO dual-wielding)
    { // release when we get 'ported
        self = self.hook;
        GrappleReset();
        return;
    }

    vel = self.hook.origin - self.offmuzzlepos;
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

    GrappleTrail(self.hook, self.hook.origin - hookfwd * 6.5, self.offmuzzlepos + v_forward * 1.5);
};


