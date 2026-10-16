// buzzsaw.qc

$skin buzzsaw
$frame buzzsaw
$frame bzzrot01 bzzrot02 bzzrot03 bzzrot04 bzzrot05 bzzrot06

void() buzzsaw_stand1 = [ $buzzsaw, buzzsaw_stand1 ]
{
    if(self.pain_finished < time)
    {
        sound(self, CHAN_VOICE, "buzz/buzz1.wav", 0.2, ATTN_NORM);
        self.pain_finished = time + 1;
    }

    self.angles_x = self.angles_x - 60;
    self.avelocity_x = 60;
};

void() buzzsaw_fly =
{
    vector     dir;

    if(self.pain_finished < time)
    {
        sound(self, CHAN_VOICE, "buzz/buzz1.wav", 0.2, ATTN_NORM);
        self.pain_finished = time + 1;
    }

    dir = self.goalentity.origin - self.origin;
    dir = normalize(dir);
    dir = dir * self.speed;
    setorigin(self, self.origin + dir);
    self.angles_x = self.angles_x - 60;
    self.avelocity_x = 60;
};

/*
void() buzzsaw_fly1 = [ $bzzrot01, buzzsaw_fly2 ] { buzzsaw_fly(); };
void() buzzsaw_fly2 = [ $bzzrot02, buzzsaw_fly3 ] { buzzsaw_fly(); };
void() buzzsaw_fly3 = [ $bzzrot03, buzzsaw_fly4 ] { buzzsaw_fly(); };
void() buzzsaw_fly4 = [ $bzzrot04, buzzsaw_fly5 ] { buzzsaw_fly(); };
void() buzzsaw_fly5 = [ $bzzrot05, buzzsaw_fly6 ] { buzzsaw_fly(); };
void() buzzsaw_fly6 = [ $bzzrot06, buzzsaw_fly1 ] { buzzsaw_fly(); };
*/

void() buzzsaw_fly1 = [ $bzzrot01, buzzsaw_fly1 ] { buzzsaw_fly(); };

void() buzzsaw_touch =
{
    vector sprayDir;

    if(isragdoll(other))
    {
        return; // QVR: a ragdoll's parts are cut where they are in the blade (VR_Buzzsaw_Ragdolls below)
    }
    if(other.classname == "player" || other.flags & FL_MONSTER)
    {
        if(self.attack_finished < time)
        {
            sound(self, CHAN_WEAPON, "buzz/buzz.wav", 1, ATTN_NORM);
            self.attack_finished = time + 2;
        }
        T_Damage(other, self, self, self.currentammo);

        sprayDir = normalize(self.goalentity.origin - self.origin);
        sprayDir = sprayDir * 200;
        SpawnMeatSpray (self.origin, sprayDir);

        other.velocity = sprayDir;
        other.velocity_z = 200;
    }
};

// ----------------------------------------------------------------------------
// QVR: the blade cuts ragdolls (the author's, round 22: ROUND22.md, "The buzzsaw cuts ragdolls"). A corpse's or a
// knocked-down monster's ragdoll is Box3D's bodies, which no trigger touches; so each frame the running blade's box is
// looked into for ragdoll parts (ragdollinbox, ragdollboxpart: the engine's Box3D query at the blade only). Each part in
// it takes the blade's heavy hit through the shared path (VR_Limb_RagdollHit, vr_limbs.qc): a head or limb comes off as
// the chainsaw's cut (a corpse's once worn through: its piece health), a torso gibs, a living one dies of it.

