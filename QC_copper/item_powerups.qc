/*
===============================================================================

POWERUPS

===============================================================================
*/

/*
===============
powerup_envirosuit
===============
*/
void(entity h, float t) powerup_envirosuit =
{
	sprint (h, "It's stuffy in this thing ...\n");
	h.items |= IT_SUIT;
	h.rad_time = 1;
	h.radsuit_finished = time + t;
}

/*
===============
powerup_invulnerability
===============
*/
void(entity h, float t) powerup_invulnerability =
{
	sprint (h, "An unholy ward surrounds you ...\n");
	h.items |= IT_INVULNERABILITY;
	h.invincible_time = 1;
	h.invincible_finished = time + t;
}

/*
===============
powerup_invisibility
===============
*/
void(entity h, float t) powerup_invisibility =
{
	sprint (h, "Others' eyes deceive them, but yours give you away ...\n");
	h.items |= IT_INVISIBILITY;
	h.invisible_time = 1;
	h.invisible_finished = time + t;
}

/*
===============
powerup_super_damage
===============
*/
void(entity h, float t) powerup_super_damage =
{
	sprint (h, "You begin to channel unstoppable power ...\n");
	h.items |= IT_QUAD;
	h.super_damage_finished = time + t;
	h.super_time = 1;
}

/*
===============
powerup_touch
===============
*/
void() powerup_touch =
{
	if ( !CheckValidTouch() ) return;

	sound (other, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	stuffcmd (other, "bf\n");
	other.items = other.items | self.items;

	if (self.classname == "item_artifact_envirosuit")
		powerup_envirosuit(other, 30);
	
	if (self.classname == "item_artifact_invulnerability")
		powerup_invulnerability(other, 30);
	
	if (self.classname == "item_artifact_invisibility")
		powerup_invisibility(other, 30);

	if (self.classname == "item_artifact_super_damage")
		powerup_super_damage(other, 30);

	ItemTouched();
}


/*
===============================================================================

PENTAGRAM

===============================================================================
*/
void() precache_invulnerability =
{
	precache_model ("progs/invul.mdl");
	precache_sound ("items/protect.wav");
	precache_sound ("items/protect2.wav");
	precache_sound ("items/protect3.wav");
}
/*QUAKED item_artifact_invulnerability (0 .5 .5) (-16 -16 -24) (16 16 32) ? ? SUSPENDED
Player is invulnerable for 30 seconds.

Keys:
"target/2/3/4/k" - entity to trigger when picked up
"targetname" - will not spawn until triggered
"wait" - will respawn after 'wait' seconds. fires targets every time.
"count" - limit number of times to respawn
*/
/*FGD
@PointClass size(-16 -16 -24, 16 16 32) base(Item) model({ "path": ":progs/invulner.mdl" }) =
	item_artifact_invulnerability : "Pentagram of Protection" []
*/
void() item_artifact_invulnerability =
{
	if (!SUB_ShouldSpawn()) return;
	self.touch = powerup_touch;
	precache_invulnerability();
	
	self.noise = "items/protect.wav";
	setmodel (self, "progs/invul.mdl");
	self.netname = "Pentagram of Protection";
	self.items = IT_INVULNERABILITY;
	setsize (self, '-16 -16 -24', '16 16 32');
	if (deathmatch)
		self.wait = 300;
	StartItem ();
}


/*
===============================================================================

BIOSUIT

===============================================================================
*/
void() precache_envirosuit =
{
	precache_model ("progs/suit.mdl");
	precache_sound ("items/suit.wav");
	precache_sound ("items/suit2.wav");
}
/*QUAKED item_artifact_envirosuit (0 .5 .5) (-16 -16 -8) (16 16 48) ? ? SUSPENDED
Player takes no damage from water or slime for 30 seconds.

Keys:
"target/2/3/4/k" - entity to trigger when picked up
"targetname" - will not spawn until triggered
"wait" - will respawn after 'wait' seconds. fires targets every time.
"count" - limit number of times to respawn
*/
/*FGD
@PointClass size(-16 -16 -24, 16 16 32) base(Item) model({ "path": ":progs/suit.mdl" }) =
	item_artifact_envirosuit : "Environmental protection suit" []
*/
void() item_artifact_envirosuit =
{
	if (!SUB_ShouldSpawn()) return;
	self.touch = powerup_touch;

	precache_envirosuit();
	self.noise = "items/suit.wav";
	setmodel (self, "progs/suit.mdl");
	self.netname = "Biosuit";
	self.items = IT_SUIT;
	setsize (self, '-16 -16 -8', '16 16 48');
	if (deathmatch)
		self.wait = 60;
	StartItem ();
}


/*
===============================================================================

RING OF SHADOWS

===============================================================================
*/
void() precache_invisibility =
{
	precache_model ("progs/invis.mdl");
	precache_sound ("items/inv1.wav");
	precache_sound ("items/inv2.wav");
	precache_sound ("items/inv3.wav");
}

/*QUAKED item_artifact_invisibility (0 .5 .5) (-16 -16 -24) (16 16 32) ? ? SUSPENDED
Player is invisible for 30 seconds. Monsters will not awaken on sight, but they will on hearing sound (like gunfire). Awakened monsters will try to attack an invisible player and do an inaccurate job, possibly starting infights. 
monster_dog can still smell an invisible player, but monster_tarbaby can't find the player at all.

Keys:
"target/2/3/4/k" - entity to trigger when picked up
"targetname" - will not spawn until triggered
"wait" - will respawn after 'wait' seconds. fires targets every time.
"count" - limit number of times to respawn
*/
/*FGD
@PointClass size(-16 -16 -24, 16 16 32) base(Item) model({ "path": ":progs/invisibl.mdl" }) = 
	item_artifact_invisibility : "Ring of Shadows" []
*/
void() item_artifact_invisibility =
{
	if (!SUB_ShouldSpawn()) return;
	self.touch = powerup_touch;
	precache_invisibility();
	self.noise = "items/inv1.wav";
	setmodel (self, "progs/invis.mdl");
	self.netname = "Ring of Shadows";
	self.items = IT_INVISIBILITY;
	setsize (self, '-16 -16 -24', '16 16 32');
	if (deathmatch)
		self.wait = 300;
	StartItem ();
}


/*
===============================================================================

QUAD

===============================================================================
*/
void() precache_super_damage =
{
	precache_model ("progs/quaddama.mdl");
	precache_sound ("items/damage.wav");
	precache_sound ("items/damage2.wav");
	precache_sound ("items/damage3.wav");
}

/*QUAKED item_artifact_super_damage (0 .5 .5) (-16 -16 -24) (16 16 32) ? ? SUSPENDED
Quad Damage. Player's attacks do 4x damage and he stops paying attention to anything else.

Keys:
"target/2/3/4/k" - entity to trigger when picked up
"targetname" - will not spawn until triggered
"wait" - will respawn after 'wait' seconds. fires targets every time.
"count" - limit number of times to respawn
*/
/*FGD
@PointClass size(-16 -16 -24, 16 16 32) base(Item) model({ "path": ":progs/quaddama.mdl" }) =
	item_artifact_super_damage : "Quad damage" []
*/
void() item_artifact_super_damage =
{
	if (!SUB_ShouldSpawn()) return;
	self.touch = powerup_touch;

	precache_super_damage();
	setmodel (self, "progs/quaddama.mdl");
	self.netname = "Quad Damage";
	self.noise = "items/damage.wav";
	self.items = IT_QUAD;
	setsize (self, '-16 -16 -24', '16 16 32');
	if (deathmatch)
		self.wait = 60;
	StartItem ();
}





