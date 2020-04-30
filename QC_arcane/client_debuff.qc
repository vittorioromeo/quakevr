/*======================================================================
  CheckDeBuff - Check for negative buffs affecting the player
======================================================================*/

.float        cshift_upd;            // Screen updates (v_cshift) are active
.float        cshift_time;        // Timer (0.1s tick) for screen updates
.vector        cshift_col;            // RGB / XYZ values for screen colour
.float        cshift_den;            // Screen colour density
.float        cshift_step;        // Quantity to reduce density by per tick

.float        debuff_sound;        // Heartbeat sound when tick happens
.float        debuff_qty;            // Quantity to do something with (temp)

.float        bleeding;            // Bleeding debuff status
.float        bleeding_time;        // Timer (1s tick) for screen/sound update
.float        bleeding_qty;        // Total amount of health stored (HoT)

.float        burning;            // Burning debuff status
.float        burning_time;        // Timer (1s tick) for screen/sound update
.float        burning_qty;        // Total amount of damage stored (DoT)

.float        poisoned;            // Poison debuff status
.float        poisoned_time;        // Timer (1s tick) for screen/sound update
.float        poisoned_qty;        // Total amount of damage stored (DoT)

// IT_INVISIBILITY + IT_INVULNERABILITY + IT_SUIT + IT_QUAD
// 524288 + 1048576 + 2097152 + 4194304
float ALL_ITARTIFACTS = 7864320;

// Wraith Health drain
vector DEBUFF_BLEED_COL = '200 0 0';    // Strong red
float DEBUFF_BLEED_DEN = 120;
float DEBUFF_BLEED_STEP = 10;
float DEBUFF_BLEED_HEAL = 5;
float DEBUFF_BLEED_MIN = 5;

// Pyro flame burn
vector DEBUFF_BURN_COL = '200 100 0';    // Orange
float DEBUFF_BURN_DEN = 120;
float DEBUFF_BURN_STEP = 10;
float DEBUFF_BURN_DMG = 1;
float DEBUFF_BURN_MAX = 10;

// Mummy poisoned gibs
vector DEBUFF_POISON_COL = '0 200 0';    // Strong Green
float DEBUFF_POISON_DEN = 84;
float DEBUFF_POISON_STEP = 7;
float DEBUFF_POISON_DMG = 1;

//----------------------------------------------------------------------
// Various debuff reset functions, used all over the place
//----------------------------------------------------------------------
void(entity targ) ResetDebuffScreen =
{
    targ.cshift_upd = FALSE;
    targ.cshift_time = LARGE_TIMER;
    stuffcmd(targ, "v_cshift 0 0 0 0\n");
    targ.cshift_col = '0 0 0';
    targ.cshift_den = 0;
};

void(entity targ) ResetDebuffBurning =
{ targ.burning = targ.burning_qty = 0; };

void(entity targ) ResetDebuffPoisoned =
{ targ.poisoned = targ.poisoned_qty = 0; };

void(entity targ) ResetDebuffBleeding =
{ targ.bleeding = targ.bleeding_qty = 0; };

void(entity targ) ResetDebuffSystem =
{
    ResetDebuffScreen(targ);
    ResetDebuffBurning(targ);
    ResetDebuffPoisoned(targ);
    ResetDebuffBleeding(targ);
};