float vr_buzzsaws;                // buzzsaws spawned in this map (saved): none, no frame looks for ragdolls
.float vr_buzz_next;              // a ragdoll's next blade hit (VR_BUZZ_GAP after its last)
float VR_BUZZ_GAP = 0.1;          // s between a ragdoll's blade hits (the blade's spin step)
float VR_BUZZ_RAGDOLL_SCALE = 10; // a ragdoll's part takes the blade's damage (currentammo, 10) times this a hit
float VR_BUZZ_GROW = 2;           // units the blade's box (thin across) grows each way for ragdoll parts
float VR_BUZZ_FLING = 350;        // u/s a piece cut off flies off the blade's edge (and up, VR_BUZZ_FLING_UP)
float VR_BUZZ_FLING_UP = 150;

// The blade (self) strikes ragdoll `e` at `xAt` on a part: its sound, blood, then the shared hit. Its piece returned.
float(entity e, vector xAt) VR_Buzzsaw_Hit =
{
    float alive = e.health > 0;
    vector vel = normalize(xAt - self.origin) * VR_BUZZ_FLING;
    vel_z = vel_z + VR_BUZZ_FLING_UP;
    float damage = self.currentammo * VR_BUZZ_RAGDOLL_SCALE;
    if(self.attack_finished < time)
    {
        sound(self, CHAN_WEAPON, "buzz/buzz.wav", 1, ATTN_NORM);
        self.attack_finished = time + 2;
    }
    SpawnMeatSpray(xAt, vel * 0.5);
    SpawnBlood(xAt, vel * 0.2, damage);
    float piece = VR_Limb_RagdollHit(e, self, xAt, vel, damage, QVR_DECAP_SAW);
    dprint(sprintf("buzzsaw: %s (%s) struck at %v on %s for %g: health %g, ragdoll %g\n", e.classname,
        alive ? "alive" : "corpse", xAt, piece < 0 ? "its torso" : (piece == 0 ? "its head" : sprintf("limb %g", piece)),
        damage, e.health, isragdoll(e)));
    return piece;
};

// The running blade (self): every ragdoll with a part in it struck, each part (a corpse's limbs and head before its
// torso, which may gib it; a living one's once: it dies of it).
void() VR_Buzzsaw_Ragdolls =
{
    vector lo = self.absmin - VR_BUZZ_GROW * '1 1 1';
    vector hi = self.absmax + VR_BUZZ_GROW * '1 1 1';
    for(entity e = ragdollinbox(lo, hi, world); e != world; e = ragdollinbox(lo, hi, e))
    {
        if(e.vr_buzz_next > time || !e.takedamage)
        {
            continue;
        }
        e.vr_buzz_next = time + VR_BUZZ_GAP;
        float alive = e.health > 0;
        float struck = FALSE;
        float torso = FALSE;
        vector torsoAt = '0 0 0';
        for(float part = ragdollboxpart(e, lo, hi, -1); part >= 0 && isragdoll(e); part = ragdollboxpart(e, lo, hi, part))
        {
            vector at = trace_endpos;
            if(VR_Limb_RagdollPiece(e, at) < 0)
            {
                if(!torso)
                {
                    torso = TRUE;
                    torsoAt = at;
                }
                continue;
            }
            VR_Buzzsaw_Hit(e, at);
            struck = TRUE;
            if(alive)
            {
                break;
            }
        }
        if(torso && !(alive && struck) && isragdoll(e) && e.takedamage)
        {
            VR_Buzzsaw_Hit(e, torsoAt);
        }
    }
};

// Each frame (StartFrame): every running buzzsaw cuts the ragdolls in its blade.
void() VR_Buzzsaw_Frame =
{
    if(!vr_buzzsaws)
    {
        return;
    }
    entity oself = self;
    for(entity saw = find(world, classname, "buzzsaw"); saw; saw = find(saw, classname, "buzzsaw"))
    {
        if(saw.touch == buzzsaw_touch)
        {
            self = saw;
            VR_Buzzsaw_Ragdolls();
        }
    }
    self = oself;
};

