/*
====================================================
BDW_DelayedRemove

This is a hack that prevents unwanted sprite frame
interpolation in DarkPlaces. It appears that a newly
spawned sprite will try to interpolate from the last
frame of the previous edict (whose slot it now fills).
====================================================
*/

void() BDW_SpriteGenericLoop;
void() BDW_SpriteGenericPlayOnce;

void() BDW_DelayedRemove = {
    self.model = "";
    self.modelindex = 0;
    self.frame = 0;    
    self.skin = 0;
    self.colormap = 0;
    self.effects = 0;
    self.alpha = 0.0001;
    self.solid = SOLID_NOT;
    self.takedamage = DAMAGE_NO;
    self.movetype = MOVETYPE_NONE;
    self.velocity = VEC_ORIGIN;
    self.angles = VEC_ORIGIN;
    self.origin = VEC_ORIGIN;
    self.think = SUB_Remove;
    self.nextthink = time + 0.1;
};

/*
======================================================
BDW_SpriteGenericLoop

Loops a sprite animation

self.idleweaponframe represents the total number 
of frames in the animation. Frames are numbered 
starting from 0, so the last frame in the animation 
is actually (self.idleweaponframe - 1). Useful for
missiles.

BDW_SpriteGenericPlayOnce

Similar to the above, but removes sprite at end of
animation. Useful for explosions.
======================================================
*/

void() BDW_SpriteGenericLoop = {
    self.frame = self.frame + 1;
    if (self.frame > (self.idleweaponframe - 1))
        self.frame = 0;

    self.think = BDW_SpriteGenericLoop;
    self.nextthink = time + 0.05;
};

void() BDW_SpriteGenericPlayOnce = {
    self.frame = self.frame + 1;
    if (self.frame > (self.idleweaponframe - 1))
    {
        BDW_DelayedRemove();    // hack!
        return;
    }
        
    self.think = BDW_SpriteGenericPlayOnce;
    self.nextthink = time + 0.05;
};

void() BDW_BodyDropSound = { sound(self, CHAN_BODY, "demon/dland2.wav", 0.7, ATTN_NORM); };