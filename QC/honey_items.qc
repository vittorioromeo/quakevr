float ITEM_SECRET = 128;
float ITEM_FLOATING = 16;

void() itemTouch =
{
    if(other.classname != "player")
        return;

    if(other.health <= 0)
        return;

    //self.touch = self.th_die;
    self.th_die();

    if(self.spawnflags & ITEM_SECRET){
        found_secrets = found_secrets + 1;
        WriteByte (MSG_ALL, SVC_FOUNDSECRET);
        self.spawnflags = self.spawnflags - (self.spawnflags & ITEM_SECRET);

        sound(self, CHAN_ITEM, "misc/secret.wav", 1, ATTN_NORM);
        centerprint(other, self.message);
        self.message = string_null;
        if(self.classname == "item_health" && self.spawnflags & H_MEGA) // Do nothing
            SUB_Null();
        else
            self.think = SUB_Null;
    }

    if(self.wait) {
        self.nextthink = time + self.wait;
        self.think = SUB_regen;
    }

    activator = other;
    SUB_UseTargets();                // fire all targets / killtargets
};


void() item_spawn =
{
    self.use = SUB_Null;
    self.model = self.mdl;
    setmodel(self, self.model);

    self.solid = SOLID_TRIGGER;
    self.handtouch = self.th_pain;

    force_retouch = 2;

    if(self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN_TFOG){
        self.spawnflags = self.spawnflags - SPAWNFLAG_TRIGGER_SPAWN_TFOG;
        spawn_tfog(self.origin);
    }

}

void() item_unspawn =
{
    self.mdl = self.model;        // so it can be restored on respawn

    self.th_pain = self.handtouch;

    self.solid = SOLID_NOT;
    self.model = string_null;

    self.use = item_spawn;
}

