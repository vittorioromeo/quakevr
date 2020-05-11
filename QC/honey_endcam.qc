
/*
    All of this is horrible shit that noone should use.

*/


/***************************************************************************************************/

entity endcam;
void() execute_endcam;

void() info_endcam =
{
    endcam = self;
    if(!self.wait) self.wait = 180;
    self.think = execute_endcam;
    self.nextthink = 2;
    //self.view_ofs = '0 0 1';
}

void() clear_idlescale =
{
    entity p;
    endcam.pos1_x = cvar_hget(cvarh_v_iyaw_level);
    endcam.pos1_y = cvar_hget(cvarh_v_iroll_level);
    endcam.pos1_z = cvar_hget(cvarh_v_ipitch_level);

    p = find(world, classname, "player");
    while (p != world)
    {
        stuffcmd(p, "v_iyaw_level 0\nv_iroll_level 0\nv_ipitch_level 0\n");
        p = find(p, classname, "player");
    }
}

void() reset_idlescale =
{
    entity p;

    p = find(world, classname, "player");
    while (p != world)
    {
        stuffcmd(p, "v_iyaw_level ");
        stuffcmd_float(p, endcam.pos1_x);
        stuffcmd(p, "\nv_iroll_level ");
        stuffcmd_float(p, endcam.pos1_y);
        stuffcmd(p, "\nv_ipitch_level ");
        stuffcmd_float(p, endcam.pos1_z);
        stuffcmd(p, "\n");
        p = find(p, classname, "player");
    }
}

void() endcam_done =
{
    reset_idlescale();

    if(!nextmap)
        if(endcam.map) nextmap = endcam.map;
        else nextmap = "start";

    GotoNextMap();
}


void() endcam_cutscene =
{
    other = find(world, classname, "player");
    while (other != world)
    {
        other.view_ofs = '0 0 0';
        other.angles = other.v_angle = endcam.mangle;
        other.fixangle = TRUE;        // turn this way immediately
        other.nextthink = time + 0.5;
        other.takedamage = DAMAGE_NO;
        other.solid = SOLID_NOT;
        other.movetype = MOVETYPE_NONE;
        other.weaponmodel = "";
        other.modelindex = 0;
        setorigin(other, endcam.origin);

        other = find(other, classname, "player");
    }

    WriteByte(MSG_ALL, SVC_CUTSCENE);
    WriteString (MSG_ALL, "");

}


void() execute_endcam =
{
    clear_idlescale();

    WriteByte(MSG_ALL, SVC_CDTRACK);
    WriteByte(MSG_ALL, 0);
    WriteByte(MSG_ALL, 0);

    other = find(world, classname, "player");
    while (other != world)
    {
        other.movetype = MOVETYPE_NOCLIP;

        other = find(other, classname, "player");
    }


    SUB_DoLater(endcam_cutscene, 0);


    //WTF does this do?
    //Oh ok seems to be a roundabout way of doing SUB_UseTargets
    SUB_Activate(endcam);

    endcam.think = endcam_done;
    //FIXME: should probably never exit...
    endcam.nextthink = time + endcam.wait;
};

/***************************************************************************************************/


void() endcam_muzak_start =
{
    sound(self, CHAN_VOICE, "honey/muzak4.wav", 1, ATTN_NONE);
}

/*QUAKED endcam_muzak (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void() endcam_muzak =
{
    precache_sound("honey/muzak4.wav");

    if(self.targetname){
        self.use = endcam_muzak_start;
    }else{
        endcam_muzak_start();
    }

};

/***************************************************************************************************/


void() func_slide_done;

void() func_slide_think =
{
    vector tdest;
    tdest = self.origin + self.movedir;
    SUB_CalcMove(tdest, self.speed, func_slide_done);
}

void() func_slide_done =
{
    //eprint(self);
    if(self.cnt){
        self.cnt = self.cnt - 1;
        self.think = func_slide_think;
        self.nextthink = self.ltime + self.wait;
        if(self.cnt == 3)
            self.nextthink = self.ltime + 0.2; //lol
        else if(self.cnt == 1)
            self.nextthink = self.ltime + 4.20; //heh :)
    }else{
        SUB_UseTargets();
        SUB_Remove();
    }
}

/*QUAKED func_slide (0.3 0.1 0.6) (? ? ?) (? ? ?)
*/
void() func_slide =
{
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_PUSH;
    setorigin(self, self.origin);
    setmodel(self, self.model);


    if(!self.movedir) self.movedir= '128 0 0';
    if(!self.speed)   self.speed = 512;

    if(!self.wait)    self.wait = 3;
    if(!self.cnt)     self.cnt = 20;

    self.use = func_slide_think;
}


/***************************************************************************************************/


void() func_slide_repeating_done;
void() func_slide_repeating_think;

void() func_slide_repeating_think_first =
{
    self.pos1 = self.origin;
    self.pos2 = self.pos1 + self.movedir;

    self.enemy.pos1 = self.pos1;
    self.enemy.pos2 = self.pos2;

    if(self.delay){
        self.think = func_slide_repeating_think;
        self.nextthink = self.ltime + self.delay;
    }else{
        func_slide_repeating_think();
    }

}

void() func_slide_repeating_think =
{
    setorigin(self, self.pos1);
    SUB_CalcMove(self.pos2, 128, func_slide_repeating_done);
}

void() func_slide_repeating_done =
{
    entity oself;
    oself = self;

    if(self.enemy){
        self = self.enemy;

    }else if(self.owner){
        self = self.owner;

    }

    func_slide_repeating_think();

    self = oself;
}

/*QUAKED func_slide_repeating (0.3 0.1 0.6) (? ? ?) (? ? ?)
*/
void() func_slide_repeating =
{
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_PUSH;

    vector org;
    org = self.origin + self.pos1;

    setorigin(self, org);
    setmodel(self, self.model);


    if(!self.movedir) self.movedir= '128 0 128';


    self.use = func_slide_repeating_think_first;

    //Make a duplicate
    self.enemy = spawn();
    self.enemy.owner = self;

    setorigin(self.enemy, org);
    setmodel(self.enemy, self.model);
    self.enemy.movedir = self.movedir;
    self.enemy.speed = self.speed;


}
