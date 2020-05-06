// Rogue Runes
// Jan'97 by ZOID <zoid@threewave.com>
// Under contract to id software for Rogue Entertainment

.float rune;        // what rune does this guy have?
.float runetime;    // last time we told him he's already got a rune

.float rune1_last_noise_time;
.float rune2_last_noise_time;
.float rune3_last_noise_time;
.float rune4_last_regen_time;

float runespawn;    // have we spawned runes?
entity rune_spawn_spot; // spawn spot for runes

float ITEM_RUNE1 = 1;    // Rune of Earth Magic:  Resistance
float ITEM_RUNE2 = 2;    // Rune of Black Magic:  Strength
float ITEM_RUNE3 = 4;    // Rune of Hell Magic:  Haste
float ITEM_RUNE4 = 8;    // Rune of Edler Magic:  Regeneration

void() RuneTouch =
{
    if(other.classname != "player" || other.health <= 0)
        return;

    if(other.rune) {
        if(other.runetime < time)
            centerprint(other, "You already have a rune\n");
        other.runetime = time + 5;
        return;
    }

    other.rune = other.rune | self.rune;

    sound(other, CHAN_ITEM, "weapons/pkup.wav", 1, ATTN_NORM);

    if(self.rune & ITEM_RUNE1)
        centerprint(other, "Earth Magic\n\nRESISTANCE");
    else if(self.rune & ITEM_RUNE2)
        centerprint(other, "Black Magic\n\nSTRENGTH");
    else if(self.rune & ITEM_RUNE3)
        centerprint(other, "Hell Magic\n\nHASTE");
    else if(self.rune & ITEM_RUNE4)
        centerprint(other, "Edler Magic\n\nRegeneration");
    remove(self);
};

entity() RuneSpawnPoint =
{
    while (1) {
        rune_spawn_spot = find(rune_spawn_spot, classname,
            "info_player_deathmatch");

        if(rune_spawn_spot != world)
            return rune_spawn_spot;
    }
};

void() RuneRespawn =
{
    local entity spot;

    spot = RuneSpawnPoint();
    setorigin(self, spot.origin);

    self.velocity_z = 300;
    self.velocity_x = -300 + (random() * 600);
    self.velocity_y = -300 + (random() * 600);

    self.nextthink = time + 120;
};

void(float r, entity spot) SpawnRune =
{
    local entity item;

    if(spot == world)
        spot = RuneSpawnPoint();

    item = spawn();

    setorigin(item, spot.origin);
    item.rune = r;
    item.velocity_z = 300;
    item.velocity_x = -300 + (random() * 600);
    item.velocity_y = -300 + (random() * 600);

    item.flags = FL_ITEM;
    item.solid = SOLID_TRIGGER;
    item.movetype = MOVETYPE_TOSS;

    if(r & ITEM_RUNE1)
            setmodel(item, "progs/end1.mdl");
    else if(r & ITEM_RUNE2)
            setmodel(item, "progs/end2.mdl");
    else if(r & ITEM_RUNE3)
            setmodel(item, "progs/end3.mdl");
    else if(r & ITEM_RUNE4)
            setmodel(item, "progs/end4.mdl");

    setsize(item, '-16 -16 0', '16 16 56');
    item.touch = RuneTouch;

    item.nextthink = time + 120;    // remove after 2 minutes
    item.think = RuneRespawn;
};

// self is player
void() DropRune =
{
    if(self.rune & ITEM_RUNE1)
        SpawnRune(ITEM_RUNE1, self);
    if(self.rune & ITEM_RUNE2)
        SpawnRune(ITEM_RUNE2, self);
    if(self.rune & ITEM_RUNE3)
        SpawnRune(ITEM_RUNE3, self);
    if(self.rune & ITEM_RUNE4)
        SpawnRune(ITEM_RUNE4, self);
    self.rune = 0;
};

void() DoSpawnRunes =
{
    remove(self);

    SpawnRune(ITEM_RUNE1, world);
    SpawnRune(ITEM_RUNE2, world);
    SpawnRune(ITEM_RUNE3, world);
    SpawnRune(ITEM_RUNE4, world);
};

void() SpawnRunes =
{
    local entity spawner;

    if(!deathmatch || !(cvar_hget(cvarh_gamecfg) & GAMECFG_ENABLE_RUNES) || runespawn)
        return;
    runespawn = 1;
    spawner = spawn();
    spawner.think = DoSpawnRunes;
    spawner.nextthink = time + 0.1;
};

float(float damage, entity who) RuneApplyEarth =
{
    if(who.rune & ITEM_RUNE1) {
        if(who.rune1_last_noise_time < time) {
            sound(who, CHAN_ITEM, "runes/end1.wav", 1, ATTN_NORM);
            // tune as needed
            who.rune1_last_noise_time = time + 1.0;
        }
        return damage/2;
    }
    return damage;
};

void(entity who) RuneApplyBlackNoise =
{
    if(who.rune & ITEM_RUNE2)
        if(who.rune2_last_noise_time < time) {
            sound(who, CHAN_ITEM, "runes/end2.wav", 1, ATTN_NORM);
            // tune as needed
            who.rune2_last_noise_time = time + 1.0;
        }
};

float(float damage, entity who) RuneApplyBlack =
{
    if(who.rune & ITEM_RUNE2)
        return damage*2;
    return damage;
};

float(float tvalue, entity who) RuneApplyHell =
{
    if(who.rune & ITEM_RUNE3) {
        if(who.rune3_last_noise_time < time) {
            sound(who, CHAN_ITEM, "runes/end3.wav", 1, ATTN_NORM);
            // tune as needed
            who.rune3_last_noise_time = time + 1.0;
        }
        return (tvalue * 2) / 3;
    }
    return tvalue;
};

void(entity who) RuneApplyElder =
{
    if(who.rune & ITEM_RUNE4) {
        if(who.rune4_last_regen_time < time && who.health < 100) {
            sound(who, CHAN_ITEM, "runes/end4.wav", 1, ATTN_NORM);
            who.health = who.health + 5;
            if(who.health > 100)
                who.health = 100;
            who.rune4_last_regen_time = time + 1.0;
        }
    }
};

float(entity who) RuneHasElder =
{
    if(who.rune & ITEM_RUNE4)
        return TRUE;
    return FALSE;
};

void() RunePrecache =
{
    if(cvar_hget(cvarh_deathmatch))
        if(cvar_hget(cvarh_gamecfg) & GAMECFG_ENABLE_RUNES) {
            precache_model("progs/end1.mdl");
            precache_model("progs/end2.mdl");
            precache_model("progs/end3.mdl");
            precache_model("progs/end4.mdl");

            precache_sound("runes/end1.wav");
            precache_sound("runes/end2.wav");
            precache_sound("runes/end3.wav");
            precache_sound("runes/end4.wav");
        }
};
