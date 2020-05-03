void(vector tdest, float traveltime, void() func) SUB_CalcMove_time =
{
local vector    vdestdelta;
local float        len;

    if (!traveltime)
        objerror("No time is defined!");

    self.think1 = func;
    self.finaldest = tdest;
    self.think = SUB_CalcMoveDone;

    if (tdest == self.origin)
    {
        self.velocity = '0 0 0';
        self.nextthink = self.ltime + traveltime;
        return;
    }

// set destdelta to the vector needed to move
    vdestdelta = tdest - self.origin;

    // set nextthink to trigger a think when dest is reached
    self.nextthink = self.ltime + traveltime;

// scale the destdelta vector by the time spent traveling to get velocity
    self.velocity = vdestdelta * (1/traveltime);    // qcc won't take vec/float
}


void(entity e) SUB_Activate =
{
    entity tmp;
    tmp = self;
    self = e;
    SUB_UseTargets();
    self = tmp;
}


void(string t) SUB_Trigger=
{
    local float oldD;
    activator = self;
    self.target = t;
    oldD = self.delay;
    self.delay = 0;
    SUB_UseTargets();
    self.delay = oldD;
}

void(string t, string kt) SUB_KillTrigger=
{
    local float oldD;
    activator = self;
    self.target = t;
    self.killtarget = kt;
    oldD = self.delay;
    self.delay = 0;
    SUB_UseTargets();
    self.delay = oldD;
}

void(string t, float d) SUB_DelayTrigger=
{
    local float oldD;
    activator = self;
    self.target = t;
    oldD = self.delay;
    self.delay = d;
    SUB_UseTargets();
    self.delay = oldD;
}

void(string t, string kt, float d) SUB_DelayKillTrigger=
{
    local float oldD;
    activator = self;
    self.target = t;
    self.killtarget = kt;
    oldD = self.delay;
    self.delay = d;
    SUB_UseTargets();
    self.delay = oldD;
}

void() DelayCenterprint_think=
{
    centerprint (self.enemy, self.message);
    sound (self.enemy, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NORM);
}

void(entity to, string msg, float dly) SUB_DelayCenterprint=
{
    local entity t;
    if(dly){
        t = spawn();
        t.message = msg;
        t.enemy = to;
        t.think = DelayCenterprint_think;
        t.nextthink = time + dly;
        return;
    }
    centerprint (to, msg);
    sound (to, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NORM);
}

void(void() t, float d) SUB_DelayCall =
{
    self.think = t;
    self.nextthink = self.ltime + d;
}
