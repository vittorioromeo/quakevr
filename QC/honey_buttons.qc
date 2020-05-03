float START_DISABLED = 2;

void(entity but) button_disable =
{
    but.state = STATE_DISABLED;
    but.nextthink = -1;
    but.think = SUB_Null;
    but.frame = 1;			// use alternate textures
    but.velocity = '0 0 0';
    but.takedamage = DAMAGE_NO;
    setorigin (but, but.pos2);
};

void(entity but) button_enable =
{
    but.state = STATE_BOTTOM;
    but.nextthink = -1;
    but.think = SUB_Null;
    but.frame = 0;			// use normal textures
    but.velocity = '0 0 0';
    if(but.max_health){
        but.takedamage = DAMAGE_YES;
        but.health = but.max_health;
    }
    setorigin (but, but.pos1);
};

void() trigger_buttonenabler_enable =
{
    local entity butt;

    if(self.spawnflags & START_DISABLED){
        self.spawnflags = self.spawnflags - START_DISABLED;

        butt = find(world, targetname, self.target);
        while(butt){
            button_enable(butt);
            butt = find(butt, targetname, self.target);
        }
    }
}

void() trigger_buttonenabler_disable =
{
    local entity butt;

    if((self.spawnflags & START_DISABLED) == 0){
        self.spawnflags = self.spawnflags | START_DISABLED;

        butt = find(world, targetname, self.target);
        while(butt){
            button_disable(butt);
            butt = find(butt, targetname, self.target);
        }
    }

}

void() trigger_buttonenabler_toggle =
{
    if(self.spawnflags & START_DISABLED){
        trigger_buttonenabler_enable();
    }else{
        trigger_buttonenabler_disable();
    }
}

void() trigger_buttontoggle =
{
    self.use = trigger_buttonenabler_toggle;
}
