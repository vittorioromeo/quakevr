// timemach.qc
// pmack
// jan97

// ------------------------------------------------
// time machine and related parts
// ------------------------------------------------
void() time_chunk =
{
    local entity    timeChunk;

    timeChunk = spawn();
    setmodel(timeChunk, "progs/timegib.mdl");
    timeChunk.solid = SOLID_NOT;
    timeChunk.movetype = MOVETYPE_TOSS;
    makevectors (self.owner.angles);
    setorigin(timeChunk, self.owner.origin + v_forward*84 - v_up * 136);
    timeChunk.velocity = v_up * -50;
    timeChunk.avelocity = '300 300 300';
    timeChunk.angles = self.owner.angles;

    sound(self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NONE);
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, timeChunk.origin_x);
    WriteCoord (MSG_BROADCAST, timeChunk.origin_y);
    WriteCoord (MSG_BROADCAST, timeChunk.origin_z);

    self.owner.frame = 1;

    timeChunk.think = SUB_Remove;
    timeChunk.nextthink = time + 5;
};

void() time_stop_shake =
{
    SUB_UseTargets();
    remove(self);
};

void() time_boom =
{
    local entity    stopShakeEnt;

    SUB_UseTargets();

    if(self.owner.health < 1250 && self.owner.frame > 0)
    {
        if(self.owner.skin < 2)
        {
            self.owner.frame = 2;
            self.owner.skin = 2;
        }
    }
    else if(self.owner.health < 1500)
    {
        if(self.owner.frame == 0)
        {
            time_chunk();
            self.owner.frame = 1;
            self.owner.skin = 1;
        }
    }

    sound(self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NONE);

    if(random() < 0.5)
    {
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_EXPLOSION);
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
    }
    else
    {
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
        WriteByte (MSG_BROADCAST, 244);
        WriteByte (MSG_BROADCAST, 3);
    }

    BecomeExplosion();
    stopShakeEnt = spawn();
    stopShakeEnt.target = self.target;
    stopShakeEnt.think = time_stop_shake;
    stopShakeEnt.nextthink = time + 0.7;
};

void() time_fall =
{
    if(!self.pain_finished)
    {
        if(self.origin_z < -20)
        {
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, TE_LAVASPLASH);
            WriteCoord (MSG_BROADCAST, self.origin_x);
            WriteCoord (MSG_BROADCAST, self.origin_y);
            WriteCoord (MSG_BROADCAST, self.origin_z - 80);
            self.pain_finished = 1;
        }
        else if(random() < 0.3)
        {
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, TE_EXPLOSION);
            WriteCoord (MSG_BROADCAST, self.origin_x);
            WriteCoord (MSG_BROADCAST, self.origin_y);
            WriteCoord (MSG_BROADCAST, self.origin_z);
        }
    }

    self.velocity_z = self.velocity_z - 5;
    self.nextthink = time + 0.1;
    self.think = time_fall;
};

void() time_crash_impl =
{
    self.takedamage = DAMAGE_NO;
    self.movetype = MOVETYPE_FLY;
    self.solid = SOLID_NOT;
    self.velocity = '0 0 -50';
    self.avelocity = '15 0 5';
    setsize(self, '0 0 0', '0 0 0');
    self.think = time_fall;
    self.nextthink = time + 0.1;

    self.target = "timeramp";
    SUB_UseTargets();
};

void() time_crash_think =
{
    time_crash_impl();
}

void() time_crash_die =
{
    time_crash_impl();
}

void(entity attacker, float damage) time_crash_pain =
{
    time_crash_impl();
}

void() time_pain_impl =
{
    local entity    painEnt;
    local vector    painOrg;
    local float     painRnd;

    if(self.health > 1100)
        if(self.pain_finished > time)
            return;

    painRnd = random();
    if(painRnd < 0.4)
    {
        self.pain_finished = time + 2;
        painRnd = random();
        makevectors (self.angles);
        painEnt = spawn();
        if(painRnd < 0.33)
            painEnt.origin = self.origin + v_forward*80 - v_up*64;
        else if(painRnd < 0.66)
            painEnt.origin = self.origin + v_right*80 - v_up*24;
        else
            painEnt.origin = self.origin + v_forward*64 - v_up*48 - v_right*48;
        painEnt.target = self.target;
        painEnt.owner = self;
        painEnt.think = time_boom;
        painEnt.nextthink = time + 0.2 + random() * 0.3;
    }
    if(self.health < 1000)
    {
        self.pain_finished = 0;
        self.th_pain = SUB_NullPain;
        self.th_die = SUB_Null;
        cutscene_running = 1;
        return;
    }
}

void(entity attacker, float take) time_pain =
{
    time_pain_impl();
};

void() time_pain_die =
{
    time_pain_impl();
};

//(-16 -16 -32) (16 16 0)
/*QUAKED item_time_machine (1 1 0) (-64 -64 -144) (64 64 0)
*/
void() item_time_machine =
{
    if(deathmatch)
    {
        remove(self);
        return;
    }

    precache_model("progs/timemach.mdl");
    precache_model("progs/timegib.mdl");
    setmodel(self, "progs/timemach.mdl");
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    self.health = 1600;
    self.takedamage = DAMAGE_YES;
    self.view_ofs = '0 0 25';
    self.movetype = MOVETYPE_FLY;
    setsize(self, '-64 -64 -144', '64 64 0');
    self.avelocity = '0 60 0';
    self.flags = self.flags | FL_MONSTER;

    self.th_stand = SUB_Null;
    self.th_walk = SUB_Null;
    self.th_run = SUB_Null;
    self.th_die = time_pain_die;
    self.th_missile = SUB_Null;
    self.th_pain = time_pain;

    theMachine = self;
};

/*QUAKED item_time_core (1 0 0) (-8 -8 -8) (8 8 8)
*/
void() item_time_core =
{
    if(deathmatch)
    {
        remove(self);
        return;
    }

    precache_model ("progs/timecore.mdl");
    setmodel(self, "progs/timecore.mdl");
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_FLY;
    self.avelocity = '60 60 60';
};
