void() light_fade_in =
{
         if(self.cnt == 0)   lightstyle(self.style, "b");
    else if(self.cnt == 1)   lightstyle(self.style, "c");
    else if(self.cnt == 2)   lightstyle(self.style, "d");
    else if(self.cnt == 3)   lightstyle(self.style, "e");
    else if(self.cnt == 4)   lightstyle(self.style, "f");
    else if(self.cnt == 5)   lightstyle(self.style, "g");
    else if(self.cnt == 6)   lightstyle(self.style, "h");
    else if(self.cnt == 7)   lightstyle(self.style, "i");
    else if(self.cnt == 8)   lightstyle(self.style, "j");
    else if(self.cnt == 9)   lightstyle(self.style, "k");
    else if(self.cnt == 10)  lightstyle(self.style, "l");
    else if(self.cnt == 11){ lightstyle(self.style, "m");
                             return;
         }
    self.cnt = self.cnt + 1;
    self.nextthink = time + self.waitmin;
}

void() light_fade_out=
{
         if(self.cnt == 11)  lightstyle(self.style, "l");
    else if(self.cnt == 10)  lightstyle(self.style, "k");
    else if(self.cnt == 9)   lightstyle(self.style, "j");
    else if(self.cnt == 8)   lightstyle(self.style, "i");
    else if(self.cnt == 7)   lightstyle(self.style, "h");
    else if(self.cnt == 6)   lightstyle(self.style, "g");
    else if(self.cnt == 5)   lightstyle(self.style, "f");
    else if(self.cnt == 4)   lightstyle(self.style, "e");
    else if(self.cnt == 3)   lightstyle(self.style, "d");
    else if(self.cnt == 2)   lightstyle(self.style, "c");
    else if(self.cnt == 1)   lightstyle(self.style, "b");
    else if(self.cnt == 0) { lightstyle(self.style, "a");
                             return;
         }
    self.cnt = self.cnt - 1;
    self.nextthink = time + self.waitmin;
}

void() model_candle_think =
{
    self.frame = self.frame + 1;
    if(self.frame > 3)
        self.frame = 0;
    self.nextthink = time + 0.1;
};

void() model_candle =
{

    precache_model("progs/candle.mdl");

    setmodel(self, "progs/candle.mdl");

    //self.think = model_candle_think;
    //self.nextthink = time + 0.1;
};

void() air_bubbles_use =
{
    self.state = 1 - self.state;

    if(self.state){ //switch them on
        make_bubbles();
    }else{ //switch them off
        self.nextthink = -1;
    }
}

void() wow_whoa_plopp =
{
    precache_sound("honey/blaahd.wav");
    ambientsound(self.origin, "honey/blaahd.wav", 1, ATTN_STATIC);
    SUB_Remove();
}

// Conflict with hipnotic
/*
void() ambient_waterfall =
{
    precache_sound("honey/q2watr.wav");
    ambientsound(self.origin, "honey/q2watr.wav", 1, ATTN_IDLE);
    SUB_Remove();
}
*/

void() ambient_use =
{
    sound(self, CHAN_VOICE, self.noise, self.volume, self.delay);
    self.use = SUB_Null;
}

void() ambient_machine =
{
    self.noise = "honey/machine.wav";
    if(!self.volume) self.volume = 0.7;
    if(!self.delay) self.delay = ATTN_NORM;
    precache_sound(self.noise);
    self.use = ambient_use;
}

void() ambient_teleport =
{
    precache_sound("ambience/hum1.wav");
    ambientsound(self.origin, "ambience/hum1.wav",0.5 , ATTN_STATIC);
    SUB_Remove();
}


void() ambient_secret_think =
{
    float rnd, rnd2;

    rnd = random();
    rnd2 = 0.5 + 0.5 * random();

    if(VR_GameUtil_EnableHoneyExtensions())
    {
        if(rnd < 0.25)
        {
            sound(self, CHAN_AUTO, "honey/ting1.wav",rnd2 , 1.2);
        }
        else if(rnd < 0.5)
        {
            sound(self, CHAN_AUTO, "honey/ting2.wav",rnd2 , 1.2);
        }
        else if(rnd < 0.75)
        {
            sound(self, CHAN_AUTO, "honey/ting3.wav",rnd2 , 1.2);
        }
        else
        {
            sound(self, CHAN_AUTO, "honey/ting4.wav",rnd2 , 1.2);
        }
    }

    rnd = 0.2 + 3 * random();
    self.think = ambient_secret_think;
    self.nextthink = time + rnd;

}

entity(vector org) ambient_secret_spawn =
{
    entity ambi, oself;
    ambi = spawn();
    setorigin(ambi, org);
    oself= self;
    self = ambi;
    ambient_secret_think();
    self = oself;
    return ambi;
}