//----------------------------------------------------------------------
// Core debuff function, run every frame from client.qc
//----------------------------------------------------------------------
void() ClientDeBuff =
{
    // If player is dead or intermission running, turn everything off
    if (intermission_running > 0 || self.health < 1) {
        ResetDebuffSystem(self);
        return;
    }

    // Check for burning debuff (DoT)
    if (self.burning) {
        // Is it time for a debuff update
        if (self.burning_time < time) {
            if (self.burning_qty > 0) {
                // Reset burning tick timer
                self.burning_time = time + 1;
                
                // Give priority to bleeding(heal) and poisoned
                if (!self.bleeding && !self.poisoned) {
                    if (self.debuff_sound < time) self.debuff_sound = time + 1;
                    self.cshift_upd = TRUE;        // Update screen colour
                    self.cshift_time = -1;        // Update striaght away
                    self.cshift_col = DEBUFF_BURN_COL;
                    self.cshift_den = DEBUFF_BURN_DEN;
                    self.cshift_step = DEBUFF_BURN_STEP;
                }
                
                // Setup default burning damage
                self.debuff_qty = DEBUFF_BURN_DMG;
                // Check burning does not exceed stored damage
                if (self.burning_qty < self.debuff_qty) self.debuff_qty = self.burning_qty;
                // Subtract burning damage from counters
                self.burning_qty = self.burning_qty - self.debuff_qty;
                
                // Use global damage function (has death checks)
                T_Damage (self, world, world, self.debuff_qty, NOARMOR);
            }
        }
        // Check if player is in water up to waist?
        // 0 = not in, 1 = feet, 2 = waist, 3 = eyes
        if (self.waterlevel >= 2 || self.burning_qty < 1) {
            ResetDebuffBurning(self);
        }
    }
    
    // Check for poisoned debuff (DoT)
    if (self.poisoned) {
        // Is it time for a debuff update
        if (self.poisoned_time < time) {
            if (self.poisoned_qty > 0) {
                // Reset poisoned tick timer
                self.poisoned_time = time + 1;

                // Give priority to bleeding(heal)
                if (!self.bleeding) {
                    if (self.debuff_sound < time) self.debuff_sound = time + 1;
                    self.cshift_upd = TRUE;        // Update screen colour
                    self.cshift_time = -1;        // Update striaght away
                    self.cshift_col = DEBUFF_POISON_COL;
                    self.cshift_den = DEBUFF_POISON_DEN;
                    self.cshift_step = DEBUFF_POISON_STEP;
                }
                
                // Setup default poison damage
                self.debuff_qty = DEBUFF_POISON_DMG;
                // Check poison does not exceed damage left
                if (self.poisoned_qty < self.debuff_qty) self.debuff_qty = self.poisoned_qty;
                // Subtract poison damage from counters
                self.poisoned_qty = self.poisoned_qty - self.debuff_qty;
                
                // Use global damage function (has death checks)
                T_Damage (self, world, world, self.debuff_qty, NOARMOR);
            }
        }
        // Check if poison has finished?
        if (self.poisoned_qty < 1) ResetDebuffPoisoned(self);
    }

    // Check for bleeding debuff (HoT)
    if (self.bleeding) {
        // Is it time for a debuff update
        if (self.bleeding_time < time) {
            if (self.bleeding_qty > 0) {
                // Reset bleeding(healing) tick timer
                self.bleeding_time = time + 1;
                if (self.debuff_sound < time) self.debuff_sound = time + 1;
                self.cshift_upd = TRUE;        // Update screen colour
                self.cshift_time = -1;        // Update striaght away
                self.cshift_col = DEBUFF_BLEED_COL;
                self.cshift_den = DEBUFF_BLEED_DEN;
                self.cshift_step = DEBUFF_BLEED_STEP;

                // Setup default healing
                self.debuff_qty = DEBUFF_BLEED_HEAL;
                // Check healing does not exceed current reserve
                if (self.bleeding_qty < self.debuff_qty) self.debuff_qty = self.bleeding_qty;
                // Subtract bleeding heals from counters
                self.bleeding_qty = self.bleeding_qty - self.debuff_qty;
                // Check healing does not go over max player limit
                if (self.health + self.debuff_qty > self.max_health)
                    self.debuff_qty = self.max_health - self.health;
                
                // Directly add health back to player
                self.health = self.health + self.debuff_qty;
            }
        }
        // Check if player is over max health or run out of healing
        if (self.health >= self.max_health || self.bleeding_qty < 1) {
            ResetDebuffBleeding(self);
        }
    }
    
    // Check for any debuff screen updates?
    if (self.cshift_upd) {
        // Play different debuff sounds (pressure is on!)
        if (self.debuff_sound > time) {
            self.debuff_sound = -1;        // Reset

            // Sound exception for god/pentagram mode
            if (self.flags & FL_GODMODE || self.invincible_finished >= time) {
                if (self.invincible_sound < time) {
                    sound (self, CHAN_VOICE, SOUND_ARTPENT3, 1, ATTN_NORM);
                    self.invincible_sound = time + 2;
                }
            }
            else {
                // Play heartbeat only for bleeding/healing
                if (self.bleeding) 
                    sound (self, CHAN_VOICE, SOUND_HEARTBEAT, 1, ATTN_IDLE);
                // Re-use player pain sounds
                else if (self.poisoned) {
                    if (random() < 0.5) sound (self, CHAN_VOICE, "player/pain2.wav", 1, ATTN_IDLE);
                    else sound (self, CHAN_VOICE, "player/pain3.wav", 1, ATTN_IDLE);
                }
                // Play on the same channel as player pain
                else if (self.burning) {
                    if (random() < 0.5) sound (self, CHAN_VOICE, "player/pain1.wav", 1, ATTN_IDLE);
                    else sound (self, CHAN_VOICE, "player/pain4.wav", 1, ATTN_IDLE);
                }
            }
        }
        // Check for screen update? 0.1s tick interval
        if (self.cshift_time < time) {
            // Generate some particles based on debuff
            // Priority is bleeding > poisoned > burning
            if (self.bleeding) particle_debuff(self.origin, 16, rint(1+random()*4), PARTICLE_BURST_RED);
            else if (self.poisoned) particle_debuff(self.origin, 16, rint(1+random()*4), PARTICLE_BURST_GREEN);
            else if (self.burning) particle_debuff(self.origin, 16, rint(1+random()*4), PARTICLE_BURST_YELLOW);

            // Count down density and check for reset condition
            self.cshift_den = self.cshift_den - self.cshift_step;
            if (self.cshift_den < 1) ResetDebuffScreen(self);
            else {
                // Update screen using stuffcmds!
                self.cshift_time = time + 0.1;

                // Don't update the screen if an artifact is active
                // The engine will be updating at the same time
                if (self.items & ALL_ITARTIFACTS == 0) {
                    stuffcmd(self, "v_cshift ");
                    stuffcmd(self, ftos(rint(self.cshift_col_x)));
                    stuffcmd(self, " ");
                    stuffcmd(self, ftos(rint(self.cshift_col_y)));
                    stuffcmd(self, " ");
                    stuffcmd(self, ftos(rint(self.cshift_col_z)));
                    stuffcmd(self, " ");
                    stuffcmd(self, ftos(rint(self.cshift_den)));
                    stuffcmd(self, "\n");
                }
            }
        }
    }
    // Reset cshift if nothing is active (engine does not reset)
    // This is to catch new games or quickload resets
    else if (self.cshift_time < time) ResetDebuffScreen(self);
};

