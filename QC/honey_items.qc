float HONEY_SPAWNFLAG_ITEM_SECRET = 128;
float HONEY_SPAWNFLAG_ITEM_FLOATING = 16;

void() itemTouch =
{
    // Machine sigils own their target dispatch and pickup lock; never fire twice or through a locked egg.
    if(MG1_Campaign() && self.classname == "item_sigil") { MG1_sigil_touch(); return; }
    if(other.classname != "player")
        return;

    if(other.health <= 0)
        return;

    //self.touch = self.th_die;
    self.th_die();

    if(VR_GameUtil_EnableHoneyExtensions())
    {
        if(self.spawnflags & HONEY_SPAWNFLAG_ITEM_SECRET)
        {
            found_secrets = found_secrets + 1;
            WriteByte(MSG_ALL, SVC_FOUNDSECRET);
            self.spawnflags = self.spawnflags - (self.spawnflags & HONEY_SPAWNFLAG_ITEM_SECRET);

            sound(self, CHAN_ITEM, "misc/secret.wav", 1, ATTN_NORM);
            centerprint(other, self.message);
            self.message = string_null;
            if(self.classname == "item_health" && self.spawnflags & H_MEGA) // Do nothing
                SUB_Null();
            else
                self.think = SUB_Null;
        }
    }

    if(self.wait && self.solid == SOLID_NOT) {
        self.nextthink2 = time + self.wait;
        self.think2 = SUB_regen;
    }

    activator = other;
    SUB_UseTargets();                // fire all targets / killtargets
};


// VR: what an item a trigger brings is when it comes (item_unspawn keeps it): a pickup's SOLID_TRIGGER, or an ammo or
// health box object's SOLID_NOT_BUT_TOUCHABLE (MakeItemGrabbable). The solid is set before the link, and the box kept:
// a solid set after setmodel's link left the item in the server's solid list as a trigger ("Trigger in clipping
// list", a crash: The Matriarch of Decay's palace button, ROUND22.md).
.float vr_unspawned_solid;

void() item_spawn =
{
    self.use = SUB_Null;
    self.model = self.mdl;
    self.solid = self.vr_unspawned_solid ? self.vr_unspawned_solid : SOLID_TRIGGER;
    vector lo = self.mins, hi = self.maxs;
    setmodel(self, self.model);
    setsize(self, lo, hi); // (setmodel takes a brush model's own box: a box object's is its shrunk one)
    setorigin(self, self.origin);

    self.handtouch = fromPainToTouchFn(self.th_pain);

    force_retouch = 2;

    if(self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN_TFOG){
        self.spawnflags = self.spawnflags - SPAWNFLAG_TRIGGER_SPAWN_TFOG;
        spawn_tfog(self.origin);
    }

}

void() item_unspawn =
{
    self.mdl = self.model;        // so it can be restored on respawn

    self.th_pain = asPainFn(self.handtouch);

    self.vr_unspawned_solid = self.solid;
    self.solid = SOLID_NOT;
    self.model = string_null;
    self.modelindex = 0; // VR: no physics body (Box3D) nor anything else for an item not there yet
    setorigin(self, self.origin); // out of the server's lists until it comes

    self.use = item_spawn;
}