void() buzzsaw_use =
{
    self.touch = buzzsaw_touch;
    if(self.target != NULLSTR)
    {
        self.movetarget = find(world, targetname, self.target);
        self.goalentity = self.movetarget;
        self.th_stand =  buzzsaw_fly1;
        self.th_walk = buzzsaw_fly1;
        self.th_run = buzzsaw_fly1;
        self.think = buzzsaw_fly1;
        self.nextthink = time + 0.1;
    }
    else
    {
        self.nextthink = time + 0.1;
        self.think = buzzsaw_stand1;
    }
    self.use = SUB_Null;
};

/*QUAKED buzzsaw (0 .5 .8) (-18 -18 -18) (18 18 18) Vertical
The buzzsaw trap.

currentammo: amount of damage for each contact. (default 10)
speed: speed that it will follow it's path. (default 10)

Use the angle buttons to point the buzzsaw in the direction it
should face.

Place on a monster path if you want it to move.

If it is targeted, it will wait until triggered to activate.

It will not damage players until activated.
*/
void() buzzsaw =
{
    if(!VR_Pack_RequireSpawn(VR_Has4("progs/buzzsaw.mdl", "sound/buzz/buzz.wav", "sound/buzz/buzz1.wav", ""), "Rogue")) { return; }

    precache_model("progs/buzzsaw.mdl");

    precache_sound("buzz/buzz.wav");
    precache_sound("buzz/buzz1.wav");

    setmodel(self, "progs/buzzsaw.mdl");

    self.takedamage = DAMAGE_NO;
    self.solid = SOLID_TRIGGER;
    self.movetype = MOVETYPE_FLY;

    // QVR: made by the toolgun (vr_spawn_live: vr_walltorch.qc), turned any way: its blade along the nearest axis.
    if(vr_spawn_live)
    {
        self.angles_y = anglemod(rint(self.angles_y / 90) * 90);
        if(self.angles_y >= 359) { self.angles_y = 0; }
    }

    if(self.angles_y == 0 || self.angles_y == 180)
        setsize(self, '-18 0 -18', '18 0 18');
    else if(self.angles_y == 90 || self.angles_y == 270)
        setsize(self, '0 -18 -18', '0 18 18');
    else
        objerror ("Buzzsaw: Not at 90 degree angle!");

    setorigin(self, self.origin);

    if(!self.speed)
        self.speed = 10;
    if(!self.currentammo)
        self.currentammo = 10;

    self.pain_finished = time + random()*2;
    vr_buzzsaws = vr_buzzsaws + 1; // QVR: StartFrame looks for ragdolls in its blade (VR_Buzzsaw_Frame)

    if(!self.targetname)
    {
        self.think = buzzsaw_use;
        self.nextthink = time + 0.2;
    }
    else
    {
        self.use = buzzsaw_use;
    }
};

// QVR, for tests (vr_knockdown_test 70; Debug > Combat, "Buzzsaw on the Nearest Body"; self the player): a running
// buzzsaw made at the middle of the nearest ragdoll (a corpse's or a knocked-down monster's), its blade across your view.
void() VR_Buzzsaw_Test =
{
    entity best = world;
    float bestDist = 999999;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(isragdoll(e) && vlen(e.origin - self.origin) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - self.origin);
        }
    }
    if(best == world)
    {
        dprint("buzzsaw test: no ragdoll\n");
        return;
    }
    entity saw = spawn();
    saw.classname = "buzzsaw";
    saw.angles = '0 0 0';
    saw.angles_y = self.angles_y;
    setorigin(saw, best.origin);
    entity oself = self;
    float oldLive = vr_spawn_live;
    vr_spawn_live = TRUE; // (its yaw snapped to the nearest axis)
    self = saw;
    buzzsaw();
    buzzsaw_use(); // (running at once: a body falling through it now is cut)
    self = oself;
    vr_spawn_live = oldLive;
    dprint(sprintf("buzzsaw test: a buzzsaw at %s's middle %v (%s, health %g)\n", best.classname, best.origin,
        best.health > 0 ? "alive" : "a corpse", best.health));
};