//----------------------------------------------------------------------
void(entity targ) BleedDeBuff =
{
    local float debuffqty;
    
    // Check for minimum amount of health for debuff
    // Immune to debuff if using Pent of Protection or BioSuit
    if (targ.health < DEBUFF_BLEED_MIN) return;
    if (targ.flags & FL_CLIENT == FALSE) return;
    if (targ.items & (IT_INVULNERABILITY | IT_SUIT)) {
        T_Damage (targ, self, self, DAMGE_WRAITHBOLT, DAMARMOR);
        return;
    }
    
    // if debuff not active, starting bleeding!
    if (!targ.bleeding) {
        targ.bleeding = TRUE;
        targ.bleeding_time = -1;
    }

    // Take health from player and store it
    debuffqty = rint(targ.health * 0.5);
    T_Damage (targ, self, self, debuffqty, NOARMOR);
    targ.bleeding_qty = targ.bleeding_qty + debuffqty;
};

//----------------------------------------------------------------------
void(entity targ) BurnDeBuff =
{
    local float debuffqty;
    
    // Do not stack burning time beyond a certain limit
    // Cannot be burning if knee deep in water
    // Immune to debuff if using Pent of Protection or BioSuit
    if (targ.burning_qty > DEBUFF_BURN_MAX) return;
    if (targ.flags & FL_CLIENT == FALSE) return;
    if (targ.waterlevel >= 3) return;
    if (targ.items & (IT_INVULNERABILITY | IT_SUIT)) return;
    
    // if debuff not active, starting burning!
    if (!targ.burning) {
        targ.burning = TRUE;
        targ.burning_time = -1;
    }
        
    // work out time remaining on debuff
    debuffqty = rint(5*skill + 3 + 2*random());
    targ.burning_qty = targ.burning_qty + debuffqty;
    if (targ.burning_qty > DEBUFF_BURN_MAX) targ.burning_qty = DEBUFF_BURN_MAX;

    targ.cshift_upd = TRUE;        // Update screen colour
    targ.cshift_time = -1;        // Update striaght away
    targ.cshift_col = DEBUFF_BURN_COL;
    targ.cshift_den = DEBUFF_BURN_DEN;
    targ.cshift_step = DEBUFF_BURN_STEP;
};

//----------------------------------------------------------------------
void(entity targ) PoisonDeBuff =
{
    local float debuffqty;
    
    // Immune to debuff if using Pent of Protection or BioSuit
    if (targ.flags & FL_CLIENT == FALSE) return;
    if (targ.items & (IT_INVULNERABILITY | IT_SUIT)) return;
    if (targ.poisoned) return;    // Only poison once
    
    // if debuff not active, start poison!
    targ.poisoned = TRUE;
    targ.poisoned_time = -1;
    // work out time remaining on debuff
    debuffqty = rint(skill + 3 + 2*random());
    targ.poisoned_qty = targ.poisoned_qty + debuffqty;

    targ.cshift_upd = TRUE;        // Update screen colour
    targ.cshift_time = -1;        // Update striaght away
    targ.cshift_col = DEBUFF_POISON_COL;
    targ.cshift_den = DEBUFF_POISON_DEN;
    targ.cshift_step = DEBUFF_POISON_STEP;
};

//----------------------------------------------------------------------
// Used by Pyro and Wraith; consistent damage from both monsters
//----------------------------------------------------------------------
void(entity targ, float playdmg, float mondmg) ApplyFireDmg =
{
    local float reduction, firedamage;
    
    // Bio suit reduction - easy=80%, normal=70%, hard=60%, nm=50%
    reduction = 0.2 + skill * 0.1;
    
    // Damage is different for monsters/players
    if (targ.flags & FL_MONSTER) firedamage = mondmg;
    else if (targ.items & IT_SUIT) firedamage = rint(playdmg * reduction);
    else firedamage = playdmg;
    
    // Final damage (no armour save)
    T_Damage (targ, self, self.owner, firedamage, NOARMOR);
    BurnDeBuff(targ);
};
